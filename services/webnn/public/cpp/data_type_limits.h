// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_DATA_TYPE_LIMITS_H_
#define SERVICES_WEBNN_PUBLIC_CPP_DATA_TYPE_LIMITS_H_

#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/cpp/supported_tensors.h"

namespace webnn {

struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) DataTypeLimits {
  explicit DataTypeLimits(mojo::DefaultConstruct::Tag);

  DataTypeLimits(SupportedTensors input,
                 SupportedTensors constant,
                 SupportedTensors arg_min_max_input,
                 SupportedTensors arg_min_max_output,
                 SupportedTensors batch_normalization_input,
                 SupportedTensors batch_normalization_mean,
                 SupportedTensors cast_input,
                 SupportedTensors clamp_input,
                 SupportedTensors concat_inputs,
                 SupportedTensors conv2d_input,
                 SupportedTensors conv2d_bias,
                 SupportedTensors conv_transpose2d_input,
                 SupportedTensors conv_transpose2d_bias,
                 SupportedTensors cumulative_sum_input,
                 SupportedTensors dequantize_linear_input,
                 SupportedTensors dequantize_linear_scale,
                 SupportedTensors dequantize_linear_zero_point,
                 SupportedTensors add_input,
                 SupportedTensors sub_input,
                 SupportedTensors mul_input,
                 SupportedTensors div_input,
                 SupportedTensors max_input,
                 SupportedTensors min_input,
                 SupportedTensors pow_input,
                 SupportedTensors equal_input,
                 SupportedTensors greater_input,
                 SupportedTensors greater_or_equal_input,
                 SupportedTensors lesser_input,
                 SupportedTensors lesser_or_equal_input,
                 SupportedTensors not_equal_input,
                 SupportedTensors logical_and_input,
                 SupportedTensors logical_or_input,
                 SupportedTensors logical_xor_input,
                 SupportedTensors logical_not_input,
                 SupportedTensors is_nan_input,
                 SupportedTensors is_infinite_input,
                 SupportedDataTypes logical_output,
                 SupportedTensors abs_input,
                 SupportedTensors ceil_input,
                 SupportedTensors cos_input,
                 SupportedTensors erf_input,
                 SupportedTensors exp_input,
                 SupportedTensors floor_input,
                 SupportedTensors identity_input,
                 SupportedTensors log_input,
                 SupportedTensors neg_input,
                 SupportedTensors reciprocal_input,
                 SupportedTensors round_even_input,
                 SupportedTensors sign_input,
                 SupportedTensors sin_input,
                 SupportedTensors sqrt_input,
                 SupportedTensors tan_input,
                 SupportedTensors elu_input,
                 SupportedTensors expand_input,
                 SupportedTensors gather_input,
                 SupportedTensors gather_indices,
                 SupportedTensors gather_elements_input,
                 SupportedTensors gather_elements_indices,
                 SupportedTensors gather_nd_input,
                 SupportedTensors gather_nd_indices,
                 SupportedTensors gelu_input,
                 SupportedTensors gemm_a,
                 SupportedTensors gemm_c,
                 SupportedTensors gru_input,
                 SupportedTensors gru_bias,
                 SupportedTensors gru_output_sequence,
                 SupportedTensors gru_cell_input,
                 SupportedTensors gru_cell_bias,
                 SupportedTensors hard_sigmoid_input,
                 SupportedTensors hard_swish_input,
                 SupportedTensors instance_normalization_input,
                 SupportedTensors instance_normalization_scale,
                 SupportedTensors layer_normalization_input,
                 SupportedTensors leaky_relu_input,
                 SupportedTensors linear_input,
                 SupportedTensors lstm_input,
                 SupportedTensors lstm_bias,
                 SupportedTensors lstm_output_sequence,
                 SupportedTensors lstm_cell_input,
                 SupportedTensors lstm_cell_bias,
                 SupportedTensors matmul_input,
                 SupportedTensors pad_input,
                 SupportedTensors average_pool2d_input,
                 SupportedTensors l2_pool2d_input,
                 SupportedTensors max_pool2d_input,
                 SupportedTensors prelu_input,
                 SupportedTensors quantize_linear_input,
                 SupportedTensors quantize_linear_zero_point,
                 SupportedTensors reduce_l1_input,
                 SupportedTensors reduce_l2_input,
                 SupportedTensors reduce_log_sum_input,
                 SupportedTensors reduce_log_sum_exp_input,
                 SupportedTensors reduce_max_input,
                 SupportedTensors reduce_mean_input,
                 SupportedTensors reduce_min_input,
                 SupportedTensors reduce_product_input,
                 SupportedTensors reduce_sum_input,
                 SupportedTensors reduce_sum_square_input,
                 SupportedTensors relu_input,
                 SupportedTensors resample2d_input,
                 SupportedTensors reshape_input,
                 SupportedTensors reverse_input,
                 SupportedTensors scatter_elements_input,
                 SupportedTensors scatter_elements_indices,
                 SupportedTensors scatter_nd_input,
                 SupportedTensors scatter_nd_indices,
                 SupportedTensors scatter_nd_updates,
                 SupportedTensors sigmoid_input,
                 SupportedTensors slice_input,
                 SupportedTensors softmax_input,
                 SupportedTensors softplus_input,
                 SupportedTensors softsign_input,
                 SupportedTensors split_input,
                 SupportedTensors tanh_input,
                 SupportedTensors tile_input,
                 SupportedTensors transpose_input,
                 SupportedTensors triangular_input,
                 SupportedTensors where_condition,
                 SupportedTensors where_value);

  // Copyable and movable.
  DataTypeLimits(const DataTypeLimits&);
  DataTypeLimits& operator=(const DataTypeLimits&);
  DataTypeLimits(DataTypeLimits&&) noexcept;
  DataTypeLimits& operator=(DataTypeLimits&&) noexcept;
  ~DataTypeLimits();

  // Output supported data types are the same as inputs.
  SupportedTensors output() const { return input; }

  SupportedTensors input;
  SupportedTensors constant;
  SupportedTensors arg_min_max_input;
  SupportedTensors arg_min_max_output;
  SupportedTensors batch_normalization_input;
  SupportedTensors batch_normalization_mean;
  SupportedTensors cast_input;
  SupportedTensors clamp_input;
  SupportedTensors concat_inputs;
  SupportedTensors conv2d_input;
  SupportedTensors conv2d_bias;
  SupportedTensors conv_transpose2d_input;
  SupportedTensors conv_transpose2d_bias;
  SupportedTensors cumulative_sum_input;
  SupportedTensors dequantize_linear_input;
  SupportedTensors dequantize_linear_scale;
  SupportedTensors dequantize_linear_zero_point;
  SupportedTensors add_input;
  SupportedTensors sub_input;
  SupportedTensors mul_input;
  SupportedTensors div_input;
  SupportedTensors max_input;
  SupportedTensors min_input;
  SupportedTensors pow_input;
  SupportedTensors equal_input;
  SupportedTensors greater_input;
  SupportedTensors greater_or_equal_input;
  SupportedTensors lesser_input;
  SupportedTensors lesser_or_equal_input;
  SupportedTensors not_equal_input;
  SupportedTensors logical_and_input;
  SupportedTensors logical_or_input;
  SupportedTensors logical_xor_input;
  SupportedTensors logical_not_input;
  SupportedTensors is_nan_input;
  SupportedTensors is_infinite_input;
  SupportedDataTypes logical_output;
  SupportedTensors abs_input;
  SupportedTensors ceil_input;
  SupportedTensors cos_input;
  SupportedTensors erf_input;
  SupportedTensors exp_input;
  SupportedTensors floor_input;
  SupportedTensors identity_input;
  SupportedTensors log_input;
  SupportedTensors neg_input;
  SupportedTensors reciprocal_input;
  SupportedTensors round_even_input;
  SupportedTensors sign_input;
  SupportedTensors sin_input;
  SupportedTensors sqrt_input;
  SupportedTensors tan_input;
  SupportedTensors elu_input;
  SupportedTensors expand_input;
  SupportedTensors gather_input;
  SupportedTensors gather_indices;
  SupportedTensors gather_elements_input;
  SupportedTensors gather_elements_indices;
  SupportedTensors gather_nd_input;
  SupportedTensors gather_nd_indices;
  SupportedTensors gelu_input;
  SupportedTensors gemm_a;
  SupportedTensors gemm_c;
  SupportedTensors gru_input;
  SupportedTensors gru_bias;
  SupportedTensors gru_output_sequence;
  SupportedTensors gru_cell_input;
  SupportedTensors gru_cell_bias;
  SupportedTensors hard_sigmoid_input;
  SupportedTensors hard_swish_input;
  SupportedTensors instance_normalization_input;
  SupportedTensors instance_normalization_scale;
  SupportedTensors layer_normalization_input;
  SupportedTensors leaky_relu_input;
  SupportedTensors linear_input;
  SupportedTensors lstm_input;
  SupportedTensors lstm_bias;
  SupportedTensors lstm_output_sequence;
  SupportedTensors lstm_cell_input;
  SupportedTensors lstm_cell_bias;
  SupportedTensors matmul_input;
  SupportedTensors pad_input;
  SupportedTensors average_pool2d_input;
  SupportedTensors l2_pool2d_input;
  SupportedTensors max_pool2d_input;
  SupportedTensors prelu_input;
  SupportedTensors quantize_linear_input;
  SupportedTensors quantize_linear_zero_point;
  SupportedTensors reduce_l1_input;
  SupportedTensors reduce_l2_input;
  SupportedTensors reduce_log_sum_input;
  SupportedTensors reduce_log_sum_exp_input;
  SupportedTensors reduce_max_input;
  SupportedTensors reduce_mean_input;
  SupportedTensors reduce_min_input;
  SupportedTensors reduce_product_input;
  SupportedTensors reduce_sum_input;
  SupportedTensors reduce_sum_square_input;
  SupportedTensors relu_input;
  SupportedTensors resample2d_input;
  SupportedTensors reshape_input;
  SupportedTensors reverse_input;
  SupportedTensors scatter_elements_input;
  SupportedTensors scatter_elements_indices;
  SupportedTensors scatter_nd_input;
  SupportedTensors scatter_nd_indices;
  SupportedTensors scatter_nd_updates;
  SupportedTensors sigmoid_input;
  SupportedTensors slice_input;
  SupportedTensors softmax_input;
  SupportedTensors softplus_input;
  SupportedTensors softsign_input;
  SupportedTensors split_input;
  SupportedTensors tanh_input;
  SupportedTensors tile_input;
  SupportedTensors transpose_input;
  SupportedTensors triangular_input;
  SupportedTensors where_condition;
  SupportedTensors where_value;
};

