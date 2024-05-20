// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/buffer_impl_dml.h"

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/dml/context_impl_dml.h"

namespace webnn::dml {

BufferImplDml::BufferImplDml(
    mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer,
    ContextImplDml* context,
    uint64_t size,
    const base::UnguessableToken& buffer_handle)
    : WebNNBufferImpl(std::move(receiver), context, size, buffer_handle),
      buffer_(std::move(buffer)) {}

BufferImplDml::~BufferImplDml() = default;

void BufferImplDml::ReadBufferImpl(ReadBufferCallback callback) {
  static_cast<ContextImplDml*>(context_.get())
      ->ReadBuffer(*this, std::move(callback));
}

void BufferImplDml::WriteBufferImpl(mojo_base::BigBuffer src_buffer) {
  static_cast<ContextImplDml*>(context_.get())
      ->WriteBuffer(*this, std::move(src_buffer));
}

}  // namespace webnn::dml
