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
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/grid_focusgroup_structure_info.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

namespace {

// Helper class to manage visual-order traversal that respects reading-flow
// for focusgroup. Similar to FocusNavigation, but scoped to only the needs
// of focusgroup traversal.
class FocusgroupVisualOrderTraversalContext {
  STACK_ALLOCATED();

 public:
  bool VisitReadingFlowContainerIfNeeded(const Element* element) {
    if (const ContainerNode* container =
            FocusController::ReadingFlowContainerOrDisplayContents(
                element, /*find_for_items*/ true)) {
      const Element* container_element = DynamicTo<Element>(container);
      if (container_element &&
          !reading_flow_elements_.Contains(container_element)) {
        BuildReadingFlowElementMappings(*container_element);
        return true;
      }
    }
    return false;
  }

  Element* Next(const Element* current, bool skip_subtree) {
    VisitReadingFlowContainerIfNeeded(current);
    if (reading_flow_next_elements_.Contains(current)) {
      return reading_flow_next_elements_.at(current);
    }

    return FocusgroupControllerUtils::NextElement(current, skip_subtree);
  }

  Element* Previous(const Element* current, bool skip_subtree) {
    VisitReadingFlowContainerIfNeeded(current);

    Element* previous =
        reading_flow_previous_elements_.Contains(current)
            ? reading_flow_previous_elements_.at(current)
            : FocusgroupControllerUtils::PreviousElement(current, skip_subtree);

    // It is possible that |previous| itself is inside a reading-flow container
    // that we haven't built mappings for yet. In that case, we need to build
    // those mappings.
    VisitReadingFlowContainerIfNeeded(previous);

    // Now that we've built the necessary mappings, check again.
    if (reading_flow_previous_elements_.Contains(current)) {
      return reading_flow_previous_elements_.at(current);
    }
    return previous;
  }

  Element* NextInDirection(const Element* current,
                           mojom::blink::FocusType direction,
                           bool skip_subtree) {
    switch (direction) {
      case mojom::blink::FocusType::kForward:
        return Next(current, skip_subtree);
      case mojom::blink::FocusType::kBackward:
        return Previous(current, skip_subtree);
      default:
        NOTREACHED();
    }
  }

  void BuildReadingFlowElementMappings(const Element& reading_flow_element) {
    DCHECK(reading_flow_element.GetLayoutBox());
    DCHECK(!reading_flow_elements_.Contains(&reading_flow_element));
    reading_flow_elements_.insert(&reading_flow_element);
    // The reading flow container itself may be reordered, save the next element
    // so we can stitch the ordering together at the end.
    Element* after_reading_flow =
        reading_flow_next_elements_.Contains(&reading_flow_element)
            ? reading_flow_next_elements_.at(&reading_flow_element)
            : FocusgroupControllerUtils::NextElement(&reading_flow_element,
                                                     /*skip_subtree=*/true);
    const auto& reading_flow_children =
        reading_flow_element.ReadingFlowChildren();

    // This has the chance of over-allocating in the case where some children
    // are not elements or are pseudo-elements, but that's preferable to
    // an additional pass to count or dynamic resizing during insertion.
    reading_flow_next_elements_.ReserveCapacityForSize(
        reading_flow_next_elements_.size() + reading_flow_children.size());
    reading_flow_previous_elements_.ReserveCapacityForSize(
        reading_flow_previous_elements_.size() + reading_flow_children.size());

    Element* prev_element = const_cast<Element*>(&reading_flow_element);
    for (Node* reading_flow_node : reading_flow_children) {
      Element* child = DynamicTo<Element>(reading_flow_node);
      // Pseudo-elements in reading-flow are not focusable and should not be
      // included in the elements to traverse. Keep in sync with the behavior
      // in FocusNavigation::SetReadingFlowInfo.
      if (!child || child->IsPseudoElement()) {
        continue;
      }
      reading_flow_previous_elements_.Set(child, prev_element);
      if (prev_element) {
        reading_flow_next_elements_.Set(prev_element, child);
      }
      prev_element = child;
    }
    if (prev_element) {
      reading_flow_next_elements_.Set(prev_element, after_reading_flow);
      if (after_reading_flow) {
        reading_flow_previous_elements_.Set(after_reading_flow, prev_element);
      }
    }
  }

 private:
  // Set of reading flow containers we've already built mappings for.
  HeapHashSet<Member<const Element>> reading_flow_elements_;

  // Mappings of elements in reading-flow order, with the "current" element as
  // the key. If the focusgroup contains elements re-ordered different reading
  // flow containers, these mappings will combine them together to produce an
  // overall mapping.
  HeapHashMap<Member<const Element>, Member<Element>>
      reading_flow_next_elements_;
  HeapHashMap<Member<const Element>, Member<Element>>
      reading_flow_previous_elements_;
};

}  // namespace

