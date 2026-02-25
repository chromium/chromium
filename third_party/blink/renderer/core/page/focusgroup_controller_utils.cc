// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/focusgroup_controller_utils.h"

#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
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
#include "third_party/blink/renderer/core/style/computed_style.h"
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
    const KeyboardEvent* event,
    const Element& focused_element) {
  DCHECK(event);
  if (event->ctrlKey() || event->metaKey() || event->shiftKey()) {
    return FocusgroupDirection::kNone;
  }

  // Determine the physical direction for the pressed arrow key.
  const AtomicString key(event->key());
  LogicalDirection logical_direction;
  if (key == keywords::kArrowDown || key == keywords::kArrowRight ||
      key == keywords::kArrowUp || key == keywords::kArrowLeft) {
    // Resolve the writing direction from the focused element's computed style.
    // This means arrow keys follow the element's local writing direction (e.g.,
    // an RTL item inside an LTR focusgroup uses RTL key mappings). Falls back
    // to horizontal-tb LTR when no style is available.
    const ComputedStyle* style = focused_element.GetComputedStyle();
    if (!style) {
      return FocusgroupDirection::kNone;
    }
    WritingDirectionMode writing_direction = style->GetWritingDirection();

    // Map the physical arrow key to a logical direction using the focused
    // element's writing direction. This correctly handles RTL (left/right
    // swap for inline) and vertical writing modes (axes swap).
    if (key == keywords::kArrowDown) {
      logical_direction = writing_direction.Bottom();
    } else if (key == keywords::kArrowRight) {
      logical_direction = writing_direction.Right();
    } else if (key == keywords::kArrowUp) {
      logical_direction = writing_direction.Top();
    } else {
      logical_direction = writing_direction.Left();
    }
  } else {
    return FocusgroupDirection::kNone;
  }

  switch (logical_direction) {
    case LogicalDirection::kInlineStart:
      return FocusgroupDirection::kBackwardInline;
    case LogicalDirection::kInlineEnd:
      return FocusgroupDirection::kForwardInline;
    case LogicalDirection::kBlockStart:
      return FocusgroupDirection::kBackwardBlock;
    case LogicalDirection::kBlockEnd:
      return FocusgroupDirection::kForwardBlock;
  }
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
    // Handle opted-out subtrees: skip entirely.
    if (HasExplicitOptOut(next_element)) {
      next_element =
          traversal_context.NextInDirection(next_element, focus_direction,
                                            /*skip_subtree=*/true);
      continue;
    }

    // Handle nested focusgroups: they can participate as items in the parent
    // focusgroup if they are keyboard focusable. After checking, we always
    // skip their subtree since their contents belong to the nested focusgroup.
    FocusgroupData next_data = next_element->GetFocusgroupData();
    if (IsActualFocusgroup(next_data)) {
      if (next_element->IsKeyboardFocusableSlow()) {
        return next_element;
      }
      next_element =
          traversal_context.NextInDirection(next_element, focus_direction,
                                            /*skip_subtree=*/true);
      continue;
    }

    if (IsFocusgroupItemWithOwner(next_element, owner)) {
      if (next_element->IsKeyboardFocusableSlow()) {
        return next_element;
      }
    }
    next_element =
        traversal_context.NextInDirection(next_element, focus_direction,
                                          /*skip_subtree=*/false);
  }
  return nullptr;
}

