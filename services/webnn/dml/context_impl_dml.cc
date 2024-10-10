// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/349653202): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "services/webnn/dml/context_impl_dml.h"

#include <limits>

#include "base/bits.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/types/expected_macros.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/graph_impl_dml.h"
#include "services/webnn/dml/tensor_impl_dml.h"
#include "services/webnn/dml/utils.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"

namespace webnn::dml {

using Microsoft::WRL::ComPtr;

namespace {

void HandleTensorCreationFailure(
    const std::string& error_message,
    WebNNContextImpl::CreateTensorImplCallback callback) {
  std::move(callback).Run(base::unexpected(
      CreateError(mojom::Error::Code::kUnknownError, error_message)));
}

}  // namespace

// The context properties follow the supported feature level on the platform.
// https://learn.microsoft.com/en-us/windows/ai/directml/dml-feature-level-history
//
// TODO(crbug.com/345271830): update the context properties based on a certain
// feature level once there is a bundled DirectML.dll.
// static
ContextProperties ContextImplDml::GetProperties(
    DML_FEATURE_LEVEL feature_level) {
  CHECK_GE(feature_level, DML_FEATURE_LEVEL_4_0);

  static constexpr SupportedDataTypes kFloat16To32Ints8{
      OperandDataType::kFloat16, OperandDataType::kFloat32,
      OperandDataType::kInt8, OperandDataType::kUint8};

  static constexpr SupportedDataTypes kFloat16To32Ints32{
      OperandDataType::kFloat16, OperandDataType::kFloat32,
      OperandDataType::kInt32, OperandDataType::kUint32};

  static constexpr SupportedDataTypes kFloat16To32Ints8To32{
      OperandDataType::kFloat16, OperandDataType::kFloat32,
      OperandDataType::kInt8,    OperandDataType::kUint8,
      OperandDataType::kInt32,   OperandDataType::kUint32};

  static constexpr SupportedDataTypes kFloat16To32Int8To64{
      OperandDataType::kFloat16, OperandDataType::kFloat32,
      OperandDataType::kInt8, OperandDataType::kInt32, OperandDataType::kInt64};

  static constexpr SupportedDataTypes kInts4To32{
      OperandDataType::kInt4,  OperandDataType::kUint4,
      OperandDataType::kInt8,  OperandDataType::kUint8,
      OperandDataType::kInt32, OperandDataType::kUint32};

  static constexpr SupportedDataTypes kInts8To32{
      OperandDataType::kInt8, OperandDataType::kUint8, OperandDataType::kInt32,
      OperandDataType::kUint32};

  static constexpr SupportedDataTypes kUint8To32{OperandDataType::kUint8,
                                                 OperandDataType::kUint32};

  static constexpr SupportedDataTypes kGatherScatterIndicesSupportedDataTypes{
      OperandDataType::kInt32, OperandDataType::kUint32,
      OperandDataType::kInt64, OperandDataType::kUint64};

  // TODO: crbug.com/345271830 - specify data types for all parameters.
  ContextProperties properties(
      /*input_operand_layout=*/InputOperandLayout::kNchw, Resample2DAxes::kAny,
      {/*input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*constant=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,

       /*arg_min_max_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*arg_min_max_output=*/DataTypeConstraint::kInt32To64,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_batch_normalization_operator_desc#tensor-support
       /*batch_normalization_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_cast_operator_desc#tensor-support
       /*cast_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_clip_operator_desc#tensor-support
       /*clamp_input=*/kFloat16To32Ints8To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_join_operator_desc#tensor-support
       /*concat_inputs=*/kFloat16To32Ints8To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_convolution_operator_desc#tensor-support
       /*conv2d_input=*/DataTypeConstraint::kFloat16To32,
       /*conv_transpose2d_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_cumulative_summation_operator_desc#tensor-support
       /*cumulative_sum_input=*/kFloat16To32Ints32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_dequantize_linear_operator_desc#tensor-support
       /*dequantize_linear_input=*/kInts8To32,
       /*dequantize_linear_scale=*/DataTypeConstraint::kFloat32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_add_operator_desc#tensor-support
       /*add_input=*/kFloat16To32Ints32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_subtract_operator_desc#tensor-support
       /*sub_input=*/kFloat16To32Ints32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_multiply_operator_desc#tensor-support
       /*mul_input=*/kFloat16To32Ints32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_divide_operator_desc#tensor-support
       /*div_input=*/kFloat16To32Ints32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_max_operator_desc#tensor-support
       /*max_input=*/kFloat16To32Ints8To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_min_operator_desc#tensor-support
       /*min_input=*/kFloat16To32Ints8To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_pow_operator_desc#tensor-support
       /*pow_input=*/kFloat16To32Ints8To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_logical_equals_operator_desc#tensor-support
       /*equal_input=*/kFloat16To32Ints8To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_logical_greater_than_operator_desc#tensor-support
       /*greater_input=*/kFloat16To32Ints8To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_logical_greater_than_or_equal_operator_desc#tensor-support
       /*greater_or_equal_input=*/kFloat16To32Ints8To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_logical_less_than_operator_desc#tensor-support
       /*lesser_input=*/kFloat16To32Ints8To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_logical_less_than_or_equal_operator_desc#tensor-support
       /*lesser_or_equal_input=*/kFloat16To32Ints8To32,

       // TODO(crbug.com/368222740): Implement logical binary ops for DML.
       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_logical_and_operator_desc#tensor-support
       /*logical_and_input=*/{},
       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_logical_or_operator_desc#tensor-support
       /*logical_or_input=*/{},
       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_logical_xor_operator_desc#tensor-support
       /*logical_xor_input=*/{},

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_logical_not_operator_desc#tensor-support
       /*logical_not_input=*/kUint8To32,

       /*logical_output=*/kUint8To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_abs_operator_desc#tensor-support
       /*abs_input=*/DataTypeConstraint::kFloat16To32Int8To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_ceil_operator_desc#tensor-support
       /*ceil_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_cos_operator_desc#tensor-support
       /*cos_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_erf_operator_desc#tensor-support
       /*erf_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_exp_operator_desc#tensor-support
       /*exp_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_floor_operator_desc#tensor-support
       /*floor_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_identity_operator_desc#tensor-support
       /*identity_input=*/kFloat16To32Ints8To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_log_operator_desc#tensor-support
       /*log_input=*/DataTypeConstraint::kFloat16To32,

       // Neg is emulated by DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC, so the
       // data type limits is set based on the spec.
       // DML_ELEMENT_WISE_NEGATE_OPERATOR_DESC introduced in feature level 5.0
       // also supports int64.
       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_negate_operator_desc#tensor-support
       /*neg_input=*/DataTypeConstraint::kFloat16To32Int8To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_recip_operator_desc#tensor-support
       /*reciprocal_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_sign_operator_desc#tensor-support
       /*sign_input=*/DataTypeConstraint::kFloat16To32Int8To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_sin_operator_desc#tensor-support
       /*sin_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_sqrt_operator_desc#tensor-support
       /*sqrt_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_tan_operator_desc#tensor-support
       /*tan_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_activation_elu_operator_desc
       /*elu_input=*/DataTypeConstraint::kFloat16To32,

       // Expand is emulated by identity.
       /*expand_input=*/kFloat16To32Ints8To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_gather_operator_desc#tensor-support
       /*gather_input=*/kFloat16To32Ints8To32,
       /*gather_indices=*/kGatherScatterIndicesSupportedDataTypes,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_gather_elements_operator_desc#tensor-support
       /*gather_elements_input=*/kFloat16To32Ints8To32,
       /*gather_elements_indices=*/kGatherScatterIndicesSupportedDataTypes,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_gather_nd_operator_desc#tensor-support
       /*gather_nd_input=*/kFloat16To32Ints8To32,
       /*gather_nd_indices=*/kGatherScatterIndicesSupportedDataTypes,

       // Gelu is emulated when the feature level is less than 5.1.
       // https://learn.microsoft.com/en-us/windows/ai/directml/api/ns-directml-dml_activation_gelu_operator_desc
       /*gelu_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_gemm_operator_desc#tensor-support
       /*gemm_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_gru_operator_desc#tensor-support
       /*gru_input=*/DataTypeConstraint::kFloat16To32,
       /*gru_cell_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_activation_hard_sigmoid_operator_desc
       /*hard_sigmoid_input=*/DataTypeConstraint::kFloat16To32,

       // HardSwish is emulated when the feature level is less than 6.2.
       // https://learn.microsoft.com/en-us/windows/ai/directml/api/ns-directml-dml_activation_hard_swish_operator_desc
       /*hard_swish_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_mean_variance_normalization1_operator_desc#tensor-support
       /*instance_normalization_input=*/DataTypeConstraint::kFloat16To32,
       /*layer_normalization_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_activation_leaky_relu_operator_desc
       /*leaky_relu_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_activation_linear_operator_desc#tensor-support
       /*linear_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_lstm_operator_desc#tensor-support
       /*lstm_input=*/DataTypeConstraint::kFloat16To32,
       /*lstm_cell_input=*/DataTypeConstraint::kFloat16To32,

       // Matmul is emulated by gemm.
       /*matmul_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_padding_operator_desc#tensor-support
       /*pad_input=*/kFloat16To32Ints8To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_average_pooling_operator_desc
       /*average_pool2d_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_lp_pooling_operator_desc
       /*l2_pool2d_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_max_pooling_operator_desc
       /*max_pool2d_input=*/kFloat16To32Ints8,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_activation_parameterized_relu_operator_desc#tensor-support
       /*prelu_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_quantize_linear_operator_desc#tensor-support
       /*quantize_linear_input=*/DataTypeConstraint::kFloat32,
       /*quantize_linear_zero_point=*/DataTypeConstraint::kInts8,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_reduce_operator_desc#tensor-support-according-to-function
       /*reduce_l1_input=*/DataTypeConstraint::kFloat16To32,
       /*reduce_l2_input=*/DataTypeConstraint::kFloat16To32,
       /*reduce_log_sum_input=*/DataTypeConstraint::kFloat16To32,
       /*reduce_log_sum_exp_input=*/DataTypeConstraint::kFloat16To32,
       /*reduce_max_input=*/kFloat16To32Ints8To32,
       /*reduce_mean_input=*/DataTypeConstraint::kFloat16To32,
       /*reduce_min_input=*/kFloat16To32Ints8To32,
       /*reduce_product_input=*/DataTypeConstraint::kFloat16To32,
       /*reduce_sum_input=*/kFloat16To32Ints32,
       /*reduce_sum_square_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_activation_relu_operator_desc
       /*relu_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_resample_operator_desc#tensor-support
       /*resample2d_input=*/DataTypeConstraint::kFloat16To32,

       // Reshape is emulated by identity.
       /*reshape_input=*/kFloat16To32Ints8To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_scatter_nd_operator_desc#tensor-support
       /*scatter_nd_input=*/kFloat16To32Ints8To32,
       /*scatter_nd_indices=*/kGatherScatterIndicesSupportedDataTypes,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_activation_sigmoid_operator_desc#tensor-support
       /*sigmoid_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_slice_operator_desc#tensor-support
       /*slice_input=*/kFloat16To32Ints8To32,

       // Softmax is emulated when the feature level is less than 5.1.
       // https://learn.microsoft.com/en-us/windows/ai/directml/api/ns-directml-dml_activation_softmax1_operator_desc
       /*softmax_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_activation_relu_operator_desc#tensor-support
       /*softplus_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_activation_softsign_operator_desc#tensor-support
       /*softsign_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_split_operator_desc#tensor-support
       /*split_input=*/kFloat16To32Ints8To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_activation_tanh_operator_desc#tensor-support
       /*tanh_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_tile_operator_desc#tensor-support
       /*tile_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,

       // Transpose is emulated by identity.
       /*transpose_input=*/kFloat16To32Ints8To32,

       // Triangular is emulated by DML_FILL_VALUE_CONSTANT_OPERATOR_DESC,
       // DML_ELEMENT_WISE_MULTIPLY_OPERATOR_DESC,
       // DML_ELEMENT_WISE_BIT_AND_OPERATOR_DESC,
       // DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC and DML_SLICE_OPERATOR_DESC
       // when the feature level is less than 5.1, so the data type limit is set
       // based on these ops.
       // https://learn.microsoft.com/en-us/windows/ai/directml/api/ns-directml-dml_diagonal_matrix1_operator_desc#tensor-support
       /*triangular_input=*/kFloat16To32Ints32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_if_operator_desc
       /*where_condition=*/DataTypeConstraint::kUint8,
       /*where_value=*/kFloat16To32Ints8To32});

  if (feature_level >= DML_FEATURE_LEVEL_4_1) {
    properties.data_type_limits.concat_inputs =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.add_input =
        DataTypeConstraint::kFloat16To32Ints32To64;
    properties.data_type_limits.sub_input =
        DataTypeConstraint::kFloat16To32Ints32To64;
    properties.data_type_limits.mul_input =
        DataTypeConstraint::kFloat16To32Ints32To64;
    properties.data_type_limits.equal_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.greater_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.greater_or_equal_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.lesser_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.lesser_or_equal_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.abs_input = kFloat16To32Int8To64;
    properties.data_type_limits.identity_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.expand_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.gather_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.gather_elements_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.gather_nd_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.reshape_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.scatter_nd_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.sign_input =
        DataTypeConstraint::kFloat16To32Int8To64;
    properties.data_type_limits.slice_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.split_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.transpose_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.triangular_input =
        DataTypeConstraint::kFloat16To32Ints32To64;
  }

  if (feature_level >= DML_FEATURE_LEVEL_5_0) {
    properties.data_type_limits.clamp_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.cumulative_sum_input =
        DataTypeConstraint::kFloat16To32Ints32To64;
    properties.data_type_limits.max_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.min_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.pad_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.reduce_l1_input =
        DataTypeConstraint::kFloat16To32Ints32To64;
    properties.data_type_limits.reduce_max_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.reduce_min_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.reduce_sum_input =
        DataTypeConstraint::kFloat16To32Ints32To64;
    properties.data_type_limits.reduce_sum_square_input =
        DataTypeConstraint::kFloat16To32Ints32To64;
    properties.data_type_limits.where_value =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.max_pool2d_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
  }

  if (feature_level >= DML_FEATURE_LEVEL_5_1) {
    properties.data_type_limits.add_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.sub_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.mul_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.div_input = kFloat16To32Ints8To32;
    properties.data_type_limits.prelu_input =
        DataTypeConstraint::kFloat16To32Int8To32;
    properties.data_type_limits.relu_input =
        DataTypeConstraint::kFloat16To32Int8To32;
    properties.data_type_limits.triangular_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
  }

  if (feature_level >= DML_FEATURE_LEVEL_6_0) {
    properties.data_type_limits.div_input =
        DataTypeConstraint::kAllDataTypesAtLeast8bits;
    properties.data_type_limits.dequantize_linear_scale =
        DataTypeConstraint::kFloat16To32;
    properties.data_type_limits.quantize_linear_input =
        DataTypeConstraint::kFloat16To32;
  }

  if (feature_level >= DML_FEATURE_LEVEL_6_2) {
    properties.data_type_limits.resample2d_input = kFloat16To32Ints8;
  }

  if (feature_level >= DML_FEATURE_LEVEL_6_3) {
    properties.data_type_limits.input = SupportedDataTypes::All();
    properties.data_type_limits.constant = SupportedDataTypes::All();
    properties.data_type_limits.dequantize_linear_input = kInts4To32;
    properties.data_type_limits.quantize_linear_zero_point = kInts4To32;
  }

  return properties;
}

ContextImplDml::ContextImplDml(
    scoped_refptr<Adapter> adapter,
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    WebNNContextProviderImpl* context_provider,
    mojom::CreateContextOptionsPtr options,
    std::unique_ptr<CommandRecorder> command_recorder,
    const gpu::GpuFeatureInfo& gpu_feature_info)
    : WebNNContextImpl(std::move(receiver),
                       context_provider,
                       GetProperties(adapter->max_supported_feature_level()),
                       std::move(options)),
      adapter_(std::move(adapter)),
      command_recorder_(std::move(command_recorder)),
      gpu_feature_info_(gpu_feature_info) {
  CHECK(command_recorder_);
}

ContextImplDml::~ContextImplDml() = default;

base::WeakPtr<WebNNContextImpl> ContextImplDml::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void ContextImplDml::CreateGraphImpl(
    mojom::GraphInfoPtr graph_info,
    WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    WebNNContextImpl::CreateGraphImplCallback callback) {
  GraphImplDml::CreateAndBuild(
      adapter_, weak_factory_.GetWeakPtr(), std::move(graph_info),
      std::move(compute_resource_info), std::move(constant_operands),
      std::move(callback),
      gpu_feature_info_->IsWorkaroundEnabled(
          gpu::DML_EXECUTION_DISABLE_META_COMMANDS));
}

void ContextImplDml::CreateTensorImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    mojom::TensorInfoPtr tensor_info,
    CreateTensorImplCallback callback) {
  // DML requires resources to be in multiple of 4 bytes.
  // https://learn.microsoft.com/en-us/windows/ai/directml/dml-helper-functions#dmlcalcbuffertensorsize
  constexpr uint64_t kDMLBufferAlignment = 4ull;
  if (std::numeric_limits<uint64_t>::max() - kDMLBufferAlignment <
      static_cast<uint64_t>(tensor_info->descriptor.PackedByteLength())) {
    LOG(ERROR) << "[WebNN] Tensor is too large to create.";
    HandleTensorCreationFailure("Failed to create tensor.",
                                std::move(callback));
    return;
  }

  const uint64_t aligned_buffer_byte_size = base::bits::AlignUp(
      static_cast<uint64_t>(tensor_info->descriptor.PackedByteLength()),
      kDMLBufferAlignment);

  HRESULT hr = S_OK;

  // If adapter supports UMA, create the custom heap with CPU memory pool. The
  // CPU will directly read/write to this heap if the GPU isn't using it.
  ComPtr<ID3D12Resource> buffer;
  if (adapter_->IsUMA()) {
    // Create a buffer configured with memory properties based on
    // usage.
    if (tensor_info->usage.Has(MLTensorUsageFlags::kWrite)) {
      // Upload buffer is used when the buffer mostly CPU writes but
      // could also CPU read. A upload buffer provides less bandwidth for CPU
      // reads in favor of GPU writes being optimal.
      hr = CreateCustomUploadBuffer(
          adapter_->d3d12_device(), aligned_buffer_byte_size,
          L"WebNN_Custom_Upload_Buffer_External", buffer);
    } else if (tensor_info->usage.Has(MLTensorUsageFlags::kRead)) {
      // Readback buffer is used when the buffer only requires CPU reads.
      hr = CreateCustomReadbackBuffer(
          adapter_->d3d12_device(), aligned_buffer_byte_size,
          L"WebNN_Custom_Readback_Buffer_External", buffer);
    } else {
      // Default buffer is used when the buffer has no need for CPU access
      // in favor of any GPU access being optimal.
      hr = CreateDefaultBuffer(adapter_->d3d12_device(),
                               aligned_buffer_byte_size,
                               L"WebNN_Default_Buffer_External", buffer);
    }
  } else {
    // Create a default buffer that can be accessed only by GPU.
    // The CPU must use a staging buffer to read/write to this buffer.
    hr = CreateDefaultBuffer(adapter_->d3d12_device(), aligned_buffer_byte_size,
                             L"WebNN_Default_Buffer_External", buffer);
  }

  if (FAILED(hr)) {
    HandleTensorCreationFailure("Failed to create tensor.",
                                std::move(callback));
    HandleContextLostOrCrash("Failed to create the external buffer.", hr);
    return;
  }

  // The receiver bound to WebNNTensorImpl.
  //
  // Safe to use ContextImplDml* because this context owns the buffer
  // being connected and that context cannot destruct before the buffer.
  std::move(callback).Run(std::make_unique<TensorImplDml>(
      std::move(receiver), std::move(buffer), this, std::move(tensor_info)));
}

