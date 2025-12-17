// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"

#include <limits.h>

#include <array>
#include <numeric>
#include <optional>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/features.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_device.mojom-blink-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom-blink.h"
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
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
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
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_tensor_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_transpose_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_triangular_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/ml/buildflags.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder_test_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/constant_folding_transformer.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/ml_graph_transformer.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/qdq_detection_transformer.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/transpose_elimination_transformer.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/utils/ml_graph_dump.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_type_converter.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_tensor.h"
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

class FakeWebNNTensor;

namespace {

static constexpr webnn::SupportedRanks kMaxRank =
    webnn::SupportedRanks::UpTo(8);

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
  HashMap<String, Vector<uint8_t>> output;
};

template <typename T>
struct OperandInfo {
  V8MLOperandDataType::Enum data_type;
  Vector<uint32_t> dimensions;
  Vector<T> values;
};

webnn::OperandDescriptor ToDescriptor(webnn::OperandDataType data_type,
                                      base::span<const uint32_t> shape) {
  return webnn::OperandDescriptor::UnsafeCreateForTesting(data_type, shape);
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
      NOTREACHED();
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
void SetArrayBufferViewValues(MaybeShared<DOMArrayBufferView> array_buffer_view,
                              const Vector<T>& values) {
  DCHECK_EQ(array_buffer_view->byteLength(), values.size() * sizeof(T));
  memcpy(array_buffer_view->BaseAddress(), values.data(),
         values.size() * sizeof(T));
}

// Helper function to create an ArrayBufferView given an operand.
MaybeShared<DOMArrayBufferView> CreateArrayBufferViewForOperand(
    const MLOperand* operand) {
  return CreateDOMArrayBufferView(operand->NumberOfElements(),
                                  operand->dataType().AsEnum());
}

// Overrode helper function to create an ArrayBufferView given an operand and
// set its data from a vector.
template <typename T>
MaybeShared<DOMArrayBufferView> CreateArrayBufferViewForOperand(
    const MLOperand* operand,
    const Vector<T>& values) {
  auto array_buffer_view = CreateArrayBufferViewForOperand(operand);
  SetArrayBufferViewValues(array_buffer_view, values);
  return array_buffer_view;
}

// Helper function to get the data of an ArrayBufferView into a vector.
template <typename T>
Vector<T> GetArrayBufferViewValues(
    MaybeShared<DOMArrayBufferView> array_buffer_view) {
  Vector<T> values(base::checked_cast<wtf_size_t>(
      array_buffer_view->byteLength() / array_buffer_view->TypeSize()));
  UNSAFE_TODO(memcpy(values.data(), array_buffer_view->BaseAddress(),
                     array_buffer_view->byteLength()));
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

MLOperand* BuildGemm(V8TestingScope& scope,
                     MLGraphBuilder* builder,
                     MLOperand* a,
                     MLOperand* b,
                     MLGemmOptions* options = MLGemmOptions::Create()) {
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
    MLOperand* a,
    MLOperand* b,
    webnn::mojom::blink::ElementWiseBinary::Kind kind,
    MLOperatorOptions* options) {
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
    case webnn::mojom::blink::ElementWiseBinary::Kind::kNotEqual:
      return builder->notEqual(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLogicalAnd:
      return builder->logicalAnd(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLogicalOr:
      return builder->logicalOr(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLogicalXor:
      return builder->logicalXor(a, b, options, scope.GetExceptionState());
  }
}

MLOperand* BuildElementWiseBinary(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    webnn::mojom::blink::ElementWiseBinary::Kind kind,
    MLOperand* a,
    MLOperand* b,
    MLOperatorOptions* options = MLOperatorOptions::Create()) {
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
                         MLNamedOperands& named_operands) {
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
  base::test::ScopedFeatureList scoped_feature_list_{
      webnn::mojom::features::kWebMachineLearningNeuralNetwork};
  test::TaskEnvironment task_environment_;

  blink_mojom::GraphInfoPtr graph_info_;
  HashMap<String, mojo_base::BigBuffer> input_array_buffers_;
  ComputeResult compute_result_;
};

class WebNNContextHelper {
 public:
  WebNNContextHelper();
  ~WebNNContextHelper();

  void ConnectWebNNTensorImpl(const blink::WebNNTensorToken& handle,
                              std::unique_ptr<FakeWebNNTensor> tensor);

  void DisconnectAndDestroyWebNNTensorImpl(
      const blink::WebNNTensorToken& handle);

 private:
  std::map<blink::WebNNTensorToken, std::unique_ptr<FakeWebNNTensor>>
      tensor_impls_;

  mojo::UniqueAssociatedReceiverSet<blink_mojom::WebNNGraphBuilder> builders_;
};

class FakeWebNNGraph : public blink_mojom::WebNNGraph {
 public:
  explicit FakeWebNNGraph(MLGraphTest& helper) : helper_(helper) {}
  FakeWebNNGraph(const FakeWebNNGraph&) = delete;
  FakeWebNNGraph(FakeWebNNGraph&&) = delete;
  ~FakeWebNNGraph() override = default;

 private:
  // Just return for testing the validation of inputs and outputs.
  void Dispatch(
      const HashMap<String, blink::WebNNTensorToken>& named_inputs,
      const HashMap<String, blink::WebNNTensorToken>& named_outputs) override {}

  // TODO(crbug.com/354741414): Fix this dangling pointer.
  const raw_ref<MLGraphTest, DanglingUntriaged> helper_;
};

class FakeWebNNTensor : public blink_mojom::WebNNTensor {
 public:
  FakeWebNNTensor(
      WebNNContextHelper& helper,
      mojo::PendingAssociatedReceiver<blink_mojom::WebNNTensor> receiver,
      const blink::WebNNTensorToken& tensor_handle,
      blink_mojom::TensorInfoPtr tensor_info)
      : helper_(helper),
        receiver_(this, std::move(receiver)),
        handle_(tensor_handle) {
    buffer_ = mojo_base::BigBuffer(tensor_info->descriptor.PackedByteLength());
    receiver_.set_disconnect_handler(
        BindOnce(&FakeWebNNTensor::OnConnectionError, Unretained(this)));
  }

  ~FakeWebNNTensor() override = default;

  FakeWebNNTensor(const FakeWebNNTensor&) = delete;
  FakeWebNNTensor(FakeWebNNTensor&&) = delete;

  const blink::WebNNTensorToken& handle() const { return handle_; }

 private:
  void ReadTensor(ReadTensorCallback callback) override {
    mojo_base::BigBuffer dst_buffer(buffer_.byte_span());

    std::move(callback).Run(
        blink_mojom::ReadTensorResult::NewBuffer(std::move(dst_buffer)));
  }

  void WriteTensor(mojo_base::BigBuffer src_buffer) override {
    ASSERT_LE(src_buffer.size(), buffer_.size());
    base::span(buffer_).copy_prefix_from(src_buffer);
  }

  void ExportTensor(ExportTensorCallback callback) override {
    NOTIMPLEMENTED();
  }

  void ImportTensor(const gpu::SyncToken& sync_token_fence) override {
    NOTIMPLEMENTED();
  }

  void OnConnectionError() {
    helper_->DisconnectAndDestroyWebNNTensorImpl(handle());
  }

  // TODO(crbug.com/354741414): Fix this dangling pointer.
  const raw_ref<WebNNContextHelper, DanglingUntriaged> helper_;

  mojo::AssociatedReceiver<blink_mojom::WebNNTensor> receiver_;

  const blink::WebNNTensorToken handle_;

  mojo_base::BigBuffer buffer_;
};

WebNNContextHelper::WebNNContextHelper() = default;
WebNNContextHelper::~WebNNContextHelper() = default;

void WebNNContextHelper::ConnectWebNNTensorImpl(
    const blink::WebNNTensorToken& handle,
    std::unique_ptr<FakeWebNNTensor> tensor) {
  const auto it = tensor_impls_.find(handle);
  ASSERT_TRUE(it == tensor_impls_.end());
  tensor_impls_.try_emplace(handle, std::move(tensor));
}

void WebNNContextHelper::DisconnectAndDestroyWebNNTensorImpl(
    const blink::WebNNTensorToken& handle) {
  tensor_impls_.erase(handle);
}

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

    auto success = blink_mojom::CreateGraphSuccess::New(
        std::move(blink_remote), Vector<blink_mojom::Device>());
    std::move(callback).Run(std::move(success));
  }

  void CreatePendingConstant(const WebNNPendingConstantToken& constant_handle,
                             webnn::OperandDataType data_type,
                             mojo_base::BigBuffer data) override {
    NOTIMPLEMENTED();
  }

  void IsValidGraphForTesting(
      const webnn::ContextProperties& context_properties,
      webnn::mojom::blink::GraphInfoPtr graph_info,
      IsValidGraphForTestingCallback callback) override {
    NOTIMPLEMENTED();
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

  void CreateTensor(blink_mojom::TensorInfoPtr tensor_info,
                    mojo_base::BigBuffer tensor_data,
                    CreateTensorCallback callback) override {
    mojo::PendingAssociatedRemote<blink_mojom::WebNNTensor> blink_remote;
    auto blink_receiver = blink_remote.InitWithNewEndpointAndPassReceiver();
    blink::WebNNTensorToken tensor_handle;
    context_helper_.ConnectWebNNTensorImpl(
        tensor_handle, std::make_unique<FakeWebNNTensor>(
                           context_helper_, std::move(blink_receiver),
                           tensor_handle, std::move(tensor_info)));

    auto success = blink_mojom::CreateTensorSuccess::New(
        std::move(blink_remote), std::move(tensor_handle));
    std::move(callback).Run(
        blink_mojom::CreateTensorResult::NewSuccess(std::move(success)));
  }

  void CreateTensorFromMailbox(blink_mojom::TensorInfoPtr tensor_info,
                               const ::gpu::Mailbox& mailbox,
                               const gpu::SyncToken& fence,
                               CreateTensorCallback callback) override {
    NOTIMPLEMENTED();
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
    receiver_.set_disconnect_handler(BindOnce(
        &FakeWebNNContextProvider::OnConnectionError, Unretained(this)));
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
        webnn::InputOperandLayout::kNchw, webnn::Resample2DAxes::kAny,
        webnn::BatchNormalizationAxis::kAny,
        /*tensor_byte_length_limit=*/INT_MAX,
        {/*input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*constant=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*arg_min_max_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*arg_min_max_output=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*batch_normalization_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*batch_normalization_mean=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*cast_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*clamp_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*concat_inputs=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*conv2d_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*conv2d_bias=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*conv_transpose2d_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*conv_transpose2d_bias=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*cumulative_sum_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*dequantize_linear_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*dequantize_linear_scale=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*dequantize_linear_zero_point=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*add_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*sub_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*mul_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*div_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*max_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*min_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*pow_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*equal_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*greater_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*greater_or_equal_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*lesser_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*lesser_or_equal_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*not_equal_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*logical_and_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*logical_or_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*logical_xor_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*logical_not_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*is_nan_input*/ {webnn::SupportedDataTypes::All(), kMaxRank},
         /*is_infinite_input*/ {webnn::SupportedDataTypes::All(), kMaxRank},
         /*logical_output=*/webnn::SupportedDataTypes::All(),
         /*abs_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*ceil_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*cos_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*erf_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*exp_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*floor_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*identity_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*log_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*neg_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reciprocal_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*round_even_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*sign_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*sin_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*sqrt_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*tan_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*elu_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*expand_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*gather_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gather_indices=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gather_elements_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gather_elements_indices=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*gather_nd_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gather_nd_indices=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gelu_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gemm_a=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gemm_c=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gru_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gru_bias=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gru_output_sequence=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gru_cell_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gru_cell_bias=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*hard_sigmoid_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*hard_swish_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*instance_normalization_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*instance_normalization_scale=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*layer_normalization_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*leaky_relu_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*linear_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*lstm_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*lstm_bias=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*lstm_output_sequence=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*lstm_cell_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*lstm_cell_bias=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*matmul_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*pad_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*average_pool2d_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*l2_pool2d_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*max_pool2d_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*prelu_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*quantize_linear_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*quantize_linear_zero_point=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reduce_l1_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reduce_l2_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reduce_log_sum_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reduce_log_sum_exp_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reduce_max_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reduce_mean_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reduce_min_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reduce_product_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reduce_sum_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reduce_sum_square_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*relu_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*resample2d_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reshape_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reverse_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*scatter_elements_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*scatter_elements_indices=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*scatter_nd_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*scatter_nd_indices=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*scatter_nd_updates=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*sigmoid_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*slice_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*softmax_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*softplus_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*softsign_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*split_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*tanh_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*tile_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*transpose_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*triangular_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*where_condition=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*where_value=*/{webnn::SupportedDataTypes::All(), kMaxRank}});
    auto success = blink_mojom::CreateContextSuccess::New(
        std::move(blink_remote), std::move(context_properties),
        blink::WebNNContextToken(), mojo::ScopedDataPipeProducerHandle(),
        mojo::ScopedDataPipeConsumerHandle());
    std::move(callback).Run(
        blink_mojom::CreateContextResult::NewSuccess(std::move(success)));
  }

  const raw_ref<MLGraphTest> helper_;
  mojo::Receiver<blink_mojom::WebNNContextProvider> receiver_;
};

class ScopedWebNNServiceBinder {
 public:
  explicit ScopedWebNNServiceBinder(MLGraphTest& helper, V8TestingScope& scope)
      : fake_webnn_context_provider_(
            std::make_unique<FakeWebNNContextProvider>(helper)),
        interface_broker_(
            scope.GetExecutionContext()->GetBrowserInterfaceBroker()) {
    interface_broker_->SetBinderForTesting(
        blink_mojom::WebNNContextProvider::Name_,
        BindRepeating(&FakeWebNNContextProvider::BindRequest,
                      Unretained(fake_webnn_context_provider_.get())));
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

  auto* lhs_operand = BuildInput(scope.GetScriptState(), builder, "lhs",
                                 {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
  auto* rhs_operand = BuildInput(scope.GetScriptState(), builder, "rhs",
                                 {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
  MLOperatorOptions* options = MLOperatorOptions::Create();
  auto* output = builder->add(lhs_operand, rhs_operand, options,
                              scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  MLNamedOperands named_outputs = {{"output", output}};
  return builder->build(scope.GetScriptState(), named_outputs,
                        scope.GetExceptionState());
}

bool IsBufferDataEqual(DOMArrayBuffer* array_buffer,
                       base::span<const uint8_t> expected_data) {
  return array_buffer->ByteSpan() == expected_data;
}

// Checks the contents of a MLTensor.
// Returns false if unable to download or the tensor data did not match
// expected.
bool DownloadMLTensorAndCheck(V8TestingScope& scope,
                              MLContext* context,
                              MLTensor* src_tensor,
                              base::span<const uint8_t> expected_data) {
  auto* script_state = scope.GetScriptState();
  ScriptPromiseTester tester(
      script_state,
      context->readTensor(script_state, src_tensor, scope.GetExceptionState()));
  tester.WaitUntilSettled();
  if (tester.IsRejected()) {
    return false;
  }
  EXPECT_TRUE(tester.IsFulfilled());
  auto* array_buffer = V8ToObject<DOMArrayBuffer>(&scope, tester.Value());
  return IsBufferDataEqual(array_buffer, expected_data);
}

MLTensor* CreateMLTensorForOperand(V8TestingScope& scope,
                                   MLContext* ml_context,
                                   const MLOperand* operand) {
  auto array_buffer_view = CreateArrayBufferViewForOperand(operand);
  auto* desc = MLTensorDescriptor::Create();
  desc->setDataType(operand->dataType());
  desc->setShape(operand->shape());
  desc->setReadable(true);
  desc->setWritable(true);

  ScriptPromiseTester tester(
      scope.GetScriptState(),
      ml_context->createTensor(scope.GetScriptState(), desc,
                               scope.GetExceptionState()));
  tester.WaitUntilSettled();
  CHECK(tester.IsFulfilled());

  MLTensor* ml_tensor = V8ToObject<MLTensor>(&scope, tester.Value());

  auto* src_data =
      MakeGarbageCollected<AllowSharedBufferSource>(array_buffer_view);
  ml_context->writeTensor(scope.GetScriptState(), ml_tensor, src_data,
                          scope.GetExceptionState());
  return ml_tensor;
}

Vector<uint8_t> GetMLTensorValues(V8TestingScope& scope,
                                  MLContext* ml_context,
                                  MLTensor* ml_tensor) {
  ScriptPromiseTester tester(
      scope.GetScriptState(),
      ml_context->readTensor(scope.GetScriptState(), ml_tensor,
                             scope.GetExceptionState()));
  tester.WaitUntilSettled();
  if (tester.IsRejected()) {
    return {};
  }
  auto* array_buffer = V8ToObject<DOMArrayBuffer>(&scope, tester.Value());
  return GetArrayBufferViewValues<uint8_t>(
      MaybeShared<DOMArrayBufferView>(blink::DOMUint8Array::Create(
          array_buffer, /*byte_offset=*/0, ml_tensor->PackedByteLength())));
}

TEST_F(MLGraphTest, BuildTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  MLContext* context = CreateContext(scope, MLContextOptions::Create());
  {
    // Test throwing exception if the named outputs is empty.
    DummyExceptionStateForTesting exception_state;
    MLNamedOperands named_outputs;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message, "At least one output needs to be provided.");
  }
  {
    // Test throwing exception if the named output is an input operand.
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());
    auto* input =
        BuildInput(scope.GetScriptState(), builder, "input", {3, 4, 5},
                   V8MLOperandDataType::Enum::kFloat32, exception_state);
    MLNamedOperands named_outputs = {{
        "output",
        input,
    }};
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
              "The operand with name \"output\" is not an output operand.");
  }
  {
    // Test throwing exception if two inputs have the same name.
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());
    auto* a = BuildInput(scope.GetScriptState(), builder, "a", {3, 4, 5},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* b = BuildInput(scope.GetScriptState(), builder, "a", {3, 4, 5},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    MLOperatorOptions* options = MLOperatorOptions::Create();
    auto* c = builder->add(a, b, options, exception_state);
    ASSERT_THAT(c, testing::NotNull());

    MLNamedOperands named_outputs = {{"c", c}};
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message, "The input name \"a\" is duplicated.");
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
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());
    auto* a = BuildInput(scope.GetScriptState(), builder, "a", {3, 4, 5},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    MLOperatorOptions* options = MLOperatorOptions::Create();
    auto* output = builder->add(a, a, options, exception_state);
    ASSERT_THAT(output, testing::NotNull());
    MLNamedOperands named_outputs = {{"b", output}};
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
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
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());
    auto* a = BuildInput(scope.GetScriptState(), builder, "a", {3, 4, 5},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    MLOperatorOptions* options = MLOperatorOptions::Create();
    auto* b = builder->relu(a, options, exception_state);
    ASSERT_THAT(b, testing::NotNull());
    auto* c = builder->sigmoid(a, options, exception_state);
    ASSERT_THAT(c, testing::NotNull());
    MLNamedOperands named_outputs = {{"b", b}, {"c", c}};
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
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
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());
    auto* a = BuildInput(scope.GetScriptState(), builder, "a", {3, 4},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* b = BuildInput(scope.GetScriptState(), builder, "b", {4, 3},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* c = BuildGemm(scope, builder, a, b);

    MLNamedOperands named_outputs = {{"c", c}};
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputConstraints();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(2));
    EXPECT_EQ(*inputs.at("a"), a->Descriptor());
    EXPECT_EQ(*inputs.at("b"), b->Descriptor());
    const auto& outputs = graph->GetOutputConstraints();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*outputs.at("c"), c->Descriptor());
  }
}

TEST_F(MLGraphTest, CreateWebNNTensorTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* script_state = scope.GetScriptState();

  MLContext* ml_context = CreateContext(scope, options);

  auto* desc = MLTensorDescriptor::Create();
  desc->setDataType(V8MLOperandDataType::Enum::kFloat32);
  desc->setShape({2, 2});

  ScriptPromiseTester tensor_tester(
      script_state,
      ml_context->createTensor(script_state, desc, scope.GetExceptionState()));
  tensor_tester.WaitUntilSettled();
  EXPECT_TRUE(tensor_tester.IsFulfilled());

  MLTensor* ml_tensor = V8ToObject<MLTensor>(&scope, tensor_tester.Value());
  ASSERT_THAT(ml_tensor, testing::NotNull());
  EXPECT_EQ(ml_tensor->dataType(), desc->dataType());
  EXPECT_EQ(ml_tensor->shape(), desc->shape());
}

TEST_F(MLGraphTest, WriteWebNNTensorTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* script_state = scope.GetScriptState();

  MLContext* ml_context = CreateContext(scope, options);

  constexpr size_t kTensorSize = 4ull;
  const Vector<uint32_t> kTensorShape{2, 2};

  auto* desc = MLTensorDescriptor::Create();
  desc->setDataType(V8MLOperandDataType::Enum::kUint8);
  desc->setShape(kTensorShape);
  desc->setReadable(true);
  desc->setWritable(true);

  ScriptPromiseTester tensor_tester(
      script_state,
      ml_context->createTensor(script_state, desc, scope.GetExceptionState()));
  tensor_tester.WaitUntilSettled();
  EXPECT_TRUE(tensor_tester.IsFulfilled());

  MLTensor* ml_tensor = V8ToObject<MLTensor>(&scope, tensor_tester.Value());
  ASSERT_THAT(ml_tensor, testing::NotNull());

  std::array<const uint8_t, kTensorSize> input_data = {0xAA, 0xAA, 0xAA, 0xAA};
  DOMArrayBuffer* array_buffer = DOMArrayBuffer::Create(input_data);
  ASSERT_THAT(array_buffer, testing::NotNull());

  // Write data to the tensor.
  auto* src_data = MakeGarbageCollected<AllowSharedBufferSource>(array_buffer);
  ml_context->writeTensor(script_state, ml_tensor, src_data,
                          scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(
      DownloadMLTensorAndCheck(scope, ml_context, ml_tensor, input_data));

  // Write different data to the tensor.
  std::array<const uint8_t, kTensorSize> new_data = {0xAA, 0xCC, 0xBB, 0xBB};
  DOMArrayBuffer* new_array_buffer = DOMArrayBuffer::Create(new_data);
  ASSERT_THAT(new_array_buffer, testing::NotNull());
  auto* new_src_data =
      MakeGarbageCollected<AllowSharedBufferSource>(new_array_buffer);
  ml_context->writeTensor(script_state, ml_tensor, new_src_data,
                          scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(DownloadMLTensorAndCheck(scope, ml_context, ml_tensor, new_data));
}

// Writing data from an array buffer to a destroyed MLTensor should not crash.
TEST_F(MLGraphTest, WriteWebNNTensorThenDestroyTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* script_state = scope.GetScriptState();

  MLContext* ml_context = CreateContext(scope, options);

  auto* desc = MLTensorDescriptor::Create();
  desc->setDataType(V8MLOperandDataType::Enum::kUint8);
  desc->setShape({2, 2});
  desc->setWritable(true);

  ScriptPromiseTester tensor_tester(
      script_state,
      ml_context->createTensor(script_state, desc, scope.GetExceptionState()));
  tensor_tester.WaitUntilSettled();
  EXPECT_TRUE(tensor_tester.IsFulfilled());

  MLTensor* ml_tensor = V8ToObject<MLTensor>(&scope, tensor_tester.Value());
  ASSERT_THAT(ml_tensor, testing::NotNull());

  ml_tensor->destroy();

  auto* src_data =
      MakeGarbageCollected<AllowSharedBufferSource>(CreateDOMArrayBufferView(
          ml_tensor->PackedByteLength(), V8MLOperandDataType::Enum::kUint8));
  ml_context->writeTensor(script_state, ml_tensor, src_data,
                          scope.GetExceptionState());
}

// Reading data from an array buffer to a destroyed MLTensor should not crash.
TEST_F(MLGraphTest, ReadWebNNTensorThenDestroyTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* script_state = scope.GetScriptState();

  MLContext* ml_context = CreateContext(scope, options);

  auto* desc = MLTensorDescriptor::Create();
  desc->setDataType(V8MLOperandDataType::Enum::kFloat32);
  desc->setShape({2, 2});
  desc->setReadable(true);

  ScriptPromiseTester create_tensor_tester(
      script_state,
      ml_context->createTensor(script_state, desc, scope.GetExceptionState()));
  create_tensor_tester.WaitUntilSettled();
  EXPECT_TRUE(create_tensor_tester.IsFulfilled());

  MLTensor* ml_tensor =
      V8ToObject<MLTensor>(&scope, create_tensor_tester.Value());
  ASSERT_THAT(ml_tensor, testing::NotNull());

  ml_tensor->destroy();

  ScriptPromise<DOMArrayBuffer> read_promise = ml_context->readTensor(
      script_state, ml_tensor, scope.GetExceptionState());
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
      BuildInput(scope.GetScriptState(), builder, "lhs", dimensions,
                 V8MLOperandDataType::Enum::kUint8, scope.GetExceptionState());
  auto* rhs_operand =
      BuildInput(scope.GetScriptState(), builder, "rhs", dimensions,
                 V8MLOperandDataType::Enum::kUint8, scope.GetExceptionState());
  auto* output_operand = BuildElementWiseBinary(
      scope, builder, webnn::mojom::blink::ElementWiseBinary::Kind::kAdd,
      lhs_operand, rhs_operand);
  MLNamedOperands named_outputs = {{"output", output_operand}};
  auto [graph, error_message, build_exception] =
      BuildGraph(scope, builder, named_outputs);
  ASSERT_THAT(graph, testing::NotNull());

  MLTensor* input_tensor =
      CreateMLTensorForOperand(scope, ml_context, lhs_operand);
  ASSERT_THAT(input_tensor, testing::NotNull());

  MLNamedTensors inputs(
      {{"lhs", input_tensor},
       {"rhs", CreateMLTensorForOperand(scope, ml_context, rhs_operand)}});
  MLNamedTensors outputs({{"output", CreateMLTensorForOperand(
                                         scope, ml_context, output_operand)}});

  {
    // Dispatch successfully.
    ml_context->dispatch(scope.GetScriptState(), graph, inputs, outputs,
                         scope.GetExceptionState());
    EXPECT_EQ(scope.GetExceptionState().Code(),
              ToExceptionCode(DOMExceptionCode::kNoError));
    Vector<uint8_t> results =
        GetMLTensorValues(scope, ml_context, outputs[0].second);
    EXPECT_EQ(results, Vector<uint8_t>(number_of_elements, 0));

    // Dispatch again successfully.
    ml_context->dispatch(scope.GetScriptState(), graph, inputs, outputs,
                         scope.GetExceptionState());
    EXPECT_EQ(scope.GetExceptionState().Code(),
              ToExceptionCode(DOMExceptionCode::kNoError));
    results = GetMLTensorValues(scope, ml_context, outputs[0].second);
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
        BuildInput(scope.GetScriptState(), builder, "input", input.dimensions,
                   input.data_type, scope.GetExceptionState());
    MLOperatorOptions* options = MLOperatorOptions::Create();
    auto* output_operand =
        builder->cast(input_operand, V8MLOperandDataType(output_data_type),
                      options, scope.GetExceptionState());
    MLNamedOperands named_outputs = {{"output", output_operand}};
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, named_outputs);
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
    ASSERT_LT(output_operand_id.value(), graph_info->operands.size());
    EXPECT_EQ(graph_info->operands[output_operand_id.value()]->descriptor,
              expected_descriptor);
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

TEST_F(MLGraphTest, MLTransformTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  MLContext* context = CreateContext(scope, MLContextOptions::Create());

  {
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());

    //   [a]
    //   / \
    //   \ /
    //   add
    //    |
    //   [b]
    auto* a = BuildInput(scope.GetScriptState(), builder, "a", {3, 4, 5},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    MLOperatorOptions* options = MLOperatorOptions::Create();
    auto* b = builder->add(a, a, options, exception_state);
    ASSERT_THAT(b, testing::NotNull());
    //  Transform the graph to:
    // [a]  [c]
    //  \   /
    //   \ /
    //   add
    //    |
    //   [b]

    auto* c =
        BuildConstant(scope.GetScriptState(), builder, {3, 4, 5},
                      V8MLOperandDataType::Enum::kFloat32, exception_state);
    ASSERT_THAT(c, testing::NotNull());
    MLGraphTransformer::Disconnect(a, b->Operator(), 1);
    MLGraphTransformer::Connect(c, b->Operator(), 1);
    EXPECT_EQ(b->Operator()->Inputs()[0], a);
    EXPECT_EQ(b->Operator()->Inputs()[1], c);
    // Build the transformed graph.
    MLNamedOperands named_outputs = {{"b", b}};
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputConstraints();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*inputs.at("a"), a->Descriptor());
    const auto& outputs = graph->GetOutputConstraints();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*outputs.at("b"), b->Descriptor());
  }

  {
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());
    // [a] -> transpose -> [b] -> relu -> [c]
    auto* a = BuildInput(scope.GetScriptState(), builder, "a", {3, 4, 5},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* transpose_options = MLTransposeOptions::Create();
    transpose_options->setPermutation({0, 2, 1});
    auto* b = builder->transpose(a, transpose_options, exception_state);
    ASSERT_THAT(b, testing::NotNull());
    auto* relu_options = MLOperatorOptions::Create();
    auto* c = builder->relu(b, relu_options, exception_state);
    ASSERT_THAT(c, testing::NotNull());

    EXPECT_EQ(c->Shape(), std::vector<uint32_t>({3, 5, 4}));
    // Transform the graph to:
    // [a] -> relu -> [c]
    MLGraphTransformer::Disconnect(a, b->Operator(), 0);
    MLGraphTransformer::Disconnect(b, c->Operator(), 0);
    MLGraphTransformer::Connect(a, c->Operator(), 0);
    // update shape of c
    auto* updated_c =
        MLGraphTransformer::ReplaceOperandWithNewShape(c, {3, 4, 5});
    EXPECT_EQ(updated_c->Shape(), std::vector<uint32_t>({3, 4, 5}));

    // Build the transformed graph.
    MLNamedOperands named_outputs = {{"c", updated_c}};
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputConstraints();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*inputs.at("a"), a->Descriptor());
    const auto& outputs = graph->GetOutputConstraints();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*outputs.at("c"), updated_c->Descriptor());
  }
}

TEST_F(MLGraphTest, MLTransposeEliminationTransformerTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  MLContext* context = CreateContext(scope, MLContextOptions::Create());

  {
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());

    // [a] -> transpose -> [b] -> transpose -> [c]
    // This shouldn't be eliminated otherwise the graph will have no operations.
    auto* a = BuildInput(scope.GetScriptState(), builder, "a", {3, 4, 5},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* transpose_options = MLTransposeOptions::Create();
    transpose_options->setPermutation({0, 2, 1});
    auto* b = builder->transpose(a, transpose_options, exception_state);
    ASSERT_THAT(b, testing::NotNull());
    auto* transpose_options2 = MLTransposeOptions::Create();
    transpose_options2->setPermutation({0, 2, 1});
    auto* c = builder->transpose(b, transpose_options2, exception_state);
    ASSERT_THAT(c, testing::NotNull());
    EXPECT_EQ(c->Shape(), std::vector<uint32_t>({3, 4, 5}));
    MLNamedOperands named_outputs = {{"c", c}};

    auto* transpose_elimination_transformer =
        MakeGarbageCollected<TransposeEliminationTransformer>(builder);
    transpose_elimination_transformer->Transform(named_outputs);

    // Expect no change in the graph.
    EXPECT_EQ(c->Operator()->Inputs()[0], b);
    EXPECT_EQ(b->Operator()->Inputs()[0], a);

    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputConstraints();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*inputs.at("a"), a->Descriptor());
    const auto& outputs = graph->GetOutputConstraints();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*outputs.at("c"), c->Descriptor());
  }

  {
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());
    // [a] -> relu -> [b] -> transpose -> [c] -> transpose -> [d]

    auto* a = BuildInput(scope.GetScriptState(), builder, "a", {3, 4, 5},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* relu_options = MLOperatorOptions::Create();
    auto* b = builder->relu(a, relu_options, exception_state);
    ASSERT_THAT(b, testing::NotNull());
    auto* transpose_options = MLTransposeOptions::Create();
    transpose_options->setPermutation({0, 2, 1});
    auto* c = builder->transpose(b, transpose_options, exception_state);
    ASSERT_THAT(c, testing::NotNull());
    auto* transpose_options2 = MLTransposeOptions::Create();
    transpose_options2->setPermutation({0, 2, 1});
    auto* d = builder->transpose(c, transpose_options2, exception_state);
    ASSERT_THAT(d, testing::NotNull());
    EXPECT_EQ(d->Shape(), std::vector<uint32_t>({3, 4, 5}));
    MLNamedOperands named_outputs = {{"d", d}};

    auto* transpose_elimination_transformer =
        MakeGarbageCollected<TransposeEliminationTransformer>(builder);
    transpose_elimination_transformer->Transform(named_outputs);

    // Should be transformed to:
    // [a] -> relu -> [b]
    EXPECT_EQ(c->Operator()->Inputs()[0], nullptr);
    EXPECT_EQ(d->Operator()->Inputs()[0], nullptr);
    EXPECT_EQ(named_outputs[0].first, "d");
    EXPECT_EQ(named_outputs[0].second, b);
    EXPECT_EQ(b->Operator()->Inputs()[0], a);

    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputConstraints();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*inputs.at("a"), a->Descriptor());
    const auto& outputs = graph->GetOutputConstraints();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*outputs.at("d"), b->Descriptor());
  }

  {
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());
    // [a] -> transpose -> [b] -> transpose -> [c] -> relu -> [d]

    auto* a = BuildInput(scope.GetScriptState(), builder, "a", {3, 4, 5},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* transpose_options = MLTransposeOptions::Create();
    transpose_options->setPermutation({0, 2, 1});
    auto* b = builder->transpose(a, transpose_options, exception_state);
    ASSERT_THAT(b, testing::NotNull());
    auto* transpose_options2 = MLTransposeOptions::Create();
    transpose_options2->setPermutation({0, 2, 1});
    auto* c = builder->transpose(b, transpose_options2, exception_state);
    ASSERT_THAT(c, testing::NotNull());
    auto* relu_options = MLOperatorOptions::Create();
    auto* d = builder->relu(c, relu_options, exception_state);
    ASSERT_THAT(d, testing::NotNull());
    EXPECT_EQ(d->Shape(), std::vector<uint32_t>({3, 4, 5}));
    MLNamedOperands named_outputs = {{"d", d}};

    auto* transpose_elimination_transformer =
        MakeGarbageCollected<TransposeEliminationTransformer>(builder);
    transpose_elimination_transformer->Transform(named_outputs);

    // Should be transformed to:
    // [a] -> relu -> [d]

    EXPECT_EQ(b->Operator()->Inputs()[0], nullptr);
    EXPECT_EQ(named_outputs[0].first, "d");
    EXPECT_EQ(named_outputs[0].second, d);
    EXPECT_EQ(d->Operator()->Inputs()[0], a);

    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputConstraints();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*inputs.at("a"), a->Descriptor());
    const auto& outputs = graph->GetOutputConstraints();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*outputs.at("d"), d->Descriptor());
  }

  {
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());
    // [a] -> transpose -> [b] -> transpose -> [c] -> relu -> [d]
    //  b and d are graph outputs.

    auto* a = BuildInput(scope.GetScriptState(), builder, "a", {3, 4, 5},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* transpose_options = MLTransposeOptions::Create();
    transpose_options->setPermutation({0, 2, 1});
    auto* b = builder->transpose(a, transpose_options, exception_state);
    ASSERT_THAT(b, testing::NotNull());
    auto* transpose_options2 = MLTransposeOptions::Create();
    transpose_options2->setPermutation({0, 2, 1});
    auto* c = builder->transpose(b, transpose_options2, exception_state);
    ASSERT_THAT(c, testing::NotNull());
    auto* relu_options = MLOperatorOptions::Create();
    auto* d = builder->relu(c, relu_options, exception_state);
    ASSERT_THAT(d, testing::NotNull());
    EXPECT_EQ(d->Shape(), std::vector<uint32_t>({3, 4, 5}));
    MLNamedOperands named_outputs = {{"b", b}, {"d", d}};

    auto* transpose_elimination_transformer =
        MakeGarbageCollected<TransposeEliminationTransformer>(builder);
    transpose_elimination_transformer->Transform(named_outputs);

    // Expect no change in the graph.
    EXPECT_EQ(b->Operator()->Inputs()[0], a);
    EXPECT_EQ(c->Operator()->Inputs()[0], b);
    EXPECT_EQ(d->Operator()->Inputs()[0], c);

    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputConstraints();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*inputs.at("a"), a->Descriptor());
    const auto& outputs = graph->GetOutputConstraints();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(2));
    EXPECT_EQ(*outputs.at("b"), b->Descriptor());
    EXPECT_EQ(*outputs.at("d"), d->Descriptor());
  }

  {
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());

    // [a] -> transpose -> [b] -> relu -> [c] -> transpose -> [d]
    auto* a = BuildInput(scope.GetScriptState(), builder, "a", {3, 4, 5},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* transpose_options = MLTransposeOptions::Create();
    transpose_options->setPermutation({0, 2, 1});
    auto* b = builder->transpose(a, transpose_options, exception_state);
    ASSERT_THAT(b, testing::NotNull());
    auto* relu_options = MLOperatorOptions::Create();
    auto* c = builder->relu(b, relu_options, exception_state);
    ASSERT_THAT(c, testing::NotNull());
    auto* transpose_options2 = MLTransposeOptions::Create();
    transpose_options2->setPermutation({0, 2, 1});
    auto* d = builder->transpose(c, transpose_options2, exception_state);
    ASSERT_THAT(d, testing::NotNull());

    EXPECT_EQ(d->Shape(), std::vector<uint32_t>({3, 4, 5}));
    MLNamedOperands named_outputs = {{"d", d}};

    auto* transpose_elimination_transformer =
        MakeGarbageCollected<TransposeEliminationTransformer>(builder);
    transpose_elimination_transformer->Transform(named_outputs);

    // Should be transformed to:
    // [a] -> relu -> [updated_c]

    // Note: Operand c still point to the relu operator but the relu operator
    // will replace c with a new operand which has the updated shape.

    MLOperand* updated_c = c->Operator()->Outputs()[0];
    EXPECT_NE(c, updated_c);
    EXPECT_NE(c->Shape(), updated_c->Shape());

    EXPECT_EQ(updated_c->Operator()->Inputs()[0], a);

    EXPECT_EQ(b->Operator()->Inputs()[0], nullptr);
    EXPECT_TRUE(b->DependentOperators().empty());

    EXPECT_EQ(d->Operator()->Inputs()[0], nullptr);
    EXPECT_TRUE(d->DependentOperators().empty());

    EXPECT_EQ(named_outputs[0].first, "d");
    EXPECT_EQ(named_outputs[0].second, updated_c);

    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputConstraints();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*inputs.at("a"), a->Descriptor());
    const auto& outputs = graph->GetOutputConstraints();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*outputs.at("d"), updated_c->Descriptor());
  }

  {
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());

    // [a] -> transpose -> [b] -> relu -> [c] -> transpose -> [d]
    // c and d are graph outputs.
    auto* a = BuildInput(scope.GetScriptState(), builder, "a", {3, 4, 5},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* transpose_options = MLTransposeOptions::Create();
    transpose_options->setPermutation({0, 2, 1});
    auto* b = builder->transpose(a, transpose_options, exception_state);
    ASSERT_THAT(b, testing::NotNull());
    auto* relu_options = MLOperatorOptions::Create();
    auto* c = builder->relu(b, relu_options, exception_state);
    ASSERT_THAT(c, testing::NotNull());
    auto* transpose_options2 = MLTransposeOptions::Create();
    transpose_options2->setPermutation({0, 2, 1});
    auto* d = builder->transpose(c, transpose_options2, exception_state);
    ASSERT_THAT(d, testing::NotNull());

    EXPECT_EQ(d->Shape(), std::vector<uint32_t>({3, 4, 5}));
    MLNamedOperands named_outputs = {{"c", c}, {"d", d}};

    auto* transpose_elimination_transformer =
        MakeGarbageCollected<TransposeEliminationTransformer>(builder);
    transpose_elimination_transformer->Transform(named_outputs);

    // Expect no change in the graph.
    EXPECT_EQ(b->Operator()->Inputs()[0], a);
    EXPECT_EQ(c->Operator()->Inputs()[0], b);
    EXPECT_EQ(d->Operator()->Inputs()[0], c);

    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputConstraints();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*inputs.at("a"), a->Descriptor());
    const auto& outputs = graph->GetOutputConstraints();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(2));
    EXPECT_EQ(*outputs.at("c"), c->Descriptor());
    EXPECT_EQ(*outputs.at("d"), d->Descriptor());
  }

  {
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());
    // [a] -> relu -> [b] -> transpose -> [c] -> transpose -> [d]
    // Note: the two transposes are not inversable.

    auto* a = BuildInput(scope.GetScriptState(), builder, "a", {3, 4, 5},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* relu_options = MLOperatorOptions::Create();
    auto* b = builder->relu(a, relu_options, exception_state);
    ASSERT_THAT(b, testing::NotNull());
    auto* transpose_options = MLTransposeOptions::Create();
    transpose_options->setPermutation({0, 2, 1});
    auto* c = builder->transpose(b, transpose_options, exception_state);
    ASSERT_THAT(c, testing::NotNull());
    auto* transpose_options2 = MLTransposeOptions::Create();
    transpose_options2->setPermutation({1, 0, 2});
    auto* d = builder->transpose(c, transpose_options2, exception_state);
    ASSERT_THAT(d, testing::NotNull());
    EXPECT_EQ(d->Shape(), std::vector<uint32_t>({5, 3, 4}));
    MLNamedOperands named_outputs = {{"d", d}};

    auto* transpose_elimination_transformer =
        MakeGarbageCollected<TransposeEliminationTransformer>(builder);
    transpose_elimination_transformer->Transform(named_outputs);

    // Expect no elimination of transpose since the two transposes have
    // different permutation.

    EXPECT_EQ(d->Operator()->Inputs()[0], c);
    EXPECT_EQ(c->Operator()->Inputs()[0], b);
    EXPECT_EQ(b->Operator()->Inputs()[0], a);

    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputConstraints();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*inputs.at("a"), a->Descriptor());
    const auto& outputs = graph->GetOutputConstraints();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*outputs.at("d"), d->Descriptor());
  }

  {
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());

    //      input0                 input1
    //            \                 /
    //             transpose0    transpose1
    //      input2            \   /        \
    //            \            \ /          \
    //            transpose2  add0          gelu
    //      input3          \  |  \              \
    //            \          \ |   \              \
    //            transpose3  add1  transpose4   transpose7
    //                      \  |  \
    //                       \ |   \
    //                        add2  transpose5
    //                         |
    //                      transpose6
    //                         |
    //                       relu

    auto* input0 =
        BuildInput(scope.GetScriptState(), builder, "input0", {1, 3, 1, 1},
                   V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* input1 =
        BuildInput(scope.GetScriptState(), builder, "input1", {1, 3, 1, 1},
                   V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* input2 =
        BuildInput(scope.GetScriptState(), builder, "input2", {1, 3, 1, 1},
                   V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* input3 =
        BuildInput(scope.GetScriptState(), builder, "input3", {1, 3, 1, 1},
                   V8MLOperandDataType::Enum::kFloat32, exception_state);

    auto* transpose_options0 = MLTransposeOptions::Create();
    transpose_options0->setPermutation({0, 3, 1, 2});
    auto* transpose0 =
        builder->transpose(input0, transpose_options0, exception_state);
    ASSERT_THAT(transpose0, testing::NotNull());

    auto* transpose_options1 = MLTransposeOptions::Create();
    transpose_options1->setPermutation({0, 3, 1, 2});
    auto* transpose1 =
        builder->transpose(input1, transpose_options1, exception_state);
    ASSERT_THAT(transpose1, testing::NotNull());

    auto* transpose_options2 = MLTransposeOptions::Create();
    transpose_options2->setPermutation({0, 3, 1, 2});
    auto* transpose2 =
        builder->transpose(input2, transpose_options2, exception_state);
    ASSERT_THAT(transpose2, testing::NotNull());

    auto* transpose_options3 = MLTransposeOptions::Create();
    transpose_options3->setPermutation({0, 3, 1, 2});
    auto* transpose3 =
        builder->transpose(input3, transpose_options3, exception_state);
    ASSERT_THAT(transpose3, testing::NotNull());

    auto* add_options0 = MLOperatorOptions::Create();
    auto* add0 =
        builder->add(transpose0, transpose1, add_options0, exception_state);
    ASSERT_THAT(add0, testing::NotNull());

    auto* add_options1 = MLOperatorOptions::Create();
    auto* add1 = builder->add(transpose2, add0, add_options1, exception_state);
    ASSERT_THAT(add1, testing::NotNull());

    auto* add_options2 = MLOperatorOptions::Create();
    auto* add2 = builder->add(transpose3, add1, add_options2, exception_state);
    ASSERT_THAT(add2, testing::NotNull());

    auto* gelu_options = MLOperatorOptions::Create();
    auto* gelu = builder->gelu(transpose1, gelu_options, exception_state);
    ASSERT_THAT(gelu, testing::NotNull());

    auto* transpose_options4 = MLTransposeOptions::Create();
    transpose_options4->setPermutation({0, 2, 3, 1});
    auto* transpose4 =
        builder->transpose(add0, transpose_options4, exception_state);
    ASSERT_THAT(transpose4, testing::NotNull());

    auto* transpose_options5 = MLTransposeOptions::Create();
    transpose_options5->setPermutation({0, 2, 3, 1});
    auto* transpose5 =
        builder->transpose(add1, transpose_options5, exception_state);
    ASSERT_THAT(transpose5, testing::NotNull());

    auto* transpose_options6 = MLTransposeOptions::Create();
    transpose_options6->setPermutation({0, 2, 3, 1});
    auto* transpose6 =
        builder->transpose(add2, transpose_options6, exception_state);
    ASSERT_THAT(transpose6, testing::NotNull());

    auto* transpose_options7 = MLTransposeOptions::Create();
    transpose_options7->setPermutation({0, 2, 3, 1});
    auto* transpose7 =
        builder->transpose(gelu, transpose_options7, exception_state);
    ASSERT_THAT(transpose7, testing::NotNull());

    auto* relu_options = MLOperatorOptions::Create();
    auto* relu = builder->relu(transpose6, relu_options, exception_state);
    ASSERT_THAT(relu, testing::NotNull());

    MLNamedOperands named_outputs = {{"relu", relu},
                                     {"transpose4", transpose4},
                                     {"transpose5", transpose5},
                                     {"transpose7", transpose7}};
    auto* transpose_elimination_transformer =
        MakeGarbageCollected<TransposeEliminationTransformer>(builder);
    transpose_elimination_transformer->Transform(named_outputs);

    // should be transformed to:
    //
    //    input0          input1
    //          \         /    \
    //  input2 add0(updated)    gelu(updated)
    //       \  |
    //        \ |
    //  input3 add1(updated)
    //       \  |
    //        \ |
    //         add2(updated)
    //          |
    //         relu

    MLOperand* add2_updated = relu->Operator()->Inputs()[0];
    MLOperand* add1_updated = add2_updated->Operator()->Inputs()[1];
    MLOperand* add0_updated = add1_updated->Operator()->Inputs()[1];

    MLOperand* gelu_updated = named_outputs[3].second;

    CHECK_EQ(gelu_updated->Operator()->Kind(),
             webnn::mojom::blink::Operation::Tag::kGelu);

    EXPECT_NE(add0_updated, add0);
    EXPECT_NE(add1_updated, add1);
    EXPECT_NE(add2_updated, add2);
    EXPECT_NE(gelu_updated, gelu);

    EXPECT_EQ(named_outputs[0].first, "relu");
    EXPECT_EQ(named_outputs[1].first, "transpose4");
    EXPECT_EQ(named_outputs[2].first, "transpose5");
    EXPECT_EQ(named_outputs[3].first, "transpose7");

    EXPECT_EQ(named_outputs[0].second, relu);
    EXPECT_EQ(named_outputs[1].second, add0_updated);
    EXPECT_EQ(named_outputs[2].second, add1_updated);

    EXPECT_EQ(relu->Operator()->Inputs()[0], add2_updated);
    EXPECT_EQ(add2_updated->Operator()->Inputs()[0], input3);
    EXPECT_EQ(add2_updated->Operator()->Inputs()[1], add1_updated);
    EXPECT_EQ(add1_updated->Operator()->Inputs()[0], input2);
    EXPECT_EQ(add1_updated->Operator()->Inputs()[1], add0_updated);
    EXPECT_EQ(add0_updated->Operator()->Inputs()[0], input0);
    EXPECT_EQ(add0_updated->Operator()->Inputs()[1], input1);
    EXPECT_EQ(gelu_updated->Operator()->Inputs()[0], input1);

    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputConstraints();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(4));
    EXPECT_EQ(*inputs.at("input0"), input0->Descriptor());
    EXPECT_EQ(*inputs.at("input1"), input1->Descriptor());
    EXPECT_EQ(*inputs.at("input2"), input2->Descriptor());
    EXPECT_EQ(*inputs.at("input3"), input3->Descriptor());

    const auto& outputs = graph->GetOutputConstraints();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(4));
    EXPECT_EQ(*outputs.at("relu"), relu->Descriptor());
    EXPECT_EQ(*outputs.at("transpose4"), add0_updated->Descriptor());
    EXPECT_EQ(*outputs.at("transpose5"), add1_updated->Descriptor());
    EXPECT_EQ(*outputs.at("transpose7"), gelu_updated->Descriptor());
  }
}

