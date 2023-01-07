/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2009, 2013 Apple Inc.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TABLE_CELL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TABLE_CELL_H_

#include <memory>
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/collapsed_border_value.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/layout_table_section.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell_interface.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/text/writing_mode_utils.h"

namespace blink {

#define BITS_OF_ABSOLUTE_COLUMN_INDEX 25
static const unsigned kUnsetColumnIndex =
    (1u << BITS_OF_ABSOLUTE_COLUMN_INDEX) - 1;
static const unsigned kMaxColumnIndex = kUnsetColumnIndex - 1;

class SubtreeLayoutScope;
class LayoutNGTableInterface;

// LayoutTableCell is used to represent a table cell (display: table-cell).
//
// Because rows are as tall as the tallest cell, cells need to be aligned into
// the enclosing row space. To achieve this, LayoutTableCell introduces the
// concept of 'intrinsic padding'. Those 2 paddings are used to shift the box
// into the row as follows:
//
//        --------------------------------
//        ^  ^
//        |  |
//        |  |    cell's border before
//        |  |
//        |  v
//        |  ^
//        |  |
//        |  | m_intrinsicPaddingBefore
//        |  |
//        |  v
//        |  -----------------------------
//        |  |                           |
// row    |  |   cell's padding box      |
// height |  |                           |
//        |  -----------------------------
//        |  ^
//        |  |
//        |  | m_intrinsicPaddingAfter
//        |  |
//        |  v
//        |  ^
//        |  |
//        |  |    cell's border after
//        |  |
//        v  v
//        ---------------------------------
//
// Note that this diagram is not impacted by collapsing or separate borders
// (see 'border-collapse').
// Also there is no margin on table cell (or any internal table element).
//
// LayoutTableCell is positioned with respect to the enclosing
// LayoutTableSection. See callers of
// LayoutTableSection::setLogicalPositionForCell() for when it is placed.
class CORE_EXPORT LayoutTableCell : public LayoutBlockFlow,
                                    public LayoutNGTableCellInterface {
 public:
  explicit LayoutTableCell(Element*);

  unsigned ColSpan() const final {
    NOT_DESTROYED();
    if (!has_col_span_)
      return 1;
    return ParseColSpanFromDOM();
  }
  unsigned ParsedRowSpan() const {
    NOT_DESTROYED();
    if (!has_row_span_)
      return 1;
    return ParseRowSpanFromDOM();
  }
  unsigned ResolvedRowSpan() const final {
    NOT_DESTROYED();
    unsigned row_span = ParsedRowSpan();
    if (!row_span) {
      DCHECK(!Section()->NeedsCellRecalc());
      row_span = Section()->NumRows() - RowIndex();
    }
    return std::min<unsigned>(row_span, kMaxRowIndex);
  }

  // Called from HTMLTableCellElement.
  void ColSpanOrRowSpanChanged() final;

  void SetAbsoluteColumnIndex(unsigned column) {
    NOT_DESTROYED();
    CHECK_LE(column, kMaxColumnIndex);
    absolute_column_index_ = column;
  }

  bool HasSetAbsoluteColumnIndex() const {
    NOT_DESTROYED();
    return absolute_column_index_ != kUnsetColumnIndex;
  }

  unsigned AbsoluteColumnIndex() const final {
    NOT_DESTROYED();
    DCHECK(HasSetAbsoluteColumnIndex());
    return absolute_column_index_;
  }

  LayoutTableRow* Row() const {
    NOT_DESTROYED();
    return To<LayoutTableRow>(Parent());
  }
  LayoutTableSection* Section() const {
    NOT_DESTROYED();
    return To<LayoutTableSection>(Parent()->Parent());
  }
  LayoutTable* Table() const {
    NOT_DESTROYED();
    return To<LayoutTable>(Parent()->Parent()->Parent());
  }

  LayoutTableCell* PreviousCell() const;
  LayoutTableCell* NextCell() const;

  unsigned RowIndex() const final {
    NOT_DESTROYED();
    // This function shouldn't be called on a detached cell.
    DCHECK(Row());
    return Row()->RowIndex();
  }

  Length StyleOrColLogicalWidth() const {
    NOT_DESTROYED();
    const Length& style_width = StyleRef().LogicalWidth();
    if (!style_width.IsAuto())
      return style_width;
    if (LayoutTableCol* first_column =
            Table()
                ->ColElementAtAbsoluteColumn(AbsoluteColumnIndex())
                .InnermostColOrColGroup())
      return LogicalWidthFromColumns(first_column, style_width);
    return style_width;
  }

  int LogicalHeightFromStyle() const {
    NOT_DESTROYED();
    const Length& height = StyleRef().LogicalHeight();
    int style_logical_height =
        height.IsSpecified() ? ValueForLength(height, LayoutUnit()).ToInt() : 0;

    // In strict mode, box-sizing: content-box do the right thing and actually
    // add in the border and padding.
    // Call computedCSSPadding* directly to avoid including implicitPadding.
    if (!GetDocument().InQuirksMode() &&
        StyleRef().BoxSizing() != EBoxSizing::kBorderBox) {
      style_logical_height +=
          (ComputedCSSPaddingBefore() + ComputedCSSPaddingAfter()).Floor() +
          (BorderBefore() + BorderAfter()).Floor();
    }
    return style_logical_height;
  }

  int LogicalHeightForRowSizing() const {
    NOT_DESTROYED();
    // FIXME: This function does too much work, and is very hot during table
    // layout!
    int adjusted_logical_height =
        PixelSnappedLogicalHeight() -
        (IntrinsicPaddingBefore() + IntrinsicPaddingAfter());
    int style_logical_height = LogicalHeightFromStyle();
    return max(style_logical_height, adjusted_logical_height);
  }

  void SetCellLogicalWidth(int constrained_logical_width, SubtreeLayoutScope&);

  // Returns true if a non-column-spanning cell is in a collapsed column, or if
  // a column-spanning cell starts in a collapsed column.
  bool IsFirstColumnCollapsed() const;

  LayoutUnit BorderLeft() const override;
  LayoutUnit BorderRight() const override;
  LayoutUnit BorderTop() const override;
  LayoutUnit BorderBottom() const override;

  void UpdateLayout() override;

  LayoutUnit CellBaselinePosition() const;
  bool IsBaselineAligned() const {
    NOT_DESTROYED();
    EVerticalAlign va = StyleRef().VerticalAlign();
    return va == EVerticalAlign::kBaseline ||
           va == EVerticalAlign::kTextBottom ||
           va == EVerticalAlign::kTextTop || va == EVerticalAlign::kSuper ||
           va == EVerticalAlign::kSub || va == EVerticalAlign::kLength;
  }

  // Align the cell in the block direction. This is done by calculating an
  // intrinsic padding before and after the cell contents, so that all cells in
  // the row get the same logical height.
  void ComputeIntrinsicPadding(int collapsed_height,
                               int row_height,
                               EVerticalAlign,
                               SubtreeLayoutScope&);

  void ClearIntrinsicPadding() {
    NOT_DESTROYED();
    SetIntrinsicPadding(0, 0);
  }

  int IntrinsicPaddingBefore() const {
    NOT_DESTROYED();
    return intrinsic_padding_before_;
  }
  int IntrinsicPaddingAfter() const {
    NOT_DESTROYED();
    return intrinsic_padding_after_;
  }

  LayoutUnit PaddingTop() const override;
  LayoutUnit PaddingBottom() const override;
  LayoutUnit PaddingLeft() const override;
  LayoutUnit PaddingRight() const override;

  // TODO(crbug.com/962299): This is incorrect in some cases.
  gfx::Size PixelSnappedSize() const {
    NOT_DESTROYED();
    return FrameRect().PixelSnappedSize();
  }

  void SetOverrideLogicalHeightFromRowHeight(LayoutUnit);

  void ScrollbarsChanged(bool horizontal_scrollbar_changed,
                         bool vertical_scrollbar_changed,
                         ScrollbarChangeContext = kLayout) override;

  bool CellChildrenNeedLayout() const {
    NOT_DESTROYED();
    return cell_children_need_layout_;
  }
  void SetCellChildrenNeedLayout(bool b = true) {
    NOT_DESTROYED();
    cell_children_need_layout_ = b;
  }

  static LayoutTableCell* CreateAnonymous(Document*,
                                          scoped_refptr<ComputedStyle>,
                                          LegacyLayout);

  LayoutBox* CreateAnonymousBoxWithSameTypeAs(
      const LayoutObject* parent) const override;

  // The table's style determines cell order and cell adjacency in the table.
  // Collapsed borders also use in table's inline and block directions.
  const ComputedStyle& TableStyle() const {
    NOT_DESTROYED();
    return Table()->StyleRef();
  }

  BorderValue BorderStartInTableDirection() const {
    NOT_DESTROYED();
    return StyleRef().BorderStartUsing(TableStyle());
  }
  BorderValue BorderEndInTableDirection() const {
    NOT_DESTROYED();
    return StyleRef().BorderEndUsing(TableStyle());
  }
  BorderValue BorderBeforeInTableDirection() const {
    NOT_DESTROYED();
    return StyleRef().BorderBeforeUsing(TableStyle());
  }
  BorderValue BorderAfterInTableDirection() const {
    NOT_DESTROYED();
    return StyleRef().BorderAfterUsing(TableStyle());
  }

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutTableCell";
  }

