// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/tensor_impl_ort.h"

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/ort/context_impl_ort.h"
#include "services/webnn/ort/error_ort.h"
#include "services/webnn/ort/utils_ort.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/resource_task.h"

namespace webnn::ort {

TensorImplOrt::TensorImplOrt(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    ContextImplOrt* context,
    mojom::TensorInfoPtr tensor_info)
    : WebNNTensorImpl(std::move(receiver), context, std::move(tensor_info)) {
  // Convert the shape from uint32_t to int64_t.
  std::vector<int64_t> ort_shape(shape().begin(), shape().end());
  ONNXTensorElementDataType ort_data_type =
      OperandTypeToONNXTensorElementDataType(data_type());

  auto buffer_content = std::make_unique<BufferContentOrt>(
      context->allocator()->allocator(), std::move(ort_shape), ort_data_type);
  buffer_state_ =
      base::MakeRefCounted<QueueableResourceState<BufferContentOrt>>(
          std::move(buffer_content));
}

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
            mojo_base::BigBuffer output_buffer(
                base::span(static_cast<const uint8_t*>(ort_tensor_raw_data),
                           bytes_to_read));

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
