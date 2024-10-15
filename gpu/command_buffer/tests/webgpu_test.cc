// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/tests/webgpu_test.h"

#include <dawn/dawn_proc.h>
#include <dawn/dawn_thread_dispatch_proc.h>

#include "base/command_line.h"
#include "base/test/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/client/webgpu_cmd_helper.h"
#include "gpu/command_buffer/client/webgpu_implementation.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/webgpu_decoder.h"
#include "gpu/config/gpu_test_config.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "gpu/ipc/webgpu_in_process_context.h"
#include "gpu/webgpu/callback.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {

void CountCallback(int* count) {
  (*count)++;
}

}  // anonymous namespace

#define SKIP_TEST_IF(condition) \
  if (condition)                \
  GTEST_SKIP() << #condition

WebGPUTest::Options::Options() = default;

std::map<std::pair<WGPUDevice, wgpu::ErrorType>, /* matched */ bool>
    WebGPUTest::s_expected_errors = {};

WebGPUTest::WebGPUTest() = default;
WebGPUTest::~WebGPUTest() = default;

bool WebGPUTest::WebGPUSupported() const {
  // Nexus 5X does not support WebGPU
  if (GPUTestBotConfig::CurrentConfigMatches("Android Qualcomm 0x4010800")) {
    return false;
  }

  // Pixel 2 does not support WebGPU
  if (GPUTestBotConfig::CurrentConfigMatches("Android Qualcomm 0x5040001")) {
    return false;
  }

  return true;
}

bool WebGPUTest::WebGPUSharedImageSupported() const {
  // Currently WebGPUSharedImage is only implemented on Mac, Linux, Windows
  // and ChromeOS.
#if (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
     BUILDFLAG(IS_WIN)) &&                                                 \
    BUILDFLAG(USE_DAWN)
  // TODO(crbug.com/40166640): Re-enable on AMD when the RX 5500 XT issues are
  // resolved.
  return !GPUTestBotConfig::CurrentConfigMatches("Linux AMD");
#else
  return false;
#endif
}

void WebGPUTest::SetUp() {
  SKIP_TEST_IF(!WebGPUSupported());
}

void WebGPUTest::TearDown() {
  adapter_ = nullptr;
  instance_ = nullptr;
  cmd_helper_ = nullptr;
  context_ = nullptr;
}

void WebGPUTest::Initialize(const Options& options) {
  // Some tests that inherit from WebGPUTest call Initialize in SetUp, which
  // won't be skipped even if the SKIP_TEST_IF in WebGPUTest::SetUp() is
  // triggered. As a result, to avoid potential crashes, skip initializing if
  // this device has been marked as not supporting WebGPU.
  if (!WebGPUSupported()) {
    return;
  }

  gpu::GpuPreferences gpu_preferences;
  gpu_preferences.enable_webgpu = true;
  gpu_preferences.use_passthrough_cmd_decoder =
      gles2::UsePassthroughCommandDecoder(
          base::CommandLine::ForCurrentProcess());
  if (options.use_skia_graphite) {
    gpu_preferences.gr_context_type = gpu::GrContextType::kGraphiteDawn;
  } else {
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && BUILDFLAG(USE_DAWN)
    gpu_preferences.use_vulkan = gpu::VulkanImplementationName::kNative;
    gpu_preferences.gr_context_type = gpu::GrContextType::kVulkan;
#endif
  }
  gpu_preferences.enable_unsafe_webgpu = options.enable_unsafe_webgpu;
  if (!options.adapter_blocklist) {
    gpu_preferences.disabled_dawn_features_list = {"adapter_blocklist"};
  }

  gpu_service_holder_ =
      std::make_unique<viz::TestGpuServiceHolder>(gpu_preferences);

  ContextCreationAttribs attributes;
  attributes.bind_generates_resource = false;
  attributes.enable_gles2_interface = false;
  attributes.context_type = CONTEXT_TYPE_WEBGPU;

  context_ = std::make_unique<WebGPUInProcessContext>();
  ContextResult result =
      context_->Initialize(gpu_service_holder_->task_executor(), attributes,
                           options.shared_memory_limits);
  ASSERT_EQ(result, ContextResult::kSuccess) << "Context failed to initialize";

  cmd_helper_ = std::make_unique<webgpu::WebGPUCmdHelper>(
      context_->GetCommandBufferForTest());

  webgpu_impl()->SetLostContextCallback(base::BindLambdaForTesting(
      []() { GTEST_FAIL() << "Context lost unexpectedly."; }));

  // Use the wire procs for the test main thread.
  dawnProcSetPerThreadProcs(&dawn::wire::client::GetProcs());

  instance_ = wgpu::Instance(webgpu()->GetAPIChannel()->GetWGPUInstance());

  wgpu::RequestAdapterOptions ra_options = {};
  ra_options.forceFallbackAdapter = options.force_fallback_adapter;
  ra_options.compatibilityMode = options.compatibility_mode;

  bool done = false;
  instance_.RequestAdapter(&ra_options, wgpu::CallbackMode::AllowSpontaneous,
                           [&](wgpu::RequestAdapterStatus status,
                               wgpu::Adapter adapter, const char* message) {
                             if (!options.force_fallback_adapter) {
                               // If we don't force a particular adapter, we
                               // should always find one.
                               EXPECT_EQ(status,
                                         wgpu::RequestAdapterStatus::Success);
                               EXPECT_NE(adapter, nullptr);
                             }
                             this->adapter_ = std::move(adapter);
                             done = true;
                           });
  webgpu()->FlushCommands();
  while (!done) {
    RunPendingTasks();
  }
}

