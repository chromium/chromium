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
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
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

struct ClampTester {
  OperandInfoBlink input;
  struct ClampOptions {
    std::optional<float> min_value;
    std::optional<float> max_value;
  };
  ClampOptions options;
  OperandInfoMojo expected_operand;
  ClampOptions expected_attributes;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    MLClampOptions* ml_clamp_options = MLClampOptions::Create();
    if (options.min_value) {
      ml_clamp_options->setMinValue(options.min_value.value());
    }
    if (options.max_value) {
      ml_clamp_options->setMaxValue(options.max_value.value());
    }
    auto* output_operand = builder->clamp(input_operand, ml_clamp_options,
                                          scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_TRUE(operation->is_clamp());
    auto& clamp = operation->get_clamp();
    EXPECT_EQ(clamp->min_value, expected_attributes.min_value);
    EXPECT_EQ(clamp->max_value, expected_attributes.max_value);
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

TEST_P(MLGraphTestMojo, ClampTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test clamp operator with default options that no minimum and maximum
    // values are defined.
    ClampTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 2, 2, 1}},
        .expected_attributes = {.min_value =
                                    -std::numeric_limits<float>::infinity(),
                                .max_value =
                                    +std::numeric_limits<float>::infinity()}}
        .Test(*this, scope, builder);
  }
  {
    // Test clamp operator with the minimum value defined.
    ClampTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {2, 4}},
        .options = {0.0, std::nullopt},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {2, 4}},
        .expected_attributes = {.min_value = 0.0,
                                .max_value =
                                    +std::numeric_limits<float>::infinity()}}
        .Test(*this, scope, builder);
  }
  {
    // Test clamp operator with the maximum value defined.
    ClampTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                  .dimensions = {3, 1, 6}},
        .options = {std::nullopt, 6.0},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kInt32,
                             .dimensions = {3, 1, 6}},
        .expected_attributes = {.min_value =
                                    -std::numeric_limits<float>::infinity(),
                                .max_value = 6.0}}
        .Test(*this, scope, builder);
  }
  {
    // Test clamp operator with both the minimum and maximum values defined.
    ClampTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                          .dimensions = {7}},
                .options = {0.0, 6.0},
                .expected_operand = {.data_type =
                                         blink_mojom::Operand::DataType::kUint8,
                                     .dimensions = {7}},
                .expected_attributes = {.min_value = 0.0, .max_value = 6.0}}
        .Test(*this, scope, builder);
  }
  {
    // Test clamp operator with scalar.
    ClampTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                          .dimensions = {}},
                .options = {0.0, 6.0},
                .expected_operand = {.data_type =
                                         blink_mojom::Operand::DataType::kUint8,
                                     .dimensions = {}},
                .expected_attributes = {.min_value = 0.0, .max_value = 6.0}}
        .Test(*this, scope, builder);
  }
}

// TODO: crbug.com/325598628 - Consider replacing this with direct use of the
// mojo Activation struct.
struct Activation {
  webnn::mojom::blink::Activation::Tag kind;
  std::optional<ClampTester::ClampOptions> clamp_options;
  std::optional<float> hard_sigmoid_alpha;
  std::optional<float> hard_sigmoid_beta;
  std::optional<float> elu_alpha;
  std::optional<float> leaky_relu_alpha;
  std::optional<float> linear_alpha;
  std::optional<float> linear_beta;
  std::optional<float> softplus_steepness;
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
    case webnn::mojom::blink::Activation::Tag::kSoftplus: {
      auto* softplus_options = MLSoftplusOptions::Create();
      CHECK(softplus_options);
      if (activation.softplus_steepness.has_value()) {
        softplus_options->setSteepness(activation.softplus_steepness.value());
      }
      return builder->softplus(softplus_options, scope.GetExceptionState());
    }
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
      ASSERT_TRUE(mojom_activation->is_softplus());
      auto& softplus = mojom_activation->get_softplus();
      CHECK(softplus);
      CHECK(expected_activation.softplus_steepness.has_value());
      EXPECT_EQ(softplus->steepness,
                expected_activation.softplus_steepness.value());
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
                            .clamp_options =
                                ClampTester::ClampOptions{.min_value = 1.0,
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
                         ClampTester::ClampOptions{.min_value = 1.0,
                                                   .max_value = 6.0}}}}
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
    // Test batchNormalization with softplus activation with default options.
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
                     .kind = webnn::mojom::blink::Activation::Tag::kSoftplus,
                     .softplus_steepness = 1.0}}}
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

