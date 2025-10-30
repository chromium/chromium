// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/focusgroup_controller_utils.h"

#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/page/grid_focusgroup_structure_info.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

FocusgroupDirection FocusgroupControllerUtils::FocusgroupDirectionForEvent(
    KeyboardEvent* event) {
  DCHECK(event);
  if (event->ctrlKey() || event->metaKey() || event->shiftKey())
    return FocusgroupDirection::kNone;

  const AtomicString key(event->key());
  // TODO(bebeaudr): Support RTL. Will it be as simple as inverting the
  // direction associated with the left and right arrows when in a RTL element?
  if (key == keywords::kArrowDown) {
    return FocusgroupDirection::kForwardBlock;
  } else if (key == keywords::kArrowRight) {
    return FocusgroupDirection::kForwardInline;
  } else if (key == keywords::kArrowUp) {
    return FocusgroupDirection::kBackwardBlock;
  } else if (key == keywords::kArrowLeft) {
    return FocusgroupDirection::kBackwardInline;
  }

  return FocusgroupDirection::kNone;
}

bool FocusgroupControllerUtils::IsDirectionForward(
    FocusgroupDirection direction) {
  return direction == FocusgroupDirection::kForwardInline ||
         direction == FocusgroupDirection::kForwardBlock;
}

bool FocusgroupControllerUtils::IsDirectionBackward(
    FocusgroupDirection direction) {
  return direction == FocusgroupDirection::kBackwardInline ||
         direction == FocusgroupDirection::kBackwardBlock;
}

bool FocusgroupControllerUtils::IsDirectionInline(
    FocusgroupDirection direction) {
  return direction == FocusgroupDirection::kBackwardInline ||
         direction == FocusgroupDirection::kForwardInline;
}

bool FocusgroupControllerUtils::IsDirectionBlock(
    FocusgroupDirection direction) {
  return direction == FocusgroupDirection::kBackwardBlock ||
         direction == FocusgroupDirection::kForwardBlock;
}

bool FocusgroupControllerUtils::IsAxisSupported(FocusgroupFlags flags,
                                                FocusgroupDirection direction) {
  return ((flags & FocusgroupFlags::kInline) && IsDirectionInline(direction)) ||
         ((flags & FocusgroupFlags::kBlock) && IsDirectionBlock(direction));
}

bool FocusgroupControllerUtils::WrapsInDirection(
    FocusgroupFlags flags,
    FocusgroupDirection direction) {
  return ((flags & FocusgroupFlags::kWrapInline) &&
          IsDirectionInline(direction)) ||
         ((flags & FocusgroupFlags::kWrapBlock) && IsDirectionBlock(direction));
}

Element* FocusgroupControllerUtils::FindNearestFocusgroupAncestor(
    const Element* element,
    FocusgroupType type) {
  if (!element)
    return nullptr;

  for (Element* ancestor = FlatTreeTraversal::ParentElement(*element); ancestor;
       ancestor = FlatTreeTraversal::ParentElement(*ancestor)) {
    FocusgroupBehavior ancestor_behavior =
        ancestor->GetFocusgroupData().behavior;
    if (ancestor_behavior != FocusgroupBehavior::kNoBehavior) {
      switch (type) {
        case FocusgroupType::kGrid:
          // Respect the FocusgroupGrid feature gate.
          CHECK(RuntimeEnabledFeatures::FocusgroupGridEnabled(
              element->GetExecutionContext()));
          // TODO(bebeaudr): Support grid focusgroups that aren't based on the
          // table layout objects.
          if (ancestor_behavior == FocusgroupBehavior::kGrid &&
              IsA<LayoutTable>(ancestor->GetLayoutObject())) {
            return ancestor;
          }
          break;
        case FocusgroupType::kLinear:
          if (ancestor_behavior != FocusgroupBehavior::kGrid) {
            return ancestor;
          }
          break;
        default:
          NOTREACHED();
      }
      return nullptr;
    }
  }

  return nullptr;
}

Element* FocusgroupControllerUtils::NextElement(const Element* current,
                                                bool skip_subtree) {
  DCHECK(current);
  Node* node;
  if (skip_subtree) {
    node = FlatTreeTraversal::NextSkippingChildren(*current);
  } else {
    node = FlatTreeTraversal::Next(*current);
  }

  Element* next_element;
  // Here, we don't need to skip the subtree when getting the next element since
  // we've already skipped the subtree we wanted to skipped by calling
  // NextSkippingChildren above.
  for (; node; node = FlatTreeTraversal::Next(*node)) {
    next_element = DynamicTo<Element>(node);
    if (next_element) {
      return next_element;
    }
  }
  return nullptr;
}

