// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/skia_vk_memory_allocator_impl.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <array>

#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {

SkiaVulkanMemoryAllocator::SkiaVulkanMemoryAllocator(VmaAllocator allocator)
    : allocator_(allocator) {}

VkResult SkiaVulkanMemoryAllocator::allocateImageMemory(
    VkImage image,
    uint32_t flags,
    skgpu::VulkanBackendMemory* backend_memory) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
               "SkiaVulkanMemoryAllocator::allocateMemoryForImage");
  VmaAllocationCreateInfo info = {
      .flags = 0,
      .usage = VMA_MEMORY_USAGE_UNKNOWN,
      .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      .preferredFlags = 0,
      .memoryTypeBits = 0,
      .pool = VK_NULL_HANDLE,
      .pUserData = nullptr,
  };

  if (kDedicatedAllocation_AllocationPropertyFlag & flags) {
    info.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  }

  if (kLazyAllocation_AllocationPropertyFlag & flags) {
    // If the caller asked for lazy allocation then they already set up the
    // VkImage for it so we must require the lazy property.
    info.requiredFlags |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    // Make the transient allocation a dedicated allocation for tracking
    // memory separately.
    info.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  }

  if (kProtected_AllocationPropertyFlag & flags) {
    info.requiredFlags |= VK_MEMORY_PROPERTY_PROTECTED_BIT;
  }

  VmaAllocation allocation;
  VkResult result = vma::AllocateMemoryForImage(allocator_, image, &info,
                                                &allocation, nullptr);
  if (VK_SUCCESS == result) {
    if (info.requiredFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) {
      VmaAllocationInfo vma_info;
      vma::GetAllocationInfo(allocator_, allocation, &vma_info);
      lazy_allocated_size_ += vma_info.size;
    }
    *backend_memory = reinterpret_cast<skgpu::VulkanBackendMemory>(allocation);
  }

  return result;
}

VkResult SkiaVulkanMemoryAllocator::allocateBufferMemory(
    VkBuffer buffer,
    BufferUsage usage,
    uint32_t flags,
    skgpu::VulkanBackendMemory* backend_memory) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
               "SkiaVulkanMemoryAllocator::allocateMemoryForBuffer");
  VmaAllocationCreateInfo info = {
      .flags = 0,
      .usage = VMA_MEMORY_USAGE_UNKNOWN,
      .memoryTypeBits = 0,
      .pool = VK_NULL_HANDLE,
      .pUserData = nullptr,
  };

  switch (usage) {
    case BufferUsage::kGpuOnly:
      info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      info.preferredFlags = 0;
      break;
    case BufferUsage::kCpuWritesGpuReads:
      info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
      info.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      break;
    case BufferUsage::kTransfersFromCpuToGpu:
      info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
      info.preferredFlags = 0;
      break;
    case BufferUsage::kTransfersFromGpuToCpu:
      info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
      info.preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
      break;
  }

  if (kDedicatedAllocation_AllocationPropertyFlag & flags) {
    info.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  }

  if ((kLazyAllocation_AllocationPropertyFlag & flags) &&
      BufferUsage::kGpuOnly == usage) {
    info.preferredFlags |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    // Make the transient allocation a dedicated allocation for tracking
    // memory separately.
    info.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  }

  if (kPersistentlyMapped_AllocationPropertyFlag & flags) {
    SkASSERT(BufferUsage::kGpuOnly != usage);
    info.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
  }

  VmaAllocation allocation;
  VkResult result = vma::AllocateMemoryForBuffer(allocator_, buffer, &info,
                                                 &allocation, nullptr);
  if (VK_SUCCESS == result) {
    if (info.preferredFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) {
      VmaAllocationInfo vma_info;
      vma::GetAllocationInfo(allocator_, allocation, &vma_info);
      lazy_allocated_size_ += vma_info.size;
    }
    *backend_memory = reinterpret_cast<skgpu::VulkanBackendMemory>(allocation);
  }

  return result;
}

