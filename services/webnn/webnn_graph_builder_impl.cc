// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_graph_builder_impl.h"

#include "services/webnn/error.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn {

WebNNGraphBuilderImpl::WebNNGraphBuilderImpl(WebNNContextImpl& context)
    : context_(context) {}

WebNNGraphBuilderImpl::~WebNNGraphBuilderImpl() = default;

void WebNNGraphBuilderImpl::CreateGraph(mojom::GraphInfoPtr graph_info,
                                        CreateGraphCallback callback) {
  if (!WebNNGraphImpl::ValidateGraph(context_->properties(), *graph_info)) {
    context_->ReportBadGraphBuilderMessage(
        kBadMessageInvalidGraph, base::PassKey<WebNNGraphBuilderImpl>());
    return;
  }

  context_->CreateGraph(std::move(graph_info), std::move(callback));
}

}  // namespace webnn
