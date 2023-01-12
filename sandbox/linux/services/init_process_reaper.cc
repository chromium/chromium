// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/init_process_reaper.h"

#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"

namespace sandbox {

namespace {

void DoNothingSignalHandler(int signal) {}

}  // namespace

bool CreateInitProcessReaper(base::OnceClosure post_fork_parent_callback) {
  int sync_fds[2];
  // We want to use send, so we can't use a pipe
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sync_fds)) {
    PLOG(ERROR) << "Failed to create socketpair";
    return false;
  }
  pid_t child_pid = fork();
  if (child_pid == -1) {
    int close_ret;
    close_ret = IGNORE_EINTR(close(sync_fds[0]));
    DPCHECK(!close_ret);
    close_ret = IGNORE_EINTR(close(sync_fds[1]));
    DPCHECK(!close_ret);
    return false;
  }
  if (child_pid) {
    // In the parent, assuming the role of an init process.
    // The disposition for SIGCHLD cannot be SIG_IGN or wait() will only return
    // once all of our childs are dead. Since we're init we need to reap childs
    // as they come.
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &DoNothingSignalHandler;
    CHECK(sigaction(SIGCHLD, &action, NULL) == 0);

    int close_ret;
    close_ret = IGNORE_EINTR(close(sync_fds[0]));
    DPCHECK(!close_ret);
    close_ret = shutdown(sync_fds[1], SHUT_RD);
    DPCHECK(!close_ret);
    if (!post_fork_parent_callback.is_null())
      std::move(post_fork_parent_callback).Run();

    // Tell the child to continue
    CHECK(HANDLE_EINTR(send(sync_fds[1], "C", 1, MSG_NOSIGNAL)) == 1);
    close_ret = IGNORE_EINTR(close(sync_fds[1]));
    DPCHECK(!close_ret);

    for (;;) {
      // Loop until we have reaped our one natural child
      siginfo_t reaped_child_info;
      int wait_ret =
          HANDLE_EINTR(waitid(P_ALL, 0, &reaped_child_info, WEXITED));
      if (wait_ret)
        _exit(1);
      if (reaped_child_info.si_pid == child_pid) {
        int exit_code = 0;
        // We're done waiting
        if (reaped_child_info.si_code == CLD_EXITED) {
          exit_code = reaped_child_info.si_status;
        }
        // Exit with the same exit code as our child. Exit with 0 if we got
        // signaled.
        _exit(exit_code);
      }
    }
  } else {
    // The child needs to wait for the parent to run the callback to avoid a
    // race condition.
    int close_ret;
    close_ret = IGNORE_EINTR(close(sync_fds[1]));
    DPCHECK(!close_ret);
    close_ret = shutdown(sync_fds[0], SHUT_WR);
    DPCHECK(!close_ret);
    char should_continue;
    int read_ret = HANDLE_EINTR(read(sync_fds[0], &should_continue, 1));
    close_ret = IGNORE_EINTR(close(sync_fds[0]));
    DPCHECK(!close_ret);
    if (read_ret == 1)
      return true;
    else
      return false;
  }
}

}  // namespace sandbox.
