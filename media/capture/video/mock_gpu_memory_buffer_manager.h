// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MOCK_GPU_MEMORY_BUFFER_MANAGER_H_
#define MEDIA_CAPTURE_VIDEO_MOCK_GPU_MEMORY_BUFFER_MANAGER_H_

#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/color_space.h"

namespace media {
namespace unittest_internal {

class MockGpuMemoryBufferManager : public gpu::GpuMemoryBufferManager {
 public:
  MockGpuMemoryBufferManager();

  MockGpuMemoryBufferManager(const MockGpuMemoryBufferManager&) = delete;
  MockGpuMemoryBufferManager& operator=(const MockGpuMemoryBufferManager&) =
      delete;

  ~MockGpuMemoryBufferManager() override;

  MOCK_METHOD5(CreateGpuMemoryBuffer,
               std::unique_ptr<gfx::GpuMemoryBuffer>(
                   const gfx::Size& size,
                   gfx::BufferFormat format,
                   gfx::BufferUsage usage,
                   gpu::SurfaceHandle surface_handle,
                   base::WaitableEvent* shutdown_event));

  MOCK_METHOD3(CopyGpuMemoryBufferAsync,
               void(gfx::GpuMemoryBufferHandle buffer_handle,
                    base::UnsafeSharedMemoryRegion memory_region,
                    base::OnceCallback<void(bool)> callback));

  MOCK_METHOD2(CopyGpuMemoryBufferSync,
               bool(gfx::GpuMemoryBufferHandle buffer_handle,
                    base::UnsafeSharedMemoryRegion memory_region));

  static std::unique_ptr<gfx::GpuMemoryBuffer> CreateFakeGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::SurfaceHandle surface_handle,
      base::WaitableEvent* shutdown_event);
};

}  // namespace unittest_internal
}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_MOCK_GPU_MEMORY_BUFFER_MANAGER_H_
