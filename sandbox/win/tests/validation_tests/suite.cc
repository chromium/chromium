// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the validation tests for the sandbox.
// It includes the tests that need to be performed inside the
// sandbox.

#include <stddef.h>

#include "base/win/shlwapi.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/tests/common/controller.h"
#include "sandbox/win/tests/validation_tests/commands.h"
#include "testing/gtest/include/gtest/gtest.h"

// Callback that generates fresh TestRunners for process access tests.
typedef std::unique_ptr<sandbox::OpenProcessCmdTestRunner> (*RunnerGenerator)();

namespace {

void TestProcessAccess(RunnerGenerator runner_gen, DWORD target) {
  // Test all the scary process permissions.
  EXPECT_EQ(sandbox::SBOX_TEST_DENIED,
            runner_gen()->RunTest(target, PROCESS_CREATE_THREAD));

  EXPECT_EQ(sandbox::SBOX_TEST_DENIED,
            runner_gen()->RunTest(target, PROCESS_DUP_HANDLE));

  EXPECT_EQ(sandbox::SBOX_TEST_DENIED,
            runner_gen()->RunTest(target, PROCESS_SET_INFORMATION));

  EXPECT_EQ(sandbox::SBOX_TEST_DENIED,
            runner_gen()->RunTest(target, PROCESS_VM_OPERATION));

  EXPECT_EQ(sandbox::SBOX_TEST_DENIED,
            runner_gen()->RunTest(target, PROCESS_VM_READ));

  EXPECT_EQ(sandbox::SBOX_TEST_DENIED,
            runner_gen()->RunTest(target, PROCESS_VM_WRITE));

  EXPECT_EQ(sandbox::SBOX_TEST_DENIED,
            runner_gen()->RunTest(target, PROCESS_QUERY_INFORMATION));

  EXPECT_EQ(sandbox::SBOX_TEST_DENIED,
            runner_gen()->RunTest(target, WRITE_DAC));

  EXPECT_EQ(sandbox::SBOX_TEST_DENIED,
            runner_gen()->RunTest(target, WRITE_OWNER));

  EXPECT_EQ(sandbox::SBOX_TEST_DENIED,
            runner_gen()->RunTest(target, READ_CONTROL));
}

}  // namespace

namespace sandbox {

// Returns true if the volume that contains any_path supports ACL security. The
// input path can contain unexpanded environment strings. Returns false on any
// failure or if the file system does not support file security (such as FAT).
bool VolumeSupportsACLs(const wchar_t* any_path) {
  wchar_t expand[MAX_PATH +1];
  DWORD len =::ExpandEnvironmentStringsW(any_path, expand, _countof(expand));
  if (0 == len) return false;
  if (len >  _countof(expand)) return false;
  if (!::PathStripToRootW(expand)) return false;
  DWORD fs_flags = 0;
  if (!::GetVolumeInformationW(expand, NULL, 0, 0, NULL, &fs_flags, NULL, 0))
    return false;
  if (fs_flags & FILE_PERSISTENT_ACLS) return true;
  return false;
}

// Tests if the suite is working properly.
TEST(ValidationSuite, TestSuite) {
  PingCommandTestRunner runner;
  ASSERT_EQ(SBOX_TEST_PING_OK, runner.RunTest());
}

// Tests if the file system is correctly protected by the sandbox.
TEST(ValidationSuite, TestFileSystem) {
  // Do not perform the test if the system is using FAT or any other
  // file system that does not have file security.
  ASSERT_TRUE(VolumeSupportsACLs(L"%SystemDrive%\\"));
  ASSERT_TRUE(VolumeSupportsACLs(L"%SystemRoot%\\"));
  ASSERT_TRUE(VolumeSupportsACLs(L"%ProgramFiles%\\"));
  ASSERT_TRUE(VolumeSupportsACLs(L"%Temp%\\"));
  ASSERT_TRUE(VolumeSupportsACLs(L"%AppData%\\"));

  OpenFileCmdTestRunner runner_sysdrive;
  EXPECT_EQ(SBOX_TEST_DENIED, runner_sysdrive.RunTest(L"%SystemDrive%"));

  OpenFileCmdTestRunner runner_sysroot;
  EXPECT_EQ(SBOX_TEST_DENIED, runner_sysroot.RunTest(L"%SystemRoot%"));

  OpenFileCmdTestRunner runner_programfiles;
  EXPECT_EQ(SBOX_TEST_DENIED, runner_programfiles.RunTest(L"%ProgramFiles%"));

  OpenFileCmdTestRunner runner_system32;
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner_system32.RunTest(L"%SystemRoot%\\System32"));

