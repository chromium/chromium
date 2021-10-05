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
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"

#include <set>

namespace blink {
namespace {

// Returns the nearest non-inclusive ancestor of |node| that is display
// locked.
Element* NearestLockedExclusiveAncestor(const Node& node) {
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled() ||
      !node.isConnected() ||
      node.GetDocument()
              .GetDisplayLockDocumentState()
              .LockedDisplayLockCount() == 0 ||
      node.IsShadowRoot()) {
    return nullptr;
  }
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

const Element* NearestLockedInclusiveAncestor(const Node& node) {
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return NearestLockedExclusiveAncestor(node);
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled() ||
      !node.isConnected() ||
      node.GetDocument()
              .GetDisplayLockDocumentState()
              .LockedDisplayLockCount() == 0 ||
      node.IsShadowRoot()) {
    return nullptr;
  }
  if (auto* context = element->GetDisplayLockContext()) {
    if (context->IsLocked())
      return element;
  }
  return NearestLockedExclusiveAncestor(node);
}

Element* NearestLockedInclusiveAncestor(Node& node) {
  return const_cast<Element*>(
      NearestLockedInclusiveAncestor(static_cast<const Node&>(node)));
}

Element* NearestLockedInclusiveAncestor(const LayoutObject& object) {
  auto* node = object.GetNode();
  auto* ancestor = object.Parent();
  while (ancestor && !node) {
    node = ancestor->GetNode();
    ancestor = ancestor->Parent();
  }
  return node ? NearestLockedInclusiveAncestor(*node) : nullptr;
}

Element* NearestLockedExclusiveAncestor(const LayoutObject& object) {
  if (auto* node = object.GetNode())
    return NearestLockedExclusiveAncestor(*node);
  // Since we now navigate to an ancestor, use the inclusive version.
  if (auto* parent = object.Parent())
    return NearestLockedInclusiveAncestor(*parent);
  return nullptr;
}

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
  for (auto* ancestor = NearestLockedExclusiveAncestor(node); ancestor;
       ancestor = NearestLockedExclusiveAncestor(*ancestor)) {
    DCHECK(ancestor->GetDisplayLockContext());
    if (update_is_prevented(ancestor->GetDisplayLockContext()))
      return ancestor;
  }
  return nullptr;
}

template <typename Lambda>
const Element* LockedInclusiveAncestorPreventingUpdate(
    const Node& node,
    Lambda update_is_prevented) {
  if (auto* element = DynamicTo<Element>(node)) {
    if (auto* context = element->GetDisplayLockContext()) {
      if (update_is_prevented(context))
        return element;
    }
  }
  return LockedAncestorPreventingUpdate(node, std::move(update_is_prevented));
}

template <typename Lambda>
Element* LockedAncestorPreventingUpdate(const LayoutObject& object,
                                        Lambda update_is_prevented) {
  if (auto* ancestor = NearestLockedExclusiveAncestor(object)) {
    if (update_is_prevented(ancestor->GetDisplayLockContext()))
      return ancestor;
    return LockedAncestorPreventingUpdate(*ancestor, update_is_prevented);
  }
  return nullptr;
}

