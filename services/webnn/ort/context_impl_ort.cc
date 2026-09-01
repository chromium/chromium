// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/context_impl_ort.h"

#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/cstring_view.h"
#include "services/webnn/gpu_task_scheduler.h"
#include "services/webnn/ort/graph_impl_ort.h"
#include "services/webnn/ort/ort_data_type.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/tensor_impl_ort.h"
#include "services/webnn/public/cpp/execution_providers_info.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_service_introspection.mojom.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::ort {

namespace {

using Microsoft::WRL::ComPtr;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Additions to this enum should also be reflected in RecordFirstSelectedEP().
//
// LINT.IfChange(WebNNOrtEPUma)
enum class WebNNOrtEPUma {
  kOther = 0,
  kCPU = 1,
  kDml = 2,
  kWebGpu = 3,
  kMIGraphX = 4,
  kNvTensorRT = 5,
  kOpenVINO = 6,
  kQNN = 7,
  kVitisAI = 8,
  kMaxValue = kVitisAI,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webnn/enums.xml:WebNNOrtEPUma)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(WebNNOrtDeviceUma)
enum class WebNNOrtDeviceUma {
  kCPU = 0,
  kGPU = 1,
  kNPU = 2,
  kMaxValue = kNPU,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webnn/enums.xml:WebNNOrtDeviceUma)

void RecordFirstSelectedEP(std::string_view ep_name) {
  // It is expected that `ep_name` is one of the execution providers defined in
  // `services/webnn/public/cpp/execution_providers_info.h`. If a new EP is
  // added, it should be added to this map as well. Otherwise, it will be
  // recorded as `kOther`.
  static constexpr auto kEPUmaMap =
      base::MakeFixedFlatMap<std::string_view, WebNNOrtEPUma>({
          {kCPUExecutionProvider, WebNNOrtEPUma::kCPU},
          {kDmlExecutionProvider, WebNNOrtEPUma::kDml},
          {kWebGpuExecutionProvider, WebNNOrtEPUma::kWebGpu},
          {kMIGraphXExecutionProvider, WebNNOrtEPUma::kMIGraphX},
          {kNvTensorRTRTXExecutionProvider, WebNNOrtEPUma::kNvTensorRT},
          {kOpenVINOExecutionProvider, WebNNOrtEPUma::kOpenVINO},
          {kQNNExecutionProvider, WebNNOrtEPUma::kQNN},
          {kVitisAIExecutionProvider, WebNNOrtEPUma::kVitisAI},
      });
  static_assert(
      kEPUmaMap.size() == static_cast<size_t>(WebNNOrtEPUma::kMaxValue),
      "All EP enum values (except kOther) must be in kEPUmaMap.");

  auto it = kEPUmaMap.find(ep_name);
  WebNNOrtEPUma uma_value = WebNNOrtEPUma::kOther;
  if (it != kEPUmaMap.end()) {
    uma_value = it->second;
  }
  base::UmaHistogramEnumeration("WebNN.ORT.FirstSelectedEP", uma_value);
}

void RecordFirstSelectedDevice(OrtHardwareDeviceType device_type) {
  WebNNOrtDeviceUma uma_value;
  switch (device_type) {
    case OrtHardwareDeviceType_CPU:
      uma_value = WebNNOrtDeviceUma::kCPU;
      break;
    case OrtHardwareDeviceType_GPU:
      uma_value = WebNNOrtDeviceUma::kGPU;
      break;
    case OrtHardwareDeviceType_NPU:
      uma_value = WebNNOrtDeviceUma::kNPU;
      break;
  }
  base::UmaHistogramEnumeration("WebNN.ORT.FirstSelectedDevice", uma_value);
}

// The feature flag allows us to try using device allocator to create device
// tensors for EPs, e.g. OpenVINO EP.
BASE_FEATURE(kUseDeviceTensor, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

// static
std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter> ContextImplOrt::Create(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    base::WeakPtr<WebNNContextProviderImpl> context_provider,
    mojom::CreateContextOptionsPtr options,
    mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
    mojo::ScopedDataPipeProducerHandle read_tensor_producer,
    scoped_refptr<Environment> env,
    scoped_refptr<SessionOptions> session_options,
    std::unique_ptr<GpuTaskScheduler> gpu_task_scheduler,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    ScopedTrace scoped_trace) {
  DCHECK(owning_task_runner->RunsTasksInCurrentSequence());
  auto task_runner = owning_task_runner;

  // Currently，only device type from WebNN context options is used to
  // determine ORT device type.
  // TODO(crbug.com/469455162): Use power preference and accelerated
  // attributes from WebNN context options to determine ORT device type.
  OrtHardwareDeviceType device_type = WebnnToOrtDeviceType(options->device);
  const EpWorkarounds ep_workarounds = env->GetEpWorkarounds(device_type);

  std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter> context_impl(
      new ContextImplOrt(
          std::move(receiver), std::move(context_provider),
          std::move(ep_workarounds),
          std::move(options), std::move(session_options),
          std::move(write_tensor_consumer), std::move(read_tensor_producer),
          std::move(env), std::move(gpu_task_scheduler),
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
    scoped_refptr<SessionOptions> session_options,
    mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
    mojo::ScopedDataPipeProducerHandle write_tensor_producer,
    scoped_refptr<Environment> env,
    std::unique_ptr<GpuTaskScheduler> gpu_task_scheduler,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : WebNNContextImpl(
          std::move(receiver),
          std::move(context_provider),
          ContextBackendUma::kONNXRuntime,
          GetContextProperties(ep_workarounds.resample2d_limit_to_nchw),
          std::move(options),
          std::move(write_tensor_consumer),
          std::move(write_tensor_producer),
          std::move(gpu_task_scheduler),
          std::move(memory_tracker),
          std::move(owning_task_runner),
          shared_image_manager,
          std::move(main_task_runner)),
      env_(std::move(env)),
      session_options_(std::move(session_options)) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  const OrtEpDevice* first_selected_device =
      session_options_->first_selected_device();

  std::string_view ep_name = ort_api->EpDevice_EpName(first_selected_device);
  RecordFirstSelectedEP(ep_name);

  OrtHardwareDeviceType hardware_device_type = ort_api->HardwareDevice_Type(
      ort_api->EpDevice_Device(first_selected_device));
  RecordFirstSelectedDevice(hardware_device_type);

  if (base::FeatureList::IsEnabled(kUseDeviceTensor)) {
    device_allocator_ = DeviceAllocator::Create(session_options_, env_);
  }

  ScopedOrtExternalResourceImporter external_resource_importer;
  const OrtInteropApi* ort_interop_api =
      PlatformFunctions::GetInstance()->ort_interop_api();
  if (ort_interop_api) {
    ScopedOrtStatus status(
        ort_interop_api->CreateExternalResourceImporterForDevice(
            first_selected_device, ScopedOrtExternalResourceImporter::Receiver(
                                       external_resource_importer)
                                       .get()));
    if (!status.is_valid() && external_resource_importer != nullptr) {
      bool can_import = false;
      CHECK_STATUS(ort_interop_api->CanImportMemory(
          external_resource_importer.get(),
          ORT_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP, &can_import));

      if (can_import) {
        external_resource_importer_ = std::move(external_resource_importer);
      }
    }
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
       /*dequantize_linear_input=*/
       {kInts4To8Int32, kMaxRank},
       /*dequantize_linear_scale=*/{DataTypeConstraint::kFloat16To32, kMaxRank},
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
       // TODO(crbug.com/470357751): Support uint32 indices when ORT CPU EP
       // implements it.
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void ContextImplOrt::HandleContextLostOrCrash(const std::string& error_message,
                                              OrtErrorCode error_code) {
  // Currently, we decide to destroy all contexts and kill the GPU process for
  // any error code.
  // TODO(crbug.com/462937875): Handle errors differently when ORT can report a
  // device-removal error code in the future.
  //
  // Currently, we decide to not broadcast the lost reason across all contexts
  // especially for contexts created from different origins as it may leak
  // information about one origin to another.
  // TODO(crbug.com/474141334): Broadcast the context lost reason across all
  // contexts from the same origin.
  OnLost(error_message);
  DestroyAllContextsAndKillGpuProcess();
}

void ContextImplOrt::CreateGraphImpl(
    mojom::GraphInfoPtr graph_info,
    WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    CreateGraphImplCallback callback) {
  GraphImplOrt::CreateAndBuild(
      std::move(graph_info), std::move(compute_resource_info),
      std::move(constant_operands), *this, std::move(callback));
}

base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
ContextImplOrt::CreateTensorImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    mojom::TensorInfoPtr tensor_info) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  OrtAllocator* allocator = nullptr;
  // Use the device allocator if it's present. Otherwise, use the default
  // allocator which is CPU based and non-arena.
  if (device_allocator_) {
    allocator = device_allocator_->get();
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
      std::move(receiver), *this, std::move(tensor_info), size,
      std::move(tensor), device_allocator_);
}

base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
ContextImplOrt::CreateTensorFromSharedImageImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    mojom::TensorInfoPtr tensor_info,
    WebNNTensorImpl::RepresentationPtr representation) {
  // ORT requires a thread-safe shared image representation because it accesses
  // shared image data on the WebNN sequence.
  DCHECK(representation->is_thread_safe());
  if (!representation->is_thread_safe()) {
    return base::unexpected(
        mojom::Error::New(mojom::Error::Code::kNotSupportedError,
                          "WebGPU interop is not supported."));
  }

  // Validate the shared image logical size matches TensorInfo.
  // The logical size must equal PackedByteLength() since read/write access
  // requires an exact buffer size. Only the backing heap allocation may be
  // rounded up for alignment.
  const size_t shared_image_byte_size =
      base::checked_cast<size_t>(representation->size().width());
  if (shared_image_byte_size != tensor_info->descriptor.PackedByteLength()) {
    return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                              "Failed to create tensor."));
  }

  base::win::ScopedHandle d3d12_heap_handle;
  if (external_resource_importer_.is_valid()) {
    d3d12_heap_handle = representation->GetD3D12HeapHandle();
  }

  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  ScopedOrtExternalMemoryHandle external_memory_handle;
  ScopedOrtValue tensor;
  ONNXTensorElementDataType ort_data_type =
      WebnnToOnnxDataType(tensor_info->descriptor.data_type());
  std::vector<int64_t> ort_shape =
      WebnnToOnnxShape(tensor_info->descriptor.shape());
  bool can_access_on_cpu = false;
  if (d3d12_heap_handle.is_valid()) {
    const OrtInteropApi* ort_interop_api =
        PlatformFunctions::GetInstance()->ort_interop_api();

    OrtExternalMemoryDescriptor memory_descriptor{};
    memory_descriptor.version = ORT_API_VERSION;
    memory_descriptor.handle_type = ORT_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP;
    memory_descriptor.size_bytes = shared_image_byte_size;
    memory_descriptor.native_handle = d3d12_heap_handle.get();
    ScopedOrtExternalMemoryHandle memory_handle;
    ScopedOrtStatus status(ort_interop_api->ImportMemory(
        external_resource_importer_.get(), &memory_descriptor,
        ScopedOrtExternalMemoryHandle::Receiver(memory_handle).get()));
    base::UmaHistogramBoolean("WebNN.ORT.ImportMemorySuccess",
                              status.is_valid());
    if (!status.is_valid()) {
      OrtExternalTensorDescriptor tensor_descriptor{};
      tensor_descriptor.version = ORT_API_VERSION;
      tensor_descriptor.element_type = ort_data_type;
      tensor_descriptor.shape = ort_shape.data();
      tensor_descriptor.rank = ort_shape.size();

      CHECK_STATUS(ort_interop_api->CreateTensorFromMemory(
          external_resource_importer_.get(), memory_handle.get(),
          &tensor_descriptor, ScopedOrtValue::Receiver(tensor).get()));
      // Successfully imported the D3D12 buffer as an ORT tensor.
      external_memory_handle = std::move(memory_handle);

      // The OrtValue owns the lifetime of this OrtMemoryInfo.
      const OrtMemoryInfo* memory_info;
      CHECK_STATUS(ort_api->GetTensorMemoryInfo(tensor.get(), &memory_info));
      can_access_on_cpu = ort_api->MemoryInfoGetDeviceMemType(memory_info) ==
                          OrtDeviceMemoryType_HOST_ACCESSIBLE;
    } else {
      LOG(WARNING) << "[WebNN] ImportMemory failed for D3D12 buffer. Falling "
                      "back to using mapped buffers.";
    }
  }

  // If external resource importer is not available or import fails, fall back
  // to using mapped D3D12 buffer. ORT holds no reference of its own to the
  // buffer in this case, so `d3d12_buffer` is kept below and passed to
  // TensorImplOrt to keep the buffer alive independent of `representation`.
  ComPtr<ID3D12Resource> d3d12_buffer;
  if (tensor.get() == nullptr) {
    d3d12_buffer = representation->GetD3D12Buffer();
    CHECK(d3d12_buffer)
        << "[WebNN] Failed to get D3D12 buffer from shared image.";

    void* mapped_ptr = nullptr;
    HRESULT hr = d3d12_buffer->Map(0, nullptr, &mapped_ptr);
    if (FAILED(hr)) {
      return base::unexpected(
          mojom::Error::New(mojom::Error::Code::kNotSupportedError,
                            "WebGPU interop is not supported."));
    }

    // CreateTensorWithDataAsOrtValue only allows CPU memory.
    ScopedOrtMemoryInfo memory_info;
    CHECK_STATUS(ort_api->CreateCpuMemoryInfo(
        OrtDeviceAllocator, OrtMemTypeCPU,
        ScopedOrtMemoryInfo::Receiver(memory_info).get()));

    CHECK_STATUS(ort_api->CreateTensorWithDataAsOrtValue(
        memory_info.get(), mapped_ptr, shared_image_byte_size, ort_shape.data(),
        ort_shape.size(), ort_data_type,
        ScopedOrtValue::Receiver(tensor).get()));
    CHECK(tensor.get());
    can_access_on_cpu = true;
  }

  if (!can_access_on_cpu &&
      (tensor_info->usage.Has(MLTensorUsageFlags::kRead) ||
       tensor_info->usage.Has(MLTensorUsageFlags::kWrite))) {
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kNotSupportedError,
        "CPU read/write access is not supported with exportable tensors."));
  }

  // ORT requires that we keep the OrtExternalMemoryHandle alive as long as the
  // tensor is alive, so we also need to pass the handle to TensorImplOrt.
  return base::MakeRefCounted<TensorImplOrt>(
      std::move(receiver), *this, std::move(tensor_info),
      std::move(representation), shared_image_byte_size,
      std::move(external_memory_handle), std::move(d3d12_buffer),
      std::move(tensor));
}

std::string_view ContextImplOrt::GetBackendName() const {
  return "ONNX Runtime";
}

std::vector<mojom::WebNNExecutionProviderDetailsPtr>
ContextImplOrt::GetExecutionProvidersInfo() const {
  return env_->GetSelectedEpDetails(WebnnToOrtDeviceType(options_->device));
}

}  // namespace webnn::ort
