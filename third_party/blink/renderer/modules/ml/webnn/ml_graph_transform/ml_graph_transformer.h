// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_ML_GRAPH_TRANSFORMER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_ML_GRAPH_TRANSFORMER_H_

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"

namespace blink {

class MLConstantOperand;
using OperandIndex = wtf_size_t;

class MODULES_EXPORT MLGraphTransformer
    : public GarbageCollected<MLGraphTransformer> {
 public:
  explicit MLGraphTransformer(MLGraphBuilder* graph_builder)
      : graph_builder_(graph_builder) {}

  virtual ~MLGraphTransformer() = default;

  virtual void Trace(Visitor* visitor) const;

  // Apply the transformation to the given graph.
  virtual void Transform(MLNamedOperands& named_outputs) = 0;

  virtual const StringView Name() const = 0;

  static void Disconnect(MLOperand* from,
                         MLOperator* to,
                         OperandIndex positional_input_index);

  static void Connect(MLOperand* from,
                      MLOperator* to,
                      OperandIndex positional_input_index);

  static void SwapInput(MLOperator* op,
                        OperandIndex positional_input_index,
                        MLOperand* new_input);

  // If old_input is used as multiple input arguments to `op`, all are swapped.
  static void SwapInput(MLOperator* op,
                        MLOperand* old_input,
                        MLOperand* new_input);

  static void RemoveUnaryOperator(MLOperator* op);

  // The reshaped operand should have the same number of elements as the
  // original operand
  static MLOperand* ReplaceOperandWithNewShape(
      MLOperand* old_operand,
      const Vector<uint32_t>& new_shape);

  // Replace constant operand with a new constant operand, the constant handle
  // gets reused for the new constant.
  static MLConstantOperand* ReplaceConstantOperandWithNewShape(
      const MLConstantOperand* old_operand,
      const Vector<uint32_t>& new_shape);

  static MLOperand* ReplaceOperandWithNewDataType(
      MLOperand* old_operand,
      webnn::OperandDataType new_data_type);

 protected:
  static HeapHashSet<Member<const MLOperator>> GetGraphOutputOperators(
      const MLNamedOperands& named_outputs);

  const ExceptionState GetExceptionState();

  static void DebugPrint(const MLNamedOperands& named_outputs);

  Member<MLGraphBuilder> graph_builder_;

 private:
  static MLOperand* CloneOperandAndResetShape(const MLOperand* operand,
                                              const Vector<uint32_t>& shape);

  static MLOperand* CloneOperandAndResetDataType(
      const MLOperand* operand,
      webnn::OperandDataType data_type);

  static void ReplaceOperand(const MLOperand* old_operand,
                             MLOperand* new_operand);
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_ML_GRAPH_TRANSFORMER_H_
