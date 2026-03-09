// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <winternl.h>

#include <ntstatus.h>

#include "base/win/windows_version.h"
#include "sandbox/win/src/target_services.h"
#include "sandbox/win/tests/common/controller.h"
#include "sandbox/win/tests/integration_tests/integration_tests_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace {
#if defined(_WIN64)
// This is the same on x64 and arm64.
constexpr ptrdiff_t kShimDataOffset = 0x2d8;
#else
constexpr ptrdiff_t kShimDataOffset = 0x1e8;
#endif
}  // namespace

// Validate that we can zero the member and still execute.
SBOX_TEST_COMMAND(ZeroAppShimCommand) {
  PROCESS_BASIC_INFORMATION info = {};
  NTSTATUS status =
      ::NtQueryInformationProcess(GetCurrentProcess(), ProcessBasicInformation,
                                  &info, sizeof(info), nullptr);
  if (STATUS_SUCCESS != status) {
    return SBOX_TEST_FAILED;
  }

  void** ppShimData = reinterpret_cast<void**>(
      reinterpret_cast<uintptr_t>(info.PebBaseAddress) + kShimDataOffset);

  if (*ppShimData) {
    return SBOX_TEST_FAILED;
  }

  return SBOX_TEST_SUCCEEDED;
}

// This test validates that writing zero to the pShimData member of the child's
// PEB works.
TEST(ZeroAppShimTest, ZeroAppShim) {
  if (!base::win::OSInfo::GetInstance()->IsWowDisabled() ||
      base::win::OSInfo::IsRunningEmulatedOnArm64()) {
    GTEST_SKIP() << "ZeroAppShim not supported in WoW or ARM64 emulated modes.";
  }

  ZeroAppShimCommandTestRunner runner;
  runner.GetConfig()->SetZeroAppShim();

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest());
}

}  // namespace sandbox
