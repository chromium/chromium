// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_graph_impl.h"

#include <math.h>
#include <utility>
#include <vector>

#include "base/types/expected.h"
#include "components/ml/webnn/graph_validation_utils.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace webnn {

namespace {

// Maps the id to its `mojo::Operand`.
using IdToOperandMap = base::flat_map<uint64_t, mojom::OperandPtr>;

size_t GetBytesPerElement(mojom::Operand::DataType operand_type) {
  switch (operand_type) {
    case mojom::Operand::DataType::kFloat32:
      return sizeof(float);
    case mojom::Operand::DataType::kFloat16:
      return sizeof(uint16_t);
    case mojom::Operand::DataType::kInt32:
      return sizeof(int32_t);
    case mojom::Operand::DataType::kUint32:
      return sizeof(uint32_t);
    case mojom::Operand::DataType::kInt8:
      return sizeof(int8_t);
    case mojom::Operand::DataType::kUint8:
      return sizeof(uint8_t);
  }
  NOTREACHED();
}

bool IsFloatingPointType(mojom::Operand::DataType data_type) {
  switch (data_type) {
    case mojom::Operand::DataType::kFloat32:
    case mojom::Operand::DataType::kFloat16:
      return true;
    case mojom::Operand::DataType::kInt32:
    case mojom::Operand::DataType::kUint32:
    case mojom::Operand::DataType::kInt8:
    case mojom::Operand::DataType::kUint8:
      return false;
  }
  NOTREACHED_NORETURN();
}

bool ValidateInputOperand(const IdToOperandMap& id_to_operand_map,
                          uint64_t input_id) {
  if (!id_to_operand_map.contains(input_id)) {
    // Invalid input operand.
    return false;
  }

  const mojom::OperandPtr& operand = id_to_operand_map.at(input_id);
  if (operand->kind != mojom::Operand::Kind::kInput) {
    // Invalid input kind.
    return false;
  }
  const absl::optional<std::string>& name = operand->name;
  if (name && name.value().empty()) {
    // The name of input operand is empty.
    return false;
  }

  return true;
}

bool ValidateOutputOperand(const IdToOperandMap& id_to_operand_map,
                           uint64_t output_id) {
  if (!id_to_operand_map.contains(output_id)) {
    // Invalid output operand.
    return false;
  }

  const mojom::OperandPtr& operand = id_to_operand_map.at(output_id);
  if (operand->kind != mojom::Operand::Kind::kOutput) {
    // Invalid output kind.
    return false;
  }
  absl::optional<std::string>& name = operand->name;
  if (name && name.value().empty()) {
    // The name of output operand is empty.
    return false;
  }
  return true;
}

const mojom::Operand* GetMojoOperand(
    const IdToOperandMap& id_to_operand_map,
    const std::vector<uint64_t>& operand_id_array,
    size_t index = 0) {
  if (index >= operand_id_array.size()) {
    // Index out of range.
    return nullptr;
  }
  uint64_t operand_id = operand_id_array[index];
  if (!id_to_operand_map.contains(operand_id)) {
    // There is no operand for the id.
    return nullptr;
  }
  return id_to_operand_map.at(operand_id).get();
}

bool ValidateClamp(const IdToOperandMap& id_to_operand_map,
                   const mojom::OperatorPtr& operation) {
  auto* input = GetMojoOperand(id_to_operand_map, operation->input_operands);
  auto* output = GetMojoOperand(id_to_operand_map, operation->output_operands);
  if (!input || !output || !operation->attributes) {
    // The clamp operator is invalid.
    return false;
  }
  auto& clamp_attributes = operation->attributes->get_clamp();
  if (!clamp_attributes) {
    // The attributes of clamp were not configured.
    return false;
  }
  if (std::isnan(clamp_attributes->min_value) ||
      std::isnan(clamp_attributes->max_value)) {
    // The min or max value are nan.
    return false;
  }
  if (clamp_attributes->min_value >= clamp_attributes->max_value) {
    // The min value must be below the max value.
    return false;
  }
  if (output->data_type != input->data_type) {
    // The output data type doesn't match input data type.
    return false;
  }

  if (output->dimensions != input->dimensions) {
    // The output shape is not expected.
    return false;
  }

  return true;
}

bool ValidateElementWiseBinary(const IdToOperandMap& id_to_operand_map,
                               const mojom::OperatorPtr& operation) {
  auto* a = GetMojoOperand(id_to_operand_map, operation->input_operands, 0);
  auto* b = GetMojoOperand(id_to_operand_map, operation->input_operands, 1);
  auto* output = GetMojoOperand(id_to_operand_map, operation->output_operands);
  if (!a || !b || !output) {
    // The elementWise binary operator is invalid.
    return false;
  }
  if (a->data_type != b->data_type || output->data_type != a->data_type) {
    // The input types don't match.
    return false;
  }

  auto dims_output = BroadcastShapes(a->dimensions, b->dimensions);
  if (!dims_output) {
    // The input shapes are not broadcastable.
    return false;
  }
  if (output->dimensions != dims_output.value()) {
    // The output shape is not expected.
    return false;
  }
  return true;
}

bool ValidateRelu(const IdToOperandMap& id_to_operand_map,
                  const mojom::OperatorPtr& operation) {
  auto* input = GetMojoOperand(id_to_operand_map, operation->input_operands);
  auto* output = GetMojoOperand(id_to_operand_map, operation->output_operands);
  if (!input || !output) {
    // The relu operator is invalid.
    return false;
  }
  if (output->data_type != input->data_type) {
    // The output data type doesn't match input data type.
    return false;
  }

  if (output->dimensions != input->dimensions) {
    // The output shape is not expected.
    return false;
  }
  return true;
}

bool ValidateReshape(const IdToOperandMap& id_to_operand_map,
                     const mojom::OperatorPtr& operation) {
  auto* input = GetMojoOperand(id_to_operand_map, operation->input_operands);
  auto* output = GetMojoOperand(id_to_operand_map, operation->output_operands);
  if (!input || !output) {
    // The reshape operator is invalid.
    return false;
  }
  if (output->data_type != input->data_type) {
    // The output data type doesn't match input data type.
    return false;
  }

  base::expected<size_t, std::string> output_number_of_elements =
      ValidateAndCalculateElementsNumber(output->dimensions);
  // The dimensions of input and output operand are valid which were already
  // validated before calling this function.
  CHECK(output_number_of_elements.has_value());
  base::expected<size_t, std::string> input_number_of_elements =
      ValidateAndCalculateElementsNumber(input->dimensions);
  CHECK(input_number_of_elements.has_value());
  if (output_number_of_elements.value() != input_number_of_elements.value()) {
    // The output shape is not expected.
    return false;
  }
  return true;
}

bool ValidateSoftmax(const IdToOperandMap& id_to_operand_map,
                     const mojom::OperatorPtr& operation) {
  auto* input = GetMojoOperand(id_to_operand_map, operation->input_operands);
  auto* output = GetMojoOperand(id_to_operand_map, operation->output_operands);
  if (!input || !output) {
    // The softmax operator is invalid.
    return false;
  }
  if (input->dimensions.size() != 2) {
    // The input must be a 2-D tensor.
    return false;
  }
  if (output->dimensions != input->dimensions) {
    // The output shape is not expected.
    return false;
  }
  if (!IsFloatingPointType(input->data_type)) {
    // The input type must be one of the floating point types.
    return false;
  }
  if (output->data_type != input->data_type) {
    // The output data type doesn't match input data type.
    return false;
  }

  return true;
}

bool ValidateOperator(const IdToOperandMap& id_to_operand_map,
                      const mojom::OperatorPtr& operation) {
  switch (operation->kind) {
    case mojom::Operator::Kind::kClamp:
      return ValidateClamp(id_to_operand_map, operation);
    case mojom::Operator::Kind::kAdd:
    case mojom::Operator::Kind::kSub:
    case mojom::Operator::Kind::kMul:
    case mojom::Operator::Kind::kDiv:
    case mojom::Operator::Kind::kMax:
    case mojom::Operator::Kind::kMin:
      return ValidateElementWiseBinary(id_to_operand_map, operation);
    case mojom::Operator::Kind::kRelu:
      return ValidateRelu(id_to_operand_map, operation);
    case mojom::Operator::Kind::kReshape:
      return ValidateReshape(id_to_operand_map, operation);
    case mojom::Operator::Kind::kSoftmax:
      return ValidateSoftmax(id_to_operand_map, operation);
  }
  NOTREACHED_NORETURN();
}

bool ValidateGraphInfo(const mojom::GraphInfoPtr& graph_info) {
  // The input operands of graph can be empty.
  if (graph_info->id_to_operand_map.empty() || graph_info->operators.empty() ||
      graph_info->output_operands.empty()) {
    return false;
  }

  // Validate all operands in the graph for the dimensions and the byte length
  // of operand that can't be out of range.
  for (auto& [_, operand] : graph_info->id_to_operand_map) {
    base::expected<size_t, std::string> byte_length =
        ValidateAndCalculateByteLength(GetBytesPerElement(operand->data_type),
                                       operand->dimensions);
    if (!byte_length.has_value()) {
      return false;
    }
  }

  // Validate the input operands of graph for the name that can't be empty, and
  // the kind of operand must be `kInput`.
  for (auto& input_id : graph_info->input_operands) {
    if (!ValidateInputOperand(graph_info->id_to_operand_map, input_id)) {
      return false;
    }
  }

  // Validate the operators which are sorted in the topological order.
  for (auto& operation : graph_info->operators) {
    if (!ValidateOperator(graph_info->id_to_operand_map, operation)) {
      return false;
    }
  }

  // Validate the output operands in the entire graph for the name that can't be
  // empty, and the kind of operand must be `kOutput`.
  for (auto& output_id : graph_info->output_operands) {
    if (!ValidateOutputOperand(graph_info->id_to_operand_map, output_id)) {
      return false;
    }
  }

  return true;
}

}  // namespace

WebNNGraphImpl::WebNNGraphImpl() = default;

WebNNGraphImpl::~WebNNGraphImpl() = default;

// static
bool WebNNGraphImpl::ValidateAndBuildGraph(
    mojom::WebNNContext::CreateGraphCallback callback,
    const mojom::GraphInfoPtr& graph_info) {
  if (!ValidateGraphInfo(graph_info)) {
    return false;
  }

  // The remote sent to the renderer.
  mojo::PendingRemote<mojom::WebNNGraph> blink_remote;
  // The receiver bound to WebNNGraphImpl.
  mojo::MakeSelfOwnedReceiver<mojom::WebNNGraph>(
      std::make_unique<WebNNGraphImpl>(),
      blink_remote.InitWithNewPipeAndPassReceiver());
  // TODO(crbug.com/1273291): Build graph with OS machine learning APIs.
  // webnn_graph_impl->BuildGraph(std::move(callback), std::move(blink_remote));
  std::move(callback).Run(std::move(blink_remote));
  return true;
}

}  // namespace webnn
