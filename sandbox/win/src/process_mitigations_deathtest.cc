// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/win/src/process_mitigations.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_number_conversions_win.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {
enum MethodCall { kRatchetDown = 1, kSetStart, kLockdown };
}
//------------------------------------------------------------------------------
// Exported functions called by child test processes.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Common test function for checking that a policy was enabled.
// - Use enum TestPolicy defined in integration_tests_common.h to specify which
//   policy to check - passed as arg1.
//------------------------------------------------------------------------------
SBOX_TESTS_COMMAND int CheckDeath(int argc, wchar_t** argv) {
  if (argc < 2)
    return SBOX_TEST_INVALID_PARAMETER;

  for (int i = 0; i < argc; i++) {
    int test;
    if (!base::StringToInt(argv[i], &test)) {
      return SBOX_TEST_INVALID_PARAMETER;
    }

    MethodCall call = static_cast<MethodCall>(test);
    switch (call) {
      case kRatchetDown: {
        RatchetDownSecurityMitigations(sandbox::MITIGATION_RELOCATE_IMAGE);
        break;
      }
      case kSetStart: {
        SetStartingMitigations(sandbox::MITIGATION_RELOCATE_IMAGE);
        break;
      }
      case kLockdown: {
        LockDownSecurityMitigations(sandbox::MITIGATION_RELOCATE_IMAGE);
        break;
      }
    }
  }

  // We should crash before we get here
  return SBOX_TEST_FAILED;
}

//------------------------------------------------------------------------------
// Common test function to run a command and ensure it DCHECKs.
//------------------------------------------------------------------------------
void RunCommandAndExpectDCHECK(const std::wstring& test_command) {
  TestRunner runner;
  // We should hit a DCHECK in the child which will cause the process to crash
  // STATUS_STACK_BUFFER_OVERRUN is either caused by TRAP_SEQUENCE or
  // abort within base/immediate_crash.h
  EXPECT_EQ(STATUS_STACK_BUFFER_OVERRUN,
            static_cast<ULONG>(runner.RunTest(test_command.c_str())));
}

TEST(ProcessMitigationsDeathTest, CheckRatchetDownOrderMatters) {
  std::wstring test_command = L"CheckDeath ";
  test_command += base::NumberToWString(kRatchetDown);
  test_command += L" ";
  test_command += base::NumberToWString(kSetStart);
  RunCommandAndExpectDCHECK(test_command);
}

TEST(ProcessMitigationsDeathTest, CheckRatchetDownAndLockdownExclusive) {
  std::wstring test_command = L"CheckDeath ";
  test_command += base::NumberToWString(kRatchetDown);
  test_command += L" ";
  test_command += base::NumberToWString(kLockdown);
  RunCommandAndExpectDCHECK(test_command);
}

TEST(ProcessMitigationsDeathTest, CheckRatchetDownAndLockdownExclusive2) {
  std::wstring test_command = L"CheckDeath ";
  test_command += base::NumberToWString(kLockdown);
  test_command += L" ";
  test_command += base::NumberToWString(kRatchetDown);
  RunCommandAndExpectDCHECK(test_command);
}

TEST(ProcessMitigationsDeathTest, CheckSetStartAndLockdownExclusive) {
  std::wstring test_command = L"CheckDeath ";
  test_command += base::NumberToWString(kLockdown);
  test_command += L" ";
  test_command += base::NumberToWString(kSetStart);
  RunCommandAndExpectDCHECK(test_command);
}

TEST(ProcessMitigationsDeathTest, CheckSetStartAndLockdownExclusive2) {
  std::wstring test_command = L"CheckDeath ";
  test_command += base::NumberToWString(kSetStart);
  test_command += L" ";
  test_command += base::NumberToWString(kLockdown);
  RunCommandAndExpectDCHECK(test_command);
}

}  // namespace sandbox
