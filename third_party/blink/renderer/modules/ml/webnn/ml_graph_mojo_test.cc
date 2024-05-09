// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_mojo.h"

#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/test/scoped_feature_list.h"
#include "components/ml/webnn/features.mojom-blink.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_buffer_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_elu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_hard_sigmoid_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_leaky_relu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_linear_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_triangular_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_activation.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_buffer.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder_test.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_test_base.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_type_converter.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"

namespace blink {

namespace blink_mojom = webnn::mojom::blink;

// Helper struct to create faked mojom result of inference.
struct ComputeResult {
  WTF::HashMap<WTF::String, WTF::Vector<uint8_t>> output;
};

class FakeWebNNBuffer;

class MLGraphTestMojo : public MLGraphTestBase {
 public:
  MLGraphTestMojo()
      : scoped_feature_list_(webnn::mojom::features::kWebMachineLearningNeuralNetwork) {}

  void SetGraphInfo(blink_mojom::GraphInfoPtr graph_info) {
    graph_info_ = std::move(graph_info);
  }

  blink_mojom::GraphInfoPtr GetGraphInfo() { return std::move(graph_info_); }

  void SetComputeResult(const ComputeResult& compute_result) {
    compute_result_ = std::move(compute_result);
  }

  const ComputeResult& GetComputeResult() const { return compute_result_; }

  void SetInputArrayBuffers(HashMap<String, mojo_base::BigBuffer> buffers) {
    input_array_buffers_ = std::move(buffers);
  }

  const HashMap<String, mojo_base::BigBuffer>& GetInputArrayBuffers() const {
    return input_array_buffers_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  blink_mojom::GraphInfoPtr graph_info_;
  HashMap<String, mojo_base::BigBuffer> input_array_buffers_;
  ComputeResult compute_result_;
};

class WebNNContextHelper {
 public:
  WebNNContextHelper() = default;
  ~WebNNContextHelper() = default;

  void ConnectWebNNBufferImpl(const base::UnguessableToken& handle,
                              std::unique_ptr<FakeWebNNBuffer> buffer) {
    const auto it = buffer_impls_.find(handle);
    ASSERT_TRUE(it == buffer_impls_.end());
    buffer_impls_.try_emplace(handle, std::move(buffer));
  }

  void DisconnectAndDestroyWebNNBufferImpl(
      const base::UnguessableToken& handle) {
    buffer_impls_.erase(handle);
  }

 private:
  std::map<base::UnguessableToken, std::unique_ptr<FakeWebNNBuffer>>
      buffer_impls_;
};

class FakeWebNNGraph : public blink_mojom::WebNNGraph {
 public:
  explicit FakeWebNNGraph(MLGraphTestMojo& helper) : helper_(helper) {}
  FakeWebNNGraph(const FakeWebNNGraph&) = delete;
  FakeWebNNGraph(FakeWebNNGraph&&) = delete;
  ~FakeWebNNGraph() override = default;

 private:
  void Compute(HashMap<String, mojo_base::BigBuffer> inputs,
               blink_mojom::WebNNGraph::ComputeCallback callback) override {
    // Set the input array buffers for validation in the test.
    helper_->SetInputArrayBuffers(std::move(inputs));

    // Return the compute result with shared memory.
    auto& compute_result = helper_->GetComputeResult();
    HashMap<String, mojo_base::BigBuffer> mojo_outputs;
    for (const auto& [name, output_data] : compute_result.output) {
      mojo_outputs.insert(name, base::span(output_data));
    }
    std::move(callback).Run(
        blink_mojom::ComputeResult::NewNamedOutputs(std::move(mojo_outputs)));
  }

  // Just return for testing the validation of inputs and outputs.
  void Dispatch(
      const HashMap<WTF::String, base::UnguessableToken>& named_inputs,
      const HashMap<WTF::String, base::UnguessableToken>& named_outputs)
      override {}

  const raw_ref<MLGraphTestMojo, DanglingUntriaged> helper_;
};

class FakeWebNNBuffer : public blink_mojom::WebNNBuffer {
 public:
  FakeWebNNBuffer(
      WebNNContextHelper& helper,
      mojo::PendingAssociatedReceiver<blink_mojom::WebNNBuffer> receiver,
      const base::UnguessableToken& buffer_handle,
      uint64_t size)
      : helper_(helper),
        receiver_(this, std::move(receiver)),
        handle_(buffer_handle),
        buffer_(size) {
    receiver_.set_disconnect_handler(WTF::BindOnce(
        &FakeWebNNBuffer::OnConnectionError, WTF::Unretained(this)));
  }

  ~FakeWebNNBuffer() override = default;

  FakeWebNNBuffer(const FakeWebNNBuffer&) = delete;
  FakeWebNNBuffer(FakeWebNNBuffer&&) = delete;

  const base::UnguessableToken& handle() const { return handle_; }

 private:
  void ReadBuffer(ReadBufferCallback callback) override {
    mojo_base::BigBuffer dst_buffer(buffer_.byte_span());

    std::move(callback).Run(
        blink_mojom::ReadBufferResult::NewBuffer(std::move(dst_buffer)));
  }

  void WriteBuffer(mojo_base::BigBuffer src_buffer) override {
    ASSERT_LE(src_buffer.size(), buffer_.size());
    base::span(buffer_).first(src_buffer.size()).copy_from(src_buffer);
  }

  void OnConnectionError() {
    helper_->DisconnectAndDestroyWebNNBufferImpl(handle());
  }

  const raw_ref<WebNNContextHelper, DanglingUntriaged> helper_;

  mojo::AssociatedReceiver<blink_mojom::WebNNBuffer> receiver_;

  const base::UnguessableToken handle_;

  mojo_base::BigBuffer buffer_;
};

class FakeWebNNContext : public blink_mojom::WebNNContext {
 public:
  explicit FakeWebNNContext(MLGraphTestMojo& helper) : helper_(helper) {}
  FakeWebNNContext(const FakeWebNNContext&) = delete;
  FakeWebNNContext(FakeWebNNContext&&) = delete;
  ~FakeWebNNContext() override = default;

 private:
  // Override methods from webnn::mojom::WebNNContext.
  void CreateGraph(blink_mojom::GraphInfoPtr graph_info,
                   CreateGraphCallback callback) override {
    helper_->SetGraphInfo(std::move(graph_info));

    mojo::PendingAssociatedRemote<blink_mojom::WebNNGraph> blink_remote;
    // The receiver bind to FakeWebNNGraph.
    mojo::MakeSelfOwnedAssociatedReceiver<blink_mojom::WebNNGraph>(
        std::make_unique<FakeWebNNGraph>(*helper_),
        blink_remote.InitWithNewEndpointAndPassReceiver());

    std::move(callback).Run(blink_mojom::CreateGraphResult::NewGraphRemote(
        std::move(blink_remote)));
  }

  void CreateBuffer(
      mojo::PendingAssociatedReceiver<blink_mojom::WebNNBuffer> receiver,
      blink_mojom::BufferInfoPtr buffer_info,
      const base::UnguessableToken& buffer_handle) override {
    context_helper_.ConnectWebNNBufferImpl(
        buffer_handle,
        std::make_unique<FakeWebNNBuffer>(context_helper_, std::move(receiver),
                                          buffer_handle, buffer_info->size));
  }

  WebNNContextHelper context_helper_;

  const raw_ref<MLGraphTestMojo, DanglingUntriaged> helper_;
};

class FakeWebNNContextProvider : public blink_mojom::WebNNContextProvider {
 public:
  explicit FakeWebNNContextProvider(MLGraphTestMojo& helper)
      : helper_(helper), receiver_(this) {}
  FakeWebNNContextProvider(const FakeWebNNContextProvider&) = delete;
  FakeWebNNContextProvider(FakeWebNNContextProvider&&) = delete;
  ~FakeWebNNContextProvider() override = default;

  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    DCHECK(!receiver_.is_bound());
    receiver_.Bind(mojo::PendingReceiver<blink_mojom::WebNNContextProvider>(
        std::move(handle)));
    receiver_.set_disconnect_handler(WTF::BindOnce(
        &FakeWebNNContextProvider::OnConnectionError, WTF::Unretained(this)));
  }

  bool IsBound() const { return receiver_.is_bound(); }

  void OnConnectionError() { receiver_.reset(); }

