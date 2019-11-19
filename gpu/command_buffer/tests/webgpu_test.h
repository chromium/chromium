// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_TESTS_WEBGPU_TEST_H_
#define GPU_COMMAND_BUFFER_TESTS_WEBGPU_TEST_H_

#include <dawn/webgpu_cpp.h>

#include <memory>

#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
class TestGpuServiceHolder;
}

namespace gpu {

class SharedImageInterface;
class WebGPUInProcessContext;

namespace webgpu {

class WebGPUInterface;

}  // namespace webgpu

class WebGPUTest : public testing::Test {
 protected:
  struct Options {
    Options();

    // Shared memory limits
    SharedMemoryLimits shared_memory_limits =
        SharedMemoryLimits::ForWebGPUContext();
  };

  WebGPUTest();
  ~WebGPUTest() override;

  bool WebGPUSupported() const;
  bool WebGPUSharedImageSupported() const;
  void SetUp() override;
  void TearDown() override;

  void Initialize(const Options& options);

  webgpu::WebGPUInterface* webgpu() const;
  SharedImageInterface* GetSharedImageInterface() const;

  void RunPendingTasks();
  void WaitForCompletion(wgpu::Device device);

 private:
  std::unique_ptr<viz::TestGpuServiceHolder> gpu_service_holder_;
  std::unique_ptr<WebGPUInProcessContext> context_;
  bool is_initialized_ = false;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_TESTS_WEBGPU_TEST_H_
