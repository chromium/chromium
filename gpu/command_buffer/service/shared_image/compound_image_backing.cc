// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/compound_image_backing.h"

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/process_memory_dump.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_memory_image_backing.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {
namespace {

bool IsValidSharedMemoryBufferFormat(const gfx::Size& size,
                                     gfx::BufferFormat buffer_format,
                                     gfx::BufferPlane plane) {
  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, buffer_format)) {
    DLOG(ERROR) << "Invalid image size for format.";
    return false;
  }
  switch (plane) {
    case gfx::BufferPlane::DEFAULT:
    case gfx::BufferPlane::Y:
    case gfx::BufferPlane::UV:
      break;
    default:
      DLOG(ERROR) << "Invalid plane " << gfx::BufferPlaneToString(plane);
      return false;
  }

  return true;
}

// Unique GUIDs for child backings.
base::trace_event::MemoryAllocatorDumpGuid GetSubBackingGUIDForTracing(
    const Mailbox& mailbox,
    int backing_index) {
  return base::trace_event::MemoryAllocatorDumpGuid(
      base::StringPrintf("gpu-shared-image/%s/sub-backing/%d",
                         mailbox.ToDebugString().c_str(), backing_index));
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

class WrappedSkiaCompoundImageRepresentation : public SkiaImageRepresentation {
 public:
  WrappedSkiaCompoundImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<SkiaImageRepresentation> wrapped)
      : SkiaImageRepresentation(manager, backing, tracker),
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

  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) final {
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kSkia,
                                          AccessMode::kWrite);
    return wrapped_->BeginWriteAccess(final_msaa_count, surface_props,
                                      begin_semaphores, end_semaphores,
                                      end_state);
  }
  sk_sp<SkPromiseImageTexture> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) final {
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kSkia,
                                          AccessMode::kWrite);
    return wrapped_->BeginWriteAccess(begin_semaphores, end_semaphores,
                                      end_state);
  }
  void EndWriteAccess(sk_sp<SkSurface> surface) final {
    wrapped_->EndWriteAccess(std::move(surface));
  }

  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) final {
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kSkia,
                                          AccessMode::kRead);
    return wrapped_->BeginReadAccess(begin_semaphores, end_semaphores,
                                     end_state);
  }
  void EndReadAccess() final { wrapped_->EndReadAccess(); }

 private:
  std::unique_ptr<SkiaImageRepresentation> wrapped_;
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
  WGPUTexture BeginAccess(WGPUTextureUsage webgpu_usage) final {
    AccessMode access_mode =
        webgpu_usage & kWriteUsage ? AccessMode::kWrite : AccessMode::kRead;
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kDawn,
                                          access_mode);
    return wrapped_->BeginAccess(webgpu_usage);
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
      SharedImageAccessStream access_stream,
      std::unique_ptr<OverlayImageRepresentation> wrapped)
      : OverlayImageRepresentation(manager, backing, tracker),
        access_stream_(access_stream),
        wrapped_(std::move(wrapped)) {
    DCHECK(wrapped_);
  }

  CompoundImageBacking* compound_backing() {
    return static_cast<CompoundImageBacking*>(backing());
  }

  // OverlayImageRepresentation implementation.
  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) final {
    compound_backing()->NotifyBeginAccess(access_stream_, AccessMode::kRead);

    return wrapped_->BeginReadAccess(acquire_fence);
  }
  void EndReadAccess(gfx::GpuFenceHandle release_fence) final {
    return wrapped_->EndReadAccess(std::move(release_fence));
  }
  gl::GLImage* GetGLImage() final { return wrapped_->GetGLImage(); }

 private:
  const SharedImageAccessStream access_stream_;
  std::unique_ptr<OverlayImageRepresentation> wrapped_;
};

// static
std::unique_ptr<SharedImageBacking> CompoundImageBacking::CreateSharedMemory(
    SharedImageBackingFactory* gpu_backing_factory,
    bool allow_shm_overlays,
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  if (!IsValidSharedMemoryBufferFormat(size, buffer_format, plane))
    return nullptr;

  const gfx::Size plane_size = GetPlaneSize(plane, size);
  const viz::ResourceFormat plane_format =
      viz::GetResourceFormat(GetPlaneBufferFormat(plane, buffer_format));

  const size_t plane_index = plane == gfx::BufferPlane::UV ? 1 : 0;
  handle.offset +=
      gfx::BufferOffsetForBufferFormat(size, buffer_format, plane_index);

  SharedMemoryRegionWrapper shm_wrapper;
  if (!shm_wrapper.Initialize(handle, plane_size, plane_format)) {
    DLOG(ERROR) << "Failed to create SharedMemoryRegionWrapper";
    return nullptr;
  }

  auto si_format = viz::SharedImageFormat::SinglePlane(plane_format);

  auto shm_backing = std::make_unique<SharedMemoryImageBacking>(
      mailbox, si_format, plane_size, color_space, surface_origin, alpha_type,
      SHARED_IMAGE_USAGE_CPU_WRITE, std::move(shm_wrapper));
  shm_backing->SetNotRefCounted();

  return std::make_unique<CompoundImageBacking>(
      mailbox, si_format, plane_size, color_space, surface_origin, alpha_type,
      usage, surface_handle, allow_shm_overlays, std::move(shm_backing),
      gpu_backing_factory->GetWeakPtr());
}

