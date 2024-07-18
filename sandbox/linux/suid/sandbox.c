// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// https://chromium.googlesource.com/chromium/src/+/main/docs/linux/suid_sandbox.md

#include "sandbox/linux/suid/common/sandbox.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <asm/unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <sys/wait.h>
#include <unistd.h>

#include "sandbox/linux/suid/common/suid_unsafe_environment_variables.h"
#include "sandbox/linux/suid/process_util.h"

#if !defined(CLONE_NEWPID)
#define CLONE_NEWPID 0x20000000
#endif
#if !defined(CLONE_NEWNET)
#define CLONE_NEWNET 0x40000000
#endif

static bool DropRoot();

#define HANDLE_EINTR(x) ({ \
  long eintr_wrapper_result; \
  do { \
    eintr_wrapper_result = (x); \
  } while (eintr_wrapper_result == -1L && errno == EINTR); \
  eintr_wrapper_result; \
})

static void FatalError(const char* msg, ...)
    __attribute__((noreturn, format(printf, 1, 2)));

static void FatalError(const char* msg, ...) {
  va_list ap;
  va_start(ap, msg);

  vfprintf(stderr, msg, ap);
  fprintf(stderr, ": %s\n", strerror(errno));
  fflush(stderr);
  va_end(ap);
  _exit(1);
}

static void ExitWithErrorSignalHandler(int signal) {
  const char msg[] = "\nThe setuid sandbox got signaled, exiting.\n";
  if (-1 == write(2, msg, sizeof(msg) - 1)) {
    // Do nothing.
  }

  _exit(1);
}

// We will chroot() to the helper's /proc/self directory. Anything there will
// not exist anymore if we make sure to wait() for the helper.
//
// /proc/self/fdinfo or /proc/self/fd are especially safe and will be empty
// even if the helper survives as a zombie.
//
// There is very little reason to use fdinfo/ instead of fd/ but we are
// paranoid. fdinfo/ only exists since 2.6.22 so we allow fallback to fd/
#define SAFE_DIR "/proc/self/fdinfo"
#define SAFE_DIR2 "/proc/self/fd"

static bool SpawnChrootHelper() {
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
    perror("socketpair");
    return false;
  }

  char* safedir = NULL;
  struct stat sdir_stat;
  if (!stat(SAFE_DIR, &sdir_stat) && S_ISDIR(sdir_stat.st_mode)) {
    safedir = SAFE_DIR;
  } else if (!stat(SAFE_DIR2, &sdir_stat) && S_ISDIR(sdir_stat.st_mode)) {
    safedir = SAFE_DIR2;
  } else {
    fprintf(stderr, "Could not find %s\n", SAFE_DIR2);
    return false;
  }

  const pid_t pid = syscall(__NR_clone, CLONE_FS | SIGCHLD, 0, 0, 0);

  if (pid == -1) {
    perror("clone");
    close(sv[0]);
    close(sv[1]);
    return false;
  }

  if (pid == 0) {
    // We share our files structure with an untrusted process. As a security in
    // depth measure, we make sure that we can't open anything by mistake.
    // TODO(agl): drop CAP_SYS_RESOURCE / use SECURE_NOROOT

    const struct rlimit nofile = {0, 0};
    if (setrlimit(RLIMIT_NOFILE, &nofile))
      FatalError("Setting RLIMIT_NOFILE");

    if (close(sv[1]))
      FatalError("close");

    // wait for message
    char msg;
    ssize_t bytes;
    do {
      bytes = read(sv[0], &msg, 1);
    } while (bytes == -1 && errno == EINTR);

    if (bytes == 0)
      _exit(0);
    if (bytes != 1)
      FatalError("read");

    // do chrooting
    if (msg != kMsgChrootMe)
      FatalError("Unknown message from sandboxed process");

    // sanity check
    if (chdir(safedir))
      FatalError("Cannot chdir into /proc/ directory");

    if (chroot(safedir))
      FatalError("Cannot chroot into /proc/ directory");

    if (chdir("/"))
      FatalError("Cannot chdir to / after chroot");

    const char reply = kMsgChrootSuccessful;
    do {
      bytes = write(sv[0], &reply, 1);
    } while (bytes == -1 && errno == EINTR);

    if (bytes != 1)
      FatalError("Writing reply");

    _exit(0);
    // We now become a zombie. /proc/self/fd(info) is now an empty dir and we
    // are chrooted there.
    // Our (unprivileged) parent should not even be able to open "." or "/"
    // since they would need to pass the ptrace() check. If our parent wait()
    // for us, our root directory will completely disappear.
  }

  if (close(sv[0])) {
    close(sv[1]);
    perror("close");
    return false;
  }

  // In the parent process, we install an environment variable containing the
  // number of the file descriptor.
  char desc_str[64];
  int printed = snprintf(desc_str, sizeof(desc_str), "%u", sv[1]);
  if (printed < 0 || printed >= (int)sizeof(desc_str)) {
    fprintf(stderr, "Failed to snprintf\n");
    return false;
  }

  if (setenv(kSandboxDescriptorEnvironmentVarName, desc_str, 1)) {
    perror("setenv");
    close(sv[1]);
    return false;
  }

  // We also install an environment variable containing the pid of the child
  char helper_pid_str[64];
  printed = snprintf(helper_pid_str, sizeof(helper_pid_str), "%u", pid);
  if (printed < 0 || printed >= (int)sizeof(helper_pid_str)) {
    fprintf(stderr, "Failed to snprintf\n");
    return false;
  }

  if (setenv(kSandboxHelperPidEnvironmentVarName, helper_pid_str, 1)) {
    perror("setenv");
    close(sv[1]);
    return false;
  }

  return true;
}

