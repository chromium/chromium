// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_GRAPH_IMPL_H_
#define SERVICES_WEBNN_WEBNN_GRAPH_IMPL_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_service.mojom.h"

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

    base::flat_map<std::string, size_t> input_name_to_byte_length_map;
    // TODO(crbug.com/1455278): Add output information.
    // base::flat_map<std::string, size_t> output_name_to_byte_length_map;
  };

  explicit WebNNGraphImpl(
      std::unique_ptr<ComputeResourceInfo> compute_resource_info);
  WebNNGraphImpl(const WebNNGraphImpl&) = delete;
  WebNNGraphImpl& operator=(const WebNNGraphImpl&) = delete;
  ~WebNNGraphImpl() override;

  // Return false if the graph is invalid.
  static bool ValidateGraph(const mojom::GraphInfoPtr& graph_info);

 private:
  // The validator is to make sure the inputs from a compute call match the
  // built graph's expected.
  std::unique_ptr<ComputeResourceInfo> compute_resource_info_;

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
