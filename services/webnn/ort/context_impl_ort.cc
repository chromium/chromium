// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/context_impl_ort.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notimplemented.h"
#include "services/webnn/error.h"
#include "services/webnn/ort/error_ort.h"
#include "services/webnn/ort/graph_builder_ort.h"
#include "services/webnn/ort/graph_impl_ort.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/ort/utils_ort.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::ort {

namespace {

void HandleTensorCreationFailure(
    const std::string& error_message,
    WebNNContextImpl::CreateTensorImplCallback callback) {
  std::move(callback).Run(base::unexpected(
      CreateError(mojom::Error::Code::kUnknownError, error_message)));
}

}  // namespace

ContextImplOrt::ContextImplOrt(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    WebNNContextProviderImpl* context_provider,
    mojom::CreateContextOptionsPtr options,
    ScopedOrtEnvPtr env)
    : WebNNContextImpl(std::move(receiver),
                       context_provider,
                       GetContextProperties(),
                       std::move(options)),
      env_(std::move(env)) {}

ContextImplOrt::~ContextImplOrt() = default;

// static
ContextProperties ContextImplOrt::GetContextProperties() {
  // TODO(https://github.com/shiyi9801/chromium/issues/103): Investigate how to
  // set the tensor byte length limit and supported tensor ranks
  static constexpr uint64_t kTensorByteLengthLimit =
      std::numeric_limits<int32_t>::max();

  static constexpr SupportedRanks kMaxRank = SupportedRanks::UpTo(8);
  static constexpr SupportedRanks kNonScalarMaxRank =
      SupportedRanks::NonScalarUpTo(8);

  static constexpr SupportedDataTypes kDequantizeLinearInputSupportedDataTypes{
      OperandDataType::kInt4, OperandDataType::kUint4, OperandDataType::kUint8,
      OperandDataType::kInt8, OperandDataType::kInt32};

  static constexpr SupportedDataTypes kQuantizeLinearInputSupportedDataTypes{
      OperandDataType::kFloat16, OperandDataType::kFloat32,
      OperandDataType::kInt32};

  return ContextProperties(
      InputOperandLayout::kNchw, Resample2DAxes::kChannelsFirst,
      /*tensor_byte_length_limit=*/kTensorByteLengthLimit,
      {/*input=*/SupportedDataTypes::All(),
       /*constant=*/SupportedDataTypes::All(),
       /*arg_min_max_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kNonScalarMaxRank},
       /*arg_min_max_output=*/DataTypeConstraint::kInt32To64,
       /*batch_normalization_input=*/
       {DataTypeConstraint::kFloat16To32, kNonScalarMaxRank},
       /*batch_normalization_mean=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)},
       /*cast_input=*/{DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*clamp_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*concat_inputs=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*conv2d_input=*/DataTypeConstraint::kFloat16To32,
       /*conv_transpose2d_input=*/DataTypeConstraint::kFloat16To32,
       /*cumulative_sum_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kNonScalarMaxRank},
       /*dequantize_linear_input=*/
       {kDequantizeLinearInputSupportedDataTypes, kMaxRank},
       /*dequantize_linear_scale=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*dequantize_linear_zero_point=*/
       {kDequantizeLinearInputSupportedDataTypes, kMaxRank},
       /*add_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*sub_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*mul_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*div_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*max_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*min_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*pow_input=*/
       {DataTypeConstraint::kFloat16To32, kMaxRank},
       /*equal_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*greater_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*greater_or_equal_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*lesser_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*lesser_or_equal_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*not_equal_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*logical_and_input=*/
       {DataTypeConstraint::kUint8, kMaxRank},
       /*logical_or_input=*/
       {DataTypeConstraint::kUint8, kMaxRank},
       /*logical_xor_input=*/
       {DataTypeConstraint::kUint8, kMaxRank},
       /*logical_not_input=*/{DataTypeConstraint::kUint8, kMaxRank},
       /*logical_output=*/DataTypeConstraint::kUint8,
       /*abs_input=*/{DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*ceil_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*cos_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*erf_input=*/{DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*exp_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*floor_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*identity_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*log_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*neg_input=*/{DataTypeConstraint::kFloat16To32Int8To64, kMaxRank},
       /*reciprocal_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*sign_input=*/{DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*sin_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*sqrt_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*tan_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*elu_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)},
       /*expand_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*gather_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*gather_indices=*/DataTypeConstraint::kInt32To64,
       /*gather_elements_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*gather_elements_indices=*/DataTypeConstraint::kInt32To64,
       /*gather_nd_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*gather_nd_indices=*/
       DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes,
       /*gelu_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*gemm_input=*/DataTypeConstraint::kFloat16To32Ints32To64,
       /*gru_input=*/{},
       /*gru_cell_input=*/{},
       /*hard_sigmoid_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*hard_swish_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*instance_normalization_input=*/DataTypeConstraint::kFloat16To32,
       /*layer_normalization_input=*/DataTypeConstraint::kFloat16To32,
       /*leaky_relu_input=*/
       {DataTypeConstraint::kFloat16To32, kMaxRank},
       /*linear_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*lstm_input=*/{},
       /*lstm_cell_input=*/{},
       /*matmul_input=*/
       {DataTypeConstraint::kFloat16To32Ints32To64, kMaxRank},
       // TODO: Support more data types including int4.
       // https://github.com/shiyi9801/chromium/issues/85
       /*pad_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*average_pool2d_input=*/
       {DataTypeConstraint::kFloat16To32, kNonScalarMaxRank},
       /*l2_pool2d_input=*/
       {DataTypeConstraint::kFloat16To32, kNonScalarMaxRank},
       /*max_pool2d_input=*/
       {DataTypeConstraint::kFloat16To32, kNonScalarMaxRank},
       /*prelu_input=*/
       {DataTypeConstraint::kFloat16To32Ints32To64, kMaxRank},
       /*quantize_linear_input=*/
       {kQuantizeLinearInputSupportedDataTypes, kMaxRank},
       /*quantize_linear_zero_point=*/
       {DataTypeConstraint::kInts4ToInts8, kMaxRank},
       /*reduce_l1_input=*/
       {DataTypeConstraint::kFloat16To32Ints32To64, kMaxRank},
       /*reduce_l2_input=*/
       {DataTypeConstraint::kFloat16To32Ints32To64, kMaxRank},
       /*reduce_log_sum_input=*/
       {DataTypeConstraint::kFloat16To32Ints32To64, kMaxRank},
       /*reduce_log_sum_exp_input=*/
       {DataTypeConstraint::kFloat16To32Ints32To64, kMaxRank},
       /*reduce_max_input=*/
       {DataTypeConstraint::kFloat16To32Ints32To64, kMaxRank},
       /*reduce_mean_input=*/
       {DataTypeConstraint::kFloat16To32Ints32To64, kMaxRank},
       /*reduce_min_input=*/
       {DataTypeConstraint::kFloat16To32Ints32To64, kMaxRank},
       /*reduce_product_input=*/
       {DataTypeConstraint::kFloat16To32Ints32To64, kMaxRank},
       /*reduce_sum_input=*/
       {DataTypeConstraint::kFloat16To32Ints32To64, kMaxRank},
       /*reduce_sum_square_input=*/
       {DataTypeConstraint::kFloat16To32Ints32To64, kMaxRank},
       /*relu_input=*/{DataTypeConstraint::kFloat16To32Int8To32, kMaxRank},
       /*resample2d_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*reshape_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*reverse_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*scatter_elements_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kNonScalarMaxRank},
       /*scatter_elements_indices=*/
       {DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes,
        kNonScalarMaxRank},
       /*scatter_nd_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kNonScalarMaxRank},
       /*scatter_nd_indices=*/
       {DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes,
        kNonScalarMaxRank},
       /*scatter_nd_updates=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*sigmoid_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*slice_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*softmax_input=*/{DataTypeConstraint::kFloat16To32, kNonScalarMaxRank},
       /*softplus_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*softsign_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*split_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kNonScalarMaxRank},
       /*tanh_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*tile_input=*/{DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*transpose_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*triangular_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, {2, 8}},
       /*where_condition=*/{DataTypeConstraint::kUint8, kMaxRank},
       /*where_value=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank}});
}

base::WeakPtr<WebNNContextImpl> ContextImplOrt::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void ContextImplOrt::CreateGraphImpl(
    mojom::GraphInfoPtr graph_info,
    WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    CreateGraphImplCallback callback) {
  GraphImplOrt::CreateAndBuild(
      std::move(graph_info), std::move(compute_resource_info),
      std::move(constant_operands), this, std::move(callback));
}

void ContextImplOrt::CreateTensorImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    mojom::TensorInfoPtr tensor_info,
    CreateTensorImplCallback callback) {
  std::move(callback).Run(
      TensorImplOrt::Create(std::move(receiver), this, std::move(tensor_info)));
}

}  // namespace webnn::ort
