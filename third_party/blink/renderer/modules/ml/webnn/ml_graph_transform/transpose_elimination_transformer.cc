// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/transpose_elimination_transformer.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_transpose_options.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

namespace blink {

void TransposeEliminationTransformer::Transform(
    MLNamedOperands& named_outputs) {
  HeapVector<Member<MLOperator>> sorted_operators =
      GetOperatorsInTopologicalOrder(named_outputs);
  HeapHashSet<Member<const MLOperator>> graph_output_operators =
      GetGraphOutputOperators(named_outputs);
  for (auto& op : sorted_operators) {
    // HandleTranspose will only remove operators before "op", so the for loop
    // is safe to continue.
    if (op->Kind() == webnn::mojom::blink::Operation::Tag::kTranspose) {
      HandleTranspose(op.Get(), graph_output_operators, named_outputs);
    }
  }
}

namespace {
bool IsLayoutAgnosticNode(MLOperator* node) {
  switch (node->Kind()) {
    case webnn::mojom::blink::Operation::Tag::kClamp:
    case webnn::mojom::blink::Operation::Tag::kRelu:
      return true;
    // TODO(crbug.com/406666712): Add more layout agnostic nodes
    default:
      return false;
  }
}

Vector<uint32_t> InversePermutation(const Vector<uint32_t>& permutation) {
  Vector<uint32_t> inverse(permutation.size());
  for (wtf_size_t i = 0; i < permutation.size(); ++i) {
    CHECK_LT(permutation[i], permutation.size());
    inverse[permutation[i]] = i;
  }
  return inverse;
}

bool IsInversePermutations(const Vector<uint32_t>& perm0,
                           const Vector<uint32_t>& perm1) {
  if (perm0.size() != perm1.size()) {
    return false;
  }
  return perm0 == InversePermutation(perm1);
}

// Skip layout agnostic nodes and find the front transpose.
// For example
// node0 -> transpose0 -> clamp0 -> clamp1 -> transpose1 -> node1
// Can be eliminated to:
// node0 -> clamp0 -> clamp1 -> node1
std::optional<MLOperator*> TryFindEliminatableFrontTranspose(
    MLOperator* transpose) {
  if (transpose->PositionalInputs()[0]->Kind() !=
      webnn::mojom::blink::Operand::Kind::kOutput) {
    return std::nullopt;
  }
  MLOperator* cur_node = transpose->PositionalInputs()[0].Get()->Operator();
  while (true) {
    if (cur_node->Outputs().size() != 1 || cur_node->Inputs().size() != 1) {
      break;
    }
    if (cur_node->Kind() == webnn::mojom::blink::Operation::Tag::kTranspose) {
      return cur_node;
    }
    if (IsLayoutAgnosticNode(cur_node) &&
        cur_node->PositionalInputs()[0]->Kind() ==
            webnn::mojom::blink::Operand::Kind::kOutput &&
        cur_node->Outputs()[0]->DependentOperators().size() == 1) {
      cur_node = cur_node->PositionalInputs()[0].Get()->Operator();
    } else {
      break;
    }
  }
  return std::nullopt;
}
}  // namespace

void TransposeEliminationTransformer::HandleTranspose(
    MLOperator* transpose,
    HeapHashSet<Member<const MLOperator>>& graph_output_operators,
    MLNamedOperands& named_outputs) {
  CHECK_EQ(transpose->Kind(), webnn::mojom::blink::Operation::Tag::kTranspose);
  auto optional_front_transpose = TryFindEliminatableFrontTranspose(transpose);
  if (!optional_front_transpose.has_value()) {
    return;
  }

  MLOperator* front_transpose = optional_front_transpose.value();

  if (graph_output_operators.Contains(front_transpose)) {
    return;
  }

  // We should guarantee that after elmination, the graph should have at least
  // one valid operator. So if the input of "front_transpose" is graph input and
  // "transpose" produces the graph output operand  and there is no intermediate
  // node between the two transpose ops, we should not do the elimination. For
  // example, the following graph should not be eliminated:
  // [a] -> transpose0 -> [b] -> transpose1 -> [c]

  if (front_transpose->PositionalInputs()[0]->Kind() ==
          webnn::mojom::blink::Operand::Kind::kInput &&
      graph_output_operators.Contains(transpose) &&
      transpose->PositionalInputs()[0]->Operator() == front_transpose) {
    return;
  }

  auto* options = static_cast<const MLTransposeOptions*>(transpose->Options());
  auto* front_options =
      static_cast<const MLTransposeOptions*>(front_transpose->Options());

  MLOperand* transpose_input_operand = transpose->PositionalInputs()[0].Get();
  wtf_size_t rank = transpose_input_operand->Rank();
  const Vector<uint32_t> default_permutation = CreateDefaultPermutation(rank);
  const Vector<uint32_t> permutation =
      front_options->getPermutationOr(default_permutation);
  const Vector<uint32_t> front_permutation =
      options->getPermutationOr(default_permutation);
  if (!IsInversePermutations(permutation, front_permutation)) {
    return;
  }

  MLOperand* front_transpose_input_operand =
      front_transpose->PositionalInputs()[0].Get();
  MLOperand* back_transpose_output_operand = transpose->Outputs()[0].Get();

  MLOperator* layout_agnostic_node_back = nullptr;
  MLOperator* layout_agnostic_node_front = nullptr;

  if (transpose->PositionalInputs()[0]->Operator() != front_transpose) {
    layout_agnostic_node_back =
        transpose->PositionalInputs()[0].Get()->Operator();
    MLOperator* cur_node = layout_agnostic_node_back;
    while (cur_node != front_transpose) {
      CHECK_EQ(cur_node->Inputs().size(), 1u);
      CHECK(IsLayoutAgnosticNode(cur_node));
      layout_agnostic_node_front = cur_node;
      cur_node = cur_node->PositionalInputs()[0].Get()->Operator();
    }
  }

  if (layout_agnostic_node_back != nullptr) {
    CHECK_NE(layout_agnostic_node_front, nullptr);
    for (MLOperator* cur_node = layout_agnostic_node_back;
         cur_node != front_transpose;
         cur_node = cur_node->PositionalInputs()[0].Get()->Operator()) {
      if (graph_output_operators.Contains(cur_node)) {
        return;
      }
    }
  }

  auto original_front_transpose_deps_num =
      front_transpose->Outputs()[0]->DependentOperators().size();
  HeapHashSet<Member<MLOperator>>& transpose_deps =
      transpose->Outputs()[0]->DependentOperators();

  HeapHashSet<Member<MLOperator>> original_transpose_deps;
  for (auto& dep : transpose_deps) {
    original_transpose_deps.insert(dep);
  }

  RemoveUnaryOperator(transpose);
  if (original_front_transpose_deps_num == 1) {
    RemoveUnaryOperator(front_transpose);
  } else if (layout_agnostic_node_front == nullptr) {
    for (auto& transpose_dep : original_transpose_deps) {
      SwapInput(transpose_dep.Get(), front_transpose->Outputs()[0].Get(),
                front_transpose_input_operand);
    }
  } else {
    SwapInput(layout_agnostic_node_front, front_transpose->Outputs()[0].Get(),
              front_transpose_input_operand);
  }

  if (layout_agnostic_node_back != nullptr) {
    CHECK_NE(layout_agnostic_node_front, nullptr);
    // Update layout agnostic nodes' shapes.
    for (MLOperator* cur_node = layout_agnostic_node_back;;
         cur_node = cur_node->PositionalInputs()[0].Get()->Operator()) {
      ReplaceOperandWithNewShape(cur_node->Outputs()[0].Get(),
                                 front_transpose_input_operand->shape());

      if (cur_node == layout_agnostic_node_front) {
        break;
      }
    }
  }

  // If the removed transpose is producing a graph output operand, update
  // graph_output_operators and named_output.
  if (graph_output_operators.Contains(transpose)) {
    graph_output_operators.erase(transpose);
    if (layout_agnostic_node_back) {
      // The new graph output is 'layout_agnostic_node_back'
      graph_output_operators.insert(layout_agnostic_node_back);
      for (auto& named_output : named_outputs) {
        if (named_output.second.Get() == back_transpose_output_operand) {
          named_output.second = layout_agnostic_node_back->Outputs()[0];
          break;
        }
      }
    } else {
      // The new graph output is 'front_transpose_input_operand'.
      CHECK_NE(front_transpose_input_operand->Kind(),
               webnn::mojom::Operand_Kind::kInput);
      graph_output_operators.insert(front_transpose_input_operand->Operator());
      for (auto& named_output : named_outputs) {
        if (named_output.second.Get() == back_transpose_output_operand) {
          named_output.second = front_transpose_input_operand;
          break;
        }
      }
    }
  }
}

}  // namespace blink
