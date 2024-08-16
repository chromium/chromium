// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"

#include <array>
#include <numeric>
#include <optional>
#include <utility>

#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/features.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_buffer_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_compute_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_elu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_hard_sigmoid_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_leaky_relu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_linear_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_data_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operator_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_recurrent_network_activation.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_triangular_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/ml_trace.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_buffer.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder_test_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_type_converter.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

namespace blink_mojom = webnn::mojom::blink;

class FakeWebNNBuffer;

namespace {

// BuildResult is returned by Build() method. If the graph building is
// successful, `graph` points to the MLGraph and `error_name` and
// `error_message` are null. Otherwise, `graph` is a nullptr and
// `error_name` and `error_message` are populated from the JS error or
// DOMException.
struct BuildResult {
  Persistent<MLGraph> graph;
  String error_name;
  String error_message;
};

// Helper struct to create faked mojom result of inference.
struct ComputeResult {
  WTF::HashMap<WTF::String, WTF::Vector<uint8_t>> output;
};

template <typename T>
struct OperandInfo {
  V8MLOperandDataType::Enum data_type;
  Vector<uint32_t> dimensions;
  Vector<T> values;
};

webnn::OperandDescriptor ToDescriptor(webnn::OperandDataType data_type,
                                      base::span<const uint32_t> shape) {
  return *webnn::OperandDescriptor::Create(data_type, shape);
}

template <typename T>
T* V8ToObject(V8TestingScope* scope, ScriptValue value) {
  return NativeValueTraits<T>::NativeValue(scope->GetIsolate(), value.V8Value(),
                                           scope->GetExceptionState());
}

String ExceptionCodeToString(ExceptionCode exception_code) {
  switch (static_cast<ESErrorType>(exception_code)) {
    case ESErrorType::kTypeError:
      return "TypeError";
    default:
      NOTREACHED_IN_MIGRATION();
      return "UnknownError";
  }
}

std::pair<String, String> GetErrorNameAndMessage(V8TestingScope* scope,
                                                 ScriptValue value) {
  v8::Local<v8::Object> object;
  if (!value.V8Value()
           ->ToObject(scope->GetScriptState()->GetContext())
           .ToLocal(&object)) {
    return {"undefined", "undefined"};
  }
  const auto& Get = [&scope, object](const String& key) -> String {
    v8::Local<v8::Value> prop_value;
    if (!object
             ->Get(scope->GetScriptState()->GetContext(),
                   V8AtomicString(scope->GetScriptState()->GetIsolate(), key))
             .ToLocal(&prop_value)) {
      return "undefined";
    }
    return ToCoreStringWithUndefinedOrNullCheck(
        scope->GetScriptState()->GetIsolate(), prop_value);
  };
  return {Get("name"), Get("message")};
}

// Helper function to set the data of an ArrayBufferView from a vector.
template <typename T>
void SetArrayBufferViewValues(NotShared<DOMArrayBufferView> array_buffer_view,
                              const Vector<T>& values) {
  DCHECK_EQ(array_buffer_view->byteLength(), values.size() * sizeof(T));
  memcpy(array_buffer_view->BaseAddress(), values.data(),
         values.size() * sizeof(T));
}

// Helper function to create an ArrayBufferView given an operand.
NotShared<DOMArrayBufferView> CreateArrayBufferViewForOperand(
    const MLOperand* operand) {
  return CreateDOMArrayBufferView(operand->NumberOfElements(),
                                  operand->dataType().AsEnum());
}

// Overrode helper function to create an ArrayBufferView given an operand and
// set its data from a vector.
template <typename T>
NotShared<DOMArrayBufferView> CreateArrayBufferViewForOperand(
    const MLOperand* operand,
    const Vector<T>& values) {
  auto array_buffer_view = CreateArrayBufferViewForOperand(operand);
  SetArrayBufferViewValues(array_buffer_view, values);
  return array_buffer_view;
}

// Helper function to get the data of an ArrayBufferView into a vector.
template <typename T>
Vector<T> GetArrayBufferViewValues(
    NotShared<DOMArrayBufferView> array_buffer_view) {
  Vector<T> values(base::checked_cast<wtf_size_t>(
      array_buffer_view->byteLength() / array_buffer_view->TypeSize()));
  memcpy(values.data(), array_buffer_view->BaseAddress(),
         array_buffer_view->byteLength());
  return values;
}

MLContext* CreateContext(V8TestingScope& scope, MLContextOptions* options) {
  auto* ml = MakeGarbageCollected<ML>(scope.GetExecutionContext());
  ScriptPromiseTester tester(scope.GetScriptState(),
                             ml->createContext(scope.GetScriptState(), options,
                                               scope.GetExceptionState()));
  tester.WaitUntilSettled();
  CHECK(tester.IsFulfilled());

  return NativeValueTraits<MLContext>::NativeValue(
      scope.GetIsolate(), tester.Value().V8Value(), scope.GetExceptionState());
}

std::pair<String, String> ComputeGraph(V8TestingScope& scope,
                                       MLGraph* graph,
                                       MLNamedArrayBufferViews& inputs,
                                       MLNamedArrayBufferViews& outputs) {
  ScriptPromiseTester tester(
      scope.GetScriptState(),
      graph->Compute(ScopedMLTrace("Compute"), inputs, outputs,
                     scope.GetScriptState(), scope.GetExceptionState()));
  if (scope.GetExceptionState().HadException()) {
    return {ExceptionCodeToString(scope.GetExceptionState().Code()),
            scope.GetExceptionState().Message()};
  }
  tester.WaitUntilSettled();
  if (tester.IsFulfilled()) {
    // For `MLGraph::Compute()`, the input and output ArrayBufferViews
    // are transferred. The new ArrayBufferViews are returned via the
    // MLComputeResult. Set the inputs and outputs to the returned ones.
    auto* results = V8ToObject<MLComputeResult>(&scope, tester.Value());
    inputs = results->inputs();
    outputs = results->outputs();
    return {};
  } else {
    return GetErrorNameAndMessage(&scope, tester.Value());
  }
}

template <typename T>
MLOperand* BuildConstant(MLGraphBuilder* builder,
                         const Vector<uint32_t>& dimensions,
                         V8MLOperandDataType::Enum data_type,
                         const Vector<T>& values,
                         ExceptionState& exception_state) {
  size_t buffer_size = std::accumulate(dimensions.begin(), dimensions.end(),
                                       size_t(1), std::multiplies<uint32_t>());
  auto buffer = CreateDOMArrayBufferView(buffer_size, data_type);
  DCHECK_EQ(buffer->byteLength(), values.size() * sizeof(T));
  memcpy(buffer->BaseAddress(), values.data(), buffer->byteLength());
  return BuildConstant(builder, dimensions, data_type, exception_state, buffer);
}

MLOperand* BuildConv2d(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    const MLOperand* input,
    const MLOperand* filter,
    const MLConv2dOptions* options = MLConv2dOptions::Create()) {
  auto* output =
      builder->conv2d(input, filter, options, scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), input->DataType());
  auto* conv2d = output->Operator();
  EXPECT_THAT(conv2d, testing::NotNull());
  EXPECT_EQ(conv2d->Kind(), webnn::mojom::blink::Operation::Tag::kConv2d);
  EXPECT_THAT(conv2d->Options(), testing::NotNull());
  return output;
}