 private:
  // Override methods from webnn::mojom::WebNNContextProvider.
  void CreateWebNNContext(blink_mojom::CreateContextOptionsPtr options,
                          CreateWebNNContextCallback callback) override {
    mojo::PendingRemote<blink_mojom::WebNNContext> blink_remote;
    // The receiver bind to FakeWebNNContext.
    mojo::MakeSelfOwnedReceiver<blink_mojom::WebNNContext>(
        std::make_unique<FakeWebNNContext>(*helper_),
        blink_remote.InitWithNewPipeAndPassReceiver());

    std::move(callback).Run(blink_mojom::CreateContextResult::NewContextRemote(
        std::move(blink_remote)));
  }

  const raw_ref<MLGraphTestMojo> helper_;
  mojo::Receiver<blink_mojom::WebNNContextProvider> receiver_;
};

class ScopedWebNNServiceBinder {
 public:
  explicit ScopedWebNNServiceBinder(MLGraphTestMojo& helper,
                                    V8TestingScope& scope)
      : fake_webnn_context_provider_(
            std::make_unique<FakeWebNNContextProvider>(helper)),
        interface_broker_(
            scope.GetExecutionContext()->GetBrowserInterfaceBroker()) {
    interface_broker_->SetBinderForTesting(
        blink_mojom::WebNNContextProvider::Name_,
        WTF::BindRepeating(
            &FakeWebNNContextProvider::BindRequest,
            WTF::Unretained(fake_webnn_context_provider_.get())));
  }

  ~ScopedWebNNServiceBinder() {
    interface_broker_->SetBinderForTesting(
        blink_mojom::WebNNContextProvider::Name_, base::NullCallback());
  }

  bool IsWebNNContextBound() const {
    return fake_webnn_context_provider_->IsBound();
  }

