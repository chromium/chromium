/*
 * Copyright (C) 2002 Lars Knoll (knoll@kde.org)
 *           (C) 2002 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License.
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

#include "third_party/blink/renderer/core/layout/table_layout_algorithm_fixed.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_table_col.h"
#include "third_party/blink/renderer/core/layout/layout_table_section.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

/*
  The text below is from the CSS 2.1 specs.

  Fixed table layout

  With this (fast) algorithm, the horizontal layout of the table does
  not depend on the contents of the cells; it only depends on the
  table's width, the width of the columns, and borders or cell
  spacing.

  The table's width may be specified explicitly with the 'width'
  property. A value of 'auto' (for both 'display: table' and 'display:
  inline-table') means use the automatic table layout algorithm.

  In the fixed table layout algorithm, the width of each column is
  determined as follows:

    1. A column element with a value other than 'auto' for the 'width'
    property sets the width for that column.

    2. Otherwise, a cell in the first row with a value other than
    'auto' for the 'width' property sets the width for that column. If
    the cell spans more than one column, the width is divided over the
    columns.

    3. Any remaining columns equally divide the remaining horizontal
    table space (minus borders or cell spacing).

  The width of the table is then the greater of the value of the
  'width' property for the table element and the sum of the column
  widths (plus cell spacing or borders). If the table is wider than
  the columns, the extra space should be distributed over the columns.


  In this manner, the user agent can begin to lay out the table once
  the entire first row has been received. Cells in subsequent rows do
  not affect column widths. Any cell that has content that overflows
  uses the 'overflow' property to determine whether to clip the
  overflow content.
*/

