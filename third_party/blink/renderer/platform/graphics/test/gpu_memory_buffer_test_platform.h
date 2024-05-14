// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_GPU_MEMORY_BUFFER_TEST_PLATFORM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_GPU_MEMORY_BUFFER_TEST_PLATFORM_H_

#include "gpu/command_buffer/client/test_gpu_memory_buffer_manager.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {
class GpuMemoryBufferTestPlatform : public blink::TestingPlatformSupport {
 public:
  GpuMemoryBufferTestPlatform() {
    SharedGpuContext::SetGpuMemoryBufferManagerForTesting(
        &test_gpu_memory_buffer_manager_);
  }
  ~GpuMemoryBufferTestPlatform() override {
    SharedGpuContext::SetGpuMemoryBufferManagerForTesting(nullptr);
  }

  bool IsGpuCompositingDisabled() const override { return false; }

 private:
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override {
    return &test_gpu_memory_buffer_manager_;
  }

  gpu::TestGpuMemoryBufferManager test_gpu_memory_buffer_manager_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_GPU_MEMORY_BUFFER_TEST_PLATFORM_H_
