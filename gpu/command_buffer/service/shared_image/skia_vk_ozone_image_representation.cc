// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/skia_vk_ozone_image_representation.h"

#include <utility>

#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/external_semaphore_pool.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurfaceMutableState.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"

namespace gpu {

// Vk backed Skia representation of OzoneImageBacking.
SkiaVkOzoneImageRepresentation::SkiaVkOzoneImageRepresentation(
    SharedImageManager* manager,
    OzoneImageBacking* backing,
    scoped_refptr<SharedContextState> context_state,
    std::unique_ptr<VulkanImage> vulkan_image,
    MemoryTypeTracker* tracker)
    : SkiaGaneshImageRepresentation(context_state->gr_context(),
                                    manager,
                                    backing,
                                    tracker),
      vulkan_image_(std::move(vulkan_image)),
      context_state_(std::move(context_state)) {
  DCHECK(backing);
  DCHECK(context_state_);
  DCHECK(context_state_->vk_context_provider());
  DCHECK(vulkan_image_);

  promise_texture_ = GrPromiseImageTexture::Make(
      GrBackendTexture(size().width(), size().height(),
                       CreateGrVkImageInfo(vulkan_image_.get())));
  DCHECK(promise_texture_);
}

SkiaVkOzoneImageRepresentation::~SkiaVkOzoneImageRepresentation() {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  surface_.reset();
  if (vulkan_image_) {
    VulkanFenceHelper* fence_helper = context_state_->vk_context_provider()
                                          ->GetDeviceQueue()
                                          ->GetFenceHelper();
    fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
        std::move(vulkan_image_));
  }
}

std::vector<sk_sp<SkSurface>> SkiaVkOzoneImageRepresentation::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  DCHECK(promise_texture_);

  if (!BeginAccess(/*readonly=*/false, begin_semaphores, end_semaphores))
    return {};

  auto* gr_context = context_state_->gr_context();
  if (gr_context->abandoned()) {
    LOG(ERROR) << "GrContext is abandoned.";
    ozone_backing()->EndAccess(/*readonly=*/false,
                               OzoneImageBacking::AccessStream::kVulkan,
                               gfx::GpuFenceHandle());
    return {};
  }

  if (!surface_ || final_msaa_count != surface_msaa_count_ ||
      surface_props != surface_->props()) {
    SkColorType sk_color_type = viz::ToClosestSkColorType(
        /*gpu_compositing=*/true, format());
    surface_ = SkSurfaces::WrapBackendTexture(
        gr_context, promise_texture_->backendTexture(), surface_origin(),
        final_msaa_count, sk_color_type, color_space().ToSkColorSpace(),
        &surface_props);
    if (!surface_) {
      LOG(ERROR) << "MakeFromBackendTexture() failed.";
      ozone_backing()->EndAccess(/*readonly=*/false,
                                 OzoneImageBacking::AccessStream::kVulkan,
                                 gfx::GpuFenceHandle());
      return {};
    }
    surface_msaa_count_ = final_msaa_count;
  }

  *end_state = GetEndAccessState();

  if (!surface_)
    return {};
  return {surface_};
}

std::vector<sk_sp<GrPromiseImageTexture>>
SkiaVkOzoneImageRepresentation::BeginWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  DCHECK(promise_texture_);

  if (!BeginAccess(/*readonly=*/false, begin_semaphores, end_semaphores)) {
    return {};
  }

  *end_state = GetEndAccessState();

  if (!promise_texture_)
    return {};
  return {promise_texture_};
}

void SkiaVkOzoneImageRepresentation::EndWriteAccess() {
  DCHECK_EQ(mode_, RepresentationAccessMode::kWrite);
  if (surface_)
    DCHECK(surface_->unique());
  EndAccess(/*readonly=*/false);
}

std::vector<sk_sp<GrPromiseImageTexture>>
SkiaVkOzoneImageRepresentation::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  DCHECK(!surface_);
  DCHECK(promise_texture_);

  if (!BeginAccess(/*readonly=*/true, begin_semaphores, end_semaphores)) {
    return {};
  }

  *end_state = GetEndAccessState();

  if (!promise_texture_)
    return {};
  return {promise_texture_};
}

void SkiaVkOzoneImageRepresentation::EndReadAccess() {
  DCHECK_EQ(mode_, RepresentationAccessMode::kRead);
  DCHECK(!surface_);

  EndAccess(/*readonly=*/true);
}