template <typename Lambda>
Element* LockedInclusiveAncestorPreventingUpdate(const LayoutObject& object,
                                                 Lambda update_is_prevented) {
  if (auto* ancestor = NearestLockedInclusiveAncestor(object)) {
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

const HeapVector<Member<Element>>
DisplayLockUtilities::ActivatableLockedInclusiveAncestors(
    const Node& node,
    DisplayLockActivationReason reason) {
  HeapVector<Member<Element>> elements_to_activate;
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

DisplayLockUtilities::ScopedForcedUpdate::Impl::Impl(
    const Range* range,
    DisplayLockContext::ForcedPhase phase)
    : node_(range->FirstNode()), phase_(phase) {
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled())
    return;
  if (!node_)
    return;

  // Selection doesn't span frames, so we don't need to worry about including
  // subframes inside the Range or multiple parent frames.
  auto* owner_node = GetFrameOwnerNode(node_);
  if (owner_node)
    parent_frame_impl_ = MakeGarbageCollected<Impl>(owner_node, phase, true);

  range->OwnerDocument().GetDisplayLockDocumentState().BeginRangeForcedScope(
      range, this);

  if (node_->GetDocument()
          .GetDisplayLockDocumentState()
          .LockedDisplayLockCount() == 0)
    return;

  // TODO(crbug.com/1256849): Move this loop to a shared iterator class so we
  //   can combine it with the one in DisplayLockDocumentState.
  // Ranges use NodeTraversal::Next to go in between their start and end nodes,
  // and will access the layout information of each of those nodes. In order to
  // ensure that each of these nodes has unlocked layout information, we have to
  // do a scoped unlock for each of those nodes by unlocking all of their flat
  // tree ancestors.
  for (Node* node = range->FirstNode(); node != range->PastLastNode();
       node = NodeTraversal::Next(*node)) {
    if (node->IsChildOfShadowHost()) {
      // This node may be slotted into another place in the flat tree, so we
      // have to do a flat tree parent traversal for it.
      for (Node* ancestor = node; ancestor;
           ancestor = FlatTreeTraversal::Parent(*ancestor)) {
        if (Element* element = DynamicTo<Element>(ancestor)) {
          if (DisplayLockContext* context = element->GetDisplayLockContext()) {
            forced_context_set_.insert(context);
          }
        }
      }
    } else {
      if (Element* element = DynamicTo<Element>(node)) {
        if (DisplayLockContext* context = element->GetDisplayLockContext()) {
          forced_context_set_.insert(context);
        }
      }
    }
  }
  for (Node* node = range->FirstNode(); node;
       node = FlatTreeTraversal::Parent(*node)) {
    if (Element* element = DynamicTo<Element>(node)) {
      if (DisplayLockContext* context = element->GetDisplayLockContext()) {
        forced_context_set_.insert(context);
      }
    }
  }
  for (DisplayLockContext* context : forced_context_set_) {
    context->NotifyForcedUpdateScopeStarted(phase_);
  }
}

DisplayLockUtilities::ScopedForcedUpdate::Impl::Impl(
    const Node* node,
    DisplayLockContext::ForcedPhase phase,
    bool include_self)
    : node_(node), phase_(phase) {
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled())
    return;

  if (!node_)
    return;

  auto* owner_node = GetFrameOwnerNode(node);
  if (owner_node)
    parent_frame_impl_ = MakeGarbageCollected<Impl>(owner_node, phase, true);

  node->GetDocument().GetDisplayLockDocumentState().BeginNodeForcedScope(
      node, include_self, this);

  if (node->GetDocument()
          .GetDisplayLockDocumentState()
          .LockedDisplayLockCount() == 0)
    return;

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
      context->NotifyForcedUpdateScopeStarted(phase_);
      forced_context_set_.insert(context);
    }
  }
}

void DisplayLockUtilities::ScopedForcedUpdate::Impl::Destroy() {
  if (!node_)
    return;
  if (RuntimeEnabledFeatures::CSSContentVisibilityEnabled())
    node_->GetDocument().GetDisplayLockDocumentState().EndForcedScope(this);
  if (parent_frame_impl_)
    parent_frame_impl_->Destroy();
  for (auto context : forced_context_set_) {
    context->NotifyForcedUpdateScopeEnded(phase_);
  }
}

void DisplayLockUtilities::ScopedForcedUpdate::Impl::
    AddForcedUpdateScopeForContext(DisplayLockContext* context) {
  auto result = forced_context_set_.insert(context);
  if (result.is_new_entry)
    context->NotifyForcedUpdateScopeStarted(phase_);
}

Element* DisplayLockUtilities::NearestHiddenMatchableInclusiveAncestor(
    Element& element) {
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled() ||
      !element.isConnected() ||
      element.GetDocument()
              .GetDisplayLockDocumentState()
              .LockedDisplayLockCount() == 0 ||
      element.IsShadowRoot()) {
    return nullptr;
  }

  if (auto* context = element.GetDisplayLockContext()) {
    if (context->GetState() == EContentVisibility::kHiddenMatchable) {
      return &element;
    }
  }

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

Element*
DisplayLockUtilities::LockedInclusiveAncestorPreventingStyleWithinTreeScope(
    const Node& node) {
  if (!node.isConnected() || node.GetDocument()
                                     .GetDisplayLockDocumentState()
                                     .LockedDisplayLockCount() == 0) {
    return nullptr;
  }

  for (Node& ancestor : NodeTraversal::InclusiveAncestorsOf(node)) {
    DCHECK(ancestor.GetTreeScope() == node.GetTreeScope());
    Element* ancestor_element = DynamicTo<Element>(ancestor);
    if (!ancestor_element)
      continue;
    if (DisplayLockContext* context =
            ancestor_element->GetDisplayLockContext()) {
      if (!context->ShouldStyleChildren())
        return ancestor_element;
    }
  }
  return nullptr;
}

