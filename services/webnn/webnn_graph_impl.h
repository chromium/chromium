// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_GRAPH_IMPL_H_
#define SERVICES_WEBNN_WEBNN_GRAPH_IMPL_H_

#include <string>

#include "base/containers/flat_map.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"

namespace webnn {

class WebNNGraphImpl : public mojom::WebNNGraph {
 public:
  // The members of `ComputeResourceInfo` are used to validate the inputs
  // of a graph execution. The input name and byte length of computation must
  // match graph's expectation, the output name and byte length are used to
  // create the result of computation.
  struct ComputeResourceInfo {
    explicit ComputeResourceInfo(const mojom::GraphInfoPtr& graph_info);
    ~ComputeResourceInfo();

    ComputeResourceInfo(const ComputeResourceInfo&) = delete;
    ComputeResourceInfo& operator=(const ComputeResourceInfo&) = delete;

    ComputeResourceInfo(ComputeResourceInfo&&);
    ComputeResourceInfo& operator=(ComputeResourceInfo&&);

    base::flat_map<std::string, size_t> input_name_to_byte_length_map;
    base::flat_map<std::string, size_t> output_name_to_byte_length_map;
  };

  explicit WebNNGraphImpl(ComputeResourceInfo compute_resource_info);
  WebNNGraphImpl(const WebNNGraphImpl&) = delete;
  WebNNGraphImpl& operator=(const WebNNGraphImpl&) = delete;
  ~WebNNGraphImpl() override;

  // Return false if the graph is invalid.
  static bool ValidateGraph(const mojom::GraphInfoPtr& graph_info);

  const ComputeResourceInfo& compute_resource_info() const {
    return compute_resource_info_;
  }

 private:
  // The validator is to make sure the inputs from a compute call match the
  // built graph's expected.
  ComputeResourceInfo compute_resource_info_;

  // mojom::WebNNGraph
  void Compute(base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
               mojom::WebNNGraph::ComputeCallback callback) override;

  // An WebNNGraph backend should implement this method to execute the compiled
  // platform graph asynchronously.
  virtual void ComputeImpl(
      base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
      mojom::WebNNGraph::ComputeCallback callback) = 0;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_GRAPH_IMPL_H_