CompoundImageBacking::CompoundImageBacking(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    SurfaceHandle surface_handle,
    bool allow_shm_overlays,
    std::unique_ptr<SharedMemoryImageBacking> shm_backing,
    base::WeakPtr<SharedImageBackingFactory> gpu_backing_factory)
    : SharedImageBacking(mailbox,
                         format,
                         size,
                         color_space,
                         surface_origin,
                         alpha_type,
                         usage,
                         shm_backing->estimated_size(),
                         /*is_thread_safe=*/false),
      surface_handle_(surface_handle),
      allow_shm_overlays_(allow_shm_overlays),
      shm_backing_(std::move(shm_backing)),
      gpu_backing_factory_(std::move(gpu_backing_factory)) {
  DCHECK(shm_backing_);
  DCHECK_EQ(size, shm_backing_->size());
}

CompoundImageBacking::~CompoundImageBacking() = default;

void CompoundImageBacking::NotifyBeginAccess(SharedImageAccessStream stream,
                                             RepresentationAccessMode mode) {
  // Compound backings don't support VAAPI yet.
  DCHECK_NE(stream, SharedImageAccessStream::kVaapi);

  // TODO(kylechar): Keep track of access to the compound backing as we
  // only want to update a backing if it's not currently being accessed.

  if (stream == SharedImageAccessStream::kMemory) {
    DCHECK_EQ(mode, RepresentationAccessMode::kRead);
    return;
  }

  if (!gpu_has_latest_content_) {
    DCHECK(shm_has_latest_content_);
    DCHECK(gpu_backing_);

    auto& wrapper = static_cast<SharedMemoryImageBacking*>(shm_backing_.get())
                        ->shared_memory_wrapper();
    DCHECK(wrapper.IsValid());

    SkPixmap pixmap(shm_backing_->AsSkImageInfo(), wrapper.GetMemory(),
                    wrapper.GetStride());

    if (gpu_backing_->UploadFromMemory(pixmap)) {
      gpu_has_latest_content_ = true;
    } else {
      DLOG(ERROR) << "Failed to upload from shared memory to GPU backing";
    }
  }

  if (mode == RepresentationAccessMode::kWrite) {
    // On GPU write access set shared memory contents as stale.
    shm_has_latest_content_ = false;
  }
}

SharedImageBackingType CompoundImageBacking::GetType() const {
  return SharedImageBackingType::kCompound;
}

void CompoundImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  DCHECK(!in_fence);
  shm_has_latest_content_ = true;
  gpu_has_latest_content_ = false;
}

bool CompoundImageBacking::CopyToGpuMemoryBuffer() {
  // TODO(crbug.com/1293509): Return early if `shm_has_latest_content_` is true
  // since shared memory should already be up to date. Just need to verify GL
  // isn't modifying the texture without acquiring write access first.

  auto& wrapper = static_cast<SharedMemoryImageBacking*>(shm_backing_.get())
                      ->shared_memory_wrapper();
  DCHECK(wrapper.IsValid());

  SkPixmap pixmap(shm_backing_->AsSkImageInfo(), wrapper.GetMemory(),
                  wrapper.GetStride());

  if (!gpu_backing_->ReadbackToMemory(pixmap)) {
    DLOG(ERROR) << "Failed to copy from GPU backing to shared memory";
    return false;
  }

  shm_has_latest_content_ = true;
  return true;
}

gfx::Rect CompoundImageBacking::ClearedRect() const {
  // Copy on access will always ensure backing is cleared by first access.
  return gfx::Rect(size());
}

void CompoundImageBacking::SetClearedRect(const gfx::Rect& cleared_rect) {}

std::unique_ptr<DawnImageRepresentation> CompoundImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    WGPUBackendType backend_type) {
  LazyAllocateGpuBacking();
  if (!gpu_backing_)
    return nullptr;

  auto real_rep =
      gpu_backing_->ProduceDawn(manager, tracker, device, backend_type);
  if (!real_rep)
    return nullptr;

  return std::make_unique<WrappedDawnCompoundImageRepresentation>(
      manager, this, tracker, std::move(real_rep));
}

