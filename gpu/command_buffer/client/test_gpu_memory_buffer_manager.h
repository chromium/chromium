// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_TEST_GPU_MEMORY_BUFFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_CLIENT_TEST_GPU_MEMORY_BUFFER_MANAGER_H_

#include <memory>

#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace base {
class WaitableEvent;
}

namespace gpu {

class TestGpuMemoryBufferManager {
 public:
  TestGpuMemoryBufferManager();

  TestGpuMemoryBufferManager(const TestGpuMemoryBufferManager&) = delete;
  TestGpuMemoryBufferManager& operator=(const TestGpuMemoryBufferManager&) =
      delete;

  ~TestGpuMemoryBufferManager();

  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::SurfaceHandle surface_handle,
      base::WaitableEvent* shutdown_event);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_TEST_GPU_MEMORY_BUFFER_MANAGER_H_
