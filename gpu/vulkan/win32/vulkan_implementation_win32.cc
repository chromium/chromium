// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/win32/vulkan_implementation_win32.h"

#include <Windows.h>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_util.h"
#include "gpu/vulkan/win32/vulkan_surface_win32.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

VulkanImplementationWin32::VulkanImplementationWin32(bool use_swiftshader)
    : VulkanImplementation(use_swiftshader) {}

VulkanImplementationWin32::~VulkanImplementationWin32() = default;

bool VulkanImplementationWin32::InitializeVulkanInstance(bool using_surface) {
  DCHECK(using_surface);

  base::FilePath loader_path(use_swiftshader() ? L"vk_swiftshader.dll"
                                               : L"vulkan-1.dll");
  std::vector<const char*> required_extensions = {
      VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
  };
  return vulkan_instance_.Initialize(loader_path, required_extensions, {});
}

VulkanInstance* VulkanImplementationWin32::GetVulkanInstance() {
  return &vulkan_instance_;
}

std::unique_ptr<VulkanSurface> VulkanImplementationWin32::CreateViewSurface(
    gfx::AcceleratedWidget window) {
  return VulkanSurfaceWin32::Create(vulkan_instance_.vk_instance(), window);
}

bool VulkanImplementationWin32::GetPhysicalDevicePresentationSupport(
    VkPhysicalDevice device,
    const std::vector<VkQueueFamilyProperties>& queue_family_properties,
    uint32_t queue_family_index) {
  return vkGetPhysicalDeviceWin32PresentationSupportKHR(device,
                                                        queue_family_index);
}

std::vector<const char*>
VulkanImplementationWin32::GetRequiredDeviceExtensions() {
  return {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };
}

std::vector<const char*>
VulkanImplementationWin32::GetOptionalDeviceExtensions() {
  return {
      VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
      VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
  };
}

VkFence VulkanImplementationWin32::CreateVkFenceForGpuFence(
    VkDevice vk_device) {
  NOTREACHED_IN_MIGRATION();
  return VK_NULL_HANDLE;
}

std::unique_ptr<gfx::GpuFence>
VulkanImplementationWin32::ExportVkFenceToGpuFence(VkDevice vk_device,
                                                   VkFence vk_fence) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

VkExternalSemaphoreHandleTypeFlagBits
VulkanImplementationWin32::GetExternalSemaphoreHandleType() {
  return VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
}

bool VulkanImplementationWin32::CanImportGpuMemoryBuffer(
    VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferType memory_buffer_type) {
  return false;
}

std::unique_ptr<VulkanImage>
VulkanImplementationWin32::CreateImageFromGpuMemoryHandle(
    VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferHandle gmb_handle,
    gfx::Size size,
    VkFormat vk_format,
    const gfx::ColorSpace& color_space) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace gpu
