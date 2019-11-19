// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// gpu/vulkan/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include "gpu/vulkan/vulkan_function_pointers.h"

#include "base/no_destructor.h"

namespace gpu {

VulkanFunctionPointers* GetVulkanFunctionPointers() {
  static base::NoDestructor<VulkanFunctionPointers> vulkan_function_pointers;
  return vulkan_function_pointers.get();
}

VulkanFunctionPointers::VulkanFunctionPointers() = default;
VulkanFunctionPointers::~VulkanFunctionPointers() = default;

bool VulkanFunctionPointers::BindUnassociatedFunctionPointers() {
  // vkGetInstanceProcAddr must be handled specially since it gets its function
  // pointer through base::GetFunctionPOinterFromNativeLibrary(). Other Vulkan
  // functions don't do this.
  vkGetInstanceProcAddrFn = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      base::GetFunctionPointerFromNativeLibrary(vulkan_loader_library_,
                                                "vkGetInstanceProcAddr"));
  if (!vkGetInstanceProcAddrFn)
    return false;

  vkEnumerateInstanceVersionFn =
      reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
          vkGetInstanceProcAddrFn(nullptr, "vkEnumerateInstanceVersion"));
  // vkEnumerateInstanceVersion didn't exist in Vulkan 1.0, so we should
  // proceed even if we fail to get vkEnumerateInstanceVersion pointer.
  vkCreateInstanceFn = reinterpret_cast<PFN_vkCreateInstance>(
      vkGetInstanceProcAddrFn(nullptr, "vkCreateInstance"));
  if (!vkCreateInstanceFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateInstance";
    return false;
  }

  vkEnumerateInstanceExtensionPropertiesFn =
      reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
          vkGetInstanceProcAddrFn(nullptr,
                                  "vkEnumerateInstanceExtensionProperties"));
  if (!vkEnumerateInstanceExtensionPropertiesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEnumerateInstanceExtensionProperties";
    return false;
  }

  vkEnumerateInstanceLayerPropertiesFn =
      reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(
          vkGetInstanceProcAddrFn(nullptr,
                                  "vkEnumerateInstanceLayerProperties"));
  if (!vkEnumerateInstanceLayerPropertiesFn) {
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
  vkCreateDeviceFn = reinterpret_cast<PFN_vkCreateDevice>(
      vkGetInstanceProcAddrFn(vk_instance, "vkCreateDevice"));
  if (!vkCreateDeviceFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateDevice";
    return false;
  }

  vkDestroyInstanceFn = reinterpret_cast<PFN_vkDestroyInstance>(
      vkGetInstanceProcAddrFn(vk_instance, "vkDestroyInstance"));
  if (!vkDestroyInstanceFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyInstance";
    return false;
  }

  vkEnumerateDeviceLayerPropertiesFn =
      reinterpret_cast<PFN_vkEnumerateDeviceLayerProperties>(
          vkGetInstanceProcAddrFn(vk_instance,
                                  "vkEnumerateDeviceLayerProperties"));
  if (!vkEnumerateDeviceLayerPropertiesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEnumerateDeviceLayerProperties";
    return false;
  }

  vkEnumeratePhysicalDevicesFn =
      reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
          vkGetInstanceProcAddrFn(vk_instance, "vkEnumeratePhysicalDevices"));
  if (!vkEnumeratePhysicalDevicesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEnumeratePhysicalDevices";
    return false;
  }

  vkGetDeviceProcAddrFn = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
      vkGetInstanceProcAddrFn(vk_instance, "vkGetDeviceProcAddr"));
  if (!vkGetDeviceProcAddrFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetDeviceProcAddr";
    return false;
  }

  vkGetPhysicalDeviceFeaturesFn =
      reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures>(
          vkGetInstanceProcAddrFn(vk_instance, "vkGetPhysicalDeviceFeatures"));
  if (!vkGetPhysicalDeviceFeaturesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceFeatures";
    return false;
  }

  vkGetPhysicalDeviceFormatPropertiesFn =
      reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(
          vkGetInstanceProcAddrFn(vk_instance,
                                  "vkGetPhysicalDeviceFormatProperties"));
  if (!vkGetPhysicalDeviceFormatPropertiesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceFormatProperties";
    return false;
  }

  vkGetPhysicalDeviceMemoryPropertiesFn =
      reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
          vkGetInstanceProcAddrFn(vk_instance,
                                  "vkGetPhysicalDeviceMemoryProperties"));
  if (!vkGetPhysicalDeviceMemoryPropertiesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceMemoryProperties";
    return false;
  }

  vkGetPhysicalDevicePropertiesFn =
      reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
          vkGetInstanceProcAddrFn(vk_instance,
                                  "vkGetPhysicalDeviceProperties"));
  if (!vkGetPhysicalDevicePropertiesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceProperties";
    return false;
  }

  vkGetPhysicalDeviceQueueFamilyPropertiesFn =
      reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
          vkGetInstanceProcAddrFn(vk_instance,
                                  "vkGetPhysicalDeviceQueueFamilyProperties"));
  if (!vkGetPhysicalDeviceQueueFamilyPropertiesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceQueueFamilyProperties";
    return false;
  }

  if (gfx::HasExtension(enabled_extensions, VK_KHR_SURFACE_EXTENSION_NAME)) {
    vkDestroySurfaceKHRFn = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
        vkGetInstanceProcAddrFn(vk_instance, "vkDestroySurfaceKHR"));
    if (!vkDestroySurfaceKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkDestroySurfaceKHR";
      return false;
    }

    vkGetPhysicalDeviceSurfaceCapabilitiesKHRFn =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
            vkGetInstanceProcAddrFn(
                vk_instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    if (!vkGetPhysicalDeviceSurfaceCapabilitiesKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceSurfaceCapabilitiesKHR";
      return false;
    }

    vkGetPhysicalDeviceSurfaceFormatsKHRFn =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
            vkGetInstanceProcAddrFn(vk_instance,
                                    "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    if (!vkGetPhysicalDeviceSurfaceFormatsKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceSurfaceFormatsKHR";
      return false;
    }

    vkGetPhysicalDeviceSurfaceSupportKHRFn =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
            vkGetInstanceProcAddrFn(vk_instance,
                                    "vkGetPhysicalDeviceSurfaceSupportKHR"));
    if (!vkGetPhysicalDeviceSurfaceSupportKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceSurfaceSupportKHR";
      return false;
    }
  }

