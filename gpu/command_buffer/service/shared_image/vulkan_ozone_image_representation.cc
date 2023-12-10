// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/vulkan_ozone_image_representation.h"

#if BUILDFLAG(ENABLE_VULKAN)

#include "gpu/command_buffer/service/shared_image/ozone_image_backing.h"

namespace gpu {

VulkanOzoneImageRepresentation::VulkanOzoneImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    std::unique_ptr<gpu::VulkanImage> vulkan_image,
    gpu::VulkanDeviceQueue* vulkan_device_queue,
    gpu::VulkanImplementation& vulkan_impl)
    : VulkanImageRepresentation(manager,
                                backing,
                                tracker,
                                std::move(vulkan_image),
                                vulkan_device_queue,
                                vulkan_impl) {}

VulkanOzoneImageRepresentation::~VulkanOzoneImageRepresentation() = default;

std::unique_ptr<VulkanImageRepresentation::ScopedAccess>
VulkanOzoneImageRepresentation::BeginScopedAccess(
    AccessMode access_mode,
    std::vector<VkSemaphore>& begin_semaphores,
    std::vector<VkSemaphore>& end_semaphores) {
  std::vector<gfx::GpuFenceHandle> fences;
  bool need_end_fence;
  if (!ozone_backing()->BeginAccess(access_mode == AccessMode::kRead,
                                    OzoneImageBacking::AccessStream::kVulkan,
                                    &fences, need_end_fence)) {
    return nullptr;
  }

  VkSemaphore end_semaphore = VK_NULL_HANDLE;
  if (need_end_fence) {
    end_semaphore = vulkan_impl_->CreateExternalSemaphore(
        vulkan_device_queue_->GetVulkanDevice());
    end_semaphores.emplace_back(end_semaphore);
  }

  for (auto& fence : fences) {
    begin_semaphores.emplace_back(vulkan_impl_->ImportSemaphoreHandle(
        vulkan_device_queue_->GetVulkanDevice(),
        SemaphoreHandle(std::move(fence))));
  }

  return std::make_unique<VulkanImageRepresentation::ScopedAccess>(
      this, access_mode, begin_semaphores, end_semaphore);
}

void VulkanOzoneImageRepresentation::EndScopedAccess(
    bool is_read_only,
    VkSemaphore end_semaphore) {
  if (end_semaphore != VK_NULL_HANDLE) {
    ozone_backing()->EndAccess(
        is_read_only, OzoneImageBacking::AccessStream::kVulkan,
        std::move(vulkan_impl_->GetSemaphoreHandle(
                      vulkan_device_queue_->GetVulkanDevice(), end_semaphore))
            .ToGpuFenceHandle());
  }
}

}  // namespace gpu

#endif
