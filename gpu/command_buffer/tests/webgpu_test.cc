// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/tests/webgpu_test.h"

#include <dawn/dawn_proc.h>
#include <dawn/webgpu.h>

#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/client/webgpu_implementation.h"
#include "gpu/command_buffer/service/webgpu_decoder.h"
#include "gpu/config/gpu_test_config.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "gpu/ipc/webgpu_in_process_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

WebGPUTest::Options::Options() = default;

WebGPUTest::WebGPUTest() = default;
WebGPUTest::~WebGPUTest() = default;

bool WebGPUTest::WebGPUSupported() const {
  DCHECK(is_initialized_);  // Did you call WebGPUTest::Initialize?

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
#if (defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_WIN)) && \
    BUILDFLAG(USE_DAWN)
  return true;
#else
  return false;
#endif
}

void WebGPUTest::SetUp() {
  gpu::GpuPreferences gpu_preferences;
  gpu_preferences.enable_webgpu = true;
#if defined(OS_LINUX) && BUILDFLAG(USE_DAWN)
  gpu_preferences.use_vulkan = gpu::VulkanImplementationName::kNative;
  gpu_preferences.gr_context_type = gpu::GrContextType::kVulkan;
#endif
  gpu_service_holder_ =
      std::make_unique<viz::TestGpuServiceHolder>(gpu_preferences);
}

void WebGPUTest::TearDown() {
  context_.reset();
}

void WebGPUTest::Initialize(const Options& options) {
  is_initialized_ = true;

  if (!WebGPUSupported()) {
    return;
  }

  ContextCreationAttribs attributes;
  attributes.bind_generates_resource = false;
  attributes.enable_gles2_interface = false;
  attributes.context_type = CONTEXT_TYPE_WEBGPU;

  static constexpr GpuMemoryBufferManager* memory_buffer_manager = nullptr;
  static constexpr ImageFactory* image_factory = nullptr;
  static constexpr GpuChannelManagerDelegate* channel_manager = nullptr;
  context_ = std::make_unique<WebGPUInProcessContext>();
  ContextResult result =
      context_->Initialize(gpu_service_holder_->task_executor(), attributes,
                           options.shared_memory_limits, memory_buffer_manager,
                           image_factory, channel_manager);
  ASSERT_EQ(result, ContextResult::kSuccess);

  webgpu()->RequestAdapter(webgpu::PowerPreference::kHighPerformance);

  DawnProcTable procs = webgpu()->GetProcs();
  dawnProcSetProcs(&procs);
}

webgpu::WebGPUInterface* WebGPUTest::webgpu() const {
  return context_->GetImplementation();
}

SharedImageInterface* WebGPUTest::GetSharedImageInterface() const {
  return context_->GetCommandBufferForTest()->GetSharedImageInterface();
}

void WebGPUTest::RunPendingTasks() {
  context_->GetTaskRunner()->RunPendingTasks();
}

void WebGPUTest::WaitForCompletion(wgpu::Device device) {
  // Insert a fence signal and wait for it to be signaled. The guarantees of
  // Dawn are that all previous operations will have been completed and more
  // importantly the callbacks will have been called.
  wgpu::Queue queue = device.CreateQueue();
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

TEST_F(WebGPUTest, FlushNoCommands) {
  Initialize(WebGPUTest::Options());

  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }

  webgpu()->FlushCommands();
}

}  // namespace gpu
