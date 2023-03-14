// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"

#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"

namespace blink {

HeapVector<Member<const MLOperator>>* GetOperatorsInTopologicalOrder(
    const MLNamedOperands& named_outputs) {
  // A WebNN graph is represented by a directed acyclic graph (DAG) that has
  // operators as vertices and operand as edges. The topological sorting is
  // implemented by depth-first search (DFS) and visiting vertices in
  // post-order. It means a vertex (operator) is visited (pushed to the back of
  // the sorted list) after all its dependent vertices (operators) are visited.
  // With that, it ensures operator 'j' appears before operator 'i' in the
  // result, if 'i' depends on 'j'. The DFS algorithm is based on the
  // non-recursive implementation of:
  // https://en.wikipedia.org/wiki/Depth-first_search

  // The topologically sorted operators.
  auto* toposorted_operators =
      MakeGarbageCollected<HeapVector<Member<const MLOperator>>>();

  // The to-visit stack and visited set for DFS graph traversal.
  HeapDeque<Member<const MLOperator>> operators_to_visit;
  HeapHashSet<Member<const MLOperator>> visited_operators;
  // Enumerate output operands and initialize the to-visit stack with their
  // dependent operators.
  for (const auto& output : named_outputs) {
    const auto* operand = output.second.Get();
    operators_to_visit.push_back(operand->Operator());
  }
  while (operators_to_visit.size() > 0) {
    // Get the current operator from the top of the to-visit stack.
    const auto& current_operator = operators_to_visit.back();
    if (!visited_operators.Contains(current_operator.Get())) {
      // The current operator is not visited, check whether its dependent
      // operators are visited or not.
      bool skip_visit = false;
      for (const auto& operand : current_operator->Inputs()) {
        if (operand->Kind() == MLOperand::OperandKind::kOutput) {
          const auto* dependent_operator = operand->Operator();
          CHECK(dependent_operator);
          if (!visited_operators.Contains(dependent_operator)) {
            // As there is an dependent operator is not visited, skip visiting
            // this operator and push the dependent operator into the to-visit
            // stack.
            skip_visit = true;
            operators_to_visit.push_back(dependent_operator);
          }
        }
      }
      if (!skip_visit) {
        // When all dependent operators have been visited, visit the current
        // operator and add it into the visited set.
        toposorted_operators->push_back(current_operator);
        visited_operators.insert(current_operator);
        // Pop the current operator from the to-visit stack.
        operators_to_visit.pop_back();
      }
    } else {
      // The current operator is already visited, pop it and check the next
      // one.
      operators_to_visit.pop_back();
    }
  }
  return toposorted_operators;
}

}  // namespace blink
