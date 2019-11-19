// Copyright (c) 2018 The Chromium Authors. All rights reserved.
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
  VkFence CreateVkFenceForGpuFence(VkDevice vk_device) override;
  std::unique_ptr<gfx::GpuFence> ExportVkFenceToGpuFence(
      VkDevice vk_device,
      VkFence vk_fence) override;
  VkSemaphore CreateExternalSemaphore(VkDevice vk_device) override;
  VkSemaphore ImportSemaphoreHandle(VkDevice vk_device,
                                    SemaphoreHandle handle) override;
  SemaphoreHandle GetSemaphoreHandle(VkDevice vk_device,
                                     VkSemaphore vk_semaphore) override;
  VkExternalMemoryHandleTypeFlagBits GetExternalImageHandleType() override;
  bool CanImportGpuMemoryBuffer(
      gfx::GpuMemoryBufferType memory_buffer_type) override;
  bool CreateImageFromGpuMemoryHandle(
      VkDevice vk_device,
      gfx::GpuMemoryBufferHandle gmb_handle,
      gfx::Size size,
      VkImage* vk_image,
      VkImageCreateInfo* vk_image_info,
      VkDeviceMemory* vk_device_memory,
      VkDeviceSize* mem_allocation_size,
      base::Optional<VulkanYCbCrInfo>* ycbcr_info) override;
  bool CreateVkImageAndImportAHB(
      const VkDevice& vk_device,
      const VkPhysicalDevice& vk_physical_device,
      const gfx::Size& size,
      base::android::ScopedHardwareBufferHandle ahb_handle,
      VkImage* vk_image,
      VkImageCreateInfo* vk_image_info,
      VkDeviceMemory* vk_device_memory,
      VkDeviceSize* mem_allocation_size,
      VulkanYCbCrInfo* ycbcr_info) override;
  bool GetSamplerYcbcrConversionInfo(
      const VkDevice& vk_device,
      base::android::ScopedHardwareBufferHandle ahb_handle,
      VulkanYCbCrInfo* ycbcr_info) override;

 private:
  VulkanInstance vulkan_instance_;

  DISALLOW_COPY_AND_ASSIGN(VulkanImplementationAndroid);
};

}  // namespace gpu

#endif  // GPU_VULKAN_ANDROID_VULKAN_IMPLEMENTATION_ANDROID_H_
