// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/compound_image_backing.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/process_memory_dump.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_memory_image_backing.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "third_party/skia/include/gpu/graphite/BackendTexture.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

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

#if BUILDFLAG(IS_WIN)
// Only allow shmem overlays for NV12 on Windows.
// This moves the SCANOUT flag from the GPU backing to the shmem backing in the
// CompoundImageBacking.
constexpr bool kAllowShmOverlays = true;
#else
constexpr bool kAllowShmOverlays = false;
#endif

gpu::SharedImageUsageSet GetShmSharedImageUsage(SharedImageUsageSet usage) {
  if (kAllowShmOverlays) {
    return usage.Has(gpu::SHARED_IMAGE_USAGE_SCANOUT)
               ? SHARED_IMAGE_USAGE_CPU_WRITE | SHARED_IMAGE_USAGE_SCANOUT
               : SHARED_IMAGE_USAGE_CPU_WRITE;
  }

  return SHARED_IMAGE_USAGE_CPU_WRITE;
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
      std::unique_ptr<GLTextureImageRepresentation> wrapped)
      : GLTextureImageRepresentation(manager, backing, tracker),
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
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kGL,
                                          access_mode);
    return wrapped_->BeginAccess(mode);
  }

  void EndAccess() final { wrapped_->EndAccess(); }

  gpu::TextureBase* GetTextureBase(int plane_index) final {
    return wrapped_->GetTextureBase(plane_index);
  }

  bool SupportsMultipleConcurrentReadAccess() final {
    return wrapped_->SupportsMultipleConcurrentReadAccess();
  }

  gles2::Texture* GetTexture(int plane_index) final {
    return wrapped_->GetTexture(plane_index);
  }

 private:
  std::unique_ptr<GLTextureImageRepresentation> wrapped_;
};

class WrappedGLTexturePassthroughCompoundImageRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  WrappedGLTexturePassthroughCompoundImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<GLTexturePassthroughImageRepresentation> wrapped)
      : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
        wrapped_(std::move(wrapped)) {
    DCHECK(wrapped_);
  }

  CompoundImageBacking* compound_backing() {
    return static_cast<CompoundImageBacking*>(backing());
  }

  // GLTexturePassthroughImageRepresentation implementation.
  bool BeginAccess(GLenum mode) final {
    AccessMode access_mode =
        mode == kReadAccessMode ? AccessMode::kRead : AccessMode::kWrite;
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kGL,
                                          access_mode);
    return wrapped_->BeginAccess(mode);
  }
  void EndAccess() final { wrapped_->EndAccess(); }

  gpu::TextureBase* GetTextureBase(int plane_index) final {
    return wrapped_->GetTextureBase(plane_index);
  }

  bool SupportsMultipleConcurrentReadAccess() final {
    return wrapped_->SupportsMultipleConcurrentReadAccess();
  }

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) final {
    return wrapped_->GetTexturePassthrough(plane_index);
  }

 private:
  std::unique_ptr<GLTexturePassthroughImageRepresentation> wrapped_;
};

class WrappedSkiaGaneshCompoundImageRepresentation
    : public SkiaGaneshImageRepresentation {
 public:
  WrappedSkiaGaneshCompoundImageRepresentation(
      GrDirectContext* gr_context,
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<SkiaGaneshImageRepresentation> wrapped)
      : SkiaGaneshImageRepresentation(gr_context, manager, backing, tracker),
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
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kSkia,
                                          AccessMode::kWrite);
    return wrapped_->BeginWriteAccess(final_msaa_count, surface_props,
                                      update_rect, begin_semaphores,
                                      end_semaphores, end_state);
  }
  std::vector<sk_sp<GrPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) final {
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kSkia,
                                          AccessMode::kWrite);
    return wrapped_->BeginWriteAccess(begin_semaphores, end_semaphores,
                                      end_state);
  }
  void EndWriteAccess() final { wrapped_->EndWriteAccess(); }

  std::vector<sk_sp<GrPromiseImageTexture>> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) final {
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kSkia,
                                          AccessMode::kRead);
    return wrapped_->BeginReadAccess(begin_semaphores, end_semaphores,
                                     end_state);
  }
  void EndReadAccess() final { wrapped_->EndReadAccess(); }

 private:
  std::unique_ptr<SkiaGaneshImageRepresentation> wrapped_;
};

