// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_ozone.h"

#include <dawn/webgpu.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image_representation_gl_ozone.h"
#include "gpu/command_buffer/service/shared_image_representation_skia_gl.h"
#include "gpu/command_buffer/service/shared_image_representation_skia_vk_ozone.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_image_native_pixmap.h"

#if BUILDFLAG(USE_DAWN)
#include "gpu/command_buffer/service/shared_image_representation_dawn_ozone.h"
#endif  // BUILDFLAG(USE_DAWN)

namespace gpu {
namespace {

size_t GetPixmapSizeInBytes(const gfx::NativePixmap& pixmap) {
  return gfx::BufferSizeForBufferFormat(pixmap.GetBufferSize(),
                                        pixmap.GetBufferFormat());
}

}  // namespace

class SharedImageBackingOzone::SharedImageRepresentationVaapiOzone
    : public SharedImageRepresentationVaapi {
 public:
  SharedImageRepresentationVaapiOzone(SharedImageManager* manager,
                                      SharedImageBacking* backing,
                                      MemoryTypeTracker* tracker,
                                      VaapiDependencies* vaapi_dependency)
      : SharedImageRepresentationVaapi(manager,
                                       backing,
                                       tracker,
                                       vaapi_dependency) {}

 private:
  SharedImageBackingOzone* ozone_backing() {
    return static_cast<SharedImageBackingOzone*>(backing());
  }
  void EndAccess() override { ozone_backing()->has_pending_va_writes_ = true; }
  void BeginAccess() override {
    // TODO(andrescj): DCHECK that there are no fences to wait on (because the
    // compositor should be completely done with a VideoFrame before returning
    // it).
  }
};

class SharedImageBackingOzone::SharedImageRepresentationOverlayOzone
    : public SharedImageRepresentationOverlay {
 public:
  SharedImageRepresentationOverlayOzone(SharedImageManager* manager,
                                        SharedImageBacking* backing,
                                        MemoryTypeTracker* tracker)
      : SharedImageRepresentationOverlay(manager, backing, tracker) {}
  ~SharedImageRepresentationOverlayOzone() override = default;

 private:
  bool BeginReadAccess(std::vector<gfx::GpuFence>* acquire_fences) override {
    auto* ozone_backing = static_cast<SharedImageBackingOzone*>(backing());
    std::vector<gfx::GpuFenceHandle> fences;
    bool need_end_fence;
    ozone_backing->BeginAccess(/*readonly=*/true, AccessStream::kOverlay,
                               &fences, need_end_fence);
    // Always need an end fence when finish reading from overlays.
    DCHECK(need_end_fence);
    for (auto& fence : fences) {
      acquire_fences->emplace_back(std::move(fence));
    }
    return true;
  }
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override {
    auto* ozone_backing = static_cast<SharedImageBackingOzone*>(backing());
    ozone_backing->EndAccess(/*readonly=*/true, AccessStream::kOverlay,
                             std::move(release_fence));
  }

  gl::GLImage* GetGLImage() override {
    if (!gl_image_) {
      gfx::BufferFormat buffer_format = viz::BufferFormat(format());
      auto pixmap =
          static_cast<SharedImageBackingOzone*>(backing())->GetNativePixmap();
      gl_image_ = base::MakeRefCounted<gl::GLImageNativePixmap>(
          pixmap->GetBufferSize(), buffer_format);
      gl_image_->InitializeForOverlay(pixmap);
    }

    return gl_image_.get();
  }

  scoped_refptr<gl::GLImageNativePixmap> gl_image_;
};

SharedImageBackingOzone::~SharedImageBackingOzone() = default;

void SharedImageBackingOzone::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  if (shared_memory_wrapper_.IsValid()) {
    DCHECK(!in_fence);
    if (context_state_->context_lost())
      return;

    DCHECK(context_state_->IsCurrent(nullptr));
    if (!WritePixels(shared_memory_wrapper_.GetMemoryAsSpan(),
                     context_state_.get(), format(), size(), alpha_type())) {
      DLOG(ERROR) << "Failed to write pixels.";
    }
  }
}