Element* FocusgroupControllerUtils::PreviousElement(const Element* current,
                                                    bool skip_subtree) {
  DCHECK(current);
  Node* node;
  if (skip_subtree) {
    node = FlatTreeTraversal::PreviousAbsoluteSibling(*current);
  } else {
    node = FlatTreeTraversal::Previous(*current);
  }
  for (; node; node = FlatTreeTraversal::Previous(*node)) {
    if (Element* previous_element = DynamicTo<Element>(node)) {
      return previous_element;
    }
  }
  return nullptr;
}

Element* FocusgroupControllerUtils::NextElementInDirection(
    const Element* current,
    FocusgroupDirection direction,
    bool skip_subtree) {
  DCHECK_NE(IsDirectionForward(direction), IsDirectionBackward(direction));
  mojom::blink::FocusType focus_type = IsDirectionForward(direction)
                                           ? mojom::blink::FocusType::kForward
                                           : mojom::blink::FocusType::kBackward;
  return NextElementInDirection(current, focus_type, skip_subtree);
}

Element* FocusgroupControllerUtils::NextElementInDirection(
    const Element* current,
    mojom::blink::FocusType direction,
    bool skip_subtree) {
  if (!current) {
    return nullptr;
  }
  switch (direction) {
    case mojom::blink::FocusType::kForward:
      return NextElement(current, skip_subtree);
    case mojom::blink::FocusType::kBackward:
      return PreviousElement(current, skip_subtree);
    default:
      NOTREACHED();
  }
}

// Returns next candidate focusgroup item inside |owner| relative to
// |current_item| in the specified direction.
Element* FocusgroupControllerUtils::NextFocusgroupItemInDirection(
    const Element* owner,
    const Element* current_item,
    FocusgroupDirection direction) {
  if (!owner || !current_item || owner == current_item) {
    return nullptr;
  }

  Element* next_element =
      NextElementInDirection(current_item, direction, /*skip_subtree=*/false);
  while (next_element &&
         FlatTreeTraversal::IsDescendantOf(*next_element, *owner)) {
    if (next_element != owner) {
      if (next_element->GetFocusgroupData().behavior !=
          FocusgroupBehavior::kNoBehavior) {
        // We can skip the entire subtree for both nested focusgroups and
        // opted out subtrees.
        next_element = NextElementInDirection(next_element, direction,
                                              /*skip_subtree=*/true);
        continue;
      }
    }
    if (IsFocusgroupItemWithOwner(next_element, owner)) {
      return next_element;
    }
    next_element =
        NextElementInDirection(next_element, direction, /*skip_subtree=*/false);
  }
  return nullptr;
}

Element* FocusgroupControllerUtils::GetFocusgroupOwnerOfItem(
    const Element* element) {
  if (!element || !element->IsFocusable()) {
    return nullptr;
  }

  return focusgroup::FindFocusgroupOwner(element);
}

bool FocusgroupControllerUtils::IsFocusgroupItemWithOwner(
    const Element* element,
    const Element* focusgroup_owner) {
  return GetFocusgroupOwnerOfItem(element) == focusgroup_owner;
}

bool FocusgroupControllerUtils::IsGridFocusgroupItem(const Element* element) {
  CHECK(element);
  CHECK(RuntimeEnabledFeatures::FocusgroupGridEnabled(
      element->GetExecutionContext()));
  if (!element->IsFocusable())
    return false;

  // TODO(bebeaudr): Add support for manual grids, where the grid focusgroup
  // items aren't necessarily on an table cell layout object.
  return IsA<LayoutTableCell>(element->GetLayoutObject());
}

bool FocusgroupControllerUtils::IsEntryElementForFocusgroupSegment(
    Element& item,
    Element& owner,
    mojom::blink::FocusType direction) {
  if (!IsFocusgroupItemWithOwner(&item, &owner)) {
    return false;
  }
  return &item == GetEntryElementForFocusgroupSegment(item, owner, direction);
}

