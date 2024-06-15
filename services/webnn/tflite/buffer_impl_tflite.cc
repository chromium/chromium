// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/buffer_impl_tflite.h"

#include <climits>

#include "base/memory/ptr_util.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom.h"
#include "services/webnn/webnn_utils.h"

namespace webnn::tflite {

std::unique_ptr<WebNNBufferImpl> BufferImplTflite::Create(
    mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
    WebNNContextImpl* context,
    mojom::BufferInfoPtr buffer_info,
    const base::UnguessableToken& buffer_handle) {
  // Limit to INT_MAX for security reasons (similar to PartitionAlloc).
  if (!base::IsValueInRangeForNumericType<int>(
          buffer_info->descriptor.PackedByteLength())) {
    DLOG(ERROR) << "Buffer is too large to create.";
    return nullptr;
  }

  return base::WrapUnique(new BufferImplTflite(
      std::move(receiver), context, std::move(buffer_info), buffer_handle));
}

BufferImplTflite::BufferImplTflite(
    mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
    WebNNContextImpl* context,
    mojom::BufferInfoPtr buffer_info,
    const base::UnguessableToken& buffer_handle)
    : WebNNBufferImpl(std::move(receiver),
                      context,
                      std::move(buffer_info),
                      buffer_handle) {
  buffer_ = base::HeapArray<uint8_t>::WithSize(PackedByteLength());
}

BufferImplTflite::~BufferImplTflite() = default;

void BufferImplTflite::ReadBufferImpl(ReadBufferCallback callback) {
  std::move(callback).Run(
      mojom::ReadBufferResult::NewBuffer(mojo_base::BigBuffer(buffer_)));
}

void BufferImplTflite::WriteBufferImpl(mojo_base::BigBuffer src_buffer) {
  buffer_.first(src_buffer.size()).copy_from(base::span(src_buffer));
}

}  // namespace webnn::tflite
