// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
  VmaVulkanFunctions functions = {};
  functions.vkGetPhysicalDeviceProperties =
      function_pointers->vkGetPhysicalDeviceProperties.get();
  functions.vkGetPhysicalDeviceMemoryProperties =
      function_pointers->vkGetPhysicalDeviceMemoryProperties.get();
  functions.vkAllocateMemory = function_pointers->vkAllocateMemory.get();
  functions.vkFreeMemory = function_pointers->vkFreeMemory.get();
  functions.vkMapMemory = function_pointers->vkMapMemory.get();
  functions.vkUnmapMemory = function_pointers->vkUnmapMemory.get();
  functions.vkFlushMappedMemoryRanges =
      function_pointers->vkFlushMappedMemoryRanges.get();
  functions.vkInvalidateMappedMemoryRanges =
      function_pointers->vkInvalidateMappedMemoryRanges.get();
  functions.vkBindBufferMemory = function_pointers->vkBindBufferMemory.get();
  functions.vkBindImageMemory = function_pointers->vkBindImageMemory.get();
  functions.vkGetBufferMemoryRequirements =
      function_pointers->vkGetBufferMemoryRequirements.get();
  functions.vkGetImageMemoryRequirements =
      function_pointers->vkGetImageMemoryRequirements.get();
  functions.vkCreateBuffer = function_pointers->vkCreateBuffer.get();
  functions.vkDestroyBuffer = function_pointers->vkDestroyBuffer.get();
  functions.vkCreateImage = function_pointers->vkCreateImage.get();
  functions.vkDestroyImage = function_pointers->vkDestroyImage.get();
  functions.vkCmdCopyBuffer = function_pointers->vkCmdCopyBuffer.get();
  functions.vkGetBufferMemoryRequirements2KHR =
      function_pointers->vkGetBufferMemoryRequirements2.get();
  functions.vkGetImageMemoryRequirements2KHR =
      function_pointers->vkGetImageMemoryRequirements2.get();
  functions.vkBindBufferMemory2KHR =
      function_pointers->vkBindBufferMemory2.get();
  functions.vkBindImageMemory2KHR = function_pointers->vkBindImageMemory2.get();
  functions.vkGetPhysicalDeviceMemoryProperties2KHR =
      function_pointers->vkGetPhysicalDeviceMemoryProperties2.get();

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
  vmaGetHeapBudgets(allocator, budget);
}

std::pair<uint64_t, uint64_t> GetTotalAllocatedAndUsedMemory(
    VmaAllocator allocator) {
  // See VulkanAMDMemoryAllocator::totalAllocatedAndUsedMemory() in skia for
  // reference.
  VmaBudget budget[VK_MAX_MEMORY_HEAPS];
  GetBudget(allocator, budget);
  const VkPhysicalDeviceMemoryProperties* pPhysicalDeviceMemoryProperties;
  vmaGetMemoryProperties(allocator, &pPhysicalDeviceMemoryProperties);
  uint64_t total_allocated_memory = 0, total_used_memory = 0;
  for (uint32_t i = 0; i < pPhysicalDeviceMemoryProperties->memoryHeapCount;
       ++i) {
    total_allocated_memory += budget[i].statistics.blockBytes;
    total_used_memory += budget[i].statistics.allocationBytes;
  }
  DCHECK_LE(total_used_memory, total_allocated_memory);

  return {total_allocated_memory, total_used_memory};
}

}  // namespace vma
}  // namespace gpu
