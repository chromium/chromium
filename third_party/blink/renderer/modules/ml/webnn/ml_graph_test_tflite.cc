// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/builtin_op_kernels.h"
#include "third_party/tflite/src/tensorflow/lite/model.h"
#include "third_party/tflite/src/tensorflow/lite/mutable_op_resolver.h"
#include "third_party/tflite/src/tensorflow/lite/schema/schema_generated.h"

namespace blink {

// The version number of the Schema. Ideally all changes will be backward
// compatible. If that ever changes, we must ensure that version is the first
// entry in the new tflite root so that we can see that version is not 1.
#define TFLITE_SCHEMA_VERSION (3)

// This class maintains all the currently supported TFLite
// operations for the Chromium build of TFLite and registers them for use.
class TFLiteOpResolver : public tflite::MutableOpResolver {
 public:
  TFLiteOpResolver() {
    AddBuiltin(tflite::BuiltinOperator_ADD,
               tflite::ops::builtin::Register_ADD(),
               /* min_version = */ 1,
               /* max_version = */ 2);
  }
};

// Helper function to get the data of result into a vector.
template <typename T>
Vector<T> GetResult(const WTF::Vector<uint8_t>& result) {
  Vector<T> values(base::checked_cast<wtf_size_t>(result.size() / sizeof(T)));
  memcpy(values.data(), result.data(), result.size());
  return values;
}

template <typename T>
struct OperandInfo {
  tflite::TensorType type;
  Vector<int32_t> dimensions;
  Vector<T> values;
};

template <typename T>
struct ElementWiseBinaryTester {
  tflite::BuiltinOperator kind;
  OperandInfo<T> lhs;
  OperandInfo<T> rhs;
  Vector<T> expected;

  void Test() {
    flatbuffers::FlatBufferBuilder builder;
    // It is required that the first entry in the buffers of model is always an
    // empty buffer. This is so that the default buffer index of zero in Tensor
    // will always refer to a valid empty buffer.
    Vector<flatbuffers::Offset<tflite::Buffer>> buffers = {
        tflite::CreateBuffer(builder, builder.CreateVector({})),
    };
    // Create tflite |Buffer| for first input tensor.
    buffers.push_back(tflite::CreateBuffer(
        builder, builder.CreateVector(
                     reinterpret_cast<const uint8_t*>(lhs.values.data()),
                     sizeof(T) * lhs.values.size())));
    // Create tflite |Buffer| for second input tensor.
    buffers.push_back(tflite::CreateBuffer(
        builder, builder.CreateVector(
                     reinterpret_cast<const uint8_t*>(rhs.values.data()),
                     sizeof(T) * rhs.values.size())));

    // A list of all tflite |Tensor| used in this model.
    Vector<flatbuffers::Offset<tflite::Tensor>> tensors;
    // Create tflite |Tensor| for first input tensor.
    uint32_t lhs_buffer_index = 1;
    tensors.emplace_back(tflite::CreateTensor(
        builder, builder.CreateVector<int32_t>(lhs.dimensions), lhs.type,
        lhs_buffer_index));
    // Create tflite |Tensor| for second input tensor.
    uint32_t rhs_buffer_index = 2;
    tensors.emplace_back(tflite::CreateTensor(
        builder, builder.CreateVector<int32_t>(rhs.dimensions), rhs.type,
        rhs_buffer_index));
    // Create tflite |Tensor| for output tensor.
    DCHECK(lhs.dimensions == rhs.dimensions);
    tensors.emplace_back(tflite::CreateTensor(
        builder, builder.CreateVector<int32_t>(lhs.dimensions), lhs.type));

    // A list of all tflite |Operator| used in this model.
    Vector<flatbuffers::Offset<tflite::Operator>> operators;
    int32_t lhs_tensor_index = 0, rhs_tensor_index = 1, output_tensor_index = 2;
    Vector<int32_t> op_inputs = {lhs_tensor_index, rhs_tensor_index};
    Vector<int32_t> op_outputs = {output_tensor_index};
    operators.emplace_back(tflite::CreateOperator(
        builder, 0, builder.CreateVector<int32_t>(op_inputs),
        builder.CreateVector<int32_t>(op_outputs)));

    // Create subgraph in the model.
    Vector<int32_t> subgraph_outputs = {output_tensor_index};
    flatbuffers::Offset<tflite::SubGraph> subgraph = tflite::CreateSubGraph(
        builder, builder.CreateVector(tensors.data(), tensors.size()),
        builder.CreateVector<int32_t>({}),
        builder.CreateVector<int32_t>(subgraph_outputs),
        builder.CreateVector(operators.data(), operators.size()));

    flatbuffers::Offset<flatbuffers::String> description =
        builder.CreateString("ElementWise Binary model");

    Vector<flatbuffers::Offset<tflite::OperatorCode>> operator_codes = {
        {tflite::CreateOperatorCode(builder, kind)}};
    flatbuffers::Offset<tflite::Model> model_buffer = tflite::CreateModel(
        builder, TFLITE_SCHEMA_VERSION,
        builder.CreateVector(operator_codes.data(), operator_codes.size()),
        builder.CreateVector(&subgraph, 1), description,
        builder.CreateVector(buffers.data(), buffers.size()));

    tflite::FinishModelBuffer(builder, model_buffer);

    // Compute the graph.
    std::unique_ptr<tflite::Interpreter> interpreter;
    const tflite::Model* model = tflite::GetModel(builder.GetBufferPointer());
    EXPECT_NE(model, nullptr);
    TFLiteOpResolver op_resolver;
    EXPECT_EQ(tflite::InterpreterBuilder(model, op_resolver)(&interpreter),
              kTfLiteOk);
    EXPECT_NE(interpreter, nullptr);
    EXPECT_EQ(interpreter->AllocateTensors(), kTfLiteOk);

    EXPECT_EQ(interpreter->Invoke(), kTfLiteOk);

    // Get output data after computing the model.
    EXPECT_EQ(interpreter->outputs().size(), 1u);
    auto* tensor = interpreter->tensor(output_tensor_index);
    WTF::Vector<uint8_t> output_data(static_cast<wtf_size_t>(tensor->bytes));
    memcpy(output_data.data(), tensor->data.raw, tensor->bytes);
    auto results = GetResult<T>(output_data);
    EXPECT_EQ(results, expected);
  }
};

class MLGraphTestTfLite : public testing::Test {
 public:
  MLGraphTestTfLite() = default;
  ~MLGraphTestTfLite() override = default;
};

TEST_F(MLGraphTestTfLite, ElementWiseAddTest) {
  {
    // Test element-wise add operator for two 1-D tensors.
    // The expected results should be the sum of the values of the two input
    // tensors, element-wise.
    ElementWiseBinaryTester<float>{.kind = tflite::BuiltinOperator_ADD,
                                   .lhs = {.type = tflite::TensorType_FLOAT32,
                                           .dimensions = {2},
                                           .values = {1.0, 2.0}},
                                   .rhs = {.type = tflite::TensorType_FLOAT32,
                                           .dimensions = {2},
                                           .values = {3.0, 4.0}},
                                   .expected = {4.0, 6.0}}
        .Test();
  }
}

}  // namespace blink
