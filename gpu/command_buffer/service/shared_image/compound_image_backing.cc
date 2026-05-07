// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/compound_image_backing.h"

#include <ostream>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/process_memory_dump.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_info.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_copy_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_memory_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_memory_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "third_party/skia/include/gpu/graphite/BackendTexture.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/win/d3d_shared_fence.h"
#endif

namespace gpu {
namespace {

constexpr AccessStreamSet kMemoryStreamSet = {SharedImageAccessStream::kMemory};

// Unique GUIDs for child backings.
base::trace_event::MemoryAllocatorDumpGuid GetSubBackingGUIDForTracing(
    const Mailbox& mailbox,
    int backing_index) {
  return base::trace_event::MemoryAllocatorDumpGuid(
      base::StringPrintf("gpu-shared-image/%s/sub-backing/%d",
                         mailbox.ToDebugString().c_str(), backing_index));
}

// LINT.IfChange(ContentSyncReason)
enum class ContentSyncReason { kRead = 0, kWrite = 1, kMaxValue = kWrite };
// LINT.ThenChange(//tools/metrics/histograms/metadata/gpu/enums.xml:ContentSyncReason)

#if BUILDFLAG(IS_WIN)
// Only allow shmem overlays for NV12 on Windows.
// This moves the SCANOUT flag from the GPU backing to the shmem backing in the
// CompoundImageBacking.
constexpr bool kAllowShmOverlays = true;
#else
constexpr bool kAllowShmOverlays = false;
#endif

gpu::SharedImageUsageSet GetShmSharedImageUsage(SharedImageUsageSet usage) {
  gpu::SharedImageUsageSet new_usage = SHARED_IMAGE_USAGE_CPU_WRITE_ONLY;
  if (usage.Has(SHARED_IMAGE_USAGE_CPU_READ)) {
    new_usage |= SHARED_IMAGE_USAGE_CPU_READ;
  }
  if (kAllowShmOverlays && usage.Has(gpu::SHARED_IMAGE_USAGE_SCANOUT)) {
    new_usage |= SHARED_IMAGE_USAGE_SCANOUT;
  }
  return new_usage;
}

// This might need further tweaking in order to be able to choose appropriate
// backing. Note that the backing usually doesnt know at this point if the
// access stream will be used for read or write.
SharedImageUsageSet GetUsageFromAccessStream(SharedImageAccessStream stream) {
  switch (stream) {
    case SharedImageAccessStream::kGL:
      return SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_GLES2_WRITE;
    case SharedImageAccessStream::kSkia:
      return SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE |
             SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE;
    case SharedImageAccessStream::kDawn:
      return SHARED_IMAGE_USAGE_WEBGPU_READ | SHARED_IMAGE_USAGE_WEBGPU_WRITE;
    case SharedImageAccessStream::kDawnBuffer:
      return SHARED_IMAGE_USAGE_WEBGPU_READ | SHARED_IMAGE_USAGE_WEBGPU_WRITE;
    case SharedImageAccessStream::kOverlay:
      return SHARED_IMAGE_USAGE_SCANOUT;
    case SharedImageAccessStream::kVaapi:
      return SHARED_IMAGE_USAGE_VIDEO_DECODE;
    case SharedImageAccessStream::kWebNNTensor:
      // Note that SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR is the main usage, we
      // always need it for WebNN, the two other(*_TENSOR_READ/WRITE) are for
      // additional functionality in webnn (upload/readback of the tensor).
      return SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR;
    case SharedImageAccessStream::kVulkan:
      return SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE |
             SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE;
    case SharedImageAccessStream::kMemory:
      // Below usage set ensures that only SharedMemoryImageBacking will be able
      // to support this stream.
      return SHARED_IMAGE_USAGE_CPU_WRITE_ONLY | SHARED_IMAGE_USAGE_CPU_READ |
             SHARED_IMAGE_USAGE_RASTER_COPY_SOURCE;
    default:
      NOTREACHED();
  }
}

}  // namespace

// Wrapped representation types are not in the anonymous namespace because they
// need to be friend classes to real representations to access protected
// virtual functions.
class WrappedGLTextureCompoundImageRepresentation
    : public GLTextureImageRepresentation {
 public:
  WrappedGLTextureCompoundImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<GLTextureImageRepresentation> wrapped,
      std::unique_ptr<SharedImageBacking> owned_backing)
      : GLTextureImageRepresentation(manager, backing, tracker),
        owned_backing_(std::move(owned_backing)),
        wrapped_(std::move(wrapped)) {
    DCHECK(wrapped_);
  }

  CompoundImageBacking* compound_backing() {
    return static_cast<CompoundImageBacking*>(backing());
  }

  // GLTextureImageRepresentation implementation.
  bool BeginAccess(GLenum mode) final {
    AccessMode access_mode =
        mode == kReadAccessMode ? AccessMode::kRead : AccessMode::kWrite;
    compound_backing()->NotifyBeginAccess(wrapped_->backing(), access_mode,
                                          SharedImageAccessStream::kGL);
    access_mode_ = access_mode;
    return wrapped_->BeginAccess(mode);
  }

  void EndAccess() override {
    wrapped_->EndAccess();
    compound_backing()->NotifyEndAccess(wrapped_->backing(), access_mode_);
  }

  gpu::TextureBase* GetTextureBase(size_t plane_index) final {
    return wrapped_->GetTextureBase(plane_index);
  }

  bool SupportsMultipleConcurrentReadAccess() final {
    return wrapped_->SupportsMultipleConcurrentReadAccess();
  }

  gles2::Texture* GetTexture(size_t plane_index) final {
    return wrapped_->GetTexture(plane_index);
  }

  void SetClearedRect(const gfx::Rect& cleared_rect) override {
    SharedImageRepresentation::SetClearedRect(cleared_rect);
    if (owned_backing_) {
      wrapped_->SetClearedRect(cleared_rect);
    }
  }

 private:
  std::unique_ptr<SharedImageBacking> owned_backing_;
  std::unique_ptr<GLTextureImageRepresentation> wrapped_;
  AccessMode access_mode_ = AccessMode::kNone;
};

class WrappedGLTexturePassthroughCompoundImageRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  WrappedGLTexturePassthroughCompoundImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<GLTexturePassthroughImageRepresentation> wrapped,
      std::unique_ptr<SharedImageBacking> owned_backing)
      : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
        owned_backing_(std::move(owned_backing)),
        wrapped_(std::move(wrapped)) {
    DCHECK(wrapped_);
  }

  CompoundImageBacking* compound_backing() {
    return static_cast<CompoundImageBacking*>(backing());
  }

  // GLTexturePassthroughImageRepresentation implementation.
  bool BeginAccess(GLenum mode) override {
    AccessMode access_mode =
        mode == kReadAccessMode ? AccessMode::kRead : AccessMode::kWrite;
    compound_backing()->NotifyBeginAccess(wrapped_->backing(), access_mode,
                                          SharedImageAccessStream::kGL);
    access_mode_ = access_mode;
    return wrapped_->BeginAccess(mode);
  }
  void EndAccess() override {
    wrapped_->EndAccess();
    compound_backing()->NotifyEndAccess(wrapped_->backing(), access_mode_);
  }

  gpu::TextureBase* GetTextureBase(size_t plane_index) final {
    return wrapped_->GetTextureBase(plane_index);
  }

  bool SupportsMultipleConcurrentReadAccess() final {
    return wrapped_->SupportsMultipleConcurrentReadAccess();
  }

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      size_t plane_index) final {
    return wrapped_->GetTexturePassthrough(plane_index);
  }

  void SetClearedRect(const gfx::Rect& cleared_rect) override {
    SharedImageRepresentation::SetClearedRect(cleared_rect);
    if (owned_backing_) {
      wrapped_->SetClearedRect(cleared_rect);
    }
  }

 private:
  std::unique_ptr<SharedImageBacking> owned_backing_;
  std::unique_ptr<GLTexturePassthroughImageRepresentation> wrapped_;
  AccessMode access_mode_ = AccessMode::kNone;
};

