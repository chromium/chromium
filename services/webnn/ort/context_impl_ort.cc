// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/context_impl_ort.h"

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
    scoped_refptr<Environment> env)
    : WebNNContextImpl(std::move(receiver),
                       context_provider,
                       GetContextProperties(),
                       std::move(options)),
      env_(std::move(env)),
      session_options_(SessionOptions::Create(this->options().device)),
      is_external_data_supported_(
          env_->IsExternalDataSupported(this->options().device)) {}

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

  static constexpr SupportedDataTypes kFloat16To32Uint8Int32To64 = {
      OperandDataType::kFloat16, OperandDataType::kFloat32,
      OperandDataType::kUint8, OperandDataType::kInt32,
      OperandDataType::kInt64};

  static constexpr SupportedDataTypes kFloat16To32Uint8Int8To32 = {
      OperandDataType::kFloat16, OperandDataType::kFloat32,
      OperandDataType::kUint8, OperandDataType::kInt8, OperandDataType::kInt32};

  static constexpr SupportedDataTypes kFloat16To32Int64 = {
      OperandDataType::kFloat16, OperandDataType::kFloat32,
      OperandDataType::kInt64};

  static constexpr SupportedDataTypes kInts4To8Int32 = {
      OperandDataType::kInt4, OperandDataType::kUint4, OperandDataType::kUint8,
      OperandDataType::kInt8, OperandDataType::kInt32};

  static constexpr SupportedDataTypes kFloat16To32Int32 = {
      OperandDataType::kFloat16, OperandDataType::kFloat32,
      OperandDataType::kInt32};

  return ContextProperties(
      InputOperandLayout::kNchw, Resample2DAxes::kAny,
      BatchNormalizationAxis::kChannelsFirst,
      /*tensor_byte_length_limit=*/kTensorByteLengthLimit,
      {/*input=*/SupportedDataTypes::All(),
       /*constant=*/SupportedDataTypes::All(),
       /*arg_min_max_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxNonScalarRank},
       // ONNX ArgMin/Max only supports int64 output, int32 output is supported
       // by inserting a cast operator.
       /*arg_min_max_output=*/DataTypeConstraint::kInt32To64,
       /*batch_normalization_input=*/
       {DataTypeConstraint::kFloat16To32, kMaxNonScalarRank},
       /*batch_normalization_mean=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)},
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
       /*cumulative_sum_input=*/{kFloat16To32Int32To64, kMaxNonScalarRank},
       /*dequantize_linear_input=*/{kInts4To8Int32, kMaxRank},
       /*dequantize_linear_scale=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*dequantize_linear_zero_point=*/{kInts4To8Int32, kMaxRank},
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
       /*identity_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*log_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*neg_input=*/{DataTypeConstraint::kFloat16To32Int8To64, kMaxRank},
       /*reciprocal_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*sign_input=*/{DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*sin_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*sqrt_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*tan_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*elu_input=*/
       {DataTypeConstraint::kFloat16To32, kMaxRank},
       /*expand_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*gather_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxNonScalarRank},
       /*gather_indices=*/{DataTypeConstraint::kInt32To64, kMaxRank},
       /*gather_elements_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxNonScalarRank},
       /*gather_elements_indices=*/
       {DataTypeConstraint::kInt32To64, kMaxNonScalarRank},
       /*gather_nd_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxNonScalarRank},
       /*gather_nd_indices=*/
       {DataTypeConstraint::kInt32To64, kMaxNonScalarRank},
       /*gelu_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*gemm_a=*/
       {DataTypeConstraint::kFloat16To32Ints32To64, SupportedRanks::Exactly(2)},
       /*gemm_c=*/
       {DataTypeConstraint::kFloat16To32Ints32To64, SupportedRanks::UpTo(2)},
       /*gru_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(3)},
       /*gru_bias=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(2)},
       /*gru_cell_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(2)},
       /*gru_cell_bias=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)},
       /*hard_sigmoid_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*hard_swish_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*instance_normalization_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(4)},
       /*instance_normalization_scale=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)},
       /*layer_normalization_input=*/
       {DataTypeConstraint::kFloat16To32, kMaxRank},
       /*leaky_relu_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*linear_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*lstm_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(3)},
       /*lstm_bias=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(2)},
       /*lstm_cell_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(2)},
       /*lstm_cell_bias=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)},
       /*matmul_input=*/{DataTypeConstraint::kFloat16To32Ints32To64, kMaxRank},
       /*pad_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxNonScalarRank},
       /*average_pool2d_input=*/{DataTypeConstraint::kFloat16To32, {3, 8}},
       /*l2_pool2d_input=*/{DataTypeConstraint::kFloat16To32, {3, 8}},
       /*max_pool2d_input=*/{kInts8Float16To32, {3, 8}},
       /*prelu_input=*/{DataTypeConstraint::kFloat16To32Ints32To64, kMaxRank},
       /*quantize_linear_input=*/{kFloat16To32Int32, kMaxRank},
       /*quantize_linear_zero_point=*/
       {DataTypeConstraint::kInts4ToInts8, kMaxRank},
       /*reduce_l1_input=*/
       {kFloat16To32Int32To64, kMaxRank},
       /*reduce_l2_input=*/
       {kFloat16To32Int32To64, kMaxRank},
       /*reduce_log_sum_input=*/
       {kFloat16To32Int32To64, kMaxRank},
       /*reduce_log_sum_exp_input=*/
       {kFloat16To32Int32To64, kMaxRank},
       /*reduce_max_input=*/
       {kFloat16To32Int32To64, kMaxRank},
       /*reduce_mean_input=*/
       {kFloat16To32Int32To64, kMaxRank},
       /*reduce_min_input=*/
       {kFloat16To32Int32To64, kMaxRank},
       /*reduce_product_input=*/
       {kFloat16To32Int32To64, kMaxRank},
       /*reduce_sum_input=*/
       {kFloat16To32Int32To64, kMaxRank},
       /*reduce_sum_square_input=*/
       {kFloat16To32Int32To64, kMaxRank},
       /*relu_input=*/{DataTypeConstraint::kFloat16To32Int8To64, kMaxRank},
       /*resample2d_input=*/
       {kFloat16To32Uint8Int8To32, SupportedRanks::Exactly(4)},
       // TODO(crbug.com/425151000): Add int4/uint4 support for reshape once the
       // related ORT issue is fixed.
       // https://github.com/microsoft/onnxruntime/issues/24285
       /*reshape_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*reverse_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*scatter_elements_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxNonScalarRank},
       /*scatter_elements_indices=*/
       {DataTypeConstraint::kInt32To64, kMaxNonScalarRank},
       /*scatter_nd_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxNonScalarRank},
       /*scatter_nd_indices=*/
       {DataTypeConstraint::kInt32To64, kMaxNonScalarRank},
       /*scatter_nd_updates=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*sigmoid_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*slice_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*softmax_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*softplus_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*softsign_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*split_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxNonScalarRank},
       /*tanh_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*tile_input=*/{DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
       /*transpose_input=*/{SupportedDataTypes::All(), kMaxRank},
       /*triangular_input=*/{kFloat16To32Int64, {2, 8}},
       /*where_condition=*/{DataTypeConstraint::kUint8, kMaxRank},
       // TODO(crbug.com/429859156): ORT CPU EP should support int8, uint32, and
       // uint64 for where operation.
       /*where_value=*/
       {kFloat16To32Uint8Int32To64, kMaxRank}});
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
  std::move(callback).Run(base::MakeRefCounted<TensorImplOrt>(
      std::move(receiver), AsWeakPtr(), std::move(tensor_info),
      std::move(buffer_state)));
}

void ContextImplOrt::CreateTensorFromMailboxImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    mojom::TensorInfoPtr tensor_info,
    gpu::Mailbox mailbox,
    CreateTensorImplCallback callback) {
  std::move(callback).Run(
      base::unexpected(mojom::Error::New(mojom::Error::Code::kNotSupportedError,
                                         "WebGPU Interop is not supported.")));
}

}  // namespace webnn::ort