MLOperand* BuildGemm(V8TestingScope& scope,
                     MLGraphBuilder* builder,
                     const MLOperand* a,
                     const MLOperand* b,
                     const MLGemmOptions* options = MLGemmOptions::Create()) {
  auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), a->DataType());
  auto* gemm = output->Operator();
  EXPECT_THAT(gemm, testing::NotNull());
  EXPECT_EQ(gemm->Kind(), webnn::mojom::blink::Operation::Tag::kGemm);
  EXPECT_THAT(gemm->Options(), testing::NotNull());
  return output;
}

MLOperand* BuildElementWiseBinaryOperator(
    MLGraphBuilder* builder,
    V8TestingScope& scope,
    const MLOperand* a,
    const MLOperand* b,
    webnn::mojom::blink::ElementWiseBinary::Kind kind,
    const MLOperatorOptions* options) {
  switch (kind) {
    case webnn::mojom::blink::ElementWiseBinary::Kind::kAdd:
      return builder->add(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kSub:
      return builder->sub(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMul:
      return builder->mul(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kDiv:
      return builder->div(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMin:
      return builder->min(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMax:
      return builder->max(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kPow:
      return builder->pow(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kEqual:
      return builder->equal(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kGreater:
      return builder->greater(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kGreaterOrEqual:
      return builder->greaterOrEqual(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLesser:
      return builder->lesser(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLesserOrEqual:
      return builder->lesserOrEqual(a, b, options, scope.GetExceptionState());
  }
}

MLOperand* BuildElementWiseBinary(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    webnn::mojom::blink::ElementWiseBinary::Kind kind,
    const MLOperand* a,
    const MLOperand* b,
    const MLOperatorOptions* options = MLOperatorOptions::Create()) {
  MLOperand* output =
      BuildElementWiseBinaryOperator(builder, scope, a, b, kind, options);
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);

  if (IsLogicalBinaryOperator(kind)) {
    EXPECT_EQ(output->dataType().AsEnum(), V8MLOperandDataType::Enum::kUint8);
  } else {
    EXPECT_EQ(output->DataType(), a->DataType());
  }

  auto* op = output->Operator();
  EXPECT_THAT(op, testing::NotNull());
  EXPECT_EQ(op->Kind(),
            webnn::mojom::blink::Operation::Tag::kElementWiseBinary);
  EXPECT_EQ(op->SubKind<webnn::mojom::blink::ElementWiseBinary::Kind>(), kind);
  return output;
}

}  // namespace

class MLGraphTest : public testing::Test {
 public:
  MLGraphTest()
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

  BuildResult BuildGraph(V8TestingScope& scope,
                         MLGraphBuilder* builder,
                         const MLNamedOperands& named_operands) {
    ScriptPromise<MLGraph> build_promise = builder->build(
        scope.GetScriptState(), named_operands, scope.GetExceptionState());
    // An empty promise will be returned if `build()` synchronously rejects.
    if (build_promise.IsEmpty()) {
      return BuildResult{
          .error_name = ExceptionCodeToString(scope.GetExceptionState().Code()),
          .error_message = scope.GetExceptionState().Message()};
    }

    ScriptPromiseTester tester(scope.GetScriptState(), build_promise);
    tester.WaitUntilSettled();
    if (tester.IsFulfilled()) {
      return BuildResult{.graph = V8ToObject<MLGraph>(&scope, tester.Value())};
    } else {
      auto [name, message] = GetErrorNameAndMessage(&scope, tester.Value());
      return BuildResult{.error_name = name, .error_message = message};
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  test::TaskEnvironment task_environment_;

  blink_mojom::GraphInfoPtr graph_info_;
  HashMap<String, mojo_base::BigBuffer> input_array_buffers_;
  ComputeResult compute_result_;
};

class WebNNContextHelper {
 public:
  WebNNContextHelper() = default;
  ~WebNNContextHelper() = default;

  void ConnectWebNNBufferImpl(const blink::WebNNBufferToken& handle,
                              std::unique_ptr<FakeWebNNBuffer> buffer) {
    const auto it = buffer_impls_.find(handle);
    ASSERT_TRUE(it == buffer_impls_.end());
    buffer_impls_.try_emplace(handle, std::move(buffer));
  }

  void DisconnectAndDestroyWebNNBufferImpl(
      const blink::WebNNBufferToken& handle) {
    buffer_impls_.erase(handle);
  }

 private:
  std::map<blink::WebNNBufferToken, std::unique_ptr<FakeWebNNBuffer>>
      buffer_impls_;

  mojo::UniqueAssociatedReceiverSet<blink_mojom::WebNNGraphBuilder> builders_;
};

class FakeWebNNGraph : public blink_mojom::WebNNGraph {
 public:
  explicit FakeWebNNGraph(MLGraphTest& helper) : helper_(helper) {}
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
      const HashMap<WTF::String, blink::WebNNBufferToken>& named_inputs,
      const HashMap<WTF::String, blink::WebNNBufferToken>& named_outputs)
      override {}

  // TODO(crbug.com/354741414): Fix this dangling pointer.
  const raw_ref<MLGraphTest, DanglingUntriaged> helper_;
};

class FakeWebNNBuffer : public blink_mojom::WebNNBuffer {
 public:
  FakeWebNNBuffer(
      WebNNContextHelper& helper,
      mojo::PendingAssociatedReceiver<blink_mojom::WebNNBuffer> receiver,
      const blink::WebNNBufferToken& buffer_handle,
      blink_mojom::BufferInfoPtr buffer_info)
      : helper_(helper),
        receiver_(this, std::move(receiver)),
        handle_(buffer_handle) {
    buffer_ = mojo_base::BigBuffer(buffer_info->descriptor.PackedByteLength());
    receiver_.set_disconnect_handler(WTF::BindOnce(
        &FakeWebNNBuffer::OnConnectionError, WTF::Unretained(this)));
  }

  ~FakeWebNNBuffer() override = default;

  FakeWebNNBuffer(const FakeWebNNBuffer&) = delete;
  FakeWebNNBuffer(FakeWebNNBuffer&&) = delete;

  const blink::WebNNBufferToken& handle() const { return handle_; }

 private:
  void ReadBuffer(ReadBufferCallback callback) override {
    mojo_base::BigBuffer dst_buffer(buffer_.byte_span());

    std::move(callback).Run(
        blink_mojom::ReadBufferResult::NewBuffer(std::move(dst_buffer)));
  }

  void WriteBuffer(mojo_base::BigBuffer src_buffer) override {
    ASSERT_LE(src_buffer.size(), buffer_.size());
    base::span(buffer_).copy_prefix_from(src_buffer);
  }

  void OnConnectionError() {
    helper_->DisconnectAndDestroyWebNNBufferImpl(handle());
  }

  // TODO(crbug.com/354741414): Fix this dangling pointer.
  const raw_ref<WebNNContextHelper, DanglingUntriaged> helper_;

  mojo::AssociatedReceiver<blink_mojom::WebNNBuffer> receiver_;

  const blink::WebNNBufferToken handle_;

  mojo_base::BigBuffer buffer_;
};

class FakeWebNNGraphBuilder : public blink_mojom::WebNNGraphBuilder {
 public:
  explicit FakeWebNNGraphBuilder(MLGraphTest& helper) : helper_(helper) {}
  FakeWebNNGraphBuilder(const FakeWebNNGraphBuilder&) = delete;
  FakeWebNNGraphBuilder(FakeWebNNGraphBuilder&&) = delete;
  ~FakeWebNNGraphBuilder() override = default;

 private:
  // webnn::mojom::blink::WebNNGraphBuilder:
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

  // TODO(crbug.com/354741414): Fix this dangling pointer.
  const raw_ref<MLGraphTest, DanglingUntriaged> helper_;
};

class FakeWebNNContext : public blink_mojom::WebNNContext {
 public:
  explicit FakeWebNNContext(MLGraphTest& helper) : helper_(helper) {}
  FakeWebNNContext(const FakeWebNNContext&) = delete;
  FakeWebNNContext(FakeWebNNContext&&) = delete;
  ~FakeWebNNContext() override = default;

 private:
  // Override methods from webnn::mojom::WebNNContext.
  void CreateGraphBuilder(
      mojo::PendingAssociatedReceiver<blink_mojom::WebNNGraphBuilder> receiver)
      override {
    mojo::MakeSelfOwnedAssociatedReceiver<blink_mojom::WebNNGraphBuilder>(
        std::make_unique<FakeWebNNGraphBuilder>(*helper_), std::move(receiver));
  }

  void CreateBuffer(blink_mojom::BufferInfoPtr buffer_info,
                    CreateBufferCallback callback) override {
    mojo::PendingAssociatedRemote<blink_mojom::WebNNBuffer> blink_remote;
    auto blink_receiver = blink_remote.InitWithNewEndpointAndPassReceiver();
    blink::WebNNBufferToken buffer_handle;
    context_helper_.ConnectWebNNBufferImpl(
        buffer_handle, std::make_unique<FakeWebNNBuffer>(
                           context_helper_, std::move(blink_receiver),
                           buffer_handle, std::move(buffer_info)));

    auto success = blink_mojom::CreateBufferSuccess::New(
        std::move(blink_remote), std::move(buffer_handle));
    std::move(callback).Run(
        blink_mojom::CreateBufferResult::NewSuccess(std::move(success)));
  }

  // TODO(crbug.com/354741414): Fix this dangling pointer.
  const raw_ref<MLGraphTest, DanglingUntriaged> helper_;

  WebNNContextHelper context_helper_;
};

class FakeWebNNContextProvider : public blink_mojom::WebNNContextProvider {
 public:
  explicit FakeWebNNContextProvider(MLGraphTest& helper)
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

    webnn::ContextProperties context_properties(
        webnn::InputOperandLayout::kNchw,
        {/*input=*/webnn::SupportedDataTypes::All(),
         /*constant=*/webnn::SupportedDataTypes::All(),
         /*arg_min_max_input=*/
         webnn::SupportedDataTypes::All(),
         /*arg_min_max_output=*/
         webnn::SupportedDataTypes::All(),
         /*concat_inputs=*/
         webnn::SupportedDataTypes::All(),
         /*add_input=*/webnn::SupportedDataTypes::All(),
         /*sub_input=*/webnn::SupportedDataTypes::All(),
         /*mul_input=*/webnn::SupportedDataTypes::All(),
         /*div_input=*/webnn::SupportedDataTypes::All(),
         /*max_input=*/webnn::SupportedDataTypes::All(),
         /*min_input=*/webnn::SupportedDataTypes::All(),
         /*pow_input=*/webnn::SupportedDataTypes::All(),
         /*equal_input=*/webnn::SupportedDataTypes::All(),
         /*greater_input=*/webnn::SupportedDataTypes::All(),
         /*greater_or_equal_input=*/webnn::SupportedDataTypes::All(),
         /*lesser_input=*/webnn::SupportedDataTypes::All(),
         /*lesser_or_equal_input=*/webnn::SupportedDataTypes::All(),
         /*logical_not_input=*/webnn::SupportedDataTypes::All(),
         /*logical_output=*/webnn::SupportedDataTypes::All(),
         /*abs_input=*/webnn::SupportedDataTypes::All(),
         /*ceil_input=*/webnn::SupportedDataTypes::All(),
         /*cos_input=*/webnn::SupportedDataTypes::All(),
         /*erf_input=*/webnn::SupportedDataTypes::All(),
         /*exp_input=*/webnn::SupportedDataTypes::All(),
         /*floor_input=*/webnn::SupportedDataTypes::All(),
         /*identity_input=*/webnn::SupportedDataTypes::All(),
         /*log_input=*/webnn::SupportedDataTypes::All(),
         /*neg_input=*/webnn::SupportedDataTypes::All(),
         /*reciprocal_input=*/webnn::SupportedDataTypes::All(),
         /*sin_input=*/webnn::SupportedDataTypes::All(),
         /*sqrt_input=*/webnn::SupportedDataTypes::All(),
         /*tan_input=*/webnn::SupportedDataTypes::All(),
         /*elu_input=*/webnn::SupportedDataTypes::All(),
         /*gather_input=*/webnn::SupportedDataTypes::All(),
         /*gather_indices=*/
         webnn::SupportedDataTypes::All(),
         /*gelu_input=*/webnn::SupportedDataTypes::All(),
         /*leaky_relu_input=*/webnn::SupportedDataTypes::All(),
         /*relu_input=*/webnn::SupportedDataTypes::All(),
         /*sigmoid_input=*/webnn::SupportedDataTypes::All(),
         /*slice_input=*/webnn::SupportedDataTypes::All(),
         /*softmax_input=*/webnn::SupportedDataTypes::All(),
         /*softplus_input=*/webnn::SupportedDataTypes::All(),
         /*softsign_input=*/webnn::SupportedDataTypes::All(),
         /*split_input=*/webnn::SupportedDataTypes::All(),
         /*where_condition=*/
         webnn::SupportedDataTypes::All(),
         /*where_value=*/
         webnn::SupportedDataTypes::All()});
    auto success = blink_mojom::CreateContextSuccess::New(
        std::move(blink_remote), std::move(context_properties),
        blink::WebNNContextToken());
    std::move(callback).Run(
        blink_mojom::CreateContextResult::NewSuccess(std::move(success)));
  }

  const raw_ref<MLGraphTest> helper_;
  mojo::Receiver<blink_mojom::WebNNContextProvider> receiver_;
};

class ScopedWebNNServiceBinder {
 public:
  explicit ScopedWebNNServiceBinder(MLGraphTest& helper,
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

// Build a simple MLGraph asynchronously with only one relu operator.
ScriptPromise<MLGraph> BuildSimpleGraph(V8TestingScope& scope,
                                        MLContextOptions* context_options) {
  auto* context = CreateContext(scope, context_options);
  auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                         scope.GetExceptionState());
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
  const MLOperatorOptions* options = MLOperatorOptions::Create();
  auto* output = builder->add(lhs_operand, rhs_operand, options,
                              scope.GetExceptionState());
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
  array_buffer->ByteSpan().copy_prefix_from(data);
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
  desc->setDataType(operand->dataType());
  desc->setDimensions(operand->shape());

  ScriptPromiseTester tester(
      scope.GetScriptState(),
      ml_context->createBuffer(scope.GetScriptState(), desc,
                               scope.GetExceptionState()));
  tester.WaitUntilSettled();
  CHECK(tester.IsFulfilled());

  MLBuffer* ml_buffer = V8ToObject<MLBuffer>(&scope, tester.Value());

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
          array_buffer, /*byte_offset=*/0, ml_buffer->PackedByteLength())));
}

TEST_F(MLGraphTest, BuildTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  MLContext* context = CreateContext(scope, MLContextOptions::Create());
  {
    // Test throwing exception if the named outputs is empty.
    MLNamedOperands named_outputs;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           scope.GetExceptionState());
    ASSERT_THAT(builder, testing::NotNull());
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message, "At least one output needs to be provided.");
    scope.GetExceptionState().ClearException();
  }
  {
    // Test throwing exception if the named output is an input operand.
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           scope.GetExceptionState());
    ASSERT_THAT(builder, testing::NotNull());
    auto* input = BuildInput(builder, "input", {3, 4, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, {{"output", input}});
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
              "The operand with name \"output\" is not an output operand.");
    scope.GetExceptionState().ClearException();
  }
  {
    // Test throwing exception if the named output is a constant operand.
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           scope.GetExceptionState());
    ASSERT_THAT(builder, testing::NotNull());
    auto* constant =
        BuildConstant(builder, {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, {{"output", constant}});
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
              "The operand with name \"output\" is not an output operand.");
    scope.GetExceptionState().ClearException();
  }
  {
    // Test throwing exception if the named outputs is a mix of input and
    // constant operands.
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           scope.GetExceptionState());
    ASSERT_THAT(builder, testing::NotNull());
    auto* input = BuildInput(builder, "input", {3, 4, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* constant =
        BuildConstant(builder, {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, {{"output1", input}, {"output2", constant}});
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
              "The operand with name \"output1\" is not an output operand.");
    scope.GetExceptionState().ClearException();
  }
  {
    // Test throwing exception if two inputs have the same name.
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           scope.GetExceptionState());
    ASSERT_THAT(builder, testing::NotNull());
    auto* a =
        BuildInput(builder, "a", {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "a", {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    const MLOperatorOptions* options = MLOperatorOptions::Create();
    auto* c = builder->add(a, b, options, scope.GetExceptionState());
    ASSERT_THAT(c, testing::NotNull());

    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, {{"c", c}});
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message, "The input name \"a\" is duplicated.");
    scope.GetExceptionState().ClearException();
  }
  {
    // Test building a graph with an elementwise add operator that uses the same
    // input for both lhs and rhs:
    //   [a]
    //   / \
    //   \ /
    //   add
    //    |
    //   [b]
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           scope.GetExceptionState());
    ASSERT_THAT(builder, testing::NotNull());
    auto* a =
        BuildInput(builder, "a", {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    const MLOperatorOptions* options = MLOperatorOptions::Create();
    auto* output = builder->add(a, a, options, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, {{"b", output}});
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputConstraints();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*inputs.at("a"), a->Descriptor());
    const auto& outputs = graph->GetOutputConstraints();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*outputs.at("b"), output->Descriptor());
  }
  {
    // Test building a graph with two operators sharing a same input:
    //      [a]
    //     /   \
    //  relu   sigmoid
    //    |      |
    //   [b]    [c]
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           scope.GetExceptionState());
    ASSERT_THAT(builder, testing::NotNull());
    auto* a =
        BuildInput(builder, "a", {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    const MLOperatorOptions* options = MLOperatorOptions::Create();
    auto* b = builder->relu(a, options, scope.GetExceptionState());
    ASSERT_THAT(b, testing::NotNull());
    auto* c = builder->sigmoid(a, options, scope.GetExceptionState());
    ASSERT_THAT(c, testing::NotNull());
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, {{"b", b}, {"c", c}});
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputConstraints();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*inputs.at("a"), a->Descriptor());
    const auto& outputs = graph->GetOutputConstraints();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(2));
    EXPECT_EQ(*outputs.at("b"), b->Descriptor());
    EXPECT_EQ(*outputs.at("c"), c->Descriptor());
  }
  {
    // Test building a fake graph with two inputs, one gemm operation and one
    // output.
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           scope.GetExceptionState());
    ASSERT_THAT(builder, testing::NotNull());
    auto* a =
        BuildInput(builder, "a", {3, 4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {4, 3}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* c = BuildGemm(scope, builder, a, b);

    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, {{"c", c}});
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputConstraints();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(2));
    EXPECT_EQ(*inputs.at("a"), a->Descriptor());
    EXPECT_EQ(*inputs.at("b"), b->Descriptor());
    const auto& outputs = graph->GetOutputConstraints();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*outputs.at("c"), c->Descriptor());
  }
  {
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           scope.GetExceptionState());
    ASSERT_THAT(builder, testing::NotNull());
    // Test building a fake graph with conv2d, add and relu operations.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* conv2d = BuildConv2d(scope, builder, input, filter);
    auto* bias =
        BuildConstant(builder, {1}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    const MLOperatorOptions* options = MLOperatorOptions::Create();
    auto* add = builder->add(conv2d, bias, options, scope.GetExceptionState());
    ASSERT_THAT(add, testing::NotNull());
    auto* output = builder->relu(add, options, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());

    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, {{"output", output}});
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputConstraints();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*inputs.at("input"), input->Descriptor());
    const auto& outputs = graph->GetOutputConstraints();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*outputs.at("output"), output->Descriptor());
  }
}

