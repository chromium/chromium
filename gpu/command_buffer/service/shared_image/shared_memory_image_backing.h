// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_MEMORY_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_MEMORY_IMAGE_BACKING_H_

#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

// Implementation of SharedImageBacking that uses Shared Memory GMB.
class SharedMemoryImageBacking : public SharedImageBacking {
 public:
  SharedMemoryImageBacking(const Mailbox& mailbox,
                           viz::SharedImageFormat format,
                           const gfx::Size& size,
                           const gfx::ColorSpace& color_space,
                           GrSurfaceOrigin surface_origin,
                           SkAlphaType alpha_type,
                           uint32_t usage,
                           SharedMemoryRegionWrapper wrapper);

  ~SharedMemoryImageBacking() override;

  // gpu::SharedImageBacking:
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  SharedImageBackingType GetType() const override;
  gfx::Rect ClearedRect() const override;
  void SetClearedRect(const gfx::Rect& cleared_rect) override;

  const SharedMemoryRegionWrapper& shared_memory_wrapper();

 protected:
  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device,
      WGPUBackendType backend_type,
      std::vector<WGPUTextureFormat> view_formats) override;
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
  std::unique_ptr<VaapiImageRepresentation> ProduceVASurface(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      VaapiDependenciesFactory* dep_factory) override;
  std::unique_ptr<MemoryImageRepresentation> ProduceMemory(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

 private:
  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDumpGuid client_guid,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override;

  // Set for shared memory GMB.
  SharedMemoryRegionWrapper shared_memory_wrapper_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_MEMORY_IMAGE_BACKING_H_
