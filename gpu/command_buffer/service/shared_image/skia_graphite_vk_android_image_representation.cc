// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/skia_graphite_vk_android_image_representation.h"

#include <utility>

#include "base/check_op.h"
#include "base/logging.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "gpu/command_buffer/service/graphite_shared_context.h"
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
#include "third_party/skia/include/gpu/MutableTextureState.h"
#include "third_party/skia/include/gpu/graphite/BackendSemaphore.h"
#include "third_party/skia/include/gpu/graphite/BackendTexture.h"
#include "third_party/skia/include/gpu/graphite/Surface.h"
#include "third_party/skia/include/gpu/graphite/vk/VulkanGraphiteTypes.h"
#include "third_party/skia/include/gpu/vk/VulkanMutableTextureState.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gl/gl_utils.h"

namespace gpu {
namespace {

bool IsSingleDeviceUsage(const AndroidImageBacking* backing) {
  // `kSingleDeviceUsage` defines the set of usages for which only the Vulkan
  // device from SharedContextState is used. If the SI has any usages outside
  // this set (eg, if it has any GLES2 usage), then it will be accessed beyond
  // the Vulkan device from SharedContextState and hence does not have
  // single-device usage.
  static constexpr SharedImageUsageSet kSingleDeviceUsage =
      SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE |
      SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE;

  // Note that for multithreading we use same vkDevice, so technically we could
  // transfer between queues instead of jumping to external queue. But currently
  // it's not possible because we create new vkImage each time.
  return kSingleDeviceUsage.HasAll(backing->usage()) &&
         !backing->is_thread_safe();
}

}  // namespace

SkiaGraphiteVkAndroidImageRepresentation::
    SkiaGraphiteVkAndroidImageRepresentation(
        SharedImageManager* manager,
        AndroidImageBacking* backing,
        scoped_refptr<SharedContextState> context_state,
        MemoryTypeTracker* tracker)
    : SkiaGraphiteImageRepresentation(manager, backing, tracker),
      context_state_(std::move(context_state)),
      needs_end_access_state_(!IsSingleDeviceUsage(backing)) {
  // This doesn't support per plane access for multi-planar images.
  CHECK(format().is_single_plane() || format().PrefersExternalSampler());
}

SkiaGraphiteVkAndroidImageRepresentation::
    ~SkiaGraphiteVkAndroidImageRepresentation() {
  CHECK_EQ(mode_, RepresentationAccessMode::kNone);

  graphite_texture_holder_.reset();
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
    const gfx::Rect& update_rect,
    EndAccessState& end_access_state) {
  CHECK(graphite_texture_holder_);

  SkColorType sk_color_type = viz::ToClosestSkColorType(format());

  auto surface = SkSurfaces::WrapBackendTexture(
      context_state_->gpu_main_graphite_recorder(),
      graphite_texture_holder_->texture(), sk_color_type,
      color_space().ToSkColorSpace(), &surface_props,
      /*release_proc=*/nullptr,
      /*release_context=*/nullptr, WrappedTextureDebugLabel(0));
  if (!surface) {
    return {};
  }

  if (!BeginAccessInternal(/*readonly=*/false)) {
    return {};
  }

  write_surface_ = std::move(surface);
  end_access_state = GenerateEndAccessState();
  return {write_surface_};
}

std::vector<
    scoped_refptr<SkiaGraphiteImageRepresentation::GraphiteTextureHolder>>
SkiaGraphiteVkAndroidImageRepresentation::BeginWriteAccess(
    EndAccessState& end_access_state) {
  CHECK(graphite_texture_holder_);

  if (!BeginAccessInternal(/*readonly=*/false)) {
    return {};
  }

  end_access_state = GenerateEndAccessState();
  return {graphite_texture_holder_};
}

void SkiaGraphiteVkAndroidImageRepresentation::EndWriteAccess() {
  write_surface_ = nullptr;
  EndAccessInternal(/*readonly=*/false);
}

std::vector<
    scoped_refptr<SkiaGraphiteImageRepresentation::GraphiteTextureHolder>>
SkiaGraphiteVkAndroidImageRepresentation::BeginReadAccess(
    EndAccessState& end_access_state) {
  CHECK(graphite_texture_holder_);

  if (!BeginAccessInternal(/*readonly=*/true)) {
    return {};
  }

  end_access_state = GenerateEndAccessState();
  return {graphite_texture_holder_};
}

void SkiaGraphiteVkAndroidImageRepresentation::EndReadAccess() {
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
  graphite_texture_holder_ = CreateGraphiteTextureHolder();
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
  absl::Cleanup end_access_helper = [this, readonly, &begin_access_fd]() {
    if (readonly) {
      android_backing()->EndRead(this, std::move(begin_access_fd));
    } else {
      android_backing()->EndWrite(std::move(begin_access_fd));
    }
  };

  DCHECK(end_access_semaphore_ == VK_NULL_HANDLE);
  if (needs_end_access_state_) {
    end_access_semaphore_ =
        GetVulkanImplementation()->CreateExternalSemaphore(GetVkDevice());
    if (end_access_semaphore_ == VK_NULL_HANDLE) {
      DLOG(ERROR) << "Failed to create external semaphore for end access.";
      return false;
    }
  }

  DCHECK(begin_access_semaphore_ == VK_NULL_HANDLE);
  if (begin_access_fd.is_valid()) {
    begin_access_semaphore_ = GetVulkanImplementation()->ImportSemaphoreHandle(
        GetVkDevice(),
        SemaphoreHandle(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
                        std::move(begin_access_fd)));
    if (begin_access_semaphore_ == VK_NULL_HANDLE) {
      DLOG(ERROR) << "Failed to import semaphore from sync_fd.";
      vkDestroySemaphore(GetVkDevice(),
                         std::exchange(end_access_semaphore_, VK_NULL_HANDLE),
                         /*pAllocator=*/nullptr);
      return false;
    }

    // Any work submitted after this access begins will wait for the semaphore.
    context_state_->graphite_shared_context()->AddPendingWaitSemaphore(
        skgpu::graphite::BackendSemaphores::MakeVulkan(
            begin_access_semaphore_));
  }

  std::move(end_access_helper).Cancel();
  mode_ = readonly ? RepresentationAccessMode::kRead
                   : RepresentationAccessMode::kWrite;
  return true;
}

void SkiaGraphiteVkAndroidImageRepresentation::EndAccessInternal(
    bool readonly) {
  base::ScopedFD end_access_fd;
  if (end_access_semaphore_ != VK_NULL_HANDLE) {
    // ~EndAccessState() guarantees the signal semaphore has been submitted by
    // now.
    SemaphoreHandle semaphore_handle =
        GetVulkanImplementation()->GetSemaphoreHandle(GetVkDevice(),
                                                      end_access_semaphore_);
    end_access_fd = semaphore_handle.TakeHandle();
    CHECK(end_access_fd.is_valid());
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

EndAccessState
SkiaGraphiteVkAndroidImageRepresentation::GenerateEndAccessState() {
  if (!needs_end_access_state_) {
    return {};
  }

  // The signal semaphore must not be enqueued until all work using this image
  // has been inserted, otherwise another insert/submit could signal it before
  // that work executes. The scoped access enqueues it from
  // ApplyBackendSurfaceEndState().
  return EndAccessState(
      context_state_->graphite_shared_context(),
      graphite_texture_holder_->texture(),
      std::make_unique<skgpu::MutableTextureState>(
          skgpu::MutableTextureStates::MakeVulkan(
              VK_IMAGE_LAYOUT_UNDEFINED, vulkan_image_->queue_family_index())),
      skgpu::graphite::BackendSemaphores::MakeVulkan(end_access_semaphore_));
}

}  // namespace gpu