// Helper struct to create an ArrayBufferView for MLNamedArrayBufferViews test.
struct ArrayBufferViewHelper {
  size_t number_of_elements;
  V8MLOperandDataType::Enum data_type;

  NotShared<DOMArrayBufferView> ToArrayBufferView() {
    return CreateDOMArrayBufferView(number_of_elements, data_type);
  }
};

TEST_F(MLGraphTest, CreateNamedArrayBufferViewsTest) {
  constexpr auto kOperandDataTypes =
      base::MakeFixedFlatSet<V8MLOperandDataType::Enum>(
          {V8MLOperandDataType::Enum::kFloat32,
           V8MLOperandDataType::Enum::kFloat16,
           V8MLOperandDataType::Enum::kInt32,
           V8MLOperandDataType::Enum::kUint32,
           V8MLOperandDataType::Enum::kInt64,
           V8MLOperandDataType::Enum::kUint64, V8MLOperandDataType::Enum::kInt8,
           V8MLOperandDataType::Enum::kUint8});
  static_assert(kOperandDataTypes.size() == V8MLOperandDataType::kEnumSize,
                "The number of operand data types declared here needs to match "
                "all possible enumeration values defined in the IDL.");

  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  MLContext* context = CreateContext(scope, MLContextOptions::Create());
  auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                         scope.GetExceptionState());
  ASSERT_THAT(builder, testing::NotNull());

  {
    for (auto operand_data_type : kOperandDataTypes) {
      SCOPED_TRACE(testing::Message()
                   << "Testing for MLOperandDataType: "
                   << V8MLOperandDataType(operand_data_type).AsString());
      auto* input = BuildInput(builder, "input", {3, 4}, operand_data_type,
                               scope.GetExceptionState());
      MLNamedArrayBufferViews inputs;
      inputs.emplace_back("input", CreateArrayBufferViewForOperand(input));
      auto inputs_info = TransferNamedArrayBufferViews(
          scope.GetIsolate(), inputs, scope.GetExceptionState());
      ASSERT_THAT(inputs_info, testing::NotNull());
      auto* input_views = CreateNamedArrayBufferViews(std::move(inputs_info));
      ASSERT_THAT(input_views, testing::NotNull());
      EXPECT_EQ((*input_views)[0].first, "input");
      auto input_data_type = (*input_views)[0].second->GetType();
      switch (operand_data_type) {
        case V8MLOperandDataType::Enum::kFloat32:
          EXPECT_EQ(input_data_type, DOMArrayBufferView::kTypeFloat32);
          break;
        case V8MLOperandDataType::Enum::kFloat16:
          EXPECT_EQ(input_data_type, DOMArrayBufferView::kTypeUint16);
          break;
        case V8MLOperandDataType::Enum::kInt32:
          EXPECT_EQ(input_data_type, DOMArrayBufferView::kTypeInt32);
          break;
        case V8MLOperandDataType::Enum::kUint32:
          EXPECT_EQ(input_data_type, DOMArrayBufferView::kTypeUint32);
          break;
        case V8MLOperandDataType::Enum::kInt64:
          EXPECT_EQ(input_data_type, DOMArrayBufferView::kTypeBigInt64);
          break;
        case V8MLOperandDataType::Enum::kUint64:
          EXPECT_EQ(input_data_type, DOMArrayBufferView::kTypeBigUint64);
          break;
        case V8MLOperandDataType::Enum::kInt8:
          EXPECT_EQ(input_data_type, DOMArrayBufferView::kTypeInt8);
          break;
        case V8MLOperandDataType::Enum::kUint8:
          EXPECT_EQ(input_data_type, DOMArrayBufferView::kTypeUint8);
          break;
      }
    }
  }
}

