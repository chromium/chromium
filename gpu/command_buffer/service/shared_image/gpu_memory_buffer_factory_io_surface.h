// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GPU_MEMORY_BUFFER_FACTORY_IO_SURFACE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GPU_MEMORY_BUFFER_FACTORY_IO_SURFACE_H_

#include "gpu/command_buffer/service/shared_image/gpu_memory_buffer_factory.h"

namespace gpu {

class GPU_GLES2_EXPORT GpuMemoryBufferFactoryIOSurface
    : public GpuMemoryBufferFactory {
 public:
  GpuMemoryBufferFactoryIOSurface();

  GpuMemoryBufferFactoryIOSurface(const GpuMemoryBufferFactoryIOSurface&) =
      delete;
  GpuMemoryBufferFactoryIOSurface& operator=(
      const GpuMemoryBufferFactoryIOSurface&) = delete;

  ~GpuMemoryBufferFactoryIOSurface() override;

  // Overridden from GpuMemoryBufferFactory:
  gfx::GpuMemoryBufferHandle CreateNativeGmbHandle(
      const gfx::Size& size,
      viz::SharedImageFormat format,
      gfx::BufferUsage usage) override;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GPU_MEMORY_BUFFER_FACTORY_IO_SURFACE_H_