Element* FocusgroupControllerUtils::GetEntryElementForFocusgroupSegment(
    Element& item,
    Element& owner,
    mojom::blink::FocusType direction) {
  DCHECK(IsFocusgroupItemWithOwner(&item, &owner));

  Element* memory_item = owner.GetFocusgroupLastFocused();

  // Walk through all items in the segment to find the best candidate.
  Element* item_in_segment = nullptr;

  // Start from the beginning/end of the segment based on direction.
  if (direction == mojom::blink::FocusType::kForward) {
    item_in_segment = FirstFocusgroupItemInSegment(item);
  } else {
    DCHECK(direction == mojom::blink::FocusType::kBackward);
    item_in_segment = LastFocusgroupItemInSegment(item);
  }

  if (!item_in_segment) {
    return nullptr;
  }

  Element* best_positive_tabindex = nullptr;
  Element* best_zero_tabindex = nullptr;
  Element* best_negative_tabindex = nullptr;
  bool memory_item_in_segment = false;

  // Iterate through all items in segment.
  while (item_in_segment) {
    DCHECK(IsFocusgroupItemWithOwner(item_in_segment, &owner));
    if (item_in_segment->IsFocusedElementInDocument()) {
      // If another item in the segment is already focused (which can occur
      // when checking whether we should tab from that focused item to another
      // item in the same segment), return nullptr to ensure there is only one
      // item in sequential tab order per segment.
      return nullptr;
    }

    if (memory_item && item_in_segment == memory_item) {
      // If we found the memory item, we no longer need to look for other
      // candidates, but do need to continue to ensure that there is no focused
      // element in the segment.
      memory_item_in_segment = true;
      item_in_segment = NextFocusgroupItemInSegmentInDirection(
          *item_in_segment, owner, direction);
      continue;
    }

    int tab_index = item_in_segment->tabIndex();
    if (tab_index > 0) {
      if (!best_positive_tabindex) {
        best_positive_tabindex = item_in_segment;
      } else if (direction == mojom::blink::FocusType::kForward &&
                 tab_index < best_positive_tabindex->tabIndex()) {
        best_positive_tabindex = item_in_segment;
      } else if (direction == mojom::blink::FocusType::kBackward &&
                 tab_index > best_positive_tabindex->tabIndex()) {
        best_positive_tabindex = item_in_segment;
      }
    } else if (tab_index == 0) {
      // Zero tabindex: keep the first one (in direction).
      if (!best_zero_tabindex) {
        best_zero_tabindex = item_in_segment;
      }
    } else {
      // Negative tabindex: keep the first one (in direction).
      if (!best_negative_tabindex) {
        best_negative_tabindex = item_in_segment;
      }
    }
    item_in_segment = NextFocusgroupItemInSegmentInDirection(*item_in_segment,
                                                             owner, direction);
  }

  if (memory_item_in_segment) {
    return memory_item;
  }

  // Return in priority order.
  if (best_positive_tabindex) {
    return best_positive_tabindex;
  }
  if (best_zero_tabindex) {
    return best_zero_tabindex;
  }
  if (best_negative_tabindex) {
    return best_negative_tabindex;
  }

  return nullptr;
}

bool FocusgroupControllerUtils::IsElementInOptedOutSubtree(
    const Element* element) {
  // Starting with this element, walk up the ancestor chain looking for an
  // opted-out focusgroup. Stop when we reach a focusgroup root or the document
  // root.
  while (element) {
    if (element->GetFocusgroupData().behavior == FocusgroupBehavior::kOptOut) {
      return true;
    }
    // Stop at the first focusgroup root.
    if (IsActualFocusgroup(element->GetFocusgroupData())) {
      return false;
    }
    element = FlatTreeTraversal::ParentElement(*element);
  }
  return false;
}

GridFocusgroupStructureInfo*
FocusgroupControllerUtils::CreateGridFocusgroupStructureInfoForGridRoot(
    Element* root) {
  if (IsA<LayoutTable>(root->GetLayoutObject()) &&
      root->GetFocusgroupData().behavior == FocusgroupBehavior::kGrid) {
    return MakeGarbageCollected<AutomaticGridFocusgroupStructureInfo>(
        root->GetLayoutObject());
  } else {
    // TODO(bebeaudr): Handle manual-grid focusgroups.
    return nullptr;
  }
}

Element* FocusgroupControllerUtils::WrappedFocusgroupCandidate(
    const Element* owner,
    const Element* current,
    FocusgroupDirection direction) {
  DCHECK(owner && current);
  DCHECK(IsFocusgroupItemWithOwner(current, owner));

  Element* wrap_candidate = nullptr;
  if (IsDirectionForward(direction)) {
    wrap_candidate = FirstFocusgroupItemWithin(owner);
  } else if (IsDirectionBackward(direction)) {
    wrap_candidate = LastFocusgroupItemWithin(owner);
  }

  // If the wrap candidate is valid and isn't the current element, return it.
  if (wrap_candidate && wrap_candidate != current) {
    return wrap_candidate;
  }
  return nullptr;
}