TEST_F(MLGraphTest, MLQDQDetectionTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  MLContext* context = CreateContext(scope, MLContextOptions::Create());

  {
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());
    //   input             filter
    //      \               /
    //       DQ(0)         DQ(1)
    //        \           /
    //    transpose(0)  transpose(1)
    //          \      /
    //           conv2d
    //             \
    //          transpose(2)
    //               \
    //                Q
    //                 \
    //                relu
    auto* input =
        BuildInput(scope.GetScriptState(), builder, "input", {1, 1, 1, 3},
                   V8MLOperandDataType::Enum::kInt8, exception_state);
    auto* filter =
        BuildConstant(scope.GetScriptState(), builder, {1, 1, 1, 3},
                      V8MLOperandDataType::Enum::kInt8, exception_state);

    auto* input_scale =
        BuildConstant(scope.GetScriptState(), builder, {1, 1, 1, 1},
                      V8MLOperandDataType::Enum::kFloat32, exception_state);

    auto* input_zero_point =
        BuildConstant(scope.GetScriptState(), builder, {1, 1, 1, 1},
                      V8MLOperandDataType::Enum::kInt8, exception_state);

    auto* filter_scale =
        BuildConstant(scope.GetScriptState(), builder, {1, 1, 1, 1},
                      V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* filter_zero_point =
        BuildConstant(scope.GetScriptState(), builder, {1, 1, 1, 1},
                      V8MLOperandDataType::Enum::kInt8, exception_state);

    auto* dq0_output_operand =
        builder->dequantizeLinear(input, input_scale, input_zero_point,
                                  MLOperatorOptions::Create(), exception_state);

    ASSERT_THAT(dq0_output_operand, testing::NotNull());
    auto* dq1_operand =
        builder->dequantizeLinear(filter, filter_scale, filter_zero_point,
                                  MLOperatorOptions::Create(), exception_state);
    ASSERT_THAT(dq1_operand, testing::NotNull());

    auto* transpose_options0 = MLTransposeOptions::Create();
    transpose_options0->setPermutation({0, 2, 3, 1});
    auto* transpose0_output_operand = builder->transpose(
        dq0_output_operand, transpose_options0, exception_state);
    ASSERT_THAT(transpose0_output_operand, testing::NotNull());
    auto* transpose_options1 = MLTransposeOptions::Create();
    transpose_options1->setPermutation({0, 2, 3, 1});
    auto* transpose1_output_operand =
        builder->transpose(dq1_operand, transpose_options1, exception_state);
    ASSERT_THAT(transpose1_output_operand, testing::NotNull());

    auto* conv2d_options = MLConv2dOptions::Create();
    conv2d_options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    conv2d_options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOhwi);

    auto* conv2d_output_operand =
        builder->conv2d(transpose0_output_operand, transpose1_output_operand,
                        conv2d_options, exception_state);
    ASSERT_THAT(conv2d_output_operand, testing::NotNull());

    auto* transpose_options2 = MLTransposeOptions::Create();
    transpose_options2->setPermutation({0, 3, 1, 2});
    auto* transpose2_output_operand = builder->transpose(
        conv2d_output_operand, transpose_options2, exception_state);
    ASSERT_THAT(transpose2_output_operand, testing::NotNull());
    auto* output_scale =
        BuildConstant(scope.GetScriptState(), builder, {1, 1, 1, 1},
                      V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* output_zero_point =
        BuildConstant(scope.GetScriptState(), builder, {1, 1, 1, 1},
                      V8MLOperandDataType::Enum::kInt8, exception_state);

    auto* q_output_operand = builder->quantizeLinear(
        transpose2_output_operand, output_scale, output_zero_point,
        MLOperatorOptions::Create(), exception_state);
    ASSERT_THAT(q_output_operand, testing::NotNull());

    auto* relu_options = MLOperatorOptions::Create();
    auto* relu_output_operand =
        builder->relu(q_output_operand, relu_options, exception_state);
    ASSERT_THAT(relu_output_operand, testing::NotNull());

    MLNamedOperands named_outputs = {{"q", q_output_operand},
                                     {"relu", relu_output_operand}};

    auto* qdq_detection_transformer =
        MakeGarbageCollected<QDQDetectionTransformer>(builder);

    qdq_detection_transformer->Transform(named_outputs);
    // should be transformed to:
    //    ...               ...
    //      \               /
    //   transpose(0)     transpose(1)
    //        \           /
    //        DQ(0)      DQ(1)
    //          \      /
    //           conv2d(cener operator)
    //             \
    //              Q
    //               \
    //            transpose(2)
    //                 \
    //                relu
    //
    // Except of conv2d and relu, all other operators' operands are replaced.

    MLOperator* conv2d = conv2d_output_operand->Operator();
    MLOperator* q = conv2d_output_operand->DependentOperators().begin()->Get();
    MLOperator* transpose2 =
        q->Outputs()[0]->DependentOperators().begin()->Get();
    MLOperator* dq0 = conv2d->Inputs()[0]->Operator();
    MLOperator* dq1 = conv2d->Inputs()[1]->Operator();
    MLOperator* transpose0 = dq0->Inputs()[0]->Operator();
    MLOperator* transpose1 = dq1->Inputs()[0]->Operator();

    EXPECT_EQ(q->Kind(), webnn::mojom::blink::Operation::Tag::kQuantizeLinear);
    EXPECT_EQ(transpose2->Kind(),
              webnn::mojom::blink::Operation::Tag::kTranspose);
    EXPECT_EQ(dq0->Kind(),
              webnn::mojom::blink::Operation::Tag::kDequantizeLinear);
    EXPECT_EQ(dq1->Kind(),
              webnn::mojom::blink::Operation::Tag::kDequantizeLinear);
    EXPECT_EQ(transpose0->Kind(),
              webnn::mojom::blink::Operation::Tag::kTranspose);
    EXPECT_EQ(transpose1->Kind(),
              webnn::mojom::blink::Operation::Tag::kTranspose);

    // Original operands still point to the operators.
    EXPECT_EQ(q_output_operand->Operator(), q);
    EXPECT_EQ(transpose2_output_operand->Operator(), transpose2);
    EXPECT_EQ(dq0_output_operand->Operator(), dq0);
    EXPECT_EQ(dq1_operand->Operator(), dq1);
    EXPECT_EQ(transpose0_output_operand->Operator(), transpose0);
    EXPECT_EQ(transpose1_output_operand->Operator(), transpose1);

    // Those operands are replaced.
    EXPECT_NE(q_output_operand, q->Outputs()[0]);
    EXPECT_NE(transpose2_output_operand, transpose2->Outputs()[0]);
    EXPECT_NE(dq0_output_operand, dq0->Outputs()[0]);
    EXPECT_NE(dq1_operand, dq1->Outputs()[0]);
    EXPECT_NE(transpose0_output_operand, transpose0->Outputs()[0]);
    EXPECT_NE(transpose1_output_operand, transpose1->Outputs()[0]);

    EXPECT_EQ(transpose2->Outputs()[0]->DataType(),
              webnn::OperandDataType::kInt8);
    EXPECT_EQ(transpose0->Outputs()[0]->DataType(),
              webnn::OperandDataType::kInt8);
    EXPECT_EQ(transpose1->Outputs()[0]->DataType(),
              webnn::OperandDataType::kInt8);

    EXPECT_EQ(q->Outputs()[0]->Shape(), conv2d_output_operand->Shape());
    EXPECT_EQ(dq0->Outputs()[0]->Shape(), transpose0_output_operand->Shape());
    EXPECT_EQ(dq1->Outputs()[0]->Shape(), transpose1_output_operand->Shape());

    for (auto& [name, operand] : named_outputs) {
      if (name == "q") {
        EXPECT_EQ(operand, transpose2->Outputs()[0]);
      } else if (name == "relu") {
        EXPECT_EQ(operand, relu_output_operand);
      } else {
        FAIL() << "Unexpected named output: " << name;
      }
    }

    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    // This case have complicate pattern, other optimizations may be performed
    // duiring BuildGraph. So we just make sure the graph will build
    // successfully here.
    ASSERT_THAT(graph, testing::NotNull());
  }
}

