// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_MOJO_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_MOJO_H_

#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_context_mojo.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class ScriptPromiseResolver;

// The `Mojo` in the class name means this graph is backed by a service running
// outside of Blink.
class MODULES_EXPORT MLGraphMojo final : public MLGraph {
 public:
  // Create and build an MLGraphMojo object. Resolve the promise with
  // this concrete object if the graph builds successfully out of renderer
  // process.
  static void ValidateAndBuildAsync(MLContextMojo* context,
                                    const MLNamedOperands& named_outputs,
                                    ScriptPromiseResolver* resolver);

  // Create and build an MLGraphMojo object.
  static MLGraph* ValidateAndBuildSync(ScriptState* script_state,
                                       MLContextMojo* context,
                                       const MLNamedOperands& named_outputs,
                                       ExceptionState& exception_state);

  MLGraphMojo(ScriptState* script_state, MLContextMojo* context);
  ~MLGraphMojo() override;

  void Trace(Visitor* visitor) const override;

 private:
  // Create `WebNNGraph` message pipe with `WebNNContext` mojo interface, then
  // build the computational graph with the hardware accelerated OS machine
  // learning API in the WebNN Service.
  void BuildAsyncImpl(const MLNamedOperands& outputs,
                      ScriptPromiseResolver* resolver) override;

  MLGraph* BuildSyncImpl(ScriptState* script_state,
                         const MLNamedOperands& named_outputs,
                         ExceptionState& exception_state) override;

  void ComputeAsyncImpl(const MLNamedArrayBufferViews& inputs,
                        const MLNamedArrayBufferViews& outputs,
                        ScriptPromiseResolver* resolver,
                        ExceptionState& exception_state) override;
  // The callback of computing `WebNNGraph` by calling hardware accelerated OS
  // machine learning APIs.
  void OnDidCompute(
      ScriptPromiseResolver* resolver,
      std::unique_ptr<Vector<std::pair<String, ArrayBufferViewInfo>>>
          inputs_info,
      std::unique_ptr<Vector<std::pair<String, ArrayBufferViewInfo>>>
          outputs_info,
      webnn::mojom::blink::ComputeResult mojo_result,
      const absl::optional<HashMap<String, mojo_base::BigBuffer>> mojo_outputs);

  void ComputeSyncImpl(const MLNamedArrayBufferViews& inputs,
                       const MLNamedArrayBufferViews& outputs,
                       ExceptionState& exception_state) override;

  // The callback of creating `WebNNGraph` mojo interface from WebNN Service.
  // The returned `CreateGraphResultPtr` contains a `pending_remote<WebNNGraph>`
  // if the graph was successfully created and an `Error` otherwise.
  void OnCreateWebNNGraph(ScriptPromiseResolver* resolver,
                          webnn::mojom::blink::CreateGraphResultPtr result);

  Member<MLContextMojo> ml_context_mojo_;

  // The `WebNNGraph` is compiled graph that can be executed by the hardware
  // accelerated OS machine learning API.
  HeapMojoRemote<webnn::mojom::blink::WebNNGraph> remote_graph_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_MOJO_H_
