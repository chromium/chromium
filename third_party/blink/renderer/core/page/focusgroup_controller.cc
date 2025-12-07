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
  if (!focused || focused != event->RawTarget()) {
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
  if (RuntimeEnabledFeatures::FocusgroupGridEnabled(
          initial_element->GetExecutionContext())) {
    Element* grid_root = utils::FindNearestFocusgroupAncestor(
        initial_element, FocusgroupType::kGrid);
    if (grid_root && utils::IsGridFocusgroupItem(initial_element)) {
      return AdvanceInGrid(initial_element, grid_root, direction);
    }
  }

  // Only allow linear focusgroup navigation when the focus is on a focusgroup
  // item.
  Element* owner = utils::FindNearestFocusgroupAncestor(
      initial_element, FocusgroupType::kLinear);
  if (!owner) {
    return false;
  }
  if (!utils::IsFocusgroupItemWithOwner(initial_element, owner)) {
    return false;
  }

  FocusgroupData owner_data = owner->GetFocusgroupData();
  if (!utils::IsAxisSupported(owner_data.flags, direction)) {
    // Axis not supported; no navigation allowed.
    return false;
  }

  // Attempt to find next candidate in chosen direction.
  Element* candidate =
      utils::NextFocusgroupItemInDirection(owner, initial_element, direction);
  if (candidate) {
    Focus(candidate, direction);
    return true;
  }

  // No candidate found – wrap if allowed for the given direction.
  if (!utils::WrapsInDirection(owner_data.flags, direction)) {
    return false;
  }
  Element* wrap_item =
      utils::WrappedFocusgroupCandidate(owner, initial_element, direction);
  if (!wrap_item) {
    return false;
  }
  Focus(wrap_item, direction);
  return true;
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
        NOTREACHED();
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
  FocusgroupData data = helper->Root()->GetFocusgroupData();

  switch (direction) {
    case FocusgroupDirection::kBackwardInline:
      // This is only possible when on the first cell within a row.
      if (data.flags & FocusgroupFlags::kWrapInline) {
        // Wrapping backward in a row means that we should move the focus to the
        // last cell in the same row.
        Element* row = helper->RowForCell(element);
        DCHECK(row);
        return helper->LastCellInRow(row);
      } else if (data.flags & FocusgroupFlags::kRowFlow) {
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
      if (data.flags & FocusgroupFlags::kWrapInline) {
        // Wrapping forward in a row means that we should move the focus to the
        // first cell of the same row.
        Element* row = helper->RowForCell(element);
        DCHECK(row);
        return helper->FirstCellInRow(row);
      } else if (data.flags & FocusgroupFlags::kRowFlow) {
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
      if (data.flags & FocusgroupFlags::kWrapBlock) {
        // Wrapping backward in a column means that we should move the focus to
        // the last cell in the same column.
        unsigned cell_index = helper->ColumnIndexForCell(element);
        return helper->LastCellInColumn(cell_index);
      } else if (data.flags & FocusgroupFlags::kColFlow) {
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
      if (data.flags & FocusgroupFlags::kWrapBlock) {
        // Wrapping forward in a column means that we should move the focus to
        // first cell in the same column.
        unsigned cell_index = helper->ColumnIndexForCell(element);
        return helper->FirstCellInColumn(cell_index);
      } else if (data.flags & FocusgroupFlags::kColFlow) {
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
      NOTREACHED();
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
