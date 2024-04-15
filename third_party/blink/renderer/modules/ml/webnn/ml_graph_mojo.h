// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_MOJO_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_MOJO_H_

#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/ml_trace.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"

namespace blink {

// The `Mojo` in the class name means this graph is backed by a service running
// outside of Blink.
class MODULES_EXPORT MLGraphMojo final : public MLGraph {
 public:
  // Create and build an MLGraphMojo object. Resolve the promise with this
  // concrete object if the graph builds successfully out of renderer process.
  // The caller must call `Promise()` on `resolver` before calling this method.
  static void ValidateAndBuild(ScopedMLTrace scoped_trace,
                               MLContext* context,
                               const MLNamedOperands& named_outputs,
                               ScriptPromiseResolver<MLGraph>* resolver);

  MLGraphMojo(ScriptState* script_state, MLContext* context);
  ~MLGraphMojo() override;

  void Trace(Visitor* visitor) const override;

 private:
  // Create `WebNNGraph` message pipe with `WebNNContext` mojo interface, then
  // build the computational graph with the hardware accelerated OS machine
  // learning API in the WebNN Service.
  void BuildImpl(ScopedMLTrace scoped_trace,
                 const MLNamedOperands& outputs,
                 ScriptPromiseResolver<MLGraph>* resolver) override;

  void ComputeImpl(ScopedMLTrace scoped_trace,
                   const MLNamedArrayBufferViews& inputs,
                   const MLNamedArrayBufferViews& outputs,
                   ScriptPromiseResolver<MLComputeResult>* resolver,
                   ExceptionState& exception_state) override;
  // The callback of computing `WebNNGraph` by calling hardware accelerated OS
  // machine learning APIs.
  void OnDidCompute(
      ScopedMLTrace scoped_trace,
      ScriptPromiseResolver<MLComputeResult>* resolver,
      std::unique_ptr<Vector<std::pair<String, ArrayBufferViewInfo>>>
          inputs_info,
      std::unique_ptr<Vector<std::pair<String, ArrayBufferViewInfo>>>
          outputs_info,
      webnn::mojom::blink::ComputeResultPtr mojo_result);

  // The callback of creating `WebNNGraph` mojo interface from WebNN Service.
  // The returned `CreateGraphResultPtr` contains a `pending_remote<WebNNGraph>`
  // if the graph was successfully created and an `Error` otherwise.
  void OnCreateWebNNGraph(ScopedMLTrace scoped_trace,
                          ScriptPromiseResolver<MLGraph>* resolver,
                          webnn::mojom::blink::CreateGraphResultPtr result);

  Member<MLContext> ml_context_;

  // The `WebNNGraph` is compiled graph that can be executed by the hardware
  // accelerated OS machine learning API.
  HeapMojoAssociatedRemote<webnn::mojom::blink::WebNNGraph> remote_graph_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_MOJO_H_
