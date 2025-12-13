// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/transpose_elimination_transformer.h"

#include "base/containers/adapters.h"
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

  for (auto& op : base::Reversed(sorted_operators)) {
    if (op->Kind() == webnn::mojom::blink::Operation::Tag::kTranspose &&
        !visited_transposes_.Contains(op)) {
      HandleTranspose(op, graph_output_operators, named_outputs);
    }
  }
}

void TransposeEliminationTransformer::Trace(Visitor* visitor) const {
  MLGraphTransformer::Trace(visitor);
  visitor->Trace(visited_transposes_);
}

namespace {

Vector<uint32_t> InversePermutation(const Vector<uint32_t>& permutation) {
  Vector<uint32_t> inverse(permutation.size());
  for (wtf_size_t i = 0; i < permutation.size(); ++i) {
    CHECK_LT(permutation[i], permutation.size());
    inverse[permutation[i]] = i;
  }
  return inverse;
}

class BracketingContext : public GarbageCollected<BracketingContext> {
 public:
  void Trace(Visitor* vistor) const {
    vistor->Trace(in_transposes);
    vistor->Trace(out_transposes);
    vistor->Trace(intermediate_nodes);
  }

  HeapVector<Member<MLOperator>> in_transposes;
  HeapVector<Member<MLOperator>> out_transposes;
  HeapVector<Member<MLOperator>> intermediate_nodes;
};

// This algorithm assumes all constant operands of this subgraph being already
// transposed.
// Match a subgraph, whose inputs and outputs are all transposes.
// Here is the constraint of the matched subgraph:
// 1. All input transposes should have the same permutation.
// 2. All output transposes should have the same permutation.
// 3. The input transpose and output transpose should be inverse to each other.
// 4. All input transposes should not be graph output operators.
// 5. The intermediate nodes in the subgraph should not contain graph input or
//    output operators.
// 6. The intermediate nodes should pass
//    IsValidOpkindForBracketingElimination check. Generally, the operator
//    should be layout agnostic. And it has only 1 output port. And all inputs
//    should have the same shape.
// 7. If the subgraph is the whole graph, we should make sure the final graph
//    has at least one operator.
class BracketingContextBuilder
    : public GarbageCollected<BracketingContextBuilder> {
 public:
  void Trace(Visitor* vistor) const {
    vistor->Trace(to_visit_output_transposes_);
    vistor->Trace(to_visit_input_transposes_);
    vistor->Trace(to_visit_intermediate_nodes_);
    vistor->Trace(visited_);
    vistor->Trace(context_);
    vistor->Trace(graph_output_operators_);
  }

  explicit BracketingContextBuilder(
      const HeapHashSet<Member<const MLOperator>>& graph_output_operators)
      : graph_output_operators_(graph_output_operators) {}

  static BracketingContext* CreateAndBuildBracketingContext(
      const HeapHashSet<Member<const MLOperator>>& graph_output_operators,
      MLOperator* seed) {
    CHECK_EQ(seed->Kind(), webnn::mojom::blink::Operation::Tag::kTranspose);

    auto* builder =
        MakeGarbageCollected<BracketingContextBuilder>(graph_output_operators);

    builder->out_transpose_perm_ =
        static_cast<const MLTransposeOptions*>(seed->Options())
            ->getPermutationOr(
                CreateDefaultPermutation(seed->Outputs()[0]->Rank()));
    builder->input_transpose_perm_ =
        InversePermutation(builder->out_transpose_perm_);

    builder->context_ = MakeGarbageCollected<BracketingContext>();
    builder->context_->out_transposes.push_back(seed);
    builder->to_visit_output_transposes_.push_back(seed);

    // This traverses the graph in both directions:
    // - When visiting an output transpose, it traverses up.
    // - When visiting an input transpose, it traverses down.
    // - When visiting an intermediate operation, it traverses both directions.
    while (!(builder->to_visit_output_transposes_.empty() &&
             builder->to_visit_input_transposes_.empty() &&
             builder->to_visit_intermediate_nodes_.empty())) {
      // Visit output transpose.
      if (!builder->to_visit_output_transposes_.empty()) {
        const MLOperator* cur_node =
            builder->to_visit_output_transposes_.back();
        CHECK_EQ(cur_node->Kind(),
                 webnn::mojom::blink::Operation::Tag::kTranspose);
        builder->to_visit_output_transposes_.pop_back();
        builder->visited_.insert(cur_node);

        // Visit the input node of cur_node.
        if (!builder->VisitInputs(cur_node)) {
          return nullptr;
        }
      }

      // Visit input transpose.
      if (!builder->to_visit_input_transposes_.empty()) {
        const MLOperator* cur_node =
            builder->to_visit_input_transposes_.back().Get();
        CHECK_EQ(cur_node->Kind(),
                 webnn::mojom::blink::Operation::Tag::kTranspose);
        builder->to_visit_input_transposes_.pop_back();
        builder->visited_.insert(cur_node);
        // Visit the output nodes of cur_node.
        if (!builder->VisitDeps(cur_node)) {
          return nullptr;
        }
      }

      // Visit intermediate node.
      if (!builder->to_visit_intermediate_nodes_.empty()) {
        const MLOperator* cur_node =
            builder->to_visit_intermediate_nodes_.back().Get();

        // For simplicity we restrict intermediate nodes to only have 1 output,
        // we can loosen the requirement once we have such use cases.
        if (cur_node->Outputs().size() != 1u) {
          return nullptr;
        }

        builder->to_visit_intermediate_nodes_.pop_back();
        builder->visited_.insert(cur_node);

        // Visit the input node of cur_node.
        if (!builder->VisitInputs(cur_node)) {
          return nullptr;
        }

        // Visit the output nodes of cur_node.
        if (!builder->VisitDeps(cur_node)) {
          return nullptr;
        }
      }
    }

    return builder->context_;
  }

 private:
  bool IsValidOpkindForBracketingElimination(MLOperator* op) {
    switch (op->Kind()) {
      case webnn::mojom::blink::Operation::Tag::kClamp:
      case webnn::mojom::blink::Operation::Tag::kRelu:
      case webnn::mojom::blink::Operation::Tag::kElu:
      case webnn::mojom::blink::Operation::Tag::kGelu:
      case webnn::mojom::blink::Operation::Tag::kHardSigmoid:
      case webnn::mojom::blink::Operation::Tag::kHardSwish:
      case webnn::mojom::blink::Operation::Tag::kLeakyRelu:
      case webnn::mojom::blink::Operation::Tag::kLinear:
      case webnn::mojom::blink::Operation::Tag::kSigmoid:
      case webnn::mojom::blink::Operation::Tag::kSoftmax:
      case webnn::mojom::blink::Operation::Tag::kSoftplus:
      case webnn::mojom::blink::Operation::Tag::kSoftsign:
      case webnn::mojom::blink::Operation::Tag::kTanh:
      case webnn::mojom::blink::Operation::Tag::kElementWiseUnary:
        return true;
      case webnn::mojom::blink::Operation::Tag::kElementWiseBinary: {
        // Avoid broadcasting cases.
        return op->Inputs()[0]->Shape() == op->Inputs()[1]->Shape();
      }
      default:
        return false;
    }
  }

  bool VisitInputs(const MLOperator* cur_node) {
    for (auto& input : cur_node->PositionalInputs()) {
      if (input->Kind() != webnn::mojom::blink::Operand::Kind::kOutput ||
          graph_output_operators_.Contains(input->Operator())) {
        return false;
      }

      if (visited_.Contains(input->Operator())) {
        continue;
      }

      if (input->Operator()->Kind() ==
          webnn::mojom::blink::Operation::Tag::kTranspose) {
        auto perm =
            static_cast<const MLTransposeOptions*>(input->Operator()->Options())
                ->getPermutationOr(CreateDefaultPermutation(input->Rank()));
        if (perm != input_transpose_perm_) {
          return false;
        }

        context_->in_transposes.push_back(input->Operator());
        to_visit_input_transposes_.push_back(input->Operator());
        continue;
      }

      if (!IsValidOpkindForBracketingElimination(input->Operator())) {
        return false;
      }

      context_->intermediate_nodes.push_back(input->Operator());
      to_visit_intermediate_nodes_.push_back(input->Operator());
    }
    return true;
  }

  bool VisitDeps(const MLOperator* cur_node) {
    for (auto& dep : cur_node->Outputs()[0]->DependentOperators()) {
      if (visited_.Contains(dep)) {
        continue;
      }

      if (dep->Kind() == webnn::mojom::blink::Operation::Tag::kTranspose) {
        auto perm = static_cast<const MLTransposeOptions*>(dep->Options())
                        ->getPermutationOr(CreateDefaultPermutation(
                            dep->Outputs()[0]->Rank()));

        if (perm != out_transpose_perm_) {
          return false;
        }

        context_->out_transposes.push_back(dep);
        continue;
      }

      if (!IsValidOpkindForBracketingElimination(dep)) {
        return false;
      }

      if (graph_output_operators_.Contains(dep)) {
        return false;
      }
      context_->intermediate_nodes.push_back(dep);
      to_visit_intermediate_nodes_.push_back(dep);
    }
    return true;
  }

  HeapVector<Member<const MLOperator>> to_visit_output_transposes_;
  HeapVector<Member<const MLOperator>> to_visit_input_transposes_;
  HeapVector<Member<const MLOperator>> to_visit_intermediate_nodes_;
  HeapHashSet<Member<const MLOperator>> visited_;

  Vector<uint32_t> input_transpose_perm_;
  Vector<uint32_t> out_transpose_perm_;

  Member<BracketingContext> context_;

  const HeapHashSet<Member<const MLOperator>> graph_output_operators_;
};

}  // namespace