class WrappedSkiaGaneshCompoundImageRepresentation
    : public SkiaGaneshImageRepresentation {
 public:
  WrappedSkiaGaneshCompoundImageRepresentation(
      GrDirectContext* gr_context,
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<SkiaGaneshImageRepresentation> wrapped,
      std::unique_ptr<SharedImageBacking> owned_backing)
      : SkiaGaneshImageRepresentation(gr_context, manager, backing, tracker),
        owned_backing_(std::move(owned_backing)),
        wrapped_(std::move(wrapped)) {
    DCHECK(wrapped_);
  }

  CompoundImageBacking* compound_backing() {
    return static_cast<CompoundImageBacking*>(backing());
  }

  // SkiaImageRepresentation implementation.
  bool SupportsMultipleConcurrentReadAccess() final {
    return wrapped_->SupportsMultipleConcurrentReadAccess();
  }

  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) final {
    compound_backing()->NotifyBeginAccess(wrapped_->backing(),
                                          AccessMode::kWrite,
                                          SharedImageAccessStream::kSkia);
    return wrapped_->BeginWriteAccess(final_msaa_count, surface_props,
                                      update_rect, begin_semaphores,
                                      end_semaphores, end_state);
  }
  std::vector<sk_sp<GrPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) final {
    compound_backing()->NotifyBeginAccess(wrapped_->backing(),
                                          AccessMode::kWrite,
                                          SharedImageAccessStream::kSkia);
    return wrapped_->BeginWriteAccess(begin_semaphores, end_semaphores,
                                      end_state);
  }
  void EndWriteAccess() final {
    wrapped_->EndWriteAccess();
    compound_backing()->NotifyEndAccess(wrapped_->backing(),
                                        AccessMode::kWrite);
  }

  std::vector<sk_sp<GrPromiseImageTexture>> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) final {
    compound_backing()->NotifyBeginAccess(
        wrapped_->backing(), AccessMode::kRead, SharedImageAccessStream::kSkia);
    return wrapped_->BeginReadAccess(begin_semaphores, end_semaphores,
                                     end_state);
  }
  void EndReadAccess() final {
    wrapped_->EndReadAccess();
    compound_backing()->NotifyEndAccess(wrapped_->backing(), AccessMode::kRead);
  }

  void SetClearedRect(const gfx::Rect& cleared_rect) override {
    SharedImageRepresentation::SetClearedRect(cleared_rect);
    if (owned_backing_) {
      wrapped_->SetClearedRect(cleared_rect);
    }
  }

 private:
  std::unique_ptr<SharedImageBacking> owned_backing_;
  std::unique_ptr<SkiaGaneshImageRepresentation> wrapped_;
};

class WrappedSkiaGraphiteCompoundImageRepresentation
    : public SkiaGraphiteImageRepresentation {
 public:
  WrappedSkiaGraphiteCompoundImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<SkiaGraphiteImageRepresentation> wrapped,
      std::unique_ptr<SharedImageBacking> owned_backing)
      : SkiaGraphiteImageRepresentation(manager, backing, tracker),
        owned_backing_(std::move(owned_backing)),
        wrapped_(std::move(wrapped)) {
    CHECK(wrapped_);
  }

  CompoundImageBacking* compound_backing() {
    return static_cast<CompoundImageBacking*>(backing());
  }

  // SkiaGraphiteImageRepresentation implementation.
  bool SupportsMultipleConcurrentReadAccess() final {
    return wrapped_->SupportsMultipleConcurrentReadAccess();
  }
  bool SupportsDeferredGraphiteSubmit() final {
    return wrapped_->SupportsDeferredGraphiteSubmit();
  }

  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect) final {
    compound_backing()->NotifyBeginAccess(wrapped_->backing(),
                                          AccessMode::kWrite,
                                          SharedImageAccessStream::kSkia);
    return wrapped_->BeginWriteAccess(surface_props, update_rect);
  }
  std::vector<scoped_refptr<GraphiteTextureHolder>> BeginWriteAccess() final {
    compound_backing()->NotifyBeginAccess(wrapped_->backing(),
                                          AccessMode::kWrite,
                                          SharedImageAccessStream::kSkia);
    return wrapped_->BeginWriteAccess();
  }
  void EndWriteAccess() final {
    wrapped_->EndWriteAccess();
    compound_backing()->NotifyEndAccess(wrapped_->backing(),
                                        AccessMode::kWrite);
  }

  std::vector<scoped_refptr<GraphiteTextureHolder>> BeginReadAccess() final {
    compound_backing()->NotifyBeginAccess(
        wrapped_->backing(), AccessMode::kRead, SharedImageAccessStream::kSkia);
    return wrapped_->BeginReadAccess();
  }
  void EndReadAccess() final {
    wrapped_->EndReadAccess();
    compound_backing()->NotifyEndAccess(wrapped_->backing(), AccessMode::kRead);
  }

  void SetClearedRect(const gfx::Rect& cleared_rect) override {
    SharedImageRepresentation::SetClearedRect(cleared_rect);
    if (owned_backing_) {
      wrapped_->SetClearedRect(cleared_rect);
    }
  }

 private:
  std::unique_ptr<SharedImageBacking> owned_backing_;
  std::unique_ptr<SkiaGraphiteImageRepresentation> wrapped_;
};

class WrappedDawnCompoundImageRepresentation : public DawnImageRepresentation {
 public:
  WrappedDawnCompoundImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<DawnImageRepresentation> wrapped,
      std::unique_ptr<SharedImageBacking> owned_backing)
      : DawnImageRepresentation(manager, backing, tracker),
        owned_backing_(std::move(owned_backing)),
        wrapped_(std::move(wrapped)) {
    DCHECK(wrapped_);
  }

  CompoundImageBacking* compound_backing() {
    return static_cast<CompoundImageBacking*>(backing());
  }

  // DawnImageRepresentation implementation.
  wgpu::Texture BeginAccess(wgpu::TextureUsage webgpu_usage,
                            wgpu::TextureUsage internal_usage) override {
    AccessMode access_mode =
        webgpu_usage & kWriteUsage ? AccessMode::kWrite : AccessMode::kRead;
    if (internal_usage & kWriteUsage) {
      access_mode = AccessMode::kWrite;
    }
    compound_backing()->NotifyBeginAccess(wrapped_->backing(), access_mode,
                                          SharedImageAccessStream::kDawn);
    access_mode_ = access_mode;
    return wrapped_->BeginAccess(webgpu_usage, internal_usage);
  }
  void EndAccess() override {
    wrapped_->EndAccess();
    compound_backing()->NotifyEndAccess(wrapped_->backing(), access_mode_);
  }

  void SetClearedRect(const gfx::Rect& cleared_rect) override {
    SharedImageRepresentation::SetClearedRect(cleared_rect);
    if (owned_backing_) {
      wrapped_->SetClearedRect(cleared_rect);
    }
  }

 private:
  std::unique_ptr<SharedImageBacking> owned_backing_;
  std::unique_ptr<DawnImageRepresentation> wrapped_;
  AccessMode access_mode_ = AccessMode::kNone;
};

class WrappedDawnBufferCompoundImageRepresentation
    : public DawnBufferRepresentation {
 public:
  WrappedDawnBufferCompoundImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<DawnBufferRepresentation> wrapped,
      std::unique_ptr<SharedImageBacking> owned_backing)
      : DawnBufferRepresentation(manager, backing, tracker),
        owned_backing_(std::move(owned_backing)),
        wrapped_(std::move(wrapped)) {
    DCHECK(wrapped_);
  }

  void SetClearedRect(const gfx::Rect& cleared_rect) override {
    SharedImageRepresentation::SetClearedRect(cleared_rect);
    if (owned_backing_) {
      wrapped_->SetClearedRect(cleared_rect);
    }
  }

 private:
  CompoundImageBacking* compound_backing() {
    return static_cast<CompoundImageBacking*>(backing());
  }

  wgpu::Buffer BeginAccess(wgpu::BufferUsage usage) override {
    AccessMode access_mode = usage & wgpu::BufferUsage::MapWrite
                                 ? AccessMode::kWrite
                                 : AccessMode::kRead;
    compound_backing()->NotifyBeginAccess(wrapped_->backing(), access_mode,
                                          SharedImageAccessStream::kDawnBuffer);
    access_mode_ = access_mode;
    return wrapped_->BeginAccess(usage);
  }

  void EndAccess() override {
    wrapped_->EndAccess();
    compound_backing()->NotifyEndAccess(wrapped_->backing(), access_mode_);
  }

  std::unique_ptr<SharedImageBacking> owned_backing_;
  std::unique_ptr<DawnBufferRepresentation> wrapped_;
  AccessMode access_mode_ = AccessMode::kNone;
};

class WrappedOverlayCompoundImageRepresentation
    : public OverlayImageRepresentation {
 public:
  WrappedOverlayCompoundImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<OverlayImageRepresentation> wrapped,
      std::unique_ptr<SharedImageBacking> owned_backing)
      : OverlayImageRepresentation(manager, backing, tracker),
        owned_backing_(std::move(owned_backing)),
        wrapped_(std::move(wrapped)) {
    DCHECK(wrapped_);
  }

  CompoundImageBacking* compound_backing() {
    return static_cast<CompoundImageBacking*>(backing());
  }

  // OverlayImageRepresentation implementation.
  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) final {
    compound_backing()->NotifyBeginAccess(wrapped_->backing(),
                                          AccessMode::kRead,
                                          SharedImageAccessStream::kOverlay);

    return wrapped_->BeginReadAccess(acquire_fence);
  }
  void EndReadAccess(gfx::GpuFenceHandle release_fence) final {
    wrapped_->EndReadAccess(std::move(release_fence));
    compound_backing()->NotifyEndAccess(wrapped_->backing(), AccessMode::kRead);
  }
#if BUILDFLAG(IS_ANDROID)
  AHardwareBuffer* GetAHardwareBuffer() final {
    return wrapped_->GetAHardwareBuffer();
  }
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBufferFenceSync() final {
    return wrapped_->GetAHardwareBufferFenceSync();
  }
#elif BUILDFLAG(IS_WIN)
  std::optional<gl::DCLayerOverlayImage> GetDCLayerOverlayImage() final {
    return wrapped_->GetDCLayerOverlayImage();
  }
#elif BUILDFLAG(IS_APPLE)
  gfx::ScopedIOSurface GetIOSurface() const final {
    return wrapped_->GetIOSurface();
  }
  bool IsInUseByWindowServer() const final {
    return wrapped_->IsInUseByWindowServer();
  }
