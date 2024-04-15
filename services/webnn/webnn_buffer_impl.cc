// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_buffer_impl.h"

#include "services/webnn/error.h"
#include "services/webnn/webnn_context_impl.h"

namespace webnn {

WebNNBufferImpl::WebNNBufferImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
    WebNNContextImpl* context,
    uint64_t size,
    const base::UnguessableToken& buffer_handle)
    : WebNNObjectImpl(buffer_handle),
      size_(size),
      receiver_(this, std::move(receiver)),
      context_(context) {
  // Safe to use base::Unretained because `this` owns `receiver_`.
  receiver_.set_disconnect_handler(
      base::BindOnce(&WebNNBufferImpl::OnDisconnect, base::Unretained(this)));
}

WebNNBufferImpl::~WebNNBufferImpl() = default;

void WebNNBufferImpl::ReadBuffer(ReadBufferCallback callback) {
  context_->ReadBuffer(*this, std::move(callback));
}

void WebNNBufferImpl::WriteBuffer(mojo_base::BigBuffer src_buffer) {
  context_->WriteBuffer(*this, std::move(src_buffer));
}

void WebNNBufferImpl::OnDisconnect() {
  context_->DisconnectAndDestroyWebNNBufferImpl(handle());
}

}  // namespace webnn
