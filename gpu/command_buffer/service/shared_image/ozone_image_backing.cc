// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/ozone_image_backing.h"

#include <dawn/webgpu.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/gl_ozone_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/skia_gl_image_representation.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "gpu/command_buffer/service/skia_utils.h"
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

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/command_buffer/service/shared_image/skia_vk_ozone_image_representation.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif  // BUILDFLAG(ENABLE_VULKAN)

#if BUILDFLAG(USE_DAWN)
#include "gpu/command_buffer/service/shared_image/dawn_ozone_image_representation.h"
#endif  // BUILDFLAG(USE_DAWN)

namespace gpu {
namespace {

size_t GetPixmapSizeInBytes(const gfx::NativePixmap& pixmap) {
  return gfx::BufferSizeForBufferFormat(pixmap.GetBufferSize(),
                                        pixmap.GetBufferFormat());
}

}  // namespace

class OzoneImageBacking::VaapiOzoneImageRepresentation
    : public VaapiImageRepresentation {
 public:
  VaapiOzoneImageRepresentation(SharedImageManager* manager,
                                SharedImageBacking* backing,
                                MemoryTypeTracker* tracker,
                                VaapiDependencies* vaapi_dependency)
      : VaapiImageRepresentation(manager, backing, tracker, vaapi_dependency) {}

 private:
  OzoneImageBacking* ozone_backing() {
    return static_cast<OzoneImageBacking*>(backing());
  }
  void EndAccess() override { ozone_backing()->has_pending_va_writes_ = true; }
  void BeginAccess() override {
    // TODO(andrescj): DCHECK that there are no fences to wait on (because the
    // compositor should be completely done with a VideoFrame before returning
    // it).
  }
};

class OzoneImageBacking::OverlayOzoneImageRepresentation
    : public OverlayImageRepresentation {
 public:
  OverlayOzoneImageRepresentation(SharedImageManager* manager,
                                  SharedImageBacking* backing,
                                  MemoryTypeTracker* tracker)
      : OverlayImageRepresentation(manager, backing, tracker) {}
  ~OverlayOzoneImageRepresentation() override = default;

 private:
  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) override {
    auto* ozone_backing = static_cast<OzoneImageBacking*>(backing());
    std::vector<gfx::GpuFenceHandle> fences;
    bool need_end_fence;
    if (!ozone_backing->BeginAccess(/*readonly=*/true, AccessStream::kOverlay,
                                    &fences, need_end_fence)) {
      return false;
    }
    // Always need an end fence when finish reading from overlays.
    DCHECK(need_end_fence);
    if (!fences.empty()) {
      DCHECK(fences.size() == 1);
      acquire_fence = std::move(fences.front());
    }
    return true;
  }
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override {
    auto* ozone_backing = static_cast<OzoneImageBacking*>(backing());
    ozone_backing->EndAccess(/*readonly=*/true, AccessStream::kOverlay,
                             std::move(release_fence));
  }
};

SharedImageBackingType OzoneImageBacking::GetType() const {
  return SharedImageBackingType::kOzone;
}

void OzoneImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  if (in_fence) {
    external_write_fence_ = in_fence->GetGpuFenceHandle().Clone();
  }
}

scoped_refptr<gfx::NativePixmap> OzoneImageBacking::GetNativePixmap() {
  return pixmap_;
}

std::unique_ptr<DawnImageRepresentation> OzoneImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    WGPUBackendType backend_type,
    std::vector<WGPUTextureFormat> view_formats) {
#if BUILDFLAG(USE_DAWN)
  DCHECK(dawn_procs_);
  WGPUTextureFormat webgpu_format = ToWGPUFormat(format());
  if (webgpu_format == WGPUTextureFormat_Undefined) {
    return nullptr;
  }
  return std::make_unique<DawnOzoneImageRepresentation>(
      manager, this, tracker, device, webgpu_format, std::move(view_formats),
      pixmap_, dawn_procs_);
#else  // !BUILDFLAG(USE_DAWN)
  return nullptr;
#endif
}