TEST_F(MLGraphTest, MLConstantFoldingTransformerNoOpTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  MLContext* context = CreateContext(scope, MLContextOptions::Create());

  DummyExceptionStateForTesting exception_state;
  auto* builder =
      MLGraphBuilder::Create(scope.GetScriptState(), context, exception_state);
  ASSERT_THAT(builder, testing::NotNull());

  // [a] -> transpose -> [b]
  // This shouldn't be eliminated otherwise the graph will have no operations.
  auto* a = BuildConstant(scope.GetScriptState(), builder, {3, 4, 5},
                          V8MLOperandDataType::Enum::kFloat32, exception_state);
  auto* transpose_options = MLTransposeOptions::Create();
  transpose_options->setPermutation({0, 2, 1});
  auto* b = builder->transpose(a, transpose_options, exception_state);
  ASSERT_THAT(b, testing::NotNull());

  EXPECT_EQ(b->Shape(), std::vector<uint32_t>({3, 5, 4}));
  MLNamedOperands named_outputs = {{"b", b}};

  auto* constant_folding_transformer =
      MakeGarbageCollected<ConstantFoldingTransformer>(builder);
  constant_folding_transformer->Transform(named_outputs);

  // Expect no change in the graph.
  EXPECT_EQ(b->Operator()->Inputs()[0], a);

  auto [graph, error_name, error_message] =
      BuildGraph(scope, builder, named_outputs);
  ASSERT_THAT(graph, testing::NotNull());
}

