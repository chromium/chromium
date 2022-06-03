// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/scoped_process.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/callback.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/services/thread_helpers.h"

namespace sandbox {

namespace {

const char kSynchronisationChar[] = "D";

void WaitForever() {
  while(true) {
    pause();
  }
}

}  // namespace

ScopedProcess::ScopedProcess(base::OnceClosure child_callback)
    : child_process_id_(-1), process_id_(getpid()) {
  PCHECK(0 == pipe(pipe_fds_));
#if !defined(THREAD_SANITIZER)
  // Make sure that we can safely fork().
  CHECK(ThreadHelpers::IsSingleThreaded());
#endif
  child_process_id_ = fork();
  PCHECK(0 <= child_process_id_);

  if (0 == child_process_id_) {
    PCHECK(0 == IGNORE_EINTR(close(pipe_fds_[0])));
    pipe_fds_[0] = -1;
    std::move(child_callback).Run();
    // Notify the parent that the closure has run.
    CHECK_EQ(1, HANDLE_EINTR(write(pipe_fds_[1], kSynchronisationChar, 1)));
    WaitForever();
    NOTREACHED();
    _exit(1);
  }

  PCHECK(0 == IGNORE_EINTR(close(pipe_fds_[1])));
  pipe_fds_[1] = -1;
}

ScopedProcess::~ScopedProcess() {
  CHECK(IsOriginalProcess());
  if (child_process_id_ >= 0) {
    PCHECK(0 == kill(child_process_id_, SIGKILL));
    siginfo_t process_info;

    PCHECK(0 == HANDLE_EINTR(
                    waitid(P_PID, child_process_id_, &process_info, WEXITED)));
  }
  if (pipe_fds_[0] >= 0) {
    PCHECK(0 == IGNORE_EINTR(close(pipe_fds_[0])));
  }
  if (pipe_fds_[1] >= 0) {
    PCHECK(0 == IGNORE_EINTR(close(pipe_fds_[1])));
  }
}

int ScopedProcess::WaitForExit(bool* got_signaled) {
  DCHECK(got_signaled);
  CHECK(IsOriginalProcess());
  siginfo_t process_info;
  // WNOWAIT to make sure that the destructor can wait on the child.
  int ret = HANDLE_EINTR(
      waitid(P_PID, child_process_id_, &process_info, WEXITED | WNOWAIT));
  PCHECK(0 == ret) << "Did something else wait on the child?";

  if (process_info.si_code == CLD_EXITED) {
    *got_signaled = false;
  } else if (process_info.si_code == CLD_KILLED ||
             process_info.si_code == CLD_DUMPED) {
    *got_signaled = true;
  } else {
    CHECK(false) << "ScopedProcess needs to be extended for si_code "
                 << process_info.si_code;
  }
  return process_info.si_status;
}

bool ScopedProcess::WaitForClosureToRun() {
  char c = 0;
  int ret = HANDLE_EINTR(read(pipe_fds_[0], &c, 1));
  PCHECK(ret >= 0);
  if (0 == ret)
    return false;

  CHECK_EQ(c, kSynchronisationChar[0]);
  return true;
}

// It would be problematic if after a fork(), another process would start using
// this object.
// This method allows to assert it is not happening.
bool ScopedProcess::IsOriginalProcess() {
  // Make a direct syscall to bypass glibc caching of PIDs.
  pid_t pid = sys_getpid();
  return pid == process_id_;
}

}  // namespace sandbox
