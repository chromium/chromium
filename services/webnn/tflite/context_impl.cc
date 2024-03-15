// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/context_impl.h"

#include "services/webnn/tflite/graph_impl.h"
#include "services/webnn/webnn_buffer_impl.h"

namespace webnn::tflite {

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
    mojo::PendingReceiver<mojom::WebNNBuffer> receiver,
    mojom::BufferInfoPtr buffer_info,
    const base::UnguessableToken& buffer_handle) {
  // TODO(crbug.com/1472888): Implement MLBuffer for TFLite. Involve
  // an IPC security reviewer.
  NOTIMPLEMENTED();
  return {};
}

}  // namespace webnn::tflite
