// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/focusgroup_controller.h"

#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/focusgroup_controller_utils.h"
#include "third_party/blink/renderer/core/page/grid_focusgroup_structure_info.h"

namespace blink {

using utils = FocusgroupControllerUtils;

// static
bool FocusgroupController::HandleArrowKeyboardEvent(KeyboardEvent* event,
                                                    const LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->DomWindow());
  ExecutionContext* context = frame->DomWindow()->GetExecutionContext();
  DCHECK(RuntimeEnabledFeatures::FocusgroupEnabled(context));

  FocusgroupDirection direction = utils::FocusgroupDirectionForEvent(event);
  if (direction == FocusgroupDirection::kNone)
    return false;

  if (!frame->GetDocument())
    return false;

  Element* focused = frame->GetDocument()->FocusedElement();
  if (!focused || focused != event->target()) {
    // The FocusgroupController shouldn't handle this arrow key event when the
    // focus already moved to a different element than where it came from. The
    // webpage likely had a key-handler that moved the focus.
    return false;
  }

  return Advance(focused, direction);
}

// static
bool FocusgroupController::Advance(Element* initial_element,
                                   FocusgroupDirection direction) {
  // Only allow grid focusgroup navigation when the focus is on a grid
  // focusgroup item.
  Element* grid_root = utils::FindNearestFocusgroupAncestor(
      initial_element, FocusgroupType::kGrid);
  if (grid_root && utils::IsGridFocusgroupItem(initial_element))
    return AdvanceInGrid(initial_element, grid_root, direction);

  // Only allow linear focusgroup navigation when the focus is on a focusgroup
  // item.
  if (!utils::IsFocusgroupItem(initial_element))
    return false;

  if (utils::IsDirectionForward(direction)) {
    return AdvanceForward(initial_element, direction);
  } else {
    DCHECK(utils::IsDirectionBackward(direction));
    return AdvanceBackward(initial_element, direction);
  }
}

// static
bool FocusgroupController::AdvanceForward(Element* initial_element,
                                          FocusgroupDirection direction) {
  DCHECK(initial_element);
  DCHECK(utils::IsDirectionForward(direction));
  DCHECK(utils::IsFocusgroupItem(initial_element));

  Element* nearest_focusgroup = utils::FindNearestFocusgroupAncestor(
      initial_element, FocusgroupType::kLinear);
  // We only allow focusgroup navigation when we are inside of a focusgroup.
  if (!nearest_focusgroup)
    return false;

  // When the focusgroup we're in doesn't support the axis of the arrow key
  // pressed, it might still be able to descend so we can't return just yet.
  // However, if it can't descend, we should return right away.
  bool can_only_descend = !utils::IsAxisSupported(
      nearest_focusgroup->GetFocusgroupFlags(), direction);

  // We use the first element after the focusgroup we're in, excluding its
  // subtree, as a shortcut to determine if we exited the current focusgroup
  // without having to compute the current focusgroup ancestor on every pass.
  Element* first_element_after_focusgroup =
      utils::NextElement(nearest_focusgroup, /* skip_subtree */ true);

  Element* current = initial_element;

  while (true) {
    // 1. Determine whether to descend in other focusgroup.
    bool skip_subtree = false;
    FocusgroupFlags current_flags = current->GetFocusgroupFlags();
    bool descended = false;
    if (current_flags != FocusgroupFlags::kNone) {
      // When we're on a non-extending focusgroup, we shouldn't go into it. Same
      // for when we're at the root of an extending focusgroup that doesn't
      // support the axis of the arrow pressed.
      if (!(current_flags & FocusgroupFlags::kExtend) ||
          !utils::IsAxisSupported(current_flags, direction)) {
        skip_subtree = true;
      } else {
        nearest_focusgroup = current;
        first_element_after_focusgroup =
            utils::NextElement(nearest_focusgroup, /* skip_subtree */ true);
        descended = true;
      }
    }

    // See comment where |can_only_descend| is declared.
    if (can_only_descend && !descended)
      return false;

    // 2. Move |current| to the next element.
    current = utils::NextElement(current, skip_subtree);

    // 3. When |current| is located on the next element after the focusgroup
    // we're currently in, it means that we just exited the current
    // focusgroup we were in. We need to validate that we have the right to
    // exit it, since there are a few cases that might prevent us from going
    // to the next element. See the function `CanExitFocusgroupForward` for more
    // details about when we shouldn't allow exiting the current focusgroup.
    //
    // When this is true, we have exited the current focusgroup we were in. If
    // we were in an extending focusgroup, we should advance to the next item in
    // the parent focusgroup if the axis is supported.
    if (current && current == first_element_after_focusgroup) {
      if (CanExitFocusgroupForward(nearest_focusgroup, current, direction)) {
        nearest_focusgroup = utils::FindNearestFocusgroupAncestor(
            current, FocusgroupType::kLinear);
        first_element_after_focusgroup =
            utils::NextElement(nearest_focusgroup, /* skip_subtree */ true);
      } else {
        current = nullptr;
      }
    }

    // 4. When |current| is null, try to wrap.
    if (!current) {
      current = WrapForward(nearest_focusgroup, direction);

      if (!current) {
        // We couldn't wrap and we're out of options.
        break;
      }
    }

    // Avoid looping infinitely by breaking when the next logical element is the
    // one we started on.
    if (current == initial_element)
      break;

    // 5. |current| is finally on the next element. Focus it if it's one that
    // should be part of the focusgroup, otherwise continue the loop until it
    // finds the next item or can't find any.
    if (utils::IsFocusgroupItem(current)) {
      Focus(current, direction);
      return true;
    }
  }
  return false;
}

