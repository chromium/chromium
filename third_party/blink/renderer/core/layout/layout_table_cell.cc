/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc.
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

#include "third_party/blink/renderer/core/layout/layout_table_cell.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/collapsed_border_value.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/core/layout/layout_table_col.h"
#include "third_party/blink/renderer/core/layout/subtree_layout_scope.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/table_cell_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/table_cell_painter.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"

namespace blink {

struct SameSizeAsLayoutTableCell : public LayoutBlockFlow,
                                   public LayoutNGTableCellInterface {
  unsigned bitfields;
  int paddings[2];
  void* pointer1;
};

static_assert(sizeof(LayoutTableCell) == sizeof(SameSizeAsLayoutTableCell),
              "LayoutTableCell should stay small");
static_assert(sizeof(CollapsedBorderValue) == 8,
              "CollapsedBorderValue should stay small");

LayoutTableCell::LayoutTableCell(Element* element)
    : LayoutBlockFlow(element),
      absolute_column_index_(kUnsetColumnIndex),
      cell_children_need_layout_(false),
      is_spanning_collapsed_row_(false),
      is_spanning_collapsed_column_(false),
      collapsed_border_values_valid_(false),
      collapsed_borders_need_paint_invalidation_(false),
      intrinsic_padding_before_(0),
      intrinsic_padding_after_(0) {
  // We only update the flags when notified of DOM changes in
  // colSpanOrRowSpanChanged() so we need to set their initial values here in
  // case something asks for colSpan()/rowSpan() before then.
  UpdateColAndRowSpanFlags();
}

void LayoutTableCell::WillBeRemovedFromTree() {
  LayoutBlockFlow::WillBeRemovedFromTree();

  Section()->SetNeedsCellRecalc();

  // When borders collapse, removing a cell can affect the the width of
  // neighboring cells.
  LayoutTable* enclosing_table = Table();
  DCHECK(enclosing_table);
  if (!enclosing_table->ShouldCollapseBorders())
    return;
  if (PreviousCell()) {
    // TODO(dgrogan): Should this be setChildNeedsLayout or setNeedsLayout?
    // remove-cell-with-border-box.html only passes with setNeedsLayout but
    // other places use setChildNeedsLayout.
    PreviousCell()->SetNeedsLayout(layout_invalidation_reason::kTableChanged);
    PreviousCell()->SetPreferredLogicalWidthsDirty();
  }
  if (NextCell()) {
    // TODO(dgrogan): Same as above re: setChildNeedsLayout vs setNeedsLayout.
    NextCell()->SetNeedsLayout(layout_invalidation_reason::kTableChanged);
    NextCell()->SetPreferredLogicalWidthsDirty();
  }
}

unsigned LayoutTableCell::ParseColSpanFromDOM() const {
  DCHECK(GetNode());
  // TODO(dgrogan): HTMLTableCellElement::colSpan() already clamps to something
  // smaller than maxColumnIndex; can we just DCHECK here?
  if (IsHTMLTableCellElement(*GetNode()))
    return std::min<unsigned>(ToHTMLTableCellElement(*GetNode()).colSpan(),
                              kMaxColumnIndex);
  return 1;
}

unsigned LayoutTableCell::ParseRowSpanFromDOM() const {
  DCHECK(GetNode());
  if (IsHTMLTableCellElement(*GetNode()))
    return std::min<unsigned>(ToHTMLTableCellElement(*GetNode()).rowSpan(),
                              kMaxRowIndex);
  return 1;
}

void LayoutTableCell::UpdateColAndRowSpanFlags() {
  // The vast majority of table cells do not have a colspan or rowspan,
  // so we keep a bool to know if we need to bother reading from the DOM.
  has_col_span_ = GetNode() && ParseColSpanFromDOM() != 1;
  has_row_span_ = GetNode() && ParseRowSpanFromDOM() != 1;
}

void LayoutTableCell::ColSpanOrRowSpanChanged() {
  DCHECK(GetNode());
  DCHECK(IsHTMLTableCellElement(*GetNode()));

  UpdateColAndRowSpanFlags();

  SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kAttributeChanged);
  if (Parent() && Section()) {
    Section()->SetNeedsCellRecalc();
    if (Table() && Table()->ShouldCollapseBorders())
      collapsed_borders_need_paint_invalidation_ = true;
  }
}

Length LayoutTableCell::LogicalWidthFromColumns(
    LayoutTableCol* first_col_for_this_cell,
    const Length& width_from_style) const {
  DCHECK(first_col_for_this_cell);
  DCHECK_EQ(first_col_for_this_cell,
            Table()
                ->ColElementAtAbsoluteColumn(AbsoluteColumnIndex())
                .InnermostColOrColGroup());
  LayoutTableCol* table_col = first_col_for_this_cell;

  unsigned col_span_count = ColSpan();
  int col_width_sum = 0;
  for (unsigned i = 1; i <= col_span_count; i++) {
    const Length& col_width = table_col->StyleRef().LogicalWidth();

    // Percentage value should be returned only for colSpan == 1.
    // Otherwise we return original width for the cell.
    if (!col_width.IsFixed()) {
      if (col_span_count > 1)
        return width_from_style;
      return col_width;
    }

    col_width_sum += col_width.Value();
    table_col = table_col->NextColumn();
    // If no next <col> tag found for the span we just return what we have for
    // now.
    if (!table_col)
      break;
  }

  // Column widths specified on <col> apply to the border box of the cell, see
  // bug 8126.
  // FIXME: Why is border/padding ignored in the negative width case?
  if (col_width_sum > 0) {
    return Length::Fixed(
        std::max(0, col_width_sum - BorderAndPaddingLogicalWidth().Ceil()));
  }
  return Length::Fixed(col_width_sum);
}