TEST_F(MLGraphTest, ComputeTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  MLContext* context = CreateContext(scope, MLContextOptions::Create());
  auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                         scope.GetExceptionState());
  ASSERT_THAT(builder, testing::NotNull());

  // Build a fake graph represents computation 'c = a * b';
  auto* a =
      BuildInput(builder, "a", {3, 4}, V8MLOperandDataType::Enum::kFloat32,
                 scope.GetExceptionState());
  auto* b =
      BuildInput(builder, "b", {4, 3}, V8MLOperandDataType::Enum::kFloat32,
                 scope.GetExceptionState());
  auto* c = BuildGemm(scope, builder, a, b);
  auto [graph, error_name, error_message] =
      BuildGraph(scope, builder, {{"c", c}});
  ASSERT_THAT(graph, testing::NotNull());
  {
    // Test throwing exception if the inputs is empty.
    MLNamedArrayBufferViews inputs;
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("c", CreateArrayBufferViewForOperand(c));
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
              "Invalid inputs: The number (0) of the array buffer views "
              "doesn't match the expectation (2).");
  }
  {
    // Test throwing exception if the number of inputs doesn't match.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("c", CreateArrayBufferViewForOperand(c));
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
              "Invalid inputs: The number (1) of the array buffer views "
              "doesn't match the expectation (2).");
  }
  {
    // Test throwing exception if the outputs is empty.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
              "Invalid outputs: The number (0) of the array buffer views "
              "doesn't match the expectation (1).");
  }
  {
    // Test throwing exception if the number of outputs doesn't match.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("c", CreateArrayBufferViewForOperand(c));
    outputs.emplace_back("d", CreateArrayBufferViewForOperand(c));
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
              "Invalid outputs: The number (2) of the array buffer views "
              "doesn't match the expectation (1).");
  }
  {
    // Test throwing exception if the input name is unknown.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    inputs.emplace_back("invalid-input-name",
                        CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("c", CreateArrayBufferViewForOperand(c));
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
              "Invalid inputs: The name \"invalid-input-name\" isn't part of "
              "the graph.");
  }
  {
    // Test throwing exception if the output name is unknown.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("invalid-output-name",
                         CreateArrayBufferViewForOperand(c));
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
              "Invalid outputs: The name \"invalid-output-name\" isn't part of "
              "the graph.");
  }
  {
    // Test throwing exception if the input array buffer view type is wrong.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back(
        "a",
        ArrayBufferViewHelper{.number_of_elements = 12,
                              .data_type = V8MLOperandDataType::Enum::kInt32}
            .ToArrayBufferView());
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("c", CreateArrayBufferViewForOperand(c));
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(
        error_message,
        "Invalid inputs: The type (Int32) of the array buffer view with "
        "name \"a\" doesn't match the expected operand data type (float32).");
  }
  {
    // Test throwing exception if the input array buffer view size is wrong.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back(
        "a",
        ArrayBufferViewHelper{.number_of_elements = 10,
                              .data_type = V8MLOperandDataType::Enum::kFloat32}
            .ToArrayBufferView());
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("c", CreateArrayBufferViewForOperand(c));
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
              "Invalid inputs: The byte length (40) of the array buffer view "
              "with name \"a\" doesn't match the expected byte length (48).");
  }
  {
    // Test throwing exception if the output array buffer view type is wrong.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back(
        "c",
        ArrayBufferViewHelper{.number_of_elements = 9,
                              .data_type = V8MLOperandDataType::Enum::kInt32}
            .ToArrayBufferView());
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(
        error_message,
        "Invalid outputs: The type (Int32) of the array buffer view with "
        "name \"c\" doesn't match the expected operand data type (float32).");
  }
  {
    // Test throwing exception if the output array buffer view size is wrong.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back(
        "c",
        ArrayBufferViewHelper{.number_of_elements = 8,
                              .data_type = V8MLOperandDataType::Enum::kFloat32}
            .ToArrayBufferView());
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
              "Invalid outputs: The byte length (32) of the array buffer view "
              "with name \"c\" doesn't match the expected byte length (36).");
  }
}

