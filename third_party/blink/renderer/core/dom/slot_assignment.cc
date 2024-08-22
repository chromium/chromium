// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/slot_assignment.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_recalc_forbidden_scope.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_details_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/nesting_level_incrementer.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

void SlotAssignment::DidAddSlot(HTMLSlotElement& slot) {
  // Relevant DOM Standard:
  // https://dom.spec.whatwg.org/#concept-node-insert

  // |slot| was already connected to the tree, however, |slot_map_| doesn't
  // reflect the insertion yet.

  ++slot_count_;
  needs_collect_slots_ = true;

  if (owner_->IsManualSlotting()) {
    // Adding a new slot should not require assignment recalc, but still needs
    // setting up the fallback if any.
    slot.CheckFallbackAfterInsertedIntoShadowTree();
    return;
  }

  DCHECK(!slot_map_->Contains(slot.GetName()) ||
         GetCachedFirstSlotWithoutAccessingNodeTree(slot.GetName()));
  DidAddSlotInternal(slot);
  // Ensures that TreeOrderedMap has a cache if there is a slot for the name.
  DCHECK(GetCachedFirstSlotWithoutAccessingNodeTree(slot.GetName()));
}

void SlotAssignment::DidRemoveSlot(HTMLSlotElement& slot) {
  // Relevant DOM Standard:
  // https://dom.spec.whatwg.org/#concept-node-remove

  // |slot| was already removed from the tree, however, |slot_map_| doesn't
  // reflect the removal yet.

  DCHECK_GT(slot_count_, 0u);
  --slot_count_;
  needs_collect_slots_ = true;

  if (owner_->IsManualSlotting()) {
    auto& candidates = slot.ManuallyAssignedNodes();
    if (candidates.size()) {
      SetNeedsAssignmentRecalc();
      slot.DidSlotChangeAfterRemovedFromShadowTree();
    }
    return;
  }

  DidRemoveSlotInternal(slot, slot.GetName(), SlotMutationType::kRemoved);
  // Ensures that TreeOrderedMap has a cache if there is a slot for the name.
  DCHECK(!slot_map_->Contains(slot.GetName()) ||
         GetCachedFirstSlotWithoutAccessingNodeTree(slot.GetName()));
}

void SlotAssignment::DidAddSlotInternal(HTMLSlotElement& slot) {
  // There are the following 3 cases for addition:
  //         Before:              After:
  // case 1: []                -> [*slot*]
  // case 2: [old_active, ...] -> [*slot*, old_active, ...]
  // case 3: [old_active, ...] -> [old_active, ..., *slot*, ...]

  // TODO(hayato): Explain the details in README.md file.

  const AtomicString& slot_name = slot.GetName();

  // At this timing, we can't use FindSlotByName because what we are interested
  // in is the first slot *before* |slot| was inserted. Here, |slot| was already
  // connected to the tree. Thus, we can't use on FindBySlotName because
  // it might scan the current tree and return a wrong result.
  HTMLSlotElement* old_active =
      GetCachedFirstSlotWithoutAccessingNodeTree(slot_name);
  DCHECK(!old_active || old_active != slot);

  // This might invalidate the slot_map's cache.
  slot_map_->Add(slot_name, slot);

  // This also ensures that TreeOrderedMap has a cache for the first element.
  HTMLSlotElement* new_active = FindSlotByName(slot_name);
  DCHECK(new_active);
  DCHECK(new_active == slot || new_active == old_active);

  if (new_active == slot) {
    // case 1 or 2
    if (FindHostChildBySlotName(slot_name)) {
      // |slot| got assigned nodes
      slot.DidSlotChange(SlotChangeType::kSignalSlotChangeEvent);
      if (old_active) {
        // case 2
        //  |old_active| lost assigned nodes.
        old_active->DidSlotChange(SlotChangeType::kSignalSlotChangeEvent);
      }
    } else {
      // |slot| is active, but it doesn't have assigned nodes.
      // Fallback might matter.
      slot.CheckFallbackAfterInsertedIntoShadowTree();
    }
  } else {
    // case 3
    slot.CheckFallbackAfterInsertedIntoShadowTree();
  }
}

