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

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/no_destructor.h"

namespace gpu {

namespace {
NOINLINE void LogGetProcError(const char* funcName) {
  LOG(WARNING) << "Failed to bind vulkan entrypoint: " << funcName;
}
}  // namespace

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
  if (!vkGetInstanceProcAddr) {
    LOG(WARNING) << "Failed to find vkGetInstanceProcAddr";
    return false;
  }
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
  constexpr char kvkEnumerateInstanceVersion[] = "vkEnumerateInstanceVersion";
  vkEnumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
      vkGetInstanceProcAddr(nullptr, kvkEnumerateInstanceVersion));
  if (!vkEnumerateInstanceVersion) {
    LogGetProcError(kvkEnumerateInstanceVersion);
    return false;
  }

  constexpr char kvkCreateInstance[] = "vkCreateInstance";
  vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
      vkGetInstanceProcAddr(nullptr, kvkCreateInstance));
  if (!vkCreateInstance) {
    LogGetProcError(kvkCreateInstance);
    return false;
  }

  constexpr char kvkEnumerateInstanceExtensionProperties[] =
      "vkEnumerateInstanceExtensionProperties";
  vkEnumerateInstanceExtensionProperties =
      reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
          vkGetInstanceProcAddr(nullptr,
                                kvkEnumerateInstanceExtensionProperties));
  if (!vkEnumerateInstanceExtensionProperties) {
    LogGetProcError(kvkEnumerateInstanceExtensionProperties);
    return false;
  }

  constexpr char kvkEnumerateInstanceLayerProperties[] =
      "vkEnumerateInstanceLayerProperties";
  vkEnumerateInstanceLayerProperties =
      reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(
          vkGetInstanceProcAddr(nullptr, kvkEnumerateInstanceLayerProperties));
  if (!vkEnumerateInstanceLayerProperties) {
    LogGetProcError(kvkEnumerateInstanceLayerProperties);
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
  constexpr char kvkCreateDevice[] = "vkCreateDevice";
  vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(
      vkGetInstanceProcAddr(vk_instance, kvkCreateDevice));
  if (!vkCreateDevice) {
    LogGetProcError(kvkCreateDevice);
    return false;
  }

  constexpr char kvkDestroyInstance[] = "vkDestroyInstance";
  vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
      vkGetInstanceProcAddr(vk_instance, kvkDestroyInstance));
  if (!vkDestroyInstance) {
    LogGetProcError(kvkDestroyInstance);
    return false;
  }

  constexpr char kvkEnumerateDeviceExtensionProperties[] =
      "vkEnumerateDeviceExtensionProperties";
  vkEnumerateDeviceExtensionProperties =
      reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                kvkEnumerateDeviceExtensionProperties));
  if (!vkEnumerateDeviceExtensionProperties) {
    LogGetProcError(kvkEnumerateDeviceExtensionProperties);
    return false;
  }

  constexpr char kvkEnumerateDeviceLayerProperties[] =
      "vkEnumerateDeviceLayerProperties";
  vkEnumerateDeviceLayerProperties =
      reinterpret_cast<PFN_vkEnumerateDeviceLayerProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                kvkEnumerateDeviceLayerProperties));
  if (!vkEnumerateDeviceLayerProperties) {
    LogGetProcError(kvkEnumerateDeviceLayerProperties);
    return false;
  }

  constexpr char kvkEnumeratePhysicalDevices[] = "vkEnumeratePhysicalDevices";
  vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
      vkGetInstanceProcAddr(vk_instance, kvkEnumeratePhysicalDevices));
  if (!vkEnumeratePhysicalDevices) {
    LogGetProcError(kvkEnumeratePhysicalDevices);
    return false;
  }

  constexpr char kvkGetDeviceProcAddr[] = "vkGetDeviceProcAddr";
  vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
      vkGetInstanceProcAddr(vk_instance, kvkGetDeviceProcAddr));
  if (!vkGetDeviceProcAddr) {
    LogGetProcError(kvkGetDeviceProcAddr);
    return false;
  }

  constexpr char kvkGetPhysicalDeviceExternalSemaphoreProperties[] =
      "vkGetPhysicalDeviceExternalSemaphoreProperties";
  vkGetPhysicalDeviceExternalSemaphoreProperties =
      reinterpret_cast<PFN_vkGetPhysicalDeviceExternalSemaphoreProperties>(
          vkGetInstanceProcAddr(
              vk_instance, kvkGetPhysicalDeviceExternalSemaphoreProperties));
  if (!vkGetPhysicalDeviceExternalSemaphoreProperties) {
    LogGetProcError(kvkGetPhysicalDeviceExternalSemaphoreProperties);
    return false;
  }

  constexpr char kvkGetPhysicalDeviceFeatures2[] =
      "vkGetPhysicalDeviceFeatures2";
  vkGetPhysicalDeviceFeatures2 =
      reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
          vkGetInstanceProcAddr(vk_instance, kvkGetPhysicalDeviceFeatures2));
  if (!vkGetPhysicalDeviceFeatures2) {
    LogGetProcError(kvkGetPhysicalDeviceFeatures2);
    return false;
  }

  constexpr char kvkGetPhysicalDeviceFormatProperties[] =
      "vkGetPhysicalDeviceFormatProperties";
  vkGetPhysicalDeviceFormatProperties =
      reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                kvkGetPhysicalDeviceFormatProperties));
  if (!vkGetPhysicalDeviceFormatProperties) {
    LogGetProcError(kvkGetPhysicalDeviceFormatProperties);
    return false;
  }

  constexpr char kvkGetPhysicalDeviceFormatProperties2[] =
      "vkGetPhysicalDeviceFormatProperties2";
  vkGetPhysicalDeviceFormatProperties2 =
      reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties2>(
          vkGetInstanceProcAddr(vk_instance,
                                kvkGetPhysicalDeviceFormatProperties2));
  if (!vkGetPhysicalDeviceFormatProperties2) {
    LogGetProcError(kvkGetPhysicalDeviceFormatProperties2);
    return false;
  }

  constexpr char kvkGetPhysicalDeviceImageFormatProperties2[] =
      "vkGetPhysicalDeviceImageFormatProperties2";
  vkGetPhysicalDeviceImageFormatProperties2 =
      reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(
          vkGetInstanceProcAddr(vk_instance,
                                kvkGetPhysicalDeviceImageFormatProperties2));
  if (!vkGetPhysicalDeviceImageFormatProperties2) {
    LogGetProcError(kvkGetPhysicalDeviceImageFormatProperties2);
    return false;
  }

  constexpr char kvkGetPhysicalDeviceMemoryProperties[] =
      "vkGetPhysicalDeviceMemoryProperties";
  vkGetPhysicalDeviceMemoryProperties =
      reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                kvkGetPhysicalDeviceMemoryProperties));
  if (!vkGetPhysicalDeviceMemoryProperties) {
    LogGetProcError(kvkGetPhysicalDeviceMemoryProperties);
    return false;
  }

  constexpr char kvkGetPhysicalDeviceMemoryProperties2[] =
      "vkGetPhysicalDeviceMemoryProperties2";
  vkGetPhysicalDeviceMemoryProperties2 =
      reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties2>(
          vkGetInstanceProcAddr(vk_instance,
                                kvkGetPhysicalDeviceMemoryProperties2));
  if (!vkGetPhysicalDeviceMemoryProperties2) {
    LogGetProcError(kvkGetPhysicalDeviceMemoryProperties2);
    return false;
  }

  constexpr char kvkGetPhysicalDeviceProperties[] =
      "vkGetPhysicalDeviceProperties";
  vkGetPhysicalDeviceProperties =
      reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
          vkGetInstanceProcAddr(vk_instance, kvkGetPhysicalDeviceProperties));
  if (!vkGetPhysicalDeviceProperties) {
    LogGetProcError(kvkGetPhysicalDeviceProperties);
    return false;
  }

  constexpr char kvkGetPhysicalDeviceProperties2[] =
      "vkGetPhysicalDeviceProperties2";
  vkGetPhysicalDeviceProperties2 =
      reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
          vkGetInstanceProcAddr(vk_instance, kvkGetPhysicalDeviceProperties2));
  if (!vkGetPhysicalDeviceProperties2) {
    LogGetProcError(kvkGetPhysicalDeviceProperties2);
    return false;
  }

  constexpr char kvkGetPhysicalDeviceQueueFamilyProperties[] =
      "vkGetPhysicalDeviceQueueFamilyProperties";
  vkGetPhysicalDeviceQueueFamilyProperties =
      reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                kvkGetPhysicalDeviceQueueFamilyProperties));
  if (!vkGetPhysicalDeviceQueueFamilyProperties) {
    LogGetProcError(kvkGetPhysicalDeviceQueueFamilyProperties);
    return false;
  }

