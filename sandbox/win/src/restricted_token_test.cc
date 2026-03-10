// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Integration tests for restricted tokens.

#include <stddef.h>

#include <optional>
#include <string>

#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/access_token.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/target_services.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

// Declare external command.
SBOX_TEST_DECLARE_COMMAND(IntegrationTestsTest_args);

// Opens a process based on a PID and access mask passed on the command line.
// Returns SBOX_TEST_SUCCEEDED if process opened successfully.
SBOX_TEST_COMMAND(RestrictedTokenTest_openprocess) {
  if (args.size() < 2) {
    return SBOX_TEST_NOT_FOUND;
  }
  unsigned int pid;
  if (!base::StringToUint(args[0], &pid) || pid == 0) {
    return SBOX_TEST_NOT_FOUND;
  }
  unsigned int desired_access;
  if (!base::StringToUint(args[1], &desired_access)) {
    return SBOX_TEST_INVALID_PARAMETER;
  }

  base::win::ScopedHandle process_handle(
      ::OpenProcess(desired_access, false, pid));
  if (process_handle.is_valid()) {
    return SBOX_TEST_SUCCEEDED;
  }

  return SBOX_TEST_DENIED;
}

// Opens a process through duplication. This is to avoid the OpenProcess hook.
SBOX_TEST_COMMAND(RestrictedTokenTest_currentprocess_dup) {
  if (args.size() < 1) {
    return SBOX_TEST_NOT_FOUND;
  }
  unsigned int desired_access;
  if (!base::StringToUint(args[0], &desired_access)) {
    return SBOX_TEST_INVALID_PARAMETER;
  }

  HANDLE dup_handle;
  if (!::DuplicateHandle(::GetCurrentProcess(), ::GetCurrentProcess(),
                         ::GetCurrentProcess(), &dup_handle, 0, FALSE, 0)) {
    return SBOX_TEST_FIRST_ERROR;
  }
  base::win::ScopedHandle process_handle(dup_handle);
  if (::DuplicateHandle(::GetCurrentProcess(), process_handle.get(),
                        ::GetCurrentProcess(), &dup_handle, desired_access,
                        FALSE, 0)) {
    ::CloseHandle(dup_handle);
    return SBOX_TEST_SUCCEEDED;
  }

  if (::GetLastError() != ERROR_ACCESS_DENIED) {
    return SBOX_TEST_SECOND_ERROR;
  }
  return SBOX_TEST_DENIED;
}

// Opens a the process token and checks if it's restricted.
SBOX_TEST_COMMAND(RestrictedTokenTest_IsRestricted) {
  std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromCurrentProcess();
  if (!token) {
    return SBOX_TEST_FIRST_ERROR;
  }
  return token->RestrictedSids().size() > 0 ? SBOX_TEST_SUCCEEDED
                                            : SBOX_TEST_FAILED;
}

namespace {

int RunOpenProcessTest(bool unsandboxed,
                       bool lockdown_dacl,
                       DWORD access_mask) {
  IntegrationTestsTest_argsTestRunner runner(
      JobLevel::kUnprotected, USER_RESTRICTED_SAME_ACCESS, USER_LOCKDOWN);
  auto* config = runner.GetPolicy()->GetConfig();
  config->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_UNTRUSTED);
  ResultCode result = config->SetIntegrityLevel(INTEGRITY_LEVEL_LOW);
  if (result != SBOX_ALL_OK) {
    return SBOX_TEST_FAILED_SETUP;
  }
  if (lockdown_dacl) {
    config->SetLockdownDefaultDacl();
  }

  // This spins up a renderer level process, we don't care about the result.
  base::Process process = runner.RunTestAsync(1);
  if (!process.IsValid()) {
    return SBOX_TEST_FAILED_SETUP;
  }

  RestrictedTokenTest_openprocessTestRunner runner2(
      JobLevel::kUnprotected, USER_RESTRICTED_SAME_ACCESS, USER_LIMITED);
  auto* config2 = runner2.GetPolicy()->GetConfig();
  config2->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_LOW);
  result = config2->SetIntegrityLevel(INTEGRITY_LEVEL_LOW);
  if (result != SBOX_ALL_OK) {
    return SBOX_TEST_FAILED_SETUP;
  }
  runner2.SetUnsandboxed(unsandboxed);
  return runner2.RunTest(process.Pid(), access_mask);
}