#endif

  void SetClearedRect(const gfx::Rect& cleared_rect) override {
    SharedImageRepresentation::SetClearedRect(cleared_rect);
    if (owned_backing_) {
      wrapped_->SetClearedRect(cleared_rect);
    }
  }

 private:
  std::unique_ptr<SharedImageBacking> owned_backing_;
  std::unique_ptr<OverlayImageRepresentation> wrapped_;
};

class WrappedWebNNTensorCompoundImageRepresentation
    : public WebNNTensorRepresentation {
 public:
  WrappedWebNNTensorCompoundImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<WebNNTensorRepresentation> wrapped,
      std::unique_ptr<SharedImageBacking> owned_backing)
      : WebNNTensorRepresentation(manager, backing, tracker),
        owned_backing_(std::move(owned_backing)),
        wrapped_(std::move(wrapped)) {
    DCHECK(wrapped_);
  }

#if BUILDFLAG(IS_WIN)
  scoped_refptr<gfx::D3DSharedFence> GetAcquireFence() const final {
    return wrapped_->GetAcquireFence();
  }

  void SetReleaseFence(scoped_refptr<gfx::D3DSharedFence> release_fence) final {
    wrapped_->SetReleaseFence(std::move(release_fence));
  }

  Microsoft::WRL::ComPtr<ID3D12Resource> GetD3D12Buffer() const final {
    return wrapped_->GetD3D12Buffer();
  }
#endif

#if BUILDFLAG(IS_APPLE)
  IOSurfaceRef GetIOSurface() const final { return wrapped_->GetIOSurface(); }
#endif

  void SetClearedRect(const gfx::Rect& cleared_rect) override {
    SharedImageRepresentation::SetClearedRect(cleared_rect);
    if (owned_backing_) {
      wrapped_->SetClearedRect(cleared_rect);
    }
  }

 private:
  CompoundImageBacking* compound_backing() {
    return static_cast<CompoundImageBacking*>(backing());
  }

  bool BeginAccess() override {
    compound_backing()->NotifyBeginAccess(
        wrapped_->backing(), AccessMode::kWrite,
        SharedImageAccessStream::kWebNNTensor);
    return wrapped_->BeginAccess();
  }

  void EndAccess() final {
    wrapped_->EndAccess();
    compound_backing()->NotifyEndAccess(wrapped_->backing(),
                                        AccessMode::kWrite);
  }

  std::unique_ptr<SharedImageBacking> owned_backing_;
  std::unique_ptr<WebNNTensorRepresentation> wrapped_;
};

class WrappedMemoryCompoundImageRepresentation
    : public MemoryImageRepresentation {
 public:
  WrappedMemoryCompoundImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<MemoryImageRepresentation> wrapped,
      std::unique_ptr<SharedImageBacking> owned_backing)
      : MemoryImageRepresentation(manager, backing, tracker),
        owned_backing_(std::move(owned_backing)),
        wrapped_(std::move(wrapped)) {
    CHECK(wrapped_);
  }

  CompoundImageBacking* compound_backing() {
    return static_cast<CompoundImageBacking*>(backing());
  }

  SkPixmap BeginReadAccess() override {
    compound_backing()->NotifyBeginAccess(wrapped_->backing(),
                                          AccessMode::kRead,
                                          SharedImageAccessStream::kMemory);
    return wrapped_->BeginReadAccess();
  }

  void SetClearedRect(const gfx::Rect& cleared_rect) override {
    SharedImageRepresentation::SetClearedRect(cleared_rect);
    if (owned_backing_) {
      wrapped_->SetClearedRect(cleared_rect);
    }
  }

 private:
  std::unique_ptr<SharedImageBacking> owned_backing_;
  std::unique_ptr<MemoryImageRepresentation> wrapped_;
};

class WrappedVideoCompoundImageRepresentation
    : public VideoImageRepresentation {
 public:
  WrappedVideoCompoundImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<VideoImageRepresentation> wrapped,
      std::unique_ptr<SharedImageBacking> owned_backing)
      : VideoImageRepresentation(manager, backing, tracker),
        owned_backing_(std::move(owned_backing)),
        wrapped_(std::move(wrapped)) {
    CHECK(wrapped_);
  }

  CompoundImageBacking* compound_backing() {
    return static_cast<CompoundImageBacking*>(backing());
  }

  bool BeginWriteAccess() override {
    compound_backing()->NotifyBeginAccess(wrapped_->backing(),
                                          AccessMode::kWrite,
                                          SharedImageAccessStream::kVaapi);
    return wrapped_->BeginWriteAccess();
  }
  void EndWriteAccess() override {
    wrapped_->EndWriteAccess();
    compound_backing()->NotifyEndAccess(wrapped_->backing(),
                                        AccessMode::kWrite);
  }
  bool BeginReadAccess() override {
    compound_backing()->NotifyBeginAccess(wrapped_->backing(),
                                          AccessMode::kRead,
                                          SharedImageAccessStream::kVaapi);
    return wrapped_->BeginReadAccess();
  }
  void EndReadAccess() override {
    wrapped_->EndReadAccess();
    compound_backing()->NotifyEndAccess(wrapped_->backing(), AccessMode::kRead);
  }
#if BUILDFLAG(IS_WIN)
  D3D11TextureAndArrayIndex GetD3D11Texture() const override {
    return wrapped_->GetD3D11Texture();
  }
#endif
#if BUILDFLAG(IS_ANDROID)
  AHardwareBuffer* GetAHardwareBuffer() const override {
    return wrapped_->GetAHardwareBuffer();
  }
#endif

  void SetClearedRect(const gfx::Rect& cleared_rect) override {
    SharedImageRepresentation::SetClearedRect(cleared_rect);
    if (owned_backing_) {
      wrapped_->SetClearedRect(cleared_rect);
    }
  }

 private:
  std::unique_ptr<SharedImageBacking> owned_backing_;
  std::unique_ptr<VideoImageRepresentation> wrapped_;
};

#if BUILDFLAG(ENABLE_VULKAN)
class WrappedVulkanCompoundImageRepresentation
    : public VulkanImageRepresentation {
 public:
  WrappedVulkanCompoundImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      gpu::VulkanDeviceQueue* vulkan_device_queue,
      gpu::VulkanImplementation& vulkan_impl,
      std::unique_ptr<VulkanImageRepresentation> wrapped,
      std::unique_ptr<SharedImageBacking> owned_backing)
      : VulkanImageRepresentation(manager,
                                  backing,
                                  tracker,
                                  nullptr,
                                  vulkan_device_queue,
                                  vulkan_impl),
        owned_backing_(std::move(owned_backing)),
        wrapped_(std::move(wrapped)) {
    DCHECK(wrapped_);
  }

  CompoundImageBacking* compound_backing() {
    return static_cast<CompoundImageBacking*>(backing());
  }

  bool BeginAccess(AccessMode access_mode,
                   std::vector<VkSemaphore>& begin_semaphores,
                   std::vector<VkSemaphore>& end_semaphores) override {
    compound_backing()->NotifyBeginAccess(wrapped_->backing(), access_mode,
                                          SharedImageAccessStream::kVulkan);
    if (!wrapped_->BeginAccess(access_mode, begin_semaphores, end_semaphores)) {
      return false;
    }
    return true;
  }

  void EndAccess(bool is_read_only, VkSemaphore end_semaphore) override {
    wrapped_->EndAccess(is_read_only, end_semaphore);
    compound_backing()->NotifyEndAccess(
        wrapped_->backing(),
        is_read_only ? AccessMode::kRead : AccessMode::kWrite);
  }

  gpu::VulkanImage& GetVulkanImage() override {
    return wrapped_->GetVulkanImage();
  }

  void SetClearedRect(const gfx::Rect& cleared_rect) override {
    SharedImageRepresentation::SetClearedRect(cleared_rect);
    if (owned_backing_) {
      wrapped_->SetClearedRect(cleared_rect);
    }
  }

 private:
  std::unique_ptr<SharedImageBacking> owned_backing_;
  std::unique_ptr<VulkanImageRepresentation> wrapped_;
};
#endif

// static
bool CompoundImageBacking::IsValidSharedMemoryFormat(
    const gfx::Size& size,
    viz::SharedImageFormat format) {
  if (format.PrefersExternalSampler()) {
    DVLOG(1) << "Unsupported external sampler for format: "
             << format.ToString();
    return false;
  }

  if (!IsSizeForBufferHandleValid(size, format)) {
    DVLOG(1) << "Invalid image size: " << size.ToString()
             << " for format: " << format.ToString();
    return false;
  }

  return true;
}

// static
bool CompoundImageBacking::ComputeIsThreadSafe(
    SharedImageFactoryRef* factory_ref,
    SharedImageUsageSet usage) {
  bool is_thread_safe = false;
  if (factory_ref) {
    factory_ref->Execute([&](SharedImageFactory* factory) {
      is_thread_safe = factory->IsSharedBetweenThreads(usage);

      // If dynamic backing allocation is enabled, the backing must also be
      // thread-safe if the environment is multi-threaded (e.g., DrDC or
      // WebView), as we can't predict all future sharing needs from the
      // initial usage.
      if (!is_thread_safe && base::FeatureList::IsEnabled(
                                 features::kUseDynamicBackingAllocations)) {
        is_thread_safe =
            factory->shared_image_manager_->display_context_on_another_thread();
      }
    });
  }
  return is_thread_safe;
}