void SharedImageBackingOzone::SetSharedMemoryWrapper(
    SharedMemoryRegionWrapper wrapper) {
  shared_memory_wrapper_ = std::move(wrapper);
}

bool SharedImageBackingOzone::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  NOTREACHED();
  return false;
}

scoped_refptr<gfx::NativePixmap> SharedImageBackingOzone::GetNativePixmap() {
  return pixmap_;
}

std::unique_ptr<SharedImageRepresentationDawn>
SharedImageBackingOzone::ProduceDawn(SharedImageManager* manager,
                                     MemoryTypeTracker* tracker,
                                     WGPUDevice device,
                                     WGPUBackendType backend_type) {
#if BUILDFLAG(USE_DAWN)
  DCHECK(dawn_procs_);
  WGPUTextureFormat webgpu_format = viz::ToWGPUFormat(format());
  if (webgpu_format == WGPUTextureFormat_Undefined) {
    return nullptr;
  }
  return std::make_unique<SharedImageRepresentationDawnOzone>(
      manager, this, tracker, device, webgpu_format, pixmap_, dawn_procs_);
#else  // !BUILDFLAG(USE_DAWN)
  return nullptr;
#endif
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageBackingOzone::ProduceGLTexture(SharedImageManager* manager,
                                          MemoryTypeTracker* tracker) {
  return SharedImageRepresentationGLTextureOzone::Create(
      manager, this, tracker, pixmap_, format(), plane_);
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageBackingOzone::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  return SharedImageRepresentationGLTexturePassthroughOzone::Create(
      manager, this, tracker, pixmap_, format(), plane_);
}

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageBackingOzone::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (context_state->GrContextIsGL()) {
    auto gl_representation = ProduceGLTexture(manager, tracker);
    if (!gl_representation) {
      LOG(ERROR) << "SharedImageBackingOzone::ProduceSkia failed to create GL "
                    "representation";
      return nullptr;
    }
    auto skia_representation = SharedImageRepresentationSkiaGL::Create(
        std::move(gl_representation), std::move(context_state), manager, this,
        tracker);
    if (!skia_representation) {
      LOG(ERROR) << "SharedImageBackingOzone::ProduceSkia failed to create "
                    "Skia representation";
      return nullptr;
    }
    return skia_representation;
  }
  if (context_state->GrContextIsVulkan()) {
    auto* device_queue = context_state->vk_context_provider()->GetDeviceQueue();
    gfx::GpuMemoryBufferHandle gmb_handle;
    gmb_handle.type = gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
    gmb_handle.native_pixmap_handle = pixmap_->ExportHandle();
    auto* vulkan_implementation =
        context_state->vk_context_provider()->GetVulkanImplementation();
    auto vulkan_image = vulkan_implementation->CreateImageFromGpuMemoryHandle(
        device_queue, std::move(gmb_handle), size(), ToVkFormat(format()));

    if (!vulkan_image)
      return nullptr;

    return std::make_unique<SharedImageRepresentationSkiaVkOzone>(
        manager, this, std::move(context_state), std::move(vulkan_image),
        tracker);
  }
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationOverlay>
SharedImageBackingOzone::ProduceOverlay(SharedImageManager* manager,
                                        MemoryTypeTracker* tracker) {
  return std::make_unique<SharedImageRepresentationOverlayOzone>(manager, this,
                                                                 tracker);
}

SharedImageBackingOzone::SharedImageBackingOzone(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    gfx::BufferPlane plane,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    scoped_refptr<SharedContextState> context_state,
    scoped_refptr<gfx::NativePixmap> pixmap,
    scoped_refptr<base::RefCountedData<DawnProcTable>> dawn_procs)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      GetPixmapSizeInBytes(*pixmap),
                                      false),
      plane_(plane),
      pixmap_(std::move(pixmap)),
      dawn_procs_(std::move(dawn_procs)),
      context_state_(std::move(context_state)) {
  bool used_by_skia = (usage & SHARED_IMAGE_USAGE_RASTER) ||
                      (usage & SHARED_IMAGE_USAGE_DISPLAY);
  bool used_by_gl =
      (usage & SHARED_IMAGE_USAGE_GLES2) ||
      (used_by_skia && context_state_->gr_context_type() == GrContextType::kGL);
  bool used_by_vulkan = used_by_skia && context_state_->gr_context_type() ==
                                            GrContextType::kVulkan;
  bool used_by_webgpu = usage & SHARED_IMAGE_USAGE_WEBGPU;
  write_streams_count_ = 0;
  if (used_by_gl)
    write_streams_count_++;  // gl can write
  if (used_by_vulkan)
    write_streams_count_++;  // vulkan can write
  if (used_by_webgpu)
    write_streams_count_++;  // webgpu can write

  if (write_streams_count_ == 1) {
    // Initialize last_write_stream_ if its a single stream for cases where
    // read happens before write eg. video decoder with usage set as SCANOUT.
    last_write_stream_ = used_by_gl ? AccessStream::kGL
                                    : (used_by_vulkan ? AccessStream::kVulkan
                                                      : AccessStream::kWebGPU);
  }
}

