// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_XNNPACK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_XNNPACK_H_

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/xnnpack/src/include/xnnpack.h"

namespace blink {

class ScriptPromiseResolver;

namespace {
class SharedXnnpackContext;
class XnnRuntimeWrapper;
}

// Map the MLGraph's input or output name to the XNNPACK external Value ID.
using ExternalValueIdMap = HashMap<String, uint32_t>;

using DataBufferPtr = std::unique_ptr<uint8_t[]>;
using XnnSubgraphPtr =
    std::unique_ptr<xnn_subgraph, decltype(&xnn_delete_subgraph)>;
using XnnExternalValuesPtr = std::unique_ptr<Vector<xnn_external_value>>;

typedef Vector<std::pair<String, ArrayBufferViewInfo>>
    NamedArrayBufferViewsInfo;
using NamedArrayBufferViewsInfoPtr = std::unique_ptr<NamedArrayBufferViewsInfo>;

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
  // Build the XNNPACK Subgraph and Runtime asynchronously on a background
  // thread. It would execute the following four steps:
  // 1) Running `GetSharedXnnpackContextOnBackgroundThread()` on a background
  // thread. It may initialize the XNNPACK library, which can be time-consuming.
  // 2) Running `OnDidGetSharedXnnpackContext()` on the caller's thread. It
  // creates an XNNPACK Subgraph and defines its Nodes by traversing the
  // `MLOperator`s and `MLOperand`s of a WebNN graph that are allocated in the
  // heap of the caller's thread.
  // 3) Running `CreateXnnRuntimeOnBackgroundThread()` on a background thread.
  // The creation of XNNPACK Runtime includes graph optimization, operator
  // kernel creation and memory allocation and packing that can be
  // time-consuming.
  // 4) Running `OnDidCreateXnnRuntime()` on the calling thread. It resolves the
  // promise with this `MLGraphXnnpack` object if there are no errors.
  //
  // The sequence of above steps can be illustrated as the following diagram:
  //       Calling thread                    background thread
  //     `BuildAsyncImpl()`
  //              |
  //          post task ------->  `GetSharedXnnpackContextOnBackgroundThread()`
  //                                                  |
  //  `OnDidGetSharedXnnpackContext()`  <----------post task
  //              |
  //          post task ------------> `CreateXnnRuntimeOnBackgroundThread()`
  //                                                  |
  //   `OnDidCreateXnnRuntime()  <-----------------post task
  void BuildAsyncImpl(const MLNamedOperands& named_outputs,
                      ScriptPromiseResolver* resolver) override;

  static void GetSharedXnnpackContextOnBackgroundThread(
      CrossThreadHandle<MLGraphXnnpack> graph,
      CrossThreadHandle<MLNamedOperands> named_outputs,
      CrossThreadHandle<ScriptPromiseResolver> resolver,
      scoped_refptr<base::SequencedTaskRunner> resolver_task_runner);

  void OnDidGetSharedXnnpackContext(
      scoped_refptr<SharedXnnpackContext> xnn_context,
      MLNamedOperands* named_outputs,
      ScriptPromiseResolver* resolver,
      String error_message = String());

  static void CreateXnnRuntimeOnBackgroundThread(
      XnnSubgraphPtr subgraph,
      scoped_refptr<SharedXnnpackContext> xnn_context,
      Vector<DataBufferPtr> static_data_buffers,
      CrossThreadHandle<MLGraphXnnpack> graph,
      uint32_t num_threads,
      CrossThreadHandle<ScriptPromiseResolver> resolver,
      scoped_refptr<base::SequencedTaskRunner> resolver_task_runner);

  void OnDidCreateXnnRuntime(
      scoped_refptr<XnnRuntimeWrapper> xnn_runtime_wrapper,
      ScriptPromiseResolver* resolver,
      String error_message = String());

  // Build the XNNPACK Subgraph and Runtime synchronously in the caller's
  // thread. If the XNNPACK Subgraph and Runtime build successfully, it should
  // return this `MLGraphXnnpack` object. Otherwise, it returns a nullptr and
  // throw a DOMException accordingly.
  MLGraph* BuildSyncImpl(const MLNamedOperands& named_outputs,
                         ExceptionState& exception_state) override;

