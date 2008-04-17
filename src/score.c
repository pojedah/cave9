#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <assert.h>
#include "score.h"

static void score_net_finish (Score* score)
{
	if(score->udp_pkt != NULL) {
		SDLNet_FreePacket(score->udp_pkt);
		score->udp_pkt = NULL;
	}
	if(score->udp_sock != 0) {
		SDLNet_UDP_Close(score->udp_sock);
		score->udp_sock = 0;
	}
}

static void score_net_init (Score* score)
{
	if(SDLNet_Init()==-1)
	{
		fprintf(stderr, "SDLNet_Init(): %s\n",SDLNet_GetError());
		exit(1);
	}
	atexit(SDLNet_Quit);

	IPaddress addr;
	score->udp_sock = 0;
	score->udp_pkt = NULL;
	if(SDLNet_ResolveHost(&addr,GLOBAL_SCORE_HOST, GLOBAL_SCORE_PORT) == -1) {
		fprintf(stderr, "SDLNet_ResolveHost(): %s\n", SDLNet_GetError());
	} else {
		score->udp_sock=SDLNet_UDP_Open(0);
		if(score->udp_sock == 0) {
			fprintf(stderr, "SDLNet_UDP_Open(): %s\n", SDLNet_GetError());
			score_net_finish(score);
		} else {
			if(SDLNet_UDP_Bind(score->udp_sock, 0, &addr) == -1) {
				fprintf(stderr, "SDLNet_UDP_Bind(): %s\n", SDLNet_GetError());
				score_net_finish(score);
			} else {
				score->udp_pkt = SDLNet_AllocPacket(GLOBAL_SCORE_LEN);
				if(score->udp_pkt != NULL) {
					memset (score->udp_pkt->data, 0, GLOBAL_SCORE_LEN);
				}
				else {
					score_net_finish(score);
				}
			}
		}
	}
}

static void score_net_update (Score* score)
{
	if (score->udp_sock == 0)
		return;

	snprintf ((char*)score->udp_pkt->data,GLOBAL_SCORE_LEN, "%d", score->global);
	score->udp_pkt->len = GLOBAL_SCORE_LEN;
	if (SDLNet_UDP_Send (score->udp_sock, 0, score->udp_pkt) == 1)
	{
		SDL_Delay (666); // XXX only wait 666ms for hiscores
		int n = SDLNet_UDP_Recv (score->udp_sock, score->udp_pkt);
		if (n == 1) {
			sscanf ((char*)score->udp_pkt->data, "%d", &score->global);
		}
		else if (n < 0) {
			fprintf (stderr, "SDLNet_UDP_Recv(%s,%d): %s\n",
					GLOBAL_SCORE_HOST, GLOBAL_SCORE_PORT, SDLNet_GetError());
		}
	}
	else {
		fprintf (stderr, "SDLNet_UDP_Send(): %s\n", SDLNet_GetError());
	}
}

void score_init (Score* score)
{
	score->local = 0;
	score->session = 0;
	score->global = 0;

	char* home = getenv("HOME");
	if (home != NULL) {
		size_t len = strlen(home) + strlen("/.cave/") + strlen(SCORE_FILE) + 1;
		score->filename = malloc (len);

		char* dir = score->filename;
		sprintf (dir, "%s/.cave9", home);
		mkdir (dir, 0755);
		strcat (score->filename, "/");
		strcat (score->filename, SCORE_FILE);
	}
	else {
		fprintf (stderr,
			"HOME environment variable not set, using current dir to save %s\n",
			SCORE_FILE);
		score->filename = strdup (SCORE_FILE);
	}
	assert (score->filename != NULL);

	FILE* fp = fopen (score->filename, "r");
	if (fp != NULL) {
		fscanf (fp, "%d", &score->local);
		fclose (fp);
	}

	score_net_init (score);
}

void score_finish (Score* score)
{
	score_net_finish (score);
	free (score->filename);
	memset (score, 0, sizeof(Score));
}

void score_update (Score* score, int new_score, bool is_global)
{
	if (new_score > score->session)
		score->session = new_score;

	if (is_global) {

		if (new_score > score->local) {
			score->local = new_score;
			FILE* fp = fopen (score->filename, "w");
			if (fp == NULL) {
				perror ("failed to open score file");
			} else {
				fprintf (fp, "%d\n", score->local);
				fclose (fp);
			}
		}

		if (new_score > score->global) {
			score->global = new_score;
			score_net_update (score);
		}
	}
}

