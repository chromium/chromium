// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_DATA_TYPE_LIMITS_H_
#define SERVICES_WEBNN_PUBLIC_CPP_DATA_TYPE_LIMITS_H_

#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "services/webnn/public/cpp/supported_data_types.h"

namespace webnn {

struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) DataTypeLimits {
  explicit DataTypeLimits(mojo::DefaultConstruct::Tag);

  DataTypeLimits(SupportedDataTypes input,
                 SupportedDataTypes constant,
                 SupportedDataTypes arg_min_max_input,
                 SupportedDataTypes arg_min_max_output,
                 SupportedDataTypes batch_normalization_input,
                 SupportedDataTypes cast_input,
                 SupportedDataTypes clamp_input,
                 SupportedDataTypes concat_inputs,
                 SupportedDataTypes conv2d_input,
                 SupportedDataTypes conv_transpose2d_input,
                 SupportedDataTypes cumulative_sum_input,
                 SupportedDataTypes dequantize_linear_input,
                 SupportedDataTypes dequantize_linear_scale,
                 SupportedDataTypes add_input,
                 SupportedDataTypes sub_input,
                 SupportedDataTypes mul_input,
                 SupportedDataTypes div_input,
                 SupportedDataTypes max_input,
                 SupportedDataTypes min_input,
                 SupportedDataTypes pow_input,
                 SupportedDataTypes equal_input,
                 SupportedDataTypes greater_input,
                 SupportedDataTypes greater_or_equal_input,
                 SupportedDataTypes lesser_input,
                 SupportedDataTypes lesser_or_equal_input,
                 SupportedDataTypes logical_and_input,
                 SupportedDataTypes logical_or_input,
                 SupportedDataTypes logical_xor_input,
                 SupportedDataTypes logical_not_input,
                 SupportedDataTypes logical_output,
                 SupportedDataTypes abs_input,
                 SupportedDataTypes ceil_input,
                 SupportedDataTypes cos_input,
                 SupportedDataTypes erf_input,
                 SupportedDataTypes exp_input,
                 SupportedDataTypes floor_input,
                 SupportedDataTypes identity_input,
                 SupportedDataTypes log_input,
                 SupportedDataTypes neg_input,
                 SupportedDataTypes reciprocal_input,
                 SupportedDataTypes sign_input,
                 SupportedDataTypes sin_input,
                 SupportedDataTypes sqrt_input,
                 SupportedDataTypes tan_input,
                 SupportedDataTypes elu_input,
                 SupportedDataTypes expand_input,
                 SupportedDataTypes gather_input,
                 SupportedDataTypes gather_indices,
                 SupportedDataTypes gather_elements_input,
                 SupportedDataTypes gather_elements_indices,
                 SupportedDataTypes gather_nd_input,
                 SupportedDataTypes gather_nd_indices,
                 SupportedDataTypes gelu_input,
                 SupportedDataTypes gemm_input,
                 SupportedDataTypes gru_input,
                 SupportedDataTypes gru_cell_input,
                 SupportedDataTypes hard_sigmoid_input,
                 SupportedDataTypes hard_swish_input,
                 SupportedDataTypes instance_normalization_input,
                 SupportedDataTypes layer_normalization_input,
                 SupportedDataTypes leaky_relu_input,
                 SupportedDataTypes linear_input,
                 SupportedDataTypes lstm_input,
                 SupportedDataTypes lstm_cell_input,
                 SupportedDataTypes matmul_input,
                 SupportedDataTypes pad_input,
                 SupportedDataTypes average_pool2d_input,
                 SupportedDataTypes l2_pool2d_input,
                 SupportedDataTypes max_pool2d_input,
                 SupportedDataTypes prelu_input,
                 SupportedDataTypes quantize_linear_input,
                 SupportedDataTypes quantize_linear_zero_point,
                 SupportedDataTypes reduce_l1_input,
                 SupportedDataTypes reduce_l2_input,
                 SupportedDataTypes reduce_log_sum_input,
                 SupportedDataTypes reduce_log_sum_exp_input,
                 SupportedDataTypes reduce_max_input,
                 SupportedDataTypes reduce_mean_input,
                 SupportedDataTypes reduce_min_input,
                 SupportedDataTypes reduce_product_input,
                 SupportedDataTypes reduce_sum_input,
                 SupportedDataTypes reduce_sum_square_input,
                 SupportedDataTypes relu_input,
                 SupportedDataTypes resample2d_input,
                 SupportedDataTypes reshape_input,
                 SupportedDataTypes scatter_nd_input,
                 SupportedDataTypes scatter_nd_indices,
                 SupportedDataTypes sigmoid_input,
                 SupportedDataTypes slice_input,
                 SupportedDataTypes softmax_input,
                 SupportedDataTypes softplus_input,
                 SupportedDataTypes softsign_input,
                 SupportedDataTypes split_input,
                 SupportedDataTypes tanh_input,
                 SupportedDataTypes tile_input,
                 SupportedDataTypes transpose_input,
                 SupportedDataTypes triangular_input,
                 SupportedDataTypes where_condition,
                 SupportedDataTypes where_value);

  // Copyable and movable.
  DataTypeLimits(const DataTypeLimits&);
  DataTypeLimits& operator=(const DataTypeLimits&);
  DataTypeLimits(DataTypeLimits&&) noexcept;
  DataTypeLimits& operator=(DataTypeLimits&&) noexcept;
  ~DataTypeLimits();

  // Output supported data types are the same as inputs.
  SupportedDataTypes output() const { return input; }

  SupportedDataTypes input;
  SupportedDataTypes constant;
  SupportedDataTypes arg_min_max_input;
  SupportedDataTypes arg_min_max_output;
  SupportedDataTypes batch_normalization_input;
  SupportedDataTypes cast_input;
  SupportedDataTypes clamp_input;
  SupportedDataTypes concat_inputs;
  SupportedDataTypes conv2d_input;
  SupportedDataTypes conv_transpose2d_input;
  SupportedDataTypes cumulative_sum_input;
  SupportedDataTypes dequantize_linear_input;
  SupportedDataTypes dequantize_linear_scale;
  SupportedDataTypes add_input;
  SupportedDataTypes sub_input;
  SupportedDataTypes mul_input;
  SupportedDataTypes div_input;
  SupportedDataTypes max_input;
  SupportedDataTypes min_input;
  SupportedDataTypes pow_input;
  SupportedDataTypes equal_input;
  SupportedDataTypes greater_input;
  SupportedDataTypes greater_or_equal_input;
  SupportedDataTypes lesser_input;
  SupportedDataTypes lesser_or_equal_input;
  SupportedDataTypes logical_and_input;
  SupportedDataTypes logical_or_input;
  SupportedDataTypes logical_xor_input;
  SupportedDataTypes logical_not_input;
  SupportedDataTypes logical_output;
  SupportedDataTypes abs_input;
  SupportedDataTypes ceil_input;
  SupportedDataTypes cos_input;
  SupportedDataTypes erf_input;
  SupportedDataTypes exp_input;
  SupportedDataTypes floor_input;
  SupportedDataTypes identity_input;
  SupportedDataTypes log_input;
  SupportedDataTypes neg_input;
  SupportedDataTypes reciprocal_input;
  SupportedDataTypes sign_input;
  SupportedDataTypes sin_input;
  SupportedDataTypes sqrt_input;
  SupportedDataTypes tan_input;
  SupportedDataTypes elu_input;
  SupportedDataTypes expand_input;
  SupportedDataTypes gather_input;
  SupportedDataTypes gather_indices;
  SupportedDataTypes gather_elements_input;
  SupportedDataTypes gather_elements_indices;
  SupportedDataTypes gather_nd_input;
  SupportedDataTypes gather_nd_indices;
  SupportedDataTypes gelu_input;
  SupportedDataTypes gemm_input;
  SupportedDataTypes gru_input;
  SupportedDataTypes gru_cell_input;
  SupportedDataTypes hard_sigmoid_input;
  SupportedDataTypes hard_swish_input;
  SupportedDataTypes instance_normalization_input;
  SupportedDataTypes layer_normalization_input;
  SupportedDataTypes leaky_relu_input;
  SupportedDataTypes linear_input;
  SupportedDataTypes lstm_input;
  SupportedDataTypes lstm_cell_input;
  SupportedDataTypes matmul_input;
  SupportedDataTypes pad_input;
  SupportedDataTypes average_pool2d_input;
  SupportedDataTypes l2_pool2d_input;
  SupportedDataTypes max_pool2d_input;
  SupportedDataTypes prelu_input;
  SupportedDataTypes quantize_linear_input;
  SupportedDataTypes quantize_linear_zero_point;
  SupportedDataTypes reduce_l1_input;
  SupportedDataTypes reduce_l2_input;
  SupportedDataTypes reduce_log_sum_input;
  SupportedDataTypes reduce_log_sum_exp_input;
  SupportedDataTypes reduce_max_input;
  SupportedDataTypes reduce_mean_input;
  SupportedDataTypes reduce_min_input;
  SupportedDataTypes reduce_product_input;
  SupportedDataTypes reduce_sum_input;
  SupportedDataTypes reduce_sum_square_input;
  SupportedDataTypes relu_input;
  SupportedDataTypes resample2d_input;
  SupportedDataTypes reshape_input;
  SupportedDataTypes scatter_nd_input;
  SupportedDataTypes scatter_nd_indices;
  SupportedDataTypes sigmoid_input;
  SupportedDataTypes slice_input;
  SupportedDataTypes softmax_input;
  SupportedDataTypes softplus_input;
  SupportedDataTypes softsign_input;
  SupportedDataTypes split_input;
  SupportedDataTypes tanh_input;
  SupportedDataTypes tile_input;
  SupportedDataTypes transpose_input;
  SupportedDataTypes triangular_input;
  SupportedDataTypes where_condition;
  SupportedDataTypes where_value;
};