#if defined(USE_VULKAN_XLIB)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_XLIB_SURFACE_EXTENSION_NAME)) {
    vkCreateXlibSurfaceKHRFn = reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(
        vkGetInstanceProcAddrFn(vk_instance, "vkCreateXlibSurfaceKHR"));
    if (!vkCreateXlibSurfaceKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateXlibSurfaceKHR";
      return false;
    }

    vkGetPhysicalDeviceXlibPresentationSupportKHRFn =
        reinterpret_cast<PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR>(
            vkGetInstanceProcAddrFn(
                vk_instance, "vkGetPhysicalDeviceXlibPresentationSupportKHR"));
    if (!vkGetPhysicalDeviceXlibPresentationSupportKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceXlibPresentationSupportKHR";
      return false;
    }
  }
#endif  // defined(USE_VULKAN_XLIB)

#if defined(OS_ANDROID)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME)) {
    vkCreateAndroidSurfaceKHRFn =
        reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
            vkGetInstanceProcAddrFn(vk_instance, "vkCreateAndroidSurfaceKHR"));
    if (!vkCreateAndroidSurfaceKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateAndroidSurfaceKHR";
      return false;
    }
  }
#endif  // defined(OS_ANDROID)

#if defined(OS_FUCHSIA)
  if (gfx::HasExtension(enabled_extensions,
                        VK_FUCHSIA_IMAGEPIPE_SURFACE_EXTENSION_NAME)) {
    vkCreateImagePipeSurfaceFUCHSIAFn =
        reinterpret_cast<PFN_vkCreateImagePipeSurfaceFUCHSIA>(
            vkGetInstanceProcAddrFn(vk_instance,
                                    "vkCreateImagePipeSurfaceFUCHSIA"));
    if (!vkCreateImagePipeSurfaceFUCHSIAFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateImagePipeSurfaceFUCHSIA";
      return false;
    }
  }