  OpenFileCmdTestRunner runner_explorer;
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner_explorer.RunTest(L"%SystemRoot%\\explorer.exe"));

  OpenFileCmdTestRunner runner_cursors;
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner_cursors.RunTest(L"%SystemRoot%\\Cursors\\arrow_i.cur"));

  OpenFileCmdTestRunner runner_profiles;
  EXPECT_EQ(SBOX_TEST_DENIED, runner_profiles.RunTest(L"%AllUsersProfile%"));

  OpenFileCmdTestRunner runner_temp;
  EXPECT_EQ(SBOX_TEST_DENIED, runner_temp.RunTest(L"%Temp%"));

  OpenFileCmdTestRunner runner_appdata;
  EXPECT_EQ(SBOX_TEST_DENIED, runner_appdata.RunTest(L"%AppData%"));
}

// Tests if the registry is correctly protected by the sandbox.
TEST(ValidationSuite, TestRegistry) {
  OpenKeyTestRunner runner_hklm;
  EXPECT_EQ(SBOX_TEST_DENIED, runner_hklm.RunTest(L"HKLM"));

  OpenKeyTestRunner runner_hkcu;
  EXPECT_EQ(SBOX_TEST_DENIED, runner_hkcu.RunTest(L"HKCU"));

  OpenKeyTestRunner runner_hku;
  EXPECT_EQ(SBOX_TEST_DENIED, runner_hku.RunTest(L"HKU"));

  OpenKeyTestRunner runner_hklm_key;
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner_hklm_key.RunTest(
                L"HKLM",
                L"Software\\Microsoft\\Windows NT\\CurrentVersion\\WinLogon"));
}

template <typename Runner>
std::unique_ptr<Runner> DesktopRunner() {
  auto runner = std::make_unique<Runner>();
  EXPECT_EQ(SBOX_ALL_OK, runner->broker()->CreateAlternateDesktop(
                             Desktop::kAlternateWinstation));
  runner->GetConfig()->SetDesktop(Desktop::kAlternateWinstation);
  EXPECT_EQ(SBOX_ALL_OK,
            runner->GetConfig()->SetIntegrityLevel(INTEGRITY_LEVEL_LOW));
  return runner;
}

// Tests that the permissions on the Windowstation does not allow the sandbox
// to get to the interactive desktop or to make the sbox desktop interactive.
TEST(ValidationSuite, TestDesktop) {
  EXPECT_EQ(SBOX_TEST_DENIED,
            DesktopRunner<OpenInteractiveDesktopTestRunner>()->RunTest());
  EXPECT_EQ(SBOX_TEST_DENIED,
            DesktopRunner<SwitchToSboxDesktopTestRunner>()->RunTest());
}

// Tests that the permissions on the Windowstation does not allow the sandbox
// to get to the interactive desktop or to make the sbox desktop interactive.
TEST(ValidationSuite, TestAlternateDesktop) {
  EnumAlternateWinstaTestRunner runner_no_policy;
  EXPECT_EQ(SBOX_TEST_DENIED, runner_no_policy.RunTest());

  OpenAlternateDesktopTestRunner runner;
  runner.SetTimeout(3600000);
  EXPECT_EQ(SBOX_ALL_OK, runner.broker()->CreateAlternateDesktop(
                             Desktop::kAlternateWinstation));
  runner.GetPolicy()->GetConfig()->SetDesktop(Desktop::kAlternateWinstation);
  EXPECT_EQ(SBOX_ALL_OK, runner.GetPolicy()->GetConfig()->SetIntegrityLevel(
                             INTEGRITY_LEVEL_LOW));
  // Ensure the desktop is created.
  EXPECT_EQ(SBOX_ALL_OK, runner.broker()->CreateAlternateDesktop(
                             Desktop::kAlternateWinstation));
  std::wstring desktop_name =
      runner.broker()->GetDesktopName(Desktop::kAlternateWinstation);
  desktop_name = desktop_name.substr(desktop_name.find('\\') + 1);
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(desktop_name));
}

template <typename Runner>
std::unique_ptr<Runner> AlternateDesktopLocalWinstationRunner() {
  auto runner = std::make_unique<Runner>();
  EXPECT_EQ(SBOX_ALL_OK, runner->broker()->CreateAlternateDesktop(
                             Desktop::kAlternateDesktop));
  runner->GetConfig()->SetDesktop(Desktop::kAlternateDesktop);
  EXPECT_EQ(SBOX_ALL_OK,
            runner->GetConfig()->SetIntegrityLevel(INTEGRITY_LEVEL_LOW));
  return runner;
}

// Same as TestDesktop, but uses the local winstation, instead of an alternate
// one.
TEST(ValidationSuite, TestAlternateDesktopLocalWinstation) {
  EXPECT_EQ(
      SBOX_TEST_DENIED,
      AlternateDesktopLocalWinstationRunner<OpenInteractiveDesktopTestRunner>()
          ->RunTest());
  EXPECT_EQ(
      SBOX_TEST_DENIED,
      AlternateDesktopLocalWinstationRunner<SwitchToSboxDesktopTestRunner>()
          ->RunTest());
}

