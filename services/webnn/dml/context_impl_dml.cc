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
#include "services/webnn/dml/buffer_impl_dml.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/graph_impl_dml.h"
#include "services/webnn/dml/utils.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom.h"
#include "services/webnn/webnn_context_impl.h"

namespace webnn::dml {

using Microsoft::WRL::ComPtr;

namespace {

void HandleBufferCreationFailure(
    const std::string& error_message,
    WebNNContextImpl::CreateBufferImplCallback callback) {
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

  static constexpr SupportedDataTypes kFloat16To32Ints32To64{
      OperandDataType::kFloat16, OperandDataType::kFloat32,
      OperandDataType::kInt32,   OperandDataType::kUint32,
      OperandDataType::kInt64,   OperandDataType::kUint64};

  static constexpr SupportedDataTypes kUint8To32{OperandDataType::kUint8,
                                                 OperandDataType::kUint32};

  static constexpr SupportedDataTypes kGatherIndicesSupportedDataTypes{
      OperandDataType::kInt32, OperandDataType::kUint32,
      OperandDataType::kInt64, OperandDataType::kUint64};

  // TODO: crbug.com/345271830 - specify data types for all parameters.
  ContextProperties properties(
      /*input_operand_layout=*/InputOperandLayout::kNchw,
      {/*input=*/SupportedDataTypes::All(),
       /*constant=*/SupportedDataTypes::All(),

       /*arg_min_max_input=*/SupportedDataTypes::All(),
       /*arg_min_max_output=*/DataTypeConstraint::kInt32To64,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_join_operator_desc#tensor-support
       /*concat_inputs=*/kFloat16To32Ints8To32,

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

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_sin_operator_desc#tensor-support
       /*sin_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_sqrt_operator_desc#tensor-support
       /*sqrt_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_tan_operator_desc#tensor-support
       /*tan_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_activation_elu_operator_desc
       /*elu_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_gather_operator_desc#tensor-support
       /*gather_input=*/kFloat16To32Ints8To32,
       /*gather_indices=*/kGatherIndicesSupportedDataTypes,

       // Gelu is emulated when the feature level is less than 5.1.
       // https://learn.microsoft.com/en-us/windows/ai/directml/api/ns-directml-dml_activation_gelu_operator_desc
       /*gelu_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_activation_leaky_relu_operator_desc
       /*leaky_relu_input=*/DataTypeConstraint::kFloat16To32,

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_activation_relu_operator_desc
       /*relu_input=*/DataTypeConstraint::kFloat16To32,

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

       // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_if_operator_desc
       /*where_condition=*/DataTypeConstraint::kUint8,
       /*where_value=*/kFloat16To32Ints8To32});

  if (feature_level >= DML_FEATURE_LEVEL_4_1) {
    properties.data_type_limits.concat_inputs = SupportedDataTypes::All();
    properties.data_type_limits.add_input = kFloat16To32Ints32To64;
    properties.data_type_limits.sub_input = kFloat16To32Ints32To64;
    properties.data_type_limits.mul_input = kFloat16To32Ints32To64;
    properties.data_type_limits.equal_input = SupportedDataTypes::All();
    properties.data_type_limits.greater_input = SupportedDataTypes::All();
    properties.data_type_limits.greater_or_equal_input =
        SupportedDataTypes::All();
    properties.data_type_limits.lesser_input = SupportedDataTypes::All();
    properties.data_type_limits.lesser_or_equal_input =
        SupportedDataTypes::All();
    properties.data_type_limits.abs_input = kFloat16To32Int8To64;
    properties.data_type_limits.identity_input = SupportedDataTypes::All();
    properties.data_type_limits.gather_input = SupportedDataTypes::All();
    properties.data_type_limits.slice_input = SupportedDataTypes::All();
    properties.data_type_limits.split_input = SupportedDataTypes::All();
  }

  if (feature_level >= DML_FEATURE_LEVEL_5_0) {
    properties.data_type_limits.max_input = SupportedDataTypes::All();
    properties.data_type_limits.min_input = SupportedDataTypes::All();
    properties.data_type_limits.where_value = SupportedDataTypes::All();
  }

  if (feature_level >= DML_FEATURE_LEVEL_5_1) {
    properties.data_type_limits.add_input = SupportedDataTypes::All();
    properties.data_type_limits.sub_input = SupportedDataTypes::All();
    properties.data_type_limits.mul_input = SupportedDataTypes::All();
    properties.data_type_limits.div_input = kFloat16To32Ints8To32;
    properties.data_type_limits.relu_input =
        DataTypeConstraint::kFloat16To32Int8To32;
  }

  if (feature_level >= DML_FEATURE_LEVEL_6_0) {
    properties.data_type_limits.div_input = SupportedDataTypes::All();
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
    WebNNContextImpl::CreateGraphImplCallback callback) {
  GraphImplDml::CreateAndBuild(
      adapter_, weak_factory_.GetWeakPtr(), std::move(graph_info),
      std::move(compute_resource_info), std::move(callback),
      gpu_feature_info_->IsWorkaroundEnabled(
          gpu::DML_EXECUTION_DISABLE_META_COMMANDS));
}

void ContextImplDml::CreateBufferImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
    mojom::BufferInfoPtr buffer_info,
    CreateBufferImplCallback callback) {
  // DML requires resources to be in multiple of 4 bytes.
  // https://learn.microsoft.com/en-us/windows/ai/directml/dml-helper-functions#dmlcalcbuffertensorsize
  constexpr uint64_t kDMLBufferAlignment = 4ull;
  if (std::numeric_limits<uint64_t>::max() - kDMLBufferAlignment <
      static_cast<uint64_t>(buffer_info->descriptor.PackedByteLength())) {
    LOG(ERROR) << "[WebNN] Buffer is too large to create.";
    HandleBufferCreationFailure("Failed to create buffer.",
                                std::move(callback));
    return;
  }

  const uint64_t aligned_buffer_byte_size = base::bits::AlignUp(
      static_cast<uint64_t>(buffer_info->descriptor.PackedByteLength()),
      kDMLBufferAlignment);

  HRESULT hr = S_OK;

  // If adapter supports UMA, create the custom heap with CPU memory pool. The
  // CPU will directly read/write to this heap if the GPU isn't using it.
  ComPtr<ID3D12Resource> buffer;
  if (adapter_->IsUMA()) {
    // TODO(crbug.com/40278771): consider introducing buffer usages for INPUT or
    // OUTPUT since using upload-equivelent custom heaps everywhere could be
    // inefficient.
    hr = CreateCustomUploadBuffer(
        adapter_->d3d12_device(), aligned_buffer_byte_size,
        L"WebNN_Custom_Upload_Buffer_External", buffer);
  } else {
    // Create a default buffer that can be accessed only by GPU.
    // The CPU must use a staging buffer to read/write to this buffer.
    hr = CreateDefaultBuffer(adapter_->d3d12_device(), aligned_buffer_byte_size,
                             L"WebNN_Default_Buffer_External", buffer);
  }

  if (FAILED(hr)) {
    HandleBufferCreationFailure("Failed to create buffer.",
                                std::move(callback));
    HandleContextLostOrCrash("Failed to create the external buffer.", hr);
    return;
  }

  // The receiver bound to WebNNBufferImpl.
  //
  // Safe to use ContextImplDml* because this context owns the buffer
  // being connected and that context cannot destruct before the buffer.
  std::move(callback).Run(std::make_unique<BufferImplDml>(
      std::move(receiver), std::move(buffer), this, std::move(buffer_info)));
}

void ContextImplDml::ReadBuffer(
    BufferImplDml* src_buffer,
    mojom::WebNNBuffer::ReadBufferCallback callback) {
  const size_t src_buffer_size = src_buffer->PackedByteLength();

  HRESULT hr = S_OK;

  // Map entire buffer to readback the output data.
  if (adapter_->IsUMA() && adapter_->command_queue()->GetCompletedValue() >=
                               src_buffer->last_submission_fence_value()) {
    ContextImplDml::OnReadbackComplete(src_buffer->buffer(), src_buffer_size,
                                       std::move(callback), hr);
    return;
  }

  // Copy the buffer into a staging buffer to readback the output data.
  ComPtr<ID3D12Resource> download_buffer;
  hr = CreateReadbackBuffer(adapter_->d3d12_device(),
                            static_cast<uint64_t>(src_buffer_size),
                            L"WebNN_Readback_Buffer", download_buffer);
  if (FAILED(hr)) {
    std::move(callback).Run(ToError<mojom::ReadBufferResult>(
        mojom::Error::Code::kUnknownError, "Failed to read buffer."));
    HandleContextLostOrCrash("Failed to create the download buffer.", hr);
    return;
  }

  hr = StartRecordingIfNecessary();
  if (FAILED(hr)) {
    std::move(callback).Run(ToError<mojom::ReadBufferResult>(
        mojom::Error::Code::kUnknownError, "Failed to read buffer."));
    HandleRecordingError("Failed to start recording.", hr);
    return;
  }

  command_recorder_->ReadbackBufferWithBarrier(download_buffer, src_buffer,
                                               src_buffer_size);

  // Submit copy and schedule GPU wait.
  hr = command_recorder_->CloseAndExecute();
  if (FAILED(hr)) {
    std::move(callback).Run(ToError<mojom::ReadBufferResult>(
        mojom::Error::Code::kUnknownError, "Failed to read buffer."));
    HandleRecordingError("Failed to close and execute the command list.", hr);
    return;
  }

  // The source and readback buffer is held alive during execution by the
  // recorder by calling `ReadbackBufferWithBarrier()` then
  // CommandRecorder::CloseAndExecute().
  adapter_->command_queue()->WaitAsync(base::BindOnce(
      &ContextImplDml::OnReadbackComplete, weak_factory_.GetWeakPtr(),
      std::move(download_buffer), src_buffer_size, std::move(callback)));
}

void ContextImplDml::OnReadbackComplete(
    ComPtr<ID3D12Resource> download_buffer,
    size_t read_byte_size,
    mojom::WebNNBuffer::ReadBufferCallback callback,
    HRESULT hr) {
  if (FAILED(hr)) {
    std::move(callback).Run(ToError<mojom::ReadBufferResult>(
        mojom::Error::Code::kUnknownError, "Failed to read buffer."));
    HandleRecordingError("Failed to download the buffer.", hr);
    return;
  }

  CHECK(download_buffer);

  // Copy over data from the download buffer to the destination buffer.
  void* mapped_download_data = nullptr;
  hr = download_buffer->Map(0, nullptr, &mapped_download_data);
  if (FAILED(hr)) {
    std::move(callback).Run(ToError<mojom::ReadBufferResult>(
        mojom::Error::Code::kUnknownError, "Failed to read buffer."));
    HandleContextLostOrCrash("Failed to map the download buffer.", hr);
    return;
  }

  mojo_base::BigBuffer dst_buffer(base::make_span(
      static_cast<const uint8_t*>(mapped_download_data), read_byte_size));

  download_buffer->Unmap(0, nullptr);

  std::move(callback).Run(
      mojom::ReadBufferResult::NewBuffer(std::move(dst_buffer)));
}

void ContextImplDml::WriteBuffer(BufferImplDml* dst_buffer,
                                 mojo_base::BigBuffer src_buffer) {
  HRESULT hr = S_OK;
  ComPtr<ID3D12Resource> buffer_to_map = dst_buffer->buffer();

  // Create a staging buffer to upload data into when the existing buffer
  // cannot be updated by the CPU.
  if (!adapter_->IsUMA() || adapter_->command_queue()->GetCompletedValue() <
                                dst_buffer->last_submission_fence_value()) {
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
  if (dst_buffer->buffer() != buffer_to_map.Get()) {
    hr = StartRecordingIfNecessary();
    if (FAILED(hr)) {
      HandleRecordingError("Failed to start recording.", hr);
      return;
    }

    command_recorder_->UploadBufferWithBarrier(
        dst_buffer, std::move(buffer_to_map), src_buffer.size());

    // TODO(crbug.com/40278771): consider not submitting after every write.
    // CloseAndExecute() only needs to be called once, when the buffer is read
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
  }

  std::string_view message_for_promise;
  switch (hr) {
    case E_OUTOFMEMORY:
      message_for_promise = "out of memory.";
      break;
    case DXGI_ERROR_DEVICE_REMOVED:
      message_for_promise = "device removed.";
      break;
    case DXGI_ERROR_DEVICE_RESET:
      message_for_promise = "device reset.";
      break;
    default:
      message_for_promise = "internal error.";
  }

  OnLost(base::StrCat({"WebNN context is lost due to ", message_for_promise}));
  CHECK(hr == E_OUTOFMEMORY || hr == DXGI_ERROR_DEVICE_REMOVED ||
        hr == DXGI_ERROR_DEVICE_RESET);
}

}  // namespace webnn::dml
