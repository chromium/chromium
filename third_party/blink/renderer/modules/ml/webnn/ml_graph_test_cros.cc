// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ml/mojom/ml_service.mojom-blink.h"
#include "components/ml/mojom/web_platform_model.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/ml_model_loader_test_util.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder_test.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_cros.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_test_base.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/builtin_op_kernels.h"
#include "third_party/tflite/src/tensorflow/lite/model.h"
#include "third_party/tflite/src/tensorflow/lite/mutable_op_resolver.h"
#include "third_party/tflite/src/tensorflow/lite/schema/schema_generated.h"

namespace blink {

namespace blink_mojom = ml::model_loader::mojom::blink;

namespace {

// The version number of the Schema. Ideally all changes will be backward
// compatible. If that ever changes, we must ensure that version is the first
// entry in the new tflite root so that we can see that version is not 1.
#define TFLITE_SCHEMA_VERSION (3)

Vector<int32_t> ConvertDimensions(const Vector<uint32_t>& dimensions) {
  Vector<int32_t> new_dims;
  for (auto dim : dimensions) {
    new_dims.push_back(base::checked_cast<int32_t>(dim));
  }
  return new_dims;
}

blink_mojom::TensorInfoPtr ConvertToMojom(const TfLiteTensor* tensor) {
  auto tensor_info = blink_mojom::TensorInfo::New();
  tensor_info->byte_size = base::checked_cast<uint32_t>(tensor->bytes);
  WTF::Vector<uint32_t> dims;
  dims.reserve(tensor->dims->size);
  for (int32_t i = 0; i < tensor->dims->size; ++i) {
    dims.push_back(tensor->dims->data[i]);
  }
  tensor_info->dimensions = std::move(dims);
  return tensor_info;
}

// This class maintains all the currently supported TFLite
// operations for the Chromium build of TFLite and registers them for use.
class TfLiteOpResolver : public tflite::MutableOpResolver {
 public:
  TfLiteOpResolver() {
    AddBuiltin(tflite::BuiltinOperator_ADD,
               tflite::ops::builtin::Register_ADD(),
               /* min_version = */ 1,
               /* max_version = */ 2);
  }
};

class TfLiteRuntime {
 public:
  TfLiteRuntime() = default;
  TfLiteRuntime(const TfLiteRuntime&) = delete;
  TfLiteRuntime(TfLiteRuntime&&) = delete;
  ~TfLiteRuntime() = default;

  TfLiteStatus Load(const mojo_base::BigBuffer& buffer,
                    blink_mojom::ModelInfoPtr& info) {
    const tflite::Model* model = tflite::GetModel(buffer.data());
    EXPECT_NE(model, nullptr);
    TfLiteOpResolver op_resolver;
    EXPECT_EQ(tflite::InterpreterBuilder(model, op_resolver)(&interpreter_),
              kTfLiteOk);
    EXPECT_NE(interpreter_, nullptr);
    EXPECT_EQ(interpreter_->AllocateTensors(), kTfLiteOk);

    for (auto index : interpreter_->inputs()) {
      auto* tensor = interpreter_->tensor(index);
      info->input_tensor_info.insert(WTF::String(tensor->name),
                                     ConvertToMojom(tensor));
    }

    for (auto index : interpreter_->outputs()) {
      auto* tensor = interpreter_->tensor(index);
      info->output_tensor_info.insert(WTF::String(tensor->name),
                                      ConvertToMojom(tensor));
    }
    return kTfLiteOk;
  }

  TfLiteStatus Compute(
      const WTF::HashMap<WTF::String, WTF::Vector<uint8_t>>& named_input,
      WTF::HashMap<WTF::String, WTF::Vector<uint8_t>>& named_output) {
    for (auto index : interpreter_->inputs()) {
      auto* tensor = interpreter_->tensor(index);
      Vector<uint8_t> input_data = named_input.at(WTF::String(tensor->name));
      memcpy(tensor->data.raw, input_data.data(), tensor->bytes);
    }

    // Compute the graph.
    EXPECT_EQ(interpreter_->Invoke(), kTfLiteOk);

    for (auto index : interpreter_->outputs()) {
      auto* tensor = interpreter_->tensor(index);
      WTF::Vector<uint8_t> output_data(
          base::checked_cast<wtf_size_t>(tensor->bytes));
      memcpy(output_data.data(), tensor->data.raw, tensor->bytes);
      named_output.insert(WTF::String(tensor->name), std::move(output_data));
    }
    return kTfLiteOk;
  }

 private:
  std::unique_ptr<tflite::Interpreter> interpreter_;
};

}  // namespace

class FakeWebNNModel : public blink_mojom::Model {
 public:
  FakeWebNNModel() : runtime_(std::make_unique<TfLiteRuntime>()) {}
  FakeWebNNModel(const FakeWebNNModel&) = delete;
  FakeWebNNModel(FakeWebNNModel&&) = delete;
  ~FakeWebNNModel() override = default;