TEST_F(MLGraphTest, MLConstantFoldingTransformerTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  MLContext* context = CreateContext(scope, MLContextOptions::Create());
  DummyExceptionStateForTesting exception_state;
  auto* builder =
      MLGraphBuilder::Create(scope.GetScriptState(), context, exception_state);
  ASSERT_THAT(builder, testing::NotNull());

  // [a] -> transpose -> reshape -> transpose -> relu -> [e]
  auto* a = BuildConstant(scope.GetScriptState(), builder, {3, 4, 5},
                          V8MLOperandDataType::Enum::kFloat32, exception_state);
  auto* transpose_options = MLTransposeOptions::Create();
  transpose_options->setPermutation({0, 2, 1});
  auto* b = builder->transpose(a, transpose_options, exception_state);
  ASSERT_THAT(b, testing::NotNull());
  auto* c = builder->reshape(b, {3, 20}, MLOperatorOptions::Create(),
                             exception_state);
  ASSERT_THAT(c, testing::NotNull());
  auto* d =
      builder->transpose(c, MLTransposeOptions::Create(), exception_state);
  ASSERT_THAT(d, testing::NotNull());
  auto* e = builder->relu(d, MLOperatorOptions::Create(), exception_state);
  ASSERT_THAT(e, testing::NotNull());

  MLNamedOperands named_outputs = {{"e", e}};

  auto* constant_folding_transformer =
      MakeGarbageCollected<ConstantFoldingTransformer>(builder);
  constant_folding_transformer->Transform(named_outputs);
  auto& relu_input = e->Operator()->Inputs()[0];
  EXPECT_EQ(relu_input->Kind(), webnn::mojom::blink::Operand::Kind::kConstant);
  Vector<uint32_t> expected_shape{20, 3};
  EXPECT_EQ(e->shape(), expected_shape);
  EXPECT_EQ(e->shape(), relu_input->shape());

  auto [graph, error_name, error_message] =
      BuildGraph(scope, builder, named_outputs);
  ASSERT_THAT(graph, testing::NotNull());
}

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
TEST_F(MLGraphTest, MLGraphDumpTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  MLContext* context = CreateContext(scope, MLContextOptions::Create());

  {
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());
    //
    //     input0     input1
    //        \        /
    //        relu0   /
    //          \    /
    //           add
    //            |
    //           relu1

    auto* input0 =
        BuildInput(scope.GetScriptState(), builder, "input0", {3, 4, 5},
                   V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* input1 =
        BuildInput(scope.GetScriptState(), builder, "input1", {3, 4, 5},
                   V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* relu0_options = MLOperatorOptions::Create();
    auto* relu0 = builder->relu(input0, relu0_options, exception_state);
    ASSERT_THAT(relu0, testing::NotNull());
    auto* add_options = MLOperatorOptions::Create();
    auto* add = builder->add(relu0, input1, add_options, exception_state);
    ASSERT_THAT(add, testing::NotNull());
    auto* relu1_options = MLOperatorOptions::Create();
    auto* relu1 = builder->relu(add, relu1_options, exception_state);
    ASSERT_THAT(relu1, testing::NotNull());

    MLNamedOperands named_outputs = {{"output0", relu1}, {"output1", add}};
    MLGraphDumper* graph_dumper = MakeGarbageCollected<MLGraphDumper>();
    graph_dumper->RecordGraph("test_graph", named_outputs);

    const base::Value::Dict& json_root = graph_dumper->GetRoot();
    const base::Value::List* graphs = json_root.FindList("graphs");
    ASSERT_TRUE(graphs);
    EXPECT_EQ(graphs->size(), 1u);
    const base::Value::Dict& graph_json = (*graphs)[0].GetDict();
    EXPECT_TRUE(graph_json.FindString("id"));
    const base::Value::List* nodes = graph_json.FindList("nodes");
    ASSERT_TRUE(nodes);
    EXPECT_EQ(nodes->size(), 7u);  // 2 inputs + 1 add + 2 relu + 2 outputs

    const base::Value::Dict* json_output0 = nullptr;
    const base::Value::Dict* json_output1 = nullptr;
    const base::Value::Dict* json_relu0 = nullptr;
    const base::Value::Dict* json_relu1 = nullptr;
    const base::Value::Dict* json_add = nullptr;

    std::map<std::string, const base::Value::Dict*> id_to_node_json;

    for (const auto& node_val : *nodes) {
      const base::Value::Dict& node_json = node_val.GetDict();
      const std::string* id = node_json.FindString("id");
      ASSERT_TRUE(id);
      id_to_node_json[*id] = &node_json;

      const std::string* label = node_json.FindString("label");
      ASSERT_TRUE(label);
      if (*label == "Output") {
        const base::Value::List* attrs = node_json.FindList("attrs");
        ASSERT_TRUE(attrs);
        for (const auto& attr_val : *attrs) {
          const base::Value::Dict& attr = attr_val.GetDict();
          const std::string* key = attr.FindString("key");
          const std::string* value = attr.FindString("value");
          ASSERT_TRUE(key);
          ASSERT_TRUE(value);
          if (*key == "output_name") {
            if (*value == "output0") {
              json_output0 = &node_json;
            } else if (*value == "output1") {
              json_output1 = &node_json;
            } else {
              FAIL() << "Unexpected output name: " << *value;
            }
          }
        }
      }
    }

    ASSERT_TRUE(json_output0);
    ASSERT_TRUE(json_output1);

    {
      const base::Value::List* incoming_edges =
          json_output0->FindList("incomingEdges");
      ASSERT_TRUE(incoming_edges);
      EXPECT_EQ(incoming_edges->size(), 1u);
      const base::Value::Dict& incoming_edge_json =
          (*incoming_edges)[0].GetDict();
      const std::string* source_node_id =
          incoming_edge_json.FindString("sourceNodeId");
      ASSERT_TRUE(source_node_id);
      ASSERT_TRUE(id_to_node_json.count(*source_node_id));
      const base::Value::Dict* relu_node_json =
          id_to_node_json[*source_node_id];
      EXPECT_EQ(*relu_node_json->FindString("label"), "relu");
      json_relu1 = relu_node_json;
    }

    {
      const base::Value::List* incoming_edges =
          json_output1->FindList("incomingEdges");
      ASSERT_TRUE(incoming_edges);
      EXPECT_EQ(incoming_edges->size(), 1u);
      const base::Value::Dict& incoming_edge_json =
          (*incoming_edges)[0].GetDict();
      const std::string* source_node_id =
          incoming_edge_json.FindString("sourceNodeId");
      ASSERT_TRUE(source_node_id);
      ASSERT_TRUE(id_to_node_json.count(*source_node_id));
      const base::Value::Dict* add_node_json = id_to_node_json[*source_node_id];
      EXPECT_EQ(*add_node_json->FindString("label"), "add");
      json_add = add_node_json;
    }

    {
      const base::Value::List* incoming_edges =
          json_relu1->FindList("incomingEdges");
      ASSERT_TRUE(incoming_edges);
      EXPECT_EQ(incoming_edges->size(), 1u);
      const base::Value::Dict& incoming_edge_json =
          (*incoming_edges)[0].GetDict();
      const std::string* source_node_id =
          incoming_edge_json.FindString("sourceNodeId");
      ASSERT_TRUE(source_node_id);
      ASSERT_TRUE(id_to_node_json.count(*source_node_id));
      const base::Value::Dict* add_node_json = id_to_node_json[*source_node_id];
      EXPECT_EQ(*add_node_json->FindString("label"), "add");
    }

    {
      const base::Value::List* incoming_edges =
          json_add->FindList("incomingEdges");
      ASSERT_TRUE(incoming_edges);
      EXPECT_EQ(incoming_edges->size(), 2u);
      {
        const base::Value::Dict& incoming_edge_json =
            (*incoming_edges)[0].GetDict();
        const std::string* source_node_id =
            incoming_edge_json.FindString("sourceNodeId");
        ASSERT_TRUE(source_node_id);
        ASSERT_TRUE(id_to_node_json.count(*source_node_id));
        const base::Value::Dict* relu0_node_json =
            id_to_node_json[*source_node_id];
        EXPECT_EQ(*relu0_node_json->FindString("label"), "relu");
        json_relu0 = relu0_node_json;
      }
      {
        const base::Value::Dict& incoming_edge_json =
            (*incoming_edges)[1].GetDict();
        const std::string* source_node_id =
            incoming_edge_json.FindString("sourceNodeId");
        ASSERT_TRUE(source_node_id);
        ASSERT_TRUE(id_to_node_json.count(*source_node_id));
        const base::Value::Dict* input1_node_json =
            id_to_node_json[*source_node_id];
        EXPECT_EQ(*input1_node_json->FindString("label"), "Input");
      }
    }

    {
      const base::Value::List* incoming_edges =
          json_relu0->FindList("incomingEdges");
      ASSERT_TRUE(incoming_edges);
      EXPECT_EQ(incoming_edges->size(), 1u);
      const base::Value::Dict& incoming_edge_json =
          (*incoming_edges)[0].GetDict();
      const std::string* source_node_id =
          incoming_edge_json.FindString("sourceNodeId");
      ASSERT_TRUE(source_node_id);
      ASSERT_TRUE(id_to_node_json.count(*source_node_id));
      const base::Value::Dict* input0_node_json =
          id_to_node_json[*source_node_id];
      EXPECT_EQ(*input0_node_json->FindString("label"), "Input");
    }
  }
}
#endif

}  // namespace blink