 private:
  std::unique_ptr<FakeWebNNContextProvider> fake_webnn_context_provider_;
  const raw_ref<const BrowserInterfaceBrokerProxy> interface_broker_;
};

template <typename T>
T* V8ToObject(V8TestingScope* scope, ScriptValue value) {
  return NativeValueTraits<T>::NativeValue(scope->GetIsolate(), value.V8Value(),
                                           scope->GetExceptionState());
}

MLGraphMojo* ToMLGraphMojo(V8TestingScope* scope, ScriptValue value) {
  return V8ToObject<MLGraphMojo>(scope, value);
}

// Build a simple MLGraph asynchronously with only one relu operator.
ScriptPromise<MLGraph> BuildSimpleGraph(V8TestingScope& scope,
                                        MLContextOptions* context_options) {
  auto* builder = MLGraphTestBase::CreateGraphBuilder(scope, context_options);
  if (builder == nullptr) {
    return ScriptPromise<MLGraph>::RejectWithDOMException(
        scope.GetScriptState(),
        DOMException::Create(
            "Unable to create graph builder.",
            DOMException::GetErrorName(DOMExceptionCode::kOperationError)));
  }

  auto* lhs_operand =
      BuildInput(builder, "lhs", {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                 scope.GetExceptionState());
  auto* rhs_operand =
      BuildInput(builder, "rhs", {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                 scope.GetExceptionState());
  auto* output =
      builder->add(lhs_operand, rhs_operand, scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  return builder->build(scope.GetScriptState(), {{"output", output}},
                        scope.GetExceptionState());
}

bool IsBufferDataEqual(DOMArrayBuffer* array_buffer,
                       base::span<const uint8_t> expected_data) {
  return array_buffer->ByteSpan() == expected_data;
}

MaybeShared<DOMArrayBufferView> CreateArrayBufferViewFromBytes(
    DOMArrayBuffer* array_buffer,
    base::span<const uint8_t> data) {
  array_buffer->ByteSpan().first(data.size()).copy_from(data);
  return MaybeShared<DOMArrayBufferView>(
      blink::DOMUint8Array::Create(array_buffer, /*byte_offset=*/0,
                                   /*length=*/array_buffer->ByteLength()));
}

// Checks the contents of a MLBuffer.
// Returns false if unable to download or the buffer data did not match
// expected.
bool DownloadMLBufferAndCheck(V8TestingScope& scope,
                              MLContext* context,
                              MLBuffer* src_buffer,
                              base::span<const uint8_t> expected_data) {
  auto* script_state = scope.GetScriptState();
  ScriptPromiseTester tester(
      script_state,
      context->readBuffer(script_state, src_buffer, scope.GetExceptionState()));
  tester.WaitUntilSettled();
  if (tester.IsRejected()) {
    return false;
  }
  EXPECT_TRUE(tester.IsFulfilled());
  auto* array_buffer = V8ToObject<DOMArrayBuffer>(&scope, tester.Value());
  return IsBufferDataEqual(array_buffer, expected_data);
}

MLBuffer* CreateMLBufferForOperand(V8TestingScope& scope,
                                   MLContext* ml_context,
                                   const MLOperand* operand) {
  auto array_buffer_view = CreateArrayBufferViewForOperand(operand);
  auto* desc = MLBufferDescriptor::Create();
  desc->setSize(array_buffer_view->byteLength());

  MLBuffer* ml_buffer = ml_context->createBuffer(scope.GetScriptState(), desc,
                                                 scope.GetExceptionState());
  ml_context->writeBuffer(
      scope.GetScriptState(), ml_buffer,
      MaybeShared<DOMArrayBufferView>(array_buffer_view.Get()),
      /*src_element_offset=*/0, scope.GetExceptionState());
  return ml_buffer;
}

Vector<uint8_t> GetMLBufferValues(V8TestingScope& scope,
                                  MLContext* ml_context,
                                  MLBuffer* ml_buffer) {
  ScriptPromiseTester tester(
      scope.GetScriptState(),
      ml_context->readBuffer(scope.GetScriptState(), ml_buffer,
                             scope.GetExceptionState()));
  tester.WaitUntilSettled();
  if (tester.IsRejected()) {
    return {};
  }
  auto* array_buffer = V8ToObject<DOMArrayBuffer>(&scope, tester.Value());
  return GetArrayBufferViewValues<uint8_t>(
      NotShared<DOMArrayBufferView>(blink::DOMUint8Array::Create(
          array_buffer, /*byte_offset=*/0, ml_buffer->size())));
}

TEST_P(MLGraphTestMojo, CreateWebNNBufferTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* script_state = scope.GetScriptState();

  ScriptPromiseTester context_tester(script_state,
                                     CreateContext(scope, options));
  context_tester.WaitUntilSettled();
  EXPECT_TRUE(context_tester.IsFulfilled());
  MLContext* ml_context = V8ToObject<MLContext>(&scope, context_tester.Value());

  auto* desc = MLBufferDescriptor::Create();
  desc->setSize(4ull);

  MLBuffer* ml_buffer =
      ml_context->createBuffer(script_state, desc, scope.GetExceptionState());

  if (scope.GetExceptionState().Code() ==
      ToExceptionCode(DOMExceptionCode::kNotSupportedError)) {
    GTEST_SKIP() << "MLBuffer has not been implemented on this platform.";
  }

  ASSERT_THAT(ml_buffer, testing::NotNull());
  EXPECT_EQ(ml_buffer->size(), desc->size());
}

TEST_P(MLGraphTestMojo, WriteWebNNBufferTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* script_state = scope.GetScriptState();

  ScriptPromiseTester context_tester(script_state,
                                     CreateContext(scope, options));
  context_tester.WaitUntilSettled();
  EXPECT_TRUE(context_tester.IsFulfilled());
  MLContext* ml_context = V8ToObject<MLContext>(&scope, context_tester.Value());

  constexpr uint64_t kBufferSize = 4ull;

  auto* desc = MLBufferDescriptor::Create();
  desc->setSize(kBufferSize);

  MLBuffer* ml_buffer =
      ml_context->createBuffer(script_state, desc, scope.GetExceptionState());

  if (scope.GetExceptionState().Code() ==
      ToExceptionCode(DOMExceptionCode::kNotSupportedError)) {
    GTEST_SKIP() << "MLBuffer has not been implemented on this platform.";
  }

  ASSERT_THAT(ml_buffer, testing::NotNull());

  const std::array<const uint8_t, kBufferSize> input_data = {0xAA, 0xAA, 0xAA,
                                                             0xAA};
  DOMArrayBuffer* array_buffer =
      DOMArrayBuffer::Create(input_data.data(), input_data.size());
  ASSERT_THAT(array_buffer, testing::NotNull());

  // Writing the full buffer.
  ml_context->writeBuffer(
      script_state, ml_buffer,
      CreateArrayBufferViewFromBytes(array_buffer, input_data),
      /*src_element_offset=*/0, scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  ml_context->writeBuffer(
      script_state, ml_buffer,
      MaybeShared<DOMArrayBufferView>(blink::DOMUint32Array::Create(
          array_buffer, /*byte_offset=*/0,
          /*length=*/array_buffer->ByteLength() / 4)),
      /*src_element_offset=*/0, scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  EXPECT_TRUE(
      DownloadMLBufferAndCheck(scope, ml_context, ml_buffer, input_data));

  // Writing to the remainder of the buffer from source offset.
  ml_context->writeBuffer(
      script_state, ml_buffer,
      CreateArrayBufferViewFromBytes(
          array_buffer,
          std::array<const uint8_t, kBufferSize>{0xAA, 0xAA, 0xBB, 0xBB}),
      /*src_element_offset=*/2, scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  // Writing zero bytes at the end of the buffer.
  ml_context->writeBuffer(
      script_state, ml_buffer,
      MaybeShared<DOMArrayBufferView>(blink::DOMUint32Array::Create(
          array_buffer, /*byte_offset=*/0,
          /*length=*/array_buffer->ByteLength() / 4)),
      /*src_element_offset=*/1, scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  EXPECT_TRUE(DownloadMLBufferAndCheck(
      scope, ml_context, ml_buffer,
      std::array<const uint8_t, kBufferSize>{0xBB, 0xBB, 0xAA, 0xAA}));

  // Writing with both a source offset and size.
  ml_context->writeBuffer(
      script_state, ml_buffer,
      CreateArrayBufferViewFromBytes(
          array_buffer,
          std::array<const uint8_t, kBufferSize>{0xCC, 0xCC, 0xCC, 0xCC}),
      /*src_element_offset=*/2, /*src_element_count=*/1,
      scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  EXPECT_TRUE(DownloadMLBufferAndCheck(
      scope, ml_context, ml_buffer,
      std::array<const uint8_t, kBufferSize>{0xCC, 0xBB, 0xAA, 0xAA}));
}

// Writing data from an array buffer to a destroyed MLBuffer should not crash.
TEST_P(MLGraphTestMojo, WriteWebNNBufferThenDestroyTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* script_state = scope.GetScriptState();

  ScriptPromiseTester context_tester(script_state,
                                     CreateContext(scope, options));
  context_tester.WaitUntilSettled();
  EXPECT_TRUE(context_tester.IsFulfilled());
  MLContext* ml_context = V8ToObject<MLContext>(&scope, context_tester.Value());

  auto* desc = MLBufferDescriptor::Create();
  desc->setSize(4ull);

  MLBuffer* ml_buffer =
      ml_context->createBuffer(script_state, desc, scope.GetExceptionState());

  if (scope.GetExceptionState().Code() ==
      ToExceptionCode(DOMExceptionCode::kNotSupportedError)) {
    GTEST_SKIP() << "MLBuffer has not been implemented on this platform.";
  }

  ASSERT_THAT(ml_buffer, testing::NotNull());

  ml_buffer->destroy();

  ml_context->writeBuffer(
      script_state, ml_buffer,
      CreateDOMArrayBufferView(desc->size(), V8MLOperandDataType::Enum::kUint8)
          ->BufferBase(),
      /*src_byte_offset=*/0, scope.GetExceptionState());
}

// Reading data from an array buffer to a destroyed MLBuffer should not crash.
TEST_P(MLGraphTestMojo, ReadWebNNBufferThenDestroyTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* script_state = scope.GetScriptState();

  ScriptPromiseTester context_tester(script_state,
                                     CreateContext(scope, options));
  context_tester.WaitUntilSettled();
  EXPECT_TRUE(context_tester.IsFulfilled());
  MLContext* ml_context = V8ToObject<MLContext>(&scope, context_tester.Value());

  auto* desc = MLBufferDescriptor::Create();
  desc->setSize(4ull);

  MLBuffer* ml_buffer =
      ml_context->createBuffer(script_state, desc, scope.GetExceptionState());

  if (scope.GetExceptionState().Code() ==
      ToExceptionCode(DOMExceptionCode::kNotSupportedError)) {
    GTEST_SKIP() << "MLBuffer has not been implemented on this platform.";
  }

  ASSERT_THAT(ml_buffer, testing::NotNull());

  ml_buffer->destroy();

  ScriptPromiseTester buffer_tester(
      script_state, ml_context->readBuffer(script_state, ml_buffer,
                                           scope.GetExceptionState()));
  buffer_tester.WaitUntilSettled();
  EXPECT_TRUE(buffer_tester.IsRejected());
}

TEST_P(MLGraphTestMojo, WebNNGraphDispatchTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  const Vector<uint32_t> dimensions = {3, 5};
  const wtf_size_t number_of_elements = base::checked_cast<wtf_size_t>(
      webnn::ValidateAndCalculateElementsNumber(dimensions).value());

  // Build the graph.
  auto* lhs_operand =
      BuildInput(builder, "lhs", dimensions, V8MLOperandDataType::Enum::kUint8,
                 scope.GetExceptionState());
  auto* rhs_operand =
      BuildInput(builder, "rhs", dimensions, V8MLOperandDataType::Enum::kUint8,
                 scope.GetExceptionState());
  auto* output_operand = BuildElementWiseBinary(
      scope, builder, webnn::mojom::blink::ElementWiseBinary::Kind::kAdd,
      lhs_operand, rhs_operand);
  auto [graph, error_message, build_exception] =
      BuildGraph(scope, builder, {{"output", output_operand}});
  ASSERT_THAT(graph, testing::NotNull());

  MLContext* ml_context = builder->GetContext();

  // Check if MLBuffer is supported.
  auto* desc = MLBufferDescriptor::Create();
  desc->setSize(4ull);

  MLBuffer* ml_buffer = ml_context->createBuffer(scope.GetScriptState(), desc,
                                                 scope.GetExceptionState());

  if (scope.GetExceptionState().Code() ==
      ToExceptionCode(DOMExceptionCode::kNotSupportedError)) {
    GTEST_SKIP() << "MLBuffer has not been implemented on this platform.";
  }

  ASSERT_THAT(ml_buffer, testing::NotNull());

  MLNamedBuffers inputs(
      {{"lhs", CreateMLBufferForOperand(scope, ml_context, lhs_operand)},
       {"rhs", CreateMLBufferForOperand(scope, ml_context, rhs_operand)}});
  MLNamedBuffers outputs({{"output", CreateMLBufferForOperand(
                                         scope, ml_context, output_operand)}});

  {
    // Dispatch successfully.
    ml_context->dispatch(scope.GetScriptState(), graph, inputs, outputs,
                         scope.GetExceptionState());
    EXPECT_EQ(scope.GetExceptionState().Code(),
              ToExceptionCode(DOMExceptionCode::kNoError));
    Vector<uint8_t> results =
        GetMLBufferValues(scope, ml_context, outputs[0].second);
    EXPECT_EQ(results, Vector<uint8_t>(number_of_elements, 0));

    // Dispatch again successfully.
    ml_context->dispatch(scope.GetScriptState(), graph, inputs, outputs,
                         scope.GetExceptionState());
    EXPECT_EQ(scope.GetExceptionState().Code(),
              ToExceptionCode(DOMExceptionCode::kNoError));
    results = GetMLBufferValues(scope, ml_context, outputs[0].second);
    EXPECT_EQ(results, Vector<uint8_t>(number_of_elements, 0));
  }
}

struct OperandInfoMojo {
  blink_mojom::Operand::DataType data_type;
  Vector<uint32_t> dimensions;
};

using OperandInfoBlink = OperandInfo<float>;

TEST_P(MLGraphTestMojo, CreateWebNNGraphTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* script_state = scope.GetScriptState();
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);

  {
    ScriptPromiseTester tester(script_state, BuildSimpleGraph(scope, options));
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
    auto* mojo_graph = ToMLGraphMojo(&scope, tester.Value());
    ASSERT_THAT(mojo_graph, testing::NotNull());
    EXPECT_TRUE(scoped_setup_binder.IsWebNNContextBound());
  }
}

struct ClampOptions {
  std::optional<float> min_value;
  std::optional<float> max_value;
};

// TODO: crbug.com/325598628 - Consider replacing this with direct use of the
// mojo Activation struct.
struct Activation {
  webnn::mojom::blink::Activation::Tag kind;
  std::optional<ClampOptions> clamp_options;
  std::optional<float> hard_sigmoid_alpha;
  std::optional<float> hard_sigmoid_beta;
  std::optional<float> elu_alpha;
  std::optional<float> leaky_relu_alpha;
  std::optional<float> linear_alpha;
  std::optional<float> linear_beta;
};

MLActivation* CreateActivation(V8TestingScope& scope,
                               MLGraphBuilder* builder,
                               const Activation& activation) {
  switch (activation.kind) {
    case webnn::mojom::blink::Activation::Tag::kClamp: {
      auto* clamp_options = MLClampOptions::Create();
      CHECK(clamp_options);
      clamp_options->setMinValue(activation.clamp_options->min_value.value());
      clamp_options->setMaxValue(activation.clamp_options->max_value.value());
      return builder->clamp(clamp_options, scope.GetExceptionState());
    }
    case webnn::mojom::blink::Activation::Tag::kElu: {
      auto* elu_options = MLEluOptions::Create();
      CHECK(elu_options);
      if (activation.elu_alpha.has_value()) {
        elu_options->setAlpha(activation.elu_alpha.value());
      }
      return builder->elu(elu_options, scope.GetExceptionState());
    }
    case webnn::mojom::blink::Activation::Tag::kGelu:
      return builder->gelu(scope.GetExceptionState());
    case webnn::mojom::blink::Activation::Tag::kHardSigmoid: {
      auto* hard_sigmoid_options = MLHardSigmoidOptions::Create();
      CHECK(hard_sigmoid_options);
      if (activation.hard_sigmoid_alpha.has_value()) {
        hard_sigmoid_options->setAlpha(activation.hard_sigmoid_alpha.value());
      }
      if (activation.hard_sigmoid_beta.has_value()) {
        hard_sigmoid_options->setBeta(activation.hard_sigmoid_beta.value());
      }
      return builder->hardSigmoid(hard_sigmoid_options,
                                  scope.GetExceptionState());
    }
    case webnn::mojom::blink::Activation::Tag::kLeakyRelu: {
      auto* leaky_relu_options = MLLeakyReluOptions::Create();
      CHECK(leaky_relu_options);
      if (activation.leaky_relu_alpha.has_value()) {
        leaky_relu_options->setAlpha(activation.leaky_relu_alpha.value());
      }
      return builder->leakyRelu(leaky_relu_options, scope.GetExceptionState());
    }
    case webnn::mojom::blink::Activation::Tag::kLinear: {
      auto* linear_options = MLLinearOptions::Create();
      CHECK(linear_options);
      if (activation.linear_alpha.has_value()) {
        linear_options->setAlpha(activation.linear_alpha.value());
      }
      if (activation.linear_beta.has_value()) {
        linear_options->setBeta(activation.linear_beta.value());
      }
      return builder->linear(linear_options, scope.GetExceptionState());
    }
    case webnn::mojom::blink::Activation::Tag::kRelu:
      return builder->relu(scope.GetExceptionState());
    case webnn::mojom::blink::Activation::Tag::kSigmoid:
      return builder->sigmoid(scope.GetExceptionState());
    case webnn::mojom::blink::Activation::Tag::kSoftmax:
      return builder->softmax(scope.GetExceptionState());
    case webnn::mojom::blink::Activation::Tag::kSoftplus:
      return builder->softplus(scope.GetExceptionState());
    case webnn::mojom::blink::Activation::Tag::kSoftsign:
      return builder->softsign(scope.GetExceptionState());
    case webnn::mojom::blink::Activation::Tag::kTanh:
      return builder->tanh(scope.GetExceptionState());
  }
}

void CheckActivation(const webnn::mojom::blink::ActivationPtr& mojom_activation,
                     const Activation& expected_activation) {
  switch (expected_activation.kind) {
    case webnn::mojom::blink::Activation::Tag::kClamp: {
      ASSERT_TRUE(mojom_activation->is_clamp());
      auto& clamp = mojom_activation->get_clamp();
      CHECK(clamp);
      auto& clamp_options = expected_activation.clamp_options;
      CHECK(clamp_options);
      EXPECT_EQ(clamp->min_value, clamp_options->min_value);
      EXPECT_EQ(clamp->max_value, clamp_options->max_value);
      break;
    }
    case webnn::mojom::blink::Activation::Tag::kElu: {
      ASSERT_TRUE(mojom_activation->is_elu());
      auto& elu = mojom_activation->get_elu();
      CHECK(elu);
      CHECK(expected_activation.elu_alpha.has_value());
      EXPECT_EQ(elu->alpha, expected_activation.elu_alpha.value());
      break;
    }
    case webnn::mojom::blink::Activation::Tag::kGelu:
      EXPECT_TRUE(mojom_activation->is_gelu());
      break;
    case webnn::mojom::blink::Activation::Tag::kHardSigmoid: {
      ASSERT_TRUE(mojom_activation->is_hard_sigmoid());
      auto& hard_sigmoid = mojom_activation->get_hard_sigmoid();
      CHECK(hard_sigmoid);
      CHECK(expected_activation.hard_sigmoid_alpha.has_value());
      EXPECT_EQ(hard_sigmoid->alpha,
                expected_activation.hard_sigmoid_alpha.value());
      CHECK(expected_activation.hard_sigmoid_beta.has_value());
      EXPECT_EQ(hard_sigmoid->beta,
                expected_activation.hard_sigmoid_beta.value());
      break;
    }
    case webnn::mojom::blink::Activation::Tag::kLeakyRelu: {
      ASSERT_TRUE(mojom_activation->is_leaky_relu());
      auto& leaky_relu = mojom_activation->get_leaky_relu();
      CHECK(leaky_relu);
      CHECK(expected_activation.leaky_relu_alpha.has_value());
      EXPECT_EQ(leaky_relu->alpha,
                expected_activation.leaky_relu_alpha.value());
      break;
    }
    case webnn::mojom::blink::Activation::Tag::kLinear: {
      ASSERT_TRUE(mojom_activation->is_linear());
      auto& linear = mojom_activation->get_linear();
      CHECK(linear);
      CHECK(expected_activation.linear_alpha.has_value());
      EXPECT_EQ(linear->alpha, expected_activation.linear_alpha.value());
      CHECK(expected_activation.linear_beta.has_value());
      EXPECT_EQ(linear->beta, expected_activation.linear_beta.value());
      break;
    }
    case webnn::mojom::blink::Activation::Tag::kRelu:
      EXPECT_TRUE(mojom_activation->is_relu());
      break;
    case webnn::mojom::blink::Activation::Tag::kSigmoid:
      EXPECT_TRUE(mojom_activation->is_sigmoid());
      break;
    case webnn::mojom::blink::Activation::Tag::kSoftmax:
      EXPECT_TRUE(mojom_activation->is_softmax());
      break;
    case webnn::mojom::blink::Activation::Tag::kSoftplus: {
      EXPECT_TRUE(mojom_activation->is_softplus());
      break;
    }
    case webnn::mojom::blink::Activation::Tag::kSoftsign:
      EXPECT_TRUE(mojom_activation->is_softsign());
      break;
    case webnn::mojom::blink::Activation::Tag::kTanh:
      EXPECT_TRUE(mojom_activation->is_tanh());
      break;
  }
}

struct BatchNormalizationTester {
  OperandInfoBlink input;
  OperandInfoBlink mean;
  OperandInfoBlink variance;
  struct BatchNormalizationOptions {
    std::optional<OperandInfoBlink> scale;
    std::optional<OperandInfoBlink> bias;
    std::optional<uint32_t> axis;
    std::optional<float> epsilon;
    std::optional<Activation> activation;
  };
  struct BatchNormalizationAttributes {
    std::optional<OperandInfoMojo> scale;
    std::optional<OperandInfoMojo> bias;
    uint32_t axis = 1;
    float epsilon = 1e-5;
    std::optional<Activation> activation;
  };
  BatchNormalizationOptions options;
  OperandInfoMojo expected_operand;
  BatchNormalizationAttributes expected_attributes;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* mean_operand = BuildInput(builder, "mean", mean.dimensions,
                                    mean.data_type, scope.GetExceptionState());
    auto* variance_operand =
        BuildInput(builder, "variance", variance.dimensions, variance.data_type,
                   scope.GetExceptionState());
    MLBatchNormalizationOptions* batch_normalization_options =
        MLBatchNormalizationOptions::Create();
    if (options.scale) {
      batch_normalization_options->setScale(
          BuildInput(builder, "scale", options.scale->dimensions,
                     options.scale->data_type, scope.GetExceptionState()));
    }
    if (options.bias) {
      batch_normalization_options->setBias(
          BuildInput(builder, "bias", options.bias->dimensions,
                     options.bias->data_type, scope.GetExceptionState()));
    }
    if (options.axis) {
      batch_normalization_options->setAxis(options.axis.value());
    }
    if (options.epsilon) {
      batch_normalization_options->setEpsilon(options.epsilon.value());
    }
    if (options.activation) {
      auto* activation =
          CreateActivation(scope, builder, options.activation.value());
      CHECK(activation);
      batch_normalization_options->setActivation(activation);
    }

    auto* output_operand = builder->batchNormalization(
        input_operand, mean_operand, variance_operand,
        batch_normalization_options, scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_TRUE(operation->is_batch_normalization());
    auto& batch_normalization = operation->get_batch_normalization();
    EXPECT_EQ(batch_normalization->axis, expected_attributes.axis);
    EXPECT_FLOAT_EQ(batch_normalization->epsilon, expected_attributes.epsilon);
    if (options.scale) {
      auto scale_operand_iter = graph_info->id_to_operand_map.find(
          batch_normalization->scale_operand_id.value());
      ASSERT_TRUE(scale_operand_iter != graph_info->id_to_operand_map.end());
      EXPECT_EQ(scale_operand_iter->value->data_type,
                expected_attributes.scale->data_type);
      EXPECT_EQ(scale_operand_iter->value->dimensions,
                expected_attributes.scale->dimensions);
    }
    if (options.bias) {
      auto bias_operand_iter = graph_info->id_to_operand_map.find(
          batch_normalization->bias_operand_id.value());
      ASSERT_TRUE(bias_operand_iter != graph_info->id_to_operand_map.end());
      EXPECT_EQ(bias_operand_iter->value->data_type,
                expected_attributes.bias->data_type);
      EXPECT_EQ(bias_operand_iter->value->dimensions,
                expected_attributes.bias->dimensions);
    }
    if (options.activation) {
      CHECK(expected_attributes.activation);
      CheckActivation(batch_normalization->activation,
                      expected_attributes.activation.value());
    }
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
  }
};

TEST_P(MLGraphTestMojo, BatchNormalizationTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test batchNormalization with default options.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes = {.scale = std::nullopt,
                                .bias = std::nullopt,
                                .axis = 1,
                                .epsilon = 1e-5,
                                .activation = std::nullopt}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchnormalization with axis = 3.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 4, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {5}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {5}},
        .options = {.axis = 3},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 4, 5}},
        .expected_attributes = {.scale = std::nullopt,
                                .bias = std::nullopt,
                                .axis = 3,
                                .epsilon = 1e-5,
                                .activation = std::nullopt}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchnormalization with epsilon = 0.01.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 4, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options = {.epsilon = 0.01},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 4, 5}},
        .expected_attributes = {.scale = std::nullopt,
                                .bias = std::nullopt,
                                .axis = 1,
                                .epsilon = 0.01,
                                .activation = std::nullopt}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchnormalization with scale and bias.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options = {.scale =
                        OperandInfoBlink{
                            .data_type = V8MLOperandDataType::Enum::kFloat32,
                            .dimensions = {3}},
                    .bias =
                        OperandInfoBlink{
                            .data_type = V8MLOperandDataType::Enum::kFloat32,
                            .dimensions = {3}}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale =
                 OperandInfoMojo{
                     .data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {3}},
             .bias =
                 OperandInfoMojo{
                     .data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {3}},
             .axis = 1,
             .epsilon = 1e-5,
             .activation = std::nullopt}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with clamp activation.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options = {.activation =
                        Activation{
                            .kind =
                                webnn::mojom::blink::Activation::Tag::kClamp,
                            .clamp_options = ClampOptions{.min_value = 1.0,
                                                          .max_value = 6.0}}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale = std::nullopt,
             .bias = std::nullopt,
             .axis = 1,
             .epsilon = 1e-5,
             .activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kClamp,
                     .clamp_options =
                         ClampOptions{.min_value = 1.0, .max_value = 6.0}}}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with elu activation with default options.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options = {.activation =
                        Activation{
                            .kind =
                                webnn::mojom::blink::Activation::Tag::kElu}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale = std::nullopt,
             .bias = std::nullopt,
             .axis = 1,
             .epsilon = 1e-5,
             .activation =
                 Activation{.kind = webnn::mojom::blink::Activation::Tag::kElu,
                            .elu_alpha = 1.0}}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with elu activation with given options.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options = {.activation =
                        Activation{
                            .kind = webnn::mojom::blink::Activation::Tag::kElu,
                            .elu_alpha = 0.5}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale = std::nullopt,
             .bias = std::nullopt,
             .axis = 1,
             .epsilon = 1e-5,
             .activation =
                 Activation{.kind = webnn::mojom::blink::Activation::Tag::kElu,
                            .elu_alpha = 0.5}}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with hardSigmoid activation with default options.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options =
            {.activation =
                 Activation{
                     .kind =
                         webnn::mojom::blink::Activation::Tag::kHardSigmoid}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale = std::nullopt,
             .bias = std::nullopt,
             .axis = 1,
             .epsilon = 1e-5,
             .activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kHardSigmoid,
                     .hard_sigmoid_alpha = 0.2,
                     .hard_sigmoid_beta = 0.5}}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with leaky relu activation with default options.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options =
            {.activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kLeakyRelu}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale = std::nullopt,
             .bias = std::nullopt,
             .axis = 1,
             .epsilon = 1e-5,
             .activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kLeakyRelu,
                     .leaky_relu_alpha = 0.01}}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with leaky relu activation with given options.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options =
            {.activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kLeakyRelu,
                     .leaky_relu_alpha = 0.02}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale = std::nullopt,
             .bias = std::nullopt,
             .axis = 1,
             .epsilon = 1e-5,
             .activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kLeakyRelu,
                     .leaky_relu_alpha = 0.02}}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with relu activation.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options = {.activation =
                        Activation{
                            .kind =
                                webnn::mojom::blink::Activation::Tag::kRelu}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale = std::nullopt,
             .bias = std::nullopt,
             .axis = 1,
             .epsilon = 1e-5,
             .activation =
                 Activation{.kind =
                                webnn::mojom::blink::Activation::Tag::kRelu}}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with sigmoid activation.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options =
            {.activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kSigmoid}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale = std::nullopt,
             .bias = std::nullopt,
             .axis = 1,
             .epsilon = 1e-5,
             .activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kSigmoid}}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with softmax activation.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options =
            {.activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kSoftmax}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale = std::nullopt,
             .bias = std::nullopt,
             .axis = 1,
             .epsilon = 1e-5,
             .activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kSoftmax}}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with softplus activation.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options =
            {.activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kSoftplus}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale = std::nullopt,
             .bias = std::nullopt,
             .axis = 1,
             .epsilon = 1e-5,
             .activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kSoftplus}}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with softsign activation.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options =
            {.activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kSoftsign}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale = std::nullopt,
             .bias = std::nullopt,
             .axis = 1,
             .epsilon = 1e-5,
             .activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kSoftsign}}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with tanh activation.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options = {.activation =
                        Activation{
                            .kind =
                                webnn::mojom::blink::Activation::Tag::kTanh}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale = std::nullopt,
             .bias = std::nullopt,
             .axis = 1,
             .epsilon = 1e-5,
             .activation =
                 Activation{.kind =
                                webnn::mojom::blink::Activation::Tag::kTanh}}}
        .Test(*this, scope, builder);
  }
}