void LayoutTableCell::ComputePreferredLogicalWidths() {
  // The child cells rely on the grids up in the sections to do their
  // computePreferredLogicalWidths work.  Normally the sections are set up
  // early, as table cells are added, but relayout can cause the cells to be
  // freed, leaving stale pointers in the sections' grids. We must refresh those
  // grids before the child cells try to use them.
  Table()->RecalcSectionsIfNeeded();

  // We don't want the preferred width from children to be affected by any
  // notional height on the cell, such as can happen when a percent sized image
  // scales up its width to match the available height. Setting a zero override
  // height prevents this from happening.
  LayoutUnit logical_height =
      HasOverrideLogicalHeight() ? OverrideLogicalHeight() : LayoutUnit(-1);
  if (logical_height > -1)
    SetOverrideLogicalHeight(LayoutUnit());
  LayoutBlockFlow::ComputePreferredLogicalWidths();
  if (logical_height > -1)
    SetOverrideLogicalHeight(logical_height);

  if (GetNode() && StyleRef().AutoWrap()) {
    // See if nowrap was set.
    Length w = StyleOrColLogicalWidth();
    const AtomicString& nowrap =
        To<Element>(GetNode())->FastGetAttribute(html_names::kNowrapAttr);
    if (!nowrap.IsNull() && w.IsFixed()) {
      // Nowrap is set, but we didn't actually use it because of the fixed width
      // set on the cell. Even so, it is a WinIE/Moz trait to make the minwidth
      // of the cell into the fixed width. They do this even in strict mode, so
      // do not make this a quirk. Affected the top of hiptop.com.
      min_preferred_logical_width_ =
          std::max(LayoutUnit(w.Value()), min_preferred_logical_width_);
    }
  }
}

void LayoutTableCell::ComputeIntrinsicPadding(int collapsed_height,
                                              int row_height,
                                              EVerticalAlign vertical_align,
                                              SubtreeLayoutScope& layouter) {
  int old_intrinsic_padding_before = IntrinsicPaddingBefore();
  int old_intrinsic_padding_after = IntrinsicPaddingAfter();
  int logical_height_without_intrinsic_padding = PixelSnappedLogicalHeight() -
                                                 old_intrinsic_padding_before -
                                                 old_intrinsic_padding_after;

  int intrinsic_padding_before = 0;
  switch (vertical_align) {
    case EVerticalAlign::kSub:
    case EVerticalAlign::kSuper:
    case EVerticalAlign::kTextTop:
    case EVerticalAlign::kTextBottom:
    case EVerticalAlign::kLength:
    case EVerticalAlign::kBaseline: {
      LayoutUnit baseline = CellBaselinePosition();
      if (baseline > BorderBefore() + PaddingBefore()) {
        intrinsic_padding_before = (Section()->RowBaseline(RowIndex()) -
                                    (baseline - old_intrinsic_padding_before))
                                       .Round();
      }
      break;
    }
    case EVerticalAlign::kTop:
      break;
    case EVerticalAlign::kMiddle:
      intrinsic_padding_before = (row_height + collapsed_height -
                                  logical_height_without_intrinsic_padding) /
                                 2;
      break;
    case EVerticalAlign::kBottom:
      intrinsic_padding_before = row_height + collapsed_height -
                                 logical_height_without_intrinsic_padding;
      break;
    case EVerticalAlign::kBaselineMiddle:
      break;
  }

  int intrinsic_padding_after = row_height -
                                logical_height_without_intrinsic_padding -
                                intrinsic_padding_before;
  SetIntrinsicPaddingBefore(intrinsic_padding_before);
  SetIntrinsicPaddingAfter(intrinsic_padding_after);

  // FIXME: Changing an intrinsic padding shouldn't trigger a relayout as it
  // only shifts the cell inside the row but doesn't change the logical height.
  if (intrinsic_padding_before != old_intrinsic_padding_before ||
      intrinsic_padding_after != old_intrinsic_padding_after)
    layouter.SetNeedsLayout(this, layout_invalidation_reason::kPaddingChanged);
}

void LayoutTableCell::UpdateLogicalWidth() {}

void LayoutTableCell::SetCellLogicalWidth(int table_layout_logical_width,
                                          SubtreeLayoutScope& layouter) {
  if (table_layout_logical_width == LogicalWidth())
    return;

  layouter.SetNeedsLayout(this, layout_invalidation_reason::kSizeChanged);

  SetLogicalWidth(LayoutUnit(table_layout_logical_width));
  SetCellChildrenNeedLayout(true);
}

void LayoutTableCell::UpdateLayout() {
  DCHECK(NeedsLayout());
  LayoutAnalyzer::Scope analyzer(*this);

  UpdateBlockLayout(CellChildrenNeedLayout());

  // FIXME: This value isn't the intrinsic content logical height, but we need
  // to update the value as its used by flexbox layout. crbug.com/367324
  SetIntrinsicContentLogicalHeight(ContentLogicalHeight());

  SetCellChildrenNeedLayout(false);
}

LayoutUnit LayoutTableCell::PaddingTop() const {
  auto result =
      ComputedCSSPaddingTop() + LogicalIntrinsicPaddingToPhysical().Top();
  // TODO(crbug.com/377847): The ToInt call should be removed when Table is
  // sub-pixel aware.
  return StyleRef().IsHorizontalWritingMode() ? LayoutUnit(result.ToInt())
                                              : result;
}

LayoutUnit LayoutTableCell::PaddingBottom() const {
  auto result =
      ComputedCSSPaddingBottom() + LogicalIntrinsicPaddingToPhysical().Bottom();
  // TODO(crbug.com/377847): The ToInt call should be removed when Table is
  // sub-pixel aware.
  return StyleRef().IsHorizontalWritingMode() ? LayoutUnit(result.ToInt())
                                              : result;
}

