// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_GRAPH_IMPL_TFLITE_H_
#define SERVICES_WEBNN_TFLITE_GRAPH_IMPL_TFLITE_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_error.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-forward.h"
#include "services/webnn/queueable_resource_state.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn {

class WebNNConstantOperand;

namespace tflite {

class ContextImplTflite;

// GraphImplTflite inherits from WebNNGraphImpl to represent a TFLite graph
// implementation. It is mainly responsible for building a TFLite flatbuffer
// model from mojom::GraphInfo via tflite::GraphBuilderTflite, then initializing
// and executing the graph.
class GraphImplTflite final : public WebNNGraphImpl {
 public:
  static base::expected<scoped_refptr<GraphImplTflite>, mojom::ErrorPtr>
  CreateAndBuild(
      mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
      mojom::GraphInfoPtr graph_info,
      ComputeResourceInfo compute_resource_info,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      base::flat_map<OperandId, WebNNTensorImpl*> constant_tensor_operands,
      ContextImplTflite* context);

  class ComputeResources;
  GraphImplTflite(mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
                  ComputeResourceInfo compute_resource_info,
                  base::flat_map<std::string, int> input_name_to_index,
                  base::flat_map<std::string, int> output_name_to_index,
                  scoped_refptr<QueueableResourceState<ComputeResources>>
                      compute_resources_state,
                  base::WeakPtr<WebNNContextImpl> context,
                  std::vector<mojom::Device> devices);

  GraphImplTflite(const GraphImplTflite&) = delete;
  GraphImplTflite& operator=(const GraphImplTflite&) = delete;

 private:
  ~GraphImplTflite() override;

  // Execute the compiled platform graph asynchronously. The inputs were
  // validated in base class so we can use them to compute directly.
  void DispatchImpl(
      base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>> named_inputs,
      base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>> named_outputs)
      override;

  scoped_refptr<QueueableResourceState<ComputeResources>>
      compute_resources_state_;
  base::flat_map<std::string, int> input_name_to_index_;
  base::flat_map<std::string, int> output_name_to_index_;

  base::WeakPtrFactory<GraphImplTflite> weak_factory_{this};
};

}  // namespace tflite
}  // namespace webnn

#endif  // SERVICES_WEBNN_TFLITE_GRAPH_IMPL_TFLITE_H_