class WrappedSkiaGraphiteCompoundImageRepresentation
    : public SkiaGraphiteImageRepresentation {
 public:
  WrappedSkiaGraphiteCompoundImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<SkiaGraphiteImageRepresentation> wrapped)
      : SkiaGraphiteImageRepresentation(manager, backing, tracker),
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

  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect) final {
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kSkia,
                                          AccessMode::kWrite);
    return wrapped_->BeginWriteAccess(surface_props, update_rect);
  }
  std::vector<skgpu::graphite::BackendTexture> BeginWriteAccess() final {
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kSkia,
                                          AccessMode::kWrite);
    return wrapped_->BeginWriteAccess();
  }
  void EndWriteAccess() final { wrapped_->EndWriteAccess(); }

  std::vector<skgpu::graphite::BackendTexture> BeginReadAccess() final {
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kSkia,
                                          AccessMode::kRead);
    return wrapped_->BeginReadAccess();
  }
  void EndReadAccess() final { wrapped_->EndReadAccess(); }

 private:
  std::unique_ptr<SkiaGraphiteImageRepresentation> wrapped_;
};

class WrappedDawnCompoundImageRepresentation : public DawnImageRepresentation {
 public:
  WrappedDawnCompoundImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<DawnImageRepresentation> wrapped)
      : DawnImageRepresentation(manager, backing, tracker),
        wrapped_(std::move(wrapped)) {
    DCHECK(wrapped_);
  }

  CompoundImageBacking* compound_backing() {
    return static_cast<CompoundImageBacking*>(backing());
  }

  // DawnImageRepresentation implementation.
  wgpu::Texture BeginAccess(wgpu::TextureUsage webgpu_usage,
                            wgpu::TextureUsage internal_usage) final {
    AccessMode access_mode =
        webgpu_usage & kWriteUsage ? AccessMode::kWrite : AccessMode::kRead;
    if (internal_usage & kWriteUsage) {
      access_mode = AccessMode::kWrite;
    }
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kDawn,
                                          access_mode);
    return wrapped_->BeginAccess(webgpu_usage, internal_usage);
  }
  void EndAccess() final { wrapped_->EndAccess(); }

 private:
  std::unique_ptr<DawnImageRepresentation> wrapped_;
};

class WrappedOverlayCompoundImageRepresentation
    : public OverlayImageRepresentation {
 public:
  WrappedOverlayCompoundImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<OverlayImageRepresentation> wrapped)
      : OverlayImageRepresentation(manager, backing, tracker),
        wrapped_(std::move(wrapped)) {
    DCHECK(wrapped_);
  }

  CompoundImageBacking* compound_backing() {
    return static_cast<CompoundImageBacking*>(backing());
  }

  // OverlayImageRepresentation implementation.
  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) final {
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kOverlay,
                                          AccessMode::kRead);

    return wrapped_->BeginReadAccess(acquire_fence);
  }
  void EndReadAccess(gfx::GpuFenceHandle release_fence) final {
    return wrapped_->EndReadAccess(std::move(release_fence));
  }
#if BUILDFLAG(IS_WIN)
  std::optional<gl::DCLayerOverlayImage> GetDCLayerOverlayImage() final {
    return wrapped_->GetDCLayerOverlayImage();
  }
#endif

 private:
  std::unique_ptr<OverlayImageRepresentation> wrapped_;
};

// static
bool CompoundImageBacking::IsValidSharedMemoryBufferFormat(
    const gfx::Size& size,
    viz::SharedImageFormat format) {
  if (format.PrefersExternalSampler() ||
      !viz::HasEquivalentBufferFormat(format)) {
    DVLOG(1) << "Not a valid format: " << format.ToString();
    return false;
  }

  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size,
                                                     ToBufferFormat(format))) {
    DVLOG(1) << "Invalid image size: " << size.ToString()
             << " for format: " << format.ToString();
    return false;
  }

  return true;
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
    return usage;
  }

  return usage;
}

// static
std::unique_ptr<SharedImageBacking> CompoundImageBacking::CreateSharedMemory(
    SharedImageBackingFactory* gpu_backing_factory,
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label) {
  DCHECK(IsValidSharedMemoryBufferFormat(size, format));

  SharedMemoryRegionWrapper shm_wrapper;
  if (!shm_wrapper.Initialize(handle, size, ToBufferFormat(format))) {
    DLOG(ERROR) << "Failed to create SharedMemoryRegionWrapper";
    return nullptr;
  }

  auto shm_backing = std::make_unique<SharedMemoryImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type,
      GetShmSharedImageUsage(usage), debug_label, std::move(shm_wrapper));
  shm_backing->SetNotRefCounted();

  return base::WrapUnique(new CompoundImageBacking(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), std::move(shm_backing),
      gpu_backing_factory->GetWeakPtr()));
}

