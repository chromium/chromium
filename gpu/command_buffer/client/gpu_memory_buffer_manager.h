// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GPU_MEMORY_BUFFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_CLIENT_GPU_MEMORY_BUFFER_MANAGER_H_

#include <memory>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/threading/platform_thread.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace base {
class WaitableEvent;
}

namespace gpu {

// Used to observe the destruction of GpuMemoryBufferManager.
class GPU_EXPORT GpuMemoryBufferManagerObserver : public base::CheckedObserver {
 public:
  virtual void OnGpuMemoryBufferManagerDestroyed() = 0;

 protected:
  ~GpuMemoryBufferManagerObserver() override = default;
};

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

  // Checks if the GpuMemoryBufferManager is connected to the GPU Service
  // Currently on GPU process crash the connection isn't restored.
  virtual bool IsConnected() = 0;

  // Implementations of GpuMemoryBufferManager can override below methods if
  // they want to add/remove observers to notify its destruction.
  virtual void AddObserver(GpuMemoryBufferManagerObserver* observer);
  virtual void RemoveObserver(GpuMemoryBufferManagerObserver* observer);

 protected:
  void NotifyObservers();
  base::ObserverList<GpuMemoryBufferManagerObserver> observers_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_GPU_MEMORY_BUFFER_MANAGER_H_
