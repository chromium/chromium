// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// gpu/vulkan/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include "gpu/vulkan/vulkan_function_pointers.h"

#include "base/logging.h"
#include "base/no_destructor.h"

namespace gpu {

VulkanFunctionPointers* GetVulkanFunctionPointers() {
  static base::NoDestructor<VulkanFunctionPointers> vulkan_function_pointers;
  return vulkan_function_pointers.get();
}

VulkanFunctionPointers::VulkanFunctionPointers() = default;
VulkanFunctionPointers::~VulkanFunctionPointers() = default;

bool VulkanFunctionPointers::BindUnassociatedFunctionPointersFromLoaderLib(
    base::NativeLibrary lib) {
  base::AutoLock lock(write_lock_);
  loader_library_ = lib;

  // vkGetInstanceProcAddr must be handled specially since it gets its
  // function pointer through base::GetFunctionPointerFromNativeLibrary().
  // Other Vulkan functions don't do this.
  vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      base::GetFunctionPointerFromNativeLibrary(loader_library_,
                                                "vkGetInstanceProcAddr"));
  if (!vkGetInstanceProcAddr)
    return false;
  return BindUnassociatedFunctionPointersCommon();
}

bool VulkanFunctionPointers::BindUnassociatedFunctionPointersFromGetProcAddr(
    PFN_vkGetInstanceProcAddr proc) {
  DCHECK(proc);
  DCHECK(!loader_library_);

  base::AutoLock lock(write_lock_);
  vkGetInstanceProcAddr = proc;
  return BindUnassociatedFunctionPointersCommon();
}

bool VulkanFunctionPointers::BindUnassociatedFunctionPointersCommon() {
  vkEnumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
      vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
  if (!vkEnumerateInstanceVersion) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEnumerateInstanceVersion";
    return false;
  }

  vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
      vkGetInstanceProcAddr(nullptr, "vkCreateInstance"));
  if (!vkCreateInstance) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateInstance";
    return false;
  }

  vkEnumerateInstanceExtensionProperties =
      reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
          vkGetInstanceProcAddr(nullptr,
                                "vkEnumerateInstanceExtensionProperties"));
  if (!vkEnumerateInstanceExtensionProperties) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEnumerateInstanceExtensionProperties";
    return false;
  }

  vkEnumerateInstanceLayerProperties =
      reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(
          vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceLayerProperties"));
  if (!vkEnumerateInstanceLayerProperties) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEnumerateInstanceLayerProperties";
    return false;
  }

  return true;
}

bool VulkanFunctionPointers::BindInstanceFunctionPointers(
    VkInstance vk_instance,
    uint32_t api_version,
    const gfx::ExtensionSet& enabled_extensions) {
  DCHECK_GE(api_version, kVulkanRequiredApiVersion);
  base::AutoLock lock(write_lock_);
  vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(
      vkGetInstanceProcAddr(vk_instance, "vkCreateDevice"));
  if (!vkCreateDevice) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateDevice";
    return false;
  }

  vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
      vkGetInstanceProcAddr(vk_instance, "vkDestroyInstance"));
  if (!vkDestroyInstance) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyInstance";
    return false;
  }

  vkEnumerateDeviceExtensionProperties =
      reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkEnumerateDeviceExtensionProperties"));
  if (!vkEnumerateDeviceExtensionProperties) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEnumerateDeviceExtensionProperties";
    return false;
  }

  vkEnumerateDeviceLayerProperties =
      reinterpret_cast<PFN_vkEnumerateDeviceLayerProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkEnumerateDeviceLayerProperties"));
  if (!vkEnumerateDeviceLayerProperties) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEnumerateDeviceLayerProperties";
    return false;
  }

  vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
      vkGetInstanceProcAddr(vk_instance, "vkEnumeratePhysicalDevices"));
  if (!vkEnumeratePhysicalDevices) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEnumeratePhysicalDevices";
    return false;
  }

  vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
      vkGetInstanceProcAddr(vk_instance, "vkGetDeviceProcAddr"));
  if (!vkGetDeviceProcAddr) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetDeviceProcAddr";
    return false;
  }

  vkGetPhysicalDeviceFeatures2 =
      reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
          vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceFeatures2"));
  if (!vkGetPhysicalDeviceFeatures2) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceFeatures2";
    return false;
  }

  vkGetPhysicalDeviceFormatProperties =
      reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkGetPhysicalDeviceFormatProperties"));
  if (!vkGetPhysicalDeviceFormatProperties) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceFormatProperties";
    return false;
  }

  vkGetPhysicalDeviceFormatProperties2 =
      reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties2>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkGetPhysicalDeviceFormatProperties2"));
  if (!vkGetPhysicalDeviceFormatProperties2) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceFormatProperties2";
    return false;
  }

  vkGetPhysicalDeviceImageFormatProperties2 =
      reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkGetPhysicalDeviceImageFormatProperties2"));
  if (!vkGetPhysicalDeviceImageFormatProperties2) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceImageFormatProperties2";
    return false;
  }

  vkGetPhysicalDeviceMemoryProperties =
      reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkGetPhysicalDeviceMemoryProperties"));
  if (!vkGetPhysicalDeviceMemoryProperties) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceMemoryProperties";
    return false;
  }

  vkGetPhysicalDeviceMemoryProperties2 =
      reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties2>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkGetPhysicalDeviceMemoryProperties2"));
  if (!vkGetPhysicalDeviceMemoryProperties2) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceMemoryProperties2";
    return false;
  }

  vkGetPhysicalDeviceProperties =
      reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
          vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceProperties"));
  if (!vkGetPhysicalDeviceProperties) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceProperties";
    return false;
  }

  vkGetPhysicalDeviceProperties2 =
      reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
          vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceProperties2"));
  if (!vkGetPhysicalDeviceProperties2) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceProperties2";
    return false;
  }

  vkGetPhysicalDeviceQueueFamilyProperties =
      reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkGetPhysicalDeviceQueueFamilyProperties"));
  if (!vkGetPhysicalDeviceQueueFamilyProperties) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceQueueFamilyProperties";
    return false;
  }