// Block until child_pid exits, then exit. Try to preserve the exit code.
static void WaitForChildAndExit(pid_t child_pid) {
  int exit_code = -1;
  siginfo_t reaped_child_info;

  // Don't "Core" on SIGABRT. SIGABRT is sent by the Chrome OS session manager
  // when things are hanging.
  // Here, the current process is going to waitid() and _exit(), so there is no
  // point in generating a crash report. The child process is the one
  // blocking us.
  if (signal(SIGABRT, ExitWithErrorSignalHandler) == SIG_ERR) {
    FatalError("Failed to change signal handler");
  }

  int wait_ret =
      HANDLE_EINTR(waitid(P_PID, child_pid, &reaped_child_info, WEXITED));

  if (!wait_ret && reaped_child_info.si_pid == child_pid) {
    if (reaped_child_info.si_code == CLD_EXITED) {
      exit_code = reaped_child_info.si_status;
    } else {
      // Exit with code 0 if the child got signaled.
      exit_code = 0;
    }
  }
  _exit(exit_code);
}

static bool MoveToNewNamespaces() {
  // These are the sets of flags which we'll try, in order.
  const int kCloneExtraFlags[] = {CLONE_NEWPID | CLONE_NEWNET, CLONE_NEWPID, };

  // We need to close kZygoteIdFd before the child can continue. We use this
  // socketpair to tell the child when to continue;
  int sync_fds[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sync_fds)) {
    FatalError("Failed to create a socketpair");
  }

  for (size_t i = 0; i < sizeof(kCloneExtraFlags) / sizeof(kCloneExtraFlags[0]);
       i++) {
    pid_t pid = syscall(__NR_clone, SIGCHLD | kCloneExtraFlags[i], 0, 0, 0);
    const int clone_errno = errno;

    if (pid > 0) {
      if (!DropRoot()) {
        FatalError("Could not drop privileges");
      } else {
        if (close(sync_fds[0]) || shutdown(sync_fds[1], SHUT_RD))
          FatalError("Could not close socketpair");
        // The kZygoteIdFd needs to be closed in the parent before
        // Zygote gets started.
        if (close(kZygoteIdFd))
          FatalError("close");
        // Tell our child to continue
        if (HANDLE_EINTR(send(sync_fds[1], "C", 1, MSG_NOSIGNAL)) != 1)
          FatalError("send");
        if (close(sync_fds[1]))
          FatalError("close");
        // We want to keep a full process tree and we don't want our childs to
        // be reparented to (the outer PID namespace) init. So we wait for it.
        WaitForChildAndExit(pid);
      }
      // NOTREACHED
      FatalError("Not reached");
    }

    if (pid == 0) {
      if (close(sync_fds[1]) || shutdown(sync_fds[0], SHUT_WR))
        FatalError("Could not close socketpair");

      // Wait for the parent to confirm it closed kZygoteIdFd before we
      // continue
      char should_continue;
      if (HANDLE_EINTR(read(sync_fds[0], &should_continue, 1)) != 1)
        FatalError("Read on socketpair");
      if (close(sync_fds[0]))
        FatalError("close");

      if (kCloneExtraFlags[i] & CLONE_NEWPID) {
        setenv(kSandboxPIDNSEnvironmentVarName, "", 1 /* overwrite */);
      } else {
        unsetenv(kSandboxPIDNSEnvironmentVarName);
      }

      if (kCloneExtraFlags[i] & CLONE_NEWNET) {
        setenv(kSandboxNETNSEnvironmentVarName, "", 1 /* overwrite */);
      } else {
        unsetenv(kSandboxNETNSEnvironmentVarName);
      }

      break;
    }

    // If EINVAL then the system doesn't support the requested flags, so
    // continue to try a different set.
    // On any other errno value the system *does* support these flags but
    // something went wrong, hence we bail with an error message rather then
    // provide less security.
    if (errno != EINVAL) {
      fprintf(stderr, "Failed to move to new namespace:");
      if (kCloneExtraFlags[i] & CLONE_NEWPID) {
        fprintf(stderr, " PID namespaces supported,");
      }
      if (kCloneExtraFlags[i] & CLONE_NEWNET) {
        fprintf(stderr, " Network namespace supported,");
      }
      fprintf(stderr, " but failed: errno = %s\n", strerror(clone_errno));
      return false;
    }
  }

  // If the system doesn't support NEWPID then we carry on anyway.
  return true;
}