LayoutUnit LayoutTableCell::PaddingLeft() const {
  auto result =
      ComputedCSSPaddingLeft() + LogicalIntrinsicPaddingToPhysical().Left();
  // TODO(crbug.com/377847): The ToInt call should be removed when Table is
  // sub-pixel aware.
  return StyleRef().IsHorizontalWritingMode() ? result
                                              : LayoutUnit(result.ToInt());
}

LayoutUnit LayoutTableCell::PaddingRight() const {
  auto result =
      ComputedCSSPaddingRight() + LogicalIntrinsicPaddingToPhysical().Right();
  // TODO(crbug.com/377847): The ToInt call should be removed when Table is
  // sub-pixel aware.
  return StyleRef().IsHorizontalWritingMode() ? result
                                              : LayoutUnit(result.ToInt());
}

void LayoutTableCell::SetOverrideLogicalHeightFromRowHeight(
    LayoutUnit row_height) {
  ClearIntrinsicPadding();
  SetOverrideLogicalHeight(row_height);
}

PhysicalOffset LayoutTableCell::OffsetFromContainerInternal(
    const LayoutObject* o,
    bool ignore_scroll_offset) const {
  DCHECK_EQ(o, Container());

  PhysicalOffset offset =
      LayoutBlockFlow::OffsetFromContainerInternal(o, ignore_scroll_offset);
  if (Parent())
    offset -= ParentBox()->PhysicalLocation();

  return offset;
}

void LayoutTableCell::SetIsSpanningCollapsedRow(bool spanning_collapsed_row) {
  if (is_spanning_collapsed_row_ != spanning_collapsed_row) {
    is_spanning_collapsed_row_ = spanning_collapsed_row;
    SetShouldClipOverflow(ComputeShouldClipOverflow());
  }
}

void LayoutTableCell::SetIsSpanningCollapsedColumn(
    bool spanning_collapsed_column) {
  if (is_spanning_collapsed_column_ != spanning_collapsed_column) {
    is_spanning_collapsed_column_ = spanning_collapsed_column;
    SetShouldClipOverflow(ComputeShouldClipOverflow());
  }
}

void LayoutTableCell::ComputeVisualOverflow(
    bool recompute_floats) {
  LayoutBlockFlow::ComputeVisualOverflow(recompute_floats);

  UpdateCollapsedBorderValues();
  if (!collapsed_border_values_)
    return;

  // Calculate local visual rect of collapsed borders.
  // Our border rect already includes the inner halves of the collapsed borders,
  // so here we get the outer halves.
  bool rtl = !TableStyle().IsLeftToRightDirection();
  unsigned left = CollapsedBorderHalfLeft(true);
  unsigned right = CollapsedBorderHalfRight(true);
  unsigned top = CollapsedBorderHalfTop(true);
  unsigned bottom = CollapsedBorderHalfBottom(true);

  // TODO(layout-ng): The following looks incorrect for vertical direction.
  // This cell's borders may be lengthened to match the widths of orthogonal
  // borders of adjacent cells. Expand visual overflow to cover the lengthened
  // parts.
  if ((left && !rtl) || (right && rtl)) {
    if (LayoutTableCell* preceding = Table()->CellPreceding(*this)) {
      top = std::max(top, preceding->CollapsedBorderHalfTop(true));
      bottom = std::max(bottom, preceding->CollapsedBorderHalfBottom(true));
    }
  }
  if ((left && rtl) || (right && !rtl)) {
    if (LayoutTableCell* following = Table()->CellFollowing(*this)) {
      top = std::max(top, following->CollapsedBorderHalfTop(true));
      bottom = std::max(bottom, following->CollapsedBorderHalfBottom(true));
    }
  }
  if (top) {
    if (LayoutTableCell* above = Table()->CellAbove(*this)) {
      left = std::max(left, above->CollapsedBorderHalfLeft(true));
      right = std::max(right, above->CollapsedBorderHalfRight(true));
    }
  }
  if (bottom) {
    if (LayoutTableCell* below = Table()->CellBelow(*this)) {
      left = std::max(left, below->CollapsedBorderHalfLeft(true));
      right = std::max(right, below->CollapsedBorderHalfRight(true));
    }
  }

  LayoutRect rect = BorderBoxRect();
  rect.ExpandEdges(LayoutUnit(top), LayoutUnit(right), LayoutUnit(bottom),
                   LayoutUnit(left));
  collapsed_border_values_->SetLocalVisualRect(rect);
}

bool LayoutTableCell::ComputeShouldClipOverflow() const {
  return IsSpanningCollapsedRow() || IsSpanningCollapsedColumn() ||
         LayoutBox::ComputeShouldClipOverflow();
}

LayoutUnit LayoutTableCell::CellBaselinePosition() const {
  // <http://www.w3.org/TR/2007/CR-CSS21-20070719/tables.html#height-layout>:
  // The baseline of a cell is the baseline of the first in-flow line box in the
  // cell, or the first in-flow table-row in the cell, whichever comes first. If
  // there is no such line box or table-row, the baseline is the bottom of
  // content edge of the cell box.
  LayoutUnit first_line_baseline = FirstLineBoxBaseline();
  if (first_line_baseline != -1)
    return first_line_baseline;
  return BorderBefore() + PaddingBefore() + ContentLogicalHeight();
}

