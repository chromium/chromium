// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_overlay_representation.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_FUCHSIA)
#include "gpu/vulkan/fuchsia/vulkan_fuchsia_ext.h"
#endif

#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_implementation.h"

namespace gpu {

ExternalVkImageOverlayRepresentation::ExternalVkImageOverlayRepresentation(
    SharedImageManager* manager,
    ExternalVkImageBacking* backing,
    MemoryTypeTracker* tracker)
    : gpu::SharedImageRepresentationOverlay(manager, backing, tracker),
      vk_image_backing_(backing) {}

ExternalVkImageOverlayRepresentation::~ExternalVkImageOverlayRepresentation() =
    default;

bool ExternalVkImageOverlayRepresentation::BeginReadAccess(
    std::vector<gfx::GpuFence>* acquire_fences) {
  DCHECK(read_begin_semaphores_.empty());
  if (!vk_image_backing_->BeginAccess(/*readonly=*/true,
                                      &read_begin_semaphores_,
                                      /*is_gl=*/false)) {
    return false;
  }

  GetAcquireFences(acquire_fences);
  return true;
}

void ExternalVkImageOverlayRepresentation::EndReadAccess(
    gfx::GpuFenceHandle release_fence) {
  ExternalSemaphore read_end_semaphore;

  // Not every window manager provides us with release fence or they are not
  // importable to Vulkan. On these systems it's safe to access the image after
  // EndReadAccess without waiting for the fence.
  if (!release_fence.is_null()) {
    read_end_semaphore = ExternalSemaphore::CreateFromHandle(
        vk_image_backing_->context_provider(),
        SemaphoreHandle(std::move(release_fence)));
  }

  vk_image_backing_->EndAccess(/*readonly=*/true, std::move(read_end_semaphore),
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

void ExternalVkImageOverlayRepresentation::GetAcquireFences(
    std::vector<gfx::GpuFence>* fences) {
  const VkDevice& device = vk_image_backing_->context_provider()
                               ->GetDeviceQueue()
                               ->GetVulkanDevice();
  for (auto& semaphore : read_begin_semaphores_) {
    DCHECK(semaphore.is_valid());
    fences->emplace_back(
        vk_image_backing_->vulkan_implementation()
            ->GetSemaphoreHandle(device, semaphore.GetVkSemaphore())
            .ToGpuFenceHandle());
  }
}

}  // namespace gpu
