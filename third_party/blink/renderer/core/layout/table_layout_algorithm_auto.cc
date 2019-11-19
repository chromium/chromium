/*
 * Copyright (C) 2002 Lars Knoll (knoll@kde.org)
 *           (C) 2002 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2008, 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/table_layout_algorithm_auto.h"

#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_table_col.h"
#include "third_party/blink/renderer/core/layout/layout_table_section.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell_interface.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_interface.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"

namespace blink {

TableLayoutAlgorithmAuto::TableLayoutAlgorithmAuto(LayoutTable* table)
    : TableLayoutAlgorithm(table),
      has_percent_(false),
      effective_logical_width_dirty_(true),
      scaled_width_from_percent_columns_() {}

TableLayoutAlgorithmAuto::~TableLayoutAlgorithmAuto() = default;

void TableLayoutAlgorithmAuto::RecalcColumn(unsigned eff_col) {
  Layout& column_layout = layout_struct_[eff_col];

  LayoutTableCell* fixed_contributor = nullptr;
  LayoutTableCell* max_contributor = nullptr;

  for (LayoutObject* child = table_->Children()->FirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsLayoutTableCol()) {
      // LayoutTableCols don't have the concept of preferred logical width, but
      // we need to clear their dirty bits so that if we call
      // setPreferredWidthsDirty(true) on a col or one of its descendants, we'll
      // mark it's ancestors as dirty.
      ToLayoutTableCol(child)->ClearPreferredLogicalWidthsDirtyBits();
    } else if (child->IsTableSection()) {
      LayoutTableSection* section = To<LayoutTableSection>(child);
      unsigned num_rows = section->NumRows();
      for (unsigned i = 0; i < num_rows; i++) {
        if (eff_col >= section->NumCols(i))
          continue;
        auto& grid_cell = section->GridCellAt(i, eff_col);
        LayoutTableCell* cell = grid_cell.PrimaryCell();

        if (grid_cell.InColSpan() || !cell)
          continue;
        column_layout.column_has_no_cells = false;

        if (cell->MaxPreferredLogicalWidth())
          column_layout.empty_cells_only = false;

        if (cell->ColSpan() == 1) {
          column_layout.min_logical_width =
              std::max<int>(cell->MinPreferredLogicalWidth().ToInt(),
                            column_layout.min_logical_width);
          if (cell->MaxPreferredLogicalWidth() >
              column_layout.max_logical_width) {
            column_layout.max_logical_width =
                cell->MaxPreferredLogicalWidth().ToInt();
            max_contributor = cell;
          }

          // All browsers implement a size limit on the cell's max width.
          // Our limit is based on KHTML's representation that used 16 bits
          // widths.
          // FIXME: Other browsers have a lower limit for the cell's max width.
          const int kCCellMaxWidth = 32760;
          Length cell_logical_width = cell->StyleOrColLogicalWidth();
          // A calculated width that mixes lengths and percentages in fixed
          // table layout must be treated as 'auto'.
          // https://drafts.csswg.org/css-values-4/#calc-computed-value
          if (cell_logical_width.IsCalculated()) {
            const CalculationValue& calc =
                cell_logical_width.GetCalculationValue();
            if (!calc.IsExpression() && !calc.Pixels()) {
              cell_logical_width = Length::Percent(calc.Percent());
            } else {
              cell_logical_width = Length();  // Make it Auto
            }
          }
          if (cell_logical_width.Value() > kCCellMaxWidth)
            cell_logical_width = Length::Fixed(kCCellMaxWidth);
          if (cell_logical_width.IsNegative())
            cell_logical_width = Length::Fixed(0);
          switch (cell_logical_width.GetType()) {
            case Length::kFixed:
              // ignore width=0
              if (cell_logical_width.IsPositive() &&
                  !column_layout.logical_width.IsPercentOrCalc()) {
                int logical_width =
                    cell->AdjustBorderBoxLogicalWidthForBoxSizing(
                            cell_logical_width.Value())
                        .ToInt();
                if (column_layout.logical_width.IsFixed()) {
                  // Nav/IE weirdness
                  if ((logical_width > column_layout.logical_width.Value()) ||
                      ((column_layout.logical_width.Value() == logical_width) &&
                       (max_contributor == cell))) {
                    column_layout.logical_width = Length::Fixed(logical_width);
                    fixed_contributor = cell;
                  }
                } else {
                  column_layout.logical_width = Length::Fixed(logical_width);
                  fixed_contributor = cell;
                }
              }
              break;
            case Length::kPercent:
              has_percent_ = true;
              // TODO(alancutter): Make this work correctly for calc lengths.
              if (cell_logical_width.IsPositive() &&
                  (!column_layout.logical_width.IsPercentOrCalc() ||
                   cell_logical_width.Value() >
                       column_layout.logical_width.Value()))
                column_layout.logical_width = cell_logical_width;
              break;
            default:
              break;
          }
        } else if (!eff_col || section->PrimaryCellAt(i, eff_col - 1) != cell) {
          // If a cell originates in this spanning column ensure we have a
          // min/max width of at least 1px for it.
          column_layout.min_logical_width =
              std::max<int>(column_layout.min_logical_width,
                            cell->MaxPreferredLogicalWidth() ? 1 : 0);

          // This spanning cell originates in this column. Insert the cell into
          // spanning cells list.
          InsertSpanCell(cell);
        }
      }
    }
  }

  // Nav/IE weirdness
  if (column_layout.logical_width.IsFixed()) {
    if (table_->GetDocument().InQuirksMode() &&
        column_layout.max_logical_width > column_layout.logical_width.Value() &&
        fixed_contributor != max_contributor) {
      column_layout.logical_width = Length();
      fixed_contributor = nullptr;
    }
  }

  column_layout.max_logical_width = std::max(column_layout.max_logical_width,
                                             column_layout.min_logical_width);
}

void TableLayoutAlgorithmAuto::FullRecalc() {
  has_percent_ = false;
  effective_logical_width_dirty_ = true;

  unsigned n_eff_cols = table_->NumEffectiveColumns();
  layout_struct_.resize(n_eff_cols);
  layout_struct_.Fill(Layout());
  span_cells_.Fill(0);

  Length group_logical_width;
  unsigned current_column = 0;
  for (LayoutTableCol* column = table_->FirstColumn(); column;
       column = column->NextColumn()) {
    if (column->IsTableColumnGroupWithColumnChildren()) {
      group_logical_width = column->StyleRef().LogicalWidth();
    } else {
      Length col_logical_width = column->StyleRef().LogicalWidth();
      if (col_logical_width.IsCalculated() || col_logical_width.IsAuto())
        col_logical_width = group_logical_width;
      // TODO(alancutter): Make this work correctly for calc lengths.
      if ((col_logical_width.IsFixed() ||
           col_logical_width.IsPercentOrCalc()) &&
          col_logical_width.IsZero())
        col_logical_width = Length();
      unsigned eff_col =
          table_->AbsoluteColumnToEffectiveColumn(current_column);
      unsigned span = column->Span();
      if (!col_logical_width.IsAuto() && span == 1 && eff_col < n_eff_cols &&
          table_->SpanOfEffectiveColumn(eff_col) == 1) {
        layout_struct_[eff_col].logical_width = col_logical_width;
        if (col_logical_width.IsFixed() &&
            layout_struct_[eff_col].max_logical_width <
                col_logical_width.Value())
          layout_struct_[eff_col].max_logical_width = col_logical_width.Value();
      }
      current_column += span;
    }

    // For the last column in a column-group, we invalidate our group logical
    // width.
    if (column->IsTableColumn() && !column->NextSibling())
      group_logical_width = Length();
  }

  for (unsigned i = 0; i < n_eff_cols; i++)
    RecalcColumn(i);
}

static bool ShouldScaleColumnsForParent(LayoutTable* table) {
  LayoutBlock* cb = table->ContainingBlock();
  // TODO(layout-dev): We can probably abort before reaching LayoutView in many
  // cases. For example, if we find an object with contain:size, or even if we
  // find a regular block with fixed logical width.
  while (!cb->IsLayoutView()) {
    // It doesn't matter if our table is auto or fixed: auto means we don't
    // scale. Fixed doesn't care if we do or not because it doesn't depend
    // on the cell contents' preferred widths.
    if (cb->IsTableCell())
      return false;
    // The max logical width of a table may be "infinity" (or kTableMaxWidth, to
    // be more exact) if the sum of the columns' percentages is 100% or more,
    // AND there is at least one column that has a non-percentage-based positive
    // logical width. In such situations no table logical width will be large
    // enough to satisfy the constraint set by the contents. So the idea is to
    // use ~infinity to make sure we use all available size in the containing
    // block. However, this just doesn't work if this is a flex or grid item, so
    // disallow scaling in that case.
    const bool is_deprecated_webkit_box =
        cb->StyleRef().IsDeprecatedWebkitBox();
    if ((!is_deprecated_webkit_box && cb->IsFlexibleBoxIncludingNG()) ||
        cb->IsLayoutGrid()) {
      return false;
    }
    cb = cb->ContainingBlock();
  }
  return true;
}

// FIXME: This needs to be adapted for vertical writing modes.
static bool ShouldScaleColumnsForSelf(LayoutNGTableInterface* table) {
  // Normally, scale all columns to satisfy this from CSS2.2:
  // "A percentage value for a column width is relative to the table width.
  // If the table has 'width: auto', a percentage represents a constraint on the
  // column's width"

  // A special case.  If this table is not fixed width and contained inside
  // a cell, then don't bloat the maxwidth by examining percentage growth.
  while (true) {
    const LayoutObject* layout_table = table->ToLayoutObject();
    const Length& tw = layout_table->StyleRef().Width();
    if ((!tw.IsAuto() && !tw.IsPercentOrCalc()) ||
        layout_table->IsOutOfFlowPositioned())
      return true;
    LayoutBlock* cb = layout_table->ContainingBlock();

    while (!cb->IsLayoutView() && !cb->IsTableCell() &&
           cb->StyleRef().Width().IsAuto() && !cb->IsOutOfFlowPositioned())
      cb = cb->ContainingBlock();

    // TODO(dgrogan): Should the second clause check for isFixed() instead?
    if (!cb->IsTableCell() || (!cb->StyleRef().Width().IsAuto() &&
                               !cb->StyleRef().Width().IsPercentOrCalc()))
      return true;

    LayoutNGTableCellInterface* cell =
        ToInterface<LayoutNGTableCellInterface>(cb);
    table = cell->TableInterface();
    const Length& table_logical_width =
        table->ToLayoutObject()->StyleRef().LogicalWidth();
    bool width_is_auto = (!table_logical_width.IsSpecified() ||
                          !table_logical_width.IsPositive()) &&
                         !table_logical_width.IsIntrinsic();
    if (cell->ColSpan() > 1 || width_is_auto)
      return false;
  }
  NOTREACHED();
  return true;
}

void TableLayoutAlgorithmAuto::ComputeIntrinsicLogicalWidths(
    LayoutUnit& min_width,
    LayoutUnit& max_width) {
  TextAutosizer::TableLayoutScope text_autosizer_table_layout_scope(table_);

  FullRecalc();

  int span_max_logical_width = CalcEffectiveLogicalWidth();
  min_width = LayoutUnit();
  max_width = LayoutUnit();
  float max_percent = 0;
  float max_non_percent = 0;
  bool scale_columns_for_self = ShouldScaleColumnsForSelf(table_);

  float remaining_percent = 100;
  for (wtf_size_t i = 0; i < layout_struct_.size(); ++i) {
    min_width += layout_struct_[i].effective_min_logical_width;
    max_width += layout_struct_[i].effective_max_logical_width;
    if (scale_columns_for_self) {
      if (layout_struct_[i].effective_logical_width.IsPercentOrCalc()) {
        float percent =
            std::min(static_cast<float>(
                         layout_struct_[i].effective_logical_width.Percent()),
                     remaining_percent);
        // When percent columns meet or exceed 100% and there are remaining
        // columns, the other browsers (FF, Edge) use an artificially high max
        // width, so we do too. Instead of division by zero, logical_width and
        // max_non_percent are set to kTableMaxWidth. Issue:
        // https://github.com/w3c/csswg-drafts/issues/1501
        float logical_width =
            (percent > 0) ? static_cast<float>(
                                layout_struct_[i].effective_max_logical_width) *
                                100 / percent
                          : kTableMaxWidth;
        max_percent = std::max(logical_width, max_percent);
        remaining_percent -= percent;
      } else {
        max_non_percent += layout_struct_[i].effective_max_logical_width;
      }
    }
  }

  if (scale_columns_for_self) {
    if (max_non_percent != 0) {
      max_non_percent = (remaining_percent > 0)
                            ? max_non_percent * 100 / remaining_percent
                            : kTableMaxWidth;
    }
    scaled_width_from_percent_columns_ =
        std::min(LayoutUnit(kTableMaxWidth),
                 LayoutUnit(std::max(max_percent, max_non_percent)));
    if (scaled_width_from_percent_columns_ > max_width &&
        ShouldScaleColumnsForParent(table_))
      max_width = scaled_width_from_percent_columns_;
  }

  max_width = LayoutUnit(std::max(max_width.Floor(), span_max_logical_width));
}

void TableLayoutAlgorithmAuto::ApplyPreferredLogicalWidthQuirks(
    LayoutUnit& min_width,
    LayoutUnit& max_width) const {
  const Length& table_logical_width = table_->StyleRef().LogicalWidth();
  if (table_logical_width.IsFixed() && table_logical_width.IsPositive()) {
    // |minWidth| is the result of measuring the intrinsic content's size. Keep
    // it to make sure we are *never* smaller than the actual content.
    LayoutUnit min_content_width = min_width;
    // FIXME: This line looks REALLY suspicious as it could allow the minimum
    // preferred logical width to be smaller than the table content. This has
    // to be cross-checked against other browsers.
    min_width = max_width = LayoutUnit(
        std::max<int>(min_width.Floor(), table_logical_width.Value()));

    const Length& style_max_logical_width =
        table_->StyleRef().LogicalMaxWidth();
    if (style_max_logical_width.IsFixed() &&
        !style_max_logical_width.IsNegative()) {
      min_width = LayoutUnit(
          std::min<int>(min_width.Floor(), style_max_logical_width.Value()));
      min_width = std::max(min_width, min_content_width);
      max_width = min_width;
    }
  }
}

/*
  This method takes care of colspans.
  effWidth is the same as width for cells without colspans. If we have colspans,
  they get modified.
 */