void LayoutTableCell::StyleDidChange(StyleDifference diff,
                                     const ComputedStyle* old_style) {
  DCHECK_EQ(StyleRef().Display(), EDisplay::kTableCell);

  LayoutBlockFlow::StyleDidChange(diff, old_style);
  SetHasBoxDecorationBackground(true);

  if (Row() && Section() && Table() && Table()->ShouldCollapseBorders())
    SetHasNonCollapsedBorderDecoration(false);

  if (!old_style)
    return;

  if (Parent() && Section() && StyleRef().Height() != old_style->Height())
    Section()->RowLogicalHeightChanged(Row());

  // Our intrinsic padding pushes us down to align with the baseline of other
  // cells on the row. If our vertical-align has changed then so will the
  // padding needed to align with other cells - clear it so we can recalculate
  // it from scratch.
  if (StyleRef().VerticalAlign() != old_style->VerticalAlign())
    ClearIntrinsicPadding();

  if (!Parent())
    return;
  LayoutTable* table = Table();
  if (!table)
    return;

  if (old_style->Visibility() != StyleRef().Visibility() &&
      table->ShouldCollapseBorders()) {
    table->InvalidateCollapsedBorders();
    collapsed_borders_need_paint_invalidation_ = true;
  }

  LayoutTableBoxComponent::InvalidateCollapsedBordersOnStyleChange(
      *this, *table, diff, *old_style);

  if (LayoutTableBoxComponent::DoCellsHaveDirtyWidth(*this, *table, diff,
                                                     *old_style)) {
    if (PreviousCell()) {
      // TODO(dgrogan) Add a web test showing that SetChildNeedsLayout is
      // needed instead of SetNeedsLayout.
      PreviousCell()->SetChildNeedsLayout();
      PreviousCell()->SetPreferredLogicalWidthsDirty(kMarkOnlyThis);
    }
    if (NextCell()) {
      // TODO(dgrogan) Add a web test showing that SetChildNeedsLayout is
      // needed instead of SetNeedsLayout.
      NextCell()->SetChildNeedsLayout();
      NextCell()->SetPreferredLogicalWidthsDirty(kMarkOnlyThis);
    }
  }
}

static CollapsedBorderValue ChooseBorder(const CollapsedBorderValue& border1,
                                         const CollapsedBorderValue& border2) {
  return border1.LessThan(border2) ? border2 : border1;
}

bool LayoutTableCell::IsInEndColumn() const {
  return Table()->AbsoluteColumnToEffectiveColumn(AbsoluteColumnIndex() +
                                                  ColSpan() - 1) ==
         Table()->NumEffectiveColumns() - 1;
}

const CSSProperty& LayoutTableCell::ResolveBorderProperty(
    const CSSProperty& property) const {
  return property.ResolveDirectionAwareProperty(TableStyle().Direction(),
                                                TableStyle().GetWritingMode());
}

CollapsedBorderValue LayoutTableCell::ComputeCollapsedStartBorder() const {
  LayoutTable* table = Table();
  bool in_start_column = IsInStartColumn();
  LayoutTableCell* cell_preceding =
      in_start_column ? nullptr : table->CellPreceding(*this);
  // We can use the border shared with |cell_before| if it is valid.
  if (StartsAtSameRow(cell_preceding) &&
      cell_preceding->collapsed_border_values_valid_) {
    return cell_preceding->GetCollapsedBorderValues()
               ? cell_preceding->GetCollapsedBorderValues()->EndBorder()
               : CollapsedBorderValue();
  }

  // For the start border, we need to check, in order of precedence:
  // (1) Our start border.
  const CSSProperty& start_color_property =
      ResolveBorderProperty(GetCSSPropertyBorderInlineStartColor());
  const CSSProperty& end_color_property =
      ResolveBorderProperty(GetCSSPropertyBorderInlineEndColor());
  CollapsedBorderValue result(BorderStartInTableDirection(),
                              ResolveColor(start_color_property),
                              kBorderPrecedenceCell);

  // (2) The end border of the preceding cell.
  if (cell_preceding) {
    CollapsedBorderValue cell_before_adjoining_border =
        CollapsedBorderValue(cell_preceding->BorderEndInTableDirection(),
                             cell_preceding->ResolveColor(end_color_property),
                             kBorderPrecedenceCell);
    // |result| should be the 2nd argument as |cellBefore| should win in case of
    // equality per CSS 2.1 (Border conflict resolution, point 4).
    result = ChooseBorder(cell_before_adjoining_border, result);
    if (!result.Exists())
      return result;
  }

  if (in_start_column) {
    // (3) Our row's start border.
    result = ChooseBorder(
        result,
        CollapsedBorderValue(Row()->BorderStartInTableDirection(),
                             Parent()->ResolveColor(start_color_property),
                             kBorderPrecedenceRow));
    if (!result.Exists())
      return result;

    // (4) Our row group's start border.
    result = ChooseBorder(
        result,
        CollapsedBorderValue(Section()->BorderStartInTableDirection(),
                             Section()->ResolveColor(start_color_property),
                             kBorderPrecedenceRowGroup));
    if (!result.Exists())
      return result;
  }

  // (5) Our column and column group's start borders.
  LayoutTable::ColAndColGroup col_and_col_group =
      table->ColElementAtAbsoluteColumn(AbsoluteColumnIndex());
  if (col_and_col_group.colgroup &&
      col_and_col_group.adjoins_start_border_of_col_group) {
    // Only apply the colgroup's border if this cell touches the colgroup edge.
    result = ChooseBorder(
        result,
        CollapsedBorderValue(
            col_and_col_group.colgroup->BorderStartInTableDirection(),
            col_and_col_group.colgroup->ResolveColor(start_color_property),
            kBorderPrecedenceColumnGroup));
    if (!result.Exists())
      return result;
  }
  if (col_and_col_group.col) {
    // Always apply the col's border irrespective of whether this cell touches
    // it. This is per HTML5: "For the purposes of the CSS table model, the col
    // element is expected to be treated as if it "was present as many times as
    // its span attribute specifies".
    result = ChooseBorder(
        result, CollapsedBorderValue(
                    col_and_col_group.col->BorderStartInTableDirection(),
                    col_and_col_group.col->ResolveColor(start_color_property),
                    kBorderPrecedenceColumn));
    if (!result.Exists())
      return result;
  }

  // (6) The end border of the preceding column.
  if (cell_preceding) {
    LayoutTable::ColAndColGroup col_and_col_group =
        table->ColElementAtAbsoluteColumn(AbsoluteColumnIndex() - 1);
    // Only apply the colgroup's border if this cell touches the colgroup edge.
    if (col_and_col_group.colgroup &&
        col_and_col_group.adjoins_end_border_of_col_group) {
      result = ChooseBorder(
          CollapsedBorderValue(
              col_and_col_group.colgroup->BorderEndInTableDirection(),
              col_and_col_group.colgroup->ResolveColor(end_color_property),
              kBorderPrecedenceColumnGroup),
          result);
      if (!result.Exists())
        return result;
    }
    // Always apply the col's border irrespective of whether this cell touches
    // it. This is per HTML5: "For the purposes of the CSS table model, the col
    // element is expected to be treated as if it "was present as many times as
    // its span attribute specifies".
    if (col_and_col_group.col) {
      result = ChooseBorder(
          CollapsedBorderValue(
              col_and_col_group.col->BorderEndInTableDirection(),
              col_and_col_group.col->ResolveColor(end_color_property),
              kBorderPrecedenceColumn),
          result);
      if (!result.Exists())
        return result;
    }
  }

  if (in_start_column) {
    // (7) The table's start border.
    result = ChooseBorder(
        result, CollapsedBorderValue(table->StyleRef().BorderStart(),
                                     table->ResolveColor(start_color_property),
                                     kBorderPrecedenceTable));
    if (!result.Exists())
      return result;
  }

  return result;
}

