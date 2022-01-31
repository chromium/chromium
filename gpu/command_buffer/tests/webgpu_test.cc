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
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/webgpu_decoder.h"
#include "gpu/config/gpu_test_config.h"
#include "gpu/ipc/host/gpu_memory_buffer_support.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "gpu/ipc/webgpu_in_process_context.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "gpu/command_buffer/tests/gl_manager.h"
#include "ui/gl/gl_context.h"
#endif

namespace gpu {

namespace {

void CountCallback(int* count) {
  (*count)++;
}

}  // anonymous namespace

WebGPUTest::Options::Options() = default;

WebGPUTest::WebGPUTest() = default;
WebGPUTest::~WebGPUTest() = default;

bool WebGPUTest::WebGPUSupported() const {
  // TODO(crbug.com/1172447): Re-enable on AMD when the RX 5500 XT issues are
  // resolved.
  // Win7 does not support WebGPU
  if (GPUTestBotConfig::CurrentConfigMatches("Linux AMD") ||
      GPUTestBotConfig::CurrentConfigMatches("Win7")) {
    return false;
  }

  return true;
}

bool WebGPUTest::WebGPUSharedImageSupported() const {
  // Currently WebGPUSharedImage is only implemented on Mac, Linux and Windows
#if (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
     BUILDFLAG(IS_WIN)) &&                                                 \
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
}

void WebGPUTest::TearDown() {
  context_.reset();
}

void WebGPUTest::Initialize(const Options& options) {
  if (!WebGPUSupported()) {
    return;
  }

  gpu::GpuPreferences gpu_preferences;
  gpu_preferences.enable_webgpu = true;
  gpu_preferences.use_passthrough_cmd_decoder =
      gles2::UsePassthroughCommandDecoder(
          base::CommandLine::ForCurrentProcess());
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && BUILDFLAG(USE_DAWN)
  gpu_preferences.use_vulkan = gpu::VulkanImplementationName::kNative;
  gpu_preferences.gr_context_type = gpu::GrContextType::kVulkan;
#endif
  gpu_preferences.enable_unsafe_webgpu = options.enable_unsafe_webgpu;
  gpu_preferences.texture_target_exception_list =
      gpu::CreateBufferUsageAndFormatExceptionList();

  gpu_service_holder_ =
      std::make_unique<viz::TestGpuServiceHolder>(gpu_preferences);

  ContextCreationAttribs attributes;
  attributes.bind_generates_resource = false;
  attributes.enable_gles2_interface = false;
  attributes.context_type = CONTEXT_TYPE_WEBGPU;

  static constexpr GpuMemoryBufferManager* memory_buffer_manager = nullptr;
#if BUILDFLAG(IS_MAC)
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

  bool done = false;
  webgpu()->RequestAdapterAsync(
      webgpu::PowerPreference::kDefault, options.force_fallback_adapter,
      base::BindOnce(
          [](WebGPUTest* test, bool force_fallback_adapter, bool* done,
             int32_t adapter_id, const WGPUDeviceProperties& properties,
             const char*) {
            if (!force_fallback_adapter) {
              // If we don't force a particular adapter, we should always find
              // one.
              EXPECT_GE(adapter_id, 0);
            }
            test->adapter_id_ = adapter_id;
            test->device_properties_ = properties;
            *done = true;
          },
          this, options.force_fallback_adapter, &done));

  while (!done) {
    RunPendingTasks();
  }

  DawnProcTable procs = webgpu()->GetAPIChannel()->GetProcs();
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
  gpu_service_holder_->ScheduleGpuTask(base::BindOnce(
      [](webgpu::WebGPUDecoder* decoder) {
        if (decoder->HasPollingWork()) {
          decoder->PerformPollingWork();
        }
      },
      GetDecoder()));
}

void WebGPUTest::WaitForCompletion(wgpu::Device device) {
  // Wait for any work submitted to the queue to be finished. The guarantees of
  // Dawn are that all previous operations will have been completed and more
  // importantly the callbacks will have been called.
  wgpu::Queue queue = device.GetQueue();
  bool done = false;
  queue.OnSubmittedWorkDone(
      0u,
      [](WGPUQueueWorkDoneStatus, void* userdata) {
        *static_cast<bool*>(userdata) = true;
      },
      &done);

  while (!done) {
    device.Tick();
    webgpu()->FlushCommands();
    RunPendingTasks();
  }
}