std::unique_ptr<SharedImageRepresentationVaapi>
SharedImageBackingOzone::ProduceVASurface(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    VaapiDependenciesFactory* dep_factory) {
  DCHECK(pixmap_);
  if (!vaapi_deps_)
    vaapi_deps_ = dep_factory->CreateVaapiDependencies(pixmap_);

  if (!vaapi_deps_) {
    LOG(ERROR) << "SharedImageBackingOzone::ProduceVASurface failed to create "
                  "VaapiDependencies";
    return nullptr;
  }
  return std::make_unique<
      SharedImageBackingOzone::SharedImageRepresentationVaapiOzone>(
      manager, this, tracker, vaapi_deps_.get());
}

bool SharedImageBackingOzone::VaSync() {
  if (has_pending_va_writes_)
    has_pending_va_writes_ = !vaapi_deps_->SyncSurface();
  return !has_pending_va_writes_;
}

bool SharedImageBackingOzone::WritePixels(
    base::span<const uint8_t> pixel_data,
    SharedContextState* const shared_context_state,
    viz::ResourceFormat format,
    const gfx::Size& size,
    SkAlphaType alpha_type) {
  auto representation =
      ProduceSkia(nullptr, shared_context_state->memory_type_tracker(),
                  shared_context_state);

  SkImageInfo info = SkImageInfo::Make(size.width(), size.height(),
                                       ResourceFormatToClosestSkColorType(
                                           /*gpu_compositing=*/true, format),
                                       alpha_type);
  SkPixmap sk_pixmap(info, pixel_data.data(), info.minRowBytes());

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  // Allow uncleared access, as we manually handle clear tracking.
  auto dest_scoped_access = representation->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes,
      /*use_sk_surface=*/false);
  if (!dest_scoped_access) {
    return false;
  }
  if (!begin_semaphores.empty()) {
    bool result = shared_context_state->gr_context()->wait(
        begin_semaphores.size(), begin_semaphores.data(),
        /*deleteSemaphoresAfterWait=*/false);
    DCHECK(result);
  }

  DCHECK_EQ(size, representation->size());
  bool written = shared_context_state->gr_context()->updateBackendTexture(
      dest_scoped_access->promise_image_texture()->backendTexture(), &sk_pixmap,
      /*numLevels=*/1, representation->surface_origin(), nullptr, nullptr);

  FlushAndSubmitIfNecessary(std::move(end_semaphores), shared_context_state);
  if (written && !representation->IsCleared()) {
    representation->SetClearedRect(gfx::Rect(info.width(), info.height()));
  }
  return written;
}