void TransposeEliminationTransformer::HandleTranspose(
    MLOperator* transpose,
    HeapHashSet<Member<const MLOperator>>& graph_output_operators,
    MLNamedOperands& named_outputs) {
  CHECK_EQ(transpose->Kind(), webnn::mojom::blink::Operation::Tag::kTranspose);

  const auto* context =
      BracketingContextBuilder::CreateAndBuildBracketingContext(
          graph_output_operators, transpose);

  if (!context) {
    return;
  }

  CHECK_GT(context->in_transposes.size(), 0u);
  CHECK_GT(context->out_transposes.size(), 0u);

  bool is_in_transposes_input_all_graph_input = true;
  bool is_out_transposes_output_all_graph_output = true;

  for (auto& in_transpose : context->in_transposes) {
    if (in_transpose->PositionalInputs()[0]->Kind() !=
        webnn::mojom::blink::Operand::Kind::kInput) {
      is_in_transposes_input_all_graph_input = false;
      break;
    }
  }

  for (auto& out_transpose : context->out_transposes) {
    if (!graph_output_operators.Contains(out_transpose)) {
      is_out_transposes_output_all_graph_output = false;
      break;
    }
  }

  // Make sure the final graph at least has one operator.
  if (is_in_transposes_input_all_graph_input &&
      is_out_transposes_output_all_graph_output &&
      context->intermediate_nodes.size() == 0) {
    return;
  }

  // Perform the eliminations.
  auto original_input_shape = context->in_transposes[0]->Inputs()[0]->shape();

  visited_transposes_.insert(transpose);
  for (auto& in_transpose : context->in_transposes) {
    CHECK(!graph_output_operators.Contains(in_transpose));
    visited_transposes_.insert(in_transpose);
    RemoveUnaryOperator(in_transpose);
  }

  // Update the shapes of intermediate nodes.
  for (auto& intermediate_node : context->intermediate_nodes) {
    CHECK_EQ(intermediate_node->Outputs().size(), 1u);
    ReplaceOperandWithNewShape(intermediate_node->Outputs()[0],
                               original_input_shape);
  }

  for (auto& out_transpose : context->out_transposes) {
    visited_transposes_.insert(out_transpose);
    MLOperand* output_transpose_input = out_transpose->Inputs()[0].Get();
    RemoveUnaryOperator(out_transpose);

    // If the removed transpose is producing a graph output operand, update
    // graph_output_operators and named_output.
    if (graph_output_operators.Contains(out_transpose)) {
      graph_output_operators.erase(out_transpose);

      graph_output_operators.insert(output_transpose_input->Operator());
      for (auto& named_output : named_outputs) {
        if (named_output.second.Get() == out_transpose->Outputs()[0].Get()) {
          named_output.second = output_transpose_input;
          break;
        }
      }
    }
  }
}

}  // namespace blink