struct Conv2dTester {
  OperandInfoBlink input;
  OperandInfoBlink filter;
  struct Conv2dOptions {
    std::optional<Vector<uint32_t>> padding;
    std::optional<Vector<uint32_t>> strides;
    std::optional<Vector<uint32_t>> dilations;
    std::optional<uint32_t> groups;
    std::optional<blink::V8MLInputOperandLayout::Enum> input_layout;
    std::optional<blink::V8MLConv2dFilterOperandLayout::Enum> filter_layout;
    std::optional<OperandInfoBlink> bias;
    std::optional<Activation> activation;
  };
  struct Conv2dAttributes {
    Vector<uint32_t> padding = {0, 0, 0, 0};
    Vector<uint32_t> strides = {1, 1};
    Vector<uint32_t> dilations = {1, 1};
    uint32_t groups = 1;
    blink_mojom::InputOperandLayout input_layout =
        blink_mojom::InputOperandLayout::kChannelsFirst;
    std::optional<OperandInfoMojo> bias;
    std::optional<Activation> activation;
  };
  Conv2dOptions options;
  OperandInfoMojo expected_operand;
  Conv2dAttributes expected_attributes;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* filter_operand =
        BuildInput(builder, "filter", filter.dimensions, filter.data_type,
                   scope.GetExceptionState());
    MLConv2dOptions* ml_conv2d_options = MLConv2dOptions::Create();
    if (options.padding) {
      ml_conv2d_options->setPadding(options.padding.value());
    }
    if (options.strides) {
      ml_conv2d_options->setStrides(options.strides.value());
    }
    if (options.dilations) {
      ml_conv2d_options->setDilations(options.dilations.value());
    }
    if (options.groups) {
      ml_conv2d_options->setGroups(options.groups.value());
    }
    if (options.input_layout) {
      ml_conv2d_options->setInputLayout(options.input_layout.value());
    }
    if (options.filter_layout) {
      ml_conv2d_options->setFilterLayout(options.filter_layout.value());
    }
    if (options.bias) {
      ml_conv2d_options->setBias(
          BuildInput(builder, "bias", options.bias->dimensions,
                     options.bias->data_type, scope.GetExceptionState()));
    }
    if (options.activation) {
      auto* activation =
          CreateActivation(scope, builder, options.activation.value());
      CHECK(activation);
      ml_conv2d_options->setActivation(activation);
    }
    auto* output_operand =
        builder->conv2d(input_operand, filter_operand, ml_conv2d_options,
                        scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_TRUE(operation->is_conv2d());
    auto& conv2d = operation->get_conv2d();
    // Validate explicit padding.
    auto& expected_padding = expected_attributes.padding;
    EXPECT_EQ(conv2d->padding->beginning->height, expected_padding[0]);
    EXPECT_EQ(conv2d->padding->ending->height, expected_padding[1]);
    EXPECT_EQ(conv2d->padding->beginning->width, expected_padding[2]);
    EXPECT_EQ(conv2d->padding->ending->width, expected_padding[3]);
    // Validate strides
    EXPECT_EQ(conv2d->strides->height, expected_attributes.strides[0]);
    EXPECT_EQ(conv2d->strides->width, expected_attributes.strides[1]);
    // Validate dilations.
    EXPECT_EQ(conv2d->dilations->height, expected_attributes.dilations[0]);
    EXPECT_EQ(conv2d->dilations->width, expected_attributes.dilations[1]);
    EXPECT_EQ(conv2d->groups, expected_attributes.groups);
    EXPECT_EQ(conv2d->input_layout, expected_attributes.input_layout);
    if (options.bias) {
      auto bias_operand_iter =
          graph_info->id_to_operand_map.find(conv2d->bias_operand_id.value());
      ASSERT_TRUE(bias_operand_iter != graph_info->id_to_operand_map.end());
      EXPECT_EQ(bias_operand_iter->value->data_type,
                expected_attributes.bias->data_type);
      EXPECT_EQ(bias_operand_iter->value->dimensions,
                expected_attributes.bias->dimensions);
    }
    if (options.activation) {
      CHECK(expected_attributes.activation);
      CheckActivation(conv2d->activation,
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

TEST_P(MLGraphTestMojo, Conv2dTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test conv2d with default options.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes = {.padding = {0, 0, 0, 0},
                                .strides = {1, 1},
                                .dilations = {1, 1},
                                .groups = 1}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with strides=2 and padding=1.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options = {.padding = Vector<uint32_t>({1, 1, 1, 1}),
                    .strides = Vector<uint32_t>({2, 2})},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes = {.padding = {1, 1, 1, 1},
                                .strides = {2, 2},
                                .dilations = {1, 1},
                                .groups = 1}}
        .Test(*this, scope, builder);
  }
  {
    // Test depthwise conv2d by setting groups to input channels.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 4, 2, 2}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {4, 1, 2, 2}},
        .options = {.groups = 4},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 4, 1, 1}},
        .expected_attributes = {.padding = {0, 0, 0, 0},
                                .strides = {1, 1},
                                .dilations = {1, 1},
                                .groups = 4}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with clamp activation.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options = {.activation =
                        Activation{
                            .kind =
                                webnn::mojom::blink::Activation::Tag::kClamp,
                            .clamp_options =
                                ClampTester::ClampOptions{.min_value = 1.0,
                                                          .max_value = 6.0}}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes =
            {.padding = Vector<uint32_t>({0, 0, 0, 0}),
             .strides = Vector<uint32_t>({1, 1}),
             .dilations = Vector<uint32_t>({1, 1}),
             .groups = 1,
             .activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kClamp,
                     .clamp_options =
                         ClampTester::ClampOptions{.min_value = 1.0,
                                                   .max_value = 6.0}}}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with elu activation with default options.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options = {.activation =
                        Activation{
                            .kind =
                                webnn::mojom::blink::Activation::Tag::kElu}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes =
            {.padding = Vector<uint32_t>({0, 0, 0, 0}),
             .strides = Vector<uint32_t>({1, 1}),
             .dilations = Vector<uint32_t>({1, 1}),
             .groups = 1,
             .activation =
                 Activation{.kind = webnn::mojom::blink::Activation::Tag::kElu,
                            .elu_alpha = 1.0}}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with elu activation with given alpha.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options = {.activation =
                        Activation{
                            .kind = webnn::mojom::blink::Activation::Tag::kElu,
                            .elu_alpha = 0.5}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes =
            {.padding = Vector<uint32_t>({0, 0, 0, 0}),
             .strides = Vector<uint32_t>({1, 1}),
             .dilations = Vector<uint32_t>({1, 1}),
             .groups = 1,
             .activation =
                 Activation{.kind = webnn::mojom::blink::Activation::Tag::kElu,
                            .elu_alpha = 0.5}}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with hardSigmoid activation with default alpha = 0.1 and beta
    // = -1.0.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options =
            {.activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kHardSigmoid,
                     .hard_sigmoid_alpha = 0.1,
                     .hard_sigmoid_beta = -1.0}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes =
            {.padding = Vector<uint32_t>({0, 0, 0, 0}),
             .strides = Vector<uint32_t>({1, 1}),
             .dilations = Vector<uint32_t>({1, 1}),
             .groups = 1,
             .activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kHardSigmoid,
                     .hard_sigmoid_alpha = 0.1,
                     .hard_sigmoid_beta = -1.0}}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with leaky relu activation with default options.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options =
            {.activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kLeakyRelu}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes =
            {.padding = Vector<uint32_t>({0, 0, 0, 0}),
             .strides = Vector<uint32_t>({1, 1}),
             .dilations = Vector<uint32_t>({1, 1}),
             .groups = 1,
             .activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kLeakyRelu,
                     .leaky_relu_alpha = 0.01}}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with leaky relu activation with given alpha.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options =
            {.activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kLeakyRelu,
                     .leaky_relu_alpha = 0.02}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes =
            {.padding = Vector<uint32_t>({0, 0, 0, 0}),
             .strides = Vector<uint32_t>({1, 1}),
             .dilations = Vector<uint32_t>({1, 1}),
             .groups = 1,
             .activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kLeakyRelu,
                     .leaky_relu_alpha = 0.02}}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with relu activation.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options = {.activation =
                        Activation{
                            .kind =
                                webnn::mojom::blink::Activation::Tag::kRelu}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes =
            {.padding = Vector<uint32_t>({0, 0, 0, 0}),
             .strides = Vector<uint32_t>({1, 1}),
             .dilations = Vector<uint32_t>({1, 1}),
             .groups = 1,
             .activation =
                 Activation{.kind =
                                webnn::mojom::blink::Activation::Tag::kRelu}}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with sigmoid activation.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options =
            {.activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kSigmoid}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes =
            {.padding = Vector<uint32_t>({0, 0, 0, 0}),
             .strides = Vector<uint32_t>({1, 1}),
             .dilations = Vector<uint32_t>({1, 1}),
             .groups = 1,
             .activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kSigmoid}}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with softmax activation.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                   .dimensions = {1, 1, 3, 3}},
        .options =
            {.activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kSoftmax}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes =
            {.padding = Vector<uint32_t>({0, 0, 0, 0}),
             .strides = Vector<uint32_t>({1, 1}),
             .dilations = Vector<uint32_t>({1, 1}),
             .groups = 1,
             .activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kSoftmax}}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with softplus activation with steepness = 2.0.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                   .dimensions = {1, 1, 3, 3}},
        .options = {.activation =
                        Activation{
                            .kind =
                                webnn::mojom::blink::Activation::Tag::kSoftplus,
                            .softplus_steepness = 2.0}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes =
            {.padding = Vector<uint32_t>({0, 0, 0, 0}),
             .strides = Vector<uint32_t>({1, 1}),
             .dilations = Vector<uint32_t>({1, 1}),
             .groups = 1,
             .activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kSoftplus,
                     .softplus_steepness = 2.0}}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with softsign activation.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                   .dimensions = {1, 1, 3, 3}},
        .options =
            {.activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kSoftsign}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes =
            {.padding = Vector<uint32_t>({0, 0, 0, 0}),
             .strides = Vector<uint32_t>({1, 1}),
             .dilations = Vector<uint32_t>({1, 1}),
             .groups = 1,
             .activation =
                 Activation{
                     .kind = webnn::mojom::blink::Activation::Tag::kSoftsign}}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with tanh activation.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options = {.activation =
                        Activation{
                            .kind =
                                webnn::mojom::blink::Activation::Tag::kTanh}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes =
            {.padding = Vector<uint32_t>({0, 0, 0, 0}),
             .strides = Vector<uint32_t>({1, 1}),
             .dilations = Vector<uint32_t>({1, 1}),
             .groups = 1,
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

struct EluTester {
  OperandInfoBlink input;
  std::optional<float> alpha;
  OperandInfoMojo expected_operand;
  float expected_alpha;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    MLEluOptions* ml_elu_options = MLEluOptions::Create();
    if (alpha) {
      ml_elu_options->setAlpha(alpha.value());
    }
    auto* output_operand =
        builder->elu(input_operand, ml_elu_options, scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo is as expected.
    ASSERT_EQ(graph_info->id_to_operand_map.size(), 2u);

    // Verify the input `mojo::Operand`.
    ASSERT_EQ(graph_info->input_operands.size(), 1u);
    auto input_operand_id = graph_info->input_operands[0];
    auto input_operand_iter =
        graph_info->id_to_operand_map.find(input_operand_id);
    ASSERT_TRUE(input_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(input_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(input_operand_iter->value->data_type, expected_operand.data_type);
    EXPECT_EQ(input_operand_iter->value->dimensions, input.dimensions);
    EXPECT_EQ(input_operand_iter->value->name, "input");

    // Verify the output `mojo::Operand`.
    ASSERT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kOutput);
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
    EXPECT_EQ(output_operand_iter->value->name, "output");

    // Verify the `mojo::Operator`.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    ASSERT_TRUE(operation->is_elu());
    auto& elu = operation->get_elu();
    EXPECT_EQ(elu->input_operand_id, input_operand_id);
    EXPECT_EQ(elu->output_operand_id, output_operand_id);
    EXPECT_EQ(elu->alpha, expected_alpha);
  }
};

TEST_P(MLGraphTestMojo, EluTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test elu operator for 0-D tensor with default options.
    EluTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                        .dimensions = {}},
              .expected_operand = {.data_type =
                                       blink_mojom::Operand::DataType::kFloat32,
                                   .dimensions = {}},
              .expected_alpha = 1}
        .Test(*this, scope, builder);
  }
  {
    // Test elu operator for 1-D tensor with default options.
    EluTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                        .dimensions = {2}},
              .expected_operand = {.data_type =
                                       blink_mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2}},
              .expected_alpha = 1}
        .Test(*this, scope, builder);
  }
  {
    // Test elu operator for 2-D tensor with default options.
    EluTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                        .dimensions = {3, 7}},
              .expected_operand = {.data_type =
                                       blink_mojom::Operand::DataType::kFloat16,
                                   .dimensions = {3, 7}},
              .expected_alpha = 1}
        .Test(*this, scope, builder);
  }
  {
    // Test elu operator for 3-D tensor with given alpha.
    EluTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                        .dimensions = {1, 5, 3}},
              .alpha = 0.5,
              .expected_operand = {.data_type =
                                       blink_mojom::Operand::DataType::kFloat32,
                                   .dimensions = {1, 5, 3}},
              .expected_alpha = 0.5}
        .Test(*this, scope, builder);
  }
  {
    // Test elu operator for 4-D tensor with given alpha.
    EluTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                        .dimensions = {1, 2, 2, 1}},
              .alpha = 0.7,
              .expected_operand = {.data_type =
                                       blink_mojom::Operand::DataType::kFloat16,
                                   .dimensions = {1, 2, 2, 1}},
              .expected_alpha = 0.7}
        .Test(*this, scope, builder);
  }
}