gpu::VulkanImplementation* SkiaVkOzoneImageRepresentation::vk_implementation() {
  return context_state_->vk_context_provider()->GetVulkanImplementation();
}

VkDevice SkiaVkOzoneImageRepresentation::vk_device() {
  return context_state_->vk_context_provider()
      ->GetDeviceQueue()
      ->GetVulkanDevice();
}

bool SkiaVkOzoneImageRepresentation::BeginAccess(
    bool readonly,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  DCHECK(begin_semaphores);
  DCHECK(end_access_semaphore_ == VK_NULL_HANDLE);

  std::vector<gfx::GpuFenceHandle> fences;
  if (!ozone_backing()->BeginAccess(readonly,
                                    OzoneImageBacking::AccessStream::kVulkan,
                                    &fences, need_end_fence_))
    return false;

  VkDevice device = vk_device();
  auto* implementation = vk_implementation();

  for (auto& fence : fences) {
    VkSemaphore vk_semaphore = implementation->ImportSemaphoreHandle(
        device, SemaphoreHandle(std::move(fence)));

    begin_access_semaphores_.emplace_back(vk_semaphore);
    begin_semaphores->emplace_back();
    begin_semaphores->back().initVulkan(vk_semaphore);
  }

  if (end_semaphores && need_end_fence_) {
    end_access_semaphore_ =
        vk_implementation()->CreateExternalSemaphore(vk_device());

    if (end_access_semaphore_ == VK_NULL_HANDLE) {
      DLOG(ERROR) << "Failed to create the external semaphore.";
      ozone_backing()->EndAccess(readonly,
                                 OzoneImageBacking::AccessStream::kVulkan,
                                 gfx::GpuFenceHandle());
      return false;
    }

    end_semaphores->emplace_back();
    end_semaphores->back().initVulkan(end_access_semaphore_);
  }

  mode_ = readonly ? RepresentationAccessMode::kRead
                   : RepresentationAccessMode::kWrite;
  return true;
}

void SkiaVkOzoneImageRepresentation::EndAccess(bool readonly) {
  gfx::GpuFenceHandle fence;
  if (end_access_semaphore_ != VK_NULL_HANDLE) {
    SemaphoreHandle semaphore_handle = vk_implementation()->GetSemaphoreHandle(
        vk_device(), end_access_semaphore_);
    fence = std::move(semaphore_handle).ToGpuFenceHandle();
    DLOG_IF(ERROR, fence.is_null()) << "Failed to convert the external semaphore to fence.";
  }

  ozone_backing()->EndAccess(readonly, OzoneImageBacking::AccessStream::kVulkan,
                             std::move(fence));

  std::vector<VkSemaphore> semaphores = std::move(begin_access_semaphores_);
  begin_access_semaphores_.clear();
  if (end_access_semaphore_ != VK_NULL_HANDLE) {
    semaphores.emplace_back(end_access_semaphore_);
    end_access_semaphore_ = VK_NULL_HANDLE;
  }
  if (!semaphores.empty()) {
    VulkanFenceHelper* fence_helper = context_state_->vk_context_provider()
                                          ->GetDeviceQueue()
                                          ->GetFenceHelper();
    fence_helper->EnqueueSemaphoresCleanupForSubmittedWork(
        std::move(semaphores));
  }

  mode_ = RepresentationAccessMode::kNone;
}

std::unique_ptr<GrBackendSurfaceMutableState>
SkiaVkOzoneImageRepresentation::GetEndAccessState() {
  // There is no layout to change if there is no image.
  if (!vulkan_image_)
    return nullptr;

  const uint32_t kSingleDeviceUsage =
      SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE |
      SHARED_IMAGE_USAGE_RASTER | SHARED_IMAGE_USAGE_OOP_RASTERIZATION;

  // If SharedImage is used outside of current VkDeviceQueue we need to transfer
  // image back to it's original queue. Note, that for multithreading we use
  // same vkDevice, so technically we could transfer between queues instead of
  // jumping to external queue. But currently it's not possible because we
  // create new vkImage each time.
  if ((ozone_backing()->usage() & ~kSingleDeviceUsage) ||
      ozone_backing()->is_thread_safe()) {
    DCHECK_NE(vulkan_image_->queue_family_index(), VK_QUEUE_FAMILY_IGNORED);

    return std::make_unique<GrBackendSurfaceMutableState>(
        VK_IMAGE_LAYOUT_UNDEFINED, vulkan_image_->queue_family_index());
  }
  return nullptr;
}

}  // namespace gpu