// static
std::unique_ptr<SharedImageBacking> CompoundImageBacking::CreateSharedMemory(
    SharedImageBackingFactory* gpu_backing_factory,
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    gfx::BufferUsage buffer_usage) {
  DCHECK(IsValidSharedMemoryBufferFormat(size, format));

  auto buffer_format = ToBufferFormat(format);
  auto handle = GpuMemoryBufferImplSharedMemory::CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId(0), size, buffer_format, buffer_usage);

  SharedMemoryRegionWrapper shm_wrapper;
  if (!shm_wrapper.Initialize(handle, size, buffer_format)) {
    DLOG(ERROR) << "Failed to create SharedMemoryRegionWrapper";
    return nullptr;
  }

  auto shm_backing = std::make_unique<SharedMemoryImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type,
      GetShmSharedImageUsage(usage), debug_label, std::move(shm_wrapper),
      std::move(handle));
  shm_backing->SetNotRefCounted();

  return base::WrapUnique(new CompoundImageBacking(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), std::move(shm_backing),
      gpu_backing_factory->GetWeakPtr(), std::move(buffer_usage)));
}

CompoundImageBacking::CompoundImageBacking(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    std::unique_ptr<SharedMemoryImageBacking> shm_backing,
    base::WeakPtr<SharedImageBackingFactory> gpu_backing_factory,
    std::optional<gfx::BufferUsage> buffer_usage)
    : SharedImageBacking(mailbox,
                         format,
                         size,
                         color_space,
                         surface_origin,
                         alpha_type,
                         usage,
                         debug_label,
                         shm_backing->GetEstimatedSize(),
                         /*is_thread_safe=*/false,
                         std::move(buffer_usage)) {
  DCHECK(shm_backing);
  DCHECK_EQ(size, shm_backing->size());
  elements_[0].backing = std::move(shm_backing);
  elements_[0].access_streams = kMemoryStreamSet;
  if (kAllowShmOverlays && usage.Has(gpu::SHARED_IMAGE_USAGE_SCANOUT)) {
    elements_[0].access_streams.Put(SharedImageAccessStream::kOverlay);
  }
  elements_[0].content_id_ = latest_content_id_;

  elements_[1].create_callback = base::BindOnce(
      &CompoundImageBacking::LazyCreateBacking, base::Unretained(this),
      std::move(gpu_backing_factory), std::move(debug_label));
  elements_[1].access_streams =
      base::Difference(AccessStreamSet::All(), kMemoryStreamSet);
}

CompoundImageBacking::~CompoundImageBacking() {
  if (pending_copy_to_gmb_callback_) {
    std::move(pending_copy_to_gmb_callback_).Run(/*success=*/false);
  }
}

void CompoundImageBacking::NotifyBeginAccess(SharedImageAccessStream stream,
                                             RepresentationAccessMode mode) {
  // Compound backings don't support VAAPI yet.
  DCHECK_NE(stream, SharedImageAccessStream::kVaapi);

  // TODO(kylechar): Keep track of access to the compound backing as we
  // only want to update a backing if it's not currently being accessed.

  auto& access_element = GetElement(stream);

  if (access_element.access_streams.Has(SharedImageAccessStream::kMemory)) {
    DCHECK_EQ(mode, RepresentationAccessMode::kRead);
    return;
  }

  auto& shm_element = GetElement(SharedImageAccessStream::kMemory);
  DCHECK_NE(&shm_element, &access_element);

  bool updated_backing = false;

  if (!HasLatestContent(access_element)) {
    DCHECK(HasLatestContent(shm_element));

    auto* gpu_backing = access_element.GetBacking();
    if (gpu_backing &&
        gpu_backing->UploadFromMemory(GetSharedMemoryPixmaps())) {
      updated_backing = true;
    } else {
      DLOG(ERROR) << "Failed to upload from shared memory to GPU backing";
    }
  }

  // If a backing was updated or this is write access update what has the latest
  // content.
  bool is_write_access = mode == RepresentationAccessMode::kWrite;
  if (updated_backing || is_write_access)
    SetLatestContent(stream, is_write_access);
}

SharedImageBackingType CompoundImageBacking::GetType() const {
  return SharedImageBackingType::kCompound;
}

void CompoundImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  DCHECK(!in_fence);
  SetLatestContent(SharedImageAccessStream::kMemory,
                   /*write_access=*/true);
}