  FakeMLModelLoader::LoadFn CreateFromThis() {
    return WTF::BindOnce(&FakeWebNNModel::OnCreateModel, WTF::Unretained(this));
  }

 private:
  void OnCreateModel(mojo_base::BigBuffer buffer,
                     blink_mojom::ModelLoader::LoadCallback callback) {
    blink_mojom::ModelInfoPtr info = blink_mojom::ModelInfo::New();
    EXPECT_EQ(runtime_->Load(buffer, info), kTfLiteOk);

    // Hold the flatbuffer for computing with tflite runtime.
    buffer_ = std::move(buffer);

    receiver_.reset();
    std::move(callback).Run(blink_mojom::LoadModelResult::kOk,
                            receiver_.BindNewPipeAndPassRemote(),
                            std::move(info));
  }

  // Override methods from blink_mojom::Model.
  void Compute(const WTF::HashMap<WTF::String, WTF::Vector<uint8_t>>& input,
               blink_mojom::Model::ComputeCallback callback) override {
    WTF::HashMap<WTF::String, WTF::Vector<uint8_t>> named_output;
    EXPECT_EQ(runtime_->Compute(input, named_output), kTfLiteOk);
    std::move(callback).Run(blink_mojom::ComputeResult::kOk, named_output);
  }

  mojo::Receiver<blink_mojom::Model> receiver_{this};
  std::unique_ptr<TfLiteRuntime> runtime_;
  // The buffer of tflite model must be alive for computing.
  mojo_base::BigBuffer buffer_;
};

class MLGraphTestCrOS : public MLGraphTestBase {
 public:
  ScopedSetMLServiceBinder SetUpMLService(V8TestingScope& scope) {
    service_.SetCreateModelLoader(loader_.CreateFromThis());
    loader_.SetLoad(model_.CreateFromThis());

    return ScopedSetMLServiceBinder(&service_, scope);
  }

 private:
  FakeMLService service_;
  FakeMLModelLoader loader_;
  FakeWebNNModel model_;
};

template <typename T>
struct ElementWiseAddTester {
  OperandInfo<T> lhs;
  OperandInfo<T> rhs;
  OperandInfo<T> expected;

  ~ElementWiseAddTester() { MLGraphCrOS::SetFlatbufferForTesting(nullptr); }

  void Test(MLGraphTestCrOS& helper, V8TestingScope& scope) {
    // Setup binder for MLService
    ScopedSetMLServiceBinder scoped_setup_binder = helper.SetUpMLService(scope);
    // Set the flatbuffer of tflite model converted from the WebNN graph.
    flatbuffers::DetachedBuffer flatbuffer = GetFlatBuffer();
    MLGraphCrOS::SetFlatbufferForTesting(&flatbuffer);

    // Test building graph for the operands in the following topology:
    //       [input] [constant]
    //           \   /
    //            add
    //             |
    //          [output]
    auto* builder =
        CreateMLGraphBuilder(scope.GetExecutionContext(),
                             scope.GetScriptState(), scope.GetExceptionState());
    auto* input = BuildInput(builder, "input", lhs.dimensions, lhs.type,
                             scope.GetExceptionState());
    auto* constant = BuildConstant(builder, rhs.dimensions, rhs.type,
                                   rhs.values, scope.GetExceptionState());
    auto* output = BuildElementWiseBinary(
        scope, builder, ElementWiseBinaryKind::kAdd, input, constant);
    EXPECT_EQ(output->Type(), expected.type);
    auto [graph, exception] =
        helper.BuildGraph(scope, builder, {{"output", output}});
    EXPECT_NE(graph, nullptr);
    MLGraphCrOS* cros_graph = static_cast<MLGraphCrOS*>(graph.Get());
    const auto& input_tensor_info = cros_graph->GetInputResourcesInfo();
    EXPECT_EQ(input_tensor_info.size(), 1u);
    EXPECT_EQ(input_tensor_info.Contains("input"), true);
    const auto& output_tensor_info = cros_graph->GetOutputResourcesInfo();
    EXPECT_EQ(output_tensor_info.size(), 1u);
    EXPECT_EQ(output_tensor_info.Contains("output"), true);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input", CreateArrayBufferViewForOperand(input, lhs.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output)}});
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected.values);
  }