int TableLayoutAlgorithmAuto::CalcEffectiveLogicalWidth() {
  int max_logical_width = 0;

  wtf_size_t n_eff_cols = layout_struct_.size();
  int16_t spacing_in_row_direction = table_->HBorderSpacing();

  for (wtf_size_t i = 0; i < n_eff_cols; ++i) {
    layout_struct_[i].effective_logical_width = layout_struct_[i].logical_width;
    layout_struct_[i].effective_min_logical_width =
        layout_struct_[i].min_logical_width;
    layout_struct_[i].effective_max_logical_width =
        layout_struct_[i].max_logical_width;
  }

  for (wtf_size_t i = 0; i < span_cells_.size(); ++i) {
    LayoutTableCell* cell = span_cells_[i];
    if (!cell)
      break;

    unsigned span = cell->ColSpan();

    Length cell_logical_width = cell->StyleOrColLogicalWidth();
    if (cell_logical_width.IsZero() || cell_logical_width.IsCalculated())
      cell_logical_width = Length();  // Make it Auto

    unsigned eff_col =
        table_->AbsoluteColumnToEffectiveColumn(cell->AbsoluteColumnIndex());
    wtf_size_t last_col = eff_col;
    int cell_min_logical_width =
        (cell->MinPreferredLogicalWidth() + spacing_in_row_direction).ToInt();
    int cell_max_logical_width =
        (cell->MaxPreferredLogicalWidth() + spacing_in_row_direction).ToInt();
    float total_percent = 0;
    int span_min_logical_width = 0;
    int span_max_logical_width = 0;
    bool all_cols_are_percent = true;
    bool all_cols_are_fixed = true;
    bool have_auto = false;
    bool span_has_empty_cells_only = true;
    int fixed_width = 0;
    while (last_col < n_eff_cols && span > 0) {
      Layout& column_layout = layout_struct_[last_col];
      switch (column_layout.logical_width.GetType()) {
        case Length::kPercent:
          total_percent += column_layout.logical_width.Percent();
          all_cols_are_fixed = false;
          break;
        case Length::kFixed:
          if (column_layout.logical_width.Value() > 0) {
            fixed_width += column_layout.logical_width.Value();
            all_cols_are_percent = false;
            // IE resets effWidth to Auto here, but this breaks the konqueror
            // about page and seems to be some bad legacy behaviour anyway.
            // mozilla doesn't do this so I decided we don't neither.
            break;
          }
          FALLTHROUGH;
        case Length::kAuto:
          have_auto = true;
          FALLTHROUGH;
        default:
          // If the column is a percentage width, do not let the spanning cell
          // overwrite the width value.  This caused a mis-layout on amazon.com.
          // Sample snippet:
          // <table border=2 width=100%><
          //   <tr><td>1</td><td colspan=2>2-3</tr>
          //   <tr><td>1</td><td colspan=2 width=100%>2-3</td></tr>
          // </table>
          // TODO(alancutter): Make this work correctly for calc lengths.
          if (!column_layout.effective_logical_width.IsPercentOrCalc()) {
            column_layout.effective_logical_width = Length();
            all_cols_are_percent = false;
          } else {
            total_percent += column_layout.effective_logical_width.Percent();
          }
          all_cols_are_fixed = false;
      }
      if (!column_layout.empty_cells_only)
        span_has_empty_cells_only = false;
      span -= table_->SpanOfEffectiveColumn(last_col);
      span_min_logical_width += column_layout.effective_min_logical_width;
      span_max_logical_width += column_layout.effective_max_logical_width;
      last_col++;
      cell_min_logical_width -= spacing_in_row_direction;
      cell_max_logical_width -= spacing_in_row_direction;
    }

    // adjust table max width if needed
    if (cell_logical_width.IsPercentOrCalc()) {
      if (total_percent > cell_logical_width.Percent() ||
          all_cols_are_percent) {
        // can't satify this condition, treat as variable
        cell_logical_width = Length();
      } else {
        max_logical_width =
            std::max(max_logical_width,
                     static_cast<int>(std::max(span_max_logical_width,
                                               cell_max_logical_width) *
                                      100 / cell_logical_width.Percent()));

        // all non percent columns in the span get percent values to sum up
        // correctly.
        float percent_missing = cell_logical_width.Percent() - total_percent;
        int total_width = 0;
        for (unsigned pos = eff_col; pos < last_col; ++pos) {
          if (!layout_struct_[pos].effective_logical_width.IsPercentOrCalc())
            total_width +=
                layout_struct_[pos].ClampedEffectiveMaxLogicalWidth();
        }

        for (unsigned pos = eff_col; pos < last_col && total_width > 0; ++pos) {
          if (!layout_struct_[pos].effective_logical_width.IsPercentOrCalc()) {
            float percent =
                percent_missing *
                static_cast<float>(
                    layout_struct_[pos].effective_max_logical_width) /
                total_width;
            total_width -=
                layout_struct_[pos].ClampedEffectiveMaxLogicalWidth();
            percent_missing -= percent;
            layout_struct_[pos].effective_logical_width =
                percent > 0 ? Length::Percent(percent) : Length();
          }
        }
      }
    }

    // make sure minWidth and maxWidth of the spanning cell are honoured
    if (cell_min_logical_width > span_min_logical_width) {
      if (all_cols_are_fixed) {
        for (unsigned pos = eff_col; fixed_width > 0 && pos < last_col; ++pos) {
          int cell_logical_width = std::max(
              layout_struct_[pos].effective_min_logical_width,
              static_cast<int>(cell_min_logical_width *
                               layout_struct_[pos].logical_width.Value() /
                               fixed_width));
          fixed_width -= layout_struct_[pos].logical_width.Value();
          cell_min_logical_width -= cell_logical_width;
          layout_struct_[pos].effective_min_logical_width = cell_logical_width;
        }
      } else if (all_cols_are_percent) {
        // In this case, we just split the colspan's min amd max widths
        // following the percentage.
        int allocated_min_logical_width = 0;
        int allocated_max_logical_width = 0;
        for (unsigned pos = eff_col; pos < last_col; ++pos) {
          // TODO(alancutter): Make this work correctly for calc lengths.
          DCHECK(layout_struct_[pos].logical_width.IsPercentOrCalc() ||
                 layout_struct_[pos].effective_logical_width.IsPercentOrCalc());
          // |allColsArePercent| means that either the logicalWidth *or* the
          // effectiveLogicalWidth are percents, handle both of them here.
          float percent =
              layout_struct_[pos].logical_width.IsPercentOrCalc()
                  ? layout_struct_[pos].logical_width.Percent()
                  : layout_struct_[pos].effective_logical_width.Percent();
          int column_min_logical_width = static_cast<int>(
              percent * cell_min_logical_width / total_percent);
          int column_max_logical_width = static_cast<int>(
              percent * cell_max_logical_width / total_percent);
          layout_struct_[pos].effective_min_logical_width =
              std::max(layout_struct_[pos].effective_min_logical_width,
                       column_min_logical_width);
          layout_struct_[pos].effective_max_logical_width =
              column_max_logical_width;
          allocated_min_logical_width += column_min_logical_width;
          allocated_max_logical_width += column_max_logical_width;
        }
        DCHECK_LE(allocated_min_logical_width, cell_min_logical_width);
        DCHECK_LE(allocated_max_logical_width, cell_max_logical_width);
        cell_min_logical_width -= allocated_min_logical_width;
        cell_max_logical_width -= allocated_max_logical_width;
      } else {
        int remaining_max_logical_width = span_max_logical_width;
        int remaining_min_logical_width = span_min_logical_width;

        // Give min to variable first, to fixed second, and to others third.
        for (unsigned pos = eff_col;
             remaining_max_logical_width >= 0 && pos < last_col; ++pos) {
          if (layout_struct_[pos].logical_width.IsFixed() && have_auto &&
              fixed_width <= cell_min_logical_width) {
            int col_min_logical_width =
                std::max<int>(layout_struct_[pos].effective_min_logical_width,
                              layout_struct_[pos].logical_width.Value());
            fixed_width -= layout_struct_[pos].logical_width.Value();
            remaining_min_logical_width -=
                layout_struct_[pos].effective_min_logical_width;
            remaining_max_logical_width -=
                layout_struct_[pos].effective_max_logical_width;
            cell_min_logical_width -= col_min_logical_width;
            layout_struct_[pos].effective_min_logical_width =
                col_min_logical_width;
          }
        }

        for (unsigned pos = eff_col;
             remaining_max_logical_width >= 0 && pos < last_col &&
             remaining_min_logical_width < cell_min_logical_width;
             ++pos) {
          if (!(layout_struct_[pos].logical_width.IsFixed() && have_auto &&
                fixed_width <= cell_min_logical_width)) {
            int col_min_logical_width = std::max<int>(
                layout_struct_[pos].effective_min_logical_width,
                static_cast<int>(
                    remaining_max_logical_width
                        ? cell_min_logical_width *
                              static_cast<float>(
                                  layout_struct_[pos]
                                      .effective_max_logical_width) /
                              remaining_max_logical_width
                        : cell_min_logical_width));
            col_min_logical_width = std::min<int>(
                layout_struct_[pos].effective_min_logical_width +
                    (cell_min_logical_width - remaining_min_logical_width),
                col_min_logical_width);
            remaining_max_logical_width -=
                layout_struct_[pos].effective_max_logical_width;
            remaining_min_logical_width -=
                layout_struct_[pos].effective_min_logical_width;
            cell_min_logical_width -= col_min_logical_width;
            layout_struct_[pos].effective_min_logical_width =
                col_min_logical_width;
          }
        }
      }
    }
    if (!cell_logical_width.IsPercentOrCalc()) {
      if (cell_max_logical_width > span_max_logical_width) {
        for (unsigned pos = eff_col;
             span_max_logical_width >= 0 && pos < last_col; ++pos) {
          int col_max_logical_width = std::max(
              layout_struct_[pos].effective_max_logical_width,
              static_cast<int>(span_max_logical_width
                                   ? cell_max_logical_width *
                                         static_cast<float>(
                                             layout_struct_[pos]
                                                 .effective_max_logical_width) /
                                         span_max_logical_width
                                   : cell_max_logical_width));
          span_max_logical_width -=
              layout_struct_[pos].effective_max_logical_width;
          cell_max_logical_width -= col_max_logical_width;
          layout_struct_[pos].effective_max_logical_width =
              col_max_logical_width;
        }
      }
    } else {
      for (unsigned pos = eff_col; pos < last_col; ++pos)
        layout_struct_[pos].max_logical_width =
            std::max(layout_struct_[pos].max_logical_width,
                     layout_struct_[pos].min_logical_width);
    }
    // treat span ranges consisting of empty cells only as if they had content
    if (span_has_empty_cells_only) {
      for (unsigned pos = eff_col; pos < last_col; ++pos)
        layout_struct_[pos].empty_cells_only = false;
    }
  }
  effective_logical_width_dirty_ = false;

  return std::min(max_logical_width, INT_MAX / 2);
}

