// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_TEST_UTILS_H_
#define SERVICES_WEBNN_WEBNN_TEST_UTILS_H_

#include <string>
#include <vector>

#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"

namespace webnn {

// GraphInfoBuilder is a helper class for test cases that builds a GraphInfoPtr
// defined by mojom which describes an entire WebNN graph information. It
// provides methods to create all of the operands and operators for the
// GraphInfoPtr.
class GraphInfoBuilder final {
 public:
  GraphInfoBuilder();
  GraphInfoBuilder(const GraphInfoBuilder&) = delete;
  GraphInfoBuilder& operator=(const GraphInfoBuilder&) = delete;
  ~GraphInfoBuilder();

  uint64_t BuildIntermediateOperand(const std::vector<uint32_t>& dimensions,
                                    mojom::Operand::DataType type);

  uint64_t BuildInput(const std::string& name,
                      const std::vector<uint32_t>& dimensions,
                      mojom::Operand::DataType type);

  uint64_t BuildConstant(const std::vector<uint32_t>& dimensions,
                         mojom::Operand::DataType type,
                         base::span<const uint8_t> values);

  uint64_t BuildOutput(const std::string& name,
                       const std::vector<uint32_t>& dimensions,
                       mojom::Operand::DataType type);

  void BuildClamp(uint64_t input_operand_id,
                  uint64_t output_operand_id,
                  float min_value,
                  float max_value);

  void BuildConcat(std::vector<uint64_t> input_operand_ids,
                   uint64_t output_operand_id,
                   uint32_t axis);

  // A `Conv2dAttributes` type should have the following members:
  // struct Conv2dAttributes {
  //   std::vector<uint32_t> padding;
  //   std::vector<uint32_t> strides;
  //   std::vector<uint32_t> dilations;
  //   uint32_t groups;
  //   mojom::InputOperandLayout input_layout;
  //   absl::optional<uint64_t> bias_operand_id,
  //   absl::optional<mojom::Activation::Tag> activation;
  //   absl::optional<ClampAttributes> clamp_attributes;
  // };
  template <typename Conv2dAttributes>
  void BuildConv2d(uint64_t input_operand_id,
                   uint64_t filter_operand_id,
                   uint64_t output_operand_id,
                   const Conv2dAttributes& attributes,
                   absl::optional<uint64_t> bias_operand_id) {
    mojom::Conv2dPtr conv2d = mojom::Conv2d::New();
    conv2d->input_operand_id = input_operand_id;
    conv2d->filter_operand_id = filter_operand_id;
    conv2d->output_operand_id = output_operand_id;

    // Configure the attributes of conv2d.
    CHECK_EQ(attributes.padding.size(), 4u);
    conv2d->padding = mojom::Padding2d::New(
        /* beginning padding*/ mojom::Size2d::New(attributes.padding[0],
                                                  attributes.padding[2]),
        /* ending padding*/ mojom::Size2d::New(attributes.padding[1],
                                               attributes.padding[3]));
    CHECK_EQ(attributes.strides.size(), 2u);
    conv2d->strides =
        mojom::Size2d::New(attributes.strides[0], attributes.strides[1]);
    CHECK_EQ(attributes.dilations.size(), 2u);
    conv2d->dilations =
        mojom::Size2d::New(attributes.dilations[0], attributes.dilations[1]);
    conv2d->groups = attributes.groups;
    conv2d->input_layout = attributes.input_layout;
    conv2d->bias_operand_id = bias_operand_id;

    if (attributes.activation.has_value()) {
      switch (attributes.activation.value()) {
        case mojom::Activation::Tag::kClamp: {
          auto clamp_attributes = attributes.clamp_attributes;
          CHECK_EQ(clamp_attributes.has_value(), true);
          auto clamp = mojom::Clamp::New();
          clamp->min_value = clamp_attributes->min_value;
          clamp->max_value = clamp_attributes->max_value;
          conv2d->activation = mojom::Activation::NewClamp(std::move(clamp));
          break;
        }
        case mojom::Activation::Tag::kRelu:
          conv2d->activation = mojom::Activation::NewRelu(mojom::Relu::New());
          break;
        case mojom::Activation::Tag::kSigmoid:
          conv2d->activation =
              mojom::Activation::NewSigmoid(mojom::Sigmoid::New());
          break;
        case mojom::Activation::Tag::kSoftmax:
          conv2d->activation =
              mojom::Activation::NewSoftmax(mojom::Softmax::New());
          break;
        case mojom::Activation::Tag::kTanh:
          conv2d->activation = mojom::Activation::NewTanh(mojom::Tanh::New());
          break;
      }
    }

    graph_info_->operations.push_back(
        mojom::Operation::NewConv2d(std::move(conv2d)));
  }

