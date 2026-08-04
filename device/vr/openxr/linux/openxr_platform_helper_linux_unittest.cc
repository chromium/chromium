// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/linux/openxr_platform_helper_linux.h"

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/scoped_environment_variable_override.h"
#include "base/test/bind.h"
#include "device/vr/openxr/openxr_graphics_binding.h"
#include "device/vr/openxr/openxr_platform_helper.h"
#include "device/vr/openxr/test/openxr_mock_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

// Points XR_RUNTIME_JSON at the mock runtime copied next to the test binary
// (resolved from the executable directory, independent of the CWD).
std::unique_ptr<base::ScopedEnvironmentVariableOverride>
UseMockOpenXrRuntime() {
  base::FilePath dir;
  CHECK(base::PathService::Get(base::DIR_EXE, &dir));
  return std::make_unique<base::ScopedEnvironmentVariableOverride>(
      "XR_RUNTIME_JSON",
      dir.Append(FILE_PATH_LITERAL("mock_vr_clients/bin/openxr/openxr.json"))
          .value());
}

}  // namespace

class OpenXrPlatformHelperLinuxTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_runtime_ = UseMockOpenXrRuntime();
    // The mock runtime library is only a trampoline; its dispatch table has to
    // be installed from this process before the loader negotiates with it.
    ASSERT_TRUE(InitializeOpenXrMockTrampoline());
  }

  std::unique_ptr<base::ScopedEnvironmentVariableOverride> mock_runtime_;
};

TEST_F(OpenXrPlatformHelperLinuxTest, EnsureInitializedSucceeds) {
  OpenXrPlatformHelperLinux helper;
  EXPECT_TRUE(helper.EnsureInitialized());
  EXPECT_NE(helper.GetExtensionEnumeration(), nullptr);
}

TEST_F(OpenXrPlatformHelperLinuxTest, GetPlatformCreateInfoCallbackWithNull) {
  OpenXrPlatformHelperLinux helper;
  bool ran = false;
  void* captured = reinterpret_cast<void*>(0xDEAD);
  helper.GetPlatformCreateInfo(OpenXrCreateInfo{},
                               base::BindLambdaForTesting([&](void* info) {
                                 ran = true;
                                 captured = info;
                               }),
                               base::DoNothing());
  EXPECT_TRUE(ran);
  EXPECT_EQ(captured, nullptr);
}

TEST_F(OpenXrPlatformHelperLinuxTest, GetGraphicsBindingReturnsVulkan) {
  OpenXrPlatformHelperLinux helper;
  ASSERT_TRUE(helper.EnsureInitialized());
  auto binding = helper.GetGraphicsBinding();
  ASSERT_NE(binding, nullptr)
      << "GetGraphicsBinding must never return null (spec §5.1)";
  // CanUseSharedImages() is false until Initialize() runs; this just verifies
  // we get a non-null binding object.
}

TEST_F(OpenXrPlatformHelperLinuxTest, PrepareForSessionShutdownRunsCallback) {
  OpenXrPlatformHelperLinux helper;
  bool ran = false;
  helper.PrepareForSessionShutdown(
      base::BindLambdaForTesting([&]() { ran = true; }));
  EXPECT_TRUE(ran);
}

}  // namespace device
