// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_XNNPACK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_XNNPACK_H_

#include <optional>

#include "base/containers/heap_array.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/ml/ml_trace.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/xnnpack/src/include/xnnpack.h"

namespace blink {

namespace {
class SharedXnnpackContext;
class XnnRuntimeWrapper;
}

namespace internal {

// Hold static or input tensor data for XNNPACK Runtime.
using DataBuffer = base::HeapArray<uint8_t>;

using XnnSubgraphPtr =
    std::unique_ptr<xnn_subgraph, decltype(&xnn_delete_subgraph)>;
using XnnExternalValuesPtr = std::unique_ptr<Vector<xnn_external_value>>;

typedef Vector<std::pair<String, ArrayBufferViewInfo>>
    NamedArrayBufferViewsInfo;

}  // namespace internal

// XNNPACK requires the input tensor data to be padded with `XNN_EXTRA_BYTES`
// bytes for performance reasons. For each compute, `MLGraphXnnpack` allocates
// new input buffers with this many extra bytes, copies the input data from
// input array buffers to the new input buffers and feeds them to XNNPACK.
class MODULES_EXPORT MLGraphXnnpack final : public MLGraph {
 public:
  // Map the MLGraph's input or output name to the XNNPACK external Value ID.
  using ExternalValueIdMap = HashMap<String, uint32_t>;

  // Create and build an MLGraphXnnpack object. Resolve the promise with
  // this concrete object if the underlying XNNPACK subgraph builds
  // successfully.
  // The caller must call `Promise()` on `resolver` before calling this method.
  static void ValidateAndBuild(ScopedMLTrace scoped_trace,
                               MLContext* context,
                               const MLNamedOperands& named_outputs,
                               ScriptPromiseResolver<MLGraph>* resolver);

  // The constructor shouldn't be called directly. The callers should use the
  // `ValidateAndBuild()` method instead.
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
  //        `BuildImpl()`
  //              |
  //          post task ------->  `GetSharedXnnpackContextOnBackgroundThread()`
  //                                                  |
  //  `OnDidGetSharedXnnpackContext()`  <----------post task
  //              |
  //          post task ------------> `CreateXnnRuntimeOnBackgroundThread()`
  //                                                  |
  //   `OnDidCreateXnnRuntime()  <-----------------post task
  void BuildImpl(ScopedMLTrace scoped_trace,
                 const MLNamedOperands& named_outputs,
                 ScriptPromiseResolver<MLGraph>* resolver) override;

  static void GetSharedXnnpackContextOnBackgroundThread(
      ScopedMLTrace scoped_trace,
      CrossThreadHandle<MLGraphXnnpack> graph,
      CrossThreadHandle<MLNamedOperands> named_outputs,
      CrossThreadHandle<ScriptPromiseResolver<MLGraph>> resolver,
      scoped_refptr<base::SequencedTaskRunner> resolver_task_runner);

  void OnDidGetSharedXnnpackContext(
      ScopedMLTrace scoped_trace,
      scoped_refptr<SharedXnnpackContext> xnn_context,
      MLNamedOperands* named_outputs,
      ScriptPromiseResolver<MLGraph>* resolver,
      String error_message = String());

  static void CreateXnnRuntimeOnBackgroundThread(
      ScopedMLTrace scoped_trace,
      internal::XnnSubgraphPtr subgraph,
      scoped_refptr<SharedXnnpackContext> xnn_context,
      Vector<internal::DataBuffer> static_data_buffers,
      CrossThreadHandle<MLGraphXnnpack> graph,
      uint32_t num_threads,
      CrossThreadHandle<ScriptPromiseResolver<MLGraph>> resolver,
      scoped_refptr<base::SequencedTaskRunner> resolver_task_runner);

  void OnDidCreateXnnRuntime(
      ScopedMLTrace scoped_trace,
      scoped_refptr<XnnRuntimeWrapper> xnn_runtime_wrapper,
      ScriptPromiseResolver<MLGraph>* resolver,
      String error_message = String());

