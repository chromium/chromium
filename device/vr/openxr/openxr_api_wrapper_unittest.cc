// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_api_wrapper.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/scoped_environment_variable_override.h"
#include "base/test/task_environment.h"
#include "device/vr/openxr/exit_xr_present_reason.h"
#include "device/vr/openxr/linux/openxr_platform_helper_linux.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_graphics_binding.h"
#include "device/vr/openxr/test/openxr_mock_helper.h"
#include "device/vr/openxr/test/openxr_test_helper.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

namespace {

// Points XR_RUNTIME_JSON at the mock runtime copied next to the test binary.
std::unique_ptr<base::ScopedEnvironmentVariableOverride>
UseMockOpenXrRuntime() {
  base::FilePath dir;
  CHECK(base::PathService::Get(base::DIR_EXE, &dir));
  return std::make_unique<base::ScopedEnvironmentVariableOverride>(
      "XR_RUNTIME_JSON",
      dir.Append(FILE_PATH_LITERAL("mock_vr_clients/bin/openxr/openxr.json"))
          .value());
}

// Points the Vulkan loader at the SwiftShader ICD built next to the test
// binary, for the graphics binding.
std::vector<std::unique_ptr<base::ScopedEnvironmentVariableOverride>>
UseSwiftShaderVulkan() {
  base::FilePath dir;
  CHECK(base::PathService::Get(base::DIR_EXE, &dir));
  const std::string icd_path =
      dir.Append(FILE_PATH_LITERAL("vk_swiftshader_icd.json")).value();

  std::vector<std::unique_ptr<base::ScopedEnvironmentVariableOverride>>
      overrides;
  overrides.push_back(std::make_unique<base::ScopedEnvironmentVariableOverride>(
      "VK_DRIVER_FILES", icd_path));
  overrides.push_back(std::make_unique<base::ScopedEnvironmentVariableOverride>(
      "VK_ICD_FILENAMES", icd_path));
  return overrides;
}

// How long it takes OpenXrApiWrapper to poll OpenXR events while it isn't
// rendering frames.
constexpr base::TimeDelta kEventPollingInterval = base::Seconds(1);

}  // namespace

// Runs an OpenXrApiWrapper session against the mock OpenXR runtime.
class OpenXrApiWrapperTest : public ::testing::Test {
 protected:
  void SetUp() override {
    swiftshader_icd_ = UseSwiftShaderVulkan();
    mock_runtime_ = UseMockOpenXrRuntime();
    ASSERT_TRUE(InitializeOpenXrMockTrampoline());
    platform_helper_ = std::make_unique<OpenXrPlatformHelperLinux>();
    ASSERT_TRUE(platform_helper_->EnsureInitialized());
    ASSERT_TRUE(XR_SUCCEEDED(platform_helper_->CreateInstance(&instance_)));
    extension_helper_ = std::make_unique<OpenXrExtensionHelper>(
        instance_, platform_helper_->GetExtensionEnumeration());
    graphics_binding_ = platform_helper_->GetGraphicsBinding();
    openxr_ = OpenXrApiWrapper::Create(instance_, graphics_binding_.get());
    ASSERT_TRUE(openxr_);
  }

  void TearDown() override {
    openxr_.reset();
    graphics_binding_.reset();
    extension_helper_.reset();
    platform_helper_->DestroyInstance(instance_);
    platform_helper_.reset();
  }

  // Starts a session and lets the runtime make it ready.
  void StartSession() {
    auto options = mojom::XRRuntimeSessionOptions::New();
    options->mode = mojom::XRSessionMode::kImmersiveVr;
    openxr_->SetOnSessionResumedCallback(base::BindRepeating(
        [](int* resumed_count) { ++*resumed_count; }, &resumed_count_));
    ASSERT_TRUE(XR_SUCCEEDED(openxr_->InitSession(
        std::move(options), *extension_helper_,
        base::BindOnce([](std::optional<XrResult>* started_result,
                          mojom::XRRuntimeSessionOptionsPtr options,
                          XrResult result) { *started_result = result; },
                       &started_result_),
        base::BindRepeating(
            [](std::optional<ExitXrPresentReason>* ended_reason,
               ExitXrPresentReason reason) { *ended_reason = reason; },
            &ended_reason_),
        base::DoNothing())));
    PollEvents();
    ASSERT_EQ(started_result_, XR_SUCCESS);
    ASSERT_TRUE(openxr_->IsSessionRunning());
  }

  void PollEvents() { task_environment_.FastForwardBy(kEventPollingInterval); }

  OpenXrTestHelper& runtime() { return OpenXrTestHelper::Get(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::vector<std::unique_ptr<base::ScopedEnvironmentVariableOverride>>
      swiftshader_icd_;
  std::unique_ptr<base::ScopedEnvironmentVariableOverride> mock_runtime_;
  std::unique_ptr<OpenXrPlatformHelperLinux> platform_helper_;
  XrInstance instance_ = XR_NULL_HANDLE;
  std::unique_ptr<OpenXrExtensionHelper> extension_helper_;
  std::unique_ptr<OpenXrGraphicsBinding> graphics_binding_;
  std::unique_ptr<OpenXrApiWrapper> openxr_;

  std::optional<XrResult> started_result_;
  std::optional<ExitXrPresentReason> ended_reason_;
  int resumed_count_ = 0;
};

// The runtime stopping the session (STOPPING, then IDLE) doesn't end it, and
// it runs again once the runtime makes it READY. (The mock runtime can't run
// frames without a browser test's hook, so this checks that frames are refused
// while the session is stopped, and that it resumes.)
TEST_F(OpenXrApiWrapperTest, SessionSurvivesBeingStopped) {
  StartSession();

  runtime().StopSession(/*state_after_end=*/std::nullopt);
  PollEvents();
  EXPECT_FALSE(openxr_->IsSessionRunning());
  EXPECT_TRUE(openxr_->IsInitialized());
  EXPECT_FALSE(ended_reason_);
  EXPECT_EQ(openxr_->BeginFrame(), XR_ERROR_SESSION_NOT_RUNNING);

  runtime().MakeSessionReady();
  PollEvents();
  EXPECT_TRUE(openxr_->IsSessionRunning());
  EXPECT_EQ(resumed_count_, 1);
  EXPECT_FALSE(ended_reason_);
}

// The session still ends when the runtime exits it after stopping it.
TEST_F(OpenXrApiWrapperTest, SessionEndsWhenExited) {
  StartSession();

  runtime().StopSession(/*state_after_end=*/XR_SESSION_STATE_EXITING);
  PollEvents();
  EXPECT_FALSE(openxr_->IsInitialized());
  EXPECT_EQ(ended_reason_, ExitXrPresentReason::kOpenXrUninitialize);
  EXPECT_EQ(resumed_count_, 0);
}

}  // namespace device