// clang-format off
inline bool operator==(const DataTypeLimits& lhs, const DataTypeLimits& rhs) {
  return lhs.input == rhs.input &&
         lhs.constant == rhs.constant &&
         lhs.arg_min_max_input == rhs.arg_min_max_input &&
         lhs.arg_min_max_output == rhs.arg_min_max_output &&
         lhs.batch_normalization_input == rhs.batch_normalization_input &&
         lhs.cast_input == rhs.cast_input &&
         lhs.clamp_input == rhs.clamp_input &&
         lhs.concat_inputs == rhs.concat_inputs &&
         lhs.conv2d_input == rhs.conv2d_input &&
         lhs.conv_transpose2d_input == rhs.conv_transpose2d_input &&
         lhs.cumulative_sum_input == rhs.cumulative_sum_input &&
         lhs.dequantize_linear_input == rhs.dequantize_linear_input &&
         lhs.dequantize_linear_scale == rhs.dequantize_linear_scale &&
         lhs.add_input == rhs.add_input &&
         lhs.sub_input == rhs.sub_input &&
         lhs.mul_input == rhs.mul_input &&
         lhs.div_input == rhs.div_input &&
         lhs.max_input == rhs.max_input &&
         lhs.min_input == rhs.min_input &&
         lhs.pow_input == rhs.pow_input &&
         lhs.equal_input == rhs.equal_input &&
         lhs.greater_input == rhs.greater_input &&
         lhs.greater_or_equal_input == rhs.greater_or_equal_input &&
         lhs.lesser_input == rhs.lesser_input &&
         lhs.lesser_or_equal_input == rhs.lesser_or_equal_input &&
         lhs.logical_and_input == rhs.logical_and_input &&
         lhs.logical_or_input == rhs.logical_or_input &&
         lhs.logical_xor_input == rhs.logical_xor_input &&
         lhs.logical_not_input == rhs.logical_not_input &&
         lhs.logical_output == rhs.logical_output &&
         lhs.abs_input == rhs.abs_input &&
         lhs.ceil_input == rhs.ceil_input &&
         lhs.cos_input == rhs.cos_input &&
         lhs.erf_input == rhs.erf_input &&
         lhs.exp_input == rhs.exp_input &&
         lhs.floor_input == rhs.floor_input &&
         lhs.identity_input == rhs.identity_input &&
         lhs.log_input == rhs.log_input &&
         lhs.neg_input == rhs.neg_input &&
         lhs.reciprocal_input == rhs.reciprocal_input &&
         lhs.sign_input == rhs.sign_input &&
         lhs.sin_input == rhs.sin_input &&
         lhs.sqrt_input == rhs.sqrt_input &&
         lhs.tan_input == rhs.tan_input &&
         lhs.elu_input == rhs.elu_input &&
         lhs.expand_input == rhs.expand_input &&
         lhs.gather_input == rhs.gather_input &&
         lhs.gather_indices == rhs.gather_indices &&
         lhs.gather_elements_input == rhs.gather_elements_input &&
         lhs.gather_elements_indices == rhs.gather_elements_indices &&
         lhs.gather_nd_input == rhs.gather_nd_input &&
         lhs.gather_nd_indices == rhs.gather_nd_indices &&
         lhs.gelu_input == rhs.gelu_input &&
         lhs.gemm_input == rhs.gemm_input &&
         lhs.gru_input == rhs.gru_input &&
         lhs.gru_cell_input == rhs.gru_cell_input &&
         lhs.hard_sigmoid_input == rhs.hard_sigmoid_input &&
         lhs.hard_swish_input == rhs.hard_swish_input &&
         lhs.instance_normalization_input == rhs.instance_normalization_input &&
         lhs.layer_normalization_input == rhs.layer_normalization_input &&
         lhs.leaky_relu_input == rhs.leaky_relu_input &&
         lhs.linear_input == rhs.linear_input &&
         lhs.lstm_input == rhs.lstm_input &&
         lhs.lstm_cell_input == rhs.lstm_cell_input &&
         lhs.matmul_input == rhs.matmul_input &&
         lhs.pad_input == rhs.pad_input &&
         lhs.average_pool2d_input == rhs.average_pool2d_input &&
         lhs.l2_pool2d_input == rhs.l2_pool2d_input &&
         lhs.max_pool2d_input == rhs.max_pool2d_input &&
         lhs.prelu_input == rhs.prelu_input &&
         lhs.quantize_linear_input == rhs.quantize_linear_input &&
         lhs.quantize_linear_zero_point == rhs.quantize_linear_zero_point &&
         lhs.reduce_l1_input == rhs.reduce_l1_input &&
         lhs.reduce_l2_input == rhs.reduce_l2_input &&
         lhs.reduce_log_sum_input == rhs.reduce_log_sum_input &&
         lhs.reduce_log_sum_exp_input == rhs.reduce_log_sum_exp_input &&
         lhs.reduce_max_input == rhs.reduce_max_input &&
         lhs.reduce_mean_input == rhs.reduce_mean_input &&
         lhs.reduce_min_input == rhs.reduce_min_input &&
         lhs.reduce_product_input == rhs.reduce_product_input &&
         lhs.reduce_sum_input == rhs.reduce_sum_input &&
         lhs.reduce_sum_square_input == rhs.reduce_sum_square_input &&
         lhs.relu_input == rhs.relu_input &&
         lhs.resample2d_input == rhs.resample2d_input &&
         lhs.reshape_input == rhs.reshape_input &&
         lhs.scatter_nd_input == rhs.scatter_nd_input &&
         lhs.scatter_nd_indices == rhs.scatter_nd_indices &&
         lhs.sigmoid_input == rhs.sigmoid_input &&
         lhs.slice_input == rhs.slice_input &&
         lhs.softmax_input == rhs.softmax_input &&
         lhs.softplus_input == rhs.softplus_input &&
         lhs.softsign_input == rhs.softsign_input &&
         lhs.split_input == rhs.split_input &&
         lhs.tanh_input == rhs.tanh_input &&
         lhs.tile_input == rhs.tile_input &&
         lhs.transpose_input == rhs.transpose_input &&
         lhs.triangular_input == rhs.triangular_input &&
         lhs.where_condition == rhs.where_condition &&
         lhs.where_value == rhs.where_value;
}
// clang-format on

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_DATA_TYPE_LIMITS_H_
