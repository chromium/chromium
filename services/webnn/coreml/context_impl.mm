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
  // TODO(crbug.com/1472888): Implement MLBuffer for CoreML. Involve
  // an IPC security reviewer.
  NOTIMPLEMENTED();
  return {};
}

void ContextImpl::ReadBufferImpl(
    const WebNNBufferImpl& src_buffer,
    mojom::WebNNBuffer::ReadBufferCallback callback) {
  // TODO(crbug.com/1472888): Implement MLBuffer for CoreML. Involve
  // an IPC security reviewer.
  NOTIMPLEMENTED();
}

void ContextImpl::WriteBufferImpl(const WebNNBufferImpl& dst_buffer,
                                  mojo_base::BigBuffer src_buffer) {
  // TODO(crbug.com/1472888): Implement MLBuffer for CoreML. Involve
  // an IPC security reviewer.
  NOTIMPLEMENTED();
}

}  // namespace webnn::coreml