/* gets all cells that originate in a column and have a cellspan > 1
   Sorts them by increasing cellspan
*/
void TableLayoutAlgorithmAuto::InsertSpanCell(LayoutTableCell* cell) {
  DCHECK(cell);
  DCHECK_NE(cell->ColSpan(), 1u);
  if (!cell || cell->ColSpan() == 1)
    return;

  unsigned size = span_cells_.size();
  if (!size || span_cells_[size - 1] != 0) {
    span_cells_.Grow(size + 10);
    for (unsigned i = 0; i < 10; i++)
      span_cells_[size + i] = 0;
    size += 10;
  }

  // Add them in sort. This is a slow algorithm, and a binary search or a fast
  // sorting after collection would be better.
  unsigned pos = 0;
  unsigned span = cell->ColSpan();
  while (pos < span_cells_.size() && span_cells_[pos] &&
         span > span_cells_[pos]->ColSpan())
    pos++;
  memmove(span_cells_.data() + pos + 1, span_cells_.data() + pos,
          (size - pos - 1) * sizeof(LayoutTableCell*));
  span_cells_[pos] = cell;
}

void TableLayoutAlgorithmAuto::UpdateLayout() {
  // table layout based on the values collected in the layout structure.
  int table_logical_width = (table_->LogicalWidth() -
                             table_->BordersPaddingAndSpacingInRowDirection())
                                .ToInt();
  int available = table_logical_width;
  unsigned n_eff_cols = table_->NumEffectiveColumns();

  // FIXME: It is possible to be called without having properly updated our
  // internal representation.  This means that our preferred logical widths were
  // not recomputed as expected.
  if (n_eff_cols != layout_struct_.size()) {
    FullRecalc();
    // FIXME: Table layout shouldn't modify our table structure (but does due to
    // columns and column-groups).
    n_eff_cols = table_->NumEffectiveColumns();
  }

  if (effective_logical_width_dirty_)
    CalcEffectiveLogicalWidth();

  bool have_percent = false;
  int num_auto = 0;
  int num_fixed = 0;
  float total_auto = 0;
  float total_fixed = 0;
  float total_percent = 0;
  int alloc_auto = 0;
  unsigned num_auto_empty_cells_only = 0;

  // fill up every cell with its minWidth
  for (unsigned i = 0; i < n_eff_cols; ++i) {
    int cell_logical_width = layout_struct_[i].effective_min_logical_width;
    layout_struct_[i].computed_logical_width = cell_logical_width;
    available -= cell_logical_width;
    Length& logical_width = layout_struct_[i].effective_logical_width;
    switch (logical_width.GetType()) {
      case Length::kPercent:
        have_percent = true;
        total_percent += logical_width.Percent();
        break;
      case Length::kFixed:
        num_fixed++;
        total_fixed += layout_struct_[i].ClampedEffectiveMaxLogicalWidth();
        // fall through
        break;
      case Length::kAuto:
        if (layout_struct_[i].empty_cells_only) {
          num_auto_empty_cells_only++;
        } else {
          num_auto++;
          total_auto += layout_struct_[i].ClampedEffectiveMaxLogicalWidth();
        }
        if (!layout_struct_[i].column_has_no_cells)
          alloc_auto += cell_logical_width;
        break;
      default:
        break;
    }
  }

  // allocate width to percent cols
  if (available > 0 && have_percent) {
    for (unsigned i = 0; i < n_eff_cols; ++i) {
      Length& logical_width = layout_struct_[i].effective_logical_width;
      if (logical_width.IsPercentOrCalc()) {
        int cell_logical_width =
            std::max<int>(layout_struct_[i].effective_min_logical_width,
                          MinimumValueForLength(logical_width,
                                                LayoutUnit(table_logical_width))
                              .ToInt());
        available +=
            layout_struct_[i].computed_logical_width - cell_logical_width;
        layout_struct_[i].computed_logical_width = cell_logical_width;
      }
    }
    if (total_percent > 100) {
      // remove overallocated space from the last columns
      int excess = table_logical_width * (total_percent - 100) / 100;
      for (unsigned i = n_eff_cols; i;) {
        --i;
        if (layout_struct_[i].effective_logical_width.IsPercentOrCalc()) {
          int cell_logical_width = layout_struct_[i].computed_logical_width;
          int reduction = std::min(cell_logical_width, excess);
          // The lines below might look inconsistent, but that's the way it's
          // handled in mozilla.
          excess -= reduction;
          int new_logical_width =
              std::max<int>(layout_struct_[i].effective_min_logical_width,
                            cell_logical_width - reduction);
          available += cell_logical_width - new_logical_width;
          layout_struct_[i].computed_logical_width = new_logical_width;
        }
      }
    }
  }

  // then allocate width to fixed cols
  if (available > 0) {
    for (unsigned i = 0; i < n_eff_cols; ++i) {
      Length& logical_width = layout_struct_[i].effective_logical_width;
      if (logical_width.IsFixed() &&
          logical_width.Value() > layout_struct_[i].computed_logical_width) {
        available +=
            layout_struct_[i].computed_logical_width - logical_width.Value();
        layout_struct_[i].computed_logical_width = logical_width.Value();
      }
    }
  }

  // Give each auto width column its share of the available width, non-empty
  // columns then empty columns.
  if (available > 0 && (num_auto || num_auto_empty_cells_only)) {
    available += alloc_auto;
    if (num_auto) {
      DistributeWidthToColumns<float, Length::kAuto, kNonEmptyCells,
                               kInitialWidth, kStartToEnd>(available,
                                                           total_auto);
    }
    if (num_auto_empty_cells_only) {
      DistributeWidthToColumns<unsigned, Length::kAuto, kEmptyCells,
                               kInitialWidth, kStartToEnd>(
          available, num_auto_empty_cells_only);
    }
  }

  // Any remaining available width expands fixed width, percent width, and
  // non-empty auto width columns, in that order.
  if (available > 0 && num_fixed) {
    DistributeWidthToColumns<float, Length::kFixed, kAllCells, kExtraWidth,
                             kStartToEnd>(available, total_fixed);
  }

  if (available > 0 && has_percent_ && total_percent < 100) {
    DistributeWidthToColumns<float, Length::kPercent, kAllCells, kExtraWidth,
                             kStartToEnd>(available, total_percent);
  }

  if (available > 0 && n_eff_cols > num_auto_empty_cells_only) {
    unsigned total = n_eff_cols - num_auto_empty_cells_only;
    // Starting from the last cell is for compatibility with FF/IE - it isn't
    // specified anywhere.
    DistributeWidthToColumns<unsigned, Length::kAuto, kNonEmptyCells,
                             kLeftoverWidth, kEndToStart>(available, total);
  }

  // If we have overallocated, reduce every cell according to the difference
  // between desired width and minwidth. This seems to produce to the pixel
  // exact results with IE. Wonder is some of this also holds for width
  // distributing. This is basically the reverse of how we grew the cells.
  if (available < 0)
    ShrinkColumnWidth(Length::kAuto, available);
  if (available < 0)
    ShrinkColumnWidth(Length::kFixed, available);
  if (available < 0)
    ShrinkColumnWidth(Length::kPercent, available);

  DCHECK_EQ(table_->EffectiveColumnPositions().size(), n_eff_cols + 1);
  int pos = 0;
  for (unsigned i = 0; i < n_eff_cols; ++i) {
    table_->SetEffectiveColumnPosition(i, pos);
    pos += layout_struct_[i].computed_logical_width + table_->HBorderSpacing();
  }
  // The extra position is for the imaginary column after the last column.
  table_->SetEffectiveColumnPosition(n_eff_cols, pos);
}

