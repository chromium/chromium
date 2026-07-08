// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/debug/asan_service.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/allow_check_is_test_for_testing.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_future.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
#include "services/webnn/public/mojom/webnn_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_test_environment.h"
#include "services/webnn/webnn_test_utils.h"
#include "services/webnn/webnn_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fp16/src/include/fp16.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "third_party/fuzztest/src/fuzztest/googletest_fixture_adapter.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

namespace webnn::test {

namespace {

#define ASSIGN_OR_RETURN_VOID(lhs, rexpr) \
  ASSIGN_OR_RETURN(lhs, rexpr, [](std::string error) { return; });

#define ASSIGN_OR_RETURN_FALSE(lhs, rexpr) \
  ASSIGN_OR_RETURN(lhs, rexpr, [](std::string error) { return false; });

#define ASSIGN_OR_RETURN_NULLOPT(lhs, rexpr) \
  ASSIGN_OR_RETURN(lhs, rexpr, [](std::string error) { return std::nullopt; });

#define ASSIGN_OPTIONAL_OR_RETURN_VOID(lhs, rexpr) \
  ASSIGN_OR_RETURN(lhs, rexpr, [] { return; });

// Registers a fuzz test for all three device types (CPU, GPU, NPU).
// The variadic args carry the .WithDomains()/.WithSeeds() chain.
#define WEBNN_FUZZ_TEST_F(func, ...)  \
  FUZZ_TEST_F(CPU, func) __VA_ARGS__; \
  FUZZ_TEST_F(GPU, func) __VA_ARGS__; \
  FUZZ_TEST_F(NPU, func) __VA_ARGS__

template <typename T>
std::vector<uint8_t> CreateBufferAs(size_t buffer_size, int64_t fill_value) {
  std::vector<uint8_t> buffer(buffer_size, 0);
  // SAFETY: The span is only used for filling values, and the size is
  // divided by the size of the element type.
  std::ranges::fill(
      UNSAFE_BUFFERS(base::span(reinterpret_cast<T*>(buffer.data()),
                                buffer.size() / sizeof(T))),
      static_cast<T>(fill_value));
  return buffer;
}

std::vector<uint8_t> CreateBufferAsIndicesType(
    size_t buffer_size,
    OperandDataType indices_data_type,
    int64_t fill_value) {
  switch (indices_data_type) {
    case OperandDataType::kInt32:
      return CreateBufferAs<int32_t>(buffer_size, fill_value);
    case OperandDataType::kUint32:
      return CreateBufferAs<uint32_t>(buffer_size, fill_value);
    case OperandDataType::kInt64:
      return CreateBufferAs<int64_t>(buffer_size, fill_value);
    default:
      NOTREACHED();
  }
}

// Represents an activation that takes no extra options and whose output has the
// same shape and data type as its input. These are all tested by the shared
// `Activation` fuzzer.
//
// Activations that have extra options are not listed here; they are tested by
// their own dedicated fuzzers.
enum class ActivationKind : uint8_t {
  kGelu = 0,
  kHardSwish = 1,
  kRelu = 2,
  kSigmoid = 3,
  kSoftplus = 4,
  kSoftsign = 5,
  kTanh = 6,
};

enum class Conv2dActivationKind : uint8_t {
  kNone = 0,
  kRelu = 1,
  kRelu6 = 2,
  kReluN1To1 = 3,
  // clamp(0, +inf), which also maps to RELU but exercises the clamp
  // code path instead of BuildRelu.
  kReluViaClamp = 4,
};

enum class GemmCShapeKind : uint8_t {
  kScalar = 0,
  k1D = 1,
  k2D_1xN = 2,
  k2D_MxN = 3,
};

// Tri-state for optional operands: not present, constant, or input.
enum class OptionalOperandKind : uint8_t {
  kNone = 0,
  kConstant = 1,
  kInput = 2,
};

enum class QuantizationKind : uint32_t {
  kPerTensor = 0,
  kPerChannel = 1,
  kPerBlock = 2,
};

struct ActivationParams {
  OperandDataType data_type;
  uint32_t rank;
  std::array<uint32_t, 8> input_dims;
  ActivationKind kind;
  bool is_input_constant;
};

struct BatchNormalizationParams {
  OperandDataType data_type;
  uint32_t rank;
  std::array<uint32_t, 8> input_dims;
  uint32_t axis;
  float epsilon;
  OptionalOperandKind scale_kind;
  OptionalOperandKind bias_kind;
  bool is_input_constant;
  bool is_mean_constant;
  bool is_variance_constant;
};

struct ClampParams {
  OperandDataType data_type;
  uint32_t rank;
  std::array<uint32_t, 8> input_dims;
  float min_value;
  float max_value;
  bool is_input_constant;
};

struct ConcatParams {
  OperandDataType data_type;
  uint32_t rank;
  std::array<uint32_t, 8> input_dims;
  uint32_t axis;
  // Dimension size along the concat axis for each additional input beyond the
  // first.
  std::vector<uint32_t> extra_axis_dims;
  bool is_input_constant;
};

struct Conv2dParams {
  OperandDataType data_type;
  mojom::Conv2d::Kind conv2d_kind;
  uint32_t batch;
  uint32_t input_channels;
  uint32_t input_height;
  uint32_t input_width;
  uint32_t output_channels;
  Padding2d padding;
  Size2d<uint32_t> filter_dimensions;
  Size2d<uint32_t> strides;
  Size2d<uint32_t> dilations;
  Size2d<uint32_t> output_padding;
  uint32_t groups;
  bool is_input_constant;
  bool is_filter_constant;
  OptionalOperandKind bias_kind;
  bool is_depthwise;
  Conv2dActivationKind activation_kind;
};

struct DequantizeLinearParams {
  OperandDataType input_data_type;
  OperandDataType scale_data_type;
  uint32_t rank;
  std::array<uint32_t, 8> input_dims;
  QuantizationKind quantization_kind;
  uint32_t channel_axis;
  uint32_t channel_block_size;
  bool is_input_constant;
  bool is_scale_constant;
  bool is_zero_point_constant;
};

struct ElementWiseBinaryParams {
  OperandDataType data_type;
  mojom::ElementWiseBinary::Kind kind;
  uint32_t lhs_rank;
  uint32_t rhs_rank;
  std::array<uint32_t, 8> lhs_dims;
  std::array<uint32_t, 8> rhs_dims;
  bool is_lhs_constant;
  bool is_rhs_constant;
};

struct EluParams {
  OperandDataType data_type;
  uint32_t rank;
  std::array<uint32_t, 8> input_dims;
  float alpha;
  bool is_input_constant;
};

struct ExpandParams {
  OperandDataType data_type;
  uint32_t input_rank;
  uint32_t output_rank;
  std::array<uint32_t, 8> input_dims;
  std::array<uint32_t, 8> output_dims;
  bool is_input_constant;
};

struct GatherParams {
  OperandDataType input_data_type;
  OperandDataType indices_data_type;
  uint32_t input_rank;
  std::array<uint32_t, 8> input_dims;
  uint32_t indices_rank;
  std::array<uint32_t, 8> indices_dims;
  uint32_t axis;
  int64_t indices_fill_value;
  bool is_input_constant;
  bool is_indices_constant;
};

struct GatherNDParams {
  OperandDataType input_data_type;
  OperandDataType indices_data_type;
  uint32_t input_rank;
  std::array<uint32_t, 8> input_dims;
  uint32_t indices_rank;
  std::array<uint32_t, 8> indices_dims;
  int64_t indices_fill_value;
  bool is_input_constant;
  bool is_indices_constant;
};

struct GemmParams {
  OperandDataType data_type;
  uint32_t m;
  uint32_t k;
  uint32_t n;
  float alpha;
  float beta;
  bool a_transpose;
  bool b_transpose;
  bool has_c;
  GemmCShapeKind c_shape_kind;
  bool is_a_constant;
  bool is_b_constant;
  bool is_c_constant;
};

struct HardSigmoidParams {
  OperandDataType data_type;
  uint32_t rank;
  std::array<uint32_t, 8> input_dims;
  float alpha;
  float beta;
  bool is_input_constant;
};

struct InstanceNormalizationParams {
  OperandDataType data_type;
  uint32_t batch;
  uint32_t channels;
  uint32_t input_height;
  uint32_t input_width;
  float epsilon;
  OptionalOperandKind scale_kind;
  OptionalOperandKind bias_kind;
  bool is_input_constant;
};

struct LayerNormalizationParams {
  OperandDataType data_type;
  uint32_t rank;
  std::array<uint32_t, 8> input_dims;
  uint32_t num_axes;
  std::array<uint32_t, 8> axes;
  float epsilon;
  OptionalOperandKind scale_kind;
  OptionalOperandKind bias_kind;
  bool is_input_constant;
};

struct LeakyReluParams {
  OperandDataType data_type;
  uint32_t rank;
  std::array<uint32_t, 8> input_dims;
  float alpha;
  bool is_input_constant;
};

struct LinearParams {
  OperandDataType data_type;
  uint32_t rank;
  std::array<uint32_t, 8> input_dims;
  float alpha;
  float beta;
  bool is_input_constant;
};

struct LstmParams {
  OperandDataType data_type;
  uint32_t steps;
  uint32_t batch_size;
  uint32_t input_size;
  uint32_t hidden_size;
  mojom::RecurrentNetworkDirection direction;
  mojom::LstmWeightLayout layout;
  OptionalOperandKind bias_kind;
  OptionalOperandKind recurrent_bias_kind;
  OptionalOperandKind peephole_weight_kind;
  OptionalOperandKind initial_hidden_state_kind;
  OptionalOperandKind initial_cell_state_kind;
  bool return_sequence;
  bool is_input_constant;
  bool is_weight_constant;
  bool is_recurrent_weight_constant;
  std::array<mojom::RecurrentNetworkActivation, 3> activations;
};

struct LstmCellParams {
  OperandDataType data_type;
  uint32_t batch_size;
  uint32_t input_size;
  uint32_t hidden_size;
  mojom::LstmWeightLayout layout;
  OptionalOperandKind bias_kind;
  OptionalOperandKind recurrent_bias_kind;
  OptionalOperandKind peephole_weight_kind;
  bool is_input_constant;
  bool is_weight_constant;
  bool is_recurrent_weight_constant;
  bool is_hidden_state_constant;
  bool is_cell_state_constant;
  std::array<mojom::RecurrentNetworkActivation, 3> activations;
};

struct MatmulParams {
  OperandDataType data_type;
  uint32_t a_rank;
  uint32_t b_rank;
  std::array<uint32_t, 8> a_dims;
  std::array<uint32_t, 8> b_dims;
  bool is_a_constant;
  bool is_b_constant;
};

struct PadParams {
  OperandDataType data_type;
  uint32_t rank;
  std::array<uint32_t, 8> input_dims;
  // Padding values for each dimension. Only first `rank` entries are used.
  std::array<uint32_t, 8> beginning_padding;
  std::array<uint32_t, 8> ending_padding;
  mojom::PaddingMode::Tag mode;
  float value;
  bool is_input_constant;
};

struct Pool2dParams {
  OperandDataType data_type;
  mojom::Pool2d::Kind pool2d_kind;
  RoundingType output_shape_rounding;
  uint32_t batch;
  uint32_t channels;
  uint32_t input_height;
  uint32_t input_width;
  Padding2d padding;
  Size2d<uint32_t> window_dimensions;
  Size2d<uint32_t> strides;
  Size2d<uint32_t> dilations;
  bool is_input_constant;
};

struct PreluParams {
  OperandDataType data_type;
  uint32_t rank;
  std::array<uint32_t, 8> input_dims;
  uint32_t slope_rank;
  std::array<uint32_t, 8> slope_dims;
  bool is_input_constant;
  bool is_slope_constant;
};

struct QuantizationParams {
  OperandDataType quantized_type;
  QuantizationKind quantization_kind;
  uint32_t channel_block_size;
};

struct QuantizeLinearParams {
  OperandDataType input_data_type;
  OperandDataType zero_point_data_type;
  uint32_t rank;
  std::array<uint32_t, 8> input_dims;
  QuantizationKind quantization_kind;
  uint32_t channel_axis;
  uint32_t channel_block_size;
  bool is_input_constant;
  bool is_scale_constant;
  bool is_zero_point_constant;
};

struct ReduceParams {
  OperandDataType data_type;
  mojom::Reduce::Kind reduce_kind;
  uint32_t rank;
  std::array<uint32_t, 8> input_dims;
  // Number of axes to reduce. Must be in [0, rank].
  uint32_t num_axes;
  // Which axes to reduce. Only the first `num_axes` entries are used.
  std::array<uint32_t, 8> axes;
  bool keep_dimensions;
  bool is_input_constant;
};

struct Resample2dParams {
  OperandDataType data_type;
  mojom::Resample2d::InterpolationMode mode;
  uint32_t batch;
  uint32_t channels;
  uint32_t input_height;
  uint32_t input_width;
  // When true, use explicit output sizes; when false, use scales to determine
  // output size.
  bool use_sizes;
  float scale_height;
  float scale_width;
  uint32_t output_height;
  uint32_t output_width;
  bool is_input_constant;
};

struct ScatterElementsParams {
  OperandDataType input_data_type;
  OperandDataType indices_data_type;
  uint32_t rank;
  uint32_t axis;
  std::array<uint32_t, 8> input_dims;
  // Dimension size of the indices tensor along `axis`.
  uint32_t indices_axis_dim_size;
  int64_t indices_fill_value;
  bool is_input_constant;
  bool is_indices_constant;
  bool is_updates_constant;
};

struct SliceParams {
  OperandDataType data_type;
  uint32_t rank;
  std::array<uint32_t, 8> input_dims;
  std::array<uint32_t, 8> starts;
  std::array<uint32_t, 8> sizes;
  std::array<uint32_t, 8> strides;
  bool is_input_constant;
};

struct SoftmaxParams {
  OperandDataType data_type;
  uint32_t rank;
  std::array<uint32_t, 8> input_dims;
  uint32_t axis;
  bool is_input_constant;
};

struct SplitParams {
  OperandDataType data_type;
  uint32_t rank;
  std::array<uint32_t, 8> input_dims;
  uint32_t axis;
  bool use_equal_splits;
  // Only used when `use_equal_splits` is true to split into `num_splits` equal
  // parts.
  uint32_t num_splits;
  // Only used when `use_equal_splits` is false. The size of this vector
  // determines the number of splits and the values are used as weights to
  // proportionally distribute the axis dimension among the splits.
  std::vector<uint32_t> split_sizes;
  bool is_input_constant;
};

struct TransposeParams {
  OperandDataType data_type;
  uint32_t rank;
  std::array<uint32_t, 8> input_dims;
  std::array<uint32_t, 8> permutation;
  bool is_input_constant;
};

struct TriangularParams {
  OperandDataType data_type;
  uint32_t rank;
  std::array<uint32_t, 8> input_dims;
  bool upper;
  int32_t diagonal;
  bool is_input_constant;
};

SupportedDataTypes GetActivationDataTypes(ActivationKind kind) {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  switch (kind) {
    case ActivationKind::kGelu:
      return limits.gelu_input.data_types;
    case ActivationKind::kHardSwish:
      return limits.hard_swish_input.data_types;
    case ActivationKind::kRelu:
      return limits.relu_input.data_types;
    case ActivationKind::kSigmoid:
      return limits.sigmoid_input.data_types;
    case ActivationKind::kSoftplus:
      return limits.softplus_input.data_types;
    case ActivationKind::kSoftsign:
      return limits.softsign_input.data_types;
    case ActivationKind::kTanh:
      return limits.tanh_input.data_types;
  }
}

SupportedDataTypes GetElementWiseBinaryDataTypes(
    mojom::ElementWiseBinary::Kind kind) {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  switch (kind) {
    case mojom::ElementWiseBinary::Kind::kAdd:
      return limits.add_input.data_types;
    case mojom::ElementWiseBinary::Kind::kSub:
      return limits.sub_input.data_types;
    case mojom::ElementWiseBinary::Kind::kMul:
      return limits.mul_input.data_types;
    case mojom::ElementWiseBinary::Kind::kDiv:
      return limits.div_input.data_types;
    case mojom::ElementWiseBinary::Kind::kMax:
      return limits.max_input.data_types;
    case mojom::ElementWiseBinary::Kind::kMin:
      return limits.min_input.data_types;
    case mojom::ElementWiseBinary::Kind::kPow:
      return limits.pow_input.data_types;
    case mojom::ElementWiseBinary::Kind::kEqual:
    case mojom::ElementWiseBinary::Kind::kGreater:
    case mojom::ElementWiseBinary::Kind::kGreaterOrEqual:
    case mojom::ElementWiseBinary::Kind::kLesser:
    case mojom::ElementWiseBinary::Kind::kLesserOrEqual:
    case mojom::ElementWiseBinary::Kind::kNotEqual:
    case mojom::ElementWiseBinary::Kind::kLogicalAnd:
    case mojom::ElementWiseBinary::Kind::kLogicalOr:
    case mojom::ElementWiseBinary::Kind::kLogicalXor:
      NOTREACHED();
  }
}

SupportedDataTypes GetPool2dDataTypes(mojom::Pool2d::Kind pool2d_kind) {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  switch (pool2d_kind) {
    case mojom::Pool2d::Kind::kAveragePool2d:
      return limits.average_pool2d_input.data_types;
    case mojom::Pool2d::Kind::kL2Pool2d:
      return limits.l2_pool2d_input.data_types;
    case mojom::Pool2d::Kind::kMaxPool2d:
      return limits.max_pool2d_input.data_types;
  }
}

SupportedDataTypes GetReduceDataTypes(mojom::Reduce::Kind reduce_kind) {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  switch (reduce_kind) {
    case mojom::Reduce::Kind::kL1:
      return limits.reduce_l1_input.data_types;
    case mojom::Reduce::Kind::kL2:
      return limits.reduce_l2_input.data_types;
    case mojom::Reduce::Kind::kLogSum:
      return limits.reduce_log_sum_input.data_types;
    case mojom::Reduce::Kind::kLogSumExp:
      return limits.reduce_log_sum_exp_input.data_types;
    case mojom::Reduce::Kind::kMax:
      return limits.reduce_max_input.data_types;
    case mojom::Reduce::Kind::kMean:
      return limits.reduce_mean_input.data_types;
    case mojom::Reduce::Kind::kMin:
      return limits.reduce_min_input.data_types;
    case mojom::Reduce::Kind::kProduct:
      return limits.reduce_product_input.data_types;
    case mojom::Reduce::Kind::kSum:
      return limits.reduce_sum_input.data_types;
    case mojom::Reduce::Kind::kSumSquare:
      return limits.reduce_sum_square_input.data_types;
  }
}

void BuildActivation(GraphInfoBuilder& builder,
                     ActivationKind kind,
                     OperandId input_id,
                     OperandId output_id) {
  switch (kind) {
    case ActivationKind::kGelu:
      builder.BuildGelu(input_id, output_id);
      return;
    case ActivationKind::kHardSwish:
      builder.BuildHardSwish(input_id, output_id);
      return;
    case ActivationKind::kRelu:
      builder.BuildRelu(input_id, output_id);
      return;
    case ActivationKind::kSigmoid:
      builder.BuildSigmoid(input_id, output_id);
      return;
    case ActivationKind::kSoftplus:
      builder.BuildSoftplus(input_id, output_id);
      return;
    case ActivationKind::kSoftsign:
      builder.BuildSoftsign(input_id, output_id);
      return;
    case ActivationKind::kTanh:
      builder.BuildTanh(input_id, output_id);
      return;
  }
}

auto AnyConv2dKind() {
  return fuzztest::ElementOf<mojom::Conv2d::Kind>(
      {mojom::Conv2d::Kind::kDirect, mojom::Conv2d::Kind::kTransposed});
}

// All activations that take no extra options. Exercised by the `Activation`
// fuzzer.
constexpr auto kAllActivationKinds = std::to_array<ActivationKind>({
    ActivationKind::kGelu,
    ActivationKind::kHardSwish,
    ActivationKind::kRelu,
    ActivationKind::kSigmoid,
    ActivationKind::kSoftplus,
    ActivationKind::kSoftsign,
    ActivationKind::kTanh,
});

// The subset of activations that have a fusible quantized path. Only sigmoid
// and tanh satisfy the condition.
constexpr auto kAllActivationQuantizedKinds = std::to_array<ActivationKind>({
    ActivationKind::kSigmoid,
    ActivationKind::kTanh,
});

constexpr auto kAllElementWiseBinaryKinds =
    std::to_array<mojom::ElementWiseBinary::Kind>({
        mojom::ElementWiseBinary::Kind::kAdd,
        mojom::ElementWiseBinary::Kind::kSub,
        mojom::ElementWiseBinary::Kind::kMul,
        mojom::ElementWiseBinary::Kind::kDiv,
        mojom::ElementWiseBinary::Kind::kMax,
        mojom::ElementWiseBinary::Kind::kMin,
        mojom::ElementWiseBinary::Kind::kPow,
    });

// kDiv and kPow are excluded because they are not supported for quantized
// operands.
constexpr auto kAllElementWiseBinaryQuantizedKinds =
    std::to_array<mojom::ElementWiseBinary::Kind>({
        mojom::ElementWiseBinary::Kind::kAdd,
        mojom::ElementWiseBinary::Kind::kSub,
        mojom::ElementWiseBinary::Kind::kMul,
        mojom::ElementWiseBinary::Kind::kMax,
        mojom::ElementWiseBinary::Kind::kMin,
    });

constexpr auto kAllPool2dKinds = std::to_array<mojom::Pool2d::Kind>({
    mojom::Pool2d::Kind::kMaxPool2d,
    mojom::Pool2d::Kind::kAveragePool2d,
    mojom::Pool2d::Kind::kL2Pool2d,
});

auto AnyPool2dKind() {
  return fuzztest::ElementOf<mojom::Pool2d::Kind>(kAllPool2dKinds);
}

auto AnyQuantizationKind() {
  return fuzztest::ElementOf<QuantizationKind>({QuantizationKind::kPerTensor,
                                                QuantizationKind::kPerChannel,
                                                QuantizationKind::kPerBlock});
}

constexpr auto kAllReduceKinds = std::to_array<mojom::Reduce::Kind>({
    mojom::Reduce::Kind::kL1,
    mojom::Reduce::Kind::kL2,
    mojom::Reduce::Kind::kLogSum,
    mojom::Reduce::Kind::kLogSumExp,
    mojom::Reduce::Kind::kMax,
    mojom::Reduce::Kind::kMean,
    mojom::Reduce::Kind::kMin,
    mojom::Reduce::Kind::kProduct,
    mojom::Reduce::Kind::kSum,
    mojom::Reduce::Kind::kSumSquare,
});

auto AnyReduceKind() {
  return fuzztest::ElementOf<mojom::Reduce::Kind>(kAllReduceKinds);
}

auto AnyRoundingType() {
  return fuzztest::ElementOf<RoundingType>(
      {RoundingType::kFloor, RoundingType::kCeil});
}

auto AnyOptionalOperandKind() {
  return fuzztest::ElementOf<OptionalOperandKind>(
      {OptionalOperandKind::kNone, OptionalOperandKind::kConstant,
       OptionalOperandKind::kInput});
}

auto AnyPaddingModeTag() {
  return fuzztest::ElementOf<mojom::PaddingMode::Tag>(
      {mojom::PaddingMode::Tag::kConstant, mojom::PaddingMode::Tag::kEdge,
       mojom::PaddingMode::Tag::kReflection});
}

auto AnyRecurrentNetworkActivation() {
  return fuzztest::ElementOf<mojom::RecurrentNetworkActivation>(
      {mojom::RecurrentNetworkActivation::kRelu,
       mojom::RecurrentNetworkActivation::kSigmoid,
       mojom::RecurrentNetworkActivation::kTanh});
}

auto AnyLstmDirection() {
  return fuzztest::ElementOf<mojom::RecurrentNetworkDirection>(
      {mojom::RecurrentNetworkDirection::kForward,
       mojom::RecurrentNetworkDirection::kBackward,
       mojom::RecurrentNetworkDirection::kBoth});
}

auto AnyLstmWeightLayout() {
  return fuzztest::ElementOf<mojom::LstmWeightLayout>(
      {mojom::LstmWeightLayout::kIofg, mojom::LstmWeightLayout::kIfgo});
}

// Generates values in [min_val, max_val] with log-uniform distribution,
// strongly biasing toward small values. ~50% of values fall below
// sqrt(max_val), making small dimensions much more likely while keeping the
// full range reachable for edge case coverage.
// Note: `SmallBiasedInRange` doesn't support seeds.
auto SmallBiasedInRange(uint32_t min_val, uint32_t max_val) {
  CHECK_LT(min_val, max_val);
  const double log_min = std::log(static_cast<double>(std::max(min_val, 1u)));
  const double log_max = std::log(static_cast<double>(max_val));
  const double log_range = log_max - log_min;
  const double range = static_cast<double>(max_val - min_val);
  return fuzztest::Map(
      [min_val, max_val, log_min, log_range, range](uint32_t raw) -> uint32_t {
        // Preserve exact boundary values.
        if (raw == min_val) {
          return min_val;
        }
        if (raw == max_val) {
          return max_val;
        }
        double t = static_cast<double>(raw - min_val) / range;
        double result = std::exp(log_min + t * log_range);
        result = std::clamp(result, static_cast<double>(min_val),
                            static_cast<double>(max_val));
        return static_cast<uint32_t>(result);
      },
      fuzztest::InRange<uint32_t>(min_val, max_val));
}

auto AnyDimSize() {
  return fuzztest::OneOf(
      // This range is used for supporting seeds.
      fuzztest::InRange<uint32_t>(1, 224),
      SmallBiasedInRange(1, std::numeric_limits<uint16_t>::max()));
}

auto AnyDimSizeOrZero() {
  return fuzztest::OneOf(
      // This range is used for supporting seeds.
      fuzztest::InRange<uint32_t>(0, 224),
      SmallBiasedInRange(0, std::numeric_limits<uint16_t>::max()));
}

auto AnySize2d() {
  return fuzztest::StructOf<Size2d<uint32_t>>(AnyDimSize(), AnyDimSize());
}

auto AnySizeOrZero2d() {
  return fuzztest::StructOf<Size2d<uint32_t>>(AnyDimSizeOrZero(),
                                              AnyDimSizeOrZero());
}

auto AnyPadding2d() {
  auto zero_padding = fuzztest::StructOf<Size2d<uint32_t>>(fuzztest::Just(0u),
                                                           fuzztest::Just(0u));
  auto one_padding = fuzztest::StructOf<Size2d<uint32_t>>(fuzztest::Just(1u),
                                                          fuzztest::Just(1u));
  return fuzztest::OneOf(
      // No padding.
      fuzztest::StructOf<Padding2d>(zero_padding, zero_padding),
      // Symmetric 1x1 padding.
      fuzztest::StructOf<Padding2d>(one_padding, one_padding),
      // Symmetric padding.
      fuzztest::Map([](Size2d<uint32_t> s) -> Padding2d { return {s, s}; },
                    AnySizeOrZero2d()),
      // Random padding.
      fuzztest::StructOf<Padding2d>(AnySizeOrZero2d(), AnySizeOrZero2d()));
}

auto AnyFilterDimensions2d() {
  return fuzztest::OneOf(
      // Common filter sizes: 1x1, 1x3, 2x2, 3x3, 5x5.
      fuzztest::StructOf<Size2d<uint32_t>>(fuzztest::Just(1u),
                                           fuzztest::Just(1u)),
      fuzztest::StructOf<Size2d<uint32_t>>(fuzztest::Just(1u),
                                           fuzztest::Just(3u)),
      fuzztest::StructOf<Size2d<uint32_t>>(fuzztest::Just(2u),
                                           fuzztest::Just(2u)),
      fuzztest::StructOf<Size2d<uint32_t>>(fuzztest::Just(3u),
                                           fuzztest::Just(3u)),
      fuzztest::StructOf<Size2d<uint32_t>>(fuzztest::Just(5u),
                                           fuzztest::Just(5u)),
      // Random filter dimensions.
      AnySize2d());
}

auto AnyStrides2d() {
  return fuzztest::OneOf(
      // No striding.
      fuzztest::StructOf<Size2d<uint32_t>>(fuzztest::Just(1u),
                                           fuzztest::Just(1u)),
      // Common stride=2.
      fuzztest::StructOf<Size2d<uint32_t>>(fuzztest::Just(2u),
                                           fuzztest::Just(2u)),
      // Random strides.
      AnySize2d());
}

auto AnyDilations2d() {
  return fuzztest::OneOf(
      // No dilation.
      fuzztest::StructOf<Size2d<uint32_t>>(fuzztest::Just(1u),
                                           fuzztest::Just(1u)),
      // Common dilation=2.
      fuzztest::StructOf<Size2d<uint32_t>>(fuzztest::Just(2u),
                                           fuzztest::Just(2u)),
      // Random dilations.
      AnySize2d());
}

auto AnyOutputPadding2d() {
  return fuzztest::OneOf(
      // No output padding.
      fuzztest::StructOf<Size2d<uint32_t>>(fuzztest::Just(0u),
                                           fuzztest::Just(0u)),
      // Random output padding.
      AnySizeOrZero2d());
}

auto AnyQuantizedDataType() {
  return fuzztest::ElementOf<OperandDataType>(
      {OperandDataType::kInt8, OperandDataType::kUint8});
}

auto AnyTensorRank() {
  return fuzztest::OneOf(fuzztest::InRange<uint32_t>(1, 8),
                         fuzztest::InRange<uint32_t>(1, 2));
}

auto AnyTensorRankIncludeZero() {
  return fuzztest::OneOf(fuzztest::InRange<uint32_t>(0, 8),
                         fuzztest::InRange<uint32_t>(0, 2));
}

// Returns a domain of OperandDataType values filtered by the given
// SupportedDataTypes.
auto AnyOperandDataTypeFor(SupportedDataTypes supported) {
  std::vector<OperandDataType> types(supported.begin(), supported.end());
  return fuzztest::ElementOf<OperandDataType>(std::move(types));
}

auto AnyActivationParams(base::span<const ActivationKind> kinds) {
  SupportedDataTypes activation_data_types;
  for (auto kind : kinds) {
    activation_data_types.PutAll(GetActivationDataTypes(kind));
  }

  std::vector<ActivationKind> kinds_vec(kinds.begin(), kinds.end());
  return fuzztest::Filter(
      [](const ActivationParams& params) {
        return GetActivationDataTypes(params.kind).Has(params.data_type);
      },
      fuzztest::StructOf<ActivationParams>(
          AnyOperandDataTypeFor(activation_data_types),
          AnyTensorRankIncludeZero(),          // rank
          fuzztest::ArrayOf<8>(AnyDimSize()),  // input_dims
          fuzztest::ElementOf<ActivationKind>(std::move(kinds_vec)),
          fuzztest::Arbitrary<bool>()  // is_input_constant
          ));
}

auto AnyBatchNormalizationParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<BatchNormalizationParams>(
      AnyOperandDataTypeFor(limits.batch_normalization_input.data_types),
      AnyTensorRank(),                     // rank
      fuzztest::ArrayOf<8>(AnyDimSize()),  // input_dims
      fuzztest::InRange<uint32_t>(0, 7),   // axis
      fuzztest::OneOf(fuzztest::Just(1e-5f),
                      fuzztest::Positive<float>()),  // epsilon
      AnyOptionalOperandKind(),                      // scale_kind
      AnyOptionalOperandKind(),                      // bias_kind
      fuzztest::Arbitrary<bool>(),                   // is_input_constant
      fuzztest::Arbitrary<bool>(),                   // is_mean_constant
      fuzztest::Arbitrary<bool>()                    // is_variance_constant
  );
}

auto AnyClampParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<ClampParams>(
      AnyOperandDataTypeFor(limits.clamp_input.data_types),
      AnyTensorRankIncludeZero(),          // rank
      fuzztest::ArrayOf<8>(AnyDimSize()),  // input_dims
      // Bias toward the special min/max pairs that GetClampOperatorCode()
      // recognizes (e.g. relu1, relu0To1, relu6, relu) so those code paths get
      // exercised, while still allowing arbitrary floats.
      fuzztest::OneOf(fuzztest::Just(-1.0f), fuzztest::Just(0.0f),
                      fuzztest::Arbitrary<float>()),  // min_value
      fuzztest::OneOf(fuzztest::Just(1.0f), fuzztest::Just(6.0f),
                      fuzztest::Just(std::numeric_limits<float>::infinity()),
                      fuzztest::Arbitrary<float>()),  // max_value
      fuzztest::Arbitrary<bool>()                     // is_input_constant
  );
}

