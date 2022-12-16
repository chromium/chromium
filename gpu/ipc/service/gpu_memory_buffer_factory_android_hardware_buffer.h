// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_ANDROID_HARDWARE_BUFFER_H_
#define GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_ANDROID_HARDWARE_BUFFER_H_

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/synchronization/lock.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"

namespace gpu {
class GpuMemoryBufferImplAndroidHardwareBuffer;

class GPU_IPC_SERVICE_EXPORT GpuMemoryBufferFactoryAndroidHardwareBuffer
    : public GpuMemoryBufferFactory {
 public:
  GpuMemoryBufferFactoryAndroidHardwareBuffer();

  GpuMemoryBufferFactoryAndroidHardwareBuffer(
      const GpuMemoryBufferFactoryAndroidHardwareBuffer&) = delete;
  GpuMemoryBufferFactoryAndroidHardwareBuffer& operator=(
      const GpuMemoryBufferFactoryAndroidHardwareBuffer&) = delete;

  ~GpuMemoryBufferFactoryAndroidHardwareBuffer() override;

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
  using BufferMapKey = std::pair<gfx::GpuMemoryBufferId, int>;
  using BufferMap =
      base::flat_map<BufferMapKey,
                     std::unique_ptr<GpuMemoryBufferImplAndroidHardwareBuffer>>;

  base::Lock lock_;
  BufferMap buffer_map_ GUARDED_BY(lock_);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_ANDROID_HARDWARE_BUFFER_H_