FocusgroupDirection FocusgroupControllerUtils::FocusgroupDirectionForEvent(
    const KeyboardEvent* event) {
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
  mojom::blink::FocusType focus_direction =
      IsDirectionForward(direction) ? mojom::blink::FocusType::kForward
                                    : mojom::blink::FocusType::kBackward;

  // Use a stack of traversal contexts to handle reading-flow containers.
  FocusgroupVisualOrderTraversalContext traversal_context;

  Element* next_element = traversal_context.NextInDirection(
      current_item, focus_direction, /*skip_subtree=*/false);
  while (next_element &&
         FlatTreeTraversal::IsDescendantOf(*next_element, *owner)) {
    // Skip nested focusgroups and opted-out subtrees.
    FocusgroupData next_data = next_element->GetFocusgroupData();
    if (next_data.behavior == FocusgroupBehavior::kOptOut ||
        IsActualFocusgroup(next_data)) {
      next_element =
          traversal_context.NextInDirection(next_element, focus_direction,
                                            /*skip_subtree=*/true);
      continue;
    }
    if (IsFocusgroupItemWithOwner(next_element, owner)) {
      return next_element;
    }
    next_element =
        traversal_context.NextInDirection(next_element, focus_direction,
                                          /*skip_subtree=*/false);
  }
  return nullptr;
}

