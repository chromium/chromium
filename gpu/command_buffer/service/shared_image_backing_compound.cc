// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_compound.h"

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/trace_event/process_memory_dump.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing_compound.h"
#include "gpu/command_buffer/service/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image_backing_shared_memory.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
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
  if (gfx::NumberOfPlanesForLinearBufferFormat(buffer_format) != 1) {
    DLOG(ERROR) << "Invalid image format.";
    return false;
  }
  if (plane != gfx::BufferPlane::DEFAULT) {
    DLOG(ERROR) << "Invalid plane " << gfx::BufferPlaneToString(plane);
    return false;
  }

  return true;
}

}  // namespace

// Wrapped representation types are not in the anonymous namespace because they
// need to be friend classes to real representations to access protected
// virtual functions.
class WrappedGLTextureRepresentation
    : public SharedImageRepresentationGLTexture {
 public:
  WrappedGLTextureRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<SharedImageRepresentationGLTexture> wrapped)
      : SharedImageRepresentationGLTexture(manager, backing, tracker),
        wrapped_(std::move(wrapped)) {
    DCHECK(wrapped_);
  }

  SharedImageBackingCompound* compound_backing() {
    return static_cast<SharedImageBackingCompound*>(backing());
  }

  // SharedImageRepresentationGLTexture implementation.
  bool BeginAccess(GLenum mode) final {
    AccessMode access_mode =
        mode == kReadAccessMode ? AccessMode::kRead : AccessMode::kWrite;
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kGL,
                                          access_mode);
    return wrapped_->BeginAccess(mode);
  }

  void EndAccess() final { wrapped_->EndAccess(); }

  gpu::TextureBase* GetTextureBase() final {
    return wrapped_->GetTextureBase();
  }

  bool SupportsMultipleConcurrentReadAccess() final {
    return wrapped_->SupportsMultipleConcurrentReadAccess();
  }

  gles2::Texture* GetTexture() final { return wrapped_->GetTexture(); }

 private:
  std::unique_ptr<SharedImageRepresentationGLTexture> wrapped_;
};

class WrappedGLTexturePassthroughRepresentation
    : public SharedImageRepresentationGLTexturePassthrough {
 public:
  WrappedGLTexturePassthroughRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<SharedImageRepresentationGLTexturePassthrough> wrapped)
      : SharedImageRepresentationGLTexturePassthrough(manager,
                                                      backing,
                                                      tracker),
        wrapped_(std::move(wrapped)) {
    DCHECK(wrapped_);
  }

  SharedImageBackingCompound* compound_backing() {
    return static_cast<SharedImageBackingCompound*>(backing());
  }

  // SharedImageRepresentationGLTexturePassthrough implementation.
  bool BeginAccess(GLenum mode) final {
    AccessMode access_mode =
        mode == kReadAccessMode ? AccessMode::kRead : AccessMode::kWrite;
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kGL,
                                          access_mode);
    return wrapped_->BeginAccess(mode);
  }
  void EndAccess() final { wrapped_->EndAccess(); }

  gpu::TextureBase* GetTextureBase() final {
    return wrapped_->GetTextureBase();
  }

  bool SupportsMultipleConcurrentReadAccess() final {
    return wrapped_->SupportsMultipleConcurrentReadAccess();
  }

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      final {
    return wrapped_->GetTexturePassthrough();
  }

 private:
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough> wrapped_;
};

class WrappedSkiaRepresentation : public SharedImageRepresentationSkia {
 public:
  WrappedSkiaRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<SharedImageRepresentationSkia> wrapped)
      : SharedImageRepresentationSkia(manager, backing, tracker),
        wrapped_(std::move(wrapped)) {
    DCHECK(wrapped_);
  }

  SharedImageBackingCompound* compound_backing() {
    return static_cast<SharedImageBackingCompound*>(backing());
  }

  // SharedImageRepresentationSkia implementation.
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
  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) final {
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kSkia,
                                          AccessMode::kWrite);
    return wrapped_->BeginWriteAccess(final_msaa_count, surface_props,
                                      begin_semaphores, end_semaphores);
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
  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) final {
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kSkia,
                                          AccessMode::kRead);
    return wrapped_->BeginReadAccess(begin_semaphores, end_semaphores);
  }
  void EndReadAccess() final { wrapped_->EndReadAccess(); }

 private:
  std::unique_ptr<SharedImageRepresentationSkia> wrapped_;
};

