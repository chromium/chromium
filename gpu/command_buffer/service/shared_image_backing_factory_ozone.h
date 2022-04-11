// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_OZONE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_OZONE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image_backing_ozone.h"
#include "gpu/gpu_gles2_export.h"

struct DawnProcTable;

namespace gpu {

class SharedContextState;

// Implementation of SharedImageBackingFactory that produces NativePixmap
// backed SharedImages.
class GPU_GLES2_EXPORT SharedImageBackingFactoryOzone
    : public SharedImageBackingFactory {
 public:
  explicit SharedImageBackingFactoryOzone(
      SharedContextState* shared_context_state);

  ~SharedImageBackingFactoryOzone() override;

  // SharedImageBackingFactory implementation
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      bool is_thread_safe) override;

  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      base::span<const uint8_t> pixel_data) override;

  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      int client_id,
      gfx::GpuMemoryBufferHandle handle,
      gfx::BufferFormat format,
      gfx::BufferPlane plane,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage) override;

  bool IsSupported(uint32_t usage,
                   viz::ResourceFormat format,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   GrContextType gr_context_type,
                   bool* allow_legacy_mailbox,
                   bool is_pixel_used) override;

 private:
  bool CanImportGpuMemoryBufferToVulkan(
      gfx::GpuMemoryBufferType memory_buffer_type);

  SharedContextState* const shared_context_state_;
  scoped_refptr<base::RefCountedData<DawnProcTable>> dawn_procs_;

  std::unique_ptr<SharedImageBackingOzone> CreateSharedImageInternal(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_OZONE_H_
