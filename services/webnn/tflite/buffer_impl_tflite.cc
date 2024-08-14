// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/buffer_impl_tflite.h"

#include <climits>

#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom.h"
#include "services/webnn/queueable_resource_state.h"
#include "services/webnn/queueable_resource_state_base.h"
#include "services/webnn/resource_task.h"
#include "services/webnn/tflite/buffer_content.h"
#include "services/webnn/webnn_utils.h"
#include "third_party/tflite/src/tensorflow/lite/util.h"

namespace webnn::tflite {

// static
base::expected<std::unique_ptr<WebNNBufferImpl>, mojom::ErrorPtr>
BufferImplTflite::Create(
    mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
    WebNNContextImpl* context,
    mojom::BufferInfoPtr buffer_info) {
  size_t size = buffer_info->descriptor.PackedByteLength();

  // Limit to INT_MAX for security reasons (similar to PartitionAlloc).
  //
  // TODO(crbug.com/356670455): Consider moving this check to the renderer and
  // throwing a TypeError.
  if (!base::IsValueInRangeForNumericType<int>(size)) {
    LOG(ERROR) << "[WebNN] Buffer is too large to create.";
    return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                              "Failed to create buffer."));
  }

  auto buffer_content = std::make_unique<BufferContent>(size);
  auto buffer_state =
      base::MakeRefCounted<QueueableResourceState<BufferContent>>(
          std::move(buffer_content));
  return std::make_unique<BufferImplTflite>(
      std::move(receiver), context, std::move(buffer_info),
      std::move(buffer_state), base::PassKey<BufferImplTflite>());
}

BufferImplTflite::BufferImplTflite(
    mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
    WebNNContextImpl* context,
    mojom::BufferInfoPtr buffer_info,
    scoped_refptr<QueueableResourceState<BufferContent>> buffer_state,
    base::PassKey<BufferImplTflite>)
    : WebNNBufferImpl(std::move(receiver), context, std::move(buffer_info)),
      buffer_state_(std::move(buffer_state)) {}

BufferImplTflite::~BufferImplTflite() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

const scoped_refptr<QueueableResourceState<BufferContent>>&
BufferImplTflite::GetBufferState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return buffer_state_;
}

void BufferImplTflite::ReadBufferImpl(ReadBufferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Lock the buffer contents as shared/read-only.
  std::vector<scoped_refptr<QueueableResourceStateBase>> shared_resources = {
      buffer_state_};

  auto task = base::MakeRefCounted<ResourceTask>(
      std::move(shared_resources),
      /*exclusive_resources=*/
      std::vector<scoped_refptr<QueueableResourceStateBase>>(),
      base::BindOnce(
          [](scoped_refptr<QueueableResourceState<BufferContent>>
                 content_handle,
             ReadBufferCallback callback,
             base::OnceClosure completion_closure) {
            // Memory copies are fast, avoid the overhead of posting a task
            // to the thread pool and do the work synchronously.
            std::move(callback).Run(
                mojom::ReadBufferResult::NewBuffer(mojo_base::BigBuffer(
                    content_handle->GetSharedLockedResource().AsSpan())));
            std::move(completion_closure).Run();
          },
          buffer_state_, std::move(callback)));
  task->Enqueue();
}

void BufferImplTflite::WriteBufferImpl(mojo_base::BigBuffer src_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Take an exclusive lock to the buffer contents while reading.
  std::vector<scoped_refptr<QueueableResourceStateBase>> exclusive_resources = {
      buffer_state_};

  auto task = base::MakeRefCounted<ResourceTask>(
      /*shared_resources=*/std::vector<
          scoped_refptr<QueueableResourceStateBase>>(),
      /*exclusive_resources=*/std::move(exclusive_resources),
      base::BindOnce(
          [](scoped_refptr<QueueableResourceState<BufferContent>>
                 content_handle,
             mojo_base::BigBuffer src_buffer,
             base::OnceClosure completion_closure) {
            // Memory copies are fast, avoid the overhead of posting a task to
            // the thread pool and do the work synchronously.
            content_handle->GetExclusivelyLockedResource()
                ->AsSpan()
                .copy_prefix_from(src_buffer);
            std::move(completion_closure).Run();
          },
          buffer_state_, std::move(src_buffer)));
  task->Enqueue();
}

}  // namespace webnn::tflite
