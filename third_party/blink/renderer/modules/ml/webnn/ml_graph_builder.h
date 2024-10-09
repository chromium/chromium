// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_H_

#include "base/types/expected.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_data_type.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"

namespace blink {

class ExceptionState;
class MLArgMinMaxOptions;
class MLBatchNormalizationOptions;
class MLContext;
class MLClampOptions;
class MLConstantOperand;
class MLConv2dOptions;
class MLConvTranspose2dOptions;
class MLCumulativeSumOptions;
class MLEluOptions;
class MLGatherOptions;
class MLGemmOptions;
class MLGruOptions;
class MLGruCellOptions;
class MLGraph;
class MLHardSigmoidOptions;
class MLInstanceNormalizationOptions;
class MLLayerNormalizationOptions;
class MLLeakyReluOptions;
class MLLinearOptions;
class MLLstmOptions;
class MLLstmCellOptions;
class MLOperatorOptions;
class MLPadOptions;
class MLPool2dOptions;
class MLReduceOptions;
class MLResample2dOptions;
class MLSplitOptions;
class MLTransposeOptions;
class MLTriangularOptions;
class MLOperand;
class MLOperandDescriptor;
class ScriptState;

typedef HeapVector<std::pair<String, Member<MLOperand>>> MLNamedOperands;

class MODULES_EXPORT MLGraphBuilder final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MLGraphBuilder* Create(ScriptState* script_state,
                                MLContext* context,
                                ExceptionState& exception_state);

  explicit MLGraphBuilder(
      ExecutionContext* execution_context,
      MLContext* context,
      mojo::PendingAssociatedRemote<webnn::mojom::blink::WebNNGraphBuilder>
          pending_remote);

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
  MLOperand* input(ScriptState* script_state,
                   String name,
                   const MLOperandDescriptor* desc,
                   ExceptionState& exception_state);
  MLOperand* constant(ScriptState* script_state,
                      const MLOperandDescriptor* desc,
                      NotShared<DOMArrayBufferView> buffer_view,
                      ExceptionState& exception_state);

  // The order of operations declaration is the same as spec.
  MLOperand* argMin(const MLOperand* input,
                    const uint32_t axis,
                    const MLArgMinMaxOptions* options,
                    ExceptionState& exception_state);
  MLOperand* argMax(const MLOperand* input,
                    const uint32_t axis,
                    const MLArgMinMaxOptions* options,
                    ExceptionState& exception_state);

  MLOperand* batchNormalization(const MLOperand* input,
                                const MLOperand* mean,
                                const MLOperand* variance,
                                const MLBatchNormalizationOptions* options,
                                ExceptionState& exception_state);

  MLOperand* clamp(const MLOperand* input,
                   const MLClampOptions* options,
                   ExceptionState& exception_state);

  MLOperand* concat(const HeapVector<Member<MLOperand>>& inputs,
                    const uint32_t axis,
                    const MLOperatorOptions* options,
                    ExceptionState& exception_state);

  MLOperand* conv2d(const MLOperand* input,
                    const MLOperand* filter,
                    const MLConv2dOptions* options,
                    ExceptionState& exception_state);

  MLOperand* convTranspose2d(const MLOperand* input,
                             const MLOperand* filter,
                             const MLConvTranspose2dOptions* options,
                             ExceptionState& exception_state);

  MLOperand* cumulativeSum(const MLOperand* input,
                           const uint32_t axis,
                           const MLCumulativeSumOptions* options,
                           ExceptionState& exception_state);