struct ElementWiseBinaryTester {
  OperandInfoBlink lhs;
  OperandInfoBlink rhs;
  OperandInfoMojo expected_out;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    Test(helper, scope, builder,
         webnn::mojom::blink::ElementWiseBinary::Kind::kAdd);
    Test(helper, scope, builder,
         webnn::mojom::blink::ElementWiseBinary::Kind::kSub);
    Test(helper, scope, builder,
         webnn::mojom::blink::ElementWiseBinary::Kind::kMul);
    Test(helper, scope, builder,
         webnn::mojom::blink::ElementWiseBinary::Kind::kDiv);
    Test(helper, scope, builder,
         webnn::mojom::blink::ElementWiseBinary::Kind::kMin);
    Test(helper, scope, builder,
         webnn::mojom::blink::ElementWiseBinary::Kind::kMax);
    Test(helper, scope, builder,
         webnn::mojom::blink::ElementWiseBinary::Kind::kPow);
  }

  void TestLogicalComparison(MLGraphTestMojo& helper,
                             V8TestingScope& scope,
                             MLGraphBuilder* builder) {
    Test(helper, scope, builder,
         webnn::mojom::blink::ElementWiseBinary::Kind::kEqual);
    Test(helper, scope, builder,
         webnn::mojom::blink::ElementWiseBinary::Kind::kGreater);
    Test(helper, scope, builder,
         webnn::mojom::blink::ElementWiseBinary::Kind::kGreaterOrEqual);
    Test(helper, scope, builder,
         webnn::mojom::blink::ElementWiseBinary::Kind::kLesser);
    Test(helper, scope, builder,
         webnn::mojom::blink::ElementWiseBinary::Kind::kLesserOrEqual);
  }

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder,
            webnn::mojom::blink::ElementWiseBinary::Kind kind) {
    // Build the graph.
    auto* lhs_operand = BuildInput(builder, "lhs", lhs.dimensions,
                                   lhs.data_type, scope.GetExceptionState());
    auto* rhs_operand = BuildInput(builder, "rhs", rhs.dimensions,
                                   rhs.data_type, scope.GetExceptionState());
    auto* output_operand =
        BuildElementWiseBinary(scope, builder, kind, lhs_operand, rhs_operand);
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    EXPECT_EQ(graph_info->id_to_operand_map.size(), 3u);
    EXPECT_EQ(graph_info->input_operands.size(), 2u);
    // Verify the left `mojo::Operand`.
    auto lhs_operand_id = graph_info->input_operands[0];
    auto lhs_operand_iter = graph_info->id_to_operand_map.find(lhs_operand_id);
    ASSERT_TRUE(lhs_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(lhs_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(lhs_operand_iter->value->data_type,
              mojo::BlinkOperandTypeToMojo(lhs.data_type));
    EXPECT_EQ(lhs_operand_iter->value->dimensions, lhs.dimensions);
    EXPECT_EQ(lhs_operand_iter->value->name, "lhs");
    // Verify the right `mojo::Operand`.
    auto rhs_operand_id = graph_info->input_operands[1];
    auto rhs_operand_iter = graph_info->id_to_operand_map.find(rhs_operand_id);
    ASSERT_TRUE(rhs_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(rhs_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(rhs_operand_iter->value->data_type,
              mojo::BlinkOperandTypeToMojo(rhs.data_type));
    EXPECT_EQ(rhs_operand_iter->value->dimensions, rhs.dimensions);
    EXPECT_EQ(rhs_operand_iter->value->name, "rhs");
    // Verify the output `mojo::Operand`.
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kOutput);
    EXPECT_EQ(output_operand_iter->value->data_type, expected_out.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions, expected_out.dimensions);
    EXPECT_EQ(output_operand_iter->value->name, "output");
    // Verify the `mojo::Operator`.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_TRUE(operation->is_element_wise_binary());
    auto& binary_mojo = operation->get_element_wise_binary();

    blink_mojom::ElementWiseBinary::Kind binary_kind;
    switch (kind) {
      case webnn::mojom::blink::ElementWiseBinary::Kind::kAdd:
        binary_kind = blink_mojom::ElementWiseBinary::Kind::kAdd;
        break;
      case webnn::mojom::blink::ElementWiseBinary::Kind::kSub:
        binary_kind = blink_mojom::ElementWiseBinary::Kind::kSub;
        break;
      case webnn::mojom::blink::ElementWiseBinary::Kind::kMul:
        binary_kind = blink_mojom::ElementWiseBinary::Kind::kMul;
        break;
      case webnn::mojom::blink::ElementWiseBinary::Kind::kDiv:
        binary_kind = blink_mojom::ElementWiseBinary::Kind::kDiv;
        break;
      case webnn::mojom::blink::ElementWiseBinary::Kind::kMin:
        binary_kind = blink_mojom::ElementWiseBinary::Kind::kMin;
        break;
      case webnn::mojom::blink::ElementWiseBinary::Kind::kMax:
        binary_kind = blink_mojom::ElementWiseBinary::Kind::kMax;
        break;
      case webnn::mojom::blink::ElementWiseBinary::Kind::kPow:
        binary_kind = blink_mojom::ElementWiseBinary::Kind::kPow;
        break;
      case webnn::mojom::blink::ElementWiseBinary::Kind::kEqual:
        binary_kind = blink_mojom::ElementWiseBinary::Kind::kEqual;
        break;
      case webnn::mojom::blink::ElementWiseBinary::Kind::kGreater:
        binary_kind = blink_mojom::ElementWiseBinary::Kind::kGreater;
        break;
      case webnn::mojom::blink::ElementWiseBinary::Kind::kGreaterOrEqual:
        binary_kind = blink_mojom::ElementWiseBinary::Kind::kGreaterOrEqual;
        break;
      case webnn::mojom::blink::ElementWiseBinary::Kind::kLesser:
        binary_kind = blink_mojom::ElementWiseBinary::Kind::kLesser;
        break;
      case webnn::mojom::blink::ElementWiseBinary::Kind::kLesserOrEqual:
        binary_kind = blink_mojom::ElementWiseBinary::Kind::kLesserOrEqual;
        break;
    }
    EXPECT_EQ(binary_mojo->kind, binary_kind);
    EXPECT_EQ(binary_mojo->lhs_operand_id, lhs_operand_id);
    EXPECT_EQ(binary_mojo->rhs_operand_id, rhs_operand_id);
    EXPECT_EQ(binary_mojo->output_operand_id, output_operand_id);
  }
};

TEST_P(MLGraphTestMojo, ElementWiseBinaryLogicalTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());

  {
    // Test element-wise operators for two 0-D scalars.
    ElementWiseBinaryTester{
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {}},
        .expected_out = {.data_type = blink_mojom::Operand::DataType::kUint8,
                         .dimensions = {}}}
        .TestLogicalComparison(*this, scope, builder);
  }
  {
    // Test element-wise operators for two 1-D tensors.
    ElementWiseBinaryTester{
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {2}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {2}},
        .expected_out = {.data_type = blink_mojom::Operand::DataType::kUint8,
                         .dimensions = {2}}}
        .TestLogicalComparison(*this, scope, builder);
  }
  {
    // Test element-wise operators for two 2-D tensors.
    ElementWiseBinaryTester{
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                .dimensions = {3, 7}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                .dimensions = {3, 7}},
        .expected_out = {.data_type = blink_mojom::Operand::DataType::kUint8,
                         .dimensions = {3, 7}}}
        .TestLogicalComparison(*this, scope, builder);
  }
  {
    // Test element-wise operators for broadcasting to 2-D tensor.
    ElementWiseBinaryTester{
        .lhs = {.data_type = V8MLOperandDataType::Enum::kInt32,
                .dimensions = {5, 3}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kInt32,
                .dimensions = {5, 1}},
        .expected_out = {.data_type = blink_mojom::Operand::DataType::kUint8,
                         .dimensions = {5, 3}}}
        .TestLogicalComparison(*this, scope, builder);
  }
  {
    // Test element-wise operators for broadcasting to 3-D tensor.
    ElementWiseBinaryTester{
        .lhs = {.data_type = V8MLOperandDataType::Enum::kInt8,
                .dimensions = {4, 2, 1}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kInt8,
                .dimensions = {4}},
        .expected_out = {.data_type = blink_mojom::Operand::DataType::kUint8,
                         .dimensions = {4, 2, 4}}}
        .TestLogicalComparison(*this, scope, builder);
  }
  {
    // Test element-wise operators for broadcasting to 4-D tensors.
    ElementWiseBinaryTester{
        .lhs = {.data_type = V8MLOperandDataType::Enum::kUint8,
                .dimensions = {8, 1, 6, 1}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kUint8,
                .dimensions = {7, 1, 5}},
        .expected_out = {.data_type = blink_mojom::Operand::DataType::kUint8,
                         .dimensions = {8, 7, 6, 5}}}
        .TestLogicalComparison(*this, scope, builder);
  }
}

