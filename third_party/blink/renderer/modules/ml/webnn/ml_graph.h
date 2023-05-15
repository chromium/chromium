// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

class MLContext;
class ScriptPromiseResolver;

// Implement the MLNamedArrayBufferViews type definition of WebNN spec:
// https://www.w3.org/TR/webnn/#typedefdef-mlnamedarraybufferviews
typedef HeapVector<std::pair<String, NotShared<DOMArrayBufferView>>>
    MLNamedArrayBufferViews;

class MODULES_EXPORT MLGraph : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  MLGraph(const MLGraph&) = delete;
  MLGraph& operator=(const MLGraph&) = delete;

  ~MLGraph() override;

  void Trace(Visitor* visitor) const override;

  // The members of ResourceInfo are used to validate the inputs and outputs of
  // an MLGraph execution. The validation steps are described by WebNN spec of
  // MLContext.compute() and MLContext.computeSync() methods:
  // https://www.w3.org/TR/webnn/#api-mlcontext-async-execution
  // https://www.w3.org/TR/webnn/#api-mlcontext-sync-execution
  // The plain struct ResourceInfo is introduced instead of using
  // MLOperandDescriptor because neither byte length calculation from dimensions
  // nor GC support is needed for the implementation.
  struct ResourceInfo {
    V8MLOperandType::Enum type;
    size_t byte_length;
  };
  const HashMap<String, ResourceInfo>& GetInputResourcesInfo() const;
  const HashMap<String, ResourceInfo>& GetOutputResourcesInfo() const;

  // This method validates the input and output MLNamedArrayBufferViews against
  // the graph's input and output resources info. If there are no errors, it
  // transfers the input and output ArrayBufferViews to new ones and passes them
  // ones to ComputeAsyncImpl() implemented by an MLGraph backend that binds the
  // array buffer views and executes the compiled platform graph. This method is
  // called by MLContext to implement MLContext.compute() method.
  void ComputeAsync(const MLNamedArrayBufferViews& inputs,
                    const MLNamedArrayBufferViews& outputs,
                    ScriptPromiseResolver* resolver,
                    ExceptionState& exception_state);

  // ComputeSync() has the similar function as ComputeAsync(). The difference is
  // if there are no validation errors, it calls ComputeSyncImpl() implemented
  // by an MLGraph backend that binds the array buffer views and executes the
  // compiled platform graph synchronously in the caller's thread. This method
  // is called by MLContext to implement MLContext.computeSync() method.
  void ComputeSync(const MLNamedArrayBufferViews& inputs,
                   const MLNamedArrayBufferViews& outputs,
                   ExceptionState& exception_state);

  const MLContext* Context() const;

 protected:
  explicit MLGraph(MLContext* context);

  // BuildAsync() should be called right after constructing a concrete
  // MLGraph object. BuildAsync() validates the named outputs and initializes
  // the input and output resources info. If there are no errors, it calls
  // BuildAsyncImpl() implemented by an MLGraph backend that builds the platform
  // specific graph.
  void BuildAsync(const MLNamedOperands& named_outputs,
                  ScriptPromiseResolver* resolver);

  // An MLGraph backend should implement this method to build and compile a
  // platform specific graph asynchronously. The actual graph construction and
  // compilation work should be handled by a worker thread without blocking the
  // main thread. Once the platform graph is compiled, the resolver should be
  // resolved with a concrete MLGraph object. Otherwise, the resolver should be
  // rejected with a DOMException accordingly.
  virtual void BuildAsyncImpl(const MLNamedOperands& outputs,
                              ScriptPromiseResolver* resolver) = 0;

  // BuildSync() has the similar function as BuildAsync() and should also be
  // called right after constructing a concrete MLGraph object. The difference
  // is if there are no validation errors, it calls BuildSyncImpl() implemented
  // by an MLGraph backend that builds the platform specific graph in the
  // caller's thread synchronously.
  MLGraph* BuildSync(const MLNamedOperands& named_outputs,
                     ExceptionState& exception_state);

  // An MLGraph backend should implement this method to build and compile a
  // platform specific graph synchronously in the caller's thread. Once the
  // platform graph is compiled, it should return a concrete MLGraph object.
  // Otherwise, it should return a nullptr and throw a DOMException accordingly.
  virtual MLGraph* BuildSyncImpl(const MLNamedOperands& named_outputs,
                                 ExceptionState& exception_state) = 0;

  // An MLGraph backend should implement this method to execute the compiled
  // platform graph asynchronously. The actual graph execution work should be
  // handled by a worker thread without blocking the main thread.
  //
  // The implementation should transfer the input and output
  // `MLNamedArrayBufferViews` to new views that share the same backing memory
  // allocations.
  //
  // If compute is successful, the results will be stored in output buffers and
  // the resolver will be resolved with an MLComputeResult that contains the
  // input and output buffers. Otherwise, the resolver will be rejected with a
  // DOMException accordingly.
  virtual void ComputeAsyncImpl(const MLNamedArrayBufferViews& inputs,
                                const MLNamedArrayBufferViews& outputs,
                                ScriptPromiseResolver* resolver,
                                ExceptionState& exception_state) = 0;

  // An MLGraph backend should implement this method to execute the compiled
  // platform graph synchronously in the caller's thread. Results will be stored
  // in output buffers if no errors occurred. Otherwise, this method will throw
  // a DOMException accordingly.
  virtual void ComputeSyncImpl(const MLNamedArrayBufferViews& inputs,
                               const MLNamedArrayBufferViews& outputs,
                               ExceptionState& exception_state) = 0;

  Member<MLContext> ml_context_;
  bool resources_info_initialized_{false};
  HashMap<String, ResourceInfo> input_resources_info_;
  HashMap<String, ResourceInfo> output_resources_info_;

 private:
  // This helper method is called by BuildAsync(). It validates named outputs
  // and initializes the input and output resources info by graph traversal.
  bool ValidateAndInitializeResourcesInfo(const MLNamedOperands& named_outputs,
                                          String& error_message);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_H_