#if DCHECK_IS_ON()
  if (gfx::HasExtension(enabled_extensions,
                        VK_EXT_DEBUG_REPORT_EXTENSION_NAME)) {
    constexpr char kvkCreateDebugReportCallbackEXT[] =
        "vkCreateDebugReportCallbackEXT";
    vkCreateDebugReportCallbackEXT =
        reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(
            vkGetInstanceProcAddr(vk_instance,
                                  kvkCreateDebugReportCallbackEXT));
    if (!vkCreateDebugReportCallbackEXT) {
      LogGetProcError(kvkCreateDebugReportCallbackEXT);
      return false;
    }

    constexpr char kvkDestroyDebugReportCallbackEXT[] =
        "vkDestroyDebugReportCallbackEXT";
    vkDestroyDebugReportCallbackEXT =
        reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
            vkGetInstanceProcAddr(vk_instance,
                                  kvkDestroyDebugReportCallbackEXT));
    if (!vkDestroyDebugReportCallbackEXT) {
      LogGetProcError(kvkDestroyDebugReportCallbackEXT);
      return false;
    }
  }
#endif  // DCHECK_IS_ON()

  if (gfx::HasExtension(enabled_extensions, VK_KHR_SURFACE_EXTENSION_NAME)) {
    constexpr char kvkDestroySurfaceKHR[] = "vkDestroySurfaceKHR";
    vkDestroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
        vkGetInstanceProcAddr(vk_instance, kvkDestroySurfaceKHR));
    if (!vkDestroySurfaceKHR) {
      LogGetProcError(kvkDestroySurfaceKHR);
      return false;
    }

    constexpr char kvkGetPhysicalDeviceSurfaceCapabilitiesKHR[] =
        "vkGetPhysicalDeviceSurfaceCapabilitiesKHR";
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
            vkGetInstanceProcAddr(vk_instance,
                                  kvkGetPhysicalDeviceSurfaceCapabilitiesKHR));
    if (!vkGetPhysicalDeviceSurfaceCapabilitiesKHR) {
      LogGetProcError(kvkGetPhysicalDeviceSurfaceCapabilitiesKHR);
      return false;
    }

    constexpr char kvkGetPhysicalDeviceSurfaceFormatsKHR[] =
        "vkGetPhysicalDeviceSurfaceFormatsKHR";
    vkGetPhysicalDeviceSurfaceFormatsKHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
            vkGetInstanceProcAddr(vk_instance,
                                  kvkGetPhysicalDeviceSurfaceFormatsKHR));
    if (!vkGetPhysicalDeviceSurfaceFormatsKHR) {
      LogGetProcError(kvkGetPhysicalDeviceSurfaceFormatsKHR);
      return false;
    }

    constexpr char kvkGetPhysicalDeviceSurfaceSupportKHR[] =
        "vkGetPhysicalDeviceSurfaceSupportKHR";
    vkGetPhysicalDeviceSurfaceSupportKHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
            vkGetInstanceProcAddr(vk_instance,
                                  kvkGetPhysicalDeviceSurfaceSupportKHR));
    if (!vkGetPhysicalDeviceSurfaceSupportKHR) {
      LogGetProcError(kvkGetPhysicalDeviceSurfaceSupportKHR);
      return false;
    }
  }

  if (gfx::HasExtension(enabled_extensions,
                        VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME)) {
    constexpr char kvkCreateHeadlessSurfaceEXT[] = "vkCreateHeadlessSurfaceEXT";
    vkCreateHeadlessSurfaceEXT =
        reinterpret_cast<PFN_vkCreateHeadlessSurfaceEXT>(
            vkGetInstanceProcAddr(vk_instance, kvkCreateHeadlessSurfaceEXT));
    if (!vkCreateHeadlessSurfaceEXT) {
      LogGetProcError(kvkCreateHeadlessSurfaceEXT);
      return false;
    }
  }

#if defined(USE_VULKAN_XCB)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_XCB_SURFACE_EXTENSION_NAME)) {
    constexpr char kvkCreateXcbSurfaceKHR[] = "vkCreateXcbSurfaceKHR";
    vkCreateXcbSurfaceKHR = reinterpret_cast<PFN_vkCreateXcbSurfaceKHR>(
        vkGetInstanceProcAddr(vk_instance, kvkCreateXcbSurfaceKHR));
    if (!vkCreateXcbSurfaceKHR) {
      LogGetProcError(kvkCreateXcbSurfaceKHR);
      return false;
    }

    constexpr char kvkGetPhysicalDeviceXcbPresentationSupportKHR[] =
        "vkGetPhysicalDeviceXcbPresentationSupportKHR";
    vkGetPhysicalDeviceXcbPresentationSupportKHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR>(
            vkGetInstanceProcAddr(
                vk_instance, kvkGetPhysicalDeviceXcbPresentationSupportKHR));
    if (!vkGetPhysicalDeviceXcbPresentationSupportKHR) {
      LogGetProcError(kvkGetPhysicalDeviceXcbPresentationSupportKHR);
      return false;
    }
  }
#endif  // defined(USE_VULKAN_XCB)