const Element* DisplayLockUtilities::LockedInclusiveAncestorPreventingLayout(
    const Node& node) {
  return LockedInclusiveAncestorPreventingUpdate(
      node, [](DisplayLockContext* context) {
        return !context->ShouldLayoutChildren();
      });
}

const Element* DisplayLockUtilities::LockedInclusiveAncestorPreventingPaint(
    const Node& node) {
  return LockedInclusiveAncestorPreventingUpdate(
      node, [](DisplayLockContext* context) {
        return !context->ShouldPaintChildren();
      });
}

const Element* DisplayLockUtilities::LockedInclusiveAncestorPreventingPaint(
    const LayoutObject& object) {
  return LockedInclusiveAncestorPreventingUpdate(
      object, [](DisplayLockContext* context) {
        return !context->ShouldPaintChildren();
      });
}

Element* DisplayLockUtilities::HighestLockedInclusiveAncestor(
    const Node& node) {
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled() ||
      node.IsShadowRoot()) {
    return nullptr;
  }
  auto* node_ptr = const_cast<Node*>(&node);
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
      node.IsShadowRoot()) {
    return nullptr;
  }

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

    if (!parent)
      parent = GetFrameOwnerNode(last_node);
  }
  return locked_ancestor;
}

bool DisplayLockUtilities::IsInUnlockedOrActivatableSubtree(
    const Node& node,
    DisplayLockActivationReason activation_reason) {
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled(
          node.GetExecutionContext()) ||
      node.GetDocument()
              .GetDisplayLockDocumentState()
              .LockedDisplayLockCount() == 0 ||
      node.IsShadowRoot()) {
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
    const Node& source_node,
    IncludeSelfOrNot self) {
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled())
    return false;
  if (LocalFrameView* frame_view = source_node.GetDocument().View()) {
    if (frame_view->IsDisplayLocked())
      return true;
  }
  const Node* node = &source_node;

  // If we don't need to check self, skip to the parent immediately.
  if (self == kExcludeSelf)
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

Element* DisplayLockUtilities::LockedAncestorPreventingPaint(
    const LayoutObject& object) {
  return LockedAncestorPreventingUpdate(
      object, [](DisplayLockContext* context) {
        return !context->ShouldPaintChildren();
      });
}

Element* DisplayLockUtilities::LockedAncestorPreventingPaint(const Node& node) {
  return LockedAncestorPreventingUpdate(node, [](DisplayLockContext* context) {
    return !context->ShouldPaintChildren();
  });
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

Element* DisplayLockUtilities::LockedAncestorPreventingLayout(
    const Node& node) {
  return LockedAncestorPreventingUpdate(node, [](DisplayLockContext* context) {
    return !context->ShouldLayoutChildren();
  });
}

Element* DisplayLockUtilities::LockedAncestorPreventingStyle(const Node& node) {
  return LockedAncestorPreventingUpdate(node, [](DisplayLockContext* context) {
    return !context->ShouldStyleChildren();
  });
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

bool DisplayLockUtilities::RevealHiddenUntilFoundAncestors(const Node& node) {
  // Since setting the open attribute fires mutation events which could mess
  // with the FlatTreeTraversal iterator, we should first iterate details
  // elements to open and then open them all.
  VectorOf<HTMLElement> elements_to_reveal;

  for (Node& parent : FlatTreeTraversal::AncestorsOf(node)) {
    if (HTMLElement* element = DynamicTo<HTMLElement>(parent)) {
      if (EqualIgnoringASCIICase(
              element->FastGetAttribute(html_names::kHiddenAttr),
              "until-found")) {
        elements_to_reveal.push_back(element);
      }
    }
  }

  for (HTMLElement* element : elements_to_reveal) {
    element->removeAttribute(html_names::kHiddenAttr);
  }

  // TODO(crbug.com/1055002): Fire the beforematch event here on all
  //   |elements_to_reveal|.

  return elements_to_reveal.size();
}

}  // namespace blink
