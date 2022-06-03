# Linux PID Namespace Support

The [LinuxSUIDSandbox](suid_sandbox.md) currently relies on support for
the `CLONE_NEWPID` flag in Linux's
[clone() system call](http://www.kernel.org/doc/man-pages/online/pages/man2/clone.2.html).
You can check whether your system supports PID namespaces with the code below,
which must be run as root:

```c
#define _GNU_SOURCE
#include <unistd.h>
#include <sched.h>
#include <stdio.h>
#include <sys/wait.h>

#if !defined(CLONE_NEWPID)
#define CLONE_NEWPID 0x20000000
#endif

int worker(void* arg) {
  const pid_t pid = getpid();
  if (pid == 1) {
    printf("PID namespaces are working\n");
  } else {
    printf("PID namespaces ARE NOT working. Child pid: %d\n", pid);
  }

  return 0;
}

int main() {
  if (getuid()) {
    fprintf(stderr, "Must be run as root.\n");
    return 1;
  }

  char stack[8192];
  const pid_t child = clone(worker, stack + sizeof(stack), CLONE_NEWPID, NULL);
  if (child == -1) {
    perror("clone");
    fprintf(stderr, "Clone failed. PID namespaces ARE NOT supported\n");
  }

  waitpid(child, NULL, 0);

  return 0;
}
```
