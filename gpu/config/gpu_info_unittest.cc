// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {
// This overrides the base class to test behaviors of virtual functions.
class TestGPUInfoEnumerator : public gpu::GPUInfo::Enumerator {
 public:
  TestGPUInfoEnumerator()
      : gpu_device_active_(false),
        video_decode_accelerator_profile_active_(false),
        video_encode_accelerator_profile_active_(false),
        image_decode_accelerator_profile_active_(false),
        dx12_vulkan_version_info_active_(false),
        aux_attributes_active_(false) {}

  void AddInt64(const char* name, int64_t value) override {}

  void AddInt(const char* name, int value) override {}

  void AddString(const char* name, const std::string& value) override {}

  void AddBool(const char* name, bool value) override {}

  void AddBinary(const char* name,
                 const base::span<const uint8_t>& blob) override {}

  void AddTimeDeltaInSecondsF(const char* name,
                              const base::TimeDelta& value) override {}

  // Enumerator state mutator functions
  void BeginGPUDevice() override { gpu_device_active_ = true; }

  void EndGPUDevice() override { gpu_device_active_ = false; }

  void BeginVideoDecodeAcceleratorSupportedProfile() override {
    video_decode_accelerator_profile_active_ = true;
  }

  void EndVideoDecodeAcceleratorSupportedProfile() override {
    video_decode_accelerator_profile_active_ = false;
  }

  void BeginVideoEncodeAcceleratorSupportedProfile() override {
    video_encode_accelerator_profile_active_ = true;
  }

  void EndVideoEncodeAcceleratorSupportedProfile() override {
    video_encode_accelerator_profile_active_ = false;
  }

  void BeginImageDecodeAcceleratorSupportedProfile() override {
    image_decode_accelerator_profile_active_ = true;
  }

  void EndImageDecodeAcceleratorSupportedProfile() override {
    image_decode_accelerator_profile_active_ = false;
  }

  void BeginDx12VulkanVersionInfo() override {
    dx12_vulkan_version_info_active_ = true;
  }

  void EndDx12VulkanVersionInfo() override {
    dx12_vulkan_version_info_active_ = false;
  }

  void BeginAuxAttributes() override { aux_attributes_active_ = true; }

  void EndAuxAttributes() override { aux_attributes_active_ = false; }

  // Accessor functions
  bool gpu_device_active() const { return gpu_device_active_; }

  bool video_decode_accelerator_profile_active() const {
    return video_decode_accelerator_profile_active_;
  }

  bool video_encode_accelerator_profile_active() const {
    return video_encode_accelerator_profile_active_;
  }

  bool image_decode_accelerator_profile_active() const {
    return image_decode_accelerator_profile_active_;
  }

  bool dx12_vulkan_version_info_active() const {
    return dx12_vulkan_version_info_active_;
  }

  bool aux_attributes_active() const { return aux_attributes_active_; }

 private:
  bool gpu_device_active_;
  bool video_decode_accelerator_profile_active_;
  bool video_encode_accelerator_profile_active_;
  bool image_decode_accelerator_profile_active_;
  bool dx12_vulkan_version_info_active_;
  bool aux_attributes_active_;
};
}  // namespace

// Makes sure that after EnumerateFields is called, the field edit states
// are inactive
TEST(GpuInfoTest, FieldEditStates) {
  GPUInfo gpu_info;
  TestGPUInfoEnumerator enumerator;
  gpu_info.EnumerateFields(&enumerator);
  EXPECT_FALSE(enumerator.gpu_device_active());
  EXPECT_FALSE(enumerator.video_decode_accelerator_profile_active());
  EXPECT_FALSE(enumerator.video_encode_accelerator_profile_active());
  EXPECT_FALSE(enumerator.image_decode_accelerator_profile_active());
  EXPECT_FALSE(enumerator.dx12_vulkan_version_info_active());
  EXPECT_FALSE(enumerator.aux_attributes_active());
}

}  // namespace gpu
