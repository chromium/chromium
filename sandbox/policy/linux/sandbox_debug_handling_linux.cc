// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/sandbox_debug_handling_linux.h"

#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <sys/prctl.h>
#include <unistd.h>

#include <tuple>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/safe_sprintf.h"
#include "sandbox/policy/switches.h"

namespace sandbox {
namespace policy {

namespace {

void DoChrootSignalHandler(int) {
  const int old_errno = errno;
  const char kFirstMessage[] = "Chroot signal handler called.\n";
  std::ignore = write(STDERR_FILENO, kFirstMessage, sizeof(kFirstMessage) - 1);

  const int chroot_ret = chroot("/");

  char kSecondMessage[100];
  const ssize_t printed = base::strings::SafeSPrintf(
      kSecondMessage, "chroot() returned %d. Errno is %d.\n", chroot_ret,
      errno);
  if (printed > 0 && printed < static_cast<ssize_t>(sizeof(kSecondMessage))) {
    std::ignore = write(STDERR_FILENO, kSecondMessage, printed);
  }
  errno = old_errno;
}

// This is a quick hack to allow testing sandbox crash reports in production
// binaries.
// This installs a signal handler for SIGUSR2 that performs a chroot().
// In most of our BPF policies, it is a "watched" system call which will
// trigger a SIGSYS signal whose handler will crash.
// This has been added during the investigation of https://crbug.com/415842.
void InstallCrashTestHandler() {
  struct sigaction act = {};
  act.sa_handler = DoChrootSignalHandler;
  CHECK_EQ(0, sigemptyset(&act.sa_mask));
  act.sa_flags = 0;

  PCHECK(0 == sigaction(SIGUSR2, &act, NULL));
}

bool IsSandboxDebuggingEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAllowSandboxDebugging);
}

}  // namespace

// static
bool SandboxDebugHandling::SetDumpableStatusAndHandlers() {
  if (IsSandboxDebuggingEnabled()) {
    // If sandbox debugging is allowed, install a handler for sandbox-related
    // crash testing.
    InstallCrashTestHandler();
    return true;
  }

  if (prctl(PR_SET_DUMPABLE, 0) != 0) {
    PLOG(ERROR) << "Failed to set non-dumpable flag";
    return false;
  }

  return prctl(PR_GET_DUMPABLE) == 0;
}

}  // namespace policy
}  // namespace sandbox
