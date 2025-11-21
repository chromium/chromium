// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/tensor_impl_tflite.h"

#include <climits>

#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/notimplemented.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/queueable_resource_state.h"
#include "services/webnn/queueable_resource_state_base.h"
#include "services/webnn/resource_task.h"
#include "services/webnn/tflite/buffer_content_tflite.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_utils.h"
#include "third_party/tflite/src/tensorflow/lite/util.h"

namespace webnn::tflite {

// static
base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
TensorImplTflite::Create(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    base::WeakPtr<WebNNContextImpl> context,
    mojom::TensorInfoPtr tensor_info) {
  size_t size = tensor_info->descriptor.PackedByteLength();
  // Invalid values are rejected in GraphBuilder.
  CHECK(base::IsValueInRangeForNumericType<int>(size));

  auto buffer_content = std::make_unique<BufferContent>(size);
  auto buffer_state =
      base::MakeRefCounted<QueueableResourceState<BufferContent>>(
          std::move(buffer_content));
  return base::MakeRefCounted<TensorImplTflite>(
      std::move(receiver), std::move(context), std::move(tensor_info),
      std::move(buffer_state), base::PassKey<TensorImplTflite>());
}

TensorImplTflite::TensorImplTflite(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    base::WeakPtr<WebNNContextImpl> context,
    mojom::TensorInfoPtr tensor_info,
    scoped_refptr<QueueableResourceState<BufferContent>> buffer_state,
    base::PassKey<TensorImplTflite>)
    : WebNNTensorImpl(std::move(receiver),
                      std::move(context),
                      std::move(tensor_info)),
      buffer_state_(std::move(buffer_state)) {}

TensorImplTflite::~TensorImplTflite() = default;

const scoped_refptr<QueueableResourceState<BufferContent>>&
TensorImplTflite::GetBufferState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  return buffer_state_;
}

void TensorImplTflite::ReadTensorImpl(ReadTensorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  ScopedTrace scoped_trace("TensorImplTflite::ReadTensorImpl");

  // Lock the buffer contents as shared/read-only.
  std::vector<scoped_refptr<QueueableResourceStateBase>> shared_resources = {
      buffer_state_};

  scoped_trace.AddStep("Wait for tensor");
  auto task = base::MakeRefCounted<ResourceTask>(
      std::move(shared_resources),
      /*exclusive_resources=*/
      std::vector<scoped_refptr<QueueableResourceStateBase>>(),
      base::BindOnce(
          [](base::WeakPtr<WebNNContextImpl> context,
             scoped_refptr<QueueableResourceState<BufferContent>>
                 content_handle,
             ReadTensorCallback callback, ScopedTrace scoped_trace,
             base::OnceClosure completion_closure) {
            if (context) {
              scoped_trace.AddStep("Begin read");
              // Memory copies are fast, avoid the overhead of posting a task
              // to the thread pool and do the work synchronously.
              std::move(callback).Run(mojom::ReadTensorResult::NewBuffer(
                  context->WriteDataToDataPipeOrBigBuffer(
                      content_handle->GetSharedLockedResource().AsSpan())));

              scoped_trace.AddStep("End read");
            }
            std::move(completion_closure).Run();
          },
          context_->AsWeakPtr(), buffer_state_, std::move(callback),
          std::move(scoped_trace)));
  task->Enqueue();
}

void TensorImplTflite::WriteTensorImpl(mojo_base::BigBuffer src_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  ScopedTrace scoped_trace("TensorImplTflite::WriteTensorImpl");

  // Take an exclusive lock to the buffer contents while reading.
  std::vector<scoped_refptr<QueueableResourceStateBase>> exclusive_resources = {
      buffer_state_};

  scoped_trace.AddStep("Wait for tensor");
  auto task = base::MakeRefCounted<ResourceTask>(
      /*shared_resources=*/std::vector<
          scoped_refptr<QueueableResourceStateBase>>(),
      /*exclusive_resources=*/std::move(exclusive_resources),
      base::BindOnce(
          [](base::WeakPtr<WebNNContextImpl> context,
             scoped_refptr<QueueableResourceState<BufferContent>>
                 content_handle,
             mojo_base::BigBuffer src_buffer, ScopedTrace scoped_trace,
             base::OnceClosure completion_closure) {
            if (context) {
              scoped_trace.AddStep("Begin write");
              // Memory copies are fast, avoid the overhead of posting a task to
              // the thread pool and do the work synchronously.
              context->ReadDataFromBigBufferOrDataPipe(
                  std::move(src_buffer),
                  content_handle->GetExclusivelyLockedResource()->AsSpan());

              scoped_trace.AddStep("End write");
            }
            std::move(completion_closure).Run();
          },
          context_->AsWeakPtr(), buffer_state_, std::move(src_buffer),
          std::move(scoped_trace)));
  task->Enqueue();
}

bool TensorImplTflite::ImportTensorImpl() {
  NOTIMPLEMENTED();
  return false;
}

void TensorImplTflite::ExportTensorImpl(ScopedAccessPtr access,
                                        ExportTensorCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace webnn::tflite
