// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/ml_graph_transformer.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_batch_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_transpose_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gru_cell_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gru_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_instance_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_layer_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_lstm_cell_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_lstm_options.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_constant_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

namespace blink_mojom = webnn::mojom::blink;
namespace blink {

namespace {

void UpdateOperatorOptions(MLOperator* op,
                           MLOperand* old_input,
                           MLOperand* new_input) {
  bool updated = false;
  switch (op->Kind()) {
    case blink_mojom::Operation::Tag::kBatchNormalization: {
      auto* options = static_cast<MLBatchNormalizationOptions*>(op->Options());
      if (options->hasScale() && options->scale() == old_input) {
        options->setScale(new_input);
        updated = true;
      }
      if (options->hasBias() && options->bias() == old_input) {
        options->setBias(new_input);
        updated = true;
      }
      break;
    }
    case blink_mojom::Operation::Tag::kConv2d: {
      switch (op->SubKind<blink_mojom::Conv2d::Kind>()) {
        case blink_mojom::Conv2d::Kind::kDirect: {
          auto* options = static_cast<MLConv2dOptions*>(op->Options());
          if (options->hasBias() && options->bias() == old_input) {
            options->setBias(new_input);
            updated = true;
          }
          break;
        }
        case blink_mojom::Conv2d::Kind::kTransposed: {
          auto* options = static_cast<MLConvTranspose2dOptions*>(op->Options());
          if (options->hasBias() && options->bias() == old_input) {
            options->setBias(new_input);
            updated = true;
          }
          break;
        }
      }
      break;
    }
    case blink_mojom::Operation::Tag::kGemm: {
      auto* options = static_cast<MLGemmOptions*>(op->Options());
      if (options->hasC() && options->c() == old_input) {
        options->setC(new_input);
        updated = true;
      }
      break;
    }
    case blink_mojom::Operation::Tag::kGru: {
      auto* options = static_cast<MLGruOptions*>(op->Options());
      if (options->hasBias() && options->bias() == old_input) {
        options->setBias(new_input);
        updated = true;
      }
      if (options->hasRecurrentBias() &&
          options->recurrentBias() == old_input) {
        options->setRecurrentBias(new_input);
        updated = true;
      }
      if (options->hasInitialHiddenState() &&
          options->initialHiddenState() == old_input) {
        options->setInitialHiddenState(new_input);
        updated = true;
      }
      break;
    }
    case blink_mojom::Operation::Tag::kGruCell: {
      auto* options = static_cast<MLGruCellOptions*>(op->Options());
      if (options->hasBias() && options->bias() == old_input) {
        options->setBias(new_input);
        updated = true;
      }
      if (options->hasRecurrentBias() &&
          options->recurrentBias() == old_input) {
        options->setRecurrentBias(new_input);
        updated = true;
      }
      break;
    }
    case blink_mojom::Operation::Tag::kInstanceNormalization: {
      auto* options =
          static_cast<MLInstanceNormalizationOptions*>(op->Options());
      if (options->hasScale() && options->scale() == old_input) {
        options->setScale(new_input);
        updated = true;
      }
      if (options->hasBias() && options->bias() == old_input) {
        options->setBias(new_input);
        updated = true;
      }
      break;
    }
    case blink_mojom::Operation::Tag::kLayerNormalization: {
      auto* options = static_cast<MLLayerNormalizationOptions*>(op->Options());
      if (options->hasScale() && options->scale() == old_input) {
        options->setScale(new_input);
        updated = true;
      }
      if (options->hasBias() && options->bias() == old_input) {
        options->setBias(new_input);
        updated = true;
      }
      break;
    }
    case blink_mojom::Operation::Tag::kLstm: {
      auto* options = static_cast<MLLstmOptions*>(op->Options());
      if (options->hasBias() && options->bias() == old_input) {
        options->setBias(new_input);
        updated = true;
      }
      if (options->hasRecurrentBias() &&
          options->recurrentBias() == old_input) {
        options->setRecurrentBias(new_input);
        updated = true;
      }
      if (options->hasPeepholeWeight() &&
          options->peepholeWeight() == old_input) {
        options->setPeepholeWeight(new_input);
        updated = true;
      }
      if (options->hasInitialHiddenState() &&
          options->initialHiddenState() == old_input) {
        options->setInitialHiddenState(new_input);
        updated = true;
      }
      if (options->hasInitialCellState() &&
          options->initialCellState() == old_input) {
        options->setInitialCellState(new_input);
        updated = true;
      }
      break;
    }
    case blink_mojom::Operation::Tag::kLstmCell: {
      auto* options = static_cast<MLLstmCellOptions*>(op->Options());
      if (options->hasBias() && options->bias() == old_input) {
        options->setBias(new_input);
        updated = true;
      }
      if (options->hasRecurrentBias() &&
          options->recurrentBias() == old_input) {
        options->setRecurrentBias(new_input);
        updated = true;
      }
      if (options->hasPeepholeWeight() &&
          options->peepholeWeight() == old_input) {
        options->setPeepholeWeight(new_input);
        updated = true;
      }
      break;
    }
    case blink_mojom::Operation::Tag::kArgMinMax:
    case blink_mojom::Operation::Tag::kClamp:
    case blink_mojom::Operation::Tag::kConcat:
    case blink_mojom::Operation::Tag::kCumulativeSum:
    case blink_mojom::Operation::Tag::kDequantizeLinear:
    case blink_mojom::Operation::Tag::kElementWiseBinary:
    case blink_mojom::Operation::Tag::kElementWiseUnary:
    case blink_mojom::Operation::Tag::kElu:
    case blink_mojom::Operation::Tag::kExpand:
    case blink_mojom::Operation::Tag::kGather:
    case blink_mojom::Operation::Tag::kGatherElements:
    case blink_mojom::Operation::Tag::kGatherNd:
    case blink_mojom::Operation::Tag::kGelu:
    case blink_mojom::Operation::Tag::kHardSigmoid:
    case blink_mojom::Operation::Tag::kHardSwish:
    case blink_mojom::Operation::Tag::kLeakyRelu:
    case blink_mojom::Operation::Tag::kLinear:
    case blink_mojom::Operation::Tag::kMatmul:
    case blink_mojom::Operation::Tag::kPad:
    case blink_mojom::Operation::Tag::kPool2d:
    case blink_mojom::Operation::Tag::kPrelu:
    case blink_mojom::Operation::Tag::kQuantizeLinear:
    case blink_mojom::Operation::Tag::kReduce:
    case blink_mojom::Operation::Tag::kResample2d:
    case blink_mojom::Operation::Tag::kRelu:
    case blink_mojom::Operation::Tag::kReshape:
    case blink_mojom::Operation::Tag::kReverse:
    case blink_mojom::Operation::Tag::kScatterElements:
    case blink_mojom::Operation::Tag::kScatterNd:
    case blink_mojom::Operation::Tag::kSigmoid:
    case blink_mojom::Operation::Tag::kSlice:
    case blink_mojom::Operation::Tag::kSoftmax:
    case blink_mojom::Operation::Tag::kSoftplus:
    case blink_mojom::Operation::Tag::kSoftsign:
    case blink_mojom::Operation::Tag::kSplit:
    case blink_mojom::Operation::Tag::kTanh:
    case blink_mojom::Operation::Tag::kTile:
    case blink_mojom::Operation::Tag::kTranspose:
    case blink_mojom::Operation::Tag::kTriangular:
    case blink_mojom::Operation::Tag::kWhere: {
      break;
    }
  }
  if (updated) {
    new_input->AddDependentOperator(op);
    if (!op->Inputs().Contains(old_input)) {
      old_input->DependentOperators().erase(op);
    }
  }
}

}  // namespace

void MLGraphTransformer::Trace(Visitor* visitor) const {
  visitor->Trace(graph_builder_);
}

// static
void MLGraphTransformer::Disconnect(MLOperand* from,
                                    MLOperator* to,
                                    OperandIndex positional_input_index) {
  auto& dependent_operators = from->dependent_operators_;

  CHECK(dependent_operators.Contains(to));

  CHECK_EQ(to->inputs_[positional_input_index], from);
  to->inputs_[positional_input_index] = nullptr;
  if (!to->Inputs().Contains(from)) {
    dependent_operators.erase(to);
  }
}

// static
void MLGraphTransformer::Connect(MLOperand* from,
                                 MLOperator* to,
                                 OperandIndex positional_input_index) {
  from->AddDependentOperator(to);

  CHECK_EQ(to->inputs_[positional_input_index], nullptr);
  to->inputs_[positional_input_index] = from;
}

// static
void MLGraphTransformer::SwapInput(MLOperator* op,
                                   OperandIndex positional_input_index,
                                   MLOperand* new_input) {
  MLOperand* old_input = op->inputs_[positional_input_index].Get();
  CHECK(old_input);

  Disconnect(old_input, op, positional_input_index);
  Connect(new_input, op, positional_input_index);
}

// static
void MLGraphTransformer::SwapInput(MLOperator* op,
                                   MLOperand* old_input,
                                   MLOperand* new_input) {
  for (wtf_size_t positional_input_index = 0;
       positional_input_index < op->inputs_.size(); ++positional_input_index) {
    if (op->inputs_[positional_input_index] == old_input) {
      Disconnect(old_input, op, positional_input_index);
      Connect(new_input, op, positional_input_index);
    }
  }
  UpdateOperatorOptions(op, old_input, new_input);
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
void MLGraphTransformer::ReplaceOperand(const MLOperand* old_operand,
                                        MLOperand* new_operand) {
  if (old_operand->Kind() == webnn::mojom::blink::Operand::Kind::kOutput) {
    auto* op = old_operand->Operator();
    for (auto& output : op->outputs_) {
      if (output == old_operand) {
        output = new_operand;
      }
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

MLConstantOperand* MLGraphTransformer::ReplaceConstantOperandWithNewShape(
    const MLConstantOperand* old_operand,
    const Vector<uint32_t>& new_shape) {
  auto descriptor = webnn::OperandDescriptor::Create(
      old_operand->Builder()->GetContext()->GetProperties(),
      old_operand->DataType(), new_shape, /*label=*/"");
  CHECK(descriptor.has_value());
  CHECK_EQ(old_operand->NumberOfElements(), descriptor->NumberOfElements());

  MLConstantOperand* new_operand = MakeGarbageCollected<MLConstantOperand>(
      old_operand->Builder(), descriptor.value(), old_operand->handle());

  new_operand->dependent_operators_ = old_operand->dependent_operators_;
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