#if BUILDFLAG(IS_WIN)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_WIN32_SURFACE_EXTENSION_NAME)) {
    constexpr char kvkCreateWin32SurfaceKHR[] = "vkCreateWin32SurfaceKHR";
    vkCreateWin32SurfaceKHR = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
        vkGetInstanceProcAddr(vk_instance, kvkCreateWin32SurfaceKHR));
    if (!vkCreateWin32SurfaceKHR) {
      LogGetProcError(kvkCreateWin32SurfaceKHR);
      return false;
    }

    constexpr char kvkGetPhysicalDeviceWin32PresentationSupportKHR[] =
        "vkGetPhysicalDeviceWin32PresentationSupportKHR";
    vkGetPhysicalDeviceWin32PresentationSupportKHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR>(
            vkGetInstanceProcAddr(
                vk_instance, kvkGetPhysicalDeviceWin32PresentationSupportKHR));
    if (!vkGetPhysicalDeviceWin32PresentationSupportKHR) {
      LogGetProcError(kvkGetPhysicalDeviceWin32PresentationSupportKHR);
      return false;
    }
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME)) {
    constexpr char kvkCreateAndroidSurfaceKHR[] = "vkCreateAndroidSurfaceKHR";
    vkCreateAndroidSurfaceKHR = reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
        vkGetInstanceProcAddr(vk_instance, kvkCreateAndroidSurfaceKHR));
    if (!vkCreateAndroidSurfaceKHR) {
      LogGetProcError(kvkCreateAndroidSurfaceKHR);
      return false;
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_FUCHSIA)
  if (gfx::HasExtension(enabled_extensions,
                        VK_FUCHSIA_IMAGEPIPE_SURFACE_EXTENSION_NAME)) {
    constexpr char kvkCreateImagePipeSurfaceFUCHSIA[] =
        "vkCreateImagePipeSurfaceFUCHSIA";
    vkCreateImagePipeSurfaceFUCHSIA =
        reinterpret_cast<PFN_vkCreateImagePipeSurfaceFUCHSIA>(
            vkGetInstanceProcAddr(vk_instance,
                                  kvkCreateImagePipeSurfaceFUCHSIA));
    if (!vkCreateImagePipeSurfaceFUCHSIA) {
      LogGetProcError(kvkCreateImagePipeSurfaceFUCHSIA);
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
  constexpr char kvkAllocateCommandBuffers[] = "vkAllocateCommandBuffers";
  vkAllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(
      vkGetDeviceProcAddr(vk_device, kvkAllocateCommandBuffers));
  if (!vkAllocateCommandBuffers) {
    LogGetProcError(kvkAllocateCommandBuffers);
    return false;
  }

  constexpr char kvkAllocateDescriptorSets[] = "vkAllocateDescriptorSets";
  vkAllocateDescriptorSets = reinterpret_cast<PFN_vkAllocateDescriptorSets>(
      vkGetDeviceProcAddr(vk_device, kvkAllocateDescriptorSets));
  if (!vkAllocateDescriptorSets) {
    LogGetProcError(kvkAllocateDescriptorSets);
    return false;
  }

  constexpr char kvkAllocateMemory[] = "vkAllocateMemory";
  vkAllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(
      vkGetDeviceProcAddr(vk_device, kvkAllocateMemory));
  if (!vkAllocateMemory) {
    LogGetProcError(kvkAllocateMemory);
    return false;
  }

  constexpr char kvkBeginCommandBuffer[] = "vkBeginCommandBuffer";
  vkBeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(
      vkGetDeviceProcAddr(vk_device, kvkBeginCommandBuffer));
  if (!vkBeginCommandBuffer) {
    LogGetProcError(kvkBeginCommandBuffer);
    return false;
  }

  constexpr char kvkBindBufferMemory[] = "vkBindBufferMemory";
  vkBindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(
      vkGetDeviceProcAddr(vk_device, kvkBindBufferMemory));
  if (!vkBindBufferMemory) {
    LogGetProcError(kvkBindBufferMemory);
    return false;
  }

  constexpr char kvkBindBufferMemory2[] = "vkBindBufferMemory2";
  vkBindBufferMemory2 = reinterpret_cast<PFN_vkBindBufferMemory2>(
      vkGetDeviceProcAddr(vk_device, kvkBindBufferMemory2));
  if (!vkBindBufferMemory2) {
    LogGetProcError(kvkBindBufferMemory2);
    return false;
  }

  constexpr char kvkBindImageMemory[] = "vkBindImageMemory";
  vkBindImageMemory = reinterpret_cast<PFN_vkBindImageMemory>(
      vkGetDeviceProcAddr(vk_device, kvkBindImageMemory));
  if (!vkBindImageMemory) {
    LogGetProcError(kvkBindImageMemory);
    return false;
  }

  constexpr char kvkBindImageMemory2[] = "vkBindImageMemory2";
  vkBindImageMemory2 = reinterpret_cast<PFN_vkBindImageMemory2>(
      vkGetDeviceProcAddr(vk_device, kvkBindImageMemory2));
  if (!vkBindImageMemory2) {
    LogGetProcError(kvkBindImageMemory2);
    return false;
  }

  constexpr char kvkCmdBeginRenderPass[] = "vkCmdBeginRenderPass";
  vkCmdBeginRenderPass = reinterpret_cast<PFN_vkCmdBeginRenderPass>(
      vkGetDeviceProcAddr(vk_device, kvkCmdBeginRenderPass));
  if (!vkCmdBeginRenderPass) {
    LogGetProcError(kvkCmdBeginRenderPass);
    return false;
  }

  constexpr char kvkCmdBindDescriptorSets[] = "vkCmdBindDescriptorSets";
  vkCmdBindDescriptorSets = reinterpret_cast<PFN_vkCmdBindDescriptorSets>(
      vkGetDeviceProcAddr(vk_device, kvkCmdBindDescriptorSets));
  if (!vkCmdBindDescriptorSets) {
    LogGetProcError(kvkCmdBindDescriptorSets);
    return false;
  }

  constexpr char kvkCmdBindPipeline[] = "vkCmdBindPipeline";
  vkCmdBindPipeline = reinterpret_cast<PFN_vkCmdBindPipeline>(
      vkGetDeviceProcAddr(vk_device, kvkCmdBindPipeline));
  if (!vkCmdBindPipeline) {
    LogGetProcError(kvkCmdBindPipeline);
    return false;
  }

  constexpr char kvkCmdBindVertexBuffers[] = "vkCmdBindVertexBuffers";
  vkCmdBindVertexBuffers = reinterpret_cast<PFN_vkCmdBindVertexBuffers>(
      vkGetDeviceProcAddr(vk_device, kvkCmdBindVertexBuffers));
  if (!vkCmdBindVertexBuffers) {
    LogGetProcError(kvkCmdBindVertexBuffers);
    return false;
  }

  constexpr char kvkCmdCopyBuffer[] = "vkCmdCopyBuffer";
  vkCmdCopyBuffer = reinterpret_cast<PFN_vkCmdCopyBuffer>(
      vkGetDeviceProcAddr(vk_device, kvkCmdCopyBuffer));
  if (!vkCmdCopyBuffer) {
    LogGetProcError(kvkCmdCopyBuffer);
    return false;
  }

  constexpr char kvkCmdCopyBufferToImage[] = "vkCmdCopyBufferToImage";
  vkCmdCopyBufferToImage = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(
      vkGetDeviceProcAddr(vk_device, kvkCmdCopyBufferToImage));
  if (!vkCmdCopyBufferToImage) {
    LogGetProcError(kvkCmdCopyBufferToImage);
    return false;
  }

  constexpr char kvkCmdCopyImage[] = "vkCmdCopyImage";
  vkCmdCopyImage = reinterpret_cast<PFN_vkCmdCopyImage>(
      vkGetDeviceProcAddr(vk_device, kvkCmdCopyImage));
  if (!vkCmdCopyImage) {
    LogGetProcError(kvkCmdCopyImage);
    return false;
  }

  constexpr char kvkCmdCopyImageToBuffer[] = "vkCmdCopyImageToBuffer";
  vkCmdCopyImageToBuffer = reinterpret_cast<PFN_vkCmdCopyImageToBuffer>(
      vkGetDeviceProcAddr(vk_device, kvkCmdCopyImageToBuffer));
  if (!vkCmdCopyImageToBuffer) {
    LogGetProcError(kvkCmdCopyImageToBuffer);
    return false;
  }

  constexpr char kvkCmdDraw[] = "vkCmdDraw";
  vkCmdDraw = reinterpret_cast<PFN_vkCmdDraw>(
      vkGetDeviceProcAddr(vk_device, kvkCmdDraw));
  if (!vkCmdDraw) {
    LogGetProcError(kvkCmdDraw);
    return false;
  }

  constexpr char kvkCmdEndRenderPass[] = "vkCmdEndRenderPass";
  vkCmdEndRenderPass = reinterpret_cast<PFN_vkCmdEndRenderPass>(
      vkGetDeviceProcAddr(vk_device, kvkCmdEndRenderPass));
  if (!vkCmdEndRenderPass) {
    LogGetProcError(kvkCmdEndRenderPass);
    return false;
  }

  constexpr char kvkCmdExecuteCommands[] = "vkCmdExecuteCommands";
  vkCmdExecuteCommands = reinterpret_cast<PFN_vkCmdExecuteCommands>(
      vkGetDeviceProcAddr(vk_device, kvkCmdExecuteCommands));
  if (!vkCmdExecuteCommands) {
    LogGetProcError(kvkCmdExecuteCommands);
    return false;
  }

  constexpr char kvkCmdNextSubpass[] = "vkCmdNextSubpass";
  vkCmdNextSubpass = reinterpret_cast<PFN_vkCmdNextSubpass>(
      vkGetDeviceProcAddr(vk_device, kvkCmdNextSubpass));
  if (!vkCmdNextSubpass) {
    LogGetProcError(kvkCmdNextSubpass);
    return false;
  }

  constexpr char kvkCmdPipelineBarrier[] = "vkCmdPipelineBarrier";
  vkCmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(
      vkGetDeviceProcAddr(vk_device, kvkCmdPipelineBarrier));
  if (!vkCmdPipelineBarrier) {
    LogGetProcError(kvkCmdPipelineBarrier);
    return false;
  }

  constexpr char kvkCmdPushConstants[] = "vkCmdPushConstants";
  vkCmdPushConstants = reinterpret_cast<PFN_vkCmdPushConstants>(
      vkGetDeviceProcAddr(vk_device, kvkCmdPushConstants));
  if (!vkCmdPushConstants) {
    LogGetProcError(kvkCmdPushConstants);
    return false;
  }

  constexpr char kvkCmdSetScissor[] = "vkCmdSetScissor";
  vkCmdSetScissor = reinterpret_cast<PFN_vkCmdSetScissor>(
      vkGetDeviceProcAddr(vk_device, kvkCmdSetScissor));
  if (!vkCmdSetScissor) {
    LogGetProcError(kvkCmdSetScissor);
    return false;
  }

  constexpr char kvkCmdSetViewport[] = "vkCmdSetViewport";
  vkCmdSetViewport = reinterpret_cast<PFN_vkCmdSetViewport>(
      vkGetDeviceProcAddr(vk_device, kvkCmdSetViewport));
  if (!vkCmdSetViewport) {
    LogGetProcError(kvkCmdSetViewport);
    return false;
  }

  constexpr char kvkCreateBuffer[] = "vkCreateBuffer";
  vkCreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(
      vkGetDeviceProcAddr(vk_device, kvkCreateBuffer));
  if (!vkCreateBuffer) {
    LogGetProcError(kvkCreateBuffer);
    return false;
  }

  constexpr char kvkCreateCommandPool[] = "vkCreateCommandPool";
  vkCreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(
      vkGetDeviceProcAddr(vk_device, kvkCreateCommandPool));
  if (!vkCreateCommandPool) {
    LogGetProcError(kvkCreateCommandPool);
    return false;
  }

  constexpr char kvkCreateDescriptorPool[] = "vkCreateDescriptorPool";
  vkCreateDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(
      vkGetDeviceProcAddr(vk_device, kvkCreateDescriptorPool));
  if (!vkCreateDescriptorPool) {
    LogGetProcError(kvkCreateDescriptorPool);
    return false;
  }

  constexpr char kvkCreateDescriptorSetLayout[] = "vkCreateDescriptorSetLayout";
  vkCreateDescriptorSetLayout =
      reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(
          vkGetDeviceProcAddr(vk_device, kvkCreateDescriptorSetLayout));
  if (!vkCreateDescriptorSetLayout) {
    LogGetProcError(kvkCreateDescriptorSetLayout);
    return false;
  }

  constexpr char kvkCreateFence[] = "vkCreateFence";
  vkCreateFence = reinterpret_cast<PFN_vkCreateFence>(
      vkGetDeviceProcAddr(vk_device, kvkCreateFence));
  if (!vkCreateFence) {
    LogGetProcError(kvkCreateFence);
    return false;
  }

  constexpr char kvkCreateFramebuffer[] = "vkCreateFramebuffer";
  vkCreateFramebuffer = reinterpret_cast<PFN_vkCreateFramebuffer>(
      vkGetDeviceProcAddr(vk_device, kvkCreateFramebuffer));
  if (!vkCreateFramebuffer) {
    LogGetProcError(kvkCreateFramebuffer);
    return false;
  }

  constexpr char kvkCreateGraphicsPipelines[] = "vkCreateGraphicsPipelines";
  vkCreateGraphicsPipelines = reinterpret_cast<PFN_vkCreateGraphicsPipelines>(
      vkGetDeviceProcAddr(vk_device, kvkCreateGraphicsPipelines));
  if (!vkCreateGraphicsPipelines) {
    LogGetProcError(kvkCreateGraphicsPipelines);
    return false;
  }

  constexpr char kvkCreateImage[] = "vkCreateImage";
  vkCreateImage = reinterpret_cast<PFN_vkCreateImage>(
      vkGetDeviceProcAddr(vk_device, kvkCreateImage));
  if (!vkCreateImage) {
    LogGetProcError(kvkCreateImage);
    return false;
  }

  constexpr char kvkCreateImageView[] = "vkCreateImageView";
  vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(
      vkGetDeviceProcAddr(vk_device, kvkCreateImageView));
  if (!vkCreateImageView) {
    LogGetProcError(kvkCreateImageView);
    return false;
  }

  constexpr char kvkCreatePipelineLayout[] = "vkCreatePipelineLayout";
  vkCreatePipelineLayout = reinterpret_cast<PFN_vkCreatePipelineLayout>(
      vkGetDeviceProcAddr(vk_device, kvkCreatePipelineLayout));
  if (!vkCreatePipelineLayout) {
    LogGetProcError(kvkCreatePipelineLayout);
    return false;
  }

  constexpr char kvkCreateRenderPass[] = "vkCreateRenderPass";
  vkCreateRenderPass = reinterpret_cast<PFN_vkCreateRenderPass>(
      vkGetDeviceProcAddr(vk_device, kvkCreateRenderPass));
  if (!vkCreateRenderPass) {
    LogGetProcError(kvkCreateRenderPass);
    return false;
  }

  constexpr char kvkCreateSampler[] = "vkCreateSampler";
  vkCreateSampler = reinterpret_cast<PFN_vkCreateSampler>(
      vkGetDeviceProcAddr(vk_device, kvkCreateSampler));
  if (!vkCreateSampler) {
    LogGetProcError(kvkCreateSampler);
    return false;
  }

  constexpr char kvkCreateSemaphore[] = "vkCreateSemaphore";
  vkCreateSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(
      vkGetDeviceProcAddr(vk_device, kvkCreateSemaphore));
  if (!vkCreateSemaphore) {
    LogGetProcError(kvkCreateSemaphore);
    return false;
  }

  constexpr char kvkCreateShaderModule[] = "vkCreateShaderModule";
  vkCreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(
      vkGetDeviceProcAddr(vk_device, kvkCreateShaderModule));
  if (!vkCreateShaderModule) {
    LogGetProcError(kvkCreateShaderModule);
    return false;
  }

  constexpr char kvkDestroyBuffer[] = "vkDestroyBuffer";
  vkDestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(
      vkGetDeviceProcAddr(vk_device, kvkDestroyBuffer));
  if (!vkDestroyBuffer) {
    LogGetProcError(kvkDestroyBuffer);
    return false;
  }

  constexpr char kvkDestroyCommandPool[] = "vkDestroyCommandPool";
  vkDestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(
      vkGetDeviceProcAddr(vk_device, kvkDestroyCommandPool));
  if (!vkDestroyCommandPool) {
    LogGetProcError(kvkDestroyCommandPool);
    return false;
  }

  constexpr char kvkDestroyDescriptorPool[] = "vkDestroyDescriptorPool";
  vkDestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(
      vkGetDeviceProcAddr(vk_device, kvkDestroyDescriptorPool));
  if (!vkDestroyDescriptorPool) {
    LogGetProcError(kvkDestroyDescriptorPool);
    return false;
  }

  constexpr char kvkDestroyDescriptorSetLayout[] =
      "vkDestroyDescriptorSetLayout";
  vkDestroyDescriptorSetLayout =
      reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(
          vkGetDeviceProcAddr(vk_device, kvkDestroyDescriptorSetLayout));
  if (!vkDestroyDescriptorSetLayout) {
    LogGetProcError(kvkDestroyDescriptorSetLayout);
    return false;
  }

  constexpr char kvkDestroyDevice[] = "vkDestroyDevice";
  vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(
      vkGetDeviceProcAddr(vk_device, kvkDestroyDevice));
  if (!vkDestroyDevice) {
    LogGetProcError(kvkDestroyDevice);
    return false;
  }

  constexpr char kvkDestroyFence[] = "vkDestroyFence";
  vkDestroyFence = reinterpret_cast<PFN_vkDestroyFence>(
      vkGetDeviceProcAddr(vk_device, kvkDestroyFence));
  if (!vkDestroyFence) {
    LogGetProcError(kvkDestroyFence);
    return false;
  }

  constexpr char kvkDestroyFramebuffer[] = "vkDestroyFramebuffer";
  vkDestroyFramebuffer = reinterpret_cast<PFN_vkDestroyFramebuffer>(
      vkGetDeviceProcAddr(vk_device, kvkDestroyFramebuffer));
  if (!vkDestroyFramebuffer) {
    LogGetProcError(kvkDestroyFramebuffer);
    return false;
  }

  constexpr char kvkDestroyImage[] = "vkDestroyImage";
  vkDestroyImage = reinterpret_cast<PFN_vkDestroyImage>(
      vkGetDeviceProcAddr(vk_device, kvkDestroyImage));
  if (!vkDestroyImage) {
    LogGetProcError(kvkDestroyImage);
    return false;
  }

  constexpr char kvkDestroyImageView[] = "vkDestroyImageView";
  vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(
      vkGetDeviceProcAddr(vk_device, kvkDestroyImageView));
  if (!vkDestroyImageView) {
    LogGetProcError(kvkDestroyImageView);
    return false;
  }

  constexpr char kvkDestroyPipeline[] = "vkDestroyPipeline";
  vkDestroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(
      vkGetDeviceProcAddr(vk_device, kvkDestroyPipeline));
  if (!vkDestroyPipeline) {
    LogGetProcError(kvkDestroyPipeline);
    return false;
  }

  constexpr char kvkDestroyPipelineLayout[] = "vkDestroyPipelineLayout";
  vkDestroyPipelineLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(
      vkGetDeviceProcAddr(vk_device, kvkDestroyPipelineLayout));
  if (!vkDestroyPipelineLayout) {
    LogGetProcError(kvkDestroyPipelineLayout);
    return false;
  }

  constexpr char kvkDestroyRenderPass[] = "vkDestroyRenderPass";
  vkDestroyRenderPass = reinterpret_cast<PFN_vkDestroyRenderPass>(
      vkGetDeviceProcAddr(vk_device, kvkDestroyRenderPass));
  if (!vkDestroyRenderPass) {
    LogGetProcError(kvkDestroyRenderPass);
    return false;
  }

  constexpr char kvkDestroySampler[] = "vkDestroySampler";
  vkDestroySampler = reinterpret_cast<PFN_vkDestroySampler>(
      vkGetDeviceProcAddr(vk_device, kvkDestroySampler));
  if (!vkDestroySampler) {
    LogGetProcError(kvkDestroySampler);
    return false;
  }

  constexpr char kvkDestroySemaphore[] = "vkDestroySemaphore";
  vkDestroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(
      vkGetDeviceProcAddr(vk_device, kvkDestroySemaphore));
  if (!vkDestroySemaphore) {
    LogGetProcError(kvkDestroySemaphore);
    return false;
  }

  constexpr char kvkDestroyShaderModule[] = "vkDestroyShaderModule";
  vkDestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(
      vkGetDeviceProcAddr(vk_device, kvkDestroyShaderModule));
  if (!vkDestroyShaderModule) {
    LogGetProcError(kvkDestroyShaderModule);
    return false;
  }

  constexpr char kvkDeviceWaitIdle[] = "vkDeviceWaitIdle";
  vkDeviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(
      vkGetDeviceProcAddr(vk_device, kvkDeviceWaitIdle));
  if (!vkDeviceWaitIdle) {
    LogGetProcError(kvkDeviceWaitIdle);
    return false;
  }

  constexpr char kvkFlushMappedMemoryRanges[] = "vkFlushMappedMemoryRanges";
  vkFlushMappedMemoryRanges = reinterpret_cast<PFN_vkFlushMappedMemoryRanges>(
      vkGetDeviceProcAddr(vk_device, kvkFlushMappedMemoryRanges));
  if (!vkFlushMappedMemoryRanges) {
    LogGetProcError(kvkFlushMappedMemoryRanges);
    return false;
  }

  constexpr char kvkEndCommandBuffer[] = "vkEndCommandBuffer";
  vkEndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(
      vkGetDeviceProcAddr(vk_device, kvkEndCommandBuffer));
  if (!vkEndCommandBuffer) {
    LogGetProcError(kvkEndCommandBuffer);
    return false;
  }

  constexpr char kvkFreeCommandBuffers[] = "vkFreeCommandBuffers";
  vkFreeCommandBuffers = reinterpret_cast<PFN_vkFreeCommandBuffers>(
      vkGetDeviceProcAddr(vk_device, kvkFreeCommandBuffers));
  if (!vkFreeCommandBuffers) {
    LogGetProcError(kvkFreeCommandBuffers);
    return false;
  }

  constexpr char kvkFreeDescriptorSets[] = "vkFreeDescriptorSets";
  vkFreeDescriptorSets = reinterpret_cast<PFN_vkFreeDescriptorSets>(
      vkGetDeviceProcAddr(vk_device, kvkFreeDescriptorSets));
  if (!vkFreeDescriptorSets) {
    LogGetProcError(kvkFreeDescriptorSets);
    return false;
  }

  constexpr char kvkFreeMemory[] = "vkFreeMemory";
  vkFreeMemory = reinterpret_cast<PFN_vkFreeMemory>(
      vkGetDeviceProcAddr(vk_device, kvkFreeMemory));
  if (!vkFreeMemory) {
    LogGetProcError(kvkFreeMemory);
    return false;
  }

  constexpr char kvkInvalidateMappedMemoryRanges[] =
      "vkInvalidateMappedMemoryRanges";
  vkInvalidateMappedMemoryRanges =
      reinterpret_cast<PFN_vkInvalidateMappedMemoryRanges>(
          vkGetDeviceProcAddr(vk_device, kvkInvalidateMappedMemoryRanges));
  if (!vkInvalidateMappedMemoryRanges) {
    LogGetProcError(kvkInvalidateMappedMemoryRanges);
    return false;
  }

  constexpr char kvkGetBufferMemoryRequirements[] =
      "vkGetBufferMemoryRequirements";
  vkGetBufferMemoryRequirements =
      reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(
          vkGetDeviceProcAddr(vk_device, kvkGetBufferMemoryRequirements));
  if (!vkGetBufferMemoryRequirements) {
    LogGetProcError(kvkGetBufferMemoryRequirements);
    return false;
  }

  constexpr char kvkGetBufferMemoryRequirements2[] =
      "vkGetBufferMemoryRequirements2";
  vkGetBufferMemoryRequirements2 =
      reinterpret_cast<PFN_vkGetBufferMemoryRequirements2>(
          vkGetDeviceProcAddr(vk_device, kvkGetBufferMemoryRequirements2));
  if (!vkGetBufferMemoryRequirements2) {
    LogGetProcError(kvkGetBufferMemoryRequirements2);
    return false;
  }

  constexpr char kvkGetDeviceQueue[] = "vkGetDeviceQueue";
  vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(
      vkGetDeviceProcAddr(vk_device, kvkGetDeviceQueue));
  if (!vkGetDeviceQueue) {
    LogGetProcError(kvkGetDeviceQueue);
    return false;
  }

  constexpr char kvkGetDeviceQueue2[] = "vkGetDeviceQueue2";
  vkGetDeviceQueue2 = reinterpret_cast<PFN_vkGetDeviceQueue2>(
      vkGetDeviceProcAddr(vk_device, kvkGetDeviceQueue2));
  if (!vkGetDeviceQueue2) {
    LogGetProcError(kvkGetDeviceQueue2);
    return false;
  }

  constexpr char kvkGetFenceStatus[] = "vkGetFenceStatus";
  vkGetFenceStatus = reinterpret_cast<PFN_vkGetFenceStatus>(
      vkGetDeviceProcAddr(vk_device, kvkGetFenceStatus));
  if (!vkGetFenceStatus) {
    LogGetProcError(kvkGetFenceStatus);
    return false;
  }

  constexpr char kvkGetImageMemoryRequirements[] =
      "vkGetImageMemoryRequirements";
  vkGetImageMemoryRequirements =
      reinterpret_cast<PFN_vkGetImageMemoryRequirements>(
          vkGetDeviceProcAddr(vk_device, kvkGetImageMemoryRequirements));
  if (!vkGetImageMemoryRequirements) {
    LogGetProcError(kvkGetImageMemoryRequirements);
    return false;
  }

  constexpr char kvkGetImageMemoryRequirements2[] =
      "vkGetImageMemoryRequirements2";
  vkGetImageMemoryRequirements2 =
      reinterpret_cast<PFN_vkGetImageMemoryRequirements2>(
          vkGetDeviceProcAddr(vk_device, kvkGetImageMemoryRequirements2));
  if (!vkGetImageMemoryRequirements2) {
    LogGetProcError(kvkGetImageMemoryRequirements2);
    return false;
  }

  constexpr char kvkGetImageSubresourceLayout[] = "vkGetImageSubresourceLayout";
  vkGetImageSubresourceLayout =
      reinterpret_cast<PFN_vkGetImageSubresourceLayout>(
          vkGetDeviceProcAddr(vk_device, kvkGetImageSubresourceLayout));
  if (!vkGetImageSubresourceLayout) {
    LogGetProcError(kvkGetImageSubresourceLayout);
    return false;
  }

  constexpr char kvkMapMemory[] = "vkMapMemory";
  vkMapMemory = reinterpret_cast<PFN_vkMapMemory>(
      vkGetDeviceProcAddr(vk_device, kvkMapMemory));
  if (!vkMapMemory) {
    LogGetProcError(kvkMapMemory);
    return false;
  }

  constexpr char kvkQueueSubmit[] = "vkQueueSubmit";
  vkQueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(
      vkGetDeviceProcAddr(vk_device, kvkQueueSubmit));
  if (!vkQueueSubmit) {
    LogGetProcError(kvkQueueSubmit);
    return false;
  }

  constexpr char kvkQueueWaitIdle[] = "vkQueueWaitIdle";
  vkQueueWaitIdle = reinterpret_cast<PFN_vkQueueWaitIdle>(
      vkGetDeviceProcAddr(vk_device, kvkQueueWaitIdle));
  if (!vkQueueWaitIdle) {
    LogGetProcError(kvkQueueWaitIdle);
    return false;
  }

  constexpr char kvkResetCommandBuffer[] = "vkResetCommandBuffer";
  vkResetCommandBuffer = reinterpret_cast<PFN_vkResetCommandBuffer>(
      vkGetDeviceProcAddr(vk_device, kvkResetCommandBuffer));
  if (!vkResetCommandBuffer) {
    LogGetProcError(kvkResetCommandBuffer);
    return false;
  }

  constexpr char kvkResetFences[] = "vkResetFences";
  vkResetFences = reinterpret_cast<PFN_vkResetFences>(
      vkGetDeviceProcAddr(vk_device, kvkResetFences));
  if (!vkResetFences) {
    LogGetProcError(kvkResetFences);
    return false;
  }

  constexpr char kvkUnmapMemory[] = "vkUnmapMemory";
  vkUnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(
      vkGetDeviceProcAddr(vk_device, kvkUnmapMemory));
  if (!vkUnmapMemory) {
    LogGetProcError(kvkUnmapMemory);
    return false;
  }

  constexpr char kvkUpdateDescriptorSets[] = "vkUpdateDescriptorSets";
  vkUpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(
      vkGetDeviceProcAddr(vk_device, kvkUpdateDescriptorSets));
  if (!vkUpdateDescriptorSets) {
    LogGetProcError(kvkUpdateDescriptorSets);
    return false;
  }

  constexpr char kvkWaitForFences[] = "vkWaitForFences";
  vkWaitForFences = reinterpret_cast<PFN_vkWaitForFences>(
      vkGetDeviceProcAddr(vk_device, kvkWaitForFences));
  if (!vkWaitForFences) {
    LogGetProcError(kvkWaitForFences);
    return false;
  }

#if BUILDFLAG(IS_ANDROID)
  if (gfx::HasExtension(
          enabled_extensions,
          VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME)) {
    constexpr char kvkGetAndroidHardwareBufferPropertiesANDROID[] =
        "vkGetAndroidHardwareBufferPropertiesANDROID";
    vkGetAndroidHardwareBufferPropertiesANDROID =
        reinterpret_cast<PFN_vkGetAndroidHardwareBufferPropertiesANDROID>(
            vkGetDeviceProcAddr(vk_device,
                                kvkGetAndroidHardwareBufferPropertiesANDROID));
    if (!vkGetAndroidHardwareBufferPropertiesANDROID) {
      LogGetProcError(kvkGetAndroidHardwareBufferPropertiesANDROID);
      return false;
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_POSIX)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME)) {
    constexpr char kvkGetSemaphoreFdKHR[] = "vkGetSemaphoreFdKHR";
    vkGetSemaphoreFdKHR = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(
        vkGetDeviceProcAddr(vk_device, kvkGetSemaphoreFdKHR));
    if (!vkGetSemaphoreFdKHR) {
      LogGetProcError(kvkGetSemaphoreFdKHR);
      return false;
    }

    constexpr char kvkImportSemaphoreFdKHR[] = "vkImportSemaphoreFdKHR";
    vkImportSemaphoreFdKHR = reinterpret_cast<PFN_vkImportSemaphoreFdKHR>(
        vkGetDeviceProcAddr(vk_device, kvkImportSemaphoreFdKHR));
    if (!vkImportSemaphoreFdKHR) {
      LogGetProcError(kvkImportSemaphoreFdKHR);
      return false;
    }
  }
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_WIN)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME)) {
    constexpr char kvkGetSemaphoreWin32HandleKHR[] =
        "vkGetSemaphoreWin32HandleKHR";
    vkGetSemaphoreWin32HandleKHR =
        reinterpret_cast<PFN_vkGetSemaphoreWin32HandleKHR>(
            vkGetDeviceProcAddr(vk_device, kvkGetSemaphoreWin32HandleKHR));
    if (!vkGetSemaphoreWin32HandleKHR) {
      LogGetProcError(kvkGetSemaphoreWin32HandleKHR);
      return false;
    }

    constexpr char kvkImportSemaphoreWin32HandleKHR[] =
        "vkImportSemaphoreWin32HandleKHR";
    vkImportSemaphoreWin32HandleKHR =
        reinterpret_cast<PFN_vkImportSemaphoreWin32HandleKHR>(
            vkGetDeviceProcAddr(vk_device, kvkImportSemaphoreWin32HandleKHR));
    if (!vkImportSemaphoreWin32HandleKHR) {
      LogGetProcError(kvkImportSemaphoreWin32HandleKHR);
      return false;
    }
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_POSIX)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME)) {
    constexpr char kvkGetMemoryFdKHR[] = "vkGetMemoryFdKHR";
    vkGetMemoryFdKHR = reinterpret_cast<PFN_vkGetMemoryFdKHR>(
        vkGetDeviceProcAddr(vk_device, kvkGetMemoryFdKHR));
    if (!vkGetMemoryFdKHR) {
      LogGetProcError(kvkGetMemoryFdKHR);
      return false;
    }

    constexpr char kvkGetMemoryFdPropertiesKHR[] = "vkGetMemoryFdPropertiesKHR";
    vkGetMemoryFdPropertiesKHR =
        reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(
            vkGetDeviceProcAddr(vk_device, kvkGetMemoryFdPropertiesKHR));
    if (!vkGetMemoryFdPropertiesKHR) {
      LogGetProcError(kvkGetMemoryFdPropertiesKHR);
      return false;
    }
  }
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_WIN)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME)) {
    constexpr char kvkGetMemoryWin32HandleKHR[] = "vkGetMemoryWin32HandleKHR";
    vkGetMemoryWin32HandleKHR = reinterpret_cast<PFN_vkGetMemoryWin32HandleKHR>(
        vkGetDeviceProcAddr(vk_device, kvkGetMemoryWin32HandleKHR));
    if (!vkGetMemoryWin32HandleKHR) {
      LogGetProcError(kvkGetMemoryWin32HandleKHR);
      return false;
    }

    constexpr char kvkGetMemoryWin32HandlePropertiesKHR[] =
        "vkGetMemoryWin32HandlePropertiesKHR";
    vkGetMemoryWin32HandlePropertiesKHR =
        reinterpret_cast<PFN_vkGetMemoryWin32HandlePropertiesKHR>(
            vkGetDeviceProcAddr(vk_device,
                                kvkGetMemoryWin32HandlePropertiesKHR));
    if (!vkGetMemoryWin32HandlePropertiesKHR) {
      LogGetProcError(kvkGetMemoryWin32HandlePropertiesKHR);
      return false;
    }
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_FUCHSIA)
  if (gfx::HasExtension(enabled_extensions,
                        VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME)) {
    constexpr char kvkImportSemaphoreZirconHandleFUCHSIA[] =
        "vkImportSemaphoreZirconHandleFUCHSIA";
    vkImportSemaphoreZirconHandleFUCHSIA =
        reinterpret_cast<PFN_vkImportSemaphoreZirconHandleFUCHSIA>(
            vkGetDeviceProcAddr(vk_device,
                                kvkImportSemaphoreZirconHandleFUCHSIA));
    if (!vkImportSemaphoreZirconHandleFUCHSIA) {
      LogGetProcError(kvkImportSemaphoreZirconHandleFUCHSIA);
      return false;
    }

    constexpr char kvkGetSemaphoreZirconHandleFUCHSIA[] =
        "vkGetSemaphoreZirconHandleFUCHSIA";
    vkGetSemaphoreZirconHandleFUCHSIA =
        reinterpret_cast<PFN_vkGetSemaphoreZirconHandleFUCHSIA>(
            vkGetDeviceProcAddr(vk_device, kvkGetSemaphoreZirconHandleFUCHSIA));
    if (!vkGetSemaphoreZirconHandleFUCHSIA) {
      LogGetProcError(kvkGetSemaphoreZirconHandleFUCHSIA);
      return false;
    }
  }
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_FUCHSIA)
  if (gfx::HasExtension(enabled_extensions,
                        VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME)) {
    constexpr char kvkGetMemoryZirconHandleFUCHSIA[] =
        "vkGetMemoryZirconHandleFUCHSIA";
    vkGetMemoryZirconHandleFUCHSIA =
        reinterpret_cast<PFN_vkGetMemoryZirconHandleFUCHSIA>(
            vkGetDeviceProcAddr(vk_device, kvkGetMemoryZirconHandleFUCHSIA));
    if (!vkGetMemoryZirconHandleFUCHSIA) {
      LogGetProcError(kvkGetMemoryZirconHandleFUCHSIA);
      return false;
    }
  }
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_FUCHSIA)
  if (gfx::HasExtension(enabled_extensions,
                        VK_FUCHSIA_BUFFER_COLLECTION_EXTENSION_NAME)) {
    constexpr char kvkCreateBufferCollectionFUCHSIA[] =
        "vkCreateBufferCollectionFUCHSIA";
    vkCreateBufferCollectionFUCHSIA =
        reinterpret_cast<PFN_vkCreateBufferCollectionFUCHSIA>(
            vkGetDeviceProcAddr(vk_device, kvkCreateBufferCollectionFUCHSIA));
    if (!vkCreateBufferCollectionFUCHSIA) {
      LogGetProcError(kvkCreateBufferCollectionFUCHSIA);
      return false;
    }

    constexpr char kvkSetBufferCollectionImageConstraintsFUCHSIA[] =
        "vkSetBufferCollectionImageConstraintsFUCHSIA";
    vkSetBufferCollectionImageConstraintsFUCHSIA =
        reinterpret_cast<PFN_vkSetBufferCollectionImageConstraintsFUCHSIA>(
            vkGetDeviceProcAddr(vk_device,
                                kvkSetBufferCollectionImageConstraintsFUCHSIA));
    if (!vkSetBufferCollectionImageConstraintsFUCHSIA) {
      LogGetProcError(kvkSetBufferCollectionImageConstraintsFUCHSIA);
      return false;
    }

    constexpr char kvkGetBufferCollectionPropertiesFUCHSIA[] =
        "vkGetBufferCollectionPropertiesFUCHSIA";
    vkGetBufferCollectionPropertiesFUCHSIA =
        reinterpret_cast<PFN_vkGetBufferCollectionPropertiesFUCHSIA>(
            vkGetDeviceProcAddr(vk_device,
                                kvkGetBufferCollectionPropertiesFUCHSIA));
    if (!vkGetBufferCollectionPropertiesFUCHSIA) {
      LogGetProcError(kvkGetBufferCollectionPropertiesFUCHSIA);
      return false;
    }

    constexpr char kvkDestroyBufferCollectionFUCHSIA[] =
        "vkDestroyBufferCollectionFUCHSIA";
    vkDestroyBufferCollectionFUCHSIA =
        reinterpret_cast<PFN_vkDestroyBufferCollectionFUCHSIA>(
            vkGetDeviceProcAddr(vk_device, kvkDestroyBufferCollectionFUCHSIA));
    if (!vkDestroyBufferCollectionFUCHSIA) {
      LogGetProcError(kvkDestroyBufferCollectionFUCHSIA);
      return false;
    }
  }
