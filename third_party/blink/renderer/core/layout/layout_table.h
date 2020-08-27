/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2009, 2010 Apple Inc.
 *               All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TABLE_H_

#include <memory>
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_interface.h"
#include "third_party/blink/renderer/platform/graphics/overlay_scrollbar_clip_behavior.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class LayoutNGTableSectionInterface;
class LayoutTableCol;
class LayoutTableCaption;
class LayoutTableCell;
class LayoutTableSection;
class TableLayoutAlgorithm;

enum TableHeightChangingValue { kTableHeightNotChanging, kTableHeightChanging };

// LayoutTable is the LayoutObject associated with
// display: table or inline-table.
//
// LayoutTable is the coordinator for determining the overall table structure.
// The reason is that LayoutTableSection children have a local view over what
// their structure is but don't account for other LayoutTableSection. Thus
// LayoutTable helps keep consistency across LayoutTableSection. See e.g.
// |m_effectiveColumns| below.
//
// LayoutTable expects only 3 types of children:
// - zero or more LayoutTableCol
// - zero or more LayoutTableCaption
// - zero or more LayoutTableSection
// This is aligned with what HTML5 expects:
// https://html.spec.whatwg.org/C/#the-table-element
// with one difference: we allow more than one caption as we follow what
// CSS expects (https://bugs.webkit.org/show_bug.cgi?id=69773).
// Those expectations are enforced by LayoutTable::addChild, that wraps unknown
// children into an anonymous LayoutTableSection. This is what the "generate
// missing child wrapper" step in CSS mandates in
// http://www.w3.org/TR/CSS21/tables.html#anonymous-boxes.
//
// LayoutTable assumes a pretty strict structure that is mandated by CSS:
// (note that this structure in HTML is enforced by the HTML5 Parser).
//
//                 LayoutTable
//                 |        |
//  LayoutTableSection    LayoutTableCaption
//                 |
//      LayoutTableRow
//                 |
//     LayoutTableCell
//
// This means that we have to generate some anonymous table wrappers in order to
// satisfy the structure. See again
// http://www.w3.org/TR/CSS21/tables.html#anonymous-boxes.
// The anonymous table wrappers are inserted in LayoutTable::addChild,
// LayoutTableSection::addChild, LayoutTableRow::addChild and
// LayoutObject::addChild.
//
// Note that this yields to interesting issues in the insertion code. The DOM
// code is unaware of the anonymous LayoutObjects and thus can insert
// LayoutObjects into a different part of the layout tree. An example is:
//
// <!DOCTYPE html>
// <style>
// tablerow { display: table-row; }
// tablecell { display: table-cell; border: 5px solid purple; }
// </style>
// <tablerow id="firstRow">
//     <tablecell>Short first row.</tablecell>
// </tablerow>
// <tablecell id="cell">Long second row, shows the table structure.</tablecell>
//
// The page generates a single anonymous table (LayoutTable) and table row group
// (LayoutTableSection) to wrap the <tablerow> (#firstRow) and an anonymous
// table row (LayoutTableRow) for the second <tablecell>.
// It is possible for JavaScript to insert a new element between these 2
// <tablecell> (using Node.insertBefore), requiring us to split the anonymous
// table (or the anonymous table row group) in 2. Also note that even
// though the second <tablecell> and <tablerow> are siblings in the DOM tree,
// they are not in the layout tree.
//
//
// Note about absolute column index vs effective column index:
//
// To save memory at the expense of massive code complexity, the code tries
// to coalesce columns. This means that we try to the wider column grouping
// seen over the LayoutTableSections.
//
// Note that this is also a defensive pattern as <td colspan="6666666666">
// only allocates a single entry in this Vector. This argument is weak
// though as we cap colspans in HTMLTableCellElement.
//
// The following example would have 2 entries [ 3, 2 ] in effectiveColumns():
// <table>
//   <tr>
//     <td colspan="3"></td>
//     <td colspan="2"></td>
//   </tr>
// </table>
//
// Columns can be split if we add a row with a different colspan structure.
// See splitEffectiveColumn() and appendEffectiveColumn() for operations
// over effectiveColumns() and effectiveColumnPositions().
//
// See absoluteColumnToEffectiveColumn() for converting an absolute column
// index into an index into effectiveColumns() and effectiveColumnPositions().

