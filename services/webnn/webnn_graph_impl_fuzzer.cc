// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/debug/asan_service.h"
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

// Maximum number of inputs to concat.
constexpr uint32_t kMaxConcatInputs = 10;

#define ASSIGN_OR_RETURN_VOID(lhs, rexpr) \
  ASSIGN_OR_RETURN(lhs, rexpr, [](std::string error) { return; });

#define ASSIGN_OR_RETURN_NULLOPT(lhs, rexpr) \
  ASSIGN_OR_RETURN(lhs, rexpr, [](std::string error) { return std::nullopt; });

// Registers a fuzz test for all three device types (CPU, GPU, NPU).
// The variadic args carry the .WithDomains()/.WithSeeds() chain.
#define WEBNN_FUZZ_TEST_F(func, ...)                       \
  FUZZ_TEST_F(WebNNGraphImplFuzzer_CPU, func) __VA_ARGS__; \
  FUZZ_TEST_F(WebNNGraphImplFuzzer_GPU, func) __VA_ARGS__; \
  FUZZ_TEST_F(WebNNGraphImplFuzzer_NPU, func) __VA_ARGS__

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

struct BuildConv2dAttributes {
  std::vector<uint32_t> padding;
  std::vector<uint32_t> strides;
  std::vector<uint32_t> dilations;
  uint32_t groups;
};

struct BuildGemmAttributes {
  std::optional<OperandId> c_operand_id;
  float alpha;
  float beta;
  bool a_transpose;
  bool b_transpose;
};

struct BuildLstmAttributes {
  std::optional<OperandId> bias_operand_id;
  std::optional<OperandId> recurrent_bias_operand_id;
  std::optional<OperandId> peephole_weight_operand_id;
  std::optional<OperandId> initial_hidden_state_operand_id;
  std::optional<OperandId> initial_cell_state_operand_id;
  bool return_sequence;
  mojom::RecurrentNetworkDirection direction;
  mojom::LstmWeightLayout layout;
  std::vector<mojom::RecurrentNetworkActivation> activations;
};

struct BuildPool2dAttributes {
  std::vector<uint32_t> window_dimensions;
  std::vector<uint32_t> padding;
  std::vector<uint32_t> strides;
  std::vector<uint32_t> dilations;
};

// Tri-state for optional operands: not present, constant, or input.
enum class OptionalOperandKind : uint8_t {
  kNone = 0,
  kConstant = 1,
  kInput = 2,
};

enum class GemmCShapeKind : uint8_t {
  kScalar = 0,
  k1D = 1,
  k2D_1xN = 2,
  k2D_MxN = 3,
};

enum class QuantizationKind : uint32_t {
  kPerTensor = 0,
  kPerChannel = 1,
  kPerBlock = 2,
};

struct ConcatParams {
  OperandDataType data_type;
  uint32_t rank;
  std::array<uint32_t, 8> input_dims;
  uint32_t axis;
  // Number of inputs to concat. Must be >= 1.
  uint32_t num_inputs;
  // Dimension size along the concat axis for each additional input beyond the
  // first. Only the first `num_inputs - 1` entries are used.
  std::array<uint32_t, kMaxConcatInputs - 1> extra_axis_dims;
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
};