struct ExpandTester {
  OperandInfoBlink input;
  Vector<uint32_t> new_shape;
  OperandInfoMojo expected;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        builder->expand(input_operand, new_shape, scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_TRUE(operation->is_expand());
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type, expected.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions, expected.dimensions);
  }
};

TEST_P(MLGraphTestMojo, ExpandTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test building expand 0-D scalar to 3-D tensor.
    ExpandTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {}},
        .new_shape = {3, 4, 5},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {3, 4, 5}}}
        .Test(*this, scope, builder);
  }
  {
    // Test expanding the new shape that is the same as input.
    ExpandTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {3, 2}},
        .new_shape = {3, 2},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {3, 2}}}
        .Test(*this, scope, builder);
  }
  {
    // Test expanding the new shape that are broadcastable.
    ExpandTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {3, 1, 5}},
        .new_shape = {3, 4, 5},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat16,
                     .dimensions = {3, 4, 5}}}
        .Test(*this, scope, builder);
  }
  {
    // Test expanding the new shape that are broadcastable and the number of new
    // shapes larger than input.
    ExpandTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                  .dimensions = {2, 5}},
        .new_shape = {3, 2, 5},
        .expected = {.data_type = blink_mojom::Operand::DataType::kInt32,
                     .dimensions = {3, 2, 5}}}
        .Test(*this, scope, builder);
  }
}
struct GemmTester {
  OperandInfoBlink a;
  OperandInfoBlink b;
  struct GemmOptions {
    std::optional<OperandInfoBlink> c;
    std::optional<float> alpha;
    std::optional<float> beta;
    std::optional<bool> a_transpose;
    std::optional<bool> b_transpose;
  };
  GemmOptions options;
  OperandInfoMojo expected_operand;
  struct GemmAttributes {
    std::optional<OperandInfoMojo> c;
    float alpha;
    float beta;
    bool a_transpose;
    bool b_transpose;
  };
  GemmAttributes expected_attributes;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* a_operand = BuildInput(builder, "a", a.dimensions, a.data_type,
                                 scope.GetExceptionState());
    auto* b_operand = BuildInput(builder, "b", b.dimensions, b.data_type,
                                 scope.GetExceptionState());
    MLGemmOptions* ml_gemm_options = MLGemmOptions::Create();
    if (options.c) {
      ml_gemm_options->setC(BuildInput(builder, "c", options.c->dimensions,
                                       options.c->data_type,
                                       scope.GetExceptionState()));
    }
    if (options.alpha) {
      ml_gemm_options->setAlpha(options.alpha.value());
    }
    if (options.beta) {
      ml_gemm_options->setBeta(options.beta.value());
    }
    if (options.a_transpose) {
      ml_gemm_options->setATranspose(options.a_transpose.value());
    }
    if (options.b_transpose) {
      ml_gemm_options->setBTranspose(options.b_transpose.value());
    }
    auto* output_operand = builder->gemm(a_operand, b_operand, ml_gemm_options,
                                         scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_TRUE(operation->is_gemm());
    auto& gemm_mojo = operation->get_gemm();
    if (options.c) {
      auto c_operand_iter =
          graph_info->id_to_operand_map.find(gemm_mojo->c_operand_id.value());
      ASSERT_TRUE(c_operand_iter != graph_info->id_to_operand_map.end());
      EXPECT_EQ(c_operand_iter->value->data_type,
                expected_attributes.c->data_type);
      EXPECT_EQ(c_operand_iter->value->dimensions,
                expected_attributes.c->dimensions);
    } else {
      EXPECT_EQ(gemm_mojo->c_operand_id, std::nullopt);
    }
    EXPECT_EQ(gemm_mojo->alpha, expected_attributes.alpha);
    EXPECT_EQ(gemm_mojo->beta, expected_attributes.beta);
    EXPECT_EQ(gemm_mojo->a_transpose, expected_attributes.a_transpose);
    EXPECT_EQ(gemm_mojo->b_transpose, expected_attributes.b_transpose);
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

TEST_P(MLGraphTestMojo, GemmTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test building gemm with default option.
    GemmTester{
        .a = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {2, 3}},
        .b = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {3, 4}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 4}},
        .expected_attributes = {.c = std::nullopt,
                                .alpha = 1.0,
                                .beta = 1.0,
                                .a_transpose = false,
                                .b_transpose = false}}
        .Test(*this, scope, builder);
  }
  {
    // Test building gemm with aTranspose = true.
    // Transposed a_dimensions would be {3, 2} and it's compatible with
    // b_dimensions {2, 4}.
    GemmTester{
        .a = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {2, 3}},
        .b = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {2, 4}},
        .options = {.a_transpose = true},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {3, 4}},
        .expected_attributes = {.c = std::nullopt,
                                .alpha = 1.0,
                                .beta = 1.0,
                                .a_transpose = true,
                                .b_transpose = false}}
        .Test(*this, scope, builder);
  }
  {
    // Test building gemm with bTranspose = true.
    // Transposed b_dimensions would be {3, 4} and it's compatible with
    // a_dimensions {2, 3}.
    GemmTester{
        .a = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {2, 3}},
        .b = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {4, 3}},
        .options = {.b_transpose = true},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 4}},
        .expected_attributes = {.c = std::nullopt,
                                .alpha = 1.0,
                                .beta = 1.0,
                                .a_transpose = false,
                                .b_transpose = true}}
        .Test(*this, scope, builder);
  }
  {
    // Test building gemm with setting optional input C.
    // The output dimensions of a * b would be {2, 4} and c_dimensions {4} is
    // able to broadcast to {2, 4}.
    GemmTester{
        .a = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {2, 3}},
        .b = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {3, 4}},
        .options =
            {
                .c = OperandInfoBlink{.data_type =
                                          V8MLOperandDataType::Enum::kFloat32,
                                      .dimensions = {4}},
                .alpha = 2.0,
                .beta = 3.0,
            },
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 4}},
        .expected_attributes =
            {.c = OperandInfoMojo{.data_type =
                                      blink_mojom::Operand::DataType::kFloat32,
                                  .dimensions = {4}},
             .alpha = 2.0,
             .beta = 3.0,
             .a_transpose = false,
             .b_transpose = false}}
        .Test(*this, scope, builder);
  }
  {
    // Test building gemm with setting scalar C.
    GemmTester{
        .a = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {2, 3}},
        .b = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {3, 4}},
        .options =
            {
                .c = OperandInfoBlink{.data_type =
                                          V8MLOperandDataType::Enum::kFloat32,
                                      .dimensions = {}},
                .alpha = 2.0,
                .beta = 3.0,
            },
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 4}},
        .expected_attributes =
            {.c = OperandInfoMojo{.data_type =
                                      blink_mojom::Operand::DataType::kFloat32,
                                  .dimensions = {}},
             .alpha = 2.0,
             .beta = 3.0,
             .a_transpose = false,
             .b_transpose = false}}
        .Test(*this, scope, builder);
  }
}

