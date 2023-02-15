// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/compound_image_backing.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
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

// TODO(crbug.com/1293509): Remove after M110 branch.
BASE_FEATURE(kSkipReadbackToSharedMemory,
             "SkipReadbackToSharedMemory",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr AccessStreamSet kMemoryStreamSet = {SharedImageAccessStream::kMemory};

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

  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) final {
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kSkia,
                                          AccessMode::kWrite);
    return wrapped_->BeginWriteAccess(final_msaa_count, surface_props,
                                      update_rect, begin_semaphores,
                                      end_semaphores, end_state);
  }
  std::vector<sk_sp<SkPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) final {
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kSkia,
                                          AccessMode::kWrite);
    return wrapped_->BeginWriteAccess(begin_semaphores, end_semaphores,
                                      end_state);
  }
  void EndWriteAccess() final { wrapped_->EndWriteAccess(); }

  std::vector<sk_sp<SkPromiseImageTexture>> BeginReadAccess(
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
  gl::GLImage* GetGLImage() final { return wrapped_->GetGLImage(); }
#endif

 private:
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
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  if (!IsValidSharedMemoryBufferFormat(size, buffer_format, plane)) {
    return nullptr;
  }

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
      usage, allow_shm_overlays, std::move(shm_backing),
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
                         shm_backing->GetEstimatedSize(),
                         /*is_thread_safe=*/false) {
  DCHECK(shm_backing);
  DCHECK_EQ(size, shm_backing->size());
  elements_[0].backing = std::move(shm_backing);
  elements_[0].access_streams = kMemoryStreamSet;
  if (allow_shm_overlays)
    elements_[0].access_streams.Put(SharedImageAccessStream::kOverlay);
  elements_[0].content_id_ = latest_content_id_;

  elements_[1].create_callback =
      base::BindOnce(&CompoundImageBacking::LazyCreateBacking,
                     base::Unretained(this), std::move(gpu_backing_factory));
  elements_[1].access_streams =
      base::Difference(AccessStreamSet::All(), kMemoryStreamSet);
}

CompoundImageBacking::~CompoundImageBacking() = default;

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
    SkPixmap pixmap = GetSharedMemoryPixmap();
    if (gpu_backing && gpu_backing->UploadFromMemory(pixmap)) {
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

  // If shared memory already contains the latest content skip readback.
  if (HasLatestContent(shm_element) &&
      base::FeatureList::IsEnabled(kSkipReadbackToSharedMemory)) {
    return true;
  }

  auto* gpu_backing = elements_[1].GetBacking();
  SkPixmap pixmap = GetSharedMemoryPixmap();
  if (!gpu_backing || !gpu_backing->ReadbackToMemory(pixmap)) {
    DLOG(ERROR) << "Failed to copy from GPU backing to shared memory";
    return false;
  }

  SetLatestContent(SharedImageAccessStream::kMemory, /*write_access=*/false);

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
    WGPUBackendType backend_type,
    std::vector<WGPUTextureFormat> view_formats) {
  auto* backing = GetBacking(SharedImageAccessStream::kDawn);
  if (!backing)
    return nullptr;

  auto real_rep = backing->ProduceDawn(manager, tracker, device, backend_type,
                                       std::move(view_formats));
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

std::unique_ptr<SkiaImageRepresentation> CompoundImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  auto* backing = GetBacking(SharedImageAccessStream::kSkia);
  if (!backing)
    return nullptr;

  auto real_rep =
      backing->ProduceSkia(manager, tracker, std::move(context_state));
  if (!real_rep)
    return nullptr;

  return std::make_unique<WrappedSkiaCompoundImageRepresentation>(
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
}

SkPixmap CompoundImageBacking::GetSharedMemoryPixmap() {
  auto* shm_backing = GetElement(SharedImageAccessStream::kMemory).GetBacking();
  DCHECK(shm_backing);

  auto& wrapper = static_cast<SharedMemoryImageBacking*>(shm_backing)
                      ->shared_memory_wrapper();
  DCHECK(wrapper.IsValid());

  return SkPixmap(shm_backing->AsSkImageInfo(), wrapper.GetMemory(),
                  wrapper.GetStride());
}

CompoundImageBacking::ElementHolder& CompoundImageBacking::GetElement(
    SharedImageAccessStream stream) {
  for (auto& element : elements_) {
    // For each access stream there should be exactly one element where this
    // returns true.
    if (element.access_streams.Has(stream))
      return element;
  }

  NOTREACHED();
  return elements_.back();
}

SharedImageBacking* CompoundImageBacking::GetBacking(
    SharedImageAccessStream stream) {
  return GetElement(stream).GetBacking();
}

void CompoundImageBacking::LazyCreateBacking(
    base::WeakPtr<SharedImageBackingFactory> factory,
    std::unique_ptr<SharedImageBacking>& backing) {
  if (!factory) {
    DLOG(ERROR) << "Can't allocate backing after image has been destroyed";
    return;
  }

  backing = factory->CreateSharedImage(
      mailbox(), format(), kNullSurfaceHandle, size(), color_space(),
      surface_origin(), alpha_type(), usage() | SHARED_IMAGE_USAGE_CPU_UPLOAD,
      /*is_thread_safe=*/false);
  if (!backing) {
    DLOG(ERROR) << "Failed to allocate GPU backing";
    return;
  }

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

CompoundImageBacking::ElementHolder::ElementHolder() = default;
CompoundImageBacking::ElementHolder::~ElementHolder() = default;

SharedImageBacking* CompoundImageBacking::ElementHolder::GetBacking() {
  if (create_callback) {
    std::move(create_callback).Run(backing);
  }

  return backing.get();
}

}  // namespace gpu
