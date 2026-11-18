// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_canvas_context.h"

#include <dawn/dawn_proc.h>
#include <dawn/wire/WireClient.h>
#include <dawn/wire/WireServer.h>

#include <memory>
#include <utility>

#include "components/viz/test/test_context_provider.h"
#include "gpu/command_buffer/client/webgpu_interface_stub.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_canvas_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_device_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_request_adapter_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_format.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_performance_monitor.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webgpu/gpu.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_command_serializers.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer_test_helpers.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_native_test_support.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_compositing_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

namespace {

class GPUCanvasContextTest : public testing::Test {
 protected:
  void SetUp() override {
    test_context_provider_ = viz::TestContextProvider::CreateRaster();
    InitializeSharedGpuContext(test_context_provider_.get());

    c2s_serializer_.SetHandler(&wire_server_);
    s2c_serializer_.SetHandler(&wire_client_);
#if !BUILDFLAG(USE_DAWN)
    dawnProcSetProcs(&dawn::wire::client::GetProcs());
#endif

    wgpu::InstanceDescriptor instance_desc = {};
    auto reservation = wire_client_.ReserveInstance(
        reinterpret_cast<WGPUInstanceDescriptor*>(&instance_desc));

    WGPUInstance native_instance = MakeNativeWGPUInstance();
    wire_server_.InjectInstance(native_instance, reservation.handle);
    GetDawnNativeProcs().instanceRelease(native_instance);

    instance_ = wgpu::Instance::Acquire(reservation.instance);

    wgpu::RequestAdapterOptions options = {
        .backendType = wgpu::BackendType::Null,
    };
    instance_.RequestAdapter(
        &options, wgpu::CallbackMode::AllowSpontaneous,
        [&](wgpu::RequestAdapterStatus, wgpu::Adapter adapter,
            wgpu::StringView) { adapter_handle_ = std::move(adapter); });
    ASSERT_TRUE(c2s_serializer_.Flush());
    ASSERT_TRUE(s2c_serializer_.Flush());
    ASSERT_NE(adapter_handle_, nullptr);

    wgpu::DeviceDescriptor device_desc = {};
    adapter_handle_.RequestDevice(
        &device_desc, wgpu::CallbackMode::AllowSpontaneous,
        [&](wgpu::RequestDeviceStatus, wgpu::Device device, wgpu::StringView) {
          device_handle_ = std::move(device);
        });
    ASSERT_TRUE(c2s_serializer_.Flush());
    ASSERT_TRUE(s2c_serializer_.Flush());
    ASSERT_NE(device_handle_, nullptr);

    page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
    page_holder_->GetDocument()
        .documentElement()
        ->SetInnerHTMLWithoutTrustedTypes(
            "<body><canvas id='c' width='10' height='10'></canvas></body>");
    canvas_ = To<HTMLCanvasElement>(
        page_holder_->GetDocument().getElementById(AtomicString("c")));
    ASSERT_NE(canvas_, nullptr);
  }

  void TearDown() override {
    CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();
    canvas_ = nullptr;
    page_holder_.reset();
    device_handle_ = nullptr;
    adapter_handle_ = nullptr;
    instance_ = nullptr;
    c2s_serializer_.SetHandler(nullptr);
    s2c_serializer_.SetHandler(nullptr);
    SharedGpuContext::Reset();
  }

