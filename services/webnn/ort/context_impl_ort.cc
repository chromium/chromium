// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/context_impl_ort.h"

#include "base/notimplemented.h"
#include "services/webnn/ort/buffer_content_ort.h"
#include "services/webnn/ort/graph_impl_ort.h"
#include "services/webnn/ort/tensor_impl_ort.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/queueable_resource_state.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::ort {

ContextImplOrt::ContextImplOrt(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    WebNNContextProviderImpl* context_provider,
    mojom::CreateContextOptionsPtr options,
    ScopedOrtEnv env,
    scoped_refptr<SessionOptions> session_options)
    : WebNNContextImpl(std::move(receiver),
                       context_provider,
                       GetContextProperties(),
                       std::move(options)),
      env_(std::move(env)),
      session_options_(std::move(session_options)) {}

ContextImplOrt::~ContextImplOrt() = default;

// static
ContextProperties ContextImplOrt::GetContextProperties() {
  // TODO(crbug.com/412844034): Investigate how to set the tensor byte length
  // limit and supported tensor ranks.
  static constexpr uint64_t kTensorByteLengthLimit =
      std::numeric_limits<int32_t>::max();

  static constexpr SupportedRanks kMaxRank = SupportedRanks::UpTo(8);
  static constexpr SupportedRanks kMaxNonScalarRank =
      SupportedRanks::NonScalarUpTo(8);

  static constexpr SupportedDataTypes kFloat16To32Int32To64{
      OperandDataType::kFloat32, OperandDataType::kFloat16,
      OperandDataType::kInt32, OperandDataType::kInt64};

  static constexpr SupportedDataTypes kInts8Float16To32 = {
      OperandDataType::kUint8, OperandDataType::kInt8,
      OperandDataType::kFloat16, OperandDataType::kFloat32};

  return ContextProperties(
      InputOperandLayout::kNchw, Resample2DAxes::kChannelsFirst,
      BatchNormalizationAxis::kChannelsFirst,
      /*tensor_byte_length_limit=*/kTensorByteLengthLimit,
      {/*input=*/SupportedDataTypes::All(),
       /*constant=*/SupportedDataTypes::All(),
       /*arg_min_max_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxNonScalarRank},
       // ONNX ArgMin/Max only supports int64 output, int32 output is supported
       // by inserting a cast operator.
       /*arg_min_max_output=*/DataTypeConstraint::kInt32To64,
       /*batch_normalization_input=*/{},
       /*batch_normalization_mean=*/{},
       /*cast_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*clamp_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*concat_inputs=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxNonScalarRank},
       /*conv2d_input=*/{DataTypeConstraint::kFloat16To32, {3, 8}},
       /*conv2d_bias=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)},
       /*conv_transpose2d_input=*/{DataTypeConstraint::kFloat16To32, {3, 8}},
       /*conv_transpose2d_bias=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)},
       /*cumulative_sum_input=*/{},
       /*dequantize_linear_input=*/{},
       /*dequantize_linear_scale=*/{},
       /*dequantize_linear_zero_point=*/{},
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
       /*pow_input=*/{kFloat16To32Int32To64, kMaxRank},
       /*equal_input=*/{},
       /*greater_input=*/{},
       /*greater_or_equal_input=*/{},
       /*lesser_input=*/{},
       /*lesser_or_equal_input=*/{},
       /*not_equal_input=*/{},
       /*logical_and_input=*/{},
       /*logical_or_input=*/{},
       /*logical_xor_input=*/{},
       /*logical_not_input=*/{},
       /*logical_output=*/{},
       /*abs_input=*/{DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*ceil_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*cos_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*erf_input=*/{DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*exp_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*floor_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*identity_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*log_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*neg_input=*/{DataTypeConstraint::kFloat16To32Int8To64, kMaxRank},
       /*reciprocal_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*sign_input=*/{DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*sin_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*sqrt_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*tan_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*elu_input=*/{},
       /*expand_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*gather_input=*/{},
       /*gather_indices=*/{},
       /*gather_elements_input=*/{},
       /*gather_elements_indices=*/{},
       /*gather_nd_input=*/{},
       /*gather_nd_indices=*/{},
       /*gelu_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*gemm_a=*/
       {DataTypeConstraint::kFloat16To32Ints32To64, SupportedRanks::Exactly(2)},
       /*gemm_c=*/
       {DataTypeConstraint::kFloat16To32Ints32To64, SupportedRanks::UpTo(2)},
       /*gru_input=*/{},
       /*gru_bias=*/{},
       /*gru_cell_input=*/{},
       /*gru_cell_bias=*/{},
       /*hard_sigmoid_input=*/{},
       /*hard_swish_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*instance_normalization_input=*/{},
       /*instance_normalization_scale=*/{},
       /*layer_normalization_input=*/{},
       /*leaky_relu_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*linear_input=*/{},
       /*lstm_input=*/{},
       /*lstm_bias=*/{},
       /*lstm_cell_input=*/{},
       /*lstm_cell_bias=*/{},
       /*matmul_input=*/{},
       /*pad_input=*/{},
       /*average_pool2d_input=*/
       {DataTypeConstraint::kFloat16To32, {3, 8}},
       /*l2_pool2d_input=*/
       {DataTypeConstraint::kFloat16To32, {3, 8}},
       /*max_pool2d_input=*/
       {kInts8Float16To32, {3, 8}},
       /*prelu_input=*/{DataTypeConstraint::kFloat16To32Ints32To64, kMaxRank},
       /*quantize_linear_input=*/{},
       /*quantize_linear_zero_point=*/{},
       /*reduce_l1_input=*/{},
       /*reduce_l2_input=*/{},
       /*reduce_log_sum_input=*/{},
       /*reduce_log_sum_exp_input=*/{},
       /*reduce_max_input=*/{},
       /*reduce_mean_input=*/{},
       /*reduce_min_input=*/{},
       /*reduce_product_input=*/{},
       /*reduce_sum_input=*/{},
       /*reduce_sum_square_input=*/{},
       /*relu_input=*/{DataTypeConstraint::kFloat16To32Int8To64, kMaxRank},
       /*resample2d_input=*/{},
       // TODO(crbug.com/425151000): Add int4/uint4 support for reshape once the
       // related ORT issue is fixed.
       // https://github.com/microsoft/onnxruntime/issues/24285
       /*reshape_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*reverse_input=*/{},
       /*scatter_elements_input=*/{},
       /*scatter_elements_indices=*/{},
       /*scatter_nd_input=*/{},
       /*scatter_nd_indices=*/{},
       /*scatter_nd_updates=*/{},
       /*sigmoid_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*slice_input=*/{},
       /*softmax_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*softplus_input=*/{},
       /*softsign_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*split_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxNonScalarRank},
       /*tanh_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*tile_input=*/{},
       /*transpose_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*triangular_input=*/{},
       /*where_condition=*/{},
       /*where_value=*/{}});
}

base::WeakPtr<WebNNContextImpl> ContextImplOrt::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void ContextImplOrt::CreateGraphImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    mojom::GraphInfoPtr graph_info,
    WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    base::flat_map<OperandId, WebNNTensorImpl*> constant_tensor_operands,
    CreateGraphImplCallback callback) {
  GraphImplOrt::CreateAndBuild(
      std::move(receiver), std::move(graph_info),
      std::move(compute_resource_info), std::move(constant_operands),
      std::move(constant_tensor_operands), this, std::move(callback));
}

void ContextImplOrt::CreateTensorImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    mojom::TensorInfoPtr tensor_info,
    CreateTensorImplCallback callback) {
  // TODO(crbug.com/332350952): Implement constant tensors for ORT backend.
  if (tensor_info->usage.Has(MLTensorUsageFlags::kGraphConstant)) {
    std::move(callback).Run(base::unexpected(
        mojom::Error::New(mojom::Error::Code::kNotSupportedError,
                          "Creation of constant tensors is not supported.")));
    return;
  }

  auto buffer_content =
      std::make_unique<BufferContentOrt>(tensor_info->descriptor);
  auto buffer_state =
      base::MakeRefCounted<QueueableResourceState<BufferContentOrt>>(
          std::move(buffer_content));
  std::move(callback).Run(std::make_unique<TensorImplOrt>(
      std::move(receiver), this, std::move(tensor_info),
      std::move(buffer_state)));
}

}  // namespace webnn::ort
