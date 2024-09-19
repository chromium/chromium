// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_GRAPH_VALIDATION_UTILS_H_
#define SERVICES_WEBNN_PUBLIC_CPP_GRAPH_VALIDATION_UTILS_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/enum_set.h"
#include "base/containers/span.h"
#include "base/types/expected.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace webnn {

std::string COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    DataTypeConstraintToString(const SupportedDataTypes& constraint_set);

// Represents the `MLConv2dFilterOperandLayout` that specifies the layout format
// of the filter tensor. O is output channels, I is input channels / groups, H
// is height and W is the width of filter.
enum class Conv2dFilterOperandLayout { kOihw, kHwio, kOhwi, kIhwo };

// Represents the `MLConvTranspose2dFilterOperandLayout` that specifies the
// layout format of the filter tensor. I is input channels, O is output channels
// / groups, H is height and W is the width of filter.
enum class ConvTranspose2dFilterOperandLayout { kIohw, kHwoi, kOhwi };

enum class Pool2dKind { kAverage, kL2, kMax };

// Represents the `MLRoundingType` that is used to compute the output shape.
enum class RoundingType { kFloor, kCeil };

// Represents the `MLRecurrentNetworkDirection` that specifies the processing
// direction of the input sequence.
enum class RecurrentNetworkDirection { kForward, kBackward, kBoth };

enum class ReduceKind {
  kL1,
  kL2,
  kLogSum,
  kLogSumExp,
  kMax,
  kMean,
  kMin,
  kProduct,
  kSum,
  kSumSquare
};

// A size has height and width values.
template <typename T>
struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) Size2d {
  T height;
  T width;
};

// The additional rows and columns added to the beginning and ending of each
// spatial dimension of input.
struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) Padding2d {
  // The height and width padding at the beginning of input tensor.
  Size2d<uint32_t> beginning;
  // The height and width padding at the ending of input tensor.
  Size2d<uint32_t> ending;
};

// Contains the attributes of batchNormalization operator.
struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) BatchNormalizationAttributes {
  BatchNormalizationAttributes();
  ~BatchNormalizationAttributes();

  BatchNormalizationAttributes(BatchNormalizationAttributes&& other);
  BatchNormalizationAttributes& operator=(BatchNormalizationAttributes&& other);

  BatchNormalizationAttributes(const BatchNormalizationAttributes&) = delete;
  BatchNormalizationAttributes& operator=(const BatchNormalizationAttributes&) =
      delete;

  // The 1-D tensor of the scaling values.
  std::optional<OperandDescriptor> scale;
  // The 1-D tensor of the bias values.
  std::optional<OperandDescriptor> bias;
  // The number which specifies the index to the feature count dimension of the
  // input shape for which the mean and variance values are.
  uint32_t axis = 1;
  // The operator label defined by the user.
  std::string label = "";
};

// Contains the attributes of conv2d operator.
struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) Conv2dAttributesBase {
  Conv2dAttributesBase();
  ~Conv2dAttributesBase();

  Conv2dAttributesBase(Conv2dAttributesBase&& other);
  Conv2dAttributesBase& operator=(Conv2dAttributesBase&& other);

  Conv2dAttributesBase(const Conv2dAttributesBase&) = delete;
  Conv2dAttributesBase& operator=(const Conv2dAttributesBase&) = delete;

  // The additional rows and columns added to the beginning and ending of each
  // spatial dimension of input.
  Padding2d padding;
  // The stride of the sliding window for each spatial dimension of input.
  Size2d<uint32_t> strides;
  // The dilation factor for each spatial dimension of input.
  Size2d<uint32_t> dilations;
  // The number of groups that input channels and output channels are divided
  // into.
  uint32_t groups = 1;
  // The layout format of the input.
  InputOperandLayout input_layout = InputOperandLayout::kNchw;
  // The additional 1-D tensor with the shape of [output_channels] whose values
  // are to be added to the convolution result.
  std::optional<OperandDescriptor> bias_operand;
  // The operator label defined by the user.
  std::string label = "";
};

