// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_boundary.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"

#include <set>

namespace blink {
namespace {

// Returns the frame owner node for the frame that contains the given child, if
// one exists. Returns nullptr otherwise.
Node* GetFrameOwnerNode(const Node* child) {
  if (!child || !child->GetDocument().GetFrame() ||
      !child->GetDocument().GetFrame()->OwnerLayoutObject()) {
    return nullptr;
  }
  return child->GetDocument().GetFrame()->OwnerLayoutObject()->GetNode();
}

void PopulateAncestorContexts(Node* node,
                              std::set<DisplayLockContext*>* contexts) {
  DCHECK(node);
  for (Node& ancestor : FlatTreeTraversal::InclusiveAncestorsOf(*node)) {
    auto* ancestor_element = DynamicTo<Element>(ancestor);
    if (!ancestor_element)
      continue;
    if (auto* context = ancestor_element->GetDisplayLockContext())
      contexts->insert(context);
  }
}

template <typename Lambda>
Element* LockedAncestorPreventingUpdate(const Node& node,
                                        Lambda update_is_prevented) {
  for (auto* ancestor =
           DisplayLockUtilities::NearestLockedExclusiveAncestor(node);
       ancestor;
       ancestor =
           DisplayLockUtilities::NearestLockedExclusiveAncestor(*ancestor)) {
    DCHECK(ancestor->GetDisplayLockContext());
    if (update_is_prevented(ancestor->GetDisplayLockContext()))
      return ancestor;
  }
  return nullptr;
}

template <typename Lambda>
Element* LockedAncestorPreventingUpdate(const LayoutObject& object,
                                        Lambda update_is_prevented) {
  if (auto* ancestor =
          DisplayLockUtilities::NearestLockedExclusiveAncestor(object)) {
    if (update_is_prevented(ancestor->GetDisplayLockContext()))
      return ancestor;
    return LockedAncestorPreventingUpdate(*ancestor, update_is_prevented);
  }
  return nullptr;
}

}  // namespace

bool DisplayLockUtilities::ActivateFindInPageMatchRangeIfNeeded(
    const EphemeralRangeInFlatTree& range) {
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled())
    return false;
  DCHECK(!range.IsNull());
  DCHECK(!range.IsCollapsed());
  if (range.GetDocument()
          .GetDisplayLockDocumentState()
          .LockedDisplayLockCount() ==
      range.GetDocument()
          .GetDisplayLockDocumentState()
          .DisplayLockBlockingAllActivationCount())
    return false;
  // Find-in-page matches can't span multiple block-level elements (because the
  // text will be broken by newlines between blocks), so first we find the
  // block-level element which contains the match.
  // This means we only need to traverse up from one node in the range, in this
  // case we are traversing from the start position of the range.
  Element* enclosing_block =
      EnclosingBlock(range.StartPosition(), kCannotCrossEditingBoundary);
  // Note that we don't check the `range.EndPosition()` since we just activate
  // the beginning of the range. In find-in-page cases, the end position is the
  // same since the matches cannot cross block boundaries. However, in
  // scroll-to-text, the range might be different, but we still just activate
  // the beginning of the range. See
  // https://github.com/WICG/display-locking/issues/125 for more details.
  DCHECK(enclosing_block);
  return enclosing_block->ActivateDisplayLockIfNeeded(
      DisplayLockActivationReason::kFindInPage);
}

bool DisplayLockUtilities::ActivateSelectionRangeIfNeeded(
    const EphemeralRangeInFlatTree& range) {
  if (range.IsNull() || range.IsCollapsed())
    return false;
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled() ||
      range.GetDocument()
              .GetDisplayLockDocumentState()
              .LockedDisplayLockCount() ==
          range.GetDocument()
              .GetDisplayLockDocumentState()
              .DisplayLockBlockingAllActivationCount())
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
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled() ||
      node.GetDocument()
              .GetDisplayLockDocumentState()
              .LockedDisplayLockCount() ==
          node.GetDocument()
              .GetDisplayLockDocumentState()
              .DisplayLockBlockingAllActivationCount())
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