  // Element-wise binary operations
  MLOperand* add(const MLOperand* a,
                 const MLOperand* b,
                 const MLOperatorOptions* options,
                 ExceptionState& exception_state);
  MLOperand* sub(const MLOperand* a,
                 const MLOperand* b,
                 const MLOperatorOptions* options,
                 ExceptionState& exception_state);
  MLOperand* mul(const MLOperand* a,
                 const MLOperand* b,
                 const MLOperatorOptions* options,
                 ExceptionState& exception_state);
  MLOperand* div(const MLOperand* a,
                 const MLOperand* b,
                 const MLOperatorOptions* options,
                 ExceptionState& exception_state);
  MLOperand* max(const MLOperand* a,
                 const MLOperand* b,
                 const MLOperatorOptions* options,
                 ExceptionState& exception_state);
  MLOperand* min(const MLOperand* a,
                 const MLOperand* b,
                 const MLOperatorOptions* options,
                 ExceptionState& exception_state);
  MLOperand* pow(const MLOperand* a,
                 const MLOperand* b,
                 const MLOperatorOptions* options,
                 ExceptionState& exception_state);
  MLOperand* equal(const MLOperand* a,
                   const MLOperand* b,
                   const MLOperatorOptions* options,
                   ExceptionState& exception_state);
  MLOperand* greater(const MLOperand* a,
                     const MLOperand* b,
                     const MLOperatorOptions* options,
                     ExceptionState& exception_state);
  MLOperand* greaterOrEqual(const MLOperand* a,
                            const MLOperand* b,
                            const MLOperatorOptions* options,
                            ExceptionState& exception_state);
  MLOperand* lesser(const MLOperand* a,
                    const MLOperand* b,
                    const MLOperatorOptions* options,
                    ExceptionState& exception_state);
  MLOperand* lesserOrEqual(const MLOperand* a,
                           const MLOperand* b,
                           const MLOperatorOptions* options,
                           ExceptionState& exception_state);
  MLOperand* logicalAnd(const MLOperand* a,
                        const MLOperand* b,
                        const MLOperatorOptions* options,
                        ExceptionState& exception_state);
  MLOperand* logicalOr(const MLOperand* a,
                       const MLOperand* b,
                       const MLOperatorOptions* options,
                       ExceptionState& exception_state);
  MLOperand* logicalXor(const MLOperand* a,
                        const MLOperand* b,
                        const MLOperatorOptions* options,
                        ExceptionState& exception_state);

  // Element-wise unary operations
  MLOperand* abs(const MLOperand* input,
                 const MLOperatorOptions* options,
                 ExceptionState& exception_state);
  MLOperand* ceil(const MLOperand* input,
                  const MLOperatorOptions* options,
                  ExceptionState& exception_state);
  MLOperand* cos(const MLOperand* input,
                 const MLOperatorOptions* options,
                 ExceptionState& exception_state);
  MLOperand* exp(const MLOperand* input,
                 const MLOperatorOptions* options,
                 ExceptionState& exception_state);
  MLOperand* floor(const MLOperand* input,
                   const MLOperatorOptions* options,
                   ExceptionState& exception_state);
  MLOperand* log(const MLOperand* input,
                 const MLOperatorOptions* options,
                 ExceptionState& exception_state);
  MLOperand* neg(const MLOperand* input,
                 const MLOperatorOptions* options,
                 ExceptionState& exception_state);
  MLOperand* sign(const MLOperand* input,
                  const MLOperatorOptions* options,
                  ExceptionState& exception_state);
  MLOperand* sin(const MLOperand* input,
                 const MLOperatorOptions* options,
                 ExceptionState& exception_state);
  MLOperand* tan(const MLOperand* input,
                 const MLOperatorOptions* options,
                 ExceptionState& exception_state);
  MLOperand* erf(const MLOperand* input,
                 const MLOperatorOptions* options,
                 ExceptionState& exception_state);
  MLOperand* identity(const MLOperand* input,
                      const MLOperatorOptions* options,
                      ExceptionState& exception_state);
  MLOperand* logicalNot(const MLOperand* input,
                        const MLOperatorOptions* options,
                        ExceptionState& exception_state);
  MLOperand* reciprocal(const MLOperand* input,
                        const MLOperatorOptions* options,
                        ExceptionState& exception_state);
  MLOperand* sqrt(const MLOperand* input,
                  const MLOperatorOptions* options,
                  ExceptionState& exception_state);

  MLOperand* cast(const MLOperand* input,
                  const V8MLOperandDataType output_data_type,
                  const MLOperatorOptions* options,
                  ExceptionState& exception_state);

  MLOperand* dequantizeLinear(const MLOperand* input,
                              const MLOperand* scale,
                              const MLOperand* zeroPoint,
                              const MLOperatorOptions* options,
                              ExceptionState& exception_state);

  MLOperand* elu(const MLOperand* input,
                 const MLEluOptions* options,
                 ExceptionState& exception_state);

  MLOperand* expand(const MLOperand* input,
                    const Vector<uint32_t>& new_shape,
                    const MLOperatorOptions* options,
                    ExceptionState& exception_state);

  MLOperand* gather(const MLOperand* input,
                    const MLOperand* indices,
                    const MLGatherOptions* options,
                    ExceptionState& exception_state);

  MLOperand* gatherElements(const MLOperand* input,
                            const MLOperand* indices,
                            const MLGatherOptions* options,
                            ExceptionState& exception_state);

