// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_ML_GRAPH_TRANSFORMER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_ML_GRAPH_TRANSFORMER_H_

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"

namespace blink {

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

  // The return value is the index of the disconnected input operand of "to"
  static OperandIndex Disconnect(MLOperand* from, MLOperator* to);

  static void Disconnect(MLOperand* from,
                         MLOperator* to,
                         OperandIndex input_index);

  static void Connect(MLOperand* from,
                      MLOperator* to,
                      OperandIndex input_index);

  static void SwapInput(MLOperator* op,
                        OperandIndex input_index,
                        MLOperand* new_input);

  static void SwapInput(MLOperator* op,
                        MLOperand* old_input,
                        MLOperand* new_input);

  // The reshaped operand should have the same number of elements as the
  // original operand
  static MLOperand* ReplaceOperandWithNewShape(
      MLOperand* old_operand,
      const Vector<uint32_t>& new_shape);

 protected:
  const ExceptionState GetExceptionState();

  Member<MLGraphBuilder> graph_builder_;

 private:
  static MLOperand* CloneOperandAndResetShape(const MLOperand* operand,
                                              const Vector<uint32_t>& shape);

  static void ReplaceOperand(MLOperand* old_operand, MLOperand* new_operand);
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_ML_GRAPH_TRANSFORMER_H_