#if DCHECK_IS_ON()
  if (gfx::HasExtension(enabled_extensions,
                        VK_EXT_DEBUG_REPORT_EXTENSION_NAME)) {
    vkCreateDebugReportCallbackEXT =
        reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkCreateDebugReportCallbackEXT"));
    if (!vkCreateDebugReportCallbackEXT) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateDebugReportCallbackEXT";
      return false;
    }

    vkDestroyDebugReportCallbackEXT =
        reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkDestroyDebugReportCallbackEXT"));
    if (!vkDestroyDebugReportCallbackEXT) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkDestroyDebugReportCallbackEXT";
      return false;
    }
  }
#endif  // DCHECK_IS_ON()

  if (gfx::HasExtension(enabled_extensions, VK_KHR_SURFACE_EXTENSION_NAME)) {
    vkDestroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
        vkGetInstanceProcAddr(vk_instance, "vkDestroySurfaceKHR"));
    if (!vkDestroySurfaceKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkDestroySurfaceKHR";
      return false;
    }

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    if (!vkGetPhysicalDeviceSurfaceCapabilitiesKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceSurfaceCapabilitiesKHR";
      return false;
    }

    vkGetPhysicalDeviceSurfaceFormatsKHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    if (!vkGetPhysicalDeviceSurfaceFormatsKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceSurfaceFormatsKHR";
      return false;
    }

    vkGetPhysicalDeviceSurfaceSupportKHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkGetPhysicalDeviceSurfaceSupportKHR"));
    if (!vkGetPhysicalDeviceSurfaceSupportKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceSurfaceSupportKHR";
      return false;
    }
  }

  if (gfx::HasExtension(enabled_extensions,
                        VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME)) {
    vkCreateHeadlessSurfaceEXT =
        reinterpret_cast<PFN_vkCreateHeadlessSurfaceEXT>(
            vkGetInstanceProcAddr(vk_instance, "vkCreateHeadlessSurfaceEXT"));
    if (!vkCreateHeadlessSurfaceEXT) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateHeadlessSurfaceEXT";
      return false;
    }
  }

#if defined(USE_VULKAN_XCB)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_XCB_SURFACE_EXTENSION_NAME)) {
    vkCreateXcbSurfaceKHR = reinterpret_cast<PFN_vkCreateXcbSurfaceKHR>(
        vkGetInstanceProcAddr(vk_instance, "vkCreateXcbSurfaceKHR"));
    if (!vkCreateXcbSurfaceKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateXcbSurfaceKHR";
      return false;
    }

    vkGetPhysicalDeviceXcbPresentationSupportKHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR>(
            vkGetInstanceProcAddr(
                vk_instance, "vkGetPhysicalDeviceXcbPresentationSupportKHR"));
    if (!vkGetPhysicalDeviceXcbPresentationSupportKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceXcbPresentationSupportKHR";
      return false;
    }
  }
#endif  // defined(USE_VULKAN_XCB)

#if BUILDFLAG(IS_WIN)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_WIN32_SURFACE_EXTENSION_NAME)) {
    vkCreateWin32SurfaceKHR = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
        vkGetInstanceProcAddr(vk_instance, "vkCreateWin32SurfaceKHR"));
    if (!vkCreateWin32SurfaceKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateWin32SurfaceKHR";
      return false;
    }

    vkGetPhysicalDeviceWin32PresentationSupportKHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR>(
            vkGetInstanceProcAddr(
                vk_instance, "vkGetPhysicalDeviceWin32PresentationSupportKHR"));
    if (!vkGetPhysicalDeviceWin32PresentationSupportKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceWin32PresentationSupportKHR";
      return false;
    }
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME)) {
    vkCreateAndroidSurfaceKHR = reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
        vkGetInstanceProcAddr(vk_instance, "vkCreateAndroidSurfaceKHR"));
    if (!vkCreateAndroidSurfaceKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateAndroidSurfaceKHR";
      return false;
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_FUCHSIA)
  if (gfx::HasExtension(enabled_extensions,
                        VK_FUCHSIA_IMAGEPIPE_SURFACE_EXTENSION_NAME)) {
    vkCreateImagePipeSurfaceFUCHSIA =
        reinterpret_cast<PFN_vkCreateImagePipeSurfaceFUCHSIA>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkCreateImagePipeSurfaceFUCHSIA"));
    if (!vkCreateImagePipeSurfaceFUCHSIA) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateImagePipeSurfaceFUCHSIA";
      return false;
    }
  }