struct HardSigmoidTester {
  OperandInfoBlink input;
  std::optional<float> alpha;
  std::optional<float> beta;
  OperandInfoMojo expected_output;
  float expected_alpha;
  float expected_beta;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    MLHardSigmoidOptions* hard_sigmoid_options = MLHardSigmoidOptions::Create();
    if (alpha) {
      hard_sigmoid_options->setAlpha(alpha.value());
    }
    if (beta) {
      hard_sigmoid_options->setBeta(beta.value());
    }
    auto* output_operand = builder->hardSigmoid(
        input_operand, hard_sigmoid_options, scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the `mojo::Operator`.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    ASSERT_TRUE(operation->is_hard_sigmoid());
    auto& hard_sigmoid = operation->get_hard_sigmoid();

    // Verify the alpha and beta.
    EXPECT_FLOAT_EQ(hard_sigmoid->alpha, expected_alpha);
    EXPECT_FLOAT_EQ(hard_sigmoid->beta, expected_beta);

    // Verify the input `mojo::Operand`.
    ASSERT_EQ(graph_info->input_operands.size(), 1u);
    auto input_operand_id = graph_info->input_operands[0];
    auto input_operand_iter =
        graph_info->id_to_operand_map.find(input_operand_id);
    ASSERT_TRUE(input_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(input_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(input_operand_iter->value->data_type, expected_output.data_type);
    EXPECT_EQ(input_operand_iter->value->dimensions,
              expected_output.dimensions);
    EXPECT_EQ(input_operand_iter->value->name, "input");

    // Verify the output `mojo::Operand`.
    ASSERT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kOutput);
    EXPECT_EQ(output_operand_iter->value->data_type, expected_output.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_output.dimensions);
    EXPECT_EQ(output_operand_iter->value->name, "output");
  }
};

TEST_P(MLGraphTestMojo, HardSigmoidTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test building hardSigmoid with default options.
    HardSigmoidTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 4}},
        .expected_output = {.data_type =
                                blink_mojom::Operand::DataType::kFloat32,
                            .dimensions = {2, 4}},
        .expected_alpha = 0.2,
        .expected_beta = 0.5}
        .Test(*this, scope, builder);
  }
  {
    // Test building hardSigmoid with alpha = 0.5, beta = -3.
    HardSigmoidTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {1, 5}},
        .alpha = 0.5,
        .beta = -3,
        .expected_output = {.data_type =
                                blink_mojom::Operand::DataType::kFloat16,
                            .dimensions = {1, 5}},
        .expected_alpha = 0.5,
        .expected_beta = -3}
        .Test(*this, scope, builder);
  }
}

struct InstanceNormalizationTester {
  OperandInfoBlink input;
  struct InstanceNormalizationOptions {
    std::optional<OperandInfoBlink> scale;
    std::optional<OperandInfoBlink> bias;
    std::optional<float> epsilon;
    std::optional<blink::V8MLInputOperandLayout::Enum> layout;
  };
  struct InstanceNormalizationAttributes {
    std::optional<OperandInfoMojo> scale;
    std::optional<OperandInfoMojo> bias;
    float epsilon = 1e-5;
    blink_mojom::InputOperandLayout layout =
        blink_mojom::InputOperandLayout::kChannelsFirst;
  };
  InstanceNormalizationOptions options;
  OperandInfoMojo expected_operand;
  InstanceNormalizationAttributes expected_attributes;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    MLInstanceNormalizationOptions* instance_normalization_options =
        MLInstanceNormalizationOptions::Create();
    if (options.scale) {
      instance_normalization_options->setScale(
          BuildInput(builder, "scale", options.scale->dimensions,
                     options.scale->data_type, scope.GetExceptionState()));
    }
    if (options.bias) {
      instance_normalization_options->setBias(
          BuildInput(builder, "bias", options.bias->dimensions,
                     options.bias->data_type, scope.GetExceptionState()));
    }
    if (options.epsilon) {
      instance_normalization_options->setEpsilon(options.epsilon.value());
    }
    if (options.layout) {
      instance_normalization_options->setLayout(options.layout.value());
    }

    auto* output_operand = builder->instanceNormalization(
        input_operand, instance_normalization_options,
        scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_TRUE(operation->is_instance_normalization());
    auto& instance_normalization = operation->get_instance_normalization();
    EXPECT_EQ(instance_normalization->layout, expected_attributes.layout);
    EXPECT_FLOAT_EQ(instance_normalization->epsilon,
                    expected_attributes.epsilon);
    if (options.scale) {
      auto scale_operand_iter = graph_info->id_to_operand_map.find(
          instance_normalization->scale_operand_id.value());
      ASSERT_TRUE(scale_operand_iter != graph_info->id_to_operand_map.end());
      EXPECT_EQ(scale_operand_iter->value->data_type,
                expected_attributes.scale->data_type);
      EXPECT_EQ(scale_operand_iter->value->dimensions,
                expected_attributes.scale->dimensions);
    }
    if (options.bias) {
      auto bias_operand_iter = graph_info->id_to_operand_map.find(
          instance_normalization->bias_operand_id.value());
      ASSERT_TRUE(bias_operand_iter != graph_info->id_to_operand_map.end());
      EXPECT_EQ(bias_operand_iter->value->data_type,
                expected_attributes.bias->data_type);
      EXPECT_EQ(bias_operand_iter->value->dimensions,
                expected_attributes.bias->dimensions);
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

TEST_P(MLGraphTestMojo, InstanceNormalizationTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test instanceNormalization with default options.
    InstanceNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale = std::nullopt,
             .bias = std::nullopt,
             .epsilon = 1e-5,
             .layout = blink_mojom::InputOperandLayout::kChannelsFirst}}
        .Test(*this, scope, builder);
  }
  {
    // Test instanceNormalization with layout = nhwc.
    InstanceNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 4, 5}},
        .options = {.layout = V8MLInputOperandLayout::Enum::kNhwc},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 4, 5}},
        .expected_attributes =
            {.scale = std::nullopt,
             .bias = std::nullopt,
             .epsilon = 1e-5,
             .layout = blink_mojom::InputOperandLayout::kChannelsLast}}
        .Test(*this, scope, builder);
  }
  {
    // Test instanceNormalization with epsilon = 0.01.
    InstanceNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 4, 5}},
        .options = {.epsilon = 0.01},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 4, 5}},
        .expected_attributes =
            {.scale = std::nullopt,
             .bias = std::nullopt,
             .epsilon = 0.01,
             .layout = blink_mojom::InputOperandLayout::kChannelsFirst}}
        .Test(*this, scope, builder);
  }
  {
    // Test instanceNormalization with scale and bias.
    InstanceNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
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
             .epsilon = 1e-5,
             .layout = blink_mojom::InputOperandLayout::kChannelsFirst}}
        .Test(*this, scope, builder);
  }
}

struct LayerNormalizationTester {
  OperandInfoBlink input;
  struct LayerNormalizationOptions {
    std::optional<OperandInfoBlink> scale;
    std::optional<OperandInfoBlink> bias;
    std::optional<Vector<uint32_t>> axes;
    std::optional<float> epsilon;
  };
  struct LayerNormalizationAttributes {
    std::optional<OperandInfoMojo> scale;
    std::optional<OperandInfoMojo> bias;
    Vector<uint32_t> axes;
    float epsilon = 1e-5;
  };
  LayerNormalizationOptions options;
  OperandInfoMojo expected_operand;
  LayerNormalizationAttributes expected_attributes;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    MLLayerNormalizationOptions* layer_normalization_options =
        MLLayerNormalizationOptions::Create();
    if (options.scale) {
      layer_normalization_options->setScale(
          BuildInput(builder, "scale", options.scale->dimensions,
                     options.scale->data_type, scope.GetExceptionState()));
    }
    if (options.bias) {
      layer_normalization_options->setBias(
          BuildInput(builder, "bias", options.bias->dimensions,
                     options.bias->data_type, scope.GetExceptionState()));
    }
    if (options.axes) {
      layer_normalization_options->setAxes(options.axes.value());
    }
    if (options.epsilon) {
      layer_normalization_options->setEpsilon(options.epsilon.value());
    }