TEST_P(MLGraphTestMojo, ElementWiseBinaryTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test element-wise operators for two 0-D scalars.
    ElementWiseBinaryTester{
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {}},
        .expected_out = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                         .dimensions = {}}}
        .Test(*this, scope, builder);
  }
  {
    // Test element-wise operators for two 1-D tensors.
    ElementWiseBinaryTester{
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {2}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {2}},
        .expected_out = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                         .dimensions = {2}}}
        .Test(*this, scope, builder);
  }
  {
    // Test element-wise operators for two 2-D tensors.
    ElementWiseBinaryTester{
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                .dimensions = {3, 7}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                .dimensions = {3, 7}},
        .expected_out = {.data_type = blink_mojom::Operand::DataType::kFloat16,
                         .dimensions = {3, 7}}}
        .Test(*this, scope, builder);
  }
  {
    // Test element-wise operators for broadcasting to 2-D tensor.
    ElementWiseBinaryTester{
        .lhs = {.data_type = V8MLOperandDataType::Enum::kInt32,
                .dimensions = {5, 3}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kInt32,
                .dimensions = {5, 1}},
        .expected_out = {.data_type = blink_mojom::Operand::DataType::kInt32,
                         .dimensions = {5, 3}}}
        .Test(*this, scope, builder);
  }
  {
    // Test element-wise operators for broadcasting to 3-D tensor.
    ElementWiseBinaryTester{
        .lhs = {.data_type = V8MLOperandDataType::Enum::kInt8,
                .dimensions = {4, 2, 1}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kInt8,
                .dimensions = {4}},
        .expected_out = {.data_type = blink_mojom::Operand::DataType::kInt8,
                         .dimensions = {4, 2, 4}}}
        .Test(*this, scope, builder);
  }
  {
    // Test element-wise operators for broadcasting to 4-D tensors.
    ElementWiseBinaryTester{
        .lhs = {.data_type = V8MLOperandDataType::Enum::kUint8,
                .dimensions = {8, 1, 6, 1}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kUint8,
                .dimensions = {7, 1, 5}},
        .expected_out = {.data_type = blink_mojom::Operand::DataType::kUint8,
                         .dimensions = {8, 7, 6, 5}}}
        .Test(*this, scope, builder);
  }
}