// static
SharedImageUsageSet CompoundImageBacking::GetGpuSharedImageUsage(
    SharedImageUsageSet usage) {
  // Add allow copying from the shmem backing to the gpu backing.
  usage |= SHARED_IMAGE_USAGE_CPU_UPLOAD;

  if (kAllowShmOverlays) {
    // Remove SCANOUT usage since it was previously moved to the shmem backing.
    // See: |GetShmSharedImageUsage|
    usage.RemoveAll(SharedImageUsageSet(gpu::SHARED_IMAGE_USAGE_SCANOUT));
  }
  if (usage.Has(SHARED_IMAGE_USAGE_CPU_READ)) {
    // Remove CPU_READ usage since it was previously moved to the shmem backing.
    // See: |GetShmSharedImageUsage|
    usage.RemoveAll(SharedImageUsageSet(gpu::SHARED_IMAGE_USAGE_CPU_READ));
  }
  if (usage.Has(SHARED_IMAGE_USAGE_CPU_WRITE_ONLY)) {
    // Remove CPU_WRITE usage since it was previously moved to the shmem
    // backing. See: |GetShmSharedImageUsage|
    usage.RemoveAll(
        SharedImageUsageSet(gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY));
  }

  return usage;
}

// static
std::unique_ptr<SharedImageBacking> CompoundImageBacking::Create(
    SharedImageFactory* shared_image_factory,
    scoped_refptr<SharedImageCopyManager> copy_manager,
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    const SharedImageInfo& si_info) {
  auto format = si_info.format;
  auto size = si_info.size;
  auto usage = si_info.usage;
  if (!IsValidSharedMemoryFormat(size, format)) {
    return nullptr;
  }

  auto* gpu_backing_factory = shared_image_factory->GetFactoryByUsage(
      GetGpuSharedImageUsage(usage), format, size,
      /*pixel_data=*/{}, gfx::EMPTY_BUFFER, /*stream=*/std::nullopt,
      /*params=*/nullptr);
  if (!gpu_backing_factory) {
    return nullptr;
  }

  SharedImageInfo shm_si_info(
      format, size, si_info.color_space, si_info.surface_origin,
      si_info.alpha_type, GetShmSharedImageUsage(usage), si_info.debug_label);
  auto shm_backing = SharedMemoryImageBackingFactory().CreateSharedImage(
      mailbox, shm_si_info, /*is_thread_safe=*/false, std::move(handle));
  if (!shm_backing) {
    return nullptr;
  }
  shm_backing->SetNotRefCounted();

  return base::WrapUnique(new CompoundImageBacking(
      mailbox, si_info, std::move(shm_backing),
      shared_image_factory->GetFactoryRef(), gpu_backing_factory->GetWeakPtr(),
      std::move(copy_manager)));
}

// static
std::unique_ptr<SharedImageBacking> CompoundImageBacking::Create(
    SharedImageFactory* shared_image_factory,
    scoped_refptr<SharedImageCopyManager> copy_manager,
    const Mailbox& mailbox,
    const SharedImageInfo& si_info,
    gfx::BufferUsage buffer_usage) {
  auto format = si_info.format;
  auto size = si_info.size;
  auto usage = si_info.usage;
  if (!IsValidSharedMemoryFormat(size, format)) {
    return nullptr;
  }

  auto* gpu_backing_factory = shared_image_factory->GetFactoryByUsage(
      GetGpuSharedImageUsage(usage), format, size,
      /*pixel_data=*/{}, gfx::EMPTY_BUFFER, /*stream=*/std::nullopt,
      /*params=*/nullptr);
  if (!gpu_backing_factory) {
    return nullptr;
  }

  SharedImageInfo shm_si_info(
      format, size, si_info.color_space, si_info.surface_origin,
      si_info.alpha_type, GetShmSharedImageUsage(usage), si_info.debug_label);
  auto shm_backing = SharedMemoryImageBackingFactory().CreateSharedImage(
      mailbox, shm_si_info, kNullSurfaceHandle, /*is_thread_safe=*/false,
      buffer_usage);
  if (!shm_backing) {
    return nullptr;
  }
  shm_backing->SetNotRefCounted();

  return base::WrapUnique(new CompoundImageBacking(
      mailbox, si_info, std::move(shm_backing),
      shared_image_factory->GetFactoryRef(), gpu_backing_factory->GetWeakPtr(),
      std::move(copy_manager), std::move(buffer_usage)));
}

std::unique_ptr<SharedImageBacking> CompoundImageBacking::WrapExternalBacking(
    SharedImageFactory* shared_image_factory,
    scoped_refptr<SharedImageCopyManager> copy_manager,
    std::unique_ptr<SharedImageBacking> backing) {
  if (!backing) {
    return nullptr;
  }

  // We don't wrap an existing CompoundImageBacking which consists of a shm and
  // a gpu backing.
  CHECK_NE(backing->GetType(), SharedImageBackingType::kCompound);

  backing->SetNotRefCounted();

  auto buffer_usage = backing->buffer_usage();
  return base::WrapUnique(new CompoundImageBacking(
      std::move(buffer_usage), std::move(backing), std::move(copy_manager),
      shared_image_factory->GetFactoryRef()));
}

// static
std::unique_ptr<SharedImageBacking>
CompoundImageBacking::CreateSharedMemoryForTesting(
    SharedImageBackingFactory* gpu_backing_factory,
    scoped_refptr<SharedImageCopyManager> copy_manager,
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    const SharedImageInfo& si_info) {
  auto format = si_info.format;
  auto size = si_info.size;
  auto usage = si_info.usage;
  DCHECK(IsValidSharedMemoryFormat(size, format));

  SharedImageInfo shm_si_info(
      format, size, si_info.color_space, si_info.surface_origin,
      si_info.alpha_type, GetShmSharedImageUsage(usage), si_info.debug_label);
  auto shm_backing = SharedMemoryImageBackingFactory().CreateSharedImage(
      mailbox, shm_si_info, /*is_thread_safe=*/false, std::move(handle));
  if (!shm_backing) {
    return nullptr;
  }
  shm_backing->SetNotRefCounted();

  return base::WrapUnique(new CompoundImageBacking(
      mailbox, si_info, std::move(shm_backing),
      /*shared_image_factory=*/nullptr, gpu_backing_factory->GetWeakPtr(),
      std::move(copy_manager)));
}

// static
std::unique_ptr<SharedImageBacking>
CompoundImageBacking::CreateSharedMemoryForTesting(
    SharedImageBackingFactory* gpu_backing_factory,
    scoped_refptr<SharedImageCopyManager> copy_manager,
    const Mailbox& mailbox,
    const SharedImageInfo& si_info,
    gfx::BufferUsage buffer_usage) {
  auto format = si_info.format;
  auto size = si_info.size;
  auto usage = si_info.usage;
  DCHECK(IsValidSharedMemoryFormat(size, format));

  SharedImageInfo shm_si_info(
      format, size, si_info.color_space, si_info.surface_origin,
      si_info.alpha_type, GetShmSharedImageUsage(usage), si_info.debug_label);
  auto shm_backing = SharedMemoryImageBackingFactory().CreateSharedImage(
      mailbox, shm_si_info, kNullSurfaceHandle, /*is_thread_safe=*/false,
      buffer_usage);
  if (!shm_backing) {
    return nullptr;
  }
  shm_backing->SetNotRefCounted();

  return base::WrapUnique(new CompoundImageBacking(
      mailbox, si_info, std::move(shm_backing),
      /*shared_image_factory=*/nullptr, gpu_backing_factory->GetWeakPtr(),
      std::move(copy_manager), std::move(buffer_usage)));
}

CompoundImageBacking::CompoundImageBacking(
    const Mailbox& mailbox,
    const SharedImageInfo& si_info,
    std::unique_ptr<SharedImageBacking> shm_backing,
    scoped_refptr<SharedImageFactoryRef> shared_image_factory,
    base::WeakPtr<SharedImageBackingFactory> gpu_backing_factory,
    scoped_refptr<SharedImageCopyManager> copy_manager,
    std::optional<gfx::BufferUsage> buffer_usage)
    : ClearTrackingSharedImageBacking(
          mailbox,
          si_info,
          shm_backing->GetEstimatedSize(),
          ComputeIsThreadSafe(shared_image_factory.get(), si_info.usage),
          std::move(buffer_usage)),
      shared_image_factory_(std::move(shared_image_factory)),
      copy_manager_(std::move(copy_manager)) {
  // If the backing is thread-safe, the base class enables an internal lock that
  // protects the |elements_| vector and other metadata from concurrent access.
  DCHECK(shm_backing);
  auto size = si_info.size;
  auto usage = si_info.usage;
  DCHECK_EQ(size, shm_backing->size());

  // Create shared-memory element (stream = kMemory).
  ElementHolder shm_element;
  shm_element.access_streams = kMemoryStreamSet;
  if (kAllowShmOverlays && usage.Has(gpu::SHARED_IMAGE_USAGE_SCANOUT)) {
    shm_element.access_streams.Put(SharedImageAccessStream::kOverlay);
  }
  shm_element.content_id_ = latest_content_id_;
  shm_element.backing = std::move(shm_backing);
  has_shm_backing_ = true;
  elements_.push_back(std::move(shm_element));

  // Whenever CompoundImageBacking is created with a shm backing, mark it as
  // fully cleared.
  SetClearedRectInternal(gfx::Rect(size));

  // Create placeholder for GPU-backed element (streams = all except kMemory).
  ElementHolder gpu_element;
  gpu_element.access_streams =
      base::Difference(AccessStreamSet::All(), kMemoryStreamSet);

  // CreateBackingFromBackingFactory will be called on demand. Hence this is
  // lazy backing creation.
  gpu_element.create_callback =
      base::BindOnce(&CompoundImageBacking::CreateBackingFromBackingFactory,
                     base::Unretained(this), std::move(gpu_backing_factory),
                     si_info.debug_label, GetGpuSharedImageUsage(usage));
  elements_.push_back(std::move(gpu_element));
  max_elements_allocated_ = 2;
}