TEST_F(MLGraphTest, CreateWebNNBufferTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* script_state = scope.GetScriptState();

  MLContext* ml_context = CreateContext(scope, options);

  auto* desc = MLBufferDescriptor::Create();
  desc->setDataType(V8MLOperandDataType::Enum::kFloat32);
  desc->setDimensions({2, 2});

  ScriptPromiseTester buffer_tester(
      script_state,
      ml_context->createBuffer(script_state, desc, scope.GetExceptionState()));
  buffer_tester.WaitUntilSettled();
  EXPECT_TRUE(buffer_tester.IsFulfilled());

  if (scope.GetExceptionState().Code() ==
      ToExceptionCode(DOMExceptionCode::kNotSupportedError)) {
    GTEST_SKIP() << "MLBuffer has not been implemented on this platform.";
  }

  MLBuffer* ml_buffer = V8ToObject<MLBuffer>(&scope, buffer_tester.Value());

  ASSERT_THAT(ml_buffer, testing::NotNull());
  EXPECT_EQ(ml_buffer->dataType(), desc->dataType());
  EXPECT_EQ(ml_buffer->shape(), desc->dimensions());
}

TEST_F(MLGraphTest, WriteWebNNBufferTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* script_state = scope.GetScriptState();

  MLContext* ml_context = CreateContext(scope, options);

  constexpr size_t kBufferSize = 4ull;
  const Vector<uint32_t> kBufferShape{2, 2};

  auto* desc = MLBufferDescriptor::Create();
  desc->setDataType(V8MLOperandDataType::Enum::kUint8);
  desc->setDimensions(kBufferShape);

  ScriptPromiseTester buffer_tester(
      script_state,
      ml_context->createBuffer(script_state, desc, scope.GetExceptionState()));
  buffer_tester.WaitUntilSettled();
  EXPECT_TRUE(buffer_tester.IsFulfilled());

  if (scope.GetExceptionState().Code() ==
      ToExceptionCode(DOMExceptionCode::kNotSupportedError)) {
    GTEST_SKIP() << "MLBuffer has not been implemented on this platform.";
  }

  MLBuffer* ml_buffer = V8ToObject<MLBuffer>(&scope, buffer_tester.Value());

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
TEST_F(MLGraphTest, WriteWebNNBufferThenDestroyTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* script_state = scope.GetScriptState();

  MLContext* ml_context = CreateContext(scope, options);

  auto* desc = MLBufferDescriptor::Create();
  desc->setDataType(V8MLOperandDataType::Enum::kUint8);
  desc->setDimensions({2, 2});

  ScriptPromiseTester buffer_tester(
      script_state,
      ml_context->createBuffer(script_state, desc, scope.GetExceptionState()));
  buffer_tester.WaitUntilSettled();
  EXPECT_TRUE(buffer_tester.IsFulfilled());

  if (scope.GetExceptionState().Code() ==
      ToExceptionCode(DOMExceptionCode::kNotSupportedError)) {
    GTEST_SKIP() << "MLBuffer has not been implemented on this platform.";
  }

  MLBuffer* ml_buffer = V8ToObject<MLBuffer>(&scope, buffer_tester.Value());

  ASSERT_THAT(ml_buffer, testing::NotNull());

  ml_buffer->destroy();

  ml_context->writeBuffer(
      script_state, ml_buffer,
      CreateDOMArrayBufferView(ml_buffer->PackedByteLength(),
                               V8MLOperandDataType::Enum::kUint8)
          ->BufferBase(),
      /*src_byte_offset=*/0, scope.GetExceptionState());
}

// Reading data from an array buffer to a destroyed MLBuffer should not crash.
TEST_F(MLGraphTest, ReadWebNNBufferThenDestroyTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* script_state = scope.GetScriptState();

  MLContext* ml_context = CreateContext(scope, options);

  auto* desc = MLBufferDescriptor::Create();
  desc->setDataType(V8MLOperandDataType::Enum::kFloat32);
  desc->setDimensions({2, 2});

  ScriptPromiseTester create_buffer_tester(
      script_state,
      ml_context->createBuffer(script_state, desc, scope.GetExceptionState()));
  create_buffer_tester.WaitUntilSettled();
  EXPECT_TRUE(create_buffer_tester.IsFulfilled());

  if (scope.GetExceptionState().Code() ==
      ToExceptionCode(DOMExceptionCode::kNotSupportedError)) {
    GTEST_SKIP() << "MLBuffer has not been implemented on this platform.";
  }

  MLBuffer* ml_buffer =
      V8ToObject<MLBuffer>(&scope, create_buffer_tester.Value());

  ASSERT_THAT(ml_buffer, testing::NotNull());

  ml_buffer->destroy();

  ScriptPromise<DOMArrayBuffer> read_promise = ml_context->readBuffer(
      script_state, ml_buffer, scope.GetExceptionState());
  EXPECT_TRUE(read_promise.IsEmpty());
}

TEST_F(MLGraphTest, WebNNGraphDispatchTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  MLContext* ml_context = CreateContext(scope, options);
  auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), ml_context,
                                         scope.GetExceptionState());
  ASSERT_THAT(builder, testing::NotNull());
  const Vector<uint32_t> dimensions = {3, 5};
  const wtf_size_t number_of_elements = 15;

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

  // Check if MLBuffer is supported.
  MLBuffer* input_buffer =
      CreateMLBufferForOperand(scope, ml_context, lhs_operand);

  if (scope.GetExceptionState().Code() ==
      ToExceptionCode(DOMExceptionCode::kNotSupportedError)) {
    GTEST_SKIP() << "MLBuffer has not been implemented on this platform.";
  }

  ASSERT_THAT(input_buffer, testing::NotNull());

  MLNamedBuffers inputs(
      {{"lhs", input_buffer},
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

TEST_F(MLGraphTest, CreateWebNNGraphTest) {
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
    MLGraph* ml_graph = V8ToObject<MLGraph>(&scope, tester.Value());
    ASSERT_THAT(ml_graph, testing::NotNull());
    EXPECT_TRUE(scoped_setup_binder.IsWebNNContextBound());
  }
}

struct ClampOptions {
  std::optional<float> min_value;
  std::optional<float> max_value;
};

struct SoftmaxTester {
  OperandInfo<float> input;
  webnn::OperandDescriptor expected_descriptor;

  void Test(MLGraphTest& helper, V8TestingScope& scope, MLContext* context) {
    // Build the graph.
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           scope.GetExceptionState());
    ASSERT_THAT(builder, testing::NotNull());
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    const MLOperatorOptions* options = MLOperatorOptions::Create();
    auto* output_operand =
        builder->softmax(input_operand, options, scope.GetExceptionState());
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
    EXPECT_EQ(output_operand_iter->value->descriptor, expected_descriptor);
  }
};

