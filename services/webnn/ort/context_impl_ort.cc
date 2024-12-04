// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/context_impl_ort.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notimplemented.h"
#include "services/webnn/ort/graph_builder_ort.h"
#include "services/webnn/ort/graph_impl_ort.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/ort/platform_functions_ort.h"

namespace webnn::ort {

ContextImplOrt::ContextImplOrt(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    WebNNContextProviderImpl* context_provider,
    mojom::CreateContextOptionsPtr options)
    : WebNNContextImpl(std::move(receiver),
                       context_provider,
                       GetContextProperties(),
                       std::move(options)) {}

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
       /*clamp_input=*/{},
       /*concat_inputs=*/{},
       /*conv2d_input=*/{},
       /*conv_transpose2d_input=*/{},
       /*cumulative_sum_input=*/{},
       /*dequantize_linear_input=*/{},
       /*dequantize_linear_scale=*/{},
       /*add_input=*/{},
       /*sub_input=*/{},
       /*mul_input=*/{},
       /*div_input=*/{},
       /*max_input=*/{},
       /*min_input=*/{},
       /*pow_input=*/{},
       /*equal_input=*/{},
       /*greater_input=*/{},
       /*greater_or_equal_input=*/{},
       /*lesser_input=*/{},
       /*lesser_or_equal_input=*/{},
       /*logical_and_input=*/{},
       /*logical_or_input=*/{},
       /*logical_xor_input=*/{},
       /*logical_not_input=*/DataTypeConstraint::kUint8,
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
       /*gemm_input=*/{},
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
       /*matmul_input=*/{},
       /*pad_input=*/{},
       /*average_pool2d_input=*/{},
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
       /*reshape_input=*/{},
       /*reverse_input=*/{},
       /*scatter_elements_input=*/{},
       /*scatter_elements_indices=*/{},
       /*scatter_nd_input=*/{},
       /*scatter_nd_indices=*/{},
       /*sigmoid_input=*/{},
       /*slice_input=*/{},
       /*softmax_input=*/{},
       /*softplus_input=*/{},
       /*softsign_input=*/{},
       /*split_input=*/{},
       /*tanh_input=*/{},
       /*tile_input=*/{},
       /*transpose_input=*/{},
       /*triangular_input=*/{},
       /*where_condition=*/{},
       /*where_value=*/{}});
}

OrtEnv* ContextImplOrt::env_ = nullptr;
const OrtApi* ContextImplOrt::g_ort_ = nullptr;

// static
const OrtApi* ContextImplOrt::GetGlobalOrt() {
  if (g_ort_) {
    return g_ort_;
  }

  PlatformFunctions* platform_functions = PlatformFunctions::GetInstance();
  auto ort_get_api_base_proc = platform_functions->ort_get_api_base_proc();

  // currently, win11 inside onnxruntime.dll version is 1.10.1 and can support
  // IR_VERSION_2021_7_30.
  const char* version = ort_get_api_base_proc()->GetVersionString();
  LOG(ERROR) << "onnxruntime dll version is " << version;

  const OrtApi* g_ort = ort_get_api_base_proc()->GetApi(onnx::Version::IR_VERSION_2019_9_19);

  int num_providers = 0;
  char** providers;
  ORT_ABORT_ON_ERROR(g_ort, g_ort->GetAvailableProviders(&providers, &num_providers));
  LOG(ERROR) << "num_providers " << num_providers;
  // num_providers = 2
  for (int i = 0; i < num_providers; i++) {
    LOG(ERROR) << "provider: " << providers[i];
  }
  // provider[0]: DmlExecutionProvider
  // provider[1]: CPUExecutionProvider
  g_ort->ReleaseAvailableProviders(providers, num_providers);

  return g_ort;
}

// static
OrtEnv* ContextImplOrt::GetEnv(const OrtApi* g_ort) {
  if (env_) {
    return env_;
  }

  OrtEnv* env;
  ORT_ABORT_ON_ERROR(g_ort, g_ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "test", &env));

  return env;
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
  std::move(callback).Run(GraphImplOrt::CreateAndBuild(
      std::move(graph_info), std::move(compute_resource_info),
      std::move(constant_operands), this));
}

void ContextImplOrt::CreateTensorImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    mojom::TensorInfoPtr tensor_info,
    CreateTensorImplCallback callback) {
  NOTIMPLEMENTED()
      << "CreateTensorImpl is not implemented in OnnxRuntime backend.";
}

}  // namespace webnn::ort