  // Post the XNNPACK Runtime object invocation to a background thread. The
  // input and output `MLNamedArrayBufferViews` will be transferred to the
  // `NamedArrayBufferViewsInfo` that are posted to background thread. This
  // would prevent the calling thread from modifying the contents of the
  // `ArrayBufferView` while the background thread is accessing them. And it
  // would also avoid accessing the heap-allocated `ArrayBufferView` in the
  // background thread.
  void ComputeImpl(ScopedMLTrace scoped_trace,
                   const MLNamedArrayBufferViews& inputs,
                   const MLNamedArrayBufferViews& outputs,
                   ScriptPromiseResolver<MLComputeResult>* resolver,
                   ExceptionState& exception_state) override;

  // Invoking an XNNPACK Runtime object can be time-consuming. Calling this
  // method in a background thread avoids blocking the main thread. The
  // ownership of `input_buffers`, `external_values`, `inputs_info` and
  // `outputs_info` is transferred to the background thread that invokes the
  // XNNPACK Runtime object with the input and output buffers. The GC objects
  // wrapped by `CrossThreadHandle` must not be accessed by this method and
  // should be passed forward to `OnDidCompute()` which is called on the thread
  // owning these GC objects.
  static void ComputeOnBackgroundThread(
      ScopedMLTrace scoped_trace,
      scoped_refptr<XnnRuntimeWrapper> xnn_runtime_wrapper,
      Vector<internal::DataBuffer> input_buffers,
      internal::XnnExternalValuesPtr external_values,
      std::unique_ptr<internal::NamedArrayBufferViewsInfo> inputs_info,
      std::unique_ptr<internal::NamedArrayBufferViewsInfo> outputs_info,
      CrossThreadHandle<MLGraphXnnpack> graph,
      CrossThreadHandle<ScriptPromiseResolver<MLComputeResult>> resolver,
      scoped_refptr<base::SequencedTaskRunner> resolver_task_runner);

  // Resolve the promise with an `MLComputeResult` on the thread owning this
  // `MLGraphXnnpack` object after invoking the XNNPACK Runtime object. The
  // input and output `ArrayBufferView`s of the `MLComputeResult` are created
  // from the `inputs_info` and `outputs_info` that carry the backing memory in
  // `ArrayBufferContents` transferred from the original user supplied
  // `ArrayBufferView`s.
  void OnDidCompute(
      ScopedMLTrace scoped_trace,
      xnn_status status,
      std::unique_ptr<internal::NamedArrayBufferViewsInfo> inputs_info,
      std::unique_ptr<internal::NamedArrayBufferViewsInfo> outputs_info,
      ScriptPromiseResolver<MLComputeResult>* resolver,
      String error_message = String());

  // XNNPACK Subgraph is an abstract representation of a neural network model.
  // This method first sorts the MLOperators by searching from `named_outputs`
  // and then creates an XNNPACK Subgraph object and defines Subgraph Nodes and
  // Values from the sorted operators. The output XNNPACK Subgraph
  // `out_subgraph` will be used to create an XNNPACK Runtime. Because the
  // static data buffers of XNNPACK Values should outlive Subgraph and Runtime
  // using them, they are returned in `out_static_data_buffers`. This method
  // must be called on the thread owning this `MLGraphXnnpack` and
  // `named_outputs`.
  xnn_status CreateXnnSubgraph(
      const MLNamedOperands& named_outputs,
      internal::XnnSubgraphPtr& out_subgraph,
      Vector<internal::DataBuffer>& out_static_data_buffers,
      String& error_message);

  // This method creates the xnn_external_value vector from named input and
  // output array buffer views. The xnn_external_value vector is used to set
  // up the XNNPACK Runtime object. The returned vector is sorted by
  // `xnn_external_value::id`.
  //
  // XNNPACK requires input buffers to have additional `XNN_EXTRA_BYTES` bytes
  // at the end (for performance reasons). To prevent out-of-bounds read, this
  // method allocates the input buffers with `XNN_EXTRA_BYTES` extra bytes,
  // copies the input data from array buffers into the newly allocated buffers
  // and uses them to setup input `xnn_external_value::data`. The newly
  // allocated input buffers should be kept alive until XNNPACK Runtime
  // invocation completes.
  //
  // XNNPACK won't write beyond the end of output buffers, so the provided
  // outputs are used as-is.
  std::optional<
      std::pair<internal::XnnExternalValuesPtr, Vector<internal::DataBuffer>>>
  CreateExternalValues(const MLNamedArrayBufferViews& inputs,
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