#endif  // BUILDFLAG(IS_FUCHSIA)

  return true;
}

bool VulkanFunctionPointers::BindDeviceFunctionPointers(
    VkDevice vk_device,
    uint32_t api_version,
    const gfx::ExtensionSet& enabled_extensions) {
  DCHECK_GE(api_version, kVulkanRequiredApiVersion);
  base::AutoLock lock(write_lock_);
  // Device functions
  vkAllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(
      vkGetDeviceProcAddr(vk_device, "vkAllocateCommandBuffers"));
  if (!vkAllocateCommandBuffers) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkAllocateCommandBuffers";
    return false;
  }

  vkAllocateDescriptorSets = reinterpret_cast<PFN_vkAllocateDescriptorSets>(
      vkGetDeviceProcAddr(vk_device, "vkAllocateDescriptorSets"));
  if (!vkAllocateDescriptorSets) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkAllocateDescriptorSets";
    return false;
  }

  vkAllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(
      vkGetDeviceProcAddr(vk_device, "vkAllocateMemory"));
  if (!vkAllocateMemory) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkAllocateMemory";
    return false;
  }

  vkBeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(
      vkGetDeviceProcAddr(vk_device, "vkBeginCommandBuffer"));
  if (!vkBeginCommandBuffer) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkBeginCommandBuffer";
    return false;
  }

  vkBindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(
      vkGetDeviceProcAddr(vk_device, "vkBindBufferMemory"));
  if (!vkBindBufferMemory) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkBindBufferMemory";
    return false;
  }

  vkBindBufferMemory2 = reinterpret_cast<PFN_vkBindBufferMemory2>(
      vkGetDeviceProcAddr(vk_device, "vkBindBufferMemory2"));
  if (!vkBindBufferMemory2) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkBindBufferMemory2";
    return false;
  }

  vkBindImageMemory = reinterpret_cast<PFN_vkBindImageMemory>(
      vkGetDeviceProcAddr(vk_device, "vkBindImageMemory"));
  if (!vkBindImageMemory) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkBindImageMemory";
    return false;
  }

  vkBindImageMemory2 = reinterpret_cast<PFN_vkBindImageMemory2>(
      vkGetDeviceProcAddr(vk_device, "vkBindImageMemory2"));
  if (!vkBindImageMemory2) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkBindImageMemory2";
    return false;
  }

  vkCmdBeginRenderPass = reinterpret_cast<PFN_vkCmdBeginRenderPass>(
      vkGetDeviceProcAddr(vk_device, "vkCmdBeginRenderPass"));
  if (!vkCmdBeginRenderPass) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdBeginRenderPass";
    return false;
  }

  vkCmdCopyBuffer = reinterpret_cast<PFN_vkCmdCopyBuffer>(
      vkGetDeviceProcAddr(vk_device, "vkCmdCopyBuffer"));
  if (!vkCmdCopyBuffer) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdCopyBuffer";
    return false;
  }

  vkCmdCopyBufferToImage = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(
      vkGetDeviceProcAddr(vk_device, "vkCmdCopyBufferToImage"));
  if (!vkCmdCopyBufferToImage) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdCopyBufferToImage";
    return false;
  }

  vkCmdCopyImageToBuffer = reinterpret_cast<PFN_vkCmdCopyImageToBuffer>(
      vkGetDeviceProcAddr(vk_device, "vkCmdCopyImageToBuffer"));
  if (!vkCmdCopyImageToBuffer) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdCopyImageToBuffer";
    return false;
  }

  vkCmdEndRenderPass = reinterpret_cast<PFN_vkCmdEndRenderPass>(
      vkGetDeviceProcAddr(vk_device, "vkCmdEndRenderPass"));
  if (!vkCmdEndRenderPass) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdEndRenderPass";
    return false;
  }

  vkCmdExecuteCommands = reinterpret_cast<PFN_vkCmdExecuteCommands>(
      vkGetDeviceProcAddr(vk_device, "vkCmdExecuteCommands"));
  if (!vkCmdExecuteCommands) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdExecuteCommands";
    return false;
  }

  vkCmdNextSubpass = reinterpret_cast<PFN_vkCmdNextSubpass>(
      vkGetDeviceProcAddr(vk_device, "vkCmdNextSubpass"));
  if (!vkCmdNextSubpass) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdNextSubpass";
    return false;
  }

  vkCmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(
      vkGetDeviceProcAddr(vk_device, "vkCmdPipelineBarrier"));
  if (!vkCmdPipelineBarrier) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdPipelineBarrier";
    return false;
  }

  vkCreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(
      vkGetDeviceProcAddr(vk_device, "vkCreateBuffer"));
  if (!vkCreateBuffer) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateBuffer";
    return false;
  }

  vkCreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(
      vkGetDeviceProcAddr(vk_device, "vkCreateCommandPool"));
  if (!vkCreateCommandPool) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateCommandPool";
    return false;
  }

  vkCreateDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(
      vkGetDeviceProcAddr(vk_device, "vkCreateDescriptorPool"));
  if (!vkCreateDescriptorPool) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateDescriptorPool";
    return false;
  }

  vkCreateDescriptorSetLayout =
      reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(
          vkGetDeviceProcAddr(vk_device, "vkCreateDescriptorSetLayout"));
  if (!vkCreateDescriptorSetLayout) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateDescriptorSetLayout";
    return false;
  }

  vkCreateFence = reinterpret_cast<PFN_vkCreateFence>(
      vkGetDeviceProcAddr(vk_device, "vkCreateFence"));
  if (!vkCreateFence) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateFence";
    return false;
  }

  vkCreateFramebuffer = reinterpret_cast<PFN_vkCreateFramebuffer>(
      vkGetDeviceProcAddr(vk_device, "vkCreateFramebuffer"));
  if (!vkCreateFramebuffer) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateFramebuffer";
    return false;
  }

  vkCreateGraphicsPipelines = reinterpret_cast<PFN_vkCreateGraphicsPipelines>(
      vkGetDeviceProcAddr(vk_device, "vkCreateGraphicsPipelines"));
  if (!vkCreateGraphicsPipelines) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateGraphicsPipelines";
    return false;
  }

  vkCreateImage = reinterpret_cast<PFN_vkCreateImage>(
      vkGetDeviceProcAddr(vk_device, "vkCreateImage"));
  if (!vkCreateImage) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateImage";
    return false;
  }

  vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(
      vkGetDeviceProcAddr(vk_device, "vkCreateImageView"));
  if (!vkCreateImageView) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateImageView";
    return false;
  }

  vkCreateRenderPass = reinterpret_cast<PFN_vkCreateRenderPass>(
      vkGetDeviceProcAddr(vk_device, "vkCreateRenderPass"));
  if (!vkCreateRenderPass) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateRenderPass";
    return false;
  }

  vkCreateSampler = reinterpret_cast<PFN_vkCreateSampler>(
      vkGetDeviceProcAddr(vk_device, "vkCreateSampler"));
  if (!vkCreateSampler) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateSampler";
    return false;
  }

  vkCreateSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(
      vkGetDeviceProcAddr(vk_device, "vkCreateSemaphore"));
  if (!vkCreateSemaphore) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateSemaphore";
    return false;
  }

  vkCreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(
      vkGetDeviceProcAddr(vk_device, "vkCreateShaderModule"));
  if (!vkCreateShaderModule) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateShaderModule";
    return false;
  }

  vkDestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyBuffer"));
  if (!vkDestroyBuffer) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyBuffer";
    return false;
  }

  vkDestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyCommandPool"));
  if (!vkDestroyCommandPool) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyCommandPool";
    return false;
  }

  vkDestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyDescriptorPool"));
  if (!vkDestroyDescriptorPool) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyDescriptorPool";
    return false;
  }

  vkDestroyDescriptorSetLayout =
      reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(
          vkGetDeviceProcAddr(vk_device, "vkDestroyDescriptorSetLayout"));
  if (!vkDestroyDescriptorSetLayout) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyDescriptorSetLayout";
    return false;
  }

  vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyDevice"));
  if (!vkDestroyDevice) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyDevice";
    return false;
  }

  vkDestroyFence = reinterpret_cast<PFN_vkDestroyFence>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyFence"));
  if (!vkDestroyFence) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyFence";
    return false;
  }

  vkDestroyFramebuffer = reinterpret_cast<PFN_vkDestroyFramebuffer>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyFramebuffer"));
  if (!vkDestroyFramebuffer) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyFramebuffer";
    return false;
  }

  vkDestroyImage = reinterpret_cast<PFN_vkDestroyImage>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyImage"));
  if (!vkDestroyImage) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyImage";
    return false;
  }

  vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyImageView"));
  if (!vkDestroyImageView) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyImageView";
    return false;
  }

  vkDestroyRenderPass = reinterpret_cast<PFN_vkDestroyRenderPass>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyRenderPass"));
  if (!vkDestroyRenderPass) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyRenderPass";
    return false;
  }

  vkDestroySampler = reinterpret_cast<PFN_vkDestroySampler>(
      vkGetDeviceProcAddr(vk_device, "vkDestroySampler"));
  if (!vkDestroySampler) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroySampler";
    return false;
  }

  vkDestroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(
      vkGetDeviceProcAddr(vk_device, "vkDestroySemaphore"));
  if (!vkDestroySemaphore) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroySemaphore";
    return false;
  }

  vkDestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyShaderModule"));
  if (!vkDestroyShaderModule) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyShaderModule";
    return false;
  }

  vkDeviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(
      vkGetDeviceProcAddr(vk_device, "vkDeviceWaitIdle"));
  if (!vkDeviceWaitIdle) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDeviceWaitIdle";
    return false;
  }

  vkFlushMappedMemoryRanges = reinterpret_cast<PFN_vkFlushMappedMemoryRanges>(
      vkGetDeviceProcAddr(vk_device, "vkFlushMappedMemoryRanges"));
  if (!vkFlushMappedMemoryRanges) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkFlushMappedMemoryRanges";
    return false;
  }

  vkEndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(
      vkGetDeviceProcAddr(vk_device, "vkEndCommandBuffer"));
  if (!vkEndCommandBuffer) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEndCommandBuffer";
    return false;
  }

  vkFreeCommandBuffers = reinterpret_cast<PFN_vkFreeCommandBuffers>(
      vkGetDeviceProcAddr(vk_device, "vkFreeCommandBuffers"));
  if (!vkFreeCommandBuffers) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkFreeCommandBuffers";
    return false;
  }

  vkFreeDescriptorSets = reinterpret_cast<PFN_vkFreeDescriptorSets>(
      vkGetDeviceProcAddr(vk_device, "vkFreeDescriptorSets"));
  if (!vkFreeDescriptorSets) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkFreeDescriptorSets";
    return false;
  }

  vkFreeMemory = reinterpret_cast<PFN_vkFreeMemory>(
      vkGetDeviceProcAddr(vk_device, "vkFreeMemory"));
  if (!vkFreeMemory) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkFreeMemory";
    return false;
  }

  vkInvalidateMappedMemoryRanges =
      reinterpret_cast<PFN_vkInvalidateMappedMemoryRanges>(
          vkGetDeviceProcAddr(vk_device, "vkInvalidateMappedMemoryRanges"));
  if (!vkInvalidateMappedMemoryRanges) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkInvalidateMappedMemoryRanges";
    return false;
  }

  vkGetBufferMemoryRequirements =
      reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(
          vkGetDeviceProcAddr(vk_device, "vkGetBufferMemoryRequirements"));
  if (!vkGetBufferMemoryRequirements) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetBufferMemoryRequirements";
    return false;
  }

  vkGetBufferMemoryRequirements2 =
      reinterpret_cast<PFN_vkGetBufferMemoryRequirements2>(
          vkGetDeviceProcAddr(vk_device, "vkGetBufferMemoryRequirements2"));
  if (!vkGetBufferMemoryRequirements2) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetBufferMemoryRequirements2";
    return false;
  }

  vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(
      vkGetDeviceProcAddr(vk_device, "vkGetDeviceQueue"));
  if (!vkGetDeviceQueue) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetDeviceQueue";
    return false;
  }

  vkGetDeviceQueue2 = reinterpret_cast<PFN_vkGetDeviceQueue2>(
      vkGetDeviceProcAddr(vk_device, "vkGetDeviceQueue2"));
  if (!vkGetDeviceQueue2) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetDeviceQueue2";
    return false;
  }

  vkGetFenceStatus = reinterpret_cast<PFN_vkGetFenceStatus>(
      vkGetDeviceProcAddr(vk_device, "vkGetFenceStatus"));
  if (!vkGetFenceStatus) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetFenceStatus";
    return false;
  }

  vkGetImageMemoryRequirements =
      reinterpret_cast<PFN_vkGetImageMemoryRequirements>(
          vkGetDeviceProcAddr(vk_device, "vkGetImageMemoryRequirements"));
  if (!vkGetImageMemoryRequirements) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetImageMemoryRequirements";
    return false;
  }

  vkGetImageMemoryRequirements2 =
      reinterpret_cast<PFN_vkGetImageMemoryRequirements2>(
          vkGetDeviceProcAddr(vk_device, "vkGetImageMemoryRequirements2"));
  if (!vkGetImageMemoryRequirements2) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetImageMemoryRequirements2";
    return false;
  }

  vkGetImageSubresourceLayout =
      reinterpret_cast<PFN_vkGetImageSubresourceLayout>(
          vkGetDeviceProcAddr(vk_device, "vkGetImageSubresourceLayout"));
  if (!vkGetImageSubresourceLayout) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetImageSubresourceLayout";
    return false;
  }

  vkMapMemory = reinterpret_cast<PFN_vkMapMemory>(
      vkGetDeviceProcAddr(vk_device, "vkMapMemory"));
  if (!vkMapMemory) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkMapMemory";
    return false;
  }

  vkQueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(
      vkGetDeviceProcAddr(vk_device, "vkQueueSubmit"));
  if (!vkQueueSubmit) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkQueueSubmit";
    return false;
  }

  vkQueueWaitIdle = reinterpret_cast<PFN_vkQueueWaitIdle>(
      vkGetDeviceProcAddr(vk_device, "vkQueueWaitIdle"));
  if (!vkQueueWaitIdle) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkQueueWaitIdle";
    return false;
  }

  vkResetCommandBuffer = reinterpret_cast<PFN_vkResetCommandBuffer>(
      vkGetDeviceProcAddr(vk_device, "vkResetCommandBuffer"));
  if (!vkResetCommandBuffer) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkResetCommandBuffer";
    return false;
  }

  vkResetFences = reinterpret_cast<PFN_vkResetFences>(
      vkGetDeviceProcAddr(vk_device, "vkResetFences"));
  if (!vkResetFences) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkResetFences";
    return false;
  }

  vkUnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(
      vkGetDeviceProcAddr(vk_device, "vkUnmapMemory"));
  if (!vkUnmapMemory) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkUnmapMemory";
    return false;
  }

  vkUpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(
      vkGetDeviceProcAddr(vk_device, "vkUpdateDescriptorSets"));
  if (!vkUpdateDescriptorSets) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkUpdateDescriptorSets";
    return false;
  }

  vkWaitForFences = reinterpret_cast<PFN_vkWaitForFences>(
      vkGetDeviceProcAddr(vk_device, "vkWaitForFences"));
  if (!vkWaitForFences) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkWaitForFences";
    return false;
  }