struct SoftmaxTester {
  OperandInfoBlink input;
  OperandInfoMojo expected;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        builder->softmax(input_operand, scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_TRUE(operation->is_softmax());
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type, expected.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions, expected.dimensions);
  }
};

TEST_P(MLGraphTestMojo, SoftmaxTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test building softmax with float32 input.
    SoftmaxTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 4}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {2, 4}}}
        .Test(*this, scope, builder);
  }
  {
    // Test building softmax with float16 input.
    SoftmaxTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {1, 5}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat16,
                     .dimensions = {1, 5}}}
        .Test(*this, scope, builder);
  }
}

template <typename T>
struct ConstantTester {
  OperandInfo<T> constant;
  OperandInfoMojo expected;
  Vector<T> expected_constant_data;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* constant_operand =
        BuildConstant(builder, constant.dimensions, constant.data_type,
                      constant.values, scope.GetExceptionState());
    auto* output_operand =
        builder->relu(constant_operand, scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    EXPECT_EQ(graph_info->id_to_operand_map.size(), 2u);
    EXPECT_EQ(graph_info->constant_id_to_buffer_map.size(), 1u);
    // Verify the constant `mojo::Operand`.
    for (auto& [constant_id, constant_buffer] :
         graph_info->constant_id_to_buffer_map) {
      auto constant_operand_iter =
          graph_info->id_to_operand_map.find(constant_id);
      ASSERT_TRUE(constant_operand_iter != graph_info->id_to_operand_map.end());
      EXPECT_EQ(constant_operand_iter->value->kind,
                blink_mojom::Operand::Kind::kConstant);
      EXPECT_EQ(constant_operand_iter->value->data_type, expected.data_type);
      EXPECT_EQ(constant_operand_iter->value->dimensions, expected.dimensions);
      EXPECT_TRUE(constant_operand_iter->value->name.empty());
      // Verify the constant data in the mojo.
      const wtf_size_t constant_size =
          base::checked_cast<wtf_size_t>(constant_buffer.size() / sizeof(T));
      Vector<T> constant_data(constant_size);
      memcpy(constant_data.data(), constant_buffer.data(),
             constant_buffer.size());
      EXPECT_EQ(expected_constant_data, constant_data);
    }
  }
};

