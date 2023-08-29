// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/init/gr_vk_memory_allocator_impl.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "gpu/vulkan/vma_wrapper.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/gpu/vk/GrVkMemoryAllocator.h"
#include "third_party/skia/include/gpu/vk/GrVkTypes.h"

namespace gpu {

namespace {

class GrVkMemoryAllocatorImpl : public GrVkMemoryAllocator {
 public:
  explicit GrVkMemoryAllocatorImpl(VmaAllocator allocator)
      : allocator_(allocator) {}
  ~GrVkMemoryAllocatorImpl() override = default;

  GrVkMemoryAllocatorImpl(const GrVkMemoryAllocatorImpl&) = delete;
  GrVkMemoryAllocatorImpl& operator=(const GrVkMemoryAllocatorImpl&) = delete;

 private:
  VkResult allocateImageMemory(VkImage image,
                               uint32_t flags,
                               GrVkBackendMemory* backend_memory) override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
                 "GrVkMemoryAllocatorImpl::allocateMemoryForImage");
    VmaAllocationCreateInfo info;
    info.flags = 0;
    info.usage = VMA_MEMORY_USAGE_UNKNOWN;
    info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    info.preferredFlags = 0;
    info.memoryTypeBits = 0;
    info.pool = VK_NULL_HANDLE;
    info.pUserData = nullptr;

    if (kDedicatedAllocation_AllocationPropertyFlag & flags) {
      info.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    if (kLazyAllocation_AllocationPropertyFlag & flags) {
      // If the caller asked for lazy allocation then they already set up the
      // VkImage for it so we must require the lazy property.
      info.requiredFlags |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    }

    if (kProtected_AllocationPropertyFlag & flags) {
      info.requiredFlags |= VK_MEMORY_PROPERTY_PROTECTED_BIT;
    }

    VmaAllocation allocation;
    VkResult result = vma::AllocateMemoryForImage(allocator_, image, &info,
                                                  &allocation, nullptr);
    if (VK_SUCCESS == result)
      *backend_memory = reinterpret_cast<GrVkBackendMemory>(allocation);
    return result;
  }

  VkResult allocateBufferMemory(VkBuffer buffer,
                                BufferUsage usage,
                                uint32_t flags,
                                GrVkBackendMemory* backend_memory) override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
                 "GrVkMemoryAllocatorImpl::allocateMemoryForBuffer");
    VmaAllocationCreateInfo info;
    info.flags = 0;
    info.usage = VMA_MEMORY_USAGE_UNKNOWN;
    info.memoryTypeBits = 0;
    info.pool = VK_NULL_HANDLE;
    info.pUserData = nullptr;

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
    }

    if (kPersistentlyMapped_AllocationPropertyFlag & flags) {
      SkASSERT(BufferUsage::kGpuOnly != usage);
      info.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    VmaAllocation allocation;
    VkResult result = vma::AllocateMemoryForBuffer(allocator_, buffer, &info,
                                                   &allocation, nullptr);
    if (VK_SUCCESS == result)
      *backend_memory = reinterpret_cast<GrVkBackendMemory>(allocation);

    return result;
  }

  void freeMemory(const GrVkBackendMemory& memory) override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
                 "GrVkMemoryAllocatorImpl::freeMemory");
    vma::FreeMemory(allocator_, reinterpret_cast<const VmaAllocation>(memory));
  }

  void getAllocInfo(const GrVkBackendMemory& memory,
                    GrVkAlloc* alloc) const override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
                 "GrVkMemoryAllocatorImpl::getAllocInfo");
    const VmaAllocation allocation =
        reinterpret_cast<const VmaAllocation>(memory);
    VmaAllocationInfo vma_info;
    vma::GetAllocationInfo(allocator_, allocation, &vma_info);

    VkMemoryPropertyFlags mem_flags;
    vma::GetMemoryTypeProperties(allocator_, vma_info.memoryType, &mem_flags);

    uint32_t flags = 0;
    if (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT & mem_flags) {
      flags |= GrVkAlloc::kMappable_Flag;
    }
    if (!(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT & mem_flags)) {
      flags |= GrVkAlloc::kNoncoherent_Flag;
    }
    if (VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT & mem_flags) {
      flags |= GrVkAlloc::kLazilyAllocated_Flag;
    }

    alloc->fMemory = vma_info.deviceMemory;
    alloc->fOffset = vma_info.offset;
    alloc->fSize = vma_info.size;
    alloc->fFlags = flags;
    alloc->fBackendMemory = memory;
  }

  VkResult mapMemory(const GrVkBackendMemory& memory, void** data) override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
                 "GrVkMemoryAllocatorImpl::mapMemory");
    const VmaAllocation allocation =
        reinterpret_cast<const VmaAllocation>(memory);
    return vma::MapMemory(allocator_, allocation, data);
  }

  void unmapMemory(const GrVkBackendMemory& memory) override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
                 "GrVkMemoryAllocatorImpl::unmapMemory");
    const VmaAllocation allocation =
        reinterpret_cast<const VmaAllocation>(memory);
    vma::UnmapMemory(allocator_, allocation);
  }

  VkResult flushMemory(const GrVkBackendMemory& memory,
                       VkDeviceSize offset,
                       VkDeviceSize size) override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
                 "GrVkMemoryAllocatorImpl::flushMappedMemory");
    const VmaAllocation allocation =
        reinterpret_cast<const VmaAllocation>(memory);
    return vma::FlushAllocation(allocator_, allocation, offset, size);
  }

  VkResult invalidateMemory(const GrVkBackendMemory& memory,
                            VkDeviceSize offset,
                            VkDeviceSize size) override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
                 "GrVkMemoryAllocatorImpl::invalidateMappedMemory");
    const VmaAllocation allocation =
        reinterpret_cast<const VmaAllocation>(memory);
    return vma::InvalidateAllocation(allocator_, allocation, offset, size);
  }

  std::pair<uint64_t, uint64_t> totalAllocatedAndUsedMemory() const override {
    uint64_t total_allocated_memory = 0, total_used_memory = 0;
    VmaBudget budget[VK_MAX_MEMORY_HEAPS];
    vma::GetBudget(allocator_, budget);
    const VkPhysicalDeviceMemoryProperties* physical_device_memory_properties;
    vmaGetMemoryProperties(allocator_, &physical_device_memory_properties);
    for (uint32_t i = 0; i < physical_device_memory_properties->memoryHeapCount;
         ++i) {
      total_allocated_memory += budget[i].blockBytes;
      total_used_memory += budget[i].allocationBytes;
    }
    DCHECK_LE(total_used_memory, total_allocated_memory);
    return {total_allocated_memory, total_used_memory};
  }

  const VmaAllocator allocator_;
};

}  // namespace

sk_sp<GrVkMemoryAllocator> CreateGrVkMemoryAllocator(
    VulkanDeviceQueue* device_queue) {
  return sk_make_sp<GrVkMemoryAllocatorImpl>(device_queue->vma_allocator());
}

}  // namespace gpu
