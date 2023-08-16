// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OZONE_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OZONE_IMAGE_BACKING_H_

#include <dawn/webgpu_cpp.h>

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/gl_ozone_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
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
class OzoneImageBacking final : public ClearTrackingSharedImageBacking {
 public:
  OzoneImageBacking(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      gfx::BufferPlane plane,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      scoped_refptr<SharedContextState> context_state,
      scoped_refptr<gfx::NativePixmap> pixmap,
      const GpuDriverBugWorkarounds& workarounds,
      bool use_passthrough,
      absl::optional<gfx::BufferUsage> buffer_usage = absl::nullopt);

  OzoneImageBacking(const OzoneImageBacking&) = delete;
  OzoneImageBacking& operator=(const OzoneImageBacking&) = delete;

  ~OzoneImageBacking() override;

  // gpu::SharedImageBacking:
  SharedImageBackingType GetType() const override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool UploadFromMemory(const std::vector<SkPixmap>& pixmaps) override;
  scoped_refptr<gfx::NativePixmap> GetNativePixmap() override;
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override;

  enum class AccessStream { kGL, kVulkan, kWebGPU, kOverlay, kLast };

 protected:
  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      const wgpu::Device& device,
      wgpu::BackendType backend_type,
      std::vector<wgpu::TextureFormat> view_formats) override;
  std::unique_ptr<GLTextureImageRepresentation> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;
  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;
  std::unique_ptr<SkiaGaneshImageRepresentation> ProduceSkiaGanesh(
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

 private:
  friend class GLOzoneImageRepresentationShared;
  friend class DawnOzoneImageRepresentation;
  friend class SkiaVkOzoneImageRepresentation;
  class VaapiOzoneImageRepresentation;
  class OverlayOzoneImageRepresentation;

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
  std::vector<scoped_refptr<GLOzoneImageRepresentationShared::TextureHolder>>
      cached_texture_holders_;

  // Write fence that is external and does not do Begin/EndAccess (eg. exo)
  gfx::GpuFenceHandle external_write_fence_;
  gfx::GpuFenceHandle write_fence_;
  base::flat_map<AccessStream, gfx::GpuFenceHandle> read_fences_;
  AccessStream last_write_stream_;
  scoped_refptr<SharedContextState> context_state_;
  const GpuDriverBugWorkarounds workarounds_;
  bool use_passthrough_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OZONE_IMAGE_BACKING_H_