  bool BackgroundIsKnownToBeOpaqueInRect(const PhysicalRect&) const override;

  const CollapsedBorderValues* GetCollapsedBorderValues() const {
    NOT_DESTROYED();
    UpdateCollapsedBorderValues();
    return collapsed_border_values_.get();
  }
  void InvalidateCollapsedBorderValues() {
    NOT_DESTROYED();
    collapsed_border_values_valid_ = false;
  }

  // A table cell's location is relative to its containing section.
  LayoutBox* LocationContainer() const override {
    NOT_DESTROYED();
    return Section();
  }

  bool HasLineIfEmpty() const override;

  static bool CompareInDOMOrder(const LayoutTableCell* cell1,
                                const LayoutTableCell* cell2) {
    DCHECK(cell1->Section() == cell2->Section());
    if (cell1->RowIndex() == cell2->RowIndex())
      return cell1->absolute_column_index_ < cell2->absolute_column_index_;
    return cell1->RowIndex() < cell2->RowIndex();
  }

  // For the following methods, the 'start', 'end', 'before', 'after' directions
  // are all in the table's inline and block directions.
  unsigned CollapsedOuterBorderBefore() const {
    NOT_DESTROYED();
    return CollapsedBorderHalfBefore(true);
  }
  unsigned CollapsedOuterBorderAfter() const {
    NOT_DESTROYED();
    return CollapsedBorderHalfAfter(true);
  }
  unsigned CollapsedOuterBorderStart() const {
    NOT_DESTROYED();
    return CollapsedBorderHalfStart(true);
  }
  unsigned CollapsedOuterBorderEnd() const {
    NOT_DESTROYED();
    return CollapsedBorderHalfEnd(true);
  }
  unsigned CollapsedInnerBorderBefore() const {
    NOT_DESTROYED();
    return CollapsedBorderHalfBefore(false);
  }
  unsigned CollapsedInnerBorderAfter() const {
    NOT_DESTROYED();
    return CollapsedBorderHalfAfter(false);
  }
  unsigned CollapsedInnerBorderStart() const {
    NOT_DESTROYED();
    return CollapsedBorderHalfStart(false);
  }
  unsigned CollapsedInnerBorderEnd() const {
    NOT_DESTROYED();
    return CollapsedBorderHalfEnd(false);
  }

