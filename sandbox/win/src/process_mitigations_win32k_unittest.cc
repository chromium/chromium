// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <string>

#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/process_mitigations_win32k_policy.h"
#include "sandbox/win/src/sandbox_policy.h"
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
  std::wstring test_policy_command = L"CheckPolicy ";
  test_policy_command += std::to_wstring(TESTPOLICY_WIN32K);

  TestRunner runner;
  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();

  EXPECT_EQ(config->SetProcessMitigations(MITIGATION_WIN32K_DISABLE),
            SBOX_ALL_OK);
  EXPECT_NE(SBOX_TEST_SUCCEEDED, runner.RunTest(test_policy_command.c_str()));
}

// This test validates that setting the MITIGATION_WIN32K_DISABLE mitigation
// along with the policy to fake user32 and gdi32 initialization successfully
// launches the target process.
// The test process itself links against user32/gdi32.

TEST(ProcessMitigationsWin32kTest, CheckWin8LockDownSuccess) {
  std::wstring test_policy_command = L"CheckPolicy ";
  test_policy_command += std::to_wstring(TESTPOLICY_WIN32K);

  TestRunner runner;
  runner.SetTestState(sandbox::EVERY_STATE);

  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();
  EXPECT_EQ(config->SetProcessMitigations(MITIGATION_WIN32K_DISABLE),
            SBOX_ALL_OK);
  EXPECT_EQ(config->SetFakeGdiInit(), sandbox::SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_policy_command.c_str()));
}

// This test validates the MITIGATION_WIN32K_DISABLE works without the
// SetFakeGdiInit() interceptions that allow gdi32.dll and user32.dll to load.
TEST(ProcessMitigationsWin32kTest,
     CheckWin32kLockDownSuccessWithoutFakeGdiInit) {
  // Component build dlls statically link in gdi32 and user32 for convenience.
#if !defined(COMPONENT_BUILD)
  std::wstring test_policy_command = L"CheckPolicy ";
  test_policy_command += std::to_wstring(TESTPOLICY_WIN32K_NOFAKEGDI);

  TestRunner runner;
  runner.SetTestState(sandbox::EVERY_STATE);

  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();
  EXPECT_EQ(config->SetProcessMitigations(MITIGATION_WIN32K_DISABLE),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_policy_command.c_str()));
#endif
}

}  // namespace sandbox
