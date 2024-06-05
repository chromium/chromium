// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/context_impl_tflite.h"

#include "base/types/expected_macros.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-shared.h"
#include "services/webnn/tflite/buffer_impl_tflite.h"
#include "services/webnn/tflite/graph_impl_tflite.h"

namespace webnn::tflite {

namespace {

mojom::ContextPropertiesPtr GetProperties() {
  return mojom::ContextProperties::New(
      /*conv2d_input_layout=*/mojom::InputOperandLayout::kChannelsLast);
}

}  // namespace

ContextImplTflite::ContextImplTflite(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    WebNNContextProviderImpl* context_provider,
    mojom::CreateContextOptionsPtr options)
    : WebNNContextImpl(std::move(receiver), context_provider, GetProperties()),
      options_(std::move(options)) {}

ContextImplTflite::~ContextImplTflite() = default;

void ContextImplTflite::CreateGraphImpl(
    mojom::GraphInfoPtr graph_info,
    mojom::WebNNContext::CreateGraphCallback callback) {
  ASSIGN_OR_RETURN(std::unique_ptr<GraphImplTflite> graph,
                   GraphImplTflite::CreateAndBuild(std::move(graph_info), this),
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

std::unique_ptr<WebNNBufferImpl> ContextImplTflite::CreateBufferImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
    mojom::BufferInfoPtr buffer_info,
    const base::UnguessableToken& buffer_handle) {
  return BufferImplTflite::Create(std::move(receiver), this,
                                  std::move(buffer_info), buffer_handle);
}

}  // namespace webnn::tflite