#endif  // BUILDFLAG(IS_FUCHSIA)

  if (gfx::HasExtension(enabled_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
    constexpr char kvkAcquireNextImageKHR[] = "vkAcquireNextImageKHR";
    vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(
        vkGetDeviceProcAddr(vk_device, kvkAcquireNextImageKHR));
    if (!vkAcquireNextImageKHR) {
      LogGetProcError(kvkAcquireNextImageKHR);
      return false;
    }

    constexpr char kvkCreateSwapchainKHR[] = "vkCreateSwapchainKHR";
    vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(
        vkGetDeviceProcAddr(vk_device, kvkCreateSwapchainKHR));
    if (!vkCreateSwapchainKHR) {
      LogGetProcError(kvkCreateSwapchainKHR);
      return false;
    }

    constexpr char kvkDestroySwapchainKHR[] = "vkDestroySwapchainKHR";
    vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(
        vkGetDeviceProcAddr(vk_device, kvkDestroySwapchainKHR));
    if (!vkDestroySwapchainKHR) {
      LogGetProcError(kvkDestroySwapchainKHR);
      return false;
    }

    constexpr char kvkGetSwapchainImagesKHR[] = "vkGetSwapchainImagesKHR";
    vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(
        vkGetDeviceProcAddr(vk_device, kvkGetSwapchainImagesKHR));
    if (!vkGetSwapchainImagesKHR) {
      LogGetProcError(kvkGetSwapchainImagesKHR);
      return false;
    }

    constexpr char kvkQueuePresentKHR[] = "vkQueuePresentKHR";
    vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(
        vkGetDeviceProcAddr(vk_device, kvkQueuePresentKHR));
    if (!vkQueuePresentKHR) {
      LogGetProcError(kvkQueuePresentKHR);
      return false;
    }
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (gfx::HasExtension(enabled_extensions,
                        VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME)) {
    constexpr char kvkGetImageDrmFormatModifierPropertiesEXT[] =
        "vkGetImageDrmFormatModifierPropertiesEXT";
    vkGetImageDrmFormatModifierPropertiesEXT =
        reinterpret_cast<PFN_vkGetImageDrmFormatModifierPropertiesEXT>(
            vkGetDeviceProcAddr(vk_device,
                                kvkGetImageDrmFormatModifierPropertiesEXT));
    if (!vkGetImageDrmFormatModifierPropertiesEXT) {
      LogGetProcError(kvkGetImageDrmFormatModifierPropertiesEXT);
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
  vkGetPhysicalDeviceExternalSemaphoreProperties = nullptr;
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
  vkCmdBindDescriptorSets = nullptr;
  vkCmdBindPipeline = nullptr;
  vkCmdBindVertexBuffers = nullptr;
  vkCmdCopyBuffer = nullptr;
  vkCmdCopyBufferToImage = nullptr;
  vkCmdCopyImage = nullptr;
  vkCmdCopyImageToBuffer = nullptr;
  vkCmdDraw = nullptr;
  vkCmdEndRenderPass = nullptr;
  vkCmdExecuteCommands = nullptr;
  vkCmdNextSubpass = nullptr;
  vkCmdPipelineBarrier = nullptr;
  vkCmdPushConstants = nullptr;
  vkCmdSetScissor = nullptr;
  vkCmdSetViewport = nullptr;
  vkCreateBuffer = nullptr;
  vkCreateCommandPool = nullptr;
  vkCreateDescriptorPool = nullptr;
  vkCreateDescriptorSetLayout = nullptr;
  vkCreateFence = nullptr;
  vkCreateFramebuffer = nullptr;
  vkCreateGraphicsPipelines = nullptr;
  vkCreateImage = nullptr;
  vkCreateImageView = nullptr;
  vkCreatePipelineLayout = nullptr;
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
  vkDestroyPipeline = nullptr;
  vkDestroyPipelineLayout = nullptr;
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
