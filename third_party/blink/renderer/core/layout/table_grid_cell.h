// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TABLE_GRID_CELL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TABLE_GRID_CELL_H_

#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class LayoutTableCell;

// TableGridCell represents a single unit in the table grid.
// - Without rowspan and colspan, TableGridCells and LayoutTableCells have 1:1
//   relationship.
// - A LayoutTableCell spanning multiple rows or effective columns can cover
//   multiple TableGridCells.
// - Multiple LayoutTableCells can span into the same TableGridCell, e.g.
//    <tr><td>A</td><td rowspan="2">B</td></tr>
//    <tr><td colspan="2">C></td></tr>
//   both LayoutTableCell B and C cover the TableGridCell at (1,1).

class TableGridCell {
  DISALLOW_NEW();

 public:
  // We can't inline the constructor and destructor because cells_ needs full
  // definition of LayoutTableCell, and we can't include LayoutTableCell.h
  // from this file due to circular includes.
  TableGridCell();
  ~TableGridCell();

  // This is the LayoutTableCell covering this TableGridCell that is on top of
  // the others (aka the last LayoutTableCell in DOM order for this
  // TableGridCell).
  //
  // The concept of a primary cell is dubious at most as it doesn't correspond
  // to a DOM or rendering concept. Also callers should be careful about
  // assumptions about it. For example, even though the primary cell is visibly
  // the top most, it is not guaranteed to be the only one visible for this
  // unit due to different visual overflow rectangles.
  LayoutTableCell* PrimaryCell() {
    return HasCells() ? cells_.back() : nullptr;
  }
  const LayoutTableCell* PrimaryCell() const {
    return const_cast<TableGridCell*>(this)->PrimaryCell();
  }

  bool HasCells() const { return cells_.size() > 0; }

  Vector<LayoutTableCell*, 1>& Cells() { return cells_; }
  const Vector<LayoutTableCell*, 1>& Cells() const { return cells_; }

  bool InColSpan() const { return in_col_span_; }
  void SetInColSpan(bool in_col_span) { in_col_span_ = in_col_span; }

 private:
  // All LayoutTableCells covering this TableGridCell.
  // Due to colspan / rowpsan, it is possible to have overlapping cells
  // (see class comment about an example).
  // This Vector is sorted in DOM order.
  Vector<LayoutTableCell*, 1> cells_;

  // True for columns after the first in a colspan.
  bool in_col_span_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TABLE_GRID_CELL_H_
