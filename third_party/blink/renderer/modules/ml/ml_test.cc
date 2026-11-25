// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml.h"

#include <dawn/dawn_proc_table.h>
#include <dawn/native/DawnNative.h>
#include <dawn/wire/WireClient.h>
#include <dawn/wire/WireServer.h>

#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "gpu/command_buffer/client/webgpu_interface_stub.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_command_serializers.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

DawnProcTable MakeNullBackendServerProcs() {
  DawnProcTable procs = dawn::native::GetProcs();
  procs.instanceRequestAdapter =
      [](WGPUInstance instance, const WGPURequestAdapterOptions*,
         WGPURequestAdapterCallbackInfo callback_info) {
        WGPURequestAdapterOptions null_options =
            WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
        null_options.backendType = WGPUBackendType_Null;
        return dawn::native::GetProcs().instanceRequestAdapter(
            instance, &null_options, callback_info);
      };
  return procs;
}

// Mirrors `gpu::webgpu::DawnWireServices`: owns a real `dawn::wire::WireClient`
// and reserved `WGPUInstance` whose lifetime is tied to the channel.
class TestAPIChannel : public gpu::webgpu::APIChannel {
 public:
  explicit TestAPIChannel(bool return_null_instance = false)
      : return_null_instance_(return_null_instance) {
    WGPUInstanceDescriptor instance_desc = WGPU_INSTANCE_DESCRIPTOR_INIT;
    reserved_instance_ = wire_client_.ReserveInstance(&instance_desc);
  }

  WGPUInstance GetWGPUInstance() const override {
    return return_null_instance_ ? nullptr : reserved_instance_.instance;
  }

  void Disconnect() override {
    c2s_serializer_.SetHandler(nullptr);
    wire_client_.Disconnect();
  }

  void ProcessEvents() {
    dawn::wire::client::GetProcs().instanceProcessEvents(
        reserved_instance_.instance);
  }

  DawnTestingCommandSerializer& c2s_serializer() { return c2s_serializer_; }
  dawn::wire::WireClient& wire_client() { return wire_client_; }
  const dawn::wire::ReservedInstance& reserved_instance() const {
    return reserved_instance_;
  }

 private:
  ~TestAPIChannel() override {
    if (reserved_instance_.instance) {
      dawn::wire::client::GetProcs().instanceRelease(
          reserved_instance_.instance);
    }
  }

  bool return_null_instance_ = false;
  DawnTestingCommandSerializer c2s_serializer_;
  dawn::wire::WireClient wire_client_{{.serializer = &c2s_serializer_}};
  dawn::wire::ReservedInstance reserved_instance_;
};

class TestWebGPUInterface : public gpu::webgpu::WebGPUInterfaceStub {
 public:
  explicit TestWebGPUInterface(scoped_refptr<TestAPIChannel> api_channel,
                               int* flush_count = nullptr)
      : api_channel_(std::move(api_channel)), flush_count_(flush_count) {}

  ~TestWebGPUInterface() override { flush_count_ = nullptr; }

  scoped_refptr<gpu::webgpu::APIChannel> GetAPIChannel() const override {
    return api_channel_;
  }

  void FlushCommands() override {
    if (flush_count_) {
      ++(*flush_count_);
    }
    std::ignore = api_channel_->c2s_serializer().Flush();
  }

  TestAPIChannel* test_api_channel() const { return api_channel_.get(); }

 private:
  scoped_refptr<TestAPIChannel> api_channel_;
  raw_ptr<int> flush_count_ = nullptr;
};

class TestWebGPUContextProvider : public WebGraphicsContext3DProviderForTests {
 public:
  TestWebGPUContextProvider(
      std::unique_ptr<TestWebGPUInterface> webgpu_interface,
      base::OnceClosure destruction_callback)
      : WebGraphicsContext3DProviderForTests(std::move(webgpu_interface)),
        destruction_callback_(std::move(destruction_callback)) {
    api_channel_ = static_cast<TestWebGPUInterface*>(WebGPUInterface())
                       ->test_api_channel();
  }

  ~TestWebGPUContextProvider() override {
    api_channel_ = nullptr;
    if (destruction_callback_) {
      std::move(destruction_callback_).Run();
    }
  }

  void SetLostContextCallback(
      base::RepeatingClosure lost_context_callback) override {
    lost_context_callback_ = std::move(lost_context_callback);
  }

  // Mirrors `WebGPUImplementation::OnGpuControlLostContext()`: disconnects the
  // Dawn wire client first (synchronously completing pending `EventManager`
  // callbacks with `EventCompletionType::Shutdown`), then runs the lost-context
  // callback while `this` is still active on the call stack.
  void SimulateGpuDisconnect() {
    api_channel_->Disconnect();
    if (lost_context_callback_) {
      std::move(lost_context_callback_).Run();
    }
    disconnected_ = true;
  }

  bool disconnected() const { return disconnected_; }
  TestAPIChannel* test_api_channel() const { return api_channel_; }