#endif  // defined(OS_FUCHSIA)

  if (api_version >= VK_API_VERSION_1_1) {
    vkGetPhysicalDeviceImageFormatProperties2Fn =
        reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(
            vkGetInstanceProcAddrFn(
                vk_instance, "vkGetPhysicalDeviceImageFormatProperties2"));
    if (!vkGetPhysicalDeviceImageFormatProperties2Fn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceImageFormatProperties2";
      return false;
    }
  }

  if (api_version >= VK_API_VERSION_1_1) {
    vkGetPhysicalDeviceFeatures2Fn =
        reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
            vkGetInstanceProcAddrFn(vk_instance,
                                    "vkGetPhysicalDeviceFeatures2"));
    if (!vkGetPhysicalDeviceFeatures2Fn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceFeatures2";
      return false;
    }

  } else if (gfx::HasExtension(
                 enabled_extensions,
                 VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
    vkGetPhysicalDeviceFeatures2Fn =
        reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
            vkGetInstanceProcAddrFn(vk_instance,
                                    "vkGetPhysicalDeviceFeatures2KHR"));
    if (!vkGetPhysicalDeviceFeatures2Fn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceFeatures2KHR";
      return false;
    }
  }

  return true;
}

bool VulkanFunctionPointers::BindDeviceFunctionPointers(
    VkDevice vk_device,
    uint32_t api_version,
    const gfx::ExtensionSet& enabled_extensions) {
  // Device functions
  vkAllocateCommandBuffersFn = reinterpret_cast<PFN_vkAllocateCommandBuffers>(
      vkGetDeviceProcAddrFn(vk_device, "vkAllocateCommandBuffers"));
  if (!vkAllocateCommandBuffersFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkAllocateCommandBuffers";
    return false;
  }

  vkAllocateDescriptorSetsFn = reinterpret_cast<PFN_vkAllocateDescriptorSets>(
      vkGetDeviceProcAddrFn(vk_device, "vkAllocateDescriptorSets"));
  if (!vkAllocateDescriptorSetsFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkAllocateDescriptorSets";
    return false;
  }

  vkAllocateMemoryFn = reinterpret_cast<PFN_vkAllocateMemory>(
      vkGetDeviceProcAddrFn(vk_device, "vkAllocateMemory"));
  if (!vkAllocateMemoryFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkAllocateMemory";
    return false;
  }

  vkBeginCommandBufferFn = reinterpret_cast<PFN_vkBeginCommandBuffer>(
      vkGetDeviceProcAddrFn(vk_device, "vkBeginCommandBuffer"));
  if (!vkBeginCommandBufferFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkBeginCommandBuffer";
    return false;
  }

  vkBindBufferMemoryFn = reinterpret_cast<PFN_vkBindBufferMemory>(
      vkGetDeviceProcAddrFn(vk_device, "vkBindBufferMemory"));
  if (!vkBindBufferMemoryFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkBindBufferMemory";
    return false;
  }

  vkBindImageMemoryFn = reinterpret_cast<PFN_vkBindImageMemory>(
      vkGetDeviceProcAddrFn(vk_device, "vkBindImageMemory"));
  if (!vkBindImageMemoryFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkBindImageMemory";
    return false;
  }

  vkCmdBeginRenderPassFn = reinterpret_cast<PFN_vkCmdBeginRenderPass>(
      vkGetDeviceProcAddrFn(vk_device, "vkCmdBeginRenderPass"));
  if (!vkCmdBeginRenderPassFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdBeginRenderPass";
    return false;
  }

  vkCmdCopyBufferToImageFn = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(
      vkGetDeviceProcAddrFn(vk_device, "vkCmdCopyBufferToImage"));
  if (!vkCmdCopyBufferToImageFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdCopyBufferToImage";
    return false;
  }

  vkCmdEndRenderPassFn = reinterpret_cast<PFN_vkCmdEndRenderPass>(
      vkGetDeviceProcAddrFn(vk_device, "vkCmdEndRenderPass"));
  if (!vkCmdEndRenderPassFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdEndRenderPass";
    return false;
  }

  vkCmdExecuteCommandsFn = reinterpret_cast<PFN_vkCmdExecuteCommands>(
      vkGetDeviceProcAddrFn(vk_device, "vkCmdExecuteCommands"));
  if (!vkCmdExecuteCommandsFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdExecuteCommands";
    return false;
  }

  vkCmdNextSubpassFn = reinterpret_cast<PFN_vkCmdNextSubpass>(
      vkGetDeviceProcAddrFn(vk_device, "vkCmdNextSubpass"));
  if (!vkCmdNextSubpassFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdNextSubpass";
    return false;
  }

  vkCmdPipelineBarrierFn = reinterpret_cast<PFN_vkCmdPipelineBarrier>(
      vkGetDeviceProcAddrFn(vk_device, "vkCmdPipelineBarrier"));
  if (!vkCmdPipelineBarrierFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdPipelineBarrier";
    return false;
  }

  vkCreateBufferFn = reinterpret_cast<PFN_vkCreateBuffer>(
      vkGetDeviceProcAddrFn(vk_device, "vkCreateBuffer"));
  if (!vkCreateBufferFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateBuffer";
    return false;
  }

  vkCreateCommandPoolFn = reinterpret_cast<PFN_vkCreateCommandPool>(
      vkGetDeviceProcAddrFn(vk_device, "vkCreateCommandPool"));
  if (!vkCreateCommandPoolFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateCommandPool";
    return false;
  }

  vkCreateDescriptorPoolFn = reinterpret_cast<PFN_vkCreateDescriptorPool>(
      vkGetDeviceProcAddrFn(vk_device, "vkCreateDescriptorPool"));
  if (!vkCreateDescriptorPoolFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateDescriptorPool";
    return false;
  }

  vkCreateDescriptorSetLayoutFn =
      reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(
          vkGetDeviceProcAddrFn(vk_device, "vkCreateDescriptorSetLayout"));
  if (!vkCreateDescriptorSetLayoutFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateDescriptorSetLayout";
    return false;
  }

  vkCreateFenceFn = reinterpret_cast<PFN_vkCreateFence>(
      vkGetDeviceProcAddrFn(vk_device, "vkCreateFence"));
  if (!vkCreateFenceFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateFence";
    return false;
  }

  vkCreateFramebufferFn = reinterpret_cast<PFN_vkCreateFramebuffer>(
      vkGetDeviceProcAddrFn(vk_device, "vkCreateFramebuffer"));
  if (!vkCreateFramebufferFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateFramebuffer";
    return false;
  }

  vkCreateImageFn = reinterpret_cast<PFN_vkCreateImage>(
      vkGetDeviceProcAddrFn(vk_device, "vkCreateImage"));
  if (!vkCreateImageFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateImage";
    return false;
  }

  vkCreateImageViewFn = reinterpret_cast<PFN_vkCreateImageView>(
      vkGetDeviceProcAddrFn(vk_device, "vkCreateImageView"));
  if (!vkCreateImageViewFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateImageView";
    return false;
  }

  vkCreateRenderPassFn = reinterpret_cast<PFN_vkCreateRenderPass>(
      vkGetDeviceProcAddrFn(vk_device, "vkCreateRenderPass"));
  if (!vkCreateRenderPassFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateRenderPass";
    return false;
  }

  vkCreateSamplerFn = reinterpret_cast<PFN_vkCreateSampler>(
      vkGetDeviceProcAddrFn(vk_device, "vkCreateSampler"));
  if (!vkCreateSamplerFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateSampler";
    return false;
  }

  vkCreateSemaphoreFn = reinterpret_cast<PFN_vkCreateSemaphore>(
      vkGetDeviceProcAddrFn(vk_device, "vkCreateSemaphore"));
  if (!vkCreateSemaphoreFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateSemaphore";
    return false;
  }

  vkCreateShaderModuleFn = reinterpret_cast<PFN_vkCreateShaderModule>(
      vkGetDeviceProcAddrFn(vk_device, "vkCreateShaderModule"));
  if (!vkCreateShaderModuleFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateShaderModule";
    return false;
  }

  vkDestroyBufferFn = reinterpret_cast<PFN_vkDestroyBuffer>(
      vkGetDeviceProcAddrFn(vk_device, "vkDestroyBuffer"));
  if (!vkDestroyBufferFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyBuffer";
    return false;
  }

  vkDestroyCommandPoolFn = reinterpret_cast<PFN_vkDestroyCommandPool>(
      vkGetDeviceProcAddrFn(vk_device, "vkDestroyCommandPool"));
  if (!vkDestroyCommandPoolFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyCommandPool";
    return false;
  }

  vkDestroyDescriptorPoolFn = reinterpret_cast<PFN_vkDestroyDescriptorPool>(
      vkGetDeviceProcAddrFn(vk_device, "vkDestroyDescriptorPool"));
  if (!vkDestroyDescriptorPoolFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyDescriptorPool";
    return false;
  }

  vkDestroyDescriptorSetLayoutFn =
      reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(
          vkGetDeviceProcAddrFn(vk_device, "vkDestroyDescriptorSetLayout"));
  if (!vkDestroyDescriptorSetLayoutFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyDescriptorSetLayout";
    return false;
  }

  vkDestroyDeviceFn = reinterpret_cast<PFN_vkDestroyDevice>(
      vkGetDeviceProcAddrFn(vk_device, "vkDestroyDevice"));
  if (!vkDestroyDeviceFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyDevice";
    return false;
  }

  vkDestroyFenceFn = reinterpret_cast<PFN_vkDestroyFence>(
      vkGetDeviceProcAddrFn(vk_device, "vkDestroyFence"));
  if (!vkDestroyFenceFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyFence";
    return false;
  }

  vkDestroyFramebufferFn = reinterpret_cast<PFN_vkDestroyFramebuffer>(
      vkGetDeviceProcAddrFn(vk_device, "vkDestroyFramebuffer"));
  if (!vkDestroyFramebufferFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyFramebuffer";
    return false;
  }

  vkDestroyImageFn = reinterpret_cast<PFN_vkDestroyImage>(
      vkGetDeviceProcAddrFn(vk_device, "vkDestroyImage"));
  if (!vkDestroyImageFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyImage";
    return false;
  }

  vkDestroyImageViewFn = reinterpret_cast<PFN_vkDestroyImageView>(
      vkGetDeviceProcAddrFn(vk_device, "vkDestroyImageView"));
  if (!vkDestroyImageViewFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyImageView";
    return false;
  }

  vkDestroyRenderPassFn = reinterpret_cast<PFN_vkDestroyRenderPass>(
      vkGetDeviceProcAddrFn(vk_device, "vkDestroyRenderPass"));
  if (!vkDestroyRenderPassFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyRenderPass";
    return false;
  }

  vkDestroySamplerFn = reinterpret_cast<PFN_vkDestroySampler>(
      vkGetDeviceProcAddrFn(vk_device, "vkDestroySampler"));
  if (!vkDestroySamplerFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroySampler";
    return false;
  }

  vkDestroySemaphoreFn = reinterpret_cast<PFN_vkDestroySemaphore>(
      vkGetDeviceProcAddrFn(vk_device, "vkDestroySemaphore"));
  if (!vkDestroySemaphoreFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroySemaphore";
    return false;
  }

  vkDestroyShaderModuleFn = reinterpret_cast<PFN_vkDestroyShaderModule>(
      vkGetDeviceProcAddrFn(vk_device, "vkDestroyShaderModule"));
  if (!vkDestroyShaderModuleFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyShaderModule";
    return false;
  }

  vkDeviceWaitIdleFn = reinterpret_cast<PFN_vkDeviceWaitIdle>(
      vkGetDeviceProcAddrFn(vk_device, "vkDeviceWaitIdle"));
  if (!vkDeviceWaitIdleFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDeviceWaitIdle";
    return false;
  }

  vkEndCommandBufferFn = reinterpret_cast<PFN_vkEndCommandBuffer>(
      vkGetDeviceProcAddrFn(vk_device, "vkEndCommandBuffer"));
  if (!vkEndCommandBufferFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEndCommandBuffer";
    return false;
  }

  vkFreeCommandBuffersFn = reinterpret_cast<PFN_vkFreeCommandBuffers>(
      vkGetDeviceProcAddrFn(vk_device, "vkFreeCommandBuffers"));
  if (!vkFreeCommandBuffersFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkFreeCommandBuffers";
    return false;
  }

  vkFreeDescriptorSetsFn = reinterpret_cast<PFN_vkFreeDescriptorSets>(
      vkGetDeviceProcAddrFn(vk_device, "vkFreeDescriptorSets"));
  if (!vkFreeDescriptorSetsFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkFreeDescriptorSets";
    return false;
  }

  vkFreeMemoryFn = reinterpret_cast<PFN_vkFreeMemory>(
      vkGetDeviceProcAddrFn(vk_device, "vkFreeMemory"));
  if (!vkFreeMemoryFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkFreeMemory";
    return false;
  }

  vkGetBufferMemoryRequirementsFn =
      reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(
          vkGetDeviceProcAddrFn(vk_device, "vkGetBufferMemoryRequirements"));
  if (!vkGetBufferMemoryRequirementsFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetBufferMemoryRequirements";
    return false;
  }

  vkGetDeviceQueueFn = reinterpret_cast<PFN_vkGetDeviceQueue>(
      vkGetDeviceProcAddrFn(vk_device, "vkGetDeviceQueue"));
  if (!vkGetDeviceQueueFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetDeviceQueue";
    return false;
  }

  vkGetFenceStatusFn = reinterpret_cast<PFN_vkGetFenceStatus>(
      vkGetDeviceProcAddrFn(vk_device, "vkGetFenceStatus"));
  if (!vkGetFenceStatusFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetFenceStatus";
    return false;
  }

  vkGetImageMemoryRequirementsFn =
      reinterpret_cast<PFN_vkGetImageMemoryRequirements>(
          vkGetDeviceProcAddrFn(vk_device, "vkGetImageMemoryRequirements"));
  if (!vkGetImageMemoryRequirementsFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetImageMemoryRequirements";
    return false;
  }

  vkMapMemoryFn = reinterpret_cast<PFN_vkMapMemory>(
      vkGetDeviceProcAddrFn(vk_device, "vkMapMemory"));
  if (!vkMapMemoryFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkMapMemory";
    return false;
  }

  vkQueueSubmitFn = reinterpret_cast<PFN_vkQueueSubmit>(
      vkGetDeviceProcAddrFn(vk_device, "vkQueueSubmit"));
  if (!vkQueueSubmitFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkQueueSubmit";
    return false;
  }

  vkQueueWaitIdleFn = reinterpret_cast<PFN_vkQueueWaitIdle>(
      vkGetDeviceProcAddrFn(vk_device, "vkQueueWaitIdle"));
  if (!vkQueueWaitIdleFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkQueueWaitIdle";
    return false;
  }

  vkResetCommandBufferFn = reinterpret_cast<PFN_vkResetCommandBuffer>(
      vkGetDeviceProcAddrFn(vk_device, "vkResetCommandBuffer"));
  if (!vkResetCommandBufferFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkResetCommandBuffer";
    return false;
  }

  vkResetFencesFn = reinterpret_cast<PFN_vkResetFences>(
      vkGetDeviceProcAddrFn(vk_device, "vkResetFences"));
  if (!vkResetFencesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkResetFences";
    return false;
  }

  vkUnmapMemoryFn = reinterpret_cast<PFN_vkUnmapMemory>(
      vkGetDeviceProcAddrFn(vk_device, "vkUnmapMemory"));
  if (!vkUnmapMemoryFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkUnmapMemory";
    return false;
  }

  vkUpdateDescriptorSetsFn = reinterpret_cast<PFN_vkUpdateDescriptorSets>(
      vkGetDeviceProcAddrFn(vk_device, "vkUpdateDescriptorSets"));
  if (!vkUpdateDescriptorSetsFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkUpdateDescriptorSets";
    return false;
  }

  vkWaitForFencesFn = reinterpret_cast<PFN_vkWaitForFences>(
      vkGetDeviceProcAddrFn(vk_device, "vkWaitForFences"));
  if (!vkWaitForFencesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkWaitForFences";
    return false;
  }

  if (api_version >= VK_API_VERSION_1_1) {
    vkGetDeviceQueue2Fn = reinterpret_cast<PFN_vkGetDeviceQueue2>(
        vkGetDeviceProcAddrFn(vk_device, "vkGetDeviceQueue2"));
    if (!vkGetDeviceQueue2Fn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetDeviceQueue2";
      return false;
    }

    vkGetImageMemoryRequirements2Fn =
        reinterpret_cast<PFN_vkGetImageMemoryRequirements2>(
            vkGetDeviceProcAddrFn(vk_device, "vkGetImageMemoryRequirements2"));
    if (!vkGetImageMemoryRequirements2Fn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetImageMemoryRequirements2";
      return false;
    }
  }

#if defined(OS_ANDROID)
  if (gfx::HasExtension(
          enabled_extensions,
          VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME)) {
    vkGetAndroidHardwareBufferPropertiesANDROIDFn =
        reinterpret_cast<PFN_vkGetAndroidHardwareBufferPropertiesANDROID>(
            vkGetDeviceProcAddrFn(
                vk_device, "vkGetAndroidHardwareBufferPropertiesANDROID"));
    if (!vkGetAndroidHardwareBufferPropertiesANDROIDFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetAndroidHardwareBufferPropertiesANDROID";
      return false;
    }
  }
#endif  // defined(OS_ANDROID)

#if defined(OS_LINUX) || defined(OS_ANDROID)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME)) {
    vkGetSemaphoreFdKHRFn = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(
        vkGetDeviceProcAddrFn(vk_device, "vkGetSemaphoreFdKHR"));
    if (!vkGetSemaphoreFdKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetSemaphoreFdKHR";
      return false;
    }

    vkImportSemaphoreFdKHRFn = reinterpret_cast<PFN_vkImportSemaphoreFdKHR>(
        vkGetDeviceProcAddrFn(vk_device, "vkImportSemaphoreFdKHR"));
    if (!vkImportSemaphoreFdKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkImportSemaphoreFdKHR";
      return false;
    }
  }
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_LINUX)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME)) {
    vkGetMemoryFdKHRFn = reinterpret_cast<PFN_vkGetMemoryFdKHR>(
        vkGetDeviceProcAddrFn(vk_device, "vkGetMemoryFdKHR"));
    if (!vkGetMemoryFdKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetMemoryFdKHR";
      return false;
    }

    vkGetMemoryFdPropertiesKHRFn =
        reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(
            vkGetDeviceProcAddrFn(vk_device, "vkGetMemoryFdPropertiesKHR"));
    if (!vkGetMemoryFdPropertiesKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetMemoryFdPropertiesKHR";
      return false;
    }
  }