DisplayLockUtilities::ScopedForcedUpdate::Impl::Impl(const Node* node,
                                                     bool include_self)
    : node_(node) {
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled())
    return;

  auto* owner_node = GetFrameOwnerNode(node);
  if (owner_node)
    parent_frame_impl_ = MakeGarbageCollected<Impl>(owner_node, true);

  node->GetDocument().GetDisplayLockDocumentState().BeginNodeForcedScope(
      node, include_self, this);

  if (node->GetDocument()
          .GetDisplayLockDocumentState()
          .LockedDisplayLockCount() == 0)
    return;
  const_cast<Node*>(node)->UpdateDistributionForFlatTreeTraversal();

  // Get the right ancestor view. Only use inclusive ancestors if the node
  // itself is locked and it prevents self layout, or if |include_self| is true.
  // If self layout is not prevented, we don't need to force the subtree layout,
  // so use exclusive ancestors in that case.
  auto ancestor_view = [node, include_self] {
    if (auto* element = DynamicTo<Element>(node)) {
      auto* context = element->GetDisplayLockContext();
      if (context && include_self)
        return FlatTreeTraversal::InclusiveAncestorsOf(*node);
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
      context->NotifyForcedUpdateScopeStarted();
      forced_context_set_.insert(context);
    }
  }
}

void DisplayLockUtilities::ScopedForcedUpdate::Impl::Destroy() {
  if (RuntimeEnabledFeatures::CSSContentVisibilityEnabled())
    node_->GetDocument().GetDisplayLockDocumentState().EndNodeForcedScope(this);
  if (parent_frame_impl_)
    parent_frame_impl_->Destroy();
  for (auto context : forced_context_set_) {
    context->NotifyForcedUpdateScopeEnded();
  }
}

void DisplayLockUtilities::ScopedForcedUpdate::Impl::
    AddForcedUpdateScopeForContext(DisplayLockContext* context) {
  auto result = forced_context_set_.insert(context);
  if (result.is_new_entry)
    context->NotifyForcedUpdateScopeStarted();
}

const Element* DisplayLockUtilities::NearestLockedInclusiveAncestor(
    const Node& node) {
  const_cast<Node*>(&node)->UpdateDistributionForFlatTreeTraversal();
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return NearestLockedExclusiveAncestor(node);
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled() ||
      !node.isConnected() ||
      node.GetDocument()
              .GetDisplayLockDocumentState()
              .LockedDisplayLockCount() == 0 ||
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

Element* DisplayLockUtilities::NearestHiddenMatchableInclusiveAncestor(
    Element& element) {
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled() ||
      !element.isConnected() ||
      element.GetDocument()
              .GetDisplayLockDocumentState()
              .LockedDisplayLockCount() == 0 ||
      !element.CanParticipateInFlatTree()) {
    return nullptr;
  }

  if (auto* context = element.GetDisplayLockContext()) {
    if (context->GetState() == EContentVisibility::kHiddenMatchable) {
      return &element;
    }
  }

  element.UpdateDistributionForFlatTreeTraversal();
  // TODO(crbug.com/924550): Once we figure out a more efficient way to
  // determine whether we're inside a locked subtree or not, change this.
  for (Node& ancestor : FlatTreeTraversal::AncestorsOf(element)) {
    auto* ancestor_element = DynamicTo<Element>(ancestor);
    if (!ancestor_element)
      continue;
    if (auto* context = ancestor_element->GetDisplayLockContext()) {
      if (context->GetState() == EContentVisibility::kHiddenMatchable) {
        return ancestor_element;
      }
    }
  }
  return nullptr;
}

Element* DisplayLockUtilities::NearestLockedExclusiveAncestor(
    const Node& node) {
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled() ||
      !node.isConnected() ||
      node.GetDocument()
              .GetDisplayLockDocumentState()
              .LockedDisplayLockCount() == 0 ||
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
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled() ||
      !node.CanParticipateInFlatTree()) {
    return nullptr;
  }
  auto* node_ptr = const_cast<Node*>(&node);
  node_ptr->UpdateDistributionForFlatTreeTraversal();
  // If the exclusive result exists, then that's higher than this node, so
  // return it.
  if (auto* result = HighestLockedExclusiveAncestor(node))
    return result;

  // Otherwise, we know the node is not in a locked subtree, so the only
  // other possibility is that the node itself is locked.
  auto* element = DynamicTo<Element>(node_ptr);
  if (element && element->GetDisplayLockContext() &&
      element->GetDisplayLockContext()->IsLocked()) {
    return element;
  }
  return nullptr;
}

