/*
 * $Id$
 */

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>

#include <pthread.h>

#include <event.h>

#include <cli.h>
#include <cli_priv.h>

#include "libvarnish.h"
#include "heritage.h"
#include "shmlog.h"
#include "vcl_lang.h"
#include "cache.h"
#include "cli_event.h"

static struct event ev_keepalive;

struct hash_slinger	*hash;
struct stevedore	*stevedore;

pthread_mutex_t	sessmtx;

/*--------------------------------------------------------------------*/

static void
timer_keepalive(int a, short b, void *c)
{

	printf("%s(%d, %d, %p)\n", __func__, a, b, c);
	printf("Heeellloooo ?   Ohh bother...\n");
	exit (1);
}

static void
arm_keepalive(void)
{
	struct timeval tv;

	tv.tv_sec = 30;
	tv.tv_usec = 0;

	evtimer_del(&ev_keepalive);
	evtimer_add(&ev_keepalive, &tv);
}

/*--------------------------------------------------------------------*/

static void
cli_func_url_query(struct cli *cli, char **av, void *priv)
{

	(void)priv;
	cli_out(cli, "url <%s>", av[2]);
	cli_result(cli, CLIS_UNIMPL);
}

/*--------------------------------------------------------------------*/

static void
cli_func_ping(struct cli *cli, char **av, void *priv)
{
	time_t t;

	(void)priv;
	VSL(SLT_CLI, 0, av[1]);
	arm_keepalive();
	if (av[2] != NULL) {
		/* XXX: check clock skew is pointless here */
	}
	time(&t);
	cli_out(cli, "PONG %ld\n", t);
}

/*--------------------------------------------------------------------*/

static struct cli_proto cli_proto[] = {
	{ CLI_URL_QUERY,	cli_func_url_query },
	{ CLI_CONFIG_LOAD,	cli_func_config_load },
	{ CLI_CONFIG_LIST,	cli_func_config_list },
	{ CLI_CONFIG_UNLOAD,	cli_func_config_unload },
	{ CLI_CONFIG_USE,	cli_func_config_use },
	{ CLI_PING,		cli_func_ping },
	{ NULL }
};

/*--------------------------------------------------------------------*/

void
child_main(void)
{
	struct event_base *eb;
	struct cli *cli;
	int i;

	/* XXX: SO_NOSIGPIPE does not work reliably :-( */
	signal(SIGPIPE, SIG_IGN);

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	printf("Child starts\n");

	AZ(pthread_mutex_init(&sessmtx, NULL));
	VBE_Init();
	VSL_Init();
	CacheInitPool();

	VCA_Init();

	eb = event_init();
	assert(eb != NULL);

	hash = &hsl_slinger;
	if (hash->init != NULL)
		hash->init();

	stevedore = heritage.stevedore;
	if (stevedore->open != NULL)
		stevedore->open(stevedore);

	CVCL_Load(heritage.vcl_file, "boot");
	cli = cli_setup(eb, heritage.fds[2], heritage.fds[1], 0, cli_proto);

	evtimer_set(&ev_keepalive, timer_keepalive, NULL);
	event_base_set(eb, &ev_keepalive);
	arm_keepalive();

	i = event_base_loop(eb, 0);
	if (i != 0)
		printf("event_dispatch() = %d\n", i);

	printf("Child dies\n");
}