void ContextImplDml::ReadTensor(
    TensorImplDml* src_tensor,
    mojom::WebNNTensor::ReadTensorCallback callback) {
  const size_t src_tensor_size = src_tensor->PackedByteLength();

  HRESULT hr = S_OK;

  // Map entire buffer to readback the output data.
  if (adapter_->IsUMA() && adapter_->command_queue()->GetCompletedValue() >=
                               src_tensor->last_submission_fence_value()) {
    ContextImplDml::OnReadbackComplete(src_tensor->buffer(), src_tensor_size,
                                       std::move(callback), hr);
    return;
  }

  // Copy the buffer into a staging buffer to readback the output data.
  ComPtr<ID3D12Resource> download_buffer;
  hr = CreateReadbackBuffer(adapter_->d3d12_device(),
                            static_cast<uint64_t>(src_tensor_size),
                            L"WebNN_Readback_Buffer", download_buffer);
  if (FAILED(hr)) {
    std::move(callback).Run(ToError<mojom::ReadTensorResult>(
        mojom::Error::Code::kUnknownError, "Failed to read tensor."));
    HandleContextLostOrCrash("Failed to create the download buffer.", hr);
    return;
  }

  hr = StartRecordingIfNecessary();
  if (FAILED(hr)) {
    std::move(callback).Run(ToError<mojom::ReadTensorResult>(
        mojom::Error::Code::kUnknownError, "Failed to read tensor."));
    HandleRecordingError("Failed to start recording.", hr);
    return;
  }

  command_recorder_->ReadbackTensorWithBarrier(download_buffer, src_tensor,
                                               src_tensor_size);

  // Submit copy and schedule GPU wait.
  hr = command_recorder_->CloseAndExecute();
  if (FAILED(hr)) {
    std::move(callback).Run(ToError<mojom::ReadTensorResult>(
        mojom::Error::Code::kUnknownError, "Failed to read tensor."));
    HandleRecordingError("Failed to close and execute the command list.", hr);
    return;
  }

  // The source and readback buffer is held alive during execution by the
  // recorder by calling `ReadbackTensorWithBarrier()` then
  // CommandRecorder::CloseAndExecute().
  adapter_->command_queue()->WaitAsync(base::BindOnce(
      &ContextImplDml::OnReadbackComplete, weak_factory_.GetWeakPtr(),
      std::move(download_buffer), src_tensor_size, std::move(callback)));
}

