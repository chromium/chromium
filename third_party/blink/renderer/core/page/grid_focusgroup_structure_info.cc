// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/grid_focusgroup_structure_info.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/html_table_row_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_section.h"

namespace blink {

AutomaticGridFocusgroupStructureInfo::AutomaticGridFocusgroupStructureInfo(
    LayoutObject* root)
    : table_(root) {
  DCHECK(Table());
  DCHECK(Flags() & FocusgroupFlags::kGrid);
}

void AutomaticGridFocusgroupStructureInfo::Trace(Visitor* visitor) const {
  visitor->Trace(table_);
}

const LayoutTable* AutomaticGridFocusgroupStructureInfo::Table() {
  CHECK(table_->IsTable());
  return To<LayoutTable>(table_.Get());
}

Element* AutomaticGridFocusgroupStructureInfo::Root() {
  return DynamicTo<Element>(table_->GetNode());
}

FocusgroupFlags AutomaticGridFocusgroupStructureInfo::Flags() {
  return Root()->GetFocusgroupFlags();
}

unsigned AutomaticGridFocusgroupStructureInfo::ColumnCount() {
  // The actual column count of a table is not stored on an HTMLTableElement,
  // but it is on its associated layout object.
  return Table()->EffectiveColumnCount();
}

Element* AutomaticGridFocusgroupStructureInfo::PreviousCellInRow(
    const Element* cell_element) {
  DCHECK(cell_element);
  auto* cell = DynamicTo<LayoutTableCell>(cell_element->GetLayoutObject());
  if (!cell)
    return nullptr;

  auto* row = cell->Row();
  if (!row)
    return nullptr;

  Element* row_element = DynamicTo<Element>(row->GetNode());
  if (!row_element)
    return nullptr;

  return CellAtIndexInRow(cell->AbsoluteColumnIndex() - 1, row_element,
                          NoCellFoundAtIndexBehavior::kFindPreviousCellInRow);
}

Element* AutomaticGridFocusgroupStructureInfo::NextCellInRow(
    const Element* cell_element) {
  DCHECK(cell_element);
  auto* cell = DynamicTo<LayoutTableCell>(cell_element->GetLayoutObject());
  if (!cell)
    return nullptr;

  unsigned col_span = cell->ColSpan();
  if (col_span == 0) {
    // A colspan value of 0 means that all cells in the row are part of the same
    // cell. In this case, there can't be a next cell.
    return nullptr;
  }

  auto* row = cell->Row();
  if (!row)
    return nullptr;

  Element* row_element = DynamicTo<Element>(row->GetNode());
  if (!row_element)
    return nullptr;

  return CellAtIndexInRow(cell->AbsoluteColumnIndex() + col_span, row_element,
                          NoCellFoundAtIndexBehavior::kFindNextCellInRow);
}

Element* AutomaticGridFocusgroupStructureInfo::FirstCellInRow(Element* row) {
  DCHECK(row);
  if (!IsA<LayoutTableRow>(row->GetLayoutObject())) {
    return nullptr;
  }

  return CellAtIndexInRow(0, row,
                          NoCellFoundAtIndexBehavior::kFindNextCellInRow);
}

Element* AutomaticGridFocusgroupStructureInfo::LastCellInRow(Element* row) {
  DCHECK(row);
  if (!IsA<LayoutTableRow>(row->GetLayoutObject())) {
    return nullptr;
  }

  return CellAtIndexInRow(ColumnCount() - 1, row,
                          NoCellFoundAtIndexBehavior::kFindPreviousCellInRow);
}

unsigned AutomaticGridFocusgroupStructureInfo::ColumnIndexForCell(
    const Element* cell_element) {
  DCHECK(cell_element);

  // The actual column index takes into account the previous rowspan/colspan
  // values that might affect this cell's col index.
  auto* cell = DynamicTo<LayoutTableCell>(cell_element->GetLayoutObject());
  if (!cell)
    return 0;

  return cell->AbsoluteColumnIndex();
}

Element* AutomaticGridFocusgroupStructureInfo::PreviousCellInColumn(
    const Element* cell_element) {
  DCHECK(cell_element);
  auto* cell = DynamicTo<LayoutTableCell>(cell_element->GetLayoutObject());
  if (!cell) {
    return nullptr;
  }

  auto* row = cell->Row();
  if (!row)
    return nullptr;

  auto* previous_row = PreviousRow(row);
  if (!previous_row)
    return nullptr;

  auto* previous_row_element = DynamicTo<Element>(previous_row->GetNode());
  if (!previous_row_element)
    return nullptr;

  return CellAtIndexInRow(
      cell->AbsoluteColumnIndex(), previous_row_element,
      NoCellFoundAtIndexBehavior::kFindPreviousCellInColumn);
}

Element* AutomaticGridFocusgroupStructureInfo::NextCellInColumn(
    const Element* cell_element) {
  DCHECK(cell_element);

  auto* cell = DynamicTo<LayoutTableCell>(cell_element->GetLayoutObject());
  if (!cell)
    return nullptr;

  auto* row = cell->Row();
  if (!row)
    return nullptr;

  auto* next_row = row;
  const unsigned row_span = cell->ResolvedRowSpan();
  for (unsigned i = 0; i < row_span; i++) {
    next_row = NextRow(next_row);
    if (!next_row)
      return nullptr;
  }

  auto* next_row_element = DynamicTo<Element>(next_row->GetNode());
  if (!next_row_element)
    return nullptr;

  return CellAtIndexInRow(cell->AbsoluteColumnIndex(), next_row_element,
                          NoCellFoundAtIndexBehavior::kFindNextCellInColumn);
}

Element* AutomaticGridFocusgroupStructureInfo::FirstCellInColumn(
    unsigned index) {
  if (index >= ColumnCount())
    return nullptr;

  return CellAtIndexInRow(index, FirstRow(),
                          NoCellFoundAtIndexBehavior::kFindNextCellInColumn);
}

Element* AutomaticGridFocusgroupStructureInfo::LastCellInColumn(
    unsigned index) {
  if (index >= ColumnCount())
    return nullptr;

  return CellAtIndexInRow(
      index, LastRow(), NoCellFoundAtIndexBehavior::kFindPreviousCellInColumn);
}

Element* AutomaticGridFocusgroupStructureInfo::PreviousRow(
    Element* row_element) {
  DCHECK(row_element);
  auto* row = DynamicTo<LayoutTableRow>(row_element->GetLayoutObject());
  if (!row)
    return nullptr;

  auto* previous_row = PreviousRow(row);
  if (!previous_row)
    return nullptr;

  return DynamicTo<Element>(previous_row->GetNode());
}

Element* AutomaticGridFocusgroupStructureInfo::NextRow(Element* row_element) {
  DCHECK(row_element);
  auto* row = DynamicTo<LayoutTableRow>(row_element->GetLayoutObject());
  if (!row)
    return nullptr;

  auto* next_row = NextRow(row);
  if (!next_row)
    return nullptr;

  return DynamicTo<Element>(next_row->GetNode());
}

Element* AutomaticGridFocusgroupStructureInfo::FirstRow() {
  auto* first_section = Table()->FirstSection();
  auto* first_row = first_section->FirstRow();
  while (first_row) {
    // Layout rows can be empty (i.e., have no cells), so make sure that we
    // return the first row that has at least one cell.
    if (first_row->FirstCell()) {
      return DynamicTo<Element>(first_row->GetNode());
    }
    first_row = first_row->NextRow();
  }
  return nullptr;
}

Element* AutomaticGridFocusgroupStructureInfo::LastRow() {
  auto* last_section = Table()->LastSection();
  auto* last_row = last_section->LastRow();
  while (last_row) {
    // See comment in `PreviousRow()` to understand why we need to ensure this
    // functions returns a row that has cells.
    if (last_row->FirstCell()) {
      return DynamicTo<Element>(last_row->GetNode());
    }

    last_row = last_row->PreviousRow();
  }
  return nullptr;
}

Element* AutomaticGridFocusgroupStructureInfo::RowForCell(
    Element* cell_element) {
  auto* cell = DynamicTo<LayoutTableCell>(cell_element->GetLayoutObject());
  if (!cell) {
    return nullptr;
  }

  auto* row = cell->Row();
  if (!row)
    return nullptr;

  return DynamicTo<Element>(row->GetNode());
}

Element* AutomaticGridFocusgroupStructureInfo::CellAtIndexInRow(
    unsigned index,
    Element* row_element,
    NoCellFoundAtIndexBehavior behavior) {
  auto* row = DynamicTo<LayoutTableRow>(row_element->GetLayoutObject());
  if (!row) {
    return nullptr;
  }

  // This can happen when |row|'s nth previous sibling row has a rowspan value
  // of n + 1 and a colspan value equal to the table's column count. In that
  // case, |row| won't have any cell.
  if (!row->FirstCell()) {
    return nullptr;
  }

  unsigned total_col_count = ColumnCount();
  if (index >= total_col_count)
    return nullptr;

  auto* cell = TableCellAtIndexInRowRecursive(index, row);
  while (!cell) {
    switch (behavior) {
      case NoCellFoundAtIndexBehavior::kReturn:
        return nullptr;
      case NoCellFoundAtIndexBehavior::kFindPreviousCellInRow:
        if (index == 0) {
          // This shouldn't happen, since the row passed by parameter is
          // expected to always have at least one cell at this point.
          NOTREACHED_IN_MIGRATION();
          return nullptr;
        }
        cell = TableCellAtIndexInRowRecursive(--index, row);
        break;
      case NoCellFoundAtIndexBehavior::kFindNextCellInRow:
        if (index >= total_col_count)
          return nullptr;
        cell = TableCellAtIndexInRowRecursive(++index, row);
        break;
      case NoCellFoundAtIndexBehavior::kFindPreviousCellInColumn:
        row = PreviousRow(row);
        if (!row)
          return nullptr;
        cell = TableCellAtIndexInRowRecursive(index, row);
        break;
      case NoCellFoundAtIndexBehavior::kFindNextCellInColumn:
        row = NextRow(row);
        if (!row)
          return nullptr;
        cell = TableCellAtIndexInRowRecursive(index, row);
        break;
    }
  }

  if (!cell)
    return nullptr;

  return DynamicTo<Element>(cell->GetNode());
}

LayoutTableRow* AutomaticGridFocusgroupStructureInfo::PreviousRow(
    LayoutTableRow* current_row) {
  auto* current_section = current_row->Section();
  LayoutTableRow* previous_row = current_row->PreviousRow();

  // Here, it's possible the previous row has no cells at all if the nth
  // previous row has a rowspan attribute of value n + 1 and a colspan value
  // equal to the table's column count. Return the first previous row that
  // actually isn't just a continuation of another one.
  //
  // Also, it's possible that the previous row is actually located in the
  // previous section. When we can't find a previous row, get the last row from
  // the previous section.
  while (!previous_row || !previous_row->FirstCell()) {
    if (previous_row && previous_row->FirstCell()) {
      previous_row = previous_row->PreviousRow();
      continue;
    }

    auto* previous_section = Table()->PreviousSection(current_section);
    if (!previous_section)
      return nullptr;

    current_section = previous_section;
    previous_row = previous_section->LastRow();
  }

  return previous_row;
}

LayoutTableRow* AutomaticGridFocusgroupStructureInfo::NextRow(
    LayoutTableRow* current_row) {
  auto* current_section = current_row->Section();
  LayoutTableRow* next_row = current_row->NextRow();

  // Here, it's possible the next row has no cells at all if the current row (or
  // a previous sibling) has a rowspan attribute that encapsulates the next row
  // and a colspan value equal to the table's column count. Return the first
  // next row that actually isn't just a continuation of a previous one.
  //
  // Also, it's possible that the next row is actually located in the
  // next section. When we can't find a previous row, get the last row from
  // the previous section.
  while (!next_row || !next_row->FirstCell()) {
    if (next_row && next_row->FirstCell()) {
      next_row = next_row->NextRow();
      continue;
    }

    auto* next_section = Table()->NextSection(current_section);
    if (!next_section)
      return nullptr;

    current_section = next_section;
    next_row = next_section->FirstRow();
  }

  return next_row;
}

LayoutTableCell*
AutomaticGridFocusgroupStructureInfo::TableCellAtIndexInRowRecursive(
    unsigned index,
    LayoutTableRow* row,
    std::optional<unsigned> expected_rowspan) {
  if (!row)
    return nullptr;

  // 1. Define a starting point for the search. Start from the end.
  auto* cell = row->LastCell();
  if (auto* table_row = DynamicTo<HTMLTableRowElement>(row->GetNode())) {
    // This is a shortcut that allows us to get the cell at |index| in constant
    // time. This shortcut is only possible with HTML tables. If the table
    // contains rowspans/colspans that affect this cell, it might actually not
    // be the right one and require some adjustments. Anyway, when possible,
    // it's better performance-wise to start near a cell than to always start
    // the search on the first/last cell of a row.
    auto* table_cell =
        DynamicTo<HTMLTableCellElement>(table_row->cells()->item(index));
    if (table_cell) {
      cell = To<LayoutTableCell>(table_cell->GetLayoutObject());
    }
  }

  // 2. Get the cell's actual index. Its index might not be equal to |index|,
  // since a rowspan and/or colspan value set on a previous cell would have
  // affected the actual index.
  //
  // Example:
  // <tr>
  //   <td id=cell1 colspan=2></td>
  //   <td id=cell2></td>
  // </tr>
  //
  // |cell1|'s absolute column index would be 0, while |cell2|'s would be 2.
  // However, |cell2| would be found at index 1 of the row cells.
  unsigned actual_index = cell->AbsoluteColumnIndex();

  // 3. Find the cell at |index| by making the necessary adjustments to the
  // current |cell|.
  while (actual_index != index) {
    if (actual_index > index) {
      cell = cell->PreviousCell();
      if (cell) {
        actual_index = cell->AbsoluteColumnIndex();
        continue;
      }
    } else {
      unsigned col_span = cell->ColSpan();
      // When colspan equals 0 (meaning that the cell spans all columns), we
      // want to break since the cell most definitely contains the |index|.
      if (col_span == 0 || actual_index + col_span > index) {
        // This is only the case when the we are on a cell that spans multiple
        // columns.
        break;
      }
    }

    // We only reach this point when either:
    //    A. the cell at this |index| starts in another row because of a
    //       rowspan.
    //    B. there is no cell at this |index|. Although this is rare, it is
    //       possible to achieve when a row contains fewer columns than
    //       others.
    //
    // Here, we take care of scenario A. by getting the cell that spans multiple
    // rows by looking located in a previous row. This approach is recursive.
    unsigned rowspan_to_expect = expected_rowspan ? *expected_rowspan + 1 : 2;
    cell = TableCellAtIndexInRowRecursive(index, PreviousRow(row),
                                          rowspan_to_expect);

    if (cell)
      actual_index = cell->AbsoluteColumnIndex();

    // At this point, we either found a cell that spans multiple rows and
    // corresponds to the one we were looking for or we are in scenario B. Let
    // the caller deal with what to do next in this case.
    break;
  }

  if (!cell)
    return nullptr;

  // 4. Return early if the cell we found in in a previous doesn't span to
  // the row we started the search on. We use the |expected_rowspan| parameter
  // to determine if the cell we found can reach the row we were at.
  if (actual_index == index && expected_rowspan) {
    unsigned row_span = cell->ResolvedRowSpan();
    if (row_span == 0 || *expected_rowspan > row_span) {
      // This is to prevent going to a previous row that exist at index but
      // doesn't have rowspan. A rowspan value of 0 means "all rows".
      return nullptr;
    }
  }

  // 5. We reached a result. If |cell| is null, then no cell was found at
  // |index| in this specific row.
  return cell;
}

}  // namespace blink