std::unique_ptr<GLTextureImageRepresentation>
OzoneImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                    MemoryTypeTracker* tracker) {
  if (!cached_texture_holders_.empty() &&
      cached_texture_holders_[0]->WasContextLost()) {
    cached_texture_holders_.clear();
  }

  const bool need_cache = workarounds_.cache_texture_in_ozone_backing;
  return GLTextureOzoneImageRepresentation::Create(
      manager, this, tracker, pixmap_, plane_,
      need_cache ? &cached_texture_holders_ : nullptr);
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
OzoneImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                               MemoryTypeTracker* tracker) {
  if (!cached_texture_holders_.empty() &&
      cached_texture_holders_[0]->WasContextLost()) {
    cached_texture_holders_.clear();
  }

  const bool need_cache = workarounds_.cache_texture_in_ozone_backing;
  return GLTexturePassthroughOzoneImageRepresentation::Create(
      manager, this, tracker, pixmap_, plane_,
      need_cache ? &cached_texture_holders_ : nullptr);
}

std::unique_ptr<SkiaGaneshImageRepresentation>
OzoneImageBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (context_state->GrContextIsGL()) {
    std::unique_ptr<GLTextureImageRepresentationBase> gl_representation;
    if (use_passthrough_) {
      gl_representation = ProduceGLTexturePassthrough(manager, tracker);
    } else {
      gl_representation = ProduceGLTexture(manager, tracker);
    }
    if (!gl_representation) {
      LOG(ERROR) << "OzoneImageBacking::ProduceSkiaGanesh failed to create GL "
                    "representation";
      return nullptr;
    }
    auto skia_representation = SkiaGLImageRepresentation::Create(
        std::move(gl_representation), std::move(context_state), manager, this,
        tracker);
    if (!skia_representation) {
      LOG(ERROR) << "OzoneImageBacking::ProduceSkiaGanesh failed to create "
                    "Skia representation";
      return nullptr;
    }
    return skia_representation;
  }
  if (context_state->GrContextIsVulkan()) {
#if BUILDFLAG(ENABLE_VULKAN)
    auto* device_queue = context_state->vk_context_provider()->GetDeviceQueue();
    gfx::GpuMemoryBufferHandle gmb_handle;
    gmb_handle.type = gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
    gmb_handle.native_pixmap_handle = pixmap_->ExportHandle();
    auto* vulkan_implementation =
        context_state->vk_context_provider()->GetVulkanImplementation();
    auto vulkan_image = vulkan_implementation->CreateImageFromGpuMemoryHandle(
        device_queue, std::move(gmb_handle), size(), ToVkFormat(format()),
        gfx::ColorSpace());

    if (!vulkan_image)
      return nullptr;

    return std::make_unique<SkiaVkOzoneImageRepresentation>(
        manager, this, std::move(context_state), std::move(vulkan_image),
        tracker);
#else
    NOTREACHED() << "Vulkan is disabled.";
    return nullptr;
#endif  // BUILDFLAG(ENABLE_VULKAN)
  }
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<OverlayImageRepresentation> OzoneImageBacking::ProduceOverlay(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  return std::make_unique<OverlayOzoneImageRepresentation>(manager, this,
                                                           tracker);
}

OzoneImageBacking::OzoneImageBacking(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    gfx::BufferPlane plane,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    scoped_refptr<SharedContextState> context_state,
    scoped_refptr<gfx::NativePixmap> pixmap,
    scoped_refptr<base::RefCountedData<DawnProcTable>> dawn_procs,
    const GpuDriverBugWorkarounds& workarounds,
    bool use_passthrough)
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
      context_state_(std::move(context_state)),
      workarounds_(workarounds),
      use_passthrough_(use_passthrough) {
  bool used_by_skia = (usage & SHARED_IMAGE_USAGE_RASTER) ||
                      (usage & SHARED_IMAGE_USAGE_DISPLAY_READ);
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

OzoneImageBacking::~OzoneImageBacking() {
  if (context_state_->context_lost()) {
    for (auto& texture_holder : cached_texture_holders_) {
      texture_holder->MarkContextLost();
    }
  }
}

std::unique_ptr<VaapiImageRepresentation> OzoneImageBacking::ProduceVASurface(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    VaapiDependenciesFactory* dep_factory) {
  DCHECK(pixmap_);
  if (!vaapi_deps_)
    vaapi_deps_ = dep_factory->CreateVaapiDependencies(pixmap_);

  if (!vaapi_deps_) {
    LOG(ERROR) << "OzoneImageBacking::ProduceVASurface failed to create "
                  "VaapiDependencies";
    return nullptr;
  }
  return std::make_unique<OzoneImageBacking::VaapiOzoneImageRepresentation>(
      manager, this, tracker, vaapi_deps_.get());
}

bool OzoneImageBacking::VaSync() {
  if (has_pending_va_writes_)
    has_pending_va_writes_ = !vaapi_deps_->SyncSurface();
  return !has_pending_va_writes_;
}

bool OzoneImageBacking::UploadFromMemory(const std::vector<SkPixmap>& pixmaps) {
  if (context_state_->context_lost()) {
    return false;
  }

  DCHECK(context_state_->IsCurrent(nullptr));

  auto representation = ProduceSkiaGanesh(
      nullptr, context_state_->memory_type_tracker(), context_state_);
  DCHECK_EQ(pixmaps.size(), representation->NumPlanesExpected());

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
    bool result = context_state_->gr_context()->wait(
        begin_semaphores.size(), begin_semaphores.data(),
        /*deleteSemaphoresAfterWait=*/false);
    DCHECK(result);
  }

  bool written = true;
  for (int plane = 0; plane < format().NumberOfPlanes(); ++plane) {
    GrBackendTexture backend_texture =
        dest_scoped_access->promise_image_texture(plane)->backendTexture();
    if (!context_state_->gr_context()->updateBackendTexture(
            backend_texture, &pixmaps[plane],
            /*numLevels=*/1, surface_origin(), nullptr, nullptr)) {
      written = false;
    }
  }

  dest_scoped_access->ApplyBackendSurfaceEndState();
  FlushAndSubmitIfNecessary(std::move(end_semaphores), context_state_.get());
  if (written && !IsCleared()) {
    SetCleared();
  }
  return written;
}

