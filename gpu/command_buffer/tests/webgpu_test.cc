// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/tests/webgpu_test.h"

#include <dawn/dawn_proc.h>
#include <dawn/webgpu.h>

#include "base/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/client/webgpu_cmd_helper.h"
#include "gpu/command_buffer/client/webgpu_implementation.h"
#include "gpu/command_buffer/service/webgpu_decoder.h"
#include "gpu/config/gpu_test_config.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "gpu/ipc/webgpu_in_process_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {

void OnRequestAdapterCallback(int32_t adapter_service_id,
                              const WGPUDeviceProperties& properties,
                              const char* error_message) {}

void CountCallback(int* count) {
  (*count)++;
}

}  // anonymous namespace

WebGPUTest::Options::Options() = default;

WebGPUTest::WebGPUTest() = default;
WebGPUTest::~WebGPUTest() = default;

bool WebGPUTest::WebGPUSupported() const {
  // crbug.com(941685): Vulkan driver crashes on Linux FYI Release (AMD R7 240).
  // Win7 does not support WebGPU
  if (GPUTestBotConfig::CurrentConfigMatches("Linux AMD") ||
      GPUTestBotConfig::CurrentConfigMatches("Win7")) {
    return false;
  }

  return true;
}

bool WebGPUTest::WebGPUSharedImageSupported() const {
  // Currently WebGPUSharedImage is only implemented on Mac, Linux and Windows
#if (defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
     defined(OS_WIN)) &&                                             \
    BUILDFLAG(USE_DAWN)
  return true;
#else
  return false;
#endif
}

void WebGPUTest::SetUp() {
  if (!WebGPUSupported()) {
    return;
  }

  gpu::GpuPreferences gpu_preferences;
  gpu_preferences.enable_webgpu = true;
#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && BUILDFLAG(USE_DAWN)
  gpu_preferences.use_vulkan = gpu::VulkanImplementationName::kNative;
  gpu_preferences.gr_context_type = gpu::GrContextType::kVulkan;
#elif defined(OS_WIN)
  // D3D shared images are only supported with passthrough command decoder.
  gpu_preferences.use_passthrough_cmd_decoder = true;
#endif
  gpu_service_holder_ =
      std::make_unique<viz::TestGpuServiceHolder>(gpu_preferences);
}

void WebGPUTest::TearDown() {
  context_.reset();
}

void WebGPUTest::Initialize(const Options& options) {
  if (!WebGPUSupported()) {
    return;
  }

  ContextCreationAttribs attributes;
  attributes.bind_generates_resource = false;
  attributes.enable_gles2_interface = false;
  attributes.context_type = CONTEXT_TYPE_WEBGPU;

  static constexpr GpuMemoryBufferManager* memory_buffer_manager = nullptr;
#if defined(OS_MAC)
  ImageFactory* image_factory = &image_factory_;
#else
  static constexpr ImageFactory* image_factory = nullptr;
#endif
  static constexpr GpuChannelManagerDelegate* channel_manager = nullptr;
  context_ = std::make_unique<WebGPUInProcessContext>();
  ContextResult result =
      context_->Initialize(gpu_service_holder_->task_executor(), attributes,
                           options.shared_memory_limits, memory_buffer_manager,
                           image_factory, channel_manager);
  ASSERT_EQ(result, ContextResult::kSuccess);

  cmd_helper_ = std::make_unique<webgpu::WebGPUCmdHelper>(
      context_->GetCommandBufferForTest());
  ASSERT_TRUE(
      webgpu()->RequestAdapterAsync(webgpu::PowerPreference::kDefault,
                                    base::BindOnce(&OnRequestAdapterCallback)));

  DawnProcTable procs = webgpu()->GetProcs();
  dawnProcSetProcs(&procs);
}

webgpu::WebGPUImplementation* WebGPUTest::webgpu() const {
  return context_->GetImplementation();
}

webgpu::WebGPUCmdHelper* WebGPUTest::webgpu_cmds() const {
  return cmd_helper_.get();
}

