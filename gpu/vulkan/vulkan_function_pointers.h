// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// gpu/vulkan/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_
#define GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_

#include <vulkan/vulkan.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/native_library.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "ui/gfx/extension_set.h"

#if BUILDFLAG(IS_ANDROID)
#include <vulkan/vulkan_android.h>
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include <zircon/types.h>
// <vulkan/vulkan_fuchsia.h> must be included after <zircon/types.h>
#include <vulkan/vulkan_fuchsia.h>

#include "gpu/vulkan/fuchsia/vulkan_fuchsia_ext.h"
#endif

#if defined(USE_VULKAN_XCB)
#include <xcb/xcb.h>
// <vulkan/vulkan_xcb.h> must be included after <xcb/xcb.h>
#include <vulkan/vulkan_xcb.h>
#endif

#if BUILDFLAG(IS_WIN)
#include <vulkan/vulkan_win32.h>
#endif

namespace gpu {

struct VulkanFunctionPointers;

constexpr uint32_t kVulkanRequiredApiVersion = VK_API_VERSION_1_1;

COMPONENT_EXPORT(VULKAN) VulkanFunctionPointers* GetVulkanFunctionPointers();

struct COMPONENT_EXPORT(VULKAN) VulkanFunctionPointers {
  VulkanFunctionPointers();
  ~VulkanFunctionPointers();

  bool BindUnassociatedFunctionPointersFromLoaderLib(base::NativeLibrary lib);
  bool BindUnassociatedFunctionPointersFromGetProcAddr(
      PFN_vkGetInstanceProcAddr proc);

  // These functions assume that vkGetInstanceProcAddr has been populated.
  bool BindInstanceFunctionPointers(
      VkInstance vk_instance,
      uint32_t api_version,
      const gfx::ExtensionSet& enabled_extensions);

  // These functions assume that vkGetDeviceProcAddr has been populated.
  bool BindDeviceFunctionPointers(VkDevice vk_device,
                                  uint32_t api_version,
                                  const gfx::ExtensionSet& enabled_extensions);

  void ResetForTesting();

  // This is used to allow thread safe access to a given vulkan queue when
  // multiple gpu threads are accessing it. Note that this map will be only
  // accessed by multiple gpu threads concurrently to read the data, so it
  // should be thread safe to use this map by multiple threads.
  base::flat_map<VkQueue, std::unique_ptr<base::Lock>> per_queue_lock_map;

  template <typename T>
  class VulkanFunction;
  template <typename R, typename... Args>
  class VulkanFunction<R(VKAPI_PTR*)(Args...)> {
   public:
    using Fn = R(VKAPI_PTR*)(Args...);

    explicit operator bool() const { return !!fn_; }

    NO_SANITIZE("cfi-icall")
    R operator()(Args... args) const { return fn_(args...); }

    Fn get() const { return fn_; }

    void OverrideForTesting(Fn fn) { fn_ = fn; }

   private:
    friend VulkanFunctionPointers;

    Fn operator=(Fn fn) {
      fn_ = fn;
      return fn_;
    }

    Fn fn_ = nullptr;
  };

  // Unassociated functions
  VulkanFunction<PFN_vkGetInstanceProcAddr> vkGetInstanceProcAddr;

  VulkanFunction<PFN_vkEnumerateInstanceVersion> vkEnumerateInstanceVersion;
  VulkanFunction<PFN_vkCreateInstance> vkCreateInstance;
  VulkanFunction<PFN_vkEnumerateInstanceExtensionProperties>
      vkEnumerateInstanceExtensionProperties;
  VulkanFunction<PFN_vkEnumerateInstanceLayerProperties>
      vkEnumerateInstanceLayerProperties;

  // Instance functions
  VulkanFunction<PFN_vkCreateDevice> vkCreateDevice;
  VulkanFunction<PFN_vkDestroyInstance> vkDestroyInstance;
  VulkanFunction<PFN_vkEnumerateDeviceExtensionProperties>
      vkEnumerateDeviceExtensionProperties;
  VulkanFunction<PFN_vkEnumerateDeviceLayerProperties>
      vkEnumerateDeviceLayerProperties;
  VulkanFunction<PFN_vkEnumeratePhysicalDevices> vkEnumeratePhysicalDevices;
  VulkanFunction<PFN_vkGetDeviceProcAddr> vkGetDeviceProcAddr;
  VulkanFunction<PFN_vkGetPhysicalDeviceExternalSemaphoreProperties>
      vkGetPhysicalDeviceExternalSemaphoreProperties;
  VulkanFunction<PFN_vkGetPhysicalDeviceFeatures2> vkGetPhysicalDeviceFeatures2;
  VulkanFunction<PFN_vkGetPhysicalDeviceFormatProperties>
      vkGetPhysicalDeviceFormatProperties;
  VulkanFunction<PFN_vkGetPhysicalDeviceFormatProperties2>
      vkGetPhysicalDeviceFormatProperties2;
  VulkanFunction<PFN_vkGetPhysicalDeviceImageFormatProperties2>
      vkGetPhysicalDeviceImageFormatProperties2;
  VulkanFunction<PFN_vkGetPhysicalDeviceMemoryProperties>
      vkGetPhysicalDeviceMemoryProperties;
  VulkanFunction<PFN_vkGetPhysicalDeviceMemoryProperties2>
      vkGetPhysicalDeviceMemoryProperties2;
  VulkanFunction<PFN_vkGetPhysicalDeviceProperties>
      vkGetPhysicalDeviceProperties;
  VulkanFunction<PFN_vkGetPhysicalDeviceProperties2>
      vkGetPhysicalDeviceProperties2;
  VulkanFunction<PFN_vkGetPhysicalDeviceQueueFamilyProperties>
      vkGetPhysicalDeviceQueueFamilyProperties;

#if DCHECK_IS_ON()
  VulkanFunction<PFN_vkCreateDebugReportCallbackEXT>
      vkCreateDebugReportCallbackEXT;
  VulkanFunction<PFN_vkDestroyDebugReportCallbackEXT>
      vkDestroyDebugReportCallbackEXT;
#endif  // DCHECK_IS_ON()

  VulkanFunction<PFN_vkDestroySurfaceKHR> vkDestroySurfaceKHR;
  VulkanFunction<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>
      vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
  VulkanFunction<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>
      vkGetPhysicalDeviceSurfaceFormatsKHR;
  VulkanFunction<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>
      vkGetPhysicalDeviceSurfaceSupportKHR;

  VulkanFunction<PFN_vkCreateHeadlessSurfaceEXT> vkCreateHeadlessSurfaceEXT;

#if defined(USE_VULKAN_XCB)
  VulkanFunction<PFN_vkCreateXcbSurfaceKHR> vkCreateXcbSurfaceKHR;
  VulkanFunction<PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR>
      vkGetPhysicalDeviceXcbPresentationSupportKHR;
#endif  // defined(USE_VULKAN_XCB)

#if BUILDFLAG(IS_WIN)
  VulkanFunction<PFN_vkCreateWin32SurfaceKHR> vkCreateWin32SurfaceKHR;
  VulkanFunction<PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR>
      vkGetPhysicalDeviceWin32PresentationSupportKHR;
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
  VulkanFunction<PFN_vkCreateAndroidSurfaceKHR> vkCreateAndroidSurfaceKHR;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_FUCHSIA)
  VulkanFunction<PFN_vkCreateImagePipeSurfaceFUCHSIA>
      vkCreateImagePipeSurfaceFUCHSIA;
#endif  // BUILDFLAG(IS_FUCHSIA)