Element* FocusgroupControllerUtils::FirstFocusgroupItemWithin(
    const Element* owner) {
  if (!owner || !IsActualFocusgroup(owner->GetFocusgroupData())) {
    return nullptr;
  }

  for (Element* el = NextElement(owner, /*skip_subtree=*/false);
       el && FlatTreeTraversal::IsDescendantOf(*el, *owner);
       el = NextElement(el, /*skip_subtree=*/false)) {
    if (el != owner) {
      FocusgroupData data = el->GetFocusgroupData();
      if (data.behavior != FocusgroupBehavior::kNoBehavior) {
        // Skip nested focusgroup subtree entirely.
        el = NextElement(el, /*skip_subtree=*/true);
        if (!el) {
          break;
        }
        el = PreviousElement(el);
        continue;
      }
    }
    if (IsFocusgroupItemWithOwner(el, owner)) {
      return el;
    }
  }
  return nullptr;
}

Element* FocusgroupControllerUtils::LastFocusgroupItemWithin(
    const Element* owner) {
  if (!owner || !IsActualFocusgroup(owner->GetFocusgroupData())) {
    return nullptr;
  }

  Element* last = nullptr;
  for (Element* el = NextElement(owner, /*skip_subtree=*/false);
       el && FlatTreeTraversal::IsDescendantOf(*el, *owner);
       el = NextElement(el, /*skip_subtree=*/false)) {
    if (el != owner) {
      FocusgroupData data = el->GetFocusgroupData();
      if (data.behavior != FocusgroupBehavior::kNoBehavior) {
        el = NextElement(el, /*skip_subtree=*/true);
        if (!el) {
          break;
        }
        el = PreviousElement(el);
        continue;
      }
    }
    if (IsFocusgroupItemWithOwner(el, owner)) {
      last = el;
    }
  }
  return last;
}

bool FocusgroupControllerUtils::DoesFocusgroupContainBarrier(
    const Element& focusgroup) {
  DCHECK(IsActualFocusgroup(focusgroup.GetFocusgroupData()));

  // Walk through descendants looking for barriers.
  Element* el = NextElement(&focusgroup, /*skip_subtree=*/false);
  while (el && FlatTreeTraversal::IsDescendantOf(*el, focusgroup)) {
    FocusgroupData data = el->GetFocusgroupData();

    // We can't use First/LastFocusgroupItemWithin here since we need to
    // recursively check nested focusgroups and opted-out subtrees.
    if (el->IsFocusable()) {
      if (IsActualFocusgroup(data)) {
        if (DoesFocusgroupContainBarrier(*el)) {
          return true;
        }
        // Since we're recursively checking this focusgroup, we can skip its
        // children.
        el = NextElement(el, /*skip_subtree=*/true);
        continue;
      }
    }

    // Check opted-out subtrees.
    if (data.behavior == FocusgroupBehavior::kOptOut) {
      if (DoesOptOutSubtreeContainBarrier(*el)) {
        return true;
      }
      // Since we're recursively checking this subtree, we can skip its
      // children.
      el = NextElement(el, /*skip_subtree=*/true);
      continue;
    }
    el = NextElement(el, /*skip_subtree=*/false);
  }

  return false;
}

bool FocusgroupControllerUtils::DoesOptOutSubtreeContainBarrier(
    const Element& opted_out_root) {
  DCHECK(opted_out_root.GetFocusgroupData().behavior ==
         FocusgroupBehavior::kOptOut);

  // Check if the opted-out root itself is keyboard focusable.
  if (opted_out_root.IsKeyboardFocusableSlow()) {
    return true;
  }

  // Walk through descendants looking for barriers.
  Element* el = NextElement(&opted_out_root, /*skip_subtree=*/false);
  while (el && FlatTreeTraversal::IsDescendantOf(*el, opted_out_root)) {
    if (el->IsKeyboardFocusableSlow()) {
      return true;
    }

    // Check nested focusgroups recursively.
    FocusgroupData data = el->GetFocusgroupData();
    if (IsActualFocusgroup(data)) {
      if (DoesFocusgroupContainBarrier(*el)) {
        return true;
      }
      // Since we're recursively checking this focusgroup, we can skip its
      // children.
      el = NextElement(el, /*skip_subtree=*/true);
      continue;
    }
    el = NextElement(el, /*skip_subtree=*/false);
  }

  return false;
}

