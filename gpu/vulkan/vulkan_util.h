// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines some helper functions for Vulkan API.

#ifndef GPU_VULKAN_VULKAN_UTIL_H_
#define GPU_VULKAN_VULKAN_UTIL_H_

#include <vulkan/vulkan_core.h>

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "gpu/vulkan/semaphore_handle.h"
#include "gpu/vulkan/vulkan_device_queue.h"

namespace gpu {

constexpr uint32_t kVendorARM = 0x13b5;
constexpr uint32_t kVendorQualcomm = 0x5143;
constexpr uint32_t kVendorImagination = 0x1010;
constexpr uint32_t kVendorGoogle = 0x1AE0;
constexpr uint32_t kDeviceSwiftShader = 0xC0DE;

struct GPUInfo;
class VulkanInfo;

// Submits semaphores to be signaled to the vulkan queue. Semaphores are
// signaled once this submission is executed. vk_fence is an optional handle
// to fence to be signaled once this submission completes execution.
COMPONENT_EXPORT(VULKAN)
bool SubmitSignalVkSemaphores(VkQueue vk_queue,
                              const base::span<VkSemaphore>& vk_semaphore,
                              VkFence vk_fence = VK_NULL_HANDLE);

// Submits a semaphore to be signaled to the vulkan queue. Semaphore is
// signaled once this submission is executed. vk_fence is an optional handle
// to fence to be signaled once this submission completes execution.
COMPONENT_EXPORT(VULKAN)
bool SubmitSignalVkSemaphore(VkQueue vk_queue,
                             VkSemaphore vk_semaphore,
                             VkFence vk_fence = VK_NULL_HANDLE);

// Submits semaphores to be waited upon to the vulkan queue. Semaphores are
// waited on before this submission is executed. vk_fence is an optional
// handle to fence to be signaled once this submission completes execution.
COMPONENT_EXPORT(VULKAN)
bool SubmitWaitVkSemaphores(VkQueue vk_queue,
                            const base::span<VkSemaphore>& vk_semaphores,
                            VkFence vk_fence = VK_NULL_HANDLE);

// Submits a semaphore to be waited upon to the vulkan queue. Semaphore is
// waited on before this submission is executed. vk_fence is an optional
// handle to fence to be signaled once this submission completes execution.
COMPONENT_EXPORT(VULKAN)
bool SubmitWaitVkSemaphore(VkQueue vk_queue,
                           VkSemaphore vk_semaphore,
                           VkFence vk_fence = VK_NULL_HANDLE);

// Creates semaphore that can be exported to external handles of the specified
// |handle_types|.
COMPONENT_EXPORT(VULKAN)
VkSemaphore CreateExternalVkSemaphore(
    VkDevice vk_device,
    VkExternalSemaphoreHandleTypeFlags handle_types);

// Imports a semaphore from a handle.
COMPONENT_EXPORT(VULKAN)
VkSemaphore ImportVkSemaphoreHandle(VkDevice vk_device, SemaphoreHandle handle);

// Gets a handle from a semaphore
COMPONENT_EXPORT(VULKAN)
SemaphoreHandle GetVkSemaphoreHandle(
    VkDevice vk_device,
    VkSemaphore vk_semaphore,
    VkExternalSemaphoreHandleTypeFlagBits handle_type);

COMPONENT_EXPORT(VULKAN)
std::string VkVersionToString(uint32_t version);

COMPONENT_EXPORT(VULKAN)
VKAPI_ATTR VkResult VKAPI_CALL
CreateGraphicsPipelinesHook(VkDevice device,
                            VkPipelineCache pipelineCache,
                            uint32_t createInfoCount,
                            const VkGraphicsPipelineCreateInfo* pCreateInfos,
                            const VkAllocationCallbacks* pAllocator,
                            VkPipeline* pPipelines);

// Below vulkanQueue*Hook methods are used to ensure that Skia calls the correct
// version of those methods which are made thread safe by using locks. See
// vulkan_function_pointers.h vkQueue* method references for more details.
COMPONENT_EXPORT(VULKAN)
VKAPI_ATTR VkResult VKAPI_CALL
VulkanQueueSubmitHook(VkQueue queue,
                      uint32_t submitCount,
                      const VkSubmitInfo* pSubmits,
                      VkFence fence);

COMPONENT_EXPORT(VULKAN)
VKAPI_ATTR VkResult VKAPI_CALL VulkanQueueWaitIdleHook(VkQueue queue);

COMPONENT_EXPORT(VULKAN)
VKAPI_ATTR VkResult VKAPI_CALL
VulkanQueuePresentKHRHook(VkQueue queue, const VkPresentInfoKHR* pPresentInfo);

COMPONENT_EXPORT(VULKAN)
bool CheckVulkanCompabilities(const VulkanInfo& vulkan_info,
                              const GPUInfo& gpu_info,
                              std::string enable_by_device_name);

COMPONENT_EXPORT(VULKAN)
VkImageLayout GLImageLayoutToVkImageLayout(uint32_t layout);

COMPONENT_EXPORT(VULKAN)
uint32_t VkImageLayoutToGLImageLayout(VkImageLayout layout);

COMPONENT_EXPORT(VULKAN)
bool IsVkExternalSemaphoreHandleTypeSupported(
    VulkanDeviceQueue* device_queue,
    VkExternalSemaphoreHandleTypeFlagBits handle_type);

COMPONENT_EXPORT(VULKAN)
VkResult QueryVkExternalMemoryProperties(
    VkPhysicalDevice physical_device,
    VkFormat format,
    VkImageType type,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags,
    VkExternalMemoryHandleTypeFlagBits handle_type,
    VkExternalMemoryProperties* external_memory_properties);

COMPONENT_EXPORT(VULKAN)
bool IsVkOpaqueExternalSemaphoreSupported(VulkanDeviceQueue* device_queue);

COMPONENT_EXPORT(VULKAN)
VkSemaphore CreateVkOpaqueExternalSemaphore(VkDevice vk_device);

COMPONENT_EXPORT(VULKAN)
SemaphoreHandle ExportVkOpaqueExternalSemaphore(VkDevice vk_device,
                                                VkSemaphore vk_semaphore);

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_UTIL_H_