TEST_F(MLGraphTest, SoftmaxTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  MLContext* context = CreateContext(scope, options);

  {
    // Test building softmax with float32 input.
    SoftmaxTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 4}},
        .expected_descriptor = ToDescriptor(webnn::OperandDataType::kFloat32,
                                            std::array<uint32_t, 2>{2, 4})}
        .Test(*this, scope, context);
  }
  {
    // Test building softmax with float16 input.
    SoftmaxTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {1, 5}},
        .expected_descriptor = ToDescriptor(webnn::OperandDataType::kFloat16,
                                            std::array<uint32_t, 2>{1, 5})}
        .Test(*this, scope, context);
  }
}

template <typename T>
struct ConstantTester {
  OperandInfo<T> constant;
  webnn::OperandDescriptor expected_descriptor;
  Vector<T> expected_constant_data;

  void Test(MLGraphTest& helper, V8TestingScope& scope, MLContext* context) {
    // Build the graph.
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           scope.GetExceptionState());
    ASSERT_THAT(builder, testing::NotNull());
    auto* constant_operand =
        BuildConstant(builder, constant.dimensions, constant.data_type,
                      constant.values, scope.GetExceptionState());
    const MLOperatorOptions* options = MLOperatorOptions::Create();
    auto* output_operand =
        builder->relu(constant_operand, options, scope.GetExceptionState());
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
      EXPECT_EQ(constant_operand_iter->value->descriptor, expected_descriptor);
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

TEST_F(MLGraphTest, ConstantTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  MLContext* context = CreateContext(scope, options);

  {  // Test scalar constant operand.
    ConstantTester<float>{
        .constant = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {},
                     .values = {1.0}},
        .expected_descriptor = ToDescriptor(webnn::OperandDataType::kFloat32,
                                            std::array<uint32_t, 0>{}),
        .expected_constant_data = {1.0}}
        .Test(*this, scope, context);
  }
  {
    // Test Constant operand for Float32 data type.
    ConstantTester<float>{
        .constant = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {2, 3},
                     .values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0}},
        .expected_descriptor = ToDescriptor(webnn::OperandDataType::kFloat32,
                                            std::array<uint32_t, 2>{2, 3}),
        .expected_constant_data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0}}
        .Test(*this, scope, context);
  }
  {
    // Test Constant operand for Float16 data type.
    ConstantTester<uint16_t>{
        .constant = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                     .dimensions = {2, 3},
                     .values = {1, 2, 3, 4, 5, 6}},
        .expected_descriptor = ToDescriptor(webnn::OperandDataType::kFloat16,
                                            std::array<uint32_t, 2>{2, 3}),
        .expected_constant_data = {1, 2, 3, 4, 5, 6}}
        .Test(*this, scope, context);
  }
  {
    // Test Constant operand for Int32 data type.
    ConstantTester<int32_t>{
        .constant = {.data_type = V8MLOperandDataType::Enum::kInt32,
                     .dimensions = {2, 3},
                     .values = {1, 2, 3, 4, 5, 6}},
        .expected_descriptor = ToDescriptor(webnn::OperandDataType::kInt32,
                                            std::array<uint32_t, 2>{2, 3}),
        .expected_constant_data = {1, 2, 3, 4, 5, 6}}
        .Test(*this, scope, context);
  }
  {
    // Test Constant operand for Int8 data type.
    ConstantTester<int8_t>{
        .constant = {.data_type = V8MLOperandDataType::Enum::kInt8,
                     .dimensions = {2, 3},
                     .values = {1, 2, 3, 4, 5, 6}},
        .expected_descriptor = ToDescriptor(webnn::OperandDataType::kInt8,
                                            std::array<uint32_t, 2>{2, 3}),
        .expected_constant_data = {1, 2, 3, 4, 5, 6}}
        .Test(*this, scope, context);
  }
}