  std::tuple<GPUCanvasContext*,
             GPUDevice*,
             scoped_refptr<DawnControlClientHolder>>
  CreateConfiguredContext(V8TestingScope& v8_test_scope) {
    ExecutionContext* execution_context =
        ExecutionContext::From(v8_test_scope.GetScriptState());
    Navigator* navigator = page_holder_->GetFrame().DomWindow()->navigator();
    auto* gpu = MakeGarbageCollected<GPU>(*navigator);

    auto provider = std::make_unique<WebGraphicsContext3DProviderForTests>(
        std::make_unique<gpu::webgpu::WebGPUInterfaceStub>());
    auto dawn_control_client = DawnControlClientHolder::Create(
        std::move(provider),
        execution_context->GetTaskRunner(TaskType::kWebGPU));
    gpu->SetDawnControlClientHolderForTesting(dawn_control_client);

    auto* adapter = MakeGarbageCollected<GPUAdapter>(
        gpu, adapter_handle_, dawn_control_client,
        GPURequestAdapterOptions::Create());
    auto* device = MakeGarbageCollected<GPUDevice>(
        execution_context, dawn_control_client, adapter, String());
    device->Initialize(device_handle_, GPUDeviceDescriptor::Create(),
                       /*lost_info=*/nullptr);

    CanvasContextCreationAttributesCore attrs;
    auto* context = static_cast<GPUCanvasContext*>(
        canvas_->GetCanvasRenderingContext(execution_context, "webgpu", attrs));
    EXPECT_NE(context, nullptr);

    auto* config = GPUCanvasConfiguration::Create();
    config->setDevice(device);
    config->setFormat(V8GPUTextureFormat::Enum::kRgba8Unorm);
    DummyExceptionStateForTesting exception_state;
    context->configure(config, exception_state);
    EXPECT_FALSE(exception_state.HadException());

    return {context, device, dawn_control_client};
  }

  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<GpuCompositingTestPlatform> platform_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;

  DawnTestingCommandSerializer c2s_serializer_;
  DawnTestingCommandSerializer s2c_serializer_;
  dawn::wire::WireClient wire_client_{{.serializer = &c2s_serializer_}};
  dawn::wire::WireServer wire_server_{{.procs = &GetDawnNativeProcs(),
                                       .serializer = &s2c_serializer_,
                                       .useSpontaneousCallbacks = true}};
  wgpu::Instance instance_;
  wgpu::Adapter adapter_handle_;
  wgpu::Device device_handle_;

  std::unique_ptr<DummyPageHolder> page_holder_;
  Persistent<HTMLCanvasElement> canvas_;
};

TEST_F(GPUCanvasContextTest, PaintRenderingResultsToSnapshotErrorCases) {
  V8TestingScope v8_test_scope;
  auto [context, device, dawn_control_client] =
      CreateConfiguredContext(v8_test_scope);

  // Before getCurrentTexture() is called, there is no texture to copy from.
  // Currently, PaintRenderingResultsToSnapshot() returns a snapshot of the
  // uncleared resource provider (except on Linux for kFrontBuffer).
  EXPECT_NE(context->PaintRenderingResultsToSnapshot(kBackBuffer), nullptr);
#if BUILDFLAG(IS_LINUX)
  EXPECT_EQ(context->PaintRenderingResultsToSnapshot(kFrontBuffer), nullptr);
#else
  EXPECT_NE(context->PaintRenderingResultsToSnapshot(kFrontBuffer), nullptr);
#endif

  // Simulate WebGPU context destruction so that getCurrentTexture() produces
  // an error texture and CopyTextureToResourceProvider() fails.
  dawn_control_client->Destroy();
  DummyExceptionStateForTesting exception_state;
  ASSERT_NE(context->getCurrentTexture(v8_test_scope.GetScriptState(),
                                       exception_state),
            nullptr);
  ASSERT_FALSE(exception_state.HadException());

  // Currently, when CopyTextureToResourceProvider() fails,
  // PaintRenderingResultsToSnapshot() still returns a snapshot of the resource
  // provider.
  EXPECT_NE(context->PaintRenderingResultsToSnapshot(kBackBuffer), nullptr);

  // When the GPUDevice is destroyed while the context is configured,
  // PaintRenderingResultsToSnapshot() clears the resource provider and returns
  // a valid fallback snapshot.
  device->destroy(v8_test_scope.GetIsolate());
  EXPECT_NE(context->PaintRenderingResultsToSnapshot(kBackBuffer), nullptr);

  // Currently, once the context is unconfigured,
  // PaintRenderingResultsToSnapshot() still returns a snapshot from the cached
  // resource provider.
  context->unconfigure();
  EXPECT_NE(context->PaintRenderingResultsToSnapshot(kBackBuffer), nullptr);
}

}  // namespace

}  // namespace blink
