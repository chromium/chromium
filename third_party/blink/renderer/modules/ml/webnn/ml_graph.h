// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_H_

#include "base/types/pass_key.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/modules/ml/ml_trace.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"

namespace blink {

class MLBuffer;
class MLComputeResult;
class MLContext;
class MLGraphBuilder;
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

// Represents a handle to a compiled, platform-specific computational graph.
class MODULES_EXPORT MLGraph : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // The members of ResourceInfo are used to validate the inputs and outputs of
  // an MLGraph execution. The validation steps are described by WebNN spec of
  // the MLContext.compute() method:
  // https://www.w3.org/TR/webnn/#api-mlcontext-compute The plain struct
  // ResourceInfo is introduced instead of using MLOperandDescriptor because
  // neither byte length calculation from dimensions nor GC support is needed
  // for the implementation.
  //
  // TODO(crbug.com/325612086): Consider removing this struct in favor of
  // something like MLOperand::ValidatedDescriptor.
  struct ResourceInfo {
    V8MLOperandDataType::Enum data_type;
    size_t byte_length;
  };

  // Instances should only be constructed via `MLGraphBuilder.build()`.
  // This method is public as required by the `MakeGarbageCollected` helper.
  //
  // `pending_graph_remote` is a handle to the computational graph.
  // `input_resources_info` and `output_resources_info` describe the constraints
  // on the inputs and outputs which may be used to execute the respective
  // graph.
  MLGraph(ExecutionContext* execution_context,
          MLContext* context,
          mojo::PendingAssociatedRemote<webnn::mojom::blink::WebNNGraph>
              pending_graph_remote,
          HashMap<String, ResourceInfo> input_resources_info,
          HashMap<String, ResourceInfo> output_resources_info,
          base::PassKey<MLGraphBuilder> pass_key);

  MLGraph(const MLGraph&) = delete;
  MLGraph& operator=(const MLGraph&) = delete;

  ~MLGraph() override;

  void Trace(Visitor* visitor) const override;

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

  const HashMap<String, ResourceInfo> input_resources_info_;
  const HashMap<String, ResourceInfo> output_resources_info_;

  Member<MLContext> ml_context_;

  // The `WebNNGraph` is a compiled graph that can be executed by the hardware
  // accelerated OS machine learning API.
  HeapMojoAssociatedRemote<webnn::mojom::blink::WebNNGraph> remote_graph_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_H_
