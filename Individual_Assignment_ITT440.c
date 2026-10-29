#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

#define CHILDREN 3
#define SIZE 256

volatile sig_atomic_t interrupted = 0;

void handle_interrupt(int sig)
{
    interrupted = 1;
}

int main(void)
{
    int msg_pipe[CHILDREN][2];
    int req_pipe[CHILDREN][2];
    pid_t children[CHILDREN];
    char message[SIZE];

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_interrupt;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    printf("\n");
    printf("==================================================\n");
    printf("       FORK & PIPE - PROCESS COMMUNICATION\n");
    printf("==================================================\n");
    printf("  Parent PID : %d\n", getpid());
    printf("  Child Count: %d\n", CHILDREN);
    printf("--------------------------------------------------\n");

    // Create pipes
    for (int i = 0; i < CHILDREN; i++) {
        if (pipe(msg_pipe[i]) == -1 ||
            pipe(req_pipe[i]) == -1) {
            perror("Pipe creation failed");
            return 1;
        }
    }

    // Create children
    for (int i = 0; i < CHILDREN; i++) {
        children[i] = fork();

        if (children[i] < 0) {
            perror("Fork failed");
            return 1;
        }

        if (children[i] == 0) {
            for (int j = 0; j < CHILDREN; j++) {
                close(msg_pipe[j][1]);
                close(req_pipe[j][0]);

                if (j != i) {
                    close(msg_pipe[j][0]);
                    close(req_pipe[j][1]);
                }
            }

            char received[SIZE];
            char request = 'R';
            char permission;

            write(req_pipe[i][1], &request, 1);

            if (read(msg_pipe[i][0], &permission, 1) == 1
                && permission == 'Y') {

                ssize_t n = read(msg_pipe[i][0],
                                 received, SIZE - 1);

                if (n > 0) {
                    received[n] = '\0';

                    printf("  [Child %d] PID: %-6d | Message: %s",
                           i + 1, getpid(), received);
                    fflush(stdout);
                }
            }

            close(msg_pipe[i][0]);
            close(req_pipe[i][1]);
            _exit(0);
        }
    }

    // Parent closes unused ends
    for (int i = 0; i < CHILDREN; i++) {
        close(msg_pipe[i][0]);
        close(req_pipe[i][1]);
    }

    printf("\n  Enter message to send to children:\n");
    printf("  > ");
    fflush(stdout);

    if (fgets(message, sizeof(message), stdin) == NULL) {
        message[0] = '\0';
    }

    if (interrupted) {
        printf("\n\n  [INTERRUPT] Ctrl+C detected!\n");
    } else {
        printf("\n--------------------------------------------------\n");
        printf("  PERMISSION & MESSAGE DELIVERY\n");
        printf("--------------------------------------------------\n");

        for (int i = 0; i < CHILDREN; i++) {
            char request;
            char permission = 'Y';

            if (read(req_pipe[i][0], &request, 1) == 1
                && request == 'R') {

                printf("  [Parent] Permission granted to Child %d\n",
                       i + 1);
                fflush(stdout);

                write(msg_pipe[i][1], &permission, 1);
                write(msg_pipe[i][1], message, strlen(message));
            }

            close(req_pipe[i][0]);
            close(msg_pipe[i][1]);

            waitpid(children[i], NULL, 0);
        }
    }

    if (interrupted) {
        for (int i = 0; i < CHILDREN; i++) {
            close(req_pipe[i][0]);
            close(msg_pipe[i][1]);
            waitpid(children[i], NULL, 0);
        }
    }

    printf("--------------------------------------------------\n");

    if (interrupted) {
        printf("  STATUS: Process interrupted successfully.\n");
    } else {
        printf("  STATUS: All child processes have finished.\n");
    }

    printf("==================================================\n\n");

    return 0;
}