// Contains the attributes of conv2d operator.
struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) Conv2dAttributes
    : Conv2dAttributesBase {
  Conv2dAttributes();
  ~Conv2dAttributes();

  Conv2dAttributes(Conv2dAttributes&& other);
  Conv2dAttributes& operator=(Conv2dAttributes&& other);

  Conv2dAttributes(const Conv2dAttributes&) = delete;
  Conv2dAttributes& operator=(const Conv2dAttributes&) = delete;

  // The layout format of the conv2d filter.
  Conv2dFilterOperandLayout filter_layout = Conv2dFilterOperandLayout::kOihw;
};

// Contains the attributes of convTranspose2d operator.
struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) ConvTranspose2dAttributes
    : Conv2dAttributesBase {
  ConvTranspose2dAttributes();
  ~ConvTranspose2dAttributes();

  ConvTranspose2dAttributes(ConvTranspose2dAttributes&& other);
  ConvTranspose2dAttributes& operator=(ConvTranspose2dAttributes&& other);

  ConvTranspose2dAttributes(const ConvTranspose2dAttributes&) = delete;
  ConvTranspose2dAttributes& operator=(const ConvTranspose2dAttributes&) =
      delete;

  // The padding values applied to each spatial dimension of the output tensor.
  Size2d<uint32_t> output_padding;
  // The sizes of the last two dimensions of the output tensor.
  std::optional<Size2d<uint32_t>> output_sizes;
  // The layout format of the convTranspose2d filter.
  ConvTranspose2dFilterOperandLayout filter_layout =
      ConvTranspose2dFilterOperandLayout::kIohw;
};

// Contains the attributes of pool2d operator.
struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) Pool2dAttributes {
  Pool2dAttributes();
  ~Pool2dAttributes();

  Pool2dAttributes(Pool2dAttributes&& other);
  Pool2dAttributes& operator=(Pool2dAttributes&& other);

  Pool2dAttributes(const Pool2dAttributes&) = delete;
  Pool2dAttributes& operator=(const Pool2dAttributes&) = delete;

  // The dimensions of the sliding window.
  std::optional<Size2d<uint32_t>> window_dimensions;
  // The additional rows and columns added to the beginning and ending of each
  // spatial dimension of input.
  Padding2d padding;
  // The element stride of the sliding window for each spatial dimension of
  // input.
  Size2d<uint32_t> strides;
  // The dilation factor for each spatial dimension of input.
  Size2d<uint32_t> dilations;
  // The layout format of the input.
  InputOperandLayout layout = InputOperandLayout::kNchw;
  // The rounding function used to compute the output shape.
  RoundingType rounding_type = RoundingType::kFloor;
  // The element height and width of the output tensor.
  std::optional<Size2d<uint32_t>> output_sizes;
  // The operator label defined by the user.
  std::string label = "";
};

// Contains the attributes of gemm operator.
struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) GemmAttributes {
  GemmAttributes();
  ~GemmAttributes();

  GemmAttributes(GemmAttributes&& other);
  GemmAttributes& operator=(GemmAttributes&& other);

  GemmAttributes(const GemmAttributes&) = delete;
  GemmAttributes& operator=(const GemmAttributes&) = delete;

  // The optional third tensor in expression alpha * A * B + beta * C.
  std::optional<OperandDescriptor> c_operand;
  // A float scalar multiplier for the `A * B`.
  float alpha = 1.0;
  // A float scalar multiplier for the third tensor.
  float beta = 1.0;
  // True is to transpose the first tensor matrix multiplication.
  bool a_transpose = false;
  // True is to transpose the second tensor matrix multiplication.
  bool b_transpose = false;
  // The operator label defined by the user.
  std::string label = "";
};

// Contains the attributes of gru operator.
struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) GruAttributes {
  GruAttributes();
  ~GruAttributes();

  GruAttributes(GruAttributes&& other);
  GruAttributes& operator=(GruAttributes&& other);

  GruAttributes(const GruAttributes&) = delete;
  GruAttributes& operator=(const GruAttributes&) = delete;

  // The bias operand.
  std::optional<OperandDescriptor> bias;
  // The recurrent bias operand.
  std::optional<OperandDescriptor> recurrent_bias;
  // The initial hidden state operand.
  std::optional<OperandDescriptor> initial_hidden_state;
  // Indicates whether to return the outputs of the entire sequence.
  bool return_sequence;
  // Specifies the processing direction of the input sequence.
  RecurrentNetworkDirection direction;
  // The number of activations.
  uint32_t activation_count;
  // The operator label defined by the user.
  std::string label = "";
};

