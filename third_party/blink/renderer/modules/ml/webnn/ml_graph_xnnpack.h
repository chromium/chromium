// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_XNNPACK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_XNNPACK_H_

#include "base/task/sequenced_task_runner.h"
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

// Map the MLGraph's input or output name to the XNNPACK external Value ID.
using ExternalValueIdMap = HashMap<String, uint32_t>;

using DataBufferPtr = std::unique_ptr<uint8_t[]>;

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

  const ExternalValueIdMap& GetInputExternalValueIdMapForTesting() const;
  const ExternalValueIdMap& GetOutputExternalValueIdMapForTesting() const;
  const Vector<xnn_external_value>& GetXnnExternalValuesTesting() const;

 private:
  // Post the XNNPACK Subgraph and Runtime building to a background thread.
  void BuildAsyncImpl(const MLNamedOperands& named_outputs,
                      ScriptPromiseResolver* resolver) override;

  // Build the XNNPACK Subgraph and Runtime off the main thread.
  static void BuildOnBackgroundThread(
      CrossThreadPersistent<MLGraphXnnpack> graph,
      CrossThreadPersistent<MLNamedOperands> named_outputs,
      CrossThreadPersistent<HeapVector<Member<const MLOperator>>>
          toposorted_operators,
      CrossThreadPersistent<ScriptPromiseResolver> resolver,
      scoped_refptr<base::SequencedTaskRunner> resolver_task_runner);

  // Resolve the promise on the main thread after finish building the XNNPACK
  // Subgraph and Runtime.
  void OnBuildFinished(CrossThreadPersistent<ScriptPromiseResolver> resolver,
                       xnn_status status,
                       String error_message = String());

  // Build the XNNPACK Subgraph synchronously in the caller's thread. If the
  // XNNPACK Subgraph builds successfully, it should return this MLGraphXnnpack
  // object. Otherwise, it returns a nullptr and throw a DOMException
  // accordingly.
  MLGraph* BuildSyncImpl(const MLNamedOperands& named_outputs,
                         ExceptionState& exception_state) override;

  // Post the XNNPACK Runtime object invocation to a background thread.
  void ComputeAsyncImpl(const MLNamedArrayBufferViews& inputs,
                        const MLNamedArrayBufferViews& outputs,
                        ScriptPromiseResolver* resolver) override;

  // Invoke the XNNPACK Runtime object off the main thread.
  static void ComputeOnBackgroundThread(
      CrossThreadPersistent<MLGraphXnnpack> graph,
      CrossThreadPersistent<MLNamedArrayBufferViews> inputs,
      CrossThreadPersistent<MLNamedArrayBufferViews> outputs,
      CrossThreadPersistent<ScriptPromiseResolver> resolver,
      scoped_refptr<base::SequencedTaskRunner> resolver_task_runner);

  // Resolve the promise with an MLComputeResult that contains input and output
  // ArrayBufferViews on the main thread after finish invoking the XNNPACK
  // Runtime object.
  void OnComputeFinished(CrossThreadPersistent<MLNamedArrayBufferViews> inputs,
                         CrossThreadPersistent<MLNamedArrayBufferViews> outputs,
                         CrossThreadPersistent<ScriptPromiseResolver> resolver,
                         xnn_status status,
                         String error_message = String());

  // Invoke the XNNPACK Runtime object in the caller's thread.
  void ComputeSyncImpl(const MLNamedArrayBufferViews& inputs,
                       const MLNamedArrayBufferViews& outputs,
                       ExceptionState& exception_state) override;

  // This method firstly creates an XNNPACK Subgraph object and defines Subgraph
  // Nodes and Values for the operators and operands of a WebNN graph. Then it
  // creates an XNNPACK Runtime object from the Subgraph object. The Runtime
  // object is a combination of an execution plan for Subgraph Nodes and a
  // memory manager for Subgraph Values and will be used for the accelerated
  // executions. This method can run either in a background thread for
  // asynchronous graph building or in the caller's thread for synchronous graph
  // building.
  xnn_status CreateXnnSubgraphAndRuntime(
      const MLNamedOperands& named_outputs,
      const HeapVector<Member<const MLOperator>>& toposorted_operators,
      String& error_message);

  // This method creates the xnn_external_value vector from named input and
  // output array buffer views. The xnn_external_value vector is used to set up
  // the XNNPACK Runtime object. The returned vector is sorted by
  // `xnn_external_value::id`, and can be passed to
  // `NeedToSetupExternalValues()`.
  Vector<xnn_external_value> CreateExternalValues(
      const MLNamedArrayBufferViews& inputs,
      const MLNamedArrayBufferViews& outputs) const;

  // This method checks if any data pointers of the provided
  // `xnn_external_values` changed against the pointers that has been setup
  // (stored in `xnn_external_values_`).
  //
  // The change may be caused by user providing a different ArrayBufferView that
  // is backed by a newly allocated or reallocated store.
  //
  // The XNNPACK Runtime object setup may be expensive. If the data pointers
  // haven't changed, there's no need to redo the setup.
  bool NeedToSetupExternalValues(
      const Vector<xnn_external_value>& xnn_external_values) const;

  // This method sets up data pointers for XNNPACK external values, performs the
  // forward pass, then stores the result in the array buffer views provided by
  // `outputs`.
  //
  // This method can be called in the main thread or a background thread.
  xnn_status InvokeXnnRuntime(const MLNamedArrayBufferViews& inputs,
                              const MLNamedArrayBufferViews& outputs,
                              String& error_message);

  // Schedules resolving promises of asynchronous MLGraph build and compute.
  scoped_refptr<base::SequencedTaskRunner> resolver_task_runner_;

  // The SharedXnnpackContext is shared and reference-counted by all instances
  // of MLGraphXnnpack. It initializes (and also deinitializes) the XNNPACK
  // library for graph building and execution.
  scoped_refptr<SharedXnnpackContext> xnn_context_;

  // Holds the static data of XNNPACK Values for MLGraph's constant operands.
  // The data must outlive XNNPACK Subgraph and Runtime objects using them.
  Vector<DataBufferPtr> static_data_buffers_;

  // Map the names of the MLGraph's inputs/outputs to the XNNPACK external Value
  // IDs. They will be used to set up the xnn_external_value structures from the
  // input/output named array buffer views when invoking the XNNPACK Runtime
  // object for the MLGraph compute.
  ExternalValueIdMap input_external_value_id_map_;
  ExternalValueIdMap output_external_value_id_map_;

  // Used to track external values that have been setup, to avoid unnecessary
  // xnn_runtime_setup calls (which may be expensive). Sorted by
  // `xnn_external_value::id`.
  Vector<xnn_external_value> xnn_external_values_;

  // The XNNPACK Runtime object for the accelerated executions.
  std::unique_ptr<xnn_runtime, decltype(&xnn_delete_runtime)> xnn_runtime_{
      nullptr, &xnn_delete_runtime};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_XNNPACK_H_
