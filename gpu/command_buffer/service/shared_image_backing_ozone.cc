// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_ozone.h"

#include <dawn/webgpu.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
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
  SharedImageRepresentationOverlayOzone(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gl::GLImageNativePixmap> image)
      : SharedImageRepresentationOverlay(manager, backing, tracker),
        gl_image_(image) {}
  ~SharedImageRepresentationOverlayOzone() override = default;

 private:
  bool BeginReadAccess(std::vector<gfx::GpuFence>* acquire_fences) override {
    auto* ozone_backing = static_cast<SharedImageBackingOzone*>(backing());
    std::vector<gfx::GpuFenceHandle> fences;
    ozone_backing->BeginAccess(&fences);
    for (auto& fence : fences) {
      acquire_fences->emplace_back(std::move(fence));
    }
    return true;
  }
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override {
    auto* ozone_backing = static_cast<SharedImageBackingOzone*>(backing());
    ozone_backing->EndAccess(true, std::move(release_fence));
  }
  gl::GLImage* GetGLImage() override { return gl_image_.get(); }

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
  return SharedImageRepresentationGLTextureOzone::Create(manager, this, tracker,
                                                         pixmap_, format());
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageBackingOzone::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  return SharedImageRepresentationGLTexturePassthroughOzone::Create(
      manager, this, tracker, pixmap_, format());
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
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationOverlay>
SharedImageBackingOzone::ProduceOverlay(SharedImageManager* manager,
                                        MemoryTypeTracker* tracker) {
  gfx::BufferFormat buffer_format = viz::BufferFormat(format());
  auto image = base::MakeRefCounted<gl::GLImageNativePixmap>(
      pixmap_->GetBufferSize(), buffer_format);
  image->Initialize(std::move(pixmap_));
  return std::make_unique<SharedImageRepresentationOverlayOzone>(
      manager, this, tracker, image);
}

SharedImageBackingOzone::SharedImageBackingOzone(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
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
      pixmap_(std::move(pixmap)),
      dawn_procs_(std::move(dawn_procs)),
      context_state_(std::move(context_state)) {}

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

bool SharedImageBackingOzone::NeedsSynchronization() const {
  return (usage() & SHARED_IMAGE_USAGE_WEBGPU) ||
         (usage() & SHARED_IMAGE_USAGE_SCANOUT);
}

void SharedImageBackingOzone::BeginAccess(
    std::vector<gfx::GpuFenceHandle>* fences) {
  if (NeedsSynchronization()) {
    // Technically, we don't need to wait on other read fences when performing
    // a read access, but like in the case of |ExternalVkImageBacking|, reading
    // repeatedly without a write access will cause us to run out of FDs.
    // TODO(penghuang): Avoid waiting on read semaphores.
    *fences = std::move(read_fences_);
    read_fences_.clear();
    if (!write_fence_.is_null()) {
      fences->push_back(std::move(write_fence_));
      write_fence_ = gfx::GpuFenceHandle();
    }
  }
}

void SharedImageBackingOzone::EndAccess(bool readonly,
                                        gfx::GpuFenceHandle fence) {
  if (NeedsSynchronization() && !fence.is_null()) {
    if (readonly) {
      read_fences_.push_back(std::move(fence));
    } else {
      DCHECK(write_fence_.is_null());
      DCHECK(read_fences_.empty());
      write_fence_ = std::move(fence);
    }
  }
}

}  // namespace gpu