// Contains the attributes of gruCell operator.
struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) GruCellAttributes {
  GruCellAttributes();
  ~GruCellAttributes();

  GruCellAttributes(GruCellAttributes&& other);
  GruCellAttributes& operator=(GruCellAttributes&& other);

  GruCellAttributes(const GruCellAttributes&) = delete;
  GruCellAttributes& operator=(const GruCellAttributes&) = delete;

  // The bias operand.
  std::optional<OperandDescriptor> bias;
  // The recurrent bias operand.
  std::optional<OperandDescriptor> recurrent_bias;
  // The number of activations.
  uint32_t activation_count;
  // The operator label defined by the user.
  std::string label = "";
};

// Contains the attributes of instanceNormalization operator.
struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) InstanceNormalizationAttributes {
  InstanceNormalizationAttributes();
  ~InstanceNormalizationAttributes();

  InstanceNormalizationAttributes(InstanceNormalizationAttributes&& other);
  InstanceNormalizationAttributes& operator=(
      InstanceNormalizationAttributes&& other);

  InstanceNormalizationAttributes(const InstanceNormalizationAttributes&) =
      delete;
  InstanceNormalizationAttributes& operator=(
      const InstanceNormalizationAttributes&) = delete;

  // The scale operand.
  std::optional<OperandDescriptor> scale;
  // The bias operand.
  std::optional<OperandDescriptor> bias;
  // The layout format of the input.
  InputOperandLayout layout = InputOperandLayout::kNchw;
  // The operator label defined by the user.
  std::string label = "";
};

// Contains the attributes of layerNormalization operator.
struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) LayerNormalizationAttributes {
  LayerNormalizationAttributes();
  ~LayerNormalizationAttributes();

  LayerNormalizationAttributes(LayerNormalizationAttributes&& other);
  LayerNormalizationAttributes& operator=(LayerNormalizationAttributes&& other);

  LayerNormalizationAttributes(const LayerNormalizationAttributes&) = delete;
  LayerNormalizationAttributes& operator=(const LayerNormalizationAttributes&) =
      delete;

  // The scale operand.
  std::optional<OperandDescriptor> scale;
  // The bias operand.
  std::optional<OperandDescriptor> bias;
  // The operator label defined by the user.
  std::string label = "";
};

struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) LstmAttributes {
  LstmAttributes();
  ~LstmAttributes();

  LstmAttributes(LstmAttributes&& other);
  LstmAttributes& operator=(LstmAttributes&& other);

  LstmAttributes(const LstmAttributes&) = delete;
  LstmAttributes& operator=(const LstmAttributes&) = delete;

  // The bias operand.
  std::optional<OperandDescriptor> bias;
  // The recurrent bias operand.
  std::optional<OperandDescriptor> recurrent_bias;
  // The peephole weight operand.
  std::optional<OperandDescriptor> peephole_weight;
  // The initial hidden state operand.
  std::optional<OperandDescriptor> initial_hidden_state;
  // The initial cell state operand.
  std::optional<OperandDescriptor> initial_cell_state;
  // The number of activations.
  size_t activation_count;
  // Indicates whether to return the outputs of the entire sequence.
  bool return_sequence;
  // The processing direction of the input sequence.
  RecurrentNetworkDirection direction;
  // The operator label defined by the user.
  std::string label = "";
};

struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) LstmCellAttributes {
  LstmCellAttributes();
  ~LstmCellAttributes();

  LstmCellAttributes(LstmCellAttributes&& other);
  LstmCellAttributes& operator=(LstmCellAttributes&& other);

  LstmCellAttributes(const LstmCellAttributes&) = delete;
  LstmCellAttributes& operator=(const LstmCellAttributes&) = delete;

  // The bias operand.
  std::optional<OperandDescriptor> bias;
  // The recurrent bias operand.
  std::optional<OperandDescriptor> recurrent_bias;
  // The peephole weight operand.
  std::optional<OperandDescriptor> peephole_weight;
  // The number of activations.
  size_t activation_count;
  // The operator label defined by the user.
  std::string label = "";
};

struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) SliceAttributes {
  SliceAttributes();
  ~SliceAttributes();

  SliceAttributes(SliceAttributes&& other);
  SliceAttributes& operator=(SliceAttributes&& other);

  SliceAttributes(const SliceAttributes&) = delete;
  SliceAttributes& operator=(const SliceAttributes&) = delete;

  // The sequence of unsigned integer values indicating the starting index to
  // slice of each input dimension.
  std::vector<uint32_t> starts;
  // The sequence of unsigned integer values indicating the number of elements
  // to slice of each input dimension.
  std::vector<uint32_t> sizes;
  // The operator label defined by the user.
  std::string label = "";
};

// Validate argMin and argMax operators defined in WebIDL here:
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-argminmax.
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateArgMinMaxAndInferOutput(const ContextProperties& context_properties,
                                    const OperandDescriptor& input,
                                    std::string_view label,
                                    uint32_t axis,
                                    OperandDataType output_data_type,
                                    bool keep_dimensions = false);

// Validate softmax operator defined in WebIDL here:
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-softmax.
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateSoftmaxAndInferOutput(const ContextProperties& context_properties,
                                  const OperandDescriptor& input,
                                  uint32_t axis,
                                  std::string_view label);

// Contains the attributes of the split operator.
struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) SplitAttribute {
  // splits defines how the input tensor will be split.
  //  uint32_t: The input tensor will be split into splits number of outputs
  //   with equal sizes.
  //  base::span<const uint32_t>: The input tensor will be split into
  //   splits.size() number of outputs with sizes specified in splits.
  absl::variant<uint32_t, base::span<const uint32_t>> splits;
  // Axis specifies which input tensor dimension will be split.
  uint32_t axis = 0;
  // The operator label defined by the user.
  std::string label = "";
};

// Validate and infer the output tensors' ranks and sizes for split operator
// based on the WebNN WebIDL
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-split
base::expected<std::vector<OperandDescriptor>, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateSplitAndInferOutput(const ContextProperties& context_properties,
                                const OperandDescriptor& input,
                                const SplitAttribute& attributes);

// Validate and infer output information of batchNormalization operator defined
// in WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-batchnorm.
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateBatchNormalizationAndInferOutput(
        const ContextProperties& context_properties,
        const OperandDescriptor& input,
        const OperandDescriptor& mean,
        const OperandDescriptor& variance,
        const BatchNormalizationAttributes& attributes);

// Validate and infer output information of 2-D convolution operator defined in
// WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-conv2d
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateConv2dAndInferOutput(const ContextProperties& context_properties,
                                 const OperandDescriptor& input,
                                 const OperandDescriptor& filter,
                                 const Conv2dAttributes& attributes);

// Validate and infer output information of cumulativeSum operator defined in
// WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-cumulativesum
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateCumulativeSumAndInferOutput(
        const ContextProperties& context_properties,
        const OperandDescriptor& input,
        const uint32_t axis,
        std::string_view label);

// Validate and infer output information of 2-D transposed convolution operator
// defined in WebIDL here
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-convtranspose2d
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateConvTranspose2dAndInferOutput(
        const ContextProperties& context_properties,
        const OperandDescriptor& input,
        const OperandDescriptor& filter,
        const ConvTranspose2dAttributes& attributes);

base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateDequantizeLinearAndInferOutput(
        const ContextProperties& context_properties,
        const OperandDescriptor& input,
        const OperandDescriptor& scale,
        const OperandDescriptor& zero_point,
        std::string_view label);

// Validate and infer output information of pad operator defined in
// WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-pad
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidatePadAndInferOutput(const ContextProperties& context_properties,
                              const OperandDescriptor& input,
                              base::span<const uint32_t> beginning_padding,
                              base::span<const uint32_t> ending_padding,
                              std::string_view label);

// Validate and infer output information of matmul operator defined in
// WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-matmul
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateMatmulAndInferOutput(const ContextProperties& context_properties,
                                 const OperandDescriptor& a,
                                 const OperandDescriptor& b,
                                 std::string_view label);

// Validate and infer output information of 2-D pooling operator defined in
// WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-pool2d
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidatePool2dAndInferOutput(const ContextProperties& context_properties,
                                 const OperandDescriptor& input,
                                 const Pool2dAttributes& attributes,
                                 Pool2dKind kind);

