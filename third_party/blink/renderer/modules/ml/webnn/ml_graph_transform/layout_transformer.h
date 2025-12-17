// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_LAYOUT_TRANSFORMER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_LAYOUT_TRANSFORMER_H_

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/ml_graph_transformer.h"

namespace blink {

// Some operators like Conv2d will specify inputLayout and filterLayout
// in the options. Base on the layout information, LayoutTransformer will
// insert corresponding transpose operators.

// Note, during the transformation, we may need to update some
// MLOperatorOptions.  MLOperatorOptions is not ScriptWrappable, it's invisible
// to JS. So we can safely update the options in-place.

class LayoutTransformer : public MLGraphTransformer {
 public:
  explicit LayoutTransformer(MLGraphBuilder* graph_builder)
      : MLGraphTransformer(graph_builder) {}
  void Transform(MLNamedOperands& named_outputs) override;

  const StringView Name() const override { return "LayoutTransformer"; }

 private:
  void InsertInputTranspose(MLOperator* op,
                            OperandIndex positional_input_index,
                            base::span<const uint32_t> permutation,
                            String label,
                            ExceptionState& exception_state);
  void PermuteOperandShape(MLOperand* operand,
                           base::span<const uint32_t> permutation);
  MLOperand* InsertOutputTranspose(MLOperator* op,
                                   OperandIndex output_index,
                                   base::span<const uint32_t> permutation,
                                   String label,
                                   ExceptionState& exception_state);
  template <typename MLConv2dOptionsType>
  MLOperand* HandleConv2d(MLOperator* conv2d);
  MLOperand* HandleResample2d(MLOperator* resample2d);
  MLOperand* HandleBatchNormalization(MLOperator* batch_norm);
  MLOperand* HandleInstanceNormalization(MLOperator* instance_norm);
  MLOperand* HandlePool2d(MLOperator* pool2d);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_LAYOUT_TRANSFORMER_H_
