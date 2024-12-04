// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_GRAPH_IMPL_ORT_H_
#define SERVICES_WEBNN_ORT_GRAPH_IMPL_ORT_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/ort/graph_builder_ort.h"
#include "services/webnn/public/mojom/webnn_error.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-forward.h"
#include "services/webnn/webnn_graph_impl.h"
#include "third_party/microsoft_dxheaders/include/onnxruntime_c_api.h"

namespace webnn {

class WebNNConstantOperand;

namespace ort {

class ContextImplOrt;

// GraphImplOrt inherits from WebNNGraphImpl to represent a ort graph
// implementation. It is mainly responsible for building a ort
// model from mojom::GraphInfo via ort::GraphBuilderOrt, then initializing
// and executing the graph.
class GraphImplOrt final : public WebNNGraphImpl {
 public:
  static base::expected<std::unique_ptr<GraphImplOrt>, mojom::ErrorPtr>
  CreateAndBuild(mojom::GraphInfoPtr graph_info,
                 ComputeResourceInfo compute_resource_info,
                 base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
                     constant_operands,
                 ContextImplOrt* context);

  GraphImplOrt(const GraphImplOrt&) = delete;
  GraphImplOrt& operator=(const GraphImplOrt&) = delete;
  ~GraphImplOrt() override;

 private:
  GraphImplOrt(
      ComputeResourceInfo compute_resource_info,
      // std::map<uint64_t, GraphBuilderOrt::OperandInfo> operand_infos,
      const OrtApi* g_ort,
      OrtEnv* env,
      OrtSession* session,
      OrtSessionOptions* session_options,
      ContextImplOrt* context);

  // Execute the compiled platform graph asynchronously. The inputs were
  // validated in base class so we can use them to compute directly.
  void DispatchImpl(
      const base::flat_map<std::string_view, WebNNTensorImpl*>& named_inputs,
      const base::flat_map<std::string_view, WebNNTensorImpl*>& named_outputs)
      override;

  // std::map<uint64_t, GraphBuilderOrt::OperandInfo> operand_infos_;
  raw_ptr<const OrtApi> g_ort_;
  raw_ptr<OrtEnv> env_;
  raw_ptr<OrtSession> session_;
  raw_ptr<OrtSessionOptions> session_options_;
  base::WeakPtrFactory<GraphImplOrt> weak_factory_{this};
};

}  // namespace ort
}  // namespace webnn

#endif  // SERVICES_WEBNN_ORT_GRAPH_IMPL_ORT_H_