void SharedImageBackingOzone::FlushAndSubmitIfNecessary(
    std::vector<GrBackendSemaphore> signal_semaphores,
    SharedContextState* const shared_context_state) {
  bool sync_cpu = gpu::ShouldVulkanSyncCpuForSkiaSubmit(
      shared_context_state->vk_context_provider());
  GrFlushInfo flush_info = {};
  if (!signal_semaphores.empty()) {
    flush_info = {
        .fNumSemaphores = signal_semaphores.size(),
        .fSignalSemaphores = signal_semaphores.data(),
    };
    gpu::AddVulkanCleanupTaskForSkiaFlush(
        shared_context_state->vk_context_provider(), &flush_info);
  }

  shared_context_state->gr_context()->flush(flush_info);
  if (sync_cpu || !signal_semaphores.empty()) {
    shared_context_state->gr_context()->submit();
  }
}

bool SharedImageBackingOzone::BeginAccess(
    bool readonly,
    AccessStream access_stream,
    std::vector<gfx::GpuFenceHandle>* fences,
    bool& need_end_fence) {
  if (is_write_in_progress_) {
    DLOG(ERROR) << "Unable to begin read or write access because another write "
                   "access is in progress";
    return false;
  }

  if (reads_in_progress_ && !readonly) {
    DLOG(ERROR)
        << "Unable to begin write access because a read access is in progress";
    return false;
  }

  if (readonly) {
    ++reads_in_progress_;
  } else {
    is_write_in_progress_ = true;
  }

  // We don't wait for read-after-read.
  if (!readonly) {
    for (auto& fence : read_fences_) {
      // Wait on fence only if reading from stream different than current
      // stream.
      if (fence.first != access_stream) {
        DCHECK(!fence.second.is_null());
        fences->emplace_back(std::move(fence.second));
      }
    }
    read_fences_.clear();
  }

  // If current stream is different than last_write_stream_ then wait on that
  // stream's write_fence_.
  if (last_write_stream_ != access_stream && !write_fence_.is_null()) {
    // For write access we expect new write_fence_ so we can move the old fence
    // here.
    if (!readonly)
      fences->emplace_back(std::move(write_fence_));
    else
      fences->emplace_back(write_fence_.Clone());
  }

  if (readonly) {
    // Optimization for single write streams. Normally we need a read fence to
    // wait before write on a write stream. But if it single write stream, we
    // can skip read fence for it because there's no need to wait for fences on
    // the same stream.
    need_end_fence =
        (write_streams_count_ > 1) || (last_write_stream_ != access_stream);
  } else {
    // Log if it's a single write stream and write comes from a different stream
    // than expected (GL, Vulkan or WebGPU).
    LOG_IF(DFATAL,
           write_streams_count_ == 1 && last_write_stream_ != access_stream)
        << "Unexpected write stream: " << static_cast<int>(access_stream)
        << ", " << static_cast<int>(last_write_stream_) << ", "
        << write_streams_count_;
    // Always need end fence for writes.
    need_end_fence = true;
  }

  return true;
}

void SharedImageBackingOzone::EndAccess(bool readonly,
                                        AccessStream access_stream,
                                        gfx::GpuFenceHandle fence) {
  if (readonly) {
    DCHECK_GT(reads_in_progress_, 0u);
    --reads_in_progress_;
  } else {
    DCHECK(is_write_in_progress_);
    is_write_in_progress_ = false;
  }

  if (readonly) {
    if (!fence.is_null()) {
      read_fences_[access_stream] = std::move(fence);
    }
  } else {
    DCHECK(read_fences_.find(access_stream) == read_fences_.end());
    write_fence_ = std::move(fence);
    last_write_stream_ = access_stream;
  }
}

}  // namespace gpu
