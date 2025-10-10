// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/focusgroup_controller_utils.h"

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
  if (!current) {
    return nullptr;
  }
  if (IsDirectionForward(direction)) {
    return NextElement(current, skip_subtree);
  }
  if (IsDirectionBackward(direction)) {
    return PreviousElement(current, skip_subtree);
  }
  return nullptr;
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

bool FocusgroupControllerUtils::IsFocusgroupItemWithOwner(
    const Element* element,
    const Element* focusgroup_owner) {
  if (!element || !element->IsFocusable() || !focusgroup_owner) {
    return false;
  }
  if (!IsActualFocusgroup(focusgroup_owner->GetFocusgroupData())) {
    return false;
  }

  // An element is a focusgroup item in a specific focusgroup context if:
  // 1. It is focusable.
  // 2. It is not opted out or in an opted out subtree.
  // 3. It is a descendant of a focusgroup.
  // 4. It is not inside a nested focusgroup which would create a separate
  // scope.

  // Check if this element has been opted out from focusgroup participation.
  if (IsElementInOptedOutSubtree(element)) {
    return false;
  }

  // Check if the element is a descendant of the focusgroup context.
  bool is_descendant = false;
  for (Element* ancestor = FlatTreeTraversal::ParentElement(*element); ancestor;
       ancestor = FlatTreeTraversal::ParentElement(*ancestor)) {
    if (ancestor == focusgroup_owner) {
      is_descendant = true;
      break;
    }
  }
  if (!is_descendant) {
    return false;
  }

  // Check if there's any nested focusgroup between this element and the
  // focusgroup context. If so, this element belongs to the nested focusgroup,
  // not the outer focusgroup.
  for (Element* ancestor = FlatTreeTraversal::ParentElement(*element);
       ancestor && ancestor != focusgroup_owner;
       ancestor = FlatTreeTraversal::ParentElement(*ancestor)) {
    FocusgroupData ancestor_data = ancestor->GetFocusgroupData();
    if (IsActualFocusgroup(ancestor_data)) {
      // Found a nested focusgroup - this element belongs to that scope instead.
      return false;
    }
  }

  return true;
}

// This function is called whenever the |element| passed by parameter has fallen
// into a subtree while navigating backward. Its objective is to prevent
// |element| from having descended into an opted-out focusgroup. When it
// detects this case, it returns |element|'s first ancestor who is still part
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
// where |fg2| is a focusgroup that opts out of the focusgroup |fg1|. While
// elements within |fg2| are not managed by |fg1|. If the focus is on
// |a2|, the second item of the top-most focusgroup, and we go backward using
// the arrow keys, the focus should move to |fg2|. It shouldn't go inside of
// |fg2|, since it's a different focusgroup that has opted out of its parent
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
  FocusgroupData focusgroup_data = stop_ancestor->GetFocusgroupData();
  if (focusgroup_data.behavior == FocusgroupBehavior::kNoBehavior) {
    Element* focusgroup =
        FindNearestFocusgroupAncestor(stop_ancestor, FocusgroupType::kLinear);
    DCHECK(focusgroup);
    focusgroup_data = focusgroup->GetFocusgroupData();
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
    // one we were previously in when one of its ancestor has any focusgroup
    // declaration, which creates a separate scope.
    FocusgroupData ancestor_data = ancestor->GetFocusgroupData();
    if (IsActualFocusgroup(ancestor_data)) {
      adjusted_element = ancestor;
    }
  }

  return adjusted_element;
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

}  // namespace blink