  // Device functions
  VulkanFunction<PFN_vkAllocateCommandBuffers> vkAllocateCommandBuffers;
  VulkanFunction<PFN_vkAllocateDescriptorSets> vkAllocateDescriptorSets;
  VulkanFunction<PFN_vkAllocateMemory> vkAllocateMemory;
  VulkanFunction<PFN_vkBeginCommandBuffer> vkBeginCommandBuffer;
  VulkanFunction<PFN_vkBindBufferMemory> vkBindBufferMemory;
  VulkanFunction<PFN_vkBindBufferMemory2> vkBindBufferMemory2;
  VulkanFunction<PFN_vkBindImageMemory> vkBindImageMemory;
  VulkanFunction<PFN_vkBindImageMemory2> vkBindImageMemory2;
  VulkanFunction<PFN_vkCmdBeginRenderPass> vkCmdBeginRenderPass;
  VulkanFunction<PFN_vkCmdBindDescriptorSets> vkCmdBindDescriptorSets;
  VulkanFunction<PFN_vkCmdBindPipeline> vkCmdBindPipeline;
  VulkanFunction<PFN_vkCmdBindVertexBuffers> vkCmdBindVertexBuffers;
  VulkanFunction<PFN_vkCmdCopyBuffer> vkCmdCopyBuffer;
  VulkanFunction<PFN_vkCmdCopyBufferToImage> vkCmdCopyBufferToImage;
  VulkanFunction<PFN_vkCmdCopyImage> vkCmdCopyImage;
  VulkanFunction<PFN_vkCmdCopyImageToBuffer> vkCmdCopyImageToBuffer;
  VulkanFunction<PFN_vkCmdDraw> vkCmdDraw;
  VulkanFunction<PFN_vkCmdEndRenderPass> vkCmdEndRenderPass;
  VulkanFunction<PFN_vkCmdExecuteCommands> vkCmdExecuteCommands;
  VulkanFunction<PFN_vkCmdNextSubpass> vkCmdNextSubpass;
  VulkanFunction<PFN_vkCmdPipelineBarrier> vkCmdPipelineBarrier;
  VulkanFunction<PFN_vkCmdPushConstants> vkCmdPushConstants;
  VulkanFunction<PFN_vkCmdSetScissor> vkCmdSetScissor;
  VulkanFunction<PFN_vkCmdSetViewport> vkCmdSetViewport;
  VulkanFunction<PFN_vkCreateBuffer> vkCreateBuffer;
  VulkanFunction<PFN_vkCreateCommandPool> vkCreateCommandPool;
  VulkanFunction<PFN_vkCreateDescriptorPool> vkCreateDescriptorPool;
  VulkanFunction<PFN_vkCreateDescriptorSetLayout> vkCreateDescriptorSetLayout;
  VulkanFunction<PFN_vkCreateFence> vkCreateFence;
  VulkanFunction<PFN_vkCreateFramebuffer> vkCreateFramebuffer;
  VulkanFunction<PFN_vkCreateGraphicsPipelines> vkCreateGraphicsPipelines;
  VulkanFunction<PFN_vkCreateImage> vkCreateImage;
  VulkanFunction<PFN_vkCreateImageView> vkCreateImageView;
  VulkanFunction<PFN_vkCreatePipelineLayout> vkCreatePipelineLayout;
  VulkanFunction<PFN_vkCreateRenderPass> vkCreateRenderPass;
  VulkanFunction<PFN_vkCreateSampler> vkCreateSampler;
  VulkanFunction<PFN_vkCreateSemaphore> vkCreateSemaphore;
  VulkanFunction<PFN_vkCreateShaderModule> vkCreateShaderModule;
  VulkanFunction<PFN_vkDestroyBuffer> vkDestroyBuffer;
  VulkanFunction<PFN_vkDestroyCommandPool> vkDestroyCommandPool;
  VulkanFunction<PFN_vkDestroyDescriptorPool> vkDestroyDescriptorPool;
  VulkanFunction<PFN_vkDestroyDescriptorSetLayout> vkDestroyDescriptorSetLayout;
  VulkanFunction<PFN_vkDestroyDevice> vkDestroyDevice;
  VulkanFunction<PFN_vkDestroyFence> vkDestroyFence;
  VulkanFunction<PFN_vkDestroyFramebuffer> vkDestroyFramebuffer;
  VulkanFunction<PFN_vkDestroyImage> vkDestroyImage;
  VulkanFunction<PFN_vkDestroyImageView> vkDestroyImageView;
  VulkanFunction<PFN_vkDestroyPipeline> vkDestroyPipeline;
  VulkanFunction<PFN_vkDestroyPipelineLayout> vkDestroyPipelineLayout;
  VulkanFunction<PFN_vkDestroyRenderPass> vkDestroyRenderPass;
  VulkanFunction<PFN_vkDestroySampler> vkDestroySampler;
  VulkanFunction<PFN_vkDestroySemaphore> vkDestroySemaphore;
  VulkanFunction<PFN_vkDestroyShaderModule> vkDestroyShaderModule;
  VulkanFunction<PFN_vkDeviceWaitIdle> vkDeviceWaitIdle;
  VulkanFunction<PFN_vkFlushMappedMemoryRanges> vkFlushMappedMemoryRanges;
  VulkanFunction<PFN_vkEndCommandBuffer> vkEndCommandBuffer;
  VulkanFunction<PFN_vkFreeCommandBuffers> vkFreeCommandBuffers;
  VulkanFunction<PFN_vkFreeDescriptorSets> vkFreeDescriptorSets;
  VulkanFunction<PFN_vkFreeMemory> vkFreeMemory;
  VulkanFunction<PFN_vkInvalidateMappedMemoryRanges>
      vkInvalidateMappedMemoryRanges;
  VulkanFunction<PFN_vkGetBufferMemoryRequirements>
      vkGetBufferMemoryRequirements;
  VulkanFunction<PFN_vkGetBufferMemoryRequirements2>
      vkGetBufferMemoryRequirements2;
  VulkanFunction<PFN_vkGetDeviceQueue> vkGetDeviceQueue;
  VulkanFunction<PFN_vkGetDeviceQueue2> vkGetDeviceQueue2;
  VulkanFunction<PFN_vkGetFenceStatus> vkGetFenceStatus;
  VulkanFunction<PFN_vkGetImageMemoryRequirements> vkGetImageMemoryRequirements;
  VulkanFunction<PFN_vkGetImageMemoryRequirements2>
      vkGetImageMemoryRequirements2;
  VulkanFunction<PFN_vkGetImageSubresourceLayout> vkGetImageSubresourceLayout;
  VulkanFunction<PFN_vkMapMemory> vkMapMemory;
  VulkanFunction<PFN_vkQueueSubmit> vkQueueSubmit;
  VulkanFunction<PFN_vkQueueWaitIdle> vkQueueWaitIdle;
  VulkanFunction<PFN_vkResetCommandBuffer> vkResetCommandBuffer;
  VulkanFunction<PFN_vkResetFences> vkResetFences;
  VulkanFunction<PFN_vkUnmapMemory> vkUnmapMemory;
  VulkanFunction<PFN_vkUpdateDescriptorSets> vkUpdateDescriptorSets;
  VulkanFunction<PFN_vkWaitForFences> vkWaitForFences;

#if BUILDFLAG(IS_ANDROID)
  VulkanFunction<PFN_vkGetAndroidHardwareBufferPropertiesANDROID>
      vkGetAndroidHardwareBufferPropertiesANDROID;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_POSIX)
  VulkanFunction<PFN_vkGetSemaphoreFdKHR> vkGetSemaphoreFdKHR;
  VulkanFunction<PFN_vkImportSemaphoreFdKHR> vkImportSemaphoreFdKHR;
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_WIN)
  VulkanFunction<PFN_vkGetSemaphoreWin32HandleKHR> vkGetSemaphoreWin32HandleKHR;
  VulkanFunction<PFN_vkImportSemaphoreWin32HandleKHR>
      vkImportSemaphoreWin32HandleKHR;
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_POSIX)
  VulkanFunction<PFN_vkGetMemoryFdKHR> vkGetMemoryFdKHR;
  VulkanFunction<PFN_vkGetMemoryFdPropertiesKHR> vkGetMemoryFdPropertiesKHR;
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_WIN)
  VulkanFunction<PFN_vkGetMemoryWin32HandleKHR> vkGetMemoryWin32HandleKHR;
  VulkanFunction<PFN_vkGetMemoryWin32HandlePropertiesKHR>
      vkGetMemoryWin32HandlePropertiesKHR;
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_FUCHSIA)
  VulkanFunction<PFN_vkImportSemaphoreZirconHandleFUCHSIA>
      vkImportSemaphoreZirconHandleFUCHSIA;
  VulkanFunction<PFN_vkGetSemaphoreZirconHandleFUCHSIA>
      vkGetSemaphoreZirconHandleFUCHSIA;
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_FUCHSIA)
  VulkanFunction<PFN_vkGetMemoryZirconHandleFUCHSIA>
      vkGetMemoryZirconHandleFUCHSIA;
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_FUCHSIA)
  VulkanFunction<PFN_vkCreateBufferCollectionFUCHSIA>
      vkCreateBufferCollectionFUCHSIA;
  VulkanFunction<PFN_vkSetBufferCollectionImageConstraintsFUCHSIA>
      vkSetBufferCollectionImageConstraintsFUCHSIA;
  VulkanFunction<PFN_vkGetBufferCollectionPropertiesFUCHSIA>
      vkGetBufferCollectionPropertiesFUCHSIA;
  VulkanFunction<PFN_vkDestroyBufferCollectionFUCHSIA>
      vkDestroyBufferCollectionFUCHSIA;
