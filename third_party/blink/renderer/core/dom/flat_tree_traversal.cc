/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_node_data.h"
#include "third_party/blink/renderer/core/dom/slot_assignment.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"

namespace blink {

#if DCHECK_IS_ON()
void FlatTreeTraversal::AssertFlatTreeNodeDataUpdated(
    const Node& root,
    int& assigned_nodes_in_slot_count,
    int& nodes_which_have_assigned_slot_count) {
  for (Node& node : NodeTraversal::StartsAt(root)) {
    if (auto* element = DynamicTo<Element>(node)) {
      if (ShadowRoot* shadow_root = element->GetShadowRoot()) {
        DCHECK(!shadow_root->NeedsSlotAssignmentRecalc());
        AssertFlatTreeNodeDataUpdated(*shadow_root,
                                      assigned_nodes_in_slot_count,
                                      nodes_which_have_assigned_slot_count);
      }
    }
    if (HTMLSlotElement* slot =
            ToHTMLSlotElementIfSupportsAssignmentOrNull(node)) {
      assigned_nodes_in_slot_count += slot->AssignedNodes().size();
    }
    if (node.IsChildOfShadowHost()) {
      ShadowRoot* parent_shadow_root = node.ParentElementShadowRoot();
      DCHECK(parent_shadow_root);
      if (!parent_shadow_root->HasSlotAssignment()) {
        // |node|'s FlatTreeNodeData can be anything in this case.
        // Nothing can be checked.
        continue;
      }
      if (!node.IsSlotable()) {
        DCHECK(!node.GetFlatTreeNodeData());
        continue;
      }
      if (HTMLSlotElement* assigned_slot =
              parent_shadow_root->AssignedSlotFor(node)) {
        ++nodes_which_have_assigned_slot_count;
        DCHECK(node.GetFlatTreeNodeData());
        DCHECK_EQ(node.GetFlatTreeNodeData()->AssignedSlot(), assigned_slot);
        if (Node* previous =
                node.GetFlatTreeNodeData()->PreviousInAssignedNodes()) {
          DCHECK(previous->GetFlatTreeNodeData());
          DCHECK_EQ(previous->GetFlatTreeNodeData()->NextInAssignedNodes(),
                    node);
          DCHECK_EQ(previous->parentElement(), node.parentElement());
        }
        if (Node* next = node.GetFlatTreeNodeData()->NextInAssignedNodes()) {
          DCHECK(next->GetFlatTreeNodeData());
          DCHECK_EQ(next->GetFlatTreeNodeData()->PreviousInAssignedNodes(),
                    node);
          DCHECK_EQ(next->parentElement(), node.parentElement());
        }
      } else {
        DCHECK(!node.GetFlatTreeNodeData() ||
               node.GetFlatTreeNodeData()->IsCleared());
      }
    }
  }
}
#endif

Node* FlatTreeTraversal::TraverseChild(const Node& node,
                                       TraversalDirection direction) {
  if (auto* slot = ToHTMLSlotElementIfSupportsAssignmentOrNull(node)) {
    if (slot->AssignedNodes().empty()) {
      return direction == kTraversalDirectionForward ? slot->firstChild()
                                                     : slot->lastChild();
    }
    return direction == kTraversalDirectionForward ? slot->FirstAssignedNode()
                                                   : slot->LastAssignedNode();
  }
  Node* child;
  if (ShadowRoot* shadow_root = node.GetShadowRoot()) {
    child = direction == kTraversalDirectionForward ? shadow_root->firstChild()
                                                    : shadow_root->lastChild();
  } else {
    child = direction == kTraversalDirectionForward ? node.firstChild()
                                                    : node.lastChild();
  }
  return child;
}

Node* FlatTreeTraversal::TraverseSiblings(const Node& node,
                                          TraversalDirection direction) {
  if (node.IsChildOfShadowHost())
    return TraverseSiblingsForHostChild(node, direction);

  return direction == kTraversalDirectionForward ? node.nextSibling()
                                                 : node.previousSibling();
}

Node* FlatTreeTraversal::TraverseSiblingsForHostChild(
    const Node& node,
    TraversalDirection direction) {
  ShadowRoot* shadow_root = node.ParentElementShadowRoot();
  DCHECK(shadow_root);
  if (!shadow_root->HasSlotAssignment()) {
    // The shadow root doesn't have any slot.
    return nullptr;
  }
  shadow_root->GetSlotAssignment().RecalcAssignment();

  FlatTreeNodeData* flat_tree_node_data = node.GetFlatTreeNodeData();
  if (!flat_tree_node_data) {
    // This node has never been assigned to any slot.
    return nullptr;
  }
  if (flat_tree_node_data->AssignedSlot()) {
    return direction == kTraversalDirectionForward
               ? flat_tree_node_data->NextInAssignedNodes()
               : flat_tree_node_data->PreviousInAssignedNodes();
  }
  // This node is not assigned to any slot.
  DCHECK(!flat_tree_node_data->NextInAssignedNodes());
  DCHECK(!flat_tree_node_data->PreviousInAssignedNodes());
  return nullptr;
}

ContainerNode* FlatTreeTraversal::TraverseParent(const Node& node) {
  // This code is called extensively, so it minimizes repetitive work (such
  // as avoiding multiple calls to parentElement()).

  // TODO(hayato): Stop this hack for a pseudo element because a pseudo element
  // is not a child of its parentOrShadowHostNode() in a flat tree.
  if (node.IsPseudoElement())
    return node.ParentOrShadowHostNode();

  ContainerNode* parent_node = node.parentNode();
  if (!parent_node)
    return nullptr;

  if (Element* parent_element = DynamicTo<Element>(parent_node)) {
    if (parent_element->GetShadowRoot())
      return node.AssignedSlot();

    if (auto* parent_slot =
            ToHTMLSlotElementIfSupportsAssignmentOrNull(*parent_element)) {
      if (!parent_slot->AssignedNodes().empty())
        return nullptr;
      return parent_slot;
    }
  }

  auto* shadow_root = DynamicTo<ShadowRoot>(parent_node);
  if (!shadow_root)
    return parent_node;

  return &shadow_root->host();
}

Node* FlatTreeTraversal::ChildAt(const Node& node, unsigned index) {
  AssertPrecondition(node);
  Node* child = TraverseFirstChild(node);
  while (child && index--)
    child = NextSibling(*child);
  AssertPostcondition(child);
  return child;
}

Node* FlatTreeTraversal::NextSkippingChildren(const Node& node) {
  if (Node* next_sibling = TraverseNextSibling(node))
    return next_sibling;
  return TraverseNextAncestorSibling(node);
}

bool FlatTreeTraversal::ContainsIncludingPseudoElement(
    const ContainerNode& container,
    const Node& node) {
  AssertPrecondition(container);
  AssertPrecondition(node);
  // This can be slower than FlatTreeTraversal::contains() because we
  // can't early exit even when container doesn't have children.
  for (const Node* current = &node; current;
       current = TraverseParent(*current)) {
    if (current == &container)
      return true;
  }
  return false;
}

Node* FlatTreeTraversal::PreviousAbsoluteSibling(const Node& node) {
  if (Node* previous_sibling = TraversePreviousSibling(node))
    return previous_sibling;
  return TraversePreviousAncestorSibling(node);
}

Node* FlatTreeTraversal::PreviousAncestorSiblingPostOrder(
    const Node& current,
    const Node* stay_within) {
  DCHECK(!FlatTreeTraversal::PreviousSibling(current));
  for (Node* parent = FlatTreeTraversal::Parent(current); parent;
       parent = FlatTreeTraversal::Parent(*parent)) {
    if (parent == stay_within)
      return nullptr;
    if (Node* previous_sibling = FlatTreeTraversal::PreviousSibling(*parent))
      return previous_sibling;
  }
  return nullptr;
}

// TODO(yosin) We should consider introducing template class to share code
// between DOM tree traversal and flat tree tarversal.
Node* FlatTreeTraversal::PreviousPostOrder(const Node& current,
                                           const Node* stay_within) {
  AssertPrecondition(current);
  if (stay_within)
    AssertPrecondition(*stay_within);
  if (Node* last_child = TraverseLastChild(current)) {
    AssertPostcondition(last_child);
    return last_child;
  }
  if (current == stay_within)
    return nullptr;
  if (Node* previous_sibling = TraversePreviousSibling(current)) {
    AssertPostcondition(previous_sibling);
    return previous_sibling;
  }
  return PreviousAncestorSiblingPostOrder(current, stay_within);
}

bool FlatTreeTraversal::IsDescendantOf(const Node& node, const Node& other) {
  AssertPrecondition(node);
  AssertPrecondition(other);
  if (!HasChildren(other) || node.isConnected() != other.isConnected())
    return false;
  for (const ContainerNode* n = TraverseParent(node); n;
       n = TraverseParent(*n)) {
    if (n == other)
      return true;
  }
  return false;
}

Node* FlatTreeTraversal::CommonAncestor(const Node& node_a,
                                        const Node& node_b) {
  AssertPrecondition(node_a);
  AssertPrecondition(node_b);
  Node* result = node_a.CommonAncestor(
      node_b, [](const Node& node) { return FlatTreeTraversal::Parent(node); });
  AssertPostcondition(result);
  return result;
}

Node* FlatTreeTraversal::TraverseNextAncestorSibling(const Node& node) {
  DCHECK(!TraverseNextSibling(node));
  for (Node* parent = TraverseParent(node); parent;
       parent = TraverseParent(*parent)) {
    if (Node* next_sibling = TraverseNextSibling(*parent))
      return next_sibling;
  }
  return nullptr;
}

Node* FlatTreeTraversal::TraversePreviousAncestorSibling(const Node& node) {
  DCHECK(!TraversePreviousSibling(node));
  for (Node* parent = TraverseParent(node); parent;
       parent = TraverseParent(*parent)) {
    if (Node* previous_sibling = TraversePreviousSibling(*parent))
      return previous_sibling;
  }
  return nullptr;
}

unsigned FlatTreeTraversal::Index(const Node& node) {
  AssertPrecondition(node);
  unsigned count = 0;
  for (Node* runner = TraversePreviousSibling(node); runner;
       runner = PreviousSibling(*runner))
    ++count;
  return count;
}

unsigned FlatTreeTraversal::CountChildren(const Node& node) {
  AssertPrecondition(node);
  unsigned count = 0;
  for (Node* runner = TraverseFirstChild(node); runner;
       runner = TraverseNextSibling(*runner))
    ++count;
  return count;
}

Node* FlatTreeTraversal::LastWithin(const Node& node) {
  AssertPrecondition(node);
  Node* descendant = TraverseLastChild(node);
  for (Node* child = descendant; child; child = LastChild(*child))
    descendant = child;
  AssertPostcondition(descendant);
  return descendant;
}

Node& FlatTreeTraversal::LastWithinOrSelf(const Node& node) {
  AssertPrecondition(node);
  Node* last_descendant = LastWithin(node);
  Node& result = last_descendant ? *last_descendant : const_cast<Node&>(node);
  AssertPostcondition(&result);
  return result;
}

const Element* FlatTreeTraversal::InclusiveParentElement(const Node& node) {
  AssertPrecondition(node);
  const Element* inclusive_parent = DynamicTo<Element>(node);
  if (!inclusive_parent) {
    inclusive_parent = ParentElement(node);
  }
  AssertPostcondition(inclusive_parent);
  return inclusive_parent;
}

}  // namespace blink