// static
//
// This function validates that we can exit the current focusgroup by calling
// `CanExitFocusgroupForwardRecursive`, which validates that all ancestor
// focusgroups can be exited safely. We need to validate that the ancestor
// focusgroups can be exited only if they are exited. Here are the key scenarios
// where we prohibit a focusgroup from being exited: a. If we're going to an
// element that isn't part of a focusgroup. b. If we're exiting a root
// focusgroup (one that doesn't extend). c. If we're going to a focusgroup that
// doesn't support the direction. d. If we're exiting a focusgroup that should
// wrap.
bool FocusgroupController::CanExitFocusgroupForward(
    const Element* exiting_focusgroup,
    const Element* next_element,
    FocusgroupDirection direction) {
  DCHECK(exiting_focusgroup);
  DCHECK(next_element);
  DCHECK(utils::NextElement(exiting_focusgroup, /*skip_subtree */ true) ==
         next_element);

  const Element* next_element_focusgroup = utils::FindNearestFocusgroupAncestor(
      next_element, FocusgroupType::kLinear);
  if (!next_element_focusgroup)
    return false;

  return CanExitFocusgroupForwardRecursive(
      exiting_focusgroup, next_element, direction,
      utils::WrapsInDirection(exiting_focusgroup->GetFocusgroupFlags(),
                              direction));
}

// static
bool FocusgroupController::CanExitFocusgroupForwardRecursive(
    const Element* exiting_focusgroup,
    const Element* next_element,
    FocusgroupDirection direction,
    bool check_wrap) {
  DCHECK(exiting_focusgroup);
  DCHECK(next_element);

  // When this is true, we are not exiting |exiting_focusgroup| and thus won't
  // be exiting any ancestor focusgroup.
  if (utils::NextElement(exiting_focusgroup, /* skip_subtree */ true) !=
      next_element) {
    return true;
  }

  FocusgroupFlags exiting_focusgroup_flags =
      exiting_focusgroup->GetFocusgroupFlags();
  DCHECK(exiting_focusgroup_flags != FocusgroupFlags::kNone);

  if (!(exiting_focusgroup_flags & FocusgroupFlags::kExtend))
    return false;

  const Element* parent_focusgroup = utils::FindNearestFocusgroupAncestor(
      exiting_focusgroup, FocusgroupType::kLinear);
  FocusgroupFlags parent_focusgroup_flags =
      parent_focusgroup ? parent_focusgroup->GetFocusgroupFlags()
                        : FocusgroupFlags::kNone;

  DCHECK(utils::IsAxisSupported(exiting_focusgroup_flags, direction));
  if (!utils::IsAxisSupported(parent_focusgroup_flags, direction))
    return false;

  if (check_wrap) {
    DCHECK(utils::WrapsInDirection(exiting_focusgroup_flags, direction));
    if (!utils::WrapsInDirection(parent_focusgroup_flags, direction))
      return false;
  }

  return CanExitFocusgroupForwardRecursive(parent_focusgroup, next_element,
                                           direction, check_wrap);
}

