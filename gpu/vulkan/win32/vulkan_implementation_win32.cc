// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/win32/vulkan_implementation_win32.h"

#include <Windows.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "ui/gfx/gpu_fence.h"

namespace gpu {

VulkanImplementationWin32::~VulkanImplementationWin32() = default;

bool VulkanImplementationWin32::InitializeVulkanInstance() {
  std::vector<const char*> required_extensions = {
      VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};

  VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();

  base::NativeLibraryLoadError native_library_load_error;
  vulkan_function_pointers->vulkan_loader_library_ = base::LoadNativeLibrary(
      base::FilePath(L"vulkan-1.dll"), &native_library_load_error);
  if (!vulkan_function_pointers->vulkan_loader_library_)
    return false;

  if (!vulkan_instance_.Initialize(required_extensions, {})) {
    vulkan_instance_.Destroy();
    return false;
  }

  // Initialize platform function pointers
  vkGetPhysicalDeviceWin32PresentationSupportKHR_ =
      reinterpret_cast<PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR>(
          vkGetInstanceProcAddr(
              vulkan_instance_.vk_instance(),
              "vkGetPhysicalDeviceWin32PresentationSupportKHR"));
  if (!vkGetPhysicalDeviceWin32PresentationSupportKHR_) {
    LOG(ERROR) << "vkGetPhysicalDeviceWin32PresentationSupportKHR not found";
    vulkan_instance_.Destroy();
    return false;
  }

  vkCreateWin32SurfaceKHR_ =
      reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(vkGetInstanceProcAddr(
          vulkan_instance_.vk_instance(), "vkCreateWin32SurfaceKHR"));
  if (!vkCreateWin32SurfaceKHR_) {
    LOG(ERROR) << "vkCreateWin32SurfaceKHR not found";
    vulkan_instance_.Destroy();
    return false;
  }

  return true;
}

VkInstance VulkanImplementationWin32::GetVulkanInstance() {
  return vulkan_instance_.vk_instance();
}

std::unique_ptr<VulkanSurface> VulkanImplementationWin32::CreateViewSurface(
    gfx::AcceleratedWidget window) {
  VkSurfaceKHR surface;
  VkWin32SurfaceCreateInfoKHR surface_create_info = {};
  surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  surface_create_info.hinstance =
      reinterpret_cast<HINSTANCE>(GetWindowLongPtr(window, GWLP_HINSTANCE));
  surface_create_info.hwnd = window;
  VkResult result = vkCreateWin32SurfaceKHR_(
      GetVulkanInstance(), &surface_create_info, nullptr, &surface);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkCreatWin32SurfaceKHR() failed: " << result;
    return nullptr;
  }

  return std::make_unique<VulkanSurface>(GetVulkanInstance(), surface);
}

bool VulkanImplementationWin32::GetPhysicalDevicePresentationSupport(
    VkPhysicalDevice device,
    const std::vector<VkQueueFamilyProperties>& queue_family_properties,
    uint32_t queue_family_index) {
  return vkGetPhysicalDeviceWin32PresentationSupportKHR_(device,
                                                         queue_family_index);
}

std::vector<const char*>
VulkanImplementationWin32::GetRequiredDeviceExtensions() {
  return {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
}

VkFence VulkanImplementationWin32::CreateVkFenceForGpuFence(
    VkDevice vk_device) {
  NOTREACHED();
  return VK_NULL_HANDLE;
}

std::unique_ptr<gfx::GpuFence>
VulkanImplementationWin32::ExportVkFenceToGpuFence(VkDevice vk_device,
                                                   VkFence vk_fence) {
  NOTREACHED();
  return nullptr;
}

}  // namespace gpu