bool CompoundImageBacking::CopyToGpuMemoryBuffer() {
  auto& shm_element = GetElement(SharedImageAccessStream::kMemory);

  if (HasLatestContent(shm_element)) {
    return true;
  }

  auto* gpu_backing = elements_[1].GetBacking();
  const std::vector<SkPixmap>& pixmaps = GetSharedMemoryPixmaps();
  if (!gpu_backing || !gpu_backing->ReadbackToMemory(pixmaps)) {
    DLOG(ERROR) << "Failed to copy from GPU backing to shared memory";
    return false;
  }

  SetLatestContent(SharedImageAccessStream::kMemory, /*write_access=*/false);

  return true;
}

void CompoundImageBacking::CopyToGpuMemoryBufferAsync(
    base::OnceCallback<void(bool)> callback) {
  auto& shm_element = GetElement(SharedImageAccessStream::kMemory);

  if (HasLatestContent(shm_element)) {
    std::move(callback).Run(true);
    return;
  }

  if (pending_copy_to_gmb_callback_) {
    DLOG(ERROR) << "Existing CopyToGpuMemoryBuffer operation pending";
    std::move(callback).Run(false);
    return;
  }

  auto* gpu_backing = elements_[1].GetBacking();
  if (!gpu_backing) {
    DLOG(ERROR) << "Failed to copy from GPU backing to shared memory";
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
  if (success) {
    SetLatestContent(SharedImageAccessStream::kMemory, /*write_access=*/false);
  }
  std::move(pending_copy_to_gmb_callback_).Run(success);
}

gfx::Rect CompoundImageBacking::ClearedRect() const {
  // Copy on access will always ensure backing is cleared by first access.
  return gfx::Rect(size());
}

void CompoundImageBacking::SetClearedRect(const gfx::Rect& cleared_rect) {}

gfx::GpuMemoryBufferHandle CompoundImageBacking::GetGpuMemoryBufferHandle() {
  auto& element = GetElement(SharedImageAccessStream::kMemory);
  CHECK(element.backing);
  return element.backing->GetGpuMemoryBufferHandle();
}

std::unique_ptr<DawnImageRepresentation> CompoundImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    std::vector<wgpu::TextureFormat> view_formats,
    scoped_refptr<SharedContextState> context_state) {
  auto* backing = GetBacking(SharedImageAccessStream::kDawn);
  if (!backing)
    return nullptr;

  auto real_rep = backing->ProduceDawn(manager, tracker, device, backend_type,
                                       std::move(view_formats), context_state);
  if (!real_rep)
    return nullptr;

  return std::make_unique<WrappedDawnCompoundImageRepresentation>(
      manager, this, tracker, std::move(real_rep));
}

std::unique_ptr<GLTextureImageRepresentation>
CompoundImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                       MemoryTypeTracker* tracker) {
  auto* backing = GetBacking(SharedImageAccessStream::kGL);
  if (!backing)
    return nullptr;

  auto real_rep = backing->ProduceGLTexture(manager, tracker);
  if (!real_rep)
    return nullptr;

  return std::make_unique<WrappedGLTextureCompoundImageRepresentation>(
      manager, this, tracker, std::move(real_rep));
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
CompoundImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                                  MemoryTypeTracker* tracker) {
  auto* backing = GetBacking(SharedImageAccessStream::kGL);
  if (!backing)
    return nullptr;

  auto real_rep = backing->ProduceGLTexturePassthrough(manager, tracker);
  if (!real_rep)
    return nullptr;

  return std::make_unique<
      WrappedGLTexturePassthroughCompoundImageRepresentation>(
      manager, this, tracker, std::move(real_rep));
}

std::unique_ptr<SkiaGaneshImageRepresentation>
CompoundImageBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  auto* backing = GetBacking(SharedImageAccessStream::kSkia);
  if (!backing)
    return nullptr;

  auto real_rep = backing->ProduceSkiaGanesh(manager, tracker, context_state);
  if (!real_rep)
    return nullptr;

  auto* gr_context = context_state ? context_state->gr_context() : nullptr;
  return std::make_unique<WrappedSkiaGaneshCompoundImageRepresentation>(
      gr_context, manager, this, tracker, std::move(real_rep));
}

std::unique_ptr<SkiaGraphiteImageRepresentation>
CompoundImageBacking::ProduceSkiaGraphite(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  auto* backing = GetBacking(SharedImageAccessStream::kSkia);
  if (!backing) {
    return nullptr;
  }

  auto real_rep = backing->ProduceSkiaGraphite(manager, tracker, context_state);
  if (!real_rep) {
    return nullptr;
  }

  return std::make_unique<WrappedSkiaGraphiteCompoundImageRepresentation>(
      manager, this, tracker, std::move(real_rep));
}