void OzoneImageBacking::FlushAndSubmitIfNecessary(
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

bool OzoneImageBacking::BeginAccess(bool readonly,
                                    AccessStream access_stream,
                                    std::vector<gfx::GpuFenceHandle>* fences,
                                    bool& need_end_fence) {
  // Track reads and writes if not being used for concurrent read/writes.
  if (!(usage() & SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE)) {
    if (is_write_in_progress_) {
      DLOG(ERROR) << "Unable to begin read or write access because another "
                     "write access is in progress";
      return false;
    }

    if (reads_in_progress_ && !readonly) {
      DLOG(ERROR) << "Unable to begin write access because a read access is in "
                     "progress ";
      return false;
    }

    if (readonly) {
      ++reads_in_progress_;
    } else {
      is_write_in_progress_ = true;
    }
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

  // Always wait on an `external_write_fence_` if present.
  if (!external_write_fence_.is_null()) {
    DCHECK(write_fence_.is_null());  // `write_fence_` should be null.
    // For write access we expect new `write_fence_` so we can move the
    // old fence here.
    if (!readonly)
      fences->emplace_back(std::move(external_write_fence_));
    else
      fences->emplace_back(external_write_fence_.Clone());
  }

  // If current stream is different than `last_write_stream_` then wait on that
  // stream's `write_fence_`.
  if (!write_fence_.is_null() && last_write_stream_ != access_stream) {
    DCHECK(external_write_fence_
               .is_null());  // `external_write_fence_` should be null.
    // For write access we expect new `write_fence_` so we can move the old
    // fence here.
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
    // Always need end fence for multiple write streams. For single write stream
    // need an end fence for all usages except for raster using delegated
    // compositing. If the image will be used for delegated compositing, no need
    // to put fences at this moment as there are many raster tasks in the CPU gl
    // context that end up creating a big number of fences, which may have some
    // performance overhead depending on the gpu. Instead, when these images
    // will be scheduled as overlays, a single fence will be created.
    // TODO(crbug.com/1254033): this block of code shall be removed after cc is
    // able to set a single (duplicated) fence for bunch of tiles instead of
    // having the SI framework creating fences for each single message when
    // write access ends.
    need_end_fence =
        (write_streams_count_ > 1) ||
        !(usage() & SHARED_IMAGE_USAGE_RASTER_DELEGATED_COMPOSITING);
  }

  return true;
}

void OzoneImageBacking::EndAccess(bool readonly,
                                  AccessStream access_stream,
                                  gfx::GpuFenceHandle fence) {
  // Track reads and writes if not being used for concurrent read/writes.
  if (!(usage() & SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE)) {
    if (readonly) {
      DCHECK_GT(reads_in_progress_, 0u);
      --reads_in_progress_;
    } else {
      DCHECK(is_write_in_progress_);
      is_write_in_progress_ = false;
    }
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
