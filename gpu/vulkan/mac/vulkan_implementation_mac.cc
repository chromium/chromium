// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/mac/vulkan_implementation_mac.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_util.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

VulkanImplementationMac::VulkanImplementationMac(bool use_swiftshader)
    : VulkanImplementation(use_swiftshader) {}

VulkanImplementationMac::~VulkanImplementationMac() = default;

bool VulkanImplementationMac::InitializeVulkanInstance(bool using_surface) {
  DCHECK(using_surface);

  base::FilePath loader_path(use_swiftshader() ? "libvk_swiftshader.dylib"
                                               : "libvulkan.1.dylib");
  std::vector<const char*> required_extensions = {
      VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_EXT_METAL_SURFACE_EXTENSION_NAME,
  };
  return vulkan_instance_.Initialize(loader_path, required_extensions, {});
}

VulkanInstance* VulkanImplementationMac::GetVulkanInstance() {
  return &vulkan_instance_;
}

std::unique_ptr<VulkanSurface> VulkanImplementationMac::CreateViewSurface(
    gfx::AcceleratedWidget window) {
  NOTIMPLEMENTED();
  return nullptr;
}

bool VulkanImplementationMac::GetPhysicalDevicePresentationSupport(
    VkPhysicalDevice device,
    const std::vector<VkQueueFamilyProperties>& queue_family_properties,
    uint32_t queue_family_index) {
  return true;
}

std::vector<const char*>
VulkanImplementationMac::GetRequiredDeviceExtensions() {
  return {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };
}

std::vector<const char*>
VulkanImplementationMac::GetOptionalDeviceExtensions() {
  return {
      VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
      VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
  };
}

VkFence VulkanImplementationMac::CreateVkFenceForGpuFence(VkDevice vk_device) {
  NOTREACHED_IN_MIGRATION();
  return VK_NULL_HANDLE;
}

std::unique_ptr<gfx::GpuFence> VulkanImplementationMac::ExportVkFenceToGpuFence(
    VkDevice vk_device,
    VkFence vk_fence) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

VkExternalSemaphoreHandleTypeFlagBits
VulkanImplementationMac::GetExternalSemaphoreHandleType() {
  return VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
}

bool VulkanImplementationMac::CanImportGpuMemoryBuffer(
    VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferType memory_buffer_type) {
  return false;
}

std::unique_ptr<VulkanImage>
VulkanImplementationMac::CreateImageFromGpuMemoryHandle(
    VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferHandle gmb_handle,
    gfx::Size size,
    VkFormat vk_format,
    const gfx::ColorSpace& color_space) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace gpu
