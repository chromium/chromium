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

#include "base/containers/span.h"
#include "gpu/vulkan/vulkan_export.h"

namespace gpu {

// Submits semaphores to be signaled to the vulkan queue. Semaphores are
// signaled once this submission is executed. vk_fence is an optional handle
// to fence to be signaled once this submission completes execution.
VULKAN_EXPORT bool SubmitSignalVkSemaphores(
    VkQueue vk_queue,
    const base::span<VkSemaphore>& vk_semaphore,
    VkFence vk_fence = VK_NULL_HANDLE);

// Submits a semaphore to be signaled to the vulkan queue. Semaphore is
// signaled once this submission is executed. vk_fence is an optional handle
// to fence to be signaled once this submission completes execution.
VULKAN_EXPORT bool SubmitSignalVkSemaphore(VkQueue vk_queue,
                                           VkSemaphore vk_semaphore,
                                           VkFence vk_fence = VK_NULL_HANDLE);

// Submits semaphores to be waited upon to the vulkan queue. Semaphores are
// waited on before this submission is executed. vk_fence is an optional
// handle to fence to be signaled once this submission completes execution.
VULKAN_EXPORT bool SubmitWaitVkSemaphores(
    VkQueue vk_queue,
    const base::span<VkSemaphore>& vk_semaphores,
    VkFence vk_fence = VK_NULL_HANDLE);

// Submits a semaphore to be waited upon to the vulkan queue. Semaphore is
// waited on before this submission is executed. vk_fence is an optional
// handle to fence to be signaled once this submission completes execution.
VULKAN_EXPORT bool SubmitWaitVkSemaphore(VkQueue vk_queue,
                                         VkSemaphore vk_semaphore,
                                         VkFence vk_fence = VK_NULL_HANDLE);

// Creates semaphore that can be exported to external handles of the specified
// |handle_types|.
VULKAN_EXPORT VkSemaphore
CreateExternalVkSemaphore(VkDevice vk_device,
                          VkExternalSemaphoreHandleTypeFlags handle_types);

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_UTIL_H_
