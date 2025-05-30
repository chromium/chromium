// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/tensor_impl_ort.h"

#include "base/check_op.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/resource_task.h"

namespace webnn::ort {

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

  ScopedTrace scoped_trace("TensorImplOrt::ReadTensorImpl");

  // Lock the buffer contents as shared/read-only.
  std::vector<scoped_refptr<QueueableResourceStateBase>> shared_resources = {
      buffer_state_};

  scoped_trace.AddStep("Wait for tensor");
  auto task = base::MakeRefCounted<ResourceTask>(
      /*shared_resources=*/
      std::move(shared_resources),
      /*exclusive_resources=*/
      std::vector<scoped_refptr<QueueableResourceStateBase>>(),
      base::BindOnce(
          [](size_t bytes_to_read,
             scoped_refptr<QueueableResourceState<BufferContentOrt>>
                 buffer_state,
             ReadTensorCallback callback, ScopedTrace scoped_trace,
             base::OnceClosure completion_closure) {
            scoped_trace.AddStep("Begin read");
            // Memory copies are fast, avoid the overhead of posting a task
            // to the thread pool and do the work synchronously.
            base::span<const uint8_t> buffer_span =
                buffer_state->GetSharedLockedResource().AsSpan();
            CHECK_EQ(bytes_to_read, buffer_span.size());
            std::move(callback).Run(mojom::ReadTensorResult::NewBuffer(
                mojo_base::BigBuffer(buffer_span)));

            scoped_trace.AddStep("End read");
            // Unlock the buffer contents.
            std::move(completion_closure).Run();
          },
          /*bytes_to_read=*/PackedByteLength(), buffer_state_,
          std::move(callback), std::move(scoped_trace)));
  task->Enqueue();
}

void TensorImplOrt::WriteTensorImpl(mojo_base::BigBuffer src_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ScopedTrace scoped_trace("TensorImplOrt::WriteTensorImpl");

  // Take an exclusive lock to the buffer contents while writing.
  std::vector<scoped_refptr<QueueableResourceStateBase>> exclusive_resources = {
      buffer_state_};

  scoped_trace.AddStep("Wait for tensor");
  auto task = base::MakeRefCounted<ResourceTask>(
      /*shared_resources=*/
      std::vector<scoped_refptr<QueueableResourceStateBase>>(),
      /*exclusive_resources=*/
      std::move(exclusive_resources),
      base::BindOnce(
          [](scoped_refptr<QueueableResourceState<BufferContentOrt>>
                 buffer_state,
             mojo_base::BigBuffer src_buffer, ScopedTrace scoped_trace,
             base::OnceClosure completion_closure) {
            scoped_trace.AddStep("Begin write");
            // Memory copies are fast, avoid the overhead of posting a task to
            // the thread pool and do the work synchronously.
            base::span<uint8_t> buffer_span =
                buffer_state->GetExclusivelyLockedResource()->AsSpan();
            CHECK_EQ(src_buffer.size(), buffer_span.size());
            buffer_span.copy_from(src_buffer);

            scoped_trace.AddStep("End write");
            // Unlock the buffer contents.
            std::move(completion_closure).Run();
          },
          buffer_state_, std::move(src_buffer), std::move(scoped_trace)));
  task->Enqueue();
}

}  // namespace webnn::ort