  bool StartsAtSameColumn(const LayoutTableCell* other) const {
    NOT_DESTROYED();
    return other && AbsoluteColumnIndex() == other->AbsoluteColumnIndex();
  }
  bool EndsAtSameColumn(const LayoutTableCell* other) const {
    NOT_DESTROYED();
    return other && AbsoluteColumnIndex() + ColSpan() ==
                        other->AbsoluteColumnIndex() + other->ColSpan();
  }
  bool StartsAtSameRow(const LayoutTableCell* other) const {
    NOT_DESTROYED();
    return other && RowIndex() == other->RowIndex();
  }
  bool EndsAtSameRow(const LayoutTableCell* other) const {
    NOT_DESTROYED();
    return other && RowIndex() + ResolvedRowSpan() ==
                        other->RowIndex() + other->ResolvedRowSpan();
  }

  void SetIsSpanningCollapsedRow(bool spanning_collapsed_row);

  bool IsSpanningCollapsedRow() const {
    NOT_DESTROYED();
    return is_spanning_collapsed_row_;
  }

  void SetIsSpanningCollapsedColumn(bool spanningCollapsedColumn);

  bool IsSpanningCollapsedColumn() const {
    NOT_DESTROYED();
    return is_spanning_collapsed_column_;
  }

  void ComputeVisualOverflow(bool recompute_floats) override;

