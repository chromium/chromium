// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_tflite_converter.h"

#include "base/ranges/algorithm.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

namespace blink {

namespace {

// The version number of the Schema. Ideally all changes will be backward
// compatible. If that ever changes, we must ensure that version is the first
// entry in the new tflite root so that we can see that version is not 1.
#define TFLITE_SCHEMA_VERSION (3)

// Maps MLOperand to its index of `tflite::Tensor` array.
using OperandToIndexMap = HeapHashMap<Member<const MLOperand>, int32_t>;
using OperatorCodeOffset = flatbuffers::Offset<tflite::OperatorCode>;
using OperatorOffset = flatbuffers::Offset<tflite::Operator>;
using BufferOffset = flatbuffers::Offset<tflite::Buffer>;

int32_t GetOperatorInputIndex(const MLOperator* op,
                              const OperandToIndexMap& operand_to_index_map,
                              wtf_size_t index = 0) {
  CHECK_NE(op, nullptr);
  CHECK_LE(index, op->Inputs().size());
  const auto* const input = op->Inputs()[index].Get();
  return operand_to_index_map.at(input);
}

int32_t GetOperatorOutputIndex(const MLOperator* op,
                               const OperandToIndexMap& operand_to_index_map,
                               wtf_size_t index = 0) {
  CHECK_NE(op, nullptr);
  CHECK_LE(index, op->Outputs().size());
  const auto* const output = op->Outputs()[index].Get();
  return operand_to_index_map.at(output);
}

Vector<int32_t> ConvertDimensions(const Vector<uint32_t>& input_dimensions) {
  Vector<int32_t> output_dimensions;
  output_dimensions.reserve(input_dimensions.size());
  base::ranges::transform(input_dimensions,
                          std::back_inserter(output_dimensions),
                          [](const auto& dimension) {
                            return base::checked_cast<int32_t>(dimension);
                          });
  return output_dimensions;
}

tflite::TensorType BlinkOperandTypeToTFLite(V8MLOperandType::Enum type) {
  switch (type) {
    case V8MLOperandType::Enum::kFloat32:
      return tflite::TensorType_FLOAT32;
    case V8MLOperandType::Enum::kFloat16:
      return tflite::TensorType_FLOAT16;
    case V8MLOperandType::Enum::kInt32:
      return tflite::TensorType_INT32;
    case V8MLOperandType::Enum::kUint32:
      return tflite::TensorType_UINT32;
    case V8MLOperandType::Enum::kInt8:
      return tflite::TensorType_INT8;
    case V8MLOperandType::Enum::kUint8:
      return tflite::TensorType_UINT8;
  }
  NOTREACHED_NORETURN();
}

uint32_t GetOperatorCodeIndex(tflite::BuiltinOperator code,
                              flatbuffers::FlatBufferBuilder& builder,
                              Vector<OperatorCodeOffset>& operator_codes) {
  auto operator_code_index =
      base::checked_cast<uint32_t>(operator_codes.size());
  operator_codes.push_back(tflite::CreateOperatorCode(builder, code));
  // The type of operation is determined by the index into the list of the valid
  // OperatorCodes.
  return operator_code_index;
}

OperatorOffset SerializeRelu(const OperandToIndexMap& operand_to_index_map,
                             const MLOperator* relu,
                             flatbuffers::FlatBufferBuilder& builder,
                             Vector<OperatorCodeOffset>& operator_codes) {
  const int32_t input_index = GetOperatorInputIndex(relu, operand_to_index_map);
  const int32_t output_index =
      GetOperatorOutputIndex(relu, operand_to_index_map);

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const auto operator_code_index = GetOperatorCodeIndex(
      tflite::BuiltinOperator_RELU, builder, operator_codes);
  const std::vector<int32_t> op_inputs = {input_index};
  const std::vector<int32_t> op_outputs = {output_index};
  return tflite::CreateOperator(builder, operator_code_index,
                                builder.CreateVector<int32_t>(op_inputs),
                                builder.CreateVector<int32_t>(op_outputs));
}

}  // namespace

MLGraphTfLiteConverter::MLGraphTfLiteConverter() {
  // TFLite requires the first entry in FlatBuffer to be an empty buffer.
  buffers_.push_back(tflite::CreateBuffer(builder_, builder_.CreateVector({})));
}

MLGraphTfLiteConverter::~MLGraphTfLiteConverter() = default;

uint32_t MLGraphTfLiteConverter::SerializeBuffer(
    const Member<const MLOperand>& constant) {
  auto* const array_buffer_view = constant->ArrayBufferView();
  CHECK_NE(array_buffer_view, nullptr);
  CHECK(!array_buffer_view->IsDetached());
  // Create `tflite::Buffer` with raw data buffers for constant operand.
  const auto buffer_data =
      builder_.CreateVector(reinterpret_cast<const uint8_t*>(
                                array_buffer_view->BaseAddressMaybeShared()),
                            array_buffer_view->byteLength());
  const auto buffer_index = base::checked_cast<uint32_t>(buffers_.size());
  buffers_.emplace_back(tflite::CreateBuffer(builder_, buffer_data));
  // The index of buffer is referenced by tensors.
  return buffer_index;
}

int32_t MLGraphTfLiteConverter::SerializeTensor(
    const Member<const MLOperand>& operand,
    absl::optional<String> graph_output_name) {
  // The buffer index 0 represents input and output operand because there is no
  // data buffer associated.
  uint32_t buffer_index = 0;
  // The index of `tflite::Tensor` array, each `MLOperand` (input, constant,
  // output) will be converted and pushed back into the array, so it's increased
  // by one after each serialization in flat buffer.
  int32_t tensor_index = base::checked_cast<int32_t>(tensors_.size());
  CHECK_GE(tensor_index, int32_t(0));
  // The name identifies the tensor for inference, so only inputs and outputs of
  // graph have this attribute.
  absl::optional<String> name;
  switch (operand->Kind()) {
    case MLOperand::OperandKind::kInput: {
      name = operand->Name();
      // Fill the graph inputs with the index of input tensor.
      graph_input_ids_.push_back(tensor_index);
      break;
    }
    case MLOperand::OperandKind::kConstant: {
      // Serialize buffer and return buffer index which starts from 1, it is
      // used to create the constant's tensor.
      buffer_index = SerializeBuffer(operand);
      break;
    }
    case MLOperand::OperandKind::kOutput: {
      // The `kOutput` represents not only the intermediate operands of
      // operation, but also the outputs of graph.
      // It's a graph output if the argument `graph_output_name` has value.
      if (graph_output_name) {
        name = graph_output_name.value();
        // Fill the graph outputs with the index of output tensor.
        graph_outputs_ids_.push_back(tensor_index);
      }
      break;
    }
  }
  // Create `Tensor` with operand shape, the index of buffer and the name.
  const auto dimensions =
      builder_.CreateVector<int32_t>(ConvertDimensions(operand->Dimensions()));
  const auto operand_type = BlinkOperandTypeToTFLite(operand->Type());
  const auto operand_name =
      name.has_value() ? builder_.CreateString(name->Utf8()) : 0;
  tensors_.emplace_back(tflite::CreateTensor(builder_, std::move(dimensions),
                                             operand_type, buffer_index,
                                             operand_name));
  return tensor_index;
}

base::expected<void, String> MLGraphTfLiteConverter::SerializeOperation(
    const OperandToIndexMap& operand_to_index_map,
    const MLOperator* op) {
  OperatorOffset operator_offset;
  switch (op->Kind()) {
    case MLOperator::OperatorKind::kRelu:
      operator_offset =
          SerializeRelu(operand_to_index_map, op, builder_, operator_codes_);
      break;
    default:
      return base::unexpected(MLOperator::OperatorKindToString(op->Kind()) +
                              " is not implemented.");
  }
  operators_.emplace_back(operator_offset);

  return base::ok();
}

flatbuffers::DetachedBuffer MLGraphTfLiteConverter::FinishAndTakeFlatBuffer() {
  CHECK(!is_created_model_);

  // Create `tflite::SubGraph`, which typically represents an entire model.
  // The inputs of subgraph are the list of non-static tensors that feed into
  // the subgraph for inference. The outputs of subgraph are considered the
  // product of the subgraph's inference. The operators are in execution order.
  flatbuffers::Offset<tflite::SubGraph> subgraph = tflite::CreateSubGraph(
      builder_, builder_.CreateVector(tensors_.data(), tensors_.size()),
      builder_.CreateVector<int32_t>(graph_input_ids_),
      builder_.CreateVector<int32_t>(graph_outputs_ids_),
      builder_.CreateVector(operators_.data(), operators_.size()));

  flatbuffers::Offset<flatbuffers::String> description =
      builder_.CreateString("TF-Lite model converted from WebNN Graph");

  // The operator codes used in this model are kept in order because operators
  // carry an index into this vector.
  // There is only one subgraph in the model. The buffers of the model must be
  // initialized an empty buffer.
  flatbuffers::Offset<tflite::Model> model_buffer = tflite::CreateModel(
      builder_, TFLITE_SCHEMA_VERSION,
      builder_.CreateVector(operator_codes_.data(), operator_codes_.size()),
      builder_.CreateVector(&subgraph, 1), description,
      builder_.CreateVector(buffers_.data(), buffers_.size()));

  tflite::FinishModelBuffer(builder_, model_buffer);
  is_created_model_ = true;

  return builder_.Release();
}

}  // namespace blink
