// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shared_image_representation_skia_vk_android.h"

#include <utility>

#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image_representation_skia_gl.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gl/gl_utils.h"

namespace gpu {

SharedImageRepresentationSkiaVkAndroid::SharedImageRepresentationSkiaVkAndroid(
    SharedImageManager* manager,
    SharedImageBackingAndroid* backing,
    scoped_refptr<SharedContextState> context_state,
    MemoryTypeTracker* tracker)
    : SharedImageRepresentationSkia(manager, backing, tracker),
      context_state_(std::move(context_state)) {
  DCHECK(backing);
  DCHECK(context_state_);
  DCHECK(context_state_->vk_context_provider());
}

SharedImageRepresentationSkiaVkAndroid::
    ~SharedImageRepresentationSkiaVkAndroid() {
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

sk_sp<SkSurface> SharedImageRepresentationSkiaVkAndroid::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  DCHECK(promise_texture_);

  if (!BeginAccess(false /* readonly */, begin_semaphores, end_semaphores,
                   base::ScopedFD()))
    return nullptr;

  auto* gr_context = context_state_->gr_context();
  if (gr_context->abandoned()) {
    LOG(ERROR) << "GrContext is abandoned.";
    return nullptr;
  }

  if (!surface_ || final_msaa_count != surface_msaa_count_ ||
      surface_props != surface_->props()) {
    SkColorType sk_color_type = viz::ResourceFormatToClosestSkColorType(
        /*gpu_compositing=*/true, format());
    surface_ = SkSurface::MakeFromBackendTexture(
        gr_context, promise_texture_->backendTexture(), surface_origin(),
        final_msaa_count, sk_color_type, color_space().ToSkColorSpace(),
        &surface_props);
    if (!surface_) {
      LOG(ERROR) << "MakeFromBackendTexture() failed.";
      return nullptr;
    }
    surface_msaa_count_ = final_msaa_count;
  }

  // If the backing could be used for scanout, we always set the layout to
  // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR after each accessing.
  if (android_backing()->usage() & SHARED_IMAGE_USAGE_SCANOUT) {
    *end_state = std::make_unique<GrBackendSurfaceMutableState>(
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_QUEUE_FAMILY_IGNORED);
  }

  return surface_;
}

void SharedImageRepresentationSkiaVkAndroid::EndWriteAccess(
    sk_sp<SkSurface> surface) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kWrite);
  DCHECK_EQ(surface.get(), surface_.get());

  surface.reset();
  DCHECK(surface_->unique());
  // TODO(penghuang): reset canvas cached in |surface_|, when skia provides an
  // API to do it.
  // Currently, the |surface_| is only used with SkSurface::draw(ddl), it
  // doesn't create a canvas and change the state of it, so we don't get any
  // render issues. But we shouldn't assume this backing will only be used in
  // this way.
  EndAccess(false /* readonly */);
}

sk_sp<SkPromiseImageTexture>
SharedImageRepresentationSkiaVkAndroid::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  DCHECK(!surface_);
  DCHECK(promise_texture_);

  if (!BeginAccess(true /* readonly */, begin_semaphores, end_semaphores,
                   std::move(init_read_fence_)))
    return nullptr;

  // If the backing could be used for scanout, we always set the layout to
  // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR after each accessing.
  if (android_backing()->usage() & SHARED_IMAGE_USAGE_SCANOUT) {
    *end_state = std::make_unique<GrBackendSurfaceMutableState>(
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_QUEUE_FAMILY_IGNORED);
  }

  return promise_texture_;
}

void SharedImageRepresentationSkiaVkAndroid::EndReadAccess() {
  DCHECK_EQ(mode_, RepresentationAccessMode::kRead);
  DCHECK(!surface_);

  EndAccess(true /* readonly */);
}

gpu::VulkanImplementation*
SharedImageRepresentationSkiaVkAndroid::vk_implementation() {
  return context_state_->vk_context_provider()->GetVulkanImplementation();
}

VkDevice SharedImageRepresentationSkiaVkAndroid::vk_device() {
  return context_state_->vk_context_provider()
      ->GetDeviceQueue()
      ->GetVulkanDevice();
}

VkPhysicalDevice SharedImageRepresentationSkiaVkAndroid::vk_phy_device() {
  return context_state_->vk_context_provider()
      ->GetDeviceQueue()
      ->GetVulkanPhysicalDevice();
}

VkQueue SharedImageRepresentationSkiaVkAndroid::vk_queue() {
  return context_state_->vk_context_provider()
      ->GetDeviceQueue()
      ->GetVulkanQueue();
}

bool SharedImageRepresentationSkiaVkAndroid::BeginAccess(
    bool readonly,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    base::ScopedFD init_read_fence) {
  DCHECK(begin_semaphores);
  DCHECK(end_access_semaphore_ == VK_NULL_HANDLE);

  // Synchronise the read access with the writes.
  base::ScopedFD sync_fd;
  if (readonly) {
    if (!android_backing()->BeginRead(this, &sync_fd))
      return false;
  } else {
    if (!android_backing()->BeginWrite(&sync_fd))
      return false;
  }

  sync_fd = gl::MergeFDs(std::move(sync_fd), std::move(init_read_fence));
  DCHECK(begin_access_semaphore_ == VK_NULL_HANDLE);
  if (sync_fd.is_valid()) {
    begin_access_semaphore_ = vk_implementation()->ImportSemaphoreHandle(
        vk_device(),
        SemaphoreHandle(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
                        std::move(sync_fd)));
    if (begin_access_semaphore_ == VK_NULL_HANDLE) {
      DLOG(ERROR) << "Failed to import semaphore from sync_fd.";
      return false;
    }
  }

  if (end_semaphores) {
    end_access_semaphore_ =
        vk_implementation()->CreateExternalSemaphore(vk_device());

    if (end_access_semaphore_ == VK_NULL_HANDLE) {
      DLOG(ERROR) << "Failed to create the external semaphore.";
      if (begin_access_semaphore_ != VK_NULL_HANDLE) {
        vkDestroySemaphore(vk_device(), begin_access_semaphore_,
                           nullptr /* pAllocator */);
        begin_access_semaphore_ = VK_NULL_HANDLE;
      }
      return false;
    }
  }

  if (begin_access_semaphore_ != VK_NULL_HANDLE) {
    begin_semaphores->emplace_back();
    begin_semaphores->back().initVulkan(begin_access_semaphore_);
  }
  if (end_semaphores) {
    end_semaphores->emplace_back();
    end_semaphores->back().initVulkan(end_access_semaphore_);
  }

  mode_ = readonly ? RepresentationAccessMode::kRead
                   : RepresentationAccessMode::kWrite;
  return true;
}

void SharedImageRepresentationSkiaVkAndroid::EndAccess(bool readonly) {
  base::ScopedFD sync_fd;
  if (end_access_semaphore_ != VK_NULL_HANDLE) {
    SemaphoreHandle semaphore_handle = vk_implementation()->GetSemaphoreHandle(
        vk_device(), end_access_semaphore_);
    sync_fd = semaphore_handle.TakeHandle();
    DCHECK(sync_fd.is_valid());
  }

  if (readonly) {
    android_backing()->EndRead(this, std::move(sync_fd));
  } else {
    android_backing()->EndWrite(std::move(sync_fd));
  }

  std::vector<VkSemaphore> semaphores;
  semaphores.reserve(2);
  if (begin_access_semaphore_ != VK_NULL_HANDLE) {
    semaphores.emplace_back(begin_access_semaphore_);
    begin_access_semaphore_ = VK_NULL_HANDLE;
  }
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

}  // namespace gpu
