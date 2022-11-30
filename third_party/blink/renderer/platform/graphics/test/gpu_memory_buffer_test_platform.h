// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_GPU_MEMORY_BUFFER_TEST_PLATFORM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_GPU_MEMORY_BUFFER_TEST_PLATFORM_H_

#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {
class GpuMemoryBufferTestPlatform : public blink::TestingPlatformSupport {
 private:
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override {
    return &test_gpu_memory_buffer_manager_;
  }

  viz::TestGpuMemoryBufferManager test_gpu_memory_buffer_manager_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_GPU_MEMORY_BUFFER_TEST_PLATFORM_H_
