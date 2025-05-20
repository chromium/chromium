// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_test_utils.h"

#include <limits.h>

#include "base/check_is_test.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/supported_tensors.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/webnn_context_impl.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace webnn {

GraphInfoBuilder::GraphInfoBuilder(
    mojo::AssociatedRemote<mojom::WebNNGraphBuilder>& graph_builder_remote)
    : graph_info_(mojom::GraphInfo::New()),
      graph_builder_remote_(graph_builder_remote) {}

GraphInfoBuilder::~GraphInfoBuilder() = default;

OperandId GraphInfoBuilder::BuildOperand(
    const std::vector<uint32_t>& dimensions,
    OperandDataType type,
    mojom::Operand::Kind kind) {
  mojom::OperandPtr operand = mojom::Operand::New();

  operand->descriptor =
      OperandDescriptor::UnsafeCreateForTesting(type, dimensions);
  operand->kind = kind;

  graph_info_->operands.push_back(std::move(operand));
  return OperandId(graph_info_->operands.size() - 1);
}

OperandId GraphInfoBuilder::BuildIntermediateOperand(
    const std::vector<uint32_t>& dimensions,
    OperandDataType type) {
  return BuildOperand(dimensions, type, mojom::Operand::Kind::kOutput);
}

OperandId GraphInfoBuilder::BuildInput(const std::string& name,
                                       const std::vector<uint32_t>& dimensions,
                                       OperandDataType type) {
  OperandId operand_id =
      BuildOperand(dimensions, type, mojom::Operand::Kind::kInput);
  graph_info_->operands[operand_id.value()]->name = name;
  graph_info_->input_operands.push_back(operand_id);
  return operand_id;
}

OperandId GraphInfoBuilder::BuildConstant(
    const std::vector<uint32_t>& dimensions,
    OperandDataType type,
    base::span<const uint8_t> values,
    blink::WebNNPendingConstantToken handle) {
  OperandId operand_id =
      BuildOperand(dimensions, type, mojom::Operand::Kind::kConstant);

  graph_builder_remote_->get()->CreatePendingConstant(
      handle, type, mojo_base::BigBuffer(values));
  graph_info_->constant_operand_ids_to_handles[operand_id.value()] =
      std::move(handle);
  return operand_id;
}

void GraphInfoBuilder::AddOutput(const std::string& name,
                                 OperandId operand_id) {
  graph_info_->operands[operand_id.value()]->name = name;
  graph_info_->output_operands.push_back(operand_id);
}

OperandId GraphInfoBuilder::BuildOutput(const std::string& name,
                                        const std::vector<uint32_t>& dimensions,
                                        OperandDataType type) {
  OperandId operand_id = BuildOperand(dimensions, type);
  AddOutput(name, operand_id);
  return operand_id;
}

void GraphInfoBuilder::BuildArgMinMax(mojom::ArgMinMax::Kind kind,
                                      OperandId input_operand_id,
                                      OperandId output_operand_id,
                                      uint32_t axis,
                                      bool keep_dimensions) {
  mojom::ArgMinMaxPtr arg_min_max = mojom::ArgMinMax::New();
  arg_min_max->kind = kind;
  arg_min_max->input_operand_id = input_operand_id;
  arg_min_max->output_operand_id = output_operand_id;
  arg_min_max->axis = axis;
  arg_min_max->keep_dimensions = keep_dimensions;
  graph_info_->operations.push_back(
      mojom::Operation::NewArgMinMax(std::move(arg_min_max)));
}

void GraphInfoBuilder::BuildElu(OperandId input_operand_id,
                                OperandId output_operand_id,
                                float alpha) {
  mojom::EluPtr elu = mojom::Elu::New();
  elu->input_operand_id = input_operand_id;
  elu->output_operand_id = output_operand_id;
  elu->alpha = alpha;
  graph_info_->operations.push_back(mojom::Operation::NewElu(std::move(elu)));
}

