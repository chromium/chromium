// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_TEST_UTILS_H_
#define SERVICES_WEBNN_WEBNN_TEST_UTILS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/stack_allocated.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"
#include "services/webnn/webnn_graph_builder_impl.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace webnn {

struct BuildBatchNormalizationAttributes {
  std::optional<OperandId> scale_operand_id;
  std::optional<OperandId> bias_operand_id;
  uint32_t axis = 1;
  float epsilon = 1e-5;
};

struct BuildConv2dAttributes {
  std::vector<uint32_t> padding = {0, 0, 0, 0};
  std::vector<uint32_t> strides = {1, 1};
  std::vector<uint32_t> dilations = {1, 1};
  uint32_t groups = 1;
};

struct BuildGemmAttributes {
  std::optional<OperandId> c_operand_id;
  float alpha = 1.0;
  float beta = 1.0;
  bool a_transpose = false;
  bool b_transpose = false;
};

struct BuildGruAttributes {
  std::optional<OperandId> bias_operand_id;
  std::optional<OperandId> recurrent_bias_operand_id;
  std::optional<OperandId> initial_hidden_state_operand_id;
  bool reset_after = true;
  bool return_sequence = false;
  mojom::RecurrentNetworkDirection direction =
      mojom::RecurrentNetworkDirection::kForward;
  mojom::GruWeightLayout layout = mojom::GruWeightLayout::kZrn;
  std::vector<mojom::RecurrentNetworkActivation> activations = {
      mojom::RecurrentNetworkActivation::kSigmoid,
      mojom::RecurrentNetworkActivation::kTanh};
};

struct BuildGruCellAttributes {
  std::optional<OperandId> bias_operand_id;
  std::optional<OperandId> recurrent_bias_operand_id;
  bool reset_after = true;
  mojom::GruWeightLayout layout = mojom::GruWeightLayout::kZrn;
  std::vector<mojom::RecurrentNetworkActivation> activations = {
      mojom::RecurrentNetworkActivation::kSigmoid,
      mojom::RecurrentNetworkActivation::kTanh};
};

struct BuildInstanceNormalizationAttributes {
  std::optional<OperandId> scale_operand_id;
  std::optional<OperandId> bias_operand_id;
  float epsilon = 1e-5;
};

struct BuildLayerNormalizationAttributes {
  std::optional<OperandId> scale_operand_id;
  std::optional<OperandId> bias_operand_id;
  std::vector<uint32_t> axes;
  float epsilon = 1e-5;
};

struct BuildLstmAttributes {
  std::optional<OperandId> bias_operand_id;
  std::optional<OperandId> recurrent_bias_operand_id;
  std::optional<OperandId> peephole_weight_operand_id;
  std::optional<OperandId> initial_hidden_state_operand_id;
  std::optional<OperandId> initial_cell_state_operand_id;
  bool return_sequence = false;
  mojom::RecurrentNetworkDirection direction =
      mojom::RecurrentNetworkDirection::kForward;
  mojom::LstmWeightLayout layout = mojom::LstmWeightLayout::kIofg;
  std::vector<mojom::RecurrentNetworkActivation> activations = {
      mojom::RecurrentNetworkActivation::kSigmoid,
      mojom::RecurrentNetworkActivation::kTanh,
      mojom::RecurrentNetworkActivation::kTanh};
};

struct BuildLstmCellAttributes {
  std::optional<OperandId> bias_operand_id;
  std::optional<OperandId> recurrent_bias_operand_id;
  std::optional<OperandId> peephole_weight_operand_id;
  mojom::LstmWeightLayout layout = mojom::LstmWeightLayout::kIofg;
  std::vector<mojom::RecurrentNetworkActivation> activations = {
      mojom::RecurrentNetworkActivation::kSigmoid,
      mojom::RecurrentNetworkActivation::kTanh,
      mojom::RecurrentNetworkActivation::kTanh};
};

struct BuildPool2dAttributes {
  std::vector<uint32_t> window_dimensions;
  std::vector<uint32_t> padding = {0, 0, 0, 0};
  std::vector<uint32_t> strides = {1, 1};
  std::vector<uint32_t> dilations = {1, 1};
};

struct BuildResample2dAttributes {
  mojom::Resample2d::InterpolationMode mode =
      mojom::Resample2d::InterpolationMode::kNearestNeighbor;
  std::optional<std::vector<float>> scales;
  std::vector<uint32_t> axes = {2, 3};
};

// GraphInfoBuilder is a helper class for test cases that builds a GraphInfoPtr
// defined by mojom which describes an entire WebNN graph information. It
// provides methods to create all of the operands and operators for the
// GraphInfoPtr.
//
// The instances of the class may not be allocated on the heap, but as a member
// variable of a non-stack-allocated class.
class GraphInfoBuilder final {
  STACK_ALLOCATED();

 public:
  explicit GraphInfoBuilder(
      mojo::Remote<mojom::WebNNGraphBuilder>& graph_builder_remote);
  GraphInfoBuilder(const GraphInfoBuilder&) = delete;
  GraphInfoBuilder& operator=(const GraphInfoBuilder&) = delete;
  ~GraphInfoBuilder();

