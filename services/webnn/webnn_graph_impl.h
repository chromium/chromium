// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_GRAPH_IMPL_H_
#define SERVICES_WEBNN_WEBNN_GRAPH_IMPL_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"

namespace webnn {

class WebNNBufferImpl;
class WebNNContextImpl;

class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNGraphImpl
    : public mojom::WebNNGraph {
 public:
  // The members of `ComputeResourceInfo` are used to validate the inputs
  // of a graph execution. The input name and byte length of computation must
  // match graph's expectation, the output name and byte length are used to
  // create the result of computation.
  struct COMPONENT_EXPORT(WEBNN_SERVICE) ComputeResourceInfo {
    explicit ComputeResourceInfo(const mojom::GraphInfoPtr& graph_info);
    ~ComputeResourceInfo();

    ComputeResourceInfo(const ComputeResourceInfo&) = delete;
    ComputeResourceInfo& operator=(const ComputeResourceInfo&) = delete;

    ComputeResourceInfo(ComputeResourceInfo&&);
    ComputeResourceInfo& operator=(ComputeResourceInfo&&);

    base::flat_map<std::string, size_t> input_name_to_byte_length_map;
    base::flat_map<std::string, size_t> output_name_to_byte_length_map;
  };

  // TODO(crbug.com/333188631): remove once no GraphImpls need to be created as
  // self-receiver.
  explicit WebNNGraphImpl(ComputeResourceInfo compute_resource_info);

  // Constructs a graph where the receiever and implementation is owned by the
  // context upon calling WebNNContextImpl::OnWebNNGraphImplCreated.
  WebNNGraphImpl(WebNNContextImpl* context,
                 ComputeResourceInfo compute_resource_info);

  WebNNGraphImpl(const WebNNGraphImpl&) = delete;
  WebNNGraphImpl& operator=(const WebNNGraphImpl&) = delete;
  ~WebNNGraphImpl() override;

  // Return false if the graph is invalid.
  static bool ValidateGraph(const mojom::ContextProperties& context_properties,
                            const mojom::GraphInfoPtr& graph_info);

  const ComputeResourceInfo& compute_resource_info() const {
    return compute_resource_info_;
  }

 private:
  // The validator is to make sure the inputs from a compute call match the
  // built graph's expected.
  ComputeResourceInfo compute_resource_info_;

  // WebNNContextImpl owns this object.
  const raw_ptr<WebNNContextImpl> context_;

  // mojom::WebNNGraph
  void Compute(base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
               mojom::WebNNGraph::ComputeCallback callback) override;

  void Dispatch(
      const base::flat_map<std::string, base::UnguessableToken>& named_inputs,
      const base::flat_map<std::string, base::UnguessableToken>& named_outputs)
      override;

  // An WebNNGraph backend should implement this method to execute the compiled
  // platform graph asynchronously.
  virtual void ComputeImpl(
      base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
      mojom::WebNNGraph::ComputeCallback callback) = 0;

  // Execute the compiled platform graph. The `named_inputs` and `named_outputs`
  // were validated in base class.
  virtual void DispatchImpl(
      const base::flat_map<std::string_view, WebNNBufferImpl*>& named_inputs,
      const base::flat_map<std::string_view, WebNNBufferImpl*>&
          named_outputs) = 0;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_GRAPH_IMPL_H_
