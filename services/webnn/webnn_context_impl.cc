// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_impl.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/self_owned_receiver.h"
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
    receiver_.ReportBadMessage("Invalid graph from renderer.");
    return;
  }
  // Call CreateGraphImpl() implemented by a backend.
  CreateGraphImpl(std::move(graph_info), std::move(callback));
}

}  // namespace webnn
