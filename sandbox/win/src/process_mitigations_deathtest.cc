// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_number_conversions_win.h"
#include "sandbox/win/src/process_mitigations.h"
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
SBOX_TEST_COMMAND(CheckDeath) {
  if (args.size() < 2) {
    return SBOX_TEST_INVALID_PARAMETER;
  }

  for (const auto& arg : args) {
    int test;
    if (!base::StringToInt(arg, &test)) {
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

TEST(ProcessMitigationsDeathTest, CheckRatchetDownOrderMatters) {
  CheckDeathTestRunner runner;
  // We should hit a DCHECK in the child which will cause the process to crash
  EXPECT_EQ(STATUS_BREAKPOINT,
            static_cast<ULONG>(runner.RunTest(kRatchetDown, kSetStart)));
}

TEST(ProcessMitigationsDeathTest, CheckRatchetDownAndLockdownExclusive) {
  CheckDeathTestRunner runner;
  // We should hit a DCHECK in the child which will cause the process to crash
  EXPECT_EQ(STATUS_BREAKPOINT,
            static_cast<ULONG>(runner.RunTest(kRatchetDown, kLockdown)));
}

TEST(ProcessMitigationsDeathTest, CheckRatchetDownAndLockdownExclusive2) {
  CheckDeathTestRunner runner;
  // We should hit a DCHECK in the child which will cause the process to crash
  EXPECT_EQ(STATUS_BREAKPOINT,
            static_cast<ULONG>(runner.RunTest(kLockdown, kRatchetDown)));
}

TEST(ProcessMitigationsDeathTest, CheckSetStartAndLockdownExclusive) {
  CheckDeathTestRunner runner;
  // We should hit a DCHECK in the child which will cause the process to crash
  EXPECT_EQ(STATUS_BREAKPOINT,
            static_cast<ULONG>(runner.RunTest(kLockdown, kSetStart)));
}

TEST(ProcessMitigationsDeathTest, CheckSetStartAndLockdownExclusive2) {
  CheckDeathTestRunner runner;
  // We should hit a DCHECK in the child which will cause the process to crash
  EXPECT_EQ(STATUS_BREAKPOINT,
            static_cast<ULONG>(runner.RunTest(kSetStart, kLockdown)));
}

}  // namespace sandbox
