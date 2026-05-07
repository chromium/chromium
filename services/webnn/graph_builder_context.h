// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_GRAPH_BUILDER_CONTEXT_H_
#define SERVICES_WEBNN_GRAPH_BUILDER_CONTEXT_H_

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_error.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn {

class WebNNGraphBuilderImpl;
class WebNNTensorImpl;

// Interface for the context that hosts WebNNGraphBuilderImpl instances.
// Implemented by WebNNContextImpl (GPU process) and CompilerContextImplOrt
// (Compiler process).
class COMPONENT_EXPORT(WEBNN_SERVICE) GraphBuilderContext {
 public:
  // Result of a successful graph creation, returned via
  // BuildGraphCallback.
  struct GraphCreationResult {
    GraphCreationResult();
    GraphCreationResult(GraphCreationResult&&);
    GraphCreationResult& operator=(GraphCreationResult&&);
    ~GraphCreationResult();

    blink::WebNNGraphToken graph_token;
    std::vector<mojom::Device> devices;
  };

  using BuildGraphCallback = base::OnceCallback<void(
      base::expected<GraphCreationResult, mojom::ErrorPtr>)>;

  virtual ~GraphBuilderContext() = default;

  virtual const ContextProperties& properties() const = 0;
  virtual const mojom::CreateContextOptions& options() const = 0;

  // Creates a graph from validated graph info. For GPU-hosted contexts, this
  // compiles the graph and loads it locally. For Compiler-hosted contexts, this
  // compiles the graph and sends the compiled model to GPU process.
  virtual void BuildGraph(
      mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
      mojom::GraphInfoPtr graph_info,
      WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      base::flat_map<OperandId, scoped_refptr<WebNNTensorImpl>>
          constant_tensor_operands,
      BuildGraphCallback callback) = 0;

  // Called by a graph builder to destroy itself.
  virtual void RemoveGraphBuilder(
      mojo::ReceiverId graph_builder_id,
      base::PassKey<WebNNGraphBuilderImpl> pass_key) = 0;

  // Report the currently dispatching Message as bad and remove the
  // GraphBuilder receiver which received it.
  virtual void ReportBadGraphBuilderMessage(
      const std::string& message,
      base::PassKey<WebNNGraphBuilderImpl> pass_key) = 0;

 protected:
  // Returns a pass key for setting graph builder IDs.
  static base::PassKey<GraphBuilderContext> GetPassKey() {
    return base::PassKey<GraphBuilderContext>();
  }
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_GRAPH_BUILDER_CONTEXT_H_