#if BUILDFLAG(IS_ANDROID)
  if (gfx::HasExtension(
          enabled_extensions,
          VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME)) {
    vkGetAndroidHardwareBufferPropertiesANDROID =
        reinterpret_cast<PFN_vkGetAndroidHardwareBufferPropertiesANDROID>(
            vkGetDeviceProcAddr(vk_device,
                                "vkGetAndroidHardwareBufferPropertiesANDROID"));
    if (!vkGetAndroidHardwareBufferPropertiesANDROID) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetAndroidHardwareBufferPropertiesANDROID";
      return false;
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_POSIX)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME)) {
    vkGetSemaphoreFdKHR = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(
        vkGetDeviceProcAddr(vk_device, "vkGetSemaphoreFdKHR"));
    if (!vkGetSemaphoreFdKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetSemaphoreFdKHR";
      return false;
    }

    vkImportSemaphoreFdKHR = reinterpret_cast<PFN_vkImportSemaphoreFdKHR>(
        vkGetDeviceProcAddr(vk_device, "vkImportSemaphoreFdKHR"));
    if (!vkImportSemaphoreFdKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkImportSemaphoreFdKHR";
      return false;
    }
  }
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_WIN)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME)) {
    vkGetSemaphoreWin32HandleKHR =
        reinterpret_cast<PFN_vkGetSemaphoreWin32HandleKHR>(
            vkGetDeviceProcAddr(vk_device, "vkGetSemaphoreWin32HandleKHR"));
    if (!vkGetSemaphoreWin32HandleKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetSemaphoreWin32HandleKHR";
      return false;
    }

    vkImportSemaphoreWin32HandleKHR =
        reinterpret_cast<PFN_vkImportSemaphoreWin32HandleKHR>(
            vkGetDeviceProcAddr(vk_device, "vkImportSemaphoreWin32HandleKHR"));
    if (!vkImportSemaphoreWin32HandleKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkImportSemaphoreWin32HandleKHR";
      return false;
    }
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_POSIX)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME)) {
    vkGetMemoryFdKHR = reinterpret_cast<PFN_vkGetMemoryFdKHR>(
        vkGetDeviceProcAddr(vk_device, "vkGetMemoryFdKHR"));
    if (!vkGetMemoryFdKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetMemoryFdKHR";
      return false;
    }

    vkGetMemoryFdPropertiesKHR =
        reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(
            vkGetDeviceProcAddr(vk_device, "vkGetMemoryFdPropertiesKHR"));
    if (!vkGetMemoryFdPropertiesKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetMemoryFdPropertiesKHR";
      return false;
    }
  }
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_WIN)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME)) {
    vkGetMemoryWin32HandleKHR = reinterpret_cast<PFN_vkGetMemoryWin32HandleKHR>(
        vkGetDeviceProcAddr(vk_device, "vkGetMemoryWin32HandleKHR"));
    if (!vkGetMemoryWin32HandleKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetMemoryWin32HandleKHR";
      return false;
    }

    vkGetMemoryWin32HandlePropertiesKHR =
        reinterpret_cast<PFN_vkGetMemoryWin32HandlePropertiesKHR>(
            vkGetDeviceProcAddr(vk_device,
                                "vkGetMemoryWin32HandlePropertiesKHR"));
    if (!vkGetMemoryWin32HandlePropertiesKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetMemoryWin32HandlePropertiesKHR";
      return false;
    }
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_FUCHSIA)
  if (gfx::HasExtension(enabled_extensions,
                        VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME)) {
    vkImportSemaphoreZirconHandleFUCHSIA =
        reinterpret_cast<PFN_vkImportSemaphoreZirconHandleFUCHSIA>(
            vkGetDeviceProcAddr(vk_device,
                                "vkImportSemaphoreZirconHandleFUCHSIA"));
    if (!vkImportSemaphoreZirconHandleFUCHSIA) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkImportSemaphoreZirconHandleFUCHSIA";
      return false;
    }

    vkGetSemaphoreZirconHandleFUCHSIA =
        reinterpret_cast<PFN_vkGetSemaphoreZirconHandleFUCHSIA>(
            vkGetDeviceProcAddr(vk_device,
                                "vkGetSemaphoreZirconHandleFUCHSIA"));
    if (!vkGetSemaphoreZirconHandleFUCHSIA) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetSemaphoreZirconHandleFUCHSIA";
      return false;
    }
  }
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_FUCHSIA)
  if (gfx::HasExtension(enabled_extensions,
                        VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME)) {
    vkGetMemoryZirconHandleFUCHSIA =
        reinterpret_cast<PFN_vkGetMemoryZirconHandleFUCHSIA>(
            vkGetDeviceProcAddr(vk_device, "vkGetMemoryZirconHandleFUCHSIA"));
    if (!vkGetMemoryZirconHandleFUCHSIA) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetMemoryZirconHandleFUCHSIA";
      return false;
    }
  }
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_FUCHSIA)
  if (gfx::HasExtension(enabled_extensions,
                        VK_FUCHSIA_BUFFER_COLLECTION_EXTENSION_NAME)) {
    vkCreateBufferCollectionFUCHSIA =
        reinterpret_cast<PFN_vkCreateBufferCollectionFUCHSIA>(
            vkGetDeviceProcAddr(vk_device, "vkCreateBufferCollectionFUCHSIA"));
    if (!vkCreateBufferCollectionFUCHSIA) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateBufferCollectionFUCHSIA";
      return false;
    }

    vkSetBufferCollectionImageConstraintsFUCHSIA =
        reinterpret_cast<PFN_vkSetBufferCollectionImageConstraintsFUCHSIA>(
            vkGetDeviceProcAddr(
                vk_device, "vkSetBufferCollectionImageConstraintsFUCHSIA"));
    if (!vkSetBufferCollectionImageConstraintsFUCHSIA) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkSetBufferCollectionImageConstraintsFUCHSIA";
      return false;
    }

    vkGetBufferCollectionPropertiesFUCHSIA =
        reinterpret_cast<PFN_vkGetBufferCollectionPropertiesFUCHSIA>(
            vkGetDeviceProcAddr(vk_device,
                                "vkGetBufferCollectionPropertiesFUCHSIA"));
    if (!vkGetBufferCollectionPropertiesFUCHSIA) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetBufferCollectionPropertiesFUCHSIA";
      return false;
    }

    vkDestroyBufferCollectionFUCHSIA =
        reinterpret_cast<PFN_vkDestroyBufferCollectionFUCHSIA>(
            vkGetDeviceProcAddr(vk_device, "vkDestroyBufferCollectionFUCHSIA"));
    if (!vkDestroyBufferCollectionFUCHSIA) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkDestroyBufferCollectionFUCHSIA";
      return false;
    }
  }