#endif  // defined(OS_LINUX)

#if defined(OS_FUCHSIA)
  if (gfx::HasExtension(enabled_extensions,
                        VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME)) {
    vkImportSemaphoreZirconHandleFUCHSIAFn =
        reinterpret_cast<PFN_vkImportSemaphoreZirconHandleFUCHSIA>(
            vkGetDeviceProcAddrFn(vk_device,
                                  "vkImportSemaphoreZirconHandleFUCHSIA"));
    if (!vkImportSemaphoreZirconHandleFUCHSIAFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkImportSemaphoreZirconHandleFUCHSIA";
      return false;
    }

    vkGetSemaphoreZirconHandleFUCHSIAFn =
        reinterpret_cast<PFN_vkGetSemaphoreZirconHandleFUCHSIA>(
            vkGetDeviceProcAddrFn(vk_device,
                                  "vkGetSemaphoreZirconHandleFUCHSIA"));
    if (!vkGetSemaphoreZirconHandleFUCHSIAFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetSemaphoreZirconHandleFUCHSIA";
      return false;
    }
  }
#endif  // defined(OS_FUCHSIA)

#if defined(OS_FUCHSIA)
  if (gfx::HasExtension(enabled_extensions,
                        VK_FUCHSIA_BUFFER_COLLECTION_EXTENSION_NAME)) {
    vkCreateBufferCollectionFUCHSIAFn =
        reinterpret_cast<PFN_vkCreateBufferCollectionFUCHSIA>(
            vkGetDeviceProcAddrFn(vk_device,
                                  "vkCreateBufferCollectionFUCHSIA"));
    if (!vkCreateBufferCollectionFUCHSIAFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateBufferCollectionFUCHSIA";
      return false;
    }

    vkSetBufferCollectionConstraintsFUCHSIAFn =
        reinterpret_cast<PFN_vkSetBufferCollectionConstraintsFUCHSIA>(
            vkGetDeviceProcAddrFn(vk_device,
                                  "vkSetBufferCollectionConstraintsFUCHSIA"));
    if (!vkSetBufferCollectionConstraintsFUCHSIAFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkSetBufferCollectionConstraintsFUCHSIA";
      return false;
    }

    vkGetBufferCollectionPropertiesFUCHSIAFn =
        reinterpret_cast<PFN_vkGetBufferCollectionPropertiesFUCHSIA>(
            vkGetDeviceProcAddrFn(vk_device,
                                  "vkGetBufferCollectionPropertiesFUCHSIA"));
    if (!vkGetBufferCollectionPropertiesFUCHSIAFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetBufferCollectionPropertiesFUCHSIA";
      return false;
    }

    vkDestroyBufferCollectionFUCHSIAFn =
        reinterpret_cast<PFN_vkDestroyBufferCollectionFUCHSIA>(
            vkGetDeviceProcAddrFn(vk_device,
                                  "vkDestroyBufferCollectionFUCHSIA"));
    if (!vkDestroyBufferCollectionFUCHSIAFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkDestroyBufferCollectionFUCHSIA";
      return false;
    }
  }
