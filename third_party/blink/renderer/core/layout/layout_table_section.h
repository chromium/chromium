/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2009, 2013 Apple Inc. All rights
 * reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TABLE_SECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TABLE_SECTION_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_box_component.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_section_interface.h"
#include "third_party/blink/renderer/core/layout/table_grid_cell.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Helper class for paintObject.
class CellSpan {
  STACK_ALLOCATED();

 public:
  CellSpan() : start_(0), end_(0) {}
  CellSpan(unsigned start, unsigned end) : start_(start), end_(end) {}

  unsigned Start() const { return start_; }
  unsigned End() const { return end_; }

  void DecreaseStart() { --start_; }
  void IncreaseEnd() { ++end_; }

  void EnsureConsistency(const unsigned);

 private:
  unsigned start_;
  unsigned end_;
};

inline bool operator==(const CellSpan& s1, const CellSpan& s2) {
  return s1.Start() == s2.Start() && s1.End() == s2.End();
}
inline bool operator!=(const CellSpan& s1, const CellSpan& s2) {
  return !(s1 == s2);
}

class LayoutTableCell;
class LayoutTableRow;

// LayoutTableSection is used to represent table row group (display:
// table-row-group), header group (display: table-header-group) and footer group
// (display: table-footer-group).
//
// The object holds the internal representation of the rows (m_grid). See
// recalcCells() below for some extra explanation.
//
// A lot of the complexity in this class is related to handling rowspan, colspan
// or just non-regular tables.
//
// Example of rowspan / colspan leading to overlapping cells (rowspan and
// colspan are overlapping):
// <table>
//   <tr>
//       <td>first row</td>
//       <td rowspan="2">rowspan</td>
//     </tr>
//    <tr>
//        <td colspan="2">colspan</td>
//     </tr>
// </table>
//
// Example of non-regular table (missing one cell in the first row):
// <!DOCTYPE html>
// <table>
//   <tr><td>First row only child.</td></tr>
//   <tr>
//     <td>Second row first child</td>
//     <td>Second row second child</td>
//   </tr>
// </table>
//
// LayoutTableSection is responsible for laying out LayoutTableRows and
// LayoutTableCells (see layoutRows()). However it is not their containing
// block, the enclosing LayoutTable (this object's parent()) is. This is why
// this class inherits from LayoutTableBoxComponent and not LayoutBlock.
class CORE_EXPORT LayoutTableSection final
    : public LayoutTableBoxComponent,
      public LayoutNGTableSectionInterface {
 public:
  explicit LayoutTableSection(Element*);
  ~LayoutTableSection() override;

  LayoutTableRow* FirstRow() const;
  LayoutTableRow* LastRow() const;

  void AddChild(LayoutObject* child,
                LayoutObject* before_child = nullptr) override;

  LayoutUnit FirstLineBoxBaseline() const override;

  void AddCell(LayoutTableCell*, LayoutTableRow*);

  int16_t VBorderSpacingBeforeFirstRow() const;
  int CalcRowLogicalHeight();
  void LayoutRows();
  bool RecalcLayoutOverflow() final;
  void RecalcVisualOverflow() final;

  void MarkAllCellsWidthsDirtyAndOrNeedsLayout(LayoutTable::WhatToMarkAllCells);

  LayoutTable* Table() const final { return To<LayoutTable>(Parent()); }

  typedef Vector<LayoutTableCell*, 2> SpanningLayoutTableCells;

  struct SpanningRowsHeight {
    STACK_ALLOCATED();

   public:
    SpanningRowsHeight()
        : total_rows_height(0),
          spanning_cell_height_ignoring_border_spacing(0),
          is_any_row_with_only_spanning_cells(false) {}
    SpanningRowsHeight(const SpanningRowsHeight&) = delete;
    SpanningRowsHeight& operator=(const SpanningRowsHeight&) = delete;
    Vector<int> row_height;
    int total_rows_height;
    int spanning_cell_height_ignoring_border_spacing;
    bool is_any_row_with_only_spanning_cells;
  };

  TableGridCell& GridCellAt(unsigned row, unsigned effective_column) {
    SECURITY_DCHECK(!needs_cell_recalc_);
    return grid_[row].grid_cells[effective_column];
  }
  const TableGridCell& GridCellAt(unsigned row,
                                  unsigned effective_column) const {
    SECURITY_DCHECK(!needs_cell_recalc_);
    return grid_[row].grid_cells[effective_column];
  }
  LayoutTableCell* PrimaryCellAt(unsigned row, unsigned effective_column) {
    SECURITY_DCHECK(!needs_cell_recalc_);
    auto& grid_cells = grid_[row].grid_cells;
    if (effective_column >= grid_cells.size())
      return nullptr;
    return grid_cells[effective_column].PrimaryCell();
  }
  const LayoutTableCell* PrimaryCellAt(unsigned row,
                                       unsigned effective_column) const {
    return const_cast<LayoutTableSection*>(this)->PrimaryCellAt(
        row, effective_column);
  }

  // Returns the primary cell at (row, effectiveColumn) if the cell exists and
  // originates from (instead of spanning into) the grid slot, or nullptr.
  LayoutTableCell* OriginatingCellAt(unsigned row, unsigned effective_column);
  const LayoutTableCell* OriginatingCellAt(unsigned row,
                                           unsigned effective_column) const {
    return const_cast<LayoutTableSection*>(this)->OriginatingCellAt(
        row, effective_column);
  }

  unsigned NumCols(unsigned row) const final {
    DCHECK(!NeedsCellRecalc());
    return grid_[row].grid_cells.size();
  }

  // Returns null for cells with a rowspan that exceed the last row. Possibly
  // others.
  LayoutTableRow* RowLayoutObjectAt(unsigned row) {
    SECURITY_DCHECK(!needs_cell_recalc_);
    return grid_[row].row;
  }
  const LayoutTableRow* RowLayoutObjectAt(unsigned row) const {
    SECURITY_DCHECK(!needs_cell_recalc_);
    return grid_[row].row;
  }

  void AppendEffectiveColumn(unsigned pos);
  void SplitEffectiveColumn(unsigned pos, unsigned first);

  unsigned NumRows() const final {
    DCHECK(!NeedsCellRecalc());
    return grid_.size();
  }
  unsigned NumEffectiveColumns() const final;

  // recalcCells() is used when we are not sure about the section's structure
  // and want to do an expensive (but safe) reconstruction of m_grid from
  // scratch.
  // An example of this is inserting a new cell in the middle of an existing
  // row or removing a row.
  //
  // Accessing m_grid when m_needsCellRecalc is set is UNSAFE as pointers can
  // be left dangling. Thus care should be taken in the code to check
  // m_needsCellRecalc before accessing m_grid.
  void RecalcCells();
  void RecalcCellsIfNeeded() {
    if (needs_cell_recalc_)
      RecalcCells();
  }

  bool NeedsCellRecalc() const { return needs_cell_recalc_; }
  void SetNeedsCellRecalc() final;

  LayoutUnit RowBaseline(unsigned row) { return grid_[row].baseline; }

  void RowLogicalHeightChanged(LayoutTableRow*);

  // distributeExtraLogicalHeightToRows methods return the *consumed* extra
  // logical height.
  // FIXME: We may want to introduce a structure holding the in-flux layout
  // information.
  int DistributeExtraLogicalHeightToRows(int extra_logical_height);

  LayoutBox* CreateAnonymousBoxWithSameTypeAs(
      const LayoutObject* parent) const override;

  void Paint(const PaintInfo&) const override;

  // Flip the rect so it aligns with the coordinates used by the rowPos and
  // columnPos vectors.
  LayoutRect LogicalRectForWritingModeAndDirection(const PhysicalRect&) const;

  // Sets |rows| and |columns| to cover all cells needing repaint in
  // |damage_rect|.
  void DirtiedRowsAndEffectiveColumns(const LayoutRect& damage_rect,
                                      CellSpan& rows,
                                      CellSpan& columns) const;

  const HashSet<const LayoutTableCell*>& VisuallyOverflowingCells() const {
    return visually_overflowing_cells_;
  }
  bool HasVisuallyOverflowingCell() const {
    return visually_overflowing_cells_.size() || force_full_paint_;
  }
  bool HasMultipleCellLevels() const { return has_multiple_cell_levels_; }

  const char* GetName() const override { return "LayoutTableSection"; }

  // Whether a section has opaque background depends on many factors, e.g.
  // border spacing, border collapsing, missing cells, etc. For simplicity,
  // just conservatively assume all table sections are not opaque.
  bool ForegroundIsKnownToBeOpaqueInRect(const PhysicalRect&,
                                         unsigned) const override {
    return false;
  }
  bool BackgroundIsKnownToBeOpaqueInRect(const PhysicalRect&) const override {
    return false;
  }

  int PaginationStrutForRow(LayoutTableRow*, LayoutUnit logical_offset) const;

  bool MapToVisualRectInAncestorSpaceInternal(
      const LayoutBoxModelObject* ancestor,
      TransformState&,
      VisualRectFlags = kDefaultVisualRectFlags) const override;

  bool IsRepeatingHeaderGroup() const final {
    return is_repeating_header_group_;
  }
  bool IsRepeatingFooterGroup() const final {
    return is_repeating_footer_group_;
  }

  void UpdateLayout() override;

  CellSpan FullSectionRowSpan() const { return CellSpan(0, grid_.size()); }
  CellSpan FullTableEffectiveColumnSpan() const {
    return CellSpan(0, Table()->NumEffectiveColumns());
  }

  void DetermineIfHeaderGroupShouldRepeat() {
    is_repeating_header_group_ = HeaderGroupShouldRepeat();
  }

  // Check whether row or row group has visibility:collapse.
  bool RowHasVisibilityCollapse(unsigned row) const;

  void DetermineIfFooterGroupShouldRepeat() {
    is_repeating_footer_group_ = FooterGroupShouldRepeat();
  }

  // Update widths of cells affected by collapsed columns and sets whether cells
  // are spanning any collapsed columns.
  void UpdateLogicalWidthForCollapsedCells(
      const Vector<int>& col_collapsed_width);

  void ComputeLayoutOverflowFromDescendants();

  // LayoutNGTableSectionInterface methods start.

  const LayoutNGTableSectionInterface* ToLayoutNGTableSectionInterface()
      const final {
    return this;
  }
  const LayoutTableSection* ToLayoutTableSection() const final { return this; }
  const LayoutObject* ToLayoutObject() const final { return this; }
  LayoutObject* ToMutableLayoutObject() final { return this; }

  LayoutNGTableInterface* TableInterface() const final { return Table(); }
  LayoutNGTableRowInterface* FirstRowInterface() const final;
  LayoutNGTableRowInterface* LastRowInterface() const final;
  const LayoutNGTableCellInterface* PrimaryCellInterfaceAt(
      unsigned row,
      unsigned effective_column) const final;

  // LayoutNGTableSectionInterface methods end.
 protected:
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   HitTestAction) override;

 private:
  MinMaxSizes ComputeIntrinsicLogicalWidths() const final {
    NOTREACHED();
    return MinMaxSizes();
  }

  void ComputeVisualOverflowFromDescendants();

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectTableSection || LayoutBox::IsOfType(type);
  }

  void WillBeRemovedFromTree() override;

  int BorderSpacingForRow(unsigned row) const {
    return grid_[row].row ? Table()->VBorderSpacing() : 0;
  }

  void EnsureRows(unsigned num_rows) {
    if (num_rows > grid_.size())
      grid_.Grow(num_rows);
  }

  void EnsureCols(unsigned row_index, unsigned num_cols) {
    if (num_cols > NumCols(row_index))
      grid_[row_index].grid_cells.Grow(num_cols);
  }

  bool RowHasOnlySpanningCells(unsigned);
  unsigned CalcRowHeightHavingOnlySpanningCells(unsigned,
                                                int&,
                                                unsigned,
                                                unsigned&,
                                                Vector<int>&);
  void UpdateRowsHeightHavingOnlySpanningCells(LayoutTableCell*,
                                               struct SpanningRowsHeight&,
                                               unsigned&,
                                               Vector<int>&);

  void PopulateSpanningRowsHeightFromCell(LayoutTableCell*,
                                          struct SpanningRowsHeight&);
  void DistributeExtraRowSpanHeightToPercentRows(LayoutTableCell*,
                                                 float,
                                                 int&,
                                                 Vector<int>&);
  void DistributeWholeExtraRowSpanHeightToPercentRows(LayoutTableCell*,
                                                      float,
                                                      int&,
                                                      Vector<int>&);
  void DistributeExtraRowSpanHeightToAutoRows(LayoutTableCell*,
                                              int,
                                              int&,
                                              Vector<int>&);
  void DistributeExtraRowSpanHeightToRemainingRows(LayoutTableCell*,
                                                   int,
                                                   int&,
                                                   Vector<int>&);
  void DistributeRowSpanHeightToRows(SpanningLayoutTableCells& row_span_cells);

  void DistributeExtraLogicalHeightToPercentRows(int& extra_logical_height,
                                                 int total_percent);
  void DistributeExtraLogicalHeightToAutoRows(int& extra_logical_height,
                                              unsigned auto_rows_count);
  void DistributeRemainingExtraLogicalHeight(int& extra_logical_height);

  void UpdateBaselineForCell(LayoutTableCell*,
                             unsigned row,
                             LayoutUnit& baseline_descent);

  // These two functions take a rectangle as input that has been flipped by
  // logicalRectForWritingModeAndDirection.
  // The returned span of rows or columns is end-exclusive, and empty if
  // start==end.
  CellSpan SpannedRows(const LayoutRect& flipped_rect) const;
  CellSpan SpannedEffectiveColumns(const LayoutRect& flipped_rect) const;

  void SetLogicalPositionForCell(LayoutTableCell*,
                                 unsigned effective_column) const;

  void RelayoutCellIfFlexed(LayoutTableCell&, int row_index, int row_height);

  int LogicalHeightForRow(const LayoutTableRow&) const;

  // Honor breaking restrictions inside the table row, and adjust position and
  // size accordingly.
  void AdjustRowForPagination(LayoutTableRow&, SubtreeLayoutScope&);

  // The offset at which the first row in the section will get positioned to
  // avoid any repeating headers in its table or ancestor tables.
  int OffsetForRepeatedHeader() const;

  bool HeaderGroupShouldRepeat() const {
    return Table()->Header() == this && GroupShouldRepeat();
  }

  bool FooterGroupShouldRepeat() const {
    return Table()->Footer() == this && GroupShouldRepeat();
  }

  bool GroupShouldRepeat() const;

  struct TableGridRow {
    DISALLOW_NEW();

   public:
    inline void SetRowLogicalHeightToRowStyleLogicalHeight();
    inline void UpdateLogicalHeightForCell(const LayoutTableCell*);

    // The index is effective column index.
    Vector<TableGridCell> grid_cells;
    LayoutTableRow* row = nullptr;
    LayoutUnit baseline = LayoutUnit(-1);
    Length logical_height;
  };

  // The representation of the rows and their grid cells.
  Vector<TableGridRow> grid_;

  // The logical offset of each row from the top of the section.
  //
  // Note that this Vector has one more entry than the number of rows so that
  // we can keep track of the final size of the section. That is,
  // m_rowPos[m_grid.size()] is a valid entry.
  //
  // To know a row's height at |rowIndex|, use the formula:
  // m_rowPos[rowIndex + 1] - m_rowPos[rowIndex]
  Vector<int> row_pos_;

  // The amount of height collapsed in each row.
  //
  // This is used to adjust the padding of row-spanning cells. The padding
  // should stay the same as if the row were not collapsed.
  Vector<int> row_collapsed_height_;

  // Whether any row in the table section is or has been collapsed.
  bool is_any_row_collapsed_;

  // The current insertion position in the grid.
  // The position is used when inserting a new cell into the section to
  // know where it should be inserted and expand our internal structure.
  //
  // The reason for them is that we process cells as we discover them
  // during parsing or during recalcCells (ie in DOM order). This means
  // that we can discover changes in the structure later (e.g. due to
  // colspans, extra cells, ...).
  //
  // Do not use outside of recalcCells and addChild.
  unsigned c_col_;
  unsigned c_row_;

  bool needs_cell_recalc_;

  // This HashSet holds the overflowing cells for the partial paint path. If we
  // have too many overflowing cells, it will be empty and force_full_paint_
  // will be set to save memory. See ComputeVisualOverflowFromDescendants().
  HashSet<const LayoutTableCell*> visually_overflowing_cells_;
  bool force_full_paint_;

  // This boolean tracks if we have cells overlapping due to rowspan / colspan
  // (see class comment above about when it could appear).
  //
  // The use is to disable a painting optimization where we just paint the
  // invalidated cells.
  bool has_multiple_cell_levels_;

  // Whether any cell spans multiple rows or cols.
  bool has_spanning_cells_;

  // Header group should be painted on every page.
  bool is_repeating_header_group_;

  // Footer group should be painted on every page.
  bool is_repeating_footer_group_;
};

// To<LayoutTableSection>() helper.
template <>
struct DowncastTraits<LayoutTableSection> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsTableSection();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TABLE_SECTION_H_