CompoundImageBacking::CompoundImageBacking(
    std::optional<gfx::BufferUsage> buffer_usage,
    std::unique_ptr<SharedImageBacking> backing,
    scoped_refptr<SharedImageCopyManager> copy_manager,
    scoped_refptr<SharedImageFactoryRef> shared_image_factory)
    : ClearTrackingSharedImageBacking(
          backing->mailbox(),
          SharedImageInfo(backing->format(),
                          backing->size(),
                          backing->color_space(),
                          backing->surface_origin(),
                          backing->alpha_type(),
                          backing->usage(),
                          backing->debug_label()),
          backing->GetEstimatedSize(),
          ComputeIsThreadSafe(shared_image_factory.get(), backing->usage()),
          std::move(buffer_usage)),
      shared_image_factory_(std::move(shared_image_factory)),
      copy_manager_(std::move(copy_manager)) {
  // Create the element from the backing.
  ElementHolder element;

  // We set the inner backing to support all streams so that we forward all
  // Produce calls to it. This is necessary because:
  // 1. The inner backing might support more than we assume (e.g. SharedMemory
  // supporting Overlay).
  // 2. We want to delegate the decision of "is this supported?" to the inner
  // backing. For example, if ProduceMemory is called on a GPU backing, we want
  // to forward it so the backing can return nullptr (unsupported), rather than
  // CompoundImageBacking logging an error because it thinks no backing exists
  // for that stream.
  // 3. This matches current behavior where we just have one backing handling
  // everything.
  // Future work for dynamic backing allocation will need a way to identify
  // backing support for specific streams based on usage.
  element.access_streams = AccessStreamSet::All();

  // |backing| may have a cleared rect set (e.g. from initial pixel data).
  // Propagate this to the CompoundImageBacking to keep them in sync.
  SetClearedRectInternal(backing->ClearedRect());

  // The backing is already created, so this is not a lazy initialization.
  element.backing = std::move(backing);

  // Mark the backing as having the latest content since it's the only one
  // created so far.
  element.content_id_ = latest_content_id_;
  elements_.push_back(std::move(element));
  CHECK_EQ(elements_.size(), 1u);
  max_elements_allocated_ = 1;
}

CompoundImageBacking::~CompoundImageBacking() {
  UMA_HISTOGRAM_COUNTS_100("GPU.CompoundImageBacking.TotalElementsAllocated",
                           max_elements_allocated_);
  if (pending_copy_to_gmb_callback_) {
    std::move(pending_copy_to_gmb_callback_).Run(/*success=*/false);
  }
}

void CompoundImageBacking::NotifyBeginAccess(SharedImageBacking* backing,
                                             RepresentationAccessMode mode,
                                             SharedImageAccessStream stream) {
  AutoLock auto_lock(this);

  // Identify if this backing is a permanent element of the container or a
  // transient one allocated for a specific representation.
  ElementHolder* access_element = GetElement(backing);

  // FAST PATH: If this is a permanent element and it already has the latest
  // content, we can skip synchronization for read access.
  if (access_element && access_element->content_id_ == latest_content_id_) {
    if (mode == RepresentationAccessMode::kWrite) {
      // For write access, this backing will become the new "source of truth".
      ++latest_content_id_;
      access_element->content_id_ = latest_content_id_;
    }
    return;
  }

  // STALE OR TRANSIENT PATH: Either this permanent backing is out-of-date, or
  // it's a transient backing that needs to be initialized. We must find the
  // permanent element that currently holds the latest content and copy from it.
  ElementHolder* latest_content_element = GetElementWithLatestContent();
  bool updated_backing = false;
  bool copy_succeeded = false;

  if (latest_content_element) {
    // Copy data from the latest permanent backing into the current backing
    // (which could be another permanent backing or a transient one).
    copy_succeeded = copy_manager_->CopyImage(
        /*src_backing=*/latest_content_element->GetBacking(),
        /*dst_backing=*/backing);

    if (copy_succeeded) {
      updated_backing = true;

      // When we sync data, we also sync the logical clear state. Propagate the
      // clear rect from the source to the destination, all child backings, and
      // the container itself.
      const gfx::Rect src_cleared_rect =
          latest_content_element->GetBacking()->ClearedRect();
      SetClearedRectInternal(src_cleared_rect);
      for (auto& element : elements_) {
        if (element.backing) {
          element.backing->SetClearedRect(src_cleared_rect);
        }
      }
    } else {
      LOG(ERROR) << "Failed to copy from "
                 << latest_content_element->GetBacking()->GetName() << " to "
                 << backing->GetName() << ". Backing can be using stale data";
    }

    UMA_HISTOGRAM_BOOLEAN("GPU.CompoundImageBacking.ContentSync.Success",
                          copy_succeeded);
    UMA_HISTOGRAM_ENUMERATION(
        "GPU.CompoundImageBacking.ContentSync.SourceBackingType",
        latest_content_element->GetBacking()->GetType());
    UMA_HISTOGRAM_ENUMERATION(
        "GPU.CompoundImageBacking.ContentSync.DestBackingType",
        backing->GetType());
    UMA_HISTOGRAM_ENUMERATION("GPU.CompoundImageBacking.ContentSync.Reason",
                              mode == RepresentationAccessMode::kRead
                                  ? ContentSyncReason::kRead
                                  : ContentSyncReason::kWrite);
    UMA_HISTOGRAM_ENUMERATION(
        "GPU.CompoundImageBacking.ContentSync.TriggeringAccessStream", stream);
    UMA_HISTOGRAM_SPARSE(
        "GPU.CompoundImageBacking.ContentSync.InitialSharedImageUsage",
        static_cast<int32_t>(static_cast<uint32_t>(this->usage())));
  }

  // UPDATE VERSIONING:
  // 1. For WRITE access: Increment the global version. If this is a permanent
  //    element, track that it now holds this latest version. Transient backings
  //    don't track their own version locally as they are destroyed after use,
  //    but their write will be synced back to permanent elements in EndAccess.
  // 2. For READ access: If the copy succeeded, mark this permanent element as
  //    being up-to-date with the latest version.
  if (mode == RepresentationAccessMode::kWrite) {
    ++latest_content_id_;
    if (access_element) {
      access_element->content_id_ = latest_content_id_;
    }
  } else if (updated_backing && access_element) {
    access_element->content_id_ = latest_content_id_;
  }
}

void CompoundImageBacking::NotifyEndAccess(SharedImageBacking* backing,
                                           RepresentationAccessMode mode) {
  AutoLock auto_lock(this);
  CHECK(backing);

  bool is_transient_backing = !GetElement(backing);

  // If the last access was a write and an underlying backing was accessed,
  // propagate its cleared rect to the compound backing if it's different.
  if (mode == RepresentationAccessMode::kWrite) {
    auto cleared_rect = backing->ClearedRect();
    if (cleared_rect != ClearedRect()) {
      // For transient backings, only update if it's more cleared to avoid
      // overwriting CSI with stale state.
      if (!is_transient_backing || cleared_rect.Contains(ClearedRect())) {
        SetClearedRectInternal(cleared_rect);
      }
    }

    // When is_thread_safe() is true, multiple threads can access the backings.
    // If a backing was written to, we proactively sync its content to all other
    // backings. This is especially critical for transient backings (which are
    // not thread-safe but used in a thread-safe container): we must copy their
    // latest content to the permanent backings before the transient backing is
    // destroyed. This ensures that subsequent accesses on other threads will
    // see the updated data. Note that we are copying back to all the backings
    // here, even to those that will never be read. This can be a performance
    // bottleneck when there are multiple backings.
    if (base::FeatureList::IsEnabled(features::kUseDynamicBackingAllocations) &&
        is_thread_safe() && is_transient_backing) {
      for (auto& element : elements_) {
        auto* dst_backing = element.GetBacking();
        if (dst_backing && dst_backing != backing &&
            element.content_id_ != latest_content_id_) {
          if (copy_manager_->CopyImage(backing, dst_backing)) {
            element.content_id_ = latest_content_id_;
          } else {
            LOG(ERROR) << "DCSI: Proactive copy from " << backing->GetName()
                       << " to " << dst_backing->GetName() << " failed.";
          }
        }
      }
    }
  }
}

