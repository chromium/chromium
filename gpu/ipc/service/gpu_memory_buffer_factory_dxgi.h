// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_DXGI_H_
#define GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_DXGI_H_

#include <d3d11_1.h>
#include <dxgi.h>
#include <wrl/client.h>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

class GPU_IPC_SERVICE_EXPORT GpuMemoryBufferFactoryDXGI
    : public GpuMemoryBufferFactory {
 public:
  // Creates new instance of GpuMemoryBufferFactoryDXGI. `io_runner` is needed
  // in order to create GpuMemoryBuffers on the correct thread. GpuServiceImpl
  // calls into this class from IO runner (when processing IPC requests), so
  // we need to ensure that other callers are able to thread-hop to that runner
  // when creating GMBs (so far, the other caller for whom it matters is
  // `FrameSinkVideoCapturerImpl` when running in GMB mode on Windows).
  explicit GpuMemoryBufferFactoryDXGI(
      scoped_refptr<base::SingleThreadTaskRunner> io_runner = nullptr);
  ~GpuMemoryBufferFactoryDXGI() override;

  GpuMemoryBufferFactoryDXGI(const GpuMemoryBufferFactoryDXGI&) = delete;
  GpuMemoryBufferFactoryDXGI& operator=(const GpuMemoryBufferFactoryDXGI&) =
      delete;

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

 private:
  Microsoft::WRL::ComPtr<ID3D11Device> GetOrCreateD3D11Device();

  gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferOnIO(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      const gfx::Size& framebuffer_size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      int client_id,
      SurfaceHandle surface_handle);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_
      GUARDED_BY_CONTEXT(thread_checker_);

  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture_;

  // May be null for testing:
  scoped_refptr<base::SingleThreadTaskRunner> io_runner_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_DXGI_H_
