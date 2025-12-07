// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_TEST_UTILS_H_
#define SERVICES_WEBNN_WEBNN_TEST_UTILS_H_

#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/stack_allocated.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace webnn {

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
      mojo::AssociatedRemote<mojom::WebNNGraphBuilder>& graph_builder_remote);
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
  OperandId BuildConstant(const std::vector<uint32_t>& dimensions,
                          OperandDataType type,
                          base::span<const uint8_t> values,
                          blink::WebNNPendingConstantToken handle =
                              blink::WebNNPendingConstantToken());

  void AddOutput(const std::string& name, OperandId operand_id);

  OperandId BuildOutput(const std::string& name,
                        const std::vector<uint32_t>& dimensions,
                        OperandDataType type);

  void BuildArgMinMax(mojom::ArgMinMax::Kind kind,
                      OperandId input_operand_id,
                      OperandId output_operand_id,
                      uint32_t axis,
                      bool keep_dimensions);

  // A `BatchNormalizationAttributes` type should have the following members:
  // struct BatchNormalizationAttributes {
  //  std::optional<OperandId> scale_operand_id;
  //  std::optional<OperandId> bias_operand_id;
  //  uint32_t axis = 1;
  //  float epsilon = 1e-5;
  // };
  template <typename BatchNormalizationAttributes>
  void BuildBatchNormalization(OperandId input_operand_id,
                               OperandId mean_operand_id,
                               OperandId variance_operand_id,
                               OperandId output_operand_id,
                               const BatchNormalizationAttributes& attributes) {
    mojom::BatchNormalizationPtr batch_normalization =
        mojom::BatchNormalization::New();
    batch_normalization->input_operand_id = input_operand_id;
    batch_normalization->mean_operand_id = mean_operand_id;
    batch_normalization->variance_operand_id = variance_operand_id;
    batch_normalization->output_operand_id = output_operand_id;

    batch_normalization->scale_operand_id = attributes.scale_operand_id;
    batch_normalization->bias_operand_id = attributes.bias_operand_id;
    batch_normalization->axis = attributes.axis;
    batch_normalization->epsilon = attributes.epsilon;

    graph_info_->operations.push_back(mojom::Operation::NewBatchNormalization(
        std::move(batch_normalization)));
  }

  void BuildClamp(OperandId input_operand_id,
                  OperandId output_operand_id,
                  float min_value,
                  float max_value);

  void BuildConcat(std::vector<OperandId> input_operand_ids,
                   OperandId output_operand_id,
                   uint32_t axis);

  // A `Conv2dAttributes` type should have the following members:
  // struct Conv2dAttributes {
  //   std::vector<uint32_t> padding;
  //   std::vector<uint32_t> strides;
  //   std::vector<uint32_t> dilations;
  //   uint32_t groups;
  //   std::optional<OperandId> bias_operand_id,
  // };
  template <typename Conv2dAttributes>
  void BuildConv2d(mojom::Conv2d::Kind type,
                   OperandId input_operand_id,
                   OperandId filter_operand_id,
                   OperandId output_operand_id,
                   const Conv2dAttributes& attributes,
                   std::optional<OperandId> bias_operand_id) {
    mojom::Conv2dPtr conv2d = mojom::Conv2d::New();
    conv2d->input_operand_id = input_operand_id;
    conv2d->filter_operand_id = filter_operand_id;
    conv2d->output_operand_id = output_operand_id;

    // Configure the attributes of conv2d.
    conv2d->kind = type;
    CHECK_EQ(attributes.padding.size(), 4u);
    conv2d->padding = mojom::Padding2d::New(
        /*beginning padding*/ mojom::Size2d::New(attributes.padding[0],
                                                 attributes.padding[2]),
        /*ending padding*/ mojom::Size2d::New(attributes.padding[1],
                                              attributes.padding[3]));
    CHECK_EQ(attributes.strides.size(), 2u);
    conv2d->strides =
        mojom::Size2d::New(attributes.strides[0], attributes.strides[1]);
    CHECK_EQ(attributes.dilations.size(), 2u);
    conv2d->dilations =
        mojom::Size2d::New(attributes.dilations[0], attributes.dilations[1]);
    conv2d->groups = attributes.groups;
    conv2d->bias_operand_id = bias_operand_id;

    graph_info_->operations.push_back(
        mojom::Operation::NewConv2d(std::move(conv2d)));
  }

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

  void BuildElu(OperandId input_operand_id,
                OperandId output_operand_id,
                float alpha);

  void BuildElementWiseUnary(mojom::ElementWiseUnary::Kind kind,
                             OperandId input_operand,
                             OperandId output_operand);

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

  // A `GemmAttributes` type should have the following members:
  // struct GemmAttributes {
  //   std::optional<OperandId> c_operand_id,
  //   float alpha = 1.0;
  //   float beta = 1.0;
  //   bool a_transpose = false;
  //   bool b_transpose = false;
  // };
  template <typename GemmAttributes>
  void BuildGemm(OperandId a_operand_id,
                 OperandId b_operand_id,
                 OperandId output_operand_id,
                 const GemmAttributes& attributes) {
    mojom::GemmPtr gemm = mojom::Gemm::New();
    gemm->a_operand_id = a_operand_id;
    gemm->b_operand_id = b_operand_id;
    gemm->output_operand_id = output_operand_id;

    gemm->c_operand_id = attributes.c_operand_id;
    gemm->alpha = attributes.alpha;
    gemm->beta = attributes.beta;
    gemm->a_transpose = attributes.a_transpose;
    gemm->b_transpose = attributes.b_transpose;

    graph_info_->operations.push_back(
        mojom::Operation::NewGemm(std::move(gemm)));
  }

  // A `GruAttributes` type should have the following members:
  // struct GruAttributes {
  //   std::optional<OperandId> bias_operand_id;
  //   std::optional<OperandId> recurrent_bias_operand_id;
  //   std::optional<OperandId> initial_hidden_state_operand_id;
  //   bool reset_after;
  //   bool return_sequence;
  //   mojom::RecurrentNetworkDirection direction;
  //   mojom::GruWeightLayout layout;
  //   std::vector<mojom::RecurrentNetworkActivation> activations;
  // };
  template <typename GruAttributes>
  void BuildGru(OperandId input_operand_id,
                OperandId weight_operand_id,
                OperandId recurrent_weight_operand_id,
                std::vector<OperandId> output_operand_ids,
                uint32_t steps,
                uint32_t hidden_size,
                const GruAttributes& attributes) {
    mojom::GruPtr gru = mojom::Gru::New();
    gru->input_operand_id = input_operand_id;
    gru->weight_operand_id = weight_operand_id;
    gru->recurrent_weight_operand_id = recurrent_weight_operand_id;
    gru->output_operand_ids = std::move(output_operand_ids);
    gru->steps = steps;
    gru->hidden_size = hidden_size;

    gru->bias_operand_id = attributes.bias_operand_id;
    gru->recurrent_bias_operand_id = attributes.recurrent_bias_operand_id;
    gru->initial_hidden_state_operand_id =
        attributes.initial_hidden_state_operand_id;
    gru->reset_after = attributes.reset_after;
    gru->return_sequence = attributes.return_sequence;
    gru->direction = attributes.direction;
    gru->layout = attributes.layout;
    gru->activations = attributes.activations;

    graph_info_->operations.push_back(mojom::Operation::NewGru(std::move(gru)));
  }

  // A `GruCellAttributes` type should have the following members:
  // struct GruCellAttributes {
  //   std::optional<OperandId> bias_operand_id;
  //   std::optional<OperandId> recurrent_bias_operand_id;
  //   bool reset_after;
  //   mojom::GruWeightLayout layout;
  //   std::vector<mojom::RecurrentNetworkActivation> activations;
  // };
  template <typename GruCellAttributes>
  void BuildGruCell(OperandId input_operand_id,
                    OperandId weight_operand_id,
                    OperandId recurrent_weight_operand_id,
                    OperandId hidden_state_operand_id,
                    OperandId output_operand_id,
                    uint32_t hidden_size,
                    const GruCellAttributes& attributes) {
    mojom::GruCellPtr gru_cell = mojom::GruCell::New(
        input_operand_id, weight_operand_id, recurrent_weight_operand_id,
        hidden_state_operand_id, hidden_size, output_operand_id,
        attributes.bias_operand_id, attributes.recurrent_bias_operand_id,
        attributes.reset_after, attributes.layout, attributes.activations, "");

    graph_info_->operations.push_back(
        mojom::Operation::NewGruCell(std::move(gru_cell)));
  }

  void BuildHardSigmoid(OperandId input_operand_id,
                        OperandId output_operand_id,
                        std::optional<float> alpha,
                        std::optional<float> beta);

  void BuildHardSwish(OperandId input_operand_id, OperandId output_operand_id);

  // A `LayerNormalizationAttributes` type should have the following members:
  // struct LayerNormalizationAttributes {
  //  std::optional<OperandId> scale_operand_id;
  //  std::optional<OperandId> bias_operand_id;
  //  std::vector<uint32_t> axes;
  //  float epsilon = 1e-5;
  // };
  template <typename LayerNormalizationAttributes>
  void BuildLayerNormalization(OperandId input_operand_id,
                               OperandId output_operand_id,
                               const LayerNormalizationAttributes& attributes) {
    mojom::LayerNormalizationPtr layer_normalization =
        mojom::LayerNormalization::New();
    layer_normalization->input_operand_id = input_operand_id;
    layer_normalization->output_operand_id = output_operand_id;

    layer_normalization->scale_operand_id = attributes.scale_operand_id;
    layer_normalization->bias_operand_id = attributes.bias_operand_id;
    layer_normalization->axes = attributes.axes;
    layer_normalization->epsilon = attributes.epsilon;

    graph_info_->operations.push_back(mojom::Operation::NewLayerNormalization(
        std::move(layer_normalization)));
  }

  // A `LstmAttributes` type should have the following members:
  // struct LstmAttributes {
  //   std::optional<OperandId> bias_operand_id;
  //   std::optional<OperandId> recurrent_bias_operand_id;
  //   std::optional<OperandId> peephole_weight_operand_id;
  //   std::optional<OperandId> initial_hidden_state_operand_id;
  //   std::optional<OperandId> initial_cell_state_operand_id;
  //   bool return_sequence;
  //   mojom::RecurrentNetworkDirection direction;
  //   mojom::LstmWeightLayout layout;
  //   std::vector<mojom::RecurrentNetworkActivation> activations;
  // };
  template <typename LstmAttributes>
  void BuildLstm(OperandId input_operand_id,
                 OperandId weight_operand_id,
                 OperandId recurrent_weight_operand_id,
                 std::vector<OperandId> output_operand_ids,
                 uint32_t steps,
                 uint32_t hidden_size,
                 const LstmAttributes& attributes) {
    mojom::LstmPtr lstm = mojom::Lstm::New();
    lstm->input_operand_id = input_operand_id;
    lstm->weight_operand_id = weight_operand_id;
    lstm->recurrent_weight_operand_id = recurrent_weight_operand_id;
    lstm->output_operand_ids = std::move(output_operand_ids);
    lstm->steps = steps;
    lstm->hidden_size = hidden_size;

    lstm->bias_operand_id = attributes.bias_operand_id;
    lstm->recurrent_bias_operand_id = attributes.recurrent_bias_operand_id;
    lstm->peephole_weight_operand_id = attributes.peephole_weight_operand_id;
    lstm->initial_hidden_state_operand_id =
        attributes.initial_hidden_state_operand_id;
    lstm->initial_cell_state_operand_id =
        attributes.initial_cell_state_operand_id;
    lstm->return_sequence = attributes.return_sequence;
    lstm->direction = attributes.direction;
    lstm->layout = attributes.layout;
    lstm->activations = attributes.activations;

    graph_info_->operations.push_back(
        mojom::Operation::NewLstm(std::move(lstm)));
  }

  // A `LstmCellAttributes` type should have the following members:
  // struct LstmCellAttributes {
  //   std::optional<OperandId> bias_operand_id;
  //   std::optional<OperandId> recurrent_bias_operand_id;
  //   std::optional<OperandId> peephole_weight_operand_id;
  //   mojom::LstmWeightLayout layout;
  //   std::vector<mojom::RecurrentNetworkActivation> activations;
  // };
  template <typename LstmCellAttributes>
  void BuildLstmCell(OperandId input_operand_id,
                     OperandId weight_operand_id,
                     OperandId recurrent_weight_operand_id,
                     OperandId hidden_state_operand_id,
                     OperandId cell_state_operand_id,
                     std::vector<OperandId> output_operand_ids,
                     uint32_t hidden_size,
                     const LstmCellAttributes& attributes) {
    auto lstm_cell = mojom::LstmCell::New(
        input_operand_id, weight_operand_id, recurrent_weight_operand_id,
        hidden_state_operand_id, cell_state_operand_id,
        std::move(output_operand_ids), hidden_size, attributes.bias_operand_id,
        attributes.recurrent_bias_operand_id,
        attributes.peephole_weight_operand_id, attributes.layout,
        attributes.activations, "");

    graph_info_->operations.push_back(
        mojom::Operation::NewLstmCell(std::move(lstm_cell)));
  }

  // A `InstanceNormalizationAttributes` type should have the following members:
  // struct InstanceNormalizationAttributes {
  //  std::optional<OperandId> scale_operand_id;
  //  std::optional<OperandId> bias_operand_id;
  //  float epsilon = 1e-5;
  // };
  template <typename InstanceNormalizationAttributes>
  void BuildInstanceNormalization(
      OperandId input_operand_id,
      OperandId output_operand_id,
      const InstanceNormalizationAttributes& attributes) {
    mojom::InstanceNormalizationPtr instance_normalization =
        mojom::InstanceNormalization::New();
    instance_normalization->input_operand_id = input_operand_id;
    instance_normalization->output_operand_id = output_operand_id;

    instance_normalization->scale_operand_id = attributes.scale_operand_id;
    instance_normalization->bias_operand_id = attributes.bias_operand_id;
    instance_normalization->epsilon = attributes.epsilon;

    graph_info_->operations.push_back(
        mojom::Operation::NewInstanceNormalization(
            std::move(instance_normalization)));
  }

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

  // A `Pool2dAttributes` type should have the following members:
  // struct Pool2dAttributes {
  //   std::vector<uint32_t> window_dimensions;
  //   std::vector<uint32_t> padding;
  //   std::vector<uint32_t> strides;
  //   std::vector<uint32_t> dilations;
  // };
  template <typename Pool2dAttributes>
  void BuildPool2d(mojom::Pool2d::Kind kind,
                   OperandId input_operand_id,
                   OperandId output_operand_id,
                   const Pool2dAttributes& attributes) {
    mojom::Pool2dPtr pool2d = mojom::Pool2d::New();
    pool2d->kind = kind;
    pool2d->input_operand_id = input_operand_id;
    pool2d->output_operand_id = output_operand_id;

    auto& window_dimensions = attributes.window_dimensions;
    CHECK_EQ(window_dimensions.size(), 2u);
    pool2d->window_dimensions =
        mojom::Size2d::New(window_dimensions[0], window_dimensions[1]);
    CHECK_EQ(attributes.padding.size(), 4u);
    pool2d->padding = mojom::Padding2d::New(
        /*beginning padding*/ mojom::Size2d::New(attributes.padding[0],
                                                 attributes.padding[2]),
        /*ending padding*/ mojom::Size2d::New(attributes.padding[1],
                                              attributes.padding[3]));
    CHECK_EQ(attributes.strides.size(), 2u);
    pool2d->strides =
        mojom::Size2d::New(attributes.strides[0], attributes.strides[1]);
    CHECK_EQ(attributes.dilations.size(), 2u);
    pool2d->dilations =
        mojom::Size2d::New(attributes.dilations[0], attributes.dilations[1]);

    graph_info_->operations.push_back(
        mojom::Operation::NewPool2d(std::move(pool2d)));
  }

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

  // A `Resample2dAttributes` type should have the following members:
  // struct Resample2dAttributes {
  //   mojom::Resample2d::InterpolationMode mode =
  //       mojom::Resample2d::InterpolationMode::kNearestNeighbor;
  //   std::optional<std::vector<float>> scales;
  //   std::vector<uint32_t> axes = {2, 3};};
  template <typename Resample2dAttributes>
  void BuildResample2d(OperandId input_operand_id,
                       OperandId output_operand_id,
                       const Resample2dAttributes& attributes) {
    mojom::Resample2dPtr resample2d = mojom::Resample2d::New();
    resample2d->input_operand_id = input_operand_id;
    resample2d->output_operand_id = output_operand_id;
    resample2d->mode = attributes.mode;
    if (attributes.scales) {
      resample2d->scales = attributes.scales;
    }
    resample2d->axes = attributes.axes;

    graph_info_->operations.push_back(
        mojom::Operation::NewResample2d(std::move(resample2d)));
  }

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

  base::raw_ref<mojo::AssociatedRemote<mojom::WebNNGraphBuilder>>
      graph_builder_remote_;
};

mojom::GraphInfoPtr CloneGraphInfoForTesting(
    const mojom::GraphInfo& graph_info);

// A default set of WebNNContext properties for testing purposes.
ContextProperties GetContextPropertiesForTesting();

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_TEST_UTILS_H_