// Validate and infer output information of 2-D resample operator defined in
// WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-resample2d
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateResample2dAndInferOutput(
        const ContextProperties& context_properties,
        const OperandDescriptor& input,
        const absl::variant<base::span<const float>,
                            base::span<const uint32_t>>& scales_or_sizes,
        base::span<const uint32_t> axes,
        std::string_view label);

// Validate and infer output information of gather operator defined in
// WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-gather
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateGatherAndInferOutput(const ContextProperties& context_properties,
                                 const OperandDescriptor& input,
                                 const OperandDescriptor& indices,
                                 const uint32_t axis,
                                 std::string_view label);

// Validate and infer output information of gatherElements operator defined in
// WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-gatherElements
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateGatherElementsAndInferOutput(
        const ContextProperties& context_properties,
        const OperandDescriptor& input,
        const OperandDescriptor& indices,
        const uint32_t axis,
        std::string_view label);

// Validate and infer output information of gatherND operator defined in
// WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-gatherND
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateGatherNDAndInferOutput(const ContextProperties& context_properties,
                                   const OperandDescriptor& input,
                                   const OperandDescriptor& indices,
                                   std::string_view label);

// Validate gemm operator defined in WebIDL here
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-gemm
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateGemmAndInferOutput(const ContextProperties& context_properties,
                               const OperandDescriptor& a,
                               const OperandDescriptor& b,
                               const GemmAttributes& attributes);

// Validate and infer output information of gru operator defined in WebIDL here
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-gru.
base::expected<std::vector<OperandDescriptor>, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateGruAndInferOutput(const ContextProperties& context_properties,
                              const OperandDescriptor& input,
                              const OperandDescriptor& weight,
                              const OperandDescriptor& recurrent_weight,
                              uint32_t steps,
                              uint32_t hidden_size,
                              const GruAttributes& attributes);

// Validate and infer output information of gruCell operator defined in WebIDL
// here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-grucell.
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateGruCellAndInferOutput(const ContextProperties& context_properties,
                                  const OperandDescriptor& input,
                                  const OperandDescriptor& weight,
                                  const OperandDescriptor& recurrent_weight,
                                  const OperandDescriptor& hidden_state,
                                  uint32_t hidden_size,
                                  const GruCellAttributes& attributes);

// Validate and infer output information of instanceNormalization operator
// defined in WebIDL here
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-instancenorm.
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateInstanceNormalizationAndInferOutput(
        const ContextProperties& context_properties,
        const OperandDescriptor& input,
        const InstanceNormalizationAttributes& attributes);

// Validate and infer output information of layerNormalization operator defined
// in WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-layernorm.
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateLayerNormalizationAndInferOutput(
        const ContextProperties& context_properties,
        const OperandDescriptor& input,
        base::span<const uint32_t> axes,
        const LayerNormalizationAttributes& attributes);

// Validate and infer output information of lstm operator defined
// in WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-lstm.
base::expected<std::vector<OperandDescriptor>, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateLstmAndInferOutput(const ContextProperties& context_properties,
                               const OperandDescriptor& input,
                               const OperandDescriptor& weight,
                               const OperandDescriptor& recurrent_weight,
                               const uint32_t steps,
                               const uint32_t hidden_size,
                               const LstmAttributes& attributes);

// Validate and infer output information of lstmCell operator defined
// in WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-lstmcell.
base::expected<std::vector<OperandDescriptor>, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateLstmCellAndInferOutput(const ContextProperties& context_properties,
                                   const OperandDescriptor& input,
                                   const OperandDescriptor& weight,
                                   const OperandDescriptor& recurrent_weight,
                                   const OperandDescriptor& hidden_state,
                                   const OperandDescriptor& cell_state,
                                   const uint32_t hidden_size,
                                   const LstmCellAttributes& attributes);

// Validate concat operator defined in WebIDL here
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-concat
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateConcatAndInferOutput(const ContextProperties& context_properties,
                                 const std::vector<OperandDescriptor>& input,
                                 const uint32_t axis,
                                 std::string_view label);

