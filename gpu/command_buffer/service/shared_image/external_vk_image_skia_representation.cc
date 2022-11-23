// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/external_vk_image_skia_representation.h"

#include <utility>

#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"

namespace gpu {

ExternalVkImageSkiaImageRepresentation::ExternalVkImageSkiaImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : SkiaImageRepresentation(manager, backing, tracker) {}

ExternalVkImageSkiaImageRepresentation::
    ~ExternalVkImageSkiaImageRepresentation() {
  DCHECK_EQ(access_mode_, kNone) << "Previous access hasn't end yet.";
  DCHECK(!end_access_semaphore_);
  backing_impl()->context_state()->EraseCachedSkSurface(
      backing_impl()->promise_texture().get());
}

std::vector<sk_sp<SkSurface>>
ExternalVkImageSkiaImageRepresentation::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  auto* gr_context = backing_impl()->context_state()->gr_context();
  if (gr_context->abandoned()) {
    LOG(ERROR) << "GrContext is abandonded.";
    return {};
  }

  if (access_mode_ != kNone) {
    LOG(DFATAL) << "Previous access hasn't ended yet. mode=" << access_mode_;
    return {};
  }

  auto promise_texture =
      BeginAccess(false /* readonly */, begin_semaphores, end_semaphores);
  if (!promise_texture) {
    LOG(ERROR) << "BeginAccess failed";
    return {};
  }

  auto surface = backing_impl()->context_state()->GetCachedSkSurface(
      promise_texture.get());

  // If surface properties are different from the last access, then we cannot
  // reuse the cached SkSurface.
  if (!surface || surface_props != surface->props() ||
      final_msaa_count != surface_msaa_count_) {
    SkColorType sk_color_type =
        viz::ToClosestSkColorType(true /* gpu_compositing */, format());
    surface = SkSurface::MakeFromBackendTexture(
        gr_context, promise_texture->backendTexture(), surface_origin(),
        final_msaa_count, sk_color_type,
        backing_impl()->color_space().ToSkColorSpace(), &surface_props);
    if (!surface) {
      LOG(ERROR) << "MakeFromBackendTexture() failed.";
      backing_impl()->context_state()->EraseCachedSkSurface(
          promise_texture.get());
      return {};
    }
    surface_msaa_count_ = final_msaa_count;
    backing_impl()->context_state()->CacheSkSurface(promise_texture.get(),
                                                    surface);
  }

  [[maybe_unused]] int count = surface->getCanvas()->save();
  DCHECK_EQ(count, 1);

  access_mode_ = kWrite;

  if (backing_impl()->need_synchronization()) {
    // If Vulkan/GL/Dawn share the same memory backing, we need to set
    // |end_state| VK_QUEUE_FAMILY_EXTERNAL, and then the caller will set the
    // VkImage to VK_QUEUE_FAMILY_EXTERNAL before calling EndAccess().
    *end_state = std::make_unique<GrBackendSurfaceMutableState>(
        VK_IMAGE_LAYOUT_UNDEFINED, VK_QUEUE_FAMILY_EXTERNAL);
  }

  write_surface_ = surface;
  return {surface};
}

std::vector<sk_sp<SkPromiseImageTexture>>
ExternalVkImageSkiaImageRepresentation::BeginWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  if (access_mode_ != kNone) {
    LOG(DFATAL) << "Previous access hasn't ended yet. mode=" << access_mode_;
    return {};
  }

  auto promise_texture =
      BeginAccess(false /* readonly */, begin_semaphores, end_semaphores);
  if (!promise_texture) {
    LOG(ERROR) << "BeginAccess failed";
    return {};
  }

  access_mode_ = kWrite;

  // If Vulkan/GL/Dawn share the same memory backing, we need to set
  // |end_state| VK_QUEUE_FAMILY_EXTERNAL, and then the caller will set the
  // VkImage to VK_QUEUE_FAMILY_EXTERNAL before calling EndAccess().
  if (backing_impl()->need_synchronization()) {
    *end_state = std::make_unique<GrBackendSurfaceMutableState>(
        VK_IMAGE_LAYOUT_UNDEFINED, VK_QUEUE_FAMILY_EXTERNAL);
  }

  return {promise_texture};
}

