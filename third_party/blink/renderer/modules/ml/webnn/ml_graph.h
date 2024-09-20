// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_H_

#include "base/types/pass_key.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/modules/ml/ml_trace.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"

namespace blink {

class MLTensor;
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

typedef HeapVector<std::pair<String, Member<MLTensor>>> MLNamedTensors;

// Represents a handle to a compiled, platform-specific computational graph.
class MODULES_EXPORT MLGraph : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using NamedOperandDescriptors =
      HashMap<String, std::optional<webnn::OperandDescriptor>>;

  // Instances should only be constructed via `MLGraphBuilder.build()`.
  // This method is public as required by the `MakeGarbageCollected` helper.
  //
  // `pending_graph_remote` is a handle to the computational graph.
  // `input_constraints` and `output_constraints` describe the constraints on
  // the inputs and outputs which may be used to execute the respective graph.
  MLGraph(ExecutionContext* execution_context,
          MLContext* context,
          mojo::PendingAssociatedRemote<webnn::mojom::blink::WebNNGraph>
              pending_graph_remote,
          NamedOperandDescriptors input_constraints,
          NamedOperandDescriptors output_constraints,
          base::PassKey<MLGraphBuilder> pass_key);

  MLGraph(const MLGraph&) = delete;
  MLGraph& operator=(const MLGraph&) = delete;

  ~MLGraph() override;

  void Trace(Visitor* visitor) const override;

  // ml_graph.idl
  void destroy();

  const NamedOperandDescriptors& GetInputConstraints() const;
  const NamedOperandDescriptors& GetOutputConstraints() const;

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
  // This method validates the input and output MLNamedTensors against the
  // graph's input and output resources info and then executes the compiled
  // platform graph.
  void Dispatch(ScopedMLTrace scoped_trace,
                const MLNamedTensors& inputs,
                const MLNamedTensors& outputs,
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

  void OnConnectionError();

  // Describes the constraints on the inputs or outputs to this graph.
  // Note that `WTF::HashMap` values must be nullable, but
  // `webnn::OperandDescriptor` lacks a default constructor, so an optional is
  // used. Do not add std::nullopt values to these maps.
  const NamedOperandDescriptors input_constraints_;
  const NamedOperandDescriptors output_constraints_;

  Member<MLContext> ml_context_;

  // The `WebNNGraph` is a compiled graph that can be executed by the hardware
  // accelerated OS machine learning API.
  HeapMojoAssociatedRemote<webnn::mojom::blink::WebNNGraph> remote_graph_;

  // Keep a set of unresolved `ScriptPromiseResolver`s which will be
  // rejected when the Mojo pipe is unexpectedly disconnected.
  HeapHashSet<Member<ScriptPromiseResolver<MLComputeResult>>>
      pending_resolvers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_H_
