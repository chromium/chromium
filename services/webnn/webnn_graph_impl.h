// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_GRAPH_IMPL_H_
#define SERVICES_WEBNN_WEBNN_GRAPH_IMPL_H_

#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_service.mojom.h"

namespace webnn {

class WebNNGraphImpl : public mojom::WebNNGraph {
 public:
  WebNNGraphImpl();
  WebNNGraphImpl(const WebNNGraphImpl&) = delete;
  WebNNGraphImpl& operator=(const WebNNGraphImpl&) = delete;
  ~WebNNGraphImpl() override;

  // Return false if the graph is invalid.
  static bool ValidateAndBuildGraph(
      mojom::WebNNContext::CreateGraphCallback callback,
      const mojom::GraphInfoPtr& graph_info);

  // The actual platform graph creation and building will be bypassed if it is
  // set true.
  static void SetValidationOnlyForTesting(bool is_validation_only_for_testing);
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_GRAPH_IMPL_H_