class WrappedDawnRepresentation : public SharedImageRepresentationDawn {
 public:
  WrappedDawnRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<SharedImageRepresentationDawn> wrapped)
      : SharedImageRepresentationDawn(manager, backing, tracker),
        wrapped_(std::move(wrapped)) {
    DCHECK(wrapped_);
  }

  SharedImageBackingCompound* compound_backing() {
    return static_cast<SharedImageBackingCompound*>(backing());
  }

  // SharedImageRepresentationDawn implementation.
  WGPUTexture BeginAccess(WGPUTextureUsage webgpu_usage) final {
    AccessMode access_mode =
        webgpu_usage & kWriteUsage ? AccessMode::kWrite : AccessMode::kRead;
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kDawn,
                                          access_mode);
    return wrapped_->BeginAccess(webgpu_usage);
  }
  void EndAccess() final { wrapped_->EndAccess(); }

 private:
  std::unique_ptr<SharedImageRepresentationDawn> wrapped_;
};

class WrappedOverlayRepresentation : public SharedImageRepresentationOverlay {
 public:
  WrappedOverlayRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<SharedImageRepresentationOverlay> wrapped)
      : SharedImageRepresentationOverlay(manager, backing, tracker),
        wrapped_(std::move(wrapped)) {
    DCHECK(wrapped_);
  }

  SharedImageBackingCompound* compound_backing() {
    return static_cast<SharedImageBackingCompound*>(backing());
  }

  // SharedImageRepresentationOverlay implementation.
  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) final {
    compound_backing()->NotifyBeginAccess(SharedImageAccessStream::kOverlay,
                                          AccessMode::kRead);

    return wrapped_->BeginReadAccess(acquire_fence);
  }
  void EndReadAccess(gfx::GpuFenceHandle release_fence) final {
    return wrapped_->EndReadAccess(std::move(release_fence));
  }
  gl::GLImage* GetGLImage() final { return wrapped_->GetGLImage(); }

 private:
  std::unique_ptr<SharedImageRepresentationOverlay> wrapped_;
};

// static
std::unique_ptr<SharedImageBacking>
SharedImageBackingCompound::CreateSharedMemory(
    SharedImageBackingFactory* gpu_backing_factory,
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

  viz::ResourceFormat format = viz::GetResourceFormat(buffer_format);
  SharedMemoryRegionWrapper shm_wrapper;
  if (!shm_wrapper.Initialize(handle, size, format)) {
    DLOG(ERROR) << "Failed to create SharedMemoryRegionWrapper";
    return nullptr;
  }

  auto shm_backing = std::make_unique<SharedImageBackingSharedMemory>(
      gpu::Mailbox(), format, size, color_space, surface_origin, alpha_type,
      SHARED_IMAGE_USAGE_CPU_WRITE, std::move(shm_wrapper));

  auto gpu_backing = gpu_backing_factory->CreateSharedImage(
      gpu::Mailbox(), format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage | SHARED_IMAGE_USAGE_CPU_UPLOAD,
      /*is_thread_safe=*/false);
  if (!gpu_backing) {
    DLOG(ERROR) << "Failed to create GPU backing";
    return nullptr;
  }

  return std::make_unique<SharedImageBackingCompound>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(shm_backing), std::move(gpu_backing));
}

SharedImageBackingCompound::SharedImageBackingCompound(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::unique_ptr<SharedImageBackingSharedMemory> shm_backing,
    std::unique_ptr<SharedImageBacking> gpu_backing)
    : SharedImageBacking(mailbox,
                         format,
                         size,
                         color_space,
                         surface_origin,
                         alpha_type,
                         usage,
                         gpu_backing->estimated_size(),
                         /*is_thread_safe=*/false),
      shm_backing_(std::move(shm_backing)),
      gpu_backing_(std::move(gpu_backing)) {
  DCHECK(gpu_backing_);
  DCHECK(shm_backing_);
  DCHECK_EQ(size, gpu_backing_->size());
  DCHECK_EQ(size, shm_backing_->size());

  // First access will write pixels from shared memory backing to GPU backing
  // clearing it.
  shm_has_update_ = true;
  SetClearedRect(gfx::Rect(size));
}