void SlotAssignment::DidRemoveSlotInternal(
    HTMLSlotElement& slot,
    const AtomicString& slot_name,
    SlotMutationType slot_mutation_type) {
  // There are the following 3 cases for removal:
  //         Before:                            After:
  // case 1: [*slot*]                        -> []
  // case 2: [*slot*, new_active, ...]       -> [new_active, ...]
  // case 3: [new_active, ..., *slot*, ...]  -> [new_active, ...]

  // TODO(hayato): Explain the details in README.md file.

  // At this timing, we can't use FindSlotByName because what we are interested
  // in is the first slot *before* |slot| was removed. Here, |slot| was already
  // disconnected from the tree. Thus, we can't use FindBySlotName because
  // it might scan the current tree and return a wrong result.
  HTMLSlotElement* old_active =
      GetCachedFirstSlotWithoutAccessingNodeTree(slot_name);

  // If we don't have a cached slot for this slot name, then we're
  // likely removing a nested identically named slot, e.g.
  // <slot id=removed><slot></slot</slot>, and this is the inner
  // slot. It has already been removed from the map, so return.
  if (!old_active)
    return;

  slot_map_->Remove(slot_name, slot);
  // This also ensures that TreeOrderedMap has a cache for the first element.
  HTMLSlotElement* new_active = FindSlotByName(slot_name);
  DCHECK(!new_active || new_active != slot);

  if (old_active == slot) {
    // case 1 or 2
    if (FindHostChildBySlotName(slot_name)) {
      // |slot| lost assigned nodes
      if (slot_mutation_type == SlotMutationType::kRemoved) {
        // |slot|'s previously assigned nodes' flat tree node data became
        // dirty. Call SetNeedsAssignmentRecalc() to clear their flat tree
        // node data surely in recalc timing.
        SetNeedsAssignmentRecalc();
        slot.DidSlotChangeAfterRemovedFromShadowTree();
      } else {
        slot.DidSlotChangeAfterRenaming();
      }
      if (new_active) {
        // case 2
        // |new_active| got assigned nodes
        new_active->DidSlotChange(SlotChangeType::kSignalSlotChangeEvent);
      }
    } else {
      // |slot| was active, but it didn't have assigned nodes.
      // Fallback might matter.
      slot.CheckFallbackAfterRemovedFromShadowTree();
    }
  } else {
    // case 3
    slot.CheckFallbackAfterRemovedFromShadowTree();
  }
}

bool SlotAssignment::FindHostChildBySlotName(
    const AtomicString& slot_name) const {
  // TODO(hayato): Avoid traversing children every time.
  for (Node& child : NodeTraversal::ChildrenOf(owner_->host())) {
    if (!child.IsSlotable())
      continue;
    if (child.SlotName() == slot_name)
      return true;
  }
  return false;
}

void SlotAssignment::DidRenameSlot(const AtomicString& old_slot_name,
                                   HTMLSlotElement& slot) {
  // Rename can be thought as "Remove and then Add", except that
  // we don't need to set needs_collect_slots_.
  DCHECK(GetCachedFirstSlotWithoutAccessingNodeTree(old_slot_name));
  DidRemoveSlotInternal(slot, old_slot_name, SlotMutationType::kRenamed);
  DidAddSlotInternal(slot);
  DCHECK(GetCachedFirstSlotWithoutAccessingNodeTree(slot.GetName()));
}

void SlotAssignment::DidChangeHostChildSlotName(const AtomicString& old_value,
                                                const AtomicString& new_value) {
  if (HTMLSlotElement* slot =
          FindSlotByName(HTMLSlotElement::NormalizeSlotName(old_value))) {
    slot->DidSlotChange(SlotChangeType::kSignalSlotChangeEvent);
  }
  if (HTMLSlotElement* slot =
          FindSlotByName(HTMLSlotElement::NormalizeSlotName(new_value))) {
    slot->DidSlotChange(SlotChangeType::kSignalSlotChangeEvent);
  }
}

