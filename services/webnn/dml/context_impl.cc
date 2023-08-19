// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/context_impl.h"

#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/graph_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"

namespace webnn::dml {

ContextImpl::ContextImpl(scoped_refptr<Adapter> adapter,
                         mojo::PendingReceiver<mojom::WebNNContext> receiver,
                         WebNNContextProviderImpl* context_provider)
    : WebNNContextImpl(std::move(receiver), context_provider),
      adapter_(std::move(adapter)) {}

ContextImpl::~ContextImpl() = default;

void ContextImpl::CreateGraphImpl(
    mojom::GraphInfoPtr graph_info,
    mojom::WebNNContext::CreateGraphCallback callback) {
  GraphImpl::CreateAndBuild(adapter_->command_queue(), adapter_->dml_device(),
                            std::move(graph_info), std::move(callback));
}

}  // namespace webnn::dml