#endif  // BUILDFLAG(IS_FUCHSIA)

  if (gfx::HasExtension(enabled_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
    vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(
        vkGetDeviceProcAddr(vk_device, "vkAcquireNextImageKHR"));
    if (!vkAcquireNextImageKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkAcquireNextImageKHR";
      return false;
    }

    vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(
        vkGetDeviceProcAddr(vk_device, "vkCreateSwapchainKHR"));
    if (!vkCreateSwapchainKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateSwapchainKHR";
      return false;
    }

    vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(
        vkGetDeviceProcAddr(vk_device, "vkDestroySwapchainKHR"));
    if (!vkDestroySwapchainKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkDestroySwapchainKHR";
      return false;
    }

    vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(
        vkGetDeviceProcAddr(vk_device, "vkGetSwapchainImagesKHR"));
    if (!vkGetSwapchainImagesKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetSwapchainImagesKHR";
      return false;
    }

    vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(
        vkGetDeviceProcAddr(vk_device, "vkQueuePresentKHR"));
    if (!vkQueuePresentKHR) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkQueuePresentKHR";
      return false;
    }
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (gfx::HasExtension(enabled_extensions,
                        VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME)) {
    vkGetImageDrmFormatModifierPropertiesEXT =
        reinterpret_cast<PFN_vkGetImageDrmFormatModifierPropertiesEXT>(
            vkGetDeviceProcAddr(vk_device,
                                "vkGetImageDrmFormatModifierPropertiesEXT"));
    if (!vkGetImageDrmFormatModifierPropertiesEXT) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetImageDrmFormatModifierPropertiesEXT";
      return false;
    }
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  return true;
}

