// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/linux/openxr_graphics_binding_vulkan.h"

#include <memory>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/scoped_environment_variable_override.h"
#include "device/vr/openxr/linux/openxr_platform_helper_linux.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/openxr/test/openxr_mock_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

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

// Points the Vulkan loader at the SwiftShader ICD built next to the test
// binary, so the binding runs against a software device rather than the host
// driver. VK_ICD_FILENAMES is the pre-1.3.207 spelling of VK_DRIVER_FILES.
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

}  // namespace

class OpenXrGraphicsBindingVulkanTest : public ::testing::Test {
 protected:
  void SetUp() override {
    swiftshader_icd_ = UseSwiftShaderVulkan();
    mock_runtime_ = UseMockOpenXrRuntime();
    // The mock runtime library is only a trampoline; its dispatch table has to
    // be installed from this process before the loader negotiates with it.
    ASSERT_TRUE(InitializeOpenXrMockTrampoline());
    helper_ = std::make_unique<OpenXrPlatformHelperLinux>();
    ASSERT_TRUE(helper_->EnsureInitialized());
  }

  void TearDown() override {
    if (xr_session_ != XR_NULL_HANDLE) {
      xrDestroySession(xr_session_);
    }
    if (xr_instance_ != XR_NULL_HANDLE) {
      helper_->DestroyInstance(xr_instance_);
    }
    helper_.reset();
  }

  bool CreateXrInstanceAndSystem() {
    XrResult result = helper_->CreateInstance(&xr_instance_);
    if (XR_FAILED(result)) {
      return false;
    }

    XrSystemGetInfo system_info{XR_TYPE_SYSTEM_GET_INFO};
    system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    result = xrGetSystem(xr_instance_, &system_info, &system_id_);
    return XR_SUCCEEDED(result);
  }

  bool InitializeBinding() {
    binding_ = std::make_unique<OpenXrGraphicsBindingVulkan>(
        helper_->GetExtensionEnumeration());
    return binding_->Initialize(xr_instance_, system_id_);
  }

  bool CreateSession() {
    const void* session_create_info = binding_->GetSessionCreateInfo();
    if (!session_create_info) {
      return false;
    }

    XrSessionCreateInfo create_info{XR_TYPE_SESSION_CREATE_INFO};
    create_info.systemId = system_id_;
    create_info.next = session_create_info;
    XrResult result = xrCreateSession(xr_instance_, &create_info, &xr_session_);
    return XR_SUCCEEDED(result);
  }

  std::vector<std::unique_ptr<base::ScopedEnvironmentVariableOverride>>
      swiftshader_icd_;
  std::unique_ptr<base::ScopedEnvironmentVariableOverride> mock_runtime_;
  std::unique_ptr<OpenXrPlatformHelperLinux> helper_;
  std::unique_ptr<OpenXrGraphicsBindingVulkan> binding_;
  XrInstance xr_instance_ = XR_NULL_HANDLE;
  XrSystemId system_id_ = XR_NULL_SYSTEM_ID;
  XrSession xr_session_ = XR_NULL_HANDLE;
};

TEST_F(OpenXrGraphicsBindingVulkanTest, InitializeSucceeds) {
  ASSERT_TRUE(CreateXrInstanceAndSystem());

  auto binding = std::make_unique<OpenXrGraphicsBindingVulkan>(
      helper_->GetExtensionEnumeration());

  // Before initialization, the binding is not usable.
  EXPECT_FALSE(binding->CanUseSharedImages());
  EXPECT_EQ(binding->GetSessionCreateInfo(), nullptr);

  // Initialize should succeed with the mock runtime and SwiftShader.
  EXPECT_TRUE(binding->Initialize(xr_instance_, system_id_));
  EXPECT_TRUE(binding->CanUseSharedImages());

  // Verify the binding struct is populated.
  const auto* info = static_cast<const XrGraphicsBindingVulkan2KHR*>(
      binding->GetSessionCreateInfo());
  ASSERT_NE(info, nullptr);
  EXPECT_EQ(info->type, XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR);
  EXPECT_NE(info->instance, VK_NULL_HANDLE);
  EXPECT_NE(info->physicalDevice, VK_NULL_HANDLE);
  EXPECT_NE(info->device, VK_NULL_HANDLE);
}

