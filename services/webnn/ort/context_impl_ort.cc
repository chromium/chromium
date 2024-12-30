// Copyright 2024 The Chromium Authors
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
    scoped_refptr<AllocatorOrt> allocator_ort)
    : WebNNContextImpl(std::move(receiver),
                       context_provider,
                       GetContextProperties(),
                       std::move(options)),
      allocator_ort_(std::move(allocator_ort)) {}

ContextImplOrt::~ContextImplOrt() = default;

// static
ContextProperties ContextImplOrt::GetContextProperties() {
  return ContextProperties(
      InputOperandLayout::kNchw, Resample2DAxes::kChannelsFirst,
      {/*input=*/SupportedDataTypes::All(),
       /*constant=*/SupportedDataTypes::All(),
       /*arg_min_max_input=*/{},
       /*arg_min_max_output=*/{},
       /*batch_normalization_input=*/{},
       /*cast_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*clamp_input=*/DataTypeConstraint::kFloat16To32,
       /*concat_inputs=*/{},
       /*conv2d_input=*/DataTypeConstraint::kFloat16To32,
       /*conv_transpose2d_input=*/{},
       /*cumulative_sum_input=*/{},
       /*dequantize_linear_input=*/{},
       /*dequantize_linear_scale=*/{},
       /*add_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*sub_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*mul_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*div_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*max_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*min_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*pow_input=*/DataTypeConstraint::kFloat16To32,
       /*equal_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*greater_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*greater_or_equal_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*lesser_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*lesser_or_equal_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*logical_and_input=*/DataTypeConstraint::kUint8,
       /*logical_or_input=*/DataTypeConstraint::kUint8,
       /*logical_xor_input=*/DataTypeConstraint::kUint8,
       /*logical_not_input=*/{},
       /*logical_output=*/DataTypeConstraint::kUint8,
       /*abs_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*ceil_input=*/DataTypeConstraint::kFloat16To32,
       /*cos_input=*/DataTypeConstraint::kFloat16To32,
       /*erf_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*exp_input=*/DataTypeConstraint::kFloat16To32,
       /*floor_input=*/DataTypeConstraint::kFloat16To32,
       /*identity_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*log_input=*/DataTypeConstraint::kFloat16To32,
       /*neg_input=*/DataTypeConstraint::kFloat16To32Int8To64,
       /*reciprocal_input=*/DataTypeConstraint::kFloat16To32,
       /*sign_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*sin_input=*/DataTypeConstraint::kFloat16To32,
       /*sqrt_input=*/DataTypeConstraint::kFloat16To32,
       /*tan_input=*/DataTypeConstraint::kFloat16To32,
       /*elu_input=*/{},
       /*expand_input=*/{},
       /*gather_input=*/{},
       /*gather_indices=*/{},
       /*gather_elements_input=*/{},
       /*gather_elements_indices=*/{},
       /*gather_nd_input=*/{},
       /*gather_nd_indices=*/{},
       /*gelu_input=*/{},
       /*gemm_input=*/DataTypeConstraint::kFloat16To32Ints32To64,
       /*gru_input=*/{},
       /*gru_cell_input=*/{},
       /*hard_sigmoid_input=*/{},
       /*hard_swish_input=*/{},
       /*instance_normalization_input=*/{},
       /*layer_normalization_input=*/{},
       /*leaky_relu_input=*/{},
       /*linear_input=*/{},
       /*lstm_input=*/{},
       /*lstm_cell_input=*/{},
       /*matmul_input=*/DataTypeConstraint::kFloat16To32Ints32To64,
       /*pad_input=*/{},
       /*average_pool2d_input=*/DataTypeConstraint::kFloat16To32,
       /*l2_pool2d_input=*/{},
       /*max_pool2d_input=*/{},
       /*prelu_input=*/{},
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
       /*relu_input=*/DataTypeConstraint::kFloat16To32Int8To32,
       /*resample2d_input=*/{},
       /*reshape_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*reverse_input=*/{},
       /*scatter_elements_input=*/{},
       /*scatter_elements_indices=*/{},
       /*scatter_nd_input=*/{},
       /*scatter_nd_indices=*/{},
       /*sigmoid_input=*/{},
       /*slice_input=*/{},
       /*softmax_input=*/DataTypeConstraint::kFloat16To32,
       /*softplus_input=*/{},
       /*softsign_input=*/{},
       /*split_input=*/{},
       /*tanh_input=*/{},
       /*tile_input=*/{},
       /*transpose_input=*/DataTypeConstraint::kAllDataTypesAtLeast8bits,
       /*triangular_input=*/{},
       /*where_condition=*/{},
       /*where_value=*/{}});
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
  std::move(callback).Run(std::make_unique<TensorImplOrt>(
      std::move(receiver), this, std::move(tensor_info)));
}

}  // namespace webnn::ort