TEST_P(MLGraphTestMojo, ConstantTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test scalar constant operand.
    ConstantTester<float>{
        .constant = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {},
                     .values = {1.0}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {}},
        .expected_constant_data = {1.0}}
        .Test(*this, scope, builder);
  }
  {
    // Test Constant operand for Float32 data type.
    ConstantTester<float>{
        .constant = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {2, 3},
                     .values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {2, 3}},
        .expected_constant_data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0}}
        .Test(*this, scope, builder);
  }
  {
    // Test Constant operand for Float16 data type.
    ConstantTester<uint16_t>{
        .constant = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                     .dimensions = {2, 3},
                     .values = {1, 2, 3, 4, 5, 6}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat16,
                     .dimensions = {2, 3}},
        .expected_constant_data = {1, 2, 3, 4, 5, 6}}
        .Test(*this, scope, builder);
  }
  {
    // Test Constant operand for Int32 data type.
    ConstantTester<int32_t>{
        .constant = {.data_type = V8MLOperandDataType::Enum::kInt32,
                     .dimensions = {2, 3},
                     .values = {1, 2, 3, 4, 5, 6}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kInt32,
                     .dimensions = {2, 3}},
        .expected_constant_data = {1, 2, 3, 4, 5, 6}}
        .Test(*this, scope, builder);
  }
  {
    // Test Constant operand for Int8 data type.
    ConstantTester<int8_t>{
        .constant = {.data_type = V8MLOperandDataType::Enum::kInt8,
                     .dimensions = {2, 3},
                     .values = {1, 2, 3, 4, 5, 6}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kInt8,
                     .dimensions = {2, 3}},
        .expected_constant_data = {1, 2, 3, 4, 5, 6}}
        .Test(*this, scope, builder);
  }
}

struct CastTester {
  OperandInfoBlink input;
  V8MLOperandDataType::Enum output_data_type;
  OperandInfoMojo expected_operand;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        builder->cast(input_operand, V8MLOperandDataType(output_data_type),
                      scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_TRUE(operation->is_element_wise_unary());
    webnn::mojom::blink::ElementWiseUnaryPtr& element_wise_unary =
        operation->get_element_wise_unary();
    EXPECT_EQ(element_wise_unary->kind,
              blink_mojom::ElementWiseUnary::Kind::kCast);
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
  }
};