TEST_F(OpenXrGraphicsBindingVulkanTest, InitializeIdempotent) {
  ASSERT_TRUE(CreateXrInstanceAndSystem());

  auto binding = std::make_unique<OpenXrGraphicsBindingVulkan>(
      helper_->GetExtensionEnumeration());

  EXPECT_TRUE(binding->Initialize(xr_instance_, system_id_));

  // Calling Initialize again should succeed (idempotent).
  EXPECT_TRUE(binding->Initialize(xr_instance_, system_id_));
  EXPECT_TRUE(binding->CanUseSharedImages());
}

TEST_F(OpenXrGraphicsBindingVulkanTest, DestructorCleansUp) {
  ASSERT_TRUE(CreateXrInstanceAndSystem());

  {
    auto binding = std::make_unique<OpenXrGraphicsBindingVulkan>(
        helper_->GetExtensionEnumeration());
    ASSERT_TRUE(binding->Initialize(xr_instance_, system_id_));
    // binding goes out of scope — destructor should clean up
    // VkDevice/VkInstance without crashing.
  }
}

TEST_F(OpenXrGraphicsBindingVulkanTest, GetSwapchainFormatReturnsSRGB) {
  ASSERT_TRUE(CreateXrInstanceAndSystem());
  ASSERT_TRUE(InitializeBinding());
  ASSERT_TRUE(CreateSession());

  // The mock advertises both RGBA and BGRA SRGB on Linux. With no switch, RGBA
  // is preferred.
  int64_t format = binding_->GetSwapchainFormat(xr_session_);
  EXPECT_EQ(format, VK_FORMAT_R8G8B8A8_SRGB);
}

TEST_F(OpenXrGraphicsBindingVulkanTest, GetSwapchainFormatBeforeSession) {
  ASSERT_TRUE(CreateXrInstanceAndSystem());
  ASSERT_TRUE(InitializeBinding());

  // Without a valid session, GetSwapchainFormat should return 0.
  int64_t format = binding_->GetSwapchainFormat(XR_NULL_HANDLE);
  EXPECT_EQ(format, 0);
}

TEST_F(OpenXrGraphicsBindingVulkanTest, SupportsLayersReturnsFalse) {
  ASSERT_TRUE(CreateXrInstanceAndSystem());
  ASSERT_TRUE(InitializeBinding());
  EXPECT_FALSE(binding_->SupportsLayers());
}

TEST_F(OpenXrGraphicsBindingVulkanTest, GetMaxTextureSize) {
  ASSERT_TRUE(CreateXrInstanceAndSystem());
  ASSERT_TRUE(InitializeBinding());
  gfx::Size max_size = binding_->GetMaxTextureSize();
  EXPECT_GT(max_size.width(), 0);
  EXPECT_GT(max_size.height(), 0);
}

TEST_F(OpenXrGraphicsBindingVulkanTest, CleanupWithoutSubmitDoesNotCrash) {
  ASSERT_TRUE(CreateXrInstanceAndSystem());
  ASSERT_TRUE(InitializeBinding());
  // Should not crash.
  binding_->CleanupWithoutSubmit();
}

TEST_F(OpenXrGraphicsBindingVulkanTest, SetOverlayTextureReturnsTrue) {
  ASSERT_TRUE(CreateXrInstanceAndSystem());
  ASSERT_TRUE(InitializeBinding());

  gfx::GpuMemoryBufferHandle handle;
  gpu::SyncToken sync_token;
  gfx::RectF left(0, 0, 0.5f, 1.0f);
  gfx::RectF right(0.5f, 0, 0.5f, 1.0f);

  EXPECT_TRUE(
      binding_->SetOverlayTexture(std::move(handle), sync_token, left, right));
}

TEST_F(OpenXrGraphicsBindingVulkanTest, CanCreateSession) {
  ASSERT_TRUE(CreateXrInstanceAndSystem());
  ASSERT_TRUE(InitializeBinding());

  // Creating a session with the Vulkan binding should succeed.
  EXPECT_TRUE(CreateSession());
  EXPECT_NE(xr_session_, XR_NULL_HANDLE);
}

}  // namespace device
