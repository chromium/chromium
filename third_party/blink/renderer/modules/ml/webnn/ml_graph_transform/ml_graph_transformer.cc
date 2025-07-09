// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/ml_graph_transformer.h"

#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

namespace blink {

void MLGraphTransformer::Trace(Visitor* visitor) const {
  visitor->Trace(graph_builder_);
}

// static
wtf_size_t MLGraphTransformer::Disconnect(MLOperand* from, MLOperator* to) {
  auto& dependent_operators = from->dependent_operators_;

  CHECK(dependent_operators.Contains(to));
  dependent_operators.erase(to);

  OperandIndex input_index = to->inputs_.Find(from);
  CHECK(input_index != kNotFound);
  to->inputs_[input_index] = nullptr;
  return input_index;
}

// static
void MLGraphTransformer::Disconnect(MLOperand* from,
                                    MLOperator* to,
                                    OperandIndex input_index) {
  auto& dependent_operators = from->dependent_operators_;

  CHECK(dependent_operators.Contains(to));
  dependent_operators.erase(to);

  CHECK(to->inputs_[input_index] == from);
  to->inputs_[input_index] = nullptr;
}

// static
void MLGraphTransformer::Connect(MLOperand* from,
                                 MLOperator* to,
                                 OperandIndex input_index) {
  CHECK(!from->dependent_operators_.Contains(to));
  from->AddDependentOperator(to);

  CHECK_EQ(to->inputs_[input_index], nullptr);
  to->inputs_[input_index] = from;
}

// static
void MLGraphTransformer::SwapInput(MLOperator* op,
                                   OperandIndex input_index,
                                   MLOperand* new_input) {
  MLOperand* old_input = op->inputs_[input_index].Get();
  CHECK(old_input);

  Disconnect(old_input, op, input_index);
  Connect(new_input, op, input_index);
}

// static
void MLGraphTransformer::SwapInput(MLOperator* op,
                                   MLOperand* old_input,
                                   MLOperand* new_input) {
  int index = Disconnect(old_input, op);
  Connect(new_input, op, index);
}

// static
void MLGraphTransformer::RemoveUnaryOperator(MLOperator* op) {
  CHECK_EQ(op->inputs_.size(), 1u);
  CHECK_EQ(op->outputs_.size(), 1u);

  MLOperand* input_operand = op->inputs_[0].Get();
  MLOperand* output_operand = op->outputs_[0].Get();

  Disconnect(input_operand, op, 0);

  auto dep_operators = output_operand->DependentOperators();

  for (auto& dep : dep_operators) {
    SwapInput(dep.Get(), output_operand, input_operand);
  }
}

// static
MLOperand* MLGraphTransformer::CloneOperandAndResetShape(
    const MLOperand* operand,
    const Vector<uint32_t>& shape) {
  auto descriptor = webnn::OperandDescriptor::Create(
      operand->Builder()->GetContext()->GetProperties(), operand->DataType(),
      shape, /*label=*/"");
  CHECK(descriptor.has_value());
  CHECK_EQ(operand->NumberOfElements(), descriptor->NumberOfElements());

  MLOperand* clone = MakeGarbageCollected<MLOperand>(
      operand->Builder(), operand->Kind(), descriptor.value());

  clone->operator_ = operand->Operator();
  clone->dependent_operators_ = operand->dependent_operators_;
  return clone;
}

// static
MLOperand* MLGraphTransformer::CloneOperandAndResetDataType(
    const MLOperand* operand,
    webnn::OperandDataType data_type) {
  auto descriptor = webnn::OperandDescriptor::Create(
      operand->Builder()->GetContext()->GetProperties(), data_type,
      operand->Shape(), /*label=*/"");
  CHECK(descriptor.has_value());
  CHECK_EQ(operand->NumberOfElements(), descriptor->NumberOfElements());

  MLOperand* clone = MakeGarbageCollected<MLOperand>(
      operand->Builder(), operand->Kind(), descriptor.value());

  clone->operator_ = operand->Operator();
  clone->dependent_operators_ = operand->dependent_operators_;
  return clone;
}

// static
void MLGraphTransformer::ReplaceOperand(MLOperand* old_operand,
                                        MLOperand* new_operand) {
  auto* op = old_operand->Operator();
  for (auto& output : op->outputs_) {
    if (output == old_operand) {
      output = new_operand;
    }
  }

  auto& deps = old_operand->dependent_operators_;
  for (auto& dep : deps) {
    auto* dep_op = dep.Get();
    for (auto& input : dep_op->inputs_) {
      if (input == old_operand) {
        input = new_operand;
      }
    }
  }
}

// static
MLOperand* MLGraphTransformer::ReplaceOperandWithNewShape(
    MLOperand* old_operand,
    const Vector<uint32_t>& new_shape) {
  auto* new_operand = CloneOperandAndResetShape(old_operand, new_shape);
  ReplaceOperand(old_operand, new_operand);
  return new_operand;
}

MLOperand* MLGraphTransformer::ReplaceOperandWithNewDataType(
    MLOperand* old_operand,
    webnn::OperandDataType new_data_type) {
  auto* new_operand = CloneOperandAndResetDataType(old_operand, new_data_type);
  ReplaceOperand(old_operand, new_operand);
  return new_operand;
}

const ExceptionState MLGraphTransformer::GetExceptionState() {
  auto* isolate = graph_builder_->GetExecutionContext()->GetIsolate();
  return ExceptionState(isolate);
}

// static
HeapHashSet<Member<const MLOperator>>
MLGraphTransformer::GetGraphOutputOperators(
    const MLNamedOperands& named_outputs) {
  HeapHashSet<Member<const MLOperator>> graph_output_operators;
  for (const auto& named_output : named_outputs) {
    MLOperand* output_operand = named_output.second.Get();
    graph_output_operators.insert(output_operand->Operator());
  }
  return graph_output_operators;
}

// static
void MLGraphTransformer::DebugPrint(const MLNamedOperands& named_outputs) {
  HeapVector<Member<MLOperator>> sorted_operators =
      GetOperatorsInTopologicalOrder(named_outputs);

  size_t id = 0;

  HeapHashMap<Member<const MLOperand>, size_t> input_constant_operand_ids;

  HeapHashMap<Member<const MLOperator>, size_t> operator_ids;

  DLOG(INFO) << "MLGraphTransformer Debug Print:\n";

  for (auto& op : sorted_operators) {
    for (auto& input : op->Inputs()) {
      if (input->Kind() == webnn::mojom::blink::Operand::Kind::kInput) {
        if (!input_constant_operand_ids.Contains(input)) {
          input_constant_operand_ids.insert(input, ++id);
          DLOG(INFO) << "#" << id << " Input: " << input->Name() << "\n";
        }
      } else if (input->Kind() == webnn::mojom::Operand_Kind::kConstant) {
        if (!input_constant_operand_ids.Contains(input)) {
          input_constant_operand_ids.insert(input, ++id);
          DLOG(INFO) << "#" << id << " Constant" << "\n";
        }
      }
    }

    String opname = MLOperator::OperatorKindToString(op->Kind(), op->SubKind());
    operator_ids.insert(op, ++id);
    DLOG(INFO) << "#" << id << " " << opname.Utf8() << " (";
    for (auto& input : op->Inputs()) {
      if (input->Kind() == webnn::mojom::Operand_Kind::kInput ||
          input->Kind() == webnn::mojom::Operand_Kind::kConstant) {
        DLOG(INFO) << "#" << input_constant_operand_ids.at(input) << " ";
      } else {
        DLOG(INFO) << "#" << operator_ids.at(input->Operator()) << " ";
      }
    }
    DLOG(INFO) << ")\n";
  }

  DLOG(INFO) << "\n\n";
}

}  // namespace blink
