// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_group_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_boundary.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/html_details_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

DisplayLockUtilities::LockCheckMemoizationScope*
    DisplayLockUtilities::memoizer_ = nullptr;

namespace {

template <class Traversal = FlatTreeTraversal>
class AncestorTraversal {
  STACK_ALLOCATED();

 public:
  class Iterator {
    STACK_ALLOCATED();

   public:
    explicit Iterator(const Node* start)
        : current_node_(const_cast<Node*>(start)) {}

    bool operator==(const Iterator& other) const {
      return other.current_node_ == current_node_;
    }
    Iterator& operator++() {
      if (auto* element = DynamicTo<Element>(current_node_);
          element && element->IsScrollMarkerPseudoElement()) {
        current_node_ = static_cast<ScrollMarkerPseudoElement*>(element)
                            ->ScrollMarkerGroup();
      } else {
        current_node_ = Traversal::Parent(*current_node_);
      }
      return *this;
    }

    Node& operator*() const {
      CHECK(current_node_);
      return *current_node_;
    }

   private:
    Node* current_node_;
  };

  explicit AncestorTraversal(const Node* start, bool inclusive = false)
      : iterator_(start) {
    if (!inclusive) {
      ++iterator_;
    }
  }

  Iterator begin() { return iterator_; }
  Iterator end() { return Iterator(nullptr); }

