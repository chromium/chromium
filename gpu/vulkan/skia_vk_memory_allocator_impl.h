// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_SKIA_VK_MEMORY_ALLOCATOR_IMPL_H_
#define GPU_VULKAN_SKIA_VK_MEMORY_ALLOCATOR_IMPL_H_

#include "base/component_export.h"
#include "gpu/vulkan/vma_wrapper.h"
#include "third_party/skia/include/gpu/vk/VulkanMemoryAllocator.h"
#include "third_party/skia/include/gpu/vk/VulkanTypes.h"

namespace gpu {

class COMPONENT_EXPORT(VULKAN) SkiaVulkanMemoryAllocator
    : public skgpu::VulkanMemoryAllocator {
 public:
  explicit SkiaVulkanMemoryAllocator(VmaAllocator allocator);
  ~SkiaVulkanMemoryAllocator() override = default;

  SkiaVulkanMemoryAllocator(const SkiaVulkanMemoryAllocator&) = delete;
  SkiaVulkanMemoryAllocator& operator=(const SkiaVulkanMemoryAllocator&) =
      delete;

  uint64_t totalLazyAllocatedMemory() const { return lazy_allocated_size_; }

 private:
  // skgpu::VulkanMemoryAllocator:
  VkResult allocateImageMemory(
      VkImage image,
      uint32_t flags,
      skgpu::VulkanBackendMemory* backend_memory) override;
  VkResult allocateBufferMemory(
      VkBuffer buffer,
      BufferUsage usage,
      uint32_t flags,
      skgpu::VulkanBackendMemory* backend_memory) override;
  void freeMemory(const skgpu::VulkanBackendMemory& memory) override;

  void getAllocInfo(const skgpu::VulkanBackendMemory& memory,
                    skgpu::VulkanAlloc* alloc) const override;

  VkResult mapMemory(const skgpu::VulkanBackendMemory& memory,
                     void** data) override;
  void unmapMemory(const skgpu::VulkanBackendMemory& memory) override;

  VkResult flushMemory(const skgpu::VulkanBackendMemory& memory,
                       VkDeviceSize offset,
                       VkDeviceSize size) override;
  VkResult invalidateMemory(const skgpu::VulkanBackendMemory& memory,
                            VkDeviceSize offset,
                            VkDeviceSize size) override;

  std::pair<uint64_t, uint64_t> totalAllocatedAndUsedMemory() const override;

  const VmaAllocator allocator_;

  // Tracks vulkan memory that has lazily allocated flag.
  VkDeviceSize lazy_allocated_size_ = 0;
};

}  // namespace gpu

#endif  // GPU_VULKAN_SKIA_VK_MEMORY_ALLOCATOR_IMPL_H_