struct ExpandParams {
  OperandDataType data_type;
  uint32_t input_rank;
  uint32_t output_rank;
  std::array<uint32_t, 8> input_dims;
  std::array<uint32_t, 8> output_dims;
  bool is_input_constant;
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

struct Pool2dParams {
  OperandDataType data_type;
  mojom::Pool2d::Kind pool2d_kind;
  RoundingType rounding_type;
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

struct QuantizationParams {
  OperandDataType quantized_type;
  QuantizationKind quantization_kind;
  uint32_t channel_block_size;
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

auto AnyConv2dKind() {
  return fuzztest::ElementOf<mojom::Conv2d::Kind>(
      {mojom::Conv2d::Kind::kDirect, mojom::Conv2d::Kind::kTransposed});
}

auto AnyPool2dKind() {
  return fuzztest::ElementOf<mojom::Pool2d::Kind>(
      {mojom::Pool2d::Kind::kMaxPool2d, mojom::Pool2d::Kind::kAveragePool2d,
       mojom::Pool2d::Kind::kL2Pool2d});
}

auto AnyQuantizationKind() {
  return fuzztest::ElementOf<QuantizationKind>({QuantizationKind::kPerTensor,
                                                QuantizationKind::kPerChannel,
                                                QuantizationKind::kPerBlock});
}

auto AnyReduceKind() {
  return fuzztest::ElementOf<mojom::Reduce::Kind>(
      {mojom::Reduce::Kind::kL1, mojom::Reduce::Kind::kL2,
       mojom::Reduce::Kind::kLogSum, mojom::Reduce::Kind::kLogSumExp,
       mojom::Reduce::Kind::kMax, mojom::Reduce::Kind::kMean,
       mojom::Reduce::Kind::kMin, mojom::Reduce::Kind::kProduct,
       mojom::Reduce::Kind::kSum, mojom::Reduce::Kind::kSumSquare});
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

auto AnyConcatParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<ConcatParams>(
      AnyOperandDataTypeFor(limits.concat_inputs.data_types),
      fuzztest::InRange<uint32_t>(1, 8),                      // rank
      fuzztest::ArrayOf<8>(AnyDimSize()),                     // input_dims
      fuzztest::InRange<uint32_t>(0, 7),                      // axis
      fuzztest::InRange<uint32_t>(1, kMaxConcatInputs),       // num_inputs
      fuzztest::ArrayOf<kMaxConcatInputs - 1>(AnyDimSize()),  // extra_axis_dims
      fuzztest::Arbitrary<bool>()  // is_input_constant
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
      fuzztest::Arbitrary<bool>()     // is_depthwise
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
      // The 1.0f values are used to exercise the fusiable path for TFLite
      // backend:
      // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=2083;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
      fuzztest::OneOf(fuzztest::Just(1.0f),
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

auto AnyPool2dParams() {
  return fuzztest::FlatMap(
      [](mojom::Pool2d::Kind pool2d_kind) {
        return fuzztest::StructOf<Pool2dParams>(
            AnyOperandDataTypeFor(GetPool2dDataTypes(pool2d_kind)),
            fuzztest::Just(pool2d_kind), AnyRoundingType(),
            AnyDimSize(),                // batch
            AnyDimSize(),                // channels
            AnyDimSize(),                // input_height
            AnyDimSize(),                // input_width
            AnyPadding2d(),              // padding
            AnySize2d(),                 // window_dimensions
            AnyStrides2d(),              // strides
            AnyDilations2d(),            // dilations
            fuzztest::Arbitrary<bool>()  // is_input_constant
        );
      },
      AnyPool2dKind());
}

auto AnyQuantizationParams() {
  return fuzztest::StructOf<QuantizationParams>(
      AnyQuantizedDataType(), AnyQuantizationKind(),
      fuzztest::InRange<uint32_t>(  // channel_block_size
          1, std::numeric_limits<int16_t>::max()));
}

auto AnyReduceParams() {
  return fuzztest::FlatMap(
      [](mojom::Reduce::Kind reduce_kind) {
        return fuzztest::StructOf<ReduceParams>(
            AnyOperandDataTypeFor(GetReduceDataTypes(reduce_kind)),
            fuzztest::Just(reduce_kind),
            fuzztest::InRange<uint32_t>(1, 8),   // rank
            fuzztest::ArrayOf<8>(AnyDimSize()),  // input_dims
            fuzztest::InRange<uint32_t>(0, 8),   // num_axes
            fuzztest::ArrayOf<8>(                // axes
                fuzztest::InRange<uint32_t>(0, 7)),
            fuzztest::Arbitrary<bool>(),  // keep_dimensions
            fuzztest::Arbitrary<bool>()   // is_input_constant
        );
      },
      AnyReduceKind());
}

auto AnyScatterElementsParams() {
  const auto& limits = GetContextPropertiesForTesting().data_type_limits;
  return fuzztest::StructOf<ScatterElementsParams>(
      AnyOperandDataTypeFor(limits.scatter_elements_input.data_types),
      AnyOperandDataTypeFor(limits.scatter_elements_indices.data_types),
      fuzztest::InRange<uint32_t>(1, 8),   // rank
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

// Build a constant operand from float values, converting to the appropriate
// byte representation based on the descriptor's data type (float32 or float16).
OperandId BuildFloatConstant(GraphInfoBuilder& builder,
                             const OperandDescriptor& desc,
                             const std::vector<float>& values) {
  CHECK(desc.data_type() == OperandDataType::kFloat32 ||
        desc.data_type() == OperandDataType::kFloat16);
  if (desc.data_type() == OperandDataType::kFloat32) {
    return builder.BuildConstant(
        desc.shape(), desc.data_type(),
        base::as_byte_span(base::allow_nonunique_obj, values));
  }
  // float16: convert each float to its 16-bit IEEE precision format.
  std::vector<uint16_t> f16_values(values.size());
  for (size_t i = 0; i < values.size(); ++i) {
    f16_values[i] = fp16_ieee_from_fp32_value(values[i]);
  }
  return builder.BuildConstant(desc.shape(), desc.data_type(),
                               base::as_byte_span(f16_values));
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
  input_descs.reserve(params.num_inputs);

  // First input uses base_dims.
  ASSIGN_OR_RETURN_NULLOPT(
      auto first_desc,
      OperandDescriptor::Create(context_properties, params.data_type, base_dims,
                                ""));
  input_descs.push_back(std::move(first_desc));

  // Additional inputs share all dims except the concat axis.
  for (uint32_t i = 1; i < params.num_inputs; ++i) {
    std::vector<uint32_t> dims = base_dims;
    dims[params.axis] = params.extra_axis_dims[i - 1];
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
          filter_dims = {params.input_channels, params.filter_dimensions.height,
                         params.filter_dimensions.width, 1};
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
  attr.rounding_type = params.rounding_type;

  ASSIGN_OR_RETURN_NULLOPT(
      auto output_desc,
      ValidatePool2dAndInferOutput(context_properties, input_desc, attr,
                                   FromMojoPool2dType(params.pool2d_kind)));

  return Pool2dDescriptors{
      .input_desc = std::move(input_desc),
      .output_desc = std::move(output_desc),
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

    webnn_test_environment_ = std::make_unique<WebNNTestEnvironment>();

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
    mojo::AssociatedRemote<mojom::WebNNGraphBuilder> graph_builder_remote,
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

  mojo::AssociatedRemote<mojom::WebNNGraph> graph_remote;
  graph_remote.Bind(std::move(create_graph_result.value()->graph_remote));
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

  graph_remote.reset();
  graph_builder_remote.reset();
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

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> BindNewGraphBuilderRemote();

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

mojo::AssociatedRemote<mojom::WebNNGraphBuilder>
WebNNGraphImplFuzzerBase::BindNewGraphBuilderRemote() {
  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> remote;
  context_->CreateGraphBuilder(remote.BindNewEndpointAndPassReceiver());
  return remote;
}

template <typename BaseFixture>
class WebNNGraphImplFuzzerImpl
    : public fuzztest::PerFuzzTestFixtureAdapter<BaseFixture> {
 public:
  void SingleOpConcat(ConcatParams params, uint8_t seed_for_data);
  void SingleOpConv2d(Conv2dParams params, uint8_t seed_for_data);
  void SingleOpExpand(ExpandParams params, uint8_t seed_for_data);
  void SingleOpGatherND(GatherNDParams params, uint8_t seed_for_data);
  void SingleOpGemm(GemmParams params, uint8_t seed_for_data);
  void SingleOpLstm(LstmParams params, uint8_t seed_for_data);
  void SingleOpPool2d(Pool2dParams params, uint8_t seed_for_data);
  void SingleOpReduce(ReduceParams params, uint8_t seed_for_data);
  void SingleOpScatterElements(ScatterElementsParams params,
                               uint8_t seed_for_data);
  void SubgraphDQConcatQ(ConcatParams concat_params,
                         OperandDataType quantized_type,
                         uint8_t seed_for_input,
                         float seed_for_scale,
                         uint8_t seed_for_zero_point);
  void SubgraphDQConv2dQ(Conv2dParams conv2d_params,
                         QuantizationParams quantization_params,
                         uint8_t seed_for_data);
  void SubgraphDQGemmQ(GemmParams gemm_params,
                       QuantizationParams quantization_params,
                       uint8_t seed_for_data);
  void SubgraphDQPool2dQ(Pool2dParams pool2d_params,
                         QuantizationParams quantization_params,
                         uint8_t seed_for_data);
  void SubgraphDQReduceQ(ReduceParams reduce_params,
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

class WebNNGraphImplFuzzer_CPU
    : public WebNNGraphImplFuzzerImpl<
          WebNNGraphImplFuzzerDevice<mojom::Device::kCpu>> {};

class WebNNGraphImplFuzzer_GPU
    : public WebNNGraphImplFuzzerImpl<
          WebNNGraphImplFuzzerDevice<mojom::Device::kGpu>> {};

class WebNNGraphImplFuzzer_NPU
    : public WebNNGraphImplFuzzerImpl<
          WebNNGraphImplFuzzerDevice<mojom::Device::kNpu>> {};

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::SingleOpConcat(
    ConcatParams params,
    uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto concat_descs,
      SetUpConcatDescriptors(this->context_properties(), params));

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> remote =
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
    input_data_buffers.emplace_back(desc.PackedByteLength(), seed_for_data);

    OperandId input_id;
    if (params.is_input_constant) {
      input_id = builder.BuildConstant(desc.shape(), desc.data_type(),
                                       input_data_buffers.back());
    } else {
      std::string name = "input" + base::NumberToString(i);
      input_id = builder.BuildInput(name, desc.shape(), desc.data_type());
      named_inputs.insert({std::move(name), input_data_buffers.back()});
    }
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
void WebNNGraphImplFuzzerImpl<BaseFixture>::SingleOpConv2d(
    Conv2dParams params,
    uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto conv2d_descs,
      SetUpConv2dDescriptors(this->context_properties(), params));

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  OperandId input_id;
  OperandId filter_id;
  std::optional<OperandId> bias_id;
  std::vector<uint8_t> input_data(conv2d_descs.input_desc.PackedByteLength(),
                                  seed_for_data);
  std::vector<uint8_t> filter_data(conv2d_descs.filter_desc.PackedByteLength(),
                                   seed_for_data);
  std::vector<uint8_t> bias_data;
  if (conv2d_descs.bias_desc.has_value()) {
    bias_data.resize(conv2d_descs.bias_desc->PackedByteLength(), seed_for_data);
  }

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  if (params.is_input_constant) {
    input_id =
        builder.BuildConstant(conv2d_descs.input_desc.shape(),
                              conv2d_descs.input_desc.data_type(), input_data);
  } else {
    input_id = builder.BuildInput("input", conv2d_descs.input_desc.shape(),
                                  conv2d_descs.input_desc.data_type());
    named_inputs.insert({"input", input_data});
  }
  if (params.is_filter_constant) {
    filter_id = builder.BuildConstant(conv2d_descs.filter_desc.shape(),
                                      conv2d_descs.filter_desc.data_type(),
                                      filter_data);
  } else {
    filter_id = builder.BuildInput("filter", conv2d_descs.filter_desc.shape(),
                                   conv2d_descs.filter_desc.data_type());
    named_inputs.insert({"filter", filter_data});
  }

  switch (params.bias_kind) {
    case OptionalOperandKind::kNone:
      break;
    case OptionalOperandKind::kInput: {
      bias_id = builder.BuildInput("bias", conv2d_descs.bias_desc->shape(),
                                   conv2d_descs.bias_desc->data_type());
      named_inputs.insert({"bias", bias_data});
      break;
    }
    case OptionalOperandKind::kConstant: {
      bias_id =
          builder.BuildConstant(conv2d_descs.bias_desc->shape(),
                                conv2d_descs.bias_desc->data_type(), bias_data);
      break;
    }
  }

  OperandId output_id =
      builder.BuildOutput("output", conv2d_descs.output_desc.shape(),
                          conv2d_descs.output_desc.data_type());

  BuildConv2dAttributes conv2d_attr;
  conv2d_attr.padding = {
      params.padding.beginning.height, params.padding.ending.height,
      params.padding.beginning.width, params.padding.ending.width};
  conv2d_attr.strides = {params.strides.height, params.strides.width};
  conv2d_attr.dilations = {params.dilations.height, params.dilations.width};
  conv2d_attr.groups = params.groups;
  builder.BuildConv2d(params.conv2d_kind, input_id, filter_id, output_id,
                      conv2d_attr, bias_id);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::SingleOpExpand(
    ExpandParams params,
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

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  std::vector<uint8_t> input_data(input_desc.PackedByteLength(), seed_for_data);

  OperandId input_id;
  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  if (params.is_input_constant) {
    input_id = builder.BuildConstant(input_desc.shape(), input_desc.data_type(),
                                     input_data);
  } else {
    input_id =
        builder.BuildInput("input", input_desc.shape(), input_desc.data_type());
    named_inputs.insert({"input", input_data});
  }

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
void WebNNGraphImplFuzzerImpl<BaseFixture>::SingleOpGatherND(
    GatherNDParams params,
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

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  std::vector<uint8_t> input_data(input_desc.PackedByteLength(), seed_for_data);

  std::vector<uint8_t> indices_data = CreateBufferAsIndicesType(
      indices_desc.PackedByteLength(), params.indices_data_type,
      params.indices_fill_value);

  OperandId input_id;
  OperandId indices_id;

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  if (params.is_input_constant) {
    input_id = builder.BuildConstant(input_desc.shape(), input_desc.data_type(),
                                     input_data);
  } else {
    input_id =
        builder.BuildInput("input", input_desc.shape(), input_desc.data_type());
    named_inputs.insert({"input", input_data});
  }
  if (params.is_indices_constant) {
    indices_id = builder.BuildConstant(indices_desc.shape(),
                                       indices_desc.data_type(), indices_data);
  } else {
    indices_id = builder.BuildInput("indices", indices_desc.shape(),
                                    indices_desc.data_type());
    named_inputs.insert({"indices", indices_data});
  }

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
void WebNNGraphImplFuzzerImpl<BaseFixture>::SingleOpGemm(
    GemmParams params,
    uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto gemm_descs,
      SetUpGemmDescriptors(this->context_properties(), params));

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  OperandId a_id;
  OperandId b_id;
  std::vector<uint8_t> a_data(gemm_descs.a_desc.PackedByteLength(),
                              seed_for_data);
  std::vector<uint8_t> b_data(gemm_descs.b_desc.PackedByteLength(),
                              seed_for_data);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  if (params.is_a_constant) {
    a_id = builder.BuildConstant(gemm_descs.a_desc.shape(),
                                 gemm_descs.a_desc.data_type(), a_data);
  } else {
    a_id = builder.BuildInput("a", gemm_descs.a_desc.shape(),
                              gemm_descs.a_desc.data_type());
    named_inputs.insert({"a", a_data});
  }
  if (params.is_b_constant) {
    b_id = builder.BuildConstant(gemm_descs.b_desc.shape(),
                                 gemm_descs.b_desc.data_type(), b_data);
  } else {
    b_id = builder.BuildInput("b", gemm_descs.b_desc.shape(),
                              gemm_descs.b_desc.data_type());
    named_inputs.insert({"b", b_data});
  }

  BuildGemmAttributes gemm_attr;
  gemm_attr.alpha = params.alpha;
  gemm_attr.beta = params.beta;
  gemm_attr.a_transpose = params.a_transpose;
  gemm_attr.b_transpose = params.b_transpose;

  std::vector<uint8_t> c_data;
  if (params.has_c) {
    c_data.assign(gemm_descs.c_desc->PackedByteLength(), seed_for_data);
    if (params.is_c_constant) {
      OperandId c_id = builder.BuildConstant(
          gemm_descs.c_desc->shape(), gemm_descs.c_desc->data_type(), c_data);
      gemm_attr.c_operand_id = c_id;
    } else {
      OperandId c_id = builder.BuildInput("c", gemm_descs.c_desc->shape(),
                                          gemm_descs.c_desc->data_type());
      named_inputs.insert({"c", c_data});
      gemm_attr.c_operand_id = c_id;
    }
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
void WebNNGraphImplFuzzerImpl<BaseFixture>::SingleOpLstm(
    LstmParams params,
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

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  std::vector<uint8_t> input_data(input_desc.PackedByteLength(), seed_for_data);
  std::vector<uint8_t> weight_data(weight_desc.PackedByteLength(),
                                   seed_for_data);
  std::vector<uint8_t> recurrent_weight_data(
      recurrent_weight_desc.PackedByteLength(), seed_for_data);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;

  OperandId input_id;
  if (params.is_input_constant) {
    input_id = builder.BuildConstant(input_desc.shape(), input_desc.data_type(),
                                     input_data);
  } else {
    input_id =
        builder.BuildInput("input", input_desc.shape(), input_desc.data_type());
    named_inputs.insert({"input", input_data});
  }

  OperandId weight_id;
  if (params.is_weight_constant) {
    weight_id = builder.BuildConstant(weight_desc.shape(),
                                      weight_desc.data_type(), weight_data);
  } else {
    weight_id = builder.BuildInput("weight", weight_desc.shape(),
                                   weight_desc.data_type());
    named_inputs.insert({"weight", weight_data});
  }
  OperandId recurrent_weight_id;
  if (params.is_recurrent_weight_constant) {
    recurrent_weight_id = builder.BuildConstant(
        recurrent_weight_desc.shape(), recurrent_weight_desc.data_type(),
        recurrent_weight_data);
  } else {
    recurrent_weight_id =
        builder.BuildInput("recurrent_weight", recurrent_weight_desc.shape(),
                           recurrent_weight_desc.data_type());
    named_inputs.insert({"recurrent_weight", recurrent_weight_data});
  }

  BuildLstmAttributes lstm_attr;
  lstm_attr.return_sequence = params.return_sequence;
  lstm_attr.direction = params.direction;
  lstm_attr.layout = params.layout;
  lstm_attr.activations.assign(params.activations.begin(),
                               params.activations.end());

  // Owns data buffers for optional operands built as inputs.
  std::vector<std::vector<uint8_t>> optional_operand_data;
  optional_operand_data.reserve(5);

  auto build_optional_operand =
      [&](const std::optional<OperandDescriptor>& desc,
          OptionalOperandKind state,
          std::string name) -> std::optional<OperandId> {
    switch (state) {
      case OptionalOperandKind::kNone: {
        return std::nullopt;
      }
      case OptionalOperandKind::kConstant: {
        // `BuildConstant()` copies the data internally.
        std::vector<uint8_t> data(desc->PackedByteLength(), seed_for_data);
        return builder.BuildConstant(desc->shape(), desc->data_type(), data);
      }
      case OptionalOperandKind::kInput: {
        optional_operand_data.emplace_back(desc->PackedByteLength(),
                                           seed_for_data);
        OperandId id =
            builder.BuildInput(name, desc->shape(), desc->data_type());
        named_inputs.insert({std::move(name), optional_operand_data.back()});
        return id;
      }
    }
  };

  lstm_attr.bias_operand_id =
      build_optional_operand(bias_desc, params.bias_kind, "bias");
  lstm_attr.recurrent_bias_operand_id = build_optional_operand(
      recurrent_bias_desc, params.recurrent_bias_kind, "recurrent_bias");
  lstm_attr.peephole_weight_operand_id = build_optional_operand(
      peephole_weight_desc, params.peephole_weight_kind, "peephole_weight");
  lstm_attr.initial_hidden_state_operand_id = build_optional_operand(
      initial_hidden_state_desc, params.initial_hidden_state_kind,
      "initial_hidden_state");
  lstm_attr.initial_cell_state_operand_id = build_optional_operand(
      initial_cell_state_desc, params.initial_cell_state_kind,
      "initial_cell_state");

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
void WebNNGraphImplFuzzerImpl<BaseFixture>::SingleOpPool2d(
    Pool2dParams params,
    uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto pool2d_descs,
      SetUpPool2dDescriptors(this->context_properties(), params));

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  OperandId input_id;
  std::vector<uint8_t> input_data(pool2d_descs.input_desc.PackedByteLength(),
                                  seed_for_data);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  if (params.is_input_constant) {
    input_id =
        builder.BuildConstant(pool2d_descs.input_desc.shape(),
                              pool2d_descs.input_desc.data_type(), input_data);
  } else {
    input_id = builder.BuildInput("input", pool2d_descs.input_desc.shape(),
                                  pool2d_descs.input_desc.data_type());
    named_inputs.insert({"input", input_data});
  }

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
void WebNNGraphImplFuzzerImpl<BaseFixture>::SingleOpReduce(
    ReduceParams params,
    uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto reduce_descs,
      SetUpReduceDescriptors(this->context_properties(), params));

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  OperandId input_id;
  std::vector<uint8_t> input_data(reduce_descs.input_desc.PackedByteLength(),
                                  seed_for_data);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  if (params.is_input_constant) {
    input_id =
        builder.BuildConstant(reduce_descs.input_desc.shape(),
                              reduce_descs.input_desc.data_type(), input_data);
  } else {
    input_id = builder.BuildInput("input", reduce_descs.input_desc.shape(),
                                  reduce_descs.input_desc.data_type());
    named_inputs.insert({"input", input_data});
  }

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
void WebNNGraphImplFuzzerImpl<BaseFixture>::SingleOpScatterElements(
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

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  std::vector<uint8_t> input_data(input_desc.PackedByteLength(), seed_for_data);
  std::vector<uint8_t> updates_data(updates_desc.PackedByteLength(),
                                    seed_for_data);

  std::vector<uint8_t> indices_data = CreateBufferAsIndicesType(
      indices_desc.PackedByteLength(), params.indices_data_type,
      params.indices_fill_value);

  OperandId input_id;
  OperandId indices_id;
  OperandId updates_id;

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  if (params.is_input_constant) {
    input_id = builder.BuildConstant(input_desc.shape(), input_desc.data_type(),
                                     input_data);
  } else {
    input_id =
        builder.BuildInput("input", input_desc.shape(), input_desc.data_type());
    named_inputs.insert({"input", input_data});
  }
  if (params.is_indices_constant) {
    indices_id = builder.BuildConstant(indices_desc.shape(),
                                       indices_desc.data_type(), indices_data);
  } else {
    indices_id = builder.BuildInput("indices", indices_desc.shape(),
                                    indices_desc.data_type());
    named_inputs.insert({"indices", indices_data});
  }
  if (params.is_updates_constant) {
    updates_id = builder.BuildConstant(updates_desc.shape(),
                                       updates_desc.data_type(), updates_data);
  } else {
    updates_id = builder.BuildInput("updates", updates_desc.shape(),
                                    updates_desc.data_type());
    named_inputs.insert({"updates", updates_data});
  }

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
void WebNNGraphImplFuzzerImpl<BaseFixture>::SubgraphDQConcatQ(
    ConcatParams concat_params,
    OperandDataType quantized_type,
    uint8_t seed_for_input,
    float seed_for_scale,
    uint8_t seed_for_zero_point) {
  ASSIGN_OR_RETURN_VOID(
      auto concat_descs,
      SetUpConcatDescriptors(this->context_properties(), concat_params));

  // kPerTensor quantization is used to exercise the fusiable path for TFLite
  // backend:
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=1845;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
  // TODO(crbug.com/498987226): Remove this restriction to increase test
  // coverage.
  QuantizationParams per_tensor_quantization_params{
      .quantized_type = quantized_type,
      .quantization_kind = QuantizationKind::kPerTensor,
      .channel_block_size = 1};

  const size_t input_num = concat_descs.input_descs.size();

  // Build dequantize descriptors for each input.
  std::vector<OperandDescriptor> input_dq_descs;
  std::vector<OperandDescriptor> input_scale_descs;
  std::vector<OperandDescriptor> input_zero_descs;
  input_dq_descs.reserve(input_num);
  input_scale_descs.reserve(input_num);
  input_zero_descs.reserve(input_num);

  for (const auto& input_desc : concat_descs.input_descs) {
    auto scale_shape = ComputeQuantizationScaleShape(
        input_desc.shape(), per_tensor_quantization_params);

    ASSIGN_OR_RETURN_VOID(
        auto dq_desc,
        OperandDescriptor::Create(this->context_properties(), quantized_type,
                                  input_desc.shape(), ""));
    ASSIGN_OR_RETURN_VOID(
        auto scale_desc,
        OperandDescriptor::Create(this->context_properties(),
                                  concat_params.data_type, scale_shape, ""));
    ASSIGN_OR_RETURN_VOID(auto zero_desc, OperandDescriptor::Create(
                                              this->context_properties(),
                                              quantized_type, scale_shape, ""));

    ASSIGN_OR_RETURN_VOID(
        auto desc_result,
        ValidateDequantizeLinearAndInferOutput(
            this->context_properties(), dq_desc, scale_desc, zero_desc, ""));

    input_dq_descs.push_back(std::move(dq_desc));
    input_scale_descs.push_back(std::move(scale_desc));
    input_zero_descs.push_back(std::move(zero_desc));
  }

  // Build quantize descriptors for output.
  auto output_scale_shape = ComputeQuantizationScaleShape(
      concat_descs.output_desc.shape(), per_tensor_quantization_params);

  ASSIGN_OR_RETURN_VOID(auto output_scale_desc,
                        OperandDescriptor::Create(this->context_properties(),
                                                  concat_params.data_type,
                                                  output_scale_shape, ""));
  ASSIGN_OR_RETURN_VOID(
      auto output_zero_desc,
      OperandDescriptor::Create(this->context_properties(), quantized_type,
                                output_scale_shape, ""));

  ASSIGN_OR_RETURN_VOID(
      auto quantized_output_desc,
      ValidateQuantizeLinearAndInferOutput(
          this->context_properties(), concat_descs.output_desc,
          output_scale_desc, output_zero_desc, ""));

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  std::vector<std::vector<uint8_t>> input_dq_data_buffers;
  std::vector<std::vector<float>> input_scale_data_buffers;
  std::vector<std::vector<uint8_t>> input_zero_data_buffers;
  input_dq_data_buffers.reserve(input_num);
  input_scale_data_buffers.reserve(input_num);
  input_zero_data_buffers.reserve(input_num);
  std::vector<OperandId> concat_input_ids;
  concat_input_ids.reserve(input_num);

  for (size_t i = 0; i < input_num; ++i) {
    input_dq_data_buffers.emplace_back(input_dq_descs[i].PackedByteLength(),
                                       seed_for_input);
    input_scale_data_buffers.emplace_back(
        input_scale_descs[i].NumberOfElements(), seed_for_scale);
    input_zero_data_buffers.emplace_back(input_zero_descs[i].PackedByteLength(),
                                         seed_for_zero_point);

    OperandId input_dq_id;
    if (concat_params.is_input_constant) {
      input_dq_id = builder.BuildConstant(input_dq_descs[i].shape(),
                                          input_dq_descs[i].data_type(),
                                          input_dq_data_buffers.back());
    } else {
      std::string name = "input" + base::NumberToString(i);
      input_dq_id = builder.BuildInput(name, input_dq_descs[i].shape(),
                                       input_dq_descs[i].data_type());
      named_inputs.insert({std::move(name), input_dq_data_buffers.back()});
    }

    OperandId input_scale_id = BuildFloatConstant(
        builder, input_scale_descs[i], input_scale_data_buffers.back());
    OperandId input_zero_id = builder.BuildConstant(
        input_zero_descs[i].shape(), input_zero_descs[i].data_type(),
        input_zero_data_buffers.back());

    OperandId concat_input_id = builder.BuildIntermediateOperand(
        concat_descs.input_descs[i].shape(),
        concat_descs.input_descs[i].data_type());

    builder.BuildDequantizeLinear(input_dq_id, input_scale_id, input_zero_id,
                                  concat_input_id);
    concat_input_ids.push_back(concat_input_id);
  }

  OperandId concat_output_id = builder.BuildIntermediateOperand(
      concat_descs.output_desc.shape(), concat_descs.output_desc.data_type());

  builder.BuildConcat(std::move(concat_input_ids), concat_output_id,
                      concat_descs.axis);

  std::vector<float> output_scale_data(output_scale_desc.NumberOfElements(),
                                       seed_for_scale);
  std::vector<uint8_t> output_zero_data(output_zero_desc.PackedByteLength(),
                                        seed_for_zero_point);
  OperandId output_scale_id =
      BuildFloatConstant(builder, output_scale_desc, output_scale_data);
  OperandId output_zero_id = builder.BuildConstant(
      output_zero_desc.shape(), output_zero_desc.data_type(), output_zero_data);

  OperandId quantize_output_id =
      builder.BuildOutput("output", quantized_output_desc.shape(),
                          quantized_output_desc.data_type());
  builder.BuildQuantizeLinear(concat_output_id, output_scale_id, output_zero_id,
                              quantize_output_id);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::SubgraphDQConv2dQ(
    Conv2dParams conv2d_params,
    QuantizationParams quantization_params,
    uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto conv2d_descs,
      SetUpConv2dDescriptors(this->context_properties(), conv2d_params));

  OperandDataType quantized_type = quantization_params.quantized_type;
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

  auto input_scale_shape = ComputeQuantizationScaleShape(
      conv2d_descs.input_desc.shape(), quantization_params, input_channel_axis);
  auto filter_scale_shape =
      ComputeQuantizationScaleShape(conv2d_descs.filter_desc.shape(),
                                    quantization_params, filter_channel_axis);
  std::vector<uint32_t> bias_scale_shape;
  if (conv2d_descs.bias_desc.has_value()) {
    bias_scale_shape =
        ComputeQuantizationScaleShape(conv2d_descs.bias_desc->shape(),
                                      quantization_params, bias_channel_axis);
  }

  ASSIGN_OR_RETURN_VOID(
      auto input_dq_desc,
      OperandDescriptor::Create(this->context_properties(), quantized_type,
                                conv2d_descs.input_desc.shape(), ""));
  ASSIGN_OR_RETURN_VOID(auto input_scale_desc,
                        OperandDescriptor::Create(this->context_properties(),
                                                  conv2d_params.data_type,
                                                  input_scale_shape, ""));
  ASSIGN_OR_RETURN_VOID(
      auto input_zero_desc,
      OperandDescriptor::Create(this->context_properties(), quantized_type,
                                input_scale_shape, ""));
  ASSIGN_OR_RETURN_VOID(
      auto filter_dq_desc,
      OperandDescriptor::Create(this->context_properties(), quantized_type,
                                conv2d_descs.filter_desc.shape(), ""));
  ASSIGN_OR_RETURN_VOID(auto filter_scale_desc,
                        OperandDescriptor::Create(this->context_properties(),
                                                  conv2d_params.data_type,
                                                  filter_scale_shape, ""));
  ASSIGN_OR_RETURN_VOID(
      auto filter_zero_desc,
      OperandDescriptor::Create(this->context_properties(), quantized_type,
                                filter_scale_shape, ""));
  // "kInt32" is necessary to exercise the fusiable path for TFLite backend:
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=1746;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a;
  // TODO(crbug.com/498987226): Remove this restriction to increase test
  // coverage.
  std::optional<OperandDescriptor> bias_dq_desc;
  std::optional<OperandDescriptor> bias_scale_desc;
  std::optional<OperandDescriptor> bias_zero_desc;
  if (conv2d_descs.bias_desc.has_value()) {
    ASSIGN_OR_RETURN_VOID(
        bias_dq_desc, OperandDescriptor::Create(
                          this->context_properties(), OperandDataType::kInt32,
                          conv2d_descs.bias_desc->shape(), ""));
    ASSIGN_OR_RETURN_VOID(bias_scale_desc,
                          OperandDescriptor::Create(this->context_properties(),
                                                    conv2d_params.data_type,
                                                    bias_scale_shape, ""));
    ASSIGN_OR_RETURN_VOID(bias_zero_desc,
                          OperandDescriptor::Create(this->context_properties(),
                                                    OperandDataType::kInt32,
                                                    bias_scale_shape, ""));
  }

  ASSIGN_OR_RETURN_VOID(auto input_desc_result,
                        ValidateDequantizeLinearAndInferOutput(
                            this->context_properties(), input_dq_desc,
                            input_scale_desc, input_zero_desc, ""));
  ASSIGN_OR_RETURN_VOID(auto filter_desc_result,
                        ValidateDequantizeLinearAndInferOutput(
                            this->context_properties(), filter_dq_desc,
                            filter_scale_desc, filter_zero_desc, ""));
  std::optional<OperandDescriptor> bias_desc_result;
  if (bias_dq_desc.has_value()) {
    ASSIGN_OR_RETURN_VOID(bias_desc_result,
                          ValidateDequantizeLinearAndInferOutput(
                              this->context_properties(), *bias_dq_desc,
                              *bias_scale_desc, *bias_zero_desc, ""));
  }

  auto output_scale_shape =
      ComputeQuantizationScaleShape(conv2d_descs.output_desc.shape(),
                                    quantization_params, output_channel_axis);

  ASSIGN_OR_RETURN_VOID(auto output_scale_desc,
                        OperandDescriptor::Create(this->context_properties(),
                                                  conv2d_params.data_type,
                                                  output_scale_shape, ""));
  ASSIGN_OR_RETURN_VOID(
      auto output_zero_desc,
      OperandDescriptor::Create(this->context_properties(), quantized_type,
                                output_scale_shape, ""));

  ASSIGN_OR_RETURN_VOID(
      auto quantized_output_desc,
      ValidateQuantizeLinearAndInferOutput(
          this->context_properties(), conv2d_descs.output_desc,
          output_scale_desc, output_zero_desc, ""));

  std::vector<uint8_t> input_dq_data(input_dq_desc.PackedByteLength(),
                                     seed_for_data);
  std::vector<uint8_t> filter_dq_data(filter_dq_desc.PackedByteLength(),
                                      seed_for_data);
  std::vector<uint8_t> bias_dq_data;
  if (bias_dq_desc.has_value()) {
    bias_dq_data.assign(bias_dq_desc->PackedByteLength(), seed_for_data);
  }
  // These values are used to exercise the fusiable path for TFLite backend:
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=1809;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=1754;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
  // TODO(crbug.com/498987226): Remove this restriction to increase test
  // coverage.
  std::vector<float> input_scale_data(input_scale_desc.NumberOfElements(),
                                      0.5f);
  std::vector<float> filter_scale_data(filter_scale_desc.NumberOfElements(),
                                       0.25f);
  std::vector<float> bias_scale_data;
  if (bias_scale_desc.has_value()) {
    bias_scale_data.assign(bias_scale_desc->NumberOfElements(), 0.125f);
  }
  std::vector<float> output_scale_data(output_scale_desc.NumberOfElements(),
                                       0.125f);
  std::vector<uint8_t> input_zero_data(input_zero_desc.PackedByteLength(), 0);
  std::vector<uint8_t> filter_zero_data(filter_zero_desc.PackedByteLength(), 0);
  std::vector<uint8_t> bias_zero_data;
  if (bias_zero_desc.has_value()) {
    bias_zero_data.assign(bias_zero_desc->PackedByteLength(), 0);
  }
  std::vector<uint8_t> output_zero_data(output_zero_desc.PackedByteLength(), 0);

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  OperandId input_dq_id;
  OperandId filter_dq_id;
  std::optional<OperandId> bias_dq_id;
  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;

  if (conv2d_params.is_input_constant) {
    input_dq_id = builder.BuildConstant(
        input_dq_desc.shape(), input_dq_desc.data_type(), input_dq_data);
  } else {
    input_dq_id = builder.BuildInput("input", input_dq_desc.shape(),
                                     input_dq_desc.data_type());
    named_inputs.insert({"input", input_dq_data});
  }
  if (conv2d_params.is_filter_constant) {
    filter_dq_id = builder.BuildConstant(
        filter_dq_desc.shape(), filter_dq_desc.data_type(), filter_dq_data);
  } else {
    filter_dq_id = builder.BuildInput("filter", filter_dq_desc.shape(),
                                      filter_dq_desc.data_type());
    named_inputs.insert({"filter", filter_dq_data});
  }
  switch (conv2d_params.bias_kind) {
    case OptionalOperandKind::kNone:
      break;
    case OptionalOperandKind::kInput:
      bias_dq_id = builder.BuildInput("bias", bias_dq_desc->shape(),
                                      bias_dq_desc->data_type());
      named_inputs.insert({"bias", bias_dq_data});
      break;
    case OptionalOperandKind::kConstant:
      bias_dq_id = builder.BuildConstant(
          bias_dq_desc->shape(), bias_dq_desc->data_type(), bias_dq_data);
      break;
  }

  OperandId input_scale_id =
      BuildFloatConstant(builder, input_scale_desc, input_scale_data);
  OperandId input_zero_id = builder.BuildConstant(
      input_zero_desc.shape(), input_zero_desc.data_type(), input_zero_data);
  OperandId filter_scale_id =
      BuildFloatConstant(builder, filter_scale_desc, filter_scale_data);
  OperandId filter_zero_id = builder.BuildConstant(
      filter_zero_desc.shape(), filter_zero_desc.data_type(), filter_zero_data);
  std::optional<OperandId> bias_scale_id;
  std::optional<OperandId> bias_zero_id;
  if (bias_scale_desc.has_value()) {
    bias_scale_id =
        BuildFloatConstant(builder, *bias_scale_desc, bias_scale_data);
    bias_zero_id = builder.BuildConstant(
        bias_zero_desc->shape(), bias_zero_desc->data_type(), bias_zero_data);
  }
  OperandId output_scale_id =
      BuildFloatConstant(builder, output_scale_desc, output_scale_data);
  OperandId output_zero_id = builder.BuildConstant(
      output_zero_desc.shape(), output_zero_desc.data_type(), output_zero_data);

  OperandId conv2d_input_id = builder.BuildIntermediateOperand(
      conv2d_descs.input_desc.shape(), conv2d_descs.input_desc.data_type());
  OperandId conv2d_filter_id = builder.BuildIntermediateOperand(
      conv2d_descs.filter_desc.shape(), conv2d_descs.filter_desc.data_type());
  std::optional<OperandId> conv2d_bias_id;
  if (bias_dq_id.has_value()) {
    conv2d_bias_id = builder.BuildIntermediateOperand(
        conv2d_descs.bias_desc->shape(), conv2d_descs.bias_desc->data_type());
  }

  builder.BuildDequantizeLinear(input_dq_id, input_scale_id, input_zero_id,
                                conv2d_input_id);
  builder.BuildDequantizeLinear(filter_dq_id, filter_scale_id, filter_zero_id,
                                conv2d_filter_id);
  if (bias_dq_id.has_value()) {
    builder.BuildDequantizeLinear(*bias_dq_id, *bias_scale_id, *bias_zero_id,
                                  *conv2d_bias_id);
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

  OperandId quantize_output_id =
      builder.BuildOutput("output", quantized_output_desc.shape(),
                          quantized_output_desc.data_type());
  builder.BuildQuantizeLinear(conv_output_id, output_scale_id, output_zero_id,
                              quantize_output_id);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::SubgraphDQGemmQ(
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

  OperandDataType quantized_type = quantization_params.quantized_type;
  const uint32_t b_channel_axis = gemm_params.b_transpose ? 0u : 1u;

  auto a_scale_shape = ComputeQuantizationScaleShape(
      gemm_descs.a_desc.shape(), per_tensor_quantization_params);
  auto b_scale_shape = ComputeQuantizationScaleShape(
      gemm_descs.b_desc.shape(), quantization_params, b_channel_axis);

  ASSIGN_OR_RETURN_VOID(
      auto a_dq_desc,
      OperandDescriptor::Create(this->context_properties(), quantized_type,
                                gemm_descs.a_desc.shape(), ""));
  ASSIGN_OR_RETURN_VOID(
      auto a_scale_desc,
      OperandDescriptor::Create(this->context_properties(),
                                gemm_params.data_type, a_scale_shape, ""));
  ASSIGN_OR_RETURN_VOID(
      auto a_zero_desc,
      OperandDescriptor::Create(this->context_properties(), quantized_type,
                                a_scale_shape, ""));

  ASSIGN_OR_RETURN_VOID(
      auto b_dq_desc,
      OperandDescriptor::Create(this->context_properties(), quantized_type,
                                gemm_descs.b_desc.shape(), ""));
  ASSIGN_OR_RETURN_VOID(
      auto b_scale_desc,
      OperandDescriptor::Create(this->context_properties(),
                                gemm_params.data_type, b_scale_shape, ""));
  ASSIGN_OR_RETURN_VOID(
      auto b_zero_desc,
      OperandDescriptor::Create(this->context_properties(), quantized_type,
                                b_scale_shape, ""));

  ASSIGN_OR_RETURN_VOID(auto a_desc_result,
                        ValidateDequantizeLinearAndInferOutput(
                            this->context_properties(), a_dq_desc, a_scale_desc,
                            a_zero_desc, ""));
  ASSIGN_OR_RETURN_VOID(auto b_desc_result,
                        ValidateDequantizeLinearAndInferOutput(
                            this->context_properties(), b_dq_desc, b_scale_desc,
                            b_zero_desc, ""));

  std::optional<OperandDescriptor> c_dq_desc;
  std::optional<OperandDescriptor> c_scale_desc;
  std::optional<OperandDescriptor> c_zero_desc;
  if (gemm_params.has_c) {
    // C shape is {1}, {N}, {1, N}, or {M, N}. For 1D shapes, axis 0 is the
    // only option. For 2D shapes, quantize along the N dimension at axis 1.
    const uint32_t c_channel_axis =
        gemm_descs.c_desc->shape().size() == 1 ? 0u : 1u;
    auto c_scale_shape = ComputeQuantizationScaleShape(
        gemm_descs.c_desc->shape(), quantization_params, c_channel_axis);

    // The specific values and data types in this test are used to exercise
    // the fusiable path for TFLite backend:
    // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=2079;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
    // TODO(crbug.com/498987226): Remove these restrictions to increase test
    // coverage.
    ASSIGN_OR_RETURN_VOID(
        c_dq_desc, OperandDescriptor::Create(this->context_properties(),
                                             OperandDataType::kInt32,
                                             gemm_descs.c_desc->shape(), ""));
    ASSIGN_OR_RETURN_VOID(
        c_scale_desc,
        OperandDescriptor::Create(this->context_properties(),
                                  gemm_params.data_type, c_scale_shape, ""));
    ASSIGN_OR_RETURN_VOID(
        c_zero_desc,
        OperandDescriptor::Create(this->context_properties(),
                                  OperandDataType::kInt32, c_scale_shape, ""));

    ASSIGN_OR_RETURN_VOID(auto c_desc_result,
                          ValidateDequantizeLinearAndInferOutput(
                              this->context_properties(), *c_dq_desc,
                              *c_scale_desc, *c_zero_desc, ""));
  }

  auto output_scale_shape = ComputeQuantizationScaleShape(
      gemm_descs.output_desc.shape(), per_tensor_quantization_params);

  ASSIGN_OR_RETURN_VOID(
      auto output_scale_desc,
      OperandDescriptor::Create(this->context_properties(),
                                gemm_params.data_type, output_scale_shape, ""));
  ASSIGN_OR_RETURN_VOID(
      auto output_zero_desc,
      OperandDescriptor::Create(this->context_properties(), quantized_type,
                                output_scale_shape, ""));

  ASSIGN_OR_RETURN_VOID(auto quantized_output_desc,
                        ValidateQuantizeLinearAndInferOutput(
                            this->context_properties(), gemm_descs.output_desc,
                            output_scale_desc, output_zero_desc, ""));

  std::vector<uint8_t> a_dq_data(a_dq_desc.PackedByteLength(), seed_for_data);
  std::vector<uint8_t> b_dq_data(b_dq_desc.PackedByteLength(), seed_for_data);
  std::vector<float> a_scale_data(a_scale_desc.NumberOfElements(), 0.5f);
  std::vector<float> b_scale_data(b_scale_desc.NumberOfElements(), 0.25f);
  std::vector<float> output_scale_data(output_scale_desc.NumberOfElements(),
                                       0.125f);
  std::vector<uint8_t> a_zero_data(a_zero_desc.PackedByteLength(), 0);
  std::vector<uint8_t> b_zero_data(b_zero_desc.PackedByteLength(), 0);
  std::vector<uint8_t> output_zero_data(output_zero_desc.PackedByteLength(), 0);

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  OperandId a_dq_id;
  OperandId b_dq_id;
  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;

  if (gemm_params.is_a_constant) {
    a_dq_id = builder.BuildConstant(a_dq_desc.shape(), a_dq_desc.data_type(),
                                    a_dq_data);
  } else {
    a_dq_id = builder.BuildInput("a", a_dq_desc.shape(), a_dq_desc.data_type());
    named_inputs.insert({"a", a_dq_data});
  }
  if (gemm_params.is_b_constant) {
    b_dq_id = builder.BuildConstant(b_dq_desc.shape(), b_dq_desc.data_type(),
                                    b_dq_data);
  } else {
    b_dq_id = builder.BuildInput("b", b_dq_desc.shape(), b_dq_desc.data_type());
    named_inputs.insert({"b", b_dq_data});
  }

  OperandId a_scale_id =
      BuildFloatConstant(builder, a_scale_desc, a_scale_data);
  OperandId a_zero_id = builder.BuildConstant(
      a_zero_desc.shape(), a_zero_desc.data_type(), a_zero_data);
  OperandId b_scale_id =
      BuildFloatConstant(builder, b_scale_desc, b_scale_data);
  OperandId b_zero_id = builder.BuildConstant(
      b_zero_desc.shape(), b_zero_desc.data_type(), b_zero_data);
  OperandId output_scale_id =
      BuildFloatConstant(builder, output_scale_desc, output_scale_data);
  OperandId output_zero_id = builder.BuildConstant(
      output_zero_desc.shape(), output_zero_desc.data_type(), output_zero_data);

  OperandId gemm_a_id = builder.BuildIntermediateOperand(
      gemm_descs.a_desc.shape(), gemm_descs.a_desc.data_type());
  OperandId gemm_b_id = builder.BuildIntermediateOperand(
      gemm_descs.b_desc.shape(), gemm_descs.b_desc.data_type());

  builder.BuildDequantizeLinear(a_dq_id, a_scale_id, a_zero_id, gemm_a_id);
  builder.BuildDequantizeLinear(b_dq_id, b_scale_id, b_zero_id, gemm_b_id);

  BuildGemmAttributes gemm_attr;
  gemm_attr.alpha = gemm_params.alpha;
  gemm_attr.beta = gemm_params.beta;
  gemm_attr.a_transpose = gemm_params.a_transpose;
  gemm_attr.b_transpose = gemm_params.b_transpose;

  std::vector<uint8_t> c_dq_data;
  std::vector<float> c_scale_data;
  std::vector<uint8_t> c_zero_data;
  if (gemm_params.has_c) {
    c_dq_data.assign(c_dq_desc->PackedByteLength(), seed_for_data);
    c_scale_data.assign(c_scale_desc->NumberOfElements(), 0.125f);
    c_zero_data.assign(c_zero_desc->PackedByteLength(), 0);

    OperandId c_dq_id;
    if (gemm_params.is_c_constant) {
      c_dq_id = builder.BuildConstant(c_dq_desc->shape(),
                                      c_dq_desc->data_type(), c_dq_data);
    } else {
      c_dq_id =
          builder.BuildInput("c", c_dq_desc->shape(), c_dq_desc->data_type());
      named_inputs.insert({"c", c_dq_data});
    }

    OperandId c_scale_id =
        BuildFloatConstant(builder, *c_scale_desc, c_scale_data);
    OperandId c_zero_id = builder.BuildConstant(
        c_zero_desc->shape(), c_zero_desc->data_type(), c_zero_data);

    OperandId gemm_c_id = builder.BuildIntermediateOperand(
        gemm_descs.c_desc->shape(), gemm_descs.c_desc->data_type());
    builder.BuildDequantizeLinear(c_dq_id, c_scale_id, c_zero_id, gemm_c_id);
    gemm_attr.c_operand_id = gemm_c_id;
  }

  OperandId gemm_output_id = builder.BuildIntermediateOperand(
      gemm_descs.output_desc.shape(), gemm_descs.output_desc.data_type());
  builder.BuildGemm(gemm_a_id, gemm_b_id, gemm_output_id, gemm_attr);

  OperandId quantize_output_id =
      builder.BuildOutput("output", quantized_output_desc.shape(),
                          quantized_output_desc.data_type());
  builder.BuildQuantizeLinear(gemm_output_id, output_scale_id, output_zero_id,
                              quantize_output_id);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::SubgraphDQPool2dQ(
    Pool2dParams pool2d_params,
    QuantizationParams quantization_params,
    uint8_t seed_for_data) {
  ASSIGN_OR_RETURN_VOID(
      auto pool2d_descs,
      SetUpPool2dDescriptors(this->context_properties(), pool2d_params));

  OperandDataType quantized_type = quantization_params.quantized_type;
  InputOperandLayout input_layout =
      this->context_properties().input_operand_layout;
  const uint32_t input_channel_axis =
      input_layout == InputOperandLayout::kNchw ? 1u : 3u;
  const uint32_t output_channel_axis = input_channel_axis;

  auto input_scale_shape = ComputeQuantizationScaleShape(
      pool2d_descs.input_desc.shape(), quantization_params, input_channel_axis);

  ASSIGN_OR_RETURN_VOID(
      auto input_dq_desc,
      OperandDescriptor::Create(this->context_properties(), quantized_type,
                                pool2d_descs.input_desc.shape(), ""));
  ASSIGN_OR_RETURN_VOID(auto input_scale_desc,
                        OperandDescriptor::Create(this->context_properties(),
                                                  pool2d_params.data_type,
                                                  input_scale_shape, ""));
  ASSIGN_OR_RETURN_VOID(
      auto input_zero_desc,
      OperandDescriptor::Create(this->context_properties(), quantized_type,
                                input_scale_shape, ""));

  ASSIGN_OR_RETURN_VOID(auto input_desc_result,
                        ValidateDequantizeLinearAndInferOutput(
                            this->context_properties(), input_dq_desc,
                            input_scale_desc, input_zero_desc, ""));

  auto output_scale_shape =
      ComputeQuantizationScaleShape(pool2d_descs.output_desc.shape(),
                                    quantization_params, output_channel_axis);

  ASSIGN_OR_RETURN_VOID(auto output_scale_desc,
                        OperandDescriptor::Create(this->context_properties(),
                                                  pool2d_params.data_type,
                                                  output_scale_shape, ""));
  ASSIGN_OR_RETURN_VOID(
      auto output_zero_desc,
      OperandDescriptor::Create(this->context_properties(), quantized_type,
                                output_scale_shape, ""));

  ASSIGN_OR_RETURN_VOID(
      auto quantized_output_desc_result,
      ValidateQuantizeLinearAndInferOutput(
          this->context_properties(), pool2d_descs.output_desc,
          output_scale_desc, output_zero_desc, ""));

  std::vector<uint8_t> input_dq_data(input_dq_desc.PackedByteLength(),
                                     seed_for_data);
  // These values are used to exercise the fusiable path for TFLite backend:
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=2262;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
  // https://source.chromium.org/chromium/chromium/src/+/main:services/webnn/tflite/graph_builder_tflite.cc;l=2273;drc=ec4ff4bae24916aaad3186ce4bc1339313b6fb5a
  // TODO(crbug.com/498987226): Remove this restriction to increase test
  // coverage.
  std::vector<float> input_scale_data(input_scale_desc.NumberOfElements(),
                                      0.25f);
  std::vector<float> output_scale_data(output_scale_desc.NumberOfElements(),
                                       0.25f);
  std::vector<uint8_t> input_zero_data(input_zero_desc.PackedByteLength(), 0);
  std::vector<uint8_t> output_zero_data(output_zero_desc.PackedByteLength(), 0);

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  OperandId input_dq_id;
  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;

  if (pool2d_params.is_input_constant) {
    input_dq_id = builder.BuildConstant(
        input_dq_desc.shape(), input_dq_desc.data_type(), input_dq_data);
  } else {
    input_dq_id = builder.BuildInput("input", input_dq_desc.shape(),
                                     input_dq_desc.data_type());
    named_inputs.insert({"input", input_dq_data});
  }

  OperandId input_scale_id =
      BuildFloatConstant(builder, input_scale_desc, input_scale_data);
  OperandId input_zero_id = builder.BuildConstant(
      input_zero_desc.shape(), input_zero_desc.data_type(), input_zero_data);
  OperandId output_scale_id =
      BuildFloatConstant(builder, output_scale_desc, output_scale_data);
  OperandId output_zero_id = builder.BuildConstant(
      output_zero_desc.shape(), output_zero_desc.data_type(), output_zero_data);

  OperandId pool2d_input_id = builder.BuildIntermediateOperand(
      pool2d_descs.input_desc.shape(), pool2d_descs.input_desc.data_type());

  builder.BuildDequantizeLinear(input_dq_id, input_scale_id, input_zero_id,
                                pool2d_input_id);

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

  OperandId quantize_output_id =
      builder.BuildOutput("output", quantized_output_desc_result.shape(),
                          quantized_output_desc_result.data_type());
  builder.BuildQuantizeLinear(pool_output_id, output_scale_id, output_zero_id,
                              quantize_output_id);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::SubgraphDQReduceQ(
    ReduceParams reduce_params,
    QuantizationParams quantization_params,
    uint32_t channel_axis,
    uint8_t seed_for_input,
    float seed_for_scale,
    uint8_t seed_for_zero_point) {
  ASSIGN_OR_RETURN_VOID(
      auto reduce_descs,
      SetUpReduceDescriptors(this->context_properties(), reduce_params));

  OperandDataType quantized_type = quantization_params.quantized_type;

  // Clamp channel_axis to be valid for the input shape.
  const uint32_t input_channel_axis =
      channel_axis % reduce_descs.input_desc.shape().size();
  auto input_scale_shape = ComputeQuantizationScaleShape(
      reduce_descs.input_desc.shape(), quantization_params, input_channel_axis);

  ASSIGN_OR_RETURN_VOID(
      auto input_dq_desc,
      OperandDescriptor::Create(this->context_properties(), quantized_type,
                                reduce_descs.input_desc.shape(), ""));
  ASSIGN_OR_RETURN_VOID(auto input_scale_desc,
                        OperandDescriptor::Create(this->context_properties(),
                                                  reduce_params.data_type,
                                                  input_scale_shape, ""));
  ASSIGN_OR_RETURN_VOID(
      auto input_zero_desc,
      OperandDescriptor::Create(this->context_properties(), quantized_type,
                                input_scale_shape, ""));

  ASSIGN_OR_RETURN_VOID(auto input_desc_result,
                        ValidateDequantizeLinearAndInferOutput(
                            this->context_properties(), input_dq_desc,
                            input_scale_desc, input_zero_desc, ""));

  // Use per-tensor quantization for the output when reduce produces a scalar
  // (keep_dimensions is false and all axes are reduced), since
  // per-channel/per-block quantization requires a non-empty shape. Otherwise,
  // clamp `output_channel_axis` to be valid for the output shape.
  QuantizationParams output_quantization_params = quantization_params;
  uint32_t output_channel_axis = 0;
  if (reduce_descs.output_desc.shape().empty()) {
    output_quantization_params.quantization_kind = QuantizationKind::kPerTensor;
  } else {
    output_channel_axis =
        channel_axis % reduce_descs.output_desc.shape().size();
  }

  auto output_scale_shape = ComputeQuantizationScaleShape(
      reduce_descs.output_desc.shape(), output_quantization_params,
      output_channel_axis);

  ASSIGN_OR_RETURN_VOID(auto output_scale_desc,
                        OperandDescriptor::Create(this->context_properties(),
                                                  reduce_params.data_type,
                                                  output_scale_shape, ""));
  ASSIGN_OR_RETURN_VOID(
      auto output_zero_desc,
      OperandDescriptor::Create(this->context_properties(), quantized_type,
                                output_scale_shape, ""));

  ASSIGN_OR_RETURN_VOID(
      auto quantized_output_desc,
      ValidateQuantizeLinearAndInferOutput(
          this->context_properties(), reduce_descs.output_desc,
          output_scale_desc, output_zero_desc, ""));

  std::vector<uint8_t> input_dq_data(input_dq_desc.PackedByteLength(),
                                     seed_for_input);
  std::vector<float> input_scale_data(input_scale_desc.NumberOfElements(),
                                      seed_for_scale);
  std::vector<float> output_scale_data(output_scale_desc.NumberOfElements(),
                                       seed_for_scale);
  std::vector<uint8_t> input_zero_data(input_zero_desc.PackedByteLength(),
                                       seed_for_zero_point);
  std::vector<uint8_t> output_zero_data(output_zero_desc.PackedByteLength(),
                                        seed_for_zero_point);

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  OperandId input_dq_id;
  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;

  if (reduce_params.is_input_constant) {
    input_dq_id = builder.BuildConstant(
        input_dq_desc.shape(), input_dq_desc.data_type(), input_dq_data);
  } else {
    input_dq_id = builder.BuildInput("input", input_dq_desc.shape(),
                                     input_dq_desc.data_type());
    named_inputs.insert({"input", input_dq_data});
  }

  OperandId input_scale_id =
      BuildFloatConstant(builder, input_scale_desc, input_scale_data);
  OperandId input_zero_id = builder.BuildConstant(
      input_zero_desc.shape(), input_zero_desc.data_type(), input_zero_data);
  OperandId output_scale_id =
      BuildFloatConstant(builder, output_scale_desc, output_scale_data);
  OperandId output_zero_id = builder.BuildConstant(
      output_zero_desc.shape(), output_zero_desc.data_type(), output_zero_data);

  OperandId reduce_input_id = builder.BuildIntermediateOperand(
      reduce_descs.input_desc.shape(), reduce_descs.input_desc.data_type());

  builder.BuildDequantizeLinear(input_dq_id, input_scale_id, input_zero_id,
                                reduce_input_id);

  OperandId reduce_output_id = builder.BuildIntermediateOperand(
      reduce_descs.output_desc.shape(), reduce_descs.output_desc.data_type());

  builder.BuildReduce(reduce_params.reduce_kind, reduce_input_id,
                      reduce_output_id, reduce_descs.axes,
                      reduce_params.keep_dimensions);

  OperandId quantize_output_id =
      builder.BuildOutput("output", quantized_output_desc.shape(),
                          quantized_output_desc.data_type());
  builder.BuildQuantizeLinear(reduce_output_id, output_scale_id, output_zero_id,
                              quantize_output_id);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }

  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

WEBNN_FUZZ_TEST_F(
    SingleOpConcat,
    .WithDomains(AnyConcatParams(), fuzztest::Arbitrary<uint8_t>())
        .WithSeeds({{ConcatParams{
                         /*data_type=*/OperandDataType::kFloat32,
                         /*rank=*/4,
                         /*input_dims=*/{1, 3, 4, 4, 1, 1, 1, 1},
                         /*axis=*/1,
                         /*num_inputs=*/3,
                         /*extra_axis_dims=*/{2, 5, 1, 1, 1, 1, 1, 1, 1},
                         /*is_input_constant=*/false,
                     },
                     /*seed_for_data=*/1}}));

WEBNN_FUZZ_TEST_F(SingleOpConv2d,
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
                                   },
                                   /*seed_for_data=*/1}}));

WEBNN_FUZZ_TEST_F(SingleOpExpand,
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
    SingleOpGatherND,
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

WEBNN_FUZZ_TEST_F(SingleOpGemm,
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

WEBNN_FUZZ_TEST_F(
    SingleOpLstm,
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

WEBNN_FUZZ_TEST_F(SingleOpPool2d,
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

WEBNN_FUZZ_TEST_F(
    SingleOpScatterElements,
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

WEBNN_FUZZ_TEST_F(
    SingleOpReduce,
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
    SubgraphDQConcatQ,
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
                         /*num_inputs=*/3,
                         /*extra_axis_dims=*/{2, 5, 1, 1, 1, 1, 1, 1, 1},
                         /*is_input_constant=*/false,
                     },
                     /*quantized_type=*/OperandDataType::kUint8,
                     /*seed_for_input=*/2,
                     /*seed_for_scale=*/0.25f,
                     /*seed_for_zero_point=*/0}}));

WEBNN_FUZZ_TEST_F(
    SubgraphDQConv2dQ,
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
                                  /*is_depthwise=*/false},
                     QuantizationParams{
                         /*quantized_type=*/OperandDataType::kUint8,
                         QuantizationKind::kPerTensor,
                         // This is unused for per tensor quantization.
                         /*channel_block_size=*/1},
                     /*seed_for_data=*/1}}));

WEBNN_FUZZ_TEST_F(
    SubgraphDQGemmQ,
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
    SubgraphDQPool2dQ,
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
    SubgraphDQReduceQ,
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

}  // namespace webnn::test
