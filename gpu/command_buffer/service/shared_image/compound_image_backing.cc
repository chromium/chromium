// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/compound_image_backing.h"

#include <optional>
#include <ostream>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
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
#include "gpu/command_buffer/service/shared_image/shared_memory_image_backing_factory.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/skia/include/core/SkAlphaType.h"
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

#if BUILDFLAG(IS_ANDROID)
#include "gpu/command_buffer/service/shared_image/ahardwarebuffer_image_backing_factory.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/win/d3d_shared_fence.h"
#endif

namespace gpu {
namespace {

constexpr AccessStreamSet kMemoryStreamSet = {SharedImageAccessStream::kMemory};

gfx::GpuMemoryBufferType GetNativeBufferType() {
#if BUILDFLAG(IS_APPLE)
  return gfx::GpuMemoryBufferType::IO_SURFACE_BUFFER;
#elif BUILDFLAG(IS_ANDROID)
  return gfx::GpuMemoryBufferType::ANDROID_HARDWARE_BUFFER;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  return gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
#elif BUILDFLAG(IS_WIN)
  return gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE;
#else
  return gfx::GpuMemoryBufferType::EMPTY_BUFFER;
#endif
}

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
  if (usage.Has(SHARED_IMAGE_USAGE_RASTER_COPY_SOURCE)) {
    new_usage |= SHARED_IMAGE_USAGE_RASTER_COPY_SOURCE;
  }
  if (kAllowShmOverlays && usage.Has(gpu::SHARED_IMAGE_USAGE_SCANOUT)) {
    new_usage |= SHARED_IMAGE_USAGE_SCANOUT;
  }
  return new_usage;
}

// Returns true if `usage` contains any GPU usage beyond what a standalone
// SharedMemoryImageBackingFactory previously supported directly (CPU read/write
// and raster copy source).
bool HasGpuSharedImageUsage(SharedImageUsageSet usage) {
  constexpr SharedImageUsageSet kShmOnlyUsage =
      SHARED_IMAGE_USAGE_CPU_WRITE_ONLY | SHARED_IMAGE_USAGE_CPU_READ |
      SHARED_IMAGE_USAGE_RASTER_COPY_SOURCE;
  return !kShmOnlyUsage.HasAll(usage);
}

bool IsValidSharedMemoryFormat(const gfx::Size& size,
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

// Remove the SCANOUT flag if |kAllowShmOverlays| and strip CPU-only shared
// memory flags handled by SharedMemoryImageBacking.
SharedImageUsageSet GetGpuSharedImageUsage(SharedImageUsageSet usage) {
  // Allow copying from the shmem backing to the gpu backing.
  usage |= SHARED_IMAGE_USAGE_CPU_UPLOAD;

  if (kAllowShmOverlays) {
    usage.RemoveAll(SharedImageUsageSet(gpu::SHARED_IMAGE_USAGE_SCANOUT));
  }
  usage.RemoveAll(SharedImageUsageSet(gpu::SHARED_IMAGE_USAGE_CPU_READ));
  usage.RemoveAll(SharedImageUsageSet(gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY));
  return usage;
}

void RecordBackingTypeUMA(SharedImageBackingType backing_type) {
  UMA_HISTOGRAM_ENUMERATION("GPU.SharedImage.BackingType", backing_type);
}

// Returns the SharedImageUsageSet used to query SharedImageFactory when
// dynamically allocating a backing for `stream` and `params`. Note that the
// backing usually doesn't know at this point whether the access stream will be
// used for read or write, so both read and write usages are included.
SharedImageUsageSet GetUsageFromAccessStream(SharedImageAccessStream stream,
                                             const AccessParams& params) {
  switch (stream) {
    case SharedImageAccessStream::kGL:
      return SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_GLES2_WRITE;
    case SharedImageAccessStream::kSkia:
      // When the accessing context uses GL (e.g., Ganesh over GL), request
      // GLES2 usages so SharedImageFactory selects a GL-compatible backing
      // (such as GLTextureImageBacking or EGLImageBacking) rather than
      // WrappedSkImageBacking, and so GL factories do not reject the request
      // on RASTER/DISPLAY usage checks when the factory's main GrContextType is
      // non-GL.
      if (params.context_state && params.context_state->IsUsingGL()) {
        return SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_GLES2_WRITE;
      }
      return SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE |
             SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE;
    case SharedImageAccessStream::kDawn:
      return SHARED_IMAGE_USAGE_WEBGPU_READ | SHARED_IMAGE_USAGE_WEBGPU_WRITE;
    case SharedImageAccessStream::kDawnBuffer:
      return SHARED_IMAGE_USAGE_WEBGPU_READ | SHARED_IMAGE_USAGE_WEBGPU_WRITE;
    case SharedImageAccessStream::kOverlay:
      return SHARED_IMAGE_USAGE_SCANOUT;
    case SharedImageAccessStream::kVaapi:
      return SHARED_IMAGE_USAGE_VIDEO_DECODE |
             SHARED_IMAGE_USAGE_VIDEO_ENCODE_ACCELERATOR;
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
    if (!compound_backing()->NotifyBeginAccess(wrapped_->backing(), access_mode,
                                               SharedImageAccessStream::kGL)) {
      return false;
    }
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

  bool IsBackingCleared() const override {
    return wrapped_->IsBackingCleared();
  }

  void SetClearedRect(const gfx::Rect& cleared_rect) override {
    SharedImageRepresentation::SetClearedRect(cleared_rect);
    if (owned_backing_) {
      wrapped_->SetClearedRect(cleared_rect);
    }
  }

  void OnContextLost() override {
    GLTextureImageRepresentation::OnContextLost();
    wrapped_->OnContextLost();
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
    if (!compound_backing()->NotifyBeginAccess(wrapped_->backing(), access_mode,
                                               SharedImageAccessStream::kGL)) {
      return false;
    }
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

  bool IsBackingCleared() const override {
    return wrapped_->IsBackingCleared();
  }

  void SetClearedRect(const gfx::Rect& cleared_rect) override {
    SharedImageRepresentation::SetClearedRect(cleared_rect);
    if (owned_backing_) {
      wrapped_->SetClearedRect(cleared_rect);
    }
  }

  void OnContextLost() override {
    GLTexturePassthroughImageRepresentation::OnContextLost();
    wrapped_->OnContextLost();
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
    if (!compound_backing()->NotifyBeginAccess(
            wrapped_->backing(), AccessMode::kWrite,
            SharedImageAccessStream::kSkia)) {
      return {};
    }
    return wrapped_->BeginWriteAccess(final_msaa_count, surface_props,
                                      update_rect, begin_semaphores,
                                      end_semaphores, end_state);
  }
  std::vector<sk_sp<GrPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) final {
    if (!compound_backing()->NotifyBeginAccess(
            wrapped_->backing(), AccessMode::kWrite,
            SharedImageAccessStream::kSkia)) {
      return {};
    }
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
    if (!compound_backing()->NotifyBeginAccess(
            wrapped_->backing(), AccessMode::kRead,
            SharedImageAccessStream::kSkia)) {
      return {};
    }
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

  void OnContextLost() override {
    SkiaGaneshImageRepresentation::OnContextLost();
    wrapped_->OnContextLost();
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
      const gfx::Rect& update_rect,
      EndAccessState& end_access_state) final {
    if (!compound_backing()->NotifyBeginAccess(
            wrapped_->backing(), AccessMode::kWrite,
            SharedImageAccessStream::kSkia)) {
      return {};
    }
    return wrapped_->BeginWriteAccess(surface_props, update_rect,
                                      end_access_state);
  }
  std::vector<scoped_refptr<GraphiteTextureHolder>> BeginWriteAccess(
      EndAccessState& end_access_state) final {
    if (!compound_backing()->NotifyBeginAccess(
            wrapped_->backing(), AccessMode::kWrite,
            SharedImageAccessStream::kSkia)) {
      return {};
    }
    return wrapped_->BeginWriteAccess(end_access_state);
  }
  void EndWriteAccess() final {
    wrapped_->EndWriteAccess();
    compound_backing()->NotifyEndAccess(wrapped_->backing(),
                                        AccessMode::kWrite);
  }

  std::vector<scoped_refptr<GraphiteTextureHolder>> BeginReadAccess(
      EndAccessState& end_access_state) final {
    if (!compound_backing()->NotifyBeginAccess(
            wrapped_->backing(), AccessMode::kRead,
            SharedImageAccessStream::kSkia)) {
      return {};
    }
    return wrapped_->BeginReadAccess(end_access_state);
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

  void OnContextLost() override {
    SkiaGraphiteImageRepresentation::OnContextLost();
    wrapped_->OnContextLost();
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
    if (!compound_backing()->NotifyBeginAccess(
            wrapped_->backing(), access_mode, SharedImageAccessStream::kDawn)) {
      return nullptr;
    }
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

  void OnContextLost() override {
    DawnImageRepresentation::OnContextLost();
    wrapped_->OnContextLost();
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
    if (!compound_backing()->NotifyBeginAccess(
            wrapped_->backing(), access_mode,
            SharedImageAccessStream::kDawnBuffer)) {
      return nullptr;
    }
    access_mode_ = access_mode;
    return wrapped_->BeginAccess(usage);
  }

  void EndAccess() override {
    wrapped_->EndAccess();
    compound_backing()->NotifyEndAccess(wrapped_->backing(), access_mode_);
  }

  void OnContextLost() override {
    DawnBufferRepresentation::OnContextLost();
    wrapped_->OnContextLost();
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
    if (!compound_backing()->NotifyBeginAccess(
            wrapped_->backing(), AccessMode::kRead,
            SharedImageAccessStream::kOverlay)) {
      return false;
    }
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
  std::vector<gfx::MTLSharedEventFence> GetBackpressureFences() const final {
    return wrapped_->GetBackpressureFences();
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

  void OnContextLost() override {
    OverlayImageRepresentation::OnContextLost();
    wrapped_->OnContextLost();
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

  base::win::ScopedHandle GetD3D12HeapHandle() const final {
    return wrapped_->GetD3D12HeapHandle();
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

  void OnContextLost() override {
    WebNNTensorRepresentation::OnContextLost();
    wrapped_->OnContextLost();
  }

 private:
  CompoundImageBacking* compound_backing() {
    return static_cast<CompoundImageBacking*>(backing());
  }

  bool BeginAccess() override {
    if (!compound_backing()->NotifyBeginAccess(
            wrapped_->backing(), AccessMode::kWrite,
            SharedImageAccessStream::kWebNNTensor)) {
      return false;
    }
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
    if (!compound_backing()->NotifyBeginAccess(
            wrapped_->backing(), AccessMode::kRead,
            SharedImageAccessStream::kMemory)) {
      return SkPixmap();
    }
    return wrapped_->BeginReadAccess();
  }

  void SetClearedRect(const gfx::Rect& cleared_rect) override {
    SharedImageRepresentation::SetClearedRect(cleared_rect);
    if (owned_backing_) {
      wrapped_->SetClearedRect(cleared_rect);
    }
  }

  void OnContextLost() override {
    MemoryImageRepresentation::OnContextLost();
    wrapped_->OnContextLost();
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
    if (!compound_backing()->NotifyBeginAccess(
            wrapped_->backing(), AccessMode::kWrite,
            SharedImageAccessStream::kVaapi)) {
      return false;
    }
    return wrapped_->BeginWriteAccess();
  }
  void EndWriteAccess() override {
    wrapped_->EndWriteAccess();
    compound_backing()->NotifyEndAccess(wrapped_->backing(),
                                        AccessMode::kWrite);
  }
  bool BeginReadAccess() override {
    if (!compound_backing()->NotifyBeginAccess(
            wrapped_->backing(), AccessMode::kRead,
            SharedImageAccessStream::kVaapi)) {
      return false;
    }
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
  void EndWriteAccessVideo(scoped_refptr<gfx::D3DSharedFence> fence) override {
    wrapped_->EndWriteAccessVideo(std::move(fence));
    compound_backing()->NotifyEndAccess(wrapped_->backing(),
                                        AccessMode::kWrite);
  }
#endif
#if BUILDFLAG(IS_ANDROID)
  AHardwareBuffer* GetAHardwareBuffer() const override {
    return wrapped_->GetAHardwareBuffer();
  }
#endif
#if BUILDFLAG(IS_APPLE)
  gfx::ScopedIOSurface GetIOSurface() const override {
    return wrapped_->GetIOSurface();
  }
#endif

  void SetClearedRect(const gfx::Rect& cleared_rect) override {
    SharedImageRepresentation::SetClearedRect(cleared_rect);
    if (owned_backing_) {
      wrapped_->SetClearedRect(cleared_rect);
    }
  }

  void OnContextLost() override {
    VideoImageRepresentation::OnContextLost();
    wrapped_->OnContextLost();
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

  bool BeginAccess(
      AccessMode access_mode,
      std::vector<base::RawPtrIfPtrT<VkSemaphore, DanglingUntriaged>>&
          begin_semaphores,
      std::vector<base::RawPtrIfPtrT<VkSemaphore, DanglingUntriaged>>&
          end_semaphores) override {
    if (!compound_backing()->NotifyBeginAccess(
            wrapped_->backing(), access_mode,
            SharedImageAccessStream::kVulkan)) {
      return false;
    }
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

  void OnContextLost() override {
    VulkanImageRepresentation::OnContextLost();
    wrapped_->OnContextLost();
  }

 private:
  std::unique_ptr<SharedImageBacking> owned_backing_;
  std::unique_ptr<VulkanImageRepresentation> wrapped_;
};
#endif

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
std::unique_ptr<SharedImageBacking> CompoundImageBacking::Create(
    SharedImageFactory* shared_image_factory,
    const Mailbox& mailbox,
    const SharedImageInfo& si_info,
    SurfaceHandle surface_handle) {
  auto& format = si_info.format;
  auto& size = si_info.size;
  auto& usage = si_info.usage;
  auto* factory = shared_image_factory->GetFactoryByUsage(
      usage, format, size,
      /*pixel_data=*/{}, gfx::EMPTY_BUFFER,
      /*stream=*/std::nullopt, /*params=*/nullptr);
  if (!factory) {
    shared_image_factory->LogGetFactoryFailed(usage, format, gfx::EMPTY_BUFFER,
                                              size, si_info.debug_label);
    return nullptr;
  }

  auto temp_backing = factory->CreateSharedImage(
      mailbox, si_info, surface_handle,
      shared_image_factory->IsSharedBetweenThreads(usage));

  std::unique_ptr<SharedImageBacking> backing = CreateSingleElement(
      std::move(temp_backing), shared_image_factory->copy_manager(),
      shared_image_factory->GetFactoryRef());

  DVLOG_IF(1, !!backing) << "CreateSharedImage[" << backing->GetName()
                         << "] size=" << size.ToString()
                         << " usage=" << CreateLabelForSharedImageUsage(usage)
                         << " format=" << format.ToString();
  return backing;
}

// static
std::unique_ptr<SharedImageBacking> CompoundImageBacking::Create(
    SharedImageFactory* shared_image_factory,
    const Mailbox& mailbox,
    const SharedImageInfo& si_info,
    SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage) {
  auto& format = si_info.format;
  auto& size = si_info.size;
  auto& usage = si_info.usage;
  auto& debug_label = si_info.debug_label;
  auto native_buffer_supported =
      SharedImageFactory::IsNativeBufferSupported(format, buffer_usage);
  std::unique_ptr<SharedImageBacking> backing;

  if (native_buffer_supported) {
    auto* factory = shared_image_factory->GetFactoryByUsage(
        usage, format, size,
        /*pixel_data=*/{}, GetNativeBufferType(),
        /*stream=*/std::nullopt,
        /*params=*/nullptr);
    if (!factory) {
      shared_image_factory->LogGetFactoryFailed(
          usage, format, GetNativeBufferType(), size, debug_label);
      return nullptr;
    }

    auto temp_backing = factory->CreateSharedImage(
        mailbox, si_info, surface_handle,
        shared_image_factory->IsSharedBetweenThreads(usage), buffer_usage);

    backing = CreateSingleElement(std::move(temp_backing),
                                  shared_image_factory->copy_manager(),
                                  shared_image_factory->GetFactoryRef());

    DVLOG_IF(1, !!backing) << "CreateSharedImageBackedByBuffer["
                           << backing->GetName() << "] size=" << size.ToString()
                           << " usage=" << CreateLabelForSharedImageUsage(usage)
                           << " format=" << format.ToString();
  } else {
    // If native buffers are not supported, try to create shared memory based
    // backings.
    if (SharedMemoryImageBackingFactory::IsBufferUsageSupported(buffer_usage) &&
        SharedMemoryImageBackingFactory::IsSizeValidForFormat(size, format)) {
      // Clear the external sampler prefs for shared memory case if it is set.
      // https://issues.chromium.org/339546249.
      SharedImageInfo si_info_copy = si_info;
      if (si_info_copy.format.PrefersExternalSampler()) {
        si_info_copy.format.ClearPrefersExternalSampler();
        if (si_info_copy.alpha_type == kPremul_SkAlphaType) {
          si_info_copy.alpha_type = kUnpremul_SkAlphaType;
        }
      }
      backing = CreateSharedMemory(shared_image_factory, mailbox, si_info_copy,
                                   gfx::GpuMemoryBufferHandle(), buffer_usage);
      if (!backing) {
        shared_image_factory->LogGetFactoryFailed(usage, si_info_copy.format,
                                                  gfx::SHARED_MEMORY_BUFFER,
                                                  size, debug_label);
        return nullptr;
      }
      DVLOG_IF(1, !!backing)
          << "CreateSharedImageBackedByBuffer[" << backing->GetName()
          << "] size=" << size.ToString()
          << " usage=" << CreateLabelForSharedImageUsage(usage)
          << " format=" << si_info_copy.format.ToString();
    }
  }
  return backing;
}

// static
std::unique_ptr<SharedImageBacking> CompoundImageBacking::Create(
    SharedImageFactory* shared_image_factory,
    const Mailbox& mailbox,
    const SharedImageInfo& si_info,
    base::span<const uint8_t> pixel_data) {
  auto& format = si_info.format;
  auto& size = si_info.size;
  auto& usage = si_info.usage;
  if (!format.is_single_plane()) {
    // Pixel upload path only supports single-planar formats.
    LOG(ERROR) << "Invalid format " << format.ToString();
    return nullptr;
  }

  SharedImageBackingFactory* const factory =
      shared_image_factory->GetFactoryByUsage(
          usage, format, size, pixel_data, gfx::EMPTY_BUFFER,
          /*stream=*/std::nullopt, /*params=*/nullptr);
  if (!factory) {
    shared_image_factory->LogGetFactoryFailed(usage, format, gfx::EMPTY_BUFFER,
                                              size, si_info.debug_label);
    return nullptr;
  }

  auto temp_backing = factory->CreateSharedImage(
      mailbox, si_info, shared_image_factory->IsSharedBetweenThreads(usage),
      pixel_data);

  std::unique_ptr<SharedImageBacking> backing = CreateSingleElement(
      std::move(temp_backing), shared_image_factory->copy_manager(),
      shared_image_factory->GetFactoryRef());

  DVLOG_IF(1, !!backing) << "CreateSharedImagePixels[" << backing->GetName()
                         << "] with pixels size=" << size.ToString()
                         << " usage=" << CreateLabelForSharedImageUsage(usage)
                         << " format=" << format.ToString();

  return backing;
}

// static
std::unique_ptr<SharedImageBacking> CompoundImageBacking::Create(
    SharedImageFactory* shared_image_factory,
    const Mailbox& mailbox,
    const SharedImageInfo& si_info,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  auto& format = si_info.format;
  auto& size = si_info.size;
  auto& usage = si_info.usage;
  auto& debug_label = si_info.debug_label;
  gfx::GpuMemoryBufferType gmb_type = buffer_handle.type;

  std::unique_ptr<SharedImageBacking> backing;
  if (gmb_type == gfx::SHARED_MEMORY_BUFFER) {
    backing = CreateSharedMemory(shared_image_factory, mailbox, si_info,
                                 std::move(buffer_handle));
    if (!backing) {
      shared_image_factory->LogGetFactoryFailed(usage, format, gmb_type, size,
                                                debug_label);
      return nullptr;
    }
  } else {
    SharedImageBackingFactory* factory = nullptr;
#if BUILDFLAG(IS_ANDROID)
    if (shared_image_factory->ahb_factory_ &&
        shared_image_factory->ahb_factory_->IsSupportedForMappableBuffer(
            usage, format, gmb_type)) {
      factory = shared_image_factory->ahb_factory_;
    }
#endif  // BUILDFLAG(IS_ANDROID)
    if (!factory) {
      factory = shared_image_factory->GetFactoryByUsage(
          usage, format, size, /*pixel_data=*/{}, gmb_type,
          /*stream=*/std::nullopt, /*params=*/nullptr);
    }
    if (!factory) {
      shared_image_factory->LogGetFactoryFailed(usage, format, gmb_type, size,
                                                debug_label);
      return nullptr;
    }

    auto temp_backing = factory->CreateSharedImage(
        mailbox, si_info, shared_image_factory->IsSharedBetweenThreads(usage),
        std::move(buffer_handle));

    backing = CreateSingleElement(std::move(temp_backing),
                                  shared_image_factory->copy_manager(),
                                  shared_image_factory->GetFactoryRef());
  }

  DVLOG_IF(1, !!backing) << "CreateSharedImageWithBuffer[" << backing->GetName()
                         << "] size=" << size.ToString()
                         << " usage=" << CreateLabelForSharedImageUsage(usage)
                         << " format=" << format.ToString() << " gmb_type="
                         << SharedImageFactory::GmbTypeToString(gmb_type);

  return backing;
}

// static
std::unique_ptr<SharedImageBacking> CompoundImageBacking::CreateSharedMemory(
    SharedImageFactory* shared_image_factory,
    const Mailbox& mailbox,
    const SharedImageInfo& si_info,
    gfx::GpuMemoryBufferHandle handle,
    std::optional<gfx::BufferUsage> buffer_usage) {
  // SharedMemoryImageBacking does not support texture arrays.
  if (si_info.array_layers > 1 ||
      !IsValidSharedMemoryFormat(si_info.size, si_info.format)) {
    return nullptr;
  }

  // When GPU usages are present alongside shared memory, an accompanying GPU
  // backing factory is required and cross-thread GPU access is not supported
  // when combining shared memory with a lazy GPU backing.
  const bool has_gpu_usage = HasGpuSharedImageUsage(si_info.usage);
  if (has_gpu_usage &&
      shared_image_factory->IsSharedBetweenThreads(si_info.usage)) {
    return nullptr;
  }

  base::WeakPtr<SharedImageBackingFactory> gpu_backing_factory;
  if (has_gpu_usage) {
    auto* factory = shared_image_factory->GetFactoryByUsage(
        GetGpuSharedImageUsage(si_info.usage), si_info.format, si_info.size,
        /*pixel_data=*/{}, gfx::EMPTY_BUFFER, /*stream=*/std::nullopt,
        /*params=*/nullptr);
    if (!factory) {
      return nullptr;
    }
    gpu_backing_factory = factory->GetWeakPtr();
  }

  return CreateSharedMemoryInternal(
      mailbox, si_info, std::move(handle), std::move(buffer_usage),
      std::move(gpu_backing_factory), shared_image_factory->copy_manager(),
      shared_image_factory->GetFactoryRef());
}

// static
std::unique_ptr<SharedImageBacking>
CompoundImageBacking::CreateSharedMemoryInternal(
    const Mailbox& mailbox,
    const SharedImageInfo& si_info,
    gfx::GpuMemoryBufferHandle handle,
    std::optional<gfx::BufferUsage> buffer_usage,
    base::WeakPtr<SharedImageBackingFactory> gpu_backing_factory,
    scoped_refptr<SharedImageCopyManager> copy_manager,
    scoped_refptr<SharedImageFactoryRef> shared_image_factory) {
  if (si_info.array_layers > 1 ||
      !IsValidSharedMemoryFormat(si_info.size, si_info.format)) {
    return nullptr;
  }

  SharedImageInfo shm_si_info(si_info.format, si_info.size, si_info.color_space,
                              si_info.surface_origin, si_info.alpha_type,
                              GetShmSharedImageUsage(si_info.usage),
                              si_info.debug_label);
  std::unique_ptr<SharedImageBacking> shm_backing =
      buffer_usage.has_value()
          ? SharedMemoryImageBackingFactory().CreateSharedImage(
                mailbox, shm_si_info, kNullSurfaceHandle,
                /*is_thread_safe=*/false, *buffer_usage)
          : SharedMemoryImageBackingFactory().CreateSharedImage(
                mailbox, shm_si_info, /*is_thread_safe=*/false,
                std::move(handle));
  if (!shm_backing) {
    return nullptr;
  }

  ElementHolder shm_element;
  shm_element.access_streams = kMemoryStreamSet;
  if (kAllowShmOverlays && si_info.usage.Has(gpu::SHARED_IMAGE_USAGE_SCANOUT)) {
    shm_element.access_streams.Put(SharedImageAccessStream::kOverlay);
  }
  shm_element.backing = std::move(shm_backing);

  auto compound = base::WrapUnique(new CompoundImageBacking(
      mailbox, si_info, std::move(shm_element), std::move(copy_manager),
      std::move(shared_image_factory), std::move(buffer_usage)));
  if (gpu_backing_factory) {
    compound->AddLazyGpuElement(std::move(gpu_backing_factory),
                                GetGpuSharedImageUsage(si_info.usage),
                                si_info.debug_label);
  }
  return compound;
}

// static
std::unique_ptr<CompoundImageBacking> CompoundImageBacking::CreateSingleElement(
    std::unique_ptr<SharedImageBacking> backing,
    scoped_refptr<SharedImageCopyManager> copy_manager,
    scoped_refptr<SharedImageFactoryRef> shared_image_factory) {
  if (!backing) {
    return nullptr;
  }

  const Mailbox mailbox = backing->mailbox();
  const SharedImageInfo si_info(
      backing->format(), backing->size(), backing->color_space(),
      backing->surface_origin(), backing->alpha_type(), backing->usage(),
      backing->debug_label(), backing->array_layers());
  std::optional<gfx::BufferUsage> buffer_usage = backing->buffer_usage();

  // Delegate access stream support checks to the inner backing via
  // SupportsAccess() / Produce*() rather than pre-filtering streams here.
  ElementHolder element;
  element.access_streams = AccessStreamSet::All();
  element.backing = std::move(backing);

  return base::WrapUnique(new CompoundImageBacking(
      mailbox, si_info, std::move(element), std::move(copy_manager),
      std::move(shared_image_factory), std::move(buffer_usage)));
}

// static
std::unique_ptr<SharedImageBacking>
CompoundImageBacking::CreateSharedMemoryForTesting(
    SharedImageBackingFactory* gpu_backing_factory,
    scoped_refptr<SharedImageCopyManager> copy_manager,
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    const SharedImageInfo& si_info) {
  DCHECK(IsValidSharedMemoryFormat(si_info.size, si_info.format));
  return CreateSharedMemoryInternal(
      mailbox, si_info, std::move(handle), /*buffer_usage=*/std::nullopt,
      gpu_backing_factory ? gpu_backing_factory->GetWeakPtr() : nullptr,
      std::move(copy_manager), /*shared_image_factory=*/nullptr);
}

// static
std::unique_ptr<SharedImageBacking>
CompoundImageBacking::CreateSharedMemoryForTesting(
    SharedImageBackingFactory* gpu_backing_factory,
    scoped_refptr<SharedImageCopyManager> copy_manager,
    const Mailbox& mailbox,
    const SharedImageInfo& si_info,
    gfx::BufferUsage buffer_usage,
    scoped_refptr<SharedImageFactoryRef> shared_image_factory) {
  DCHECK(IsValidSharedMemoryFormat(si_info.size, si_info.format));
  return CreateSharedMemoryInternal(
      mailbox, si_info, gfx::GpuMemoryBufferHandle(), buffer_usage,
      gpu_backing_factory ? gpu_backing_factory->GetWeakPtr() : nullptr,
      std::move(copy_manager), std::move(shared_image_factory));
}

CompoundImageBacking::CompoundImageBacking(
    const Mailbox& mailbox,
    const SharedImageInfo& si_info,
    ElementHolder initial_element,
    scoped_refptr<SharedImageCopyManager> copy_manager,
    scoped_refptr<SharedImageFactoryRef> shared_image_factory,
    std::optional<gfx::BufferUsage> buffer_usage)
    : ClearTrackingSharedImageBacking(
          mailbox,
          si_info,
          initial_element.backing->GetEstimatedSize(),
          ComputeIsThreadSafe(shared_image_factory.get(), si_info.usage),
          std::move(buffer_usage)),
      shared_image_factory_(std::move(shared_image_factory)),
      copy_manager_(std::move(copy_manager)) {
  CHECK(initial_element.backing);
  CHECK_NE(initial_element.backing->GetType(),
           SharedImageBackingType::kCompound);
  DCHECK_EQ(si_info.size, initial_element.backing->size());

  initial_element.backing->SetNotRefCounted();
  RecordBackingTypeUMA(initial_element.backing->GetType());
  SetClearedRectInternal(initial_element.backing->ClearedRect());

  initial_element.content_id_ = latest_content_id_;
  elements_.push_back(std::move(initial_element));
  max_elements_allocated_ = elements_.size();
}

void CompoundImageBacking::AddLazyGpuElement(
    base::WeakPtr<SharedImageBackingFactory> gpu_backing_factory,
    SharedImageUsageSet gpu_usage,
    std::string debug_label) {
  AutoLock auto_lock(this);
  CHECK(gpu_backing_factory);

  // Create placeholder for GPU-backed element (streams = all except kMemory).
  ElementHolder gpu_element;
  gpu_element.access_streams =
      base::Difference(AccessStreamSet::All(), kMemoryStreamSet);

  SharedImageBackingType factory_type = gpu_backing_factory->GetBackingType();
  gpu_element.create_callback = base::BindOnce(
      &CompoundImageBacking::LazyCreateBacking, base::Unretained(this),
      factory_type, std::move(gpu_backing_factory), gpu_usage,
      std::move(debug_label));
  elements_.push_back(std::move(gpu_element));
  max_elements_allocated_ = elements_.size();
}

CompoundImageBacking::~CompoundImageBacking() {
  UMA_HISTOGRAM_COUNTS_100("GPU.CompoundImageBacking.TotalElementsAllocated",
                           max_elements_allocated_);
  if (pending_copy_to_gmb_callback_) {
    std::move(pending_copy_to_gmb_callback_).Run(/*success=*/false);
  }
}

bool CompoundImageBacking::NotifyBeginAccess(SharedImageBacking* backing,
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
    return true;
  }

  // STALE OR TRANSIENT PATH: Either this permanent backing is out-of-date, or
  // it's a transient backing that needs to be initialized. We must find the
  // permanent element that currently holds the latest content and copy from it.
  ElementHolder* latest_content_element =
      GetElementWithLatestContent(/*exclude_element=*/access_element);
  if (!latest_content_element) {
    // No permanent element holds the most recent content, so the destination
    // backing cannot be synchronized. This can happen if a transient backing
    // was written to but its content could not be copied back to a permanent
    // element on end access. Do not advance versioning so access can recover if
    // an element re-establishes the latest content (e.g. via Update() for SHM
    // backings). Note that if there is no SHM backing (e.g. GPU-only backings)
    // or no Update() occurs, subsequent access calls (including writes) will
    // continue to fail, requiring client-level context/resource re-creation.
    LOG(ERROR) << "No element with latest content available for sync to "
               << backing->GetName();
    return false;
  }

  bool copy_succeeded = false;

  // Skip copying if the source backing has never been cleared, as there is no
  // initialized pixel data to synchronize.
  if (!latest_content_element->GetBacking()->ClearedRect().IsEmpty()) {
    // Copy data from the latest permanent backing into the current backing
    // (which could be another permanent backing or a transient one).
    copy_succeeded = copy_manager_->CopyImage(
        /*src_backing=*/latest_content_element->GetBacking(),
        /*dst_backing=*/backing);

    if (copy_succeeded) {
      // Sync the clear rect from the source backing to the destination backing
      // and the container itself.
      const gfx::Rect src_cleared_rect =
          latest_content_element->GetBacking()->ClearedRect();
      SetClearedRectInternal(src_cleared_rect);
      backing->SetClearedRect(src_cleared_rect);
    } else {
      LOG(ERROR) << "Failed to copy from "
                 << latest_content_element->GetBacking()->GetName() << " to "
                 << backing->GetName();
    }

    const bool is_src_dynamic =
        latest_content_element->is_dynamically_allocated;
    const bool is_dst_dynamic =
        !access_element || access_element->is_dynamically_allocated;
    const int32_t initial_usage =
        static_cast<int32_t>(static_cast<uint32_t>(this->usage()));
    // Encoded as SSDDAA:
    //   SS = src_type (SharedImageBackingType: 00..19)
    //   DD = dst_type (SharedImageBackingType: 00..19)
    //   AA = stream   (SharedImageAccessStream: 00..08)
    const int32_t copy_info =
        static_cast<int32_t>(latest_content_element->GetBacking()->GetType()) *
            10000 +
        static_cast<int32_t>(backing->GetType()) * 100 +
        static_cast<int32_t>(stream);

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
    if (is_src_dynamic || is_dst_dynamic) {
      UMA_HISTOGRAM_SPARSE(
          "GPU.CompoundImageBacking.ContentSync.Dynamic.CopyInfo", copy_info);
      UMA_HISTOGRAM_SPARSE(
          "GPU.CompoundImageBacking.ContentSync.Dynamic."
          "InitialSharedImageUsage",
          initial_usage);
    } else {
      UMA_HISTOGRAM_SPARSE(
          "GPU.CompoundImageBacking.ContentSync.Static.CopyInfo", copy_info);
      UMA_HISTOGRAM_SPARSE(
          "GPU.CompoundImageBacking.ContentSync.Static."
          "InitialSharedImageUsage",
          initial_usage);
    }

    if (!copy_succeeded) {
      // The destination backing was not updated so it must not be exposed to
      // the caller. Do not advance versioning so a future access can retry
      // the sync.
      return false;
    }
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
  } else if (copy_succeeded && access_element) {
    access_element->content_id_ = latest_content_id_;
  }
  return true;
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
    if (cleared_rect != ClearedRectInternal()) {
      // For transient backings, only update if it's more cleared to avoid
      // overwriting CSI with stale state. Check `ClearedRect().IsEmpty()`
      // explicitly as `Contains(gfx::Rect())` is false for non-zero origins.
      if (!is_transient_backing || ClearedRectInternal().IsEmpty() ||
          cleared_rect.Contains(ClearedRectInternal())) {
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
            // Propagate the cleared rect after the copy.
            dst_backing->SetClearedRect(ClearedRectInternal());
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

void CompoundImageBacking::Update(gfx::GpuFenceHandle in_fence) {
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
  if (elements_.empty() || !elements_[0].backing ||
      elements_[0].backing->GetType() !=
          SharedImageBackingType::kSharedMemory) {
    return false;
  }
  auto& shm_element = elements_[0];
  if (HasLatestContent(shm_element)) {
    return true;
  }

  ElementHolder* src_element =
      GetElementWithLatestContent(/*exclude_element=*/&shm_element);
  if (!src_element || !copy_manager_->CopyImage(src_element->GetBacking(),
                                                shm_element.backing.get())) {
    LOG(ERROR) << "Failed to copy to shared memory";
    return false;
  }

  shm_element.content_id_ = latest_content_id_;
  shm_element.backing->SetClearedRect(ClearedRectInternal());
  return true;
}

void CompoundImageBacking::CopyToGpuMemoryBufferAsync(
    base::OnceCallback<void(bool)> callback) {
  SharedImageBacking* src_backing = nullptr;
  SharedImageBacking* shm_backing = nullptr;
  bool immediate_success = false;
  {
    AutoLock auto_lock(this);
    if (elements_.empty() || !elements_[0].backing ||
        elements_[0].backing->GetType() !=
            SharedImageBackingType::kSharedMemory) {
      DLOG(ERROR)
          << "CopyToGpuMemoryBufferAsync called without shared memory backing";
    } else {
      auto& shm_element = elements_[0];
      if (HasLatestContent(shm_element)) {
        immediate_success = true;
      } else if (pending_copy_to_gmb_callback_) {
        DLOG(ERROR) << "Existing CopyToGpuMemoryBuffer operation pending";
      } else if (ElementHolder* src_element = GetElementWithLatestContent(
                     /*exclude_element=*/&shm_element)) {
        src_backing = src_element->GetBacking();
        shm_backing = shm_element.backing.get();
        pending_copy_to_gmb_callback_ = std::move(callback);
      } else {
        LOG(ERROR) << "Failed to copy to shared memory";
      }
    }
  }

  if (!src_backing) {
    std::move(callback).Run(immediate_success);
    return;
  }

  copy_manager_->CopyImageAsync(
      src_backing, shm_backing,
      base::BindOnce(&CompoundImageBacking::OnCopyToGpuMemoryBufferComplete,
                     base::Unretained(this)));
}

void CompoundImageBacking::OnCopyToGpuMemoryBufferComplete(bool success) {
  base::OnceCallback<void(bool)> callback;
  {
    AutoLock auto_lock(this);
    if (success) {
      auto& shm_element = elements_[0];
      shm_element.content_id_ = latest_content_id_;
      shm_element.backing->SetClearedRect(ClearedRectInternal());
    }
    callback = std::move(pending_copy_to_gmb_callback_);
  }
  if (callback) {
    std::move(callback).Run(success);
  }
}

void CompoundImageBacking::SetClearedRect(const gfx::Rect& cleared_rect) {
  AutoLock auto_lock(this);
  SetClearedRectInternal(cleared_rect);

  // Only mark backings that hold the latest content as cleared so stale
  // backings aren't marked cleared before their pixels are synced. When
  // resetting to empty (e.g. WEBGPU_MAILBOX_DISCARD before BeginAccess), reset
  // all backings so none retain an old cleared state.
  for (auto& element : elements_) {
    if (element.backing &&
        (HasLatestContent(element) || cleared_rect.IsEmpty())) {
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

#if BUILDFLAG(IS_ANDROID)
std::optional<VulkanYCbCrInfo> CompoundImageBacking::GetVkCbCrInfo(
    SharedContextState* context_state) {
  AutoLock auto_lock(this);
  for (const auto& element : elements_) {
    if (element.backing) {
      auto info = element.backing->GetVkCbCrInfo(context_state);
      if (info) {
        return info;
      }
    }
  }
  return std::nullopt;
}
#endif

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
  auto* backing = GetOrAllocateBacking(SharedImageAccessStream::kVaapi,
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
CompoundImageBacking::GetElementWithLatestContent(
    const ElementHolder* exclude_element) {
  // Note that for now iterating over all elements should be fine since we would
  // likely not ever had too many backings existing concurrently. We can
  // optimize this code by using better algorithm or more suitable data
  // structure later if needed.
  for (auto& element : elements_) {
    if (&element != exclude_element &&
        element.content_id_ == latest_content_id_ && element.GetBacking()) {
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
  std::optional<SharedImageBackingType> incompatible_backing_type;

  // Note that for now iterating over all elements should be fine since we would
  // likely not ever had too many backings existing concurrently. We can
  // optimize this code by using better algorithm or more suitable data
  // structure later if needed.
  for (auto& element : elements_) {
    if (element.access_streams.Has(stream) && element.GetBacking()) {
      if (element.GetBacking()->SupportsAccess(stream, params)) {
        if (element.content_id_ == latest_content_id_) {
          best_match = &element;
          break;
        }
        if (!any_match) {
          any_match = &element;
        }
      } else if (!incompatible_backing_type) {
        incompatible_backing_type = element.GetBacking()->GetType();
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
    // When the original format has PrefersExternalSampler() set(which can be
    // only set for multiplanar formats), then per plane texture access is not
    // allowed. It can be only sampled as single RGBA texture on the GPU.
    // When a secondary or transient backings is allocated dynamically, it
    // requires data to be copied from existing backing into this new backing.
    // Since data from existing backing can only be sampled as RGBA, the new
    // backing must also be of RGBA format in order to receive readback data as
    // RGBA. Therefore, map such formats to single-plane RGBA based on channel
    // format for dynamic backing allocation.
    viz::SharedImageFormat backing_format = GetEffectiveCopyFormat(format());
    SharedImageUsageSet usage = GetUsageFromAccessStream(stream, params);
    std::unique_ptr<SharedImageBacking> new_backing;
    shared_image_factory_->Execute([&](SharedImageFactory* factory) {
      SharedImageBackingFactory* gpu_backing_factory =
          factory->GetFactoryByUsage(usage, backing_format, size(),
                                     /*pixel_data=*/{}, gfx::EMPTY_BUFFER,
                                     stream, &params);
      if (gpu_backing_factory) {
        CreateBackingFromBackingFactory(gpu_backing_factory, debug_label(),
                                        usage, backing_format, new_backing);
      }
    });

    UMA_HISTOGRAM_BOOLEAN("GPU.CompoundImageBacking.DynamicAllocation.Success",
                          !!new_backing);

    if (new_backing) {
      if (format().PrefersExternalSampler()) {
        CHECK_EQ(new_backing->format(), backing_format);
      }
      // If the CSI container is thread-safe, we treat newly created backings
      // as transient if they are not thread-safe. They will be owned by the
      // representation and destroyed after use. This ensures that a
      // non-thread-safe backing allocated on one thread doesn't persist in
      // the thread-safe container, which could lead to race conditions if
      // accessed from another thread later.
      const bool is_transient =
          is_thread_safe() && !new_backing->is_thread_safe();
      UMA_HISTOGRAM_ENUMERATION(
          "GPU.CompoundImageBacking.DynamicAllocation.BackingType",
          new_backing->GetType());
      if (incompatible_backing_type) {
        UMA_HISTOGRAM_ENUMERATION(
            "GPU.CompoundImageBacking.DynamicAllocation."
            "IncompatibleExistingBackingType",
            *incompatible_backing_type);
      }
      const bool has_shm_backing = !elements_.empty() && elements_[0].backing &&
                                   elements_[0].backing->GetType() ==
                                       SharedImageBackingType::kSharedMemory;
      UMA_HISTOGRAM_BOOLEAN(
          "GPU.CompoundImageBacking.DynamicAllocation.HasSharedMemoryBacking",
          has_shm_backing);
      UMA_HISTOGRAM_BOOLEAN(
          "GPU.CompoundImageBacking.DynamicAllocation.IsTransientBacking",
          is_transient);
      UMA_HISTOGRAM_ENUMERATION(
          "GPU.CompoundImageBacking.DynamicAllocation.AccessStream", stream);
      UMA_HISTOGRAM_SPARSE(
          "GPU.CompoundImageBacking.DynamicAllocation."
          "InitialSharedImageUsage",
          static_cast<int32_t>(static_cast<uint32_t>(this->usage())));

      if (is_transient) {
        out_transient_backing = std::move(new_backing);
        return out_transient_backing.get();
      }

      // Else we treat the backing as non-transient and add it to the list of
      // alive elements. This is done when either the container is not
      // thread-safe or the new backing itself is thread-safe.
      ElementHolder element;
      element.access_streams.Put(stream);
      element.backing = std::move(new_backing);
      element.is_dynamically_allocated = true;
      elements_.push_back(std::move(element));
      if (elements_.size() > max_elements_allocated_) {
        max_elements_allocated_ = elements_.size();
      }
      return elements_.back().GetBacking();
    }
  }

  LOG(ERROR) << "Could not find or create a backing for stream " << stream;
  return nullptr;
}

void CompoundImageBacking::CreateBackingFromBackingFactory(
    SharedImageBackingFactory* backing_factory,
    std::string debug_label,
    SharedImageUsageSet usage,
    viz::SharedImageFormat backing_format,
    std::unique_ptr<SharedImageBacking>& backing) {
  // This method assumes the caller has already ensured the factory is alive
  // and synchronized (e.g. by holding the SharedImageFactoryRef lock).
  CHECK(backing_factory);

  // The new backing must describe the same image as this compound, including
  // its layer count.
  SharedImageInfo si_info(backing_format, size(), color_space(),
                          surface_origin(), alpha_type(), usage,
                          std::move(debug_label), array_layers());
  backing =
      backing_factory->CreateSharedImage(mailbox(), si_info, kNullSurfaceHandle,
                                         /*is_thread_safe=*/false);
  if (!backing) {
    DLOG(ERROR) << "Failed to allocate GPU backing";
    return;
  }

  // Since the owned GPU backing is never registered with SharedImageManager
  // it's not recorded in UMA histogram there.
  RecordBackingTypeUMA(backing->GetType());

  backing->SetNotRefCounted();

  // Update peak GPU memory tracking with the new estimated size.
  size_t estimated_size = 0;
  for (auto& element : elements_) {
    if (element.backing) {
      estimated_size += element.backing->GetEstimatedSize();
    }
  }

  UpdateEstimatedSize(estimated_size);
}

void CompoundImageBacking::LazyCreateBacking(
    SharedImageBackingType factory_type,
    base::WeakPtr<SharedImageBackingFactory> test_factory,
    SharedImageUsageSet usage,
    std::string debug_label,
    std::unique_ptr<SharedImageBacking>& backing) {
  // This method provides a thread-safe way to lazily create a backing.
  // It uses the factory type to re-identify the correct factory under a lock,
  // avoiding the use of thread-affine WeakPtrs in production.
  if (shared_image_factory_) {
    shared_image_factory_->Execute([&](SharedImageFactory* factory) {
      // Find the factory by type while holding the lock to ensure its lifetime.
      auto* bf = factory->GetFactoryByType(factory_type);
      if (bf) {
        CreateBackingFromBackingFactory(bf, std::move(debug_label), usage,
                                        format(), backing);
      }
    });
  } else if (test_factory) {
    // Fallback for tests where SharedImageFactoryRef is not present.
    // These tests are currently single-threaded.
    CHECK_IS_TEST();
    CreateBackingFromBackingFactory(test_factory.get(), std::move(debug_label),
                                    usage, format(), backing);
  }
}

bool CompoundImageBacking::HasLatestContent(
    const ElementHolder& element) const {
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
