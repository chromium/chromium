// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_H_
#define GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace viz {
class VulkanContextProvider;
}  // namespace viz

namespace gpu {

// This enums will be used by clients when creating native gmb handles via
// GpuMemoryBufferFactory::CreateNativeGmbHandle(). This ensure each client uses
// a unique id.
enum class MappableSIClientGmbId : int {
  kGpuChannel = 1,
  kGmbVideoFramePoolContext = 2,
  kLast = 2
};

class GPU_IPC_SERVICE_EXPORT GpuMemoryBufferFactory {
 public:
  GpuMemoryBufferFactory(const GpuMemoryBufferFactory&) = delete;
  GpuMemoryBufferFactory& operator=(const GpuMemoryBufferFactory&) = delete;

  virtual ~GpuMemoryBufferFactory() = default;

  // Creates a new factory instance for native GPU memory buffers. Returns null
  // if native buffers are not supported.
  static std::unique_ptr<GpuMemoryBufferFactory> CreateNativeType(
      viz::VulkanContextProvider* vulkan_context_provider,
      scoped_refptr<base::SingleThreadTaskRunner> io_runner = nullptr);

  // Creates a native GpuMemoryBufferHandle for MappableSI work. Note that
  // every client should use a different |id| here otherwise it can result in
  // errors due to multiple clients creating and destroying GMBs with same |id|
  // from multiple threads. Using MappableSIClientGmbId here ensures that every
  // client uses unique id assigned to it and also makes it easier to track.
  gfx::GpuMemoryBufferHandle CreateNativeGmbHandle(MappableSIClientGmbId id,
                                                   const gfx::Size& size,
                                                   gfx::BufferFormat format,
                                                   gfx::BufferUsage usage);

  // Creates a new GPU memory buffer instance. A valid handle is returned on
  // success. This method is thread-safe but it should not be called on the IO
  // thread as it can lead to deadlocks (see https://crbug.com/981721). Instead
  // use the asynchronous version on the IO thread. |framebuffer_size| specifies
  // the size used to create a framebuffer when the |usage| requires it and the
  // particular GpuMemoryBufferFactory implementation supports it (for example,
  // when creating a buffer for scanout using the Ozone/DRM backend).
  virtual gfx::GpuMemoryBufferHandle CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      const gfx::Size& framebuffer_size,
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

  // Fills |shared_memory| with the contents of the provided |buffer_handle|
  virtual bool FillSharedMemoryRegionWithBufferContents(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion shared_memory) = 0;

 protected:
  GpuMemoryBufferFactory() = default;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_H_