#endif  // defined(OS_FUCHSIA)

  if (gfx::HasExtension(enabled_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
    vkAcquireNextImageKHRFn = reinterpret_cast<PFN_vkAcquireNextImageKHR>(
        vkGetDeviceProcAddrFn(vk_device, "vkAcquireNextImageKHR"));
    if (!vkAcquireNextImageKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkAcquireNextImageKHR";
      return false;
    }

    vkCreateSwapchainKHRFn = reinterpret_cast<PFN_vkCreateSwapchainKHR>(
        vkGetDeviceProcAddrFn(vk_device, "vkCreateSwapchainKHR"));
    if (!vkCreateSwapchainKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateSwapchainKHR";
      return false;
    }

    vkDestroySwapchainKHRFn = reinterpret_cast<PFN_vkDestroySwapchainKHR>(
        vkGetDeviceProcAddrFn(vk_device, "vkDestroySwapchainKHR"));
    if (!vkDestroySwapchainKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkDestroySwapchainKHR";
      return false;
    }

    vkGetSwapchainImagesKHRFn = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(
        vkGetDeviceProcAddrFn(vk_device, "vkGetSwapchainImagesKHR"));
    if (!vkGetSwapchainImagesKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetSwapchainImagesKHR";
      return false;
    }

    vkQueuePresentKHRFn = reinterpret_cast<PFN_vkQueuePresentKHR>(
        vkGetDeviceProcAddrFn(vk_device, "vkQueuePresentKHR"));
    if (!vkQueuePresentKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkQueuePresentKHR";
      return false;
    }
  }

  return true;
}

}  // namespace gpu