  OperandId BuildIntermediateOperand(const std::vector<uint32_t>& dimensions,
                                     OperandDataType type);

  OperandId BuildInput(const std::string& name,
                       const std::vector<uint32_t>& dimensions,
                       OperandDataType type);

  // Optionally provide `handle` to identify this constant operand; otherwise a
  // handle will be generated automatically.
  OperandId BuildConstant(
      const std::vector<uint32_t>& dimensions,
      OperandDataType type,
      base::span<const uint8_t> values,
      blink::WebNNPendingConstantToken handle =
          blink::WebNNPendingConstantToken(),
      const std::vector<uint32_t>& pending_permutation = {});

  void AddOutput(const std::string& name, OperandId operand_id);

  OperandId BuildOutput(const std::string& name,
                        const std::vector<uint32_t>& dimensions,
                        OperandDataType type);

  void BuildArgMinMax(mojom::ArgMinMax::Kind kind,
                      OperandId input_operand_id,
                      OperandId output_operand_id,
                      uint32_t axis,
                      bool keep_dimensions);

  void BuildBatchNormalization(
      OperandId input_operand_id,
      OperandId mean_operand_id,
      OperandId variance_operand_id,
      OperandId output_operand_id,
      const BuildBatchNormalizationAttributes& attributes);

  void BuildClamp(OperandId input_operand_id,
                  OperandId output_operand_id,
                  float min_value,
                  float max_value);

  void BuildConcat(std::vector<OperandId> input_operand_ids,
                   OperandId output_operand_id,
                   uint32_t axis);

  void BuildConv2d(mojom::Conv2d::Kind type,
                   OperandId input_operand_id,
                   OperandId filter_operand_id,
                   OperandId output_operand_id,
                   const BuildConv2dAttributes& attributes,
                   std::optional<OperandId> bias_operand_id);

  void BuildCumulativeSum(OperandId input_operand_id,
                          OperandId output_operand_id,
                          uint32_t axis,
                          std::optional<bool> exclusive,
                          std::optional<bool> reversed);

  void BuildDequantizeLinear(OperandId input_operand_id,
                             OperandId scale_operand_id,
                             OperandId zero_point_operand_id,
                             OperandId output_operand_id);

  void BuildElementWiseBinary(mojom::ElementWiseBinary::Kind kind,
                              OperandId lhs_operand,
                              OperandId rhs_operand,
                              OperandId output_operand);

  void BuildElementWiseUnary(mojom::ElementWiseUnary::Kind kind,
                             OperandId input_operand,
                             OperandId output_operand);

  void BuildElu(OperandId input_operand_id,
                OperandId output_operand_id,
                float alpha);

  void BuildExpand(OperandId input_operand_id, OperandId output_operand_id);

  void BuildGather(OperandId input_operand_id,
                   OperandId indices_operand_id,
                   OperandId output_operand_id,
                   uint32_t axis);

  void BuildGatherElements(OperandId input_operand_id,
                           OperandId indices_operand_id,
                           OperandId output_operand_id,
                           uint32_t axis);

  void BuildGatherND(OperandId input_operand_id,
                     OperandId indices_operand_id,
                     OperandId output_operand_id);

  void BuildGelu(OperandId input_operand_id, OperandId output_operand_id);

  void BuildGemm(OperandId a_operand_id,
                 OperandId b_operand_id,
                 OperandId output_operand_id,
                 const BuildGemmAttributes& attributes);

  void BuildGru(OperandId input_operand_id,
                OperandId weight_operand_id,
                OperandId recurrent_weight_operand_id,
                std::vector<OperandId> output_operand_ids,
                uint32_t steps,
                uint32_t hidden_size,
                const BuildGruAttributes& attributes);

  void BuildGruCell(OperandId input_operand_id,
                    OperandId weight_operand_id,
                    OperandId recurrent_weight_operand_id,
                    OperandId hidden_state_operand_id,
                    OperandId output_operand_id,
                    uint32_t hidden_size,
                    const BuildGruCellAttributes& attributes);

  void BuildHardSigmoid(OperandId input_operand_id,
                        OperandId output_operand_id,
                        std::optional<float> alpha,
                        std::optional<float> beta);

  void BuildHardSwish(OperandId input_operand_id, OperandId output_operand_id);

  void BuildLayerNormalization(
      OperandId input_operand_id,
      OperandId output_operand_id,
      const BuildLayerNormalizationAttributes& attributes);

  void BuildLstm(OperandId input_operand_id,
                 OperandId weight_operand_id,
                 OperandId recurrent_weight_operand_id,
                 std::vector<OperandId> output_operand_ids,
                 uint32_t steps,
                 uint32_t hidden_size,
                 const BuildLstmAttributes& attributes);

  void BuildLstmCell(OperandId input_operand_id,
                     OperandId weight_operand_id,
                     OperandId recurrent_weight_operand_id,
                     OperandId hidden_state_operand_id,
                     OperandId cell_state_operand_id,
                     std::vector<OperandId> output_operand_ids,
                     uint32_t hidden_size,
                     const BuildLstmCellAttributes& attributes);

