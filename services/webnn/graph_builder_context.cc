// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/graph_builder_context.h"

#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_graph_builder_impl.h"

namespace webnn {

GraphBuilderContext::GraphBuilderContext() = default;
GraphBuilderContext::~GraphBuilderContext() = default;

GraphBuilderContext::GraphCreationResult::GraphCreationResult(
    blink::WebNNGraphToken token,
    std::vector<mojom::Device> devices)
    : graph_token(token), devices(std::move(devices)) {}

GraphBuilderContext::GraphCreationResult::GraphCreationResult(
    GraphCreationResult&&) = default;
GraphBuilderContext::GraphCreationResult&
GraphBuilderContext::GraphCreationResult::operator=(GraphCreationResult&&) =
    default;
GraphBuilderContext::GraphCreationResult::~GraphCreationResult() = default;

void GraphBuilderContext::RemoveGraphBuilder(
    mojo::ReceiverId graph_builder_id,
    base::PassKey<WebNNGraphBuilderImpl> /*pass_key*/) {
  graph_builder_impls_.Remove(graph_builder_id);
}

void GraphBuilderContext::ReportBadGraphBuilderMessage(
    const std::string& message,
    base::PassKey<WebNNGraphBuilderImpl> /*pass_key*/) {
  graph_builder_impls_.ReportBadMessage(message);
}

void GraphBuilderContext::CreateGraphBuilderImpl(
    mojo::PendingReceiver<mojom::WebNNGraphBuilder> receiver) {
  auto graph_builder = std::make_unique<WebNNGraphBuilderImpl>(*this);
  WebNNGraphBuilderImpl* graph_builder_ptr = graph_builder.get();

  mojo::ReceiverId id =
      graph_builder_impls_.Add(std::move(graph_builder), std::move(receiver));

  graph_builder_ptr->SetId(id, base::PassKey<GraphBuilderContext>());
}

void GraphBuilderContext::ClearGraphBuilders() {
  graph_builder_impls_.Clear();
}

bool GraphBuilderContext::has_graph_builders() const {
  return !graph_builder_impls_.empty();
}

}  // namespace webnn