auto AnyConcatParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<ConcatParams>(
      AnyOperandDataTypeFor(limits.concat_inputs.data_types),
      AnyTensorRank(),                     // input_rank
      fuzztest::ArrayOf<8>(AnyDimSize()),  // input_dims
      fuzztest::InRange<uint32_t>(0, 7),   // axis
      fuzztest::VectorOf(AnyDimSize())
          .WithMaxSize(kMaxValidTensorCount - 1),  // extra_axis_dims
      fuzztest::Arbitrary<bool>()                  // is_input_constant
  );
}

auto AnyConv2dParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<Conv2dParams>(
      AnyOperandDataTypeFor(limits.conv2d_input.data_types), AnyConv2dKind(),
      AnyDimSize(),             // batch
      AnyDimSize(),             // input_channels
      AnyDimSize(),             // input_height
      AnyDimSize(),             // input_width
      AnyDimSize(),             // output_channels
      AnyPadding2d(),           // padding
      AnyFilterDimensions2d(),  // filter_dimensions
      AnyStrides2d(),           // strides
      AnyDilations2d(),         // dilations
      AnyOutputPadding2d(),     // output_padding
      fuzztest::OneOf(fuzztest::Just(1u),
                      AnyDimSize()),  // groups
      fuzztest::Arbitrary<bool>(),    // is_input_constant
      fuzztest::Arbitrary<bool>(),    // is_filter_constant
      AnyOptionalOperandKind(),       // bias_kind
      fuzztest::Arbitrary<bool>(),    // is_depthwise
      fuzztest::ElementOf<Conv2dActivationKind>(
          {Conv2dActivationKind::kNone, Conv2dActivationKind::kRelu,
           Conv2dActivationKind::kRelu6, Conv2dActivationKind::kReluN1To1,
           Conv2dActivationKind::kReluViaClamp})  // activation_kind
  );
}

auto AnyDequantizeLinearParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<DequantizeLinearParams>(
      AnyOperandDataTypeFor(limits.dequantize_linear_input.data_types),
      AnyOperandDataTypeFor(limits.dequantize_linear_scale.data_types),
      AnyTensorRankIncludeZero(),          // rank
      fuzztest::ArrayOf<8>(AnyDimSize()),  // input_dims
      AnyQuantizationKind(),               // quantization_kind
      fuzztest::InRange<uint32_t>(0, 7),   // channel_axis
      fuzztest::InRange<uint32_t>(         // channel_block_size
          1, std::numeric_limits<int16_t>::max()),
      fuzztest::Arbitrary<bool>(),  // is_input_constant
      fuzztest::Arbitrary<bool>(),  // is_scale_constant
      fuzztest::Arbitrary<bool>()   // is_zero_point_constant
  );
}

auto AnyElementWiseBinaryParams(
    base::span<const mojom::ElementWiseBinary::Kind> kinds) {
  SupportedDataTypes binary_data_types;
  for (auto kind : kinds) {
    binary_data_types.PutAll(GetElementWiseBinaryDataTypes(kind));
  }

  std::vector<mojom::ElementWiseBinary::Kind> kinds_vec(kinds.begin(),
                                                        kinds.end());
  // Bias input dims toward 1 which is broadcastable.
  auto any_input_dim = fuzztest::OneOf(fuzztest::Just(1u), AnyDimSize());
  return fuzztest::Filter(
      [](const ElementWiseBinaryParams& params) {
        return GetElementWiseBinaryDataTypes(params.kind).Has(params.data_type);
      },
      fuzztest::StructOf<ElementWiseBinaryParams>(
          AnyOperandDataTypeFor(binary_data_types),
          fuzztest::ElementOf<mojom::ElementWiseBinary::Kind>(
              std::move(kinds_vec)),
          AnyTensorRankIncludeZero(),           // lhs_rank
          AnyTensorRankIncludeZero(),           // rhs_rank
          fuzztest::ArrayOf<8>(any_input_dim),  // lhs_dims
          fuzztest::ArrayOf<8>(any_input_dim),  // rhs_dims
          fuzztest::Arbitrary<bool>(),          // is_lhs_constant
          fuzztest::Arbitrary<bool>()           // is_rhs_constant
          ));
}

auto AnyEluParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<EluParams>(
      AnyOperandDataTypeFor(limits.elu_input.data_types),
      AnyTensorRankIncludeZero(),          // rank
      fuzztest::ArrayOf<8>(AnyDimSize()),  // input_dims
      fuzztest::OneOf(fuzztest::Just(1.0f),
                      fuzztest::Arbitrary<float>()),  // alpha
      fuzztest::Arbitrary<bool>()                     // is_input_constant
  );
}

auto AnyExpandParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  // Bias input dims toward 1 which is broadcastable.
  auto any_input_dim = fuzztest::OneOf(fuzztest::Just(1u), AnyDimSize());
  return fuzztest::StructOf<ExpandParams>(
      AnyOperandDataTypeFor(limits.expand_input.data_types),
      AnyTensorRankIncludeZero(),           // input_rank
      AnyTensorRankIncludeZero(),           // output_rank
      fuzztest::ArrayOf<8>(any_input_dim),  // input_dims
      fuzztest::ArrayOf<8>(AnyDimSize()),   // output_dims
      fuzztest::Arbitrary<bool>()           // is_input_constant
  );
}

auto AnyGatherParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<GatherParams>(
      AnyOperandDataTypeFor(limits.gather_input.data_types),
      AnyOperandDataTypeFor(limits.gather_indices.data_types),
      AnyTensorRank(),                     // input_rank
      fuzztest::ArrayOf<8>(AnyDimSize()),  // input_dims
      AnyTensorRankIncludeZero(),          // indices_rank
      fuzztest::ArrayOf<8>(AnyDimSize()),  // indices_dims
      fuzztest::InRange<uint32_t>(0, 7),   // axis
      fuzztest::OneOf(fuzztest::InRange<int64_t>(-10, 10),
                      fuzztest::Arbitrary<int64_t>()),  // indices_fill_value
      fuzztest::Arbitrary<bool>(),                      // is_input_constant
      fuzztest::Arbitrary<bool>()                       // is_indices_constant
  );
}

auto AnyGatherNDParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<GatherNDParams>(
      AnyOperandDataTypeFor(limits.gather_nd_input.data_types),
      AnyOperandDataTypeFor(limits.gather_nd_indices.data_types),
      AnyTensorRank(),                     // input_rank
      fuzztest::ArrayOf<8>(AnyDimSize()),  // input_dims
      AnyTensorRank(),                     // indices_rank
      fuzztest::ArrayOf<8>(AnyDimSize()),  // indices_dims
      fuzztest::OneOf(fuzztest::InRange<int64_t>(-10, 10),
                      fuzztest::Arbitrary<int64_t>()),  // indices_fill_value
      fuzztest::Arbitrary<bool>(),                      // is_input_constant
      fuzztest::Arbitrary<bool>()                       // is_indices_constant
  );
}

auto AnyGemmParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<GemmParams>(
      AnyOperandDataTypeFor(limits.gemm_a.data_types),
      AnyDimSize(),  // m
      AnyDimSize(),  // k
      AnyDimSize(),  // n
      // The 1.0f value exercises the fusible path and 0.0f exercises the
      // alpha == 0 simplification path for TFLite backend:
      // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=2083;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
      // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=5652;drc=398e74869153a12e825bc789c2134762cbe81c36
      fuzztest::OneOf(fuzztest::Just(1.0f), fuzztest::Just(0.0f),
                      fuzztest::Arbitrary<float>()),  // alpha
      fuzztest::OneOf(fuzztest::Just(1.0f),
                      fuzztest::Arbitrary<float>()),  // beta
      fuzztest::Arbitrary<bool>(),                    // a_transpose
      fuzztest::Arbitrary<bool>(),                    // b_transpose
      fuzztest::Arbitrary<bool>(),                    // has_c
      fuzztest::ElementOf<GemmCShapeKind>(
          {GemmCShapeKind::kScalar, GemmCShapeKind::k1D,
           GemmCShapeKind::k2D_1xN, GemmCShapeKind::k2D_MxN}),  // c_shape_kind
      fuzztest::Arbitrary<bool>(),                              // is_a_constant
      fuzztest::Arbitrary<bool>(),                              // is_b_constant
      fuzztest::Arbitrary<bool>()                               // is_c_constant
  );
}

auto AnyHardSigmoidParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<HardSigmoidParams>(
      AnyOperandDataTypeFor(limits.hard_sigmoid_input.data_types),
      AnyTensorRankIncludeZero(),          // rank
      fuzztest::ArrayOf<8>(AnyDimSize()),  // input_dims
      fuzztest::Arbitrary<float>(),        // alpha
      fuzztest::Arbitrary<float>(),        // beta
      fuzztest::Arbitrary<bool>()          // is_input_constant
  );
}

auto AnyInstanceNormalizationParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<InstanceNormalizationParams>(
      AnyOperandDataTypeFor(limits.instance_normalization_input.data_types),
      AnyDimSize(),  // batch
      AnyDimSize(),  // channels
      AnyDimSize(),  // input_height
      AnyDimSize(),  // input_width
      fuzztest::OneOf(fuzztest::Just(1e-5f),
                      fuzztest::Positive<float>()),  // epsilon
      AnyOptionalOperandKind(),                      // scale_kind
      AnyOptionalOperandKind(),                      // bias_kind
      fuzztest::Arbitrary<bool>()                    // is_input_constant
  );
}

auto AnyLayerNormalizationParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<LayerNormalizationParams>(
      AnyOperandDataTypeFor(limits.layer_normalization_input.data_types),
      AnyTensorRankIncludeZero(),          // rank
      fuzztest::ArrayOf<8>(AnyDimSize()),  // input_dims
      fuzztest::InRange<uint32_t>(0, 8),   // num_axes
      fuzztest::ArrayOf<8>(                // axes
          fuzztest::InRange<uint32_t>(0, 7)),
      fuzztest::OneOf(fuzztest::Just(1e-5f),
                      fuzztest::Positive<float>()),  // epsilon
      AnyOptionalOperandKind(),                      // scale_kind
      AnyOptionalOperandKind(),                      // bias_kind
      fuzztest::Arbitrary<bool>()                    // is_input_constant
  );
}

auto AnyLeakyReluParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<LeakyReluParams>(
      AnyOperandDataTypeFor(limits.leaky_relu_input.data_types),
      AnyTensorRankIncludeZero(),          // rank
      fuzztest::ArrayOf<8>(AnyDimSize()),  // input_dims
      fuzztest::Arbitrary<float>(),        // alpha
      fuzztest::Arbitrary<bool>()          // is_input_constant
  );
}

auto AnyLinearParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<LinearParams>(
      AnyOperandDataTypeFor(limits.linear_input.data_types),
      AnyTensorRankIncludeZero(),          // rank
      fuzztest::ArrayOf<8>(AnyDimSize()),  // input_dims
      fuzztest::Arbitrary<float>(),        // alpha
      fuzztest::Arbitrary<float>(),        // beta
      fuzztest::Arbitrary<bool>()          // is_input_constant
  );
}

auto AnyLstmParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<LstmParams>(
      AnyOperandDataTypeFor(limits.lstm_input.data_types),
      AnyDimSize(),                 // steps
      AnyDimSize(),                 // batch_size
      AnyDimSize(),                 // input_size
      AnyDimSize(),                 // hidden_size
      AnyLstmDirection(),           // direction
      AnyLstmWeightLayout(),        // layout
      AnyOptionalOperandKind(),     // bias_kind
      AnyOptionalOperandKind(),     // recurrent_bias_kind
      AnyOptionalOperandKind(),     // peephole_weight_kind
      AnyOptionalOperandKind(),     // initial_hidden_state_kind
      AnyOptionalOperandKind(),     // initial_cell_state_kind
      fuzztest::Arbitrary<bool>(),  // return_sequence
      fuzztest::Arbitrary<bool>(),  // is_input_constant
      fuzztest::Arbitrary<bool>(),  // is_weight_constant
      fuzztest::Arbitrary<bool>(),  // is_recurrent_weight_constant
      fuzztest::ArrayOf<3>(AnyRecurrentNetworkActivation())  // activations
  );
}

auto AnyLstmCellParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<LstmCellParams>(
      AnyOperandDataTypeFor(limits.lstm_cell_input.data_types),
      AnyDimSize(),                 // batch_size
      AnyDimSize(),                 // input_size
      AnyDimSize(),                 // hidden_size
      AnyLstmWeightLayout(),        // layout
      AnyOptionalOperandKind(),     // bias_kind
      AnyOptionalOperandKind(),     // recurrent_bias_kind
      AnyOptionalOperandKind(),     // peephole_weight_kind
      fuzztest::Arbitrary<bool>(),  // is_input_constant
      fuzztest::Arbitrary<bool>(),  // is_weight_constant
      fuzztest::Arbitrary<bool>(),  // is_recurrent_weight_constant
      fuzztest::Arbitrary<bool>(),  // is_hidden_state_constant
      fuzztest::Arbitrary<bool>(),  // is_cell_state_constant
      fuzztest::ArrayOf<3>(AnyRecurrentNetworkActivation())  // activations
  );
}

auto AnyMatmulParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  // Bias input dims toward 1 which is broadcastable.
  auto any_input_dim = fuzztest::OneOf(fuzztest::Just(1u), AnyDimSize());
  return fuzztest::StructOf<MatmulParams>(
      AnyOperandDataTypeFor(limits.matmul_input.data_types),
      fuzztest::InRange<uint32_t>(2, 8),    // a_rank
      fuzztest::InRange<uint32_t>(2, 8),    // b_rank
      fuzztest::ArrayOf<8>(any_input_dim),  // a_dims
      fuzztest::ArrayOf<8>(any_input_dim),  // b_dims
      fuzztest::Arbitrary<bool>(),          // is_a_constant
      fuzztest::Arbitrary<bool>()           // is_b_constant
  );
}

auto AnyPadParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<PadParams>(
      AnyOperandDataTypeFor(limits.pad_input.data_types),
      AnyTensorRankIncludeZero(),                // input_rank
      fuzztest::ArrayOf<8>(AnyDimSize()),        // input_dims
      fuzztest::ArrayOf<8>(AnyDimSizeOrZero()),  // beginning_padding
      fuzztest::ArrayOf<8>(AnyDimSizeOrZero()),  // ending_padding
      AnyPaddingModeTag(),                       // mode
      fuzztest::Arbitrary<float>(),              // value
      fuzztest::Arbitrary<bool>()                // is_input_constant
  );
}

auto AnyPool2dParams() {
  SupportedDataTypes pool2d_data_types;
  for (auto kind : kAllPool2dKinds) {
    pool2d_data_types.PutAll(GetPool2dDataTypes(kind));
  }

  return fuzztest::Filter(
      [](const Pool2dParams& params) {
        return GetPool2dDataTypes(params.pool2d_kind).Has(params.data_type);
      },
      fuzztest::StructOf<Pool2dParams>(
          AnyOperandDataTypeFor(pool2d_data_types), AnyPool2dKind(),
          AnyRoundingType(),
          AnyDimSize(),                // batch
          AnyDimSize(),                // channels
          AnyDimSize(),                // input_height
          AnyDimSize(),                // input_width
          AnyPadding2d(),              // padding
          AnySize2d(),                 // window_dimensions
          AnyStrides2d(),              // strides
          AnyDilations2d(),            // dilations
          fuzztest::Arbitrary<bool>()  // is_input_constant
          ));
}

auto AnyPreluParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  // Bias slope dims toward 1 which is broadcastable.
  auto any_slope_dim = fuzztest::OneOf(fuzztest::Just(1u), AnyDimSize());
  return fuzztest::StructOf<PreluParams>(
      AnyOperandDataTypeFor(limits.prelu_input.data_types),
      AnyTensorRankIncludeZero(),           // rank
      fuzztest::ArrayOf<8>(AnyDimSize()),   // input_dims
      AnyTensorRankIncludeZero(),           // slope_rank
      fuzztest::ArrayOf<8>(any_slope_dim),  // slope_dims
      fuzztest::Arbitrary<bool>(),          // is_input_constant
      fuzztest::Arbitrary<bool>()           // is_slope_constant
  );
}

auto AnyQuantizationParams() {
  return fuzztest::StructOf<QuantizationParams>(
      AnyQuantizedDataType(), AnyQuantizationKind(),
      fuzztest::InRange<uint32_t>(  // channel_block_size
          1, std::numeric_limits<int16_t>::max()));
}

auto AnyQuantizeLinearParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<QuantizeLinearParams>(
      AnyOperandDataTypeFor(limits.quantize_linear_input.data_types),
      AnyOperandDataTypeFor(limits.quantize_linear_zero_point.data_types),
      AnyTensorRankIncludeZero(),          // rank
      fuzztest::ArrayOf<8>(AnyDimSize()),  // input_dims
      AnyQuantizationKind(),               // quantization_kind
      fuzztest::InRange<uint32_t>(0, 7),   // channel_axis
      fuzztest::InRange<uint32_t>(         // channel_block_size
          1, std::numeric_limits<int16_t>::max()),
      fuzztest::Arbitrary<bool>(),  // is_input_constant
      fuzztest::Arbitrary<bool>(),  // is_scale_constant
      fuzztest::Arbitrary<bool>()   // is_zero_point_constant
  );
}

auto AnyReduceParams() {
  SupportedDataTypes reduce_data_types;
  for (auto kind : kAllReduceKinds) {
    reduce_data_types.PutAll(GetReduceDataTypes(kind));
  }

  return fuzztest::Filter(
      [](const ReduceParams& params) {
        return GetReduceDataTypes(params.reduce_kind).Has(params.data_type);
      },
      fuzztest::StructOf<ReduceParams>(
          AnyOperandDataTypeFor(reduce_data_types), AnyReduceKind(),
          AnyTensorRankIncludeZero(),          // input_rank
          fuzztest::ArrayOf<8>(AnyDimSize()),  // input_dims
          fuzztest::InRange<uint32_t>(0, 8),   // num_axes
          fuzztest::ArrayOf<8>(                // axes
              fuzztest::InRange<uint32_t>(0, 7)),
          fuzztest::Arbitrary<bool>(),  // keep_dimensions
          fuzztest::Arbitrary<bool>()   // is_input_constant
          ));
}

auto AnyResample2dParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<Resample2dParams>(
      AnyOperandDataTypeFor(limits.resample2d_input.data_types),
      fuzztest::ElementOf<mojom::Resample2d::InterpolationMode>(
          {mojom::Resample2d::InterpolationMode::kNearestNeighbor,
           mojom::Resample2d::InterpolationMode::kLinear}),  // mode
      AnyDimSize(),                                          // batch
      AnyDimSize(),                                          // channels
      AnyDimSize(),                                          // input_height
      AnyDimSize(),                                          // input_width
      fuzztest::Arbitrary<bool>(),                           // use_sizes
      fuzztest::Positive<float>(),                           // scale_height
      fuzztest::Positive<float>(),                           // scale_width
      AnyDimSize(),                                          // output_height
      AnyDimSize(),                                          // output_width
      fuzztest::Arbitrary<bool>()  // is_input_constant
  );
}

auto AnyScatterElementsParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<ScatterElementsParams>(
      AnyOperandDataTypeFor(limits.scatter_elements_input.data_types),
      AnyOperandDataTypeFor(limits.scatter_elements_indices.data_types),
      AnyTensorRank(),                     // input_rank
      fuzztest::InRange<uint32_t>(0, 7),   // axis
      fuzztest::ArrayOf<8>(AnyDimSize()),  // input_dims
      AnyDimSize(),                        // indices_axis_dim_size
      fuzztest::OneOf(fuzztest::InRange<int64_t>(-10, 10),
                      fuzztest::Arbitrary<int64_t>()),  // indices_fill_value
      fuzztest::Arbitrary<bool>(),                      // is_input_constant
      fuzztest::Arbitrary<bool>(),                      // is_indices_constant
      fuzztest::Arbitrary<bool>()                       // is_updates_constant
  );
}

auto AnySliceParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<SliceParams>(
      AnyOperandDataTypeFor(limits.slice_input.data_types),
      AnyTensorRankIncludeZero(),                // rank
      fuzztest::ArrayOf<8>(AnyDimSize()),        // input_dims
      fuzztest::ArrayOf<8>(AnyDimSizeOrZero()),  // starts
      fuzztest::ArrayOf<8>(AnyDimSize()),        // sizes
      fuzztest::ArrayOf<8>(AnyDimSize()),        // strides
      fuzztest::Arbitrary<bool>()                // is_input_constant
  );
}

auto AnySoftmaxParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<SoftmaxParams>(
      AnyOperandDataTypeFor(limits.softmax_input.data_types),
      AnyTensorRank(),                     // rank
      fuzztest::ArrayOf<8>(AnyDimSize()),  // input_dims
      fuzztest::InRange<uint32_t>(0, 7),   // axis
      fuzztest::Arbitrary<bool>()          // is_input_constant
  );
}

auto AnySplitParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<SplitParams>(
      AnyOperandDataTypeFor(limits.split_input.data_types),
      AnyTensorRank(),                                       // input_rank
      fuzztest::ArrayOf<8>(AnyDimSize()),                    // input_dims
      fuzztest::InRange<uint32_t>(0, 7),                     // axis
      fuzztest::Arbitrary<bool>(),                           // use_equal_splits
      fuzztest::InRange<uint32_t>(1, kMaxValidTensorCount),  // num_splits
      fuzztest::VectorOf(AnyDimSize())
          .WithMinSize(1)
          .WithMaxSize(kMaxValidTensorCount),  // split_sizes
      fuzztest::Arbitrary<bool>()              // is_input_constant
  );
}

auto AnyTransposeParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<TransposeParams>(
      AnyOperandDataTypeFor(limits.transpose_input.data_types),
      AnyTensorRankIncludeZero(),                               // input_rank
      fuzztest::ArrayOf<8>(AnyDimSize()),                       // input_dims
      fuzztest::ArrayOf<8>(fuzztest::InRange<uint32_t>(0, 7)),  // permutation
      fuzztest::Arbitrary<bool>()  // is_input_constant
  );
}

auto AnyTriangularParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<TriangularParams>(
      AnyOperandDataTypeFor(limits.triangular_input.data_types),
      // Triangular requires a rank of at least 2.
      fuzztest::InRange<uint32_t>(2, 8),   // rank
      fuzztest::ArrayOf<8>(AnyDimSize()),  // input_dims
      fuzztest::Arbitrary<bool>(),         // upper
      // The small range biases toward diagonals that land inside the matrix
      // (exercising the real masking path), while the arbitrary range keeps
      // extreme values reachable to probe offset overflow.
      fuzztest::OneOf(fuzztest::InRange<int32_t>(-10, 10),
                      fuzztest::Arbitrary<int32_t>()),  // diagonal
      fuzztest::Arbitrary<bool>()                       // is_input_constant
  );
}

void PopulateConv2dAttributesBase(
    Conv2dAttributesBase& attributes,
    const Conv2dParams& params,
    InputOperandLayout input_layout,
    const std::optional<OperandDescriptor>& bias_desc) {
  attributes.padding = params.padding;
  attributes.strides = params.strides;
  attributes.dilations = params.dilations;
  attributes.groups = params.groups;
  attributes.bias_operand = bias_desc;
  attributes.input_layout = input_layout;
}

// Compute scale/zero_point shape for a given input shape based on
// QuantizationKind:
//   kPerTensor: all dims are 1
//   kPerChannel: channel dim matches input, rest are 1
//   kPerBlock: channel dim = channel_size / block_size, rest are 1
// `channel_axis` is required for kPerChannel and kPerBlock, but unused for
// kPerTensor.
std::vector<uint32_t> ComputeQuantizationScaleShape(
    base::span<const uint32_t> input_shape,
    const QuantizationParams& quantize_params,
    std::optional<uint32_t> channel_axis = std::nullopt) {
  std::vector<uint32_t> shape(input_shape.size(), 1);

  if (quantize_params.quantization_kind != QuantizationKind::kPerTensor) {
    CHECK(channel_axis.has_value());
    CHECK_LT(*channel_axis, input_shape.size());
  }

  switch (quantize_params.quantization_kind) {
    case QuantizationKind::kPerTensor:
      break;
    case QuantizationKind::kPerChannel:
      shape[*channel_axis] = input_shape[*channel_axis];
      break;
    case QuantizationKind::kPerBlock: {
      uint32_t channel_size = input_shape[*channel_axis];
      uint32_t block_size = quantize_params.channel_block_size;
      if (channel_size % block_size != 0) {
        block_size = std::gcd(channel_size, block_size);
      }
      shape[*channel_axis] = channel_size / block_size;
      break;
    }
  }
  return shape;
}

// Build the operand data buffer filled with the repeated byte `seed_for_data`.
std::vector<uint8_t> MakeOperandData(const OperandDescriptor& desc,
                                     uint8_t seed_for_data) {
  return std::vector<uint8_t>(desc.PackedByteLength(), seed_for_data);
}

// Build the operand data buffer filled with the float `seed_for_data`, in the
// byte representation matching `desc`'s data type (float32 or float16).
std::vector<uint8_t> MakeOperandData(const OperandDescriptor& desc,
                                     float seed_for_data) {
  CHECK(desc.data_type() == OperandDataType::kFloat32 ||
        desc.data_type() == OperandDataType::kFloat16);
  if (desc.data_type() == OperandDataType::kFloat32) {
    std::vector<float> values(desc.NumberOfElements(), seed_for_data);
    auto bytes = base::as_byte_span(base::allow_nonunique_obj, values);
    return std::vector<uint8_t>(bytes.begin(), bytes.end());
  }
  // float16: convert each float to its 16-bit IEEE precision format.
  std::vector<uint16_t> f16_values(desc.NumberOfElements(),
                                   fp16_ieee_from_fp32_value(seed_for_data));
  auto bytes = base::as_byte_span(f16_values);
  return std::vector<uint8_t>(bytes.begin(), bytes.end());
}

// Build a constant operand filled from `seed_for_data`, whose type determines
// the byte representation (a `uint8_t` repeats the byte; a `float` is converted
// to the descriptor's float32/float16 layout).
template <typename SeedType>
OperandId BuildConstant(GraphInfoBuilder& builder,
                        const OperandDescriptor& desc,
                        SeedType seed_for_data) {
  static_assert(
      std::is_same_v<SeedType, float> || std::is_same_v<SeedType, uint8_t>,
      "seed_for_data must be either float or uint8_t");
  return builder.BuildConstant(desc.shape(), desc.data_type(),
                               MakeOperandData(desc, seed_for_data));
}

// Build an operand as either a constant or a named input depending on
// `is_constant`, using the `data` buffer. When building an input, the buffer is
// stored in `data_buffers` to keep it alive and the operand is also inserted
// into `named_inputs`.
OperandId BuildInputOrConstant(
    GraphInfoBuilder& builder,
    bool is_constant,
    std::string name,
    const OperandDescriptor& desc,
    std::vector<uint8_t> data,
    std::vector<std::vector<uint8_t>>& data_buffers,
    base::flat_map<std::string, base::span<const uint8_t>>& named_inputs) {
  if (is_constant) {
    return builder.BuildConstant(desc.shape(), desc.data_type(), data);
  }
  // `named_inputs` stores a span into the inner vector's heap buffer. A later
  // `emplace_back` on the outer `data_buffers` may reallocate and move the
  // inner vectors, but moving a `std::vector` transfers its heap pointer rather
  // than copying, so the span stays valid.
  base::span<const uint8_t> span = data_buffers.emplace_back(std::move(data));
  OperandId id = builder.BuildInput(name, desc.shape(), desc.data_type());
  named_inputs.insert({std::move(name), span});
  return id;
}

// Build an operand as either a constant or a named input, with its data buffer
// filled from `seed_for_data`, whose type determines the byte representation (a
// `uint8_t` repeats the byte; a `float` is converted to the descriptor's
// float32/float16 layout).
template <typename SeedType>
OperandId BuildInputOrConstant(
    GraphInfoBuilder& builder,
    bool is_constant,
    std::string name,
    const OperandDescriptor& desc,
    SeedType seed_for_data,
    std::vector<std::vector<uint8_t>>& data_buffers,
    base::flat_map<std::string, base::span<const uint8_t>>& named_inputs) {
  static_assert(
      std::is_same_v<SeedType, float> || std::is_same_v<SeedType, uint8_t>,
      "seed_for_data must be either float or uint8_t");
  return BuildInputOrConstant(builder, is_constant, std::move(name), desc,
                              MakeOperandData(desc, seed_for_data),
                              data_buffers, named_inputs);
}

