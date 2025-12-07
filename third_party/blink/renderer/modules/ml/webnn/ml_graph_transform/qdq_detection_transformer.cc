// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/qdq_detection_transformer.h"

#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

namespace blink {

namespace {
class CenterOpInput : public GarbageCollected<CenterOpInput> {
 public:
  CenterOpInput(MLOperator* transpose, MLOperator* dequantize)
      : transpose_(transpose), dequantize_(dequantize) {}

  MLOperator* transpose() { return transpose_; }
  MLOperator* dequantize() { return dequantize_; }

  void Trace(Visitor* visitor) const {
    visitor->Trace(transpose_);
    visitor->Trace(dequantize_);
  }

 private:
  Member<MLOperator> transpose_;
  Member<MLOperator> dequantize_;
};

// The input should be 'dq' or 'dq -> transpose'. Returns an empty vector if the
// input does not match these patterns.
HeapVector<Member<CenterOpInput>> MatchCenterOpInputs(MLOperator* center_op) {
  HeapVector<Member<CenterOpInput>> inputs;
  for (MLOperand* input_operand : center_op->Inputs()) {
    if (input_operand->Kind() != webnn::mojom::blink::Operand::Kind::kOutput ||
        input_operand->DependentOperators().size() != 1) {
      return {};
    }
    MLOperator* input_op = input_operand->Operator();
    if (input_op->Kind() ==
        webnn::mojom::blink::Operation::Tag::kDequantizeLinear) {
      inputs.push_back(MakeGarbageCollected<CenterOpInput>(nullptr, input_op));
    } else if (input_op->Kind() ==
               webnn::mojom::blink::Operation::Tag::kTranspose) {
      MLOperand* dq_output_operand = input_op->PositionalInputs()[0];
      MLOperator* dq = dq_output_operand->Operator();
      if (dq->Kind() !=
              webnn::mojom::blink::Operation::Tag::kDequantizeLinear ||
          dq_output_operand->DependentOperators().size() != 1) {
        return {};
      }
      inputs.push_back(MakeGarbageCollected<CenterOpInput>(input_op, dq));
    } else {
      return {};
    }
  }
  return inputs;
}

}  // namespace

void QDQDetectionTransformer::Transform(MLNamedOperands& named_outputs) {
  HeapVector<Member<MLOperator>> sorted_operators =
      GetOperatorsInTopologicalOrder(named_outputs);

  HeapHashSet<Member<const MLOperator>> graph_output_operators =
      GetGraphOutputOperators(named_outputs);

  for (auto& op : sorted_operators) {
    // The for loop is safe to continue because 'HandleQuantize' will only
    // alter operators before 'op'.
    if (op->Kind() == webnn::mojom::blink::Operation::Tag::kQuantizeLinear) {
      HandleQuantize(op.Get(), graph_output_operators, named_outputs);
    }
  }
}

void QDQDetectionTransformer::HandleQuantize(
    MLOperator* quantize,
    HeapHashSet<Member<const MLOperator>>& graph_output_operators,
    MLNamedOperands& named_outputs) {
  CHECK_EQ(quantize->Kind(),
           webnn::mojom::blink::Operation::Tag::kQuantizeLinear);
  //  Recognize below pattern:
  //
  //    ...               ...
  //      \               /
  //       DQ(0)         DQ(1)
  //        \           /
  //    transpose(0)  transpose(1)
  //          \      /
  //        [center operator]
  //             \
  //          transpose(2)
  //               \
  //                Q
  //
  MLOperator* q = quantize;

  // The current context's transpose opSupportLimits needs to support the
  // quantized data type.
  if (!graph_builder_->GetContext()
           ->GetProperties()
           .data_type_limits.transpose_input.Supports(
               q->Outputs()[0]->Descriptor())) {
    return;
  }

  MLOperand* q_input_operand = q->PositionalInputs()[0];
  if (q_input_operand->Kind() != webnn::mojom::blink::Operand::Kind::kOutput) {
    return;
  }

  MLOperator* transpose2 = q_input_operand->Operator();
  if (transpose2->Kind() != webnn::mojom::blink::Operation::Tag::kTranspose ||
      graph_output_operators.Contains(transpose2) ||
      transpose2->Outputs()[0]->DependentOperators().size() != 1) {
    return;
  }

  MLOperand* transpose2_input_operand = transpose2->PositionalInputs()[0];
  if (transpose2_input_operand->Kind() !=
      webnn::mojom::blink::Operand::Kind::kOutput) {
    return;
  }

  MLOperator* center_op = transpose2_input_operand->Operator();
  if (center_op->Outputs().size() != 1 ||
      center_op->Outputs()[0]->DependentOperators().size() != 1) {
    return;
  }

  HeapVector<Member<CenterOpInput>> center_inputs =
      MatchCenterOpInputs(center_op);

  if (center_inputs.empty()) {
    // Fail to match valid input patterns.
    return;
  }

  for (auto& center_input : center_inputs) {
    MLOperator* dq = center_input->dequantize();
    MLOperator* transpose = center_input->transpose();
    if (graph_output_operators.Contains(dq) ||
        (transpose && graph_output_operators.Contains(transpose))) {
      return;
    }
  }

  for (OperandIndex i = 0; i < center_inputs.size(); ++i) {
    CenterOpInput* center_input = center_inputs[i];
    if (center_input->transpose()) {
      MLOperator* transpose = center_input->transpose();
      MLOperator* dq = center_input->dequantize();

      MLOperand* original_subgraph_input_operand =
          dq->PositionalInputs()[0].Get();

      // Swap dq and transpose.
      auto dq_input_data_type = dq->PositionalInputs()[0]->DataType();
      SwapInput(transpose, 0u, original_subgraph_input_operand);
      SwapInput(dq, 0u, transpose->Outputs()[0]);
      SwapInput(center_op, i, dq->Outputs()[0]);
      ReplaceOperandWithNewDataType(transpose->Outputs()[0],
                                    dq_input_data_type);
      ReplaceOperandWithNewShape(dq->Outputs()[0],
                                 transpose->Outputs()[0]->shape());
    }
  }

  // Swap transpose2 and q
  auto q_output_data_type = q->Outputs()[0]->DataType();
  MLOperand* original_q_output_operand = q->Outputs()[0].Get();

  HeapHashSet<Member<MLOperator>> subgraph_dep_operators =
      q->Outputs()[0]->DependentOperators();
  SwapInput(q, 0u, center_op->Outputs()[0]);
  SwapInput(transpose2, 0u, q->Outputs()[0]);
  ReplaceOperandWithNewDataType(transpose2->Outputs()[0], q_output_data_type);
  ReplaceOperandWithNewShape(q->Outputs()[0], center_op->Outputs()[0]->shape());

  for (auto& dep_op : subgraph_dep_operators) {
    SwapInput(dep_op.Get(), q->Outputs()[0], transpose2->Outputs()[0]);
  }

  if (graph_output_operators.Contains(q)) {
    // If the quantize operator is producing a graph output operand, update
    // graph_output_operators and named_outputs.
    graph_output_operators.erase(q);
    graph_output_operators.insert(transpose2);
    for (auto& named_output : named_outputs) {
      if (named_output.second.Get() == original_q_output_operand) {
        named_output.second = transpose2->Outputs()[0];
        break;
      }
    }
  }
}

}  // namespace blink
