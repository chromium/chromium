// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/mojom/context_properties_mojom_traits.h"

#include <limits.h>

#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_tensors.h"
#include "services/webnn/public/mojom/webnn_context_properties.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ContextPropertiesMojomTraitsTest, Basic) {
  static constexpr webnn::SupportedRanks kMaxRank =
      webnn::SupportedRanks::UpTo(8);

  webnn::ContextProperties input(
      webnn::InputOperandLayout::kNchw, webnn::Resample2DAxes::kChannelsFirst,
      webnn::BatchNormalizationAxis::kChannelsFirst,
      /*tensor_byte_length_limit=*/INT_MAX,
      {/*input=*/webnn::SupportedDataTypes::All(),
       /*constant=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*arg_min_max_input=*/
       {{webnn::OperandDataType::kInt32, webnn::OperandDataType::kInt64},
        kMaxRank},
       /*arg_min_max_output=*/
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       /*batch_normalization_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*batch_normalization_mean=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*cast_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*clamp_input=*/{{webnn::OperandDataType::kFloat16}, kMaxRank},
       /*concat_inputs=*/
       {{webnn::OperandDataType::kInt32, webnn::OperandDataType::kInt64},
        kMaxRank},
       /*conv2d_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*conv2d_bias=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*conv_transpose2d_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*conv_transpose2d_bias=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*cumulative_sum_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*dequantize_linear_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*dequantize_linear_scale=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*dequantize_linear_zero_point=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*add_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*sub_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*mul_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*div_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*max_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*min_input=*/
       {{webnn::OperandDataType::kInt32, webnn::OperandDataType::kInt64},
        kMaxRank},
       /*pow_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kInt8},
        kMaxRank},
       /*equal_input=*/{{webnn::OperandDataType::kFloat16}, kMaxRank},
       /*greater_input=*/{{webnn::OperandDataType::kFloat16}, kMaxRank},
       /*greater_or_equal_input=*/
       {{webnn::OperandDataType::kFloat16}, kMaxRank},
       /*lesser_input=*/{{webnn::OperandDataType::kFloat16}, kMaxRank},
       /*lesser_or_equal_input=*/{{webnn::OperandDataType::kFloat16}, kMaxRank},
       /*not_equal_input=*/{{webnn::OperandDataType::kFloat16}, kMaxRank},
       /*logical_and_input=*/{{webnn::OperandDataType::kFloat16}, kMaxRank},
       /*logical_or_input=*/{{webnn::OperandDataType::kFloat16}, kMaxRank},
       /*logical_xor_input=*/{{webnn::OperandDataType::kFloat16}, kMaxRank},
       /*logical_not_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*logical_output=*/{webnn::OperandDataType::kUint64},
       /*abs_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*ceil_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*cos_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*erf_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*exp_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*floor_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*identity_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*log_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*neg_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*reciprocal_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*sign_input=*/{{webnn::OperandDataType::kUint8}, kMaxRank},
       /*sin_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*sqrt_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*tan_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*elu_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*expand_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*gather_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*gather_indices=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*gather_elements_input=*/{{webnn::OperandDataType::kUint64}, kMaxRank},
       /*gather_elements_indices=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*gather_nd_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*gather_nd_indices=*/
       {{webnn::OperandDataType::kInt32, webnn::OperandDataType::kUint32,
         webnn::OperandDataType::kInt64},
        kMaxRank},
       /*gelu_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*gemm_a=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*gemm_c=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*gru_input=*/{{webnn::OperandDataType::kFloat32}, kMaxRank},
       /*gru_bias=*/{{webnn::OperandDataType::kFloat32}, kMaxRank},
       /*gru_cell_input=*/{{webnn::OperandDataType::kFloat32}, kMaxRank},
       /*gru_cell_bias=*/{{webnn::OperandDataType::kFloat32}, kMaxRank},
       /*hard_sigmoid_input=*/{{webnn::OperandDataType::kFloat32}, kMaxRank},
       /*hard_swish_input=*/{{webnn::OperandDataType::kFloat32}, kMaxRank},
       /*instance_normalization_input=*/
       {{webnn::OperandDataType::kFloat32}, kMaxRank},
       /*instance_normalization_scale=*/
       {{webnn::OperandDataType::kFloat32}, kMaxRank},
       /*layer_normalization_input=*/
       {{webnn::OperandDataType::kFloat32}, kMaxRank},
       /*leaky_relu_input=*/{{webnn::OperandDataType::kFloat32}, kMaxRank},
       /*linear_input=*/{{webnn::OperandDataType::kFloat32}, kMaxRank},
       /*lstm_input=*/{{webnn::OperandDataType::kFloat32}, kMaxRank},
       /*lstm_bias=*/{{webnn::OperandDataType::kFloat32}, kMaxRank},
       /*lstm_cell_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*lstm_cell_bias=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*matmul_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*pad_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*average_pool2d_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*l2_pool2d_input=*/{{webnn::OperandDataType::kUint8}, kMaxRank},
       /*max_pool2d_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*prelu_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*quantize_linear_input=*/{{webnn::OperandDataType::kFloat32}, kMaxRank},
       /*quantize_linear_zero_point=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kInt8},
        kMaxRank},
       /*reduce_l1_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kUint8},
        kMaxRank},
       /*reduce_l2_input=*/{{webnn::OperandDataType::kUint8}, kMaxRank},
       /*reduce_log_sum_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*reduce_log_sum_exp_input=*/
       {webnn::SupportedDataTypes::All(), kMaxRank},
       /*reduce_max_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*reduce_mean_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*reduce_min_input=*/{{webnn::OperandDataType::kFloat32}, kMaxRank},
       /*reduce_product_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*reduce_sum_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*reduce_sum_square_input=*/
       {{webnn::OperandDataType::kInt32, webnn::OperandDataType::kInt64},
        kMaxRank},
       /*relu_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*resample2d_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*reshape_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*reverse_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*scatter_elements_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*scatter_elements_indices=*/
       {{webnn::OperandDataType::kInt32, webnn::OperandDataType::kUint32},
        kMaxRank},
       /*scatter_nd_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*scatter_nd_indices=*/
       {{webnn::OperandDataType::kInt32, webnn::OperandDataType::kUint32},
        kMaxRank},
       /*scatter_nd_updates=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank},
       /*sigmoid_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*slice_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*softmax_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kUint8},
        kMaxRank},
       /*softplus_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kUint32},
        kMaxRank},
       /*softsign_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*split_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kInt8},
        kMaxRank},
       /*tanh_input=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kUint8},
        kMaxRank},
       /*tile_input=*/{{webnn::OperandDataType::kUint8}, kMaxRank},
       /*transpose_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*triangular_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*where_condition=*/{webnn::SupportedDataTypes::All(), kMaxRank},
       /*where_value=*/
       {{webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
        kMaxRank}});

  webnn::ContextProperties output(
      webnn::InputOperandLayout::kNhwc, webnn::Resample2DAxes::kChannelsFirst,
      webnn::BatchNormalizationAxis::kChannelsFirst,
      /*tensor_byte_length_limit=*/0,
      {{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
       {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
       {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
       {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
       {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
       {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
       {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}});

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<webnn::mojom::ContextProperties>(
          input, output));
  EXPECT_EQ(input, output);
}
