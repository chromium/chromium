// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_X_VULKAN_IMPLEMENTATION_X11_H_
#define GPU_VULKAN_X_VULKAN_IMPLEMENTATION_X11_H_

#include <memory>

#include "base/component_export.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "ui/gfx/x/x11_types.h"

namespace gpu {

class COMPONENT_EXPORT(VULKAN_X11) VulkanImplementationX11
    : public VulkanImplementation {
 public:
  explicit VulkanImplementationX11(bool use_swiftshader = false);
  ~VulkanImplementationX11() override;

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
  bool using_surface_ = true;
  VulkanInstance vulkan_instance_;

  DISALLOW_COPY_AND_ASSIGN(VulkanImplementationX11);
};

}  // namespace gpu

#endif  // GPU_VULKAN_X_VULKAN_IMPLEMENTATION_X11_H_