void GraphInfoBuilder::BuildLeakyRelu(OperandId input_operand_id,
                                      OperandId output_operand_id,
                                      float alpha) {
  mojom::LeakyReluPtr leaky_relu = mojom::LeakyRelu::New();
  leaky_relu->input_operand_id = input_operand_id;
  leaky_relu->output_operand_id = output_operand_id;
  leaky_relu->alpha = alpha;
  graph_info_->operations.push_back(
      mojom::Operation::NewLeakyRelu(std::move(leaky_relu)));
}

void GraphInfoBuilder::BuildLinear(OperandId input_operand_id,
                                   OperandId output_operand_id,
                                   float alpha,
                                   float beta) {
  mojom::LinearPtr linear = mojom::Linear::New();
  linear->input_operand_id = input_operand_id;
  linear->output_operand_id = output_operand_id;
  linear->alpha = alpha;
  linear->beta = beta;
  graph_info_->operations.push_back(
      mojom::Operation::NewLinear(std::move(linear)));
}

void GraphInfoBuilder::BuildPad(OperandId input_operand_id,
                                OperandId output_operand_id,
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
  }

  graph_info_->operations.push_back(mojom::Operation::NewPad(std::move(pad)));
}

void GraphInfoBuilder::BuildSplit(
    OperandId input_operand_id,
    const std::vector<OperandId>& output_operand_ids,
    uint32_t axis) {
  mojom::SplitPtr split = mojom::Split::New();
  split->input_operand_id = input_operand_id;
  split->output_operand_ids = output_operand_ids;
  split->axis = axis;

  graph_info_->operations.push_back(
      mojom::Operation::NewSplit(std::move(split)));
}

