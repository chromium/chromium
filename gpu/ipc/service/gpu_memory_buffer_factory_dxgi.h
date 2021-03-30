// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_DXGI_H_
#define GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_DXGI_H_

#include <utility>

#include <d3d11_1.h>
#include <dxgi.h>
#include <wrl/client.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_image_dxgi.h"

namespace gl {
class GLImage;
}

namespace gpu {

class GPU_IPC_SERVICE_EXPORT GpuMemoryBufferFactoryDXGI
    : public GpuMemoryBufferFactory,
      public ImageFactory {
 public:
  GpuMemoryBufferFactoryDXGI();
  GpuMemoryBufferFactoryDXGI(const GpuMemoryBufferFactoryDXGI&) = delete;
  GpuMemoryBufferFactoryDXGI& operator=(const GpuMemoryBufferFactoryDXGI&) =
      delete;

  ~GpuMemoryBufferFactoryDXGI() override;

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
  unsigned RequiredTextureType() override;
  bool SupportsFormatRGB() override;

 private:
  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture_;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_DXGI_H_