Element* DisplayLockUtilities::HighestLockedExclusiveAncestor(
    const Node& node) {
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled() ||
      !node.CanParticipateInFlatTree()) {
    return nullptr;
  }
  const_cast<Node*>(&node)->UpdateDistributionForFlatTreeTraversal();

  Node* parent = FlatTreeTraversal::Parent(node);
  Element* locked_ancestor = nullptr;
  while (parent) {
    auto* locked_candidate = NearestLockedInclusiveAncestor(*parent);
    auto* last_node = parent;
    if (locked_candidate) {
      locked_ancestor = locked_candidate;
      parent = FlatTreeTraversal::Parent(*parent);
    } else {
      parent = nullptr;
    }

    if (!parent) {
      parent = GetFrameOwnerNode(last_node);
      if (parent)
        parent->UpdateDistributionForFlatTreeTraversal();
    }
  }
  return locked_ancestor;
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

bool DisplayLockUtilities::IsInUnlockedOrActivatableSubtree(
    const Node& node,
    DisplayLockActivationReason activation_reason) {
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled(
          node.GetExecutionContext()) ||
      node.GetDocument()
              .GetDisplayLockDocumentState()
              .LockedDisplayLockCount() == 0 ||
      node.GetDocument()
              .GetDisplayLockDocumentState()
              .DisplayLockBlockingAllActivationCount() == 0 ||
      !node.CanParticipateInFlatTree()) {
    return true;
  }

  for (auto* element = NearestLockedExclusiveAncestor(node); element;
       element = NearestLockedExclusiveAncestor(*element)) {
    if (!element->GetDisplayLockContext()->IsActivatable(activation_reason)) {
      return false;
    }
  }
  return true;
}

bool DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(
    const Node& source_node) {
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled())
    return false;
  const Node* node = &source_node;
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

void DisplayLockUtilities::ElementLostFocus(Element* element) {
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled() ||
      (element && element->GetDocument()
                          .GetDisplayLockDocumentState()
                          .DisplayLockCount() == 0))
    return;
  for (; element; element = FlatTreeTraversal::ParentElement(*element)) {
    auto* context = element->GetDisplayLockContext();
    if (context)
      context->NotifySubtreeLostFocus();
  }
}
void DisplayLockUtilities::ElementGainedFocus(Element* element) {
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled() ||
      (element && element->GetDocument()
                          .GetDisplayLockDocumentState()
                          .DisplayLockCount() == 0))
    return;

  for (; element; element = FlatTreeTraversal::ParentElement(*element)) {
    auto* context = element->GetDisplayLockContext();
    if (context)
      context->NotifySubtreeGainedFocus();
  }
}

void DisplayLockUtilities::SelectionChanged(
    const EphemeralRangeInFlatTree& old_selection,
    const EphemeralRangeInFlatTree& new_selection) {
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled() ||
      (!old_selection.IsNull() && old_selection.GetDocument()
                                          .GetDisplayLockDocumentState()
                                          .DisplayLockCount() == 0) ||
      (!new_selection.IsNull() && new_selection.GetDocument()
                                          .GetDisplayLockDocumentState()
                                          .DisplayLockCount() == 0))
    return;

  TRACE_EVENT0("blink", "DisplayLockUtilities::SelectionChanged");
  std::set<Node*> old_nodes;
  for (Node& node : old_selection.Nodes())
    old_nodes.insert(&node);

  std::set<Node*> new_nodes;
  for (Node& node : new_selection.Nodes())
    new_nodes.insert(&node);

  std::set<DisplayLockContext*> lost_selection_contexts;
  std::set<DisplayLockContext*> gained_selection_contexts;

  // Skip common nodes and extract contexts from nodes that lost selection and
  // contexts from nodes that gained selection.
  // This is similar to std::set_symmetric_difference except that we need to
  // know which set the resulting item came from. In this version, we simply do
  // the relevant operation on each of the items instead of storing the
  // difference.
  std::set<Node*>::iterator old_it = old_nodes.begin();
  std::set<Node*>::iterator new_it = new_nodes.begin();
  while (old_it != old_nodes.end() && new_it != new_nodes.end()) {
    // Compare the addresses since that's how the nodes are ordered in the set.
    if (*old_it < *new_it) {
      PopulateAncestorContexts(*old_it++, &lost_selection_contexts);
    } else if (*old_it > *new_it) {
      PopulateAncestorContexts(*new_it++, &gained_selection_contexts);
    } else {
      ++old_it;
      ++new_it;
    }
  }
  while (old_it != old_nodes.end())
    PopulateAncestorContexts(*old_it++, &lost_selection_contexts);
  while (new_it != new_nodes.end())
    PopulateAncestorContexts(*new_it++, &gained_selection_contexts);

  // Now do a similar thing with contexts: skip common ones, and mark the ones
  // that lost selection or gained selection as such.
  std::set<DisplayLockContext*>::iterator lost_it =
      lost_selection_contexts.begin();
  std::set<DisplayLockContext*>::iterator gained_it =
      gained_selection_contexts.begin();
  while (lost_it != lost_selection_contexts.end() &&
         gained_it != gained_selection_contexts.end()) {
    if (*lost_it < *gained_it) {
      (*lost_it++)->NotifySubtreeLostSelection();
    } else if (*lost_it > *gained_it) {
      (*gained_it++)->NotifySubtreeGainedSelection();
    } else {
      ++lost_it;
      ++gained_it;
    }
  }
  while (lost_it != lost_selection_contexts.end())
    (*lost_it++)->NotifySubtreeLostSelection();
  while (gained_it != gained_selection_contexts.end())
    (*gained_it++)->NotifySubtreeGainedSelection();
}

