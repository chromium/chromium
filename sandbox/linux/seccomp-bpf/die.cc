// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/die.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <string>
#include <tuple>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/system_headers/linux_signal.h"

namespace sandbox {

void Die::ExitGroup() {
  // exit_group() should exit our program. After all, it is defined as a
  // function that doesn't return. But things can theoretically go wrong.
  // Especially, since we are dealing with system call filters. Continuing
  // execution would be very bad in most cases where ExitGroup() gets called.
  // So, we'll try a few other strategies too.
  Syscall::Call(__NR_exit_group, 1);

  // We have no idea what our run-time environment looks like. So, signal
  // handlers might or might not do the right thing. Try to reset settings
  // to a defined state; but we have not way to verify whether we actually
  // succeeded in doing so. Nonetheless, triggering a fatal signal could help
  // us terminate.
  struct sigaction sa = {};
  sa.sa_handler = LINUX_SIG_DFL;
  sa.sa_flags = LINUX_SA_RESTART;
  sys_sigaction(LINUX_SIGSEGV, &sa, nullptr);
  Syscall::Call(__NR_prctl, PR_SET_DUMPABLE, (void*)0, (void*)0, (void*)0);
  if (*(volatile char*)0) {
  }

  // If there is no way for us to ask for the program to exit, the next
  // best thing we can do is to loop indefinitely. Maybe, somebody will notice
  // and file a bug...
  // We in fact retry the system call inside of our loop so that it will
  // stand out when somebody tries to diagnose the problem by using "strace".
  for (;;) {
    Syscall::Call(__NR_exit_group, 1);
  }
}

void Die::SandboxDie(const char* msg, const char* file, int line) {
  if (simple_exit_) {
    LogToStderr(msg, file, line);
  } else {
    logging::LogMessageFatal(file, line, logging::LOGGING_FATAL).stream()
        << msg;
  }
  ExitGroup();
}

void Die::RawSandboxDie(const char* msg) {
  if (!msg)
    msg = "";
  RAW_LOG(FATAL, msg);
  ExitGroup();
}

void Die::SandboxInfo(const char* msg, const char* file, int line) {
  if (!suppress_info_) {
    logging::LogMessage(file, line, logging::LOGGING_INFO).stream() << msg;
  }
}

void Die::LogToStderr(const char* msg, const char* file, int line) {
  if (msg) {
    char buf[40];
    snprintf(buf, sizeof(buf), "%d", line);
    std::string s = std::string(file) + ":" + buf + ":" + msg + "\n";

    // No need to loop. Short write()s are unlikely and if they happen we
    // probably prefer them over a loop that blocks.
    std::ignore =
        HANDLE_EINTR(Syscall::Call(__NR_write, 2, s.c_str(), s.length()));
  }
}

bool Die::simple_exit_ = false;
bool Die::suppress_info_ = false;

}  // namespace sandbox
