// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GPU_MEMORY_BUFFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_CLIENT_GPU_MEMORY_BUFFER_MANAGER_H_

#include <memory>

#include "base/threading/platform_thread.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace base {
class WaitableEvent;
}

namespace gpu {

class GPU_EXPORT GpuMemoryBufferManager {
 public:
  GpuMemoryBufferManager();
  virtual ~GpuMemoryBufferManager();

  // Creates a GpuMemoryBuffer that can be shared with another process. It can
  // be called on any thread. If |shutdown_event| is specified, then the browser
  // implementation (HostGpuMemoryBufferManager) will cancel pending create
  // calls if this is signalled.
  virtual std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::SurfaceHandle surface_handle,
      base::WaitableEvent* shutdown_event) = 0;

  // Copies pixel data of GMB to the provided shared memory region.
  virtual void CopyGpuMemoryBufferAsync(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion memory_region,
      base::OnceCallback<void(bool)> callback) = 0;
  virtual bool CopyGpuMemoryBufferSync(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion memory_region) = 0;

 protected:
  class GPU_EXPORT AllocatedBufferInfo {
   public:
    AllocatedBufferInfo(const gfx::GpuMemoryBufferHandle& handle,
                        const gfx::Size& size,
                        gfx::BufferFormat format);
    ~AllocatedBufferInfo();

    gfx::GpuMemoryBufferType type() const { return type_; }

    // Add a memory dump for this buffer to |pmd|. Returns false if adding the
    // dump failed.
    bool OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                      int client_id,
                      uint64_t client_tracing_process_id) const;

   private:
    gfx::GpuMemoryBufferId buffer_id_;
    gfx::GpuMemoryBufferType type_;
    size_t size_in_bytes_;
    base::UnguessableToken shared_memory_guid_;
  };
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_GPU_MEMORY_BUFFER_MANAGER_H_