std::unique_ptr<GLTextureImageRepresentation>
CompoundImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                       MemoryTypeTracker* tracker) {
  LazyAllocateGpuBacking();
  if (!gpu_backing_)
    return nullptr;

  auto real_rep = gpu_backing_->ProduceGLTexture(manager, tracker);
  if (!real_rep)
    return nullptr;

  return std::make_unique<WrappedGLTextureCompoundImageRepresentation>(
      manager, this, tracker, std::move(real_rep));
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
CompoundImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                                  MemoryTypeTracker* tracker) {
  LazyAllocateGpuBacking();
  if (!gpu_backing_)
    return nullptr;

  auto real_rep = gpu_backing_->ProduceGLTexturePassthrough(manager, tracker);
  if (!real_rep)
    return nullptr;

  return std::make_unique<
      WrappedGLTexturePassthroughCompoundImageRepresentation>(
      manager, this, tracker, std::move(real_rep));
}

std::unique_ptr<SkiaImageRepresentation> CompoundImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  LazyAllocateGpuBacking();
  if (!gpu_backing_)
    return nullptr;

  auto real_rep =
      gpu_backing_->ProduceSkia(manager, tracker, std::move(context_state));
  if (!real_rep)
    return nullptr;

  return std::make_unique<WrappedSkiaCompoundImageRepresentation>(
      manager, this, tracker, std::move(real_rep));
}

std::unique_ptr<OverlayImageRepresentation>
CompoundImageBacking::ProduceOverlay(SharedImageManager* manager,
                                     MemoryTypeTracker* tracker) {
  if (allow_shm_overlays_) {
    // The client has stated it wants shared memory backed overlays.
    auto real_rep = shm_backing_->ProduceOverlay(manager, tracker);
    if (!real_rep)
      return nullptr;

    return std::make_unique<WrappedOverlayCompoundImageRepresentation>(
        manager, this, tracker, SharedImageAccessStream::kMemory,
        std::move(real_rep));
  }

  LazyAllocateGpuBacking();
  if (!gpu_backing_)
    return nullptr;

  auto real_rep = gpu_backing_->ProduceOverlay(manager, tracker);
  if (!real_rep)
    return nullptr;

  return std::make_unique<WrappedOverlayCompoundImageRepresentation>(
      manager, this, tracker, SharedImageAccessStream::kOverlay,
      std::move(real_rep));
}

void CompoundImageBacking::OnMemoryDump(
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
  pmd->AddOwnershipEdge(dump->guid(), client_guid, kNonOwningEdgeImportance);

  // Add dumps nested under `dump_name` for child backings owned by compound
  // image. These get different shared GUIDs to add ownership edges with GPU
  // texture or shared memory.
  auto shm_client_guid = GetSubBackingGUIDForTracing(mailbox(), 1);
  std::string shm_dump_name =
      base::StringPrintf("%s/shared_memory", dump_name.c_str());
  shm_backing_->OnMemoryDump(shm_dump_name, shm_client_guid, pmd,
                             client_tracing_id);

  if (gpu_backing_) {
    auto gpu_client_guid = GetSubBackingGUIDForTracing(mailbox(), 2);
    std::string gpu_dump_name = base::StringPrintf("%s/gpu", dump_name.c_str());
    gpu_backing_->OnMemoryDump(gpu_dump_name, gpu_client_guid, pmd,
                               client_tracing_id);
  }
}

size_t CompoundImageBacking::EstimatedSizeForMemTracking() const {
  size_t estimated_size = shm_backing_->EstimatedSizeForMemTracking();
  if (gpu_backing_)
    estimated_size += gpu_backing_->EstimatedSizeForMemTracking();
  return estimated_size;
}

void CompoundImageBacking::LazyAllocateGpuBacking() {
  if (gpu_backing_)
    return;

  if (!gpu_backing_factory_) {
    if (gpu_backing_factory_.WasInvalidated()) {
      // The SharedImageFactory must no longer exist so the compound shared
      // image must already have been destroyed.
      LOG(ERROR) << "Can't allocate backing after image has been destroyed";
      gpu_backing_factory_.reset();
    }
    return;
  }

  gpu_backing_ = gpu_backing_factory_->CreateSharedImage(
      mailbox(), format(), surface_handle_, size(), color_space(),
      surface_origin(), alpha_type(), usage() | SHARED_IMAGE_USAGE_CPU_UPLOAD,
      /*is_thread_safe=*/false);
  if (!gpu_backing_) {
    LOG(ERROR) << "Failed to allocate GPU backing";
    gpu_backing_factory_.reset();
    return;
  }

  gpu_backing_->SetNotRefCounted();
  gpu_backing_->SetClearedRect(gfx::Rect(size()));
}

}  // namespace gpu
