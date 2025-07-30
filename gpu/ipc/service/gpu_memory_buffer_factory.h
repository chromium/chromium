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
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace viz {
class VulkanContextProvider;
}  // namespace viz

namespace gpu {

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

  // Creates a native GpuMemoryBufferHandle for MappableSI.
  virtual gfx::GpuMemoryBufferHandle CreateNativeGmbHandle(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) = 0;

  // Fills |shared_memory| with the contents of the provided |buffer_handle|
  virtual bool FillSharedMemoryRegionWithBufferContents(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion shared_memory) = 0;

 protected:
  GpuMemoryBufferFactory() = default;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_H_
