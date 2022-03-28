// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_OZONE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_OZONE_H_

#include <dawn/webgpu.h>

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/native_pixmap.h"

namespace gpu {
class VaapiDependencies;

// Implementation of SharedImageBacking that uses a NativePixmap created via
// an Ozone surface factory. The memory associated with the pixmap can be
// aliased by both GL and Vulkan for use in rendering or compositing.
class SharedImageBackingOzone final : public ClearTrackingSharedImageBacking {
 public:
  SharedImageBackingOzone(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      gfx::BufferPlane plane,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      scoped_refptr<SharedContextState> context_state,
      scoped_refptr<gfx::NativePixmap> pixmap,
      scoped_refptr<base::RefCountedData<DawnProcTable>> dawn_procs);

  SharedImageBackingOzone(const SharedImageBackingOzone&) = delete;
  SharedImageBackingOzone& operator=(const SharedImageBackingOzone&) = delete;

  ~SharedImageBackingOzone() override;

  // gpu::SharedImageBacking:
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override;
  scoped_refptr<gfx::NativePixmap> GetNativePixmap() override;
  bool WritePixels(base::span<const uint8_t> pixel_data,
                   SharedContextState* const shared_context_state,
                   viz::ResourceFormat format,
                   const gfx::Size& size,
                   SkAlphaType alpha_type);
  void SetSharedMemoryWrapper(SharedMemoryRegionWrapper wrapper);

  enum class AccessStream { kGL, kVulkan, kWebGPU, kOverlay, kLast };

 protected:
  std::unique_ptr<SharedImageRepresentationDawn> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device,
      WGPUBackendType backend_type) override;
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  std::unique_ptr<SharedImageRepresentationOverlay> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;
  std::unique_ptr<SharedImageRepresentationVaapi> ProduceVASurface(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      VaapiDependenciesFactory* dep_factory) override;

 private:
  friend class SharedImageRepresentationGLOzoneShared;
  friend class SharedImageRepresentationDawnOzone;
  friend class SharedImageRepresentationSkiaVkOzone;
  class SharedImageRepresentationVaapiOzone;
  class SharedImageRepresentationOverlayOzone;

  bool VaSync();

  void FlushAndSubmitIfNecessary(
      std::vector<GrBackendSemaphore> signal_semaphores,
      SharedContextState* const shared_context_state);

  bool BeginAccess(bool readonly,
                   AccessStream access_stream,
                   std::vector<gfx::GpuFenceHandle>* fences,
                   bool& need_end_fence);
  void EndAccess(bool readonly,
                 AccessStream access_stream,
                 gfx::GpuFenceHandle fence);

  // Indicates if this backing produced a VASurface that may have pending work.
  bool has_pending_va_writes_ = false;
  std::unique_ptr<VaapiDependencies> vaapi_deps_;
  gfx::BufferPlane plane_;
  uint32_t reads_in_progress_ = 0;
  bool is_write_in_progress_ = false;
  int write_streams_count_;

  scoped_refptr<gfx::NativePixmap> pixmap_;
  scoped_refptr<base::RefCountedData<DawnProcTable>> dawn_procs_;
  gfx::GpuFenceHandle write_fence_;
  base::flat_map<AccessStream, gfx::GpuFenceHandle> read_fences_;
  AccessStream last_write_stream_;
  // Set for shared memory GMB.
  SharedMemoryRegionWrapper shared_memory_wrapper_;
  scoped_refptr<SharedContextState> context_state_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_OZONE_H_