// Validate prelu operator defined in WebIDL here:
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-prelu
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidatePreluAndInferOutput(const ContextProperties& context_properties,
                                const OperandDescriptor& input,
                                const OperandDescriptor& slope,
                                std::string_view label);

base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateQuantizeLinearAndInferOutput(
        const ContextProperties& context_properties,
        const OperandDescriptor& input,
        const OperandDescriptor& scale,
        const OperandDescriptor& zero_point,
        std::string_view label);

// Validate tile operator defined in WebIDL here
// https://github.com/webmachinelearning/webnn/issues/375
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateTileAndInferOutput(const ContextProperties& context_properties,
                               const OperandDescriptor& input,
                               base::span<const uint32_t> repetitions,
                               std::string_view label);

// Validate transpose operator defined in WebIDL here
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-transpose
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateTransposeAndInferOutput(const ContextProperties& context_properties,
                                    const OperandDescriptor& input,
                                    base::span<const uint32_t> permutation,
                                    std::string_view label);

// Validate slice operator defined in WebIDL here:
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-slice
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateSliceAndInferOutput(const ContextProperties& context_properties,
                                const OperandDescriptor& input,
                                const SliceAttributes& attributes);

// Validate and infer output information of reduce operator defined in
// WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-reduce
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateReduceAndInferOutput(const ContextProperties& context_properties,
                                 ReduceKind kind,
                                 const OperandDescriptor& input,
                                 std::string_view label,
                                 base::span<const uint32_t> axes,
                                 bool keepDimensions = false);

// Validate and infer output information of scatterND operator defined in
// WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-scatternd
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateScatterNDAndInferOutput(const ContextProperties& context_properties,
                                    const OperandDescriptor& input,
                                    const OperandDescriptor& indices,
                                    const OperandDescriptor& updates,
                                    std::string_view label);

// Validate triangular operator defined in WebIDL here:
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-triangular.
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateTriangularAndInferOutput(
        const ContextProperties& context_properties,
        const OperandDescriptor& input,
        std::string_view label);

// Validate where operator defined in WebIDL here:
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-where.
base::expected<OperandDescriptor, std::string> COMPONENT_EXPORT(
    WEBNN_PUBLIC_CPP)
    ValidateWhereAndInferOutput(const ContextProperties& context_properties,
                                const OperandDescriptor& condition,
                                const OperandDescriptor& true_value,
                                const OperandDescriptor& false_value,
                                std::string_view label);

// Validate the creation of an MLTensor given `descriptor`.
base::expected<void, std::string> COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    ValidateTensor(const ContextProperties& context_properties,
                   OperandDescriptor descriptor);

// Validate that the axes are within the range of [0, rank - 1] without
// duplication.
base::expected<void, std::string> COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    ValidateAxes(base::span<const uint32_t> axes,
                 uint32_t rank,
                 std::string_view label);

// Broadcast the input shapes and return the output shape.
// If bidirectional is true, its behavior follows the numpy-broadcasting-rule:
// https://numpy.org/doc/stable/user/basics.broadcasting.html#general-broadcasting-rules.
// Otherwise, it unidirectionally broadcasts the lhs to the rhs.
std::optional<std::vector<uint32_t>> COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    BroadcastShapes(base::span<const uint32_t> dims_lhs,
                    base::span<const uint32_t> dims_rhs,
                    bool bidirectional = true);

// Calculate the output size for convTranspose2d based on WebNN spec:
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-convtranspose2d
// Return the calculated output size if no error.
base::expected<uint32_t, std::string> COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    CalculateConvTranspose2dOutputSize(const uint32_t input_size,
                                       const uint32_t filter_size,
                                       const uint32_t beginning_padding,
                                       const uint32_t ending_padding,
                                       const uint32_t stride,
                                       const uint32_t dilation,
                                       const uint32_t output_padding);

bool COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    IsFloatingPointType(OperandDataType data_type);

// A depthwise conv2d operation is a variant of grouped convolution where the
// options.groups == input_channels == output_channels according to WebNN conv2d
// spec: https://www.w3.org/TR/webnn/#api-mlgraphbuilder-conv2d.
bool COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    IsDepthwiseConv2d(uint32_t input_channels,
                      uint32_t output_channels,
                      uint32_t groups);

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_GRAPH_VALIDATION_UTILS_H_
