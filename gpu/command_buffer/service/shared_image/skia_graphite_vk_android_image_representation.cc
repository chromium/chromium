// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/skia_graphite_vk_android_image_representation.h"

#include <utility>

#include "base/check_op.h"
#include "base/logging.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/gpu/graphite/BackendSemaphore.h"
#include "third_party/skia/include/gpu/graphite/BackendTexture.h"
#include "third_party/skia/include/gpu/graphite/Surface.h"
#include "third_party/skia/include/gpu/graphite/vk/VulkanGraphiteTypes.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gl/gl_utils.h"

namespace gpu {

SkiaGraphiteVkAndroidImageRepresentation::
    SkiaGraphiteVkAndroidImageRepresentation(
        SharedImageManager* manager,
        AndroidImageBacking* backing,
        scoped_refptr<SharedContextState> context_state,
        MemoryTypeTracker* tracker)
    : SkiaGraphiteImageRepresentation(manager, backing, tracker),
      context_state_(std::move(context_state)) {
  // This doesn't support per plane access for multi-planar images.
  CHECK(format().is_single_plane() || format().PrefersExternalSampler());
}

SkiaGraphiteVkAndroidImageRepresentation::
    ~SkiaGraphiteVkAndroidImageRepresentation() {
  CHECK_EQ(mode_, RepresentationAccessMode::kNone);
  if (vulkan_image_) {
    VulkanFenceHelper* fence_helper = context_state_->vk_context_provider()
                                          ->GetDeviceQueue()
                                          ->GetFenceHelper();
    fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
        std::move(vulkan_image_));
  }
}

std::vector<sk_sp<SkSurface>>
SkiaGraphiteVkAndroidImageRepresentation::BeginWriteAccess(
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect) {
  if (!BeginAccessInternal(/*readonly=*/false)) {
    return {};
  }

  if (!graphite_texture_holder_) {
    graphite_texture_holder_ = CreateGraphiteTextureHolder();
  }

  SkColorType sk_color_type = viz::ToClosestSkColorType(format());

  write_surface_ = SkSurfaces::WrapBackendTexture(
      context_state_->gpu_main_graphite_recorder(),
      graphite_texture_holder_->texture(), sk_color_type,
      color_space().ToSkColorSpace(), &surface_props,
      /*release_proc=*/nullptr,
      /*release_context=*/nullptr, WrappedTextureDebugLabel(0));
  if (!write_surface_) {
    EndAccessInternal(/*readonly=*/false);
    return {};
  }

  return {write_surface_};
}

std::vector<
    scoped_refptr<SkiaGraphiteImageRepresentation::GraphiteTextureHolder>>
SkiaGraphiteVkAndroidImageRepresentation::BeginWriteAccess() {
  if (!BeginAccessInternal(/*readonly=*/false)) {
    return {};
  }

  if (!graphite_texture_holder_) {
    graphite_texture_holder_ = CreateGraphiteTextureHolder();
  }

  return {graphite_texture_holder_};
}

void SkiaGraphiteVkAndroidImageRepresentation::EndWriteAccess() {
  write_surface_ = nullptr;
  graphite_texture_holder_.reset();
  EndAccessInternal(/*readonly=*/false);
}

std::vector<
    scoped_refptr<SkiaGraphiteImageRepresentation::GraphiteTextureHolder>>
SkiaGraphiteVkAndroidImageRepresentation::BeginReadAccess() {
  if (!BeginAccessInternal(/*readonly=*/true)) {
    return {};
  }

  if (!graphite_texture_holder_) {
    graphite_texture_holder_ = CreateGraphiteTextureHolder();
  }

  return {graphite_texture_holder_};
}

void SkiaGraphiteVkAndroidImageRepresentation::EndReadAccess() {
  graphite_texture_holder_.reset();
  EndAccessInternal(/*readonly=*/true);
}

gpu::VulkanImplementation*
SkiaGraphiteVkAndroidImageRepresentation::GetVulkanImplementation() const {
  return context_state_->vk_context_provider()->GetVulkanImplementation();
}

VkDevice SkiaGraphiteVkAndroidImageRepresentation::GetVkDevice() const {
  return context_state_->vk_context_provider()
      ->GetDeviceQueue()
      ->GetVulkanDevice();
}

void SkiaGraphiteVkAndroidImageRepresentation::SetVulkanImage(
    std::unique_ptr<VulkanImage> vulkan_image) {
  CHECK(vulkan_image);
  CHECK(!vulkan_image_);
  vulkan_image_ = std::move(vulkan_image);
}

void SkiaGraphiteVkAndroidImageRepresentation::SetInitialFence(
    base::ScopedFD init_read_fence) {
  initial_fence_ = std::move(init_read_fence);
}