CollapsedBorderValue LayoutTableCell::ComputeCollapsedEndBorder() const {
  LayoutTable* table = Table();
  // Note: We have to use the effective column information instead of whether we
  // have a cell after as a table doesn't have to be regular (any row can have
  // less cells than the total cell count).
  bool in_end_column = IsInEndColumn();
  LayoutTableCell* cell_following =
      in_end_column ? nullptr : table->CellFollowing(*this);
  // We can use the border shared with |cell_after| if it is valid.
  if (StartsAtSameRow(cell_following) &&
      cell_following->collapsed_border_values_valid_) {
    return cell_following->GetCollapsedBorderValues()
               ? cell_following->GetCollapsedBorderValues()->StartBorder()
               : CollapsedBorderValue();
  }

  // For end border, we need to check, in order of precedence:
  // (1) Our end border.
  const CSSProperty& start_color_property =
      ResolveBorderProperty(GetCSSPropertyBorderInlineStartColor());
  const CSSProperty& end_color_property =
      ResolveBorderProperty(GetCSSPropertyBorderInlineEndColor());
  CollapsedBorderValue result = CollapsedBorderValue(
      BorderEndInTableDirection(), ResolveColor(end_color_property),
      kBorderPrecedenceCell);

  // (2) The start border of the following cell.
  if (cell_following) {
    CollapsedBorderValue cell_after_adjoining_border =
        CollapsedBorderValue(cell_following->BorderStartInTableDirection(),
                             cell_following->ResolveColor(start_color_property),
                             kBorderPrecedenceCell);
    result = ChooseBorder(result, cell_after_adjoining_border);
    if (!result.Exists())
      return result;
  }

  if (in_end_column) {
    // (3) Our row's end border.
    result = ChooseBorder(
        result, CollapsedBorderValue(Row()->BorderEndInTableDirection(),
                                     Parent()->ResolveColor(end_color_property),
                                     kBorderPrecedenceRow));
    if (!result.Exists())
      return result;

    // (4) Our row group's end border.
    result = ChooseBorder(
        result,
        CollapsedBorderValue(Section()->BorderEndInTableDirection(),
                             Section()->ResolveColor(end_color_property),
                             kBorderPrecedenceRowGroup));
    if (!result.Exists())
      return result;
  }

  // (5) Our column and column group's end borders.
  LayoutTable::ColAndColGroup col_and_col_group =
      table->ColElementAtAbsoluteColumn(AbsoluteColumnIndex() + ColSpan() - 1);
  if (col_and_col_group.colgroup &&
      col_and_col_group.adjoins_end_border_of_col_group) {
    // Only apply the colgroup's border if this cell touches the colgroup edge.
    result = ChooseBorder(
        result,
        CollapsedBorderValue(
            col_and_col_group.colgroup->BorderEndInTableDirection(),
            col_and_col_group.colgroup->ResolveColor(end_color_property),
            kBorderPrecedenceColumnGroup));
    if (!result.Exists())
      return result;
  }
  if (col_and_col_group.col) {
    // Always apply the col's border irrespective of whether this cell touches
    // it. This is per HTML5: "For the purposes of the CSS table model, the col
    // element is expected to be treated as if it "was present as many times as
    // its span attribute specifies".
    result = ChooseBorder(
        result, CollapsedBorderValue(
                    col_and_col_group.col->BorderEndInTableDirection(),
                    col_and_col_group.col->ResolveColor(end_color_property),
                    kBorderPrecedenceColumn));
    if (!result.Exists())
      return result;
  }

  // (6) The start border of the next column.
  if (!in_end_column) {
    LayoutTable::ColAndColGroup col_and_col_group =
        table->ColElementAtAbsoluteColumn(AbsoluteColumnIndex() + ColSpan());
    if (col_and_col_group.colgroup &&
        col_and_col_group.adjoins_start_border_of_col_group) {
      // Only apply the colgroup's border if this cell touches the colgroup
      // edge.
      result = ChooseBorder(
          result,
          CollapsedBorderValue(
              col_and_col_group.colgroup->BorderStartInTableDirection(),
              col_and_col_group.colgroup->ResolveColor(start_color_property),
              kBorderPrecedenceColumnGroup));
      if (!result.Exists())
        return result;
    }
    if (col_and_col_group.col) {
      // Always apply the col's border irrespective of whether this cell touches
      // it. This is per HTML5: "For the purposes of the CSS table model, the
      // col element is expected to be treated as if it "was present as many
      // times as its span attribute specifies".
      result = ChooseBorder(
          result, CollapsedBorderValue(
                      col_and_col_group.col->BorderStartInTableDirection(),
                      col_and_col_group.col->ResolveColor(start_color_property),
                      kBorderPrecedenceColumn));
      if (!result.Exists())
        return result;
    }
  }

  if (in_end_column) {
    // (7) The table's end border.
    result = ChooseBorder(
        result, CollapsedBorderValue(table->StyleRef().BorderEnd(),
                                     table->ResolveColor(end_color_property),
                                     kBorderPrecedenceTable));
    if (!result.Exists())
      return result;
  }

  return result;
}