SlotAssignment::SlotAssignment(ShadowRoot& owner)
    : slot_map_(MakeGarbageCollected<TreeOrderedMap>()),
      owner_(&owner),
      needs_collect_slots_(false),
      slot_count_(0) {
}

void SlotAssignment::SetNeedsAssignmentRecalc() {
  needs_assignment_recalc_ = true;
  if (owner_->isConnected()) {
    owner_->GetDocument().GetSlotAssignmentEngine().AddShadowRootNeedingRecalc(
        *owner_);
    owner_->GetDocument().ScheduleLayoutTreeUpdateIfNeeded();
  }
}

void SlotAssignment::RecalcAssignment() {
  if (!needs_assignment_recalc_)
    return;
  {
    NestingLevelIncrementer slot_assignment_recalc_depth(
        owner_->GetDocument().SlotAssignmentRecalcDepth());

#if DCHECK_IS_ON()
    DCHECK(!owner_->GetDocument().IsSlotAssignmentRecalcForbidden());
#endif
    // To detect recursive RecalcAssignment, which shouldn't happen.
    SlotAssignmentRecalcForbiddenScope forbid_slot_recalc(
        owner_->GetDocument());

    // The accessibility cache must be invalidated before flat tree traversal
    // is forbidden, because the process of invalidation accesses the old flat
    // tree children in order to clean up soon to be stale relationships.
    // Any <slot> within this shadow root may lose or gain flat tree children
    // during slot reassignment, so call ChildrenChanged() on all of them.
    AXObjectCache* cache = owner_->GetDocument().ExistingAXObjectCache();
    if (cache) {
      for (Member<HTMLSlotElement> slot : Slots())
        cache->SlotAssignmentWillChange(slot);
    }

    FlatTreeTraversalForbiddenScope forbid_flat_tree_traversal(
        owner_->GetDocument());

    if (owner_->IsUserAgent() && owner_->IsManualSlotting()) {
      owner_->host().ManuallyAssignSlots();
    }
    needs_assignment_recalc_ = false;

    for (Member<HTMLSlotElement> slot : Slots())
      slot->WillRecalcAssignedNodes();

    if (owner_->IsManualSlotting()) {
      // |children_to_clear| starts with the list of all light-dom children of
      // the host that are *currently slotted*. Any of those that aren't slotted
      // during this recalc will then have their flat tree data cleared.
      HeapHashSet<Member<Node>> children_to_clear;
      for (Node& child : NodeTraversal::ChildrenOf(owner_->host())) {
        if (!child.GetFlatTreeNodeData())
          continue;
        children_to_clear.insert(&child);
      }

      for (Member<HTMLSlotElement> slot : Slots()) {
        for (Node* slottable : slot->ManuallyAssignedNodes()) {
          // Some of the manually assigned nodes might have been moved
          // to other trees or documents. In that case, don't assign them
          // here, but also don't remove/invalidate them in the manually
          // assigned nodes list, in case they come back later.
          if (slottable && slottable->IsChildOfShadowHost() &&
              slottable->parentElement() == owner_->host()) {
            slot->AppendAssignedNode(*slottable);
            children_to_clear.erase(slottable);
            // If changing tree scope, recompute the a11y subtree.
            // This normally occurs when the slottable node is removed
            // from the flat tree via the below call to RemovedFromFlatTree(),
            // which calls DetachLayoutTree().
            if (cache) {
              cache->RemoveSubtree(slottable);
            }
          }
        }
      }

      for (auto child : children_to_clear) {
        child->ClearFlatTreeNodeData();
        child->RemovedFromFlatTree();
      }
    } else {
      for (Node& child : NodeTraversal::ChildrenOf(owner_->host())) {
        if (!child.IsSlotable())
          continue;

        if (HTMLSlotElement* slot = FindSlotByName(child.SlotName())) {
          slot->AppendAssignedNode(child);
        } else {
          child.ClearFlatTreeNodeData();
          child.RemovedFromFlatTree();
        }
      }
    }

    if (owner_->isConnected()) {
      owner_->GetDocument()
          .GetSlotAssignmentEngine()
          .RemoveShadowRootNeedingRecalc(*owner_);
    }

    for (auto& slot : Slots()) {
      // TODO(crbug.com/1208573): Consider if we really need to be using
      // IsInLockedSubtreeCrossingFrames, or if
      // LockedInclusiveAncestorPreventingStyleWithinTreeScope is good enough
      // as-is.
      //
      // If we have an ancestor that blocks style recalc, we should let
      // DidRecalcAssignNodes know this, since we may need to do work that
      // would otherwise be done in layout tree building.
      slot->DidRecalcAssignedNodes(
          !!DisplayLockUtilities::
               LockedInclusiveAncestorPreventingStyleWithinTreeScope(*slot));
    }
  }

  // We need to update any slots with dir=auto for two reasons:
  //  (1) because this call might have assigned them different assigned nodes
  //      and changed the result of the dir=auto, or
  //  (2) because an earlier call to the slot's
  //      CalculateAndAdjustAutoDirectionality method was deferred because the
  //      slot needed assignment recalc (which is necessary because some such
  //      calls happen when it's not safe to recalc assignment).
  //
  // This needs to happen outside of the scope above, when flat tree traversal
  // is allowed, because Element::UpdateDescendantHasDirAutoAttribute uses
  // FlatTreeTraversal.
  for (HTMLSlotElement* slot : Slots()) {
    if (slot->HasDirectionAuto()) {
      slot->AdjustDirectionAutoAfterRecalcAssignedNodes();
    }
  }
}

