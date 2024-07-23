// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/buffer_impl_tflite.h"

#include <climits>

#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom.h"
#include "services/webnn/tflite/buffer_content.h"
#include "services/webnn/tflite/buffer_state.h"
#include "services/webnn/tflite/buffer_task.h"
#include "services/webnn/webnn_utils.h"
#include "third_party/tflite/src/tensorflow/lite/util.h"

namespace webnn::tflite {

std::unique_ptr<WebNNBufferImpl> BufferImplTflite::Create(
    mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
    WebNNContextImpl* context,
    mojom::BufferInfoPtr buffer_info,
    const base::UnguessableToken& buffer_handle) {
  size_t size = buffer_info->descriptor.PackedByteLength();

  // Limit to INT_MAX for security reasons (similar to PartitionAlloc).
  if (!base::IsValueInRangeForNumericType<int>(size)) {
    LOG(ERROR) << "[WebNN] Buffer is too large to create.";
    return nullptr;
  }

  auto state = base::MakeRefCounted<BufferState>(size);
  if (!state) {
    return nullptr;
  }

  return std::make_unique<BufferImplTflite>(
      std::move(receiver), context, std::move(buffer_info), buffer_handle,
      std::move(state), base::PassKey<BufferImplTflite>());
}

BufferImplTflite::BufferImplTflite(
    mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
    WebNNContextImpl* context,
    mojom::BufferInfoPtr buffer_info,
    const base::UnguessableToken& buffer_handle,
    scoped_refptr<BufferState> state,
    base::PassKey<BufferImplTflite>)
    : WebNNBufferImpl(std::move(receiver),
                      context,
                      std::move(buffer_info),
                      buffer_handle),
      state_(std::move(state)) {}

BufferImplTflite::~BufferImplTflite() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

const scoped_refptr<BufferState>& BufferImplTflite::GetState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_;
}

void BufferImplTflite::ReadBufferImpl(ReadBufferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto task = base::MakeRefCounted<BufferTask>(
      /*shared_buffers=*/std::vector({state_}),
      /*exclusive_buffers=*/std::vector<scoped_refptr<BufferState>>(),
      base::BindOnce(
          [](scoped_refptr<BufferContent> content, ReadBufferCallback callback,
             base::OnceClosure completion_closure) {
            // Memory copies are fast, avoid the overhead of posting a task to
            // thread pool and do the work synchronously.
            std::move(callback).Run(mojom::ReadBufferResult::NewBuffer(
                mojo_base::BigBuffer(content->AsSpan())));
            std::move(completion_closure).Run();
          },
          state_->GetContent(), std::move(callback)));
  task->Enqueue();
}

void BufferImplTflite::WriteBufferImpl(mojo_base::BigBuffer src_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto task = base::MakeRefCounted<BufferTask>(
      /*shared_buffers=*/std::vector<scoped_refptr<BufferState>>(),
      /*exclusive_buffers=*/std::vector({state_}),
      base::BindOnce(
          [](scoped_refptr<BufferContent> content,
             mojo_base::BigBuffer src_buffer,
             base::OnceClosure completion_closure) {
            // Memory copies are fast, avoid the overhead of posting a task to
            // thread pool and do the work synchronously.
            content->AsSpan().first(src_buffer.size()).copy_from(src_buffer);
            std::move(completion_closure).Run();
          },
          state_->GetContent(), std::move(src_buffer)));
  task->Enqueue();
}

}  // namespace webnn::tflite
