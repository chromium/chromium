// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_boundary.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"

namespace blink {
namespace {

// Returns the frame owner node for the frame that contains the given child, if
// one exists. Returns nullptr otherwise.
const Node* GetFrameOwnerNode(const Node* child) {
  if (!child || !child->GetDocument().GetFrame() ||
      !child->GetDocument().GetFrame()->OwnerLayoutObject()) {
    return nullptr;
  }
  return child->GetDocument().GetFrame()->OwnerLayoutObject()->GetNode();
}

bool UpdateStyleAndLayoutForRangeIfNeeded(const EphemeralRangeInFlatTree& range,
                                          DisplayLockActivationReason reason) {
  if (range.IsNull() || range.IsCollapsed())
    return false;
  if (!RuntimeEnabledFeatures::DisplayLockingEnabled(&range.GetDocument()) ||
      range.GetDocument().LockedDisplayLockCount() ==
          range.GetDocument().ActivationBlockingDisplayLockCount())
    return false;
  Vector<DisplayLockContext::ScopedForcedUpdate> scoped_forced_update_list_;
  for (Node& node : range.Nodes()) {
    for (Element* locked_activatable_ancestor :
         DisplayLockUtilities::ActivatableLockedInclusiveAncestors(node,
                                                                   reason)) {
      DCHECK(locked_activatable_ancestor->GetDisplayLockContext());
      DCHECK(locked_activatable_ancestor->GetDisplayLockContext()->IsLocked());
      if (locked_activatable_ancestor->GetDisplayLockContext()->UpdateForced())
        break;
      scoped_forced_update_list_.push_back(
          locked_activatable_ancestor->GetDisplayLockContext()
              ->GetScopedForcedUpdate());
    }
  }
  if (!scoped_forced_update_list_.IsEmpty())
    range.GetDocument().UpdateStyleAndLayout();
  return !scoped_forced_update_list_.IsEmpty();
}

}  // namespace

bool DisplayLockUtilities::ActivateFindInPageMatchRangeIfNeeded(
    const EphemeralRangeInFlatTree& range) {
  if (!RuntimeEnabledFeatures::DisplayLockingEnabled(&range.GetDocument()))
    return false;
  DCHECK(!range.IsNull());
  DCHECK(!range.IsCollapsed());
  if (range.GetDocument().LockedDisplayLockCount() ==
      range.GetDocument().ActivationBlockingDisplayLockCount())
    return false;
  // Find-in-page matches can't span multiple block-level elements (because the
  // text will be broken by newlines between blocks), so first we find the
  // block-level element which contains the match.
  // This means we only need to traverse up from one node in the range, in this
  // case we are traversing from the start position of the range.
  Element* enclosing_block =
      EnclosingBlock(range.StartPosition(), kCannotCrossEditingBoundary);
  DCHECK(enclosing_block);
  DCHECK_EQ(enclosing_block,
            EnclosingBlock(range.EndPosition(), kCannotCrossEditingBoundary));
  return enclosing_block->ActivateDisplayLockIfNeeded(
      DisplayLockActivationReason::kFindInPage);
}

bool DisplayLockUtilities::ActivateSelectionRangeIfNeeded(
    const EphemeralRangeInFlatTree& range) {
  if (range.IsNull() || range.IsCollapsed())
    return false;
  if (!RuntimeEnabledFeatures::DisplayLockingEnabled(&range.GetDocument()) ||
      range.GetDocument().LockedDisplayLockCount() ==
          range.GetDocument().ActivationBlockingDisplayLockCount())
    return false;
  UpdateStyleAndLayoutForRangeIfNeeded(range,
                                       DisplayLockActivationReason::kSelection);
  HeapHashSet<Member<Element>> elements_to_activate;
  for (Node& node : range.Nodes()) {
    DCHECK(!node.GetDocument().NeedsLayoutTreeUpdateForNode(node));
    const ComputedStyle* style = node.GetComputedStyle();
    if (!style || style->UserSelect() == EUserSelect::kNone)
      continue;
    if (auto* nearest_locked_ancestor = NearestLockedExclusiveAncestor(node))
      elements_to_activate.insert(nearest_locked_ancestor);
  }
  for (Element* element : elements_to_activate) {
    element->ActivateDisplayLockIfNeeded(
        DisplayLockActivationReason::kSelection);
  }
  return !elements_to_activate.IsEmpty();
}

const HeapVector<Member<Element>>
DisplayLockUtilities::ActivatableLockedInclusiveAncestors(
    const Node& node,
    DisplayLockActivationReason reason) {
  HeapVector<Member<Element>> elements_to_activate;
  const_cast<Node*>(&node)->UpdateDistributionForFlatTreeTraversal();
  if (!RuntimeEnabledFeatures::DisplayLockingEnabled(
          node.GetExecutionContext()) ||
      node.GetDocument().LockedDisplayLockCount() ==
          node.GetDocument().ActivationBlockingDisplayLockCount())
    return elements_to_activate;

  for (Node& ancestor : FlatTreeTraversal::InclusiveAncestorsOf(node)) {
    auto* ancestor_element = DynamicTo<Element>(ancestor);
    if (!ancestor_element)
      continue;
    if (auto* context = ancestor_element->GetDisplayLockContext()) {
      if (!context->IsLocked())
        continue;
      if (!context->IsActivatable(reason)) {
        // If we find a non-activatable locked ancestor, then we shouldn't
        // activate anything.
        elements_to_activate.clear();
        return elements_to_activate;
      }
      elements_to_activate.push_back(ancestor_element);
    }
  }
  return elements_to_activate;
}

DisplayLockUtilities::ScopedChainForcedUpdate::ScopedChainForcedUpdate(
    const Node* node,
    bool include_self) {
  if (!RuntimeEnabledFeatures::DisplayLockingEnabled(
          node->GetExecutionContext()))
    return;

  CreateParentFrameScopeIfNeeded(node);

  if (node->GetDocument().LockedDisplayLockCount() == 0)
    return;
  const_cast<Node*>(node)->UpdateDistributionForFlatTreeTraversal();

  // Get the right ancestor view. Only use inclusive ancestors if the node
  // itself is locked and it prevents self layout, or if |include_self| is true.
  // If self layout is not prevented, we don't need to force the subtree layout,
  // so use exclusive ancestors in that case.
  auto ancestor_view = [node, include_self] {
    if (auto* element = DynamicTo<Element>(node)) {
      auto* context = element->GetDisplayLockContext();
      if (context && (include_self || !context->ShouldLayout(
                                          DisplayLockLifecycleTarget::kSelf))) {
        return FlatTreeTraversal::InclusiveAncestorsOf(*node);
      }
    }
    return FlatTreeTraversal::AncestorsOf(*node);
  }();

  // TODO(vmpstr): This is somewhat inefficient, since we would pay the cost
  // of traversing the ancestor chain even for nodes that are not in the
  // locked subtree. We need to figure out if there is a supplementary
  // structure that we can use to quickly identify nodes that are in the
  // locked subtree.
  for (Node& ancestor : ancestor_view) {
    auto* ancestor_node = DynamicTo<Element>(ancestor);
    if (!ancestor_node)
      continue;
    if (auto* context = ancestor_node->GetDisplayLockContext()) {
      if (context->UpdateForced())
        break;
      scoped_update_forced_list_.push_back(context->GetScopedForcedUpdate());
    }
  }
}

void DisplayLockUtilities::ScopedChainForcedUpdate::
    CreateParentFrameScopeIfNeeded(const Node* node) {
  auto* owner_node = GetFrameOwnerNode(node);
  if (owner_node) {
    parent_frame_scope_ =
        std::make_unique<ScopedChainForcedUpdate>(owner_node, true);
  }
}

const Element* DisplayLockUtilities::NearestLockedInclusiveAncestor(
    const Node& node) {
  const_cast<Node*>(&node)->UpdateDistributionForFlatTreeTraversal();
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return NearestLockedExclusiveAncestor(node);
  if (!RuntimeEnabledFeatures::DisplayLockingEnabled(
          node.GetExecutionContext()) ||
      !node.isConnected() || node.GetDocument().LockedDisplayLockCount() == 0 ||
      !node.CanParticipateInFlatTree()) {
    return nullptr;
  }
  if (auto* context = element->GetDisplayLockContext()) {
    if (context->IsLocked())
      return element;
  }
  return NearestLockedExclusiveAncestor(node);
}

Element* DisplayLockUtilities::NearestLockedInclusiveAncestor(Node& node) {
  return const_cast<Element*>(
      NearestLockedInclusiveAncestor(static_cast<const Node&>(node)));
}

Element* DisplayLockUtilities::NearestLockedExclusiveAncestor(
    const Node& node) {
  if (!RuntimeEnabledFeatures::DisplayLockingEnabled(
          node.GetExecutionContext()) ||
      !node.isConnected() || node.GetDocument().LockedDisplayLockCount() == 0 ||
      !node.CanParticipateInFlatTree()) {
    return nullptr;
  }
  const_cast<Node*>(&node)->UpdateDistributionForFlatTreeTraversal();
  // TODO(crbug.com/924550): Once we figure out a more efficient way to
  // determine whether we're inside a locked subtree or not, change this.
  for (Node& ancestor : FlatTreeTraversal::AncestorsOf(node)) {
    auto* ancestor_element = DynamicTo<Element>(ancestor);
    if (!ancestor_element)
      continue;
    if (auto* context = ancestor_element->GetDisplayLockContext()) {
      if (context->IsLocked())
        return ancestor_element;
    }
  }
  return nullptr;
}

Element* DisplayLockUtilities::HighestLockedInclusiveAncestor(
    const Node& node) {
  if (!RuntimeEnabledFeatures::DisplayLockingEnabled(
          node.GetExecutionContext()) ||
      node.GetDocument().LockedDisplayLockCount() == 0 ||
      !node.CanParticipateInFlatTree()) {
    return nullptr;
  }
  const_cast<Node*>(&node)->UpdateDistributionForFlatTreeTraversal();
  Element* locked_ancestor = nullptr;
  for (Node& ancestor : FlatTreeTraversal::InclusiveAncestorsOf(node)) {
    auto* ancestor_node = DynamicTo<Element>(ancestor);
    if (!ancestor_node)
      continue;
    if (auto* context = ancestor_node->GetDisplayLockContext()) {
      if (context->IsLocked())
        locked_ancestor = ancestor_node;
    }
  }
  return locked_ancestor;
}

Element* DisplayLockUtilities::HighestLockedExclusiveAncestor(
    const Node& node) {
  if (!RuntimeEnabledFeatures::DisplayLockingEnabled(
          node.GetExecutionContext()) ||
      node.GetDocument().LockedDisplayLockCount() == 0 ||
      !node.CanParticipateInFlatTree()) {
    return nullptr;
  }
  const_cast<Node*>(&node)->UpdateDistributionForFlatTreeTraversal();

  if (Node* parent = FlatTreeTraversal::Parent(node))
    return HighestLockedInclusiveAncestor(*parent);
  return nullptr;
}

Element* DisplayLockUtilities::NearestLockedInclusiveAncestor(
    const LayoutObject& object) {
  auto* node = object.GetNode();
  auto* ancestor = object.Parent();
  while (ancestor && !node) {
    node = ancestor->GetNode();
    ancestor = ancestor->Parent();
  }
  return node ? NearestLockedInclusiveAncestor(*node) : nullptr;
}

Element* DisplayLockUtilities::NearestLockedExclusiveAncestor(
    const LayoutObject& object) {
  if (auto* node = object.GetNode())
    return NearestLockedExclusiveAncestor(*node);
  // Since we now navigate to an ancestor, use the inclusive version.
  if (auto* parent = object.Parent())
    return NearestLockedInclusiveAncestor(*parent);
  return nullptr;
}

bool DisplayLockUtilities::IsInNonActivatableLockedSubtree(const Node& node) {
  if (!RuntimeEnabledFeatures::DisplayLockingEnabled(
          node.GetExecutionContext()) ||
      node.GetDocument().LockedDisplayLockCount() == 0 ||
      node.GetDocument().ActivationBlockingDisplayLockCount() == 0 ||
      !node.CanParticipateInFlatTree()) {
    return false;
  }

  for (auto* element = NearestLockedExclusiveAncestor(node); element;
       element = NearestLockedExclusiveAncestor(*element)) {
    if (!element->GetDisplayLockContext()->IsActivatable(
            DisplayLockActivationReason::kAny)) {
      return true;
    }
  }
  return false;
}

bool DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(
    const Node& source_node) {
  if (!RuntimeEnabledFeatures::DisplayLockingEnabled(
          source_node.GetExecutionContext()))
    return false;
  const Node* node = &source_node;

  // Special case self-node checking.
  auto* element = DynamicTo<Element>(node);
  if (element && node->GetDocument().LockedDisplayLockCount()) {
    auto* context = element->GetDisplayLockContext();
    if (context && !context->ShouldLayout(DisplayLockLifecycleTarget::kSelf))
      return true;
  }
  const_cast<Node*>(node)->UpdateDistributionForFlatTreeTraversal();

  // Since we handled the self-check above, we need to do inclusive checks
  // starting from the parent.
  node = FlatTreeTraversal::Parent(*node);
  // If we don't have a flat-tree parent, get the |source_node|'s owner node
  // instead.
  if (!node)
    node = GetFrameOwnerNode(&source_node);

  while (node) {
    if (NearestLockedInclusiveAncestor(*node))
      return true;
    node = GetFrameOwnerNode(node);
  }
  return false;
}

}  // namespace blink