#endif  // BUILDFLAG(IS_FUCHSIA)

  VulkanFunction<PFN_vkAcquireNextImageKHR> vkAcquireNextImageKHR;
  VulkanFunction<PFN_vkCreateSwapchainKHR> vkCreateSwapchainKHR;
  VulkanFunction<PFN_vkDestroySwapchainKHR> vkDestroySwapchainKHR;
  VulkanFunction<PFN_vkGetSwapchainImagesKHR> vkGetSwapchainImagesKHR;
  VulkanFunction<PFN_vkQueuePresentKHR> vkQueuePresentKHR;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  VulkanFunction<PFN_vkGetImageDrmFormatModifierPropertiesEXT>
      vkGetImageDrmFormatModifierPropertiesEXT;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

 private:
  bool BindUnassociatedFunctionPointersCommon();
  // The `Bind*` functions will acquires lock, so should not be called with
  // with this lock held. Code that writes to members directly should take this
  // lock as well.
  base::Lock write_lock_;

  base::NativeLibrary loader_library_ = nullptr;
};

}  // namespace gpu

// Unassociated functions
ALWAYS_INLINE PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance instance,
                                                       const char* pName) {
  return gpu::GetVulkanFunctionPointers()->vkGetInstanceProcAddr(instance,
                                                                 pName);
}

ALWAYS_INLINE VkResult vkEnumerateInstanceVersion(uint32_t* pApiVersion) {
  return gpu::GetVulkanFunctionPointers()->vkEnumerateInstanceVersion(
      pApiVersion);
}
ALWAYS_INLINE VkResult vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo,
                                        const VkAllocationCallbacks* pAllocator,
                                        VkInstance* pInstance) {
  return gpu::GetVulkanFunctionPointers()->vkCreateInstance(
      pCreateInfo, pAllocator, pInstance);
}
ALWAYS_INLINE VkResult
vkEnumerateInstanceExtensionProperties(const char* pLayerName,
                                       uint32_t* pPropertyCount,
                                       VkExtensionProperties* pProperties) {
  return gpu::GetVulkanFunctionPointers()
      ->vkEnumerateInstanceExtensionProperties(pLayerName, pPropertyCount,
                                               pProperties);
}
ALWAYS_INLINE VkResult
vkEnumerateInstanceLayerProperties(uint32_t* pPropertyCount,
                                   VkLayerProperties* pProperties) {
  return gpu::GetVulkanFunctionPointers()->vkEnumerateInstanceLayerProperties(
      pPropertyCount, pProperties);
}

// Instance functions
ALWAYS_INLINE VkResult vkCreateDevice(VkPhysicalDevice physicalDevice,
                                      const VkDeviceCreateInfo* pCreateInfo,
                                      const VkAllocationCallbacks* pAllocator,
                                      VkDevice* pDevice) {
  return gpu::GetVulkanFunctionPointers()->vkCreateDevice(
      physicalDevice, pCreateInfo, pAllocator, pDevice);
}
ALWAYS_INLINE void vkDestroyInstance(VkInstance instance,
                                     const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyInstance(instance,
                                                             pAllocator);
}
ALWAYS_INLINE VkResult
vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice,
                                     const char* pLayerName,
                                     uint32_t* pPropertyCount,
                                     VkExtensionProperties* pProperties) {
  return gpu::GetVulkanFunctionPointers()->vkEnumerateDeviceExtensionProperties(
      physicalDevice, pLayerName, pPropertyCount, pProperties);
}
ALWAYS_INLINE VkResult
vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice,
                                 uint32_t* pPropertyCount,
                                 VkLayerProperties* pProperties) {
  return gpu::GetVulkanFunctionPointers()->vkEnumerateDeviceLayerProperties(
      physicalDevice, pPropertyCount, pProperties);
}
ALWAYS_INLINE VkResult
vkEnumeratePhysicalDevices(VkInstance instance,
                           uint32_t* pPhysicalDeviceCount,
                           VkPhysicalDevice* pPhysicalDevices) {
  return gpu::GetVulkanFunctionPointers()->vkEnumeratePhysicalDevices(
      instance, pPhysicalDeviceCount, pPhysicalDevices);
}
ALWAYS_INLINE PFN_vkVoidFunction vkGetDeviceProcAddr(VkDevice device,
                                                     const char* pName) {
  return gpu::GetVulkanFunctionPointers()->vkGetDeviceProcAddr(device, pName);
}
ALWAYS_INLINE void vkGetPhysicalDeviceExternalSemaphoreProperties(
    VkPhysicalDevice physicalDevice,
    const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo,
    VkExternalSemaphoreProperties* pExternalSemaphoreProperties) {
  return gpu::GetVulkanFunctionPointers()
      ->vkGetPhysicalDeviceExternalSemaphoreProperties(
          physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties);
}
ALWAYS_INLINE void vkGetPhysicalDeviceFeatures2(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceFeatures2* pFeatures) {
  return gpu::GetVulkanFunctionPointers()->vkGetPhysicalDeviceFeatures2(
      physicalDevice, pFeatures);
}
ALWAYS_INLINE void vkGetPhysicalDeviceFormatProperties(
    VkPhysicalDevice physicalDevice,
    VkFormat format,
    VkFormatProperties* pFormatProperties) {
  return gpu::GetVulkanFunctionPointers()->vkGetPhysicalDeviceFormatProperties(
      physicalDevice, format, pFormatProperties);
}
ALWAYS_INLINE void vkGetPhysicalDeviceFormatProperties2(
    VkPhysicalDevice physicalDevice,
    VkFormat format,
    VkFormatProperties2* pFormatProperties) {
  return gpu::GetVulkanFunctionPointers()->vkGetPhysicalDeviceFormatProperties2(
      physicalDevice, format, pFormatProperties);
}
ALWAYS_INLINE VkResult vkGetPhysicalDeviceImageFormatProperties2(
    VkPhysicalDevice physicalDevice,
    const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo,
    VkImageFormatProperties2* pImageFormatProperties) {
  return gpu::GetVulkanFunctionPointers()
      ->vkGetPhysicalDeviceImageFormatProperties2(
          physicalDevice, pImageFormatInfo, pImageFormatProperties);
}
ALWAYS_INLINE void vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceMemoryProperties* pMemoryProperties) {
  return gpu::GetVulkanFunctionPointers()->vkGetPhysicalDeviceMemoryProperties(
      physicalDevice, pMemoryProperties);
}
ALWAYS_INLINE void vkGetPhysicalDeviceMemoryProperties2(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceMemoryProperties2* pMemoryProperties) {
  return gpu::GetVulkanFunctionPointers()->vkGetPhysicalDeviceMemoryProperties2(
      physicalDevice, pMemoryProperties);
}
ALWAYS_INLINE void vkGetPhysicalDeviceProperties(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceProperties* pProperties) {
  return gpu::GetVulkanFunctionPointers()->vkGetPhysicalDeviceProperties(
      physicalDevice, pProperties);
}
ALWAYS_INLINE void vkGetPhysicalDeviceProperties2(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceProperties2* pProperties) {
  return gpu::GetVulkanFunctionPointers()->vkGetPhysicalDeviceProperties2(
      physicalDevice, pProperties);
}
ALWAYS_INLINE void vkGetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice physicalDevice,
    uint32_t* pQueueFamilyPropertyCount,
    VkQueueFamilyProperties* pQueueFamilyProperties) {
  return gpu::GetVulkanFunctionPointers()
      ->vkGetPhysicalDeviceQueueFamilyProperties(
          physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
}

#if DCHECK_IS_ON()
ALWAYS_INLINE VkResult vkCreateDebugReportCallbackEXT(
    VkInstance instance,
    const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugReportCallbackEXT* pCallback) {
  return gpu::GetVulkanFunctionPointers()->vkCreateDebugReportCallbackEXT(
      instance, pCreateInfo, pAllocator, pCallback);
}
ALWAYS_INLINE void vkDestroyDebugReportCallbackEXT(
    VkInstance instance,
    VkDebugReportCallbackEXT callback,
    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyDebugReportCallbackEXT(
      instance, callback, pAllocator);
}
#endif  // DCHECK_IS_ON()

ALWAYS_INLINE void vkDestroySurfaceKHR(
    VkInstance instance,
    VkSurfaceKHR surface,
    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroySurfaceKHR(
      instance, surface, pAllocator);
}
ALWAYS_INLINE VkResult vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface,
    VkSurfaceCapabilitiesKHR* pSurfaceCapabilities) {
  return gpu::GetVulkanFunctionPointers()
      ->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                                  pSurfaceCapabilities);
}
ALWAYS_INLINE VkResult
vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice,
                                     VkSurfaceKHR surface,
                                     uint32_t* pSurfaceFormatCount,
                                     VkSurfaceFormatKHR* pSurfaceFormats) {
  return gpu::GetVulkanFunctionPointers()->vkGetPhysicalDeviceSurfaceFormatsKHR(
      physicalDevice, surface, pSurfaceFormatCount, pSurfaceFormats);
}
ALWAYS_INLINE VkResult
vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice,
                                     uint32_t queueFamilyIndex,
                                     VkSurfaceKHR surface,
                                     VkBool32* pSupported) {
  return gpu::GetVulkanFunctionPointers()->vkGetPhysicalDeviceSurfaceSupportKHR(
      physicalDevice, queueFamilyIndex, surface, pSupported);
}

