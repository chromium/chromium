// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_impl.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/error.h"
#include "services/webnn/webnn_buffer_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn {

WebNNContextImpl::WebNNContextImpl(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    WebNNContextProviderImpl* context_provider)
    : receiver_(this, std::move(receiver)),
      context_provider_(context_provider) {
  CHECK(context_provider_);
  // Safe to use base::Unretained because the context_provider_ owns this class
  // that won't be destroyed until this callback executes.
  receiver_.set_disconnect_handler(base::BindOnce(
      &WebNNContextImpl::OnConnectionError, base::Unretained(this)));
}

WebNNContextImpl::~WebNNContextImpl() = default;

void WebNNContextImpl::OnConnectionError() {
  context_provider_->OnConnectionError(this);
}

void WebNNContextImpl::CreateGraph(
    mojom::GraphInfoPtr graph_info,
    mojom::WebNNContext::CreateGraphCallback callback) {
  if (!WebNNGraphImpl::ValidateGraph(graph_info)) {
    receiver_.ReportBadMessage(kBadMessageInvalidGraph);
    return;
  }
  // Call CreateGraphImpl() implemented by a backend.
  CreateGraphImpl(std::move(graph_info), std::move(callback));
}

void WebNNContextImpl::CreateBuffer(
    mojo::PendingReceiver<mojom::WebNNBuffer> receiver,
    mojom::BufferInfoPtr buffer_info,
    const base::UnguessableToken& buffer_handle) {
  // It is illegal to create the same buffer twice, a buffer is uniquely
  // identified by its UnguessableToken.
  if (IsWebNNBufferValid(buffer_handle)) {
    receiver_.ReportBadMessage(kBadMessageInvalidBuffer);
    return;
  }

  // TODO(crbug.com/1472888): handle error using MLContext.
  std::unique_ptr<WebNNBufferImpl> buffer_impl = CreateBufferImpl(
      std::move(receiver), std::move(buffer_info), buffer_handle);
  if (!buffer_impl) {
    receiver_.ReportBadMessage(kBadMessageInvalidBuffer);
    return;
  }

  // Associates a `WebNNBuffer` instance with this context so the WebNN service
  // can access the implementation.
  buffer_impls_.emplace(std::move(buffer_impl));
}

bool WebNNContextImpl::IsWebNNBufferValid(
    const base::UnguessableToken& handle) const {
  if (handle.is_empty()) {
    return false;
  }
  return buffer_impls_.contains(handle);
}

void WebNNContextImpl::DisconnectAndDestroyWebNNBufferImpl(
    const base::UnguessableToken& handle) {
  const auto it = buffer_impls_.find(handle);
  CHECK(it != buffer_impls_.end());
  // Upon calling erase, the handle will no longer refer to a valid
  // `WebNNBufferImpl`.
  buffer_impls_.erase(it);
}

}  // namespace webnn
