// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_GPU_CLIENT_GPU_MEMORY_BUFFER_MANAGER_H_
#define SERVICES_VIZ_PUBLIC_CPP_GPU_CLIENT_GPU_MEMORY_BUFFER_MANAGER_H_

#include <memory>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_pool.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/ipc/common/client_gmb_interface.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/gpu.mojom.h"

namespace base {
class WaitableEvent;
}

namespace gpu {
class GpuMemoryBufferSupport;
}

namespace viz {

// Implements gpu::GpuMemoryBufferManager based on a given
// mojom::GpuMemoryBufferFactory
class ClientGpuMemoryBufferManager : public gpu::GpuMemoryBufferManager {
 public:
  ClientGpuMemoryBufferManager(
      mojo::PendingRemote<gpu::mojom::ClientGmbInterface> gpu_direct);

  ClientGpuMemoryBufferManager(const ClientGpuMemoryBufferManager&) = delete;
  ClientGpuMemoryBufferManager& operator=(const ClientGpuMemoryBufferManager&) =
      delete;

  ~ClientGpuMemoryBufferManager() override;

 private:
  void InitThread(
      mojo::PendingRemote<gpu::mojom::ClientGmbInterface> gpu_direct_remote);
  void TearDownThread();
  void DisconnectGpuOnThread();
  void AllocateGpuMemoryBufferOnThread(const gfx::Size& size,
                                       gfx::BufferFormat format,
                                       gfx::BufferUsage usage,
                                       gfx::GpuMemoryBufferHandle* handle,
                                       base::WaitableEvent* wait);
  void OnGpuMemoryBufferAllocatedOnThread(
      gfx::GpuMemoryBufferHandle* ret_handle,
      base::WaitableEvent* wait,
      gfx::GpuMemoryBufferHandle handle);
  void DeletedGpuMemoryBuffer(gfx::GpuMemoryBufferId id);

  // Overridden from gpu::GpuMemoryBufferManager:
  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::SurfaceHandle surface_handle,
      base::WaitableEvent* shutdown_event) override;
  void CopyGpuMemoryBufferAsync(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion memory_region,
      base::OnceCallback<void(bool)> callback) override;
  bool CopyGpuMemoryBufferSync(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion memory_region) override;

  int counter_ = 0;
  // TODO(sad): Explore the option of doing this from an existing thread.
  base::Thread thread_;
  mojo::Remote<gpu::mojom::ClientGmbInterface> gpu_direct_;
  base::WeakPtr<ClientGpuMemoryBufferManager> weak_ptr_;
  std::set<raw_ptr<base::WaitableEvent, SetExperimental>>
      pending_allocation_waiters_;
  std::unique_ptr<gpu::GpuMemoryBufferSupport> gpu_memory_buffer_support_;

  scoped_refptr<base::UnsafeSharedMemoryPool> pool_;

  base::WeakPtrFactory<ClientGpuMemoryBufferManager> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // SERVICES_VIZ_PUBLIC_CPP_GPU_CLIENT_GPU_MEMORY_BUFFER_MANAGER_H_
