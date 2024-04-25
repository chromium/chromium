// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/context_impl.h"

#include "base/types/expected_macros.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/tflite/buffer_impl.h"
#include "services/webnn/tflite/graph_impl.h"

namespace webnn::tflite {

ContextImpl::ContextImpl(mojo::PendingReceiver<mojom::WebNNContext> receiver,
                         WebNNContextProviderImpl* context_provider,
                         mojom::CreateContextOptionsPtr options)
    : WebNNContextImpl(std::move(receiver), context_provider),
      options_(std::move(options)) {}

ContextImpl::~ContextImpl() = default;

void ContextImpl::CreateGraphImpl(
    mojom::GraphInfoPtr graph_info,
    mojom::WebNNContext::CreateGraphCallback callback) {
  ASSIGN_OR_RETURN(std::unique_ptr<GraphImpl> graph,
                   GraphImpl::CreateAndBuild(std::move(graph_info), this),
                   [&callback](mojom::ErrorPtr error) {
                     std::move(callback).Run(
                         mojom::CreateGraphResult::NewError(std::move(error)));
                   });

  mojo::PendingAssociatedRemote<mojom::WebNNGraph> remote;
  graph_receivers_.Add(std::move(graph),
                       remote.InitWithNewEndpointAndPassReceiver());
  std::move(callback).Run(
      mojom::CreateGraphResult::NewGraphRemote(std::move(remote)));
}

std::unique_ptr<WebNNBufferImpl> ContextImpl::CreateBufferImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
    mojom::BufferInfoPtr buffer_info,
    const base::UnguessableToken& buffer_handle) {
  return BufferImpl::Create(std::move(receiver), this, std::move(buffer_info),
                            buffer_handle);
}

}  // namespace webnn::tflite