 private:
  // Build tflite model for the element-wise graph in the flatbuffer.
  flatbuffers::DetachedBuffer GetFlatBuffer() {
    // Tflite model parameters information.
    const char* kModelDescription = "ElementWise binary model for testing";
    const tflite::TensorType type = tflite::TensorType_FLOAT32;

    flatbuffers::FlatBufferBuilder builder;
    // It is required that the first entry in the buffers of model is always an
    // empty buffer. This is so that the default buffer index of zero in Tensor
    // will always refer to a valid empty buffer.
    Vector<flatbuffers::Offset<tflite::Buffer>> buffers = {
        tflite::CreateBuffer(builder, builder.CreateVector({})),
    };
    // Create tflite |Buffer| for constant tensor.
    buffers.push_back(tflite::CreateBuffer(
        builder, builder.CreateVector(
                     reinterpret_cast<const uint8_t*>(rhs.values.data()),
                     sizeof(T) * rhs.values.size())));

    // A list of all tflite |Tensor| used in this model.
    Vector<flatbuffers::Offset<tflite::Tensor>> tensors;
    // Create tflite |Tensor| for constant tensor.
    CHECK(lhs.type == V8MLOperandType::Enum::kFloat32);
    uint32_t lhs_buffer_index = 0;
    tensors.emplace_back(tflite::CreateTensor(
        builder,
        builder.CreateVector<int32_t>(ConvertDimensions(lhs.dimensions)), type,
        lhs_buffer_index, builder.CreateString("input")));
    // Create tflite |Tensor| for input tensor.
    CHECK(rhs.type == V8MLOperandType::Enum::kFloat32);
    uint32_t rhs_buffer_index = 1;
    tensors.emplace_back(tflite::CreateTensor(
        builder,
        builder.CreateVector<int32_t>(ConvertDimensions(rhs.dimensions)), type,
        rhs_buffer_index));
    // Create tflite |Tensor| for output tensor.
    uint32_t output_buffer_index = 0;
    tensors.emplace_back(tflite::CreateTensor(
        builder,
        builder.CreateVector<int32_t>(ConvertDimensions(expected.dimensions)),
        type, output_buffer_index, builder.CreateString("output")));

    // A list of all tflite |Operator| used in this model.
    Vector<flatbuffers::Offset<tflite::Operator>> operators;
    int32_t lhs_tensor_index = 0, rhs_tensor_index = 1, output_tensor_index = 2;
    Vector<int32_t> op_inputs = {lhs_tensor_index, rhs_tensor_index};
    Vector<int32_t> op_outputs = {output_tensor_index};
    operators.emplace_back(tflite::CreateOperator(
        builder, 0, builder.CreateVector<int32_t>(op_inputs),
        builder.CreateVector<int32_t>(op_outputs)));

    // Create subgraph in the model.
    Vector<int32_t> subgraph_inputs = {lhs_tensor_index};
    Vector<int32_t> subgraph_outputs = {output_tensor_index};
    flatbuffers::Offset<tflite::SubGraph> subgraph = tflite::CreateSubGraph(
        builder, builder.CreateVector(tensors.data(), tensors.size()),
        builder.CreateVector<int32_t>(subgraph_inputs),
        builder.CreateVector<int32_t>(subgraph_outputs),
        builder.CreateVector(operators.data(), operators.size()));

    flatbuffers::Offset<flatbuffers::String> description =
        builder.CreateString(kModelDescription);

    Vector<flatbuffers::Offset<tflite::OperatorCode>> operator_codes = {
        {tflite::CreateOperatorCode(builder, tflite::BuiltinOperator_ADD)}};
    flatbuffers::Offset<tflite::Model> model_buffer = tflite::CreateModel(
        builder, TFLITE_SCHEMA_VERSION,
        builder.CreateVector(operator_codes.data(), operator_codes.size()),
        builder.CreateVector(&subgraph, 1), description,
        builder.CreateVector(buffers.data(), buffers.size()));

    tflite::FinishModelBuffer(builder, model_buffer);

    return builder.Release();
  }
};

TEST_P(MLGraphTestCrOS, BuildGraphWithTfliteModel) {
  V8TestingScope scope;

  {
    // Test element-wise add operator for two 1-D tensors.
    ElementWiseAddTester<float>{
        .lhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {2},
                .values = {1.0, 2.0}},
        .rhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {2},
                .values = {3.0, 4.0}},
        .expected = {.type = V8MLOperandType::Enum::kFloat32,
                     .dimensions = {2},
                     .values = {4.0, 6.0}}}
        .Test(*this, scope);
  }
  {
    // Test element-wise add operator for 1-D tensor broadcasting to 2-D
    // tensor.
    ElementWiseAddTester<float>{
        .lhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {2, 2},
                .values = {1.0, 2.0, 3.0, 4.0}},
        .rhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {2},
                .values = {5.0, 6.0}},
        .expected = {.type = V8MLOperandType::Enum::kFloat32,
                     .dimensions = {2, 2},
                     .values = {6.0, 8.0, 8.0, 10.0}}}
        .Test(*this, scope);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MLGraphTestCrOS,
    testing::Combine(::testing::Values(BackendType::kModelLoader),
                     ::testing::Values(ExecutionMode::kAsync)),
    TestVarietyToString);

}  // namespace blink