CollapsedBorderValue LayoutTableCell::ComputeCollapsedBeforeBorder() const {
  LayoutTable* table = Table();
  LayoutTableCell* cell_above = table->CellAbove(*this);
  // We can use the border shared with |cell_above| if it is valid.
  if (StartsAtSameColumn(cell_above) &&
      cell_above->collapsed_border_values_valid_) {
    return cell_above->GetCollapsedBorderValues()
               ? cell_above->GetCollapsedBorderValues()->AfterBorder()
               : CollapsedBorderValue();
  }

  // For before border, we need to check, in order of precedence:
  // (1) Our before border.
  const CSSProperty& before_color_property =
      ResolveBorderProperty(GetCSSPropertyBorderBlockStartColor());
  const CSSProperty& after_color_property =
      ResolveBorderProperty(GetCSSPropertyBorderBlockEndColor());
  CollapsedBorderValue result = CollapsedBorderValue(
      StyleRef().BorderBeforeStyle(), StyleRef().BorderBeforeWidth(),
      ResolveColor(before_color_property), kBorderPrecedenceCell);

  if (cell_above) {
    // (2) A before cell's after border.
    result = ChooseBorder(
        CollapsedBorderValue(cell_above->StyleRef().BorderAfterStyle(),
                             cell_above->StyleRef().BorderAfterWidth(),
                             cell_above->ResolveColor(after_color_property),
                             kBorderPrecedenceCell),
        result);
    if (!result.Exists())
      return result;
  }

  // (3) Our row's before border.
  result = ChooseBorder(
      result,
      CollapsedBorderValue(Parent()->StyleRef().BorderBeforeStyle(),
                           Parent()->StyleRef().BorderBeforeWidth(),
                           Parent()->ResolveColor(before_color_property),
                           kBorderPrecedenceRow));
  if (!result.Exists())
    return result;

  // (4) The previous row's after border.
  if (cell_above) {
    LayoutObject* prev_row = nullptr;
    if (cell_above->Section() == Section())
      prev_row = Parent()->PreviousSibling();
    else
      prev_row = cell_above->Section()->LastRow();

    if (prev_row) {
      result = ChooseBorder(
          CollapsedBorderValue(prev_row->StyleRef().BorderAfterStyle(),
                               prev_row->StyleRef().BorderAfterWidth(),
                               prev_row->ResolveColor(after_color_property),
                               kBorderPrecedenceRow),
          result);
      if (!result.Exists())
        return result;
    }
  }

  // Now check row groups.
  LayoutTableSection* curr_section = Section();
  if (!RowIndex()) {
    // (5) Our row group's before border.
    result = ChooseBorder(
        result,
        CollapsedBorderValue(curr_section->StyleRef().BorderBeforeStyle(),
                             curr_section->StyleRef().BorderBeforeWidth(),
                             curr_section->ResolveColor(before_color_property),
                             kBorderPrecedenceRowGroup));
    if (!result.Exists())
      return result;

    // (6) Previous row group's after border.
    curr_section = table->SectionAbove(curr_section, kSkipEmptySections);
    if (curr_section) {
      result = ChooseBorder(
          CollapsedBorderValue(curr_section->StyleRef().BorderAfterStyle(),
                               curr_section->StyleRef().BorderAfterWidth(),
                               curr_section->ResolveColor(after_color_property),
                               kBorderPrecedenceRowGroup),
          result);
      if (!result.Exists())
        return result;
    }
  }

  if (!curr_section) {
    // (8) Our column and column group's before borders.
    LayoutTableCol* col_elt =
        table->ColElementAtAbsoluteColumn(AbsoluteColumnIndex())
            .InnermostColOrColGroup();
    if (col_elt) {
      result = ChooseBorder(
          result,
          CollapsedBorderValue(col_elt->StyleRef().BorderBeforeStyle(),
                               col_elt->StyleRef().BorderBeforeWidth(),
                               col_elt->ResolveColor(before_color_property),
                               kBorderPrecedenceColumn));
      if (!result.Exists())
        return result;
      if (LayoutTableCol* enclosing_column_group =
              col_elt->EnclosingColumnGroup()) {
        result = ChooseBorder(
            result,
            CollapsedBorderValue(
                enclosing_column_group->StyleRef().BorderBeforeStyle(),
                enclosing_column_group->StyleRef().BorderBeforeWidth(),
                enclosing_column_group->ResolveColor(before_color_property),
                kBorderPrecedenceColumnGroup));
        if (!result.Exists())
          return result;
      }
    }

    // (9) The table's before border.
    result = ChooseBorder(
        result, CollapsedBorderValue(table->StyleRef().BorderBeforeStyle(),
                                     table->StyleRef().BorderBeforeWidth(),
                                     table->ResolveColor(before_color_property),
                                     kBorderPrecedenceTable));
    if (!result.Exists())
      return result;
  }

  return result;
}