webgpu::WebGPUInterface* WebGPUTest::webgpu() const {
  return context_->GetImplementation();
}

webgpu::WebGPUImplementation* WebGPUTest::webgpu_impl() const {
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
  gpu_service_holder_->ScheduleGpuMainTask(base::BindOnce(
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
  wgpu::FutureWaitInfo wait_info = {device.GetQueue().OnSubmittedWorkDone(
      wgpu::CallbackMode::WaitAnyOnly, [](wgpu::QueueWorkDoneStatus) {})};

  while (!wait_info.completed) {
    instance_.WaitAny(1, &wait_info, 0);
    webgpu()->FlushCommands();
    RunPendingTasks();
  }
}

void WebGPUTest::PollUntilIdle() {
  if (!context_ || !gpu_service_holder_) {
    // Never initialized. Test skipped or failed in setup.
    return;
  }
  webgpu()->FlushCommands();
  base::WaitableEvent wait;
  gpu_service_holder_->ScheduleGpuMainTask(
      base::BindLambdaForTesting([&wait, decoder = GetDecoder()]() {
        while (decoder->HasPollingWork()) {
          base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
          decoder->PerformPollingWork();
        }
        wait.Signal();
      }));
  wait.Wait();
  context_->GetTaskRunner()->RunPendingTasks();
}

wgpu::Device WebGPUTest::GetNewDevice() {
  wgpu::Device device;
  bool done = false;

  DCHECK(adapter_);
  wgpu::DeviceDescriptor device_desc = {};
  device_desc.SetDeviceLostCallback(
      wgpu::CallbackMode::AllowSpontaneous,
      [](const wgpu::Device&, wgpu::DeviceLostReason reason,
         const char* message) {
        if (reason == wgpu::DeviceLostReason::Destroyed) {
          return;
        }
        GTEST_FAIL() << "Unexpected device lost (" << reason
                     << "): " << message;
      });
  device_desc.SetUncapturedErrorCallback([](const wgpu::Device& device,
                                            wgpu::ErrorType type,
                                            const char* message) {
    auto it = s_expected_errors.find(std::make_pair(device.Get(), type));
    if (it != s_expected_errors.end() && !it->second) {
      it->second = true;
      return;
    }
    GTEST_FAIL() << "Unexpected error (" << type << "): " << message;
  });

  adapter_.RequestDevice(
      &device_desc, wgpu::CallbackMode::AllowSpontaneous,
      [&](wgpu::RequestDeviceStatus status, wgpu::Device created_device,
          const char* message) {
        // Fail the test with error message if returned status is not success
        if (status != wgpu::RequestDeviceStatus::Success) {
          if (message) {
            GTEST_FAIL() << "RequestDevice returns unexpected message: "
                         << message;
          } else {
            GTEST_FAIL()
                << "RequestDevice returns unexpected status without message.";
          }
        }
        device = std::move(created_device);
        done = true;
      });
  webgpu()->FlushCommands();
  while (!done) {
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
    RunPendingTasks();
  }

  EXPECT_NE(device, nullptr);
  return device;
}

TEST_F(WebGPUTest, FlushNoCommands) {
  Initialize(WebGPUTest::Options());

  webgpu()->FlushCommands();
}

// Referred from GLES2ImplementationTest/ReportLoss
TEST_F(WebGPUTest, ReportLoss) {
  Initialize(WebGPUTest::Options());

  GpuControlClient* webgpu_as_client = webgpu_impl();
  int lost_count = 0;
  webgpu_impl()->SetLostContextCallback(
      base::BindOnce(&CountCallback, &lost_count));
  EXPECT_EQ(0, lost_count);

  webgpu_as_client->OnGpuControlLostContext();
  // The lost context callback should be run when WebGPUImplementation is
  // notified of the loss.
  EXPECT_EQ(1, lost_count);
}

// Referred from GLES2ImplementationTest/ReportLossReentrant
TEST_F(WebGPUTest, ReportLossReentrant) {
  Initialize(WebGPUTest::Options());

  GpuControlClient* webgpu_as_client = webgpu_impl();
  int lost_count = 0;
  webgpu_impl()->SetLostContextCallback(
      base::BindOnce(&CountCallback, &lost_count));
  EXPECT_EQ(0, lost_count);

  webgpu_as_client->OnGpuControlLostContextMaybeReentrant();
  // The lost context callback should not be run yet to avoid calling back into
  // clients re-entrantly, and having them re-enter WebGPUImplementation.
  EXPECT_EQ(0, lost_count);
}

TEST_F(WebGPUTest, RequestAdapterAfterContextLost) {
  Initialize(WebGPUTest::Options());

  webgpu_impl()->SetLostContextCallback(base::DoNothing());
  webgpu_impl()->OnGpuControlLostContext();

  bool called = false;
  wgpu::RequestAdapterOptions ra_options = {};
  instance_.RequestAdapter(&ra_options, wgpu::CallbackMode::AllowSpontaneous,
                           [&](wgpu::RequestAdapterStatus status,
                               wgpu::Adapter adapter, const char* message) {
                             EXPECT_EQ(adapter, nullptr);
                             called = true;
                           });
  webgpu()->FlushCommands();
  RunPendingTasks();
  EXPECT_TRUE(called);
}

TEST_F(WebGPUTest, RequestDeviceAfterContextLost) {
  Initialize(WebGPUTest::Options());

  webgpu_impl()->SetLostContextCallback(base::DoNothing());
  webgpu_impl()->OnGpuControlLostContext();

  bool called = false;

  DCHECK(adapter_);
  wgpu::DeviceDescriptor device_desc = {};
  adapter_.RequestDevice(&device_desc, wgpu::CallbackMode::AllowSpontaneous,
                         [&](wgpu::RequestDeviceStatus status,
                             wgpu::Device device, const char* message) {
                           EXPECT_EQ(device, nullptr);
                           called = true;
                         });
  webgpu()->FlushCommands();
  RunPendingTasks();
  EXPECT_TRUE(called);
}

TEST_F(WebGPUTest, RequestDeviceWithUnsupportedFeature) {
  Initialize(WebGPUTest::Options());

  // Create device with unsupported features, expect to fail to create and
  // return nullptr
  wgpu::FeatureName invalid_feature = static_cast<wgpu::FeatureName>(-2);

  wgpu::Device device;
  bool done = false;

  DCHECK(adapter_);
  wgpu::DeviceDescriptor device_desc = {};
  device_desc.requiredFeatureCount = 1;
  device_desc.requiredFeatures = &invalid_feature;

  adapter_.RequestDevice(&device_desc, wgpu::CallbackMode::AllowSpontaneous,
                         [&](wgpu::RequestDeviceStatus status,
                             wgpu::Device created_device, const char* message) {
                           device = std::move(created_device);
                           done = true;
                         });
  webgpu()->FlushCommands();

  while (!done) {
    RunPendingTasks();
  }
  EXPECT_EQ(device, nullptr);

  // Create device again with supported features, expect success and not
  // blocked by the last failure
  GetNewDevice();
}

TEST_F(WebGPUTest, SPIRVIsDisallowed) {
  auto options = WebGPUTest::Options();
  options.enable_unsafe_webgpu = false;
  Initialize(options);

  wgpu::Device device = GetNewDevice();

  // Make a invalid ShaderModuleDescriptor because it contains SPIR-V.
  wgpu::ShaderSourceSPIRV spirvDesc;
  spirvDesc.codeSize = 0;
  spirvDesc.code = nullptr;

  wgpu::ShaderModuleDescriptor desc;
  desc.nextInChain = &spirvDesc;

  // Make sure creation fails, and for the correct reason.
  device.PushErrorScope(wgpu::ErrorFilter::Validation);
  device.CreateShaderModule(&desc);
  bool got_error = false;
  device.PopErrorScope(wgpu::CallbackMode::AllowSpontaneous,
                       [&](wgpu::PopErrorScopeStatus status,
                           wgpu::ErrorType type, const char* message) {
                         // We match on this string to make sure the shader
                         // module creation fails because SPIR-V is disallowed
                         // and not because codeSize=0.
                         EXPECT_THAT(message, testing::HasSubstr("SPIR"));
                         EXPECT_EQ(type, wgpu::ErrorType::Validation);
                         got_error = true;
                       });

  WaitForCompletion(device);
  EXPECT_TRUE(got_error);
}

TEST_F(WebGPUTest, ExplicitFallbackAdapterIsDisallowed) {
  auto options = WebGPUTest::Options();
  options.force_fallback_adapter = true;
  options.enable_unsafe_webgpu = false;
  options.adapter_blocklist = true;
  // Initialize attempts to create an adapter.
  Initialize(options);

  // No fallback adapter should be available.
  EXPECT_EQ(adapter_, nullptr);
}

TEST_F(WebGPUTest, ImplicitFallbackAdapterIsDisallowed) {
  auto options = WebGPUTest::Options();
  options.enable_unsafe_webgpu = false;
  // Initialize attempts to create an adapter.
  Initialize(options);

  if (adapter_) {
    wgpu::AdapterInfo info;
    adapter_.GetInfo(&info);
    // If we got an Adapter, it must not be a CPU adapter.
    EXPECT_NE(info.adapterType, wgpu::AdapterType::CPU);
  }
}

TEST_F(WebGPUTest, CompatibilityMode) {
  auto options = WebGPUTest::Options();
  options.compatibility_mode = true;
  options.enable_unsafe_webgpu = true;
  // Initialize attempts to create an adapter.
  Initialize(options);

  // Compatibility adapter should be available.
  EXPECT_NE(adapter_, nullptr);

  wgpu::AdapterInfo info;
  adapter_.GetInfo(&info);

  EXPECT_TRUE(info.compatibilityMode);
}

TEST_F(WebGPUTest, NonCompatibilityMode) {
  auto options = WebGPUTest::Options();
  options.compatibility_mode = false;
  options.enable_unsafe_webgpu = true;
  // Initialize attempts to create an adapter.
  Initialize(options);

  // Non-compatibility adapter should be available.
  EXPECT_NE(adapter_, nullptr);

  wgpu::AdapterInfo info;
  adapter_.GetInfo(&info);

  EXPECT_FALSE(info.compatibilityMode);
}

}  // namespace gpu
