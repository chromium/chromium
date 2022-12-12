// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class ExceptionState;
class MLContext;
class MLClampOptions;
class MLConv2dOptions;
class MLGemmOptions;
class MLGraph;
class MLPool2dOptions;
class MLResample2dOptions;
class MLOperand;
class MLOperandDescriptor;
class ScriptPromiseResolver;

typedef HeapVector<std::pair<String, Member<MLOperand>>> MLNamedOperands;

class MODULES_EXPORT MLGraphBuilder final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MLGraphBuilder* Create(MLContext* context);

  explicit MLGraphBuilder(MLContext* context);

  MLGraphBuilder(const MLGraphBuilder&) = delete;
  MLGraphBuilder& operator=(const MLGraphBuilder&) = delete;

  ~MLGraphBuilder() override;

  void Trace(Visitor* visitor) const override;

  MLContext* GetContext() const;

  // ml_graph_builder.idl
  MLOperand* input(String name,
                   const MLOperandDescriptor* desc,
                   ExceptionState& exception_state);
  MLOperand* constant(const MLOperandDescriptor* desc,
                      NotShared<DOMArrayBufferView> buffer_view,
                      ExceptionState& exception_state);

  // The order of operations declaration is the same as spec.
  MLOperand* clamp(const MLOperand* input,
                   const MLClampOptions* options,
                   ExceptionState& exception_state);
  MLOperator* clamp(const MLClampOptions* options,
                    ExceptionState& exception_state);

  MLOperand* conv2d(const MLOperand* input,
                    const MLOperand* filter,
                    const MLConv2dOptions* options,
                    ExceptionState& exception_state);

  // Element-wise binary operations
  MLOperand* add(const MLOperand* a,
                 const MLOperand* b,
                 ExceptionState& exception_state);
  MLOperand* sub(const MLOperand* a,
                 const MLOperand* b,
                 ExceptionState& exception_state);
  MLOperand* mul(const MLOperand* a,
                 const MLOperand* b,
                 ExceptionState& exception_state);
  MLOperand* div(const MLOperand* a,
                 const MLOperand* b,
                 ExceptionState& exception_state);
  MLOperand* max(const MLOperand* a,
                 const MLOperand* b,
                 ExceptionState& exception_state);
  MLOperand* min(const MLOperand* a,
                 const MLOperand* b,
                 ExceptionState& exception_state);

  MLOperand* gemm(const MLOperand* a,
                  const MLOperand* b,
                  const MLGemmOptions* options,
                  ExceptionState& exception_state);

  MLOperand* hardSwish(const MLOperand* input, ExceptionState& exception_state);
  MLOperator* hardSwish(ExceptionState& exception_state);

  // Pooling operations
  MLOperand* averagePool2d(const MLOperand* input,
                           const MLPool2dOptions* options,
                           ExceptionState& exception_state);
  MLOperand* maxPool2d(const MLOperand* input,
                       const MLPool2dOptions* options,
                       ExceptionState& exception_state);

  MLOperand* relu(const MLOperand* input, ExceptionState& exception_state);
  MLOperator* relu(ExceptionState& exception_state);

  MLOperand* reshape(const MLOperand* input,
                     const Vector<int32_t>& new_shape,
                     ExceptionState& exception_state);

  MLOperand* resample2d(const MLOperand* input,
                        const MLResample2dOptions* options,
                        ExceptionState& exception_state);

  MLOperand* softmax(const MLOperand* input, ExceptionState& exception_state);

  MLOperand* sigmoid(const MLOperand* input, ExceptionState& exception_state);
  MLOperator* sigmoid(ExceptionState& exception_state);

  ScriptPromise buildAsync(ScriptState* script_state,
                           const MLNamedOperands& named_outputs,
                           ExceptionState& exception_state);

  // TODO(ningxin.hu@intel.com): Once the web-platform-tests are updated, add
  // MLGraphBuilder.buildSync() into ml_graph_builder.idl for dedicated worker
  // as WebNN spec: https://www.w3.org/TR/webnn/#dom-mlgraphbuilder-buildsync
  MLGraph* buildSync(const MLNamedOperands& named_outputs,
                     ExceptionState& exception_state);

  // The test cases can override the graph building behavior by implementing
  // this class and setting its instance by SetBackendForTesting().
  class BackendForTesting {
   public:
    virtual void BuildGraphAsyncImpl(MLContext* context,
                                     const MLNamedOperands& named_outputs,
                                     ScriptPromiseResolver* resolver) = 0;

    virtual MLGraph* BuildGraphSyncImpl(MLContext* context,
                                        const MLNamedOperands& named_outputs,
                                        ExceptionState& exception_state) = 0;
  };

  static void SetBackendForTesting(BackendForTesting* backend_for_testing);

 private:
  Member<MLContext> ml_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_H_