    auto* output_operand = builder->layerNormalization(
        input_operand, layer_normalization_options, scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the `mojo::Operator`.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    ASSERT_TRUE(operation->is_layer_normalization());
    auto& layer_normalization = operation->get_layer_normalization();

    // Verify the axes.
    EXPECT_EQ(layer_normalization->axes, expected_attributes.axes);

    // Verify the epsilon.
    EXPECT_FLOAT_EQ(layer_normalization->epsilon, expected_attributes.epsilon);

    // Verify the scale `mojo::Operand`.
    if (expected_attributes.scale.has_value()) {
      ASSERT_TRUE(layer_normalization->scale_operand_id.has_value());
      auto scale_operand_id = layer_normalization->scale_operand_id.value();
      auto scale_operand_iter =
          graph_info->id_to_operand_map.find(scale_operand_id);
      ASSERT_TRUE(scale_operand_iter != graph_info->id_to_operand_map.end());
      EXPECT_EQ(scale_operand_iter->value->kind,
                blink_mojom::Operand::Kind::kInput);
      EXPECT_EQ(scale_operand_iter->value->data_type,
                expected_attributes.scale->data_type);
      EXPECT_EQ(scale_operand_iter->value->dimensions,
                expected_attributes.scale->dimensions);
      EXPECT_EQ(scale_operand_iter->value->name, "scale");
    }

    // Verify the bias `mojo::Operand`.
    if (expected_attributes.bias.has_value()) {
      ASSERT_TRUE(layer_normalization->bias_operand_id.has_value());
      auto bias_operand_id = layer_normalization->bias_operand_id.value();
      auto bias_operand_iter =
          graph_info->id_to_operand_map.find(bias_operand_id);
      ASSERT_TRUE(bias_operand_iter != graph_info->id_to_operand_map.end());
      EXPECT_EQ(bias_operand_iter->value->kind,
                blink_mojom::Operand::Kind::kInput);
      EXPECT_EQ(bias_operand_iter->value->data_type,
                expected_attributes.bias->data_type);
      EXPECT_EQ(bias_operand_iter->value->dimensions,
                expected_attributes.bias->dimensions);
      EXPECT_EQ(bias_operand_iter->value->name, "bias");
    }

    // Verify the output `mojo::Operand`.
    ASSERT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kOutput);
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
    EXPECT_EQ(output_operand_iter->value->name, "output");
  }
};

TEST_P(MLGraphTestMojo, LayerNormalizationTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test layerNormalization with default options for scalar input.
    LayerNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {}},
        .expected_attributes = {.axes = {}}}
        .Test(*this, scope, builder);
  }
  {
    // Test layerNormalization with given epsilon.
    LayerNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 3}},
        .options = {.epsilon = 5e-5},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 2, 3}},
        .expected_attributes = {.axes = {1, 2}, .epsilon = 5e-5}}
        .Test(*this, scope, builder);
  }
  {
    // Test layerNormalization with given axes.
    LayerNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 4, 5}},
        .options = {.axes = Vector<uint32_t>{2, 0}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 4, 5}},
        .expected_attributes = {.axes = {2, 0}}}
        .Test(*this, scope, builder);
  }
  {
    // Test layerNormalization with given scale and bias.
    LayerNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 4, 5}},
        .options = {.scale =
                        OperandInfoBlink{
                            .data_type = V8MLOperandDataType::Enum::kFloat32,
                            .dimensions = {3, 4, 5}},
                    .bias =
                        OperandInfoBlink{
                            .data_type = V8MLOperandDataType::Enum::kFloat32,
                            .dimensions = {3, 4, 5}}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 4, 5}},
        .expected_attributes =
            {.scale =
                 OperandInfoMojo{
                     .data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {3, 4, 5}},
             .bias =
                 OperandInfoMojo{
                     .data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {3, 4, 5}},
             .axes = {1, 2, 3}}}
        .Test(*this, scope, builder);
  }
  {
    // Test layerNormalization with given scale, bias and permuted axes.
    LayerNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 3, 4, 5, 6}},
        .options = {.scale =
                        OperandInfoBlink{
                            .data_type = V8MLOperandDataType::Enum::kFloat32,
                            .dimensions = {2, 5, 3, 6}},
                    .bias =
                        OperandInfoBlink{
                            .data_type = V8MLOperandDataType::Enum::kFloat32,
                            .dimensions = {2, 5, 3, 6}},
                    .axes = Vector<uint32_t>{1, 4, 2, 5}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 2, 3, 4, 5, 6}},
        .expected_attributes =
            {.scale =
                 OperandInfoMojo{
                     .data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {2, 5, 3, 6}},
             .bias =
                 OperandInfoMojo{
                     .data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {2, 5, 3, 6}},
             .axes = {1, 4, 2, 5}}}
        .Test(*this, scope, builder);
  }
}

struct LeakyReluTester {
  OperandInfoBlink input;
  std::optional<float> alpha;
  OperandInfoMojo expected_operand;
  float expected_alpha;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    MLLeakyReluOptions* ml_leaky_relu_options = MLLeakyReluOptions::Create();
    if (alpha) {
      ml_leaky_relu_options->setAlpha(alpha.value());
    }
    auto* output_operand = builder->leakyRelu(
        input_operand, ml_leaky_relu_options, scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->id_to_operand_map.size(), 2u);

    // Verify the input `mojo::Operand`.
    ASSERT_EQ(graph_info->input_operands.size(), 1u);
    auto input_operand_id = graph_info->input_operands[0];
    auto input_operand_iter =
        graph_info->id_to_operand_map.find(input_operand_id);
    ASSERT_TRUE(input_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(input_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(input_operand_iter->value->data_type, expected_operand.data_type);
    EXPECT_EQ(input_operand_iter->value->dimensions, input.dimensions);
    EXPECT_EQ(input_operand_iter->value->name, "input");

    // Verify the output `mojo::Operand`.
    ASSERT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kOutput);
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
    EXPECT_EQ(output_operand_iter->value->name, "output");

    // Verify the `mojo::Operator`.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    ASSERT_TRUE(operation->is_leaky_relu());
    auto& leaky_relu = operation->get_leaky_relu();
    EXPECT_EQ(leaky_relu->input_operand_id, input_operand_id);
    EXPECT_EQ(leaky_relu->output_operand_id, output_operand_id);
    EXPECT_EQ(leaky_relu->alpha, expected_alpha);
  }
};

TEST_P(MLGraphTestMojo, LeakyReluTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test leaky relu operator for 0-D scalar with default options.
    LeakyReluTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {}},
        .expected_alpha = 0.01}
        .Test(*this, scope, builder);
  }
  {
    // Test leaky relu operator for 1-D tensor with default options.
    LeakyReluTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2}},
        .expected_alpha = 0.01}
        .Test(*this, scope, builder);
  }
  {
    // Test leaky relu operator for 2-D tensor with default options.
    LeakyReluTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {3, 7}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {3, 7}},
        .expected_alpha = 0.01}
        .Test(*this, scope, builder);
  }
  {
    // Test leaky relu operator for 3-D tensor with given alpha.
    LeakyReluTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 5, 3}},
        .alpha = 0.05,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 5, 3}},
        .expected_alpha = 0.05}
        .Test(*this, scope, builder);
  }
  {
    // Test leaky relu operator for 4-D tensor with given alpha.
    LeakyReluTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {1, 2, 2, 1}},
        .alpha = 0.07,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {1, 2, 2, 1}},
        .expected_alpha = 0.07}
        .Test(*this, scope, builder);
  }
}

struct LinearTester {
  OperandInfoBlink input;
  struct LinearOptions {
    std::optional<float> alpha;
    std::optional<float> beta;
  };
  LinearOptions options;
  OperandInfoMojo expected_operand;
  LinearOptions expected_attributes;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    MLLinearOptions* ml_linear_options = MLLinearOptions::Create();
    if (options.alpha) {
      ml_linear_options->setAlpha(options.alpha.value());
    }
    if (options.beta) {
      ml_linear_options->setBeta(options.beta.value());
    }
    auto* output_operand = builder->linear(input_operand, ml_linear_options,
                                           scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->id_to_operand_map.size(), 2u);

