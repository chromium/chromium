// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_H_

#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/modules/ml/ml_trace.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

class MLBuffer;
class MLComputeResult;
class MLContext;
class ExecutionContext;

// Stores information about a transferred `ArrayBufferView`. This struct doesn't
// include Blink GC objects, and can be accessed by any threads.
//
// The information is used to recreate `ArrayBufferView` when computation
// completes.
struct ArrayBufferViewInfo {
  ArrayBufferViewInfo() = default;
  ~ArrayBufferViewInfo() = default;

  ArrayBufferViewInfo(ArrayBufferViewInfo&& other) = default;
  ArrayBufferViewInfo& operator=(ArrayBufferViewInfo&& other) = default;

  ArrayBufferViewInfo(const ArrayBufferViewInfo&) = delete;
  ArrayBufferViewInfo& operator=(const ArrayBufferViewInfo&) = delete;

  DOMArrayBufferView::ViewType type;
  size_t offset;
  size_t length;
  ArrayBufferContents contents;
};

// Implement the MLNamedArrayBufferViews type definition of WebNN spec:
// https://www.w3.org/TR/webnn/#typedefdef-mlnamedarraybufferviews
typedef HeapVector<std::pair<String, NotShared<DOMArrayBufferView>>>
    MLNamedArrayBufferViews;

typedef HeapVector<std::pair<String, Member<MLBuffer>>> MLNamedBuffers;

class MODULES_EXPORT MLGraph : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Build and compile a platform specific graph corresponding to the operands
  // connected to `named_outputs`. If this succeeds, resolve `resolver` with an
  // `MLGraph` object corresponding to this compiled graph.
  //
  // The caller must call `Promise()` on `resolver` before calling this method.
  static void CreateAndBuild(ScopedMLTrace scoped_trace,
                             MLContext* context,
                             const MLNamedOperands& named_outputs,
                             ScriptPromiseResolver<MLGraph>* resolver);

  // The constructor shouldn't be called directly. The callers should use the
  // `CreateAndBuild()` method instead.
  MLGraph(ExecutionContext* execution_context, MLContext* context);

  MLGraph(const MLGraph&) = delete;
  MLGraph& operator=(const MLGraph&) = delete;

  ~MLGraph() override;

  void Trace(Visitor* visitor) const override;

  // The members of ResourceInfo are used to validate the inputs and outputs of
  // an MLGraph execution. The validation steps are described by WebNN spec of
  // the MLContext.compute() method:
  // https://www.w3.org/TR/webnn/#api-mlcontext-async-execution
  // The plain struct ResourceInfo is introduced instead of using
  // MLOperandDescriptor because neither byte length calculation from dimensions
  // nor GC support is needed for the implementation.
  struct ResourceInfo {
    V8MLOperandDataType::Enum data_type;
    size_t byte_length;
  };
  const HashMap<String, ResourceInfo>& GetInputResourcesInfo() const;
  const HashMap<String, ResourceInfo>& GetOutputResourcesInfo() const;

  // Execute the compiled platform graph asynchronously.
  //
  // This method validates the input and output MLNamedArrayBufferViews against
  // the graph's input and output resources info, transfers the input and output
  // ArrayBufferViews, and then executes the compiled platform graph.
  //
  // TODO(crbug.com/331351967): Remove this method in favor of `Dispatch()`.
  ScriptPromise<MLComputeResult> Compute(ScopedMLTrace scoped_trace,
                                         const MLNamedArrayBufferViews& inputs,
                                         const MLNamedArrayBufferViews& outputs,
                                         ScriptState* script_state,
                                         ExceptionState& exception_state);

  // Execute the compiled platform graph asynchronously.
  //
  // This method validates the input and output MLNamedBuffers against the
  // graph's input and output resources info and then executes the compiled
  // platform graph.
  void Dispatch(ScopedMLTrace scoped_trace,
                const MLNamedBuffers& inputs,
                const MLNamedBuffers& outputs,
                ExceptionState& exception_state);

  const MLContext* Context() const;

 private:
  // Validates named outputs and initializes the input and output resources info
  // by graph traversal.
  base::expected<void, String> ValidateAndInitializeResourcesInfo(
      const MLNamedOperands& named_outputs);

  void DidCompute(
      ScopedMLTrace scoped_trace,
      ScriptPromiseResolver<MLComputeResult>* resolver,
      std::unique_ptr<Vector<std::pair<String, ArrayBufferViewInfo>>>
          inputs_info,
      std::unique_ptr<Vector<std::pair<String, ArrayBufferViewInfo>>>
          outputs_info,
      webnn::mojom::blink::ComputeResultPtr mojo_result);

  void DidCreateWebNNGraph(ScopedMLTrace scoped_trace,
                           ScriptPromiseResolver<MLGraph>* resolver,
                           webnn::mojom::blink::CreateGraphResultPtr result);

  Member<MLContext> ml_context_;

  // The `WebNNGraph` is a compiled graph that can be executed by the hardware
  // accelerated OS machine learning API.
  HeapMojoAssociatedRemote<webnn::mojom::blink::WebNNGraph> remote_graph_;

  bool resources_info_initialized_{false};
  HashMap<String, ResourceInfo> input_resources_info_;
  HashMap<String, ResourceInfo> output_resources_info_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_H_