int RunRestrictedOpenProcessTest(bool unsandboxed,
                                 bool lockdown_dacl,
                                 DWORD access_mask) {
  IntegrationTestsTest_argsTestRunner runner(
      JobLevel::kUnprotected, USER_RESTRICTED_SAME_ACCESS, USER_LIMITED);
  auto* config = runner.GetPolicy()->GetConfig();
  config->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_LOW);
  ResultCode result = config->SetIntegrityLevel(INTEGRITY_LEVEL_LOW);
  if (result != SBOX_ALL_OK) {
    return SBOX_TEST_FAILED_SETUP;
  }

  if (lockdown_dacl) {
    config->SetLockdownDefaultDacl();
    config->AddRestrictingRandomSid();
  }

  // This spins up a GPU level process, we don't care about the result.
  base::Process process = runner.RunTestAsync(1);
  if (!process.IsValid()) {
    return SBOX_TEST_FAILED_SETUP;
  }

  RestrictedTokenTest_openprocessTestRunner runner2(
      JobLevel::kUnprotected, USER_RESTRICTED_SAME_ACCESS, USER_LIMITED);
  auto* config2 = runner2.GetPolicy()->GetConfig();
  config2->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_LOW);
  result = config2->SetIntegrityLevel(INTEGRITY_LEVEL_LOW);
  if (result != SBOX_ALL_OK) {
    return SBOX_TEST_FAILED_SETUP;
  }
  runner2.SetUnsandboxed(unsandboxed);
  return runner2.RunTest(process.Pid(), access_mask);
}

int RunRestrictedSelfOpenProcessTest(bool add_random_sid, DWORD access_mask) {
  RestrictedTokenTest_currentprocess_dupTestRunner runner(
      JobLevel::kUnprotected, USER_RESTRICTED_SAME_ACCESS, USER_LIMITED);
  auto* config = runner.GetPolicy()->GetConfig();
  config->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_LOW);
  ResultCode result = config->SetIntegrityLevel(INTEGRITY_LEVEL_LOW);
  if (result != SBOX_ALL_OK) {
    return SBOX_TEST_FAILED_SETUP;
  }
  config->SetLockdownDefaultDacl();
  if (add_random_sid) {
    config->AddRestrictingRandomSid();
  }

  return runner.RunTest(access_mask);
}

}  // namespace

TEST(RestrictedTokenTest, OpenLowPrivilegedProcess) {
  // Test limited privilege to renderer open.
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            RunOpenProcessTest(false, false, GENERIC_READ | GENERIC_WRITE));
  // Test limited privilege to renderer open with lockdowned DACL.
  ASSERT_EQ(SBOX_TEST_DENIED,
            RunOpenProcessTest(false, true, GENERIC_READ | GENERIC_WRITE));
  // Ensure we also can't get any access to the process.
  ASSERT_EQ(SBOX_TEST_DENIED, RunOpenProcessTest(false, true, MAXIMUM_ALLOWED));
  // Also check for explicit owner allowed WRITE_DAC right.
  ASSERT_EQ(SBOX_TEST_DENIED, RunOpenProcessTest(false, true, WRITE_DAC));
  // Ensure unsandboxed process can still open the renderer for all access.
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            RunOpenProcessTest(true, true, PROCESS_ALL_ACCESS));
}

TEST(RestrictedTokenTest, CheckNonAdminRestricted) {
  RestrictedTokenTest_IsRestrictedTestRunner runner(JobLevel::kUnprotected,
                                                    USER_RESTRICTED_SAME_ACCESS,
                                                    USER_RESTRICTED_NON_ADMIN);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest());
}

TEST(RestrictedTokenTest, OpenProcessSameSandboxRandomSid) {
  // Test process to process open when not using random SID.
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            RunRestrictedOpenProcessTest(false, false, GENERIC_ALL));
  // Test process to process open when using random SID.
  ASSERT_EQ(SBOX_TEST_DENIED,
            RunRestrictedOpenProcessTest(false, true, MAXIMUM_ALLOWED));
  // Test process to process open when not using random SID and opening from
  // unsandboxed.
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            RunRestrictedOpenProcessTest(true, false, GENERIC_ALL));
  // Test process to process open when using random SID and opening from
  // unsandboxed.
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            RunRestrictedOpenProcessTest(true, true, GENERIC_ALL));
}

TEST(RestrictedTokenTest, OpenProcessSelfRandomSid) {
  // Test process can't open self when not using random SID.
  ASSERT_EQ(SBOX_TEST_DENIED,
            RunRestrictedSelfOpenProcessTest(false, PROCESS_ALL_ACCESS));
  // Test process can open self when using random SID.
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            RunRestrictedSelfOpenProcessTest(true, PROCESS_ALL_ACCESS));
}

}  // namespace sandbox