void ContextImplDml::OnReadbackComplete(
    ComPtr<ID3D12Resource> download_buffer,
    size_t read_byte_size,
    mojom::WebNNTensor::ReadTensorCallback callback,
    HRESULT hr) {
  if (FAILED(hr)) {
    std::move(callback).Run(ToError<mojom::ReadTensorResult>(
        mojom::Error::Code::kUnknownError, "Failed to read tensor."));
    HandleRecordingError("Failed to download the buffer.", hr);
    return;
  }

  CHECK(download_buffer);

  // Copy over data from the download buffer to the destination buffer.
  void* mapped_download_data = nullptr;
  hr = download_buffer->Map(0, nullptr, &mapped_download_data);
  if (FAILED(hr)) {
    std::move(callback).Run(ToError<mojom::ReadTensorResult>(
        mojom::Error::Code::kUnknownError, "Failed to read tensor."));
    HandleContextLostOrCrash("Failed to map the download buffer.", hr);
    return;
  }

  mojo_base::BigBuffer dst_buffer(base::make_span(
      static_cast<const uint8_t*>(mapped_download_data), read_byte_size));

  download_buffer->Unmap(0, nullptr);

  std::move(callback).Run(
      mojom::ReadTensorResult::NewBuffer(std::move(dst_buffer)));
}