  // Post the XNNPACK Runtime object invocation to a background thread. The
  // input and output `MLNamedArrayBufferViews` will be transferred to the
  // `NamedArrayBufferViewsInfo` that are posted to background thread. This
  // would prevent the calling thread from modifying the contents of the
  // `ArrayBufferView` while the background thread is accessing them. And it
  // would also avoid accessing the heap-allocated `ArrayBufferView` in the
  // background thread.
  void ComputeAsyncImpl(const MLNamedArrayBufferViews& inputs,
                        const MLNamedArrayBufferViews& outputs,
                        ScriptPromiseResolver* resolver,
                        ExceptionState& exception_state) override;

  // Invoking an XNNPACK Runtime object can be time-consuming. Calling this
  // method in a background thread avoids blocking the main thread. The
  // ownership of `external_values`, `inputs_info` and `outputs_info` is
  // transferred to the background thread that invokes the XNNPACK Runtime
  // object with the input and output buffers. The GC objects wrapped by
  // `CrossThreadHandle` must not be accessed by this method and should be
  // passed forward to `OnDidCompute()` which is called on the thread
  // owning these GC objects.
  static void ComputeOnBackgroundThread(
      scoped_refptr<XnnRuntimeWrapper> xnn_runtime_wrapper,
      XnnExternalValuesPtr external_values,
      NamedArrayBufferViewsInfoPtr inputs_info,
      NamedArrayBufferViewsInfoPtr outputs_info,
      CrossThreadHandle<MLGraphXnnpack> graph,
      CrossThreadHandle<ScriptPromiseResolver> resolver,
      scoped_refptr<base::SequencedTaskRunner> resolver_task_runner);

  // Resolve the promise with an `MLComputeResult` on the thread owning this
  // `MLGraphXnnpack` object after invoking the XNNPACK Runtime object. The
  // input and output `ArrayBufferView`s of the `MLComputeResult` are created
  // from the `inputs_info` and `outputs_info` that carry the backing memory in
  // `ArrayBufferContents` transferred from the original user supplied
  // `ArrayBufferView`s.
  void OnDidCompute(xnn_status status,
                    NamedArrayBufferViewsInfoPtr inputs_info,
                    NamedArrayBufferViewsInfoPtr outputs_info,
                    ScriptPromiseResolver* resolver,
                    String error_message = String());

  // Invoke the XNNPACK Runtime object in the caller's thread.
  void ComputeSyncImpl(const MLNamedArrayBufferViews& inputs,
                       const MLNamedArrayBufferViews& outputs,
                       ExceptionState& exception_state) override;

  // XNNPACK Subgraph is an abstract representation of a neural network model.
  // This method first sorts the MLOperators by searching from `named_outputs`
  // and then creates an XNNPACK Subgraph object and defines Subgraph Nodes and
  // Values from the sorted operators. The output XNNPACK Subgraph
  // `out_subgraph` will be used to create an XNNPACK Runtime. Because the
  // static data buffers of XNNPACK Values should outlive Subgraph and Runtime
  // using them, they are returned in `out_static_data_buffers`. This method
  // must be called on the thread owning this `MLGraphXnnpack` and
  // `named_outputs`.
  xnn_status CreateXnnSubgraph(const MLNamedOperands& named_outputs,
                               XnnSubgraphPtr& out_subgraph,
                               Vector<DataBufferPtr>& out_static_data_buffers,
                               String& error_message);

  // This method creates the xnn_external_value vector from named input and
  // output array buffer views. The xnn_external_value vector is used to set
  // up the XNNPACK Runtime object. The returned vector is sorted by
  // `xnn_external_value::id`.
  XnnExternalValuesPtr CreateExternalValues(
      const MLNamedArrayBufferViews& inputs,
      const MLNamedArrayBufferViews& outputs) const;

  // Task runner for running XNNPACK time-consuming operations, e.g. library
  // initialization, Runtime creation and invcation.
  scoped_refptr<base::SequencedTaskRunner> xnnpack_task_runner_;

  // Task runner to resolve promises on.
  scoped_refptr<base::SequencedTaskRunner> resolver_task_runner_;

  // Map the names of the MLGraph's inputs/outputs to the XNNPACK external Value
  // IDs. They will be used to set up the xnn_external_value structures from the
  // input/output named array buffer views when invoking the XNNPACK Runtime
  // object for the MLGraph compute.
  ExternalValueIdMap input_external_value_id_map_;
  ExternalValueIdMap output_external_value_id_map_;

  // The implementation wraps the objects for accelerated executions including
  // XNNPACK Runtime object, shared XNNPACK context, static data buffers of
  // XNNPACK Values and the vector of xnn_external_value for XNNPACK Runtime
  // setup and invocation.
  scoped_refptr<XnnRuntimeWrapper> xnn_runtime_wrapper_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_XNNPACK_H_
