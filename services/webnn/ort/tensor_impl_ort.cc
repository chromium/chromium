// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/tensor_impl_ort.h"

#include <cstring>

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/ort/context_impl_ort.h"
#include "services/webnn/ort/error_ort.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/ort/utils_ort.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/resource_task.h"

namespace webnn::ort {

// static
base::expected<std::unique_ptr<WebNNTensorImpl>, mojom::ErrorPtr>
TensorImplOrt::Create(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    WebNNContextImpl* context,
    mojom::TensorInfoPtr tensor_info) {
  const OrtApi* ort_api = GetOrtApi();
  OrtAllocator* allocator = nullptr;
  // Use the default allocator which is CPU based and non-arena.
  // `GetAllocatorWithDefaultOptions()` always returns the same pointer to the
  // same default allocator and its returned value should NOT be freed.
  //
  // TODO(https://github.com/shiyi9801/chromium/issues/65): Figure out how to
  // support allocator for other devices.
  if (ORT_CALL_FAILED(ort_api->GetAllocatorWithDefaultOptions(&allocator))) {
    return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                              "Failed to create tensor."));
  }
  CHECK(allocator);

  ONNXTensorElementDataType ort_data_type =
      OperandTypeToONNXTensorElementDataType(
          tensor_info->descriptor.data_type());
  // Convert the shape from uint32_t to int64_t.
  std::vector<int64_t> ort_shape(tensor_info->descriptor.shape().begin(),
                                 tensor_info->descriptor.shape().end());
  ScopedOrtValue tensor;
  if (ORT_CALL_FAILED(ort_api->CreateTensorAsOrtValue(
          allocator, ort_shape.data(), ort_shape.size(), ort_data_type,
          ScopedOrtValue::Receiver(tensor).get()))) {
    return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                              "Failed to create tensor."));
  }
  CHECK(tensor.get());

  // Initialize the tensor with zeros, otherwise, reading uninitialized memory
  // will get random values.
  void* ort_tensor_raw_data = nullptr;
  CHECK_STATUS(
      GetOrtApi()->GetTensorMutableData(tensor.get(), &ort_tensor_raw_data));
  CHECK(ort_tensor_raw_data);
  size_t tensor_size = tensor_info->descriptor.PackedByteLength();
  std::memset(ort_tensor_raw_data, 0, tensor_size);

  auto buffer_content = std::make_unique<BufferContentOrt>(std::move(tensor));
  auto buffer_state =
      base::MakeRefCounted<QueueableResourceState<BufferContentOrt>>(
          std::move(buffer_content));
  return std::make_unique<TensorImplOrt>(std::move(receiver), context,
                                         std::move(tensor_info),
                                         std::move(buffer_state));
}

TensorImplOrt::TensorImplOrt(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    WebNNContextImpl* context,
    mojom::TensorInfoPtr tensor_info,
    scoped_refptr<QueueableResourceState<BufferContentOrt>> buffer_state)
    : WebNNTensorImpl(std::move(receiver), context, std::move(tensor_info)),
      buffer_state_(std::move(buffer_state)) {}

TensorImplOrt::~TensorImplOrt() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

const scoped_refptr<QueueableResourceState<BufferContentOrt>>&
TensorImplOrt::GetBufferState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return buffer_state_;
}

void TensorImplOrt::ReadTensorImpl(ReadTensorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Lock the buffer contents as shared/read-only.
  std::vector<scoped_refptr<QueueableResourceStateBase>> shared_resources = {
      buffer_state_};

  auto task = base::MakeRefCounted<ResourceTask>(
      /*shared_resources=*/
      std::move(shared_resources),
      /*exclusive_resources=*/
      std::vector<scoped_refptr<QueueableResourceStateBase>>(),
      base::BindOnce(
          [](size_t bytes_to_read,
             scoped_refptr<QueueableResourceState<BufferContentOrt>>
                 buffer_state,
             ReadTensorCallback read_tensor_result_callback,
             base::OnceClosure completion_closure) {
            void* ort_tensor_raw_data = nullptr;
            CHECK_STATUS(GetOrtApi()->GetTensorMutableData(
                buffer_state->GetSharedLockedResource().tensor(),
                &ort_tensor_raw_data));
            CHECK(ort_tensor_raw_data);
            // SAFETY: ORT guarantees that it has allocated enough memory to
            // store tensor.
            mojo_base::BigBuffer output_buffer(UNSAFE_BUFFERS(
                base::span(static_cast<const uint8_t*>(ort_tensor_raw_data),
                           bytes_to_read)));

            // Unlock the buffer contents.
            std::move(completion_closure).Run();
            std::move(read_tensor_result_callback)
                .Run(mojom::ReadTensorResult::NewBuffer(
                    std::move(output_buffer)));
          },
          /*bytes_to_read=*/PackedByteLength(), buffer_state_,
          std::move(callback)));
  task->Enqueue();
}

void TensorImplOrt::WriteTensorImpl(mojo_base::BigBuffer src_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Take an exclusive lock to the buffer contents while writing.
  std::vector<scoped_refptr<QueueableResourceStateBase>> exclusive_resources = {
      buffer_state_};

  auto task = base::MakeRefCounted<ResourceTask>(
      /*shared_resources=*/
      std::vector<scoped_refptr<QueueableResourceStateBase>>(),
      /*exclusive_resources=*/
      std::move(exclusive_resources),
      base::BindOnce(
          [](scoped_refptr<QueueableResourceState<BufferContentOrt>>
                 buffer_state,
             mojo_base::BigBuffer src_buffer,
             base::OnceClosure completion_closure) {
            void* ort_tensor_raw_data = nullptr;
            CHECK_STATUS(GetOrtApi()->GetTensorMutableData(
                buffer_state->GetExclusivelyLockedResource()->tensor(),
                &ort_tensor_raw_data));
            CHECK(ort_tensor_raw_data);
            UNSAFE_BUFFERS(
                base::span(static_cast<uint8_t*>(ort_tensor_raw_data),
                           src_buffer.size()))
                .copy_from(src_buffer);

            // Unlock the buffer contents.
            std::move(completion_closure).Run();
          },
          buffer_state_, std::move(src_buffer)));
  task->Enqueue();
}

}  // namespace webnn::ort