struct CastTester {
  OperandInfo<float> input;
  V8MLOperandDataType::Enum output_data_type;
  webnn::OperandDescriptor expected_descriptor;

  void Test(MLGraphTest& helper, V8TestingScope& scope, MLContext* context) {
    // Build the graph.
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           scope.GetExceptionState());
    ASSERT_THAT(builder, testing::NotNull());
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    const MLOperatorOptions* options = MLOperatorOptions::Create();
    auto* output_operand =
        builder->cast(input_operand, V8MLOperandDataType(output_data_type),
                      options, scope.GetExceptionState());
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
    EXPECT_EQ(output_operand_iter->value->descriptor, expected_descriptor);
  }
};

TEST_F(MLGraphTest, CastTester) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  MLContext* context = CreateContext(scope, options);

  const std::array<uint32_t, 2> shape{2, 2};
  const Vector<uint32_t> wtf_shape(shape);
  {
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kFloat16,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kFloat16, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kUint32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kUint32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt8,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt8, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kUint8,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kUint8, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kFloat32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kFloat32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kUint32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kUint32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt8,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt8, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kUint8,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kUint8, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kFloat32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kFloat32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kFloat16,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kFloat16, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kUint32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kUint32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt8,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt8, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kUint8,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kUint8, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kFloat32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kFloat32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kFloat16,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kFloat16, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt8,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt8, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kUint8,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kUint8, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt8,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kFloat32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kFloat32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt8,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kFloat16,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kFloat16, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt8,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kUint32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kUint32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt8,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kUint8,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kUint8, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt8,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kFloat32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kFloat32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kFloat16,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kFloat16, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt8,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt8, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt32, shape)}
        .Test(*this, scope, context);
  }
}

TEST_F(MLGraphTest, WebNNGraphComputeTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  MLContext* context = CreateContext(scope, options);
  auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                         scope.GetExceptionState());
  ASSERT_THAT(builder, testing::NotNull());
  const Vector<uint32_t> dimensions = {3, 5};
  const wtf_size_t number_of_elements = 15;

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

}  // namespace blink