void ExternalVkImageSkiaImageRepresentation::EndWriteAccess() {
  if (access_mode_ != kWrite) {
    LOG(DFATAL) << "BeginWriteAccess is not called mode=" << access_mode_;
    return;
  }
  if (write_surface_) {
    write_surface_->getCanvas()->restoreToCount(1);
    write_surface_.reset();
    DCHECK(backing_impl()->context_state()->CachedSkSurfaceIsUnique(
        backing_impl()->promise_texture().get()));
  }
  EndAccess(false /* readonly */);
  access_mode_ = kNone;
}

std::vector<sk_sp<SkPromiseImageTexture>>
ExternalVkImageSkiaImageRepresentation::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  if (access_mode_ != kNone) {
    LOG(DFATAL) << "Previous access is not finished. mode=" << access_mode_;
    return {};
  }

  auto promise_texture =
      BeginAccess(true /* readonly */, begin_semaphores, end_semaphores);
  if (!promise_texture) {
    LOG(ERROR) << "BeginAccess failed";
    return {};
  }

  // If Vulkan/GL/Dawn share the same memory backing, we need set |end_state|
  // VK_QUEUE_FAMILY_EXTERNAL, and then the caller will set the VkImage to
  // VK_QUEUE_FAMILY_EXTERNAL before calling EndAccess().
  if (backing_impl()->need_synchronization()) {
    *end_state = std::make_unique<GrBackendSurfaceMutableState>(
        VK_IMAGE_LAYOUT_UNDEFINED, VK_QUEUE_FAMILY_EXTERNAL);
  }

  access_mode_ = kRead;
  return {promise_texture};
}

void ExternalVkImageSkiaImageRepresentation::EndReadAccess() {
  if (access_mode_ != kRead) {
    LOG(DFATAL) << "BeginReadAccess is not called. mode=" << access_mode_;
    return;
  }

  EndAccess(true /* readonly */);
  access_mode_ = kNone;
}

sk_sp<SkPromiseImageTexture>
ExternalVkImageSkiaImageRepresentation::BeginAccess(
    bool readonly,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  DCHECK_EQ(access_mode_, kNone);
  DCHECK(!end_access_semaphore_);

  DCHECK(begin_access_semaphores_.empty());
  if (!backing_impl()->BeginAccess(readonly, &begin_access_semaphores_,
                                   false /* is_gl */)) {
    return nullptr;
  }

  for (auto& external_semaphore : begin_access_semaphores_) {
    DCHECK(external_semaphore);
    VkSemaphore semaphore = external_semaphore.GetVkSemaphore();
    DCHECK(semaphore != VK_NULL_HANDLE);
    // The ownership of semaphore is passed to caller.
    begin_semaphores->emplace_back();
    begin_semaphores->back().initVulkan(semaphore);
  }

  if (backing_impl()->need_synchronization()) {
    DCHECK(end_semaphores);
    // Create an |end_access_semaphore_| which will be signalled by the caller.
    end_access_semaphore_ =
        backing_impl()->external_semaphore_pool()->GetOrCreateSemaphore();
    if (!end_access_semaphore_)
      return nullptr;
    end_semaphores->emplace_back();
    end_semaphores->back().initVulkan(end_access_semaphore_.GetVkSemaphore());
  }

  return backing_impl()->promise_texture();
}

void ExternalVkImageSkiaImageRepresentation::EndAccess(bool readonly) {
  DCHECK_NE(access_mode_, kNone);
  DCHECK(backing_impl()->need_synchronization() || !end_access_semaphore_);

  // TODO(crbug.com/1307914): This check is specific to the interop case i.e.
  // when need_synchronization() is true, but we can generalize this by making
  // the client TakeEndState() and asserting that the |end_state_| is null here.
#if DCHECK_IS_ON()
  GrVkImageInfo info;
  auto result = backing_impl()->backend_texture().getVkImageInfo(&info);
  DCHECK(result);
  DCHECK(!backing_impl()->need_synchronization() ||
         info.fCurrentQueueFamily == VK_QUEUE_FAMILY_EXTERNAL);
#endif

  backing_impl()->EndAccess(readonly, std::move(end_access_semaphore_),
                            false /* is_gl */);

  // All pending semaphores have been waited on directly or indirectly. They can
  // be reused when the next submitted GPU work is done by GPU.
  backing_impl()->ReturnPendingSemaphoresWithFenceHelper(
      std::move(begin_access_semaphores_));
  begin_access_semaphores_.clear();
}

}  // namespace gpu
