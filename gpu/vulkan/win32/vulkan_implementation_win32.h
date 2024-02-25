// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_WIN32_VULKAN_IMPLEMENTATION_WIN32_H_
#define GPU_VULKAN_WIN32_VULKAN_IMPLEMENTATION_WIN32_H_

#include "base/component_export.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_instance.h"

namespace gpu {

class COMPONENT_EXPORT(VULKAN_WIN32) VulkanImplementationWin32
    : public VulkanImplementation {
 public:
  explicit VulkanImplementationWin32(bool use_swiftshader);

  VulkanImplementationWin32(const VulkanImplementationWin32&) = delete;
  VulkanImplementationWin32& operator=(const VulkanImplementationWin32&) =
      delete;

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

 private:
  VulkanInstance vulkan_instance_;
};

}  // namespace gpu

#endif  // GPU_VULKAN_WIN32_VULKAN_IMPLEMENTATION_WIN32_H_
