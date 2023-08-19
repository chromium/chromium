// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_test_utils.h"

#include "base/check_is_test.h"

namespace webnn {

GraphInfoBuilder::GraphInfoBuilder() {
  graph_info_ = mojom::GraphInfo::New();
}
GraphInfoBuilder::~GraphInfoBuilder() = default;

uint64_t GraphInfoBuilder::BuildOperand(const std::vector<uint32_t>& dimensions,
                                        mojom::Operand::DataType type,
                                        mojom::Operand::Kind kind) {
  mojom::OperandPtr operand = mojom::Operand::New();
  operand->data_type = type;
  operand->dimensions = dimensions;
  operand->kind = kind;

  CHECK(graph_info_->id_to_operand_map.find(operand_id_) ==
        graph_info_->id_to_operand_map.end());
  graph_info_->id_to_operand_map[operand_id_] = std::move(operand);
  return operand_id_++;
}

uint64_t GraphInfoBuilder::BuildInput(const std::string& name,
                                      const std::vector<uint32_t>& dimensions,
                                      mojom::Operand::DataType type) {
  uint64_t operand_id =
      BuildOperand(dimensions, type, mojom::Operand::Kind::kInput);
  graph_info_->id_to_operand_map[operand_id]->name = name;
  graph_info_->input_operands.push_back(operand_id);
  return operand_id;
}

uint64_t GraphInfoBuilder::BuildOutput(const std::string& name,
                                       const std::vector<uint32_t>& dimensions,
                                       mojom::Operand::DataType type) {
  uint64_t operand_id = BuildOperand(dimensions, type);
  graph_info_->id_to_operand_map[operand_id]->name = name;
  graph_info_->output_operands.push_back(operand_id);
  return operand_id;
}

void GraphInfoBuilder::BuildOperator(
    mojom::Operator::Kind kind,
    const std::vector<uint64_t>& inputs,
    const std::vector<uint64_t>& outputs,
    mojom::OperatorAttributesPtr operator_attributes) {
  mojom::OperatorPtr operation = mojom::Operator::New();
  operation->kind = kind;
  operation->input_operands = inputs;
  operation->output_operands = outputs;
  operation->attributes = std::move(operator_attributes);
  graph_info_->operators.push_back(std::move(operation));
}

mojom::GraphInfoPtr GraphInfoBuilder::CloneGraphInfo() const {
  CHECK_IS_TEST();
  return graph_info_.Clone();
}

}  // namespace webnn
