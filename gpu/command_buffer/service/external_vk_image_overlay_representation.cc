// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_overlay_representation.h"

#if defined(OS_FUCHSIA)
#include "gpu/vulkan/fuchsia/vulkan_fuchsia_ext.h"
#endif

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

SemaphoreHandle GpuFenceHandleToSemaphoreHandle(
    gfx::GpuFenceHandle fence_handle) {
#if defined(OS_FUCHSIA)
  // Fuchsia's Vulkan driver allows zx::event to be obtained from a
  // VkSemaphore, which can then be used to submit present work, see
  // https://fuchsia.dev/reference/fidl/fuchsia.ui.scenic.
  return SemaphoreHandle(
      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TEMP_ZIRCON_EVENT_BIT_FUCHSIA,
      std::move(fence_handle.owned_event));
#elif defined(OS_POSIX)
  return SemaphoreHandle(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR,
                         std::move(fence_handle.owned_fd));
#elif defined(OS_WIN)
  return SemaphoreHandle(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT,
                         std::move(fence_handle.owned_handle));
#endif  // defined(OS_FUCHSIA)
  return SemaphoreHandle();
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
        GpuFenceHandleToSemaphoreHandle(std::move(release_fence)));
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

}  // namespace gpu