// Build an optional operand as either absent, a constant, or a named input
// depending on `kind`.
std::optional<OperandId> BuildOptionalOperand(
    GraphInfoBuilder& builder,
    const std::optional<OperandDescriptor>& desc,
    OptionalOperandKind kind,
    std::string name,
    uint8_t seed_for_data,
    std::vector<std::vector<uint8_t>>& data_buffers,
    base::flat_map<std::string, base::span<const uint8_t>>& named_inputs) {
  if (kind == OptionalOperandKind::kNone) {
    return std::nullopt;
  }
  return BuildInputOrConstant(builder, kind == OptionalOperandKind::kConstant,
                              std::move(name), *desc, seed_for_data,
                              data_buffers, named_inputs);
}

// Build the DequantizeLinear for the input side of a DQ-Op-Q pattern. Create
// the quantized input operand (as input or constant), scale/zero-point
// constants, an intermediate operand for the op's input, and the
// DequantizeLinear operation. The input data buffer is appended to
// `data_buffers` to keep it alive for `named_inputs`. Return nullopt if
// descriptor creation or validation fails.
std::optional<OperandId> BuildDequantizeInput(
    GraphInfoBuilder& builder,
    const ContextProperties& context_properties,
    bool is_input_constant,
    std::string_view input_name,
    const OperandDescriptor& op_input_desc,
    const QuantizationParams& quantization_params,
    std::optional<uint32_t> channel_axis,
    uint8_t seed_for_data,
    float scale_value,
    uint8_t zero_point_value,
    std::vector<std::vector<uint8_t>>& data_buffers,
    base::flat_map<std::string, base::span<const uint8_t>>& named_inputs) {
  auto scale_shape = ComputeQuantizationScaleShape(
      op_input_desc.shape(), quantization_params, channel_axis);

  ASSIGN_OR_RETURN_NULLOPT(
      auto input_dq_desc,
      OperandDescriptor::Create(context_properties,
                                quantization_params.quantized_type,
                                op_input_desc.shape(), ""));
  ASSIGN_OR_RETURN_NULLOPT(
      auto input_scale_desc,
      OperandDescriptor::Create(context_properties, op_input_desc.data_type(),
                                scale_shape, ""));
  ASSIGN_OR_RETURN_NULLOPT(
      auto input_zero_desc,
      OperandDescriptor::Create(context_properties,
                                quantization_params.quantized_type, scale_shape,
                                ""));
  ASSIGN_OR_RETURN_NULLOPT(auto input_desc_result,
                           ValidateDequantizeLinearAndInferOutput(
                               context_properties, input_dq_desc,
                               input_scale_desc, input_zero_desc, ""));

  OperandId input_dq_id = BuildInputOrConstant(
      builder, is_input_constant, std::string(input_name), input_dq_desc,
      seed_for_data, data_buffers, named_inputs);

  OperandId input_scale_id =
      BuildConstant(builder, input_scale_desc, scale_value);

  OperandId input_zero_id =
      BuildConstant(builder, input_zero_desc, zero_point_value);

  OperandId op_input_id = builder.BuildIntermediateOperand(
      op_input_desc.shape(), op_input_desc.data_type());

  builder.BuildDequantizeLinear(input_dq_id, input_scale_id, input_zero_id,
                                op_input_id);
  return op_input_id;
}

// Build the QuantizeLinear for the output side of a DQ-Op-Q pattern. Create the
// scale/zero-point constants, the graph output operand, and the QuantizeLinear
// operation. Return false if descriptor creation or validation fails.
bool BuildQuantizeOutput(GraphInfoBuilder& builder,
                         const ContextProperties& context_properties,
                         std::string_view output_name,
                         const OperandDescriptor& op_output_desc,
                         const QuantizationParams& quantization_params,
                         std::optional<uint32_t> channel_axis,
                         OperandId op_output_id,
                         float scale_value,
                         uint8_t zero_point_value) {
  auto scale_shape = ComputeQuantizationScaleShape(
      op_output_desc.shape(), quantization_params, channel_axis);

  ASSIGN_OR_RETURN_FALSE(
      auto output_scale_desc,
      OperandDescriptor::Create(context_properties, op_output_desc.data_type(),
                                scale_shape, ""));
  ASSIGN_OR_RETURN_FALSE(
      auto output_zero_desc,
      OperandDescriptor::Create(context_properties,
                                quantization_params.quantized_type, scale_shape,
                                ""));
  ASSIGN_OR_RETURN_FALSE(auto quantized_output_desc,
                         ValidateQuantizeLinearAndInferOutput(
                             context_properties, op_output_desc,
                             output_scale_desc, output_zero_desc, ""));

  OperandId output_scale_id =
      BuildConstant(builder, output_scale_desc, scale_value);

  OperandId output_zero_id =
      BuildConstant(builder, output_zero_desc, zero_point_value);

  OperandId quantize_output_id = builder.BuildOutput(
      std::string(output_name), quantized_output_desc.shape(),
      quantized_output_desc.data_type());
  builder.BuildQuantizeLinear(op_output_id, output_scale_id, output_zero_id,
                              quantize_output_id);
  return true;
}

struct ClampDescriptors {
  // The output of clamp has the same shape and data type as the input.
  OperandDescriptor input_desc;
  float min_value;
  float max_value;
};

// Helper to set up ClampDescriptors. Returns nullopt if any validation fails.
std::optional<ClampDescriptors> SetUpClampDescriptors(
    const ContextProperties& context_properties,
    const ClampParams& params) {
  std::vector<uint32_t> input_dims(params.input_dims.begin(),
                                   params.input_dims.begin() + params.rank);

  ASSIGN_OR_RETURN_NULLOPT(
      auto input_desc,
      OperandDescriptor::Create(context_properties, params.data_type,
                                input_dims, ""));

  return ClampDescriptors{
      .input_desc = std::move(input_desc),
      .min_value = std::min(params.min_value, params.max_value),
      .max_value = std::max(params.min_value, params.max_value),
  };
}

struct ConcatDescriptors {
  std::vector<OperandDescriptor> input_descs;
  OperandDescriptor output_desc;
  uint32_t axis;
};

// Helper to set up ConcatDescriptors. Returns nullopt if any validation fails.
std::optional<ConcatDescriptors> SetUpConcatDescriptors(
    const ContextProperties& context_properties,
    ConcatParams& params) {
  std::vector<uint32_t> base_dims(params.input_dims.begin(),
                                  params.input_dims.begin() + params.rank);

  params.axis = params.axis % params.rank;

  std::vector<OperandDescriptor> input_descs;
  input_descs.reserve(params.extra_axis_dims.size() + 1u);

  // First input uses base_dims.
  ASSIGN_OR_RETURN_NULLOPT(
      auto first_desc,
      OperandDescriptor::Create(context_properties, params.data_type, base_dims,
                                ""));
  input_descs.push_back(std::move(first_desc));

  // Additional inputs share all dims except the concat axis.
  for (size_t i = 0; i < params.extra_axis_dims.size(); ++i) {
    std::vector<uint32_t> dims = base_dims;
    dims[params.axis] = params.extra_axis_dims[i];
    ASSIGN_OR_RETURN_NULLOPT(
        auto desc, OperandDescriptor::Create(context_properties,
                                             params.data_type, dims, ""));
    input_descs.push_back(std::move(desc));
  }

  ASSIGN_OR_RETURN_NULLOPT(
      auto output_desc, ValidateConcatAndInferOutput(
                            context_properties, input_descs, params.axis, ""));

  return ConcatDescriptors{
      .input_descs = std::move(input_descs),
      .output_desc = std::move(output_desc),
      .axis = params.axis,
  };
}

struct Conv2dDescriptors {
  OperandDescriptor input_desc;
  OperandDescriptor filter_desc;
  std::optional<OperandDescriptor> bias_desc;
  OperandDescriptor output_desc;
};

// Helper to set up Conv2dDescriptors. Returns nullopt if any validation fails.
std::optional<Conv2dDescriptors> SetUpConv2dDescriptors(
    const ContextProperties& context_properties,
    Conv2dParams& params) {
  InputOperandLayout input_layout = context_properties.input_operand_layout;

  bool is_depthwise =
      params.conv2d_kind == mojom::Conv2d::Kind::kDirect && params.is_depthwise;
  if (is_depthwise) {
    // For depthwise conv2d, output_channels, input_channels, and groups must be
    // equal.
    params.output_channels = params.input_channels;
    params.groups = params.input_channels;
  }

#if BUILDFLAG(IS_LINUX)
  if (params.conv2d_kind == mojom::Conv2d::Kind::kTransposed) {
    // ConvTranspose2d does not support dilation and groups for TFLite backend:
    // https://source.chromium.org/chromium/chromium/src/+/db6bda50f023057ffa82845f232852dea0f271e1:services/webnn/tflite/graph_builder_tflite.cc;l=4125
    // TODO(crbug.com/498987226): Remove this restriction to increase test
    // coverage.
    params.dilations = {1, 1};
    params.groups = 1;
  }
#endif  // BUILDFLAG(IS_LINUX)

  if (params.output_channels % params.groups != 0 ||
      (params.conv2d_kind == mojom::Conv2d::Kind::kDirect &&
       params.input_channels % params.groups != 0)) {
    params.groups = std::gcd(params.output_channels, params.input_channels);
  }

  std::vector<uint32_t> input_dims;
  std::vector<uint32_t> filter_dims;
  switch (input_layout) {
    case InputOperandLayout::kNhwc: {
      input_dims = {params.batch, params.input_height, params.input_width,
                    params.input_channels};
      if (params.conv2d_kind == mojom::Conv2d::Kind::kDirect) {
        if (is_depthwise) {
          filter_dims = {1, params.filter_dimensions.height,
                         params.filter_dimensions.width, params.groups};
        } else {
          filter_dims = {params.output_channels,
                         params.filter_dimensions.height,
                         params.filter_dimensions.width,
                         params.input_channels / params.groups};
        }
      } else {
        filter_dims = {params.output_channels / params.groups,
                       params.filter_dimensions.height,
                       params.filter_dimensions.width, params.input_channels};
      }
      break;
    }
    case InputOperandLayout::kNchw: {
      input_dims = {params.batch, params.input_channels, params.input_height,
                    params.input_width};
      if (params.conv2d_kind == mojom::Conv2d::Kind::kDirect) {
        filter_dims = {
            params.output_channels, params.input_channels / params.groups,
            params.filter_dimensions.height, params.filter_dimensions.width};
      } else {
        filter_dims = {
            params.input_channels, params.output_channels / params.groups,
            params.filter_dimensions.height, params.filter_dimensions.width};
      }
      break;
    }
  }

  ASSIGN_OR_RETURN_NULLOPT(
      auto input_desc,
      OperandDescriptor::Create(context_properties, params.data_type,
                                input_dims, ""));
  ASSIGN_OR_RETURN_NULLOPT(
      auto filter_desc,
      OperandDescriptor::Create(context_properties, params.data_type,
                                filter_dims, ""));

  std::optional<OperandDescriptor> bias_desc;
  if (params.bias_kind != OptionalOperandKind::kNone) {
    ASSIGN_OR_RETURN_NULLOPT(
        bias_desc,
        OperandDescriptor::Create(context_properties, params.data_type,
                                  {params.output_channels}, ""));
  }

  std::optional<OperandDescriptor> output_desc;
  switch (params.conv2d_kind) {
    case mojom::Conv2d::Kind::kDirect: {
      Conv2dAttributes attr;
      PopulateConv2dAttributesBase(attr, params, input_layout, bias_desc);
      switch (input_layout) {
        case InputOperandLayout::kNhwc:
          if (is_depthwise) {
            attr.filter_layout = Conv2dFilterOperandLayout::kIhwo;
          } else {
            attr.filter_layout = Conv2dFilterOperandLayout::kOhwi;
          }
          break;
        case InputOperandLayout::kNchw:
          attr.filter_layout = Conv2dFilterOperandLayout::kOihw;
          break;
      }

      ASSIGN_OR_RETURN_NULLOPT(
          output_desc, ValidateConv2dAndInferOutput(
                           context_properties, input_desc, filter_desc, attr));
      break;
    }
    case mojom::Conv2d::Kind::kTransposed: {
      ConvTranspose2dAttributes attr;
      PopulateConv2dAttributesBase(attr, params, input_layout, bias_desc);
      attr.filter_layout = input_layout == InputOperandLayout::kNhwc
                               ? ConvTranspose2dFilterOperandLayout::kOhwi
                               : ConvTranspose2dFilterOperandLayout::kIohw;
      attr.output_padding = params.output_padding;
      ASSIGN_OR_RETURN_NULLOPT(
          output_desc, ValidateConvTranspose2dAndInferOutput(
                           context_properties, input_desc, filter_desc, attr));
      break;
    }
  }

  return Conv2dDescriptors{
      .input_desc = std::move(input_desc),
      .filter_desc = std::move(filter_desc),
      .bias_desc = std::move(bias_desc),
      .output_desc = std::move(*output_desc),
  };
}

struct ElementWiseBinaryDescriptors {
  OperandDescriptor lhs_desc;
  OperandDescriptor rhs_desc;
  OperandDescriptor output_desc;
};

// Helper to set up ElementWiseBinaryDescriptors. Returns nullopt if any
// validation fails.
std::optional<ElementWiseBinaryDescriptors> SetUpElementWiseBinaryDescriptors(
    const ContextProperties& context_properties,
    const ElementWiseBinaryParams& params) {
  std::vector<uint32_t> lhs_dims(params.lhs_dims.begin(),
                                 params.lhs_dims.begin() + params.lhs_rank);
  std::vector<uint32_t> rhs_dims(params.rhs_dims.begin(),
                                 params.rhs_dims.begin() + params.rhs_rank);

  // Fix up dims to ensure broadcast compatibility. For each aligned dimension
  // pair (from the right), if they're not equal and neither is 1, make rhs
  // match lhs.
  size_t min_rank = std::min(lhs_dims.size(), rhs_dims.size());
  for (size_t i = 0; i < min_rank; ++i) {
    size_t lhs_idx = lhs_dims.size() - 1 - i;
    size_t rhs_idx = rhs_dims.size() - 1 - i;
    if (lhs_dims[lhs_idx] != rhs_dims[rhs_idx] && lhs_dims[lhs_idx] != 1 &&
        rhs_dims[rhs_idx] != 1) {
      rhs_dims[rhs_idx] = lhs_dims[lhs_idx];
    }
  }

  ASSIGN_OR_RETURN_NULLOPT(
      auto lhs_desc, OperandDescriptor::Create(context_properties,
                                               params.data_type, lhs_dims, ""));
  ASSIGN_OR_RETURN_NULLOPT(
      auto rhs_desc, OperandDescriptor::Create(context_properties,
                                               params.data_type, rhs_dims, ""));

  auto output_dims = BroadcastShapes(lhs_dims, rhs_dims);
  if (!output_dims.has_value()) {
    return std::nullopt;
  }

  ASSIGN_OR_RETURN_NULLOPT(
      auto output_desc,
      OperandDescriptor::Create(context_properties, params.data_type,
                                output_dims.value(), ""));

  return ElementWiseBinaryDescriptors{
      .lhs_desc = std::move(lhs_desc),
      .rhs_desc = std::move(rhs_desc),
      .output_desc = std::move(output_desc),
  };
}

struct EluDescriptors {
  // The output of elu has the same shape and data type as the input.
  OperandDescriptor input_desc;
  float alpha;
};

// Helper to set up EluDescriptors. Returns nullopt if any validation fails.
std::optional<EluDescriptors> SetUpEluDescriptors(
    const ContextProperties& context_properties,
    const EluParams& params) {
  std::vector<uint32_t> input_dims(params.input_dims.begin(),
                                   params.input_dims.begin() + params.rank);

  ASSIGN_OR_RETURN_NULLOPT(
      auto input_desc,
      OperandDescriptor::Create(context_properties, params.data_type,
                                input_dims, ""));

  // Replace NaN or infinite alpha with the default value so the operator is
  // valid.
  float alpha = std::isfinite(params.alpha) ? params.alpha : 1.0f;

  return EluDescriptors{
      .input_desc = std::move(input_desc),
      .alpha = alpha,
  };
}

struct GatherDescriptors {
  OperandDescriptor input_desc;
  OperandDescriptor indices_desc;
  OperandDescriptor output_desc;
  uint32_t axis;
};

// Helper to set up GatherDescriptors. Returns nullopt if any validation fails.
std::optional<GatherDescriptors> SetUpGatherDescriptors(
    const ContextProperties& context_properties,
    GatherParams& params) {
  std::vector<uint32_t> input_dims(
      params.input_dims.begin(), params.input_dims.begin() + params.input_rank);

  std::vector<uint32_t> indices_dims(
      params.indices_dims.begin(),
      params.indices_dims.begin() + params.indices_rank);

  params.axis = params.axis % params.input_rank;

  ASSIGN_OR_RETURN_NULLOPT(
      auto input_desc,
      OperandDescriptor::Create(context_properties, params.input_data_type,
                                input_dims, ""));
  ASSIGN_OR_RETURN_NULLOPT(
      auto indices_desc,
      OperandDescriptor::Create(context_properties, params.indices_data_type,
                                indices_dims, ""));
  ASSIGN_OR_RETURN_NULLOPT(
      auto output_desc,
      ValidateGatherAndInferOutput(context_properties, input_desc, indices_desc,
                                   params.axis, ""));

  return GatherDescriptors{
      .input_desc = std::move(input_desc),
      .indices_desc = std::move(indices_desc),
      .output_desc = std::move(output_desc),
      .axis = params.axis,
  };
}

struct GemmDescriptors {
  OperandDescriptor a_desc;
  OperandDescriptor b_desc;
  std::optional<OperandDescriptor> c_desc;
  OperandDescriptor output_desc;
};

// Helper to set up GemmDescriptors. Returns nullopt if any validation fails.
std::optional<GemmDescriptors> SetUpGemmDescriptors(
    const ContextProperties& context_properties,
    const GemmParams& params) {
  std::vector<uint32_t> a_dims =
      params.a_transpose ? std::vector<uint32_t>{params.k, params.m}
                         : std::vector<uint32_t>{params.m, params.k};
  std::vector<uint32_t> b_dims =
      params.b_transpose ? std::vector<uint32_t>{params.n, params.k}
                         : std::vector<uint32_t>{params.k, params.n};

  ASSIGN_OR_RETURN_NULLOPT(
      auto a_desc, OperandDescriptor::Create(context_properties,
                                             params.data_type, a_dims, ""));
  ASSIGN_OR_RETURN_NULLOPT(
      auto b_desc, OperandDescriptor::Create(context_properties,
                                             params.data_type, b_dims, ""));

  GemmAttributes attr;
  attr.alpha = params.alpha;
  attr.beta = params.beta;
  attr.a_transpose = params.a_transpose;
  attr.b_transpose = params.b_transpose;

  std::optional<OperandDescriptor> c_desc;
  if (params.has_c) {
    std::vector<uint32_t> c_dims;
    switch (params.c_shape_kind) {
      case GemmCShapeKind::kScalar:
        c_dims = {1};
        break;
      case GemmCShapeKind::k1D:
        c_dims = {params.n};
        break;
      case GemmCShapeKind::k2D_1xN:
        c_dims = {1, params.n};
        break;
      case GemmCShapeKind::k2D_MxN:
        c_dims = {params.m, params.n};
        break;
    }
    ASSIGN_OR_RETURN_NULLOPT(
        c_desc, OperandDescriptor::Create(context_properties, params.data_type,
                                          c_dims, ""));
    attr.c_operand = c_desc;
  }

  ASSIGN_OR_RETURN_NULLOPT(
      auto output_desc,
      ValidateGemmAndInferOutput(context_properties, a_desc, b_desc, attr));

  return GemmDescriptors{
      .a_desc = std::move(a_desc),
      .b_desc = std::move(b_desc),
      .c_desc = std::move(c_desc),
      .output_desc = std::move(output_desc),
  };
}

struct LeakyReluDescriptors {
  // The output of leakyRelu has the same shape and data type as the input.
  OperandDescriptor input_desc;
  float alpha;
};

// Helper to set up LeakyReluDescriptors. Returns nullopt if any validation
// fails.
std::optional<LeakyReluDescriptors> SetUpLeakyReluDescriptors(
    const ContextProperties& context_properties,
    const LeakyReluParams& params) {
  std::vector<uint32_t> input_dims(params.input_dims.begin(),
                                   params.input_dims.begin() + params.rank);

  ASSIGN_OR_RETURN_NULLOPT(
      auto input_desc,
      OperandDescriptor::Create(context_properties, params.data_type,
                                input_dims, ""));

  // Replace NaN alpha with the default value so the operator is valid.
  float alpha = std::isnan(params.alpha) ? 0.01f : params.alpha;

  return LeakyReluDescriptors{
      .input_desc = std::move(input_desc),
      .alpha = alpha,
  };
}

struct PadDescriptors {
  OperandDescriptor input_desc;
  OperandDescriptor output_desc;
  std::vector<uint32_t> beginning_padding;
  std::vector<uint32_t> ending_padding;
};

// Helper to set up PadDescriptors. Returns nullopt if any validation fails.
std::optional<PadDescriptors> SetUpPadDescriptors(
    const ContextProperties& context_properties,
    const PadParams& params) {
  std::vector<uint32_t> input_dims(params.input_dims.begin(),
                                   params.input_dims.begin() + params.rank);

  ASSIGN_OR_RETURN_NULLOPT(
      auto input_desc,
      OperandDescriptor::Create(context_properties, params.data_type,
                                input_dims, ""));

  std::vector<uint32_t> beginning_padding(
      params.beginning_padding.begin(),
      params.beginning_padding.begin() + params.rank);
  std::vector<uint32_t> ending_padding(
      params.ending_padding.begin(),
      params.ending_padding.begin() + params.rank);
  PaddingMode mode = FromMojoPaddingMode(params.mode);
  // For reflection mode, padding[i] must be less than input_dims[i]. Clamp
  // the padding values to satisfy this constraint.
  if (mode == PaddingMode::kReflection) {
    for (uint32_t i = 0; i < params.rank; ++i) {
      beginning_padding[i] = beginning_padding[i] % input_dims[i];
      ending_padding[i] = ending_padding[i] % input_dims[i];
    }
  }

  ASSIGN_OR_RETURN_NULLOPT(
      auto output_desc,
      ValidatePadAndInferOutput(context_properties, input_desc,
                                beginning_padding, ending_padding, mode, ""));

  return PadDescriptors{
      .input_desc = std::move(input_desc),
      .output_desc = std::move(output_desc),
      .beginning_padding = std::move(beginning_padding),
      .ending_padding = std::move(ending_padding),
  };
}

struct Pool2dDescriptors {
  OperandDescriptor input_desc;
  OperandDescriptor output_desc;
};

// Helper to set up Pool2dDescriptors. Returns nullopt if any validation fails.
std::optional<Pool2dDescriptors> SetUpPool2dDescriptors(
    const ContextProperties& context_properties,
    Pool2dParams& params) {
  InputOperandLayout input_layout = context_properties.input_operand_layout;

#if BUILDFLAG(IS_LINUX)
  // Pool2d does not support dilation for TFLite backend:
  // https://source.chromium.org/chromium/chromium/src/+/4c1aaa2f981951e7e6f636df92fb89e48b642aa6:services/webnn/tflite/graph_builder_tflite.cc;l=7203
  // TODO(crbug.com/498987226): Remove this restriction to increase test
  // coverage.
  params.dilations = {1, 1};
#endif  // BUILDFLAG(IS_LINUX)

  std::vector<uint32_t> input_dims;
  switch (input_layout) {
    case InputOperandLayout::kNchw: {
      input_dims = {params.batch, params.channels, params.input_height,
                    params.input_width};
      break;
    }
    case InputOperandLayout::kNhwc: {
      input_dims = {params.batch, params.input_height, params.input_width,
                    params.channels};
      break;
    }
  }

  ASSIGN_OR_RETURN_NULLOPT(
      auto input_desc,
      OperandDescriptor::Create(context_properties, params.data_type,
                                input_dims, ""));

  Pool2dAttributes attr;
  attr.window_dimensions = params.window_dimensions;
  attr.padding = params.padding;
  attr.strides = params.strides;
  attr.dilations = params.dilations;
  attr.layout = input_layout;
  attr.output_shape_rounding = params.output_shape_rounding;

  ASSIGN_OR_RETURN_NULLOPT(
      auto output_desc,
      ValidatePool2dAndInferOutput(context_properties, input_desc, attr,
                                   FromMojoPool2dType(params.pool2d_kind)));

  return Pool2dDescriptors{
      .input_desc = std::move(input_desc),
      .output_desc = std::move(output_desc),
  };
}

struct ReduceDescriptors {
  OperandDescriptor input_desc;
  OperandDescriptor output_desc;
  std::vector<uint32_t> axes;
};

// Helper to set up ReduceDescriptors. Returns nullopt if any validation fails.
std::optional<ReduceDescriptors> SetUpReduceDescriptors(
    const ContextProperties& context_properties,
    ReduceParams& params) {
  std::vector<uint32_t> input_dims(params.input_dims.begin(),
                                   params.input_dims.begin() + params.rank);

  // Limit the rank of `num_axes` and remove duplicate values.
  params.num_axes = std::min(params.num_axes, params.rank);
  std::vector<uint32_t> axes;
  for (uint32_t i = 0; i < params.num_axes; ++i) {
    uint32_t axis = params.axes[i] % params.rank;
    if (!std::ranges::contains(axes, axis)) {
      axes.push_back(axis);
    }
  }

  ASSIGN_OR_RETURN_NULLOPT(
      auto input_desc,
      OperandDescriptor::Create(context_properties, params.data_type,
                                input_dims, ""));

  ASSIGN_OR_RETURN_NULLOPT(
      auto output_desc,
      ValidateReduceAndInferOutput(
          context_properties, FromMojoReduceType(params.reduce_kind),
          input_desc, "", axes, params.keep_dimensions));

  return ReduceDescriptors{
      .input_desc = std::move(input_desc),
      .output_desc = std::move(output_desc),
      .axes = std::move(axes),
  };
}

struct Resample2dDescriptors {
  OperandDescriptor input_desc;
  OperandDescriptor output_desc;
  // Set when using scales mode; nullopt when using sizes mode.
  std::optional<std::vector<float>> scales;
  std::vector<uint32_t> axes;
};

// Helper to set up Resample2dDescriptors. Returns nullopt if any validation
// fails.
std::optional<Resample2dDescriptors> SetUpResample2dDescriptors(
    const ContextProperties& context_properties,
    const Resample2dParams& params) {
  InputOperandLayout input_layout = context_properties.input_operand_layout;

  std::vector<uint32_t> input_dims;
  std::vector<uint32_t> axes;
  switch (input_layout) {
    case InputOperandLayout::kNchw: {
      input_dims = {params.batch, params.channels, params.input_height,
                    params.input_width};
      axes = {2, 3};
      break;
    }
    case InputOperandLayout::kNhwc: {
      input_dims = {params.batch, params.input_height, params.input_width,
                    params.channels};
      axes = {1, 2};
      break;
    }
  }

  ASSIGN_OR_RETURN_NULLOPT(
      auto input_desc,
      OperandDescriptor::Create(context_properties, params.data_type,
                                input_dims, ""));

  if (params.use_sizes) {
    std::vector<uint32_t> sizes = {params.output_height, params.output_width};
    ASSIGN_OR_RETURN_NULLOPT(auto output_desc,
                             ValidateResample2dAndInferOutput(
                                 context_properties, input_desc,
                                 base::span<const uint32_t>(sizes), axes, ""));

    return Resample2dDescriptors{
        .input_desc = std::move(input_desc),
        .output_desc = std::move(output_desc),
        .scales = std::nullopt,
        .axes = std::move(axes),
    };
  }

  std::vector<float> scales = {params.scale_height, params.scale_width};
  ASSIGN_OR_RETURN_NULLOPT(auto output_desc,
                           ValidateResample2dAndInferOutput(
                               context_properties, input_desc,
                               base::span<const float>(scales), axes, ""));

  return Resample2dDescriptors{
      .input_desc = std::move(input_desc),
      .output_desc = std::move(output_desc),
      .scales = std::move(scales),
      .axes = std::move(axes),
  };
}

struct SliceDescriptors {
  OperandDescriptor input_desc;
  OperandDescriptor output_desc;
  std::vector<uint32_t> starts;
  std::vector<uint32_t> sizes;
  std::vector<uint32_t> strides;
};

// Helper to set up SliceDescriptors. Returns nullopt if any validation fails.
std::optional<SliceDescriptors> SetUpSliceDescriptors(
    const ContextProperties& context_properties,
    const SliceParams& params) {
  std::vector<uint32_t> input_dims(params.input_dims.begin(),
                                   params.input_dims.begin() + params.rank);

  ASSIGN_OR_RETURN_NULLOPT(
      auto input_desc,
      OperandDescriptor::Create(context_properties, params.data_type,
                                input_dims, ""));

  // Fix up slice parameters to satisfy constraints:
  // - starts[i] < input_dims[i]
  // - sizes[i] >= 1
  // - starts[i] + sizes[i] <= input_dims[i]
  // - strides[i] >= 1 and strides[i] <= sizes[i]
  std::vector<uint32_t> starts(params.rank);
  std::vector<uint32_t> sizes(params.rank);
  std::vector<uint32_t> strides(params.rank);
  for (uint32_t i = 0; i < params.rank; ++i) {
    starts[i] = params.starts[i] % input_dims[i];
    uint32_t max_size = input_dims[i] - starts[i];
    sizes[i] = (params.sizes[i] % max_size) + 1;
    strides[i] = (params.strides[i] % sizes[i]) + 1;
  }

  SliceAttributes attributes;
  attributes.starts = starts;
  attributes.sizes = sizes;
  attributes.strides = strides;

  ASSIGN_OR_RETURN_NULLOPT(
      auto output_desc,
      ValidateSliceAndInferOutput(context_properties, input_desc, attributes));

  return SliceDescriptors{
      .input_desc = std::move(input_desc),
      .output_desc = std::move(output_desc),
      .starts = std::move(starts),
      .sizes = std::move(sizes),
      .strides = std::move(strides),
  };
}

struct SoftmaxDescriptors {
  // The output of softmax has the same shape and data type as the input.
  OperandDescriptor input_desc;
  uint32_t axis;
};

// Helper to set up SoftmaxDescriptors. Returns nullopt if any validation fails.
std::optional<SoftmaxDescriptors> SetUpSoftmaxDescriptors(
    const ContextProperties& context_properties,
    SoftmaxParams& params) {
  std::vector<uint32_t> input_dims(params.input_dims.begin(),
                                   params.input_dims.begin() + params.rank);

  params.axis %= params.rank;

  ASSIGN_OR_RETURN_NULLOPT(
      auto input_desc,
      OperandDescriptor::Create(context_properties, params.data_type,
                                input_dims, ""));

  return SoftmaxDescriptors{
      .input_desc = std::move(input_desc),
      .axis = params.axis,
  };
}

struct SplitDescriptors {
  OperandDescriptor input_desc;
  std::vector<OperandDescriptor> output_descs;
  uint32_t axis;
};