 private:
  raw_ptr<TestAPIChannel> api_channel_ = nullptr;
  base::OnceClosure destruction_callback_;
  base::RepeatingClosure lost_context_callback_;
  bool disconnected_ = false;
};

class MLWebGpuContextTest : public testing::Test {
 protected:
  void TearDown() override { s2c_serializer_.SetHandler(nullptr); }

  void ConnectWireServer(TestAPIChannel& channel) {
    channel.c2s_serializer().SetHandler(&wire_server_);
    s2c_serializer_.SetHandler(&channel.wire_client());
    ASSERT_TRUE(wire_server_.InjectInstance(
        native_instance_.Get(), channel.reserved_instance().handle));
  }

  void DisconnectWireServer(TestAPIChannel& channel) {
    channel.c2s_serializer().SetHandler(nullptr);
    s2c_serializer_.SetHandler(nullptr);
  }

  void FlushTasks(base::SingleThreadTaskRunner& task_runner) {
    base::RunLoop loop;
    task_runner.PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  test::TaskEnvironment task_environment_;
  dawn::native::Instance native_instance_;
  DawnProcTable server_procs_ = MakeNullBackendServerProcs();
  DawnTestingCommandSerializer s2c_serializer_;
  dawn::wire::WireServer wire_server_{{.procs = &server_procs_,
                                       .serializer = &s2c_serializer_,
                                       .useSpontaneousCallbacks = true}};
};

// Regression test for crbug.com/585393108: losing the GPU channel while
// `instance.RequestAdapter()` is in flight must not synchronously destroy the
// `WebGraphicsContext3DProvider` (and its `WireClient`) on the disconnect
// stack.
TEST_F(MLWebGpuContextTest, ContextLostDuringRequestAdapterDefersDestruction) {
  V8TestingScope scope;
  ExecutionContext* execution_context = scope.GetExecutionContext();
  auto task_runner =
      execution_context->GetTaskRunner(TaskType::kMachineLearning);

  base::MockCallback<base::OnceClosure> destruction_callback;
  auto webgpu_interface = std::make_unique<TestWebGPUInterface>(
      base::MakeRefCounted<TestAPIChannel>());
  auto context_provider = std::make_unique<TestWebGPUContextProvider>(
      std::move(webgpu_interface), destruction_callback.Get());
  TestWebGPUContextProvider* raw_provider = context_provider.get();

  std::optional<webnn::WebGpuContextProperties> received_properties;
  ML::InitializeWebGpuContextFromProviderForTesting(
      execution_context, task_runner,
      blink::BindOnce(
          [](std::optional<webnn::WebGpuContextProperties>* out,
             webnn::WebGpuContextProperties properties) {
            *out = std::move(properties);
          },
          blink::Unretained(&received_properties)),
      std::move(context_provider));
  // Because `context_provider` was moved, only the pending `RequestAdapter`
  // callback owns it.
  ASSERT_FALSE(received_properties.has_value());

  EXPECT_CALL(destruction_callback, Run()).Times(0);
  raw_provider->SimulateGpuDisconnect();
  EXPECT_TRUE(raw_provider->disconnected());
  testing::Mock::VerifyAndClearExpectations(&destruction_callback);

  ASSERT_TRUE(received_properties.has_value());
  EXPECT_FALSE(received_properties->IsValid());

  raw_provider = nullptr;
  EXPECT_CALL(destruction_callback, Run()).Times(1);
  FlushTasks(*task_runner);
  testing::Mock::VerifyAndClearExpectations(&destruction_callback);
}

// Regression test for crbug.com/585393108: losing the GPU channel after
// `RequestAdapter` succeeds while `adapter.RequestDevice()` is in flight must
// also defer `WebGraphicsContext3DProvider` destruction to a fresh task.
TEST_F(MLWebGpuContextTest, ContextLostDuringRequestDeviceDefersDestruction) {
  V8TestingScope scope;
  ExecutionContext* execution_context = scope.GetExecutionContext();
  auto task_runner =
      execution_context->GetTaskRunner(TaskType::kMachineLearning);

  base::MockCallback<base::OnceClosure> destruction_callback;
  auto api_channel = base::MakeRefCounted<TestAPIChannel>();
  ConnectWireServer(*api_channel);

  auto webgpu_interface =
      std::make_unique<TestWebGPUInterface>(std::move(api_channel));
  auto context_provider = std::make_unique<TestWebGPUContextProvider>(
      std::move(webgpu_interface), destruction_callback.Get());
  TestWebGPUContextProvider* raw_provider = context_provider.get();

  std::optional<webnn::WebGpuContextProperties> received_properties;
  ML::InitializeWebGpuContextFromProviderForTesting(
      execution_context, task_runner,
      blink::BindOnce(
          [](std::optional<webnn::WebGpuContextProperties>* out,
             webnn::WebGpuContextProperties properties) {
            *out = std::move(properties);
          },
          blink::Unretained(&received_properties)),
      std::move(context_provider));

  // `RequestAdapter` was flushed to `wire_server_`, which queued the adapter
  // response in `s2c_serializer_`. Detach `c2s_serializer_` before delivering
  // the adapter response so the follow-up `RequestDevice` remains in flight.
  raw_provider->test_api_channel()->c2s_serializer().SetHandler(nullptr);
  ASSERT_TRUE(s2c_serializer_.Flush());
  raw_provider->test_api_channel()->ProcessEvents();
  s2c_serializer_.SetHandler(nullptr);
  ASSERT_FALSE(received_properties.has_value());

  EXPECT_CALL(destruction_callback, Run()).Times(0);
  raw_provider->SimulateGpuDisconnect();
  EXPECT_TRUE(raw_provider->disconnected());
  testing::Mock::VerifyAndClearExpectations(&destruction_callback);

  ASSERT_TRUE(received_properties.has_value());
  EXPECT_FALSE(received_properties->IsValid());

  raw_provider = nullptr;
  EXPECT_CALL(destruction_callback, Run()).Times(1);
  FlushTasks(*task_runner);
  testing::Mock::VerifyAndClearExpectations(&destruction_callback);
}

TEST_F(MLWebGpuContextTest, NullInstanceDefersContextProviderDestruction) {
  V8TestingScope scope;
  ExecutionContext* execution_context = scope.GetExecutionContext();
  auto task_runner =
      execution_context->GetTaskRunner(TaskType::kMachineLearning);

  base::MockCallback<base::OnceClosure> destruction_callback;
  auto webgpu_interface = std::make_unique<TestWebGPUInterface>(
      base::MakeRefCounted<TestAPIChannel>(/*return_null_instance=*/true));
  auto context_provider = std::make_unique<TestWebGPUContextProvider>(
      std::move(webgpu_interface), destruction_callback.Get());

  std::optional<webnn::WebGpuContextProperties> received_properties;
  EXPECT_CALL(destruction_callback, Run()).Times(0);
  ML::InitializeWebGpuContextFromProviderForTesting(
      execution_context, task_runner,
      blink::BindOnce(
          [](std::optional<webnn::WebGpuContextProperties>* out,
             webnn::WebGpuContextProperties properties) {
            *out = std::move(properties);
          },
          blink::Unretained(&received_properties)),
      std::move(context_provider));
  testing::Mock::VerifyAndClearExpectations(&destruction_callback);

  ASSERT_TRUE(received_properties.has_value());
  EXPECT_FALSE(received_properties->IsValid());

  EXPECT_CALL(destruction_callback, Run()).Times(1);
  FlushTasks(*task_runner);
  testing::Mock::VerifyAndClearExpectations(&destruction_callback);
}

TEST_F(MLWebGpuContextTest, SuccessfulBootstrapAndCleanTeardown) {
  V8TestingScope scope;
  ExecutionContext* execution_context = scope.GetExecutionContext();
  auto task_runner =
      execution_context->GetTaskRunner(TaskType::kMachineLearning);

  int flush_count = 0;
  base::MockCallback<base::OnceClosure> destruction_callback;
  auto api_channel = base::MakeRefCounted<TestAPIChannel>();
  ConnectWireServer(*api_channel);

  auto webgpu_interface = std::make_unique<TestWebGPUInterface>(
      std::move(api_channel), &flush_count);
  auto context_provider = std::make_unique<TestWebGPUContextProvider>(
      std::move(webgpu_interface), destruction_callback.Get());
  TestAPIChannel* raw_channel = context_provider->test_api_channel();

  std::optional<webnn::WebGpuContextProperties> received_properties;
  ML::InitializeWebGpuContextFromProviderForTesting(
      execution_context, task_runner,
      blink::BindOnce(
          [](std::optional<webnn::WebGpuContextProperties>* out,
             webnn::WebGpuContextProperties properties) {
            *out = std::move(properties);
          },
          blink::Unretained(&received_properties)),
      std::move(context_provider));

  // Complete `RequestAdapter`, which triggers `RequestDevice` + `Flush()`.
  ASSERT_TRUE(s2c_serializer_.Flush());
  raw_channel->ProcessEvents();
  ASSERT_FALSE(received_properties.has_value());

  // Complete `RequestDevice`.
  ASSERT_TRUE(s2c_serializer_.Flush());
  raw_channel->ProcessEvents();
  ASSERT_TRUE(received_properties.has_value());
  EXPECT_TRUE(received_properties->IsValid());

  int flush_count_before = flush_count;
  received_properties->webgpu_flush.Run();
  EXPECT_EQ(flush_count, flush_count_before + 1);

  DisconnectWireServer(*raw_channel);
  raw_channel = nullptr;

  EXPECT_CALL(destruction_callback, Run()).Times(0);
  received_properties.reset();
  testing::Mock::VerifyAndClearExpectations(&destruction_callback);

  EXPECT_CALL(destruction_callback, Run()).Times(1);
  FlushTasks(*task_runner);
  testing::Mock::VerifyAndClearExpectations(&destruction_callback);
}

}  // namespace

}  // namespace blink
