// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_H_

#include "base/types/expected.h"
#include "components/ml/webnn/graph_validation_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_auto_pad.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_data_type.h"
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
class MLActivation;
class MLBatchNormalizationOptions;
class MLContext;
class MLClampOptions;
class MLConv2dOptions;
class MLConvTranspose2dOptions;
class MLEluOptions;
class MLGatherOptions;
class MLGemmOptions;
class MLGraph;
class MLLeakyReluOptions;
class MLPadOptions;
class MLPool2dOptions;
class MLReduceOptions;
class MLResample2dOptions;
class MLSplitOptions;
class MLTransposeOptions;
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

  struct Size2D {
    uint32_t height;
    uint32_t width;
  };

  // ml_graph_builder.idl
  MLOperand* input(String name,
                   const MLOperandDescriptor* desc,
                   ExceptionState& exception_state);
  MLOperand* constant(const MLOperandDescriptor* desc,
                      NotShared<DOMArrayBufferView> buffer_view,
                      ExceptionState& exception_state);

  // The order of operations declaration is the same as spec.
  MLOperand* batchNormalization(const MLOperand* input,
                                const MLOperand* mean,
                                const MLOperand* variance,
                                const MLBatchNormalizationOptions* options,
                                ExceptionState& exception_state);

  MLOperand* clamp(const MLOperand* input,
                   const MLClampOptions* options,
                   ExceptionState& exception_state);
  MLActivation* clamp(const MLClampOptions* options,
                      ExceptionState& exception_state);

  MLOperand* concat(const HeapVector<Member<MLOperand>>& inputs,
                    const uint32_t axis,
                    ExceptionState& exception_state);

  MLOperand* conv2d(const MLOperand* input,
                    const MLOperand* filter,
                    const MLConv2dOptions* options,
                    ExceptionState& exception_state);

  MLOperand* convTranspose2d(const MLOperand* input,
                             const MLOperand* filter,
                             const MLConvTranspose2dOptions* options,
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
  MLOperand* pow(const MLOperand* a,
                 const MLOperand* b,
                 ExceptionState& exception_state);

  // Element-wise unary operations
  MLOperand* abs(const MLOperand* input, ExceptionState& exception_state);
  MLOperand* ceil(const MLOperand* input, ExceptionState& exception_state);
  MLOperand* cos(const MLOperand* input, ExceptionState& exception_state);
  MLOperand* exp(const MLOperand* input, ExceptionState& exception_state);
  MLOperand* floor(const MLOperand* input, ExceptionState& exception_state);
  MLOperand* log(const MLOperand* input, ExceptionState& exception_state);
  MLOperand* neg(const MLOperand* input, ExceptionState& exception_state);
  MLOperand* sin(const MLOperand* input, ExceptionState& exception_state);
  MLOperand* tan(const MLOperand* input, ExceptionState& exception_state);
  MLOperand* erf(const MLOperand* input, ExceptionState& exception_state);
  MLOperand* identity(const MLOperand* input, ExceptionState& exception_state);
  MLOperand* logicalNot(const MLOperand* input,
                        ExceptionState& exception_state);
  MLOperand* reciprocal(const MLOperand* input,
                        ExceptionState& exception_state);
  MLOperand* sqrt(const MLOperand* input, ExceptionState& exception_state);

  MLOperand* cast(const MLOperand* input,
                  const V8MLOperandDataType output_data_type,
                  ExceptionState& exception_state);

  MLOperand* elu(const MLOperand* input,
                 const MLEluOptions* options,
                 ExceptionState& exception_state);
  MLActivation* elu(const MLEluOptions* options,
                    ExceptionState& exception_state);

  MLOperand* expand(const MLOperand* input,
                    const Vector<uint32_t>& new_shape,
                    ExceptionState& exception_state);

  MLOperand* gather(const MLOperand* input,
                    const MLOperand* indices,
                    const MLGatherOptions* options,
                    ExceptionState& exception_state);

  MLOperand* gemm(const MLOperand* a,
                  const MLOperand* b,
                  const MLGemmOptions* options,
                  ExceptionState& exception_state);

  MLOperand* hardSwish(const MLOperand* input, ExceptionState& exception_state);
  MLActivation* hardSwish(ExceptionState& exception_state);

  MLOperand* leakyRelu(const MLOperand* input,
                       const MLLeakyReluOptions* options,
                       ExceptionState& exception_state);
  MLActivation* leakyRelu(const MLLeakyReluOptions* options,
                          ExceptionState& exception_state);

  MLOperand* matmul(const MLOperand* a,
                    const MLOperand* b,
                    ExceptionState& exception_state);

  MLOperand* pad(const MLOperand* input,
                 const Vector<uint32_t>& beginningPadding,
                 const Vector<uint32_t>& endingPadding,
                 const MLPadOptions* options,
                 ExceptionState& exception_state);

  // Pooling operations
  MLOperand* averagePool2d(const MLOperand* input,
                           const MLPool2dOptions* options,
                           ExceptionState& exception_state);
  MLOperand* maxPool2d(const MLOperand* input,
                       const MLPool2dOptions* options,
                       ExceptionState& exception_state);

  MLOperand* prelu(const MLOperand* input,
                   const MLOperand* slope,
                   ExceptionState& exception_state);

  // Reduction operations
  MLOperand* reduceL1(const MLOperand* input,
                      const MLReduceOptions* options,
                      ExceptionState& exception_state);
  MLOperand* reduceL2(const MLOperand* input,
                      const MLReduceOptions* options,
                      ExceptionState& exception_state);
  MLOperand* reduceLogSum(const MLOperand* input,
                          const MLReduceOptions* options,
                          ExceptionState& exception_state);
  MLOperand* reduceLogSumExp(const MLOperand* input,
                             const MLReduceOptions* options,
                             ExceptionState& exception_state);
  MLOperand* reduceMax(const MLOperand* input,
                       const MLReduceOptions* options,
                       ExceptionState& exception_state);
  MLOperand* reduceMean(const MLOperand* input,
                        const MLReduceOptions* options,
                        ExceptionState& exception_state);
  MLOperand* reduceMin(const MLOperand* input,
                       const MLReduceOptions* options,
                       ExceptionState& exception_state);
  MLOperand* reduceProduct(const MLOperand* input,
                           const MLReduceOptions* options,
                           ExceptionState& exception_state);
  MLOperand* reduceSum(const MLOperand* input,
                       const MLReduceOptions* options,
                       ExceptionState& exception_state);
  MLOperand* reduceSumSquare(const MLOperand* input,
                             const MLReduceOptions* options,
                             ExceptionState& exception_state);

  MLOperand* relu(const MLOperand* input, ExceptionState& exception_state);
  MLActivation* relu(ExceptionState& exception_state);

  MLOperand* reshape(const MLOperand* input,
                     const Vector<uint32_t>& new_shape,
                     ExceptionState& exception_state);

  MLOperand* resample2d(const MLOperand* input,
                        const MLResample2dOptions* options,
                        ExceptionState& exception_state);

  MLOperand* sigmoid(const MLOperand* input, ExceptionState& exception_state);
  MLActivation* sigmoid(ExceptionState& exception_state);

  MLOperand* slice(const MLOperand* input,
                   const Vector<uint32_t>& starts,
                   const Vector<uint32_t>& sizes,
                   ExceptionState& exception_state);

  MLOperand* softmax(const MLOperand* input, ExceptionState& exception_state);
  MLActivation* softmax(ExceptionState& exception_state);

  MLOperand* softsign(const MLOperand* input, ExceptionState& exception_state);
  MLActivation* softsign(ExceptionState& exception_state);

  HeapVector<Member<const MLOperand>> split(const MLOperand* input,
                                            const uint32_t splits,
                                            const MLSplitOptions* options,
                                            ExceptionState& exception_state);
  HeapVector<Member<const MLOperand>> split(const MLOperand* input,
                                            const Vector<uint32_t>& splits,
                                            const MLSplitOptions* options,
                                            ExceptionState& exception_state);

  MLOperand* tanh(const MLOperand* input, ExceptionState& exception_state);
  MLActivation* tanh(ExceptionState& exception_state);

  MLOperand* transpose(const MLOperand* input,
                       const MLTransposeOptions* options,
                       ExceptionState& exception_state);

  MLOperand* where(const MLOperand* condition,
                   const MLOperand* true_value,
                   const MLOperand* false_value,
                   ExceptionState& exception_state);

  ScriptPromise build(ScriptState* script_state,
                      const MLNamedOperands& outputs,
                      ExceptionState& exception_state);

  MLGraph* buildSync(ScriptState* script_state,
                     const MLNamedOperands& named_outputs,
                     ExceptionState& exception_state);

  // The test cases can override the graph building behavior by implementing
  // this class and setting its instance by SetBackendForTesting().
  class BackendForTesting {
   public:
    virtual void BuildGraphAsyncImpl(MLContext* context,
                                     const MLNamedOperands& named_outputs,
                                     ScriptPromiseResolver* resolver) = 0;

    virtual MLGraph* BuildGraphSyncImpl(ScriptState* script_state,
                                        MLContext* context,
                                        const MLNamedOperands& named_outputs,
                                        ExceptionState& exception_state) = 0;
  };

  static void SetBackendForTesting(BackendForTesting* backend_for_testing);

 private:
  Member<MLContext> ml_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_H_
