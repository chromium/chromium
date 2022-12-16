// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_IO_SURFACE_H_
#define GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_IO_SURFACE_H_

#include "gpu/ipc/service/gpu_memory_buffer_factory.h"

namespace gpu {

class GPU_IPC_SERVICE_EXPORT GpuMemoryBufferFactoryIOSurface
    : public GpuMemoryBufferFactory {
 public:
  GpuMemoryBufferFactoryIOSurface();

  GpuMemoryBufferFactoryIOSurface(const GpuMemoryBufferFactoryIOSurface&) =
      delete;
  GpuMemoryBufferFactoryIOSurface& operator=(
      const GpuMemoryBufferFactoryIOSurface&) = delete;

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
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_IO_SURFACE_H_