void SkiaVulkanMemoryAllocator::freeMemory(
    const skgpu::VulkanBackendMemory& memory) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
               "SkiaVulkanMemoryAllocator::freeMemory");
  VmaAllocation allocation = reinterpret_cast<const VmaAllocation>(memory);

  // Update `lazy_allocated_size_` tracking.
  VmaAllocationInfo vma_info;
  vma::GetAllocationInfo(allocator_, allocation, &vma_info);
  VkMemoryPropertyFlags mem_flags;
  vma::GetMemoryTypeProperties(allocator_, vma_info.memoryType, &mem_flags);
  if (mem_flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) {
    lazy_allocated_size_ -= vma_info.size;
  }

  vma::FreeMemory(allocator_, allocation);
}

void SkiaVulkanMemoryAllocator::getAllocInfo(
    const skgpu::VulkanBackendMemory& memory,
    skgpu::VulkanAlloc* alloc) const {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
               "SkiaVulkanMemoryAllocator::getAllocInfo");
  const VmaAllocation allocation =
      reinterpret_cast<const VmaAllocation>(memory);
  VmaAllocationInfo vma_info;
  vma::GetAllocationInfo(allocator_, allocation, &vma_info);

  VkMemoryPropertyFlags mem_flags;
  vma::GetMemoryTypeProperties(allocator_, vma_info.memoryType, &mem_flags);

  uint32_t flags = 0;
  if (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT & mem_flags) {
    flags |= skgpu::VulkanAlloc::kMappable_Flag;
  }
  if (!(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT & mem_flags)) {
    flags |= skgpu::VulkanAlloc::kNoncoherent_Flag;
  }
  if (VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT & mem_flags) {
    flags |= skgpu::VulkanAlloc::kLazilyAllocated_Flag;
  }

  alloc->fMemory = vma_info.deviceMemory;
  alloc->fOffset = vma_info.offset;
  alloc->fSize = vma_info.size;
  alloc->fFlags = flags;
  alloc->fBackendMemory = memory;
}

VkResult SkiaVulkanMemoryAllocator::mapMemory(
    const skgpu::VulkanBackendMemory& memory,
    void** data) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
               "SkiaVulkanMemoryAllocator::mapMemory");
  const VmaAllocation allocation =
      reinterpret_cast<const VmaAllocation>(memory);
  return vma::MapMemory(allocator_, allocation, data);
}

void SkiaVulkanMemoryAllocator::unmapMemory(
    const skgpu::VulkanBackendMemory& memory) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
               "SkiaVulkanMemoryAllocator::unmapMemory");
  const VmaAllocation allocation =
      reinterpret_cast<const VmaAllocation>(memory);
  vma::UnmapMemory(allocator_, allocation);
}

VkResult SkiaVulkanMemoryAllocator::flushMemory(
    const skgpu::VulkanBackendMemory& memory,
    VkDeviceSize offset,
    VkDeviceSize size) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
               "SkiaVulkanMemoryAllocator::flushMappedMemory");
  const VmaAllocation allocation =
      reinterpret_cast<const VmaAllocation>(memory);
  return vma::FlushAllocation(allocator_, allocation, offset, size);
}

VkResult SkiaVulkanMemoryAllocator::invalidateMemory(
    const skgpu::VulkanBackendMemory& memory,
    VkDeviceSize offset,
    VkDeviceSize size) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
               "SkiaVulkanMemoryAllocator::invalidateMappedMemory");
  const VmaAllocation allocation =
      reinterpret_cast<const VmaAllocation>(memory);
  return vma::InvalidateAllocation(allocator_, allocation, offset, size);
}

std::pair<uint64_t, uint64_t>
SkiaVulkanMemoryAllocator::totalAllocatedAndUsedMemory() const {
  uint64_t total_allocated_memory = 0, total_used_memory = 0;
  std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> budget;
  vma::GetBudget(allocator_, budget.data());
  const VkPhysicalDeviceMemoryProperties* physical_device_memory_properties;
  vmaGetMemoryProperties(allocator_, &physical_device_memory_properties);
  for (uint32_t i = 0; i < physical_device_memory_properties->memoryHeapCount;
       ++i) {
    total_allocated_memory += budget[i].statistics.blockBytes;
    total_used_memory += budget[i].statistics.allocationBytes;
  }
  DCHECK_LE(total_used_memory, total_allocated_memory);
  return {total_allocated_memory, total_used_memory};
}

}  // namespace gpu
