// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_H_
#define GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace viz {
class VulkanContextProvider;
}  // namespace viz

namespace gpu {

class ImageFactory;

class GPU_IPC_SERVICE_EXPORT GpuMemoryBufferFactory {
 public:
  virtual ~GpuMemoryBufferFactory() = default;

  // Creates a new factory instance for native GPU memory buffers. Returns null
  // if native buffers are not supported.
  static std::unique_ptr<GpuMemoryBufferFactory> CreateNativeType(
      viz::VulkanContextProvider* vulkan_context_provider);

  // Creates a new GPU memory buffer instance. A valid handle is returned on
  // success. This method is thread-safe but it should not be called on the IO
  // thread as it can lead to deadlocks (see https://crbug.com/981721). Instead
  // use the asynchronous version on the IO thread.
  virtual gfx::GpuMemoryBufferHandle CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      int client_id,
      SurfaceHandle surface_handle) = 0;

  // Same as above, but returns the result asynchrounously. Safe to use on the
  // IO thread. |callback| will be called on the same thread that calls this
  // method, and it might be called on the same call stack.
  using CreateGpuMemoryBufferAsyncCallback =
      base::OnceCallback<void(gfx::GpuMemoryBufferHandle)>;
  virtual void CreateGpuMemoryBufferAsync(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      int client_id,
      SurfaceHandle surface_handle,
      CreateGpuMemoryBufferAsyncCallback callback);

  // Destroys GPU memory buffer identified by |id|. It can be called on any
  // thread.
  virtual void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                      int client_id) = 0;

  // Type-checking downcast routine.
  virtual ImageFactory* AsImageFactory() = 0;

 protected:
  GpuMemoryBufferFactory() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferFactory);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_H_