static bool DropRoot() {
  if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0)) {
    perror("prctl(PR_SET_DUMPABLE)");
    return false;
  }

  if (prctl(PR_GET_DUMPABLE, 0, 0, 0, 0)) {
    perror("Still dumpable after prctl(PR_SET_DUMPABLE)");
    return false;
  }

  gid_t rgid, egid, sgid;
  if (getresgid(&rgid, &egid, &sgid)) {
    perror("getresgid");
    return false;
  }

  if (setresgid(rgid, rgid, rgid)) {
    perror("setresgid");
    return false;
  }

  uid_t ruid, euid, suid;
  if (getresuid(&ruid, &euid, &suid)) {
    perror("getresuid");
    return false;
  }

  if (setresuid(ruid, ruid, ruid)) {
    perror("setresuid");
    return false;
  }

  return true;
}

static bool SetupChildEnvironment() {
  unsigned i;

  // ld.so may have cleared several environment variables because we are SUID.
  // However, the child process might need them so zygote_host_impl_linux.cc
  // saves a copy in SANDBOX_$x. This is safe because we have dropped root by
  // this point, so we can only exec a binary with the permissions of the user
  // who ran us in the first place.

  for (i = 0; kSUIDUnsafeEnvironmentVariables[i]; ++i) {
    const char* const envvar = kSUIDUnsafeEnvironmentVariables[i];
    char* const saved_envvar = SandboxSavedEnvironmentVariable(envvar);
    if (!saved_envvar)
      return false;

    const char* const value = getenv(saved_envvar);
    if (value) {
      setenv(envvar, value, 1 /* overwrite */);
      unsetenv(saved_envvar);
    }

    free(saved_envvar);
  }

  return true;
}

bool CheckAndExportApiVersion() {
  // Check the environment to see if a specific API version was requested.
  // assume version 0 if none.
  int api_number = -1;
  char* api_string = getenv(kSandboxEnvironmentApiRequest);
  if (!api_string) {
    api_number = 0;
  } else {
    errno = 0;
    char* endptr = NULL;
    long long_api_number = strtol(api_string, &endptr, 10);
    if (!endptr || *endptr || errno != 0 || long_api_number < INT_MIN ||
        long_api_number > INT_MAX) {
      return false;
    }
    api_number = long_api_number;
  }

  // Warn only for now.
  if (api_number != kSUIDSandboxApiNumber) {
    fprintf(
        stderr,
        "The setuid sandbox provides API version %d, "
        "but you need %d\n"
        "Please read "
        "https://chromium.googlesource.com/chromium/src/+/main/docs/linux/suid_sandbox_development.md."
        "\n\n",
        kSUIDSandboxApiNumber,
        api_number);
  }

  // Export our version so that the sandboxed process can verify it did not
  // use an old sandbox.
  char version_string[64];
  snprintf(version_string, sizeof(version_string), "%d", kSUIDSandboxApiNumber);
  if (setenv(kSandboxEnvironmentApiProvides, version_string, 1)) {
    perror("setenv");
    return false;
  }

  return true;
}

int main(int argc, char** argv) {
  if (argc <= 1) {
    if (argc <= 0) {
      return 1;
    }

    fprintf(stderr, "Usage: %s <renderer process> <args...>\n", argv[0]);
    return 1;
  }

  // Allow someone to query our API version
  if (argc == 2 && 0 == strcmp(argv[1], kSuidSandboxGetApiSwitch)) {
    printf("%d\n", kSUIDSandboxApiNumber);
    return 0;
  }

  // We cannot adjust /proc/pid/oom_adj for sandboxed renderers
  // because those files are owned by root. So we need a helper here.
  if (argc == 4 && (0 == strcmp(argv[1], kAdjustOOMScoreSwitch))) {
    char* endptr = NULL;
    long score;
    errno = 0;
    unsigned long pid_ul = strtoul(argv[2], &endptr, 10);
    if (pid_ul == ULONG_MAX || !endptr || *endptr || errno != 0)
      return 1;
    pid_t pid = pid_ul;
    endptr = NULL;
    errno = 0;
    score = strtol(argv[3], &endptr, 10);
    if (score == LONG_MAX || score == LONG_MIN || !endptr || *endptr ||
        errno != 0) {
      return 1;
    }
    return AdjustOOMScore(pid, score);
  }

  // Protect the core setuid sandbox functionality with an API version
  if (!CheckAndExportApiVersion()) {
    return 1;
  }

  if (geteuid() != 0) {
    fprintf(stderr,
            "The setuid sandbox is not running as root. Common causes:\n"
            "  * An unprivileged process using ptrace on it, like a debugger.\n"
            "  * A parent process set prctl(PR_SET_NO_NEW_PRIVS, ...)\n");
  }

  if (!MoveToNewNamespaces())
    return 1;
  if (!SpawnChrootHelper())
    return 1;
  if (!DropRoot())
    return 1;
  if (!SetupChildEnvironment())
    return 1;

  execv(argv[1], &argv[1]);
  FatalError("execv failed");
}