SharedImageBackingCompound::~SharedImageBackingCompound() = default;

void SharedImageBackingCompound::NotifyBeginAccess(
    SharedImageAccessStream stream,
    RepresentationAccessMode mode) {
  // Compound backings don't support CPU access directly or copying
  // from GPU back to CPU yet. Also no support for VAAPI.
  DCHECK_NE(stream, SharedImageAccessStream::kMemory);
  DCHECK_NE(stream, SharedImageAccessStream::kVaapi);

  // TODO(kylechar): Keep track of access to the compound backing as we
  // only want to update a backing if it's not currently being accessed.

  if (shm_has_update_) {
    auto& wrapper =
        static_cast<SharedImageBackingSharedMemory*>(shm_backing_.get())
            ->shared_memory_wrapper();
    DCHECK(wrapper.IsValid());

    SkPixmap pixmap(shm_backing_->AsSkImageInfo(), wrapper.GetMemory(),
                    wrapper.GetStride());

    if (gpu_backing_->UploadFromMemory(pixmap)) {
      shm_has_update_ = false;
    } else {
      DLOG(ERROR) << "Failed to upload from shared memory to GPU backing";
    }
  }
}

SharedImageBackingType SharedImageBackingCompound::GetType() const {
  return SharedImageBackingType::kCompound;
}

void SharedImageBackingCompound::Update(
    std::unique_ptr<gfx::GpuFence> in_fence) {
  DCHECK(!in_fence);
  shm_has_update_ = true;
}

bool SharedImageBackingCompound::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  return gpu_backing_->ProduceLegacyMailbox(mailbox_manager);
}

gfx::Rect SharedImageBackingCompound::ClearedRect() const {
  return gpu_backing_->ClearedRect();
}

void SharedImageBackingCompound::SetClearedRect(const gfx::Rect& cleared_rect) {
  // Shared memory backing doesn't track cleared rect.
  gpu_backing_->SetClearedRect(cleared_rect);
}

std::unique_ptr<SharedImageRepresentationDawn>
SharedImageBackingCompound::ProduceDawn(SharedImageManager* manager,
                                        MemoryTypeTracker* tracker,
                                        WGPUDevice device,
                                        WGPUBackendType backend_type) {
  auto real_rep =
      gpu_backing_->ProduceDawn(manager, tracker, device, backend_type);
  if (!real_rep)
    return nullptr;

  return std::make_unique<WrappedDawnRepresentation>(manager, this, tracker,
                                                     std::move(real_rep));
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageBackingCompound::ProduceGLTexture(SharedImageManager* manager,
                                             MemoryTypeTracker* tracker) {
  auto real_rep = gpu_backing_->ProduceGLTexture(manager, tracker);
  if (!real_rep)
    return nullptr;

  return std::make_unique<WrappedGLTextureRepresentation>(
      manager, this, tracker, std::move(real_rep));
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageBackingCompound::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  auto real_rep = gpu_backing_->ProduceGLTexturePassthrough(manager, tracker);
  if (!real_rep)
    return nullptr;

  return std::make_unique<WrappedGLTexturePassthroughRepresentation>(
      manager, this, tracker, std::move(real_rep));
}

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageBackingCompound::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  auto real_rep =
      gpu_backing_->ProduceSkia(manager, tracker, std::move(context_state));
  if (!real_rep)
    return nullptr;

  return std::make_unique<WrappedSkiaRepresentation>(manager, this, tracker,
                                                     std::move(real_rep));
}

std::unique_ptr<SharedImageRepresentationOverlay>
SharedImageBackingCompound::ProduceOverlay(SharedImageManager* manager,
                                           MemoryTypeTracker* tracker) {
  auto real_rep = gpu_backing_->ProduceOverlay(manager, tracker);
  if (!real_rep)
    return nullptr;

  return std::make_unique<WrappedOverlayRepresentation>(manager, this, tracker,
                                                        std::move(real_rep));
}

void SharedImageBackingCompound::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDump* dump,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
  shm_backing_.get()->OnMemoryDump(dump_name, dump, pmd, client_tracing_id);
  gpu_backing_->OnMemoryDump(dump_name, dump, pmd, client_tracing_id);
}

}  // namespace gpu
