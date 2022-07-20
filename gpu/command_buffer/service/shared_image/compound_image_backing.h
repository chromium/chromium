// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_COMPOUND_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_COMPOUND_IMAGE_BACKING_H_

#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_memory_image_backing.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

class SharedImageBackingFactory;

// TODO(kylechar): Merge with OzoneImageBacking::AccessStream enum.
enum class SharedImageAccessStream {
  kSkia,
  kOverlay,
  kGL,
  kDawn,
  kMemory,
  kVaapi
};

// A compound backing that combines a shared memory backing and real GPU
// backing. The real GPU backing must implement `UploadFromMemory()` and not
// have it's own shared memory segment.
// TODO(crbug.com/1293509): Support multiple GPU backings.
class GPU_GLES2_EXPORT CompoundImageBacking : public SharedImageBacking {
 public:
  // Creates a backing that contains a shared memory backing and GPU backing
  // provided by `gpu_backing_factory`.
  static std::unique_ptr<SharedImageBacking> CreateSharedMemory(
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
      uint32_t usage);

  CompoundImageBacking(const Mailbox& mailbox,
                       viz::ResourceFormat format,
                       const gfx::Size& size,
                       const gfx::ColorSpace& color_space,
                       GrSurfaceOrigin surface_origin,
                       SkAlphaType alpha_type,
                       uint32_t usage,
                       std::unique_ptr<SharedMemoryImageBacking> shm_backing,
                       std::unique_ptr<SharedImageBacking> gpu_backing);

  ~CompoundImageBacking() override;

  // Called by wrapped representations before access. This will update
  // the backing that is going to be accessed if most recent pixels are in
  // a different backing.
  void NotifyBeginAccess(SharedImageAccessStream stream,
                         RepresentationAccessMode mode);

  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override;
  gfx::Rect ClearedRect() const override;
  void SetClearedRect(const gfx::Rect& cleared_rect) override;

 protected:
  // SharedImageBacking implementation.
  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device,
      WGPUBackendType backend_type) override;
  std::unique_ptr<GLTextureImageRepresentation> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;
  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;
  std::unique_ptr<SkiaImageRepresentation> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  std::unique_ptr<OverlayImageRepresentation> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

 private:
  friend class CompoundImageBackingTest;

  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override;

  std::unique_ptr<SharedImageBacking> shm_backing_;
  std::unique_ptr<SharedImageBacking> gpu_backing_;

  // This will be true when shared memory backing has newer pixels than GPU
  // backing.
  bool shm_has_update_ = false;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_COMPOUND_IMAGE_BACKING_H_
