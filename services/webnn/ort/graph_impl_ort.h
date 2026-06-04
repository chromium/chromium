// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_GRAPH_IMPL_ORT_H_
#define SERVICES_WEBNN_ORT_GRAPH_IMPL_ORT_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/ort/graph_builder_ort.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/webnn_error.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-forward.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn {

class WebNNConstantOperand;

namespace ort {

class ContextImplOrt;
class Environment;
class SessionOptions;

// GraphImplOrt inherits from WebNNGraphImpl to represent an ORT graph
// implementation. It is mainly responsible for building an ORT
// model from mojom::GraphInfo via ort::GraphBuilderOrt, then executing the
// graph.
class GraphImplOrt final : public WebNNGraphImpl {
 public:
  static void CreateAndBuild(
      mojo::PendingReceiver<mojom::WebNNGraph> receiver,
      mojom::GraphInfoPtr graph_info,
      ComputeResourceInfo compute_resource_info,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      base::flat_map<OperandId, scoped_refptr<WebNNTensorImpl>>
          constant_tensor_operands,
      ContextImplOrt& context,
      WebNNContextImpl::CreateGraphImplCallback callback);

  class ComputeResources;
  GraphImplOrt(mojo::PendingReceiver<mojom::WebNNGraph> receiver,
               ComputeResourceInfo compute_resource_info,
               std::unique_ptr<ComputeResources> compute_resources,
               WebNNContextImpl& context,
               std::vector<mojom::Device> devices);

  GraphImplOrt(const GraphImplOrt&) = delete;
  GraphImplOrt& operator=(const GraphImplOrt&) = delete;

  // Creates an ORT session from a compiled graph received from the
  // Compiler process and constructs the WebNNGraphImpl.
  static base::expected<scoped_refptr<WebNNGraphImpl>, mojom::ErrorPtr>
  CreateSessionFromCompiledGraph(
      mojo::PendingReceiver<mojom::WebNNGraph> receiver,
      WebNNContextImpl& context,
      ComputeResourceInfo compute_resource_info,
      scoped_refptr<SessionOptions> session_options,
      scoped_refptr<Environment> env,
      mojo_base::BigBuffer compiled_model_data,
      base::flat_map<std::string, std::string>
          operand_input_name_to_onnx_input_name,
      base::flat_map<std::string, std::string>
          operand_output_name_to_onnx_output_name);

 private:
  ~GraphImplOrt() override;

  // Builds the model and creates the session in-process using
  // CreateSessionFromModel. ExternalWeightsManager is kept alive in
  // ComputeResources since weights are referenced, not embedded.
  static base::expected<std::unique_ptr<ComputeResources>, mojom::ErrorPtr>
  CreateAndBuildOnBackgroundThread(
      mojom::GraphInfoPtr graph_info,
      scoped_refptr<SessionOptions> session_options,
      scoped_refptr<Environment> env,
      ContextProperties context_properties,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      ScopedTrace scoped_trace);

  static void DidCreateAndBuild(
      mojo::PendingReceiver<mojom::WebNNGraph> receiver,
      WebNNContextImpl& context,
      ComputeResourceInfo compute_resource_info,
      WebNNContextImpl::CreateGraphImplCallback callback,
      base::expected<std::unique_ptr<ComputeResources>, mojom::ErrorPtr>
          result);

  // Execute the compiled platform graph asynchronously. The inputs were
  // validated in base class so we can use them to compute directly.
  void DispatchImpl(base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
                        named_input_tensors,
                    base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
                        named_output_tensors) override;

  std::unique_ptr<ComputeResources> compute_resources_;
  base::WeakPtrFactory<GraphImplOrt> weak_factory_{this};
};

}  // namespace ort
}  // namespace webnn

#endif  // SERVICES_WEBNN_ORT_GRAPH_IMPL_ORT_H_
