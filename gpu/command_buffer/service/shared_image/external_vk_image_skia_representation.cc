// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/external_vk_image_skia_representation.h"

#include <utility>

#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/MutableTextureState.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkTypes.h"
#include "third_party/skia/include/gpu/vk/VulkanMutableTextureState.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"

namespace gpu {

ExternalVkImageSkiaImageRepresentation::ExternalVkImageSkiaImageRepresentation(
    GrDirectContext* gr_context,
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : SkiaGaneshImageRepresentation(gr_context, manager, backing, tracker),
      context_state_(backing_impl()->context_state()) {}

ExternalVkImageSkiaImageRepresentation::
    ~ExternalVkImageSkiaImageRepresentation() {
  DCHECK_EQ(access_mode_, AccessMode::kNone);
  DCHECK(!end_access_semaphore_);

  for (auto& promise_texture : backing_impl()->GetPromiseTextures()) {
    context_state_->EraseCachedSkSurface(promise_texture.get());
  }
}

std::vector<sk_sp<SkSurface>>
ExternalVkImageSkiaImageRepresentation::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<skgpu::MutableTextureState>* end_state) {
  DCHECK_EQ(access_mode_, AccessMode::kNone);

  auto* gr_context = context_state_->gr_context();
  if (gr_context->abandoned()) {
    DLOG(ERROR) << "GrContext is abandonded.";
    return {};
  }

  auto promise_textures =
      BeginAccess(/*readonly=*/false, begin_semaphores, end_semaphores);
  if (promise_textures.empty()) {
    DLOG(ERROR) << "BeginAccess failed";
    return {};
  }

  std::vector<sk_sp<SkSurface>> surfaces;
  surfaces.reserve(promise_textures.size());
  for (size_t plane = 0; plane < promise_textures.size(); ++plane) {
    auto promise_texture = promise_textures[plane];
    auto surface = context_state_->GetCachedSkSurface(promise_texture.get());

    // If surface properties are different from the last access, then we cannot
    // reuse the cached SkSurface.
    if (!surface || surface_props != surface->props() ||
        final_msaa_count != surface_msaa_count_) {
      SkColorType sk_color_type = viz::ToClosestSkColorType(
          /*gpu_compositing=*/true, format(), plane);
      surface = SkSurfaces::WrapBackendTexture(
          gr_context, promise_texture->backendTexture(), surface_origin(),
          final_msaa_count, sk_color_type,
          backing_impl()->color_space().ToSkColorSpace(), &surface_props);
      if (!surface) {
        DLOG(ERROR) << "MakeFromBackendTexture() failed.";
        context_state_->EraseCachedSkSurface(promise_texture.get());
        return {};
      }
      context_state_->CacheSkSurface(promise_texture.get(), surface);
    }
    [[maybe_unused]] int count = surface->getCanvas()->save();
    DCHECK_EQ(count, 1);

    surfaces.push_back(std::move(surface));
  }

  surface_msaa_count_ = final_msaa_count;
  access_mode_ = AccessMode::kWrite;

  if (backing_impl()->need_synchronization()) {
    // If Vulkan/GL/Dawn share the same memory backing, we need to set
    // |end_state| VK_QUEUE_FAMILY_EXTERNAL, and then the caller will set the
    // VkImage to VK_QUEUE_FAMILY_EXTERNAL before calling EndAccess().
    *end_state = std::make_unique<skgpu::MutableTextureState>(
        skgpu::MutableTextureStates::MakeVulkan(VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_QUEUE_FAMILY_EXTERNAL));
  }

  write_surfaces_ = surfaces;
  return surfaces;
}

std::vector<sk_sp<GrPromiseImageTexture>>
ExternalVkImageSkiaImageRepresentation::BeginWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<skgpu::MutableTextureState>* end_state) {
  DCHECK_EQ(access_mode_, AccessMode::kNone);

  auto promise_textures =
      BeginAccess(/*readonly=*/false, begin_semaphores, end_semaphores);
  if (promise_textures.empty()) {
    DLOG(ERROR) << "BeginAccess failed";
    return {};
  }

  access_mode_ = AccessMode::kWrite;

  // If Vulkan/GL/Dawn share the same memory backing, we need to set
  // |end_state| VK_QUEUE_FAMILY_EXTERNAL, and then the caller will set the
  // VkImage to VK_QUEUE_FAMILY_EXTERNAL before calling EndAccess().
  if (backing_impl()->need_synchronization()) {
    *end_state = std::make_unique<skgpu::MutableTextureState>(
        skgpu::MutableTextureStates::MakeVulkan(VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_QUEUE_FAMILY_EXTERNAL));
  }

  return promise_textures;
}

