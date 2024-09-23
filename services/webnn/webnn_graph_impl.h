// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_GRAPH_IMPL_H_
#define SERVICES_WEBNN_WEBNN_GRAPH_IMPL_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace webnn {

class WebNNContextImpl;
class WebNNGraphBuilderImpl;
class WebNNTensorImpl;

class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNGraphImpl
    : public mojom::WebNNGraph {
 public:
  // Describes the constraints of a graph's inputs and outputs.
  struct COMPONENT_EXPORT(WEBNN_SERVICE) ComputeResourceInfo {
    ComputeResourceInfo(base::flat_map<std::string, OperandDescriptor>
                            input_names_to_descriptors,
                        base::flat_map<std::string, OperandDescriptor>
                            output_names_to_descriptors,
                        base::PassKey<WebNNGraphBuilderImpl> pass_key);
    ~ComputeResourceInfo();

    ComputeResourceInfo(const ComputeResourceInfo&) = delete;
    ComputeResourceInfo& operator=(const ComputeResourceInfo&) = delete;

    ComputeResourceInfo(ComputeResourceInfo&&);
    ComputeResourceInfo& operator=(ComputeResourceInfo&&);

    base::flat_map<std::string, OperandDescriptor> input_names_to_descriptors;
    base::flat_map<std::string, OperandDescriptor> output_names_to_descriptors;
  };

  // Constructs a graph where the receiever and implementation is owned by the
  // context.
  WebNNGraphImpl(WebNNContextImpl* context,
                 ComputeResourceInfo compute_resource_info);

  WebNNGraphImpl(const WebNNGraphImpl&) = delete;
  WebNNGraphImpl& operator=(const WebNNGraphImpl&) = delete;
  ~WebNNGraphImpl() override;

  const ComputeResourceInfo& compute_resource_info() const {
    return compute_resource_info_;
  }

  WebNNContextImpl* context() const { return context_.get(); }

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
      const base::flat_map<std::string, blink::WebNNTensorToken>& named_inputs,
      const base::flat_map<std::string, blink::WebNNTensorToken>& named_outputs)
      override;

  // An WebNNGraph backend should implement this method to execute the compiled
  // platform graph asynchronously.
  virtual void ComputeImpl(
      base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
      mojom::WebNNGraph::ComputeCallback callback) = 0;

  // Execute the compiled platform graph. The `named_inputs` and `named_outputs`
  // were validated in base class.
  virtual void DispatchImpl(
      const base::flat_map<std::string_view, WebNNTensorImpl*>& named_inputs,
      const base::flat_map<std::string_view, WebNNTensorImpl*>&
          named_outputs) = 0;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_GRAPH_IMPL_H_
