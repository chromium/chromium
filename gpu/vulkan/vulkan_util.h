// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines some helper functions for Vulkan API.

#ifndef GPU_VULKAN_VULKAN_UTIL_H_
#define GPU_VULKAN_VULKAN_UTIL_H_

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "gpu/vulkan/semaphore_handle.h"

namespace gpu {

constexpr uint32_t kVendorARM = 0x13b5;
constexpr uint32_t kVendorQualcomm = 0x5143;
constexpr uint32_t kVendorImagination = 0x1010;

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
VKAPI_ATTR VkResult VKAPI_CALL QueueSubmitHook(VkQueue queue,
                                               uint32_t submitCount,
                                               const VkSubmitInfo* pSubmits,
                                               VkFence fence);

COMPONENT_EXPORT(VULKAN)
VKAPI_ATTR VkResult VKAPI_CALL
CreateGraphicsPipelinesHook(VkDevice device,
                            VkPipelineCache pipelineCache,
                            uint32_t createInfoCount,
                            const VkGraphicsPipelineCreateInfo* pCreateInfos,
                            const VkAllocationCallbacks* pAllocator,
                            VkPipeline* pPipelines);

COMPONENT_EXPORT(VULKAN)
VKAPI_ATTR void RecordImportingVKSemaphoreIntoGL();

COMPONENT_EXPORT(VULKAN) void ReportUMAPerSwapBuffers();

COMPONENT_EXPORT(VULKAN)
bool CheckVulkanCompabilities(const VulkanInfo& vulkan_info,
                              const GPUInfo& gpu_info,
                              std::string enable_by_device_name);

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_UTIL_H_
