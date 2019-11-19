// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "sandbox/linux/services/scoped_process.h"
#include "sandbox/linux/services/yama.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

bool HasLinux32Bug() {
#if defined(__i386__)
  // On 3.2 kernels, yama doesn't work for 32-bit binaries on 64-bit kernels.
  // This is fixed in 3.4.
  bool is_kernel_64bit =
      base::SysInfo::OperatingSystemArchitecture() == "x86_64";
  bool is_linux = base::SysInfo::OperatingSystemName() == "Linux";
  bool is_3_dot_2 = base::StartsWith(
      base::SysInfo::OperatingSystemVersion(), "3.2",
      base::CompareCase::INSENSITIVE_ASCII);
  if (is_kernel_64bit && is_linux && is_3_dot_2)
    return true;
#endif  // defined(__i386__)
  return false;
}

bool CanPtrace(pid_t pid) {
  int ret;
  ret = ptrace(PTRACE_ATTACH, pid, NULL, NULL);
  if (ret == -1) {
    CHECK_EQ(EPERM, errno);
    return false;
  }
  // Wait for the process to be stopped so that it can be detached.
  siginfo_t process_info;
  int wait_ret = HANDLE_EINTR(waitid(P_PID, pid, &process_info, WSTOPPED));
  PCHECK(0 == wait_ret);
  PCHECK(0 == ptrace(PTRACE_DETACH, pid, NULL, NULL));
  return true;
}

// _exit(0) if pid can be ptraced by the current process.
// _exit(1) otherwise.
void ExitZeroIfCanPtrace(pid_t pid) {
  if (CanPtrace(pid)) {
    _exit(0);
  } else {
    _exit(1);
  }
}

bool CanSubProcessPtrace(pid_t pid) {
  ScopedProcess process(base::BindOnce(&ExitZeroIfCanPtrace, pid));
  bool signaled;
  int exit_code = process.WaitForExit(&signaled);
  CHECK(!signaled);
  return 0 == exit_code;
}

// The tests below assume that the system-level configuration will not change
// while they run.

TEST(Yama, GetStatus) {
  int status1 = Yama::GetStatus();

  // Check that the value is a possible bitmask.
  ASSERT_LE(0, status1);
  ASSERT_GE(Yama::STATUS_KNOWN | Yama::STATUS_PRESENT | Yama::STATUS_ENFORCING |
                Yama::STATUS_STRICT_ENFORCING,
            status1);

  // The status should not just be a random value.
  int status2 = Yama::GetStatus();
  EXPECT_EQ(status1, status2);

  // This test is not running sandboxed, there is no reason to not know the
  // status.
  EXPECT_NE(0, Yama::STATUS_KNOWN & status1);

  if (status1 & Yama::STATUS_STRICT_ENFORCING) {
    // If Yama is strictly enforcing, it is also enforcing.
    EXPECT_TRUE(status1 & Yama::STATUS_ENFORCING);
  }

  if (status1 & Yama::STATUS_ENFORCING) {
    // If Yama is enforcing, Yama is present.
    EXPECT_NE(0, status1 & Yama::STATUS_PRESENT);
  }

  // Verify that the helper functions work as intended.
  EXPECT_EQ(static_cast<bool>(status1 & Yama::STATUS_ENFORCING),
            Yama::IsEnforcing());
  EXPECT_EQ(static_cast<bool>(status1 & Yama::STATUS_PRESENT),
            Yama::IsPresent());

  fprintf(stdout,
          "Yama present: %s - enforcing: %s\n",
          Yama::IsPresent() ? "Y" : "N",
          Yama::IsEnforcing() ? "Y" : "N");
}

SANDBOX_TEST(Yama, RestrictPtraceSucceedsWhenYamaPresent) {
  // This call will succeed iff Yama is present.
  bool restricted = Yama::RestrictPtracersToAncestors();
  CHECK_EQ(restricted, Yama::IsPresent());
}

// Attempts to enable or disable Yama restrictions.
void SetYamaRestrictions(bool enable_restriction) {
  if (enable_restriction) {
    Yama::RestrictPtracersToAncestors();
  } else {
    Yama::DisableYamaRestrictions();
  }
}

TEST(Yama, RestrictPtraceWorks) {
  if (HasLinux32Bug())
    return;

  ScopedProcess process1(base::BindOnce(&SetYamaRestrictions, true));
  ASSERT_TRUE(process1.WaitForClosureToRun());

  if (Yama::IsEnforcing()) {
    // A sibling process cannot ptrace process1.
    ASSERT_FALSE(CanSubProcessPtrace(process1.GetPid()));
  }

  if (!(Yama::GetStatus() & Yama::STATUS_STRICT_ENFORCING)) {
    // However, parent can ptrace process1.
    ASSERT_TRUE(CanPtrace(process1.GetPid()));

    // A sibling can ptrace process2 which disables any Yama protection.
    ScopedProcess process2(base::BindOnce(&SetYamaRestrictions, false));
    ASSERT_TRUE(process2.WaitForClosureToRun());
    ASSERT_TRUE(CanSubProcessPtrace(process2.GetPid()));
  }
}

SANDBOX_TEST(Yama, RestrictPtraceIsDefault) {
  if (!Yama::IsPresent() || HasLinux32Bug())
    return;

  CHECK(Yama::DisableYamaRestrictions());
  ScopedProcess process1{base::DoNothing()};

  if (Yama::IsEnforcing()) {
    // Check that process1 is protected by Yama, even though it has
    // been created from a process that disabled Yama.
    CHECK(!CanSubProcessPtrace(process1.GetPid()));
  }
}

}  // namespace

}  // namespace sandbox