void ContextImplDml::WriteTensor(TensorImplDml* dst_tensor,
                                 mojo_base::BigBuffer src_buffer) {
  HRESULT hr = S_OK;
  ComPtr<ID3D12Resource> buffer_to_map = dst_tensor->buffer();

  // Create a staging buffer to upload data into when the existing buffer
  // cannot be updated by the CPU.
  if (!adapter_->IsUMA() || adapter_->command_queue()->GetCompletedValue() <
                                dst_tensor->last_submission_fence_value()) {
    hr = CreateUploadBuffer(adapter_->d3d12_device(), src_buffer.size(),
                            L"WebNN_Upload_Buffer", buffer_to_map);
    if (FAILED(hr)) {
      HandleContextLostOrCrash("Failed to create the upload buffer.", hr);
      return;
    }
  }

  CHECK(buffer_to_map);

  // Copy over data from the source buffer to the mapped buffer.
  void* mapped_buffer_data = nullptr;
  hr = buffer_to_map->Map(0, nullptr, &mapped_buffer_data);
  if (FAILED(hr)) {
    HandleContextLostOrCrash("Failed to map the buffer.", hr);
    return;
  }

  CHECK(mapped_buffer_data);

  // SAFETY: `buffer_to_map` was constructed with size `src_buffer.size()`.
  UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(mapped_buffer_data), src_buffer.size()))
      .copy_from(src_buffer);

  buffer_to_map->Unmap(0, nullptr);

  // Uploads are only required when the mapped buffer was a staging buffer.
  if (dst_tensor->buffer() != buffer_to_map.Get()) {
    hr = StartRecordingIfNecessary();
    if (FAILED(hr)) {
      HandleRecordingError("Failed to start recording.", hr);
      return;
    }

    command_recorder_->UploadTensorWithBarrier(
        dst_tensor, std::move(buffer_to_map), src_buffer.size());

    // TODO(crbug.com/40278771): consider not submitting after every write.
    // CloseAndExecute() only needs to be called once, when the tensor is read
    // by another context operation (ex. input into dispatch). Submitting
    // immediately prevents memory usage from increasing; however, it also
    // incurs more overhead due to a near empty command-list getting executed
    // every time.
    hr = command_recorder_->CloseAndExecute();
    if (FAILED(hr)) {
      HandleRecordingError("Failed to close and execute the command list.", hr);
      return;
    }

    // Since the queue owns the upload buffer, it does not need to be provided
    // to OnUploadComplete() and will be finally released once the wait is
    // satisfied.
    adapter_->command_queue()->WaitAsync(base::BindOnce(
        &ContextImplDml::OnUploadComplete, weak_factory_.GetWeakPtr()));
  }
}