wgpu::Device WebGPUTest::GetNewDevice() {
  WGPUDevice device = nullptr;

  bool done = false;
  DCHECK(adapter_id_ >= 0);
  webgpu()->RequestDeviceAsync(
      adapter_id_, device_properties_,
      base::BindOnce(
          [](WGPUDevice* result, bool* done, WGPUDevice device,
             const WGPUSupportedLimits*, const char*) {
            *result = device;
            *done = true;
          },
          &device, &done));
  while (!done) {
    RunPendingTasks();
  }

  EXPECT_NE(device, nullptr);
  return wgpu::Device::Acquire(device);
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

  bool called = false;
  webgpu()->RequestAdapterAsync(
      webgpu::PowerPreference::kDefault, false,
      base::BindOnce(
          [](bool* called, int32_t adapter_id, const WGPUDeviceProperties&,
             const char*) {
            EXPECT_EQ(adapter_id, -1);
            *called = true;
          },
          &called));
  RunPendingTasks();
  EXPECT_TRUE(called);
}

TEST_F(WebGPUTest, RequestDeviceAfterContextLost) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }

  Initialize(WebGPUTest::Options());

  webgpu()->OnGpuControlLostContext();

  bool called = false;
  webgpu()->RequestDeviceAsync(GetAdapterId(), GetDeviceProperties(),
                               base::BindOnce(
                                   [](bool* called, WGPUDevice device,
                                      const WGPUSupportedLimits*, const char*) {
                                     EXPECT_EQ(device, nullptr);
                                     *called = true;
                                   },
                                   &called));
  RunPendingTasks();
  EXPECT_TRUE(called);
}

TEST_F(WebGPUTest, RequestDeviceWitUnsupportedFeature) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }

#if BUILDFLAG(IS_MAC)
  // Crashing on Mac M1. Currently missing stack trace. crbug.com/1271926
  // This must be checked before WebGPUTest::Initialize otherwise context
  // switched is locked and we cannot temporarily have this GLContext.
  GLManager gl_manager;
  gl_manager.Initialize(GLManager::Options());
  std::string renderer(gl_manager.context()->GetGLRenderer());
  if (renderer.find("Apple M1") != std::string::npos) {
    gl_manager.Destroy();
    return;
  }
  gl_manager.Destroy();
#endif

  Initialize(WebGPUTest::Options());

  // Create device with unsupported features, expect to fail to create and
  // return nullptr
  WGPUDeviceProperties unsupported_device_properties = {};
  unsupported_device_properties.invalidFeature = true;
  WGPUDevice device = nullptr;
  bool done = false;

  webgpu()->RequestDeviceAsync(
      GetAdapterId(), unsupported_device_properties,
      base::BindOnce(
          [](WGPUDevice* result, bool* done, WGPUDevice device,
             const WGPUSupportedLimits*, const char*) {
            *result = device;
            *done = true;
          },
          &device, &done));

  while (!done) {
    RunPendingTasks();
  }
  EXPECT_EQ(device, nullptr);

  // Create device again with supported features, expect success and not
  // blocked by the last failure
  GetNewDevice();
}

TEST_F(WebGPUTest, SPIRVIsDisallowed) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }

  auto ExpectSPIRVDisallowedError = [](WGPUErrorType type, const char* message,
                                       void* userdata) {
    // We match on this string to make sure the shader module creation fails
    // because SPIR-V is disallowed and not because codeSize=0.
    EXPECT_NE(std::string(message).find("SPIR-V is disallowed"),
              std::string::npos);
    EXPECT_EQ(type, WGPUErrorType_Validation);
    *static_cast<bool*>(userdata) = true;
  };

  auto options = WebGPUTest::Options();
  options.enable_unsafe_webgpu = false;
  Initialize(options);
  wgpu::Device device = GetNewDevice();

  // Make a invalid ShaderModuleDescriptor because it contains SPIR-V.
  wgpu::ShaderModuleSPIRVDescriptor spirvDesc;
  spirvDesc.codeSize = 0;
  spirvDesc.code = nullptr;

  wgpu::ShaderModuleDescriptor desc;
  desc.nextInChain = &spirvDesc;

  // Make sure creation fails, and for the correct reason.
  device.PushErrorScope(wgpu::ErrorFilter::Validation);
  device.CreateShaderModule(&desc);
  bool got_error = false;
  device.PopErrorScope(ExpectSPIRVDisallowedError, &got_error);

  WaitForCompletion(device);
  EXPECT_TRUE(got_error);
}

TEST_F(WebGPUTest, ExplicitFallbackAdapterIsDisallowed) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }

  auto options = WebGPUTest::Options();
  options.force_fallback_adapter = true;
  options.enable_unsafe_webgpu = false;
  // Initialize attempts to create an adapter.
  Initialize(options);

  // The id should be -1 since no fallback adapter is available.
  EXPECT_EQ(GetAdapterId(), -1);
}

TEST_F(WebGPUTest, ImplicitFallbackAdapterIsDisallowed) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }

  auto options = WebGPUTest::Options();
  options.enable_unsafe_webgpu = false;
  // Initialize attempts to create an adapter.
  Initialize(options);

  if (GetAdapterId() != -1) {
    // If we got an Adapter, it must not be a CPU adapter.
    EXPECT_NE(GetDeviceProperties().adapterType, WGPUAdapterType_CPU);
  }
}

}  // namespace gpu
