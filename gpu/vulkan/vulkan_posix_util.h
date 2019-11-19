// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains helpers used by VulkanImplementation's on POSIX platforms
// to import/export Vulkan objects to POSIX file descriptors. They should not
// be used directly except in VulkanImplementation children.

#ifndef GPU_VULKAN_VULKAN_POSIX_UTIL_H_
#define GPU_VULKAN_VULKAN_POSIX_UTIL_H_

#include <vulkan/vulkan.h>

#include "gpu/vulkan/semaphore_handle.h"
#include "gpu/vulkan/vulkan_export.h"

namespace gpu {

VULKAN_EXPORT VkSemaphore ImportVkSemaphoreHandlePosix(VkDevice vk_device,
                                                       SemaphoreHandle handle);
VULKAN_EXPORT SemaphoreHandle
GetVkSemaphoreHandlePosix(VkDevice vk_device,
                          VkSemaphore vk_semaphore,
                          VkExternalSemaphoreHandleTypeFlagBits handle_type);

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_POSIX_UTIL_H_