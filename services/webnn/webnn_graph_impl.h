// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_GRAPH_IMPL_H_
#define SERVICES_WEBNN_WEBNN_GRAPH_IMPL_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_object_impl.h"

namespace webnn {

class WebNNContextImpl;
class WebNNGraphBuilderImpl;
class WebNNTensorImpl;

// GPU process implementation of the `MLGraph` interface. While this class is
// reference-counted a `WebNNGraphImpl` is guaranteed not to outlive the
// `WebNNContextImpl` that created it because references are only held by the
// context itself or by tasks scheduled to its `gpu::Scheduler` sequence which
// is shut down when the context is destroyed.
//
// This invariant is checked by the `raw_ref<WebNNContextImpl>` member, which
// will trigger dangling pointer warnings in debug builds and safe crashes in
// release builds.
class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNGraphImpl
    : public WebNNObjectImpl<mojom::WebNNGraph,
                             blink::WebNNGraphToken,
                             mojo::AssociatedReceiver<mojom::WebNNGraph>> {
 public:
  // Describes the constraints of a graph's inputs and outputs.
  struct COMPONENT_EXPORT(WEBNN_SERVICE) ComputeResourceInfo {
    ComputeResourceInfo(
        base::flat_map<std::string, OperandDescriptor>
            input_names_to_descriptors,
        base::flat_map<std::string, OperandDescriptor>
            output_names_to_descriptors,
        base::flat_map<OperandId, base::flat_set<OperationId>>
            operand_to_dependent_operations,
        base::flat_map<OperandId, OperationId> operand_to_producing_operation,
        base::PassKey<WebNNGraphBuilderImpl> pass_key);
    ~ComputeResourceInfo();

    ComputeResourceInfo(const ComputeResourceInfo&) = delete;
    ComputeResourceInfo& operator=(const ComputeResourceInfo&) = delete;

    ComputeResourceInfo(ComputeResourceInfo&&);
    ComputeResourceInfo& operator=(ComputeResourceInfo&&);

    base::flat_map<std::string, OperandDescriptor> input_names_to_descriptors;
    base::flat_map<std::string, OperandDescriptor> output_names_to_descriptors;
    base::flat_map<OperandId, base::flat_set<OperationId>>
        operand_to_dependent_operations;
    base::flat_map<OperandId, OperationId> operand_to_producing_operation;
  };

  // Constructs a graph where the receiever and implementation is owned by the
  // context.
  WebNNGraphImpl(mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
                 WebNNContextImpl& context,
                 ComputeResourceInfo compute_resource_info,
                 std::vector<mojom::Device> devices);

  WebNNGraphImpl(const WebNNGraphImpl&) = delete;
  WebNNGraphImpl& operator=(const WebNNGraphImpl&) = delete;

  const ComputeResourceInfo& compute_resource_info() const {
    return compute_resource_info_;
  }

  const std::vector<mojom::Device>& devices() { return devices_; }

  // Execute the dispatch on the GPU sequence (or directly if no GPU sequence).
  // Called by WebNNContextImpl::Dispatch() after input/output tensors have been
  // validated and resolved. Schedules the backend's DispatchImpl() on the GPU
  // sequence, checking that no tensors are exported before running.
  void RunDispatch(
      base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>> named_inputs,
      base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>> named_outputs,
      webnn::ScopedTrace scoped_trace,
      mojo::ReportBadMessageCallback bad_message_cb);

 protected:
  ~WebNNGraphImpl() override;

  // The `WebNNContextImpl` which owns and will outlive this object.
  const base::raw_ref<WebNNContextImpl> context_;

 private:
  void OnDisconnect() override;

  // Execute the compiled platform graph. The `named_inputs` and `named_outputs`
  // were validated in WebNNContextImpl::Dispatch().
  virtual void DispatchImpl(
      base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>> named_inputs,
      base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
          named_outputs) = 0;

  // The validator is to make sure the inputs from a compute call match the
  // built graph's expected.
  ComputeResourceInfo compute_resource_info_;

  const std::vector<mojom::Device> devices_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_GRAPH_IMPL_H_
