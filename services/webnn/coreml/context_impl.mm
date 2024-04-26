// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/coreml/context_impl.h"

#import <CoreML/CoreML.h>

#include "services/webnn/coreml/graph_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"

namespace webnn::coreml {

ContextImpl::ContextImpl(mojo::PendingReceiver<mojom::WebNNContext> receiver,
                         WebNNContextProviderImpl* context_provider)
    : WebNNContextImpl(std::move(receiver), context_provider) {}

ContextImpl::~ContextImpl() = default;

void ContextImpl::CreateGraphImpl(
    mojom::GraphInfoPtr graph_info,
    mojom::WebNNContext::CreateGraphCallback callback) {
  GraphImpl::CreateAndBuild(std::move(graph_info), std::move(callback));
}

std::unique_ptr<WebNNBufferImpl> ContextImpl::CreateBufferImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
    mojom::BufferInfoPtr buffer_info,
    const base::UnguessableToken& buffer_handle) {
  // TODO(crbug.com/40278771): Implement MLBuffer for CoreML. Involve
  // an IPC security reviewer.
  NOTIMPLEMENTED();
  return {};
}

}  // namespace webnn::coreml