const HeapVector<Member<HTMLSlotElement>>& SlotAssignment::Slots() {
  if (needs_collect_slots_)
    CollectSlots();
  return slots_;
}

HTMLSlotElement* SlotAssignment::FindSlot(const Node& node) {
  if (!node.IsSlotable())
    return nullptr;
  return owner_->IsManualSlotting()
             ? FindSlotInManualSlotting(const_cast<Node&>(node))
             : FindSlotByName(node.SlotName());
}

HTMLSlotElement* SlotAssignment::FindSlotByName(
    const AtomicString& slot_name) const {
  return slot_map_->GetSlotByName(slot_name, *owner_);
}

HTMLSlotElement* SlotAssignment::FindSlotInManualSlotting(Node& node) {
  auto* slot = node.ManuallyAssignedSlot();
  if (slot && slot->ContainingShadowRoot() == owner_ &&
      node.IsChildOfShadowHost() && node.parentElement() == owner_->host())
    return slot;

  return nullptr;
}

void SlotAssignment::CollectSlots() {
  DCHECK(needs_collect_slots_);
  slots_.clear();

  slots_.reserve(slot_count_);
  for (HTMLSlotElement& slot :
       Traversal<HTMLSlotElement>::DescendantsOf(*owner_)) {
    slots_.push_back(&slot);
  }
  needs_collect_slots_ = false;
  DCHECK_EQ(slots_.size(), slot_count_);
}

HTMLSlotElement* SlotAssignment::GetCachedFirstSlotWithoutAccessingNodeTree(
    const AtomicString& slot_name) {
  if (Element* slot =
          slot_map_->GetCachedFirstElementWithoutAccessingNodeTree(slot_name)) {
    return To<HTMLSlotElement>(slot);
  }
  return nullptr;
}

void SlotAssignment::Trace(Visitor* visitor) const {
  visitor->Trace(slots_);
  visitor->Trace(slot_map_);
  visitor->Trace(owner_);
}

}  // namespace blink
