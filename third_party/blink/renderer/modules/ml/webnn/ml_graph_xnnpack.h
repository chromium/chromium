// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_XNNPACK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_XNNPACK_H_

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/xnnpack/src/include/xnnpack.h"

namespace blink {

class ScriptPromiseResolver;

namespace {
class SharedXnnpackContext;
}

class MODULES_EXPORT MLGraphXnnpack final : public MLGraph {
 public:
  // Create and build an MLGraphXnnpack object. Resolve the promise with
  // this concrete object if the underlying XNNPACK subgraph builds
  // successfully.
  static void ValidateAndBuildAsync(MLContext* context,
                                    const MLNamedOperands& named_outputs,
                                    ScriptPromiseResolver* resolver);

  // Create and build a MLGraphXnnpack object synchronously in the caller's
  // thread. Return this concrete object if the underlying XNNPACK subgraph
  // builds successfully.
  static MLGraph* ValidateAndBuildSync(MLContext* context,
                                       const MLNamedOperands& named_outputs,
                                       ExceptionState& exception_state);

  // The constructor shouldn't be called directly. The callers should use
  // ValidateAndBuildAsync() or ValidateAndBuildSync() method instead.
  explicit MLGraphXnnpack(MLContext* context);

  ~MLGraphXnnpack() override;

  // Return the operators in topological order by searching from the named
  // output operands. It ensures operator 'j' appears before operator 'i' in the
  // result, if 'i' depends on 'j'.

  // The sorted operators will be used by CreateXnnSubgraphAndRuntime() that
  // defines the subgraph Nodes for operators in topological order.
  static HeapVector<Member<const MLOperator>>* GetOperatorsInTopologicalOrder(
      const MLNamedOperands& named_outputs);

 private:
  // Post the XNNPACK subgraph building to a background thread.
  void BuildAsyncImpl(const MLNamedOperands& named_outputs,
                      ScriptPromiseResolver* resolver) override;

  // Build the XNNPACK subgraph off the main thread.
  static void BuildOnBackgroundThread(
      CrossThreadPersistent<MLGraphXnnpack> graph,
      CrossThreadPersistent<HeapVector<Member<const MLOperator>>>
          toposorted_operators,
      CrossThreadPersistent<ScriptPromiseResolver> resolver,
      scoped_refptr<base::SequencedTaskRunner> resolver_task_runner);

  // Resolve the promise on the main thread after finish building the XNNPACK
  // subgraph.
  void OnBuildFinished(CrossThreadPersistent<ScriptPromiseResolver> resolver,
                       xnn_status status,
                       String error_message = String());

  // Build the XNNPACK Subgraph synchronously in the caller's thread. If the
  // XNNPACK Subgraph builds successfully, it should return this MLGraphXnnpack
  // object. Otherwise, it returns a nullptr and throw a DOMException
  // accordingly.
  MLGraph* BuildSyncImpl(const MLNamedOperands& named_outputs,
                         ExceptionState& exception_state) override;

  // Post the XNNPACK Runtime invocation to a background thread.
  void ComputeAsyncImpl(const MLNamedArrayBufferViews& inputs,
                        const MLNamedArrayBufferViews& outputs,
                        ScriptPromiseResolver* resolver) override;

  // The SharedXnnpackContext is shared and reference-counted by all instances
  // of MLGraphXnnpack. It initializes (and also deinitializes) the XNNPACK
  // library for graph building and execution.
  scoped_refptr<SharedXnnpackContext> xnn_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_XNNPACK_H_
