// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/mojom/context_properties_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_context_properties.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ContextPropertiesMojomTraitsTest, Basic) {
  webnn::ContextProperties input(
      webnn::InputOperandLayout::kNchw, webnn::Resample2DAxes::kChannelsFirst,
      {/*input=*/webnn::SupportedDataTypes::All(),
       /*constant=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*arg_min_max_input=*/
       {webnn::OperandDataType::kInt32, webnn::OperandDataType::kInt64},
       /*arg_min_max_output=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*batch_normalization_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*cast_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*clamp_input=*/{webnn::OperandDataType::kFloat16},
       /*concat_inputs=*/
       {webnn::OperandDataType::kInt32, webnn::OperandDataType::kInt64},
       /*conv2d_input=*/webnn::SupportedDataTypes::All(),
       /*conv_transpose2d_input=*/webnn::SupportedDataTypes::All(),
       /*cumulative_sum_input=*/webnn::SupportedDataTypes::All(),
       /*dequantize_linear_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*dequantize_linear_scale=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*add_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*sub_input=*/webnn::SupportedDataTypes::All(),
       /*mul_input=*/webnn::SupportedDataTypes::All(),
       /*div_input=*/webnn::SupportedDataTypes::All(),
       /*max_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*min_input=*/
       {webnn::OperandDataType::kInt32, webnn::OperandDataType::kInt64},
       /*pow_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kInt8},
       /*equal_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*greater_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*greater_or_equal_input=*/{webnn::OperandDataType::kFloat16},
       /*lesser_input=*/
       {webnn::OperandDataType::kInt32, webnn::OperandDataType::kInt64},
       /*lesser_or_equal_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*logical_and_input=*/{webnn::OperandDataType::kUint8},
       /*logical_or_input=*/{webnn::OperandDataType::kUint8},
       /*logical_xor_input=*/{webnn::OperandDataType::kUint8},
       /*logical_not_input=*/webnn::SupportedDataTypes::All(),
       /*logical_output=*/{webnn::OperandDataType::kUint64},
       /*abs_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*ceil_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*cos_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*erf_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*exp_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*floor_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*identity_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*log_input=*/webnn::SupportedDataTypes::All(),
       /*neg_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*reciprocal_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*sign_input=*/{webnn::OperandDataType::kUint8},
       /*sin_input=*/webnn::SupportedDataTypes::All(),
       /*sqrt_input=*/webnn::SupportedDataTypes::All(),
       /*tan_input=*/webnn::SupportedDataTypes::All(),
       /*elu_input=*/webnn::SupportedDataTypes::All(),
       /*expand_input=*/webnn::SupportedDataTypes::All(),
       /*gather_input=*/webnn::SupportedDataTypes::All(),
       /*gather_indices=*/webnn::SupportedDataTypes::All(),
       /*gather_elements_input=*/{webnn::OperandDataType::kUint64},
       /*gather_elements_indices=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*gather_nd_input=*/webnn::SupportedDataTypes::All(),
       /*gather_nd_indices=*/
       {webnn::OperandDataType::kInt32, webnn::OperandDataType::kUint32,
        webnn::OperandDataType::kInt64},
       /*gelu_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*gemm_input=*/webnn::SupportedDataTypes::All(),
       /*gru_input=*/{webnn::OperandDataType::kFloat32},
       /*gru_cell_input=*/{webnn::OperandDataType::kFloat32},
       /*hard_sigmoid_input=*/{webnn::OperandDataType::kFloat32},
       /*hard_swish_input=*/{webnn::OperandDataType::kFloat32},
       /*instance_normalization_input=*/{webnn::OperandDataType::kFloat32},
       /*layer_normalization_input=*/{webnn::OperandDataType::kFloat32},
       /*leaky_relu_input=*/{webnn::OperandDataType::kFloat32},
       /*linear_input=*/{webnn::OperandDataType::kFloat32},
       /*lstm_input=*/{webnn::OperandDataType::kFloat32},
       /*lstm_cell_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*matmul_input=*/webnn::SupportedDataTypes::All(),
       /*pad_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*average_pool2d_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*l2_pool2d_input=*/{webnn::OperandDataType::kUint8},
       /*max_pool2d_input=*/webnn::SupportedDataTypes::All(),
       /*prelu_input=*/webnn::SupportedDataTypes::All(),
       /*quantize_linear_input=*/{webnn::OperandDataType::kFloat32},
       /*quantize_linear_zero_point=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kInt8},
       /*reduce_l1_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kUint8},
       /*reduce_l2_input=*/{webnn::OperandDataType::kUint8},
       /*reduce_log_sum_input=*/webnn::SupportedDataTypes::All(),
       /*reduce_log_sum_exp_input=*/webnn::SupportedDataTypes::All(),
       /*reduce_max_input=*/webnn::SupportedDataTypes::All(),
       /*reduce_mean_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*reduce_min_input=*/{webnn::OperandDataType::kFloat32},
       /*reduce_product_input=*/webnn::SupportedDataTypes::All(),
       /*reduce_sum_input=*/webnn::SupportedDataTypes::All(),
       /*reduce_sum_square_input=*/
       {webnn::OperandDataType::kInt32, webnn::OperandDataType::kInt64},
       /*relu_input=*/webnn::SupportedDataTypes::All(),
       /*resample2d_input=*/webnn::SupportedDataTypes::All(),
       /*reshape_input=*/webnn::SupportedDataTypes::All(),
       /*reverse_input=*/webnn::SupportedDataTypes::All(),
       /*scatter_elements_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*scatter_elements_indices=*/
       {webnn::OperandDataType::kInt32, webnn::OperandDataType::kUint32},
       /*scatter_nd_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*scatter_nd_indices=*/
       {webnn::OperandDataType::kInt32, webnn::OperandDataType::kUint32},
       /*sigmoid_input=*/webnn::SupportedDataTypes::All(),
       /*slice_input=*/webnn::SupportedDataTypes::All(),
       /*softmax_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kUint8},
       /*softplus_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kUint32},
       /*softsign_input=*/webnn::SupportedDataTypes::All(),
       /*split_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kInt8},
       /*tanh_input=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kUint8},
       /*tile_input=*/{webnn::OperandDataType::kUint8},
       /*transpose_input=*/webnn::SupportedDataTypes::All(),
       /*triangular_input=*/webnn::SupportedDataTypes::All(),
       /*where_condition=*/webnn::SupportedDataTypes::All(),
       /*where_value=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32}});

  webnn::ContextProperties output(
      webnn::InputOperandLayout::kNhwc, webnn::Resample2DAxes::kChannelsFirst,
      {{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
       {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
       {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
       {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
       {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
       {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}});

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<webnn::mojom::ContextProperties>(
          input, output));
  EXPECT_EQ(input, output);
}