class CORE_EXPORT LayoutTable final : public LayoutBlock,
                                      public LayoutNGTableInterface {
 public:
  explicit LayoutTable(Element*);
  ~LayoutTable() override;

  // Per CSS 3 writing-mode: "The first and second values of the
  // 'border-spacing' property represent spacing between columns and rows
  // respectively, not necessarily the horizontal and vertical spacing
  // respectively".
  int16_t HBorderSpacing() const final { return h_spacing_; }
  int16_t VBorderSpacing() const final { return v_spacing_; }

  bool ShouldCollapseBorders() const final {
    return StyleRef().BorderCollapse() == EBorderCollapse::kCollapse;
  }

  LayoutUnit BorderLeft() const override;
  LayoutUnit BorderRight() const override;
  LayoutUnit BorderTop() const override;
  LayoutUnit BorderBottom() const override;

  void AddChild(LayoutObject* child,
                LayoutObject* before_child = nullptr) override;

  struct ColumnStruct {
    DISALLOW_NEW();
    explicit ColumnStruct(unsigned initial_span = 1) : span(initial_span) {}

    unsigned span;
  };

  void ForceSectionsRecalc() final {
    SetNeedsSectionRecalc();
    RecalcSections();
  }

  const Vector<ColumnStruct>& EffectiveColumns() const {
    return effective_columns_;
  }
  const Vector<int>& EffectiveColumnPositions() const {
    return effective_column_positions_;
  }
  void SetEffectiveColumnPosition(unsigned index, int position) {
    // Note that if our horizontal border-spacing changed, our position will
    // change but not our column's width. In practice, horizontal border-spacing
    // won't change often.
    column_logical_width_changed_ |=
        effective_column_positions_[index] != position;
    effective_column_positions_[index] = position;
  }

  LayoutTableSection* Header() const {
    DCHECK(!NeedsSectionRecalc());
    return head_;
  }
  LayoutTableSection* Footer() const {
    DCHECK(!NeedsSectionRecalc());
    return foot_;
  }
  LayoutTableSection* FirstBody() const {
    DCHECK(!NeedsSectionRecalc());
    return first_body_;
  }

  void SetRowOffsetFromRepeatingHeader(LayoutUnit offset) {
    row_offset_from_repeating_header_ = offset;
  }
  LayoutUnit RowOffsetFromRepeatingHeader() const final {
    return row_offset_from_repeating_header_;
  }

  void SetRowOffsetFromRepeatingFooter(LayoutUnit offset) {
    row_offset_from_repeating_footer_ = offset;
  }
  LayoutUnit RowOffsetFromRepeatingFooter() const final {
    return row_offset_from_repeating_footer_;
  }

  // These functions return nullptr if the table has no sections.
  LayoutTableSection* TopSection() const;
  LayoutTableSection* BottomSection() const;

  // These functions return nullptr if the table has no non-empty sections.
  LayoutTableSection* TopNonEmptySection() const;
  LayoutTableSection* BottomNonEmptySection() const;

  unsigned LastEffectiveColumnIndex() const {
    return NumEffectiveColumns() - 1;
  }

  void SplitEffectiveColumn(unsigned index, unsigned first_span);
  void AppendEffectiveColumn(unsigned span);
  unsigned NumEffectiveColumns() const { return effective_columns_.size(); }
  unsigned SpanOfEffectiveColumn(unsigned effective_column_index) const {
    return effective_columns_[effective_column_index].span;
  }

  unsigned AbsoluteColumnToEffectiveColumn(
      unsigned absolute_column_index) const final {
    if (absolute_column_index < no_cell_colspan_at_least_)
      return absolute_column_index;

    unsigned effective_column = no_cell_colspan_at_least_;
    unsigned num_columns = NumEffectiveColumns();
    for (unsigned c = no_cell_colspan_at_least_;
         effective_column < num_columns &&
         c + effective_columns_[effective_column].span - 1 <
             absolute_column_index;
         ++effective_column)
      c += effective_columns_[effective_column].span;
    return effective_column;
  }

  unsigned EffectiveColumnToAbsoluteColumn(
      unsigned effective_column_index) const {
    if (effective_column_index < no_cell_colspan_at_least_)
      return effective_column_index;

    unsigned c = no_cell_colspan_at_least_;
    for (unsigned i = no_cell_colspan_at_least_; i < effective_column_index;
         i++)
      c += effective_columns_[i].span;
    return c;
  }

  LayoutUnit BorderSpacingInRowDirection() const {
    if (unsigned effective_column_count = NumEffectiveColumns())
      return static_cast<LayoutUnit>(effective_column_count + 1) *
             HBorderSpacing();

    return LayoutUnit();
  }

  // The collapsing border model dissallows paddings on table, which is why we
  // override those functions.
  // See http://www.w3.org/TR/CSS2/tables.html#collapsing-borders.
  LayoutUnit PaddingTop() const override;
  LayoutUnit PaddingBottom() const override;
  LayoutUnit PaddingLeft() const override;
  LayoutUnit PaddingRight() const override;

  LayoutUnit BordersPaddingAndSpacingInRowDirection() const {
    // 'border-spacing' only applies to separate borders (see 17.6.1 The
    // separated borders model).
    return BorderStart() + BorderEnd() +
           (ShouldCollapseBorders() ? LayoutUnit()
                                    : (PaddingStart() + PaddingEnd() +
                                       BorderSpacingInRowDirection()));
  }

  // Return the first column or column-group.
  LayoutTableCol* FirstColumn() const;

  struct ColAndColGroup {
    ColAndColGroup()
        : col(nullptr),
          colgroup(nullptr),
          adjoins_start_border_of_col_group(false),
          adjoins_end_border_of_col_group(false) {}
    LayoutTableCol* col;
    LayoutTableCol* colgroup;
    bool adjoins_start_border_of_col_group;
    bool adjoins_end_border_of_col_group;
    LayoutTableCol* InnermostColOrColGroup() { return col ? col : colgroup; }
  };
  ColAndColGroup ColElementAtAbsoluteColumn(
      unsigned absolute_column_index) const {
    // The common case is to not have col/colgroup elements, make that case
    // fast.
    if (!has_col_elements_)
      return ColAndColGroup();
    return SlowColElementAtAbsoluteColumn(absolute_column_index);
  }
  bool HasColElements() const final { return has_col_elements_; }

  bool NeedsSectionRecalc() const { return needs_section_recalc_; }
  void SetNeedsSectionRecalc() {
    if (DocumentBeingDestroyed())
      return;
    // For all we know, sections may have been deleted at this point. Don't
    // keep pointers dangling around.
    head_ = nullptr;
    foot_ = nullptr;
    first_body_ = nullptr;

    needs_section_recalc_ = true;
    SetNeedsLayoutAndFullPaintInvalidation(
        layout_invalidation_reason::kTableChanged);

    // Grid structure affects cell adjacence relationships which affect
    // conflict resolution of collapsed borders.
    InvalidateCollapsedBorders();
  }

  LayoutTableSection* SectionAbove(
      const LayoutTableSection*,
      SkipEmptySectionsValue = kDoNotSkipEmptySections) const;
  LayoutTableSection* SectionBelow(
      const LayoutTableSection*,
      SkipEmptySectionsValue = kDoNotSkipEmptySections) const;

  // Returns the adjacent cell to the logical top, logical bottom, logical left
  // and logical right, respectively, of the given cell, in the table's
  // direction. If there are multiple adjacent cells in the direction due to row
  // or col spans, returns the primary LayoutTableCell of the first (in DOM
  // order) adjacent TableGridCell in the direction. Returns nullptr if there
  // are no adjacent cells in the direction.
  LayoutTableCell* CellAbove(const LayoutTableCell&) const;
  LayoutTableCell* CellBelow(const LayoutTableCell&) const;
  LayoutTableCell* CellPreceding(const LayoutTableCell&) const;
  LayoutTableCell* CellFollowing(const LayoutTableCell&) const;

  void InvalidateCollapsedBorders();
  void InvalidateCollapsedBordersForAllCellsIfNeeded();

  bool HasCollapsedBorders() const final {
    DCHECK(collapsed_borders_valid_);
    return has_collapsed_borders_;
  }
  bool NeedsAdjustCollapsedBorderJoints() const {
    DCHECK(collapsed_borders_valid_);
    return needs_adjust_collapsed_border_joints_;
  }

  bool HasSections() const { return Header() || Footer() || FirstBody(); }

  void RecalcSectionsIfNeeded() const final {
    if (needs_section_recalc_)
      RecalcSections();
  }

  LayoutBox* CreateAnonymousBoxWithSameTypeAs(
      const LayoutObject* parent) const override;

  void AddCaption(const LayoutTableCaption*);
  void RemoveCaption(const LayoutTableCaption*);
  void AddColumn(const LayoutTableCol*);
  void RemoveColumn(const LayoutTableCol*);

  void PaintBoxDecorationBackground(
      const PaintInfo&,
      const PhysicalOffset& paint_offset) const final;

  void PaintMask(const PaintInfo&,
                 const PhysicalOffset& paint_offset) const final;

  void SubtractCaptionRect(PhysicalRect&) const;

  bool IsLogicalWidthAuto() const;

  const char* GetName() const override { return "LayoutTable"; }

  // Whether a table has opaque foreground depends on many factors, e.g. border
  // spacing, missing cells, etc. For simplicity, just conservatively assume
  // foreground of all tables are not opaque.
  bool ForegroundIsKnownToBeOpaqueInRect(const PhysicalRect&,
                                         unsigned) const override {
    return false;
  }

  enum WhatToMarkAllCells { kMarkDirtyOnly, kMarkDirtyAndNeedsLayout };
  void MarkAllCellsWidthsDirtyAndOrNeedsLayout(WhatToMarkAllCells);

  bool IsAbsoluteColumnCollapsed(unsigned absolute_column_index) const;

  bool IsAnyColumnEverCollapsed() const {
    return is_any_column_ever_collapsed_;
  }

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void SimplifiedNormalFlowLayout() override;

  bool RecalcLayoutOverflow() final;
  void RecalcVisualOverflow() final;

  void EnsureIsReadyForPaintInvalidation() override;
  void InvalidatePaint(const PaintInvalidatorContext&) const override;
  void ColumnStructureChanged();

  // LayoutNGTableInterface methods start.

  const LayoutNGTableInterface* ToLayoutNGTableInterface() const final {
    return this;
  }
  const LayoutObject* ToLayoutObject() const final { return this; }
  LayoutObject* ToMutableLayoutObject() final { return this; }
  bool IsFixedTableLayout() const final {
    return StyleRef().IsFixedTableLayout();
  }
  LayoutNGTableSectionInterface* FirstBodyInterface() const final;
  LayoutNGTableSectionInterface* TopSectionInterface() const final;
  LayoutNGTableSectionInterface* TopNonEmptySectionInterface() const final;
  LayoutNGTableSectionInterface* BottomSectionInterface() const final;
  LayoutNGTableSectionInterface* BottomNonEmptySectionInterface() const final;
  LayoutNGTableSectionInterface* SectionBelowInterface(
      const LayoutNGTableSectionInterface*,
      SkipEmptySectionsValue) const final;
  bool IsFirstCell(const LayoutNGTableCellInterface&) const final;

  // LayoutNGTableInterface methods end.

 private:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectTable || LayoutBlock::IsOfType(type);
  }

  void PaintObject(const PaintInfo&,
                   const PhysicalOffset& paint_offset) const override;
  void UpdateLayout() override;
  MinMaxSizes ComputeIntrinsicLogicalWidths() const override;
  MinMaxSizes PreferredLogicalWidths() const override;
  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   HitTestAction) override;

  LayoutUnit BaselinePosition(
      FontBaseline,
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const override;
  LayoutUnit FirstLineBoxBaseline() const override;
  LayoutUnit InlineBlockBaseline(LineDirectionMode) const override;

  ColAndColGroup SlowColElementAtAbsoluteColumn(unsigned col) const;

  void UpdateColumnCache() const;
  void InvalidateCachedColumns();

  void UpdateLogicalWidth() override;

  LayoutUnit ConvertStyleLogicalWidthToComputedWidth(
      const Length& style_logical_width,
      LayoutUnit available_width) const;
  LayoutUnit ConvertStyleLogicalHeightToComputedHeight(
      const Length& style_logical_height) const;

  PhysicalRect OverflowClipRect(const PhysicalOffset& location,
                                OverlayScrollbarClipBehavior =
                                    kIgnoreOverlayScrollbarSize) const override;

  void ComputeVisualOverflow(bool recompute_floats) final;

  void AddVisualOverflowFromChildren();
  void AddLayoutOverflowFromChildren() override;

  void RecalcSections() const;

  void UpdateCollapsedOuterBorders() const;

  void LayoutCaption(LayoutTableCaption&, SubtreeLayoutScope&);
  void LayoutSection(LayoutTableSection&,
                     SubtreeLayoutScope&,
                     LayoutUnit logical_left,
                     TableHeightChangingValue);

  // If any columns are collapsed, populates given vector with how much width is
  // collapsed in each column. If no columns are collapsed, given vector remains
  // empty. Logical width of table is adjusted.
  void AdjustWidthsForCollapsedColumns(Vector<int>&);

  // Return the logical height based on the height, min-height and max-height
  // properties from CSS. Will return 0 if auto.
  LayoutUnit LogicalHeightFromStyle() const;

  void DistributeExtraLogicalHeight(int extra_logical_height);

  void SetIsAnyColumnEverCollapsed() { is_any_column_ever_collapsed_ = true; }

  // TODO(layout-dev): All mutables in this class are lazily updated by
  // recalcSections() which is called by various getter methods (e.g.
  // borderBefore(), borderAfter()).
  // They allow dirty layout even after DocumentLifecycle::LayoutClean which
  // seems not proper. crbug.com/538236.

  // Holds spans (number of absolute columns) of effective columns.
  // See "absolute column index vs effective column index" in comments of
  // LayoutTable.
  mutable Vector<ColumnStruct> effective_columns_;

  // Holds the logical layout positions of effective columns, and the last item
  // (whose index is numEffectiveColumns()) holds the position of the imaginary
  // column after the last column.
  // Because of the last item, m_effectiveColumnPositions.size() is always
  // numEffectiveColumns() + 1.
  mutable Vector<int> effective_column_positions_;

  // The captions associated with this object.
  mutable Vector<LayoutTableCaption*> captions_;

  // Holds pointers to LayoutTableCol objects for <col>s and <colgroup>s under
  // this table.
  // There is no direct relationship between the size of and index into this
  // vector and those of m_effectiveColumns because they hold different things.
  mutable Vector<LayoutTableCol*> column_layout_objects_;

  mutable LayoutTableSection* head_;
  mutable LayoutTableSection* foot_;
  mutable LayoutTableSection* first_body_;

  // The layout algorithm used by this table.
  //
  // CSS 2.1 defines 2 types of table layouts toggled with 'table-layout':
  // fixed (TableLayoutAlgorithmFixed) and auto (TableLayoutAlgorithmAuto).
  // See http://www.w3.org/TR/CSS21/tables.html#width-layout.
  //
  // The layout algorithm is delegated to TableLayoutAlgorithm. This enables
  // changing 'table-layout' without having to reattach the <table>.
  //
  // As the algorithm is dependent on the style, this field is nullptr before
  // the first style is applied in styleDidChange().
  std::unique_ptr<TableLayoutAlgorithm> table_layout_;

  // Collapsed borders are SUPER EXPENSIVE to compute. The reason is that we
  // need to compare a cells border against all the adjoining cells, rows,
  // row groups, column, column groups and table. Thus we cache the values in
  // LayoutTableCells and some status here.
  bool collapsed_borders_valid_ : 1;
  bool has_collapsed_borders_ : 1;
  bool needs_adjust_collapsed_border_joints_ : 1;
  bool needs_invalidate_collapsed_borders_for_all_cells_ : 1;
  mutable bool collapsed_outer_borders_valid_ : 1;

  // Whether any column in the table section is or has been collapsed.
  bool is_any_column_ever_collapsed_ : 1;

  mutable bool has_col_elements_ : 1;
  mutable bool needs_section_recalc_ : 1;

  bool column_logical_width_changed_ : 1;
  // This flag indicates whether any columns (with or without fixed widths) have
  // been added or removed since the last layout. If they have, then the true
  // size of the cell contents needs to be determined with a full layout before
  // the layout cache is updated. The layout cache can be invalid when layout is
  // valid (e.g. if the table is being painted for the first time).
  mutable bool column_structure_changed_ : 1;
  mutable bool column_layout_objects_valid_ : 1;
  mutable unsigned no_cell_colspan_at_least_;
  unsigned CalcNoCellColspanAtLeast() const {
    for (unsigned c = 0; c < NumEffectiveColumns(); c++) {
      if (effective_columns_[c].span > 1)
        return c;
    }
    return NumEffectiveColumns();
  }

  LogicalToPhysical<unsigned> LogicalCollapsedOuterBorderToPhysical() const {
    return LogicalToPhysical<unsigned>(
        StyleRef().GetWritingMode(), StyleRef().Direction(),
        collapsed_outer_border_start_, collapsed_outer_border_end_,
        collapsed_outer_border_before_, collapsed_outer_border_after_);
  }

  int16_t h_spacing_;
  int16_t v_spacing_;

  // See UpdateCollapsedOuterBorders().
  mutable unsigned collapsed_outer_border_start_;
  mutable unsigned collapsed_outer_border_end_;
  mutable unsigned collapsed_outer_border_before_;
  mutable unsigned collapsed_outer_border_after_;
  mutable unsigned collapsed_outer_border_start_overflow_;
  mutable unsigned collapsed_outer_border_end_overflow_;

  LayoutUnit row_offset_from_repeating_header_;
  LayoutUnit row_offset_from_repeating_footer_;
  LayoutUnit old_available_logical_height_;
};

inline LayoutTableSection* LayoutTable::TopSection() const {
  DCHECK(!NeedsSectionRecalc());
  if (head_)
    return head_;
  if (first_body_)
    return first_body_;
  return foot_;
}

// To<LayoutTable>() helper.
template <>
struct DowncastTraits<LayoutTable> {
  static bool AllowFrom(const LayoutObject& object) { return object.IsTable(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TABLE_H_