Element* FocusgroupControllerUtils::NextFocusgroupItemInSegmentInDirection(
    const Element& item,
    const Element& owner,
    mojom::blink::FocusType direction) {
  DCHECK(IsFocusgroupItemWithOwner(&item, &owner));

  // Walk in the given direction from the item to find the next item in its
  // segment. A segment is bounded by barriers (nested focusgroups or opted-out
  // subtrees) or by the focusgroup scope boundaries.
  Element* element =
      NextElementInDirection(&item, direction, /*skip_subtree=*/false);
  while (element && FlatTreeTraversal::IsDescendantOf(*element, owner)) {
    const Element* opted_out_subtree_root = nullptr;
    const Element* nested_focusgroup_owner = nullptr;
    if (direction == mojom::blink::FocusType::kBackward) {
      // When going backwards, we need to check the entire subtree of the
      // current element to see if it contains a barrier.
      opted_out_subtree_root = GetOptedOutSubtreeRoot(element);
      nested_focusgroup_owner = focusgroup::FindFocusgroupOwner(element);
      if (nested_focusgroup_owner == &owner) {
        nested_focusgroup_owner = nullptr;
      }
    } else {
      // When going forward, we only care if the element itself is a barrier.
      if (element->GetFocusgroupData().behavior ==
          FocusgroupBehavior::kOptOut) {
        opted_out_subtree_root = element;
      } else if (IsActualFocusgroup(element->GetFocusgroupData())) {
        nested_focusgroup_owner = element;
      }
    }
    // Check if this element contains a barrier.
    if (nested_focusgroup_owner) {
      if (DoesFocusgroupContainBarrier(*nested_focusgroup_owner)) {
        return nullptr;
      }
      // Since we've determined this nested focusgroup is not a barrier, we can
      // skip its children.
      element = NextElementInDirection(nested_focusgroup_owner, direction,
                                       /*skip_subtree=*/true);
      continue;
    }
    if (opted_out_subtree_root) {
      if (DoesOptOutSubtreeContainBarrier(*opted_out_subtree_root)) {
        return nullptr;
      }
      // Since we've determined this opted-out subtree is not a barrier, we can
      // skip its children.
      element = NextElementInDirection(opted_out_subtree_root, direction,
                                       /*skip_subtree=*/true);
      continue;
    }
    // We already know that the item is a descendant of owner, and is not opted
    // out nor in a nested focusgroup scope so we don't need to check that
    // again, all that matters is that it is focusable. If so, return it.
    if (element->IsFocusable()) {
      return element;
    }
    element =
        NextElementInDirection(element, direction, /*skip_subtree=*/false);
  }
  return nullptr;
}

Element* FocusgroupControllerUtils::FirstFocusgroupItemInSegment(
    const Element& item) {
  const Element* owner = focusgroup::FindFocusgroupOwner(&item);
  if (!owner || !item.IsFocusable()) {
    return nullptr;
  }

  // Walk backward from the item to find the start of its segment.
  // A segment starts after a barrier or at the beginning of the focusgroup
  // scope.
  Element* result = const_cast<Element*>(&item);
  for (Element* previous = NextFocusgroupItemInSegmentInDirection(
           item, *owner, mojom::blink::FocusType::kBackward);
       previous; previous = NextFocusgroupItemInSegmentInDirection(
                     *previous, *owner, mojom::blink::FocusType::kBackward)) {
    result = previous;
  }
  return result;
}

Element* FocusgroupControllerUtils::LastFocusgroupItemInSegment(
    const Element& item) {
  const Element* owner = focusgroup::FindFocusgroupOwner(&item);
  if (!owner || !item.IsFocusable()) {
    return nullptr;
  }

  // Walk forward from the item to find the end of its segment.
  // A segment ends before a barrier or at the end of the focusgroup scope.
  Element* result = const_cast<Element*>(&item);
  for (Element* next = NextFocusgroupItemInSegmentInDirection(
           item, *owner, mojom::blink::FocusType::kForward);
       next; next = NextFocusgroupItemInSegmentInDirection(
                 *next, *owner, mojom::blink::FocusType::kForward)) {
    result = next;
  }
  return result;
}

const Element* FocusgroupControllerUtils::GetOptedOutSubtreeRoot(
    const Element* element) {
  // Starting with this element, walk up the ancestor chain looking for an
  // opted-out focusgroup. Stop when we reach a focusgroup root or the document
  // root.
  while (element) {
    if (element->GetFocusgroupData().behavior == FocusgroupBehavior::kOptOut) {
      return element;
    }
    // Stop at the first focusgroup root.
    if (IsActualFocusgroup(element->GetFocusgroupData())) {
      return nullptr;
    }
    element = FlatTreeTraversal::ParentElement(*element);
  }
  return nullptr;
}

}  // namespace blink