// static
Element* FocusgroupController::WrapForward(Element* nearest_focusgroup,
                                           FocusgroupDirection direction) {
  // 1. Get the focusgroup that initiates the wrapping scope in this axis. We
  // need to go up to the root-most focusgroup in order to be able to get the
  // "next" element, ie. the first item of this focusgroup. Stopping at the
  // first focusgroup that supports wrapping in that axis would break the
  // extend behavior and return the wrong element.
  Element* focusgroup_wrap_root = nullptr;
  for (Element* focusgroup = nearest_focusgroup; focusgroup;
       focusgroup = utils::FindNearestFocusgroupAncestor(
           focusgroup, FocusgroupType::kLinear)) {
    FocusgroupFlags flags = focusgroup->GetFocusgroupFlags();
    if (!utils::WrapsInDirection(flags, direction))
      break;

    focusgroup_wrap_root = focusgroup;

    if (!(flags & FocusgroupFlags::kExtend))
      break;
  }

  // 2. There are no next valid element and we can't wrap - `AdvanceForward`
  // should fail.
  if (!focusgroup_wrap_root)
    return nullptr;

  // 3. Set the focus on the first element within the subtree of the
  // current focusgroup.
  return utils::NextElement(focusgroup_wrap_root, /* skip_subtree */ false);
}

// static
bool FocusgroupController::AdvanceBackward(Element* initial_element,
                                           FocusgroupDirection direction) {
  DCHECK(initial_element);
  DCHECK(utils::IsDirectionBackward(direction));
  DCHECK(utils::IsFocusgroupItem(initial_element));

  // 1. Validate that we're in a focusgroup. Keep the reference to the current
  // focusgroup we're in since we'll use it if we need to wrap.
  Element* initial_focusgroup = utils::FindNearestFocusgroupAncestor(
      initial_element, FocusgroupType::kLinear);
  if (!initial_focusgroup)
    return false;
  bool can_only_ascend = !utils::IsAxisSupported(
      initial_focusgroup->GetFocusgroupFlags(), direction);

  Element* current = initial_element;
  Element* parent = FlatTreeTraversal::ParentElement(*current);
  while (true) {
    // 2. To find the previous focusgroup item, we start by getting the previous
    // element in preorder traversal. We are guaranteed to have a non-null
    // previous element since, below, we return as soon as the current as
    // reached the root most focusgroup.
    current = utils::PreviousElement(current);
    DCHECK(current);

    // 3. When going to the previous element in preorder traversal, there are 3
    // possible cases. We either moved:
    // i. to the sibling of the last element;
    // ii. to a descendant of the sibling of the last element;
    // iii. to the parent of the last element.
    //
    // When in (i), we know we are still part of the focusgroup the last element
    // was in. We can assume that the value of |current| is valid.
    //
    // When in (ii), we need to validate that we didn't descend into a different
    // focusgroup. `utils::AdjustElementOutOfUnrelatedFocusgroup` takes care of
    // that and, if it did descend in a separate focusgroup, it will return an
    // adjusted value for |current| out of that other focusgroup.
    //
    // When in (iii), we first need to try to wrap. If it succeeded, the
    // |current| element will be located on the last element of the focusgroup
    // and might have descended into another focusgroup. Once again, we'll need
    // to validate and potentially adjust the element using
    // `utils::AdjustElementOutOfUnrelatedFocusgroup`. If we can't wrap, we
    // must validate that |current|, which is now located on its parent, is
    // still part of the focusgroup.
    bool ascended = false;
    if (current == parent) {
      // Case (iii).
      Element* wrap_result = WrapBackward(current, direction);
      if (wrap_result) {
        current = utils::AdjustElementOutOfUnrelatedFocusgroup(
            wrap_result, parent, direction);
        parent = FlatTreeTraversal::ParentElement(*current);
      } else {
        // Wrapping wasn't an option. At this point, we can only attempt to
        // ascend to the parent.

        // We can't ascend out of a non-extending focusgroup.
        FocusgroupFlags current_flags = current->GetFocusgroupFlags();
        if (current_flags != FocusgroupFlags::kNone &&
            !(current_flags & FocusgroupFlags::kExtend)) {
          return false;
        }

        // We can't ascend if there is no focusgroup ancestor.
        Element* parent_focusgroup = utils::FindNearestFocusgroupAncestor(
            current, FocusgroupType::kLinear);
        if (!parent_focusgroup)
          return false;

        // We can't ascend if the parent focusgroup doesn't support the axis of
        // the arrow key pressed.
        if (!utils::IsAxisSupported(parent_focusgroup->GetFocusgroupFlags(),
                                    direction)) {
          return false;
        }

        // At this point, we are certain that we can ascend to the parent
        // element.
        ascended = true;
        parent = FlatTreeTraversal::ParentElement(*parent);
        // No need to check if the new |parent| is null or not because, if that
        // was the case, the check above for the |parent_focusgroup| would have
        // failed and returned early.
      }
    } else if (FlatTreeTraversal::ParentElement(*current) != parent) {
      // Case (ii).
      current = utils::AdjustElementOutOfUnrelatedFocusgroup(current, parent,
                                                             direction);
      parent = FlatTreeTraversal::ParentElement(*current);
    }

    // Avoid looping infinitely by breaking when the previous logical element is
    // the one we started on.
    if (current == initial_element)
      break;

    // 4. At this point, we know that |current| is a valid element in our
    // focusgroup. The only thing left to do is set the focus on the element if
    // it's a focusgroup item and we're allowed to do so. If not, we'll stay in
    // the loop until we find a suitable previous focusgroup item.
    if (!utils::IsFocusgroupItem(current))
      continue;

    // 5. When in a focusgroup that doesn't support the arrow axis, we still
    // iterate over the previous elements in the hopes of ascending to another
    // focusgroup. Ascending from a focusgroup that doesn't support the arrow
    // axis is permitted only when the focused element was the first focusgroup
    // item in a focusgroup.
    if (can_only_ascend && !ascended) {
      // Here, since we found out that there was a previous item, ascending is
      // not an option anymore so we break out of the loop to indicate that
      // advancing backward wasn't possible.
      break;
    }

    Focus(current, direction);
    return true;
  }

  return false;
}

