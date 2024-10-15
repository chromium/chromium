// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_TESTS_WEBGPU_TEST_H_
#define GPU_COMMAND_BUFFER_TESTS_WEBGPU_TEST_H_

#include <dawn/webgpu_cpp.h>
#include <dawn/webgpu_cpp_print.h>

#include <memory>

#include "build/build_config.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/webgpu_cmd_ids.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
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
class WebGPUInterface;

}  // namespace webgpu

class WebGPUTest : public testing::Test {
 public:
  struct Options {
    Options();

    // Shared memory limits
    SharedMemoryLimits shared_memory_limits =
        SharedMemoryLimits::ForWebGPUContext();
    bool force_fallback_adapter = false;
    bool compatibility_mode = false;
    bool enable_unsafe_webgpu = false;
    bool use_skia_graphite = false;

    // By default, disable the blocklist so all adapters
    // can be tested.
    bool adapter_blocklist = false;
  };

 protected:
  WebGPUTest();
  ~WebGPUTest() override;

  bool WebGPUSupported() const;
  bool WebGPUSharedImageSupported() const;
  void SetUp() override;
  void TearDown() override;

  void Initialize(const Options& options);

  webgpu::WebGPUInterface* webgpu() const;
  webgpu::WebGPUImplementation* webgpu_impl() const;
  webgpu::WebGPUCmdHelper* webgpu_cmds() const;
  SharedImageInterface* GetSharedImageInterface() const;
  webgpu::WebGPUDecoder* GetDecoder() const;

  void RunPendingTasks();
  void WaitForCompletion(wgpu::Device device);
  void PollUntilIdle();

  wgpu::Device GetNewDevice();

  viz::TestGpuServiceHolder* GetGpuServiceHolder() {
    return gpu_service_holder_.get();
  }

  bool IsUsingFallbackAdapter() {
    wgpu::AdapterInfo adapter_info = {};
    adapter_.GetInfo(&adapter_info);
    return adapter_info.adapterType == wgpu::AdapterType::CPU;
  }

  static std::map<std::pair<WGPUDevice, wgpu::ErrorType>, /* matched */ bool>
      s_expected_errors;

  wgpu::Instance instance_ = nullptr;
  wgpu::Adapter adapter_ = nullptr;

 private:
  std::unique_ptr<viz::TestGpuServiceHolder> gpu_service_holder_;
  std::unique_ptr<WebGPUInProcessContext> context_;
  std::unique_ptr<webgpu::WebGPUCmdHelper> cmd_helper_;
};

#define EXPECT_WEBGPU_ERROR(device, type, statement)                           \
  do {                                                                         \
    PollUntilIdle();                                                           \
    auto it =                                                                  \
        s_expected_errors.insert({std::make_pair(device.Get(), type), false}); \
    EXPECT_TRUE(it.second)                                                     \
        << "Only one expectation per-device-per-type supported.";              \
    statement;                                                                 \
    PollUntilIdle();                                                           \
    EXPECT_TRUE(it.first->second)                                              \
        << "Expected error (" << type << ") in `" #statement "`";              \
    s_expected_errors.erase(it.first);                                         \
  } while (0)

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_TESTS_WEBGPU_TEST_H_
