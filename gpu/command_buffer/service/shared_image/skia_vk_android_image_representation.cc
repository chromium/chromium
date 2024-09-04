// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/skia_vk_android_image_representation.h"

#include <utility>

#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/skia_gl_image_representation.h"
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
#include "third_party/skia/include/gpu/MutableTextureState.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkBackendSemaphore.h"
#include "third_party/skia/include/gpu/vk/VulkanMutableTextureState.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gl/gl_utils.h"

namespace gpu {

SkiaVkAndroidImageRepresentation::SkiaVkAndroidImageRepresentation(
    SharedImageManager* manager,
    AndroidImageBacking* backing,
    scoped_refptr<SharedContextState> context_state,
    MemoryTypeTracker* tracker)
    : SkiaGaneshImageRepresentation(context_state->gr_context(),
                                    manager,
                                    backing,
                                    tracker),
      context_state_(std::move(context_state)) {
  DCHECK(backing);
  DCHECK(context_state_);
  DCHECK(context_state_->vk_context_provider());
}

SkiaVkAndroidImageRepresentation::~SkiaVkAndroidImageRepresentation() {
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

std::vector<sk_sp<SkSurface>>
SkiaVkAndroidImageRepresentation::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<skgpu::MutableTextureState>* end_state) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  DCHECK(promise_texture_);

  if (!BeginAccess(/*readonly=*/false, begin_semaphores, end_semaphores,
                   base::ScopedFD())) {
    return {};
  }

  auto* gr_context = context_state_->gr_context();
  if (gr_context->abandoned()) {
    LOG(ERROR) << "GrContext is abandoned.";
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
SkiaVkAndroidImageRepresentation::BeginWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<skgpu::MutableTextureState>* end_state) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  DCHECK(promise_texture_);

  if (!BeginAccess(/*readonly=*/false, begin_semaphores, end_semaphores,
                   base::ScopedFD())) {
    return {};
  }

  *end_state = GetEndAccessState();

  if (!promise_texture_)
    return {};
  return {promise_texture_};
}

void SkiaVkAndroidImageRepresentation::EndWriteAccess() {
  DCHECK_EQ(mode_, RepresentationAccessMode::kWrite);
  if (surface_)
    DCHECK(surface_->unique());

  // TODO(penghuang): reset canvas cached in |surface_|, when skia provides an
  // API to do it.
  // Currently, the |surface_| is only used with SkSurface::draw(ddl), it
  // doesn't create a canvas and change the state of it, so we don't get any
  // render issues. But we shouldn't assume this backing will only be used in
  // this way.
  EndAccess(/*readonly=*/false);
}

std::vector<sk_sp<GrPromiseImageTexture>>
SkiaVkAndroidImageRepresentation::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<skgpu::MutableTextureState>* end_state) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  DCHECK(!surface_);
  DCHECK(promise_texture_);

  if (!BeginAccess(/*readonly=*/true, begin_semaphores, end_semaphores,
                   std::move(init_read_fence_))) {
    return {};
  }

  *end_state = GetEndAccessState();

  if (!promise_texture_)
    return {};
  return {promise_texture_};
}

void SkiaVkAndroidImageRepresentation::EndReadAccess() {
  DCHECK_EQ(mode_, RepresentationAccessMode::kRead);
  DCHECK(!surface_);

  EndAccess(/*readonly=*/true);
}

gpu::VulkanImplementation*
SkiaVkAndroidImageRepresentation::vk_implementation() {
  return context_state_->vk_context_provider()->GetVulkanImplementation();
}

VkDevice SkiaVkAndroidImageRepresentation::vk_device() {
  return context_state_->vk_context_provider()
      ->GetDeviceQueue()
      ->GetVulkanDevice();
}

VkPhysicalDevice SkiaVkAndroidImageRepresentation::vk_phy_device() {
  return context_state_->vk_context_provider()
      ->GetDeviceQueue()
      ->GetVulkanPhysicalDevice();
}

VkQueue SkiaVkAndroidImageRepresentation::vk_queue() {
  return context_state_->vk_context_provider()
      ->GetDeviceQueue()
      ->GetVulkanQueue();
}

bool SkiaVkAndroidImageRepresentation::BeginAccess(
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
                           /*pAllocator=*/nullptr);
        begin_access_semaphore_ = VK_NULL_HANDLE;
      }
      return false;
    }
  }

  if (begin_access_semaphore_ != VK_NULL_HANDLE) {
    begin_semaphores->emplace_back(
        GrBackendSemaphores::MakeVk(begin_access_semaphore_));
  }
  if (end_semaphores) {
    end_semaphores->emplace_back(
        GrBackendSemaphores::MakeVk(end_access_semaphore_));
  }

  mode_ = readonly ? RepresentationAccessMode::kRead
                   : RepresentationAccessMode::kWrite;
  return true;
}

void SkiaVkAndroidImageRepresentation::EndAccess(bool readonly) {
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

std::unique_ptr<skgpu::MutableTextureState>
SkiaVkAndroidImageRepresentation::GetEndAccessState() {
  // There is no layout to change if there is no image.
  if (!vulkan_image_)
    return nullptr;

  // `kSingleDeviceUsage` defines the set of usages for which only the Vulkan
  // device from SharedContextState is used. If the SI has any usages outside
  // this set (e.g., if it has any GLES2 usage, including
  // RASTER_OVER_GLES2_ONLY), then it will be accessed beyond the Vulkan device
  // from SharedContextState and hence does not have single-device usage.
  const SharedImageUsageSet kSingleDeviceUsage =
      SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE |
      SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE |
      SHARED_IMAGE_USAGE_OOP_RASTERIZATION;

  // If SharedImage is used outside of current VkDeviceQueue we need to transfer
  // image back to it's original queue. Note, that for multithreading we use
  // same vkDevice, so technically we could transfer between queues instead of
  // jumping to external queue. But currently it's not possible because we
  // create new vkImage each time.
  if (!kSingleDeviceUsage.HasAll(android_backing()->usage()) ||
      android_backing()->is_thread_safe()) {
    return std::make_unique<skgpu::MutableTextureState>(
        skgpu::MutableTextureStates::MakeVulkan(
            VK_IMAGE_LAYOUT_UNDEFINED, vulkan_image_->queue_family_index()));
  }
  return nullptr;
}

}  // namespace gpu