template <typename Total,
          Length::Type lengthType,
          CellsToProcess cellsToProcess,
          DistributionMode distributionMode,
          DistributionDirection distributionDirection>
void TableLayoutAlgorithmAuto::DistributeWidthToColumns(int& available,
                                                        Total total) {
  // TODO(alancutter): Make this work correctly for calc lengths.
  int n_eff_cols = static_cast<int>(table_->NumEffectiveColumns());
  bool start_to_end = distributionDirection == kStartToEnd;
  for (int i = start_to_end ? 0 : n_eff_cols - 1;
       start_to_end ? i < n_eff_cols : i > -1; start_to_end ? ++i : --i) {
    const Length& logical_width = layout_struct_[i].effective_logical_width;
    if (cellsToProcess == kNonEmptyCells && logical_width.IsAuto() &&
        layout_struct_[i].empty_cells_only)
      continue;
    // When allocating width to columns with nothing but empty cells we avoid
    // columns that exist only to flesh out a colspan and have no actual cells.
    if (cellsToProcess == kEmptyCells && logical_width.IsAuto() &&
        (!layout_struct_[i].empty_cells_only ||
         layout_struct_[i].column_has_no_cells))
      continue;
    if (distributionMode != kLeftoverWidth &&
        logical_width.GetType() != lengthType)
      continue;

    float factor = 1;
    if (distributionMode != kLeftoverWidth) {
      if (lengthType == Length::kPercent)
        factor = logical_width.Percent();
      else if (lengthType == Length::kAuto || lengthType == Length::kFixed)
        factor = layout_struct_[i].ClampedEffectiveMaxLogicalWidth();
    }

    int new_width = available * factor / total;
    int cell_logical_width =
        (distributionMode == kInitialWidth)
            ? max<int>(layout_struct_[i].computed_logical_width, new_width)
            : new_width;
    available -= cell_logical_width;
    total -= factor;
    layout_struct_[i].computed_logical_width =
        (distributionMode == kInitialWidth)
            ? cell_logical_width
            : layout_struct_[i].computed_logical_width + cell_logical_width;

    // If we have run out of width to allocate we're done.
    // TODO(rhogan): Extend this to Fixed as well.
    if (lengthType == Length::kPercent && (!available || !total))
      return;
    if (lengthType == Length::kAuto && !total)
      return;
  }
}

void TableLayoutAlgorithmAuto::ShrinkColumnWidth(
    const Length::Type& length_type,
    int& available) {
  unsigned n_eff_cols = table_->NumEffectiveColumns();
  int logical_width_beyond_min = 0;
  for (unsigned i = n_eff_cols; i;) {
    --i;
    Length& logical_width = layout_struct_[i].effective_logical_width;
    if (logical_width.GetType() == length_type)
      logical_width_beyond_min += layout_struct_[i].computed_logical_width -
                                  layout_struct_[i].effective_min_logical_width;
  }

  for (unsigned i = n_eff_cols; i && logical_width_beyond_min > 0;) {
    --i;
    Length& logical_width = layout_struct_[i].effective_logical_width;
    if (logical_width.GetType() == length_type) {
      int min_max_diff = layout_struct_[i].computed_logical_width -
                         layout_struct_[i].effective_min_logical_width;
      int reduce = available * min_max_diff / logical_width_beyond_min;
      layout_struct_[i].computed_logical_width += reduce;
      available -= reduce;
      logical_width_beyond_min -= min_max_diff;
      if (available >= 0)
        break;
    }
  }
}
}  // namespace blink