TEST_P(MLGraphTestMojo, CastTester) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  {
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kInt32,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kFloat16,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kUint32,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kUint32,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kInt8,
        .expected_operand = {.data_type = blink_mojom::Operand::DataType::kInt8,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kUint8,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kUint8,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kFloat32,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kInt32,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kUint32,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kUint32,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kInt8,
        .expected_operand = {.data_type = blink_mojom::Operand::DataType::kInt8,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kUint8,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kUint8,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kFloat32,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kFloat16,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kUint32,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kUint32,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kInt8,
        .expected_operand = {.data_type = blink_mojom::Operand::DataType::kInt8,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kUint8,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kUint8,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kFloat32,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kFloat16,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kInt32,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kInt8,
        .expected_operand = {.data_type = blink_mojom::Operand::DataType::kInt8,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kUint8,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kUint8,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kInt8,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kFloat32,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kInt8,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kFloat16,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt8,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kUint32,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kUint32,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt8,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kUint8,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kUint8,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt8,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kInt32,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kFloat32,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kFloat16,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kInt32,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kInt8,
        .expected_operand = {.data_type = blink_mojom::Operand::DataType::kInt8,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kInt32,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
  }
}

struct ArgMinMaxTester {
  OperandInfoBlink input;
  std::optional<Vector<uint32_t>> axes;
  std::optional<bool> keep_dimensions;
  std::optional<bool> select_last_index;
  OperandInfoMojo expected_input;
  OperandInfoMojo expected_output;
  Vector<uint32_t> expected_axes;
  bool expected_keep_dimensions;
  bool expected_select_last_index;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    Test(helper, scope, builder, webnn::mojom::blink::ArgMinMax::Kind::kMin);
    Test(helper, scope, builder, webnn::mojom::blink::ArgMinMax::Kind::kMax);
  }

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder,
            webnn::mojom::blink::ArgMinMax::Kind kind) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* options = MLArgMinMaxOptions::Create();
    if (axes.has_value()) {
      options->setAxes(axes.value());
    }
    if (keep_dimensions.has_value()) {
      options->setKeepDimensions(keep_dimensions.value());
    }
    if (select_last_index.has_value()) {
      options->setSelectLastIndex(select_last_index.value());
    }
    auto* output_operand =
        BuildArgMinMax(scope, builder, kind, input_operand, options);
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    ASSERT_TRUE(operation->is_arg_min_max());
    auto& argminmax = operation->get_arg_min_max();

    blink_mojom::ArgMinMax::Kind mojom_kind;
    switch (kind) {
      case webnn::mojom::blink::ArgMinMax::Kind::kMin:
        mojom_kind = blink_mojom::ArgMinMax::Kind::kMin;
        break;
      case webnn::mojom::blink::ArgMinMax::Kind::kMax:
        mojom_kind = blink_mojom::ArgMinMax::Kind::kMax;
        break;
    }
    EXPECT_EQ(argminmax->kind, mojom_kind);
    // Validate the axes of ArgMinMax operation.
    EXPECT_EQ(argminmax->axes, expected_axes);
    // Validate the keep_dimensions of ArgMinMax operation.
    EXPECT_EQ(argminmax->keep_dimensions, expected_keep_dimensions);
    // Validate the select_last_index of ArgMinMax operation.
    EXPECT_EQ(argminmax->select_last_index, expected_select_last_index);

    // Validate the input operand.
    EXPECT_EQ(graph_info->input_operands.size(), 1u);
    auto input_operand_id = graph_info->input_operands[0];
    EXPECT_EQ(argminmax->input_operand_id, input_operand_id);
    auto input_operand_iter =
        graph_info->id_to_operand_map.find(input_operand_id);
    ASSERT_TRUE(input_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(input_operand_iter->value->data_type, expected_input.data_type);
    EXPECT_EQ(input_operand_iter->value->dimensions, expected_input.dimensions);

    // Validate the output operand.
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    EXPECT_EQ(argminmax->output_operand_id, output_operand_id);
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type, expected_output.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_output.dimensions);
  }
};

TEST_P(MLGraphTestMojo, ArgMinMaxTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test argMinMax with default options.
    ArgMinMaxTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 3, 4}},
        .expected_input = {.data_type =
                               blink_mojom::Operand::DataType::kFloat32,
                           .dimensions = {1, 2, 3, 4}},
        .expected_output = {.data_type = blink_mojom::Operand::DataType::kInt64,
                            .dimensions = {}},
        .expected_axes = {0, 1, 2, 3},
        .expected_keep_dimensions = false,
        .expected_select_last_index = false}
        .Test(*this, scope, builder);
  }
  {
    // Test argMinMax with axes = {}.
    ArgMinMaxTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 3, 4}},
        .axes = Vector<uint32_t>{},
        .expected_input = {.data_type =
                               blink_mojom::Operand::DataType::kFloat32,
                           .dimensions = {1, 2, 3, 4}},
        .expected_output = {.data_type = blink_mojom::Operand::DataType::kInt64,
                            .dimensions = {1, 2, 3, 4}},
        .expected_axes = {},
        .expected_keep_dimensions = false,
        .expected_select_last_index = false}
        .Test(*this, scope, builder);
  }
  {
    // Test argMinMax with axes = {1}.
    ArgMinMaxTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 3, 4}},
        .axes = Vector<uint32_t>{1},
        .expected_input = {.data_type =
                               blink_mojom::Operand::DataType::kFloat32,
                           .dimensions = {1, 2, 3, 4}},
        .expected_output = {.data_type = blink_mojom::Operand::DataType::kInt64,
                            .dimensions = {1, 3, 4}},
        .expected_axes = {1},
        .expected_keep_dimensions = false,
        .expected_select_last_index = false}
        .Test(*this, scope, builder);
  }
  {
    // Test argMinMax with axes = {1, 3} and keepDimensions = true.
    ArgMinMaxTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 3, 4}},
        .axes = Vector<uint32_t>{1, 3},
        .keep_dimensions = true,
        .expected_input = {.data_type =
                               blink_mojom::Operand::DataType::kFloat32,
                           .dimensions = {1, 2, 3, 4}},
        .expected_output = {.data_type = blink_mojom::Operand::DataType::kInt64,
                            .dimensions = {1, 1, 3, 1}},
        .expected_axes = {1, 3},
        .expected_keep_dimensions = true,
        .expected_select_last_index = false}
        .Test(*this, scope, builder);
  }
  {
    // Test argMinMax with axes = {1, 3}, keepDimensions = true and and
    // selectLastIndex = true.
    ArgMinMaxTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 3, 4}},
        .axes = Vector<uint32_t>{1, 3},
        .keep_dimensions = true,
        .select_last_index = true,
        .expected_input = {.data_type =
                               blink_mojom::Operand::DataType::kFloat32,
                           .dimensions = {1, 2, 3, 4}},
        .expected_output = {.data_type = blink_mojom::Operand::DataType::kInt64,
                            .dimensions = {1, 1, 3, 1}},
        .expected_axes = {1, 3},
        .expected_keep_dimensions = true,
        .expected_select_last_index = true}
        .Test(*this, scope, builder);
  }
}

TEST_P(MLGraphTestMojo, WebNNGraphComputeTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  const Vector<uint32_t> dimensions = {3, 5};
  const wtf_size_t number_of_elements = base::checked_cast<wtf_size_t>(
      webnn::ValidateAndCalculateElementsNumber(dimensions).value());

  // Build the graph.
  auto* lhs_operand =
      BuildInput(builder, "lhs", dimensions, V8MLOperandDataType::Enum::kUint8,
                 scope.GetExceptionState());
  auto* rhs_operand =
      BuildInput(builder, "rhs", dimensions, V8MLOperandDataType::Enum::kUint8,
                 scope.GetExceptionState());
  auto* output_operand = BuildElementWiseBinary(
      scope, builder, webnn::mojom::blink::ElementWiseBinary::Kind::kAdd,
      lhs_operand, rhs_operand);
  auto [graph, error_name, error_message] =
      BuildGraph(scope, builder, {{"output", output_operand}});
  ASSERT_THAT(graph, testing::NotNull());

  MLNamedArrayBufferViews inputs(
      {{"lhs", CreateArrayBufferViewForOperand(lhs_operand)},
       {"rhs", CreateArrayBufferViewForOperand(rhs_operand)}});
  MLNamedArrayBufferViews outputs(
      {{"output", CreateArrayBufferViewForOperand(output_operand)}});

  {
    // Compute successfully.
    SetComputeResult(ComputeResult{
        .output = {{"output", Vector<uint8_t>(number_of_elements, 2)}}});
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    auto results = GetArrayBufferViewValues<uint8_t>(outputs[0].second);
    EXPECT_EQ(results, Vector<uint8_t>(number_of_elements, 2));

    // Compute again successfully.
    SetComputeResult(ComputeResult{
        .output = {{"output", Vector<uint8_t>(number_of_elements, 7)}}});
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    results = GetArrayBufferViewValues<uint8_t>(outputs[0].second);
    EXPECT_EQ(results, Vector<uint8_t>(number_of_elements, 7));

    // Validate the input array buffers.
    auto& name_to_buffer_map = GetInputArrayBuffers();
    auto lhs_input_iter = name_to_buffer_map.find("lhs");
    EXPECT_NE(lhs_input_iter, name_to_buffer_map.end());
    EXPECT_EQ(lhs_input_iter->value.size(), number_of_elements);
    auto rhs_input_iter = name_to_buffer_map.find("rhs");
    EXPECT_NE(rhs_input_iter, name_to_buffer_map.end());
    EXPECT_EQ(rhs_input_iter->value.size(), number_of_elements);
  }
  {
    // Unknown error.
    SetComputeResult(ComputeResult{});
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "OperationError");
    EXPECT_EQ(error_message,
              "There is an unknown output tensor in the computation "
              "result: output");
  }
  {
    // Reset the inputs which are detached in above failed tests.
    inputs[0].second = CreateArrayBufferViewForOperand(lhs_operand);
    inputs[1].second = CreateArrayBufferViewForOperand(rhs_operand);
    outputs[0].second = CreateArrayBufferViewForOperand(output_operand);
    // Output name in computation result isn't expected.
    SetComputeResult(
        ComputeResult{.output = {{"a_different_out_name",
                                  Vector<uint8_t>(number_of_elements)}}});
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "OperationError");
    EXPECT_EQ(error_message,
              "There is an unknown output tensor in the computation "
              "result: output");
  }
  {
    // Reset the inputs which are detached in above failed tests.
    inputs[0].second = CreateArrayBufferViewForOperand(lhs_operand);
    inputs[1].second = CreateArrayBufferViewForOperand(rhs_operand);
    outputs[0].second = CreateArrayBufferViewForOperand(output_operand);
    // The size of output in computation result isn't expected.
    SetComputeResult(
        ComputeResult{.output = {{"output", Vector<uint8_t>(20)}}});
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "UnknownError");
    EXPECT_EQ(error_message,
              "The output tensor size does not match graph's expectation: "
              "output");
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         MLGraphTestMojo,
                         testing::Values(BackendType::kWebNNService),
                         TestParamInfoToString);

}  // namespace blink