  void BuildElementWiseBinary(mojom::ElementWiseBinary::Kind kind,
                              uint64_t lhs_operand,
                              uint64_t rhs_operand,
                              uint64_t output_operand);

  // A `GemmAttributes` type should have the following members:
  // struct GemmAttributes {
  //   absl::optional<uint64_t> c_operand_id,
  //   float alpha = 1.0;
  //   float beta = 1.0;
  //   bool a_transpose = false;
  //   bool b_transpose = false;
  // };
  template <typename GemmAttributes>
  void BuildGemm(uint64_t a_operand_id,
                 uint64_t b_operand_id,
                 uint64_t output_operand_id,
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

  void BuildPad(uint64_t input_operand_id,
                uint64_t output_operand_id,
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
  //   mojom::InputOperandLayout layout;
  // };
  template <typename Pool2dAttributes>
  void BuildPool2d(mojom::Pool2d::Kind kind,
                   uint64_t input_operand_id,
                   uint64_t output_operand_id,
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
        /* beginning padding*/ mojom::Size2d::New(attributes.padding[0],
                                                  attributes.padding[2]),
        /* ending padding*/ mojom::Size2d::New(attributes.padding[1],
                                               attributes.padding[3]));
    CHECK_EQ(attributes.strides.size(), 2u);
    pool2d->strides =
        mojom::Size2d::New(attributes.strides[0], attributes.strides[1]);
    CHECK_EQ(attributes.dilations.size(), 2u);
    pool2d->dilations =
        mojom::Size2d::New(attributes.dilations[0], attributes.dilations[1]);
    pool2d->layout = attributes.layout;

    graph_info_->operations.push_back(
        mojom::Operation::NewPool2d(std::move(pool2d)));
  }

  void BuildPrelu(uint64_t input_operand_id,
                  uint64_t slope_operand_id,
                  uint64_t output_operand_id);

  void BuildRelu(uint64_t input_operand_id, uint64_t output_operand_id);

  void BuildResample2d(uint64_t input_operand_id,
                       uint64_t output_operand_id,
                       mojom::Resample2d::InterpolationMode mode) {
    mojom::Resample2dPtr resample2d = mojom::Resample2d::New();
    resample2d->input_operand_id = input_operand_id;
    resample2d->output_operand_id = output_operand_id;
    resample2d->mode = mode;

    graph_info_->operations.push_back(
        mojom::Operation::NewResample2d(std::move(resample2d)));
  }

  void BuildReshape(uint64_t input_operand_id, uint64_t output_operand_id);

  void BuildSigmoid(uint64_t input_operand_id, uint64_t output_operand_id);

  void BuildSoftmax(uint64_t input_operand_id, uint64_t output_operand_id);

  void BuildSplit(uint64_t input_operand_id,
                  const std::vector<uint64_t>& output_operand_ids,
                  uint32_t axis);

  void BuildTanh(uint64_t input_operand_id, uint64_t output_operand_id);

  void BuildTranspose(uint64_t input_operand_id,
                      uint64_t output_operand_id,
                      std::vector<uint32_t> permutation);

  void BuildSlice(uint64_t input_operand_id,
                  uint64_t output_operand_id,
                  std::vector<uint32_t> starts,
                  std::vector<uint32_t> sizes);

  const mojom::GraphInfoPtr& GetGraphInfo() const { return graph_info_; }

  // Get a clone of internal graph info. This is used by
  // `WebNNContextDMLImplTest` because mojom::WebNNContext::CreateGraph()` needs
  // to take the ownership of graph info.
  //
  // Notice cloning of graph info could be expensive and should only be used in
  // tests.
  mojom::GraphInfoPtr CloneGraphInfo() const;

 private:
  uint64_t BuildOperand(
      const std::vector<uint32_t>& dimensions,
      mojom::Operand::DataType type,
      mojom::Operand::Kind kind = mojom::Operand::Kind::kOutput);

  mojom::GraphInfoPtr graph_info_;
  uint64_t operand_id_ = 0;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_TEST_UTILS_H_