  // LayoutNGTableCellInterface implementation start.

  const LayoutNGTableCellInterface* ToLayoutNGTableCellInterface() const final {
    NOT_DESTROYED();
    return this;
  }
  const LayoutObject* ToLayoutObject() const final {
    NOT_DESTROYED();
    return this;
  }
  LayoutObject* ToMutableLayoutObject() final {
    NOT_DESTROYED();
    return this;
  }
  LayoutNGTableInterface* TableInterface() const final {
    NOT_DESTROYED();
    return Table();
  }
  LayoutTableCell* NextCellInterface() const final {
    NOT_DESTROYED();
    return NextCell();
  }
  LayoutTableCell* PreviousCellInterface() const final {
    NOT_DESTROYED();
    return PreviousCell();
  }
  LayoutNGTableRowInterface* RowInterface() const final {
    NOT_DESTROYED();
    return Row();
  }
  LayoutNGTableSectionInterface* SectionInterface() const final {
    NOT_DESTROYED();
    return Section();
  }

  // LayoutNGTableCellInterface implementation end.

  MinMaxSizes PreferredLogicalWidths() const override;

  void UpdateStyleWritingModeFromRow(const LayoutObject* row);

 protected:
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  void InvalidatePaint(const PaintInvalidatorContext&) const override;

  PhysicalOffset OffsetFromContainerInternal(
      const LayoutObject*,
      bool ignore_scroll_offset) const override;

  bool CreatesNewFormattingContext() const final {
    NOT_DESTROYED();
    return true;
  }