    // Verify the input `mojo::Operand`.
    ASSERT_EQ(graph_info->input_operands.size(), 1u);
    auto input_operand_id = graph_info->input_operands[0];
    auto input_operand_iter =
        graph_info->id_to_operand_map.find(input_operand_id);
    ASSERT_TRUE(input_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(input_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(input_operand_iter->value->data_type, expected_operand.data_type);
    EXPECT_EQ(input_operand_iter->value->dimensions, input.dimensions);
    EXPECT_EQ(input_operand_iter->value->name, "input");

    // Verify the output `mojo::Operand`.
    ASSERT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kOutput);
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
    EXPECT_EQ(output_operand_iter->value->name, "output");

    // Verify the `mojo::Operator`.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    ASSERT_TRUE(operation->is_linear());
    auto& linear = operation->get_linear();
    EXPECT_EQ(linear->input_operand_id, input_operand_id);
    EXPECT_EQ(linear->output_operand_id, output_operand_id);
    EXPECT_EQ(linear->alpha, expected_attributes.alpha);
    EXPECT_EQ(linear->beta, expected_attributes.beta);
  }
};

TEST_P(MLGraphTestMojo, LinearTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test linear operator for 0-D scalar with default options.
    LinearTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {}},
        .expected_attributes = {.alpha = 1.0, .beta = 0}}
        .Test(*this, scope, builder);
  }
  {
    // Test linear operator for 1-D tensor with default options.
    LinearTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2}},
        .expected_attributes = {.alpha = 1.0, .beta = 0}}
        .Test(*this, scope, builder);
  }
  {
    // Test linear operator for 2-D tensor with given alpha.
    LinearTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {3, 7}},
        .options = {.alpha = 0.05},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {3, 7}},
        .expected_attributes = {.alpha = 0.05, .beta = 0}}
        .Test(*this, scope, builder);
  }
  {
    // Test linear operator for 3-D tensor with given beta.
    LinearTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 5, 3}},
        .options = {.beta = 0.07},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 5, 3}},
        .expected_attributes = {.alpha = 1.0, .beta = 0.07}}
        .Test(*this, scope, builder);
  }
  {
    // Test linear operator for 4-D tensor with given beta and beta.
    LinearTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {1, 2, 2, 1}},
        .options = {.alpha = 0.05, .beta = 0.07},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {1, 2, 2, 1}},
        .expected_attributes = {.alpha = 0.05, .beta = 0.07}}
        .Test(*this, scope, builder);
  }
}

struct MatmulTester {
  OperandInfoBlink a;
  OperandInfoBlink b;
  OperandInfoMojo expected_operand;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* a_operand = BuildInput(builder, "a", a.dimensions, a.data_type,
                                 scope.GetExceptionState());
    auto* b_operand = BuildInput(builder, "b", b.dimensions, b.data_type,
                                 scope.GetExceptionState());
    auto* output_operand =
        builder->matmul(a_operand, b_operand, scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    EXPECT_EQ(graph_info->id_to_operand_map.size(), 3u);
    EXPECT_EQ(graph_info->input_operands.size(), 2u);
    // Verify the a `mojo::Operand`.
    auto a_operand_id = graph_info->input_operands[0];
    auto a_operand_iter = graph_info->id_to_operand_map.find(a_operand_id);
    ASSERT_TRUE(a_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(a_operand_iter->value->kind, blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(a_operand_iter->value->data_type, expected_operand.data_type);
    EXPECT_EQ(a_operand_iter->value->dimensions, a.dimensions);
    EXPECT_EQ(a_operand_iter->value->name, "a");
    // Verify the b `mojo::Operand`.
    auto b_operand_id = graph_info->input_operands[1];
    auto b_operand_iter = graph_info->id_to_operand_map.find(b_operand_id);
    ASSERT_TRUE(b_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(b_operand_iter->value->kind, blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(b_operand_iter->value->data_type, expected_operand.data_type);
    EXPECT_EQ(b_operand_iter->value->dimensions, b.dimensions);
    EXPECT_EQ(b_operand_iter->value->name, "b");
    // Verify the output `mojo::Operand`.
    ASSERT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->name, "output");
    // Verify the `mojo::Operator`.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_TRUE(operation->is_matmul());
  }
};

TEST_P(MLGraphTestMojo, MatmulTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test building matmul with 2-D * 2-D.
    MatmulTester{
        .a = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {2, 3}},
        .b = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {3, 4}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 4}}}
        .Test(*this, scope, builder);
  }
  {
    // Test building matmul with 3-D * 4-D using broadcasting.
    MatmulTester{
        .a = {.data_type = V8MLOperandDataType::Enum::kFloat16,
              .dimensions = {2, 2, 3}},
        .b = {.data_type = V8MLOperandDataType::Enum::kFloat16,
              .dimensions = {3, 1, 3, 4}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {3, 2, 2, 4}}}
        .Test(*this, scope, builder);
  }
}

struct PadTester {
  OperandInfoBlink input;
  Vector<uint32_t> beginning_padding;
  Vector<uint32_t> ending_padding;
  struct PadOptions {
    std::optional<V8MLPaddingMode::Enum> mode;
    std::optional<float> value;
  };
  PadOptions options;

  blink_mojom::PaddingMode::Tag expected_mode =
      blink_mojom::PaddingMode::Tag::kConstant;
  float expected_value = 0;
  OperandInfoMojo expected_operand;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    MLPadOptions* ml_pad_options = MLPadOptions::Create();
    if (options.mode) {
      ml_pad_options->setMode(options.mode.value());
    }
    if (options.value) {
      ml_pad_options->setValue(options.value.value());
    }

    auto* output_operand =
        BuildPad(scope, builder, input_operand, beginning_padding,
                 ending_padding, ml_pad_options);
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_TRUE(operation->is_pad());
    auto& pad_mojo = operation->get_pad();

    // Validate the beginning padding and the ending padding.
    EXPECT_EQ(pad_mojo->beginning_padding, beginning_padding);
    EXPECT_EQ(pad_mojo->ending_padding, ending_padding);
    // Validate the padding mode.
    auto& padding_mode = pad_mojo->mode;
    EXPECT_EQ(padding_mode->which(), expected_mode);
    // Validate the padding value.
    if (padding_mode->is_constant()) {
      EXPECT_EQ(padding_mode->get_constant()->value, expected_value);
    }
    // Validate the output operand.
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

TEST_P(MLGraphTestMojo, PadTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test pad with default options, beginningPadding = {1, 2} and
    // endingPadding = {1, 2}.
    PadTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                        .dimensions = {2, 3}},
              .beginning_padding = {1, 2},
              .ending_padding = {1, 2},
              .expected_operand = {.data_type =
                                       blink_mojom::Operand::DataType::kFloat32,
                                   .dimensions = {4, 7}}}
        .Test(*this, scope, builder);
  }
  {
    // Test pad with mode = "constant", value = 1, beginningPadding = {1, 2} and
    // endingPadding = {1, 2}.
    PadTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                        .dimensions = {2, 3}},
              .beginning_padding = {1, 2},
              .ending_padding = {1, 2},
              .options = {.mode = V8MLPaddingMode::Enum::kConstant, .value = 1},
              .expected_mode = blink_mojom::PaddingMode::Tag::kConstant,
              .expected_value = 1,
              .expected_operand = {.data_type =
                                       blink_mojom::Operand::DataType::kFloat32,
                                   .dimensions = {4, 7}}}
        .Test(*this, scope, builder);
  }
  {
    // Test pad with mode = "edge", beginningPadding = {1, 2} and
    // endingPadding = {1, 2}.
    PadTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                        .dimensions = {2, 3}},
              .beginning_padding = {1, 2},
              .ending_padding = {1, 2},
              .options = {.mode = V8MLPaddingMode::Enum::kEdge},
              .expected_mode = blink_mojom::PaddingMode::Tag::kEdge,
              .expected_operand = {.data_type =
                                       blink_mojom::Operand::DataType::kFloat32,
                                   .dimensions = {4, 7}}}
        .Test(*this, scope, builder);
  }
  {
    // Test pad with mode = "reflection", beginningPadding = {1, 2} and
    // endingPadding = {1, 2}.
    PadTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                        .dimensions = {2, 3}},
              .beginning_padding = {1, 2},
              .ending_padding = {1, 2},
              .options = {.mode = V8MLPaddingMode::Enum::kReflection},
              .expected_mode = blink_mojom::PaddingMode::Tag::kReflection,
              .expected_operand = {.data_type =
                                       blink_mojom::Operand::DataType::kFloat32,
                                   .dimensions = {4, 7}}}
        .Test(*this, scope, builder);
  }
  {
    // Test pad with mode = "symmetric", beginningPadding = {1, 2} and
    // endingPadding = {1, 2}.
    PadTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                        .dimensions = {2, 3}},
              .beginning_padding = {1, 2},
              .ending_padding = {1, 2},
              .options = {.mode = V8MLPaddingMode::Enum::kSymmetric},
              .expected_mode = blink_mojom::PaddingMode::Tag::kSymmetric,
              .expected_operand = {.data_type =
                                       blink_mojom::Operand::DataType::kFloat32,
                                   .dimensions = {4, 7}}}
        .Test(*this, scope, builder);
  }
}