// Helper to set up SplitDescriptors. Returns nullopt if any validation fails.
std::optional<SplitDescriptors> SetUpSplitDescriptors(
    const ContextProperties& context_properties,
    SplitParams& params) {
  std::vector<uint32_t> input_dims(params.input_dims.begin(),
                                   params.input_dims.begin() + params.rank);

  params.axis = params.axis % params.rank;

  ASSIGN_OR_RETURN_NULLOPT(
      auto input_desc,
      OperandDescriptor::Create(context_properties, params.data_type,
                                input_dims, ""));

  SplitAttribute attributes;
  attributes.axis = params.axis;

  std::vector<uint32_t> split_sizes_vec;
  if (params.use_equal_splits) {
    // The axis dimension must be divisible by num_splits.
    if (input_dims[params.axis] % params.num_splits != 0) {
      params.num_splits = std::gcd(input_dims[params.axis], params.num_splits);
    }
    attributes.splits = params.num_splits;
  } else {
    // Each split size must be > 0 and they must sum to input_dims[axis].
    // The number of splits is determined by split_sizes.size(), capped by
    // axis_dim.
    uint32_t axis_dim = input_dims[params.axis];
    uint32_t num_splits =
        std::min(axis_dim, static_cast<uint32_t>(params.split_sizes.size()));

    // Distribute axis_dim into num_splits parts, each > 0, using the fuzzed
    // split_sizes as weights.
    split_sizes_vec.resize(num_splits);
    uint64_t weight_sum = 0;
    for (uint32_t i = 0; i < num_splits; ++i) {
      // Use at least 1 as the weight to avoid zero-sized splits.
      weight_sum += std::max(params.split_sizes[i], 1u);
    }
    uint32_t remaining = axis_dim;
    for (uint32_t i = 0; i < num_splits - 1; ++i) {
      uint32_t weight = std::max(params.split_sizes[i], 1u);
      uint32_t size = static_cast<uint32_t>(static_cast<uint64_t>(axis_dim) *
                                            weight / weight_sum);
      // Ensure each part is at least 1 and leaves enough for the rest.
      size = std::clamp(size, 1u, remaining - (num_splits - 1 - i));
      split_sizes_vec[i] = size;
      remaining -= size;
    }
    split_sizes_vec[num_splits - 1] = remaining;

    attributes.splits = base::span<const uint32_t>(split_sizes_vec);
  }

  ASSIGN_OR_RETURN_NULLOPT(
      auto output_descs,
      ValidateSplitAndInferOutput(context_properties, input_desc, attributes));

  return SplitDescriptors{
      .input_desc = std::move(input_desc),
      .output_descs = std::move(output_descs),
      .axis = params.axis,
  };
}

struct TransposeDescriptors {
  OperandDescriptor input_desc;
  OperandDescriptor output_desc;
  std::vector<uint32_t> permutation;
};

// Helper to set up TransposeDescriptors. Returns nullopt if any validation
// fails.
std::optional<TransposeDescriptors> SetUpTransposeDescriptors(
    const ContextProperties& context_properties,
    const TransposeParams& params) {
  std::vector<uint32_t> input_dims(params.input_dims.begin(),
                                   params.input_dims.begin() + params.rank);

  // Build a valid permutation from the raw fuzzed values by mapping them
  // into the range [0, rank) without duplicates.
  std::vector<uint32_t> permutation(params.rank);
  std::iota(permutation.begin(), permutation.end(), 0);
  // Use the fuzzed values to shuffle: for each position, swap with a position
  // determined by the fuzzed value.
  for (uint32_t i = 0; i < params.rank; ++i) {
    uint32_t j = params.permutation[i] % params.rank;
    std::swap(permutation[i], permutation[j]);
  }

  ASSIGN_OR_RETURN_NULLOPT(
      auto input_desc,
      OperandDescriptor::Create(context_properties, params.data_type,
                                input_dims, ""));
  ASSIGN_OR_RETURN_NULLOPT(
      auto output_desc, ValidateTransposeAndInferOutput(
                            context_properties, input_desc, permutation, ""));

  return TransposeDescriptors{
      .input_desc = std::move(input_desc),
      .output_desc = std::move(output_desc),
      .permutation = std::move(permutation),
  };
}

void MaybeIncreaseTestTimeouts() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (!command_line->HasSwitch(switches::kTestLauncherTimeout)) {
    command_line->AppendSwitchUTF8(switches::kTestLauncherTimeout, "600000");
  }
  if (!command_line->HasSwitch(switches::kUiTestActionMaxTimeout)) {
    command_line->AppendSwitchUTF8(switches::kUiTestActionMaxTimeout, "300000");
  }
  if (!command_line->HasSwitch(switches::kUiTestActionTimeout)) {
    command_line->AppendSwitchUTF8(switches::kUiTestActionTimeout, "200000");
  }
}

class GlobalFuzzEnvironment {
 public:
  GlobalFuzzEnvironment() {
    base::test::AllowCheckIsTestForTesting();

    // On POSIX this initializes the command line with empty args. A custom main
    // function would be needed to forward command line args on Linux.
    base::CommandLine::Init(0, nullptr);

    // Increase the test timeouts since large fuzzed graphs may need more time
    // to compile and execute.
    MaybeIncreaseTestTimeouts();
    TestTimeouts::Initialize();

    mojo::core::Init();

    // Currently only the ORT backend will call this callback when it encounters
    // an inference failure, crash the process so the fuzzer can catch the
    // error.
    auto lose_all_contexts_callback = base::BindOnce([]() {
      LOG(ERROR)
          << "Lose all WebNN contexts, likely due to an inference failure.";
      // Use abort() instead of LOG(FATAL) because on Windows LOG(FATAL)
      // triggers int3 (SIGTRAP), which the fuzzer cannot catch.
      abort();
    });
    webnn_test_environment_ = std::make_unique<WebNNTestEnvironment>(
        WebNNContextProviderImpl::WebNNStatus::kWebNNEnabled,
        std::move(lose_all_contexts_callback));

    // Also increase the runloop timeout.
    runloop_timeout_ = std::make_unique<base::test::ScopedRunLoopTimeout>(
        FROM_HERE, base::Minutes(10));
  }

  WebNNTestEnvironment& GetWebNNTestEnvironment() {
    return *webnn_test_environment_;
  }

 private:
  std::unique_ptr<WebNNTestEnvironment> webnn_test_environment_;
  std::unique_ptr<base::test::ScopedRunLoopTimeout> runloop_timeout_;
};

GlobalFuzzEnvironment& GetGlobalFuzzEnvironment() {
  static base::NoDestructor<GlobalFuzzEnvironment> instance;
  return *instance;
}

struct TensorRemoteAndHandle {
  mojo::AssociatedRemote<mojom::WebNNTensor> remote;
  blink::WebNNTensorToken handle;
};

TensorRemoteAndHandle CreateTensor(
    mojo::Remote<mojom::WebNNContext>& context_remote,
    mojom::TensorInfoPtr tensor_info) {
  mojo::AssociatedRemote<mojom::WebNNTensor> webnn_tensor_remote;

  base::test::TestFuture<mojom::CreateTensorResultPtr> create_tensor_future;
  context_remote->CreateTensor(std::move(tensor_info), mojo_base::BigBuffer(0),
                               create_tensor_future.GetCallback());
  mojom::CreateTensorResultPtr create_tensor_result =
      create_tensor_future.Take();
  CHECK(create_tensor_result->is_success());
  webnn_tensor_remote.Bind(
      std::move(create_tensor_result->get_success()->tensor_remote));

  return TensorRemoteAndHandle{
      .remote = std::move(webnn_tensor_remote),
      .handle = create_tensor_result->get_success()->tensor_handle};
}

TensorRemoteAndHandle CreateTensorWithValues(
    mojo::Remote<mojom::WebNNContext>& context_remote,
    mojom::TensorInfoPtr tensor_info,
    base::span<const uint8_t> data) {
  auto remote_and_handle = CreateTensor(context_remote, std::move(tensor_info));
  remote_and_handle.remote->WriteTensor(mojo_base::BigBuffer(data));
  return remote_and_handle;
}

void BuildAndCompute(
    mojo::Remote<mojom::WebNNContext>& context_remote,
    mojo::Remote<mojom::WebNNGraphBuilder> graph_builder_remote,
    mojom::GraphInfoPtr graph_info,
    base::flat_map<std::string, base::span<const uint8_t>> named_inputs) {
  // Create input tensors.
  std::vector<std::pair<std::string, TensorRemoteAndHandle>>
      named_input_remotes_and_handles;
  named_input_remotes_and_handles.reserve(graph_info->input_operands.size());

  for (OperandId operand_id : graph_info->input_operands) {
    const mojom::Operand& operand =
        *graph_info->operands.at(operand_id.value());
    ASSERT_TRUE(operand.name.has_value());

    auto it = named_inputs.find(*operand.name);
    ASSERT_TRUE(it != named_inputs.end());

    auto tensor_info = mojom::TensorInfo::New(
        operand.descriptor, MLTensorUsage{MLTensorUsageFlags::kWrite});
    named_input_remotes_and_handles.emplace_back(
        *operand.name, CreateTensorWithValues(
                           context_remote, std::move(tensor_info), it->second));
  }

  // Create output tensors.
  std::vector<std::pair<std::string, TensorRemoteAndHandle>>
      named_output_remotes_and_handles;
  named_output_remotes_and_handles.reserve(graph_info->output_operands.size());

  for (OperandId operand_id : graph_info->output_operands) {
    const mojom::Operand& operand =
        *graph_info->operands.at(operand_id.value());
    ASSERT_TRUE(operand.name.has_value());

    auto tensor_info = mojom::TensorInfo::New(
        operand.descriptor, MLTensorUsage{MLTensorUsageFlags::kRead});
    named_output_remotes_and_handles.emplace_back(
        *operand.name, CreateTensor(context_remote, std::move(tensor_info)));
  }

  base::test::TestFuture<
      base::expected<mojom::CreateGraphSuccessPtr, mojom::ErrorPtr>>
      create_graph_future;

  graph_builder_remote->CreateGraph(std::move(graph_info),
                                    create_graph_future.GetCallback());
  auto create_graph_result = create_graph_future.Take();
  if (!create_graph_result.has_value()) {
    return;
  }
  graph_builder_remote.reset();

  blink::WebNNGraphToken graph_token = create_graph_result.value()->graph_token;

  std::vector<std::pair<std::string, blink::WebNNTensorToken>>
      named_input_handles;
  named_input_handles.reserve(named_input_remotes_and_handles.size());
  std::ranges::transform(
      named_input_remotes_and_handles, std::back_inserter(named_input_handles),
      [](const auto& input) {
        return std::make_pair(input.first, input.second.handle);
      });

  std::vector<std::pair<std::string, blink::WebNNTensorToken>>
      named_output_handles;
  named_output_handles.reserve(named_output_remotes_and_handles.size());
  std::ranges::transform(
      named_output_remotes_and_handles,
      std::back_inserter(named_output_handles), [](const auto& output) {
        return std::make_pair(output.first, output.second.handle);
      });

  context_remote->Dispatch(graph_token, named_input_handles,
                           named_output_handles);

  // Wait for the computation to complete.
  for (auto& output : named_output_remotes_and_handles) {
    base::test::TestFuture<mojom::ReadTensorResultPtr> read_tensor_future;
    output.second.remote->ReadTensor(read_tensor_future.GetCallback());
    EXPECT_TRUE(read_tensor_future.Wait());
  }

  context_remote->DestroyGraph(graph_token);
}

}  // namespace

class WebNNGraphImplFuzzerBase : public testing::Test {
 public:
  WebNNGraphImplFuzzerBase()
      : scoped_feature_list_(
            webnn::mojom::features::kWebMachineLearningNeuralNetwork),
        context_properties_(GetContextPropertiesForTesting()) {}

  void SetUp() override;
  void TearDown() override;

  const ContextProperties& context_properties() const {
    return context_properties_;
  }

  mojo::Remote<mojom::WebNNGraphBuilder> BindNewGraphBuilderRemote();

 protected:
  virtual mojom::Device GetDeviceType() const = 0;

  base::test::ScopedFeatureList scoped_feature_list_;

  ContextProperties context_properties_;

  mojo::Remote<mojom::WebNNContextProvider> provider_remote_;
  mojo::Remote<mojom::WebNNContext> context_;
};

void WebNNGraphImplFuzzerBase::SetUp() {
#if BUILDFLAG(IS_MAC)
  if (base::mac::MacOSVersion() < 14'00'00) {
    GTEST_SKIP() << "Skipping test because WebNN is not supported on Mac OS "
                 << base::mac::MacOSVersion();
  }
#endif  // BUILDFLAG(IS_MAC)

#if defined(ADDRESS_SANITIZER)
  base::debug::AsanService::GetInstance()->Initialize();
#endif

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().BindWebNNContextProvider(
      provider_remote_.BindNewPipeAndPassReceiver(), /*is_incognito=*/false);

  base::test::TestFuture<mojom::CreateContextResultPtr> create_context_future;
  provider_remote_->CreateWebNNContext(
      mojom::CreateContextOptions::New(
          GetDeviceType(),
          mojom::CreateContextOptions::PowerPreference::kDefault),
      create_context_future.GetCallback());
  mojom::CreateContextResultPtr create_context_result =
      create_context_future.Take();
  if (create_context_result->is_success()) {
    context_.Bind(
        std::move(create_context_result->get_success()->context_remote));
    context_properties_ =
        create_context_result->get_success()->context_properties;
  } else {
    GTEST_SKIP() << "Failed to create WebNN context: "
                 << create_context_result->get_error()->message;
  }
}