ALWAYS_INLINE VkResult
vkCreateHeadlessSurfaceEXT(VkInstance instance,
                           const VkHeadlessSurfaceCreateInfoEXT* pCreateInfo,
                           const VkAllocationCallbacks* pAllocator,
                           VkSurfaceKHR* pSurface) {
  return gpu::GetVulkanFunctionPointers()->vkCreateHeadlessSurfaceEXT(
      instance, pCreateInfo, pAllocator, pSurface);
}

#if defined(USE_VULKAN_XCB)
ALWAYS_INLINE VkResult
vkCreateXcbSurfaceKHR(VkInstance instance,
                      const VkXcbSurfaceCreateInfoKHR* pCreateInfo,
                      const VkAllocationCallbacks* pAllocator,
                      VkSurfaceKHR* pSurface) {
  return gpu::GetVulkanFunctionPointers()->vkCreateXcbSurfaceKHR(
      instance, pCreateInfo, pAllocator, pSurface);
}
ALWAYS_INLINE VkBool32
vkGetPhysicalDeviceXcbPresentationSupportKHR(VkPhysicalDevice physicalDevice,
                                             uint32_t queueFamilyIndex,
                                             xcb_connection_t* connection,
                                             xcb_visualid_t visual_id) {
  return gpu::GetVulkanFunctionPointers()
      ->vkGetPhysicalDeviceXcbPresentationSupportKHR(
          physicalDevice, queueFamilyIndex, connection, visual_id);
}
#endif  // defined(USE_VULKAN_XCB)