void CompoundImageBacking::OnContextLost() {
  ClearTrackingSharedImageBacking::OnContextLost();
  AutoLock auto_lock(this);
  for (const auto& element : elements_) {
    if (element.backing) {
      element.backing->OnContextLost();
    }
  }
}

void CompoundImageBacking::SetPurgeable(bool purgeable) {
  AutoLock auto_lock(this);
  for (auto& element : elements_) {
    if (element.backing) {
      element.backing->SetPurgeable(purgeable);
    }
  }
}

bool CompoundImageBacking::IsPurgeable() const {
  AutoLock auto_lock(this);
  for (const auto& element : elements_) {
    // If any of the elements is purgeable, then return true. The caller may
    // rely on the return value to e.g. recreate data, so needs to be an OR, nor
    // an AND.
    if (element.backing && element.backing->IsPurgeable()) {
      return true;
    }
  }
  return false;
}

SharedImageBackingType CompoundImageBacking::GetType() const {
  return SharedImageBackingType::kCompound;
}

void CompoundImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  // Update() synchronizes CPU-side writes (from Shared Memory or GMB) with the
  // GPU. Hence it must target the backing which owns the CPU-mappable memory.
  //
  // Per this backing's design:
  // 1. SharedMemoryImageBacking if present is the exclusive owner of CPU
  // memory.
  // 2. A SharedMemoryImageBacking or other CPU-mappable backing is always
  // created only at initialization time. Hence it is guaranteed to be at
  // elements_[0].
  // 3. |elements_| always contains at least one element.
  AutoLock auto_lock(this);
  CHECK(!elements_.empty());
  auto& element = elements_[0];
  CHECK(element.backing);
  element.backing->Update(std::move(in_fence));

  // Incrementing the content ID marks this backing as the new "source
  // of truth." Subsequent access to other backings will trigger an
  // efficient copy from this element to ensure consistency.
  element.content_id_ = ++latest_content_id_;
}

bool CompoundImageBacking::CopyToGpuMemoryBuffer() {
  AutoLock auto_lock(this);
  auto& shm_element = GetShmElement();

  if (HasLatestContent(shm_element)) {
    return true;
  }

  auto* gpu_backing = GetGpuBacking();
  if (!gpu_backing ||
      !copy_manager_->CopyImage(gpu_backing, shm_element.GetBacking())) {
    LOG(ERROR) << "Failed to copy from GPU backing (" << gpu_backing->GetName()
               << ") to shared memory";
    return false;
  }

  shm_element.content_id_ = latest_content_id_;
  return true;
}

void CompoundImageBacking::CopyToGpuMemoryBufferAsync(
    base::OnceCallback<void(bool)> callback) {
  AutoLock auto_lock(this);
  auto& shm_element = GetShmElement();

  if (HasLatestContent(shm_element)) {
    std::move(callback).Run(true);
    return;
  }

  if (pending_copy_to_gmb_callback_) {
    DLOG(ERROR) << "Existing CopyToGpuMemoryBuffer operation pending";
    std::move(callback).Run(false);
    return;
  }

  auto* gpu_backing = GetGpuBacking();
  if (!gpu_backing) {
    LOG(ERROR) << "Failed to copy from GPU backing (" << gpu_backing->GetName()
               << ") to shared memory";
    std::move(callback).Run(false);
    return;
  }

  pending_copy_to_gmb_callback_ = std::move(callback);

  gpu_backing->ReadbackToMemoryAsync(
      GetSharedMemoryPixmaps(),
      base::BindOnce(&CompoundImageBacking::OnCopyToGpuMemoryBufferComplete,
                     base::Unretained(this)));
}

void CompoundImageBacking::OnCopyToGpuMemoryBufferComplete(bool success) {
  AutoLock auto_lock(this);
  if (success) {
    auto& shm_element = GetShmElement();
    shm_element.content_id_ = latest_content_id_;
  }
  std::move(pending_copy_to_gmb_callback_).Run(success);
}

gfx::Rect CompoundImageBacking::ClearedRect() const {
  // If we have a shm_backing, we always copy on access and mark entire backing
  // as cleared.
  return ClearedRectInternal();
}

void CompoundImageBacking::SetClearedRect(const gfx::Rect& cleared_rect) {
  AutoLock auto_lock(this);
  SetClearedRectInternal(cleared_rect);

  // Propagate the cleared rect to all underlying backings. This is important
  // because SetClearedRect can be called on a CompoundImageBacking without a
  // preceding BeginAccess call (e.g. in WebGPU's AssociateMailbox with the
  // WEBGPU_MAILBOX_DISCARD flag). In such cases, it is hard to know which
  // backing is currently being accessed. so we must ensure all potential
  // backings are updated
  for (auto& element : elements_) {
    if (element.backing) {
      element.backing->SetClearedRect(cleared_rect);
    }
  }
}

void CompoundImageBacking::MarkForDestruction() {
  AutoLock auto_lock(this);
  for (const auto& element : elements_) {
    if (element.backing) {
      element.backing->MarkForDestruction();
    }
  }
}

gfx::GpuMemoryBufferHandle CompoundImageBacking::GetGpuMemoryBufferHandle() {
  // A GpuMemoryBufferHandle corresponds to the shared memory or native buffer
  // (like an IOSurface, AHardwareBuffer, or DXGI Handle) that backs the image.
  //
  // Per this backing's design:
  // 1. Any CPU-mappable backing including SharedMemoryImageBacking is always
  // created only at initialization time and never allocated dynamically during
  // runtime. Hence it is guaranteed to be at elements_[0].
  // 2. |elements_| always contains at least one element.
  AutoLock auto_lock(this);
  CHECK(!elements_.empty());
  auto& element = elements_[0];
  CHECK(element.backing);
  return element.backing->GetGpuMemoryBufferHandle();
}

scoped_refptr<gfx::NativePixmap> CompoundImageBacking::GetNativePixmap() {
  // The purpose of this function is to get NativePixmap for overlay testing,
  // so it needs be the same NativePixmap that we would later get from the
  // ProduceOverlay representation. Hence using Overlay stream backing here.
  AutoLock auto_lock(this);
  for (const auto& element : elements_) {
    if (element.access_streams.Has(SharedImageAccessStream::kOverlay) &&
        element.backing) {
      return element.backing->GetNativePixmap();
    }
  }
  return nullptr;
}

std::unique_ptr<DawnImageRepresentation> CompoundImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    std::vector<wgpu::TextureFormat> view_formats,
    scoped_refptr<SharedContextState> context_state) {
  AccessParams access_params;
  access_params.wgpu_device = device;
  access_params.context_state = context_state;
  std::unique_ptr<SharedImageBacking> transient_backing;
  auto* backing = GetOrAllocateBacking(SharedImageAccessStream::kDawn,
                                       access_params, transient_backing);
  if (!backing) {
    return nullptr;
  }

  auto real_rep = backing->ProduceDawn(manager, tracker, device, backend_type,
                                       std::move(view_formats), context_state);
  if (!real_rep) {
    return nullptr;
  }

  return std::make_unique<WrappedDawnCompoundImageRepresentation>(
      manager, this, tracker, std::move(real_rep),
      std::move(transient_backing));
}

std::unique_ptr<DawnBufferRepresentation>
CompoundImageBacking::ProduceDawnBuffer(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    scoped_refptr<SharedContextState> context_state) {
  AccessParams access_params;
  access_params.wgpu_device = device;
  access_params.context_state = context_state;
  std::unique_ptr<SharedImageBacking> transient_backing;
  auto* backing = GetOrAllocateBacking(SharedImageAccessStream::kDawnBuffer,
                                       access_params, transient_backing);
  if (!backing) {
    return nullptr;
  }

  auto real_rep = backing->ProduceDawnBuffer(manager, tracker, device,
                                             backend_type, context_state);
  if (!real_rep) {
    return nullptr;
  }

  return std::make_unique<WrappedDawnBufferCompoundImageRepresentation>(
      manager, this, tracker, std::move(real_rep),
      std::move(transient_backing));
}

std::unique_ptr<GLTextureImageRepresentation>
CompoundImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                       MemoryTypeTracker* tracker) {
  // For GLTextureImageRepresentation, the SharedImageAccessStream::kGL is
  // specific enough for backing selection. While AccessParams could be extended
  // in the future to include GL context information for stricter correctness
  // checks (e.g., ensuring a backing created on one GL context isn't used on
  // another, unless it's an EglImageBacking), it is not currently needed.
  std::unique_ptr<SharedImageBacking> transient_backing;
  auto* backing = GetOrAllocateBacking(SharedImageAccessStream::kGL,
                                       AccessParams(), transient_backing);
  if (!backing) {
    return nullptr;
  }

  auto real_rep = backing->ProduceGLTexture(manager, tracker);
  if (!real_rep) {
    return nullptr;
  }

  return std::make_unique<WrappedGLTextureCompoundImageRepresentation>(
      manager, this, tracker, std::move(real_rep),
      std::move(transient_backing));
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
CompoundImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                                  MemoryTypeTracker* tracker) {
  // For GLTexturePassthroughImageRepresentation, the
  // SharedImageAccessStream::kGL is specific enough for backing selection.
  // While AccessParams could be extended in the future to include GL context
  // information for stricter correctness checks, it is not currently needed.
  std::unique_ptr<SharedImageBacking> transient_backing;
  auto* backing = GetOrAllocateBacking(SharedImageAccessStream::kGL,
                                       AccessParams(), transient_backing);
  if (!backing) {
    return nullptr;
  }

  auto real_rep = backing->ProduceGLTexturePassthrough(manager, tracker);
  if (!real_rep) {
    return nullptr;
  }

  return std::make_unique<
      WrappedGLTexturePassthroughCompoundImageRepresentation>(
      manager, this, tracker, std::move(real_rep),
      std::move(transient_backing));
}

