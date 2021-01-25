// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_overlay_representation.h"

#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_implementation.h"

namespace gpu {

namespace {

// Moves platform specific SemaphoreHandle instances to gfx::GpuFenceHandle.
gfx::GpuFenceHandle SemaphoreHandleToGpuFenceHandle(SemaphoreHandle handle) {
  gfx::GpuFenceHandle fence_handle;
#if defined(OS_FUCHSIA)
  // Fuchsia's Vulkan driver allows zx::event to be obtained from a
  // VkSemaphore, which can then be used to submit present work, see
  // https://fuchsia.dev/reference/fidl/fuchsia.ui.scenic.
  fence_handle.owned_event = handle.TakeHandle();
#elif defined(OS_POSIX)
  fence_handle.owned_fd = handle.TakeHandle();
#elif defined(OS_WIN)
  fence_handle.owned_handle = handle.TakeHandle();
#endif  // defined(OS_FUCHSIA)
  return fence_handle;
}

}  // namespace

ExternalVkImageOverlayRepresentation::ExternalVkImageOverlayRepresentation(
    SharedImageManager* manager,
    ExternalVkImageBacking* backing,
    MemoryTypeTracker* tracker)
    : gpu::SharedImageRepresentationOverlay(manager, backing, tracker),
      vk_image_backing_(backing) {}

ExternalVkImageOverlayRepresentation::~ExternalVkImageOverlayRepresentation() =
    default;

bool ExternalVkImageOverlayRepresentation::BeginReadAccess(
    std::vector<gfx::GpuFence>* acquire_fences,
    std::vector<gfx::GpuFence>* release_fences) {
  DCHECK(read_begin_semaphores_.empty());
  if (!vk_image_backing_->BeginAccess(/*readonly=*/true,
                                      &read_begin_semaphores_,
                                      /*is_gl=*/false)) {
    return false;
  }

  // Create a |read_end_semaphore_| which will be signalled by the display.
  read_end_semaphore_ =
      vk_image_backing_->external_semaphore_pool()->GetOrCreateSemaphore();

  GetAcquireFences(acquire_fences);
  GetReleaseFences(release_fences);
  return true;
}

void ExternalVkImageOverlayRepresentation::EndReadAccess() {
  DCHECK(read_end_semaphore_);
  vk_image_backing_->EndAccess(/*readonly=*/true,
                               std::move(read_end_semaphore_),
                               /*is_gl=*/false);

  // All pending semaphores have been waited on directly or indirectly. They can
  // be reused when the next submitted GPU work is done by GPU.
  vk_image_backing_->ReturnPendingSemaphoresWithFenceHelper(
      std::move(read_begin_semaphores_));
  read_begin_semaphores_.clear();
}

gl::GLImage* ExternalVkImageOverlayRepresentation::GetGLImage() {
  NOTREACHED();
  return nullptr;
}

#if defined(OS_ANDROID)
void ExternalVkImageOverlayRepresentation::NotifyOverlayPromotion(
    bool promotion,
    const gfx::Rect& bounds) {
  NOTREACHED();
}
#endif

void ExternalVkImageOverlayRepresentation::GetAcquireFences(
    std::vector<gfx::GpuFence>* fences) {
  const VkDevice& device = vk_image_backing_->context_provider()
                               ->GetDeviceQueue()
                               ->GetVulkanDevice();
  for (auto& semaphore : read_begin_semaphores_) {
    DCHECK(semaphore.is_valid());
    fences->emplace_back(SemaphoreHandleToGpuFenceHandle(
        vk_image_backing_->vulkan_implementation()->GetSemaphoreHandle(
            device, semaphore.GetVkSemaphore())));
  }
}

void ExternalVkImageOverlayRepresentation::GetReleaseFences(
    std::vector<gfx::GpuFence>* fences) {
  DCHECK(read_end_semaphore_.is_valid());
  const VkDevice& device = vk_image_backing_->context_provider()
                               ->GetDeviceQueue()
                               ->GetVulkanDevice();
  fences->emplace_back(SemaphoreHandleToGpuFenceHandle(
      vk_image_backing_->vulkan_implementation()->GetSemaphoreHandle(
          device, read_end_semaphore_.GetVkSemaphore())));
}

}  // namespace gpu