namespace blink {

TableLayoutAlgorithmFixed::TableLayoutAlgorithmFixed(LayoutTable* table)
    : TableLayoutAlgorithm(table), recorded_width_difference_(false) {}

int TableLayoutAlgorithmFixed::CalcWidthArray() {
  // FIXME: We might want to wait until we have all of the first row before
  // computing for the first time.
  int used_width = 0;

  // iterate over all <col> elements
  unsigned n_eff_cols = table_->NumEffectiveColumns();
  width_.resize(n_eff_cols);
  width_.Fill(Length::Auto());

  unsigned current_effective_column = 0;
  for (LayoutTableCol* col = table_->FirstColumn(); col;
       col = col->NextColumn()) {
    // LayoutTableCols don't have the concept of preferred logical width, but we
    // need to clear their dirty bits so that if we call
    // setPreferredWidthsDirty(true) on a col or one of its descendants, we'll
    // mark it's ancestors as dirty.
    col->ClearPreferredLogicalWidthsDirtyBits();

    // Width specified by column-groups that have column child does not affect
    // column width in fixed layout tables
    if (col->IsTableColumnGroupWithColumnChildren())
      continue;

    const Length& col_style_logical_width = col->StyleRef().LogicalWidth();
    int effective_col_width = 0;
    if (col_style_logical_width.IsFixed() &&
        col_style_logical_width.Value() > 0)
      effective_col_width = col_style_logical_width.Value();

    unsigned span = col->Span();
    while (span) {
      unsigned span_in_current_effective_column;
      if (current_effective_column >= n_eff_cols) {
        table_->AppendEffectiveColumn(span);
        n_eff_cols++;
        width_.push_back(Length());
        span_in_current_effective_column = span;
      } else {
        if (span < table_->SpanOfEffectiveColumn(current_effective_column)) {
          table_->SplitEffectiveColumn(current_effective_column, span);
          n_eff_cols++;
          width_.push_back(Length());
        }
        span_in_current_effective_column =
            table_->SpanOfEffectiveColumn(current_effective_column);
      }
      // TODO(alancutter): Make this work correctly for calc lengths.
      if ((col_style_logical_width.IsFixed() ||
           col_style_logical_width.IsPercent()) &&
          col_style_logical_width.IsPositive()) {
        width_[current_effective_column] = col_style_logical_width;
        width_[current_effective_column] *= span_in_current_effective_column;
        used_width += effective_col_width * span_in_current_effective_column;
      }
      span -= span_in_current_effective_column;
      current_effective_column++;
    }
  }

  // Iterate over the first row in case some are unspecified.
  LayoutTableSection* section = table_->TopNonEmptySection();
  if (!section)
    return used_width;

  unsigned current_column = 0;

  LayoutTableRow* first_row = section->FirstRow();
  for (LayoutTableCell* cell = first_row->FirstCell(); cell;
       cell = cell->NextCell()) {
    Length logical_width = cell->StyleOrColLogicalWidth();

    if (logical_width.IsCalculated()) {
      // A calculated width that mixes lengths and percentages in fixed table
      // layout must be treated as 'auto'.
      // https://drafts.csswg.org/css-values-4/#calc-computed-value
      const CalculationValue& calc = logical_width.GetCalculationValue();
      if (calc.IsExpression() || calc.Pixels())
        logical_width = Length();
      else
        logical_width = Length::Percent(calc.Percent());
    }

    unsigned span = cell->ColSpan();
    int fixed_border_box_logical_width = 0;
    // FIXME: Support other length types. If the width is non-auto, it should
    // probably just use LayoutBox::computeLogicalWidthUsing to compute the
    // width.
    if (logical_width.IsFixed() && logical_width.IsPositive()) {
      fixed_border_box_logical_width =
          cell->AdjustBorderBoxLogicalWidthForBoxSizing(logical_width.Value())
              .ToInt();
      logical_width = Length::Fixed(fixed_border_box_logical_width);
    }

    unsigned used_span = 0;
    while (used_span < span && current_column < n_eff_cols) {
      float e_span = table_->SpanOfEffectiveColumn(current_column);
      // Only set if no col element has already set it.
      if (width_[current_column].IsAuto() && !logical_width.IsAuto()) {
        width_[current_column] = logical_width;
        width_[current_column] *= e_span / span;
        used_width += fixed_border_box_logical_width * e_span / span;
      }
      used_span += e_span;
      ++current_column;
    }

    // TableLayoutAlgorithmFixed doesn't use min/maxPreferredLogicalWidths, but
    // we need to clear the dirty bit on the cell so that we'll correctly mark
    // its ancestors dirty in case we later call
    // setPreferredLogicalWidthsDirty() on it later.
    if (cell->PreferredLogicalWidthsDirty())
      cell->ClearPreferredLogicalWidthsDirty();
  }

  return used_width;
}

void TableLayoutAlgorithmFixed::ComputeIntrinsicLogicalWidths(
    LayoutUnit& min_width,
    LayoutUnit& max_width) {
  min_width = max_width = LayoutUnit(CalcWidthArray());
}

void TableLayoutAlgorithmFixed::ApplyPreferredLogicalWidthQuirks(
    LayoutUnit& min_width,
    LayoutUnit& max_width) const {
  const Length& table_logical_width = table_->StyleRef().LogicalWidth();
  if (table_logical_width.IsFixed() && table_logical_width.IsPositive()) {
    min_width = max_width = LayoutUnit(
        max(min_width,
            LayoutUnit(table_logical_width.Value() -
                       table_->BordersPaddingAndSpacingInRowDirection()))
            .Floor());
  }

  /*
        <table style="width:100%; background-color:red"><tr><td>
            <table style="background-color:blue"><tr><td>
                <table style="width:100%; background-color:green;
                              table-layout:fixed"><tr><td>
                    Content
                </td></tr></table>
            </td></tr></table>
        </td></tr></table>
    */
  // In this example, the two inner tables should be as large as the outer
  // table. We can achieve this effect by making the maxwidth of fixed tables
  // with percentage widths be infinite.
  if (table_->StyleRef().LogicalWidth().IsPercentOrCalc() &&
      max_width < kTableMaxWidth)
    max_width = LayoutUnit(kTableMaxWidth);
}

void TableLayoutAlgorithmFixed::UpdateLayout() {
  int table_logical_width = (table_->LogicalWidth() -
                             table_->BordersPaddingAndSpacingInRowDirection())
                                .ToInt();
  unsigned n_eff_cols = table_->NumEffectiveColumns();

  // FIXME: It is possible to be called without having properly updated our
  // internal representation. This means that our preferred logical widths were
  // not recomputed as expected.
  if (n_eff_cols != width_.size()) {
    CalcWidthArray();
    // FIXME: Table layout shouldn't modify our table structure (but does due to
    // columns and column-groups).
    n_eff_cols = table_->NumEffectiveColumns();
  }

  Vector<int> calc_width(n_eff_cols, 0);

  unsigned num_auto = 0;
  unsigned auto_span = 0;
  int total_fixed_width = 0;
  int total_percent_width = 0;
  float total_percent = 0;

  // Compute requirements and try to satisfy fixed and percent widths.
  // Percentages are of the table's width, so for example
  // for a table width of 100px with columns (40px, 10%), the 10% compute
  // to 10px here, and will scale up to 20px in the final (80px, 20px).
  for (unsigned i = 0; i < n_eff_cols; i++) {
    if (width_[i].IsFixed()) {
      calc_width[i] = width_[i].Value();
      total_fixed_width += calc_width[i];
    } else if (width_[i].IsPercent()) {
      // TODO(alancutter): Make this work correctly for calc lengths.
      calc_width[i] =
          ValueForLength(width_[i], LayoutUnit(table_logical_width)).ToInt();
      total_percent_width += calc_width[i];
      total_percent += width_[i].Percent();
    } else if (width_[i].IsAuto()) {
      num_auto++;
      auto_span += table_->SpanOfEffectiveColumn(i);
    }
  }

  int16_t hspacing = table_->HBorderSpacing();
  int total_width = total_fixed_width + total_percent_width;
  if (!num_auto || total_width > table_logical_width) {
    // If there are no auto columns, or if the total is too wide, take
    // what we have and scale it to fit as necessary.
    if (total_width != table_logical_width) {
      // Fixed widths only scale up
      if (total_fixed_width && total_width < table_logical_width) {
        int first_pass_fixed_width_total = total_fixed_width;
        total_fixed_width = 0;
        int width_available_for_fixed =
            table_logical_width - total_percent_width;
        for (unsigned i = 0; i < n_eff_cols; i++) {
          if (width_[i].IsFixed()) {
            int legacy_expanded_width =
                calc_width[i] * table_logical_width / total_width;
            if (!recorded_width_difference_) {
              int future_expanded_width = calc_width[i] *
                                          width_available_for_fixed /
                                          first_pass_fixed_width_total;
              if (future_expanded_width != legacy_expanded_width) {
                recorded_width_difference_ = true;
                UseCounter::Count(
                    table_->GetDocument(),
                    WebFeature::kFixedWidthTableDistributionChanged);
              }
            }
            calc_width[i] = legacy_expanded_width;
            total_fixed_width += calc_width[i];
          }
        }
      }
      if (total_percent) {
        total_percent_width = 0;
        for (unsigned i = 0; i < n_eff_cols; i++) {
          // TODO(alancutter): Make this work correctly for calc lengths.
          if (width_[i].IsPercent()) {
            calc_width[i] = width_[i].Percent() *
                            (table_logical_width - total_fixed_width) /
                            total_percent;
            total_percent_width += calc_width[i];
          }
        }
      }
      total_width = total_fixed_width + total_percent_width;
    }
  } else {
    // Divide the remaining width among the auto columns.
    DCHECK_GE(auto_span, num_auto);
    int remaining_width = table_logical_width - total_fixed_width -
                          total_percent_width -
                          hspacing * (auto_span - num_auto);
    int last_auto = 0;
    for (unsigned i = 0; i < n_eff_cols; i++) {
      if (width_[i].IsAuto()) {
        unsigned span = table_->SpanOfEffectiveColumn(i);
        int w = remaining_width * span / auto_span;
        calc_width[i] = std::max<int>((w + hspacing * (span - 1)), 0);
        remaining_width -= w;
        if (!remaining_width)
          break;
        last_auto = i;
        DCHECK_GE(auto_span, span);
        auto_span -= span;
      }
    }
    // Last one gets the remainder.
    if (remaining_width) {
      calc_width[last_auto] =
          std::max(calc_width[last_auto] + remaining_width, 0);
    }
    total_width = table_logical_width;
  }

  if (total_width < table_logical_width) {
    // Spread extra space over columns.
    int remaining_width = table_logical_width - total_width;
    int total = n_eff_cols;
    while (total) {
      int w = remaining_width / total;
      remaining_width -= w;
      calc_width[--total] += w;
    }
    if (n_eff_cols > 0)
      calc_width[n_eff_cols - 1] += remaining_width;
  }

  DCHECK_EQ(table_->EffectiveColumnPositions().size(), n_eff_cols + 1);
  int pos = 0;
  for (unsigned i = 0; i < n_eff_cols; i++) {
    table_->SetEffectiveColumnPosition(i, pos);
    pos += calc_width[i] + hspacing;
  }
  // The extra position is for the imaginary column after the last column.
  table_->SetEffectiveColumnPosition(n_eff_cols, pos);
}

void TableLayoutAlgorithmFixed::WillChangeTableLayout() {
  // When switching table layout algorithm, we need to dirty the preferred
  // logical widths as we cleared the bits without computing them.
  // (see calcWidthArray above.) This optimization is preferred to always
  // computing the logical widths we never intended to use.
  table_->RecalcSectionsIfNeeded();
  table_->MarkAllCellsWidthsDirtyAndOrNeedsLayout(LayoutTable::kMarkDirtyOnly);
}

}  // namespace blink