struct PreluTester {
  OperandInfoBlink input;
  OperandInfoBlink slope;
  OperandInfoMojo expected;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* slope_operand =
        BuildInput(builder, "slope", slope.dimensions, slope.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        builder->prelu(input_operand, slope_operand, scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    ASSERT_TRUE(operation->is_prelu());
    auto& prelu = operation->get_prelu();

    // Verify the input operand.
    ASSERT_EQ(graph_info->input_operands.size(), 2u);
    auto input_operand_id = graph_info->input_operands[0];
    auto input_operand_iter =
        graph_info->id_to_operand_map.find(input_operand_id);
    ASSERT_TRUE(input_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(input_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(input_operand_iter->value->data_type, expected.data_type);
    EXPECT_EQ(input_operand_iter->value->dimensions, input.dimensions);
    EXPECT_EQ(input_operand_iter->value->name, "input");
    EXPECT_EQ(prelu->input_operand_id, input_operand_id);

    // Verify the slope operand.
    auto slope_operand_id = graph_info->input_operands[1];
    auto slope_operand_iter =
        graph_info->id_to_operand_map.find(slope_operand_id);
    ASSERT_TRUE(slope_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(slope_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(slope_operand_iter->value->data_type, expected.data_type);
    EXPECT_EQ(slope_operand_iter->value->dimensions, slope.dimensions);
    EXPECT_EQ(slope_operand_iter->value->name, "slope");
    EXPECT_EQ(prelu->slope_operand_id, slope_operand_id);

    // Verify the output operand.
    ASSERT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kOutput);
    EXPECT_EQ(output_operand_iter->value->data_type, expected.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions, expected.dimensions);
    EXPECT_EQ(output_operand_iter->value->name, "output");
    EXPECT_EQ(prelu->output_operand_id, output_operand_id);
  }
};

TEST_P(MLGraphTestMojo, PreluTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test prelu operator when input shape is the same as slope shape.
    PreluTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 3, 5}},
        .slope = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 3, 5}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {2, 3, 5}}}
        .Test(*this, scope, builder);
  }
  {
    // Test prelu operator with input shape as {2, 3, 5} and slope shape as {3,
    // 5}.
    PreluTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {2, 3, 5}},
        .slope = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {3, 5}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat16,
                     .dimensions = {2, 3, 5}}}
        .Test(*this, scope, builder);
  }
  {
    // Test prelu operator with input shape as {2, 3, 5} and slope shape as {5}.
    PreluTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {2, 3, 5}},
        .slope = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {5}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat16,
                     .dimensions = {2, 3, 5}}}
        .Test(*this, scope, builder);
  }
}

struct ReshapeTester {
  OperandInfoBlink input;
  Vector<uint32_t> new_shape;
  OperandInfoMojo expected;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        builder->reshape(input_operand, new_shape, scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_TRUE(operation->is_reshape());
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type, expected.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions, expected.dimensions);
  }
};

TEST_P(MLGraphTestMojo, ReshapeTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test reshaping 1-D tensor to 0-D scalar.
    ReshapeTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1}},
        .new_shape = {},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {}}}
        .Test(*this, scope, builder);
  }
  {
    // Test reshaping 0-D scalar to 1-D tensor.
    ReshapeTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {}},
        .new_shape = {1},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {1}}}
        .Test(*this, scope, builder);
  }
  {
    // Test reshaping 2-D tensor to 1-D tensor.
    ReshapeTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 2}},
        .new_shape = {4},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {4}}}
        .Test(*this, scope, builder);
  }
  {
    // Test reshaping from 4-D tensor to 2-D tensor.
    ReshapeTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                  .dimensions = {1, 2, 2, 1}},
        .new_shape = {1, 4},
        .expected = {.data_type = blink_mojom::Operand::DataType::kInt32,
                     .dimensions = {1, 4}}}
        .Test(*this, scope, builder);
  }
}

struct SliceTester {
  struct SliceAttributes {
    Vector<uint32_t> starts;
    Vector<uint32_t> sizes;
  };
  OperandInfoBlink input;
  SliceAttributes options;
  OperandInfoMojo expected_operand;
  SliceAttributes expected_attributes;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        builder->slice(input_operand, options.starts, options.sizes,
                       scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_TRUE(operation->is_slice());
    auto& slice_mojo = operation->get_slice();

    for (uint32_t i = 0; i < slice_mojo->starts_and_sizes.size(); ++i) {
      EXPECT_EQ(slice_mojo->starts_and_sizes[i]->start,
                expected_attributes.starts[i]);
      EXPECT_EQ(slice_mojo->starts_and_sizes[i]->size,
                expected_attributes.sizes[i]);
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

TEST_P(MLGraphTestMojo, SliceTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  {
    SliceTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {4, 4}},
        .options = {.starts = Vector<uint32_t>({0, 0}),
                    .sizes = Vector<uint32_t>({4, 4})},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {4, 4}},
        .expected_attributes = {.starts = {0, 0}, .sizes = {4, 4}}}
        .Test(*this, scope, builder);
    SliceTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 3, 4, 5}},
        .options = {.starts = Vector<uint32_t>({0, 1, 2, 3, 4}),
                    .sizes = Vector<uint32_t>({1, 1, 1, 1, 1})},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 1, 1, 1}},
        .expected_attributes = {.starts = {0, 1, 2, 3, 4},
                                .sizes = {1, 1, 1, 1, 1}}}
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

struct SoftplusTester {
  OperandInfoBlink input;
  std::optional<float> steepness;
  OperandInfoMojo expected_output;
  float expected_steepness;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    MLSoftplusOptions* softplus_options = MLSoftplusOptions::Create();
    if (steepness) {
      softplus_options->setSteepness(steepness.value());
    }
    auto* output_operand = builder->softplus(input_operand, softplus_options,
                                             scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the `mojo::Operator`.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    ASSERT_TRUE(operation->is_softplus());
    auto& softplus = operation->get_softplus();

    // Verify the steepness.
    EXPECT_FLOAT_EQ(softplus->steepness, expected_steepness);

    // Verify the input `mojo::Operand`.
    ASSERT_EQ(graph_info->input_operands.size(), 1u);
    auto input_operand_id = graph_info->input_operands[0];
    auto input_operand_iter =
        graph_info->id_to_operand_map.find(input_operand_id);
    ASSERT_TRUE(input_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(input_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(input_operand_iter->value->data_type, expected_output.data_type);
    EXPECT_EQ(input_operand_iter->value->dimensions,
              expected_output.dimensions);
    EXPECT_EQ(input_operand_iter->value->name, "input");

    // Verify the output `mojo::Operand`.
    ASSERT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kOutput);
    EXPECT_EQ(output_operand_iter->value->data_type, expected_output.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_output.dimensions);
    EXPECT_EQ(output_operand_iter->value->name, "output");
  }
};

TEST_P(MLGraphTestMojo, SoftplusTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test building softplus with default options.
    SoftplusTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 4}},
        .expected_output = {.data_type =
                                blink_mojom::Operand::DataType::kFloat32,
                            .dimensions = {2, 4}},
        .expected_steepness = 1.0}
        .Test(*this, scope, builder);
  }
  {
    // Test building softplus with steepness = 5.0.
    SoftplusTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {1, 5}},
        .steepness = 5.0,
        .expected_output = {.data_type =
                                blink_mojom::Operand::DataType::kFloat16,
                            .dimensions = {1, 5}},
        .expected_steepness = 5.0}
        .Test(*this, scope, builder);
  }
}