  MLOperand* gatherND(const MLOperand* input,
                      const MLOperand* indices,
                      const MLOperatorOptions* options,
                      ExceptionState& exception_state);

  MLOperand* gelu(const MLOperand* input,
                  const MLOperatorOptions* options,
                  ExceptionState& exception_state);

  MLOperand* gemm(const MLOperand* a,
                  const MLOperand* b,
                  const MLGemmOptions* options,
                  ExceptionState& exception_state);

  HeapVector<Member<const MLOperand>> gru(const MLOperand* input,
                                          const MLOperand* weight,
                                          const MLOperand* recurrent_weight,
                                          const uint32_t steps,
                                          const uint32_t hidden_size,
                                          MLGruOptions* options,
                                          ExceptionState& exception_state);

  MLOperand* gruCell(const MLOperand* input,
                     const MLOperand* weight,
                     const MLOperand* recurrent_weight,
                     const MLOperand* hidden_state,
                     const uint32_t hidden_size,
                     MLGruCellOptions* options,
                     ExceptionState& exception_state);

  MLOperand* hardSigmoid(const MLOperand* input,
                         const MLHardSigmoidOptions* options,
                         ExceptionState& exception_state);

  MLOperand* hardSwish(const MLOperand* input,
                       const MLOperatorOptions* options,
                       ExceptionState& exception_state);

  MLOperand* instanceNormalization(
      const MLOperand* input,
      const MLInstanceNormalizationOptions* options,
      ExceptionState& exception_state);

  MLOperand* layerNormalization(const MLOperand* input,
                                const MLLayerNormalizationOptions* options,
                                ExceptionState& exception_state);

  MLOperand* leakyRelu(const MLOperand* input,
                       const MLLeakyReluOptions* options,
                       ExceptionState& exception_state);

  MLOperand* linear(const MLOperand* input,
                    const MLLinearOptions* options,
                    ExceptionState& exception_state);

  HeapVector<Member<const MLOperand>> lstm(const MLOperand* input,
                                           const MLOperand* weight,
                                           const MLOperand* recurrent_weight,
                                           const uint32_t steps,
                                           const uint32_t hidden_size,
                                           MLLstmOptions* options,
                                           ExceptionState& exception_state);

  HeapVector<Member<const MLOperand>> lstmCell(
      const MLOperand* input,
      const MLOperand* weight,
      const MLOperand* recurrent_weight,
      const MLOperand* hidden_state,
      const MLOperand* cell_state,
      uint32_t hidden_size,
      MLLstmCellOptions* options,
      ExceptionState& exception_state);

  MLOperand* matmul(const MLOperand* a,
                    const MLOperand* b,
                    const MLOperatorOptions* options,
                    ExceptionState& exception_state);

  MLOperand* pad(ScriptState* script_state,
                 const MLOperand* input,
                 const Vector<uint32_t>& beginningPadding,
                 const Vector<uint32_t>& endingPadding,
                 const MLPadOptions* options,
                 ExceptionState& exception_state);

  // Pooling operations
  MLOperand* averagePool2d(const MLOperand* input,
                           const MLPool2dOptions* options,
                           ExceptionState& exception_state);
  MLOperand* l2Pool2d(const MLOperand* input,
                      const MLPool2dOptions* options,
                      ExceptionState& exception_state);
  MLOperand* maxPool2d(const MLOperand* input,
                       const MLPool2dOptions* options,
                       ExceptionState& exception_state);

  MLOperand* prelu(const MLOperand* input,
                   const MLOperand* slope,
                   const MLOperatorOptions* options,
                   ExceptionState& exception_state);

