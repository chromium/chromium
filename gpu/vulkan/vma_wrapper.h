// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VMA_WRAPPER_H_
#define GPU_VULKAN_VMA_WRAPPER_H_

#include <algorithm>

#include <vulkan/vulkan_core.h>

#include "base/component_export.h"
#include "ui/gfx/extension_set.h"

VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)

struct VmaAllocationCreateInfo;
struct VmaAllocationInfo;
struct VmaBudget;

namespace gpu {
namespace vma {

COMPONENT_EXPORT(VULKAN)
VkResult CreateAllocator(VkPhysicalDevice physical_device,
                         VkDevice device,
                         VkInstance instance,
                         const gfx::ExtensionSet& enabled_extensions,
                         const VkDeviceSize preferred_large_heap_block_size,
                         const VkDeviceSize* heap_size_limit,
                         const bool is_thread_safe,
                         VmaAllocator* allocator);

COMPONENT_EXPORT(VULKAN) void DestroyAllocator(VmaAllocator allocator);

COMPONENT_EXPORT(VULKAN)
VkResult AllocateMemoryForImage(VmaAllocator allocator,
                                VkImage image,
                                const VmaAllocationCreateInfo* create_info,
                                VmaAllocation* allocation,
                                VmaAllocationInfo* allocation_info);

COMPONENT_EXPORT(VULKAN)
VkResult AllocateMemoryForBuffer(VmaAllocator allocator,
                                 VkBuffer buffer,
                                 const VmaAllocationCreateInfo* create_info,
                                 VmaAllocation* allocation,
                                 VmaAllocationInfo* allocationInfo);

COMPONENT_EXPORT(VULKAN)
VkResult CreateBuffer(VmaAllocator allocator,
                      const VkBufferCreateInfo* buffer_create_info,
                      VkMemoryPropertyFlags required_flags,
                      VkMemoryPropertyFlags preferred_flags,
                      VkBuffer* buffer,
                      VmaAllocation* allocation);

COMPONENT_EXPORT(VULKAN)
void DestroyBuffer(VmaAllocator allocator,
                   VkBuffer buffer,
                   VmaAllocation allocation);

COMPONENT_EXPORT(VULKAN)
VkResult MapMemory(VmaAllocator allocator,
                   VmaAllocation allocation,
                   void** data);

COMPONENT_EXPORT(VULKAN)
void UnmapMemory(VmaAllocator allocator, VmaAllocation allocation);

COMPONENT_EXPORT(VULKAN)
void FreeMemory(VmaAllocator allocator, VmaAllocation allocation);

COMPONENT_EXPORT(VULKAN)
VkResult FlushAllocation(VmaAllocator allocator,
                         VmaAllocation allocation,
                         VkDeviceSize offset,
                         VkDeviceSize size);

COMPONENT_EXPORT(VULKAN)
VkResult InvalidateAllocation(VmaAllocator allocator,
                              VmaAllocation allocation,
                              VkDeviceSize offset,
                              VkDeviceSize size);

COMPONENT_EXPORT(VULKAN)
void GetAllocationInfo(VmaAllocator allocator,
                       VmaAllocation allocation,
                       VmaAllocationInfo* allocation_info);

COMPONENT_EXPORT(VULKAN)
void GetMemoryTypeProperties(VmaAllocator allocator,
                             uint32_t memory_type_index,
                             VkMemoryPropertyFlags* flags);

COMPONENT_EXPORT(VULKAN)
void GetPhysicalDeviceProperties(
    VmaAllocator allocator,
    const VkPhysicalDeviceProperties** physical_device_properties);

COMPONENT_EXPORT(VULKAN)
void GetBudget(VmaAllocator allocator, VmaBudget* budget);

// Allocated and used, respectively.
COMPONENT_EXPORT(VULKAN)
std::pair<uint64_t, uint64_t> GetTotalAllocatedAndUsedMemory(
    VmaAllocator allocator);

}  // namespace vma
}  // namespace gpu

#endif  // GPU_VULKAN_VMA_WRAPPER_H_