Element* FocusgroupControllerUtils::GetFocusgroupOwnerOfItem(
    const Element* element) {
  if (!element || !element->IsKeyboardFocusableSlow()) {
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
  if (!element->IsKeyboardFocusableSlow()) {
    return false;
  }

  // TODO(bebeaudr): Add support for manual grids, where the grid focusgroup
  // items aren't necessarily on an table cell layout object.
  return IsA<LayoutTableCell>(element->GetLayoutObject());
}

bool FocusgroupControllerUtils::IsEntryElementForFocusgroupSegment(
    const Element& item,
    const Element& owner) {
  if (!IsFocusgroupItemWithOwner(&item, &owner)) {
    return false;
  }
  return &item == GetEntryElementForFocusgroupSegment(item, owner);
}

const Element* FocusgroupControllerUtils::GetEntryElementForFocusgroupSegment(
    const Element& item,
    const Element& owner) {
  DCHECK(IsFocusgroupItemWithOwner(&item, &owner));

  // Always start from the beginning of the segment.
  const Element* first_item_in_segment = FirstFocusgroupItemInSegment(item);

  if (!first_item_in_segment) {
    return nullptr;
  }

  return GetEntryElementForFocusgroupSegmentFromFirst(*first_item_in_segment,
                                                      owner);
}

const Element*
FocusgroupControllerUtils::GetEntryElementForFocusgroupSegmentFromFirst(
    const Element& first_item_in_segment,
    const Element& owner) {
  DCHECK(IsFocusgroupItemWithOwner(&first_item_in_segment, &owner));
  // Validate precondition: element must be the first item in its segment.
  DCHECK_EQ(FirstFocusgroupItemInSegment(first_item_in_segment),
            &first_item_in_segment)
      << "GetEntryElementForFocusgroupSegmentFromFirst called with element "
         "that is not the first item in its segment.";

  Element* memory_item = owner.GetFocusgroupLastFocused();

  // Walk through all items in the segment to find the best candidate.
  const Element* item_in_segment = &first_item_in_segment;

  const Element* entry_priority_item = nullptr;
  const Element* first_item = nullptr;
  bool memory_item_in_segment = false;

  // Iterate through all items in segment.
  while (item_in_segment) {
    DCHECK(IsFocusgroupItemWithOwner(item_in_segment, &owner));
    if (item_in_segment->IsFocusedElementInDocument()) {
      // If another item in the segment is already focused, return it, as
      // only one focusgroup item per segment can be in the sequential focus
      // order.
      return item_in_segment;
    }

    if (memory_item && item_in_segment == memory_item) {
      // If we found the memory item, we no longer need to look for other
      // candidates, but do need to continue to ensure that there is no focused
      // element in the segment.
      memory_item_in_segment = true;
      item_in_segment = NextFocusgroupItemInSegmentInDirection(
          *item_in_segment, owner, mojom::blink::FocusType::kForward);
      continue;
    }

    // Check for focusgroup-entry-priority attribute.
    if (!entry_priority_item && HasFocusgroupEntryPriority(*item_in_segment)) {
      entry_priority_item = item_in_segment;
    }

    // Track the first item in segment.
    if (!first_item) {
      first_item = item_in_segment;
    }

    item_in_segment = NextFocusgroupItemInSegmentInDirection(
        *item_in_segment, owner, mojom::blink::FocusType::kForward);
  }

  if (memory_item_in_segment) {
    return memory_item;
  }

  // Return in priority order.
  if (entry_priority_item) {
    return entry_priority_item;
  }
  if (first_item) {
    return first_item;
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
    const Element* root) {
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
  FocusgroupVisualOrderTraversalContext traversal_context;
  for (Element* el = traversal_context.Next(owner, /*skip_subtree=*/false);
       el && FlatTreeTraversal::IsDescendantOf(*el, *owner);
       el = traversal_context.Next(el, /*skip_subtree=*/false)) {
    if (el != owner) {
      FocusgroupData data = el->GetFocusgroupData();
      if (data.behavior != FocusgroupBehavior::kNoBehavior) {
        // Skip nested focusgroup subtree entirely.
        el = traversal_context.Next(el, /*skip_subtree=*/true);
        if (!el) {
          break;
        }
        el = traversal_context.Previous(el, /*skip_subtree=*/false);
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

  FocusgroupVisualOrderTraversalContext traversal_context;
  Element* last = nullptr;
  for (Element* el = traversal_context.Next(owner, /*skip_subtree=*/false);
       el && FlatTreeTraversal::IsDescendantOf(*el, *owner);
       el = traversal_context.Next(el, /*skip_subtree=*/false)) {
    if (el != owner) {
      FocusgroupData data = el->GetFocusgroupData();
      if (data.behavior != FocusgroupBehavior::kNoBehavior) {
        el = traversal_context.Next(el, /*skip_subtree=*/true);
        if (!el) {
          break;
        }
        el = traversal_context.Previous(el, /*skip_subtree=*/false);
        continue;
      }
    }
    if (IsFocusgroupItemWithOwner(el, owner)) {
      last = el;
    }
  }
  return last;
}

bool FocusgroupControllerUtils::DoesElementContainBarrier(
    const Element& element) {
  // Check if the element itself is keyboard focusable.
  if (element.IsKeyboardFocusableSlow()) {
    return true;
  }
  // Check if any descendant is keyboard focusable.
  for (Node& node : FlatTreeTraversal::DescendantsOf(element)) {
    if (const Element* el = DynamicTo<Element>(node);
        el && el->IsKeyboardFocusableSlow()) {
      return true;
    }
  }
  return false;
}

const Element*
FocusgroupControllerUtils::NextFocusgroupItemInSegmentInDirection(
    const Element& item,
    const Element& owner,
    mojom::blink::FocusType direction) {
  DCHECK(IsFocusgroupItemWithOwner(&item, &owner));

  // Walk in the given direction from the item to find the next item in its
  // segment. A segment is bounded by barriers (nested focusgroups or opted-out
  // subtrees) or by the focusgroup scope boundaries.
  FocusgroupVisualOrderTraversalContext traversal_context;
  const Element* element =
      traversal_context.NextInDirection(&item, direction,
                                        /*skip_subtree=*/false);
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
      if (DoesElementContainBarrier(*nested_focusgroup_owner)) {
        return nullptr;
      }
      // Since we've determined this nested focusgroup is not a barrier, we can
      // skip its children.
      element =
          traversal_context.NextInDirection(nested_focusgroup_owner, direction,
                                            /*skip_subtree=*/true);
      continue;
    }
    if (opted_out_subtree_root) {
      if (DoesElementContainBarrier(*opted_out_subtree_root)) {
        return nullptr;
      }
      // Since we've determined this opted-out subtree is not a barrier, we can
      // skip its children.
      element =
          traversal_context.NextInDirection(opted_out_subtree_root, direction,
                                            /*skip_subtree=*/true);
      continue;
    }
    // We already know that the item is a descendant of owner, and is not opted
    // out nor in a nested focusgroup scope so we don't need to check that
    // again, all that matters is that it is focusable. If so, return it.
    if (element->IsKeyboardFocusableSlow()) {
      return element;
    }
    element = traversal_context.NextInDirection(element, direction,
                                                /*skip_subtree=*/false);
  }
  return nullptr;
}

const Element* FocusgroupControllerUtils::FirstFocusgroupItemInSegment(
    const Element& item) {
  const Element* owner = focusgroup::FindFocusgroupOwner(&item);
  if (!owner || !item.IsKeyboardFocusableSlow()) {
    return nullptr;
  }

  // Walk backward from the item to find the start of its segment.
  // A segment starts after a barrier or at the beginning of the focusgroup
  // scope.
  const Element* result = &item;
  for (const Element* previous = NextFocusgroupItemInSegmentInDirection(
           item, *owner, mojom::blink::FocusType::kBackward);
       previous; previous = NextFocusgroupItemInSegmentInDirection(
                     *previous, *owner, mojom::blink::FocusType::kBackward)) {
    result = previous;
  }
  return result;
}

const Element* FocusgroupControllerUtils::LastFocusgroupItemInSegment(
    const Element& item) {
  const Element* owner = focusgroup::FindFocusgroupOwner(&item);
  if (!owner || !item.IsKeyboardFocusableSlow()) {
    return nullptr;
  }

  // Walk forward from the item to find the end of its segment.
  // A segment ends before a barrier or at the end of the focusgroup scope.
  const Element* result = &item;
  for (const Element* next = NextFocusgroupItemInSegmentInDirection(
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
  const Element* current = element;
  while (current) {
    if (current->GetFocusgroupData().behavior == FocusgroupBehavior::kOptOut) {
      return current;
    }
    // Stop at the first focusgroup root.
    if (IsActualFocusgroup(current->GetFocusgroupData())) {
      return nullptr;
    }
    current = FlatTreeTraversal::ParentElement(*current);
  }
  return nullptr;
}

// static
bool FocusgroupControllerUtils::HasFocusgroupEntryPriority(
    const Element& element) {
  return element.FastHasAttribute(html_names::kFocusgroupEntryPriorityAttr);
}

}  // namespace blink