  MLOperand* quantizeLinear(const MLOperand* input,
                            const MLOperand* scale,
                            const MLOperand* zeroPoint,
                            const MLOperatorOptions* options,
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

  MLOperand* relu(const MLOperand* input,
                  const MLOperatorOptions* options,
                  ExceptionState& exception_state);

  MLOperand* reshape(const MLOperand* input,
                     const Vector<uint32_t>& new_shape,
                     const MLOperatorOptions* options,
                     ExceptionState& exception_state);

  MLOperand* resample2d(ScriptState* script_state,
                        const MLOperand* input,
                        const MLResample2dOptions* options,
                        ExceptionState& exception_state);

  MLOperand* scatterND(const MLOperand* input,
                       const MLOperand* indices,
                       const MLOperand* updates,
                       const MLOperatorOptions* options,
                       ExceptionState& exception_state);

  MLOperand* sigmoid(const MLOperand* input,
                     const MLOperatorOptions* options,
                     ExceptionState& exception_state);

  MLOperand* slice(const MLOperand* input,
                   const Vector<uint32_t>& starts,
                   const Vector<uint32_t>& sizes,
                   const MLOperatorOptions* options,
                   ExceptionState& exception_state);

  MLOperand* softmax(const MLOperand* input,
                     uint32_t axis,
                     const MLOperatorOptions* options,
                     ExceptionState& exception_state);
  MLOperand* softmax(const MLOperand* input,
                     const MLOperatorOptions* options,
                     ExceptionState& exception_state);

  MLOperand* softplus(const MLOperand* input,
                      const MLOperatorOptions* options,
                      ExceptionState& exception_state);

  MLOperand* softsign(const MLOperand* input,
                      const MLOperatorOptions* options,
                      ExceptionState& exception_state);

  HeapVector<Member<const MLOperand>> split(const MLOperand* input,
                                            const uint32_t splits,
                                            const MLSplitOptions* options,
                                            ExceptionState& exception_state);
  HeapVector<Member<const MLOperand>> split(const MLOperand* input,
                                            const Vector<uint32_t>& splits,
                                            const MLSplitOptions* options,
                                            ExceptionState& exception_state);

  MLOperand* tanh(const MLOperand* input,
                  const MLOperatorOptions* options,
                  ExceptionState& exception_state);

  MLOperand* tile(const MLOperand* input,
                  const Vector<uint32_t>& repetitions,
                  const MLOperatorOptions* options,
                  ExceptionState& exception_state);

  MLOperand* transpose(const MLOperand* input,
                       const MLTransposeOptions* options,
                       ExceptionState& exception_state);

  MLOperand* triangular(const MLOperand* input,
                        const MLTriangularOptions* options,
                        ExceptionState& exception_state);

  MLOperand* where(const MLOperand* condition,
                   const MLOperand* true_value,
                   const MLOperand* false_value,
                   const MLOperatorOptions* options,
                   ExceptionState& exception_state);

  ScriptPromise<MLGraph> build(ScriptState* script_state,
                               const MLNamedOperands& outputs,
                               ExceptionState& exception_state);

  void OnConnectionError();

 private:
  void DidCreateWebNNGraph(
      ScriptPromiseResolver<blink::MLGraph>* resolver,
      std::pair<MLGraph::NamedOperandDescriptors,
                MLGraph::NamedOperandDescriptors> input_and_output_constraints,
      webnn::mojom::blink::CreateGraphResultPtr result);

  // Check whether the graph builder is in an invalid state when the
  // `has_built_` is true or the `remote_` is unbound due to context lost. It
  // must be run for each method of the graph builder.
  [[nodiscard]] base::expected<void, String> ValidateGraphBuilderState() const;

  // Performs platform-agnostic and operand-agnostic validation checks which
  // must be run for each built operand. Returns an error message which may be
  // used to throw a TypeError if `input` is not valid to use with this builder.
  [[nodiscard]] base::expected<void, String> ValidateInput(
      const MLOperand* input);
  // Convenience method to validate several inputs at once.
  [[nodiscard]] base::expected<void, String> ValidateInputs(
      const HeapVector<Member<const MLOperand>>& inputs);

  // Releases the memory held by all constant operands associated with this
  // builder. This should be called when the builder is no longer able to make a
  // graph, to avoid keeping this data around unnecessarily.
  void ReleaseConstantData();

  Member<MLContext> ml_context_;

  HeapMojoAssociatedRemote<webnn::mojom::blink::WebNNGraphBuilder> remote_;

  // Tracks whether `build()` has been called (with valid inputs). If so, `this`
  // is effectively invalid and all methods should reject.
  bool has_built_ = false;

  // Tracks all the constant operands created by this builder. The constant data
  // owned by these operands will be copied to the remote graph builder
  // when `build()` is called, then can be released.
  //
  // TODO(crbug.com/349428379): Consider eagerly transferring constant data
  // rather than waiting until build().
  HeapVector<Member<MLConstantOperand>> constant_operands_;

  // Keep the unresolved `ScriptPromiseResolver` which will be rejected when the
  // Mojo pipe is unexpectedly disconnected.
  Member<ScriptPromiseResolver<MLGraph>> pending_resolver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_H_
