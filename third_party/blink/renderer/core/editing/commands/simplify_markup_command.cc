/*
 * Copyright (C) 2012 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/commands/simplify_markup_command.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

SimplifyMarkupCommand::SimplifyMarkupCommand(Document& document,
                                             Node* first_node,
                                             Node* node_after_last)
    : CompositeEditCommand(document),
      first_node_(first_node),
      node_after_last_(node_after_last) {}

void SimplifyMarkupCommand::DoApply(EditingState* editing_state) {
  ContainerNode* root_node = first_node_->parentNode();
  HeapVector<Member<ContainerNode>> nodes_to_remove;

  // Walk through the inserted nodes, to see if there are elements that could be
  // removed without affecting the style. The goal is to produce leaner markup
  // even when starting from a verbose fragment.
  // We look at inline elements as well as non top level divs that don't have
  // attributes.
  for (Node* node = first_node_.Get(); node && node != node_after_last_;
       node = NodeTraversal::Next(*node)) {
    if (node->hasChildren() || (node->IsTextNode() && node->nextSibling()))
      continue;

    ContainerNode* const starting_node = node->parentNode();
    if (!starting_node)
      continue;
    const ComputedStyle* starting_style = starting_node->GetComputedStyle();
    if (!starting_style)
      continue;
    ContainerNode* current_node = starting_node;
    ContainerNode* top_node_with_starting_style = nullptr;
    while (current_node != root_node) {
      if (current_node->parentNode() != root_node &&
          IsRemovableBlock(current_node))
        nodes_to_remove.push_back(current_node);

      current_node = current_node->parentNode();
      if (!current_node)
        break;

      if (!current_node->GetLayoutObject() ||
          !current_node->GetLayoutObject()->IsLayoutInline() ||
          To<LayoutInline>(current_node->GetLayoutObject())
              ->AlwaysCreateLineBoxes())
        continue;

      if (current_node->firstChild() != current_node->lastChild()) {
        top_node_with_starting_style = nullptr;
        break;
      }

      if (!current_node->GetComputedStyle()
               ->VisualInvalidationDiff(GetDocument(), *starting_style)
               .HasDifference())
        top_node_with_starting_style = current_node;
    }
    if (top_node_with_starting_style) {
      for (Node& ancestor_node :
           NodeTraversal::InclusiveAncestorsOf(*starting_node)) {
        if (ancestor_node == top_node_with_starting_style)
          break;
        nodes_to_remove.push_back(static_cast<ContainerNode*>(&ancestor_node));
      }
    }
  }

  // we perform all the DOM mutations at once.
  for (wtf_size_t i = 0; i < nodes_to_remove.size(); ++i) {
    // FIXME: We can do better by directly moving children from
    // nodesToRemove[i].
    int num_pruned_ancestors =
        PruneSubsequentAncestorsToRemove(nodes_to_remove, i, editing_state);
    if (editing_state->IsAborted())
      return;
    if (num_pruned_ancestors < 0)
      continue;
    RemoveNodePreservingChildren(nodes_to_remove[i], editing_state,
                                 kAssumeContentIsAlwaysEditable);
    if (editing_state->IsAborted())
      return;
    i += num_pruned_ancestors;
  }
}

int SimplifyMarkupCommand::PruneSubsequentAncestorsToRemove(
    HeapVector<Member<ContainerNode>>& nodes_to_remove,
    wtf_size_t start_node_index,
    EditingState* editing_state) {
  wtf_size_t past_last_node_to_remove = start_node_index + 1;
  for (; past_last_node_to_remove < nodes_to_remove.size();
       ++past_last_node_to_remove) {
    if (nodes_to_remove[past_last_node_to_remove - 1]->parentNode() !=
        nodes_to_remove[past_last_node_to_remove])
      break;
    DCHECK_EQ(nodes_to_remove[past_last_node_to_remove]->firstChild(),
              nodes_to_remove[past_last_node_to_remove]->lastChild());
  }

  ContainerNode* highest_ancestor_to_remove =
      nodes_to_remove[past_last_node_to_remove - 1].Get();
  ContainerNode* parent = highest_ancestor_to_remove->parentNode();
  if (!parent)  // Parent has already been removed.
    return -1;

  if (past_last_node_to_remove == start_node_index + 1)
    return 0;

  RemoveNode(nodes_to_remove[start_node_index], editing_state,
             kAssumeContentIsAlwaysEditable);
  if (editing_state->IsAborted())
    return -1;
  InsertNodeBefore(nodes_to_remove[start_node_index],
                   highest_ancestor_to_remove, editing_state,
                   kAssumeContentIsAlwaysEditable);
  if (editing_state->IsAborted())
    return -1;
  RemoveNode(highest_ancestor_to_remove, editing_state,
             kAssumeContentIsAlwaysEditable);
  if (editing_state->IsAborted())
    return -1;

  return past_last_node_to_remove - start_node_index - 1;
}

void SimplifyMarkupCommand::Trace(Visitor* visitor) const {
  visitor->Trace(first_node_);
  visitor->Trace(node_after_last_);
  CompositeEditCommand::Trace(visitor);
}

}  // namespace blink