// static
Element* FocusgroupController::WrapBackward(Element* current,
                                            FocusgroupDirection direction) {
  DCHECK(current);
  DCHECK(utils::IsDirectionBackward(direction));

  FocusgroupFlags current_flags = current->GetFocusgroupFlags();

  if (current_flags == FocusgroupFlags::kNone)
    return nullptr;

  if (!utils::IsAxisSupported(current_flags, direction))
    return nullptr;

  if (!utils::WrapsInDirection(current_flags, direction))
    return nullptr;

  // Don't wrap when on a focusgroup that got its wrapping behavior in this
  // axis from its parent focusgroup - that other focusgroup will handle the
  // wrapping once we'll reach it.
  Element* parent_focusgroup =
      utils::FindNearestFocusgroupAncestor(current, FocusgroupType::kLinear);
  if (current_flags & FocusgroupFlags::kExtend && parent_focusgroup &&
      utils::WrapsInDirection(parent_focusgroup->GetFocusgroupFlags(),
                              direction)) {
    return nullptr;
  }

  return utils::LastElementWithin(current);
}

// static
bool FocusgroupController::AdvanceInGrid(Element* initial_element,
                                         Element* grid_root,
                                         FocusgroupDirection direction) {
  DCHECK(initial_element);
  DCHECK(grid_root);

  grid_root->GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kFocusgroup);

  auto* helper = utils::CreateGridFocusgroupStructureInfoForGridRoot(grid_root);

  Element* current = initial_element;
  while (true) {
    // 1. Move to the next cell in the appropriate |direction|.
    Element* previous = current;
    switch (direction) {
      case FocusgroupDirection::kBackwardInline:
        current = helper->PreviousCellInRow(current);
        break;
      case FocusgroupDirection::kForwardInline:
        current = helper->NextCellInRow(current);
        break;
      case FocusgroupDirection::kBackwardBlock:
        current = helper->PreviousCellInColumn(current);
        break;
      case FocusgroupDirection::kForwardBlock:
        current = helper->NextCellInColumn(current);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }

    // 2. If no next cell was found, attempt to wrap/flow.
    if (!current) {
      current = WrapOrFlowInGrid(previous, direction, helper);
      if (!current) {
        // There are no cell and we were unable to wrap/flow. The advance step
        // failed.
        break;
      }
    }

    // Avoid looping infinitely by breaking when the new next/previous logical
    // element is the one we started on.
    if (current == initial_element)
      break;

    // 3. Only set the focus on grid focusgroup items. If we're on a cell that
    // isn't a grid focusgroup item, keep going to the next/previous element
    // until we find a valid item or we exhausted all the options.
    if (utils::IsGridFocusgroupItem(current)) {
      Focus(current, direction);
      return true;
    }
  }

  return false;
}