std::unique_ptr<SkiaGaneshImageRepresentation>
CompoundImageBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  AccessParams access_params;
  access_params.context_state = context_state;
  std::unique_ptr<SharedImageBacking> transient_backing;
  auto* backing = GetOrAllocateBacking(SharedImageAccessStream::kSkia,
                                       access_params, transient_backing);
  if (!backing) {
    return nullptr;
  }

  auto real_rep = backing->ProduceSkiaGanesh(manager, tracker, context_state);
  if (!real_rep) {
    return nullptr;
  }

  auto* gr_context = context_state ? context_state->gr_context() : nullptr;
  return std::make_unique<WrappedSkiaGaneshCompoundImageRepresentation>(
      gr_context, manager, this, tracker, std::move(real_rep),
      std::move(transient_backing));
}

std::unique_ptr<SkiaGraphiteImageRepresentation>
CompoundImageBacking::ProduceSkiaGraphite(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  AccessParams access_params;
  access_params.context_state = context_state;
  std::unique_ptr<SharedImageBacking> transient_backing;
  auto* backing = GetOrAllocateBacking(SharedImageAccessStream::kSkia,
                                       access_params, transient_backing);
  if (!backing) {
    return nullptr;
  }

  auto real_rep = backing->ProduceSkiaGraphite(manager, tracker, context_state);
  if (!real_rep) {
    return nullptr;
  }

  return std::make_unique<WrappedSkiaGraphiteCompoundImageRepresentation>(
      manager, this, tracker, std::move(real_rep),
      std::move(transient_backing));
}

std::unique_ptr<OverlayImageRepresentation>
CompoundImageBacking::ProduceOverlay(SharedImageManager* manager,
                                     MemoryTypeTracker* tracker) {
  // For OverlayImageRepresentation, no specific context information is
  // currently required for backing selection, so AccessParams is empty.
  std::unique_ptr<SharedImageBacking> transient_backing;
  auto* backing = GetOrAllocateBacking(SharedImageAccessStream::kOverlay,
                                       AccessParams(), transient_backing);
  if (!backing) {
    return nullptr;
  }

  auto real_rep = backing->ProduceOverlay(manager, tracker);
  if (!real_rep) {
    return nullptr;
  }

  return std::make_unique<WrappedOverlayCompoundImageRepresentation>(
      manager, this, tracker, std::move(real_rep),
      std::move(transient_backing));
}

std::unique_ptr<WebNNTensorRepresentation>
CompoundImageBacking::ProduceWebNNTensor(SharedImageManager* manager,
                                         MemoryTypeTracker* tracker) {
  // For WebNNTensorRepresentation, no specific context information is
  // currently required for backing selection, so AccessParams is empty.
  std::unique_ptr<SharedImageBacking> transient_backing;
  auto* backing = GetOrAllocateBacking(SharedImageAccessStream::kWebNNTensor,
                                       AccessParams(), transient_backing);
  if (!backing) {
    return nullptr;
  }

  auto real_rep = backing->ProduceWebNNTensor(manager, tracker);
  if (!real_rep) {
    return nullptr;
  }

  return std::make_unique<WrappedWebNNTensorCompoundImageRepresentation>(
      manager, this, tracker, std::move(real_rep),
      std::move(transient_backing));
}

std::unique_ptr<MemoryImageRepresentation> CompoundImageBacking::ProduceMemory(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  // For MemoryImageRepresentation, no specific context information is
  // currently required for backing selection, so AccessParams is empty.
  std::unique_ptr<SharedImageBacking> transient_backing;
  auto* backing = GetOrAllocateBacking(SharedImageAccessStream::kMemory,
                                       AccessParams(), transient_backing);
  if (!backing) {
    return nullptr;
  }

  auto real_rep = backing->ProduceMemory(manager, tracker);
  if (!real_rep) {
    return nullptr;
  }

  return std::make_unique<WrappedMemoryCompoundImageRepresentation>(
      manager, this, tracker, std::move(real_rep),
      std::move(transient_backing));
}

std::unique_ptr<VideoImageRepresentation> CompoundImageBacking::ProduceVideo(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    VideoDevice device) {
  std::unique_ptr<SharedImageBacking> transient_backing;
  auto* backing = GetOrAllocateBacking(SharedImageAccessStream::kGL,
                                       AccessParams(), transient_backing);
  if (!backing) {
    return nullptr;
  }

  auto real_rep = backing->ProduceVideo(manager, tracker, device);
  if (!real_rep) {
    return nullptr;
  }

  return std::make_unique<WrappedVideoCompoundImageRepresentation>(
      manager, this, tracker, std::move(real_rep),
      std::move(transient_backing));
}

#if BUILDFLAG(ENABLE_VULKAN)
std::unique_ptr<VulkanImageRepresentation> CompoundImageBacking::ProduceVulkan(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    gpu::VulkanDeviceQueue* vulkan_device_queue,
    gpu::VulkanImplementation& vulkan_impl,
    bool needs_detiling) {
  // For VulkanImageRepresentation, AccessParams is not needed as of now.
  std::unique_ptr<SharedImageBacking> transient_backing;
  auto* backing = GetOrAllocateBacking(SharedImageAccessStream::kVulkan,
                                       AccessParams(), transient_backing);
  if (!backing) {
    return nullptr;
  }

  auto real_rep = backing->ProduceVulkan(manager, tracker, vulkan_device_queue,
                                         vulkan_impl, needs_detiling);
  if (!real_rep) {
    return nullptr;
  }

  return std::make_unique<WrappedVulkanCompoundImageRepresentation>(
      manager, this, tracker, vulkan_device_queue, vulkan_impl,
      std::move(real_rep), std::move(transient_backing));
}
#endif

base::trace_event::MemoryAllocatorDump* CompoundImageBacking::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDumpGuid client_guid,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
  // Create dump but don't add scalar size. The size will be inferred from the
  // sizes of the sub-backings.
  AutoLock auto_lock(this);
  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(dump_name);

  dump->AddString("type", "", GetName());
  dump->AddString("dimensions", "", size().ToString());
  dump->AddString("format", "", format().ToString());
  dump->AddString("usage", "", CreateLabelForSharedImageUsage(usage()));

  // Add ownership edge to `client_guid` which expresses shared ownership with
  // the client process for the top level dump.
  pmd->CreateSharedGlobalAllocatorDump(client_guid);
  pmd->AddOwnershipEdge(dump->guid(), client_guid,
                        static_cast<int>(TracingImportance::kNotOwner));

  // Add dumps nested under `dump_name` for child backings owned by compound
  // image. These get different shared GUIDs to add ownership edges with GPU
  // texture or shared memory.
  for (int i = 0; i < static_cast<int>(elements_.size()); ++i) {
    auto* backing = elements_[i].backing.get();
    if (!backing)
      continue;

    // When CompoundImageBacking wraps a single backing, use the client's global
    // Mailbox GUID instead of sub-backing GUID. This ensures correct effective
    // size attribution to the client process where client claims all of the
    // memory/effective_size(since client usually has higher
    // TracingImportance(2) than service side (0)).
    // This does not work well when CompoundImageBacking has multiple gpu
    // backings. In that case, each child element creates its own sub-backing
    // GUID and links to it rather than linking to global mailbox GUID. However,
    // the client only knows about the single Global Mailbox GUID. As a result,
    // the client claims the Global GUID, but the GPU service claims the memory
    // for the individual Sub-Backing GUIDs. This leads to over-reporting
    // (actual effective_size = Client Size + Sum of Sub-Backings instead of
    // expected effective_size = Sum of Sub-Backings). This is currently a known
    // architectural limitation that requires further design to unify memory
    // attribution (e.g., via dynamic IPC updates to the client about total
    // elements OR by shifting total ownership to the service with higher
    // importance).
    auto element_client_guid =
        elements_.size() == 1 ? client_guid
                              : GetSubBackingGUIDForTracing(mailbox(), i + 1);
    std::string element_dump_name =
        base::StringPrintf("%s/element_%d", dump_name.c_str(), i);
    backing->OnMemoryDump(element_dump_name, element_client_guid, pmd,
                          client_tracing_id);
  }
  return dump;
}

const std::vector<SkPixmap>& CompoundImageBacking::GetSharedMemoryPixmaps() {
  AutoLock auto_lock(this);
  auto* shm_backing = GetShmElement().GetBacking();
  DCHECK(shm_backing);

  // SECURITY: WrapExternalBacking wraps arbitrary backing types and gives them
  // AccessStreamSet::All() (including kMemory). GetShmElement() then returns
  // that wrapped backing here regardless of its actual type. Guard against the
  // resulting type confusion in the static_cast below. Note marking a backing
  // to support all access stream is expected behavior wheres
  // ::GetSharedMemoryPixmaps should only be invoked on
  // SharedImageBackingType::kSharedMemory currently.
  CHECK_EQ(shm_backing->GetType(), SharedImageBackingType::kSharedMemory);

  return static_cast<SharedMemoryImageBacking*>(shm_backing)->pixmaps();
}