 protected:
  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectTableCell ||
           type == kLayoutObjectTableCellLegacy ||
           LayoutBlockFlow::IsOfType(type);
  }

 private:
  friend class LayoutTableCellTest;

  void WillBeRemovedFromTree() override;

  void UpdateLogicalWidth() override;

  void PaintBoxDecorationBackground(
      const PaintInfo&,
      const PhysicalOffset& paint_offset) const override;
  void PaintMask(const PaintInfo&,
                 const PhysicalOffset& paint_offset) const override;

  OverflowClipAxes ComputeOverflowClipAxes() const override;

  using CollapsedBorderValuesMethod =
      const CollapsedBorderValue& (CollapsedBorderValues::*)() const;
  LogicalToPhysical<CollapsedBorderValuesMethod>
  CollapsedBorderValuesMethodsPhysical() const {
    NOT_DESTROYED();
    return LogicalToPhysical<CollapsedBorderValuesMethod>(
        // Collapsed border logical directions are in table's directions.
        TableStyle().GetWritingDirection(), &CollapsedBorderValues::StartBorder,
        &CollapsedBorderValues::EndBorder, &CollapsedBorderValues::BeforeBorder,
        &CollapsedBorderValues::AfterBorder);
  }

  // Give the extra pixel of half collapsed border to top and left.
  static constexpr bool kInnerHalfPixelAsOneTop = true;
  static constexpr bool kInnerHalfPixelAsOneRight = false;
  static constexpr bool kInnerHalfPixelAsOneBottom = false;
  static constexpr bool kInnerHalfPixelAsOneLeft = true;

  PhysicalToLogical<bool> InnerHalfPixelAsOneLogical() const {
    NOT_DESTROYED();
    return PhysicalToLogical<bool>(
        // Collapsed border logical directions are in table's directions.
        TableStyle().GetWritingDirection(), kInnerHalfPixelAsOneTop,
        kInnerHalfPixelAsOneRight, kInnerHalfPixelAsOneBottom,
        kInnerHalfPixelAsOneLeft);
  }

  unsigned CollapsedBorderHalfLeft(bool outer) const {
    NOT_DESTROYED();
    return CollapsedBorderHalf(kInnerHalfPixelAsOneLeft ^ outer,
                               CollapsedBorderValuesMethodsPhysical().Left());
  }
  unsigned CollapsedBorderHalfRight(bool outer) const {
    NOT_DESTROYED();
    return CollapsedBorderHalf(kInnerHalfPixelAsOneRight ^ outer,
                               CollapsedBorderValuesMethodsPhysical().Right());
  }
  unsigned CollapsedBorderHalfTop(bool outer) const {
    NOT_DESTROYED();
    return CollapsedBorderHalf(kInnerHalfPixelAsOneTop ^ outer,
                               CollapsedBorderValuesMethodsPhysical().Top());
  }
  unsigned CollapsedBorderHalfBottom(bool outer) const {
    NOT_DESTROYED();
    return CollapsedBorderHalf(kInnerHalfPixelAsOneBottom ^ outer,
                               CollapsedBorderValuesMethodsPhysical().Bottom());
  }

  // For the following methods, the 'start', 'end', 'before', 'after' directions
  // are all in the table's inline and block directions.
  unsigned CollapsedBorderHalfStart(bool outer) const {
    NOT_DESTROYED();
    return CollapsedBorderHalf(InnerHalfPixelAsOneLogical().Start() ^ outer,
                               &CollapsedBorderValues::StartBorder);
  }
  unsigned CollapsedBorderHalfEnd(bool outer) const {
    NOT_DESTROYED();
    return CollapsedBorderHalf(InnerHalfPixelAsOneLogical().End() ^ outer,
                               &CollapsedBorderValues::EndBorder);
  }
  unsigned CollapsedBorderHalfBefore(bool outer) const {
    NOT_DESTROYED();
    return CollapsedBorderHalf(InnerHalfPixelAsOneLogical().Before() ^ outer,
                               &CollapsedBorderValues::BeforeBorder);
  }
  unsigned CollapsedBorderHalfAfter(bool outer) const {
    NOT_DESTROYED();
    return CollapsedBorderHalf(InnerHalfPixelAsOneLogical().After() ^ outer,
                               &CollapsedBorderValues::AfterBorder);
  }

  unsigned CollapsedBorderHalf(bool half_pixel_as_one,
                               CollapsedBorderValuesMethod m) const {
    NOT_DESTROYED();
    UpdateCollapsedBorderValues();
    if (const auto* values = GetCollapsedBorderValues())
      return ((values->*m)().Width() + (half_pixel_as_one ? 1 : 0)) / 2;
    return 0;
  }

  LogicalToPhysical<int> LogicalIntrinsicPaddingToPhysical() const {
    NOT_DESTROYED();
    return LogicalToPhysical<int>(StyleRef().GetWritingDirection(), 0, 0,
                                  intrinsic_padding_before_,
                                  intrinsic_padding_after_);
  }
  void SetIntrinsicPaddingBefore(int p) {
    NOT_DESTROYED();
    intrinsic_padding_before_ = p;
  }
  void SetIntrinsicPaddingAfter(int p) {
    NOT_DESTROYED();
    intrinsic_padding_after_ = p;
  }
  void SetIntrinsicPadding(int before, int after) {
    NOT_DESTROYED();
    SetIntrinsicPaddingBefore(before);
    SetIntrinsicPaddingAfter(after);
  }

  bool IsInStartColumn() const {
    NOT_DESTROYED();
    return AbsoluteColumnIndex() == 0;
  }
  bool IsInEndColumn() const;

  // These functions implement the CSS collapsing border conflict resolution
  // algorithm http://www.w3.org/TR/CSS2/tables.html#border-conflict-resolution.
  // They are called during UpdateCollapsedBorderValues(). The 'start', 'end',
  // 'before', 'after' directions are all in the table's inline and block
  // directions.
  inline const CSSProperty& ResolveBorderProperty(const CSSProperty&) const;
  CollapsedBorderValue ComputeCollapsedStartBorder() const;
  CollapsedBorderValue ComputeCollapsedEndBorder() const;
  CollapsedBorderValue ComputeCollapsedBeforeBorder() const;
  CollapsedBorderValue ComputeCollapsedAfterBorder() const;

  void UpdateCollapsedBorderValues() const;

  Length LogicalWidthFromColumns(LayoutTableCol* first_col_for_this_cell,
                                 const Length& width_from_style) const;

  void UpdateColAndRowSpanFlags();

  unsigned ParseRowSpanFromDOM() const;
  unsigned ParseColSpanFromDOM() const;

  void NextSibling() const = delete;
  void PreviousSibling() const = delete;

  unsigned absolute_column_index_ : BITS_OF_ABSOLUTE_COLUMN_INDEX;

  // When adding or removing bits here, we should also adjust
  // BITS_OF_ABSOLUTE_COLUMN_INDEX to use remaining bits of a 32-bit word.
  // Note MSVC will only pack members if they have identical types, hence we use
  // unsigned instead of bool here.
  unsigned cell_children_need_layout_ : 1;
  unsigned has_col_span_ : 1;
  unsigned has_row_span_ : 1;
  unsigned is_spanning_collapsed_row_ : 1;
  unsigned is_spanning_collapsed_column_ : 1;

  // This is set to false when |collapsed_border_values_| needs update.
  mutable unsigned collapsed_border_values_valid_ : 1;
  mutable unsigned collapsed_borders_need_paint_invalidation_ : 1;
  mutable std::unique_ptr<CollapsedBorderValues> collapsed_border_values_;

  // The intrinsic padding.
  // See class comment for what they are.
  //
  // Note: Those fields are using non-subpixel units (int)
  // because we don't do fractional arithmetic on tables.
  int intrinsic_padding_before_;
  int intrinsic_padding_after_;
};

inline LayoutTableCell* LayoutTableCell::PreviousCell() const {
  return To<LayoutTableCell>(LayoutObject::PreviousSibling());
}

inline LayoutTableCell* LayoutTableCell::NextCell() const {
  return To<LayoutTableCell>(LayoutObject::NextSibling());
}

inline LayoutTableCell* LayoutTableRow::FirstCell() const {
  return To<LayoutTableCell>(FirstChild());
}

inline LayoutTableCell* LayoutTableRow::LastCell() const {
  return To<LayoutTableCell>(LastChild());
}

// To<LayoutTableCell>() helper.
template <>
struct DowncastTraits<LayoutTableCell> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsTableCellLegacy();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TABLE_CELL_H_