struct ReduceTester {
  OperandInfoBlink input;
  std::optional<Vector<uint32_t>> axes;
  std::optional<bool> keep_dimensions;
  OperandInfoMojo expected_operand;
  Vector<uint32_t> expected_axes;
  bool expected_keep_dimensions;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    Test(helper, scope, builder, webnn::mojom::blink::Reduce::Kind::kL1);
    Test(helper, scope, builder, webnn::mojom::blink::Reduce::Kind::kL2);
    Test(helper, scope, builder, webnn::mojom::blink::Reduce::Kind::kLogSum);
    Test(helper, scope, builder, webnn::mojom::blink::Reduce::Kind::kLogSumExp);
    Test(helper, scope, builder, webnn::mojom::blink::Reduce::Kind::kMax);
    Test(helper, scope, builder, webnn::mojom::blink::Reduce::Kind::kMean);
    Test(helper, scope, builder, webnn::mojom::blink::Reduce::Kind::kMin);
    Test(helper, scope, builder, webnn::mojom::blink::Reduce::Kind::kProduct);
    Test(helper, scope, builder, webnn::mojom::blink::Reduce::Kind::kSum);
    Test(helper, scope, builder, webnn::mojom::blink::Reduce::Kind::kSumSquare);
  }

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder,
            webnn::mojom::blink::Reduce::Kind kind) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    MLReduceOptions* options = MLReduceOptions::Create();
    if (axes.has_value()) {
      options->setAxes(axes.value());
    }
    if (keep_dimensions.has_value()) {
      options->setKeepDimensions(keep_dimensions.value());
    }
    auto* output_operand =
        BuildReduce(scope, builder, kind, input_operand, options);
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    ASSERT_TRUE(operation->is_reduce());
    auto& reduce = operation->get_reduce();

    blink_mojom::Reduce::Kind reduce_kind;
    switch (kind) {
      case webnn::mojom::blink::Reduce::Kind::kL1:
        reduce_kind = blink_mojom::Reduce::Kind::kL1;
        break;
      case webnn::mojom::blink::Reduce::Kind::kL2:
        reduce_kind = blink_mojom::Reduce::Kind::kL2;
        break;
      case webnn::mojom::blink::Reduce::Kind::kLogSum:
        reduce_kind = blink_mojom::Reduce::Kind::kLogSum;
        break;
      case webnn::mojom::blink::Reduce::Kind::kLogSumExp:
        reduce_kind = blink_mojom::Reduce::Kind::kLogSumExp;
        break;
      case webnn::mojom::blink::Reduce::Kind::kMax:
        reduce_kind = blink_mojom::Reduce::Kind::kMax;
        break;
      case webnn::mojom::blink::Reduce::Kind::kMean:
        reduce_kind = blink_mojom::Reduce::Kind::kMean;
        break;
      case webnn::mojom::blink::Reduce::Kind::kMin:
        reduce_kind = blink_mojom::Reduce::Kind::kMin;
        break;
      case webnn::mojom::blink::Reduce::Kind::kProduct:
        reduce_kind = blink_mojom::Reduce::Kind::kProduct;
        break;
      case webnn::mojom::blink::Reduce::Kind::kSum:
        reduce_kind = blink_mojom::Reduce::Kind::kSum;
        break;
      case webnn::mojom::blink::Reduce::Kind::kSumSquare:
        reduce_kind = blink_mojom::Reduce::Kind::kSumSquare;
        break;
    }
    EXPECT_EQ(reduce->kind, reduce_kind);
    // Validate the axes of reduce operation.
    EXPECT_EQ(reduce->axes, expected_axes);
    // Validate the keep_dimensions of reduce operation.
    EXPECT_EQ(reduce->keep_dimensions, expected_keep_dimensions);

    // Validate the input operand.
    EXPECT_EQ(graph_info->input_operands.size(), 1u);
    auto input_operand_id = graph_info->input_operands[0];
    EXPECT_EQ(reduce->input_operand_id, input_operand_id);
    auto input_operand_iter =
        graph_info->id_to_operand_map.find(input_operand_id);
    ASSERT_TRUE(input_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(input_operand_iter->value->data_type, expected_operand.data_type);
    EXPECT_EQ(input_operand_iter->value->dimensions, input.dimensions);

    // Validate the output operand.
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    EXPECT_EQ(reduce->output_operand_id, output_operand_id);
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
  }
};

TEST_P(MLGraphTestMojo, ReduceTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test reduce operator with default options.
    ReduceTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 3, 4}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {}},
        .expected_axes = {0, 1, 2, 3},
        .expected_keep_dimensions = false}
        .Test(*this, scope, builder);
  }
  {
    // Test reduce operator with a given axes and keep_dimensions.
    ReduceTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {1, 2, 3, 4}},
        .axes = Vector<uint32_t>{1},
        .keep_dimensions = true,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {1, 1, 3, 4}},
        .expected_axes = {1},
        .expected_keep_dimensions = true}
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
    // Test Constant operand for UInt32 data type.
    ConstantTester<uint32_t>{
        .constant = {.data_type = V8MLOperandDataType::Enum::kUint32,
                     .dimensions = {2, 3},
                     .values = {1, 2, 3, 4, 5, 6}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kUint32,
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
  {
    // Test Constant operand for UInt8 data type.
    ConstantTester<uint8_t>{
        .constant = {.data_type = V8MLOperandDataType::Enum::kUint8,
                     .dimensions = {2, 3},
                     .values = {1, 2, 3, 4, 5, 6}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kUint8,
                     .dimensions = {2, 3}},
        .expected_constant_data = {1, 2, 3, 4, 5, 6}}
        .Test(*this, scope, builder);
  }
}

struct SplitTester {
  OperandInfoBlink input;
  absl::variant<uint32_t, Vector<uint32_t>> splits;
  std::optional<uint32_t> axis;
  Vector<OperandInfoMojo> expected;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* attributes = MLSplitOptions::Create();
    if (axis.has_value()) {
      attributes->setAxis(axis.value());
    }
    HeapVector<Member<MLOperand>> output_operands;
    if (absl::holds_alternative<uint32_t>(splits)) {
      output_operands.assign(
          builder->split(input_operand, absl::get<uint32_t>(splits), attributes,
                         scope.GetExceptionState()));
    } else if (absl::holds_alternative<Vector<uint32_t>>(splits)) {
      output_operands.assign(
          builder->split(input_operand, absl::get<Vector<uint32_t>>(splits),
                         attributes, scope.GetExceptionState()));
    }
    MLNamedOperands output_named_operand;
    for (wtf_size_t i = 0; i < output_operands.size(); ++i) {
      output_named_operand.push_back(
          std::make_pair(String::Format("output%u", i), output_operands.at(i)));
    }
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, output_named_operand);
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_TRUE(operation->is_split());
    EXPECT_EQ(graph_info->output_operands.size(), expected.size());
    for (wtf_size_t i = 0; i < expected.size(); ++i) {
      auto output_operand_id = graph_info->output_operands[i];
      auto output_operand_iter =
          graph_info->id_to_operand_map.find(output_operand_id);
      ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
      EXPECT_EQ(output_operand_iter->value->data_type, expected[i].data_type);
      EXPECT_EQ(output_operand_iter->value->dimensions, expected[i].dimensions);
    }
  }
};

TEST_P(MLGraphTestMojo, SplitTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  using v8 = V8MLOperandDataType::Enum;
  using blink = blink_mojom::Operand::DataType;
  {
    SplitTester{
        .input = {.data_type = v8::kFloat32, .dimensions = {2, 2}},
        .splits = 2u,
        .expected = {{.data_type = blink::kFloat32, .dimensions = {1, 2}},
                     {.data_type = blink::kFloat32, .dimensions = {1, 2}}}}
        .Test(*this, scope, builder);
  }
  {
    SplitTester{
        .input = {.data_type = v8::kFloat32, .dimensions = {2, 2}},
        .splits = 2u,
        .axis = 1,
        .expected = {{.data_type = blink::kFloat32, .dimensions = {2, 1}},
                     {.data_type = blink::kFloat32, .dimensions = {2, 1}}}}
        .Test(*this, scope, builder);
  }
  {
    SplitTester{
        .input = {.data_type = v8::kFloat32, .dimensions = {6, 2}},
        .splits = Vector<uint32_t>{1, 2, 3},
        .expected = {{.data_type = blink::kFloat32, .dimensions = {1, 2}},
                     {.data_type = blink::kFloat32, .dimensions = {2, 2}},
                     {.data_type = blink::kFloat32, .dimensions = {3, 2}}}}
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