  void BuildInstanceNormalization(
      OperandId input_operand_id,
      OperandId output_operand_id,
      const BuildInstanceNormalizationAttributes& attributes);

  void BuildLeakyRelu(OperandId input_operand_id,
                      OperandId output_operand_id,
                      float alpha);

  void BuildLinear(OperandId input_operand_id,
                   OperandId output_operand_id,
                   float alpha,
                   float beta);

  void BuildMatmul(OperandId a_operand_id,
                   OperandId b_operand_id,
                   OperandId output_operand_id);

  void BuildPad(OperandId input_operand_id,
                OperandId output_operand_id,
                const std::vector<uint32_t>& beginning_padding,
                const std::vector<uint32_t>& ending_padding,
                mojom::PaddingMode::Tag mode,
                float value);

  void BuildPool2d(mojom::Pool2d::Kind kind,
                   OperandId input_operand_id,
                   OperandId output_operand_id,
                   const BuildPool2dAttributes& attributes);

  void BuildPrelu(OperandId input_operand_id,
                  OperandId slope_operand_id,
                  OperandId output_operand_id);

  void BuildQuantizeLinear(OperandId input_operand_id,
                           OperandId scale_operand_id,
                           OperandId zero_point_operand_id,
                           OperandId output_operand_id);

  void BuildReduce(mojom::Reduce::Kind kind,
                   OperandId input_operand_id,
                   OperandId output_operand_id,
                   std::vector<uint32_t> axes,
                   bool keep_dimensions);

  void BuildRelu(OperandId input_operand_id, OperandId output_operand_id);

  void BuildResample2d(OperandId input_operand_id,
                       OperandId output_operand_id,
                       const BuildResample2dAttributes& attributes);

  void BuildReshape(OperandId input_operand_id, OperandId output_operand_id);

  void BuildReverse(OperandId input_operand_id,
                    OperandId output_operand_id,
                    std::vector<uint32_t> axes);

  void BuildScatterElements(OperandId input_operand_id,
                            OperandId indices_operand_id,
                            OperandId updates_operand_id,
                            OperandId output_operand_id,
                            uint32_t axis);

  void BuildScatterND(OperandId input_operand_id,
                      OperandId indices_operand_id,
                      OperandId updates_operand_id,
                      OperandId output_operand_id);

  void BuildSigmoid(OperandId input_operand_id, OperandId output_operand_id);

  void BuildSoftmax(OperandId input_operand_id,
                    OperandId output_operand_id,
                    uint32_t axis);

  void BuildSoftplus(OperandId input_operand_id, OperandId output_operand_id);

  void BuildSoftsign(OperandId input_operand_id, OperandId output_operand_id);

  void BuildSplit(OperandId input_operand_id,
                  const std::vector<OperandId>& output_operand_ids,
                  uint32_t axis);

  void BuildTanh(OperandId input_operand_id, OperandId output_operand_id);

  void BuildTile(OperandId input_operand_id,
                 OperandId output_operand_id,
                 std::vector<uint32_t> repetitions);

  void BuildTranspose(OperandId input_operand_id,
                      OperandId output_operand_id,
                      std::vector<uint32_t> permutation);

  void BuildTriangular(OperandId input_operand_id,
                       OperandId output_operand_id,
                       bool upper,
                       int32_t diagonal);

  void BuildWhere(OperandId condition_operand_id,
                  OperandId true_value_operand_id,
                  OperandId false_value_operand_id,
                  OperandId output_operand_id);

  void BuildSlice(OperandId input_operand_id,
                  OperandId output_operand_id,
                  base::span<const uint32_t> starts,
                  base::span<const uint32_t> sizes,
                  base::span<const uint32_t> strides);

  const mojom::GraphInfo& GetGraphInfo() const { return *graph_info_; }

  // Prefer `TakeGraphInfo()` when possible. Cloning can be expensive and should
  // only be used in tests.
  mojom::GraphInfoPtr CloneGraphInfo() const;

  mojom::GraphInfoPtr TakeGraphInfo();

  [[nodiscard]] bool IsValidGraphForTesting(
      const ContextProperties& context_properties);

 private:
  OperandId BuildOperand(
      const std::vector<uint32_t>& dimensions,
      OperandDataType type,
      mojom::Operand::Kind kind = mojom::Operand::Kind::kOutput);

  mojom::GraphInfoPtr graph_info_;

  // Local copies of the constants sent to the remote graph builder. Consumed by
  // `IsValidGraphForTesting()`, which may therefore be called at most once.
  WebNNGraphBuilderImpl::PendingConstantOperandSet pending_constant_operands_;
  bool validated_ = false;

  base::raw_ref<mojo::Remote<mojom::WebNNGraphBuilder>> graph_builder_remote_;
};

mojom::GraphInfoPtr CloneGraphInfoForTesting(
    const mojom::GraphInfo& graph_info);

// A default set of WebNNContext properties for testing purposes.
ContextProperties GetContextPropertiesForTesting();

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_TEST_UTILS_H_
