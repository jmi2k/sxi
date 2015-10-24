#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static void sigIgnore(int sig) {}
int main(int argc, char **argv)
{
	struct sigaction si = { .sa_handler = sigIgnore, .sa_flags = SA_RESTART };
	sigemptyset(&si.sa_mask);
	sigaction(SIGALRM, &si, 0);
	sigaction(SIGUSR1, &si, 0);
	sigset_t mask, old;
	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR1);
	sigprocmask(SIG_BLOCK, &mask, &old);
	const char *const home = getenv("HOME");
	const int homelen = strlen(home);
	const pid_t serverpid = fork();
	if (!serverpid) {
		sigprocmask(SIG_SETMASK, &old, 0); // Unblock
		signal(SIGTTIN, SIG_IGN);
		signal(SIGTTOU, SIG_IGN);
		signal(SIGUSR1, SIG_IGN);
		setpgid(0,getpid());
		char *const xrc[3] = {"sh", malloc(homelen+strlen("/.xserverrc")+1), 0};
		memcpy(xrc[1], home, homelen);
		memcpy(xrc[1]+homelen, "/.xserverrc", strlen("/.xserverrc")+1);
		execvp(xrc[1], xrc+1);
		execvp("sh", xrc);
		_exit(EXIT_FAILURE);
	}
	if (serverpid>=0){
		waitpid(serverpid, 0, WNOHANG);
		alarm(8); // kludge to avoid tcp race
		sigsuspend(&old);
		alarm(0);
		sigprocmask(SIG_SETMASK, &old, 0);
	}
	const pid_t clientpid = fork();
	if (!clientpid){
		if (setuid(getuid()) == -1) fputs("cannot setuid", stderr);
		else{
			setpgid(0, getpid());
			char *const xrc[3] = {"sh", malloc(homelen+strlen("/.xinitrc")+1), 0};
			memcpy(xrc[1], home, homelen);
			memcpy(xrc[1]+homelen, "/.xinitrc", strlen("/.xinitrc")+1);
			execvp(xrc[1], xrc+1);
			execvp("sh", xrc);
		}
		_exit(EXIT_FAILURE);
	}
	if (serverpid > 0 && clientpid > 0) {
		pid_t pid;
		do pid = wait(0); while (pid != clientpid && pid != serverpid);
	}
	if (clientpid > 0) killpg(clientpid, SIGHUP);
	else if (clientpid < 0) fputs("client error", stderr);
	if (!(serverpid >= 0 && killpg(serverpid, SIGTERM) && killpg(serverpid, SIGKILL))) fputs("server error", stderr);
}