CollapsedBorderValue LayoutTableCell::ComputeCollapsedAfterBorder() const {
  LayoutTable* table = Table();
  LayoutTableCell* cell_below = table->CellBelow(*this);
  // We can use the border shared with |cell_below| if it is valid.
  if (StartsAtSameColumn(cell_below) &&
      cell_below->collapsed_border_values_valid_) {
    return cell_below->GetCollapsedBorderValues()
               ? cell_below->GetCollapsedBorderValues()->BeforeBorder()
               : CollapsedBorderValue();
  }

  // For after border, we need to check, in order of precedence:
  // (1) Our after border.
  const CSSProperty& before_color_property =
      ResolveBorderProperty(GetCSSPropertyBorderBlockStartColor());
  const CSSProperty& after_color_property =
      ResolveBorderProperty(GetCSSPropertyBorderBlockEndColor());
  CollapsedBorderValue result = CollapsedBorderValue(
      StyleRef().BorderAfterStyle(), StyleRef().BorderAfterWidth(),
      ResolveColor(after_color_property), kBorderPrecedenceCell);

  if (cell_below) {
    // (2) An after cell's before border.
    result = ChooseBorder(
        result,
        CollapsedBorderValue(cell_below->StyleRef().BorderBeforeStyle(),
                             cell_below->StyleRef().BorderBeforeWidth(),
                             cell_below->ResolveColor(before_color_property),
                             kBorderPrecedenceCell));
    if (!result.Exists())
      return result;
  }

  // (3) Our row's after border. (FIXME: Deal with rowspan!)
  result = ChooseBorder(
      result, CollapsedBorderValue(Parent()->StyleRef().BorderAfterStyle(),
                                   Parent()->StyleRef().BorderAfterWidth(),
                                   Parent()->ResolveColor(after_color_property),
                                   kBorderPrecedenceRow));
  if (!result.Exists())
    return result;

  // (4) The next row's before border.
  if (cell_below) {
    result = ChooseBorder(
        result, CollapsedBorderValue(
                    cell_below->Parent()->StyleRef().BorderBeforeStyle(),
                    cell_below->Parent()->StyleRef().BorderBeforeWidth(),
                    cell_below->Parent()->ResolveColor(before_color_property),
                    kBorderPrecedenceRow));
    if (!result.Exists())
      return result;
  }

  // Now check row groups.
  LayoutTableSection* curr_section = Section();
  if (RowIndex() + ResolvedRowSpan() >= curr_section->NumRows()) {
    // (5) Our row group's after border.
    result = ChooseBorder(
        result,
        CollapsedBorderValue(curr_section->StyleRef().BorderAfterStyle(),
                             curr_section->StyleRef().BorderAfterWidth(),
                             curr_section->ResolveColor(after_color_property),
                             kBorderPrecedenceRowGroup));
    if (!result.Exists())
      return result;

    // (6) Following row group's before border.
    curr_section = table->SectionBelow(curr_section, kSkipEmptySections);
    if (curr_section) {
      result = ChooseBorder(
          result, CollapsedBorderValue(
                      curr_section->StyleRef().BorderBeforeStyle(),
                      curr_section->StyleRef().BorderBeforeWidth(),
                      curr_section->ResolveColor(before_color_property),
                      kBorderPrecedenceRowGroup));
      if (!result.Exists())
        return result;
    }
  }

  if (!curr_section) {
    // (8) Our column and column group's after borders.
    LayoutTableCol* col_elt =
        table->ColElementAtAbsoluteColumn(AbsoluteColumnIndex())
            .InnermostColOrColGroup();
    if (col_elt) {
      result = ChooseBorder(
          result,
          CollapsedBorderValue(col_elt->StyleRef().BorderAfterStyle(),
                               col_elt->StyleRef().BorderAfterWidth(),
                               col_elt->ResolveColor(after_color_property),
                               kBorderPrecedenceColumn));
      if (!result.Exists())
        return result;
      if (LayoutTableCol* enclosing_column_group =
              col_elt->EnclosingColumnGroup()) {
        result = ChooseBorder(
            result,
            CollapsedBorderValue(
                enclosing_column_group->StyleRef().BorderAfterStyle(),
                enclosing_column_group->StyleRef().BorderAfterWidth(),
                enclosing_column_group->ResolveColor(after_color_property),
                kBorderPrecedenceColumnGroup));
        if (!result.Exists())
          return result;
      }
    }

    // (9) The table's after border.
    result = ChooseBorder(
        result, CollapsedBorderValue(table->StyleRef().BorderAfterStyle(),
                                     table->StyleRef().BorderAfterWidth(),
                                     table->ResolveColor(after_color_property),
                                     kBorderPrecedenceTable));
    if (!result.Exists())
      return result;
  }

  return result;
}

LayoutUnit LayoutTableCell::BorderLeft() const {
  return Table()->ShouldCollapseBorders()
             ? LayoutUnit(CollapsedBorderHalfLeft(false))
             : LayoutBlockFlow::BorderLeft();
}

LayoutUnit LayoutTableCell::BorderRight() const {
  return Table()->ShouldCollapseBorders()
             ? LayoutUnit(CollapsedBorderHalfRight(false))
             : LayoutBlockFlow::BorderRight();
}

LayoutUnit LayoutTableCell::BorderTop() const {
  return Table()->ShouldCollapseBorders()
             ? LayoutUnit(CollapsedBorderHalfTop(false))
             : LayoutBlockFlow::BorderTop();
}

LayoutUnit LayoutTableCell::BorderBottom() const {
  return Table()->ShouldCollapseBorders()
             ? LayoutUnit(CollapsedBorderHalfBottom(false))
             : LayoutBlockFlow::BorderBottom();
}

bool LayoutTableCell::IsFirstColumnCollapsed() const {
  if (!RuntimeEnabledFeatures::VisibilityCollapseColumnEnabled())
    return false;
  if (!HasSetAbsoluteColumnIndex())
    return false;
  return Table()->IsAbsoluteColumnCollapsed(AbsoluteColumnIndex());
}

