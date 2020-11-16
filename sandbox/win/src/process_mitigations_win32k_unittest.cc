// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_mitigations.h"

#include <windows.h>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/process_mitigations_win32k_policy.h"
#include "sandbox/win/tests/common/controller.h"
#include "sandbox/win/tests/integration_tests/integration_tests_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

//------------------------------------------------------------------------------
// Exported Win32k Lockdown Tests
//------------------------------------------------------------------------------

// This test validates that setting the MITIGATION_WIN32K_DISABLE mitigation on
// the target process causes the launch to fail in process initialization.
// The test process itself links against user32/gdi32.
TEST(ProcessMitigationsWin32kTest, CheckWin8LockDownFailure) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  std::wstring test_policy_command = L"CheckPolicy ";
  test_policy_command += std::to_wstring(TESTPOLICY_WIN32K);

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(policy->SetProcessMitigations(MITIGATION_WIN32K_DISABLE),
            SBOX_ALL_OK);
  EXPECT_NE(SBOX_TEST_SUCCEEDED, runner.RunTest(test_policy_command.c_str()));
}

// This test validates that setting the MITIGATION_WIN32K_DISABLE mitigation
// along with the policy to fake user32 and gdi32 initialization successfully
// launches the target process.
// The test process itself links against user32/gdi32.

TEST(ProcessMitigationsWin32kTest, CheckWin8LockDownSuccess) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  std::wstring test_policy_command = L"CheckPolicy ";
  test_policy_command += std::to_wstring(TESTPOLICY_WIN32K);

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();
  EXPECT_EQ(policy->SetProcessMitigations(MITIGATION_WIN32K_DISABLE),
            SBOX_ALL_OK);
  EXPECT_EQ(policy->AddRule(sandbox::TargetPolicy::SUBSYS_WIN32K_LOCKDOWN,
                            sandbox::TargetPolicy::FAKE_USER_GDI_INIT, nullptr),
            sandbox::SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_policy_command.c_str()));
}

}  // namespace sandbox
