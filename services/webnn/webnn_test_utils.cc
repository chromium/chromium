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

uint64_t GraphInfoBuilder::BuildIntermediateOperand(
    const std::vector<uint32_t>& dimensions,
    mojom::Operand::DataType type) {
  return BuildOperand(dimensions, type, mojom::Operand::Kind::kOutput);
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

uint64_t GraphInfoBuilder::BuildConstant(
    const std::vector<uint32_t>& dimensions,
    mojom::Operand::DataType type,
    base::span<const uint8_t> values) {
  uint64_t operand_id =
      BuildOperand(dimensions, type, mojom::Operand::Kind::kConstant);
  graph_info_->constant_id_to_buffer_map[operand_id] =
      mojo_base::BigBuffer(values);
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

void GraphInfoBuilder::BuildPad(uint64_t input_operand_id,
                                uint64_t output_operand_id,
                                const std::vector<uint32_t>& beginning_padding,
                                const std::vector<uint32_t>& ending_padding,
                                mojom::PaddingMode::Tag mode,
                                float value) {
  mojom::PadPtr pad = mojom::Pad::New();
  pad->input_operand_id = input_operand_id;
  pad->output_operand_id = output_operand_id;
  pad->beginning_padding = beginning_padding;
  pad->ending_padding = ending_padding;
  switch (mode) {
    case mojom::PaddingMode::Tag::kConstant: {
      auto constant_padding = mojom::ConstantPadding::New();
      constant_padding->value = value;
      pad->mode = mojom::PaddingMode::NewConstant(std::move(constant_padding));
      break;
    }
    case mojom::PaddingMode::Tag::kEdge:
      pad->mode = mojom::PaddingMode::NewEdge(mojom::EdgePadding::New());
      break;
    case mojom::PaddingMode::Tag::kReflection:
      pad->mode =
          mojom::PaddingMode::NewReflection(mojom::ReflectionPadding::New());
      break;
    case mojom::PaddingMode::Tag::kSymmetric:
      pad->mode =
          mojom::PaddingMode::NewSymmetric(mojom::SymmetricPadding::New());
      break;
  }

  graph_info_->operations.push_back(mojom::Operation::NewPad(std::move(pad)));
}

void GraphInfoBuilder::BuildSplit(
    uint64_t input_operand_id,
    const std::vector<uint64_t>& output_operand_ids,
    uint32_t axis) {
  mojom::SplitPtr split = mojom::Split::New();
  split->input_operand_id = input_operand_id;
  split->output_operand_ids = output_operand_ids;
  split->axis = axis;

  graph_info_->operations.push_back(
      mojom::Operation::NewSplit(std::move(split)));
}

void GraphInfoBuilder::BuildClamp(uint64_t input_operand_id,
                                  uint64_t output_operand_id,
                                  float min_value,
                                  float max_value) {
  mojom::ClampPtr clamp = mojom::Clamp::New();
  clamp->input_operand_id = input_operand_id;
  clamp->output_operand_id = output_operand_id;
  clamp->min_value = min_value;
  clamp->max_value = max_value;
  graph_info_->operations.push_back(
      mojom::Operation::NewClamp(std::move(clamp)));
}

void GraphInfoBuilder::BuildConcat(std::vector<uint64_t> input_operand_ids,
                                   uint64_t output_operand_id,
                                   uint32_t axis) {
  mojom::ConcatPtr concat = mojom::Concat::New();
  concat->input_operand_ids = std::move(input_operand_ids);
  concat->output_operand_id = output_operand_id;
  concat->axis = axis;
  graph_info_->operations.push_back(
      mojom::Operation::NewConcat(std::move(concat)));
}

void GraphInfoBuilder::BuildElementWiseBinary(
    mojom::ElementWiseBinary::Kind kind,
    uint64_t lhs_operand,
    uint64_t rhs_operand,
    uint64_t output_operand) {
  mojom::ElementWiseBinaryPtr binary = mojom::ElementWiseBinary::New();
  binary->kind = kind;
  binary->lhs_operand = lhs_operand;
  binary->rhs_operand = rhs_operand;
  binary->output_operand = output_operand;
  graph_info_->operations.push_back(
      mojom::Operation::NewElementWiseBinary(std::move(binary)));
}

void GraphInfoBuilder::BuildPrelu(uint64_t input_operand_id,
                                  uint64_t slope_operand_id,
                                  uint64_t output_operand_id) {
  mojom::PreluPtr prelu = mojom::Prelu::New();
  prelu->input_operand_id = input_operand_id;
  prelu->slope_operand_id = slope_operand_id;
  prelu->output_operand_id = output_operand_id;
  graph_info_->operations.push_back(
      mojom::Operation::NewPrelu(std::move(prelu)));
}

void GraphInfoBuilder::BuildRelu(uint64_t input_operand_id,
                                 uint64_t output_operand_id) {
  mojom::ReluPtr relu = mojom::Relu::New();
  relu->input_operand_id = input_operand_id;
  relu->output_operand_id = output_operand_id;
  graph_info_->operations.push_back(mojom::Operation::NewRelu(std::move(relu)));
}

void GraphInfoBuilder::BuildReshape(uint64_t input_operand_id,
                                    uint64_t output_operand_id) {
  mojom::ReshapePtr reshape = mojom::Reshape::New();
  reshape->input_operand_id = input_operand_id;
  reshape->output_operand_id = output_operand_id;
  graph_info_->operations.push_back(
      mojom::Operation::NewReshape(std::move(reshape)));
}

void GraphInfoBuilder::BuildSigmoid(uint64_t input_operand_id,
                                    uint64_t output_operand_id) {
  mojom::SigmoidPtr sigmoid = mojom::Sigmoid::New();
  sigmoid->input_operand_id = input_operand_id;
  sigmoid->output_operand_id = output_operand_id;
  graph_info_->operations.push_back(
      mojom::Operation::NewSigmoid(std::move(sigmoid)));
}

void GraphInfoBuilder::BuildSoftmax(uint64_t input_operand_id,
                                    uint64_t output_operand_id) {
  mojom::SoftmaxPtr softmax = mojom::Softmax::New();
  softmax->input_operand_id = input_operand_id;
  softmax->output_operand_id = output_operand_id;
  graph_info_->operations.push_back(
      mojom::Operation::NewSoftmax(std::move(softmax)));
}

void GraphInfoBuilder::BuildTanh(uint64_t input_operand_id,
                                 uint64_t output_operand_id) {
  mojom::TanhPtr tanh = mojom::Tanh::New();
  tanh->input_operand_id = input_operand_id;
  tanh->output_operand_id = output_operand_id;
  graph_info_->operations.push_back(mojom::Operation::NewTanh(std::move(tanh)));
}

void GraphInfoBuilder::BuildTranspose(uint64_t input_operand_id,
                                      uint64_t output_operand_id,
                                      std::vector<uint32_t> permutation) {
  mojom::TransposePtr transpose = mojom::Transpose::New();
  transpose->input_operand_id = input_operand_id;
  transpose->output_operand_id = output_operand_id;
  transpose->permutation = std::move(permutation);
  graph_info_->operations.push_back(
      mojom::Operation::NewTranspose(std::move(transpose)));
}

void GraphInfoBuilder::BuildSlice(uint64_t input_operand_id,
                                  uint64_t output_operand_id,
                                  std::vector<uint32_t> starts,
                                  std::vector<uint32_t> sizes) {
  CHECK(starts.size() == sizes.size());
  mojom::SlicePtr slice = mojom::Slice::New();
  slice->input_operand_id = input_operand_id;
  slice->output_operand_id = output_operand_id;
  for (uint32_t i = 0; i < starts.size(); ++i) {
    mojom::StartAndSizePtr start_and_size = mojom::StartAndSize::New();
    start_and_size->start = starts[i];
    start_and_size->size = sizes[i];
    slice->starts_and_sizes.push_back(std::move(start_and_size));
  }

  graph_info_->operations.push_back(
      mojom::Operation::NewSlice(std::move(slice)));
}

mojom::GraphInfoPtr GraphInfoBuilder::CloneGraphInfo() const {
  CHECK_IS_TEST();
  mojom::GraphInfoPtr cloned_graph_info = mojom::GraphInfo::New();
  for (auto& [operand_id, operand_info] : graph_info_->id_to_operand_map) {
    cloned_graph_info->id_to_operand_map[operand_id] = operand_info.Clone();
  }
  cloned_graph_info->input_operands = graph_info_->input_operands;
  cloned_graph_info->output_operands = graph_info_->output_operands;
  cloned_graph_info->operations.reserve(graph_info_->operations.size());
  for (auto& operation : graph_info_->operations) {
    cloned_graph_info->operations.push_back(operation.Clone());
  }
  for (auto& [constant_id, constant_buffer] :
       graph_info_->constant_id_to_buffer_map) {
    cloned_graph_info->constant_id_to_buffer_map[constant_id] =
        mojo_base::BigBuffer(constant_buffer.byte_span());
  }
  return cloned_graph_info;
}

}  // namespace webnn
