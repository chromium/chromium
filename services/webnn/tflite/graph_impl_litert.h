// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_GRAPH_IMPL_LITERT_H_
#define SERVICES_WEBNN_TFLITE_GRAPH_IMPL_LITERT_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_error.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-forward.h"
#include "services/webnn/queueable_resource_state.h"
#include "services/webnn/tflite/graph_builder_tflite.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn {

class WebNNConstantOperand;

namespace litert {

class ContextImplLiteRt;

// GraphImplLiteRt inherits from WebNNGraphImpl to represent a LiteRT graph
// implementation. It is mainly responsible for building a LiteRT flatbuffer
// model from mojom::GraphInfo via tflite::GraphBuilderTflite, then initializing
// and executing the graph.
class GraphImplLiteRt final : public WebNNGraphImpl {
 public:
  static void CreateAndBuild(
      mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
      mojom::GraphInfoPtr graph_info,
      ComputeResourceInfo compute_resource_info,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      base::flat_map<OperandId, WebNNTensorImpl*> constant_tensor_operands,
      ContextImplLiteRt* context,
      base::File weights_file,
      WebNNContextImpl::CreateGraphImplCallback callback);

  class ComputeResources;
  GraphImplLiteRt(mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
                  ComputeResourceInfo compute_resource_info,
                  std::vector<std::pair<std::string, tflite::TensorDescriptor>>
                      input_name_to_descriptor,
                  std::vector<std::pair<std::string, tflite::TensorDescriptor>>
                      output_name_to_descriptor,
                  scoped_refptr<QueueableResourceState<ComputeResources>>
                      compute_resources_state,
                  base::WeakPtr<WebNNContextImpl> context,
                  std::vector<mojom::Device> devices);

  GraphImplLiteRt(const GraphImplLiteRt&) = delete;
  GraphImplLiteRt& operator=(const GraphImplLiteRt&) = delete;

 private:
  ~GraphImplLiteRt() override;

  static base::expected<std::unique_ptr<ComputeResources>, mojom::ErrorPtr>
  CreateAndBuildOnBackgroundThread(
      ContextProperties context_properties,
      mojom::Device context_device,
      mojom::GraphInfoPtr graph_info,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      base::flat_map<OperandId, base::flat_set<OperationId>>
          operand_to_dependent_operations,
      base::flat_map<OperandId, OperationId> operand_to_producing_operation,
      base::File weights_file);

  static void DidCreateAndBuild(
      mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
      base::WeakPtr<WebNNContextImpl> context,
      ComputeResourceInfo compute_resource_info,
      WebNNContextImpl::CreateGraphImplCallback callback,
      base::expected<std::unique_ptr<ComputeResources>, mojom::ErrorPtr>
          result);

  // Execute the compiled platform graph asynchronously. The inputs were
  // validated in base class so we can use them to compute directly.
  void DispatchImpl(
      base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>> named_inputs,
      base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>> named_outputs)
      override;

  scoped_refptr<QueueableResourceState<ComputeResources>>
      compute_resources_state_;
  std::vector<std::pair<std::string, tflite::TensorDescriptor>>
      input_name_to_descriptor_;
  std::vector<std::pair<std::string, tflite::TensorDescriptor>>
      output_name_to_descriptor_;

  base::WeakPtrFactory<GraphImplLiteRt> weak_factory_{this};
};

}  // namespace litert
}  // namespace webnn

#endif  // SERVICES_WEBNN_TFLITE_GRAPH_IMPL_LITERT_H_
