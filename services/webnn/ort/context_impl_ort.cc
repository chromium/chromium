// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/context_impl_ort.h"

#include "base/feature_list.h"
#include "services/webnn/ort/graph_impl_ort.h"
#include "services/webnn/ort/ort_data_type.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/tensor_impl_ort.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/scoped_sequence.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::ort {

namespace {

// The feature flag allows us to try using device allocator to create device
// tensors for EPs, e.g. OpenVINO EP.
BASE_FEATURE(kUseDeviceTensor, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

// static
std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter> ContextImplOrt::Create(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    base::WeakPtr<WebNNContextProviderImpl> context_provider,
    const EpWorkarounds& ep_workarounds,
    mojom::CreateContextOptionsPtr options,
    mojom::Device device_type,
    mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
    mojo::ScopedDataPipeProducerHandle read_tensor_producer,
    scoped_refptr<Environment> env,
    gpu::CommandBufferId command_buffer_id,
    std::unique_ptr<ScopedSequence> sequence,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    ScopedTrace scoped_trace) {
  DCHECK(owning_task_runner->RunsTasksInCurrentSequence());
  auto task_runner = owning_task_runner;
  std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter> context_impl(
      new ContextImplOrt(
          std::move(receiver), std::move(context_provider),
          std::move(ep_workarounds), std::move(options), device_type,
          std::move(write_tensor_consumer), std::move(read_tensor_producer),
          std::move(env), command_buffer_id, std::move(sequence),
          std::move(memory_tracker), std::move(owning_task_runner),
          shared_image_manager, std::move(main_task_runner)),
      OnTaskRunnerDeleter(std::move(task_runner)));
  return context_impl;
}

ContextImplOrt::ContextImplOrt(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    base::WeakPtr<WebNNContextProviderImpl> context_provider,
    const EpWorkarounds& ep_workarounds,
    mojom::CreateContextOptionsPtr options,
    mojom::Device device_type,
    mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
    mojo::ScopedDataPipeProducerHandle write_tensor_producer,
    scoped_refptr<Environment> env,
    gpu::CommandBufferId command_buffer_id,
    std::unique_ptr<ScopedSequence> sequence,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : WebNNContextImpl(
          std::move(receiver),
          std::move(context_provider),
          GetContextProperties(ep_workarounds.resample2d_limit_to_nchw),
          std::move(options),
          std::move(write_tensor_consumer),
          std::move(write_tensor_producer),
          command_buffer_id,
          std::move(sequence),
          std::move(memory_tracker),
          std::move(owning_task_runner),
          shared_image_manager,
          std::move(main_task_runner)),
      env_(std::move(env)),
      session_options_(SessionOptions::Create(device_type, env_)) {
  if (base::FeatureList::IsEnabled(kUseDeviceTensor)) {
    device_allocator_ = DeviceAllocator::Create(this->options().device,
                                                session_options_->get(), env_);
  }
}

ContextImplOrt::~ContextImplOrt() = default;

// static
ContextProperties ContextImplOrt::GetContextProperties(
    bool resample2d_limit_to_nchw) {
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
      InputOperandLayout::kNchw,
      resample2d_limit_to_nchw ? Resample2DAxes::kChannelsFirst
                               : Resample2DAxes::kAny,
      BatchNormalizationAxis::kChannelsFirst,
      /*tensor_byte_length_limit=*/kTensorByteLengthLimit,
      {/*input=*/{SupportedDataTypes::All(), kMaxRank},
       /*constant=*/{SupportedDataTypes::All(), kMaxRank},
       /*arg_min_max_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxNonScalarRank},
       // ONNX ArgMin/Max only supports int64 output, int32 output is supported
       // by inserting a cast operator.
       /*arg_min_max_output=*/{DataTypeConstraint::kInt32To64, kMaxRank},
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
       /*is_nan_input=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
       /*is_infinite_input*/ {DataTypeConstraint::kFloat16To32, kMaxRank},
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
       /*round_even_input*/ {DataTypeConstraint::kFloat16To32, kMaxRank},
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
       /*gru_output_sequence=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(4)},
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
       /*lstm_output_sequence=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(4)},
       /*lstm_cell_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(2)},
       /*lstm_cell_bias=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)},
       /*matmul_input=*/{DataTypeConstraint::kFloat16To32Ints32To64, kMaxRank},
       /*pad_input=*/
       {DataTypeConstraint::kAllDataTypesAtLeast8bits, kMaxRank},
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
       /*softmax_input=*/{DataTypeConstraint::kFloat16To32, kMaxNonScalarRank},
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
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

base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
ContextImplOrt::CreateTensorImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    mojom::TensorInfoPtr tensor_info) {
  // TODO(crbug.com/332350952): Implement constant tensors for ORT backend.
  if (tensor_info->usage.Has(MLTensorUsageFlags::kGraphConstant)) {
    return base::unexpected(
        mojom::Error::New(mojom::Error::Code::kNotSupportedError,
                          "Creation of constant tensors is not supported."));
  }
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  OrtAllocator* allocator = nullptr;
  bool can_access_on_cpu = true;
  // Use the device allocator if it's present and should be used. Otherwise, use
  // the default allocator which is CPU based and non-arena.
  if (device_allocator_ && device_allocator_->ShouldUse(tensor_info)) {
    allocator = device_allocator_->get();
    can_access_on_cpu = device_allocator_->CanAccessOnCPU();
  } else {
    // `GetAllocatorWithDefaultOptions()` always returns the same pointer to the
    // same default allocator and its returned value should NOT be freed.
    CHECK_STATUS(ort_api->GetAllocatorWithDefaultOptions(&allocator));
  }
  CHECK(allocator);

  ONNXTensorElementDataType ort_data_type =
      WebnnToOnnxDataType(tensor_info->descriptor.data_type());
  std::vector<int64_t> ort_shape =
      WebnnToOnnxShape(tensor_info->descriptor.shape());

  // TODO(crbug.com/453420646): Implement context lost handling for ORT tensor
  // creation failures.
  // TODO(crbug.com/445971854): Emit mojom::Error since CreateTensorAsOrtValue()
  // could malloc and fail if OOM.
  ScopedOrtValue tensor;
  CHECK_STATUS(ort_api->CreateTensorAsOrtValue(
      allocator, ort_shape.data(), ort_shape.size(), ort_data_type,
      ScopedOrtValue::Receiver(tensor).get()));
  CHECK(tensor.get());

  size_t size;
  CHECK_STATUS(ort_api->GetTensorSizeInBytes(tensor.get(), &size));

  // Invalid values are rejected in GraphBuilder.
  CHECK(base::IsValueInRangeForNumericType<int>(size));

  return base::MakeRefCounted<TensorImplOrt>(
      std::move(receiver), AsWeakPtr(), std::move(tensor_info), size,
      std::move(tensor), can_access_on_cpu);
}

base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
ContextImplOrt::CreateTensorFromSharedImageImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    mojom::TensorInfoPtr tensor_info,
    WebNNTensorImpl::RepresentationPtr representation) {
  return base::unexpected(
      mojom::Error::New(mojom::Error::Code::kNotSupportedError,
                        "WebGPU Interop is not supported."));
}

}  // namespace webnn::ort