void VulkanFunctionPointers::ResetForTesting() {
  base::AutoLock lock(write_lock_);

  per_queue_lock_map.clear();
  loader_library_ = nullptr;
  vkGetInstanceProcAddr = nullptr;

  vkEnumerateInstanceVersion = nullptr;
  vkCreateInstance = nullptr;
  vkEnumerateInstanceExtensionProperties = nullptr;
  vkEnumerateInstanceLayerProperties = nullptr;

  vkCreateDevice = nullptr;
  vkDestroyInstance = nullptr;
  vkEnumerateDeviceExtensionProperties = nullptr;
  vkEnumerateDeviceLayerProperties = nullptr;
  vkEnumeratePhysicalDevices = nullptr;
  vkGetDeviceProcAddr = nullptr;
  vkGetPhysicalDeviceFeatures2 = nullptr;
  vkGetPhysicalDeviceFormatProperties = nullptr;
  vkGetPhysicalDeviceFormatProperties2 = nullptr;
  vkGetPhysicalDeviceImageFormatProperties2 = nullptr;
  vkGetPhysicalDeviceMemoryProperties = nullptr;
  vkGetPhysicalDeviceMemoryProperties2 = nullptr;
  vkGetPhysicalDeviceProperties = nullptr;
  vkGetPhysicalDeviceProperties2 = nullptr;
  vkGetPhysicalDeviceQueueFamilyProperties = nullptr;

#if DCHECK_IS_ON()
  vkCreateDebugReportCallbackEXT = nullptr;
  vkDestroyDebugReportCallbackEXT = nullptr;
#endif  // DCHECK_IS_ON()

  vkDestroySurfaceKHR = nullptr;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
  vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
  vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;

  vkCreateHeadlessSurfaceEXT = nullptr;

#if defined(USE_VULKAN_XCB)
  vkCreateXcbSurfaceKHR = nullptr;
  vkGetPhysicalDeviceXcbPresentationSupportKHR = nullptr;
#endif  // defined(USE_VULKAN_XCB)

#if BUILDFLAG(IS_WIN)
  vkCreateWin32SurfaceKHR = nullptr;
  vkGetPhysicalDeviceWin32PresentationSupportKHR = nullptr;
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
  vkCreateAndroidSurfaceKHR = nullptr;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_FUCHSIA)
  vkCreateImagePipeSurfaceFUCHSIA = nullptr;
