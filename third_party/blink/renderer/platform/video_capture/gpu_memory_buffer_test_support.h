// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_VIDEO_CAPTURE_GPU_MEMORY_BUFFER_TEST_SUPPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_VIDEO_CAPTURE_GPU_MEMORY_BUFFER_TEST_SUPPORT_H_

#include <memory>

#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"

namespace media {
class MockGpuVideoAcceleratorFactories;
}  // namespace media

namespace viz {
class TestSharedImageInterface;
}  // namespace viz

namespace blink {

class FakeGpuMemoryBufferSupport : public gpu::GpuMemoryBufferSupport {
 public:
  std::unique_ptr<gpu::GpuMemoryBufferImpl> CreateGpuMemoryBufferImplFromHandle(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::GpuMemoryBufferImpl::DestructionCallback callback) override;
};

class TestingPlatformSupportForGpuMemoryBuffer
    : public IOTaskRunnerTestingPlatformSupport {
 public:
  TestingPlatformSupportForGpuMemoryBuffer();
  ~TestingPlatformSupportForGpuMemoryBuffer() override;
  media::GpuVideoAcceleratorFactories* GetGpuFactories() override;

 private:
  std::unique_ptr<viz::TestSharedImageInterface> sii_;
  std::unique_ptr<media::MockGpuVideoAcceleratorFactories> gpu_factories_;
  base::Thread media_thread_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_VIDEO_CAPTURE_GPU_MEMORY_BUFFER_TEST_SUPPORT_H_
