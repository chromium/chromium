// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/focusgroup_controller_utils.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/page/grid_focusgroup_structure_info.h"

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

bool FocusgroupControllerUtils::FocusgroupExtendsInAxis(
    FocusgroupFlags extending_focusgroup,
    FocusgroupFlags focusgroup,
    FocusgroupDirection direction) {
  if (focusgroup == FocusgroupFlags::kNone ||
      extending_focusgroup == FocusgroupFlags::kNone) {
    return false;
  }

  return extending_focusgroup & FocusgroupFlags::kExtend &&
         (IsAxisSupported(focusgroup, direction) ==
          IsAxisSupported(extending_focusgroup, direction));
}

Element* FocusgroupControllerUtils::FindNearestFocusgroupAncestor(
    const Element* element,
    FocusgroupType type) {
  if (!element)
    return nullptr;

  for (Element* ancestor = FlatTreeTraversal::ParentElement(*element); ancestor;
       ancestor = FlatTreeTraversal::ParentElement(*ancestor)) {
    FocusgroupFlags ancestor_flags = ancestor->GetFocusgroupFlags();
    if (ancestor_flags != FocusgroupFlags::kNone) {
      switch (type) {
        case FocusgroupType::kGrid:
          // TODO(bebeaudr): Support grid focusgroups that aren't based on the
          // table layout objects.
          if (ancestor_flags & FocusgroupFlags::kGrid &&
              IsA<LayoutTable>(ancestor->GetLayoutObject())) {
            return ancestor;
          }
          break;
        case FocusgroupType::kLinear:
          if (!(ancestor_flags & FocusgroupFlags::kGrid))
            return ancestor;
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
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
  if (skip_subtree)
    node = FlatTreeTraversal::NextSkippingChildren(*current);
  else
    node = FlatTreeTraversal::Next(*current);

  Element* next_element;
  // Here, we don't need to skip the subtree when getting the next element since
  // we've already skipped the subtree we wanted to skipped by calling
  // NextSkippingChildren above.
  for (; node; node = FlatTreeTraversal::Next(*node)) {
    next_element = DynamicTo<Element>(node);
    if (next_element)
      return next_element;
  }
  return nullptr;
}

Element* FocusgroupControllerUtils::PreviousElement(const Element* current) {
  DCHECK(current);
  Node* node = FlatTreeTraversal::Previous(*current);

  Element* previous_element;
  for (; node; node = FlatTreeTraversal::Previous(*node)) {
    previous_element = DynamicTo<Element>(node);
    if (previous_element)
      return previous_element;
  }
  return nullptr;
}

Element* FocusgroupControllerUtils::LastElementWithin(const Element* current) {
  DCHECK(current);
  Node* last_node = FlatTreeTraversal::LastWithin(*current);

  // We now have the last Node, but it might not be the last Element. Find it
  // by going to the previous element in preorder if needed.
  Element* last_element;
  for (; last_node && last_node != current;
       last_node = FlatTreeTraversal::Previous(*last_node)) {
    last_element = DynamicTo<Element>(last_node);
    if (last_element)
      return last_element;
  }
  return nullptr;
}

bool FocusgroupControllerUtils::IsFocusgroupItem(const Element* element) {
  if (!element || !element->IsFocusable())
    return false;

  // All children of a focusgroup are considered focusgroup items if they are
  // focusable.
  Element* parent = FlatTreeTraversal::ParentElement(*element);
  if (!parent)
    return false;

  FocusgroupFlags parent_flags = parent->GetFocusgroupFlags();
  return parent_flags != FocusgroupFlags::kNone;
}

// This function is called whenever the |element| passed by parameter has fallen
// into a subtree while navigating backward. Its objective is to prevent
// |element| from having descended into a non-extending focusgroup. When it
// detects its the case, it returns |element|'s first ancestor who is still part
// of the same focusgroup as |stop_ancestor|. The returned element is
// necessarily an element part of the previous focusgroup, but not necessarily a
// focusgroup item.
//
// |stop_ancestor| might be a focusgroup root itself or be a descendant of one.
// Regardless, given the assumption that |stop_ancestor| is always part of the
// previous focusgroup, we can stop going up |element|'s ancestors chain as soon
// as we reached it.
//
// Let's consider this example:
//           fg1
//      ______|_____
//      |          |
//      a1       a2
//      |
//     fg2
//    __|__
//    |   |
//    b1  b2
//
// where |fg2| is a focusgroup that doesn't extend the focusgroup |fg1|. While
// |fg2| is part of the focusgroup |fg1|, its subtree isn't. If the focus is on
// |a2|, the second item of the top-most focusgroup, and we go backward using
// the arrow keys, the focus should move to |fg2|. It shouldn't go inside of
// |fg2|, since it's a different focusgroup that doesn't extend its parent
// focusgroup.
//
// However, the previous element in preorder traversal from |a2| is |b2|, which
// isn't part of the same focusgroup. This function aims at fixing this by
// moving the current element to its parent, which is part of the previous
// focusgroup we were in (when we were on |a2|), |fg1|.
Element* FocusgroupControllerUtils::AdjustElementOutOfUnrelatedFocusgroup(
    Element* element,
    Element* stop_ancestor,
    FocusgroupDirection direction) {
  DCHECK(element);
  DCHECK(stop_ancestor);

  // Get the previous focusgroup we were part of (|stop_ancestor| was
  // necessarily part of it: it was either the focusgroup itself or a descendant
  // of that focusgroup).
  FocusgroupFlags focusgroup_flags = stop_ancestor->GetFocusgroupFlags();
  if (focusgroup_flags == FocusgroupFlags::kNone) {
    Element* focusgroup =
        FindNearestFocusgroupAncestor(stop_ancestor, FocusgroupType::kLinear);
    DCHECK(focusgroup);
    focusgroup_flags = focusgroup->GetFocusgroupFlags();
  }

  // Go over each ancestor of the |element| in order to validate that it is
  // still part of the previous focusgroup. If it isn't, set the ancestor that
  // broke one of the conditions as the |adjusted_element| and continue the
  // loop from there.
  Element* adjusted_element = element;
  for (Element* ancestor = FlatTreeTraversal::ParentElement(*element); ancestor;
       ancestor = FlatTreeTraversal::ParentElement(*ancestor)) {
    if (ancestor == stop_ancestor)
      break;

    // We consider |element| as being part of a different focusgroup than the
    // one we were previously in when one of its ancestor is a focusgroup that
    // doesn't extend the previous one.
    FocusgroupFlags ancestor_flags = ancestor->GetFocusgroupFlags();
    if (ancestor_flags != FocusgroupFlags::kNone &&
        !FocusgroupExtendsInAxis(ancestor_flags, focusgroup_flags, direction)) {
      adjusted_element = ancestor;
    }
  }

  return adjusted_element;
}

bool FocusgroupControllerUtils::IsGridFocusgroupItem(const Element* element) {
  DCHECK(element);
  if (!element->IsFocusable())
    return false;

  // TODO(bebeaudr): Add support for manual grids, where the grid focusgroup
  // items aren't necessarily on an table cell layout object.
  return IsA<LayoutTableCell>(element->GetLayoutObject());
}

GridFocusgroupStructureInfo*
FocusgroupControllerUtils::CreateGridFocusgroupStructureInfoForGridRoot(
    Element* root) {
  if (IsA<LayoutTable>(root->GetLayoutObject()) &&
      root->GetFocusgroupFlags() & FocusgroupFlags::kGrid) {
    return MakeGarbageCollected<AutomaticGridFocusgroupStructureInfo>(
        root->GetLayoutObject());
  } else {
    // TODO(bebeaudr): Handle manual-grid focusgroups.
    return nullptr;
  }
}

}  // namespace blink