 private:
  Iterator iterator_;
};

// Returns the nearest non-inclusive ancestor of |node| that is display
// locked.
Element* NearestLockedExclusiveAncestor(const Node& node) {
  if (!node.isConnected() ||
      node.GetDocument()
              .GetDisplayLockDocumentState()
              .LockedDisplayLockCount() == 0 ||
      node.IsShadowRoot()) {
    return nullptr;
  }
  // TODO(crbug.com/924550): Once we figure out a more efficient way to
  // determine whether we're inside a locked subtree or not, change this.
  for (Node& ancestor : AncestorTraversal(&node)) {
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
  if (!node.isConnected() ||
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

void PopulateAncestorContexts(
    Node& node,
    HeapHashSet<Member<DisplayLockContext>>& contexts) {
  for (Node& ancestor : AncestorTraversal(&node, true)) {
    auto* ancestor_element = DynamicTo<Element>(ancestor);
    if (!ancestor_element)
      continue;
    if (auto* context = ancestor_element->GetDisplayLockContext())
      contexts.insert(context);
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
  DCHECK(!range.IsNull());
  DCHECK(!range.IsCollapsed());
  if (!range.GetDocument()
           .GetDisplayLockDocumentState()
           .HasActivatableLocks()) {
    return false;
  }
  // Find-in-page matches can't span multiple block-level elements (because the
  // text will be broken by newlines between blocks), so first we find the
  // block-level element which contains the match.
  // This means we only need to traverse up from one node in the range, in this
  // case we are traversing from the start position of the range.
  Element* enclosing_block =
      EnclosingBlock(range.StartPosition(), kCanCrossEditingBoundary);
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

bool DisplayLockUtilities::NeedsActivationForFindInPage(
    const EphemeralRangeInFlatTree& range) {
  DisplayLockDocumentState& state =
      range.GetDocument().GetDisplayLockDocumentState();
  if (!state.HasActivatableLocks()) {
    return false;
  }

  Element* enclosing_block =
      EnclosingBlock(range.StartPosition(), kCanCrossEditingBoundary);

  HeapVector<Member<Element>> activatable_targets;
  for (Node& ancestor : AncestorTraversal(enclosing_block, true)) {
    auto* ancestor_element = DynamicTo<Element>(ancestor);
    if (!ancestor_element) {
      continue;
    }
    if (auto* context = ancestor_element->GetDisplayLockContext()) {
      if (context->ShouldCommitForActivation(
              DisplayLockActivationReason::kFindInPage)) {
        return true;
      }
    }
  }

  return false;
}

const HeapVector<Member<Element>>
DisplayLockUtilities::ActivatableLockedInclusiveAncestors(
    const Node& node,
    DisplayLockActivationReason reason) {
  HeapVector<Member<Element>> elements_to_activate;
  if (node.GetDocument()
          .GetDisplayLockDocumentState()
          .LockedDisplayLockCount() ==
      node.GetDocument()
          .GetDisplayLockDocumentState()
          .DisplayLockBlockingAllActivationCount())
    return elements_to_activate;

  for (Node& ancestor : AncestorTraversal(&node, true)) {
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

VectorOf<Element> DisplayLockUtilities::InclusiveAncestorsOfRange(
    const Range& range) {
  VectorOf<Element> elements;
  // Ranges use NodeTraversal::Next to go in between their start and end nodes,
  // and will access the layout information of each of those nodes. In order to
  // ensure that each of these nodes has unlocked layout information, we have to
  // do a scoped unlock for each of those nodes by unlocking all of their flat
  // tree ancestors.
  for (Node* node = range.FirstNode(); node != range.PastLastNode();
       node = NodeTraversal::Next(*node)) {
    if (node->IsChildOfShadowHost()) {
      // This node may be slotted into another place in the flat tree, so we
      // have to do a flat tree parent traversal for it.
      for (Node& ancestor : AncestorTraversal(node, true)) {
        if (Element* element = DynamicTo<Element>(ancestor)) {
          elements.push_back(element);
        }
      }
    } else {
      if (Element* element = DynamicTo<Element>(node)) {
        elements.push_back(element);
      }
    }
  }
  for (Node& node : AncestorTraversal(range.FirstNode(), true)) {
    if (Element* element = DynamicTo<Element>(node)) {
      elements.push_back(element);
    }
  }
  return elements;
}

DisplayLockUtilities::ScopedForcedUpdate::Impl::Impl(
    const Range* range,
    DisplayLockContext::ForcedPhase phase,
    bool only_cv_auto,
    bool emit_warnings)
    : node_(range->FirstNode()),
      phase_(phase),
      only_cv_auto_(only_cv_auto),
      emit_warnings_(emit_warnings) {
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

  for (Element* element : InclusiveAncestorsOfRange(*range)) {
    if (DisplayLockContext* context = element->GetDisplayLockContext()) {
      forced_context_set_.insert(context);
      context->NotifyForcedUpdateScopeStarted(phase_, emit_warnings_);
    }
  }
}

DisplayLockUtilities::ScopedForcedUpdate::Impl::Impl(
    const Node* node,
    DisplayLockContext::ForcedPhase phase,
    bool include_self,
    bool only_cv_auto,
    bool emit_warnings)
    : node_(node),
      phase_(phase),
      only_cv_auto_(only_cv_auto),
      emit_warnings_(emit_warnings) {
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

  // We can't do flat tree traversals on shadow roots - they aren't in the flat
  // tree. However, they also can't be DisplayLocked, so just go to their host.
  if (node->IsShadowRoot()) {
    node = node->ParentOrShadowHostNode();
  }

  bool use_inclusive_walk = false;
  if (auto* element = DynamicTo<Element>(node)) {
    auto* context = element->GetDisplayLockContext();
    use_inclusive_walk = context && include_self;
  }

  // TODO(vmpstr): This is somewhat inefficient, since we would pay the cost
  // of traversing the ancestor chain even for nodes that are not in the
  // locked subtree. We need to figure out if there is a supplementary
  // structure that we can use to quickly identify nodes that are in the
  // locked subtree.
  for (Node& ancestor : AncestorTraversal(node, use_inclusive_walk)) {
    auto* ancestor_node = DynamicTo<Element>(ancestor);
    if (!ancestor_node)
      continue;
    if (auto* context = ancestor_node->GetDisplayLockContext()) {
      ForceDisplayLockIfNeeded(context);
    }
  }
}

void DisplayLockUtilities::ScopedForcedUpdate::Impl::EnsureMinimumForcedPhase(
    DisplayLockContext::ForcedPhase phase) {
  // Our `phase_` is already at least as permissive as `phase`.
  if (static_cast<int>(phase_) >= static_cast<int>(phase))
    return;
  for (auto context : forced_context_set_)
    context->UpgradeForcedScope(phase_, phase, emit_warnings_);
  phase_ = phase;
}

void DisplayLockUtilities::ScopedForcedUpdate::Impl::Destroy() {
  if (!node_)
    return;
  node_->GetDocument().GetDisplayLockDocumentState().EndForcedScope(this);
  if (parent_frame_impl_)
    parent_frame_impl_->Destroy();
  HeapVector<Member<Element>> force_updated_roots;
  auto* document_rules =
      DocumentSpeculationRules::FromIfExists(node_->GetDocument());
  for (auto context : forced_context_set_) {
    context->NotifyForcedUpdateScopeEnded(phase_);
    if (document_rules && context->is_locked_) {
      force_updated_roots.emplace_back(context->element_);
    }
  }
  if (document_rules) {
    document_rules->DisplayLockedRootsForceUpdateEnded(force_updated_roots);
  }
}

void DisplayLockUtilities::ScopedForcedUpdate::Impl::
    AddForcedUpdateScopeForContext(DisplayLockContext* context) {
  if (!forced_context_set_.Contains(context)) {
    ForceDisplayLockIfNeeded(context);
  }
}

void DisplayLockUtilities::ScopedForcedUpdate::Impl::ForceDisplayLockIfNeeded(
    DisplayLockContext* context) {
  if (!only_cv_auto_ ||
      context->IsActivatable(DisplayLockActivationReason::kViewport)) {
    forced_context_set_.insert(context);
    context->NotifyForcedUpdateScopeStarted(phase_, emit_warnings_);
  }
}

Element*
DisplayLockUtilities::LockedInclusiveAncestorPreventingStyleWithinTreeScope(
    const Node& node) {
  if (!node.isConnected() || node.GetDocument()
                                     .GetDisplayLockDocumentState()
                                     .LockedDisplayLockCount() == 0) {
    return nullptr;
  }

  for (Node& ancestor : AncestorTraversal<NodeTraversal>(&node, true)) {
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
  if (node.IsShadowRoot())
    return nullptr;
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
  if (node.IsShadowRoot())
    return nullptr;

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
  if (node.GetDocument()
              .GetDisplayLockDocumentState()
              .LockedDisplayLockCount() == 0 ||
      node.IsShadowRoot()) {
    return true;
  }

  if (activation_reason == DisplayLockActivationReason::kAccessibility &&
      memoizer_) {
    return !IsLockedForAccessibility(node);
  }

  for (auto* element = NearestLockedExclusiveAncestor(node); element;
       element = NearestLockedExclusiveAncestor(*element)) {
    if (!element->GetDisplayLockContext()->IsActivatable(activation_reason)) {
      return false;
    }
  }
  return true;
}

bool DisplayLockUtilities::IsLockedForAccessibility(const Node& node) {
  // This is a private helper for accessibility, only called if we have a
  // memoizer.
  DCHECK(memoizer_);

  // Consult the memoizer, if we know the result we can return early.
  auto result = memoizer_->IsNodeLockedForAccessibility(&node);
  if (result)
    return *result;

  // Walk up the ancestor chain checking for locked & non-activatable context.
  // See IsDisplayLockedPreventingPaint for an explanation of memoization.
  const Node* previous_ancestor = &node;
  bool ancestor_is_locked = false;
  for (Node& ancestor : AncestorTraversal(&node)) {
    // Reset ancestor is locked, we may set it again just below.
    ancestor_is_locked = false;

    // If we have a context, check if it's locked and if it's also not
    // activatable for accessibility then we found our answer: `node` is locked
    // for accessibility.
    if (auto* ancestor_element = DynamicTo<Element>(ancestor)) {
      if (auto* context = ancestor_element->GetDisplayLockContext()) {
        ancestor_is_locked = context->IsLocked();
        if (ancestor_is_locked &&
            !context->IsActivatable(
                DisplayLockActivationReason::kAccessibility)) {
          // Other than the node, we also know that previous_ancestor must be
          // locked for accessibility. Record that.
          memoizer_->NotifyLockedForAccessibility(previous_ancestor);
          return true;
        }
      }
    }

    // Since we didn't find the answer above, before continuing the walk consult
    // with the memoizer: it might know the answer.
    result = memoizer_->IsNodeLockedForAccessibility(&ancestor);
    if (result) {
      // Note that if we know the result for current ancestor, then that same
      // result applies for previous_ancestor. This is certainly true for
      // positive -- LockedForAccessibility -- results, but it's also true for
      // negative -- Unlocked -- results if the ancestor itself is not locked.
      if (*result)
        memoizer_->NotifyLockedForAccessibility(previous_ancestor);
      else if (!ancestor_is_locked)
        memoizer_->NotifyUnlocked(previous_ancestor);
      return *result;
    }

    // Update the previous ancestor.
    previous_ancestor = &ancestor;
  }

  // If we reached the end of the loop, then the last node we visited
  // (presumably the root of the flat tree) is not locked.
  memoizer_->NotifyUnlocked(previous_ancestor);
  return false;
}

bool DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(
    const Node& source_node,
    IncludeSelfOrNot self) {
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
  if (element &&
      element->GetDocument().GetDisplayLockDocumentState().DisplayLockCount() ==
          0) {
    return;
  }
  for (Node& ancestor : AncestorTraversal(element, true)) {
    if (auto* ancestor_element = DynamicTo<Element>(ancestor)) {
      auto* context = ancestor_element->GetDisplayLockContext();
      if (context) {
        context->NotifySubtreeLostFocus();
      }
    }
  }
}
void DisplayLockUtilities::ElementGainedFocus(Element* element) {
  if (element &&
      element->GetDocument().GetDisplayLockDocumentState().DisplayLockCount() ==
          0) {
    return;
  }

  for (Node& ancestor : AncestorTraversal(element, true)) {
    if (auto* ancestor_element = DynamicTo<Element>(ancestor)) {
      auto* context = ancestor_element->GetDisplayLockContext();
      if (context) {
        context->NotifySubtreeGainedFocus();
      }
    }
  }
}

// static
bool DisplayLockUtilities::NeedsSelectionChangedUpdate(
    const Document& document) {
  return document.GetDisplayLockDocumentState().DisplayLockCount() > 0;
}

void DisplayLockUtilities::SelectionChanged(
    const EphemeralRangeInFlatTree& old_selection,
    const EphemeralRangeInFlatTree& new_selection) {
  if ((!old_selection.IsNull() && old_selection.GetDocument()
                                          .GetDisplayLockDocumentState()
                                          .DisplayLockCount() == 0) ||
      (!new_selection.IsNull() && new_selection.GetDocument()
                                          .GetDisplayLockDocumentState()
                                          .DisplayLockCount() == 0)) {
    return;
  }

  TRACE_EVENT0("blink", "DisplayLockUtilities::SelectionChanged");

  HeapHashSet<Member<Node>> new_nodes;
  for (Node& node : new_selection.Nodes())
    new_nodes.insert(&node);

  HeapHashSet<Member<DisplayLockContext>> lost_selection_contexts;
  HeapHashSet<Member<DisplayLockContext>> gained_selection_contexts;

  for (Node& node : old_selection.Nodes()) {
    if (auto it = new_nodes.find(&node); it != new_nodes.end()) {
      new_nodes.erase(it);
      continue;
    }
    PopulateAncestorContexts(node, lost_selection_contexts);
  }

  for (Node* node : new_nodes) {
    PopulateAncestorContexts(*node, gained_selection_contexts);
  }

  for (DisplayLockContext* context : lost_selection_contexts) {
    if (auto it = gained_selection_contexts.find(context);
        it != gained_selection_contexts.end()) {
      gained_selection_contexts.erase(it);
      continue;
    }
    context->NotifySubtreeLostSelection();
  }

  for (DisplayLockContext* context : gained_selection_contexts) {
    context->NotifySubtreeGainedSelection();
  }
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
  auto* result =
      LockedAncestorPreventingUpdate(object, [](DisplayLockContext* context) {
        return !context->ShouldPrePaintChildren();
      });

  if (!result || !IsA<HTMLCanvasElement>(result)) {
    return result;
  }

  // If the locked ancestor preventing pre-paint is a canvas element, then
  // `object` is potentially fallback content. But fallback content is only
  // applicable if accessibility support is enabled.
  if (!result->GetDocument().ExistingAXObjectCache()) {
    return result;
  }

  // If an ancestor of the canvas is also preventing pre-paint, respect that.
  // Otherwise, don't treat this canvas ancestor of fallback content as
  // preventing pre-paint.
  if (auto* parent = result->parentElement()) {
    return LockedAncestorPreventingUpdate(
        *parent, [](DisplayLockContext* context) {
          return !context->ShouldPrePaintChildren();
        });
  }
  return nullptr;
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

#if DCHECK_IS_ON()
bool DisplayLockUtilities::AssertStyleAllowed(const Node& node) {
  if (node.GetDocument().IsFlatTreeTraversalForbidden() ||
      node.GetDocument()
          .GetSlotAssignmentEngine()
          .HasPendingSlotAssignmentRecalc()) {
    return true;
  }
  return !LockedAncestorPreventingStyle(node);
}
#endif

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

namespace {

// This method follows the old spec and will be removed, hence the "Legacy"
// name.
bool LegacyExpandDetailsAncestors(const Node& node) {
  // Since setting the open attribute could fire synchronous events (e.g.
  // `blur`), which could mess with the FlatTreeTraversal iterator, we should
  // first iterate details elements and then open them all.
  HeapVector<Member<HTMLDetailsElement>> details_to_open;

  for (Node& parent : FlatTreeTraversal::AncestorsOf(node)) {
    if (HTMLDetailsElement* details = DynamicTo<HTMLDetailsElement>(parent)) {
      // If the active match is inside the <summary> of a <details>, then we
      // shouldn't expand the <details> because the active match is already
      // visible.
      bool inside_summary = false;
      Element& summary = details->MainSummary();
      for (Node& ancestor : FlatTreeTraversal::AncestorsOf(node)) {
        if (&ancestor == &summary) {
          inside_summary = true;
          break;
        }
      }

      if (!inside_summary &&
          !details->FastHasAttribute(html_names::kOpenAttr)) {
        details_to_open.push_back(details);
      }
    }
  }

  for (HTMLDetailsElement* details : details_to_open) {
    details->SetBooleanAttribute(html_names::kOpenAttr, true);
  }

  return details_to_open.size();
}

// This method follows the old spec and will be removed, hence the "Legacy"
// name.
bool LegacyRevealHiddenUntilFoundAncestors(const Node& node) {
  // Since setting the open attribute could fire synchronous events (e.g.
  // `blur`), which could mess with the FlatTreeTraversal iterator, we should
  // first iterate details elements and then open them all.
  HeapVector<Member<HTMLElement>> elements_to_reveal;

  for (Node& parent : AncestorTraversal<FlatTreeTraversal>(&node, true)) {
    if (HTMLElement* element = DynamicTo<HTMLElement>(parent)) {
      if (EqualIgnoringASCIICase(
              element->FastGetAttribute(html_names::kHiddenAttr),
              keywords::kUntilFound)) {
        elements_to_reveal.push_back(element);
      }
    }
  }

  for (HTMLElement* element : elements_to_reveal) {
    element->DispatchEvent(
        *Event::CreateBubble(event_type_names::kBeforematch));
  }

  for (HTMLElement* element : elements_to_reveal) {
    element->removeAttribute(html_names::kHiddenAttr);
  }

  return elements_to_reveal.size();
}

}  // namespace

// static
DisplayLockUtilities::RevealResult
DisplayLockUtilities::RevealAutoExpandableAncestors(const Node& target) {
  if (!RuntimeEnabledFeatures::AncestorRevealingNewSpecEnabled()) {
    RevealResult reveal_result;
    reveal_result.revealed_details = LegacyExpandDetailsAncestors(target);
    reveal_result.revealed_hidden_until_found =
        LegacyRevealHiddenUntilFoundAncestors(target);
    return reveal_result;
  }

  // https://html.spec.whatwg.org/#ancestor-revealing-algorithm
  enum class AncestorType {
    kDetails = 0,
    kHiddenUntilFound = 1,
  };
  HeapVector<std::pair<Member<HTMLElement>, AncestorType>> ancestors_to_reveal;

  for (Node& ancestor : AncestorTraversal<FlatTreeTraversal>(&target, true)) {
    if (HTMLElement* element = DynamicTo<HTMLElement>(ancestor)) {
      if (EqualIgnoringASCIICase(
              element->FastGetAttribute(html_names::kHiddenAttr),
              keywords::kUntilFound)) {
        ancestors_to_reveal.push_back(
            std::make_pair(element, AncestorType::kHiddenUntilFound));
      }
    }
    if (auto* parent_details =
            DynamicTo<HTMLDetailsElement>(ancestor.parentNode())) {
      if (!parent_details->FastHasAttribute(html_names::kOpenAttr) &&
          parent_details->IsAssignedToContentSlot(ancestor)) {
        ancestors_to_reveal.push_back(
            std::make_pair(parent_details, AncestorType::kDetails));
      }
    }
  }

  RevealResult reveal_result;

  for (auto& reveal_pair : ancestors_to_reveal) {
    if (!reveal_pair.first->isConnected()) {
      return reveal_result;
    }
    if (reveal_pair.second == AncestorType::kHiddenUntilFound) {
      if (!EqualIgnoringASCIICase(
              reveal_pair.first->FastGetAttribute(html_names::kHiddenAttr),
              keywords::kUntilFound)) {
        return reveal_result;
      }

      reveal_pair.first->DispatchEvent(
          *Event::CreateBubble(event_type_names::kBeforematch));
      if (!reveal_pair.first->isConnected() ||
          !EqualIgnoringASCIICase(
              reveal_pair.first->FastGetAttribute(html_names::kHiddenAttr),
              keywords::kUntilFound)) {
        return reveal_result;
      }

      reveal_pair.first->removeAttribute(html_names::kHiddenAttr);
      reveal_result.revealed_hidden_until_found = true;
    } else {
      CHECK_EQ(reveal_pair.second, AncestorType::kDetails);
      if (reveal_pair.first->FastHasAttribute(html_names::kOpenAttr)) {
        return reveal_result;
      }
      reveal_pair.first->SetBooleanAttribute(html_names::kOpenAttr, true);
      reveal_result.revealed_details = true;
    }
  }

  return reveal_result;
}

static bool CheckSelf(const Node* node) {
  if (auto* element = DynamicTo<Element>(node)) {
    if (auto* context = element->GetDisplayLockContext()) {
      if (!context->ShouldPaintChildren())
        return true;
    }
  }
  return false;
}

bool DisplayLockUtilities::IsDisplayLockedPreventingPaintUnmemoized(
    const Node& node,
    bool inclusive_check) {
  return inclusive_check
             ? DisplayLockUtilities::LockedInclusiveAncestorPreventingPaint(
                   node)
             : DisplayLockUtilities::LockedAncestorPreventingPaint(node);
}

bool DisplayLockUtilities::IsDisplayLockedPreventingPaint(
    const Node* node,
    bool inclusive_check) {
  // If we have a memoizer, consult with it to see if we already know the
  // result. Otherwise, fallback to get-element versions.
  if (memoizer_) {
    // Consult memoizer with it to see if we already know the
    // result. Otherwise, fallback to get-element versions.
    auto memoized_result = memoizer_->IsNodeLocked(node);
    if (memoized_result) {
      bool final_result =
          *memoized_result || (inclusive_check && CheckSelf(node));
#if DCHECK_IS_ON()
      bool nonmemoized_result =
          IsDisplayLockedPreventingPaintUnmemoized(*node, inclusive_check);
      DCHECK_EQ(final_result, nonmemoized_result)
          << "\nMemoized result did not match non-memoized result for "
          << (inclusive_check ? "inclusive" : "non-inclusive") << " check."
          << "\n* node = " << node
          << "\n* Inclusive ancestor preventing paint: "
          << DisplayLockUtilities::LockedInclusiveAncestorPreventingPaint(*node)
          << "\n* Non-inclusive ancestor preventing paint: "
          << DisplayLockUtilities::LockedAncestorPreventingPaint(*node);
#endif

      // The memoizer can only be used for non-inclusive checks.
      return final_result;
    }
  } else {
    return inclusive_check
               ? DisplayLockUtilities::LockedInclusiveAncestorPreventingPaint(
                     *node)
               : DisplayLockUtilities::LockedAncestorPreventingPaint(*node);
  }

  // Do some sanity checks that we can early-out on.
  if (!node->isConnected() ||
      node->GetDocument()
              .GetDisplayLockDocumentState()
              .LockedDisplayLockCount() == 0 ||
      node->IsShadowRoot()) {
    return false;
  }

  // Compute the result by walking the tree and memoize the result.
  // Handle the inclusive check -- that is, check the node itself. Note that
  // it's important not to memoize that since the memoization consists of
  // ancestor checks only.
  if (inclusive_check && CheckSelf(node))
    return true;

  // Walk up the ancestor chain, and consult with both the memoizer and check
  // directly if we're skipping paint. When we find a result (or finish the
  // loop), then save the last visited ancestor (previous_ancestor) into the
  // memoizer. This ensures that any future calls in the similar subtree would
  // have to check one level less. In turn, this means that if we have many
  // calls, it will eventually only check only a few levels. This also keeps the
  // memoizer cache fairly small.
  const Node* previous_ancestor = node;
  for (Node& ancestor : AncestorTraversal(node)) {
    if (auto* ancestor_element = DynamicTo<Element>(ancestor)) {
      if (auto* context = ancestor_element->GetDisplayLockContext()) {
        // Note that technically we could do a similar approach to
        // IsLockedForAccessibility by recording whether this context is locked
        // but allow paint. However, that situation is not possible since all
        // locked contexts always prevent paint.
        DCHECK(!context->IsLocked() || !context->ShouldPaintChildren());
        if (!context->ShouldPaintChildren()) {
          memoizer_->NotifyLocked(previous_ancestor);
          return true;
        }
      }
    }

    auto result = memoizer_->IsNodeLocked(&ancestor);
    if (result) {
      // Propagate the result to the previous_ancestor as well. Note that if
      // `ancestor` is in an unlocked subtree then `previous_ancestor` must also
      // be in such a subtree because the only way it isn't is if `ancestor` is
      // itself locked, which is checked above.
      if (*result)
        memoizer_->NotifyLocked(previous_ancestor);
      else
        memoizer_->NotifyUnlocked(previous_ancestor);
      return *result;
    }

    // Update previous ancestor!
    previous_ancestor = &ancestor;
  }

  // If we reached the end of the loop, then last node that we visited is not
  // locked.
  memoizer_->NotifyUnlocked(previous_ancestor);
  return false;
}

bool DisplayLockUtilities::IsDisplayLockedPreventingPaint(
    const LayoutObject* object) {
  // If we don't have a memoizer, fall back to the get-element version.
  if (!memoizer_)
    return DisplayLockUtilities::LockedAncestorPreventingPaint(*object);

  bool inclusive_check = false;
  // Find a node to check.
  while (object) {
    if (const auto* node = object->GetNode())
      return IsDisplayLockedPreventingPaint(node, inclusive_check);
    object = object->Parent();
    // If we went to the parent, all future node calls are inclusive.
    inclusive_check = true;
  }
  return false;
}

bool DisplayLockUtilities::IsUnlockedQuickCheck(const Node& node) {
  if (node.GetDocument()
          .GetDisplayLockDocumentState()
          .LockedDisplayLockCount() == 0) {
    return true;
  }
  if (memoizer_) {
    auto result = memoizer_->IsNodeLocked(&node);
    if (result)
      return !*result;
  }
  return false;
}

bool DisplayLockUtilities::IsPotentialStyleRecalcRoot(const Node& node) {
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return false;
  auto* context = element->GetDisplayLockContext();
  if (!context)
    return false;
  if (context->StyleTraversalWasBlocked())
    return true;
  if (!context->ShouldStyleChildren())
    return context->IsElementDirtyForStyleRecalc();
  return false;
}

}  // namespace blink