void DisplayLockUtilities::SelectionRemovedFromDocument(Document& document) {
  document.GetDisplayLockDocumentState().NotifySelectionRemoved();
}

Element* DisplayLockUtilities::LockedAncestorPreventingPrePaint(
    const LayoutObject& object) {
  return LockedAncestorPreventingUpdate(
      object, [](DisplayLockContext* context) {
        return !context->ShouldPrePaintChildren();
      });
}

Element* DisplayLockUtilities::LockedAncestorPreventingLayout(
    const LayoutObject& object) {
  return LockedAncestorPreventingUpdate(
      object, [](DisplayLockContext* context) {
        return !context->ShouldLayoutChildren();
      });
}

Element* DisplayLockUtilities::LockedAncestorPreventingStyle(const Node& node) {
  return LockedAncestorPreventingUpdate(node, [](DisplayLockContext* context) {
    return !context->ShouldStyleChildren();
  });
}

bool DisplayLockUtilities::UpdateStyleAndLayoutForRangeIfNeeded(
    const EphemeralRangeInFlatTree& range,
    DisplayLockActivationReason reason) {
  if (range.IsNull() || range.IsCollapsed())
    return false;
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled() ||
      range.GetDocument()
              .GetDisplayLockDocumentState()
              .LockedDisplayLockCount() ==
          range.GetDocument()
              .GetDisplayLockDocumentState()
              .DisplayLockBlockingAllActivationCount())
    return false;
  HeapVector<Member<DisplayLockContext>> forced_context_list_;
  for (Node& node : range.Nodes()) {
    for (Element* locked_activatable_ancestor :
         DisplayLockUtilities::ActivatableLockedInclusiveAncestors(node,
                                                                   reason)) {
      DCHECK(locked_activatable_ancestor->GetDisplayLockContext());
      DCHECK(locked_activatable_ancestor->GetDisplayLockContext()->IsLocked());
      auto* context = locked_activatable_ancestor->GetDisplayLockContext();
      // TODO(vmpstr): Clean this up not to call
      // |NotifyForcedUpdateScopeStarted()| directly.
      context->NotifyForcedUpdateScopeStarted();
      forced_context_list_.push_back(context);
    }
  }
  if (!forced_context_list_.IsEmpty()) {
    range.GetDocument().UpdateStyleAndLayout(
        DocumentUpdateReason::kDisplayLock);
  }
  for (auto context : forced_context_list_) {
    context->NotifyForcedUpdateScopeEnded();
  }
  return !forced_context_list_.IsEmpty();
}

bool DisplayLockUtilities::PrePaintBlockedInParentFrame(LayoutView* view) {
  auto* owner = view->GetFrameView()->GetFrame().OwnerLayoutObject();
  if (!owner)
    return false;

  auto* element = NearestLockedInclusiveAncestor(*owner);
  while (element) {
    if (!element->GetDisplayLockContext()->ShouldPrePaintChildren())
      return true;
    element = NearestLockedExclusiveAncestor(*element);
  }
  return false;
}

bool DisplayLockUtilities::IsAutoWithoutLayout(const LayoutObject& object) {
  auto* context = object.GetDisplayLockContext();
  if (!context)
    return false;
  return !context->IsLocked() && context->IsAuto() &&
         !context->HadLifecycleUpdateSinceLastUnlock();
}

}  // namespace blink
