// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_OZONE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_OZONE_H_

#include <dawn/webgpu.h>

#include <memory>

#include "base/macros.h"
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
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/native_pixmap.h"

namespace gpu {
class VaapiDependencies;

// Implementation of SharedImageBacking that uses a NativePixmap created via
// an Ozone surface factory. The memory associated with the pixmap can be
// aliased by both GL and Vulkan for use in rendering or compositing.
class SharedImageBackingOzone final : public ClearTrackingSharedImageBacking {
 public:
  static std::unique_ptr<SharedImageBackingOzone> Create(
      scoped_refptr<base::RefCountedData<DawnProcTable>> dawn_procs,
      SharedContextState* context_state,
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      SurfaceHandle surface_handle);
  ~SharedImageBackingOzone() override;

  // gpu::SharedImageBacking:
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override;

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
  friend class SharedImageRepresentationGLOzone;
  friend class SharedImageRepresentationDawnOzone;
  class SharedImageRepresentationVaapiOzone;

  SharedImageBackingOzone(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      SharedContextState* context_state,
      scoped_refptr<gfx::NativePixmap> pixmap,
      scoped_refptr<base::RefCountedData<DawnProcTable>> dawn_procs);

  bool VaSync();

  // Indicates if this backing produced a VASurface that may have pending work.
  bool has_pending_va_writes_ = false;
  std::unique_ptr<VaapiDependencies> vaapi_deps_;
  scoped_refptr<gfx::NativePixmap> pixmap_;
  scoped_refptr<base::RefCountedData<DawnProcTable>> dawn_procs_;

  DISALLOW_COPY_AND_ASSIGN(SharedImageBackingOzone);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_OZONE_H_
