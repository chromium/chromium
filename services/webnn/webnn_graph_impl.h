// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_GRAPH_IMPL_H_
#define SERVICES_WEBNN_WEBNN_GRAPH_IMPL_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_object_impl.h"

namespace webnn {

class WebNNContextImpl;
class WebNNGraphBuilderImpl;
class WebNNTensorImpl;

class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNGraphImpl
    : public mojom::WebNNGraph,
      public WebNNObjectImpl<blink::WebNNGraphToken> {
 public:
  // Describes the constraints of a graph's inputs and outputs.
  struct COMPONENT_EXPORT(WEBNN_SERVICE) ComputeResourceInfo {
    ComputeResourceInfo(base::flat_map<std::string, OperandDescriptor>
                            input_names_to_descriptors,
                        base::flat_map<std::string, OperandDescriptor>
                            output_names_to_descriptors,
                        base::flat_map<uint64_t, base::flat_set<size_t>>
                            operand_to_dependent_operations,
                        base::PassKey<WebNNGraphBuilderImpl> pass_key);
    ~ComputeResourceInfo();

    ComputeResourceInfo(const ComputeResourceInfo&) = delete;
    ComputeResourceInfo& operator=(const ComputeResourceInfo&) = delete;

    ComputeResourceInfo(ComputeResourceInfo&&);
    ComputeResourceInfo& operator=(ComputeResourceInfo&&);

    base::flat_map<std::string, OperandDescriptor> input_names_to_descriptors;
    base::flat_map<std::string, OperandDescriptor> output_names_to_descriptors;
    base::flat_map<uint64_t, base::flat_set<size_t>>
        operand_to_dependent_operations;
  };

  // Constructs a graph where the receiever and implementation is owned by the
  // context.
  WebNNGraphImpl(mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
                 WebNNContextImpl* context,
                 ComputeResourceInfo compute_resource_info);

  WebNNGraphImpl(const WebNNGraphImpl&) = delete;
  WebNNGraphImpl& operator=(const WebNNGraphImpl&) = delete;
  ~WebNNGraphImpl() override;

  const ComputeResourceInfo& compute_resource_info() const {
    return compute_resource_info_;
  }

  WebNNContextImpl* context() const { return context_.get(); }

 private:
  void OnConnectionError();

  // mojom::WebNNGraph
  void Dispatch(
      const base::flat_map<std::string, blink::WebNNTensorToken>& named_inputs,
      const base::flat_map<std::string, blink::WebNNTensorToken>& named_outputs)
      override;

  // Execute the compiled platform graph. The `named_inputs` and `named_outputs`
  // were validated in base class.
  virtual void DispatchImpl(
      const base::flat_map<std::string_view, WebNNTensorImpl*>& named_inputs,
      const base::flat_map<std::string_view, WebNNTensorImpl*>&
          named_outputs) = 0;

  // The validator is to make sure the inputs from a compute call match the
  // built graph's expected.
  ComputeResourceInfo compute_resource_info_;

  // WebNNContextImpl owns this object.
  const raw_ptr<WebNNContextImpl> context_;

  mojo::AssociatedReceiver<mojom::WebNNGraph> receiver_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_GRAPH_IMPL_H_