void WebNNGraphImplFuzzerBase::TearDown() {
  // Give WebNNContext a chance to run disconnect.
  context_.reset();
  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

mojo::Remote<mojom::WebNNGraphBuilder>
WebNNGraphImplFuzzerBase::BindNewGraphBuilderRemote() {
  mojo::Remote<mojom::WebNNGraphBuilder> remote;
  context_->CreateGraphBuilder(remote.BindNewPipeAndPassReceiver());
  return remote;
}

template <typename BaseFixture>
class WebNNGraphImplFuzzerImpl
    : public fuzztest::PerFuzzTestFixtureAdapter<BaseFixture> {
 public:
  void Activation(ActivationParams params, uint8_t seed_for_data);
  void BatchNormalization(BatchNormalizationParams params,
                          uint8_t seed_for_data);
  void Clamp(ClampParams params, uint8_t seed_for_data);
  void Concat(ConcatParams params, uint8_t seed_for_data);
  void Conv2d(Conv2dParams params, uint8_t seed_for_data);
  void DequantizeLinear(DequantizeLinearParams params,
                        uint8_t seed_for_input,
                        float seed_for_scale,
                        uint8_t seed_for_zero_point);
  void ElementWiseBinary(ElementWiseBinaryParams params, uint8_t seed_for_data);
  void Elu(EluParams params, uint8_t seed_for_data);
  void Expand(ExpandParams params, uint8_t seed_for_data);
  void Gather(GatherParams params, uint8_t seed_for_data);
  void GatherND(GatherNDParams params, uint8_t seed_for_data);
  void Gemm(GemmParams params, uint8_t seed_for_data);
  void HardSigmoid(HardSigmoidParams params, uint8_t seed_for_data);
  void InstanceNormalization(InstanceNormalizationParams params,
                             uint8_t seed_for_data);
  void LayerNormalization(LayerNormalizationParams params,
                          uint8_t seed_for_data);
  void LeakyRelu(LeakyReluParams params, uint8_t seed_for_data);
  void Linear(LinearParams params, uint8_t seed_for_data);
  void Lstm(LstmParams params, uint8_t seed_for_data);
  void LstmCell(LstmCellParams params, uint8_t seed_for_data);
  void Matmul(MatmulParams params, uint8_t seed_for_data);
  void Pad(PadParams params, uint8_t seed_for_data);
  void Pool2d(Pool2dParams params, uint8_t seed_for_data);
  void Prelu(PreluParams params, uint8_t seed_for_data);
  void QuantizeLinear(QuantizeLinearParams params,
                      float seed_for_input,
                      float seed_for_scale,
                      uint8_t seed_for_zero_point);
  void Reduce(ReduceParams params, uint8_t seed_for_data);
  void Resample2d(Resample2dParams params, uint8_t seed_for_data);
  void ScatterElements(ScatterElementsParams params, uint8_t seed_for_data);
  void Slice(SliceParams params, uint8_t seed_for_data);
  void Softmax(SoftmaxParams params, uint8_t seed_for_data);
  void Split(SplitParams params, uint8_t seed_for_data);
  void Transpose(TransposeParams params, uint8_t seed_for_data);
  void Triangular(TriangularParams params, uint8_t seed_for_data);
  void DQActivationQ(ActivationParams activation_params,
                     OperandDataType quantized_type,
                     uint8_t seed_for_input,
                     float seed_for_scale,
                     uint8_t seed_for_zero_point);
  void DQClampQ(ClampParams clamp_params,
                QuantizationParams quantization_params,
                uint32_t channel_axis,
                uint8_t seed_for_input,
                float seed_for_scale,
                uint8_t seed_for_zero_point);
  void DQConcatQ(ConcatParams concat_params,
                 OperandDataType quantized_type,
                 uint8_t seed_for_input,
                 float seed_for_scale,
                 uint8_t seed_for_zero_point);
  void DQConv2dQ(Conv2dParams conv2d_params,
                 QuantizationParams quantization_params,
                 uint8_t seed_for_data);
  void DQElementWiseBinaryQ(ElementWiseBinaryParams params,
                            OperandDataType quantized_type,
                            uint8_t seed_for_input,
                            float seed_for_scale,
                            uint8_t seed_for_zero_point);
  void DQEluQ(EluParams elu_params,
              uint8_t seed_for_input,
              float seed_for_scale,
              uint8_t seed_for_zero_point);
  void DQGatherQ(GatherParams gather_params,
                 QuantizationParams quantization_params,
                 uint32_t channel_axis,
                 uint8_t seed_for_input,
                 float seed_for_scale,
                 uint8_t seed_for_zero_point);
  void DQGemmQ(GemmParams gemm_params,
               QuantizationParams quantization_params,
               uint8_t seed_for_data);
  void DQLeakyReluQ(LeakyReluParams leaky_relu_params,
                    OperandDataType quantized_type,
                    uint8_t seed_for_input,
                    float seed_for_scale,
                    uint8_t seed_for_zero_point);
  void DQPadQ(PadParams pad_params,
              OperandDataType quantized_type,
              uint8_t seed_for_input,
              float seed_for_scale,
              uint8_t seed_for_zero_point);
  void DQPool2dQ(Pool2dParams pool2d_params,
                 QuantizationParams quantization_params,
                 uint8_t seed_for_data);
  void DQReduceQ(ReduceParams reduce_params,
                 QuantizationParams quantization_params,
                 uint32_t channel_axis,
                 uint8_t seed_for_input,
                 float seed_for_scale,
                 uint8_t seed_for_zero_point);
  void DQResample2dQ(Resample2dParams resample2d_params,
                     OperandDataType quantized_type,
                     uint8_t seed_for_input,
                     float seed_for_scale,
                     uint8_t seed_for_zero_point);
  void DQSliceQ(SliceParams slice_params,
                OperandDataType quantized_type,
                uint8_t seed_for_input,
                float seed_for_scale,
                uint8_t seed_for_zero_point);
  void DQSoftmaxQ(SoftmaxParams softmax_params,
                  OperandDataType quantized_type,
                  uint8_t seed_for_input,
                  float seed_for_scale,
                  uint8_t seed_for_zero_point);
  void DQSplitQ(SplitParams split_params,
                OperandDataType quantized_type,
                uint8_t seed_for_input,
                float seed_for_scale,
                uint8_t seed_for_zero_point);
  void DQTransposeQ(TransposeParams transpose_params,
                    QuantizationParams quantization_params,
                    uint32_t channel_axis,
                    uint8_t seed_for_input,
                    float seed_for_scale,
                    uint8_t seed_for_zero_point);
};

template <mojom::Device device_type>
class WebNNGraphImplFuzzerDevice : public WebNNGraphImplFuzzerBase {
 protected:
  mojom::Device GetDeviceType() const override { return device_type; }
};

class CPU : public WebNNGraphImplFuzzerImpl<
                WebNNGraphImplFuzzerDevice<mojom::Device::kCpu>> {};

class GPU : public WebNNGraphImplFuzzerImpl<
                WebNNGraphImplFuzzerDevice<mojom::Device::kGpu>> {};

class NPU : public WebNNGraphImplFuzzerImpl<
                WebNNGraphImplFuzzerDevice<mojom::Device::kNpu>> {};

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Activation(ActivationParams params,
                                                       uint8_t seed_for_data) {
  std::vector<uint32_t> input_dims(params.input_dims.begin(),
                                   params.input_dims.begin() + params.rank);
  ASSIGN_OR_RETURN_VOID(auto input_desc, OperandDescriptor::Create(
                                             this->context_properties(),
                                             params.data_type, input_dims, ""));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);
  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(builder, params.is_input_constant,
                                            "input", input_desc, seed_for_data,
                                            data_buffers, named_inputs);

  // The output of the activation has the same shape and data type as the
  // input.
  OperandId output_id =
      builder.BuildOutput("output", input_desc.shape(), input_desc.data_type());

  BuildActivation(builder, params.kind, input_id, output_id);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::BatchNormalization(
    BatchNormalizationParams params,
    uint8_t seed_for_data) {
  std::vector<uint32_t> input_dims(params.input_dims.begin(),
                                   params.input_dims.begin() + params.rank);

  params.axis = params.axis % params.rank;
  uint32_t feature_count = input_dims[params.axis];

  ASSIGN_OR_RETURN_VOID(auto input_desc, OperandDescriptor::Create(
                                             this->context_properties(),
                                             params.data_type, input_dims, ""));
  ASSIGN_OR_RETURN_VOID(
      auto mean_desc,
      OperandDescriptor::Create(this->context_properties(), params.data_type,
                                {feature_count}, ""));
  ASSIGN_OR_RETURN_VOID(
      auto variance_desc,
      OperandDescriptor::Create(this->context_properties(), params.data_type,
                                {feature_count}, ""));

  std::optional<OperandDescriptor> scale_desc;
  std::optional<OperandDescriptor> bias_desc;
  if (params.scale_kind != OptionalOperandKind::kNone) {
    ASSIGN_OR_RETURN_VOID(
        scale_desc,
        OperandDescriptor::Create(this->context_properties(), params.data_type,
                                  {feature_count}, ""));
  }
  if (params.bias_kind != OptionalOperandKind::kNone) {
    ASSIGN_OR_RETURN_VOID(
        bias_desc,
        OperandDescriptor::Create(this->context_properties(), params.data_type,
                                  {feature_count}, ""));
  }

  BatchNormalizationAttributes attributes;
  attributes.scale = scale_desc;
  attributes.bias = bias_desc;
  attributes.axis = params.axis;

  ASSIGN_OR_RETURN_VOID(auto output_desc,
                        ValidateBatchNormalizationAndInferOutput(
                            this->context_properties(), input_desc, mean_desc,
                            variance_desc, attributes));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  data_buffers.reserve(5);
  OperandId input_id = BuildInputOrConstant(builder, params.is_input_constant,
                                            "input", input_desc, seed_for_data,
                                            data_buffers, named_inputs);
  OperandId mean_id =
      BuildInputOrConstant(builder, params.is_mean_constant, "mean", mean_desc,
                           seed_for_data, data_buffers, named_inputs);
  OperandId variance_id = BuildInputOrConstant(
      builder, params.is_variance_constant, "variance", variance_desc,
      seed_for_data, data_buffers, named_inputs);
  std::optional<OperandId> scale_id =
      BuildOptionalOperand(builder, scale_desc, params.scale_kind, "scale",
                           seed_for_data, data_buffers, named_inputs);
  std::optional<OperandId> bias_id =
      BuildOptionalOperand(builder, bias_desc, params.bias_kind, "bias",
                           seed_for_data, data_buffers, named_inputs);

  OperandId output_id = builder.BuildOutput("output", output_desc.shape(),
                                            output_desc.data_type());

  BuildBatchNormalizationAttributes batch_normalization_attributes{
      .scale_operand_id = scale_id,
      .bias_operand_id = bias_id,
      .axis = params.axis,
      .epsilon = params.epsilon,
  };

  builder.BuildBatchNormalization(input_id, mean_id, variance_id, output_id,
                                  batch_normalization_attributes);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Clamp(ClampParams params,
                                                  uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto clamp_descs,
      SetUpClampDescriptors(this->context_properties(), params));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(
      builder, params.is_input_constant, "input", clamp_descs.input_desc,
      seed_for_data, data_buffers, named_inputs);

  // The output of clamp has the same shape and data type as the input.
  OperandId output_id =
      builder.BuildOutput("output", clamp_descs.input_desc.shape(),
                          clamp_descs.input_desc.data_type());

  builder.BuildClamp(input_id, output_id, clamp_descs.min_value,
                     clamp_descs.max_value);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Concat(ConcatParams params,
                                                   uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto concat_descs,
      SetUpConcatDescriptors(this->context_properties(), params));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  const size_t input_num = concat_descs.input_descs.size();
  std::vector<OperandId> input_ids;
  input_ids.reserve(input_num);
  std::vector<std::vector<uint8_t>> input_data_buffers;
  input_data_buffers.reserve(input_num);
  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;

  for (size_t i = 0; i < input_num; ++i) {
    const auto& desc = concat_descs.input_descs[i];
    OperandId input_id = BuildInputOrConstant(
        builder, params.is_input_constant, "input" + base::NumberToString(i),
        desc, seed_for_data, input_data_buffers, named_inputs);
    input_ids.push_back(input_id);
  }

  OperandId output_id =
      builder.BuildOutput("output", concat_descs.output_desc.shape(),
                          concat_descs.output_desc.data_type());

  builder.BuildConcat(std::move(input_ids), output_id, concat_descs.axis);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Conv2d(Conv2dParams params,
                                                   uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto conv2d_descs,
      SetUpConv2dDescriptors(this->context_properties(), params));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(
      builder, params.is_input_constant, "input", conv2d_descs.input_desc,
      seed_for_data, data_buffers, named_inputs);
  OperandId filter_id = BuildInputOrConstant(
      builder, params.is_filter_constant, "filter", conv2d_descs.filter_desc,
      seed_for_data, data_buffers, named_inputs);
  std::optional<OperandId> bias_id =
      BuildOptionalOperand(builder, conv2d_descs.bias_desc, params.bias_kind,
                           "bias", seed_for_data, data_buffers, named_inputs);

  BuildConv2dAttributes conv2d_attr;
  conv2d_attr.padding = {
      params.padding.beginning.height, params.padding.ending.height,
      params.padding.beginning.width, params.padding.ending.width};
  conv2d_attr.strides = {params.strides.height, params.strides.width};
  conv2d_attr.dilations = {params.dilations.height, params.dilations.width};
  conv2d_attr.groups = params.groups;

  if (params.activation_kind != Conv2dActivationKind::kNone) {
    OperandId conv2d_output_id = builder.BuildIntermediateOperand(
        conv2d_descs.output_desc.shape(), conv2d_descs.output_desc.data_type());
    builder.BuildConv2d(params.conv2d_kind, input_id, filter_id,
                        conv2d_output_id, conv2d_attr, bias_id);

    OperandId output_id =
        builder.BuildOutput("output", conv2d_descs.output_desc.shape(),
                            conv2d_descs.output_desc.data_type());
    switch (params.activation_kind) {
      case Conv2dActivationKind::kNone:
        NOTREACHED();
      case Conv2dActivationKind::kRelu:
        builder.BuildRelu(conv2d_output_id, output_id);
        break;
      case Conv2dActivationKind::kRelu6:
        builder.BuildClamp(conv2d_output_id, output_id, /*min_value=*/0.0f,
                           /*max_value=*/6.0f);
        break;
      case Conv2dActivationKind::kReluN1To1:
        builder.BuildClamp(conv2d_output_id, output_id, /*min_value=*/-1.0f,
                           /*max_value=*/1.0f);
        break;
      case Conv2dActivationKind::kReluViaClamp:
        builder.BuildClamp(
            conv2d_output_id, output_id, /*min_value=*/0.0f,
            /*max_value=*/std::numeric_limits<float>::infinity());
        break;
    }
  } else {
    OperandId output_id =
        builder.BuildOutput("output", conv2d_descs.output_desc.shape(),
                            conv2d_descs.output_desc.data_type());
    builder.BuildConv2d(params.conv2d_kind, input_id, filter_id, output_id,
                        conv2d_attr, bias_id);
  }

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::DequantizeLinear(
    DequantizeLinearParams params,
    uint8_t seed_for_input,
    float seed_for_scale,
    uint8_t seed_for_zero_point) {
  std::vector<uint32_t> input_shape(params.input_dims.begin(),
                                    params.input_dims.begin() + params.rank);

  ASSIGN_OR_RETURN_VOID(
      auto input_desc,
      OperandDescriptor::Create(this->context_properties(),
                                params.input_data_type, input_shape, ""));

  // Use per-tensor quantization for scalar inputs since per-channel/per-block
  // quantization requires a non-empty shape.
  if (params.rank == 0) {
    params.quantization_kind = QuantizationKind::kPerTensor;
  }

  if (params.quantization_kind != QuantizationKind::kPerTensor) {
    params.channel_axis = params.channel_axis % params.rank;
  }

  QuantizationParams quantization_params{
      .quantized_type = params.input_data_type,
      .quantization_kind = params.quantization_kind,
      .channel_block_size = params.channel_block_size};

  auto scale_shape = ComputeQuantizationScaleShape(
      input_shape, quantization_params, params.channel_axis);

  ASSIGN_OR_RETURN_VOID(
      auto scale_desc,
      OperandDescriptor::Create(this->context_properties(),
                                params.scale_data_type, scale_shape, ""));
  ASSIGN_OR_RETURN_VOID(
      auto zero_point_desc,
      OperandDescriptor::Create(this->context_properties(),
                                params.input_data_type, scale_shape, ""));

  ASSIGN_OR_RETURN_VOID(auto output_desc,
                        ValidateDequantizeLinearAndInferOutput(
                            this->context_properties(), input_desc, scale_desc,
                            zero_point_desc, ""));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(builder, params.is_input_constant,
                                            "input", input_desc, seed_for_input,
                                            data_buffers, named_inputs);
  OperandId scale_id = BuildInputOrConstant(builder, params.is_scale_constant,
                                            "scale", scale_desc, seed_for_scale,
                                            data_buffers, named_inputs);
  OperandId zero_point_id = BuildInputOrConstant(
      builder, params.is_zero_point_constant, "zero_point", zero_point_desc,
      seed_for_zero_point, data_buffers, named_inputs);

  OperandId output_id = builder.BuildOutput("output", output_desc.shape(),
                                            output_desc.data_type());

  builder.BuildDequantizeLinear(input_id, scale_id, zero_point_id, output_id);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::ElementWiseBinary(
    ElementWiseBinaryParams params,
    uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(auto descs, SetUpElementWiseBinaryDescriptors(
                                        this->context_properties(), params));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId lhs_id = BuildInputOrConstant(builder, params.is_lhs_constant,
                                          "lhs", descs.lhs_desc, seed_for_data,
                                          data_buffers, named_inputs);
  OperandId rhs_id = BuildInputOrConstant(builder, params.is_rhs_constant,
                                          "rhs", descs.rhs_desc, seed_for_data,
                                          data_buffers, named_inputs);

  OperandId output_id = builder.BuildOutput("output", descs.output_desc.shape(),
                                            descs.output_desc.data_type());

  builder.BuildElementWiseBinary(params.kind, lhs_id, rhs_id, output_id);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Elu(EluParams params,
                                                uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto elu_descs, SetUpEluDescriptors(this->context_properties(), params));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(
      builder, params.is_input_constant, "input", elu_descs.input_desc,
      seed_for_data, data_buffers, named_inputs);

  // The output of elu has the same shape and data type as the input.
  OperandId output_id = builder.BuildOutput(
      "output", elu_descs.input_desc.shape(), elu_descs.input_desc.data_type());

  builder.BuildElu(input_id, output_id, elu_descs.alpha);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Expand(ExpandParams params,
                                                   uint8_t seed_for_data) {
  // Ensure output_rank >= input_rank for unidirectional broadcast.
  if (params.output_rank < params.input_rank) {
    params.output_rank = params.input_rank;
  }

  std::vector<uint32_t> input_dims(
      params.input_dims.begin(), params.input_dims.begin() + params.input_rank);
  std::vector<uint32_t> output_dims(
      params.output_dims.begin(),
      params.output_dims.begin() + params.output_rank);

  // Fix up output dims to be broadcastable from input dims.
  for (size_t i = 0; i < params.input_rank; ++i) {
    size_t input_idx = params.input_rank - 1 - i;
    size_t output_idx = params.output_rank - 1 - i;
    if (input_dims[input_idx] != 1) {
      output_dims[output_idx] = input_dims[input_idx];
    }
  }

  ASSIGN_OR_RETURN_VOID(auto input_desc, OperandDescriptor::Create(
                                             this->context_properties(),
                                             params.data_type, input_dims, ""));
  ASSIGN_OR_RETURN_VOID(auto output_desc, ValidateExpandAndInferOutput(
                                              this->context_properties(),
                                              input_desc, output_dims, ""));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(builder, params.is_input_constant,
                                            "input", input_desc, seed_for_data,
                                            data_buffers, named_inputs);

  OperandId output_id = builder.BuildOutput("output", output_desc.shape(),
                                            output_desc.data_type());

  builder.BuildExpand(input_id, output_id);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Gather(GatherParams params,
                                                   uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto gather_descs,
      SetUpGatherDescriptors(this->context_properties(), params));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(
      builder, params.is_input_constant, "input", gather_descs.input_desc,
      seed_for_data, data_buffers, named_inputs);
  OperandId indices_id = BuildInputOrConstant(
      builder, params.is_indices_constant, "indices", gather_descs.indices_desc,
      CreateBufferAsIndicesType(gather_descs.indices_desc.PackedByteLength(),
                                params.indices_data_type,
                                params.indices_fill_value),
      data_buffers, named_inputs);

  OperandId output_id =
      builder.BuildOutput("output", gather_descs.output_desc.shape(),
                          gather_descs.output_desc.data_type());

  builder.BuildGather(input_id, indices_id, output_id, gather_descs.axis);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::GatherND(GatherNDParams params,
                                                     uint8_t seed_for_data) {
  std::vector<uint32_t> input_dims(
      params.input_dims.begin(), params.input_dims.begin() + params.input_rank);

  std::vector<uint32_t> indices_dims(
      params.indices_dims.begin(),
      params.indices_dims.begin() + params.indices_rank);
  // The last dimension of indices must be in [1, input_rank].
  indices_dims.back() = indices_dims.back() % params.input_rank + 1;

  ASSIGN_OR_RETURN_VOID(
      auto input_desc,
      OperandDescriptor::Create(this->context_properties(),
                                params.input_data_type, input_dims, ""));
  ASSIGN_OR_RETURN_VOID(
      auto indices_desc,
      OperandDescriptor::Create(this->context_properties(),
                                params.indices_data_type, indices_dims, ""));
  ASSIGN_OR_RETURN_VOID(auto output_desc, ValidateGatherNDAndInferOutput(
                                              this->context_properties(),
                                              input_desc, indices_desc, ""));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(builder, params.is_input_constant,
                                            "input", input_desc, seed_for_data,
                                            data_buffers, named_inputs);
  OperandId indices_id = BuildInputOrConstant(
      builder, params.is_indices_constant, "indices", indices_desc,
      CreateBufferAsIndicesType(indices_desc.PackedByteLength(),
                                params.indices_data_type,
                                params.indices_fill_value),
      data_buffers, named_inputs);

  OperandId output_id = builder.BuildOutput("output", output_desc.shape(),
                                            output_desc.data_type());

  builder.BuildGatherND(input_id, indices_id, output_id);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Gemm(GemmParams params,
                                                 uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto gemm_descs,
      SetUpGemmDescriptors(this->context_properties(), params));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId a_id = BuildInputOrConstant(builder, params.is_a_constant, "a",
                                        gemm_descs.a_desc, seed_for_data,
                                        data_buffers, named_inputs);
  OperandId b_id = BuildInputOrConstant(builder, params.is_b_constant, "b",
                                        gemm_descs.b_desc, seed_for_data,
                                        data_buffers, named_inputs);

  BuildGemmAttributes gemm_attr;
  gemm_attr.alpha = params.alpha;
  gemm_attr.beta = params.beta;
  gemm_attr.a_transpose = params.a_transpose;
  gemm_attr.b_transpose = params.b_transpose;

  if (params.has_c) {
    gemm_attr.c_operand_id = BuildInputOrConstant(
        builder, params.is_c_constant, "c", *gemm_descs.c_desc, seed_for_data,
        data_buffers, named_inputs);
  }

  OperandId output_id =
      builder.BuildOutput("output", gemm_descs.output_desc.shape(),
                          gemm_descs.output_desc.data_type());

  builder.BuildGemm(a_id, b_id, output_id, gemm_attr);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::HardSigmoid(
    HardSigmoidParams params,
    uint8_t seed_for_data) {
  std::vector<uint32_t> input_dims(params.input_dims.begin(),
                                   params.input_dims.begin() + params.rank);

  ASSIGN_OR_RETURN_VOID(auto input_desc, OperandDescriptor::Create(
                                             this->context_properties(),
                                             params.data_type, input_dims, ""));

  // Replace NaN alpha or beta with the default value so the operator is valid.
  float alpha = std::isnan(params.alpha) ? 0.2f : params.alpha;
  float beta = std::isnan(params.beta) ? 0.5f : params.beta;

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(builder, params.is_input_constant,
                                            "input", input_desc, seed_for_data,
                                            data_buffers, named_inputs);

  // The output of hardSigmoid has the same shape and data type as the input.
  OperandId output_id =
      builder.BuildOutput("output", input_desc.shape(), input_desc.data_type());

  builder.BuildHardSigmoid(input_id, output_id, alpha, beta);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::InstanceNormalization(
    InstanceNormalizationParams params,
    uint8_t seed_for_data) {
  InputOperandLayout input_layout =
      this->context_properties().input_operand_layout;

  std::vector<uint32_t> input_dims;
  switch (input_layout) {
    case InputOperandLayout::kNchw: {
      input_dims = {params.batch, params.channels, params.input_height,
                    params.input_width};
      break;
    }
    case InputOperandLayout::kNhwc: {
      input_dims = {params.batch, params.input_height, params.input_width,
                    params.channels};
      break;
    }
  }

  ASSIGN_OR_RETURN_VOID(auto input_desc, OperandDescriptor::Create(
                                             this->context_properties(),
                                             params.data_type, input_dims, ""));

  std::optional<OperandDescriptor> scale_desc;
  std::optional<OperandDescriptor> bias_desc;
  if (params.scale_kind != OptionalOperandKind::kNone) {
    ASSIGN_OR_RETURN_VOID(
        scale_desc,
        OperandDescriptor::Create(this->context_properties(), params.data_type,
                                  {params.channels}, ""));
  }
  if (params.bias_kind != OptionalOperandKind::kNone) {
    ASSIGN_OR_RETURN_VOID(
        bias_desc,
        OperandDescriptor::Create(this->context_properties(), params.data_type,
                                  {params.channels}, ""));
  }

  InstanceNormalizationAttributes attributes;
  attributes.scale = scale_desc;
  attributes.bias = bias_desc;
  attributes.layout = input_layout;

  ASSIGN_OR_RETURN_VOID(
      auto output_desc,
      ValidateInstanceNormalizationAndInferOutput(this->context_properties(),
                                                  input_desc, attributes));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  data_buffers.reserve(3);
  OperandId input_id = BuildInputOrConstant(builder, params.is_input_constant,
                                            "input", input_desc, seed_for_data,
                                            data_buffers, named_inputs);
  std::optional<OperandId> scale_id =
      BuildOptionalOperand(builder, scale_desc, params.scale_kind, "scale",
                           seed_for_data, data_buffers, named_inputs);
  std::optional<OperandId> bias_id =
      BuildOptionalOperand(builder, bias_desc, params.bias_kind, "bias",
                           seed_for_data, data_buffers, named_inputs);

  OperandId output_id = builder.BuildOutput("output", output_desc.shape(),
                                            output_desc.data_type());

  BuildInstanceNormalizationAttributes build_attributes{
      .scale_operand_id = scale_id,
      .bias_operand_id = bias_id,
      .epsilon = params.epsilon,
  };

  builder.BuildInstanceNormalization(input_id, output_id, build_attributes);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::LayerNormalization(
    LayerNormalizationParams params,
    uint8_t seed_for_data) {
  std::vector<uint32_t> input_dims(params.input_dims.begin(),
                                   params.input_dims.begin() + params.rank);

  // Limit the `num_axes` and remove duplicate values.
  params.num_axes = std::min(params.num_axes, params.rank);
  std::vector<uint32_t> axes;
  for (uint32_t i = 0; i < params.num_axes; ++i) {
    uint32_t axis = params.axes[i] % params.rank;
    if (!std::ranges::contains(axes, axis)) {
      axes.push_back(axis);
    }
  }

  ASSIGN_OR_RETURN_VOID(auto input_desc, OperandDescriptor::Create(
                                             this->context_properties(),
                                             params.data_type, input_dims, ""));

  // Compute the shape for scale and bias based on the normalized axes.
  std::vector<uint32_t> scale_bias_dims;
  scale_bias_dims.reserve(axes.size());
  for (uint32_t axis : axes) {
    scale_bias_dims.push_back(input_dims[axis]);
  }

  // When `axes` is empty (e.g. rank-0 input or `num_axes` == 0), there is no
  // valid scale/bias shape, so neither operand can be built. Force both kinds
  // to kNone so that `BuildOptionalOperand` below skips them instead of
  // dereferencing an absent descriptor.
  if (scale_bias_dims.empty()) {
    params.scale_kind = OptionalOperandKind::kNone;
    params.bias_kind = OptionalOperandKind::kNone;
  }

  std::optional<OperandDescriptor> scale_desc;
  if (params.scale_kind != OptionalOperandKind::kNone) {
    ASSIGN_OR_RETURN_VOID(
        scale_desc,
        OperandDescriptor::Create(this->context_properties(), params.data_type,
                                  scale_bias_dims, ""));
  }

  std::optional<OperandDescriptor> bias_desc;
  if (params.bias_kind != OptionalOperandKind::kNone) {
    ASSIGN_OR_RETURN_VOID(
        bias_desc,
        OperandDescriptor::Create(this->context_properties(), params.data_type,
                                  scale_bias_dims, ""));
  }

  LayerNormalizationAttributes attributes;
  attributes.scale = scale_desc;
  attributes.bias = bias_desc;

  ASSIGN_OR_RETURN_VOID(
      auto output_desc,
      ValidateLayerNormalizationAndInferOutput(this->context_properties(),
                                               input_desc, axes, attributes));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  data_buffers.reserve(3);
  OperandId input_id = BuildInputOrConstant(builder, params.is_input_constant,
                                            "input", input_desc, seed_for_data,
                                            data_buffers, named_inputs);
  std::optional<OperandId> scale_id =
      BuildOptionalOperand(builder, scale_desc, params.scale_kind, "scale",
                           seed_for_data, data_buffers, named_inputs);
  std::optional<OperandId> bias_id =
      BuildOptionalOperand(builder, bias_desc, params.bias_kind, "bias",
                           seed_for_data, data_buffers, named_inputs);

  OperandId output_id = builder.BuildOutput("output", output_desc.shape(),
                                            output_desc.data_type());

  BuildLayerNormalizationAttributes build_attributes{
      .scale_operand_id = scale_id,
      .bias_operand_id = bias_id,
      .axes = axes,
      .epsilon = params.epsilon,
  };

  builder.BuildLayerNormalization(input_id, output_id, build_attributes);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::LeakyRelu(LeakyReluParams params,
                                                      uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto leaky_relu_descs,
      SetUpLeakyReluDescriptors(this->context_properties(), params));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(
      builder, params.is_input_constant, "input", leaky_relu_descs.input_desc,
      seed_for_data, data_buffers, named_inputs);

  // The output of leakyRelu has the same shape and data type as the input.
  OperandId output_id =
      builder.BuildOutput("output", leaky_relu_descs.input_desc.shape(),
                          leaky_relu_descs.input_desc.data_type());

  builder.BuildLeakyRelu(input_id, output_id, leaky_relu_descs.alpha);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Linear(LinearParams params,
                                                   uint8_t seed_for_data) {
  std::vector<uint32_t> input_dims(params.input_dims.begin(),
                                   params.input_dims.begin() + params.rank);

  ASSIGN_OR_RETURN_VOID(auto input_desc, OperandDescriptor::Create(
                                             this->context_properties(),
                                             params.data_type, input_dims, ""));

  // Replace NaN alpha or beta with the default value so the operator is valid.
  float alpha = std::isnan(params.alpha) ? 1.0f : params.alpha;
  float beta = std::isnan(params.beta) ? 0.0f : params.beta;

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(builder, params.is_input_constant,
                                            "input", input_desc, seed_for_data,
                                            data_buffers, named_inputs);

  // The output of linear has the same shape and data type as the input.
  OperandId output_id =
      builder.BuildOutput("output", input_desc.shape(), input_desc.data_type());

  builder.BuildLinear(input_id, output_id, alpha, beta);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Lstm(LstmParams params,
                                                 uint8_t seed_for_data) {
  if (params.hidden_size > std::numeric_limits<uint32_t>::max() / 4) {
    return;
  }
  const uint32_t four_hidden_size = params.hidden_size * 4;
  const uint32_t direction_count =
      params.direction == mojom::RecurrentNetworkDirection::kBoth ? 2 : 1;

  // input: [steps, batch_size, input_size]
  ASSIGN_OR_RETURN_VOID(
      auto input_desc,
      OperandDescriptor::Create(
          this->context_properties(), params.data_type,
          std::vector<uint32_t>{params.steps, params.batch_size,
                                params.input_size},
          ""));

  // weight: [direction_count, 4 * hidden_size, input_size]
  ASSIGN_OR_RETURN_VOID(
      auto weight_desc,
      OperandDescriptor::Create(
          this->context_properties(), params.data_type,
          std::vector<uint32_t>{direction_count, four_hidden_size,
                                params.input_size},
          ""));

  // recurrent_weight: [direction_count, 4 * hidden_size, hidden_size]
  ASSIGN_OR_RETURN_VOID(
      auto recurrent_weight_desc,
      OperandDescriptor::Create(
          this->context_properties(), params.data_type,
          std::vector<uint32_t>{direction_count, four_hidden_size,
                                params.hidden_size},
          ""));

  LstmAttributes attributes;
  attributes.return_sequence = params.return_sequence;
  attributes.direction =
      params.direction == mojom::RecurrentNetworkDirection::kForward
          ? RecurrentNetworkDirection::kForward
      : params.direction == mojom::RecurrentNetworkDirection::kBackward
          ? RecurrentNetworkDirection::kBackward
          : RecurrentNetworkDirection::kBoth;
  attributes.activation_count = 3;

  // Optional: bias [direction_count, 4 * hidden_size]
  std::optional<OperandDescriptor> bias_desc;
  if (params.bias_kind != OptionalOperandKind::kNone) {
    ASSIGN_OR_RETURN_VOID(
        bias_desc,
        OperandDescriptor::Create(
            this->context_properties(), params.data_type,
            std::vector<uint32_t>{direction_count, four_hidden_size}, ""));
    attributes.bias = bias_desc;
  }

  // Optional: recurrent_bias [direction_count, 4 * hidden_size]
  std::optional<OperandDescriptor> recurrent_bias_desc;
  if (params.recurrent_bias_kind != OptionalOperandKind::kNone) {
    ASSIGN_OR_RETURN_VOID(
        recurrent_bias_desc,
        OperandDescriptor::Create(
            this->context_properties(), params.data_type,
            std::vector<uint32_t>{direction_count, four_hidden_size}, ""));
    attributes.recurrent_bias = recurrent_bias_desc;
  }

  // Optional: peephole_weight [direction_count, 3 * hidden_size]
  std::optional<OperandDescriptor> peephole_weight_desc;
  if (params.peephole_weight_kind != OptionalOperandKind::kNone) {
    ASSIGN_OR_RETURN_VOID(
        peephole_weight_desc,
        OperandDescriptor::Create(
            this->context_properties(), params.data_type,
            std::vector<uint32_t>{direction_count, 3 * params.hidden_size},
            ""));
    attributes.peephole_weight = peephole_weight_desc;
  }

  // Optional: initial_hidden_state [direction_count, batch_size, hidden_size]
  std::optional<OperandDescriptor> initial_hidden_state_desc;
  if (params.initial_hidden_state_kind != OptionalOperandKind::kNone) {
    ASSIGN_OR_RETURN_VOID(
        initial_hidden_state_desc,
        OperandDescriptor::Create(
            this->context_properties(), params.data_type,
            std::vector<uint32_t>{direction_count, params.batch_size,
                                  params.hidden_size},
            ""));
    attributes.initial_hidden_state = initial_hidden_state_desc;
  }

  // Optional: initial_cell_state [direction_count, batch_size, hidden_size]
  std::optional<OperandDescriptor> initial_cell_state_desc;
  if (params.initial_cell_state_kind != OptionalOperandKind::kNone) {
    ASSIGN_OR_RETURN_VOID(
        initial_cell_state_desc,
        OperandDescriptor::Create(
            this->context_properties(), params.data_type,
            std::vector<uint32_t>{direction_count, params.batch_size,
                                  params.hidden_size},
            ""));
    attributes.initial_cell_state = initial_cell_state_desc;
  }

  ASSIGN_OR_RETURN_VOID(
      auto output_descs,
      ValidateLstmAndInferOutput(this->context_properties(), input_desc,
                                 weight_desc, recurrent_weight_desc,
                                 params.steps, params.hidden_size, attributes));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  data_buffers.reserve(8);
  OperandId input_id = BuildInputOrConstant(builder, params.is_input_constant,
                                            "input", input_desc, seed_for_data,
                                            data_buffers, named_inputs);
  OperandId weight_id = BuildInputOrConstant(
      builder, params.is_weight_constant, "weight", weight_desc, seed_for_data,
      data_buffers, named_inputs);
  OperandId recurrent_weight_id = BuildInputOrConstant(
      builder, params.is_recurrent_weight_constant, "recurrent_weight",
      recurrent_weight_desc, seed_for_data, data_buffers, named_inputs);

  BuildLstmAttributes lstm_attr;
  lstm_attr.return_sequence = params.return_sequence;
  lstm_attr.direction = params.direction;
  lstm_attr.layout = params.layout;
  lstm_attr.activations.assign(params.activations.begin(),
                               params.activations.end());

  lstm_attr.bias_operand_id =
      BuildOptionalOperand(builder, bias_desc, params.bias_kind, "bias",
                           seed_for_data, data_buffers, named_inputs);
  lstm_attr.recurrent_bias_operand_id = BuildOptionalOperand(
      builder, recurrent_bias_desc, params.recurrent_bias_kind,
      "recurrent_bias", seed_for_data, data_buffers, named_inputs);
  lstm_attr.peephole_weight_operand_id = BuildOptionalOperand(
      builder, peephole_weight_desc, params.peephole_weight_kind,
      "peephole_weight", seed_for_data, data_buffers, named_inputs);
  lstm_attr.initial_hidden_state_operand_id = BuildOptionalOperand(
      builder, initial_hidden_state_desc, params.initial_hidden_state_kind,
      "initial_hidden_state", seed_for_data, data_buffers, named_inputs);
  lstm_attr.initial_cell_state_operand_id = BuildOptionalOperand(
      builder, initial_cell_state_desc, params.initial_cell_state_kind,
      "initial_cell_state", seed_for_data, data_buffers, named_inputs);

  std::vector<OperandId> output_operand_ids;
  OperandId output_hidden_state_id =
      builder.BuildOutput("output_hidden_state", output_descs[0].shape(),
                          output_descs[0].data_type());
  output_operand_ids.push_back(output_hidden_state_id);

  OperandId output_cell_state_id =
      builder.BuildOutput("output_cell_state", output_descs[1].shape(),
                          output_descs[1].data_type());
  output_operand_ids.push_back(output_cell_state_id);

  if (params.return_sequence) {
    ASSERT_EQ(output_descs.size(), 3);
    OperandId output_sequence_id =
        builder.BuildOutput("output_sequence", output_descs[2].shape(),
                            output_descs[2].data_type());
    output_operand_ids.push_back(output_sequence_id);
  }

  builder.BuildLstm(input_id, weight_id, recurrent_weight_id,
                    std::move(output_operand_ids), params.steps,
                    params.hidden_size, lstm_attr);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::LstmCell(LstmCellParams params,
                                                     uint8_t seed_for_data) {
  if (params.hidden_size > std::numeric_limits<uint32_t>::max() / 4) {
    return;
  }
  const uint32_t four_hidden_size = params.hidden_size * 4;

  // input: [batch_size, input_size]
  ASSIGN_OR_RETURN_VOID(
      auto input_desc,
      OperandDescriptor::Create(
          this->context_properties(), params.data_type,
          std::vector<uint32_t>{params.batch_size, params.input_size}, ""));

  // weight: [4 * hidden_size, input_size]
  ASSIGN_OR_RETURN_VOID(
      auto weight_desc,
      OperandDescriptor::Create(
          this->context_properties(), params.data_type,
          std::vector<uint32_t>{four_hidden_size, params.input_size}, ""));

  // recurrent_weight: [4 * hidden_size, hidden_size]
  ASSIGN_OR_RETURN_VOID(
      auto recurrent_weight_desc,
      OperandDescriptor::Create(
          this->context_properties(), params.data_type,
          std::vector<uint32_t>{four_hidden_size, params.hidden_size}, ""));

  // hidden_state: [batch_size, hidden_size]
  ASSIGN_OR_RETURN_VOID(
      auto hidden_state_desc,
      OperandDescriptor::Create(
          this->context_properties(), params.data_type,
          std::vector<uint32_t>{params.batch_size, params.hidden_size}, ""));

  // cell_state: [batch_size, hidden_size]
  ASSIGN_OR_RETURN_VOID(
      auto cell_state_desc,
      OperandDescriptor::Create(
          this->context_properties(), params.data_type,
          std::vector<uint32_t>{params.batch_size, params.hidden_size}, ""));

  LstmCellAttributes attributes;
  attributes.activation_count = 3;

  // Optional: bias [4 * hidden_size]
  std::optional<OperandDescriptor> bias_desc;
  if (params.bias_kind != OptionalOperandKind::kNone) {
    ASSIGN_OR_RETURN_VOID(
        bias_desc,
        OperandDescriptor::Create(this->context_properties(), params.data_type,
                                  std::vector<uint32_t>{four_hidden_size}, ""));
    attributes.bias = bias_desc;
  }

  // Optional: recurrent_bias [4 * hidden_size]
  std::optional<OperandDescriptor> recurrent_bias_desc;
  if (params.recurrent_bias_kind != OptionalOperandKind::kNone) {
    ASSIGN_OR_RETURN_VOID(
        recurrent_bias_desc,
        OperandDescriptor::Create(this->context_properties(), params.data_type,
                                  std::vector<uint32_t>{four_hidden_size}, ""));
    attributes.recurrent_bias = recurrent_bias_desc;
  }

  // Optional: peephole_weight [3 * hidden_size]
  std::optional<OperandDescriptor> peephole_weight_desc;
  if (params.peephole_weight_kind != OptionalOperandKind::kNone) {
    ASSIGN_OR_RETURN_VOID(
        peephole_weight_desc,
        OperandDescriptor::Create(this->context_properties(), params.data_type,
                                  std::vector<uint32_t>{3 * params.hidden_size},
                                  ""));
    attributes.peephole_weight = peephole_weight_desc;
  }

  ASSIGN_OR_RETURN_VOID(auto output_descs,
                        ValidateLstmCellAndInferOutput(
                            this->context_properties(), input_desc, weight_desc,
                            recurrent_weight_desc, hidden_state_desc,
                            cell_state_desc, params.hidden_size, attributes));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  data_buffers.reserve(8);
  OperandId input_id = BuildInputOrConstant(builder, params.is_input_constant,
                                            "input", input_desc, seed_for_data,
                                            data_buffers, named_inputs);
  OperandId weight_id = BuildInputOrConstant(
      builder, params.is_weight_constant, "weight", weight_desc, seed_for_data,
      data_buffers, named_inputs);
  OperandId recurrent_weight_id = BuildInputOrConstant(
      builder, params.is_recurrent_weight_constant, "recurrent_weight",
      recurrent_weight_desc, seed_for_data, data_buffers, named_inputs);
  OperandId hidden_state_id = BuildInputOrConstant(
      builder, params.is_hidden_state_constant, "hidden_state",
      hidden_state_desc, seed_for_data, data_buffers, named_inputs);
  OperandId cell_state_id = BuildInputOrConstant(
      builder, params.is_cell_state_constant, "cell_state", cell_state_desc,
      seed_for_data, data_buffers, named_inputs);

  BuildLstmCellAttributes lstm_cell_attr;
  lstm_cell_attr.layout = params.layout;
  lstm_cell_attr.activations.assign(params.activations.begin(),
                                    params.activations.end());

  lstm_cell_attr.bias_operand_id =
      BuildOptionalOperand(builder, bias_desc, params.bias_kind, "bias",
                           seed_for_data, data_buffers, named_inputs);
  lstm_cell_attr.recurrent_bias_operand_id = BuildOptionalOperand(
      builder, recurrent_bias_desc, params.recurrent_bias_kind,
      "recurrent_bias", seed_for_data, data_buffers, named_inputs);
  lstm_cell_attr.peephole_weight_operand_id = BuildOptionalOperand(
      builder, peephole_weight_desc, params.peephole_weight_kind,
      "peephole_weight", seed_for_data, data_buffers, named_inputs);

  std::vector<OperandId> output_operand_ids;
  ASSERT_EQ(output_descs.size(), 2u);
  OperandId output_hidden_state_id =
      builder.BuildOutput("output_hidden_state", output_descs[0].shape(),
                          output_descs[0].data_type());
  output_operand_ids.push_back(output_hidden_state_id);

  OperandId output_cell_state_id =
      builder.BuildOutput("output_cell_state", output_descs[1].shape(),
                          output_descs[1].data_type());
  output_operand_ids.push_back(output_cell_state_id);

  builder.BuildLstmCell(
      input_id, weight_id, recurrent_weight_id, hidden_state_id, cell_state_id,
      std::move(output_operand_ids), params.hidden_size, lstm_cell_attr);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Matmul(MatmulParams params,
                                                   uint8_t seed_for_data) {
  std::vector<uint32_t> a_dims(params.a_dims.begin(),
                               params.a_dims.begin() + params.a_rank);
  std::vector<uint32_t> b_dims(params.b_dims.begin(),
                               params.b_dims.begin() + params.b_rank);

  // Matmul requires the last dimension of a to match the second-to-last
  // dimension of b (i.e., a's columns == b's rows for the matrix multiply).
  // Fix up b to satisfy this constraint.
  b_dims[params.b_rank - 2] = a_dims[params.a_rank - 1];

  // Fix up batch dimensions to ensure broadcast compatibility. For each aligned
  // batch dimension pair (from the right, skipping the last 2 dims), if they're
  // not equal and neither is 1, make b match a.
  size_t a_batch_rank = params.a_rank - 2;
  size_t b_batch_rank = params.b_rank - 2;
  size_t min_batch_rank = std::min(a_batch_rank, b_batch_rank);
  for (size_t i = 0; i < min_batch_rank; ++i) {
    size_t a_idx = a_batch_rank - 1 - i;
    size_t b_idx = b_batch_rank - 1 - i;
    if (a_dims[a_idx] != b_dims[b_idx] && a_dims[a_idx] != 1 &&
        b_dims[b_idx] != 1) {
      b_dims[b_idx] = a_dims[a_idx];
    }
  }

  ASSIGN_OR_RETURN_VOID(
      auto a_desc, OperandDescriptor::Create(this->context_properties(),
                                             params.data_type, a_dims, ""));
  ASSIGN_OR_RETURN_VOID(
      auto b_desc, OperandDescriptor::Create(this->context_properties(),
                                             params.data_type, b_dims, ""));

  ASSIGN_OR_RETURN_VOID(auto output_desc,
                        ValidateMatmulAndInferOutput(this->context_properties(),
                                                     a_desc, b_desc, ""));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId a_id =
      BuildInputOrConstant(builder, params.is_a_constant, "a", a_desc,
                           seed_for_data, data_buffers, named_inputs);
  OperandId b_id =
      BuildInputOrConstant(builder, params.is_b_constant, "b", b_desc,
                           seed_for_data, data_buffers, named_inputs);

  OperandId output_id = builder.BuildOutput("output", output_desc.shape(),
                                            output_desc.data_type());

  builder.BuildMatmul(a_id, b_id, output_id);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Pad(PadParams params,
                                                uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto pad_descs, SetUpPadDescriptors(this->context_properties(), params));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(
      builder, params.is_input_constant, "input", pad_descs.input_desc,
      seed_for_data, data_buffers, named_inputs);

  OperandId output_id =
      builder.BuildOutput("output", pad_descs.output_desc.shape(),
                          pad_descs.output_desc.data_type());

  builder.BuildPad(input_id, output_id, pad_descs.beginning_padding,
                   pad_descs.ending_padding, params.mode, params.value);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Pool2d(Pool2dParams params,
                                                   uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto pool2d_descs,
      SetUpPool2dDescriptors(this->context_properties(), params));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(
      builder, params.is_input_constant, "input", pool2d_descs.input_desc,
      seed_for_data, data_buffers, named_inputs);

  OperandId output_id =
      builder.BuildOutput("output", pool2d_descs.output_desc.shape(),
                          pool2d_descs.output_desc.data_type());

  BuildPool2dAttributes pool2d_attr;
  pool2d_attr.window_dimensions = {params.window_dimensions.height,
                                   params.window_dimensions.width};
  pool2d_attr.padding = {
      params.padding.beginning.height, params.padding.ending.height,
      params.padding.beginning.width, params.padding.ending.width};
  pool2d_attr.strides = {params.strides.height, params.strides.width};
  pool2d_attr.dilations = {params.dilations.height, params.dilations.width};
  builder.BuildPool2d(params.pool2d_kind, input_id, output_id, pool2d_attr);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Prelu(PreluParams params,
                                                  uint8_t seed_for_data) {
  std::vector<uint32_t> input_dims(params.input_dims.begin(),
                                   params.input_dims.begin() + params.rank);
  std::vector<uint32_t> slope_dims(
      params.slope_dims.begin(), params.slope_dims.begin() + params.slope_rank);

  // Fix up slope dims to be bidirectionally broadcastable with input dims.
  // The current implementation only supports unidirectional broadcasting, so
  // the bidirectional-only cases are filtered out by validation
  // (crbug.com/387892103).
  for (size_t i = 0; i < slope_dims.size() && i < input_dims.size(); ++i) {
    size_t slope_idx = slope_dims.size() - 1 - i;
    size_t input_idx = input_dims.size() - 1 - i;
    if (slope_dims[slope_idx] != input_dims[input_idx] &&
        slope_dims[slope_idx] != 1 && input_dims[input_idx] != 1) {
      slope_dims[slope_idx] = input_dims[input_idx];
    }
  }

  ASSIGN_OR_RETURN_VOID(auto input_desc, OperandDescriptor::Create(
                                             this->context_properties(),
                                             params.data_type, input_dims, ""));
  ASSIGN_OR_RETURN_VOID(auto slope_desc, OperandDescriptor::Create(
                                             this->context_properties(),
                                             params.data_type, slope_dims, ""));
  ASSIGN_OR_RETURN_VOID(auto output_desc, ValidatePreluAndInferOutput(
                                              this->context_properties(),
                                              input_desc, slope_desc, ""));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(builder, params.is_input_constant,
                                            "input", input_desc, seed_for_data,
                                            data_buffers, named_inputs);
  OperandId slope_id = BuildInputOrConstant(builder, params.is_slope_constant,
                                            "slope", slope_desc, seed_for_data,
                                            data_buffers, named_inputs);

  OperandId output_id = builder.BuildOutput("output", output_desc.shape(),
                                            output_desc.data_type());

  builder.BuildPrelu(input_id, slope_id, output_id);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::QuantizeLinear(
    QuantizeLinearParams params,
    float seed_for_input,
    float seed_for_scale,
    uint8_t seed_for_zero_point) {
  std::vector<uint32_t> input_shape(params.input_dims.begin(),
                                    params.input_dims.begin() + params.rank);

  ASSIGN_OR_RETURN_VOID(
      auto input_desc,
      OperandDescriptor::Create(this->context_properties(),
                                params.input_data_type, input_shape, ""));

  // Use per-tensor quantization for scalar inputs since per-channel/per-block
  // quantization requires a non-empty shape.
  if (params.rank == 0) {
    params.quantization_kind = QuantizationKind::kPerTensor;
  }

  if (params.quantization_kind != QuantizationKind::kPerTensor) {
    params.channel_axis = params.channel_axis % params.rank;
  }

  QuantizationParams quantization_params{
      .quantized_type = params.zero_point_data_type,
      .quantization_kind = params.quantization_kind,
      .channel_block_size = params.channel_block_size};

  auto scale_shape = ComputeQuantizationScaleShape(
      input_shape, quantization_params, params.channel_axis);

  ASSIGN_OR_RETURN_VOID(
      auto scale_desc,
      OperandDescriptor::Create(this->context_properties(),
                                params.input_data_type, scale_shape, ""));
  ASSIGN_OR_RETURN_VOID(
      auto zero_point_desc,
      OperandDescriptor::Create(this->context_properties(),
                                params.zero_point_data_type, scale_shape, ""));

  ASSIGN_OR_RETURN_VOID(auto output_desc,
                        ValidateQuantizeLinearAndInferOutput(
                            this->context_properties(), input_desc, scale_desc,
                            zero_point_desc, ""));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(builder, params.is_input_constant,
                                            "input", input_desc, seed_for_input,
                                            data_buffers, named_inputs);
  OperandId scale_id = BuildInputOrConstant(builder, params.is_scale_constant,
                                            "scale", scale_desc, seed_for_scale,
                                            data_buffers, named_inputs);
  OperandId zero_point_id = BuildInputOrConstant(
      builder, params.is_zero_point_constant, "zero_point", zero_point_desc,
      seed_for_zero_point, data_buffers, named_inputs);

  OperandId output_id = builder.BuildOutput("output", output_desc.shape(),
                                            output_desc.data_type());

  builder.BuildQuantizeLinear(input_id, scale_id, zero_point_id, output_id);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Reduce(ReduceParams params,
                                                   uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto reduce_descs,
      SetUpReduceDescriptors(this->context_properties(), params));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(
      builder, params.is_input_constant, "input", reduce_descs.input_desc,
      seed_for_data, data_buffers, named_inputs);

  OperandId output_id =
      builder.BuildOutput("output", reduce_descs.output_desc.shape(),
                          reduce_descs.output_desc.data_type());

  builder.BuildReduce(params.reduce_kind, input_id, output_id,
                      reduce_descs.axes, params.keep_dimensions);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Resample2d(Resample2dParams params,
                                                       uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto resample2d_descs,
      SetUpResample2dDescriptors(this->context_properties(), params));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(
      builder, params.is_input_constant, "input", resample2d_descs.input_desc,
      seed_for_data, data_buffers, named_inputs);

  OperandId output_id =
      builder.BuildOutput("output", resample2d_descs.output_desc.shape(),
                          resample2d_descs.output_desc.data_type());

  BuildResample2dAttributes resample2d_attr;
  resample2d_attr.mode = params.mode;
  resample2d_attr.scales = resample2d_descs.scales;
  resample2d_attr.axes = resample2d_descs.axes;
  builder.BuildResample2d(input_id, output_id, resample2d_attr);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::ScatterElements(
    ScatterElementsParams params,
    uint8_t seed_for_data) {
  std::vector<uint32_t> input_dims(params.input_dims.begin(),
                                   params.input_dims.begin() + params.rank);

  std::vector<uint32_t> indices_dims = input_dims;
  params.axis %= params.rank;
  indices_dims[params.axis] = params.indices_axis_dim_size;

  ASSIGN_OR_RETURN_VOID(
      auto input_desc,
      OperandDescriptor::Create(this->context_properties(),
                                params.input_data_type, input_dims, ""));
  ASSIGN_OR_RETURN_VOID(
      auto indices_desc,
      OperandDescriptor::Create(this->context_properties(),
                                params.indices_data_type, indices_dims, ""));
  ASSIGN_OR_RETURN_VOID(
      auto updates_desc,
      OperandDescriptor::Create(this->context_properties(),
                                params.input_data_type, indices_dims, ""));

  ASSIGN_OR_RETURN_VOID(auto output_desc,
                        ValidateScatterElementsAndInferOutput(
                            this->context_properties(), input_desc,
                            indices_desc, updates_desc, params.axis, ""));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);


  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(builder, params.is_input_constant,
                                            "input", input_desc, seed_for_data,
                                            data_buffers, named_inputs);
  OperandId indices_id = BuildInputOrConstant(
      builder, params.is_indices_constant, "indices", indices_desc,
      CreateBufferAsIndicesType(indices_desc.PackedByteLength(),
                                params.indices_data_type,
                                params.indices_fill_value),
      data_buffers, named_inputs);
  OperandId updates_id = BuildInputOrConstant(
      builder, params.is_updates_constant, "updates", updates_desc,
      seed_for_data, data_buffers, named_inputs);

  OperandId output_id = builder.BuildOutput("output", output_desc.shape(),
                                            output_desc.data_type());

  builder.BuildScatterElements(input_id, indices_id, updates_id, output_id,
                               params.axis);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Slice(SliceParams params,
                                                  uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto slice_descs,
      SetUpSliceDescriptors(this->context_properties(), params));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(
      builder, params.is_input_constant, "input", slice_descs.input_desc,
      seed_for_data, data_buffers, named_inputs);

  OperandId output_id =
      builder.BuildOutput("output", slice_descs.output_desc.shape(),
                          slice_descs.output_desc.data_type());

  builder.BuildSlice(input_id, output_id, slice_descs.starts, slice_descs.sizes,
                     slice_descs.strides);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Softmax(SoftmaxParams params,
                                                    uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto softmax_descs,
      SetUpSoftmaxDescriptors(this->context_properties(), params));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(
      builder, params.is_input_constant, "input", softmax_descs.input_desc,
      seed_for_data, data_buffers, named_inputs);

  // The output of softmax has the same shape and data type as the input.
  OperandId output_id =
      builder.BuildOutput("output", softmax_descs.input_desc.shape(),
                          softmax_descs.input_desc.data_type());

  builder.BuildSoftmax(input_id, output_id, softmax_descs.axis);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Split(SplitParams params,
                                                  uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto split_descs,
      SetUpSplitDescriptors(this->context_properties(), params));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(
      builder, params.is_input_constant, "input", split_descs.input_desc,
      seed_for_data, data_buffers, named_inputs);

  std::vector<OperandId> output_ids;
  output_ids.reserve(split_descs.output_descs.size());
  for (size_t i = 0; i < split_descs.output_descs.size(); ++i) {
    const auto& output_desc = split_descs.output_descs[i];
    OperandId output_id =
        builder.BuildOutput("output" + base::NumberToString(i),
                            output_desc.shape(), output_desc.data_type());
    output_ids.push_back(output_id);
  }

  builder.BuildSplit(input_id, output_ids, split_descs.axis);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Transpose(TransposeParams params,
                                                      uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto transpose_descs,
      SetUpTransposeDescriptors(this->context_properties(), params));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(
      builder, params.is_input_constant, "input", transpose_descs.input_desc,
      seed_for_data, data_buffers, named_inputs);

  OperandId output_id =
      builder.BuildOutput("output", transpose_descs.output_desc.shape(),
                          transpose_descs.output_desc.data_type());

  builder.BuildTranspose(input_id, output_id,
                         std::move(transpose_descs.permutation));

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::Triangular(TriangularParams params,
                                                       uint8_t seed_for_data) {
  std::vector<uint32_t> input_dims(params.input_dims.begin(),
                                   params.input_dims.begin() + params.rank);

  ASSIGN_OR_RETURN_VOID(auto input_desc, OperandDescriptor::Create(
                                             this->context_properties(),
                                             params.data_type, input_dims, ""));

  ASSIGN_OR_RETURN_VOID(auto output_desc,
                        ValidateTriangularAndInferOutput(
                            this->context_properties(), input_desc, ""));

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  OperandId input_id = BuildInputOrConstant(builder, params.is_input_constant,
                                            "input", input_desc, seed_for_data,
                                            data_buffers, named_inputs);

  OperandId output_id = builder.BuildOutput("output", output_desc.shape(),
                                            output_desc.data_type());

  builder.BuildTriangular(input_id, output_id, params.upper, params.diagonal);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::DQActivationQ(
    ActivationParams params,
    OperandDataType quantized_type,
    uint8_t seed_for_input,
    float seed_for_scale,
    uint8_t seed_for_zero_point) {
  std::vector<uint32_t> input_dims(params.input_dims.begin(),
                                   params.input_dims.begin() + params.rank);
  ASSIGN_OR_RETURN_VOID(auto input_desc, OperandDescriptor::Create(
                                             this->context_properties(),
                                             params.data_type, input_dims, ""));

  // kPerTensor quantization is used to exercise the fusible path for TFLite
  // backend:
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=2650;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
  // In particular sigmoid requires the output scale to be exactly 1.0f/256.0f:
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=2591;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
  // TODO(crbug.com/498987226): Remove this restriction to increase test
  // coverage.
  QuantizationParams per_tensor_quantization_params{
      .quantized_type = quantized_type,
      .quantization_kind = QuantizationKind::kPerTensor,
      .channel_block_size = 1};
  if (params.kind == ActivationKind::kSigmoid) {
    seed_for_scale = 1.0f / 256.0f;
  }

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  ASSIGN_OPTIONAL_OR_RETURN_VOID(
      auto activation_input_id,
      BuildDequantizeInput(
          builder, this->context_properties(), params.is_input_constant,
          "input", input_desc, per_tensor_quantization_params,
          /*channel_axis=*/std::nullopt, seed_for_input, seed_for_scale,
          seed_for_zero_point, data_buffers, named_inputs));

  // The output of the activation has the same shape and data type as the
  // input.
  OperandId activation_output_id = builder.BuildIntermediateOperand(
      input_desc.shape(), input_desc.data_type());

  BuildActivation(builder, params.kind, activation_input_id,
                  activation_output_id);

  if (!BuildQuantizeOutput(builder, this->context_properties(), "output",
                           input_desc, per_tensor_quantization_params,
                           /*channel_axis=*/std::nullopt, activation_output_id,
                           seed_for_scale, seed_for_zero_point)) {
    return;
  }

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::DQClampQ(
    ClampParams clamp_params,
    QuantizationParams quantization_params,
    uint32_t channel_axis,
    uint8_t seed_for_input,
    float seed_for_scale,
    uint8_t seed_for_zero_point) {
  ASSIGN_OR_RETURN_VOID(
      auto clamp_descs,
      SetUpClampDescriptors(this->context_properties(), clamp_params));

  // Use per-tensor quantization for the input when the input shape is empty
  // (scalar), since per-channel/per-block quantization requires a non-empty
  // shape. Otherwise, clamp `channel_axis` to be valid for the input
  // shape.
  if (clamp_descs.input_desc.shape().empty()) {
    quantization_params.quantization_kind = QuantizationKind::kPerTensor;
  } else {
    channel_axis = channel_axis % clamp_descs.input_desc.shape().size();
  }

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;

  ASSIGN_OPTIONAL_OR_RETURN_VOID(
      auto clamp_input_id,
      BuildDequantizeInput(builder, this->context_properties(),
                           clamp_params.is_input_constant, "input",
                           clamp_descs.input_desc, quantization_params,
                           channel_axis, seed_for_input, seed_for_scale,
                           seed_for_zero_point, data_buffers, named_inputs));

  // The output of clamp has the same shape and data type as the input.
  OperandId clamp_output_id = builder.BuildIntermediateOperand(
      clamp_descs.input_desc.shape(), clamp_descs.input_desc.data_type());

  builder.BuildClamp(clamp_input_id, clamp_output_id, clamp_descs.min_value,
                     clamp_descs.max_value);

  if (!BuildQuantizeOutput(builder, this->context_properties(), "output",
                           clamp_descs.input_desc, quantization_params,
                           channel_axis, clamp_output_id, seed_for_scale,
                           seed_for_zero_point)) {
    return;
  }

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::DQConcatQ(
    ConcatParams concat_params,
    OperandDataType quantized_type,
    uint8_t seed_for_input,
    float seed_for_scale,
    uint8_t seed_for_zero_point) {
  ASSIGN_OR_RETURN_VOID(
      auto concat_descs,
      SetUpConcatDescriptors(this->context_properties(), concat_params));

  // kPerTensor quantization is used to exercise the fusible path for TFLite
  // backend:
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=1845;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
  // TODO(crbug.com/498987226): Remove this restriction to increase test
  // coverage.
  QuantizationParams per_tensor_quantization_params{
      .quantized_type = quantized_type,
      .quantization_kind = QuantizationKind::kPerTensor,
      .channel_block_size = 1};

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  const size_t input_num = concat_descs.input_descs.size();
  std::vector<OperandId> concat_input_ids;
  concat_input_ids.reserve(input_num);

  for (size_t i = 0; i < input_num; ++i) {
    ASSIGN_OPTIONAL_OR_RETURN_VOID(
        auto concat_input_id,
        BuildDequantizeInput(
            builder, this->context_properties(),
            concat_params.is_input_constant, "input" + base::NumberToString(i),
            concat_descs.input_descs[i], per_tensor_quantization_params,
            /*channel_axis=*/std::nullopt, seed_for_input, seed_for_scale,
            seed_for_zero_point, data_buffers, named_inputs));
    concat_input_ids.push_back(concat_input_id);
  }

  OperandId concat_output_id = builder.BuildIntermediateOperand(
      concat_descs.output_desc.shape(), concat_descs.output_desc.data_type());

  builder.BuildConcat(std::move(concat_input_ids), concat_output_id,
                      concat_descs.axis);

  if (!BuildQuantizeOutput(builder, this->context_properties(), "output",
                           concat_descs.output_desc,
                           per_tensor_quantization_params,
                           /*channel_axis=*/std::nullopt, concat_output_id,
                           seed_for_scale, seed_for_zero_point)) {
    return;
  }

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::DQConv2dQ(
    Conv2dParams conv2d_params,
    QuantizationParams quantization_params,
    uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto conv2d_descs,
      SetUpConv2dDescriptors(this->context_properties(), conv2d_params));

  InputOperandLayout input_layout =
      this->context_properties().input_operand_layout;
  const uint32_t input_channel_axis =
      input_layout == InputOperandLayout::kNchw ? 1u : 3u;
  const uint32_t output_channel_axis = input_channel_axis;
  const uint32_t filter_channel_axis =
      (conv2d_params.conv2d_kind == mojom::Conv2d::Kind::kTransposed &&
       input_layout == InputOperandLayout::kNchw)
          ? 1u
          : 0u;
  const uint32_t bias_channel_axis = 0u;

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;

  // These scale and zero-point values are used to exercise the fusible path
  // for TFLite backend (input_scale=0.5, filter_scale=0.25, bias_scale=0.125,
  // output_scale=0.125, all zero_points=0):
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=1809;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=1754;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
  // TODO(crbug.com/498987226): Remove this restriction to increase test
  // coverage.
  ASSIGN_OPTIONAL_OR_RETURN_VOID(
      auto conv2d_input_id,
      BuildDequantizeInput(
          builder, this->context_properties(), conv2d_params.is_input_constant,
          "input", conv2d_descs.input_desc, quantization_params,
          input_channel_axis, seed_for_data, /*scale_value=*/0.5f,
          /*zero_point_value=*/0, data_buffers, named_inputs));
  ASSIGN_OPTIONAL_OR_RETURN_VOID(
      auto conv2d_filter_id,
      BuildDequantizeInput(builder, this->context_properties(),
                           conv2d_params.is_filter_constant, "filter",
                           conv2d_descs.filter_desc, quantization_params,
                           filter_channel_axis, seed_for_data,
                           /*scale_value=*/0.25f,
                           /*zero_point_value=*/0, data_buffers, named_inputs));
  std::optional<OperandId> conv2d_bias_id;
  if (conv2d_params.bias_kind != OptionalOperandKind::kNone) {
    // The bias must be quantized to int32.
    QuantizationParams bias_quantization_params = quantization_params;
    bias_quantization_params.quantized_type = OperandDataType::kInt32;
    ASSIGN_OPTIONAL_OR_RETURN_VOID(
        auto bias_id,
        BuildDequantizeInput(
            builder, this->context_properties(),
            conv2d_params.bias_kind == OptionalOperandKind::kConstant, "bias",
            *conv2d_descs.bias_desc, bias_quantization_params,
            bias_channel_axis, seed_for_data,
            /*scale_value=*/0.125f, /*zero_point_value=*/0, data_buffers,
            named_inputs));
    conv2d_bias_id = bias_id;
  }

  OperandId conv_output_id = builder.BuildIntermediateOperand(
      conv2d_descs.output_desc.shape(), conv2d_descs.output_desc.data_type());

  BuildConv2dAttributes conv2d_attr;
  conv2d_attr.padding = {conv2d_params.padding.beginning.height,
                         conv2d_params.padding.ending.height,
                         conv2d_params.padding.beginning.width,
                         conv2d_params.padding.ending.width};
  conv2d_attr.strides = {conv2d_params.strides.height,
                         conv2d_params.strides.width};
  conv2d_attr.dilations = {conv2d_params.dilations.height,
                           conv2d_params.dilations.width};
  conv2d_attr.groups = conv2d_params.groups;
  builder.BuildConv2d(conv2d_params.conv2d_kind, conv2d_input_id,
                      conv2d_filter_id, conv_output_id, conv2d_attr,
                      conv2d_bias_id);

  OperandId quantize_input_id = conv_output_id;
  if (conv2d_params.activation_kind != Conv2dActivationKind::kNone) {
    OperandId activation_output_id = builder.BuildIntermediateOperand(
        conv2d_descs.output_desc.shape(), conv2d_descs.output_desc.data_type());
    switch (conv2d_params.activation_kind) {
      case Conv2dActivationKind::kNone:
        NOTREACHED();
      case Conv2dActivationKind::kRelu:
        builder.BuildRelu(conv_output_id, activation_output_id);
        break;
      case Conv2dActivationKind::kRelu6:
        builder.BuildClamp(conv_output_id, activation_output_id,
                           /*min_value=*/0.0f, /*max_value=*/6.0f);
        break;
      case Conv2dActivationKind::kReluN1To1:
        builder.BuildClamp(conv_output_id, activation_output_id,
                           /*min_value=*/-1.0f, /*max_value=*/1.0f);
        break;
      case Conv2dActivationKind::kReluViaClamp:
        builder.BuildClamp(
            conv_output_id, activation_output_id,
            /*min_value=*/0.0f,
            /*max_value=*/std::numeric_limits<float>::infinity());
        break;
    }
    quantize_input_id = activation_output_id;
  }

  if (!BuildQuantizeOutput(builder, this->context_properties(), "output",
                           conv2d_descs.output_desc, quantization_params,
                           output_channel_axis, quantize_input_id,
                           /*scale_value=*/0.125f, /*zero_point_value=*/0)) {
    return;
  }

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::DQElementWiseBinaryQ(
    ElementWiseBinaryParams params,
    OperandDataType quantized_type,
    uint8_t seed_for_input,
    float seed_for_scale,
    uint8_t seed_for_zero_point) {
  ASSIGN_OR_RETURN_VOID(auto descs, SetUpElementWiseBinaryDescriptors(
                                        this->context_properties(), params));

  // kPerTensor quantization and the same scale/zero_point for both inputs
  // and output is used to exercise the fusible path for TFLite backend:
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=1967;drc=ce3629f6f1cdbdb670dbf759e6b7c89c4a92a8fb
  // TODO(crbug.com/498987226): Remove this restriction to increase test
  // coverage.
  QuantizationParams per_tensor_quantization_params{
      .quantized_type = quantized_type,
      .quantization_kind = QuantizationKind::kPerTensor,
      .channel_block_size = 1};

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;

  ASSIGN_OPTIONAL_OR_RETURN_VOID(
      auto binary_lhs_id,
      BuildDequantizeInput(
          builder, this->context_properties(), params.is_lhs_constant, "lhs",
          descs.lhs_desc, per_tensor_quantization_params,
          /*channel_axis=*/std::nullopt, seed_for_input, seed_for_scale,
          seed_for_zero_point, data_buffers, named_inputs));

  ASSIGN_OPTIONAL_OR_RETURN_VOID(
      auto binary_rhs_id,
      BuildDequantizeInput(
          builder, this->context_properties(), params.is_rhs_constant, "rhs",
          descs.rhs_desc, per_tensor_quantization_params,
          /*channel_axis=*/std::nullopt, seed_for_input, seed_for_scale,
          seed_for_zero_point, data_buffers, named_inputs));

  OperandId binary_output_id = builder.BuildIntermediateOperand(
      descs.output_desc.shape(), descs.output_desc.data_type());
  builder.BuildElementWiseBinary(params.kind, binary_lhs_id, binary_rhs_id,
                                 binary_output_id);

  if (!BuildQuantizeOutput(builder, this->context_properties(), "output",
                           descs.output_desc, per_tensor_quantization_params,
                           /*channel_axis=*/std::nullopt, binary_output_id,
                           seed_for_scale, seed_for_zero_point)) {
    return;
  }

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::DQEluQ(
    EluParams elu_params,
    uint8_t seed_for_input,
    float seed_for_scale,
    uint8_t seed_for_zero_point) {
  ASSIGN_OR_RETURN_VOID(
      auto elu_descs,
      SetUpEluDescriptors(this->context_properties(), elu_params));
  const OperandDescriptor& elu_desc = elu_descs.input_desc;

  // kPerTensor quantization with the Int8 quantized type and the specific
  // alpha value is used to exercise the fusible path for TFLite backend:
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=2021;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
  // TODO(crbug.com/498987226): Remove this restriction to increase test
  // coverage.
  elu_descs.alpha = 1.0f;
  QuantizationParams per_tensor_quantization_params{
      .quantized_type = OperandDataType::kInt8,
      .quantization_kind = QuantizationKind::kPerTensor,
      .channel_block_size = 1};

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  ASSIGN_OPTIONAL_OR_RETURN_VOID(
      auto elu_input_id,
      BuildDequantizeInput(
          builder, this->context_properties(), elu_params.is_input_constant,
          "input", elu_desc, per_tensor_quantization_params,
          /*channel_axis=*/std::nullopt, seed_for_input, seed_for_scale,
          seed_for_zero_point, data_buffers, named_inputs));

  // The output of elu has the same shape and data type as the input.
  OperandId elu_output_id =
      builder.BuildIntermediateOperand(elu_desc.shape(), elu_desc.data_type());

  builder.BuildElu(elu_input_id, elu_output_id, elu_descs.alpha);

  if (!BuildQuantizeOutput(builder, this->context_properties(), "output",
                           elu_desc, per_tensor_quantization_params,
                           /*channel_axis=*/std::nullopt, elu_output_id,
                           seed_for_scale, seed_for_zero_point)) {
    return;
  }

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::DQGatherQ(
    GatherParams gather_params,
    QuantizationParams quantization_params,
    uint32_t channel_axis,
    uint8_t seed_for_input,
    float seed_for_scale,
    uint8_t seed_for_zero_point) {
  ASSIGN_OR_RETURN_VOID(
      auto gather_descs,
      SetUpGatherDescriptors(this->context_properties(), gather_params));

  // Use the same quantization params for both input and output to exercise the
  // fusible path for TFLite backend:
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=2122;drc=ce3629f6f1cdbdb670dbf759e6b7c89c4a92a8fb
  // TODO(crbug.com/498987226): Remove this restriction to increase test
  // coverage.
  // Fall back to per-tensor when:
  //   - Output is scalar.
  //   - The quantization axis coincides with the gather axis, because Gather
  //     replaces that dimension with the indices shape, making the scale
  //     shapes incompatible.
  // Otherwise, compute the output channel axis from the input channel axis:
  //   - If input_channel_axis < gather_axis: unchanged.
  //   - If input_channel_axis > gather_axis: shifted by (indices_rank - 1)
  //     because the gather axis (1 dim) is replaced by indices_rank dims.
  uint32_t input_channel_axis = channel_axis % gather_params.input_rank;
  std::optional<uint32_t> output_channel_axis;

  if (gather_descs.output_desc.shape().empty()) {
    quantization_params.quantization_kind = QuantizationKind::kPerTensor;
  } else if (input_channel_axis == gather_descs.axis) {
    // The quantization axis is replaced by the indices shape in Gather,
    // so the scale shape would differ between input and output.
    quantization_params.quantization_kind = QuantizationKind::kPerTensor;
  } else if (input_channel_axis < gather_descs.axis) {
    output_channel_axis = input_channel_axis;
  } else {
    output_channel_axis = input_channel_axis + gather_params.indices_rank - 1;
  }

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;

  ASSIGN_OPTIONAL_OR_RETURN_VOID(
      auto gather_input_id,
      BuildDequantizeInput(builder, this->context_properties(),
                           gather_params.is_input_constant, "input",
                           gather_descs.input_desc, quantization_params,
                           input_channel_axis, seed_for_input, seed_for_scale,
                           seed_for_zero_point, data_buffers, named_inputs));

  OperandId indices_id = BuildInputOrConstant(
      builder, gather_params.is_indices_constant, "indices",
      gather_descs.indices_desc,
      CreateBufferAsIndicesType(gather_descs.indices_desc.PackedByteLength(),
                                gather_params.indices_data_type,
                                gather_params.indices_fill_value),
      data_buffers, named_inputs);

  OperandId gather_output_id = builder.BuildIntermediateOperand(
      gather_descs.output_desc.shape(), gather_descs.output_desc.data_type());

  builder.BuildGather(gather_input_id, indices_id, gather_output_id,
                      gather_descs.axis);

  // Reuse input scale/zero-point values for output since they should have the
  // same values.
  if (!BuildQuantizeOutput(builder, this->context_properties(), "output",
                           gather_descs.output_desc, quantization_params,
                           output_channel_axis, gather_output_id,
                           seed_for_scale, seed_for_zero_point)) {
    return;
  }

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::DQGemmQ(
    GemmParams gemm_params,
    QuantizationParams quantization_params,
    uint8_t seed_for_data) {
  // A(input) and output use per-tensor quantization, B(weights) and C(bias)
  // use per-channel or per-tensor quantization.
  QuantizationParams per_tensor_quantization_params = quantization_params;
  per_tensor_quantization_params.quantization_kind =
      QuantizationKind::kPerTensor;

  ASSIGN_OR_RETURN_VOID(
      auto gemm_descs,
      SetUpGemmDescriptors(this->context_properties(), gemm_params));

  const uint32_t b_channel_axis = gemm_params.b_transpose ? 0u : 1u;

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;

  // These scale and zero-point values are used to exercise the fusible path
  // for TFLite backend (a_scale=0.5, b_scale=0.25, c_scale=0.125,
  // output_scale=0.125, all zero_points=0):
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=2079;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
  // TODO(crbug.com/498987226): Remove these restrictions to increase test
  // coverage.
  ASSIGN_OPTIONAL_OR_RETURN_VOID(
      auto gemm_a_id,
      BuildDequantizeInput(
          builder, this->context_properties(), gemm_params.is_a_constant, "a",
          gemm_descs.a_desc, per_tensor_quantization_params,
          /*channel_axis=*/std::nullopt, seed_for_data, /*scale_value=*/0.5f,
          /*zero_point_value=*/0, data_buffers, named_inputs));
  ASSIGN_OPTIONAL_OR_RETURN_VOID(
      auto gemm_b_id,
      BuildDequantizeInput(builder, this->context_properties(),
                           gemm_params.is_b_constant, "b", gemm_descs.b_desc,
                           quantization_params, b_channel_axis, seed_for_data,
                           /*scale_value=*/0.25f,
                           /*zero_point_value=*/0, data_buffers, named_inputs));

  BuildGemmAttributes gemm_attr;
  gemm_attr.alpha = gemm_params.alpha;
  gemm_attr.beta = gemm_params.beta;
  gemm_attr.a_transpose = gemm_params.a_transpose;
  gemm_attr.b_transpose = gemm_params.b_transpose;

  if (gemm_params.has_c) {
    // C shape is {1}, {N}, {1, N}, or {M, N}. For 1D shapes, axis 0 is the
    // only option. For 2D shapes, quantize along the N dimension at axis 1.
    const uint32_t c_channel_axis =
        gemm_descs.c_desc->shape().size() == 1 ? 0u : 1u;

    // C uses int32 quantized type to exercise the fusible path for TFLite
    // backend:
    // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=2079;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
    // TODO(crbug.com/498987226): Remove these restrictions to increase test
    // coverage.
    QuantizationParams c_quantization_params = quantization_params;
    c_quantization_params.quantized_type = OperandDataType::kInt32;
    ASSIGN_OPTIONAL_OR_RETURN_VOID(
        auto gemm_c_id,
        BuildDequantizeInput(builder, this->context_properties(),
                             gemm_params.is_c_constant, "c", *gemm_descs.c_desc,
                             c_quantization_params, c_channel_axis,
                             seed_for_data,
                             /*scale_value=*/0.125f, /*zero_point_value=*/0,
                             data_buffers, named_inputs));
    gemm_attr.c_operand_id = gemm_c_id;
  }

  OperandId gemm_output_id = builder.BuildIntermediateOperand(
      gemm_descs.output_desc.shape(), gemm_descs.output_desc.data_type());
  builder.BuildGemm(gemm_a_id, gemm_b_id, gemm_output_id, gemm_attr);

  if (!BuildQuantizeOutput(builder, this->context_properties(), "output",
                           gemm_descs.output_desc,
                           per_tensor_quantization_params,
                           /*channel_axis=*/std::nullopt, gemm_output_id,
                           /*scale_value=*/0.125f, /*zero_point_value=*/0)) {
    return;
  }

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::DQLeakyReluQ(
    LeakyReluParams leaky_relu_params,
    OperandDataType quantized_type,
    uint8_t seed_for_input,
    float seed_for_scale,
    uint8_t seed_for_zero_point) {
  ASSIGN_OR_RETURN_VOID(
      auto leaky_relu_descs,
      SetUpLeakyReluDescriptors(this->context_properties(), leaky_relu_params));
  const OperandDescriptor& leaky_relu_desc = leaky_relu_descs.input_desc;

  // kPerTensor quantization and the corrected alpha value is used to exercise
  // the fusible path for TFLite backend:
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=2601;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
  // TODO(crbug.com/498987226): Remove this restriction to increase test
  // coverage.
  QuantizationParams per_tensor_quantization_params{
      .quantized_type = quantized_type,
      .quantization_kind = QuantizationKind::kPerTensor,
      .channel_block_size = 1};
  constexpr float kScalePositiveMin = 1.0f / 256.0f;
  constexpr float kScalePositiveMax = 128.0f;
  constexpr float kScaleNegativeMin = -127.99609375f;
  if (leaky_relu_descs.alpha < kScaleNegativeMin ||
      leaky_relu_descs.alpha > kScalePositiveMax ||
      std::abs(leaky_relu_descs.alpha) < kScalePositiveMin) {
    leaky_relu_descs.alpha = 0.01f;
  }

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  ASSIGN_OPTIONAL_OR_RETURN_VOID(
      auto leaky_relu_input_id,
      BuildDequantizeInput(builder, this->context_properties(),
                           leaky_relu_params.is_input_constant, "input",
                           leaky_relu_desc, per_tensor_quantization_params,
                           /*channel_axis=*/std::nullopt, seed_for_input,
                           seed_for_scale, seed_for_zero_point, data_buffers,
                           named_inputs));

  // The output of leakyRelu has the same shape and data type as the input.
  OperandId leaky_relu_output_id = builder.BuildIntermediateOperand(
      leaky_relu_desc.shape(), leaky_relu_desc.data_type());

  builder.BuildLeakyRelu(leaky_relu_input_id, leaky_relu_output_id,
                         leaky_relu_descs.alpha);

  if (!BuildQuantizeOutput(builder, this->context_properties(), "output",
                           leaky_relu_desc, per_tensor_quantization_params,
                           /*channel_axis=*/std::nullopt, leaky_relu_output_id,
                           seed_for_scale, seed_for_zero_point)) {
    return;
  }

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::DQPadQ(
    PadParams pad_params,
    OperandDataType quantized_type,
    uint8_t seed_for_input,
    float seed_for_scale,
    uint8_t seed_for_zero_point) {
  ASSIGN_OR_RETURN_VOID(
      auto pad_descs,
      SetUpPadDescriptors(this->context_properties(), pad_params));

  // kPerTensor quantization is used to exercise the fusible path for TFLite
  // backend:
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=2201;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
  // TODO(crbug.com/498987226): Remove this restriction to increase test
  // coverage.
  QuantizationParams per_tensor_quantization_params{
      .quantized_type = quantized_type,
      .quantization_kind = QuantizationKind::kPerTensor,
      .channel_block_size = 1};

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  ASSIGN_OPTIONAL_OR_RETURN_VOID(
      auto pad_input_id,
      BuildDequantizeInput(
          builder, this->context_properties(), pad_params.is_input_constant,
          "input", pad_descs.input_desc, per_tensor_quantization_params,
          /*channel_axis=*/std::nullopt, seed_for_input, seed_for_scale,
          seed_for_zero_point, data_buffers, named_inputs));

  OperandId pad_output_id = builder.BuildIntermediateOperand(
      pad_descs.output_desc.shape(), pad_descs.output_desc.data_type());

  builder.BuildPad(pad_input_id, pad_output_id, pad_descs.beginning_padding,
                   pad_descs.ending_padding, pad_params.mode, pad_params.value);

  if (!BuildQuantizeOutput(builder, this->context_properties(), "output",
                           pad_descs.output_desc,
                           per_tensor_quantization_params,
                           /*channel_axis=*/std::nullopt, pad_output_id,
                           seed_for_scale, seed_for_zero_point)) {
    return;
  }

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::DQPool2dQ(
    Pool2dParams pool2d_params,
    QuantizationParams quantization_params,
    uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto pool2d_descs,
      SetUpPool2dDescriptors(this->context_properties(), pool2d_params));

  InputOperandLayout input_layout =
      this->context_properties().input_operand_layout;
  const uint32_t input_channel_axis =
      input_layout == InputOperandLayout::kNchw ? 1u : 3u;
  const uint32_t output_channel_axis = input_channel_axis;

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;

  // These scale and zero-point values are used to exercise the fusible path
  // for TFLite backend (input_scale=0.25, output_scale=0.25, all
  // zero_points=0):
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=2262;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=2273;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
  // TODO(crbug.com/498987226): Remove this restriction to increase test
  // coverage.
  ASSIGN_OPTIONAL_OR_RETURN_VOID(
      auto pool2d_input_id,
      BuildDequantizeInput(
          builder, this->context_properties(), pool2d_params.is_input_constant,
          "input", pool2d_descs.input_desc, quantization_params,
          input_channel_axis, seed_for_data, /*scale_value=*/0.25f,
          /*zero_point_value=*/0, data_buffers, named_inputs));

  OperandId pool_output_id = builder.BuildIntermediateOperand(
      pool2d_descs.output_desc.shape(), pool2d_descs.output_desc.data_type());

  BuildPool2dAttributes pool2d_attr;
  pool2d_attr.window_dimensions = {pool2d_params.window_dimensions.height,
                                   pool2d_params.window_dimensions.width};
  pool2d_attr.padding = {pool2d_params.padding.beginning.height,
                         pool2d_params.padding.ending.height,
                         pool2d_params.padding.beginning.width,
                         pool2d_params.padding.ending.width};
  pool2d_attr.strides = {pool2d_params.strides.height,
                         pool2d_params.strides.width};
  pool2d_attr.dilations = {pool2d_params.dilations.height,
                           pool2d_params.dilations.width};
  builder.BuildPool2d(pool2d_params.pool2d_kind, pool2d_input_id,
                      pool_output_id, pool2d_attr);

  if (!BuildQuantizeOutput(builder, this->context_properties(), "output",
                           pool2d_descs.output_desc, quantization_params,
                           output_channel_axis, pool_output_id,
                           /*scale_value=*/0.25f, /*zero_point_value=*/0)) {
    return;
  }

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::DQReduceQ(
    ReduceParams reduce_params,
    QuantizationParams quantization_params,
    uint32_t channel_axis,
    uint8_t seed_for_input,
    float seed_for_scale,
    uint8_t seed_for_zero_point) {
  ASSIGN_OR_RETURN_VOID(
      auto reduce_descs,
      SetUpReduceDescriptors(this->context_properties(), reduce_params));

  // Use per-tensor quantization for the input when the input shape is empty
  // (scalar), since per-channel/per-block quantization requires a non-empty
  // shape. Otherwise, clamp `input_channel_axis` to be valid for the input
  // shape.
  QuantizationParams input_quantization_params = quantization_params;
  std::optional<uint32_t> input_channel_axis;
  if (reduce_descs.input_desc.shape().empty()) {
    input_quantization_params.quantization_kind = QuantizationKind::kPerTensor;
  } else {
    input_channel_axis = channel_axis % reduce_descs.input_desc.shape().size();
  }
  // Use per-tensor quantization for the output when reduce produces a scalar
  // (keep_dimensions is false and all axes are reduced), since
  // per-channel/per-block quantization requires a non-empty shape. Otherwise,
  // clamp `output_channel_axis` to be valid for the output shape.
  QuantizationParams output_quantization_params = quantization_params;
  std::optional<uint32_t> output_channel_axis;
  if (reduce_descs.output_desc.shape().empty()) {
    output_quantization_params.quantization_kind = QuantizationKind::kPerTensor;
  } else {
    output_channel_axis =
        channel_axis % reduce_descs.output_desc.shape().size();
  }

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;

  ASSIGN_OPTIONAL_OR_RETURN_VOID(
      auto reduce_input_id,
      BuildDequantizeInput(builder, this->context_properties(),
                           reduce_params.is_input_constant, "input",
                           reduce_descs.input_desc, input_quantization_params,
                           input_channel_axis, seed_for_input, seed_for_scale,
                           seed_for_zero_point, data_buffers, named_inputs));

  OperandId reduce_output_id = builder.BuildIntermediateOperand(
      reduce_descs.output_desc.shape(), reduce_descs.output_desc.data_type());

  builder.BuildReduce(reduce_params.reduce_kind, reduce_input_id,
                      reduce_output_id, reduce_descs.axes,
                      reduce_params.keep_dimensions);

  if (!BuildQuantizeOutput(builder, this->context_properties(), "output",
                           reduce_descs.output_desc, output_quantization_params,
                           output_channel_axis, reduce_output_id,
                           seed_for_scale, seed_for_zero_point)) {
    return;
  }

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::DQResample2dQ(
    Resample2dParams resample2d_params,
    OperandDataType quantized_type,
    uint8_t seed_for_input,
    float seed_for_scale,
    uint8_t seed_for_zero_point) {
  ASSIGN_OR_RETURN_VOID(auto resample2d_descs,
                        SetUpResample2dDescriptors(this->context_properties(),
                                                   resample2d_params));

  // kPerTensor quantization is used to exercise the fusible path for TFLite
  // backend:
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=2385;drc=ce3629f6f1cdbdb670dbf759e6b7c89c4a92a8fb
  // TODO(crbug.com/498987226): Remove this restriction to increase test
  // coverage.
  QuantizationParams per_tensor_quantization_params{
      .quantized_type = quantized_type,
      .quantization_kind = QuantizationKind::kPerTensor,
      .channel_block_size = 1};

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;

  ASSIGN_OPTIONAL_OR_RETURN_VOID(
      auto resample2d_input_id,
      BuildDequantizeInput(
          builder, this->context_properties(),
          resample2d_params.is_input_constant, "input",
          resample2d_descs.input_desc, per_tensor_quantization_params,
          /*channel_axis=*/std::nullopt, seed_for_input, seed_for_scale,
          seed_for_zero_point, data_buffers, named_inputs));

  OperandId resample_output_id = builder.BuildIntermediateOperand(
      resample2d_descs.output_desc.shape(),
      resample2d_descs.output_desc.data_type());

  BuildResample2dAttributes resample2d_attr;
  resample2d_attr.mode = resample2d_params.mode;
  resample2d_attr.scales = resample2d_descs.scales;
  resample2d_attr.axes = resample2d_descs.axes;
  builder.BuildResample2d(resample2d_input_id, resample_output_id,
                          resample2d_attr);

  if (!BuildQuantizeOutput(builder, this->context_properties(), "output",
                           resample2d_descs.output_desc,
                           per_tensor_quantization_params,
                           /*channel_axis=*/std::nullopt, resample_output_id,
                           seed_for_scale, seed_for_zero_point)) {
    return;
  }

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::DQSliceQ(
    SliceParams slice_params,
    OperandDataType quantized_type,
    uint8_t seed_for_input,
    float seed_for_scale,
    uint8_t seed_for_zero_point) {
  ASSIGN_OR_RETURN_VOID(
      auto slice_descs,
      SetUpSliceDescriptors(this->context_properties(), slice_params));

  // kPerTensor quantization is used to exercise the fusible path for TFLite
  // backend.
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=2422;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
  // TODO(crbug.com/498987226): Remove this restriction to increase test
  // coverage.
  QuantizationParams per_tensor_quantization_params{
      .quantized_type = quantized_type,
      .quantization_kind = QuantizationKind::kPerTensor,
      .channel_block_size = 1};

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;

  ASSIGN_OPTIONAL_OR_RETURN_VOID(
      auto slice_input_id,
      BuildDequantizeInput(
          builder, this->context_properties(), slice_params.is_input_constant,
          "input", slice_descs.input_desc, per_tensor_quantization_params,
          /*channel_axis=*/std::nullopt, seed_for_input, seed_for_scale,
          seed_for_zero_point, data_buffers, named_inputs));

  OperandId slice_output_id = builder.BuildIntermediateOperand(
      slice_descs.output_desc.shape(), slice_descs.output_desc.data_type());

  builder.BuildSlice(slice_input_id, slice_output_id, slice_descs.starts,
                     slice_descs.sizes, slice_descs.strides);

  if (!BuildQuantizeOutput(builder, this->context_properties(), "output",
                           slice_descs.output_desc,
                           per_tensor_quantization_params,
                           /*channel_axis=*/std::nullopt, slice_output_id,
                           seed_for_scale, seed_for_zero_point)) {
    return;
  }

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::DQSoftmaxQ(
    SoftmaxParams softmax_params,
    OperandDataType quantized_type,
    uint8_t seed_for_input,
    float seed_for_scale,
    uint8_t seed_for_zero_point) {
  ASSIGN_OR_RETURN_VOID(
      auto softmax_descs,
      SetUpSoftmaxDescriptors(this->context_properties(), softmax_params));
  const OperandDescriptor& softmax_desc = softmax_descs.input_desc;

  // kPerTensor quantization and the scale and zero-point values
  // (scale=1.0f/256.0f, zero_point=-128) are used to exercise the fusible path
  // for TFLite backend:
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=2468;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
  // TODO(crbug.com/498987226): Remove this restriction to increase test
  // coverage.
  QuantizationParams per_tensor_quantization_params{
      .quantized_type = quantized_type,
      .quantization_kind = QuantizationKind::kPerTensor,
      .channel_block_size = 1};
  if (quantized_type == OperandDataType::kInt8) {
    seed_for_scale = 1.0f / 256.0f;
    seed_for_zero_point = static_cast<uint8_t>(-128);
  }

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;
  ASSIGN_OPTIONAL_OR_RETURN_VOID(
      auto softmax_input_id,
      BuildDequantizeInput(
          builder, this->context_properties(), softmax_params.is_input_constant,
          "input", softmax_desc, per_tensor_quantization_params,
          /*channel_axis=*/std::nullopt, seed_for_input, seed_for_scale,
          seed_for_zero_point, data_buffers, named_inputs));

  // The output of softmax has the same shape and data type as the input.
  OperandId softmax_output_id = builder.BuildIntermediateOperand(
      softmax_desc.shape(), softmax_desc.data_type());

  builder.BuildSoftmax(softmax_input_id, softmax_output_id, softmax_descs.axis);

  if (!BuildQuantizeOutput(builder, this->context_properties(), "output",
                           softmax_desc, per_tensor_quantization_params,
                           /*channel_axis=*/std::nullopt, softmax_output_id,
                           seed_for_scale, seed_for_zero_point)) {
    return;
  }

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::DQSplitQ(
    SplitParams split_params,
    OperandDataType quantized_type,
    uint8_t seed_for_input,
    float seed_for_scale,
    uint8_t seed_for_zero_point) {
  ASSIGN_OR_RETURN_VOID(
      auto split_descs,
      SetUpSplitDescriptors(this->context_properties(), split_params));

  // kPerTensor quantization is used to exercise the fusible path for TFLite
  // backend:
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=2559;drc=ce3629f6f1cdbdb670dbf759e6b7c89c4a92a8fb
  // TODO(crbug.com/498987226): Remove this restriction to increase test
  // coverage.
  QuantizationParams per_tensor_quantization_params{
      .quantized_type = quantized_type,
      .quantization_kind = QuantizationKind::kPerTensor,
      .channel_block_size = 1};

  const size_t output_num = split_descs.output_descs.size();

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;

  ASSIGN_OPTIONAL_OR_RETURN_VOID(
      auto split_input_id,
      BuildDequantizeInput(
          builder, this->context_properties(), split_params.is_input_constant,
          "input", split_descs.input_desc, per_tensor_quantization_params,
          /*channel_axis=*/std::nullopt, seed_for_input, seed_for_scale,
          seed_for_zero_point, data_buffers, named_inputs));

  std::vector<OperandId> split_output_ids;
  split_output_ids.reserve(output_num);
  for (size_t i = 0; i < output_num; ++i) {
    OperandId split_output_id = builder.BuildIntermediateOperand(
        split_descs.output_descs[i].shape(),
        split_descs.output_descs[i].data_type());
    split_output_ids.push_back(split_output_id);
  }

  builder.BuildSplit(split_input_id, split_output_ids, split_descs.axis);

  // Quantize each output.
  for (size_t i = 0; i < output_num; ++i) {
    if (!BuildQuantizeOutput(builder, this->context_properties(),
                             "output" + base::NumberToString(i),
                             split_descs.output_descs[i],
                             per_tensor_quantization_params,
                             /*channel_axis=*/std::nullopt, split_output_ids[i],
                             seed_for_scale, seed_for_zero_point)) {
      return;
    }
  }

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::DQTransposeQ(
    TransposeParams transpose_params,
    QuantizationParams quantization_params,
    uint32_t channel_axis,
    uint8_t seed_for_input,
    float seed_for_scale,
    uint8_t seed_for_zero_point) {
  ASSIGN_OR_RETURN_VOID(
      auto transpose_descs,
      SetUpTransposeDescriptors(this->context_properties(), transpose_params));

  // kPerTensor quantization is used to exercise the fusible path for TFLite
  // backend:
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=2602;drc=ce3629f6f1cdbdb670dbf759e6b7c89c4a92a8fb
  // TODO(crbug.com/498987226): Remove this restriction to increase test
  // coverage.
  QuantizationParams per_tensor_quantization_params = quantization_params;
  per_tensor_quantization_params.quantization_kind =
      QuantizationKind::kPerTensor;

  // Use per-tensor quantization for the output when transpose produces a
  // scalar (rank 0), since per-channel/per-block quantization requires a
  // non-empty shape. Otherwise, clamp channel_axis to be valid for the output
  // shape.
  QuantizationParams output_quantization_params = quantization_params;
  std::optional<uint32_t> output_channel_axis;
  if (transpose_descs.output_desc.shape().empty()) {
    output_quantization_params.quantization_kind = QuantizationKind::kPerTensor;
  } else {
    output_channel_axis =
        channel_axis % transpose_descs.output_desc.shape().size();
  }

  mojo::Remote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> data_buffers;

  ASSIGN_OPTIONAL_OR_RETURN_VOID(
      auto transpose_input_id,
      BuildDequantizeInput(
          builder, this->context_properties(),
          transpose_params.is_input_constant, "input",
          transpose_descs.input_desc, per_tensor_quantization_params,
          /*channel_axis=*/std::nullopt, seed_for_input, seed_for_scale,
          seed_for_zero_point, data_buffers, named_inputs));

  OperandId transpose_output_id =
      builder.BuildIntermediateOperand(transpose_descs.output_desc.shape(),
                                       transpose_descs.output_desc.data_type());

  builder.BuildTranspose(transpose_input_id, transpose_output_id,
                         std::move(transpose_descs.permutation));

  if (!BuildQuantizeOutput(builder, this->context_properties(), "output",
                           transpose_descs.output_desc,
                           output_quantization_params, output_channel_axis,
                           transpose_output_id, seed_for_scale,
                           seed_for_zero_point)) {
    return;
  }

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

// gelu, hardSwish, relu, sigmoid, softplus, softsign and tanh are all
// activations with no extra options, so they share a single fuzzer that
// selects among them via `kind`.
WEBNN_FUZZ_TEST_F(Activation,
                  .WithDomains(AnyActivationParams(kAllActivationKinds),
                               fuzztest::Arbitrary<uint8_t>())
                      .WithSeeds({{ActivationParams{
                                       /*data_type=*/OperandDataType::kFloat32,
                                       /*rank=*/4,
                                       /*input_dims=*/{2, 6, 4, 4, 1, 1, 1, 1},
                                       /*kind=*/ActivationKind::kTanh,
                                       /*is_input_constant=*/false,
                                   },
                                   /*seed_for_data=*/4}}));

WEBNN_FUZZ_TEST_F(
    BatchNormalization,
    .WithDomains(AnyBatchNormalizationParams(), fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{BatchNormalizationParams{
                         /*data_type=*/OperandDataType::kFloat32,
                         /*rank=*/4,
                         /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                         /*axis=*/1,
                         /*epsilon=*/1e-5f,
                         /*scale_kind=*/OptionalOperandKind::kConstant,
                         /*bias_kind=*/OptionalOperandKind::kInput,
                         /*is_input_constant=*/false,
                         /*is_mean_constant=*/true,
                         /*is_variance_constant=*/true,
                     },
                     /*seed_for_data=*/1}}));

WEBNN_FUZZ_TEST_F(Clamp,
                  .WithDomains(AnyClampParams(), fuzztest::Arbitrary<uint8_t>())
                      .WithSeeds({{ClampParams{
                                       /*data_type=*/OperandDataType::kFloat32,
                                       /*rank=*/4,
                                       /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                                       /*min_value=*/-1.0f,
                                       /*max_value=*/1.0f,
                                       /*is_input_constant=*/false,
                                   },
                                   /*seed_for_data=*/4}}));

WEBNN_FUZZ_TEST_F(Concat,
                  .WithDomains(AnyConcatParams(),
                               fuzztest::Arbitrary<uint8_t>())
                      .WithSeeds({{ConcatParams{
                                       /*data_type=*/OperandDataType::kFloat32,
                                       /*rank=*/4,
                                       /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                                       /*axis=*/1,
                                       /*extra_axis_dims=*/{2, 5},
                                       /*is_input_constant=*/false,
                                   },
                                   /*seed_for_data=*/1}}));

WEBNN_FUZZ_TEST_F(Conv2d,
                  .WithDomains(AnyConv2dParams(),
                               fuzztest::Arbitrary<uint8_t>())
                      .WithSeeds({{Conv2dParams{
                                       OperandDataType::kFloat16,
                                       mojom::Conv2d::Kind::kDirect,
                                       /*batch=*/1,
                                       /*input_channels=*/3,
                                       /*input_height=*/224,
                                       /*input_width=*/224,
                                       /*output_channels=*/64,
                                       /*padding=*/{{3, 3}, {3, 3}},
                                       /*filter_dimensions=*/{7, 7},
                                       /*strides=*/{1, 1},
                                       /*dilations=*/{1, 1},
                                       /*output_padding=*/{0, 0},
                                       /*groups=*/1,
                                       /*is_input_constant=*/false,
                                       /*is_filter_constant=*/true,
                                       /*bias_kind=*/OptionalOperandKind::kNone,
                                       /*is_depthwise=*/false,
                                       /*activation_kind=*/
                                       Conv2dActivationKind::kRelu,
                                   },
                                   /*seed_for_data=*/1}}));

WEBNN_FUZZ_TEST_F(
    DequantizeLinear,
    .WithDomains(AnyDequantizeLinearParams(),
                 /*seed_for_input=*/fuzztest::Arbitrary<uint8_t>(),
                 /*seed_for_scale=*/fuzztest::Arbitrary<float>(),
                 /*seed_for_zero_point=*/fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{DequantizeLinearParams{
                         /*input_data_type=*/OperandDataType::kUint8,
                         /*scale_data_type=*/OperandDataType::kFloat32,
                         /*rank=*/4,
                         /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                         /*quantization_kind=*/QuantizationKind::kPerTensor,
                         /*channel_axis=*/1,
                         /*channel_block_size=*/1,
                         /*is_input_constant=*/false,
                         /*is_scale_constant=*/true,
                         /*is_zero_point_constant=*/true,
                     },
                     /*seed_for_input=*/2,
                     /*seed_for_scale=*/0.25f,
                     /*seed_for_zero_point=*/0}}));

WEBNN_FUZZ_TEST_F(
    ElementWiseBinary,
    .WithDomains(AnyElementWiseBinaryParams(kAllElementWiseBinaryKinds),
                 fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{ElementWiseBinaryParams{
                         /*data_type=*/OperandDataType::kFloat32,
                         /*kind=*/mojom::ElementWiseBinary::Kind::kAdd,
                         /*lhs_rank=*/5,
                         /*rhs_rank=*/2,
                         /*lhs_dims=*/{2, 3, 4, 5, 6, 1, 1, 1},
                         /*rhs_dims=*/{2, 3, 1, 1, 1, 1, 1, 1},
                         /*is_lhs_constant=*/true,
                         /*is_rhs_constant=*/false,
                     },
                     /*seed_for_data=*/1}}));

WEBNN_FUZZ_TEST_F(Elu,
                  .WithDomains(AnyEluParams(), fuzztest::Arbitrary<uint8_t>())
                      .WithSeeds({{EluParams{
                                       /*data_type=*/OperandDataType::kFloat32,
                                       /*rank=*/4,
                                       /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                                       /*alpha=*/1.0f,
                                       /*is_input_constant=*/false,
                                   },
                                   /*seed_for_data=*/1}}));

WEBNN_FUZZ_TEST_F(Expand,
                  .WithDomains(AnyExpandParams(),
                               fuzztest::Arbitrary<uint8_t>())
                      .WithSeeds({{ExpandParams{
                                       /*data_type=*/OperandDataType::kFloat32,
                                       /*input_rank=*/2,
                                       /*output_rank=*/3,
                                       /*input_dims=*/{1, 4, 1, 1, 1, 1, 1, 1},
                                       /*output_dims=*/{2, 3, 4, 1, 1, 1, 1, 1},
                                       /*is_input_constant=*/false,
                                   },
                                   /*seed_for_data=*/1}}));

WEBNN_FUZZ_TEST_F(
    Gather,
    .WithDomains(AnyGatherParams(), fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{GatherParams{
                         /*input_data_type=*/OperandDataType::kFloat32,
                         /*indices_data_type=*/OperandDataType::kInt32,
                         /*input_rank=*/3,
                         /*input_dims=*/{2, 3, 4, 1, 1, 1, 1, 1},
                         /*indices_rank=*/2,
                         /*indices_dims=*/{2, 3, 1, 1, 1, 1, 1, 1},
                         /*axis=*/1,
                         /*indices_fill_value=*/0,
                         /*is_input_constant=*/false,
                         /*is_indices_constant=*/true,
                     },
                     /*seed_for_data=*/5}}));

WEBNN_FUZZ_TEST_F(
    GatherND,
    .WithDomains(AnyGatherNDParams(), fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{GatherNDParams{
                         /*input_data_type=*/OperandDataType::kFloat32,
                         /*indices_data_type=*/OperandDataType::kInt32,
                         /*input_rank=*/3,
                         /*input_dims=*/{2, 3, 4, 1, 1, 1, 1, 1},
                         /*indices_rank=*/2,
                         /*indices_dims=*/{2, 1, 1, 1, 1, 1, 1, 1},
                         /*indices_fill_value=*/0,
                         /*is_input_constant=*/false,
                         /*is_indices_constant=*/true,
                     },
                     /*seed_for_data=*/5}}));

WEBNN_FUZZ_TEST_F(Gemm,
                  .WithDomains(AnyGemmParams(), fuzztest::Arbitrary<uint8_t>())
                      .WithSeeds({{GemmParams{
                                       OperandDataType::kFloat32,
                                       /*m=*/3,
                                       /*k=*/4,
                                       /*n=*/5,
                                       /*alpha=*/1.0f,
                                       /*beta=*/1.0f,
                                       /*a_transpose=*/false,
                                       /*b_transpose=*/false,
                                       /*has_c=*/true,
                                       /*c_shape_kind=*/GemmCShapeKind::k2D_MxN,
                                       /*is_a_constant=*/false,
                                       /*is_b_constant=*/true,
                                       /*is_c_constant=*/true,
                                   },
                                   /*seed_for_data=*/3}}));

WEBNN_FUZZ_TEST_F(HardSigmoid,
                  .WithDomains(AnyHardSigmoidParams(),
                               fuzztest::Arbitrary<uint8_t>())
                      .WithSeeds({{HardSigmoidParams{
                                       /*data_type=*/OperandDataType::kFloat32,
                                       /*rank=*/4,
                                       /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                                       /*alpha=*/0.2f,
                                       /*beta=*/0.5f,
                                       /*is_input_constant=*/false,
                                   },
                                   /*seed_for_data=*/2}}));

WEBNN_FUZZ_TEST_F(
    InstanceNormalization,
    .WithDomains(AnyInstanceNormalizationParams(),
                 fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{InstanceNormalizationParams{
                         /*data_type=*/OperandDataType::kFloat32,
                         /*batch=*/1,
                         /*channels=*/3,
                         /*input_height=*/4,
                         /*input_width=*/4,
                         /*epsilon=*/1e-5f,
                         /*scale_kind=*/OptionalOperandKind::kInput,
                         /*bias_kind=*/OptionalOperandKind::kConstant,
                         /*is_input_constant=*/false,
                     },
                     /*seed_for_data=*/1}}));

WEBNN_FUZZ_TEST_F(
    LayerNormalization,
    .WithDomains(AnyLayerNormalizationParams(), fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{LayerNormalizationParams{
                         /*data_type=*/OperandDataType::kFloat32,
                         /*rank=*/4,
                         /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                         /*num_axes=*/2,
                         /*axes=*/{2, 3, 0, 0, 0, 0, 0, 0},
                         /*epsilon=*/1e-5f,
                         /*scale_kind=*/OptionalOperandKind::kInput,
                         /*bias_kind=*/OptionalOperandKind::kConstant,
                         /*is_input_constant=*/false,
                     },
                     /*seed_for_data=*/1}}));

WEBNN_FUZZ_TEST_F(LeakyRelu,
                  .WithDomains(AnyLeakyReluParams(),
                               fuzztest::Arbitrary<uint8_t>())
                      .WithSeeds({{LeakyReluParams{
                                       /*data_type=*/OperandDataType::kFloat32,
                                       /*rank=*/4,
                                       /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                                       /*alpha=*/0.01f,
                                       /*is_input_constant=*/false,
                                   },
                                   /*seed_for_data=*/2}}));

WEBNN_FUZZ_TEST_F(Linear,
                  .WithDomains(AnyLinearParams(),
                               fuzztest::Arbitrary<uint8_t>())
                      .WithSeeds({{LinearParams{
                                       /*data_type=*/OperandDataType::kFloat32,
                                       /*rank=*/4,
                                       /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                                       /*alpha=*/1.0f,
                                       /*beta=*/0.0f,
                                       /*is_input_constant=*/false,
                                   },
                                   /*seed_for_data=*/2}}));

WEBNN_FUZZ_TEST_F(
    Lstm,
    .WithDomains(AnyLstmParams(), fuzztest::Arbitrary<uint8_t>())
        .WithSeeds(
            {{LstmParams{
                  /*data_type=*/OperandDataType::kFloat32,
                  /*steps=*/2,
                  /*batch_size=*/1,
                  /*input_size=*/3,
                  /*hidden_size=*/4,
                  /*direction=*/mojom::RecurrentNetworkDirection::kForward,
                  /*layout=*/mojom::LstmWeightLayout::kIofg,
                  /*bias_kind=*/OptionalOperandKind::kConstant,
                  /*recurrent_bias_kind=*/OptionalOperandKind::kConstant,
                  /*peephole_weight_kind=*/OptionalOperandKind::kNone,
                  /*initial_hidden_state_kind=*/OptionalOperandKind::kInput,
                  /*initial_cell_state_kind=*/OptionalOperandKind::kInput,
                  /*return_sequence=*/false,
                  /*is_input_constant=*/false,
                  /*is_weight_constant=*/true,
                  /*is_recurrent_weight_constant=*/true,
                  /*activations=*/
                  {mojom::RecurrentNetworkActivation::kSigmoid,
                   mojom::RecurrentNetworkActivation::kTanh,
                   mojom::RecurrentNetworkActivation::kTanh},
              },
              /*seed_for_data=*/1}}));

WEBNN_FUZZ_TEST_F(
    LstmCell,
    .WithDomains(AnyLstmCellParams(), fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{LstmCellParams{
                         /*data_type=*/OperandDataType::kFloat32,
                         /*batch_size=*/1,
                         /*input_size=*/3,
                         /*hidden_size=*/4,
                         /*layout=*/mojom::LstmWeightLayout::kIofg,
                         /*bias_kind=*/OptionalOperandKind::kInput,
                         /*recurrent_bias_kind=*/OptionalOperandKind::kConstant,
                         /*peephole_weight_kind=*/OptionalOperandKind::kNone,
                         /*is_input_constant=*/false,
                         /*is_weight_constant=*/true,
                         /*is_recurrent_weight_constant=*/true,
                         /*is_hidden_state_constant=*/false,
                         /*is_cell_state_constant=*/false,
                         /*activations=*/
                         {mojom::RecurrentNetworkActivation::kSigmoid,
                          mojom::RecurrentNetworkActivation::kTanh,
                          mojom::RecurrentNetworkActivation::kTanh},
                     },
                     /*seed_for_data=*/1}}));

WEBNN_FUZZ_TEST_F(Matmul,
                  .WithDomains(AnyMatmulParams(),
                               fuzztest::Arbitrary<uint8_t>())
                      .WithSeeds({{MatmulParams{
                                       /*data_type=*/OperandDataType::kFloat32,
                                       /*a_rank=*/3,
                                       /*b_rank=*/4,
                                       /*a_dims=*/{4, 2, 3, 1, 1, 1, 1, 1},
                                       /*b_dims=*/{5, 2, 3, 4, 1, 1, 1, 1},
                                       /*is_a_constant=*/true,
                                       /*is_b_constant=*/false,
                                   },
                                   /*seed_for_data=*/3}}));

WEBNN_FUZZ_TEST_F(
    Pad,
    .WithDomains(AnyPadParams(), fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{PadParams{
                         /*data_type=*/OperandDataType::kFloat32,
                         /*rank=*/4,
                         /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                         /*beginning_padding=*/{0, 0, 1, 1, 0, 0, 0, 0},
                         /*ending_padding=*/{0, 0, 1, 1, 0, 0, 0, 0},
                         /*mode=*/mojom::PaddingMode::Tag::kConstant,
                         /*value=*/0.0f,
                         /*is_input_constant=*/false,
                     },
                     /*seed_for_data=*/1}}));

WEBNN_FUZZ_TEST_F(Pool2d,
                  .WithDomains(AnyPool2dParams(),
                               fuzztest::Arbitrary<uint8_t>())
                      .WithSeeds({{Pool2dParams{
                                       OperandDataType::kFloat32,
                                       mojom::Pool2d::Kind::kMaxPool2d,
                                       RoundingType::kFloor,
                                       /*batch=*/1,
                                       /*channels=*/3,
                                       /*input_height=*/4,
                                       /*input_width=*/4,
                                       /*padding=*/{{0, 0}, {0, 0}},
                                       /*window_dimensions=*/{2, 2},
                                       /*strides=*/{2, 2},
                                       /*dilations=*/{1, 1},
                                       /*is_input_constant=*/false,
                                   },
                                   /*seed_for_data=*/2}}));

WEBNN_FUZZ_TEST_F(Prelu,
                  .WithDomains(AnyPreluParams(), fuzztest::Arbitrary<uint8_t>())
                      .WithSeeds({{PreluParams{
                                       /*data_type=*/OperandDataType::kFloat32,
                                       /*rank=*/5,
                                       /*input_dims=*/{1, 3, 4, 4, 6, 1, 1, 1},
                                       /*slope_rank=*/3,
                                       /*slope_dims=*/{3, 1, 4, 1, 1, 1, 1, 1},
                                       /*is_input_constant=*/false,
                                       /*is_slope_constant=*/true,
                                   },
                                   /*seed_for_data=*/1}}));

WEBNN_FUZZ_TEST_F(
    QuantizeLinear,
    .WithDomains(AnyQuantizeLinearParams(),
                 /*seed_for_input=*/fuzztest::Arbitrary<float>(),
                 /*seed_for_scale=*/fuzztest::Arbitrary<float>(),
                 /*seed_for_zero_point=*/fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{QuantizeLinearParams{
                         /*input_data_type=*/OperandDataType::kFloat32,
                         /*zero_point_data_type=*/OperandDataType::kUint8,
                         /*rank=*/4,
                         /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                         /*quantization_kind=*/QuantizationKind::kPerTensor,
                         /*channel_axis=*/1,
                         /*channel_block_size=*/1,
                         /*is_input_constant=*/false,
                         /*is_scale_constant=*/true,
                         /*is_zero_point_constant=*/true,
                     },
                     /*seed_for_input=*/1.0f,
                     /*seed_for_scale=*/0.25f,
                     /*seed_for_zero_point=*/0}}));

WEBNN_FUZZ_TEST_F(
    Reduce,
    .WithDomains(AnyReduceParams(), fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{ReduceParams{
                         /*data_type=*/OperandDataType::kFloat32,
                         /*reduce_kind=*/mojom::Reduce::Kind::kMax,
                         /*rank=*/4,
                         /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                         /*num_axes=*/2,
                         /*axes=*/{2, 3, 0, 0, 0, 0, 0, 0},
                         /*keep_dimensions=*/true,
                         /*is_input_constant=*/false,
                     },
                     /*seed_for_data=*/2}}));

WEBNN_FUZZ_TEST_F(
    Resample2d,
    .WithDomains(AnyResample2dParams(), fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{Resample2dParams{
                         OperandDataType::kFloat32,
                         mojom::Resample2d::InterpolationMode::kNearestNeighbor,
                         /*batch=*/1,
                         /*channels=*/1,
                         /*input_height=*/2,
                         /*input_width=*/4,
                         /*use_sizes=*/false,
                         /*scale_height=*/2.0f,
                         /*scale_width=*/2.0f,
                         /*output_height=*/4,
                         /*output_width=*/8,
                         /*is_input_constant=*/false,
                     },
                     /*seed_for_data=*/1}}));

WEBNN_FUZZ_TEST_F(
    ScatterElements,
    .WithDomains(AnyScatterElementsParams(), fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{ScatterElementsParams{
                         /*input_data_type=*/OperandDataType::kFloat32,
                         /*indices_data_type=*/OperandDataType::kInt32,
                         /*rank=*/2,
                         /*axis=*/1,
                         /*input_dims=*/{6, 5, 1, 1, 1, 1, 1, 1},
                         /*indices_axis_dim_size=*/2,
                         /*indices_fill_value=*/0,
                         /*is_input_constant=*/false,
                         /*is_indices_constant=*/false,
                         /*is_updates_constant=*/false,
                     },
                     /*seed_for_data=*/4}}));

WEBNN_FUZZ_TEST_F(Slice,
                  .WithDomains(AnySliceParams(), fuzztest::Arbitrary<uint8_t>())
                      .WithSeeds({{SliceParams{
                                       /*data_type=*/OperandDataType::kFloat32,
                                       /*rank=*/4,
                                       /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                                       /*starts=*/{0, 1, 0, 0, 0, 0, 0, 0},
                                       /*sizes=*/{1, 2, 4, 4, 1, 1, 1, 1},
                                       /*strides=*/{1, 1, 2, 4, 1, 1, 1, 1},
                                       /*is_input_constant=*/false,
                                   },
                                   /*seed_for_data=*/3}}));

WEBNN_FUZZ_TEST_F(Softmax,
                  .WithDomains(AnySoftmaxParams(),
                               fuzztest::Arbitrary<uint8_t>())
                      .WithSeeds({{SoftmaxParams{
                                       /*data_type=*/OperandDataType::kFloat32,
                                       /*rank=*/4,
                                       /*input_dims=*/{4, 5, 5, 6, 1, 1, 1, 1},
                                       /*axis=*/2,
                                       /*is_input_constant=*/false,
                                   },
                                   /*seed_for_data=*/4}}));

WEBNN_FUZZ_TEST_F(Split,
                  .WithDomains(AnySplitParams(), fuzztest::Arbitrary<uint8_t>())
                      .WithSeeds({{SplitParams{
                                       /*data_type=*/OperandDataType::kFloat32,
                                       /*rank=*/4,
                                       /*input_dims=*/{1, 6, 4, 4, 1, 1, 1, 1},
                                       /*axis=*/1,
                                       /*use_equal_splits=*/true,
                                       /*num_splits=*/3,
                                       /*split_sizes=*/{1},
                                       /*is_input_constant=*/false,
                                   },
                                   /*seed_for_data=*/1}}));

WEBNN_FUZZ_TEST_F(Transpose,
                  .WithDomains(AnyTransposeParams(),
                               fuzztest::Arbitrary<uint8_t>())
                      .WithSeeds({{TransposeParams{
                                       /*data_type=*/OperandDataType::kFloat32,
                                       /*rank=*/4,
                                       /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                                       /*permutation=*/{0, 5, 6, 3, 0, 0, 0, 0},
                                       /*is_input_constant=*/false,
                                   },
                                   /*seed_for_data=*/4}}));

WEBNN_FUZZ_TEST_F(Triangular,
                  .WithDomains(AnyTriangularParams(),
                               fuzztest::Arbitrary<uint8_t>())
                      .WithSeeds({{TriangularParams{
                                       /*data_type=*/OperandDataType::kFloat32,
                                       /*rank=*/4,
                                       /*input_dims=*/{2, 4, 4, 6, 1, 1, 1, 1},
                                       /*upper=*/true,
                                       /*diagonal=*/2,
                                       /*is_input_constant=*/false,
                                   },
                                   /*seed_for_data=*/5}}));

WEBNN_FUZZ_TEST_F(
    DQClampQ,
    .WithDomains(AnyClampParams(),
                 AnyQuantizationParams(),
                 /*channel_axis=*/fuzztest::InRange<uint32_t>(0, 7),
                 /*seed_for_input=*/fuzztest::Arbitrary<uint8_t>(),
                 /*seed_for_scale=*/
                 fuzztest::ElementOf({0.125f, 0.25f, 0.5f, 1.0f, 2.0f}),
                 /*seed_for_zero_point=*/fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{ClampParams{
                         /*data_type=*/OperandDataType::kFloat32,
                         /*rank=*/4,
                         /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                         /*min_value=*/-1.0f,
                         /*max_value=*/1.0f,
                         /*is_input_constant=*/false,
                     },
                     QuantizationParams{
                         /*quantized_type=*/OperandDataType::kUint8,
                         QuantizationKind::kPerTensor,
                         // This is unused for per tensor quantization.
                         /*channel_block_size=*/1},
                     /*channel_axis=*/2,
                     /*seed_for_input=*/4,
                     /*seed_for_scale=*/0.25f,
                     /*seed_for_zero_point=*/0}}));

WEBNN_FUZZ_TEST_F(
    DQConcatQ,
    .WithDomains(AnyConcatParams(),
                 AnyQuantizedDataType(),
                 /*seed_for_input=*/fuzztest::Arbitrary<uint8_t>(),
                 /*seed_for_scale=*/
                 fuzztest::ElementOf({0.125f, 0.25f, 0.5f, 1.0f, 2.0f}),
                 /*seed_for_zero_point=*/fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{ConcatParams{
                         /*data_type=*/OperandDataType::kFloat32,
                         /*rank=*/4,
                         /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                         /*axis=*/1,
                         /*extra_axis_dims=*/{2, 5},
                         /*is_input_constant=*/false,
                     },
                     /*quantized_type=*/OperandDataType::kUint8,
                     /*seed_for_input=*/2,
                     /*seed_for_scale=*/0.25f,
                     /*seed_for_zero_point=*/0}}));

WEBNN_FUZZ_TEST_F(
    DQConv2dQ,
    .WithDomains(AnyConv2dParams(),
                 AnyQuantizationParams(),
                 fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{Conv2dParams{OperandDataType::kFloat16,
                                  mojom::Conv2d::Kind::kDirect,
                                  /*batch=*/1,
                                  /*input_channels=*/3,
                                  /*input_height=*/224,
                                  /*input_width=*/224,
                                  /*output_channels=*/64,
                                  /*padding=*/{{3, 3}, {3, 3}},
                                  /*filter_dimensions=*/{7, 7},
                                  /*strides=*/{1, 1},
                                  /*dilations=*/{1, 1},
                                  /*output_padding=*/{0, 0},
                                  /*groups=*/1,
                                  /*is_input_constant=*/false,
                                  /*is_filter_constant=*/true,
                                  /*bias_kind=*/OptionalOperandKind::kNone,
                                  /*is_depthwise=*/false,
                                  /*activation_kind=*/
                                  Conv2dActivationKind::kRelu},
                     QuantizationParams{
                         /*quantized_type=*/OperandDataType::kUint8,
                         QuantizationKind::kPerTensor,
                         // This is unused for per tensor quantization.
                         /*channel_block_size=*/1},
                     /*seed_for_data=*/1}}));

WEBNN_FUZZ_TEST_F(
    DQElementWiseBinaryQ,
    .WithDomains(
        AnyElementWiseBinaryParams(kAllElementWiseBinaryQuantizedKinds),
        AnyQuantizedDataType(),
        /*seed_for_input=*/fuzztest::Arbitrary<uint8_t>(),
        /*seed_for_scale=*/
        fuzztest::ElementOf({0.125f, 0.25f, 0.5f, 1.0f, 2.0f}),
        /*seed_for_zero_point=*/fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{ElementWiseBinaryParams{
                         /*data_type=*/OperandDataType::kFloat32,
                         /*kind=*/mojom::ElementWiseBinary::Kind::kAdd,
                         /*lhs_rank=*/5,
                         /*rhs_rank=*/2,
                         /*lhs_dims=*/{2, 3, 4, 5, 6, 1, 1, 1},
                         /*rhs_dims=*/{2, 3, 1, 1, 1, 1, 1, 1},
                         /*is_lhs_constant=*/true,
                         /*is_rhs_constant=*/false,
                     },
                     /*quantized_type=*/OperandDataType::kUint8,
                     /*seed_for_input=*/2,
                     /*seed_for_scale=*/0.25f,
                     /*seed_for_zero_point=*/0}}));

WEBNN_FUZZ_TEST_F(
    DQEluQ,
    .WithDomains(AnyEluParams(),
                 /*seed_for_input=*/fuzztest::Arbitrary<uint8_t>(),
                 /*seed_for_scale=*/
                 fuzztest::ElementOf({0.125f, 0.25f, 0.5f, 1.0f, 2.0f}),
                 /*seed_for_zero_point=*/fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{EluParams{
                         /*data_type=*/OperandDataType::kFloat32,
                         /*rank=*/4,
                         /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                         /*alpha=*/1.0f,
                         /*is_input_constant=*/false,
                     },
                     /*seed_for_input=*/1,
                     /*seed_for_scale=*/0.25f,
                     /*seed_for_zero_point=*/0}}));

WEBNN_FUZZ_TEST_F(
    DQGatherQ,
    .WithDomains(AnyGatherParams(),
                 AnyQuantizationParams(),
                 /*channel_axis=*/fuzztest::InRange<uint32_t>(0, 7),
                 /*seed_for_input=*/fuzztest::Arbitrary<uint8_t>(),
                 /*seed_for_scale=*/
                 fuzztest::ElementOf({0.125f, 0.25f, 0.5f, 1.0f, 2.0f}),
                 /*seed_for_zero_point=*/fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{GatherParams{
                         /*input_data_type=*/OperandDataType::kFloat32,
                         /*indices_data_type=*/OperandDataType::kInt32,
                         /*input_rank=*/3,
                         /*input_dims=*/{2, 3, 4, 1, 1, 1, 1, 1},
                         /*indices_rank=*/2,
                         /*indices_dims=*/{2, 3, 1, 1, 1, 1, 1, 1},
                         /*axis=*/1,
                         /*indices_fill_value=*/0,
                         /*is_input_constant=*/false,
                         /*is_indices_constant=*/true,
                     },
                     QuantizationParams{
                         /*quantized_type=*/OperandDataType::kUint8,
                         QuantizationKind::kPerBlock,
                         /*channel_block_size=*/2},
                     /*channel_axis=*/2,
                     /*seed_for_input=*/2,
                     /*seed_for_scale=*/0.25f,
                     /*seed_for_zero_point=*/0}}));

WEBNN_FUZZ_TEST_F(
    DQGemmQ,
    .WithDomains(AnyGemmParams(),
                 AnyQuantizationParams(),
                 fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{GemmParams{OperandDataType::kFloat32,
                                /*m=*/3,
                                /*k=*/4,
                                /*n=*/5,
                                /*alpha=*/1.0f,
                                /*beta=*/1.0f,
                                /*a_transpose=*/false,
                                /*b_transpose=*/true,
                                /*has_c=*/true,
                                /*c_shape_kind=*/GemmCShapeKind::k2D_MxN,
                                /*is_a_constant=*/false,
                                /*is_b_constant=*/true,
                                /*is_c_constant=*/true},
                     QuantizationParams{
                         /*quantized_type=*/OperandDataType::kInt8,
                         QuantizationKind::kPerChannel,
                         // This is unused for per channel quantization.
                         /*channel_block_size=*/1},
                     /*seed_for_data=*/3}}));

WEBNN_FUZZ_TEST_F(
    DQLeakyReluQ,
    .WithDomains(AnyLeakyReluParams(),
                 AnyQuantizedDataType(),
                 /*seed_for_input=*/fuzztest::Arbitrary<uint8_t>(),
                 /*seed_for_scale=*/
                 fuzztest::ElementOf({0.125f, 0.25f, 0.5f, 1.0f, 2.0f}),
                 /*seed_for_zero_point=*/fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{LeakyReluParams{
                         /*data_type=*/OperandDataType::kFloat32,
                         /*rank=*/4,
                         /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                         /*alpha=*/0.01f,
                         /*is_input_constant=*/false,
                     },
                     /*quantized_type=*/OperandDataType::kUint8,
                     /*seed_for_input=*/2,
                     /*seed_for_scale=*/0.25f,
                     /*seed_for_zero_point=*/0}}));

WEBNN_FUZZ_TEST_F(
    DQPadQ,
    .WithDomains(AnyPadParams(),
                 AnyQuantizedDataType(),
                 /*seed_for_input=*/fuzztest::Arbitrary<uint8_t>(),
                 /*seed_for_scale=*/
                 fuzztest::ElementOf({0.125f, 0.25f, 0.5f, 1.0f, 2.0f}),
                 /*seed_for_zero_point=*/fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{PadParams{
                         /*data_type=*/OperandDataType::kFloat32,
                         /*rank=*/4,
                         /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                         /*beginning_padding=*/{0, 0, 1, 1, 0, 0, 0, 0},
                         /*ending_padding=*/{0, 0, 1, 1, 0, 0, 0, 0},
                         /*mode=*/mojom::PaddingMode::Tag::kConstant,
                         /*value=*/0.0f,
                         /*is_input_constant=*/false,
                     },
                     /*quantized_type=*/OperandDataType::kUint8,
                     /*seed_for_input=*/2,
                     /*seed_for_scale=*/0.25f,
                     /*seed_for_zero_point=*/0}}));

WEBNN_FUZZ_TEST_F(
    DQPool2dQ,
    .WithDomains(AnyPool2dParams(),
                 AnyQuantizationParams(),
                 fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{Pool2dParams{
                         OperandDataType::kFloat32,
                         mojom::Pool2d::Kind::kMaxPool2d,
                         RoundingType::kFloor,
                         /*batch=*/1,
                         /*channels=*/3,
                         /*input_height=*/4,
                         /*input_width=*/4,
                         /*padding=*/{{0, 0}, {0, 0}},
                         /*window_dimensions=*/{2, 2},
                         /*strides=*/{2, 2},
                         /*dilations=*/{1, 1},
                         /*is_input_constant=*/false,
                     },
                     QuantizationParams{
                         /*quantized_type=*/OperandDataType::kUint8,
                         QuantizationKind::kPerTensor,
                         // This is unused for per tensor quantization.
                         /*channel_block_size=*/1},
                     /*seed_for_data=*/2}}));

WEBNN_FUZZ_TEST_F(
    DQReduceQ,
    .WithDomains(AnyReduceParams(),
                 AnyQuantizationParams(),
                 /*channel_axis=*/fuzztest::InRange<uint32_t>(0, 7),
                 /*seed_for_input=*/fuzztest::Arbitrary<uint8_t>(),
                 /*seed_for_scale=*/
                 fuzztest::ElementOf({0.125f, 0.25f, 0.5f, 1.0f, 2.0f}),
                 /*seed_for_zero_point=*/fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{ReduceParams{
                         /*data_type=*/OperandDataType::kFloat32,
                         /*reduce_kind=*/mojom::Reduce::Kind::kMax,
                         /*rank=*/4,
                         /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                         /*num_axes=*/2,
                         /*axes=*/{2, 3, 0, 0, 0, 0, 0, 0},
                         /*keep_dimensions=*/true,
                         /*is_input_constant=*/false,
                     },
                     QuantizationParams{
                         /*quantized_type=*/OperandDataType::kUint8,
                         QuantizationKind::kPerTensor,
                         // This is unused for per tensor quantization.
                         /*channel_block_size=*/1},
                     /*channel_axis=*/1,
                     /*seed_for_input=*/2,
                     /*seed_for_scale=*/0.25f,
                     /*seed_for_zero_point=*/0}}));

WEBNN_FUZZ_TEST_F(
    DQResample2dQ,
    .WithDomains(AnyResample2dParams(),
                 AnyQuantizedDataType(),
                 /*seed_for_input=*/fuzztest::Arbitrary<uint8_t>(),
                 /*seed_for_scale=*/
                 fuzztest::ElementOf({0.125f, 0.25f, 0.5f, 1.0f, 2.0f}),
                 /*seed_for_zero_point=*/fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{Resample2dParams{
                         OperandDataType::kFloat32,
                         mojom::Resample2d::InterpolationMode::kNearestNeighbor,
                         /*batch=*/1,
                         /*channels=*/3,
                         /*input_height=*/4,
                         /*input_width=*/4,
                         /*use_sizes=*/false,
                         /*scale_height=*/2.0f,
                         /*scale_width=*/2.0f,
                         /*output_height=*/8,
                         /*output_width=*/8,
                         /*is_input_constant=*/false,
                     },
                     /*quantized_type=*/OperandDataType::kUint8,
                     /*seed_for_input=*/2,
                     /*seed_for_scale=*/0.25f,
                     /*seed_for_zero_point=*/0}}));

WEBNN_FUZZ_TEST_F(
    DQSliceQ,
    .WithDomains(AnySliceParams(),
                 AnyQuantizedDataType(),
                 /*seed_for_input=*/fuzztest::Arbitrary<uint8_t>(),
                 /*seed_for_scale=*/
                 fuzztest::ElementOf({0.125f, 0.25f, 0.5f, 1.0f, 2.0f}),
                 /*seed_for_zero_point=*/fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{SliceParams{
                         /*data_type=*/OperandDataType::kFloat32,
                         /*rank=*/4,
                         /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                         /*starts=*/{0, 1, 0, 0, 0, 0, 0, 0},
                         /*sizes=*/{1, 2, 4, 4, 1, 1, 1, 1},
                         /*strides=*/{1, 1, 2, 4, 1, 1, 1, 1},
                         /*is_input_constant=*/false,
                     },
                     /*quantized_type=*/OperandDataType::kUint8,
                     /*seed_for_input=*/2,
                     /*seed_for_scale=*/0.25f,
                     /*seed_for_zero_point=*/0}}));

WEBNN_FUZZ_TEST_F(
    DQSoftmaxQ,
    .WithDomains(AnySoftmaxParams(),
                 AnyQuantizedDataType(),
                 /*seed_for_input=*/fuzztest::Arbitrary<uint8_t>(),
                 /*seed_for_scale=*/
                 fuzztest::ElementOf({0.125f, 0.25f, 0.5f, 1.0f, 2.0f}),
                 /*seed_for_zero_point=*/fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{SoftmaxParams{
                         /*data_type=*/OperandDataType::kFloat32,
                         /*rank=*/4,
                         /*input_dims=*/{4, 5, 5, 6, 1, 1, 1, 1},
                         /*axis=*/2,
                         /*is_input_constant=*/false,
                     },
                     /*quantized_type=*/OperandDataType::kInt8,
                     /*seed_for_input=*/4,
                     /*seed_for_scale=*/0.25f,
                     /*seed_for_zero_point=*/0}}));

WEBNN_FUZZ_TEST_F(
    DQActivationQ,
    .WithDomains(AnyActivationParams(kAllActivationQuantizedKinds),
                 AnyQuantizedDataType(),
                 /*seed_for_input=*/fuzztest::Arbitrary<uint8_t>(),
                 /*seed_for_scale=*/
                 fuzztest::ElementOf({0.125f, 0.25f, 0.5f, 1.0f, 2.0f}),
                 /*seed_for_zero_point=*/fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{ActivationParams{
                         /*data_type=*/OperandDataType::kFloat32,
                         /*rank=*/4,
                         /*input_dims=*/{4, 5, 5, 6, 1, 1, 1, 1},
                         /*kind=*/ActivationKind::kTanh,
                         /*is_input_constant=*/false,
                     },
                     /*quantized_type=*/OperandDataType::kInt8,
                     /*seed_for_input=*/4,
                     /*seed_for_scale=*/0.25f,
                     /*seed_for_zero_point=*/0}}));

WEBNN_FUZZ_TEST_F(
    DQSplitQ,
    .WithDomains(AnySplitParams(),
                 AnyQuantizedDataType(),
                 /*seed_for_input=*/fuzztest::Arbitrary<uint8_t>(),
                 /*seed_for_scale=*/
                 fuzztest::ElementOf({0.125f, 0.25f, 0.5f, 1.0f, 2.0f}),
                 /*seed_for_zero_point=*/fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{SplitParams{
                         /*data_type=*/OperandDataType::kFloat32,
                         /*rank=*/4,
                         /*input_dims=*/{1, 6, 4, 4, 1, 1, 1, 1},
                         /*axis=*/1,
                         /*use_equal_splits=*/true,
                         /*num_splits=*/3,
                         /*split_sizes=*/{1},
                         /*is_input_constant=*/false,
                     },
                     /*quantized_type=*/OperandDataType::kUint8,
                     /*seed_for_input=*/2,
                     /*seed_for_scale=*/0.25f,
                     /*seed_for_zero_point=*/0}}));

WEBNN_FUZZ_TEST_F(
    DQTransposeQ,
    .WithDomains(AnyTransposeParams(),
                 AnyQuantizationParams(),
                 /*channel_axis=*/fuzztest::InRange<uint32_t>(0, 7),
                 /*seed_for_input=*/fuzztest::Arbitrary<uint8_t>(),
                 /*seed_for_scale=*/
                 fuzztest::ElementOf({0.125f, 0.25f, 0.5f, 1.0f, 2.0f}),
                 /*seed_for_zero_point=*/fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{TransposeParams{
                         /*data_type=*/OperandDataType::kFloat32,
                         /*rank=*/4,
                         /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                         /*permutation=*/{0, 5, 6, 3, 0, 0, 0, 0},
                         /*is_input_constant=*/false,
                     },
                     QuantizationParams{
                         /*quantized_type=*/OperandDataType::kUint8,
                         QuantizationKind::kPerTensor,
                         // This is unused for per tensor quantization.
                         /*channel_block_size=*/1},
                     /*channel_axis=*/1,
                     /*seed_for_input=*/2,
                     /*seed_for_scale=*/0.25f,
                     /*seed_for_zero_point=*/0}}));

}  // namespace webnn::test