bool SkiaGraphiteVkAndroidImageRepresentation::BeginAccessInternal(
    bool readonly) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  base::ScopedFD begin_access_fd;
  if (readonly) {
    if (!android_backing()->BeginRead(this, &begin_access_fd)) {
      return false;
    }
  } else {
    if (!android_backing()->BeginWrite(&begin_access_fd)) {
      return false;
    }
  }
  begin_access_fd =
      gl::MergeFDs(std::move(begin_access_fd), std::move(initial_fence_));

  // Ensure any failure after BeginRead/BeginWrite unwinds the backing state.
  absl::Cleanup end_access_helper = [this, readonly]() {
    if (readonly) {
      android_backing()->EndRead(this, base::ScopedFD());
    } else {
      android_backing()->EndWrite(base::ScopedFD());
    }
  };

  DCHECK(begin_access_semaphore_ == VK_NULL_HANDLE);
  if (begin_access_fd.is_valid()) {
    begin_access_semaphore_ = GetVulkanImplementation()->ImportSemaphoreHandle(
        GetVkDevice(),
        SemaphoreHandle(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
                        std::move(begin_access_fd)));
    if (begin_access_semaphore_ == VK_NULL_HANDLE) {
      DLOG(ERROR) << "Failed to import semaphore from sync_fd.";
      return false;
    }

    // TODO(crbug.com/55295190): Submit `begin_access_semaphore_` to Skia to
    // wait on.
  }

  // TODO(crbug.com/55295190): Create `end_access_semaphore_`, if necessary,
  // and pass to Skia to signal after work is completed.

  std::move(end_access_helper).Cancel();
  mode_ = readonly ? RepresentationAccessMode::kRead
                   : RepresentationAccessMode::kWrite;
  return true;
}

void SkiaGraphiteVkAndroidImageRepresentation::EndAccessInternal(
    bool readonly) {
  base::ScopedFD end_access_fd;
  if (end_access_semaphore_ != VK_NULL_HANDLE) {
    SemaphoreHandle semaphore_handle =
        GetVulkanImplementation()->GetSemaphoreHandle(GetVkDevice(),
                                                      end_access_semaphore_);
    end_access_fd = semaphore_handle.TakeHandle();
  }

  if (readonly) {
    android_backing()->EndRead(this, std::move(end_access_fd));
  } else {
    android_backing()->EndWrite(std::move(end_access_fd));
  }

  std::vector<base::RawPtrIfPtrT<VkSemaphore, DanglingUntriaged>> semaphores;
  semaphores.reserve(2);
  if (begin_access_semaphore_ != VK_NULL_HANDLE) {
    semaphores.push_back(
        std::exchange(begin_access_semaphore_, VK_NULL_HANDLE));
  }
  if (end_access_semaphore_ != VK_NULL_HANDLE) {
    semaphores.push_back(std::exchange(end_access_semaphore_, VK_NULL_HANDLE));
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

scoped_refptr<SkiaGraphiteImageRepresentation::GraphiteTextureHolder>
SkiaGraphiteVkAndroidImageRepresentation::CreateGraphiteTextureHolder() {
  CHECK(vulkan_image_);

  skgpu::VulkanYcbcrConversionInfo ycbcr_info;
  if (vulkan_image_->ycbcr_info()) {
    VkPhysicalDevice physical_device = context_state_->vk_context_provider()
                                           ->GetDeviceQueue()
                                           ->GetVulkanPhysicalDevice();
    ycbcr_info = CreateVulkanYcbcrConversionInfo(
        physical_device, vulkan_image_->image_tiling(), vulkan_image_->format(),
        format(), color_space(), vulkan_image_->ycbcr_info());
  }

  skgpu::graphite::VulkanTextureInfo vk_texture_info(
      VK_SAMPLE_COUNT_1_BIT, skgpu::Mipmapped::kNo, vulkan_image_->flags(),
      vulkan_image_->format(), vulkan_image_->image_tiling(),
      vulkan_image_->usage(), VK_SHARING_MODE_EXCLUSIVE,
      VK_IMAGE_ASPECT_COLOR_BIT, ycbcr_info);

  skgpu::VulkanAlloc alloc;
  alloc.fMemory = vulkan_image_->device_memory();
  alloc.fOffset = 0;
  alloc.fSize = vulkan_image_->device_size();

  skgpu::graphite::BackendTexture backend_texture =
      skgpu::graphite::BackendTextures::MakeVulkan(
          gfx::SizeToSkISize(size()), vk_texture_info,
          VK_IMAGE_LAYOUT_UNDEFINED, vulkan_image_->queue_family_index(),
          vulkan_image_->image(), alloc);

  return base::MakeRefCounted<GraphiteTextureHolder>(
      std::move(backend_texture));
}

}  // namespace gpu