void ContextImplDml::OnUploadComplete(HRESULT hr) {
  if (FAILED(hr)) {
    HandleRecordingError("Failed to upload the buffer.", hr);
    return;
  }
}

HRESULT ContextImplDml::StartRecordingIfNecessary() {
  // Recreate the recorder on error since resources recorded but
  // not executed would remain alive until this context gets destroyed and
  // this context would be prevented from recording new commands.
  if (!command_recorder_) {
    ASSIGN_OR_RETURN(command_recorder_,
                     CommandRecorder::Create(adapter_->command_queue(),
                                             adapter_->dml_device()));
  }

  CHECK(command_recorder_);

  // If the recorder is already recording, no need to re-open.
  if (command_recorder_->IsOpen()) {
    return S_OK;
  }

  // Open the command recorder for recording the context execution commands.
  RETURN_IF_FAILED(command_recorder_->Open());

  CHECK(command_recorder_->IsOpen());

  return S_OK;
}

void ContextImplDml::HandleRecordingError(std::string_view error_message,
                                       HRESULT hr) {
  command_recorder_.reset();
  HandleContextLostOrCrash(error_message, hr);
}

void ContextImplDml::HandleContextLostOrCrash(std::string_view message_for_log,
                                              HRESULT hr) {
  LOG(ERROR) << "[WebNN] " << message_for_log << " "
             << logging::SystemErrorCodeToString(hr);

  HRESULT device_removed_reason =
      adapter_->d3d12_device()->GetDeviceRemovedReason();
  if (FAILED(device_removed_reason)) {
    LOG(ERROR) << "[WebNN] Device Removed Reason: "
               << logging::SystemErrorCodeToString(device_removed_reason);
    // GPU/NPU contexts rely on the same device. If the device enters a
    // "device-removed" state, all affected contexts become unavailable and
    // should be destroyed immediately. Additionally, since other components
    // besides WebNN may reference the device, we have to terminate the GPU
    // process to allow for the re-creation of the device and recovery from
    // device removal.
    // TODO(crbug.com/364445586): Move non-GPU backends like TFLite outside of
    // the GPU process.
    context_provider()->DestroyContextsAndKillGpuProcess("device removed.");
    return;
  }

  std::string_view message_for_promise;
  switch (hr) {
    case E_OUTOFMEMORY:
      message_for_promise = "out of memory.";
      break;
    case DXGI_ERROR_DEVICE_RESET:
      message_for_promise = "device reset.";
      break;
    default:
      message_for_promise = "internal error.";
  }

  OnLost(base::StrCat({"WebNN context is lost due to ", message_for_promise}));
  CHECK(hr == E_OUTOFMEMORY || hr == DXGI_ERROR_DEVICE_RESET);
}

}  // namespace webnn::dml
