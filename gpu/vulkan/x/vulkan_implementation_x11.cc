// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/x/vulkan_implementation_x11.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/x/x11_types.h"

namespace gpu {

VulkanImplementationX11::VulkanImplementationX11()
    : VulkanImplementationX11(gfx::GetXDisplay()) {}

VulkanImplementationX11::VulkanImplementationX11(XDisplay* x_display)
    : x_display_(x_display) {}

VulkanImplementationX11::~VulkanImplementationX11() {}

bool VulkanImplementationX11::InitializeVulkanInstance() {
  std::vector<const char*> required_extensions = {
      VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XLIB_SURFACE_EXTENSION_NAME};

  VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();

  base::NativeLibraryLoadError native_library_load_error;
  vulkan_function_pointers->vulkan_loader_library_ = base::LoadNativeLibrary(
      base::FilePath("libvulkan.so.1"), &native_library_load_error);
  if (!vulkan_function_pointers->vulkan_loader_library_)
    return false;

  if (!vulkan_instance_.Initialize(required_extensions, {})) {
    vulkan_instance_.Destroy();
    return false;
  }

  // Initialize platform function pointers
  vkGetPhysicalDeviceXlibPresentationSupportKHR_ =
      reinterpret_cast<PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR>(
          vkGetInstanceProcAddr(
              vulkan_instance_.vk_instance(),
              "vkGetPhysicalDeviceXlibPresentationSupportKHR"));
  if (!vkGetPhysicalDeviceXlibPresentationSupportKHR_) {
    LOG(ERROR) << "vkGetPhysicalDeviceXlibPresentationSupportKHR not found";
    vulkan_instance_.Destroy();
    return false;
  }

  vkCreateXlibSurfaceKHR_ =
      reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(vkGetInstanceProcAddr(
          vulkan_instance_.vk_instance(), "vkCreateXlibSurfaceKHR"));
  if (!vkCreateXlibSurfaceKHR_) {
    LOG(ERROR) << "vkCreateXlibSurfaceKHR not found";
    vulkan_instance_.Destroy();
    return false;
  }

  return true;
}

VkInstance VulkanImplementationX11::GetVulkanInstance() {
  return vulkan_instance_.vk_instance();
}

std::unique_ptr<VulkanSurface> VulkanImplementationX11::CreateViewSurface(
    gfx::AcceleratedWidget window) {
  VkSurfaceKHR surface;
  VkXlibSurfaceCreateInfoKHR surface_create_info = {};
  surface_create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
  surface_create_info.dpy = x_display_;
  surface_create_info.window = window;
  VkResult result = vkCreateXlibSurfaceKHR_(
      GetVulkanInstance(), &surface_create_info, nullptr, &surface);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkCreateXlibSurfaceKHR() failed: " << result;
    return nullptr;
  }

  return std::make_unique<VulkanSurface>(GetVulkanInstance(), surface);
}

bool VulkanImplementationX11::GetPhysicalDevicePresentationSupport(
    VkPhysicalDevice device,
    const std::vector<VkQueueFamilyProperties>& queue_family_properties,
    uint32_t queue_family_index) {
  return vkGetPhysicalDeviceXlibPresentationSupportKHR_(
      device, queue_family_index, x_display_,
      XVisualIDFromVisual(
          DefaultVisual(x_display_, DefaultScreen(x_display_))));
}

std::vector<const char*>
VulkanImplementationX11::GetRequiredDeviceExtensions() {
  return {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
}

VkFence VulkanImplementationX11::CreateVkFenceForGpuFence(VkDevice vk_device) {
  NOTREACHED();
  return VK_NULL_HANDLE;
}

std::unique_ptr<gfx::GpuFence> VulkanImplementationX11::ExportVkFenceToGpuFence(
    VkDevice vk_device,
    VkFence vk_fence) {
  NOTREACHED();
  return nullptr;
}

}  // namespace gpu
