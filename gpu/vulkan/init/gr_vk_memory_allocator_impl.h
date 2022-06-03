// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_INIT_GR_VK_MEMORY_ALLOCATOR_IMPL_H_
#define GPU_VULKAN_INIT_GR_VK_MEMORY_ALLOCATOR_IMPL_H_

#include "base/component_export.h"
#include "third_party/skia/include/gpu/vk/GrVkMemoryAllocator.h"

namespace gpu {

class VulkanDeviceQueue;

COMPONENT_EXPORT(VULKAN_INIT)
sk_sp<GrVkMemoryAllocator> CreateGrVkMemoryAllocator(
    VulkanDeviceQueue* device_queue);

}  // namespace gpu

#endif  // GPU_VULKAN_INIT_GR_VK_MEMORY_ALLOCATOR_IMPL_H_
