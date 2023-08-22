// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vma_wrapper.h"

#include <algorithm>

#include <vk_mem_alloc.h>

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {
namespace vma {

VkResult CreateAllocator(VkPhysicalDevice physical_device,
                         VkDevice device,
                         VkInstance instance,
                         const gfx::ExtensionSet& enabled_extensions,
                         const VkDeviceSize preferred_large_heap_block_size,
                         const VkDeviceSize* heap_size_limit,
                         const bool is_thread_safe,
                         VmaAllocator* pAllocator) {
  auto* function_pointers = gpu::GetVulkanFunctionPointers();
  VmaVulkanFunctions functions = {
      function_pointers->vkGetPhysicalDeviceProperties.get(),
      function_pointers->vkGetPhysicalDeviceMemoryProperties.get(),
      function_pointers->vkAllocateMemory.get(),
      function_pointers->vkFreeMemory.get(),
      function_pointers->vkMapMemory.get(),
      function_pointers->vkUnmapMemory.get(),
      function_pointers->vkFlushMappedMemoryRanges.get(),
      function_pointers->vkInvalidateMappedMemoryRanges.get(),
      function_pointers->vkBindBufferMemory.get(),
      function_pointers->vkBindImageMemory.get(),
      function_pointers->vkGetBufferMemoryRequirements.get(),
      function_pointers->vkGetImageMemoryRequirements.get(),
      function_pointers->vkCreateBuffer.get(),
      function_pointers->vkDestroyBuffer.get(),
      function_pointers->vkCreateImage.get(),
      function_pointers->vkDestroyImage.get(),
      function_pointers->vkCmdCopyBuffer.get(),
      function_pointers->vkGetBufferMemoryRequirements2.get(),
      function_pointers->vkGetImageMemoryRequirements2.get(),
      function_pointers->vkBindBufferMemory2.get(),
      function_pointers->vkBindImageMemory2.get(),
      function_pointers->vkGetPhysicalDeviceMemoryProperties2.get(),
  };

  static_assert(kVulkanRequiredApiVersion >= VK_API_VERSION_1_1, "");
  VmaAllocatorCreateInfo allocator_info = {
      .physicalDevice = physical_device,
      .device = device,
      .preferredLargeHeapBlockSize = preferred_large_heap_block_size,
      .pHeapSizeLimit = heap_size_limit,
      .pVulkanFunctions = &functions,
      .instance = instance,
      .vulkanApiVersion = kVulkanRequiredApiVersion,
  };

  // Note that this extension is only requested on android as of now as a part
  // of optional extensions in VulkanImplementation.
  bool vk_ext_memory_budget_supported = gfx::HasExtension(
      enabled_extensions, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);

  // Collect data on how often it is supported.
  base::UmaHistogramBoolean("GPU.Vulkan.ExtMemoryBudgetSupported",
                            vk_ext_memory_budget_supported);

  // Enable VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT flag if extension is
  // available.
  if (vk_ext_memory_budget_supported) {
    allocator_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
  }

  // If DrDc is not enabled, use below flag which improves performance since
  // internal mutex will not be used.
  // TODO(vikassoni) : Analyze the perf impact of not using this flag and hence
  // enabling internal mutex which will be use for every vma access with DrDc.
  if (!is_thread_safe) {
    allocator_info.flags |= VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;
  }
  return vmaCreateAllocator(&allocator_info, pAllocator);
}

void DestroyAllocator(VmaAllocator allocator) {
  vmaDestroyAllocator(allocator);
}

VkResult AllocateMemoryForImage(VmaAllocator allocator,
                                VkImage image,
                                const VmaAllocationCreateInfo* create_info,
                                VmaAllocation* allocation,
                                VmaAllocationInfo* allocation_info) {
  return vmaAllocateMemoryForImage(allocator, image, create_info, allocation,
                                   allocation_info);
}

VkResult AllocateMemoryForBuffer(VmaAllocator allocator,
                                 VkBuffer buffer,
                                 const VmaAllocationCreateInfo* create_info,
                                 VmaAllocation* allocation,
                                 VmaAllocationInfo* allocation_info) {
  return vmaAllocateMemoryForBuffer(allocator, buffer, create_info, allocation,
                                    allocation_info);
}

VkResult CreateBuffer(VmaAllocator allocator,
                      const VkBufferCreateInfo* buffer_create_info,
                      VkMemoryPropertyFlags required_flags,
                      VkMemoryPropertyFlags preferred_flags,
                      VkBuffer* buffer,
                      VmaAllocation* allocation) {
  VmaAllocationCreateInfo allocation_create_info = {
      .requiredFlags = required_flags,
      .preferredFlags = preferred_flags,
  };

  return vmaCreateBuffer(allocator, buffer_create_info, &allocation_create_info,
                         buffer, allocation, nullptr);
}

void DestroyBuffer(VmaAllocator allocator,
                   VkBuffer buffer,
                   VmaAllocation allocation) {
  vmaDestroyBuffer(allocator, buffer, allocation);
}

VkResult MapMemory(VmaAllocator allocator,
                   VmaAllocation allocation,
                   void** data) {
  return vmaMapMemory(allocator, allocation, data);
}

void UnmapMemory(VmaAllocator allocator, VmaAllocation allocation) {
  return vmaUnmapMemory(allocator, allocation);
}

void FreeMemory(VmaAllocator allocator, VmaAllocation allocation) {
  vmaFreeMemory(allocator, allocation);
}

VkResult FlushAllocation(VmaAllocator allocator,
                         VmaAllocation allocation,
                         VkDeviceSize offset,
                         VkDeviceSize size) {
  return vmaFlushAllocation(allocator, allocation, offset, size);
}

VkResult InvalidateAllocation(VmaAllocator allocator,
                              VmaAllocation allocation,
                              VkDeviceSize offset,
                              VkDeviceSize size) {
  return vmaInvalidateAllocation(allocator, allocation, offset, size);
}

void GetAllocationInfo(VmaAllocator allocator,
                       VmaAllocation allocation,
                       VmaAllocationInfo* allocation_info) {
  vmaGetAllocationInfo(allocator, allocation, allocation_info);
}

void GetMemoryTypeProperties(VmaAllocator allocator,
                             uint32_t memory_type_index,
                             VkMemoryPropertyFlags* flags) {
  vmaGetMemoryTypeProperties(allocator, memory_type_index, flags);
}

void GetPhysicalDeviceProperties(
    VmaAllocator allocator,
    const VkPhysicalDeviceProperties** physical_device_properties) {
  vmaGetPhysicalDeviceProperties(allocator, physical_device_properties);
}

void GetBudget(VmaAllocator allocator, VmaBudget* budget) {
  vmaGetBudget(allocator, budget);
}

std::pair<uint64_t, uint64_t> GetTotalAllocatedAndUsedMemory(
    VmaAllocator allocator) {
  // See GrVkMemoryAllocatorImpl::totalAllocatedAndUsedMemory() in skia for
  // reference.
  VmaBudget budget[VK_MAX_MEMORY_HEAPS];
  vmaGetBudget(allocator, budget);
  const VkPhysicalDeviceMemoryProperties* pPhysicalDeviceMemoryProperties;
  vmaGetMemoryProperties(allocator, &pPhysicalDeviceMemoryProperties);
  uint64_t total_allocated_memory = 0, total_used_memory = 0;
  for (uint32_t i = 0; i < pPhysicalDeviceMemoryProperties->memoryHeapCount;
       ++i) {
    total_allocated_memory += budget[i].blockBytes;
    total_used_memory += budget[i].allocationBytes;
  }
  DCHECK_LE(total_used_memory, total_allocated_memory);

  return {total_allocated_memory, total_used_memory};
}

}  // namespace vma
}  // namespace gpu