Element* FocusgroupControllerUtils::GetFocusgroupOwnerOfItem(
    const Element* element) {
  if (!element || !element->IsFocusable()) {
    return nullptr;
  }

  // An element with focusgroup="none" is opted out of focusgroup management.
  // It and its subtree should not be considered a focusgroup item.
  // Note: We only check for explicit opt-out here, not focused arrow key
  // handlers. Arrow key handlers are still focusgroup items for the purpose
  // of memory tracking and segment computation; they just have special
  // behavior during navigation.
  if (IsInExplicitlyOptedOutSubtree(element)) {
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
  if (!element->IsFocusable()) {
    return false;
  }

  // TODO(bebeaudr): Add support for manual grids, where the grid focusgroup
  // items aren't necessarily on an table cell layout object.
  return IsA<LayoutTableCell>(element->GetLayoutObject());
}

bool FocusgroupControllerUtils::IsInArrowKeyHandler(const Element* element) {
  return GetArrowKeyHandlerRoot(element) != nullptr;
}

bool FocusgroupControllerUtils::IsInArrowKeyHandler(
    const Element& element,
    FocusgroupDirection direction) {
  if (!RuntimeEnabledFeatures::FocusgroupEnabled(
          element.GetExecutionContext())) {
    return false;
  }

  Element* owner = focusgroup::FindFocusgroupOwner(&element);
  if (!owner) {
    return false;
  }

  FocusgroupData owner_data = owner->GetFocusgroupData();
  if (!focusgroup::IsActualFocusgroup(owner_data)) {
    return false;
  }

  // Determine which axis the navigation direction uses.
  FocusgroupFlags direction_axis = IsDirectionInline(direction)
                                       ? FocusgroupFlags::kInline
                                       : FocusgroupFlags::kBlock;

  // Check if the focusgroup owner enables this axis.
  FocusgroupFlags flags = owner_data.flags;
  // If the owner doesn't enable this axis, no conflict is possible.
  if (!(flags & direction_axis)) {
    return false;
  }

  // Walk up to find an arrow key handler that uses the navigation axis.
  const Element* current = &element;
  while (current && current != owner) {
    FocusgroupFlags native_axes = current->NativeArrowKeyAxes();
    if (native_axes & direction_axis) {
      return true;
    }
    current = FlatTreeTraversal::ParentElement(*current);
  }

  return false;
}

const Element* FocusgroupControllerUtils::GetArrowKeyHandlerRoot(
    const Element* element) {
  if (!element) {
    return nullptr;
  }

  if (!RuntimeEnabledFeatures::FocusgroupEnabled(
          element->GetExecutionContext())) {
    return nullptr;
  }

  Element* owner = focusgroup::FindFocusgroupOwner(element);
  if (!owner) {
    return nullptr;
  }

  FocusgroupData owner_data = owner->GetFocusgroupData();
  if (!focusgroup::IsActualFocusgroup(owner_data)) {
    return nullptr;
  }

  FocusgroupFlags flags = owner_data.flags;
  const bool has_axis_flags = static_cast<bool>(
      flags & (FocusgroupFlags::kInline | FocusgroupFlags::kBlock));
  const FocusgroupFlags enabled_axes =
      has_axis_flags
          ? (flags & (FocusgroupFlags::kInline | FocusgroupFlags::kBlock))
          : (FocusgroupFlags::kInline | FocusgroupFlags::kBlock);

  const Element* current = element;
  while (current && current != owner) {
    FocusgroupFlags native_axes = current->NativeArrowKeyAxes();
    if (native_axes & enabled_axes) {
      return current;
    }
    current = FlatTreeTraversal::ParentElement(*current);
  }

  return nullptr;
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
  const Element* first_item_in_segment =
      FocusgroupItemInSegment(item, FocusgroupItemPosition::kFirst);

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
  DCHECK_EQ(FocusgroupItemInSegment(first_item_in_segment,
                                    FocusgroupItemPosition::kFirst),
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
      // order. This applies even to focused arrow key handlers - the focused
      // element is always the entry point for its segment during Tab
      // navigation.
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

    // Check for focusgroupstart attribute.
    if (!entry_priority_item && IsFocusgroupStart(*item_in_segment)) {
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

bool FocusgroupControllerUtils::HasExplicitOptOut(const Element* element) {
  return element &&
         element->GetFocusgroupData().behavior == FocusgroupBehavior::kOptOut;
}

bool FocusgroupControllerUtils::IsInExplicitlyOptedOutSubtree(
    const Element* element) {
  // Starting with this element, walk up the ancestor chain looking for an
  // element with focusgroup="none". Stop when we reach a focusgroup root or
  // the document root.
  while (element) {
    if (HasExplicitOptOut(element)) {
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
    wrap_candidate =
        FocusgroupItemWithin(owner, FocusgroupItemPosition::kFirst);
  } else if (IsDirectionBackward(direction)) {
    wrap_candidate = FocusgroupItemWithin(owner, FocusgroupItemPosition::kLast);
  }

  // If the wrap candidate is valid and isn't the current element, return it.
  if (wrap_candidate && wrap_candidate != current) {
    return wrap_candidate;
  }
  return nullptr;
}

namespace {

// Finds the first or last focusgroup item within |owner|'s scope.
Element* FindFocusgroupItemWithin(const Element* owner,
                                  FocusgroupItemPosition position) {
  if (!owner || !IsActualFocusgroup(owner->GetFocusgroupData())) {
    return nullptr;
  }
  FocusgroupVisualOrderTraversalContext traversal_context;
  Element* result = nullptr;
  Element* el = traversal_context.Next(owner, /*skip_subtree=*/false);
  while (el && FlatTreeTraversal::IsDescendantOf(*el, *owner)) {
    bool skip_subtree = false;

    if (FocusgroupControllerUtils::HasExplicitOptOut(el)) {
      // Skip opted-out subtree entirely.
      skip_subtree = true;
    } else if (IsActualFocusgroup(el->GetFocusgroupData())) {
      // Nested focusgroup: check if the owner itself is a focusgroup item, but
      // skip its subtree as its contents belong to the nested focusgroup.
      if (el->IsKeyboardFocusableSlow()) {
        if (position == FocusgroupItemPosition::kFirst) {
          return el;
        }
        result = el;
      }
      skip_subtree = true;
    } else if (FocusgroupControllerUtils::IsFocusgroupItemWithOwner(el,
                                                                    owner) &&
               el->IsKeyboardFocusableSlow()) {
      if (position == FocusgroupItemPosition::kFirst) {
        return el;
      }
      result = el;
    }

    el = traversal_context.Next(el, skip_subtree);
  }
  return result;
}

}  // namespace

Element* FocusgroupControllerUtils::FocusgroupItemWithin(
    const Element* owner,
    FocusgroupItemPosition position) {
  return FindFocusgroupItemWithin(owner, position);
}

bool FocusgroupControllerUtils::ContainsKeyboardFocusableContent(
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
  // segment. A segment is bounded by barriers (nested focusgroups, opted-out
  // subtrees, or focused native arrow key handlers) or by the focusgroup scope
  // boundaries. Focused arrow key handlers act as segment boundaries to ensure
  // content after them remains reachable via Tab navigation.
  FocusgroupVisualOrderTraversalContext traversal_context;
  const Element* element =
      traversal_context.NextInDirection(&item, direction,
                                        /*skip_subtree=*/false);
  while (element && FlatTreeTraversal::IsDescendantOf(*element, owner)) {
    const Element* opted_out_subtree_root = nullptr;
    const Element* nested_focusgroup_owner = nullptr;
    if (direction == mojom::blink::FocusType::kBackward) {
      // When going backwards, we need to check the entire subtree of the
      // current element to see if it is in an excluded subtree.
      opted_out_subtree_root = FindExcludedSubtreeRoot(element);
      nested_focusgroup_owner = focusgroup::FindFocusgroupOwner(element);
      if (nested_focusgroup_owner == &owner) {
        nested_focusgroup_owner = nullptr;
      }
    } else {
      // When going forward, we only care if the element itself is an
      // excluded subtree root. Check for both explicit opt-out and focused
      // arrow key handlers, which are excluded to allow using Tab to navigate
      // out of them.
      if (IsExcludedSubtreeRoot(element)) {
        opted_out_subtree_root = element;
      } else if (IsActualFocusgroup(element->GetFocusgroupData())) {
        nested_focusgroup_owner = element;
      }
    }
    // Check if this subtree contains focusable content, making it a barrier.
    if (nested_focusgroup_owner) {
      if (ContainsKeyboardFocusableContent(*nested_focusgroup_owner)) {
        return nullptr;
      }
      // Since we've determined this nested focusgroup has no focusable content,
      // we can skip its children.
      element =
          traversal_context.NextInDirection(nested_focusgroup_owner, direction,
                                            /*skip_subtree=*/true);
      continue;
    }
    if (opted_out_subtree_root) {
      if (ContainsKeyboardFocusableContent(*opted_out_subtree_root)) {
        return nullptr;
      }
      // Since we've determined this excluded subtree has no focusable content,
      // we can skip its children.
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

const Element* FocusgroupControllerUtils::FocusgroupItemInSegment(
    const Element& item,
    FocusgroupItemPosition position) {
  const Element* owner = focusgroup::FindFocusgroupOwner(&item);
  if (!owner || !item.IsKeyboardFocusableSlow()) {
    return nullptr;
  }

  mojom::blink::FocusType direction = position == FocusgroupItemPosition::kFirst
                                          ? mojom::blink::FocusType::kBackward
                                          : mojom::blink::FocusType::kForward;

  // Walk in the appropriate direction from the item to find the segment
  // boundary.
  const Element* result = &item;
  for (const Element* next =
           NextFocusgroupItemInSegmentInDirection(item, *owner, direction);
       next; next = NextFocusgroupItemInSegmentInDirection(*next, *owner,
                                                           direction)) {
    result = next;
  }
  return result;
}

const Element* FocusgroupControllerUtils::FindExcludedSubtreeRoot(
    const Element* element) {
  // Walk up the ancestor chain looking for an excluded subtree root. Stop when
  // we reach a focusgroup root or the document root.
  const Element* current = element;
  while (current) {
    if (IsExcludedSubtreeRoot(current)) {
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

bool FocusgroupControllerUtils::IsExcludedSubtreeRoot(const Element* element) {
  if (!element) {
    return false;
  }
  // Check for explicit opt-out via focusgroup="none".
  if (HasExplicitOptOut(element)) {
    return true;
  }
  // Check for focused native arrow key handler (temporary exclusion).
  // Only the root of the arrow key handler subtree is an excluded root.
  const Element* arrow_key_handler = GetArrowKeyHandlerRoot(element);
  if (arrow_key_handler == element && element->IsFocusedElementInDocument()) {
    return true;
  }
  return false;
}

// static
bool FocusgroupControllerUtils::IsFocusgroupStart(const Element& element) {
  return element.FastHasAttribute(html_names::kFocusgroupstartAttr);
}

}  // namespace blink