SharedImageInterface* WebGPUTest::GetSharedImageInterface() const {
  return context_->GetCommandBufferForTest()->GetSharedImageInterface();
}

webgpu::WebGPUDecoder* WebGPUTest::GetDecoder() const {
  return context_->GetCommandBufferForTest()->GetWebGPUDecoderForTest();
}

void WebGPUTest::RunPendingTasks() {
  context_->GetTaskRunner()->RunPendingTasks();
}

void WebGPUTest::WaitForCompletion(wgpu::Device device) {
  // Insert a fence signal and wait for it to be signaled. The guarantees of
  // Dawn are that all previous operations will have been completed and more
  // importantly the callbacks will have been called.
  wgpu::Queue queue = device.GetDefaultQueue();
  wgpu::FenceDescriptor fence_desc{nullptr, 0};
  wgpu::Fence fence = queue.CreateFence(&fence_desc);

  queue.Submit(0, nullptr);
  queue.Signal(fence, 1u);

  while (fence.GetCompletedValue() < 1) {
    device.Tick();
    webgpu()->FlushCommands();
    RunPendingTasks();
  }
}

WebGPUTest::DeviceAndClientID WebGPUTest::GetNewDeviceAndClientID() {
  DeviceAndClientID result;
  result.client_id = next_device_client_id_;

  webgpu()->RequestDeviceAsync(
      kAdapterServiceID, {},
      base::BindOnce(
          [](webgpu::DawnDeviceClientID expected_client_id, bool success,
             webgpu::DawnDeviceClientID assigned_client_id) {
            ASSERT_TRUE(success);
            ASSERT_EQ(expected_client_id, assigned_client_id);
          },
          result.client_id));

  result.device = wgpu::Device::Acquire(webgpu()->GetDevice(result.client_id));

  next_device_client_id_++;
  return result;
}

TEST_F(WebGPUTest, FlushNoCommands) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }

  Initialize(WebGPUTest::Options());

  webgpu()->FlushCommands();
}

// Referred from GLES2ImplementationTest/ReportLoss
TEST_F(WebGPUTest, ReportLoss) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }

  Initialize(WebGPUTest::Options());

  GpuControlClient* webgpu_as_client = webgpu();
  int lost_count = 0;
  webgpu()->SetLostContextCallback(base::BindOnce(&CountCallback, &lost_count));
  EXPECT_EQ(0, lost_count);

  webgpu_as_client->OnGpuControlLostContext();
  // The lost context callback should be run when WebGPUImplementation is
  // notified of the loss.
  EXPECT_EQ(1, lost_count);
}

// Referred from GLES2ImplementationTest/ReportLossReentrant
TEST_F(WebGPUTest, ReportLossReentrant) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }

  Initialize(WebGPUTest::Options());

  GpuControlClient* webgpu_as_client = webgpu();
  int lost_count = 0;
  webgpu()->SetLostContextCallback(base::BindOnce(&CountCallback, &lost_count));
  EXPECT_EQ(0, lost_count);

  webgpu_as_client->OnGpuControlLostContextMaybeReentrant();
  // The lost context callback should not be run yet to avoid calling back into
  // clients re-entrantly, and having them re-enter WebGPUImplementation.
  EXPECT_EQ(0, lost_count);
}

TEST_F(WebGPUTest, RequestAdapterAfterContextLost) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }

  Initialize(WebGPUTest::Options());

  webgpu()->OnGpuControlLostContext();
  ASSERT_FALSE(
      webgpu()->RequestAdapterAsync(webgpu::PowerPreference::kDefault,
                                    base::BindOnce(&OnRequestAdapterCallback)));
}

TEST_F(WebGPUTest, RequestDeviceAfterContextLost) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }

  Initialize(WebGPUTest::Options());

  webgpu()->OnGpuControlLostContext();
  ASSERT_FALSE(webgpu()->RequestDeviceAsync(
      kAdapterServiceID, {},
      base::BindOnce(
          [](bool success, webgpu::DawnDeviceClientID assigned_client_id) {})));
}

}  // namespace gpu
