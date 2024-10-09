// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_MOJOM_DATA_TYPE_LIMITS_MOJOM_TRAITS_H_
#define SERVICES_WEBNN_PUBLIC_MOJOM_DATA_TYPE_LIMITS_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/webnn/public/cpp/data_type_limits.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<webnn::mojom::DataTypeLimitsDataView,
                    webnn::DataTypeLimits> {
  static webnn::SupportedDataTypes input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.input;
  }
  static webnn::SupportedDataTypes constant(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.constant;
  }
  static webnn::SupportedDataTypes arg_min_max_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.arg_min_max_input;
  }
  static webnn::SupportedDataTypes arg_min_max_output(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.arg_min_max_output;
  }
  static webnn::SupportedDataTypes batch_normalization_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.batch_normalization_input;
  }
  static webnn::SupportedDataTypes cast_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.cast_input;
  }
  static webnn::SupportedDataTypes clamp_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.clamp_input;
  }
  static webnn::SupportedDataTypes concat_inputs(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.concat_inputs;
  }
  static webnn::SupportedDataTypes conv2d_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.conv2d_input;
  }
  static webnn::SupportedDataTypes conv_transpose2d_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.conv_transpose2d_input;
  }
  static webnn::SupportedDataTypes cumulative_sum_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.cumulative_sum_input;
  }
  static webnn::SupportedDataTypes dequantize_linear_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.dequantize_linear_input;
  }
  static webnn::SupportedDataTypes dequantize_linear_scale(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.dequantize_linear_scale;
  }
  static webnn::SupportedDataTypes add_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.add_input;
  }
  static webnn::SupportedDataTypes sub_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.sub_input;
  }
  static webnn::SupportedDataTypes mul_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.mul_input;
  }
  static webnn::SupportedDataTypes div_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.div_input;
  }
  static webnn::SupportedDataTypes max_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.max_input;
  }
  static webnn::SupportedDataTypes min_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.min_input;
  }
  static webnn::SupportedDataTypes pow_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.pow_input;
  }
  static webnn::SupportedDataTypes equal_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.equal_input;
  }
  static webnn::SupportedDataTypes greater_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.greater_input;
  }
  static webnn::SupportedDataTypes greater_or_equal_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.greater_or_equal_input;
  }
  static webnn::SupportedDataTypes lesser_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.lesser_input;
  }
  static webnn::SupportedDataTypes lesser_or_equal_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.lesser_or_equal_input;
  }
  static webnn::SupportedDataTypes logical_and_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.logical_and_input;
  }
  static webnn::SupportedDataTypes logical_or_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.logical_or_input;
  }
  static webnn::SupportedDataTypes logical_xor_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.logical_xor_input;
  }
  static webnn::SupportedDataTypes logical_not_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.logical_not_input;
  }
  static webnn::SupportedDataTypes logical_output(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.logical_output;
  }
  static webnn::SupportedDataTypes abs_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.abs_input;
  }
  static webnn::SupportedDataTypes ceil_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.ceil_input;
  }
  static webnn::SupportedDataTypes cos_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.cos_input;
  }
  static webnn::SupportedDataTypes erf_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.erf_input;
  }
  static webnn::SupportedDataTypes exp_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.exp_input;
  }
  static webnn::SupportedDataTypes floor_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.floor_input;
  }
  static webnn::SupportedDataTypes identity_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.identity_input;
  }
  static webnn::SupportedDataTypes log_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.log_input;
  }
  static webnn::SupportedDataTypes neg_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.neg_input;
  }
  static webnn::SupportedDataTypes reciprocal_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.reciprocal_input;
  }
  static webnn::SupportedDataTypes sign_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.sign_input;
  }
  static webnn::SupportedDataTypes sin_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.sin_input;
  }
  static webnn::SupportedDataTypes sqrt_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.sqrt_input;
  }
  static webnn::SupportedDataTypes tan_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.tan_input;
  }
  static webnn::SupportedDataTypes elu_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.elu_input;
  }
  static webnn::SupportedDataTypes expand_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.expand_input;
  }
  static webnn::SupportedDataTypes gather_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.gather_input;
  }
  static webnn::SupportedDataTypes gather_indices(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.gather_indices;
  }
  static webnn::SupportedDataTypes gather_elements_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.gather_elements_input;
  }
  static webnn::SupportedDataTypes gather_elements_indices(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.gather_elements_indices;
  }
  static webnn::SupportedDataTypes gather_nd_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.gather_nd_input;
  }
  static webnn::SupportedDataTypes gather_nd_indices(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.gather_nd_indices;
  }
  static webnn::SupportedDataTypes gelu_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.gelu_input;
  }
  static webnn::SupportedDataTypes gemm_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.gemm_input;
  }
  static webnn::SupportedDataTypes gru_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.gru_input;
  }
  static webnn::SupportedDataTypes gru_cell_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.gru_cell_input;
  }
  static webnn::SupportedDataTypes hard_sigmoid_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.hard_sigmoid_input;
  }
  static webnn::SupportedDataTypes hard_swish_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.hard_swish_input;
  }
  static webnn::SupportedDataTypes instance_normalization_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.instance_normalization_input;
  }
  static webnn::SupportedDataTypes layer_normalization_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.layer_normalization_input;
  }
  static webnn::SupportedDataTypes leaky_relu_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.leaky_relu_input;
  }
  static webnn::SupportedDataTypes linear_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.linear_input;
  }
  static webnn::SupportedDataTypes lstm_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.lstm_input;
  }
  static webnn::SupportedDataTypes lstm_cell_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.lstm_cell_input;
  }
  static webnn::SupportedDataTypes matmul_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.matmul_input;
  }
  static webnn::SupportedDataTypes pad_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.pad_input;
  }
  static webnn::SupportedDataTypes average_pool2d_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.average_pool2d_input;
  }
  static webnn::SupportedDataTypes l2_pool2d_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.l2_pool2d_input;
  }
  static webnn::SupportedDataTypes max_pool2d_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.max_pool2d_input;
  }
  static webnn::SupportedDataTypes prelu_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.prelu_input;
  }
  static webnn::SupportedDataTypes quantize_linear_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.quantize_linear_input;
  }
  static webnn::SupportedDataTypes quantize_linear_zero_point(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.quantize_linear_zero_point;
  }
  static webnn::SupportedDataTypes reduce_l1_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.reduce_l1_input;
  }
  static webnn::SupportedDataTypes reduce_l2_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.reduce_l2_input;
  }
  static webnn::SupportedDataTypes reduce_log_sum_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.reduce_log_sum_input;
  }
  static webnn::SupportedDataTypes reduce_log_sum_exp_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.reduce_log_sum_exp_input;
  }
  static webnn::SupportedDataTypes reduce_max_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.reduce_max_input;
  }
  static webnn::SupportedDataTypes reduce_mean_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.reduce_mean_input;
  }
  static webnn::SupportedDataTypes reduce_min_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.reduce_min_input;
  }
  static webnn::SupportedDataTypes reduce_product_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.reduce_product_input;
  }
  static webnn::SupportedDataTypes reduce_sum_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.reduce_sum_input;
  }
  static webnn::SupportedDataTypes reduce_sum_square_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.reduce_sum_square_input;
  }
  static webnn::SupportedDataTypes relu_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.relu_input;
  }
  static webnn::SupportedDataTypes resample2d_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.resample2d_input;
  }
  static webnn::SupportedDataTypes reshape_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.reshape_input;
  }
  static webnn::SupportedDataTypes scatter_nd_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.scatter_nd_input;
  }
  static webnn::SupportedDataTypes scatter_nd_indices(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.scatter_nd_indices;
  }
  static webnn::SupportedDataTypes sigmoid_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.sigmoid_input;
  }
  static webnn::SupportedDataTypes slice_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.slice_input;
  }
  static webnn::SupportedDataTypes softmax_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.softmax_input;
  }
  static webnn::SupportedDataTypes softplus_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.softplus_input;
  }
  static webnn::SupportedDataTypes softsign_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.softsign_input;
  }
  static webnn::SupportedDataTypes split_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.split_input;
  }
  static webnn::SupportedDataTypes tanh_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.tanh_input;
  }
  static webnn::SupportedDataTypes tile_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.tile_input;
  }
  static webnn::SupportedDataTypes transpose_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.transpose_input;
  }
  static webnn::SupportedDataTypes triangular_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.triangular_input;
  }
  static webnn::SupportedDataTypes where_condition(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.where_condition;
  }
  static webnn::SupportedDataTypes where_value(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.where_value;
  }

  static bool Read(webnn::mojom::DataTypeLimitsDataView data,
                   webnn::DataTypeLimits* out) {
    return data.ReadInput(&out->input) && data.ReadConstant(&out->constant) &&
           data.ReadArgMinMaxInput(&out->arg_min_max_input) &&
           data.ReadArgMinMaxOutput(&out->arg_min_max_output) &&
           data.ReadBatchNormalizationInput(&out->batch_normalization_input) &&
           data.ReadCastInput(&out->cast_input) &&
           data.ReadClampInput(&out->clamp_input) &&
           data.ReadConcatInputs(&out->concat_inputs) &&
           data.ReadConv2dInput(&out->conv2d_input) &&
           data.ReadConvTranspose2dInput(&out->conv_transpose2d_input) &&
           data.ReadCumulativeSumInput(&out->cumulative_sum_input) &&
           data.ReadDequantizeLinearInput(&out->dequantize_linear_input) &&
           data.ReadDequantizeLinearScale(&out->dequantize_linear_scale) &&
           data.ReadAddInput(&out->add_input) &&
           data.ReadSubInput(&out->sub_input) &&
           data.ReadMulInput(&out->mul_input) &&
           data.ReadDivInput(&out->div_input) &&
           data.ReadMaxInput(&out->max_input) &&
           data.ReadMinInput(&out->min_input) &&
           data.ReadPowInput(&out->pow_input) &&
           data.ReadEqualInput(&out->equal_input) &&
           data.ReadGreaterInput(&out->greater_input) &&
           data.ReadGreaterOrEqualInput(&out->greater_or_equal_input) &&
           data.ReadLesserInput(&out->lesser_input) &&
           data.ReadLesserOrEqualInput(&out->lesser_or_equal_input) &&
           data.ReadLogicalAndInput(&out->logical_and_input) &&
           data.ReadLogicalOrInput(&out->logical_or_input) &&
           data.ReadLogicalXorInput(&out->logical_xor_input) &&
           data.ReadLogicalNotInput(&out->logical_not_input) &&
           data.ReadLogicalOutput(&out->logical_output) &&
           data.ReadAbsInput(&out->abs_input) &&
           data.ReadCeilInput(&out->ceil_input) &&
           data.ReadCosInput(&out->cos_input) &&
           data.ReadErfInput(&out->erf_input) &&
           data.ReadExpInput(&out->exp_input) &&
           data.ReadFloorInput(&out->floor_input) &&
           data.ReadIdentityInput(&out->identity_input) &&
           data.ReadLogInput(&out->log_input) &&
           data.ReadNegInput(&out->neg_input) &&
           data.ReadReciprocalInput(&out->reciprocal_input) &&
           data.ReadSignInput(&out->sign_input) &&
           data.ReadSinInput(&out->sin_input) &&
           data.ReadSqrtInput(&out->sqrt_input) &&
           data.ReadTanInput(&out->tan_input) &&
           data.ReadEluInput(&out->elu_input) &&
           data.ReadExpandInput(&out->expand_input) &&
           data.ReadGatherInput(&out->gather_input) &&
           data.ReadGatherIndices(&out->gather_indices) &&
           data.ReadGatherElementsInput(&out->gather_elements_input) &&
           data.ReadGatherElementsIndices(&out->gather_elements_indices) &&
           data.ReadGatherNdInput(&out->gather_nd_input) &&
           data.ReadGatherNdIndices(&out->gather_nd_indices) &&
           data.ReadGeluInput(&out->gelu_input) &&
           data.ReadGemmInput(&out->gemm_input) &&
           data.ReadGruInput(&out->gru_input) &&
           data.ReadGruCellInput(&out->gru_cell_input) &&
           data.ReadHardSigmoidInput(&out->hard_sigmoid_input) &&
           data.ReadHardSwishInput(&out->hard_swish_input) &&
           data.ReadInstanceNormalizationInput(
               &out->instance_normalization_input) &&
           data.ReadLayerNormalizationInput(&out->layer_normalization_input) &&
           data.ReadLeakyReluInput(&out->leaky_relu_input) &&
           data.ReadLinearInput(&out->linear_input) &&
           data.ReadLstmInput(&out->lstm_input) &&
           data.ReadLstmCellInput(&out->lstm_cell_input) &&
           data.ReadMatmulInput(&out->matmul_input) &&
           data.ReadPadInput(&out->pad_input) &&
           data.ReadAveragePool2dInput(&out->average_pool2d_input) &&
           data.ReadL2Pool2dInput(&out->l2_pool2d_input) &&
           data.ReadMaxPool2dInput(&out->max_pool2d_input) &&
           data.ReadPreluInput(&out->prelu_input) &&
           data.ReadQuantizeLinearInput(&out->quantize_linear_input) &&
           data.ReadQuantizeLinearZeroPoint(&out->quantize_linear_zero_point) &&
           data.ReadReduceL1Input(&out->reduce_l1_input) &&
           data.ReadReduceL2Input(&out->reduce_l2_input) &&
           data.ReadReduceLogSumInput(&out->reduce_log_sum_input) &&
           data.ReadReduceLogSumExpInput(&out->reduce_log_sum_exp_input) &&
           data.ReadReduceMaxInput(&out->reduce_max_input) &&
           data.ReadReduceMeanInput(&out->reduce_mean_input) &&
           data.ReadReduceMinInput(&out->reduce_min_input) &&
           data.ReadReduceProductInput(&out->reduce_product_input) &&
           data.ReadReduceSumInput(&out->reduce_sum_input) &&
           data.ReadReduceSumSquareInput(&out->reduce_sum_square_input) &&
           data.ReadReluInput(&out->relu_input) &&
           data.ReadResample2dInput(&out->resample2d_input) &&
           data.ReadReshapeInput(&out->reshape_input) &&
           data.ReadScatterNdInput(&out->scatter_nd_input) &&
           data.ReadScatterNdIndices(&out->scatter_nd_indices) &&
           data.ReadSigmoidInput(&out->sigmoid_input) &&
           data.ReadSliceInput(&out->slice_input) &&
           data.ReadSoftmaxInput(&out->softmax_input) &&
           data.ReadSoftplusInput(&out->softplus_input) &&
           data.ReadSoftsignInput(&out->softsign_input) &&
           data.ReadSplitInput(&out->split_input) &&
           data.ReadTanhInput(&out->tanh_input) &&
           data.ReadTileInput(&out->tile_input) &&
           data.ReadTransposeInput(&out->transpose_input) &&
           data.ReadTriangularInput(&out->triangular_input) &&
           data.ReadWhereCondition(&out->where_condition) &&
           data.ReadWhereValue(&out->where_value);
  }
};

}  // namespace mojo

#endif  // SERVICES_WEBNN_PUBLIC_MOJOM_DATA_TYPE_LIMITS_MOJOM_TRAITS_H_
