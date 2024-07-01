// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_GRAPH_IMPL_CROS_H_
#define SERVICES_WEBNN_TFLITE_GRAPH_IMPL_CROS_H_

#include "components/ml/mojom/web_platform_model.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::tflite {

class ContextImplCrOS;

// GraphImplCrOS inherits from WebNNGraphImpl to represent a TFLite graph
// implementation on ChromeOS platform. It is mainly responsible for building a
// TFLite flatbuffer model from mojom::GraphInfo via tflite::GraphBuilderTflite,
// then initializing and executing the graph with ML Service.
class GraphImplCrOS final : public WebNNGraphImpl {
 public:
  static void CreateAndBuild(
      ContextImplCrOS* context_impl,
      mojom::GraphInfoPtr graph_info,
      ComputeResourceInfo compute_resource_info,
      WebNNContextImpl::CreateGraphImplCallback callback);

  GraphImplCrOS(const GraphImplCrOS&) = delete;
  GraphImplCrOS& operator=(const GraphImplCrOS&) = delete;
  ~GraphImplCrOS() override;

 private:
  GraphImplCrOS(
      ContextImplCrOS* context_impl,
      ComputeResourceInfo compute_resource_info,
      mojo::PendingRemote<ml::model_loader::mojom::Model> pending_remote);

  // Execute the compiled platform graph asynchronously. The `named_inputs` were
  // validated in base class so we can use them to compute directly, the result
  // of execution will be returned to renderer process with the `callback`.
  void ComputeImpl(
      base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
      mojom::WebNNGraph::ComputeCallback callback) override;

  void DispatchImpl(
      const base::flat_map<std::string_view, WebNNBufferImpl*>& named_inputs,
      const base::flat_map<std::string_view, WebNNBufferImpl*>& named_outputs)
      override;

  mojo::Remote<ml::model_loader::mojom::Model> model_remote_;
};

}  // namespace webnn::tflite

#endif  // SERVICES_WEBNN_TFLITE_GRAPH_IMPL_CROS_H_