void LayoutTableCell::UpdateCollapsedBorderValues() const {
  bool changed = false;

  if (!Table()->ShouldCollapseBorders()) {
    if (collapsed_border_values_) {
      changed = true;
      collapsed_border_values_ = nullptr;
    }
  } else {
    Table()->InvalidateCollapsedBordersForAllCellsIfNeeded();
    if (Section())
      Section()->RecalcCellsIfNeeded();
    if (collapsed_border_values_valid_)
      return;

    collapsed_border_values_valid_ = true;

    auto new_values = std::make_unique<CollapsedBorderValues>(
        ComputeCollapsedStartBorder(), ComputeCollapsedEndBorder(),
        ComputeCollapsedBeforeBorder(), ComputeCollapsedAfterBorder());

    // We need to save collapsed border if has a non-zero width even if it's
    // invisible because the width affects table layout.
    if (!new_values->HasNonZeroWidthBorder()) {
      if (collapsed_border_values_) {
        changed = true;
        collapsed_border_values_ = nullptr;
      }
    } else if (!collapsed_border_values_ ||
               !collapsed_border_values_->VisuallyEquals(*new_values)) {
      changed = true;
      collapsed_border_values_ = std::move(new_values);
    }
  }

  if (!changed && !collapsed_borders_need_paint_invalidation_)
    return;

  // Invalidate the rows which will paint the collapsed borders.
  auto row_span = ResolvedRowSpan();
  for (auto r = RowIndex(); r < RowIndex() + row_span; ++r) {
    if (auto* row = Section()->RowLayoutObjectAt(r))
      row->SetShouldDoFullPaintInvalidation(PaintInvalidationReason::kStyle);
  }
  collapsed_borders_need_paint_invalidation_ = false;
}

void LayoutTableCell::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) const {
  TableCellPainter(*this).PaintBoxDecorationBackground(paint_info,
                                                       paint_offset);
}

void LayoutTableCell::PaintMask(const PaintInfo& paint_info,
                                const PhysicalOffset& paint_offset) const {
  TableCellPainter(*this).PaintMask(paint_info, paint_offset);
}

void LayoutTableCell::ScrollbarsChanged(bool horizontal_scrollbar_changed,
                                        bool vertical_scrollbar_changed,
                                        ScrollbarChangeContext context) {
  LayoutBlock::ScrollbarsChanged(horizontal_scrollbar_changed,
                                 vertical_scrollbar_changed);

  // The intrinsic-padding adjustment for scrollbars is directly handled by NG.
  if (IsLayoutNGObject())
    return;

  if (context != kLayout)
    return;

  int scrollbar_height = ScrollbarLogicalHeight();
  // Not sure if we should be doing something when a scrollbar goes away or not.
  if (!scrollbar_height)
    return;

  // We only care if the scrollbar that affects our intrinsic padding has been
  // added.
  if ((IsHorizontalWritingMode() && !horizontal_scrollbar_changed) ||
      (!IsHorizontalWritingMode() && !vertical_scrollbar_changed))
    return;

  // Shrink our intrinsic padding as much as possible to accommodate the
  // scrollbar.
  if (StyleRef().VerticalAlign() == EVerticalAlign::kMiddle) {
    LayoutUnit total_height = LogicalHeight();
    LayoutUnit height_without_intrinsic_padding =
        total_height - IntrinsicPaddingBefore() - IntrinsicPaddingAfter();
    total_height -= scrollbar_height;
    LayoutUnit new_before_padding =
        (total_height - height_without_intrinsic_padding) / 2;
    LayoutUnit new_after_padding =
        total_height - height_without_intrinsic_padding - new_before_padding;
    SetIntrinsicPaddingBefore(new_before_padding.ToInt());
    SetIntrinsicPaddingAfter(new_after_padding.ToInt());
  } else {
    SetIntrinsicPaddingAfter(IntrinsicPaddingAfter() - scrollbar_height);
  }
}

LayoutTableCell* LayoutTableCell::CreateAnonymous(
    Document* document,
    scoped_refptr<ComputedStyle> style,
    LegacyLayout legacy) {
  LayoutTableCell* layout_object =
      LayoutObjectFactory::CreateTableCell(*document, *style, legacy);
  layout_object->SetDocumentForAnonymous(document);
  layout_object->SetStyle(std::move(style));
  return layout_object;
}

LayoutTableCell* LayoutTableCell::CreateAnonymousWithParent(
    const LayoutObject* parent) {
  scoped_refptr<ComputedStyle> new_style =
      ComputedStyle::CreateAnonymousStyleWithDisplay(parent->StyleRef(),
                                                     EDisplay::kTableCell);
  LegacyLayout legacy =
      parent->ForceLegacyLayout() ? LegacyLayout::kForce : LegacyLayout::kAuto;
  LayoutTableCell* new_cell = LayoutTableCell::CreateAnonymous(
      &parent->GetDocument(), std::move(new_style), legacy);
  return new_cell;
}

bool LayoutTableCell::BackgroundIsKnownToBeOpaqueInRect(
    const PhysicalRect& local_rect) const {
  // If this object has layer, the area of collapsed borders should be
  // transparent to expose the collapsed borders painted on the underlying
  // layer.
  if (HasLayer() && Table()->ShouldCollapseBorders())
    return false;
  return LayoutBlockFlow::BackgroundIsKnownToBeOpaqueInRect(local_rect);
}

bool LayoutTableCell::HasLineIfEmpty() const {
  if (GetNode() && HasEditableStyle(*GetNode()))
    return true;

  return LayoutBlock::HasLineIfEmpty();
}

void LayoutTableCell::InvalidatePaint(
    const PaintInvalidatorContext& context) const {
  TableCellPaintInvalidator(*this, context).InvalidatePaint();
}

}  // namespace blink