CompoundImageBacking::ElementHolder& CompoundImageBacking::GetShmElement() {
  for (auto& element : elements_) {
    // There should be exactly one element where this returns true.
    if (element.access_streams.Has(SharedImageAccessStream::kMemory)) {
      return element;
    }
  }

  NOTREACHED();
}

CompoundImageBacking::ElementHolder* CompoundImageBacking::GetElement(
    const SharedImageBacking* backing) {
  for (auto& element : elements_) {
    if (element.backing.get() == backing) {
      return &element;
    }
  }
  return nullptr;
}

CompoundImageBacking::ElementHolder*
CompoundImageBacking::GetElementWithLatestContent() {
  // Note that for now iterating over all elements should be fine since we would
  // likely not ever had too many backings existing concurrently. We can
  // optimize this code by using better algorithm or more suitable data
  // structure later if needed.
  for (auto& element : elements_) {
    if (element.content_id_ == latest_content_id_ && element.GetBacking()) {
      return &element;
    }
  }
  return nullptr;
}

SharedImageBacking* CompoundImageBacking::GetOrAllocateBacking(
    SharedImageAccessStream stream,
    const AccessParams& params,
    std::unique_ptr<SharedImageBacking>& out_transient_backing) {
  AutoLock auto_lock(this);
  ElementHolder* best_match = nullptr;
  ElementHolder* any_match = nullptr;

  // Note that for now iterating over all elements should be fine since we would
  // likely not ever had too many backings existing concurrently. We can
  // optimize this code by using better algorithm or more suitable data
  // structure later if needed.
  for (auto& element : elements_) {
    if (element.access_streams.Has(stream) && element.GetBacking() &&
        element.GetBacking()->SupportsAccess(stream, params)) {
      if (element.content_id_ == latest_content_id_) {
        best_match = &element;
        break;
      }
      if (!any_match) {
        any_match = &element;
      }
    }
  }

  ElementHolder* target_element = best_match ? best_match : any_match;
  if (target_element) {
    return target_element->GetBacking();
  }

  // If no backing is found, we will try to create a new one. This feature is
  // disabled by default currently until SharedImageCopyManager is fully ready
  // to support all the existing gpu-gpu copy usages.
  if (base::FeatureList::IsEnabled(features::kUseDynamicBackingAllocations) &&
      shared_image_factory_) {
    SharedImageUsageSet usage = GetUsageFromAccessStream(stream);
    SharedImageBackingFactory* gpu_backing_factory = nullptr;
    shared_image_factory_->Execute([&](SharedImageFactory* factory) {
      gpu_backing_factory = factory->GetFactoryByUsage(
          usage, format(), size(),
          /*pixel_data=*/{}, gfx::EMPTY_BUFFER, stream, &params);
    });

    if (gpu_backing_factory) {
      std::unique_ptr<SharedImageBacking> new_backing;
      CreateBackingFromBackingFactory(gpu_backing_factory->GetWeakPtr(),
                                      debug_label(), usage, new_backing);
      if (new_backing) {
        UMA_HISTOGRAM_ENUMERATION(
            "GPU.CompoundImageBacking.DynamicAllocation.BackingType",
            new_backing->GetType());
        UMA_HISTOGRAM_ENUMERATION(
            "GPU.CompoundImageBacking.DynamicAllocation.AccessStream", stream);
        UMA_HISTOGRAM_SPARSE(
            "GPU.CompoundImageBacking.DynamicAllocation."
            "InitialSharedImageUsage",
            static_cast<int32_t>(static_cast<uint32_t>(this->usage())));

        // If the CSI container is thread-safe, we treat newly created backings
        // as transient if they are not thread-safe. They will be owned by the
        // representation and destroyed after use. This ensures that a
        // non-thread-safe backing allocated on one thread doesn't persist in
        // the thread-safe container, which could lead to race conditions if
        // accessed from another thread later.
        if (is_thread_safe() && !new_backing->is_thread_safe()) {
          out_transient_backing = std::move(new_backing);
          return out_transient_backing.get();
        }

        // Else we treat the backing as non-transient and add it to the list of
        // alive elements. This is done when either the container is not
        // thread-safe or the new backing itself is thread-safe.
        ElementHolder element;
        element.access_streams.Put(stream);
        element.backing = std::move(new_backing);
        elements_.push_back(std::move(element));
        if (elements_.size() > max_elements_allocated_) {
          max_elements_allocated_ = elements_.size();
        }
        return elements_.back().GetBacking();
      }
    }
  }

  LOG(ERROR) << "Could not find or create a backing for stream " << stream;
  return nullptr;
}

SharedImageBacking* CompoundImageBacking::GetGpuBacking() {
  for (auto& element : elements_) {
    if (!element.access_streams.Has(SharedImageAccessStream::kMemory)) {
      return element.GetBacking();
    }
  }
  LOG(ERROR) << "No GPU backing found.";
  return nullptr;
}

void CompoundImageBacking::CreateBackingFromBackingFactory(
    base::WeakPtr<SharedImageBackingFactory> factory,
    std::string debug_label,
    SharedImageUsageSet usage,
    std::unique_ptr<SharedImageBacking>& backing) {
  if (!factory) {
    DLOG(ERROR) << "Can't allocate backing after image has been destroyed";
    return;
  }

  SharedImageInfo si_info(format(), size(), color_space(), surface_origin(),
                          alpha_type(), usage, debug_label);
  backing = factory->CreateSharedImage(mailbox(), si_info, kNullSurfaceHandle,
                                       /*is_thread_safe=*/false);
  if (!backing) {
    DLOG(ERROR) << "Failed to allocate GPU backing";
    return;
  }

  // Since the owned GPU backing is never registered with SharedImageManager
  // it's not recorded in UMA histogram there.
  UMA_HISTOGRAM_ENUMERATION("GPU.SharedImage.BackingType", backing->GetType());

  backing->SetNotRefCounted();

  // When a CompoundImageBacking is created with a shared memory backing, it's
  // considered logically cleared from the start, as the shared memory provides
  // the initial content. For consistency, any newly created GPU backings also
  // need to reflect this cleared state, even if their physical memory isn't
  // yet initialized. The system ensures a copy from the shared memory backing
  // to the GPU backing will occur on first access, synchronizing the physical
  // content.
  if (has_shm_backing_) {
    backing->SetCleared();
  }

  // Update peak GPU memory tracking with the new estimated size.
  size_t estimated_size = 0;
  for (auto& element : elements_) {
    if (element.backing)
      estimated_size += element.backing->GetEstimatedSize();
  }

  UpdateEstimatedSize(estimated_size);
}

bool CompoundImageBacking::HasLatestContent(ElementHolder& element) {
  return element.content_id_ == latest_content_id_;
}

void CompoundImageBacking::OnAddSecondaryReference() {
  // When client adds a reference from another processes it expects this
  // SharedImage can outlive original factory ref and so potentially
  // SharedimageFactory. We should create all backings now as we might not have
  // access to corresponding SharedImageBackingFactories later.
  AutoLock auto_lock(this);
  for (auto& element : elements_) {
    element.CreateBackingIfNecessary();
  }
}

CompoundImageBacking::ElementHolder::ElementHolder() = default;
CompoundImageBacking::ElementHolder::~ElementHolder() = default;
CompoundImageBacking::ElementHolder::ElementHolder(ElementHolder&& other) =
    default;
CompoundImageBacking::ElementHolder&
CompoundImageBacking::ElementHolder::operator=(ElementHolder&& other) = default;

void CompoundImageBacking::ElementHolder::CreateBackingIfNecessary() {
  if (create_callback) {
    std::move(create_callback).Run(backing);
  }
}

SharedImageBacking* CompoundImageBacking::ElementHolder::GetBacking() {
  CreateBackingIfNecessary();
  return backing.get();
}

GPU_GLES2_EXPORT std::ostream& operator<<(
    std::ostream& os,
    gpu::SharedImageAccessStream access_stream) {
  switch (access_stream) {
    case gpu::SharedImageAccessStream::kSkia:
      os << "kSkia";
      break;
    case gpu::SharedImageAccessStream::kOverlay:
      os << "kOverlay";
      break;
    case gpu::SharedImageAccessStream::kGL:
      os << "kGL";
      break;
    case gpu::SharedImageAccessStream::kDawn:
      os << "kDawn";
      break;
    case gpu::SharedImageAccessStream::kDawnBuffer:
      os << "kDawnBuffer";
      break;
    case gpu::SharedImageAccessStream::kMemory:
      os << "kMemory";
      break;
    case gpu::SharedImageAccessStream::kVaapi:
      os << "kVaapi";
      break;
    case gpu::SharedImageAccessStream::kWebNNTensor:
      os << "kWebNNTensor";
      break;
    case gpu::SharedImageAccessStream::kVulkan:
      os << "kVulkan";
      break;
  }
  return os;
}

}  // namespace gpu
