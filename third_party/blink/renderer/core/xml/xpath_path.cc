/*
 * Copyright (C) 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/xml/xpath_path.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/xml/xpath_predicate.h"
#include "third_party/blink/renderer/core/xml/xpath_step.h"
#include "third_party/blink/renderer/core/xml/xpath_value.h"

namespace blink {
namespace xpath {

Filter::Filter(Expression* expr, HeapVector<Member<Predicate>>& predicates)
    : expr_(expr) {
  predicates_.swap(predicates);
  SetIsContextNodeSensitive(expr_->IsContextNodeSensitive());
  SetIsContextPositionSensitive(expr_->IsContextPositionSensitive());
  SetIsContextSizeSensitive(expr_->IsContextSizeSensitive());
}

Filter::~Filter() = default;

void Filter::Trace(Visitor* visitor) const {
  visitor->Trace(expr_);
  visitor->Trace(predicates_);
  Expression::Trace(visitor);
}

Value Filter::Evaluate(EvaluationContext& evaluation_context) const {
  Value v = expr_->Evaluate(evaluation_context);

  NodeSet& nodes = v.ModifiableNodeSet(evaluation_context);
  nodes.Sort();

  for (const auto& predicate : predicates_) {
    NodeSet* new_nodes = NodeSet::Create();
    evaluation_context.size = nodes.size();
    evaluation_context.position = 0;

    for (const auto& node : nodes) {
      evaluation_context.node = node;
      ++evaluation_context.position;

      if (predicate->Evaluate(evaluation_context))
        new_nodes->Append(node);
    }
    nodes.Swap(*new_nodes);
  }

  return v;
}

LocationPath::LocationPath() : absolute_(false) {
  SetIsContextNodeSensitive(true);
}

LocationPath::~LocationPath() = default;

void LocationPath::Trace(Visitor* visitor) const {
  visitor->Trace(steps_);
  Expression::Trace(visitor);
}

Value LocationPath::Evaluate(EvaluationContext& evaluation_context) const {
  EvaluationContext cloned_context = evaluation_context;
  // http://www.w3.org/TR/xpath/
  // Section 2, Location Paths:
  // "/ selects the document root (which is always the parent of the document
  // element)"
  // "A / by itself selects the root node of the document containing the context
  // node."
  // In the case of a tree that is detached from the document, we violate
  // the spec and treat / as the root node of the detached tree.
  // This is for compatibility with Firefox, and also seems like a more
  // logical treatment of where you would expect the "root" to be.
  Node* context = evaluation_context.node;
  if (absolute_ && context->getNodeType() != Node::kDocumentNode) {
    if (context->isConnected())
      context = context->ownerDocument();
    else
      context = &NodeTraversal::HighestAncestorOrSelf(*context);
  }

  NodeSet* nodes = NodeSet::Create();
  nodes->Append(context);
  Evaluate(cloned_context, *nodes);

  return Value(nodes, Value::kAdopt);
}

void LocationPath::Evaluate(EvaluationContext& context, NodeSet& nodes) const {
  bool result_is_sorted = nodes.IsSorted();

  for (const auto& step : steps_) {
    NodeSet* new_nodes = NodeSet::Create();
    HeapHashSet<Member<Node>> new_nodes_set;

    bool need_to_check_for_duplicate_nodes =
        !nodes.SubtreesAreDisjoint() ||
        (step->GetAxis() != Step::kChildAxis &&
         step->GetAxis() != Step::kSelfAxis &&
         step->GetAxis() != Step::kDescendantAxis &&
         step->GetAxis() != Step::kDescendantOrSelfAxis &&
         step->GetAxis() != Step::kAttributeAxis);

    if (need_to_check_for_duplicate_nodes)
      result_is_sorted = false;

    // This is a simplified check that can be improved to handle more cases.
    if (nodes.SubtreesAreDisjoint() && (step->GetAxis() == Step::kChildAxis ||
                                        step->GetAxis() == Step::kSelfAxis))
      new_nodes->MarkSubtreesDisjoint(true);

    for (const auto& input_node : nodes) {
      NodeSet* matches = NodeSet::Create();
      step->Evaluate(context, input_node, *matches);

      if (!matches->IsSorted())
        result_is_sorted = false;

      for (const auto& node : *matches) {
        if (!need_to_check_for_duplicate_nodes ||
            new_nodes_set.insert(node).is_new_entry)
          new_nodes->Append(node);
      }
    }

    nodes.Swap(*new_nodes);
  }

  nodes.MarkSorted(result_is_sorted);
}

void LocationPath::AppendStep(Step* step) {
  unsigned step_count = steps_.size();
  if (step_count && OptimizeStepPair(steps_[step_count - 1], step))
    return;
  step->Optimize();
  steps_.push_back(step);
}

void LocationPath::InsertFirstStep(Step* step) {
  if (steps_.size() && OptimizeStepPair(step, steps_[0])) {
    steps_[0] = step;
    return;
  }
  step->Optimize();
  steps_.insert(0, step);
}

Path::Path(Expression* filter, LocationPath* path)
    : filter_(filter), path_(path) {
  SetIsContextNodeSensitive(filter->IsContextNodeSensitive());
  SetIsContextPositionSensitive(filter->IsContextPositionSensitive());
  SetIsContextSizeSensitive(filter->IsContextSizeSensitive());
}

Path::~Path() = default;

void Path::Trace(Visitor* visitor) const {
  visitor->Trace(filter_);
  visitor->Trace(path_);
  Expression::Trace(visitor);
}

Value Path::Evaluate(EvaluationContext& context) const {
  Value v = filter_->Evaluate(context);

  NodeSet& nodes = v.ModifiableNodeSet(context);
  path_->Evaluate(context, nodes);

  return v;
}

}  // namespace xpath

}  // namespace blink