#if BUILDFLAG(IS_WIN)
ALWAYS_INLINE VkResult
vkCreateWin32SurfaceKHR(VkInstance instance,
                        const VkWin32SurfaceCreateInfoKHR* pCreateInfo,
                        const VkAllocationCallbacks* pAllocator,
                        VkSurfaceKHR* pSurface) {
  return gpu::GetVulkanFunctionPointers()->vkCreateWin32SurfaceKHR(
      instance, pCreateInfo, pAllocator, pSurface);
}
ALWAYS_INLINE VkBool32
vkGetPhysicalDeviceWin32PresentationSupportKHR(VkPhysicalDevice physicalDevice,
                                               uint32_t queueFamilyIndex) {
  return gpu::GetVulkanFunctionPointers()
      ->vkGetPhysicalDeviceWin32PresentationSupportKHR(physicalDevice,
                                                       queueFamilyIndex);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
ALWAYS_INLINE VkResult
vkCreateAndroidSurfaceKHR(VkInstance instance,
                          const VkAndroidSurfaceCreateInfoKHR* pCreateInfo,
                          const VkAllocationCallbacks* pAllocator,
                          VkSurfaceKHR* pSurface) {
  return gpu::GetVulkanFunctionPointers()->vkCreateAndroidSurfaceKHR(
      instance, pCreateInfo, pAllocator, pSurface);
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_FUCHSIA)
ALWAYS_INLINE VkResult vkCreateImagePipeSurfaceFUCHSIA(
    VkInstance instance,
    const VkImagePipeSurfaceCreateInfoFUCHSIA* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkSurfaceKHR* pSurface) {
  return gpu::GetVulkanFunctionPointers()->vkCreateImagePipeSurfaceFUCHSIA(
      instance, pCreateInfo, pAllocator, pSurface);
}
#endif  // BUILDFLAG(IS_FUCHSIA)

// Device functions
ALWAYS_INLINE VkResult
vkAllocateCommandBuffers(VkDevice device,
                         const VkCommandBufferAllocateInfo* pAllocateInfo,
                         VkCommandBuffer* pCommandBuffers) {
  return gpu::GetVulkanFunctionPointers()->vkAllocateCommandBuffers(
      device, pAllocateInfo, pCommandBuffers);
}
ALWAYS_INLINE VkResult
vkAllocateDescriptorSets(VkDevice device,
                         const VkDescriptorSetAllocateInfo* pAllocateInfo,
                         VkDescriptorSet* pDescriptorSets) {
  return gpu::GetVulkanFunctionPointers()->vkAllocateDescriptorSets(
      device, pAllocateInfo, pDescriptorSets);
}
ALWAYS_INLINE VkResult
vkAllocateMemory(VkDevice device,
                 const VkMemoryAllocateInfo* pAllocateInfo,
                 const VkAllocationCallbacks* pAllocator,
                 VkDeviceMemory* pMemory) {
  return gpu::GetVulkanFunctionPointers()->vkAllocateMemory(
      device, pAllocateInfo, pAllocator, pMemory);
}
ALWAYS_INLINE VkResult
vkBeginCommandBuffer(VkCommandBuffer commandBuffer,
                     const VkCommandBufferBeginInfo* pBeginInfo) {
  return gpu::GetVulkanFunctionPointers()->vkBeginCommandBuffer(commandBuffer,
                                                                pBeginInfo);
}
ALWAYS_INLINE VkResult vkBindBufferMemory(VkDevice device,
                                          VkBuffer buffer,
                                          VkDeviceMemory memory,
                                          VkDeviceSize memoryOffset) {
  return gpu::GetVulkanFunctionPointers()->vkBindBufferMemory(
      device, buffer, memory, memoryOffset);
}
ALWAYS_INLINE VkResult
vkBindBufferMemory2(VkDevice device,
                    uint32_t bindInfoCount,
                    const VkBindBufferMemoryInfo* pBindInfos) {
  return gpu::GetVulkanFunctionPointers()->vkBindBufferMemory2(
      device, bindInfoCount, pBindInfos);
}
ALWAYS_INLINE VkResult vkBindImageMemory(VkDevice device,
                                         VkImage image,
                                         VkDeviceMemory memory,
                                         VkDeviceSize memoryOffset) {
  return gpu::GetVulkanFunctionPointers()->vkBindImageMemory(
      device, image, memory, memoryOffset);
}
ALWAYS_INLINE VkResult
vkBindImageMemory2(VkDevice device,
                   uint32_t bindInfoCount,
                   const VkBindImageMemoryInfo* pBindInfos) {
  return gpu::GetVulkanFunctionPointers()->vkBindImageMemory2(
      device, bindInfoCount, pBindInfos);
}
ALWAYS_INLINE void vkCmdBeginRenderPass(
    VkCommandBuffer commandBuffer,
    const VkRenderPassBeginInfo* pRenderPassBegin,
    VkSubpassContents contents) {
  return gpu::GetVulkanFunctionPointers()->vkCmdBeginRenderPass(
      commandBuffer, pRenderPassBegin, contents);
}
ALWAYS_INLINE void vkCmdBindDescriptorSets(
    VkCommandBuffer commandBuffer,
    VkPipelineBindPoint pipelineBindPoint,
    VkPipelineLayout layout,
    uint32_t firstSet,
    uint32_t descriptorSetCount,
    const VkDescriptorSet* pDescriptorSets,
    uint32_t dynamicOffsetCount,
    const uint32_t* pDynamicOffsets) {
  return gpu::GetVulkanFunctionPointers()->vkCmdBindDescriptorSets(
      commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount,
      pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}
ALWAYS_INLINE void vkCmdBindPipeline(VkCommandBuffer commandBuffer,
                                     VkPipelineBindPoint pipelineBindPoint,
                                     VkPipeline pipeline) {
  return gpu::GetVulkanFunctionPointers()->vkCmdBindPipeline(
      commandBuffer, pipelineBindPoint, pipeline);
}
ALWAYS_INLINE void vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer,
                                          uint32_t firstBinding,
                                          uint32_t bindingCount,
                                          const VkBuffer* pBuffers,
                                          const VkDeviceSize* pOffsets) {
  return gpu::GetVulkanFunctionPointers()->vkCmdBindVertexBuffers(
      commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
}
ALWAYS_INLINE void vkCmdCopyBuffer(VkCommandBuffer commandBuffer,
                                   VkBuffer srcBuffer,
                                   VkBuffer dstBuffer,
                                   uint32_t regionCount,
                                   const VkBufferCopy* pRegions) {
  return gpu::GetVulkanFunctionPointers()->vkCmdCopyBuffer(
      commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
}
ALWAYS_INLINE void vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer,
                                          VkBuffer srcBuffer,
                                          VkImage dstImage,
                                          VkImageLayout dstImageLayout,
                                          uint32_t regionCount,
                                          const VkBufferImageCopy* pRegions) {
  return gpu::GetVulkanFunctionPointers()->vkCmdCopyBufferToImage(
      commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount,
      pRegions);
}
ALWAYS_INLINE void vkCmdCopyImage(VkCommandBuffer commandBuffer,
                                  VkImage srcImage,
                                  VkImageLayout srcImageLayout,
                                  VkImage dstImage,
                                  VkImageLayout dstImageLayout,
                                  uint32_t regionCount,
                                  const VkImageCopy* pRegions) {
  return gpu::GetVulkanFunctionPointers()->vkCmdCopyImage(
      commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout,
      regionCount, pRegions);
}
ALWAYS_INLINE void vkCmdCopyImageToBuffer(VkCommandBuffer commandBuffer,
                                          VkImage srcImage,
                                          VkImageLayout srcImageLayout,
                                          VkBuffer dstBuffer,
                                          uint32_t regionCount,
                                          const VkBufferImageCopy* pRegions) {
  return gpu::GetVulkanFunctionPointers()->vkCmdCopyImageToBuffer(
      commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount,
      pRegions);
}
ALWAYS_INLINE void vkCmdDraw(VkCommandBuffer commandBuffer,
                             uint32_t vertexCount,
                             uint32_t instanceCount,
                             uint32_t firstVertex,
                             uint32_t firstInstance) {
  return gpu::GetVulkanFunctionPointers()->vkCmdDraw(
      commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}
ALWAYS_INLINE void vkCmdEndRenderPass(VkCommandBuffer commandBuffer) {
  return gpu::GetVulkanFunctionPointers()->vkCmdEndRenderPass(commandBuffer);
}
ALWAYS_INLINE void vkCmdExecuteCommands(
    VkCommandBuffer commandBuffer,
    uint32_t commandBufferCount,
    const VkCommandBuffer* pCommandBuffers) {
  return gpu::GetVulkanFunctionPointers()->vkCmdExecuteCommands(
      commandBuffer, commandBufferCount, pCommandBuffers);
}
ALWAYS_INLINE void vkCmdNextSubpass(VkCommandBuffer commandBuffer,
                                    VkSubpassContents contents) {
  return gpu::GetVulkanFunctionPointers()->vkCmdNextSubpass(commandBuffer,
                                                            contents);
}
ALWAYS_INLINE void vkCmdPipelineBarrier(
    VkCommandBuffer commandBuffer,
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask,
    VkDependencyFlags dependencyFlags,
    uint32_t memoryBarrierCount,
    const VkMemoryBarrier* pMemoryBarriers,
    uint32_t bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier* pBufferMemoryBarriers,
    uint32_t imageMemoryBarrierCount,
    const VkImageMemoryBarrier* pImageMemoryBarriers) {
  return gpu::GetVulkanFunctionPointers()->vkCmdPipelineBarrier(
      commandBuffer, srcStageMask, dstStageMask, dependencyFlags,
      memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount,
      pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}
ALWAYS_INLINE void vkCmdPushConstants(VkCommandBuffer commandBuffer,
                                      VkPipelineLayout layout,
                                      VkShaderStageFlags stageFlags,
                                      uint32_t offset,
                                      uint32_t size,
                                      const void* pValues) {
  return gpu::GetVulkanFunctionPointers()->vkCmdPushConstants(
      commandBuffer, layout, stageFlags, offset, size, pValues);
}
ALWAYS_INLINE void vkCmdSetScissor(VkCommandBuffer commandBuffer,
                                   uint32_t firstScissor,
                                   uint32_t scissorCount,
                                   const VkRect2D* pScissors) {
  return gpu::GetVulkanFunctionPointers()->vkCmdSetScissor(
      commandBuffer, firstScissor, scissorCount, pScissors);
}
ALWAYS_INLINE void vkCmdSetViewport(VkCommandBuffer commandBuffer,
                                    uint32_t firstViewport,
                                    uint32_t viewportCount,
                                    const VkViewport* pViewports) {
  return gpu::GetVulkanFunctionPointers()->vkCmdSetViewport(
      commandBuffer, firstViewport, viewportCount, pViewports);
}
ALWAYS_INLINE VkResult vkCreateBuffer(VkDevice device,
                                      const VkBufferCreateInfo* pCreateInfo,
                                      const VkAllocationCallbacks* pAllocator,
                                      VkBuffer* pBuffer) {
  return gpu::GetVulkanFunctionPointers()->vkCreateBuffer(device, pCreateInfo,
                                                          pAllocator, pBuffer);
}
ALWAYS_INLINE VkResult
vkCreateCommandPool(VkDevice device,
                    const VkCommandPoolCreateInfo* pCreateInfo,
                    const VkAllocationCallbacks* pAllocator,
                    VkCommandPool* pCommandPool) {
  return gpu::GetVulkanFunctionPointers()->vkCreateCommandPool(
      device, pCreateInfo, pAllocator, pCommandPool);
}
ALWAYS_INLINE VkResult
vkCreateDescriptorPool(VkDevice device,
                       const VkDescriptorPoolCreateInfo* pCreateInfo,
                       const VkAllocationCallbacks* pAllocator,
                       VkDescriptorPool* pDescriptorPool) {
  return gpu::GetVulkanFunctionPointers()->vkCreateDescriptorPool(
      device, pCreateInfo, pAllocator, pDescriptorPool);
}
ALWAYS_INLINE VkResult
vkCreateDescriptorSetLayout(VkDevice device,
                            const VkDescriptorSetLayoutCreateInfo* pCreateInfo,
                            const VkAllocationCallbacks* pAllocator,
                            VkDescriptorSetLayout* pSetLayout) {
  return gpu::GetVulkanFunctionPointers()->vkCreateDescriptorSetLayout(
      device, pCreateInfo, pAllocator, pSetLayout);
}
ALWAYS_INLINE VkResult vkCreateFence(VkDevice device,
                                     const VkFenceCreateInfo* pCreateInfo,
                                     const VkAllocationCallbacks* pAllocator,
                                     VkFence* pFence) {
  return gpu::GetVulkanFunctionPointers()->vkCreateFence(device, pCreateInfo,
                                                         pAllocator, pFence);
}
ALWAYS_INLINE VkResult
vkCreateFramebuffer(VkDevice device,
                    const VkFramebufferCreateInfo* pCreateInfo,
                    const VkAllocationCallbacks* pAllocator,
                    VkFramebuffer* pFramebuffer) {
  return gpu::GetVulkanFunctionPointers()->vkCreateFramebuffer(
      device, pCreateInfo, pAllocator, pFramebuffer);
}
ALWAYS_INLINE VkResult
vkCreateGraphicsPipelines(VkDevice device,
                          VkPipelineCache pipelineCache,
                          uint32_t createInfoCount,
                          const VkGraphicsPipelineCreateInfo* pCreateInfos,
                          const VkAllocationCallbacks* pAllocator,
                          VkPipeline* pPipelines) {
  return gpu::GetVulkanFunctionPointers()->vkCreateGraphicsPipelines(
      device, pipelineCache, createInfoCount, pCreateInfos, pAllocator,
      pPipelines);
}
ALWAYS_INLINE VkResult vkCreateImage(VkDevice device,
                                     const VkImageCreateInfo* pCreateInfo,
                                     const VkAllocationCallbacks* pAllocator,
                                     VkImage* pImage) {
  return gpu::GetVulkanFunctionPointers()->vkCreateImage(device, pCreateInfo,
                                                         pAllocator, pImage);
}
ALWAYS_INLINE VkResult
vkCreateImageView(VkDevice device,
                  const VkImageViewCreateInfo* pCreateInfo,
                  const VkAllocationCallbacks* pAllocator,
                  VkImageView* pView) {
  return gpu::GetVulkanFunctionPointers()->vkCreateImageView(
      device, pCreateInfo, pAllocator, pView);
}
ALWAYS_INLINE VkResult
vkCreatePipelineLayout(VkDevice device,
                       const VkPipelineLayoutCreateInfo* pCreateInfo,
                       const VkAllocationCallbacks* pAllocator,
                       VkPipelineLayout* pPipelineLayout) {
  return gpu::GetVulkanFunctionPointers()->vkCreatePipelineLayout(
      device, pCreateInfo, pAllocator, pPipelineLayout);
}
ALWAYS_INLINE VkResult
vkCreateRenderPass(VkDevice device,
                   const VkRenderPassCreateInfo* pCreateInfo,
                   const VkAllocationCallbacks* pAllocator,
                   VkRenderPass* pRenderPass) {
  return gpu::GetVulkanFunctionPointers()->vkCreateRenderPass(
      device, pCreateInfo, pAllocator, pRenderPass);
}
ALWAYS_INLINE VkResult vkCreateSampler(VkDevice device,
                                       const VkSamplerCreateInfo* pCreateInfo,
                                       const VkAllocationCallbacks* pAllocator,
                                       VkSampler* pSampler) {
  return gpu::GetVulkanFunctionPointers()->vkCreateSampler(
      device, pCreateInfo, pAllocator, pSampler);
}
ALWAYS_INLINE VkResult
vkCreateSemaphore(VkDevice device,
                  const VkSemaphoreCreateInfo* pCreateInfo,
                  const VkAllocationCallbacks* pAllocator,
                  VkSemaphore* pSemaphore) {
  return gpu::GetVulkanFunctionPointers()->vkCreateSemaphore(
      device, pCreateInfo, pAllocator, pSemaphore);
}
ALWAYS_INLINE VkResult
vkCreateShaderModule(VkDevice device,
                     const VkShaderModuleCreateInfo* pCreateInfo,
                     const VkAllocationCallbacks* pAllocator,
                     VkShaderModule* pShaderModule) {
  return gpu::GetVulkanFunctionPointers()->vkCreateShaderModule(
      device, pCreateInfo, pAllocator, pShaderModule);
}
ALWAYS_INLINE void vkDestroyBuffer(VkDevice device,
                                   VkBuffer buffer,
                                   const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyBuffer(device, buffer,
                                                           pAllocator);
}
ALWAYS_INLINE void vkDestroyCommandPool(
    VkDevice device,
    VkCommandPool commandPool,
    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyCommandPool(
      device, commandPool, pAllocator);
}
ALWAYS_INLINE void vkDestroyDescriptorPool(
    VkDevice device,
    VkDescriptorPool descriptorPool,
    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyDescriptorPool(
      device, descriptorPool, pAllocator);
}
ALWAYS_INLINE void vkDestroyDescriptorSetLayout(
    VkDevice device,
    VkDescriptorSetLayout descriptorSetLayout,
    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyDescriptorSetLayout(
      device, descriptorSetLayout, pAllocator);
}
ALWAYS_INLINE void vkDestroyDevice(VkDevice device,
                                   const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyDevice(device, pAllocator);
}
ALWAYS_INLINE void vkDestroyFence(VkDevice device,
                                  VkFence fence,
                                  const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyFence(device, fence,
                                                          pAllocator);
}
ALWAYS_INLINE void vkDestroyFramebuffer(
    VkDevice device,
    VkFramebuffer framebuffer,
    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyFramebuffer(
      device, framebuffer, pAllocator);
}
ALWAYS_INLINE void vkDestroyImage(VkDevice device,
                                  VkImage image,
                                  const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyImage(device, image,
                                                          pAllocator);
}
ALWAYS_INLINE void vkDestroyImageView(VkDevice device,
                                      VkImageView imageView,
                                      const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyImageView(device, imageView,
                                                              pAllocator);
}
ALWAYS_INLINE void vkDestroyPipeline(VkDevice device,
                                     VkPipeline pipeline,
                                     const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyPipeline(device, pipeline,
                                                             pAllocator);
}
ALWAYS_INLINE void vkDestroyPipelineLayout(
    VkDevice device,
    VkPipelineLayout pipelineLayout,
    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyPipelineLayout(
      device, pipelineLayout, pAllocator);
}
ALWAYS_INLINE void vkDestroyRenderPass(
    VkDevice device,
    VkRenderPass renderPass,
    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyRenderPass(
      device, renderPass, pAllocator);
}
ALWAYS_INLINE void vkDestroySampler(VkDevice device,
                                    VkSampler sampler,
                                    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroySampler(device, sampler,
                                                            pAllocator);
}
ALWAYS_INLINE void vkDestroySemaphore(VkDevice device,
                                      VkSemaphore semaphore,
                                      const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroySemaphore(device, semaphore,
                                                              pAllocator);
}
ALWAYS_INLINE void vkDestroyShaderModule(
    VkDevice device,
    VkShaderModule shaderModule,
    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyShaderModule(
      device, shaderModule, pAllocator);
}
ALWAYS_INLINE VkResult vkDeviceWaitIdle(VkDevice device) {
  return gpu::GetVulkanFunctionPointers()->vkDeviceWaitIdle(device);
}
ALWAYS_INLINE VkResult
vkFlushMappedMemoryRanges(VkDevice device,
                          uint32_t memoryRangeCount,
                          const VkMappedMemoryRange* pMemoryRanges) {
  return gpu::GetVulkanFunctionPointers()->vkFlushMappedMemoryRanges(
      device, memoryRangeCount, pMemoryRanges);
}
ALWAYS_INLINE VkResult vkEndCommandBuffer(VkCommandBuffer commandBuffer) {
  return gpu::GetVulkanFunctionPointers()->vkEndCommandBuffer(commandBuffer);
}
ALWAYS_INLINE void vkFreeCommandBuffers(
    VkDevice device,
    VkCommandPool commandPool,
    uint32_t commandBufferCount,
    const VkCommandBuffer* pCommandBuffers) {
  return gpu::GetVulkanFunctionPointers()->vkFreeCommandBuffers(
      device, commandPool, commandBufferCount, pCommandBuffers);
}
ALWAYS_INLINE VkResult
vkFreeDescriptorSets(VkDevice device,
                     VkDescriptorPool descriptorPool,
                     uint32_t descriptorSetCount,
                     const VkDescriptorSet* pDescriptorSets) {
  return gpu::GetVulkanFunctionPointers()->vkFreeDescriptorSets(
      device, descriptorPool, descriptorSetCount, pDescriptorSets);
}
ALWAYS_INLINE void vkFreeMemory(VkDevice device,
                                VkDeviceMemory memory,
                                const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkFreeMemory(device, memory,
                                                        pAllocator);
}
ALWAYS_INLINE VkResult
vkInvalidateMappedMemoryRanges(VkDevice device,
                               uint32_t memoryRangeCount,
                               const VkMappedMemoryRange* pMemoryRanges) {
  return gpu::GetVulkanFunctionPointers()->vkInvalidateMappedMemoryRanges(
      device, memoryRangeCount, pMemoryRanges);
}
ALWAYS_INLINE void vkGetBufferMemoryRequirements(
    VkDevice device,
    VkBuffer buffer,
    VkMemoryRequirements* pMemoryRequirements) {
  return gpu::GetVulkanFunctionPointers()->vkGetBufferMemoryRequirements(
      device, buffer, pMemoryRequirements);
}
ALWAYS_INLINE void vkGetBufferMemoryRequirements2(
    VkDevice device,
    const VkBufferMemoryRequirementsInfo2* pInfo,
    VkMemoryRequirements2* pMemoryRequirements) {
  return gpu::GetVulkanFunctionPointers()->vkGetBufferMemoryRequirements2(
      device, pInfo, pMemoryRequirements);
}
ALWAYS_INLINE void vkGetDeviceQueue(VkDevice device,
                                    uint32_t queueFamilyIndex,
                                    uint32_t queueIndex,
                                    VkQueue* pQueue) {
  return gpu::GetVulkanFunctionPointers()->vkGetDeviceQueue(
      device, queueFamilyIndex, queueIndex, pQueue);
}
ALWAYS_INLINE void vkGetDeviceQueue2(VkDevice device,
                                     const VkDeviceQueueInfo2* pQueueInfo,
                                     VkQueue* pQueue) {
  return gpu::GetVulkanFunctionPointers()->vkGetDeviceQueue2(device, pQueueInfo,
                                                             pQueue);
}
ALWAYS_INLINE VkResult vkGetFenceStatus(VkDevice device, VkFence fence) {
  return gpu::GetVulkanFunctionPointers()->vkGetFenceStatus(device, fence);
}
ALWAYS_INLINE void vkGetImageMemoryRequirements(
    VkDevice device,
    VkImage image,
    VkMemoryRequirements* pMemoryRequirements) {
  return gpu::GetVulkanFunctionPointers()->vkGetImageMemoryRequirements(
      device, image, pMemoryRequirements);
}
ALWAYS_INLINE void vkGetImageMemoryRequirements2(
    VkDevice device,
    const VkImageMemoryRequirementsInfo2* pInfo,
    VkMemoryRequirements2* pMemoryRequirements) {
  return gpu::GetVulkanFunctionPointers()->vkGetImageMemoryRequirements2(
      device, pInfo, pMemoryRequirements);
}
ALWAYS_INLINE void vkGetImageSubresourceLayout(
    VkDevice device,
    VkImage image,
    const VkImageSubresource* pSubresource,
    VkSubresourceLayout* pLayout) {
  return gpu::GetVulkanFunctionPointers()->vkGetImageSubresourceLayout(
      device, image, pSubresource, pLayout);
}
ALWAYS_INLINE VkResult vkMapMemory(VkDevice device,
                                   VkDeviceMemory memory,
                                   VkDeviceSize offset,
                                   VkDeviceSize size,
                                   VkMemoryMapFlags flags,
                                   void** ppData) {
  return gpu::GetVulkanFunctionPointers()->vkMapMemory(device, memory, offset,
                                                       size, flags, ppData);
}
ALWAYS_INLINE VkResult vkQueueSubmit(VkQueue queue,
                                     uint32_t submitCount,
                                     const VkSubmitInfo* pSubmits,
                                     VkFence fence) {
  base::Lock* lock = nullptr;
  auto it = gpu::GetVulkanFunctionPointers()->per_queue_lock_map.find(queue);
  if (it != gpu::GetVulkanFunctionPointers()->per_queue_lock_map.end()) {
    lock = it->second.get();
  }
  base::AutoLockMaybe auto_lock(lock);
  return gpu::GetVulkanFunctionPointers()->vkQueueSubmit(queue, submitCount,
                                                         pSubmits, fence);
}
ALWAYS_INLINE VkResult vkQueueWaitIdle(VkQueue queue) {
  base::Lock* lock = nullptr;
  auto it = gpu::GetVulkanFunctionPointers()->per_queue_lock_map.find(queue);
  if (it != gpu::GetVulkanFunctionPointers()->per_queue_lock_map.end()) {
    lock = it->second.get();
  }
  base::AutoLockMaybe auto_lock(lock);
  return gpu::GetVulkanFunctionPointers()->vkQueueWaitIdle(queue);
}
ALWAYS_INLINE VkResult vkResetCommandBuffer(VkCommandBuffer commandBuffer,
                                            VkCommandBufferResetFlags flags) {
  return gpu::GetVulkanFunctionPointers()->vkResetCommandBuffer(commandBuffer,
                                                                flags);
}
ALWAYS_INLINE VkResult vkResetFences(VkDevice device,
                                     uint32_t fenceCount,
                                     const VkFence* pFences) {
  return gpu::GetVulkanFunctionPointers()->vkResetFences(device, fenceCount,
                                                         pFences);
}
ALWAYS_INLINE void vkUnmapMemory(VkDevice device, VkDeviceMemory memory) {
  return gpu::GetVulkanFunctionPointers()->vkUnmapMemory(device, memory);
}
ALWAYS_INLINE void vkUpdateDescriptorSets(
    VkDevice device,
    uint32_t descriptorWriteCount,
    const VkWriteDescriptorSet* pDescriptorWrites,
    uint32_t descriptorCopyCount,
    const VkCopyDescriptorSet* pDescriptorCopies) {
  return gpu::GetVulkanFunctionPointers()->vkUpdateDescriptorSets(
      device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount,
      pDescriptorCopies);
}
ALWAYS_INLINE VkResult vkWaitForFences(VkDevice device,
                                       uint32_t fenceCount,
                                       const VkFence* pFences,
                                       VkBool32 waitAll,
                                       uint64_t timeout) {
  return gpu::GetVulkanFunctionPointers()->vkWaitForFences(
      device, fenceCount, pFences, waitAll, timeout);
}

#if BUILDFLAG(IS_ANDROID)
ALWAYS_INLINE VkResult vkGetAndroidHardwareBufferPropertiesANDROID(
    VkDevice device,
    const struct AHardwareBuffer* buffer,
    VkAndroidHardwareBufferPropertiesANDROID* pProperties) {
  return gpu::GetVulkanFunctionPointers()
      ->vkGetAndroidHardwareBufferPropertiesANDROID(device, buffer,
                                                    pProperties);
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_POSIX)
ALWAYS_INLINE VkResult
vkGetSemaphoreFdKHR(VkDevice device,
                    const VkSemaphoreGetFdInfoKHR* pGetFdInfo,
                    int* pFd) {
  return gpu::GetVulkanFunctionPointers()->vkGetSemaphoreFdKHR(device,
                                                               pGetFdInfo, pFd);
}
ALWAYS_INLINE VkResult vkImportSemaphoreFdKHR(
    VkDevice device,
    const VkImportSemaphoreFdInfoKHR* pImportSemaphoreFdInfo) {
  return gpu::GetVulkanFunctionPointers()->vkImportSemaphoreFdKHR(
      device, pImportSemaphoreFdInfo);
}
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_WIN)
ALWAYS_INLINE VkResult vkGetSemaphoreWin32HandleKHR(
    VkDevice device,
    const VkSemaphoreGetWin32HandleInfoKHR* pGetWin32HandleInfo,
    HANDLE* pHandle) {
  return gpu::GetVulkanFunctionPointers()->vkGetSemaphoreWin32HandleKHR(
      device, pGetWin32HandleInfo, pHandle);
}
ALWAYS_INLINE VkResult
vkImportSemaphoreWin32HandleKHR(VkDevice device,
                                const VkImportSemaphoreWin32HandleInfoKHR*
                                    pImportSemaphoreWin32HandleInfo) {
  return gpu::GetVulkanFunctionPointers()->vkImportSemaphoreWin32HandleKHR(
      device, pImportSemaphoreWin32HandleInfo);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_POSIX)
ALWAYS_INLINE VkResult vkGetMemoryFdKHR(VkDevice device,
                                        const VkMemoryGetFdInfoKHR* pGetFdInfo,
                                        int* pFd) {
  return gpu::GetVulkanFunctionPointers()->vkGetMemoryFdKHR(device, pGetFdInfo,
                                                            pFd);
}
ALWAYS_INLINE VkResult
vkGetMemoryFdPropertiesKHR(VkDevice device,
                           VkExternalMemoryHandleTypeFlagBits handleType,
                           int fd,
                           VkMemoryFdPropertiesKHR* pMemoryFdProperties) {
  return gpu::GetVulkanFunctionPointers()->vkGetMemoryFdPropertiesKHR(
      device, handleType, fd, pMemoryFdProperties);
}
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_WIN)
ALWAYS_INLINE VkResult vkGetMemoryWin32HandleKHR(
    VkDevice device,
    const VkMemoryGetWin32HandleInfoKHR* pGetWin32HandleInfo,
    HANDLE* pHandle) {
  return gpu::GetVulkanFunctionPointers()->vkGetMemoryWin32HandleKHR(
      device, pGetWin32HandleInfo, pHandle);
}
ALWAYS_INLINE VkResult vkGetMemoryWin32HandlePropertiesKHR(
    VkDevice device,
    VkExternalMemoryHandleTypeFlagBits handleType,
    HANDLE handle,
    VkMemoryWin32HandlePropertiesKHR* pMemoryWin32HandleProperties) {
  return gpu::GetVulkanFunctionPointers()->vkGetMemoryWin32HandlePropertiesKHR(
      device, handleType, handle, pMemoryWin32HandleProperties);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_FUCHSIA)
ALWAYS_INLINE VkResult vkImportSemaphoreZirconHandleFUCHSIA(
    VkDevice device,
    const VkImportSemaphoreZirconHandleInfoFUCHSIA*
        pImportSemaphoreZirconHandleInfo) {
  return gpu::GetVulkanFunctionPointers()->vkImportSemaphoreZirconHandleFUCHSIA(
      device, pImportSemaphoreZirconHandleInfo);
}
ALWAYS_INLINE VkResult vkGetSemaphoreZirconHandleFUCHSIA(
    VkDevice device,
    const VkSemaphoreGetZirconHandleInfoFUCHSIA* pGetZirconHandleInfo,
    zx_handle_t* pZirconHandle) {
  return gpu::GetVulkanFunctionPointers()->vkGetSemaphoreZirconHandleFUCHSIA(
      device, pGetZirconHandleInfo, pZirconHandle);
}
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_FUCHSIA)
ALWAYS_INLINE VkResult vkGetMemoryZirconHandleFUCHSIA(
    VkDevice device,
    const VkMemoryGetZirconHandleInfoFUCHSIA* pGetZirconHandleInfo,
    zx_handle_t* pZirconHandle) {
  return gpu::GetVulkanFunctionPointers()->vkGetMemoryZirconHandleFUCHSIA(
      device, pGetZirconHandleInfo, pZirconHandle);
}
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_FUCHSIA)
ALWAYS_INLINE VkResult vkCreateBufferCollectionFUCHSIA(
    VkDevice device,
    const VkBufferCollectionCreateInfoFUCHSIA* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkBufferCollectionFUCHSIA* pCollection) {
  return gpu::GetVulkanFunctionPointers()->vkCreateBufferCollectionFUCHSIA(
      device, pCreateInfo, pAllocator, pCollection);
}
ALWAYS_INLINE VkResult vkSetBufferCollectionImageConstraintsFUCHSIA(
    VkDevice device,
    VkBufferCollectionFUCHSIA collection,
    const VkImageConstraintsInfoFUCHSIA* pImageConstraintsInfo) {
  return gpu::GetVulkanFunctionPointers()
      ->vkSetBufferCollectionImageConstraintsFUCHSIA(device, collection,
                                                     pImageConstraintsInfo);
}
ALWAYS_INLINE VkResult vkGetBufferCollectionPropertiesFUCHSIA(
    VkDevice device,
    VkBufferCollectionFUCHSIA collection,
    VkBufferCollectionPropertiesFUCHSIA* pProperties) {
  return gpu::GetVulkanFunctionPointers()
      ->vkGetBufferCollectionPropertiesFUCHSIA(device, collection, pProperties);
}
ALWAYS_INLINE void vkDestroyBufferCollectionFUCHSIA(
    VkDevice device,
    VkBufferCollectionFUCHSIA collection,
    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyBufferCollectionFUCHSIA(
      device, collection, pAllocator);
}
#endif  // BUILDFLAG(IS_FUCHSIA)

