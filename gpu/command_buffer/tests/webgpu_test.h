// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_TESTS_WEBGPU_TEST_H_
#define GPU_COMMAND_BUFFER_TESTS_WEBGPU_TEST_H_

#include <dawn/webgpu_cpp.h>

#include <memory>

#include "build/build_config.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/webgpu_cmd_ids.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MAC)
#include "gpu/ipc/service/gpu_memory_buffer_factory_io_surface.h"
#endif

namespace viz {
class TestGpuServiceHolder;
}

namespace gpu {

class SharedImageInterface;
class WebGPUInProcessContext;

namespace webgpu {

class WebGPUCmdHelper;
class WebGPUDecoder;
class WebGPUImplementation;

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

  webgpu::WebGPUImplementation* webgpu() const;
  webgpu::WebGPUCmdHelper* webgpu_cmds() const;
  SharedImageInterface* GetSharedImageInterface() const;
  webgpu::WebGPUDecoder* GetDecoder() const;

  void RunPendingTasks();
  void WaitForCompletion(wgpu::Device device);

  wgpu::Device GetNewDevice();

  viz::TestGpuServiceHolder* GetGpuServiceHolder() {
    return gpu_service_holder_.get();
  }

  uint32_t GetAdapterId() const { return adapter_id_; }

  const WGPUDeviceProperties& GetDeviceProperties() const {
    return device_properties_;
  }

 private:
  std::unique_ptr<viz::TestGpuServiceHolder> gpu_service_holder_;
  std::unique_ptr<WebGPUInProcessContext> context_;
  std::unique_ptr<webgpu::WebGPUCmdHelper> cmd_helper_;
#if defined(OS_MAC)
  // SharedImages on macOS require a valid image factory.
  GpuMemoryBufferFactoryIOSurface image_factory_;
#endif
  uint32_t adapter_id_;
  WGPUDeviceProperties device_properties_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_TESTS_WEBGPU_TEST_H_