#endif  // BUILDFLAG(IS_FUCHSIA)

  vkAllocateCommandBuffers = nullptr;
  vkAllocateDescriptorSets = nullptr;
  vkAllocateMemory = nullptr;
  vkBeginCommandBuffer = nullptr;
  vkBindBufferMemory = nullptr;
  vkBindBufferMemory2 = nullptr;
  vkBindImageMemory = nullptr;
  vkBindImageMemory2 = nullptr;
  vkCmdBeginRenderPass = nullptr;
  vkCmdCopyBuffer = nullptr;
  vkCmdCopyBufferToImage = nullptr;
  vkCmdCopyImageToBuffer = nullptr;
  vkCmdEndRenderPass = nullptr;
  vkCmdExecuteCommands = nullptr;
  vkCmdNextSubpass = nullptr;
  vkCmdPipelineBarrier = nullptr;
  vkCreateBuffer = nullptr;
  vkCreateCommandPool = nullptr;
  vkCreateDescriptorPool = nullptr;
  vkCreateDescriptorSetLayout = nullptr;
  vkCreateFence = nullptr;
  vkCreateFramebuffer = nullptr;
  vkCreateGraphicsPipelines = nullptr;
  vkCreateImage = nullptr;
  vkCreateImageView = nullptr;
  vkCreateRenderPass = nullptr;
  vkCreateSampler = nullptr;
  vkCreateSemaphore = nullptr;
  vkCreateShaderModule = nullptr;
  vkDestroyBuffer = nullptr;
  vkDestroyCommandPool = nullptr;
  vkDestroyDescriptorPool = nullptr;
  vkDestroyDescriptorSetLayout = nullptr;
  vkDestroyDevice = nullptr;
  vkDestroyFence = nullptr;
  vkDestroyFramebuffer = nullptr;
  vkDestroyImage = nullptr;
  vkDestroyImageView = nullptr;
  vkDestroyRenderPass = nullptr;
  vkDestroySampler = nullptr;
  vkDestroySemaphore = nullptr;
  vkDestroyShaderModule = nullptr;
  vkDeviceWaitIdle = nullptr;
  vkFlushMappedMemoryRanges = nullptr;
  vkEndCommandBuffer = nullptr;
  vkFreeCommandBuffers = nullptr;
  vkFreeDescriptorSets = nullptr;
  vkFreeMemory = nullptr;
  vkInvalidateMappedMemoryRanges = nullptr;
  vkGetBufferMemoryRequirements = nullptr;
  vkGetBufferMemoryRequirements2 = nullptr;
  vkGetDeviceQueue = nullptr;
  vkGetDeviceQueue2 = nullptr;
  vkGetFenceStatus = nullptr;
  vkGetImageMemoryRequirements = nullptr;
  vkGetImageMemoryRequirements2 = nullptr;
  vkGetImageSubresourceLayout = nullptr;
  vkMapMemory = nullptr;
  vkQueueSubmit = nullptr;
  vkQueueWaitIdle = nullptr;
  vkResetCommandBuffer = nullptr;
  vkResetFences = nullptr;
  vkUnmapMemory = nullptr;
  vkUpdateDescriptorSets = nullptr;
  vkWaitForFences = nullptr;