// Tests if the windows are correctly protected by the sandbox.
TEST(ValidationSuite, TestWindows) {
  // Due to a bug in Windows on builds based on the 19041 branch (20H1, 20H2,
  // 21H1 and 22H2) this test will fail on these versions. See
  // crbug.com/1057656.
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  if (os_info->version_number().build >= 19041 &&
      os_info->version_number().build <= 19045) {
    GTEST_SKIP() << "Skipping test for Win10 19041 branch, crbug.com/1057656.";
  }

  ValidWindowTestRunner runner_getshellwindow;
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner_getshellwindow.RunTest(
                reinterpret_cast<size_t>(::GetShellWindow())));

  ValidWindowTestRunner runner_findwindow;
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner_findwindow.RunTest(
                reinterpret_cast<size_t>(::FindWindow(NULL, NULL))));
}

std::unique_ptr<OpenProcessCmdTestRunner> ProcessDenyLockdownRunner() {
  return std::make_unique<OpenProcessCmdTestRunner>();
}

// Tests that a locked-down process cannot open another locked-down process.
TEST(ValidationSuite, TestProcessDenyLockdown) {
  SleepCmdTestRunner target;
  base::Process process = target.RunTestAsync(30000);

  TestProcessAccess(ProcessDenyLockdownRunner, process.Pid());
  EXPECT_TRUE(process.Terminate(0, true));
}

std::unique_ptr<OpenProcessCmdTestRunner> ProcessDenyLowIntegrityRunner() {
  auto runner = std::make_unique<OpenProcessCmdTestRunner>();
  EXPECT_EQ(SBOX_ALL_OK, runner->GetPolicy()->GetConfig()->SetIntegrityLevel(
                             INTEGRITY_LEVEL_LOW));
  EXPECT_EQ(SBOX_ALL_OK, runner->GetPolicy()->GetConfig()->SetTokenLevel(
                             USER_RESTRICTED_SAME_ACCESS, USER_INTERACTIVE));
  return runner;
}

// Tests that a low-integrity process cannot open a locked-down process (due
// to the integrity label changing after startup via SetDelayedIntegrityLevel).
TEST(ValidationSuite, TestProcessDenyLowIntegrity) {
  SleepCmdTestRunner target;
  target.GetPolicy()->GetConfig()->SetDelayedIntegrityLevel(
      INTEGRITY_LEVEL_LOW);

  base::Process process = target.RunTestAsync(30000);

  TestProcessAccess(ProcessDenyLowIntegrityRunner, process.Pid());
  EXPECT_TRUE(process.Terminate(0, true));
}

std::unique_ptr<OpenProcessCmdTestRunner> ProcessDenyBelowLowIntegrityRunner() {
  auto runner = std::make_unique<OpenProcessCmdTestRunner>();
  runner->GetPolicy()->GetConfig()->SetDelayedIntegrityLevel(
      INTEGRITY_LEVEL_UNTRUSTED);
  EXPECT_EQ(SBOX_ALL_OK, runner->GetPolicy()->GetConfig()->SetTokenLevel(
                             USER_RESTRICTED_SAME_ACCESS, USER_INTERACTIVE));
  return runner;
}

// Tests that a locked-down process cannot open a low-integrity process.
TEST(ValidationSuite, TestProcessDenyBelowLowIntegrity) {
  SleepCmdTestRunner target;
  EXPECT_EQ(SBOX_ALL_OK, target.GetPolicy()->GetConfig()->SetIntegrityLevel(
                             INTEGRITY_LEVEL_LOW));
  EXPECT_EQ(SBOX_ALL_OK, target.GetPolicy()->GetConfig()->SetTokenLevel(
                             USER_RESTRICTED_SAME_ACCESS, USER_INTERACTIVE));

  base::Process process = target.RunTestAsync(30000);

  TestProcessAccess(ProcessDenyBelowLowIntegrityRunner, process.Pid());
  EXPECT_TRUE(process.Terminate(0, true));
}

// Tests if the threads are correctly protected by the sandbox.
TEST(ValidationSuite, TestThread) {
  OpenThreadCmdTestRunner runner;
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(::GetCurrentThreadId()));
}

// Tests if an over-limit allocation will be denied.
TEST(ValidationSuite, TestMemoryLimit) {
  AllocateCmdTestRunner runner;
  const int kAllocationSize = 256 * 1024 * 1024;

  runner.GetPolicy()->GetConfig()->SetJobMemoryLimit(kAllocationSize);
  EXPECT_EQ(SBOX_FATAL_MEMORY_EXCEEDED, runner.RunTest(kAllocationSize));
}

// Tests a large allocation will succeed absent limits.
TEST(ValidationSuite, TestMemoryNoLimit) {
  AllocateCmdTestRunner runner;
  const int kAllocationSize = 256 * 1024 * 1024;

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(kAllocationSize));
}

// Tests that the InitCompleted API works correctly in various states.
TEST(ValidationSuite, TestInitCompleted) {
  {
    InitCompletedTestRunner runner;
    runner.SetTestState(BEFORE_INIT);
    EXPECT_EQ(SBOX_TEST_FIRST_ERROR, runner.RunTest());
  }
  {
    InitCompletedTestRunner runner;
    runner.SetTestState(AFTER_REVERT);
    EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest());
  }
}

}  // namespace sandbox