// clang-format off
inline bool operator==(const DataTypeLimits& lhs, const DataTypeLimits& rhs) {
  return lhs.input == rhs.input &&
         lhs.constant == rhs.constant &&
         lhs.arg_min_max_input == rhs.arg_min_max_input &&
         lhs.arg_min_max_output == rhs.arg_min_max_output &&
         lhs.batch_normalization_input == rhs.batch_normalization_input &&
         lhs.batch_normalization_mean == rhs.batch_normalization_mean &&
         lhs.cast_input == rhs.cast_input &&
         lhs.clamp_input == rhs.clamp_input &&
         lhs.concat_inputs == rhs.concat_inputs &&
         lhs.conv2d_input == rhs.conv2d_input &&
         lhs.conv2d_bias == rhs.conv2d_bias &&
         lhs.conv_transpose2d_input == rhs.conv_transpose2d_input &&
         lhs.conv_transpose2d_bias == rhs.conv_transpose2d_bias &&
         lhs.cumulative_sum_input == rhs.cumulative_sum_input &&
         lhs.dequantize_linear_input == rhs.dequantize_linear_input &&
         lhs.dequantize_linear_scale == rhs.dequantize_linear_scale &&
         lhs.dequantize_linear_zero_point == rhs.dequantize_linear_zero_point &&
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
         lhs.not_equal_input == rhs.not_equal_input &&
         lhs.logical_and_input == rhs.logical_and_input &&
         lhs.logical_or_input == rhs.logical_or_input &&
         lhs.logical_xor_input == rhs.logical_xor_input &&
         lhs.logical_not_input == rhs.logical_not_input &&
         lhs.is_nan_input == rhs.is_nan_input &&
         lhs.is_infinite_input == rhs.is_infinite_input &&
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
         lhs.round_even_input == rhs.round_even_input &&
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
         lhs.gemm_a == rhs.gemm_a &&
         lhs.gemm_c == rhs.gemm_c &&
         lhs.gru_input == rhs.gru_input &&
         lhs.gru_bias == rhs.gru_bias &&
         lhs.gru_output_sequence == rhs.gru_output_sequence &&
         lhs.gru_cell_input == rhs.gru_cell_input &&
         lhs.gru_cell_bias == rhs.gru_cell_bias &&
         lhs.hard_sigmoid_input == rhs.hard_sigmoid_input &&
         lhs.hard_swish_input == rhs.hard_swish_input &&
         lhs.instance_normalization_input == rhs.instance_normalization_input &&
         lhs.instance_normalization_scale == rhs.instance_normalization_scale &&
         lhs.layer_normalization_input == rhs.layer_normalization_input &&
         lhs.leaky_relu_input == rhs.leaky_relu_input &&
         lhs.linear_input == rhs.linear_input &&
         lhs.lstm_input == rhs.lstm_input &&
         lhs.lstm_bias == rhs.lstm_bias &&
         lhs.lstm_output_sequence == rhs.lstm_output_sequence &&
         lhs.lstm_cell_input == rhs.lstm_cell_input &&
         lhs.lstm_cell_bias == rhs.lstm_cell_bias &&
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
         lhs.reverse_input == rhs.reverse_input &&
         lhs.scatter_elements_input == rhs.scatter_elements_input &&
         lhs.scatter_elements_indices == rhs.scatter_elements_indices &&
         lhs.scatter_nd_input == rhs.scatter_nd_input &&
         lhs.scatter_nd_indices == rhs.scatter_nd_indices &&
         lhs.scatter_nd_updates == rhs.scatter_nd_updates &&
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