ALWAYS_INLINE VkResult vkAcquireNextImageKHR(VkDevice device,
                                             VkSwapchainKHR swapchain,
                                             uint64_t timeout,
                                             VkSemaphore semaphore,
                                             VkFence fence,
                                             uint32_t* pImageIndex) {
  return gpu::GetVulkanFunctionPointers()->vkAcquireNextImageKHR(
      device, swapchain, timeout, semaphore, fence, pImageIndex);
}
ALWAYS_INLINE VkResult
vkCreateSwapchainKHR(VkDevice device,
                     const VkSwapchainCreateInfoKHR* pCreateInfo,
                     const VkAllocationCallbacks* pAllocator,
                     VkSwapchainKHR* pSwapchain) {
  return gpu::GetVulkanFunctionPointers()->vkCreateSwapchainKHR(
      device, pCreateInfo, pAllocator, pSwapchain);
}
ALWAYS_INLINE void vkDestroySwapchainKHR(
    VkDevice device,
    VkSwapchainKHR swapchain,
    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroySwapchainKHR(
      device, swapchain, pAllocator);
}
ALWAYS_INLINE VkResult vkGetSwapchainImagesKHR(VkDevice device,
                                               VkSwapchainKHR swapchain,
                                               uint32_t* pSwapchainImageCount,
                                               VkImage* pSwapchainImages) {
  return gpu::GetVulkanFunctionPointers()->vkGetSwapchainImagesKHR(
      device, swapchain, pSwapchainImageCount, pSwapchainImages);
}
ALWAYS_INLINE VkResult vkQueuePresentKHR(VkQueue queue,
                                         const VkPresentInfoKHR* pPresentInfo) {
  base::Lock* lock = nullptr;
  auto it = gpu::GetVulkanFunctionPointers()->per_queue_lock_map.find(queue);
  if (it != gpu::GetVulkanFunctionPointers()->per_queue_lock_map.end()) {
    lock = it->second.get();
  }
  base::AutoLockMaybe auto_lock(lock);
  return gpu::GetVulkanFunctionPointers()->vkQueuePresentKHR(queue,
                                                             pPresentInfo);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
ALWAYS_INLINE VkResult vkGetImageDrmFormatModifierPropertiesEXT(
    VkDevice device,
    VkImage image,
    VkImageDrmFormatModifierPropertiesEXT* pProperties) {
  return gpu::GetVulkanFunctionPointers()
      ->vkGetImageDrmFormatModifierPropertiesEXT(device, image, pProperties);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#endif  // GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_