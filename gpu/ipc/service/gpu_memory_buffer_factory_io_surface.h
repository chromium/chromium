// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_IO_SURFACE_H_
#define GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_IO_SURFACE_H_

#include <unordered_map>
#include <utility>

#include <IOSurface/IOSurface.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/mac/io_surface.h"

namespace gl {
class GLImage;
}

namespace gpu {

class GPU_IPC_SERVICE_EXPORT GpuMemoryBufferFactoryIOSurface
    : public GpuMemoryBufferFactory,
      public ImageFactory {
 public:
  GpuMemoryBufferFactoryIOSurface();
  ~GpuMemoryBufferFactoryIOSurface() override;

  // Overridden from GpuMemoryBufferFactory:
  gfx::GpuMemoryBufferHandle CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      const gfx::Size& framebuffer_size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      int client_id,
      SurfaceHandle surface_handle) override;
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id) override;
  bool FillSharedMemoryRegionWithBufferContents(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion shared_memory) override;
  ImageFactory* AsImageFactory() override;

  // Overridden from ImageFactory:
  scoped_refptr<gl::GLImage> CreateImageForGpuMemoryBuffer(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      int client_id,
      SurfaceHandle surface_handle) override;
  bool SupportsCreateAnonymousImage() const override;
  scoped_refptr<gl::GLImage> CreateAnonymousImage(const gfx::Size& size,
                                                  gfx::BufferFormat format,
                                                  gfx::BufferUsage usage,
                                                  SurfaceHandle surface_handle,
                                                  bool* is_cleared) override;
  unsigned RequiredTextureType() override;
  bool SupportsFormatRGB() override;

 private:
  typedef std::pair<gfx::IOSurfaceId, int> IOSurfaceMapKey;
  typedef std::unordered_map<IOSurfaceMapKey,
                             base::ScopedCFTypeRef<IOSurfaceRef>>
      IOSurfaceMap;
  // TODO(reveman): Remove |io_surfaces_| and allow IOSurface backed GMBs to be
  // used with any GPU process by passing a mach_port to CreateImageCHROMIUM.
  IOSurfaceMap io_surfaces_;
  base::Lock io_surfaces_lock_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferFactoryIOSurface);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_IO_SURFACE_H_