std::unique_ptr<OverlayImageRepresentation>
CompoundImageBacking::ProduceOverlay(SharedImageManager* manager,
                                     MemoryTypeTracker* tracker) {
  auto* backing = GetBacking(SharedImageAccessStream::kOverlay);
  if (!backing)
    return nullptr;

  auto real_rep = backing->ProduceOverlay(manager, tracker);
  if (!real_rep)
    return nullptr;

  return std::make_unique<WrappedOverlayCompoundImageRepresentation>(
      manager, this, tracker, std::move(real_rep));
}

base::trace_event::MemoryAllocatorDump* CompoundImageBacking::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDumpGuid client_guid,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
  // Create dump but don't add scalar size. The size will be inferred from the
  // sizes of the sub-backings.
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

    auto element_client_guid = GetSubBackingGUIDForTracing(mailbox(), i + 1);
    std::string element_dump_name =
        base::StringPrintf("%s/element_%d", dump_name.c_str(), i);
    backing->OnMemoryDump(element_dump_name, element_client_guid, pmd,
                          client_tracing_id);
  }
  return dump;
}

const std::vector<SkPixmap>& CompoundImageBacking::GetSharedMemoryPixmaps() {
  auto* shm_backing = GetElement(SharedImageAccessStream::kMemory).GetBacking();
  DCHECK(shm_backing);

  return static_cast<SharedMemoryImageBacking*>(shm_backing)->pixmaps();
}

CompoundImageBacking::ElementHolder& CompoundImageBacking::GetElement(
    SharedImageAccessStream stream) {
  for (auto& element : elements_) {
    // For each access stream there should be exactly one element where this
    // returns true.
    if (element.access_streams.Has(stream))
      return element;
  }

  NOTREACHED_IN_MIGRATION();
  return elements_.back();
}

SharedImageBacking* CompoundImageBacking::GetBacking(
    SharedImageAccessStream stream) {
  return GetElement(stream).GetBacking();
}

void CompoundImageBacking::LazyCreateBacking(
    base::WeakPtr<SharedImageBackingFactory> factory,
    std::string debug_label,
    std::unique_ptr<SharedImageBacking>& backing) {
  if (!factory) {
    DLOG(ERROR) << "Can't allocate backing after image has been destroyed";
    return;
  }

  backing = factory->CreateSharedImage(
      mailbox(), format(), kNullSurfaceHandle, size(), color_space(),
      surface_origin(), alpha_type(),
      GetGpuSharedImageUsage(SharedImageUsageSet(usage())),
      std::move(debug_label), /*is_thread_safe=*/false);
  if (!backing) {
    DLOG(ERROR) << "Failed to allocate GPU backing";
    return;
  }

  // Since the owned GPU backing is never registered with SharedImageManager
  // it's not recorded in UMA histogram there.
  UMA_HISTOGRAM_ENUMERATION("GPU.SharedImage.BackingType", backing->GetType());

  backing->SetNotRefCounted();
  backing->SetCleared();

  // Update peak GPU memory tracking with the new estimated size.
  size_t estimated_size = 0;
  for (auto& element : elements_) {
    if (element.backing)
      estimated_size += element.backing->GetEstimatedSize();
  }

  AutoLock auto_lock(this);
  UpdateEstimatedSize(estimated_size);
}

bool CompoundImageBacking::HasLatestContent(ElementHolder& element) {
  return element.content_id_ == latest_content_id_;
}

void CompoundImageBacking::SetLatestContent(SharedImageAccessStream stream,
                                            bool write_access) {
  if (write_access)
    ++latest_content_id_;

  auto& element = GetElement(stream);
  DCHECK(element.backing);
  element.content_id_ = latest_content_id_;
}

void CompoundImageBacking::OnAddSecondaryReference() {
  // When client adds a reference from another processes it expects this
  // SharedImage can outlive original factory ref and so potentially
  // SharedimageFactory. We should create all backings now as we might not have
  // access to corresponding SharedImageBackingFactories later.
  for (auto& element : elements_) {
    element.CreateBackingIfNecessary();
  }
}

CompoundImageBacking::ElementHolder::ElementHolder() = default;
CompoundImageBacking::ElementHolder::~ElementHolder() = default;

void CompoundImageBacking::ElementHolder::CreateBackingIfNecessary() {
  if (create_callback) {
    std::move(create_callback).Run(backing);
  }
}

SharedImageBacking* CompoundImageBacking::ElementHolder::GetBacking() {
  CreateBackingIfNecessary();
  return backing.get();
}

}  // namespace gpu
