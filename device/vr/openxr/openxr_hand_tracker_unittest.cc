// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_hand_tracker.h"

#include <memory>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/scoped_environment_variable_override.h"
#include "device/vr/openxr/linux/openxr_graphics_binding_vulkan.h"
#include "device/vr/openxr/linux/openxr_platform_helper_linux.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_interaction_profiles.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/openxr/test/openxr_mock_helper.h"
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
// binary, for the session's graphics binding.
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

// Chains a struct onto the joint locations, as OpenXrHandTrackerFb does with
// its XrHandTrackingAimStateFB, and counts how often it is asked to.
class TestHandTracker : public OpenXrHandTracker {
 public:
  using OpenXrHandTracker::OpenXrHandTracker;

  int extend_count() const { return extend_count_; }

 private:
  void ExtendLocationsNextChain(XrNextChainBuilder& next_chain) override {
    ++extend_count_;
    next_chain.Add(&aim_state_);
  }

  int extend_count_ = 0;
  XrHandTrackingAimStateFB aim_state_{XR_TYPE_HAND_TRACKING_AIM_STATE_FB};
};

}  // namespace

// Runs hand trackers in a session with the mock OpenXR runtime.
class OpenXrHandTrackerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    swiftshader_icd_ = UseSwiftShaderVulkan();
    mock_runtime_ = UseMockOpenXrRuntime();
    ASSERT_TRUE(InitializeOpenXrMockTrampoline());
    platform_helper_ = std::make_unique<OpenXrPlatformHelperLinux>();
    ASSERT_TRUE(platform_helper_->EnsureInitialized());
    ASSERT_TRUE(XR_SUCCEEDED(platform_helper_->CreateInstance(&instance_)));

    XrSystemGetInfo system_info{XR_TYPE_SYSTEM_GET_INFO};
    system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId system_id = XR_NULL_SYSTEM_ID;
    ASSERT_TRUE(XR_SUCCEEDED(xrGetSystem(instance_, &system_info, &system_id)));

    graphics_binding_ = std::make_unique<OpenXrGraphicsBindingVulkan>(
        platform_helper_->GetExtensionEnumeration());
    ASSERT_TRUE(graphics_binding_->Initialize(instance_, system_id));
    XrSessionCreateInfo session_info{XR_TYPE_SESSION_CREATE_INFO};
    session_info.systemId = system_id;
    session_info.next = graphics_binding_->GetSessionCreateInfo();
    ASSERT_TRUE(
        XR_SUCCEEDED(xrCreateSession(instance_, &session_info, &session_)));

    XrReferenceSpaceCreateInfo space_info{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    space_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    space_info.poseInReferenceSpace.orientation.w = 1.0f;
    ASSERT_TRUE(XR_SUCCEEDED(
        xrCreateReferenceSpace(session_, &space_info, &local_space_)));

    extension_helper_ = std::make_unique<OpenXrExtensionHelper>(
        instance_, platform_helper_->GetExtensionEnumeration());
  }

  void TearDown() override {
    if (local_space_ != XR_NULL_HANDLE) {
      xrDestroySpace(local_space_);
    }
    if (session_ != XR_NULL_HANDLE) {
      xrDestroySession(session_);
    }
    if (instance_ != XR_NULL_HANDLE) {
      platform_helper_->DestroyInstance(instance_);
    }
  }

  std::vector<std::unique_ptr<base::ScopedEnvironmentVariableOverride>>
      swiftshader_icd_;
  std::unique_ptr<base::ScopedEnvironmentVariableOverride> mock_runtime_;
  std::unique_ptr<OpenXrPlatformHelperLinux> platform_helper_;
  std::unique_ptr<OpenXrGraphicsBindingVulkan> graphics_binding_;
  std::unique_ptr<OpenXrExtensionHelper> extension_helper_;
  XrInstance instance_ = XR_NULL_HANDLE;
  XrSession session_ = XR_NULL_HANDLE;
  XrSpace local_space_ = XR_NULL_HANDLE;
};

// A subclass's ExtendLocationsNextChain() override runs, once.
TEST_F(OpenXrHandTrackerTest, SubclassExtendsLocationsNextChain) {
  TestHandTracker hand_tracker(*extension_helper_, session_,
                               OpenXrHandednessType::kRight);

  ASSERT_TRUE(XR_SUCCEEDED(
      hand_tracker.Update(local_space_, /*predicted_display_time=*/1)));
  EXPECT_EQ(hand_tracker.extend_count(), 1);

  ASSERT_TRUE(XR_SUCCEEDED(
      hand_tracker.Update(local_space_, /*predicted_display_time=*/2)));
  EXPECT_EQ(hand_tracker.extend_count(), 1);
}

}  // namespace device
