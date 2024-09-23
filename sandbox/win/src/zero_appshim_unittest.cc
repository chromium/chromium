// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <ntstatus.h>

#include "base/win/windows_version.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_nt_util.h"
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
SBOX_TESTS_COMMAND int ZeroAppShimCommand(int argc, wchar_t** argv) {
  PROCESS_BASIC_INFORMATION info = {};
  NTSTATUS status = GetNtExports()->QueryInformationProcess(
      GetCurrentProcess(), ProcessBasicInformation, &info, sizeof(info),
      nullptr);
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

  std::wstring test_command = L"ZeroAppShimCommand";
  TestRunner runner;
  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();

  config->SetZeroAppShim();

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));
}

}  // namespace sandbox
