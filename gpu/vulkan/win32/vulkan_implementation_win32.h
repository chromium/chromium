// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_WIN32_VULKAN_IMPLEMENTATION_WIN32_H_
#define GPU_VULKAN_WIN32_VULKAN_IMPLEMENTATION_WIN32_H_

#include <memory>

#include "base/component_export.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_instance.h"

namespace gpu {

class COMPONENT_EXPORT(VULKAN_WIN32) VulkanImplementationWin32
    : public VulkanImplementation {
 public:
  VulkanImplementationWin32() = default;
  ~VulkanImplementationWin32() override;

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

 private:
  VulkanInstance vulkan_instance_;

  PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR
      vkGetPhysicalDeviceWin32PresentationSupportKHR_ = nullptr;
  PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(VulkanImplementationWin32);
};

}  // namespace gpu

#endif  // GPU_VULKAN_WIN32_VULKAN_IMPLEMENTATION_WIN32_H_
