// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_skia_representation.h"

#include <limits>
#include <utility>

#include "base/trace_event/trace_event.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"

namespace gpu {

ExternalVkImageSkiaRepresentation::ExternalVkImageSkiaRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : SharedImageRepresentationSkia(manager, backing, tracker) {
}

ExternalVkImageSkiaRepresentation::~ExternalVkImageSkiaRepresentation() {
  DCHECK_EQ(access_mode_, kNone) << "Previoud access hasn't end yet";
  DCHECK(end_access_semaphore_ == VK_NULL_HANDLE);
}

sk_sp<SkSurface> ExternalVkImageSkiaRepresentation::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  DCHECK_EQ(access_mode_, kNone) << "Previous access hasn't ended yet";
  DCHECK(!surface_);

  auto promise_texture =
      BeginAccess(false /* readonly */, begin_semaphores, end_semaphores);
  if (!promise_texture)
    return nullptr;
  SkColorType sk_color_type = viz::ResourceFormatToClosestSkColorType(
      true /* gpu_compositing */, format());
  surface_ = SkSurface::MakeFromBackendTextureAsRenderTarget(
      backing_impl()->context_state()->gr_context(),
      promise_texture->backendTexture(), kTopLeft_GrSurfaceOrigin,
      final_msaa_count, sk_color_type,
      backing_impl()->color_space().ToSkColorSpace(), &surface_props);
  access_mode_ = kWrite;
  return surface_;
}

void ExternalVkImageSkiaRepresentation::EndWriteAccess(
    sk_sp<SkSurface> surface) {
  DCHECK_EQ(access_mode_, kWrite)
      << "EndWriteAccess is called before BeginWriteAccess";
  DCHECK(surface_);

  surface_ = nullptr;
  EndAccess(false /* readonly */);
  access_mode_ = kNone;
}

sk_sp<SkPromiseImageTexture> ExternalVkImageSkiaRepresentation::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  // TODO(penghuang): provide begin and end semaphores.
  DCHECK_EQ(access_mode_, kNone) << "Previous access hasn't ended yet";
  DCHECK(!surface_);

  auto promise_texture =
      BeginAccess(true /* readonly */, begin_semaphores, end_semaphores);
  if (!promise_texture)
    return nullptr;
  access_mode_ = kRead;
  return promise_texture;
}

void ExternalVkImageSkiaRepresentation::EndReadAccess() {
  DCHECK_EQ(access_mode_, kRead)
      << "EndReadAccess is called before BeginReadAccess";

  EndAccess(true /* readonly */);
  access_mode_ = kNone;
}

sk_sp<SkPromiseImageTexture> ExternalVkImageSkiaRepresentation::BeginAccess(
    bool readonly,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  DCHECK_EQ(access_mode_, kNone);
  DCHECK(end_access_semaphore_ == VK_NULL_HANDLE);

  std::vector<SemaphoreHandle> handles;
  if (!backing_impl()->BeginAccess(readonly, &handles, false /* is_gl */))
    return nullptr;

  for (auto& handle : handles) {
    DCHECK(handle.is_valid());
    VkSemaphore semaphore = vk_implementation()->ImportSemaphoreHandle(
        vk_device(), std::move(handle));
    DCHECK(semaphore != VK_NULL_HANDLE);
    // The ownership of semaphore is passed to caller.
    begin_semaphores->emplace_back();
    begin_semaphores->back().initVulkan(semaphore);
  }

  if (backing_impl()->need_sychronization()) {
    // Create an |end_access_semaphore_| which will be signalled by the caller.
    end_access_semaphore_ =
        vk_implementation()->CreateExternalSemaphore(backing_impl()->device());
    DCHECK(end_access_semaphore_ != VK_NULL_HANDLE);
    end_semaphores->emplace_back();
    end_semaphores->back().initVulkan(end_access_semaphore_);
  }

  return SkPromiseImageTexture::Make(backing_impl()->backend_texture());
}

void ExternalVkImageSkiaRepresentation::EndAccess(bool readonly) {
  DCHECK_NE(access_mode_, kNone);

  SemaphoreHandle handle;
  if (backing_impl()->need_sychronization()) {
    DCHECK(end_access_semaphore_ != VK_NULL_HANDLE);

    handle = vk_implementation()->GetSemaphoreHandle(vk_device(),
                                                     end_access_semaphore_);
    DCHECK(handle.is_valid());

    // We're done with the semaphore, enqueue deferred cleanup.
    fence_helper()->EnqueueSemaphoreCleanupForSubmittedWork(
        end_access_semaphore_);
    end_access_semaphore_ = VK_NULL_HANDLE;
  } else {
    DCHECK(end_access_semaphore_ == VK_NULL_HANDLE);
  }

  backing_impl()->EndAccess(readonly, std::move(handle), false /* is_gl */);
}

}  // namespace gpu