// static
Element* FocusgroupController::WrapOrFlowInGrid(
    Element* element,
    FocusgroupDirection direction,
    GridFocusgroupStructureInfo* helper) {
  DCHECK(element);
  DCHECK(helper->Root());
  FocusgroupFlags flags = helper->Root()->GetFocusgroupFlags();

  switch (direction) {
    case FocusgroupDirection::kBackwardInline:
      // This is only possible when on the first cell within a row.
      if (flags & FocusgroupFlags::kWrapInline) {
        // Wrapping backward in a row means that we should move the focus to the
        // last cell in the same row.
        Element* row = helper->RowForCell(element);
        DCHECK(row);
        return helper->LastCellInRow(row);
      } else if (flags & FocusgroupFlags::kRowFlow) {
        // Flowing backward in a row means that we should move the focus to the
        // last cell of the previous row. If there is no previous row, move the
        // focus to the last cell of the last row within the grid.
        Element* row = helper->RowForCell(element);
        Element* previous_row = helper->PreviousRow(row);
        if (!previous_row) {
          previous_row = helper->LastRow();
        }
        return helper->LastCellInRow(previous_row);
      }
      break;

    case FocusgroupDirection::kForwardInline:
      // This is only possible when on the last cell within a row.
      if (flags & FocusgroupFlags::kWrapInline) {
        // Wrapping forward in a row means that we should move the focus to the
        // first cell of the same row.
        Element* row = helper->RowForCell(element);
        DCHECK(row);
        return helper->FirstCellInRow(row);
      } else if (flags & FocusgroupFlags::kRowFlow) {
        // Flowing forward in a row means that we should move the focus to the
        // first cell in the next row. If there is no next row, then we should
        // move the focus to the first cell of the first row within the grid.
        Element* row = helper->RowForCell(element);
        Element* next_row = helper->NextRow(row);
        if (!next_row) {
          next_row = helper->FirstRow();
        }
        return helper->FirstCellInRow(next_row);
      }
      break;

    case FocusgroupDirection::kBackwardBlock:
      // This is only possible when on the first cell within a column.
      if (flags & FocusgroupFlags::kWrapBlock) {
        // Wrapping backward in a column means that we should move the focus to
        // the last cell in the same column.
        unsigned cell_index = helper->ColumnIndexForCell(element);
        return helper->LastCellInColumn(cell_index);
      } else if (flags & FocusgroupFlags::kColFlow) {
        // Flowing backward in a column means that we should move the focus to
        // the last cell of the previous column. If there is no previous
        // column, then we should move the focus to the last cell of the last
        // column in the grid.
        unsigned cell_index = helper->ColumnIndexForCell(element);
        if (cell_index == 0)
          cell_index = helper->ColumnCount();
        return helper->LastCellInColumn(cell_index - 1);
      }
      break;

    case FocusgroupDirection::kForwardBlock:
      // This is only possible when on the last cell within a column.
      if (flags & FocusgroupFlags::kWrapBlock) {
        // Wrapping forward in a column means that we should move the focus to
        // first cell in the same column.
        unsigned cell_index = helper->ColumnIndexForCell(element);
        return helper->FirstCellInColumn(cell_index);
      } else if (flags & FocusgroupFlags::kColFlow) {
        // Flowing forward in a column means that we should move the focus to
        // the first cell of the next column. If there is no next column, then
        // we should move the focus to the first cell of the first column within
        // the grid.
        unsigned cell_index = helper->ColumnIndexForCell(element) + 1;
        if (cell_index >= helper->ColumnCount())
          cell_index = 0;
        return helper->FirstCellInColumn(cell_index);
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  return nullptr;
}

// static
void FocusgroupController::Focus(Element* element,
                                 FocusgroupDirection direction) {
  DCHECK(element);
  element->Focus(FocusParams(SelectionBehaviorOnFocus::kReset,
                             utils::IsDirectionForward(direction)
                                 ? mojom::blink::FocusType::kForward
                                 : mojom::blink::FocusType::kBackward,
                             nullptr));
}

}  // namespace blink
