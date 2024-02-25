// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_ANDROID_VULKAN_IMPLEMENTATION_ANDROID_H_
#define GPU_VULKAN_ANDROID_VULKAN_IMPLEMENTATION_ANDROID_H_

#include <memory>

#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/component_export.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_surface.h"

namespace gpu {

class COMPONENT_EXPORT(VULKAN_ANDROID) VulkanImplementationAndroid
    : public VulkanImplementation {
 public:
  VulkanImplementationAndroid();

  VulkanImplementationAndroid(const VulkanImplementationAndroid&) = delete;
  VulkanImplementationAndroid& operator=(const VulkanImplementationAndroid&) =
      delete;

  ~VulkanImplementationAndroid() override;

  // VulkanImplementation:
  bool InitializeVulkanInstance(bool using_surface) override;
  VulkanInstance* GetVulkanInstance() override;
  std::unique_ptr<VulkanSurface> CreateViewSurface(
      gfx::AcceleratedWidget window) override;
  bool GetPhysicalDevicePresentationSupport(
      VkPhysicalDevice device,
      const std::vector<VkQueueFamilyProperties>& queue_family_properties,
      uint32_t queue_family_index) override;
  std::vector<const char*> GetRequiredDeviceExtensions() override;
  std::vector<const char*> GetOptionalDeviceExtensions() override;
  VkFence CreateVkFenceForGpuFence(VkDevice vk_device) override;
  std::unique_ptr<gfx::GpuFence> ExportVkFenceToGpuFence(
      VkDevice vk_device,
      VkFence vk_fence) override;
  VkExternalSemaphoreHandleTypeFlagBits GetExternalSemaphoreHandleType()
      override;
  bool CanImportGpuMemoryBuffer(
      VulkanDeviceQueue* device_queue,
      gfx::GpuMemoryBufferType memory_buffer_type) override;
  std::unique_ptr<VulkanImage> CreateImageFromGpuMemoryHandle(
      VulkanDeviceQueue* device_queue,
      gfx::GpuMemoryBufferHandle gmb_handle,
      gfx::Size size,
      VkFormat vk_format,
      const gfx::ColorSpace& color_space) override;
  bool GetSamplerYcbcrConversionInfo(
      const VkDevice& vk_device,
      base::android::ScopedHardwareBufferHandle ahb_handle,
      VulkanYCbCrInfo* ycbcr_info) override;

 private:
  VulkanInstance vulkan_instance_;
};

}  // namespace gpu

#endif  // GPU_VULKAN_ANDROID_VULKAN_IMPLEMENTATION_ANDROID_H_
