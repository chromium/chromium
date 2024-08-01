// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_buffer_impl.h"

#include "services/webnn/error.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom.h"
#include "services/webnn/webnn_context_impl.h"

namespace webnn {

WebNNBufferImpl::WebNNBufferImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
    WebNNContextImpl* context,
    mojom::BufferInfoPtr buffer_info)
    : context_(context),
      // TODO(crbug.com/343638938): Use buffer_info->usage.
      descriptor_(std::move(buffer_info->descriptor)),
      receiver_(this, std::move(receiver)) {
  // Safe to use base::Unretained because `this` owns `receiver_`.
  receiver_.set_disconnect_handler(
      base::BindOnce(&WebNNBufferImpl::OnDisconnect, base::Unretained(this)));
}

WebNNBufferImpl::~WebNNBufferImpl() = default;

void WebNNBufferImpl::ReadBuffer(ReadBufferCallback callback) {
  // Call ReadBufferImpl() implemented by a backend.
  ReadBufferImpl(std::move(callback));
}

void WebNNBufferImpl::WriteBuffer(mojo_base::BigBuffer src_buffer) {
  // TODO(https://crbug.com/40278771): Generate error using MLContext.
  if (PackedByteLength() < src_buffer.size()) {
    receiver_.ReportBadMessage(kBadMessageInvalidBuffer);
    return;
  }

  // Call WriteBufferImpl() implemented by a backend.
  WriteBufferImpl(std::move(src_buffer));
}

void WebNNBufferImpl::OnDisconnect() {
  context_->DisconnectAndDestroyWebNNBufferImpl(handle());
}

}  // namespace webnn