void GraphInfoBuilder::BuildClamp(OperandId input_operand_id,
                                  OperandId output_operand_id,
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

void GraphInfoBuilder::BuildConcat(std::vector<OperandId> input_operand_ids,
                                   OperandId output_operand_id,
                                   uint32_t axis) {
  mojom::ConcatPtr concat = mojom::Concat::New();
  concat->input_operand_ids = std::move(input_operand_ids);
  concat->output_operand_id = output_operand_id;
  concat->axis = axis;
  graph_info_->operations.push_back(
      mojom::Operation::NewConcat(std::move(concat)));
}

void GraphInfoBuilder::BuildCumulativeSum(OperandId input_operand_id,
                                          OperandId output_operand_id,
                                          uint32_t axis,
                                          std::optional<bool> exclusive,
                                          std::optional<bool> reversed) {
  mojom::CumulativeSumPtr cumulative_sum = mojom::CumulativeSum::New();
  cumulative_sum->input_operand_id = input_operand_id;
  cumulative_sum->output_operand_id = output_operand_id;
  cumulative_sum->axis = axis;
  if (exclusive.has_value()) {
    cumulative_sum->exclusive = exclusive.value();
  }
  if (reversed.has_value()) {
    cumulative_sum->reversed = reversed.value();
  }
  graph_info_->operations.push_back(
      mojom::Operation::NewCumulativeSum(std::move(cumulative_sum)));
}

void GraphInfoBuilder::BuildDequantizeLinear(OperandId input_operand_id,
                                             OperandId scale_operand_id,
                                             OperandId zero_point_operand_id,
                                             OperandId output_operand_id) {
  mojom::DequantizeLinearPtr dequantize_linear = mojom::DequantizeLinear::New();
  dequantize_linear->input_operand_id = input_operand_id;
  dequantize_linear->scale_operand_id = scale_operand_id;
  dequantize_linear->zero_point_operand_id = zero_point_operand_id;
  dequantize_linear->output_operand_id = output_operand_id;
  graph_info_->operations.push_back(
      mojom::Operation::NewDequantizeLinear(std::move(dequantize_linear)));
}

void GraphInfoBuilder::BuildElementWiseBinary(
    mojom::ElementWiseBinary::Kind kind,
    OperandId lhs_operand,
    OperandId rhs_operand,
    OperandId output_operand) {
  mojom::ElementWiseBinaryPtr binary = mojom::ElementWiseBinary::New();
  binary->kind = kind;
  binary->lhs_operand_id = lhs_operand;
  binary->rhs_operand_id = rhs_operand;
  binary->output_operand_id = output_operand;
  graph_info_->operations.push_back(
      mojom::Operation::NewElementWiseBinary(std::move(binary)));
}

void GraphInfoBuilder::BuildExpand(OperandId input_operand_id,
                                   OperandId output_operand_id) {
  graph_info_->operations.push_back(mojom::Operation::NewExpand(
      mojom::Expand::New(input_operand_id, output_operand_id, "")));
}

void GraphInfoBuilder::BuildMatmul(OperandId a_operand_id,
                                   OperandId b_operand_id,
                                   OperandId output_operand_id) {
  mojom::MatmulPtr matmul = mojom::Matmul::New();
  matmul->a_operand_id = a_operand_id;
  matmul->b_operand_id = b_operand_id;
  matmul->output_operand_id = output_operand_id;
  graph_info_->operations.push_back(
      mojom::Operation::NewMatmul(std::move(matmul)));
}

void GraphInfoBuilder::BuildElementWiseUnary(mojom::ElementWiseUnary::Kind kind,
                                             OperandId input_operand,
                                             OperandId output_operand) {
  mojom::ElementWiseUnaryPtr unary = mojom::ElementWiseUnary::New();
  unary->kind = kind;
  unary->input_operand_id = input_operand;
  unary->output_operand_id = output_operand;
  graph_info_->operations.push_back(
      mojom::Operation::NewElementWiseUnary(std::move(unary)));
}

void GraphInfoBuilder::BuildGather(OperandId input_operand_id,
                                   OperandId indices_operand_id,
                                   OperandId output_operand_id,
                                   uint32_t axis) {
  mojom::GatherPtr gather = mojom::Gather::New();
  gather->input_operand_id = input_operand_id;
  gather->output_operand_id = output_operand_id;
  gather->indices_operand_id = indices_operand_id;
  gather->axis = axis;
  graph_info_->operations.push_back(
      mojom::Operation::NewGather(std::move(gather)));
}

void GraphInfoBuilder::BuildGatherElements(OperandId input_operand_id,
                                           OperandId indices_operand_id,
                                           OperandId output_operand_id,
                                           uint32_t axis) {
  auto gather_elements = mojom::GatherElements::New();
  gather_elements->input_operand_id = input_operand_id;
  gather_elements->output_operand_id = output_operand_id;
  gather_elements->indices_operand_id = indices_operand_id;
  gather_elements->axis = axis;
  graph_info_->operations.push_back(
      mojom::Operation::NewGatherElements(std::move(gather_elements)));
}

void GraphInfoBuilder::BuildGatherND(OperandId input_operand_id,
                                     OperandId indices_operand_id,
                                     OperandId output_operand_id) {
  auto gather_nd = mojom::GatherND::New(input_operand_id, indices_operand_id,
                                        output_operand_id, "");
  graph_info_->operations.push_back(
      mojom::Operation::NewGatherNd(std::move(gather_nd)));
}

void GraphInfoBuilder::BuildGelu(OperandId input_operand_id,
                                 OperandId output_operand_id) {
  mojom::GeluPtr gelu =
      mojom::Gelu::New(input_operand_id, output_operand_id, "");
  graph_info_->operations.push_back(mojom::Operation::NewGelu(std::move(gelu)));
}

void GraphInfoBuilder::BuildHardSigmoid(OperandId input_operand_id,
                                        OperandId output_operand_id,
                                        std::optional<float> alpha,
                                        std::optional<float> beta) {
  mojom::HardSigmoidPtr hard_sigmoid = mojom::HardSigmoid::New();
  hard_sigmoid->input_operand_id = input_operand_id;
  hard_sigmoid->output_operand_id = output_operand_id;
  if (alpha.has_value()) {
    hard_sigmoid->alpha = alpha.value();
  }
  if (beta.has_value()) {
    hard_sigmoid->beta = beta.value();
  }
  graph_info_->operations.push_back(
      mojom::Operation::NewHardSigmoid(std::move(hard_sigmoid)));
}

void GraphInfoBuilder::BuildHardSwish(OperandId input_operand_id,
                                      OperandId output_operand_id) {
  mojom::HardSwishPtr hard_swish = mojom::HardSwish::New();
  hard_swish->input_operand_id = input_operand_id;
  hard_swish->output_operand_id = output_operand_id;
  graph_info_->operations.push_back(
      mojom::Operation::NewHardSwish(std::move(hard_swish)));
}

void GraphInfoBuilder::BuildPrelu(OperandId input_operand_id,
                                  OperandId slope_operand_id,
                                  OperandId output_operand_id) {
  mojom::PreluPtr prelu = mojom::Prelu::New();
  prelu->input_operand_id = input_operand_id;
  prelu->slope_operand_id = slope_operand_id;
  prelu->output_operand_id = output_operand_id;
  graph_info_->operations.push_back(
      mojom::Operation::NewPrelu(std::move(prelu)));
}

void GraphInfoBuilder::BuildQuantizeLinear(OperandId input_operand_id,
                                           OperandId scale_operand_id,
                                           OperandId zero_point_operand_id,
                                           OperandId output_operand_id) {
  mojom::QuantizeLinearPtr quantize_linear = mojom::QuantizeLinear::New();
  quantize_linear->input_operand_id = input_operand_id;
  quantize_linear->scale_operand_id = scale_operand_id;
  quantize_linear->zero_point_operand_id = zero_point_operand_id;
  quantize_linear->output_operand_id = output_operand_id;
  graph_info_->operations.push_back(
      mojom::Operation::NewQuantizeLinear(std::move(quantize_linear)));
}

void GraphInfoBuilder::BuildReduce(mojom::Reduce::Kind kind,
                                   OperandId input_operand_id,
                                   OperandId output_operand_id,
                                   std::vector<uint32_t> axes,
                                   bool keep_dimensions) {
  mojom::ReducePtr reduce = mojom::Reduce::New();
  reduce->kind = kind;
  reduce->input_operand_id = input_operand_id;
  reduce->output_operand_id = output_operand_id;
  reduce->axes = std::move(axes);
  reduce->keep_dimensions = keep_dimensions;
  graph_info_->operations.push_back(
      mojom::Operation::NewReduce(std::move(reduce)));
}

void GraphInfoBuilder::BuildRelu(OperandId input_operand_id,
                                 OperandId output_operand_id) {
  mojom::ReluPtr relu = mojom::Relu::New();
  relu->input_operand_id = input_operand_id;
  relu->output_operand_id = output_operand_id;
  graph_info_->operations.push_back(mojom::Operation::NewRelu(std::move(relu)));
}

void GraphInfoBuilder::BuildReshape(OperandId input_operand_id,
                                    OperandId output_operand_id) {
  mojom::ReshapePtr reshape = mojom::Reshape::New();
  reshape->input_operand_id = input_operand_id;
  reshape->output_operand_id = output_operand_id;
  graph_info_->operations.push_back(
      mojom::Operation::NewReshape(std::move(reshape)));
}

void GraphInfoBuilder::BuildReverse(OperandId input_operand_id,
                                    OperandId output_operand_id,
                                    std::vector<uint32_t> axes) {
  auto reverse = mojom::Reverse::New();
  reverse->input_operand_id = input_operand_id;
  reverse->output_operand_id = output_operand_id;
  reverse->axes = std::move(axes);
  graph_info_->operations.push_back(
      mojom::Operation::NewReverse(std::move(reverse)));
}

void GraphInfoBuilder::BuildScatterElements(OperandId input_operand_id,
                                            OperandId indices_operand_id,
                                            OperandId updates_operand_id,
                                            OperandId output_operand_id,
                                            uint32_t axis) {
  mojom::ScatterElementsPtr scatter_elements = mojom::ScatterElements::New(
      input_operand_id, indices_operand_id, updates_operand_id,
      output_operand_id, axis, "");
  graph_info_->operations.push_back(
      mojom::Operation::NewScatterElements(std::move(scatter_elements)));
}

void GraphInfoBuilder::BuildScatterND(OperandId input_operand_id,
                                      OperandId indices_operand_id,
                                      OperandId updates_operand_id,
                                      OperandId output_operand_id) {
  mojom::ScatterNDPtr scatter_nd =
      mojom::ScatterND::New(input_operand_id, indices_operand_id,
                            updates_operand_id, output_operand_id, "");
  graph_info_->operations.push_back(
      mojom::Operation::NewScatterNd(std::move(scatter_nd)));
}

void GraphInfoBuilder::BuildSigmoid(OperandId input_operand_id,
                                    OperandId output_operand_id) {
  mojom::SigmoidPtr sigmoid = mojom::Sigmoid::New();
  sigmoid->input_operand_id = input_operand_id;
  sigmoid->output_operand_id = output_operand_id;
  graph_info_->operations.push_back(
      mojom::Operation::NewSigmoid(std::move(sigmoid)));
}

void GraphInfoBuilder::BuildSoftmax(OperandId input_operand_id,
                                    OperandId output_operand_id,
                                    uint32_t axis) {
  mojom::SoftmaxPtr softmax =
      mojom::Softmax::New(input_operand_id, output_operand_id, axis, "");
  graph_info_->operations.push_back(
      mojom::Operation::NewSoftmax(std::move(softmax)));
}

void GraphInfoBuilder::BuildSoftplus(OperandId input_operand_id,
                                     OperandId output_operand_id) {
  auto softplus = mojom::Softplus::New(input_operand_id, output_operand_id, "");
  graph_info_->operations.push_back(
      mojom::Operation::NewSoftplus(std::move(softplus)));
}

void GraphInfoBuilder::BuildSoftsign(OperandId input_operand_id,
                                     OperandId output_operand_id) {
  mojom::SoftsignPtr softsign = mojom::Softsign::New();
  softsign->input_operand_id = input_operand_id;
  softsign->output_operand_id = output_operand_id;
  graph_info_->operations.push_back(
      mojom::Operation::NewSoftsign(std::move(softsign)));
}

void GraphInfoBuilder::BuildTanh(OperandId input_operand_id,
                                 OperandId output_operand_id) {
  mojom::TanhPtr tanh = mojom::Tanh::New();
  tanh->input_operand_id = input_operand_id;
  tanh->output_operand_id = output_operand_id;
  graph_info_->operations.push_back(mojom::Operation::NewTanh(std::move(tanh)));
}

void GraphInfoBuilder::BuildTile(OperandId input_operand_id,
                                 OperandId output_operand_id,
                                 std::vector<uint32_t> repetitions) {
  mojom::TilePtr tile = mojom::Tile::New();
  tile->input_operand_id = input_operand_id;
  tile->output_operand_id = output_operand_id;
  tile->repetitions = std::move(repetitions);
  graph_info_->operations.push_back(mojom::Operation::NewTile(std::move(tile)));
}

void GraphInfoBuilder::BuildTranspose(OperandId input_operand_id,
                                      OperandId output_operand_id,
                                      std::vector<uint32_t> permutation) {
  mojom::TransposePtr transpose = mojom::Transpose::New();
  transpose->input_operand_id = input_operand_id;
  transpose->output_operand_id = output_operand_id;
  transpose->permutation = std::move(permutation);
  graph_info_->operations.push_back(
      mojom::Operation::NewTranspose(std::move(transpose)));
}

void GraphInfoBuilder::BuildTriangular(OperandId input_operand_id,
                                       OperandId output_operand_id,
                                       bool upper,
                                       int32_t diagonal) {
  mojom::TriangularPtr triangular = mojom::Triangular::New(
      input_operand_id, output_operand_id, upper, diagonal, "");
  graph_info_->operations.push_back(
      mojom::Operation::NewTriangular(std::move(triangular)));
}

void GraphInfoBuilder::BuildWhere(OperandId condition_operand_id,
                                  OperandId true_value_operand_id,
                                  OperandId false_value_operand_id,
                                  OperandId output_operand_id) {
  mojom::WherePtr where = mojom::Where::New();
  where->condition_operand_id = condition_operand_id;
  where->true_value_operand_id = true_value_operand_id;
  where->false_value_operand_id = false_value_operand_id;
  where->output_operand_id = output_operand_id;
  graph_info_->operations.push_back(
      mojom::Operation::NewWhere(std::move(where)));
}

void GraphInfoBuilder::BuildSlice(OperandId input_operand_id,
                                  OperandId output_operand_id,
                                  base::span<const uint32_t> starts,
                                  base::span<const uint32_t> sizes,
                                  base::span<const uint32_t> strides) {
  CHECK_EQ(starts.size(), sizes.size());
  CHECK_EQ(starts.size(), strides.size());
  mojom::SlicePtr slice = mojom::Slice::New();
  slice->input_operand_id = input_operand_id;
  slice->output_operand_id = output_operand_id;
  for (size_t i = 0; i < starts.size(); ++i) {
    slice->ranges.emplace_back(starts[i], sizes[i], strides[i]);
  }
  graph_info_->operations.push_back(
      mojom::Operation::NewSlice(std::move(slice)));
}

mojom::GraphInfoPtr GraphInfoBuilder::CloneGraphInfo() const {
  return CloneGraphInfoForTesting(*graph_info_);
}

mojom::GraphInfoPtr GraphInfoBuilder::TakeGraphInfo() {
  return std::move(graph_info_);
}

[[nodiscard]] bool GraphInfoBuilder::IsValidGraphForTesting(
    const ContextProperties& context_properties) {
  base::test::TestFuture<bool> future;
  graph_builder_remote_->get()->IsValidGraphForTesting(
      context_properties, CloneGraphInfo(), future.GetCallback());
  return future.Take();
}

mojom::GraphInfoPtr CloneGraphInfoForTesting(
    const mojom::GraphInfo& graph_info) {
  mojom::GraphInfoPtr cloned_graph_info = mojom::GraphInfo::New();
  cloned_graph_info->operands.reserve(graph_info.operands.size());
  for (auto& operand_info : graph_info.operands) {
    cloned_graph_info->operands.push_back(operand_info.Clone());
  }
  cloned_graph_info->input_operands = graph_info.input_operands;
  cloned_graph_info->output_operands = graph_info.output_operands;
  cloned_graph_info->operations.reserve(graph_info.operations.size());
  for (auto& operation : graph_info.operations) {
    cloned_graph_info->operations.push_back(operation.Clone());
  }
  for (auto& [constant_id, constant_handle] :
       graph_info.constant_operand_ids_to_handles) {
    cloned_graph_info->constant_operand_ids_to_handles[constant_id] =
        constant_handle;
  }
  return cloned_graph_info;
}

ContextProperties GetContextPropertiesForTesting() {
  static constexpr SupportedRanks kMaxRank = SupportedRanks::UpTo(8);
  return WebNNContextImpl::IntersectWithBaseProperties(ContextProperties(
      InputOperandLayout::kNchw, Resample2DAxes::kAny,
      BatchNormalizationAxis::kAny,
      /*tensor_byte_length_limit=*/INT_MAX,
      {/*input=*/SupportedDataTypes::All(),
       /*constant=*/SupportedDataTypes::All(),
       /*arg_min_max_input=*/
       {SupportedDataTypes::All(), kMaxRank},
       /*arg_min_max_output=*/
       {OperandDataType::kInt32, OperandDataType::kInt64},
       /*batch_normalization_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*batch_normalization_mean=*/{SupportedDataTypes::All(), kMaxRank},
       /*cast_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*clamp_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*concat_inputs=*/{SupportedDataTypes::All(), kMaxRank},
       /*conv2d_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*conv2d_bias=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*conv_transpose2d_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*conv_transpose2d_bias=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*cumulative_sum_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*dequantize_linear_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*dequantize_linear_scale=*/{SupportedDataTypes::All(), kMaxRank},
       /*dequantize_linear_zero_point=*/{SupportedDataTypes::All(), kMaxRank},
       /*add_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*sub_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*mul_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*div_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*max_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*min_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*pow_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*equal_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*greater_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*greater_or_equal_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*lesser_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*lesser_or_equal_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*not_equal_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*logical_and_input=*/{DataTypeConstraint::kUint8, kMaxRank},
       /*logical_or_input=*/{DataTypeConstraint::kUint8, kMaxRank},
       /*logical_xor_input=*/{DataTypeConstraint::kUint8, kMaxRank},
       /*logical_not_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*logical_output=*/SupportedDataTypes::All(),
       /*abs_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*ceil_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*cos_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*erf_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*exp_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*floor_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*identity_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*log_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*neg_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*reciprocal_input=*/
       {SupportedDataTypes::All(), kMaxRank},
       /*sign_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*sin_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*sqrt_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*tan_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*elu_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*expand_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*gather_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*gather_indices=*/{SupportedDataTypes::All(), kMaxRank},
       /*gather_elements_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*gather_elements_indices=*/{SupportedDataTypes::All(), kMaxRank},
       /*gather_nd_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*gather_nd_indices=*/{SupportedDataTypes::All(), kMaxRank},
       /*gelu_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*gemm_a=*/{SupportedDataTypes::All(), kMaxRank},
       /*gemm_c=*/{SupportedDataTypes::All(), kMaxRank},
       /*gru_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*gru_bias=*/{SupportedDataTypes::All(), kMaxRank},
       /*gru_cell_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*gru_cell_bias=*/{SupportedDataTypes::All(), kMaxRank},
       /*hard_sigmoid_input=*/
       {SupportedDataTypes::All(), kMaxRank},
       /*hard_swish_input=*/
       {SupportedDataTypes::All(), kMaxRank},
       /*instance_normalization_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*instance_normalization_scale=*/{SupportedDataTypes::All(), kMaxRank},
       /*layer_normalization_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*leaky_relu_input=*/
       {SupportedDataTypes::All(), kMaxRank},
       /*linear_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*lstm_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*lstm_bias=*/{SupportedDataTypes::All(), kMaxRank},
       /*lstm_cell_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*lstm_cell_bias=*/{SupportedDataTypes::All(), kMaxRank},
       /*matmul_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*pad_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*average_pool2d_input=*/
       {SupportedDataTypes::All(), kMaxRank},
       /*l2_pool2d_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*max_pool2d_input=*/
       {SupportedDataTypes::All(), kMaxRank},
       /*prelu_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*quantize_linear_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*quantize_linear_zero_point=*/{SupportedDataTypes::All(), kMaxRank},
       /*reduce_l1_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*reduce_l2_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*reduce_log_sum_input=*/
       {SupportedDataTypes::All(), kMaxRank},
       /*reduce_log_sum_exp_input=*/
       {SupportedDataTypes::All(), kMaxRank},
       /*reduce_max_input=*/
       {SupportedDataTypes::All(), kMaxRank},
       /*reduce_mean_input=*/
       {SupportedDataTypes::All(), kMaxRank},
       /*reduce_min_input=*/
       {SupportedDataTypes::All(), kMaxRank},
       /*reduce_product_input=*/
       {SupportedDataTypes::All(), kMaxRank},
       /*reduce_sum_input=*/
       {SupportedDataTypes::All(), kMaxRank},
       /*reduce_sum_square_input=*/
       {SupportedDataTypes::All(), kMaxRank},
       /*relu_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*resample2d_input=*/
       {SupportedDataTypes::All(), kMaxRank},
       /*reshape_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*reverse_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*scatter_elements_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*scatter_elements_indices=*/{SupportedDataTypes::All(), kMaxRank},
       /*scatter_nd_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*scatter_nd_indices=*/{SupportedDataTypes::All(), kMaxRank},
       /*scatter_nd_updates=*/{SupportedDataTypes::All(), kMaxRank},
       /*sigmoid_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*slice_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*softmax_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*softplus_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*softsign_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*split_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*tanh_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*tile_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*transpose_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*triangular_input=*/
       {SupportedDataTypes::All(), kMaxRank},
       /*where_condition=*/{SupportedDataTypes::All(), kMaxRank},
       /*where_value=*/{SupportedDataTypes::All(), kMaxRank}}));
}

}  // namespace webnn