void ExternalVkImageSkiaImageRepresentation::EndWriteAccess() {
  DCHECK_EQ(access_mode_, AccessMode::kWrite);

  for (auto& write_surface : write_surfaces_) {
    write_surface->getCanvas()->restoreToCount(1);
  }
  write_surfaces_.clear();

#if DCHECK_IS_ON()
  for (auto& promise_texture : backing_impl()->GetPromiseTextures()) {
    DCHECK(context_state_->CachedSkSurfaceIsUnique(promise_texture.get()));
  }
#endif

  EndAccess(/*readonly=*/false);
  access_mode_ = AccessMode::kNone;
}

std::vector<sk_sp<GrPromiseImageTexture>>
ExternalVkImageSkiaImageRepresentation::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<skgpu::MutableTextureState>* end_state) {
  DCHECK_EQ(access_mode_, AccessMode::kNone);

  auto promise_textures =
      BeginAccess(/*readonly=*/true, begin_semaphores, end_semaphores);
  if (promise_textures.empty()) {
    LOG(ERROR) << "BeginAccess failed";
    return {};
  }

  // If Vulkan/GL/Dawn share the same memory backing, we need set |end_state|
  // VK_QUEUE_FAMILY_EXTERNAL, and then the caller will set the VkImage to
  // VK_QUEUE_FAMILY_EXTERNAL before calling EndAccess().
  if (backing_impl()->need_synchronization()) {
    *end_state = std::make_unique<skgpu::MutableTextureState>(
        skgpu::MutableTextureStates::MakeVulkan(VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_QUEUE_FAMILY_EXTERNAL));
  }

  access_mode_ = AccessMode::kRead;
  return promise_textures;
}

void ExternalVkImageSkiaImageRepresentation::EndReadAccess() {
  DCHECK_EQ(access_mode_, AccessMode::kRead);

  EndAccess(/*readonly=*/true);
  access_mode_ = AccessMode::kNone;
}

std::vector<sk_sp<GrPromiseImageTexture>>
ExternalVkImageSkiaImageRepresentation::BeginAccess(
    bool readonly,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  DCHECK_EQ(access_mode_, AccessMode::kNone);
  DCHECK(!end_access_semaphore_);

  DCHECK(begin_access_semaphores_.empty());
  if (!backing_impl()->BeginAccess(readonly, &begin_access_semaphores_,
                                   /*is_gl=*/false)) {
    return {};
  }

  for (auto& external_semaphore : begin_access_semaphores_) {
    DCHECK(external_semaphore);
    VkSemaphore semaphore = external_semaphore.GetVkSemaphore();
    DCHECK(semaphore != VK_NULL_HANDLE);
    // The ownership of semaphore is passed to caller.
    begin_semaphores->emplace_back(GrBackendSemaphores::MakeVk(semaphore));
  }

  if (backing_impl()->need_synchronization()) {
    DCHECK(end_semaphores);
    // Create an |end_access_semaphore_| which will be signalled by the caller.
    end_access_semaphore_ =
        backing_impl()->external_semaphore_pool()->GetOrCreateSemaphore();
    if (!end_access_semaphore_)
      return {};
    end_semaphores->emplace_back(
        GrBackendSemaphores::MakeVk(end_access_semaphore_.GetVkSemaphore()));
  }

  return backing_impl()->GetPromiseTextures();
}

void ExternalVkImageSkiaImageRepresentation::EndAccess(bool readonly) {
  DCHECK_NE(access_mode_, AccessMode::kNone);
  DCHECK(backing_impl()->need_synchronization() || !end_access_semaphore_);

  // TODO(crbug.com/40218936): This check is specific to the interop case i.e.
  // when need_synchronization() is true, but we can generalize this by making
  // the client TakeEndState() and asserting that the |end_state_| is null here.
#if DCHECK_IS_ON()
  GrVkImageInfo info;
  auto result = GrBackendTextures::GetVkImageInfo(
      backing_impl()->backend_texture(), &info);
  DCHECK(result);
  DCHECK(!backing_impl()->need_synchronization() ||
         info.fCurrentQueueFamily == VK_QUEUE_FAMILY_EXTERNAL);
#endif

  backing_impl()->EndAccess(readonly, std::move(end_access_semaphore_),
                            /*is_gl=*/false);

  // All pending semaphores have been waited on directly or indirectly. They can
  // be reused when the next submitted GPU work is done by GPU.
  backing_impl()->ReturnPendingSemaphoresWithFenceHelper(
      std::move(begin_access_semaphores_));
  begin_access_semaphores_.clear();
}

}  // namespace gpu
