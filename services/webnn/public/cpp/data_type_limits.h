// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_DATA_TYPE_LIMITS_H_
#define SERVICES_WEBNN_PUBLIC_CPP_DATA_TYPE_LIMITS_H_

#include <cstddef>

#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/cpp/supported_tensors.h"

namespace webnn {

// X-macro listing every `DataTypeLimits` field together with its type, in
// declaration order. It is the single source of truth for the per-field
// boilerplate below: expand it with a `V(type, name)` macro to generate the
// member declarations, `operator==`, and `RemoveDataType()`.
#define WEBNN_DATA_TYPE_LIMITS_FIELDS(V)            \
  V(SupportedTensors, input)                        \
  V(SupportedTensors, constant)                     \
  V(SupportedTensors, arg_min_max_input)            \
  V(SupportedTensors, arg_min_max_output)           \
  V(SupportedTensors, batch_normalization_input)    \
  V(SupportedTensors, batch_normalization_mean)     \
  V(SupportedTensors, cast_input)                   \
  V(SupportedTensors, clamp_input)                  \
  V(SupportedTensors, concat_inputs)                \
  V(SupportedTensors, conv2d_input)                 \
  V(SupportedTensors, conv2d_bias)                  \
  V(SupportedTensors, conv_transpose2d_input)       \
  V(SupportedTensors, conv_transpose2d_bias)        \
  V(SupportedTensors, cumulative_sum_input)         \
  V(SupportedTensors, dequantize_linear_input)      \
  V(SupportedTensors, dequantize_linear_scale)      \
  V(SupportedTensors, add_input)                    \
  V(SupportedTensors, sub_input)                    \
  V(SupportedTensors, mul_input)                    \
  V(SupportedTensors, div_input)                    \
  V(SupportedTensors, max_input)                    \
  V(SupportedTensors, min_input)                    \
  V(SupportedTensors, pow_input)                    \
  V(SupportedTensors, equal_input)                  \
  V(SupportedTensors, greater_input)                \
  V(SupportedTensors, greater_or_equal_input)       \
  V(SupportedTensors, lesser_input)                 \
  V(SupportedTensors, lesser_or_equal_input)        \
  V(SupportedTensors, not_equal_input)              \
  V(SupportedTensors, logical_and_input)            \
  V(SupportedTensors, logical_or_input)             \
  V(SupportedTensors, logical_xor_input)            \
  V(SupportedTensors, logical_not_input)            \
  V(SupportedTensors, is_nan_input)                 \
  V(SupportedTensors, is_infinite_input)            \
  V(SupportedDataTypes, logical_output)             \
  V(SupportedTensors, abs_input)                    \
  V(SupportedTensors, ceil_input)                   \
  V(SupportedTensors, cos_input)                    \
  V(SupportedTensors, erf_input)                    \
  V(SupportedTensors, exp_input)                    \
  V(SupportedTensors, floor_input)                  \
  V(SupportedTensors, identity_input)               \
  V(SupportedTensors, log_input)                    \
  V(SupportedTensors, neg_input)                    \
  V(SupportedTensors, reciprocal_input)             \
  V(SupportedTensors, round_even_input)             \
  V(SupportedTensors, sign_input)                   \
  V(SupportedTensors, sin_input)                    \
  V(SupportedTensors, sqrt_input)                   \
  V(SupportedTensors, tan_input)                    \
  V(SupportedTensors, elu_input)                    \
  V(SupportedTensors, expand_input)                 \
  V(SupportedTensors, gather_input)                 \
  V(SupportedTensors, gather_indices)               \
  V(SupportedTensors, gather_elements_input)        \
  V(SupportedTensors, gather_elements_indices)      \
  V(SupportedTensors, gather_nd_input)              \
  V(SupportedTensors, gather_nd_indices)            \
  V(SupportedTensors, gelu_input)                   \
  V(SupportedTensors, gemm_a)                       \
  V(SupportedTensors, gemm_c)                       \
  V(SupportedTensors, gru_input)                    \
  V(SupportedTensors, gru_bias)                     \
  V(SupportedTensors, gru_output_sequence)          \
  V(SupportedTensors, gru_cell_input)               \
  V(SupportedTensors, gru_cell_bias)                \
  V(SupportedTensors, hard_sigmoid_input)           \
  V(SupportedTensors, hard_swish_input)             \
  V(SupportedTensors, instance_normalization_input) \
  V(SupportedTensors, instance_normalization_scale) \
  V(SupportedTensors, layer_normalization_input)    \
  V(SupportedTensors, leaky_relu_input)             \
  V(SupportedTensors, linear_input)                 \
  V(SupportedTensors, lstm_input)                   \
  V(SupportedTensors, lstm_bias)                    \
  V(SupportedTensors, lstm_output_sequence)         \
  V(SupportedTensors, lstm_cell_input)              \
  V(SupportedTensors, lstm_cell_bias)               \
  V(SupportedTensors, matmul_input)                 \
  V(SupportedTensors, pad_input)                    \
  V(SupportedTensors, average_pool2d_input)         \
  V(SupportedTensors, l2_pool2d_input)              \
  V(SupportedTensors, max_pool2d_input)             \
  V(SupportedTensors, prelu_input)                  \
  V(SupportedTensors, quantize_linear_input)        \
  V(SupportedTensors, quantize_linear_zero_point)   \
  V(SupportedTensors, reduce_l1_input)              \
  V(SupportedTensors, reduce_l2_input)              \
  V(SupportedTensors, reduce_log_sum_input)         \
  V(SupportedTensors, reduce_log_sum_exp_input)     \
  V(SupportedTensors, reduce_max_input)             \
  V(SupportedTensors, reduce_mean_input)            \
  V(SupportedTensors, reduce_min_input)             \
  V(SupportedTensors, reduce_product_input)         \
  V(SupportedTensors, reduce_sum_input)             \
  V(SupportedTensors, reduce_sum_square_input)      \
  V(SupportedTensors, relu_input)                   \
  V(SupportedTensors, resample2d_input)             \
  V(SupportedTensors, reshape_input)                \
  V(SupportedTensors, reverse_input)                \
  V(SupportedTensors, scatter_elements_input)       \
  V(SupportedTensors, scatter_elements_indices)     \
  V(SupportedTensors, scatter_nd_input)             \
  V(SupportedTensors, scatter_nd_indices)           \
  V(SupportedTensors, scatter_nd_updates)           \
  V(SupportedTensors, sigmoid_input)                \
  V(SupportedTensors, slice_input)                  \
  V(SupportedTensors, softmax_input)                \
  V(SupportedTensors, softplus_input)               \
  V(SupportedTensors, softsign_input)               \
  V(SupportedTensors, split_input)                  \
  V(SupportedTensors, tanh_input)                   \
  V(SupportedTensors, tile_input)                   \
  V(SupportedTensors, transpose_input)              \
  V(SupportedTensors, triangular_input)             \
  V(SupportedTensors, where_condition)              \
  V(SupportedTensors, where_value)

struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) DataTypeLimits {
  explicit DataTypeLimits(mojo::DefaultConstruct::Tag);

  // Constructs limits from one value per field, in the order listed by
  // `WEBNN_DATA_TYPE_LIMITS_FIELDS`. The trailing `sentinel` parameter only
  // exists to absorb the trailing comma the expansion produces and is never
  // passed by callers.
#define WEBNN_DATA_TYPE_LIMITS_DECLARE_PARAM(type, name) type name,
  DataTypeLimits(
      WEBNN_DATA_TYPE_LIMITS_FIELDS(WEBNN_DATA_TYPE_LIMITS_DECLARE_PARAM)
          std::nullptr_t sentinel = nullptr);
#undef WEBNN_DATA_TYPE_LIMITS_DECLARE_PARAM

  // Copyable and movable.
  DataTypeLimits(const DataTypeLimits&);
  DataTypeLimits& operator=(const DataTypeLimits&);
  DataTypeLimits(DataTypeLimits&&) noexcept;
  DataTypeLimits& operator=(DataTypeLimits&&) noexcept;
  ~DataTypeLimits();

  // Output supported data types are the same as inputs.
  SupportedTensors output() const { return input; }

  // Removes `data_type` from every operand's supported data type set. Used by
  // backends whose underlying delegate cannot handle a given data type (e.g.
  // the ML Drift GPU delegate does not support int64).
  void RemoveDataType(OperandDataType data_type);

#define WEBNN_DATA_TYPE_LIMITS_DECLARE_FIELD(type, name) type name;
  WEBNN_DATA_TYPE_LIMITS_FIELDS(WEBNN_DATA_TYPE_LIMITS_DECLARE_FIELD)
#undef WEBNN_DATA_TYPE_LIMITS_DECLARE_FIELD
};

inline bool operator==(const DataTypeLimits& lhs, const DataTypeLimits& rhs) {
  return true
#define WEBNN_DATA_TYPE_LIMITS_COMPARE_FIELD(type, name) && lhs.name == rhs.name
      WEBNN_DATA_TYPE_LIMITS_FIELDS(WEBNN_DATA_TYPE_LIMITS_COMPARE_FIELD)
#undef WEBNN_DATA_TYPE_LIMITS_COMPARE_FIELD
          ;  // NOLINT(whitespace/semicolon)
}

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_DATA_TYPE_LIMITS_H_
