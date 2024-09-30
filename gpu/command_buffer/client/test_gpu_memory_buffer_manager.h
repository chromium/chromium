// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_TEST_GPU_MEMORY_BUFFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_CLIENT_TEST_GPU_MEMORY_BUFFER_MANAGER_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"

namespace gpu {

class TestGpuMemoryBufferManager : public gpu::GpuMemoryBufferManager {
 public:
  TestGpuMemoryBufferManager();

  TestGpuMemoryBufferManager(const TestGpuMemoryBufferManager&) = delete;
  TestGpuMemoryBufferManager& operator=(const TestGpuMemoryBufferManager&) =
      delete;

  ~TestGpuMemoryBufferManager() override;

  std::unique_ptr<TestGpuMemoryBufferManager>
  CreateClientGpuMemoryBufferManager();
  int GetClientId() { return client_id_; }

  void OnGpuMemoryBufferDestroyed(gfx::GpuMemoryBufferId gpu_memory_buffer_id);

  void SetFailOnCreate(bool fail_on_create) {
    fail_on_create_ = fail_on_create;
  }

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

 private:
  // This class is called by multiple threads at the same time. Hold this lock
  // for the duration of all member functions, to ensure consistency.
  // https://crbug.com/690588, https://crbug.com/859020
  base::Lock lock_;

  // Buffers allocated by this manager.
  int last_gpu_memory_buffer_id_ = 1000;
  std::map<int, raw_ptr<gfx::GpuMemoryBuffer, CtnExperimental>> buffers_;

  // Parent information for child managers.
  int client_id_ = -1;
  raw_ptr<TestGpuMemoryBufferManager> parent_gpu_memory_buffer_manager_ =
      nullptr;

  // Child infomration for parent managers.
  int last_client_id_ = 5000;
  std::map<int, raw_ptr<TestGpuMemoryBufferManager, CtnExperimental>> clients_;

  bool fail_on_create_ = false;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_TEST_GPU_MEMORY_BUFFER_MANAGER_H_
