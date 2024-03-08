// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_buffer_impl.h"

#include "services/webnn/webnn_context_impl.h"

namespace webnn {

WebNNBufferImpl::WebNNBufferImpl(
    mojo::PendingReceiver<mojom::WebNNBuffer> receiver,
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

void WebNNBufferImpl::OnDisconnect() {
  context_->DisconnectAndDestroyWebNNBufferImpl(handle());
}

}  // namespace webnn