#if BUILDFLAG(IS_ANDROID)
  vkGetAndroidHardwareBufferPropertiesANDROID = nullptr;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_POSIX)
  vkGetSemaphoreFdKHR = nullptr;
  vkImportSemaphoreFdKHR = nullptr;
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_WIN)
  vkGetSemaphoreWin32HandleKHR = nullptr;
  vkImportSemaphoreWin32HandleKHR = nullptr;
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_POSIX)
  vkGetMemoryFdKHR = nullptr;
  vkGetMemoryFdPropertiesKHR = nullptr;
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_WIN)
  vkGetMemoryWin32HandleKHR = nullptr;
  vkGetMemoryWin32HandlePropertiesKHR = nullptr;
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_FUCHSIA)
  vkImportSemaphoreZirconHandleFUCHSIA = nullptr;
  vkGetSemaphoreZirconHandleFUCHSIA = nullptr;
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_FUCHSIA)
  vkGetMemoryZirconHandleFUCHSIA = nullptr;
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_FUCHSIA)
  vkCreateBufferCollectionFUCHSIA = nullptr;
  vkSetBufferCollectionImageConstraintsFUCHSIA = nullptr;
  vkGetBufferCollectionPropertiesFUCHSIA = nullptr;
  vkDestroyBufferCollectionFUCHSIA = nullptr;
#endif  // BUILDFLAG(IS_FUCHSIA)

  vkAcquireNextImageKHR = nullptr;
  vkCreateSwapchainKHR = nullptr;
  vkDestroySwapchainKHR = nullptr;
  vkGetSwapchainImagesKHR = nullptr;
  vkQueuePresentKHR = nullptr;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  vkGetImageDrmFormatModifierPropertiesEXT = nullptr;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
}

}  // namespace gpu
