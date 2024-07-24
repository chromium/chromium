// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_GRAPH_BUILDER_IMPL_H_
#define SERVICES_WEBNN_WEBNN_GRAPH_BUILDER_IMPL_H_

#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"

namespace webnn {

class WebNNContextImpl;

// Services-side connection to an `MLGraphBuilder`. Responsible for managing
// data associated with the graph builder. This class does not own the graphs it
// mints; once a graph is built, it will no longer depend on its builder.
class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNGraphBuilderImpl
    : public mojom::WebNNGraphBuilder {
 public:
  explicit WebNNGraphBuilderImpl(WebNNContextImpl& context);

  WebNNGraphBuilderImpl(const WebNNGraphBuilderImpl&) = delete;
  WebNNGraphBuilderImpl& operator=(const WebNNGraphBuilderImpl&) = delete;

  ~WebNNGraphBuilderImpl() override;

  // mojom::WebNNGraphBuilder
  void CreateGraph(mojom::GraphInfoPtr graph_info,
                   CreateGraphCallback callback) override;

 private:
  // The `WebNNContextImpl` which owns and will outlive this object.
  const raw_ref<WebNNContextImpl> context_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_GRAPH_BUILDER_IMPL_H_
