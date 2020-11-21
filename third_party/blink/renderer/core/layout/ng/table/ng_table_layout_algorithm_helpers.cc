// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_helpers.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"

namespace blink {

namespace {

// Implements spec distribution algorithm:
// https://www.w3.org/TR/css-tables-3/#width-distribution-algorithm
Vector<LayoutUnit> DistributeInlineSizeToComputedInlineSizeAuto(
    LayoutUnit target_inline_size,
    LayoutUnit inline_border_spacing,
    const NGTableTypes::Column* start_column,
    const NGTableTypes::Column* end_column) {
  unsigned all_columns_count = 0;
  unsigned percent_columns_count = 0;
  unsigned fixed_columns_count = 0;
  unsigned auto_columns_count = 0;

  // What guesses mean is described in table specification.
  // https://www.w3.org/TR/css-tables-3/#width-distribution-algorithm
  enum { kMinGuess, kPercentageGuess, kSpecifiedGuess, kMaxGuess, kAboveMax };
  // sizes are collected for all guesses except kAboveMax
  LayoutUnit guess_sizes[kAboveMax];
  LayoutUnit guess_size_total_increases[kAboveMax];
  float total_percent = 0.0f;
  LayoutUnit total_auto_max_inline_size;
  LayoutUnit total_fixed_max_inline_size;

  for (const NGTableTypes::Column* column = start_column; column != end_column;
       ++column) {
    all_columns_count++;
    DCHECK(column->min_inline_size);
    DCHECK(column->max_inline_size);
    if (column->percent) {
      percent_columns_count++;
      total_percent += *column->percent;
      LayoutUnit percent_inline_size =
          column->ResolvePercentInlineSize(target_inline_size);
      guess_sizes[kMinGuess] += *column->min_inline_size;
      guess_sizes[kPercentageGuess] += percent_inline_size;
      guess_sizes[kSpecifiedGuess] += percent_inline_size;
      guess_sizes[kMaxGuess] += percent_inline_size;
      guess_size_total_increases[kPercentageGuess] +=
          percent_inline_size - *column->min_inline_size;
    } else if (column->is_constrained) {  // Fixed column
      fixed_columns_count++;
      total_fixed_max_inline_size += *column->max_inline_size;
      guess_sizes[kMinGuess] += *column->min_inline_size;
      guess_sizes[kPercentageGuess] += *column->min_inline_size;
      guess_sizes[kSpecifiedGuess] += *column->max_inline_size;
      guess_sizes[kMaxGuess] += *column->max_inline_size;
      guess_size_total_increases[kSpecifiedGuess] +=
          *column->max_inline_size - *column->min_inline_size;
    } else {  // Auto column
      auto_columns_count++;
      total_auto_max_inline_size += *column->max_inline_size;
      guess_sizes[kMinGuess] += *column->min_inline_size;
      guess_sizes[kPercentageGuess] += *column->min_inline_size;
      guess_sizes[kSpecifiedGuess] += *column->min_inline_size;
      guess_sizes[kMaxGuess] += *column->max_inline_size;
      guess_size_total_increases[kMaxGuess] +=
          *column->max_inline_size - *column->min_inline_size;
    }
  }

  Vector<LayoutUnit> computed_sizes;
  computed_sizes.resize(all_columns_count);

  // Distributing inline sizes can never cause cells to be < min_inline_size.
  // Target inline size must be wider than sum of min inline sizes.
  // This is always true for assignable_table_inline_size, but not for
  // colspan_cells.
  target_inline_size = std::max(target_inline_size, guess_sizes[kMinGuess]);

  unsigned starting_guess = kAboveMax;
  for (unsigned i = kMinGuess; i != kAboveMax; ++i) {
    if (guess_sizes[i] >= target_inline_size) {
      starting_guess = i;
      break;
    }
  }
  switch (starting_guess) {
    case kMinGuess: {
      // All columns are min inline size.
      LayoutUnit* computed_size = computed_sizes.begin();
      for (const NGTableTypes::Column* column = start_column;
           column != end_column; ++column, ++computed_size) {
        *computed_size = column->min_inline_size.value_or(LayoutUnit());
      }
    } break;
    case kPercentageGuess: {
      // Percent columns grow, auto/fixed get min inline size.
      LayoutUnit percent_inline_size_increases =
          guess_size_total_increases[kPercentageGuess];
      LayoutUnit distributable_inline_size =
          target_inline_size - guess_sizes[kMinGuess];
      LayoutUnit rounding_error_inline_size = distributable_inline_size;
      LayoutUnit* computed_size = computed_sizes.begin();
      LayoutUnit* last_computed_size = nullptr;
      for (const NGTableTypes::Column* column = start_column;
           column != end_column; ++column, ++computed_size) {
        if (column->percent) {
          last_computed_size = computed_size;
          LayoutUnit percent_inline_size =
              column->ResolvePercentInlineSize(target_inline_size);
          LayoutUnit column_inline_size_increase =
              percent_inline_size - *column->min_inline_size;
          LayoutUnit delta;
          if (percent_inline_size_increases != LayoutUnit()) {
            delta = LayoutUnit(distributable_inline_size *
                               column_inline_size_increase.ToFloat() /
                               percent_inline_size_increases);
          } else {
            delta = LayoutUnit(distributable_inline_size.ToFloat() /
                               percent_columns_count);
          }
          rounding_error_inline_size -= delta;
          *computed_size = *column->min_inline_size + delta;
        } else {
          // Auto/Fixed columns get min inline size.
          *computed_size = *column->min_inline_size;
        }
      }
      if (rounding_error_inline_size != LayoutUnit()) {
        DCHECK(last_computed_size);
        *last_computed_size += rounding_error_inline_size;
      }
    } break;
    case kSpecifiedGuess: {
      // Fixed columns grow, auto gets min, percent gets %max
      LayoutUnit fixed_inline_size_increase =
          guess_size_total_increases[kSpecifiedGuess];
      LayoutUnit distributable_inline_size =
          target_inline_size - guess_sizes[kPercentageGuess];
      LayoutUnit rounding_error_inline_size = distributable_inline_size;
      LayoutUnit* last_computed_size = nullptr;
      LayoutUnit* computed_size = computed_sizes.begin();
      for (const NGTableTypes::Column* column = start_column;
           column != end_column; ++column, ++computed_size) {
        if (column->percent) {
          *computed_size = column->ResolvePercentInlineSize(target_inline_size);
        } else if (column->is_constrained) {
          last_computed_size = computed_size;
          LayoutUnit column_inline_size_increase =
              *column->max_inline_size - *column->min_inline_size;
          LayoutUnit delta;
          if (fixed_inline_size_increase != LayoutUnit()) {
            delta = LayoutUnit(distributable_inline_size *
                               column_inline_size_increase.ToFloat() /
                               fixed_inline_size_increase);
          } else {
            delta = LayoutUnit(distributable_inline_size.ToFloat() /
                               fixed_columns_count);
          }
          rounding_error_inline_size -= delta;
          *computed_size = *column->min_inline_size + delta;
        } else {
          *computed_size = *column->min_inline_size;
        }
      }
      if (rounding_error_inline_size != LayoutUnit()) {
        DCHECK(last_computed_size);
        *last_computed_size += rounding_error_inline_size;
      }
    } break;
    case kMaxGuess: {
      // Auto columns grow, fixed gets max, percent gets %max
      LayoutUnit auto_inline_size_increase =
          guess_size_total_increases[kMaxGuess];
      LayoutUnit distributable_inline_size =
          target_inline_size - guess_sizes[kSpecifiedGuess];
      // When widths match exactly, this usually means that table width
      // is auto, and that columns should be wide enough to accommodate
      // content without wrapping.
      // Instead of using floating-point math to compute final column
      // width, we use max_inline_size.
      // Using floating-point math can cause rounding errors, and uninintended
      // line wrap.
      bool is_exact_match = target_inline_size == guess_sizes[kMaxGuess];
      LayoutUnit rounding_error_inline_size =
          is_exact_match ? LayoutUnit() : distributable_inline_size;
      LayoutUnit* last_computed_size = nullptr;
      LayoutUnit* computed_size = computed_sizes.begin();
      for (const NGTableTypes::Column* column = start_column;
           column != end_column; ++column, ++computed_size) {
        if (column->percent) {
          *computed_size = column->ResolvePercentInlineSize(target_inline_size);
        } else if (column->is_constrained || is_exact_match) {
          *computed_size = *column->max_inline_size;
        } else {
          last_computed_size = computed_size;
          LayoutUnit column_inline_size_increase =
              *column->max_inline_size - *column->min_inline_size;
          LayoutUnit delta;
          if (auto_inline_size_increase != LayoutUnit()) {
            delta = LayoutUnit(distributable_inline_size *
                               column_inline_size_increase.ToFloat() /
                               auto_inline_size_increase);
          } else {
            delta = LayoutUnit(distributable_inline_size.ToFloat() /
                               auto_columns_count);
          }
          rounding_error_inline_size -= delta;
          *computed_size = *column->min_inline_size + delta;
        }
      }
      if (rounding_error_inline_size != LayoutUnit()) {
        DCHECK(last_computed_size);
        *last_computed_size += rounding_error_inline_size;
      }
    } break;
    case kAboveMax: {
      LayoutUnit distributable_inline_size =
          target_inline_size - guess_sizes[kMaxGuess];
      if (auto_columns_count > 0) {
        // Grow auto columns if available
        LayoutUnit rounding_error_inline_size = distributable_inline_size;
        LayoutUnit* last_computed_size = nullptr;
        LayoutUnit* computed_size = computed_sizes.begin();
        for (const NGTableTypes::Column* column = start_column;
             column != end_column; ++column, ++computed_size) {
          if (column->percent) {
            *computed_size =
                column->ResolvePercentInlineSize(target_inline_size);
          } else if (column->is_constrained) {
            *computed_size = *column->max_inline_size;
          } else {
            last_computed_size = computed_size;
            LayoutUnit delta;
            if (total_auto_max_inline_size > LayoutUnit()) {
              delta = LayoutUnit(distributable_inline_size *
                                 (*column->max_inline_size).ToFloat() /
                                 total_auto_max_inline_size);
            } else {
              delta = distributable_inline_size / auto_columns_count;
            }
            rounding_error_inline_size -= delta;
            *computed_size = *column->max_inline_size + delta;
          }
        }
        if (rounding_error_inline_size != LayoutUnit()) {
          DCHECK(last_computed_size);
          *last_computed_size += rounding_error_inline_size;
        }
      } else if (fixed_columns_count > 0) {
        // Grow fixed columns if available.
        LayoutUnit rounding_error_inline_size = distributable_inline_size;
        LayoutUnit* last_computed_size = nullptr;
        LayoutUnit* computed_size = computed_sizes.begin();
        for (const NGTableTypes::Column* column = start_column;
             column != end_column; ++column, ++computed_size) {
          if (column->percent) {
            *computed_size =
                column->ResolvePercentInlineSize(target_inline_size);
          } else if (column->is_constrained) {
            last_computed_size = computed_size;
            LayoutUnit delta;
            if (total_fixed_max_inline_size > LayoutUnit()) {
              delta = LayoutUnit(distributable_inline_size *
                                 (*column->max_inline_size).ToFloat() /
                                 total_fixed_max_inline_size);
            } else {
              delta = distributable_inline_size / fixed_columns_count;
            }
            rounding_error_inline_size -= delta;
            *computed_size = *column->max_inline_size + delta;
          } else {
            DCHECK(false);
          }
        }
        if (rounding_error_inline_size != LayoutUnit()) {
          DCHECK(last_computed_size);
          *last_computed_size += rounding_error_inline_size;
        }
      } else if (percent_columns_count > 0) {
        // All remaining columns are percent. Grow them.
        LayoutUnit rounding_error_inline_size = target_inline_size;
        LayoutUnit* last_computed_size = nullptr;
        LayoutUnit* computed_size = computed_sizes.begin();
        for (const NGTableTypes::Column* column = start_column;
             column != end_column; ++column, ++computed_size) {
          DCHECK(column->percent);
          last_computed_size = computed_size;
          if (total_percent > 0.0f) {
            *computed_size = LayoutUnit(*column->percent / total_percent *
                                        target_inline_size);
          } else {
            *computed_size = distributable_inline_size / percent_columns_count;
          }
          rounding_error_inline_size -= *computed_size;
        }
        if (rounding_error_inline_size != LayoutUnit()) {
          DCHECK(last_computed_size);
          *last_computed_size += rounding_error_inline_size;
        }
      }
    }
  }
  return computed_sizes;
}

Vector<LayoutUnit> SynchronizeAssignableTableInlineSizeAndColumnsFixed(
    LayoutUnit target_inline_size,
    LayoutUnit inline_border_spacing,
    const NGTableTypes::Columns& column_constraints) {
  unsigned all_columns_count = 0;
  unsigned percent_columns_count = 0;
  unsigned auto_columns_count = 0;
  unsigned auto_empty_columns_count = 0;
  unsigned fixed_columns_count = 0;

  float total_percent = 0.0f;
  LayoutUnit total_percent_inline_size;
  LayoutUnit total_auto_max_inline_size;
  LayoutUnit total_fixed_inline_size;
  LayoutUnit assigned_inline_size;
  Vector<LayoutUnit> column_sizes;
  column_sizes.resize(column_constraints.data.size());
  for (const NGTableTypes::Column& column : column_constraints.data) {
    all_columns_count++;
    if (column.percent) {
      percent_columns_count++;
      total_percent += *column.percent;
      total_percent_inline_size +=
          column.ResolvePercentInlineSize(target_inline_size);
    } else if (column.is_constrained) {  // Fixed column
      fixed_columns_count++;
      total_fixed_inline_size += column.max_inline_size.value_or(LayoutUnit());
    } else {
      auto_columns_count++;
      if (*column.max_inline_size == LayoutUnit())
        auto_empty_columns_count++;
      total_auto_max_inline_size +=
          column.max_inline_size.value_or(LayoutUnit());
    }
  }

  LayoutUnit* last_column_size = nullptr;
  // Distribute to fixed columns.
  if (fixed_columns_count > 0) {
    float scale = 1.0f;
    bool scale_available = true;
    LayoutUnit target_fixed_size =
        (target_inline_size - total_percent_inline_size).ClampNegativeToZero();
    bool scale_up =
        total_fixed_inline_size < target_fixed_size && auto_columns_count == 0;
    // Fixed columns grow if there are no auto columns. They fill up space not
    // taken up by percentage columns.
    bool scale_down = total_fixed_inline_size > target_inline_size;
    if (scale_up || scale_down) {
      if (total_fixed_inline_size != LayoutUnit()) {
        scale = target_fixed_size.ToFloat() / total_fixed_inline_size;
      } else {
        scale_available = false;
      }
    }
    LayoutUnit* column_size = column_sizes.begin();
    for (const NGTableTypes::Column* column = column_constraints.data.begin();
         column != column_constraints.data.end(); ++column, ++column_size) {
      if (!column->IsFixed())
        continue;
      last_column_size = column_size;
      if (scale_available) {
        *column_size =
            LayoutUnit(scale * column->max_inline_size.value_or(LayoutUnit()));
      } else {
        DCHECK_EQ(fixed_columns_count, all_columns_count);
        *column_size =
            LayoutUnit(target_inline_size.ToFloat() / fixed_columns_count);
      }
      assigned_inline_size += *column_size;
    }
  }
  if (assigned_inline_size >= target_inline_size)
    return column_sizes;
  // Distribute to percent columns.
  if (percent_columns_count > 0) {
    float scale = 1.0f;
    bool scale_available = true;
    // Percent columns only grow if there are no auto columns.
    bool scale_up = total_percent_inline_size <
                        (target_inline_size - assigned_inline_size) &&
                    auto_columns_count == 0;
    bool scale_down =
        total_percent_inline_size > (target_inline_size - assigned_inline_size);
    if (scale_up || scale_down) {
      if (total_percent_inline_size != LayoutUnit()) {
        scale = (target_inline_size - assigned_inline_size).ToFloat() /
                total_percent_inline_size;
      } else {
        scale_available = false;
      }
    }
    LayoutUnit* column_size = column_sizes.begin();
    for (const NGTableTypes::Column* column = column_constraints.data.begin();
         column != column_constraints.data.end(); ++column, ++column_size) {
      if (!column->percent)
        continue;
      last_column_size = column_size;
      if (scale_available) {
        *column_size = LayoutUnit(
            scale * column->ResolvePercentInlineSize(target_inline_size));
      } else {
        *column_size =
            LayoutUnit((target_inline_size - assigned_inline_size).ToFloat() /
                       percent_columns_count);
      }
      assigned_inline_size += *column_size;
    }
  }
  // Distribute to auto columns.
  LayoutUnit distributing_inline_size =
      target_inline_size - assigned_inline_size;
  LayoutUnit* column_size = column_sizes.begin();

  for (const NGTableTypes::Column* column = column_constraints.data.begin();
       column != column_constraints.data.end(); ++column, ++column_size) {
    if (column->percent || column->is_constrained)
      continue;
    last_column_size = column_size;
    *column_size =
        LayoutUnit(distributing_inline_size / float(auto_columns_count));
    assigned_inline_size += *column_size;
  }
  LayoutUnit delta = target_inline_size - assigned_inline_size;
  *last_column_size += delta;

  return column_sizes;
}

void DistributeColspanCellToColumnsFixed(
    const NGTableTypes::ColspanCell& colspan_cell,
    LayoutUnit inline_border_spacing,
    NGTableTypes::Columns* column_constraints) {
  // Fixed layout does not merge columns.
  DCHECK_LE(colspan_cell.span,
            column_constraints->data.size() - colspan_cell.start_column);
  NGTableTypes::Column* start_column =
      &column_constraints->data[colspan_cell.start_column];
  NGTableTypes::Column* end_column = start_column + colspan_cell.span;
  DCHECK_NE(start_column, end_column);

  LayoutUnit colspan_cell_min_inline_size;
  LayoutUnit colspan_cell_max_inline_size;
  // Colspanned cells only distribute min inline size if constrained.
  if (colspan_cell.cell_inline_constraint.is_constrained) {
    colspan_cell_min_inline_size =
        (colspan_cell.cell_inline_constraint.min_inline_size -
         (colspan_cell.span - 1) * inline_border_spacing)
            .ClampNegativeToZero();
  }
  colspan_cell_max_inline_size =
      (colspan_cell.cell_inline_constraint.max_inline_size -
       (colspan_cell.span - 1) * inline_border_spacing)
          .ClampNegativeToZero();

  // Distribute min/max/percentage evenly between all cells.
  LayoutUnit rounding_error_min_inline_size = colspan_cell_min_inline_size;
  LayoutUnit rounding_error_max_inline_size = colspan_cell_max_inline_size;
  float rounding_error_percent =
      colspan_cell.cell_inline_constraint.percent.value_or(0.0f);

  LayoutUnit new_min_size = LayoutUnit(colspan_cell_min_inline_size /
                                       static_cast<float>(colspan_cell.span));
  LayoutUnit new_max_size = LayoutUnit(colspan_cell_max_inline_size /
                                       static_cast<float>(colspan_cell.span));
  base::Optional<float> new_percent;
  if (colspan_cell.cell_inline_constraint.percent) {
    new_percent =
        *colspan_cell.cell_inline_constraint.percent / colspan_cell.span;
  }

  NGTableTypes::Column* last_column;
  for (NGTableTypes::Column* column = start_column; column < end_column;
       ++column) {
    last_column = column;
    rounding_error_min_inline_size -= new_min_size;
    rounding_error_max_inline_size -= new_max_size;
    if (new_percent)
      rounding_error_percent -= *new_percent;

    if (!column->min_inline_size) {
      column->is_constrained |=
          colspan_cell.cell_inline_constraint.is_constrained;
      column->min_inline_size = new_min_size;
    }
    if (!column->max_inline_size) {
      column->is_constrained |=
          colspan_cell.cell_inline_constraint.is_constrained;
      column->max_inline_size = new_max_size;
    }
    if (!column->percent && new_percent)
      column->percent = new_percent;
  }
  last_column->min_inline_size =
      *last_column->min_inline_size + rounding_error_min_inline_size;
  last_column->max_inline_size =
      *last_column->max_inline_size + rounding_error_max_inline_size;
  if (new_percent)
    last_column->percent = *last_column->percent + rounding_error_percent;
}

void DistributeColspanCellToColumnsAuto(
    const NGTableTypes::ColspanCell& colspan_cell,
    LayoutUnit inline_border_spacing,
    NGTableTypes::Columns* column_constraints) {
  if (column_constraints->data.IsEmpty())
    return;
  unsigned effective_span =
      std::min(colspan_cell.span,
               column_constraints->data.size() - colspan_cell.start_column);
  NGTableTypes::Column* start_column =
      &column_constraints->data[colspan_cell.start_column];
  NGTableTypes::Column* end_column = start_column + effective_span;

  // Inline sizes for redistribution exclude border spacing.
  LayoutUnit colspan_cell_min_inline_size =
      (colspan_cell.cell_inline_constraint.min_inline_size -
       (effective_span - 1) * inline_border_spacing)
          .ClampNegativeToZero();
  LayoutUnit colspan_cell_max_inline_size =
      (colspan_cell.cell_inline_constraint.max_inline_size -
       (effective_span - 1) * inline_border_spacing)
          .ClampNegativeToZero();
  base::Optional<float> colspan_cell_percent =
      colspan_cell.cell_inline_constraint.percent;

  if (colspan_cell_percent.has_value()) {
    float columns_percent = 0.0f;
    unsigned all_columns_count = 0;
    unsigned percent_columns_count = 0;
    unsigned nonpercent_columns_count = 0;
    LayoutUnit nonpercent_columns_max_inline_size;
    for (NGTableTypes::Column* column = start_column; column != end_column;
         ++column) {
      if (!column->max_inline_size)
        column->max_inline_size = LayoutUnit();
      if (!column->min_inline_size)
        column->min_inline_size = LayoutUnit();
      all_columns_count++;
      if (column->percent) {
        percent_columns_count++;
        columns_percent += *column->percent;
      } else {
        nonpercent_columns_count++;
        nonpercent_columns_max_inline_size += *column->max_inline_size;
      }
    }
    float surplus_percent = *colspan_cell_percent - columns_percent;
    if (surplus_percent > 0.0f && all_columns_count > percent_columns_count) {
      // Distribute surplus percent to non-percent columns in proportion to
      // max_inline_size.
      for (NGTableTypes::Column* column = start_column; column != end_column;
           ++column) {
        if (column->percent)
          continue;
        float column_percent;
        if (nonpercent_columns_max_inline_size != LayoutUnit()) {
          // Column percentage is proportional to its max_inline_size.
          column_percent = surplus_percent *
                           column->max_inline_size.value_or(LayoutUnit()) /
                           nonpercent_columns_max_inline_size;
        } else {
          // Distribute evenly instead.
          // Legacy difference: Legacy forces max_inline_size to be at least
          // 1px.
          column_percent = surplus_percent / nonpercent_columns_count;
        }
        column->percent = column_percent;
      }
    }
  }

  // TODO(atotic) See crbug.com/531752 for discussion about differences
  // between FF/Chrome.
  // Minimum inline size gets distributed with standard distribution algorithm.
  for (NGTableTypes::Column* column = start_column; column != end_column;
       ++column) {
    if (!column->min_inline_size)
      column->min_inline_size = LayoutUnit();
    if (!column->max_inline_size)
      column->max_inline_size = LayoutUnit();
  }
  Vector<LayoutUnit> computed_sizes =
      DistributeInlineSizeToComputedInlineSizeAuto(colspan_cell_min_inline_size,
                                                   inline_border_spacing,
                                                   start_column, end_column);
  LayoutUnit* computed_size = computed_sizes.begin();
  for (NGTableTypes::Column* column = start_column; column != end_column;
       ++column, ++computed_size) {
    column->min_inline_size =
        std::max(*column->min_inline_size, *computed_size);
  }
  computed_sizes = DistributeInlineSizeToComputedInlineSizeAuto(
      colspan_cell_max_inline_size, inline_border_spacing, start_column,
      end_column);
  computed_size = computed_sizes.begin();
  for (NGTableTypes::Column* column = start_column; column != end_column;
       ++column, ++computed_size) {
    column->max_inline_size =
        std::max(*column->max_inline_size, *computed_size);
  }
}

void DistributeExcessBlockSizeToRows(
    const wtf_size_t start_row_index,
    const wtf_size_t row_count,
    LayoutUnit desired_block_size,
    bool desired_block_size_is_rowspan,
    LayoutUnit border_block_spacing,
    LayoutUnit percentage_resolution_block_size,
    NGTableTypes::Rows* rows) {
  DCHECK_LE(start_row_index + row_count, rows->size());
  DCHECK_GE(desired_block_size, LayoutUnit());
  // This algorithm has not been defined by the standard in 2019.
  // Discussion at https://github.com/w3c/csswg-drafts/issues/4418
  if (row_count == 0)
    return;
  NGTableTypes::Row* start_row = std::next(rows->begin(), start_row_index);
  NGTableTypes::Row* end_row =
      std::next(rows->begin(), start_row_index + row_count);

  auto RowBlockSizeDeficit = [&percentage_resolution_block_size](
                                 NGTableTypes::Row* row) {
    if (percentage_resolution_block_size == kIndefiniteSize)
      return LayoutUnit();
    DCHECK(row->percent);
    return (LayoutUnit(*row->percent * percentage_resolution_block_size / 100) -
            row->block_size)
        .ClampNegativeToZero();
  };

  auto IsUnconstrainedNonEmptyRow =
      [&percentage_resolution_block_size](NGTableTypes::Row* row) {
        if (row->block_size == LayoutUnit())
          return false;
        if (row->percent && percentage_resolution_block_size == kIndefiniteSize)
          return true;
        return !row->is_constrained;
      };

  auto IsRowWithOriginatingRowspan =
      [&start_row, &desired_block_size_is_rowspan](NGTableTypes::Row* row) {
        // Rowspans are treated specially only during rowspan distribution.
        return desired_block_size_is_rowspan && row != start_row &&
               row->has_rowspan_start;
      };

  unsigned percent_rows_with_deficit_count = 0;
  unsigned rows_with_originating_rowspan = 0;
  unsigned unconstrained_non_empty_row_count = 0;
  unsigned constrained_non_empty_row_count = 0;
  unsigned empty_row_count = 0;

  LayoutUnit total_block_size;
  LayoutUnit percentage_block_size_deficit;
  LayoutUnit unconstrained_non_empty_row_block_size;

  for (NGTableTypes::Row* row = start_row; row != end_row; ++row) {
    total_block_size += row->block_size;
    if (row->percent) {
      LayoutUnit deficit = RowBlockSizeDeficit(row);
      if (deficit != LayoutUnit()) {
        percent_rows_with_deficit_count++;
        percentage_block_size_deficit += deficit;
      }
    }
    if (IsRowWithOriginatingRowspan(row)) {
      rows_with_originating_rowspan++;
    }
    if (IsUnconstrainedNonEmptyRow(row)) {
      unconstrained_non_empty_row_count++;
      unconstrained_non_empty_row_block_size += row->block_size;
    } else if (row->is_constrained && row->block_size != LayoutUnit()) {
      constrained_non_empty_row_count++;
    }
    if (row->block_size == LayoutUnit())
      empty_row_count++;
  }

  LayoutUnit distributable_block_size =
      (desired_block_size - border_block_spacing * (row_count - 1)) -
      total_block_size;
  if (distributable_block_size <= LayoutUnit())
    return;

  // Step 1: percentage rows grow to their percentage size.
  if (percent_rows_with_deficit_count > 0) {
    float ratio = std::min(
        distributable_block_size.ToFloat() / percentage_block_size_deficit,
        1.0f);
    LayoutUnit remaining_deficit =
        LayoutUnit(ratio * percentage_block_size_deficit);
    NGTableTypes::Row* last_row;
    LayoutUnit distributed_block_size;
    for (NGTableTypes::Row* row = start_row; row != end_row; ++row) {
      if (!row->percent)
        continue;
      last_row = row;
      LayoutUnit delta = LayoutUnit(RowBlockSizeDeficit(row) * ratio);
      row->block_size += delta;
      total_block_size += delta;
      distributed_block_size += delta;
      remaining_deficit -= delta;
    }
    last_row->block_size += remaining_deficit;
    distributed_block_size += remaining_deficit;
    distributable_block_size -= distributed_block_size;
  }
  DCHECK_GE(distributable_block_size, LayoutUnit());
  if (distributable_block_size <= LayoutUnit())
    return;

  // Step 2: Distribute to rows that have an originating rowspan.
  if (rows_with_originating_rowspan > 0) {
    LayoutUnit remaining_deficit = distributable_block_size;
    NGTableTypes::Row* last_row;
    for (NGTableTypes::Row* row = start_row; row != end_row; ++row) {
      if (!IsRowWithOriginatingRowspan(row))
        continue;
      last_row = row;
      LayoutUnit delta =
          LayoutUnit(distributable_block_size / rows_with_originating_rowspan);
      row->block_size += delta;
      remaining_deficit -= delta;
    }
    last_row->block_size += remaining_deficit;
    return;
  }
  // Step 3: "unconstrained non-empty rows" grow in proportion to current
  // block size.
  if (unconstrained_non_empty_row_count > 0) {
    LayoutUnit remaining_deficit = distributable_block_size;
    NGTableTypes::Row* last_row;
    for (NGTableTypes::Row* row = start_row; row != end_row; ++row) {
      if (!IsUnconstrainedNonEmptyRow(row))
        continue;
      last_row = row;
      LayoutUnit delta =
          LayoutUnit(row->block_size * distributable_block_size.ToFloat() /
                     unconstrained_non_empty_row_block_size);
      row->block_size += delta;
      remaining_deficit -= delta;
    }
    last_row->block_size += remaining_deficit;
    return;
  }

  // Step 4: Empty row distribution
  // Table distributes evenly between all rows.
  // If there are any empty rows except start row, last row takes all the
  // excess block size.
  if (empty_row_count > 0) {
    if (desired_block_size_is_rowspan) {
      NGTableTypes::Row* last_row = nullptr;
      NGTableTypes::Row* row = start_row;
      if (empty_row_count != row_count)  // skip initial row.
        ++row;
      for (; row != end_row; ++row) {
        if (row->block_size != LayoutUnit())
          continue;
        last_row = row;
      }
      if (last_row) {
        last_row->block_size = distributable_block_size;
        return;
      }
    } else if (empty_row_count == row_count ||
               (empty_row_count + constrained_non_empty_row_count ==
                row_count)) {
      // Grow empty rows if one of these is true:
      // - all rows are empty.
      // - non-empty rows are all constrained.
      // Different browsers disagree on when to grow empty rows.
      NGTableTypes::Row* last_row;
      LayoutUnit remaining_deficit = distributable_block_size;
      for (NGTableTypes::Row* row = start_row; row != end_row; ++row) {
        if (row->block_size != LayoutUnit())
          continue;
        last_row = row;
        // Table block size distributes equally, while rowspan distributes to
        // last row.
        LayoutUnit delta =
            LayoutUnit(distributable_block_size.ToFloat() / empty_row_count);
        row->block_size = delta;
        remaining_deficit -= delta;
      }
      last_row->block_size += remaining_deficit;
      return;
    }
  }

  // Step 5: Grow non-empty rows in proportion to current block size.
  // It grows constrained, and unconstrained rows.
  NGTableTypes::Row* last_row = nullptr;
  LayoutUnit remaining_deficit = distributable_block_size;
  for (NGTableTypes::Row* row = start_row; row != end_row; ++row) {
    if (row->block_size == LayoutUnit())
      continue;
    last_row = row;
    LayoutUnit delta = LayoutUnit(distributable_block_size *
                                  row->block_size.ToFloat() / total_block_size);
    row->block_size += delta;
    remaining_deficit -= delta;
  }
  if (last_row)
    last_row->block_size += remaining_deficit;
}

}  // namespace

MinMaxSizes NGTableAlgorithmHelpers::ComputeGridInlineMinMax(
    const NGTableTypes::Columns& column_constraints,
    LayoutUnit undistributable_space,
    bool is_fixed_layout,
    bool containing_block_expects_minmax_without_percentages,
    bool skip_collapsed_columns) {
  MinMaxSizes minmax;
  // https://www.w3.org/TR/css-tables-3/#computing-the-table-width
  // Compute standard GRID_MIN/GRID_MAX. They are sum of column_constraints.
  //
  // Standard does not specify how to handle percentages.
  // "a percentage represents a constraint on the column's inline size, which a
  // UA should try to satisfy"
  // Percentages cannot be resolved into pixels because size of containing
  // block is unknown. Instead, percentages are used to enforce following
  // constraints:
  // 1) Column min inline size and percentage imply that total inline sum must
  // be large enough to fit the column. Mathematically, column with
  // min_inline_size of X, and percentage Y% implies that the
  // total inline sum MINSUM must satisfy: MINSUM * Y% >= X.
  // 2) Let T% be sum of all percentages. Let M be sum of min_inline_sizes of
  // all non-percentage columns. Total min size sum MINSUM must satisfy:
  // T% * MINSUM + M = MINSUM.

  // Minimum total size estimate based on column's min_inline_size and percent.
  LayoutUnit percent_maxsize_estimate;
  // Sum of max_inline_sizes of non-percentage columns.
  LayoutUnit non_percent_maxsize_sum;
  float percent_sum = 0;
  for (const NGTableTypes::Column& column : column_constraints.data) {
    if (skip_collapsed_columns && column.is_collapsed)
      continue;
    if (column.min_inline_size) {
      // In fixed layout, constrained cells minimum inline size is their
      // maximum.
      if (is_fixed_layout && column.IsFixed()) {
        minmax.min_size += *column.max_inline_size;
      } else {
        minmax.min_size += *column.min_inline_size;
      }
      if (column.percent && *column.percent > 0) {
        if (*column.max_inline_size > LayoutUnit()) {
          LayoutUnit estimate = LayoutUnit(
              100 / *column.percent *
              (*column.max_inline_size - column.percent_border_padding));
          percent_maxsize_estimate =
              std::max(percent_maxsize_estimate, estimate);
        }
      } else {
        non_percent_maxsize_sum += *column.max_inline_size;
      }
    }
    if (column.max_inline_size)
      minmax.max_size += *column.max_inline_size;
    if (column.percent)
      percent_sum += *column.percent;
  }
  DCHECK_LE(percent_sum, 100.0f);

  // Table max inline size constraint can be computed from:
  // total column percentage combined with max_inline_size of nonpercent
  // columns.
  if (percent_sum > 0 && !containing_block_expects_minmax_without_percentages) {
    LayoutUnit size_from_percent_and_fixed;
    DCHECK_GE(percent_sum, 0.0f);
    if (non_percent_maxsize_sum != LayoutUnit()) {
      if (percent_sum == 100.0f) {
        size_from_percent_and_fixed = NGTableTypes::kTableMaxInlineSize;
      } else {
        size_from_percent_and_fixed =
            LayoutUnit((100 / (100 - percent_sum)) * non_percent_maxsize_sum);
      }
    }
    minmax.max_size = std::max(minmax.max_size, size_from_percent_and_fixed);
    minmax.max_size = std::max(minmax.max_size, percent_maxsize_estimate);
  }

  minmax.max_size = std::max(minmax.min_size, minmax.max_size);
  minmax += undistributable_space;
  return minmax;
}

void NGTableAlgorithmHelpers::DistributeColspanCellsToColumns(
    const NGTableTypes::ColspanCells& colspan_cells,
    LayoutUnit inline_border_spacing,
    bool is_fixed_layout,
    NGTableTypes::Columns* column_constraints) {
  for (const NGTableTypes::ColspanCell& colspan_cell : colspan_cells) {
    // Clipped colspanned cells can end up having a span of 1 (which is not
    // wide).
    DCHECK_GT(colspan_cell.span, 1u);

    if (is_fixed_layout) {
      DistributeColspanCellToColumnsFixed(colspan_cell, inline_border_spacing,
                                          column_constraints);
    } else {
      DistributeColspanCellToColumnsAuto(colspan_cell, inline_border_spacing,
                                         column_constraints);
    }
  }
}

// Standard: https://www.w3.org/TR/css-tables-3/#width-distribution-algorithm
// After synchroniziation, assignable table inline size and sum of column
// final inline sizes will be equal.
Vector<LayoutUnit>
NGTableAlgorithmHelpers::SynchronizeAssignableTableInlineSizeAndColumns(
    LayoutUnit assignable_table_inline_size,
    LayoutUnit inline_border_spacing,
    bool is_fixed_layout,
    const NGTableTypes::Columns& column_constraints) {
  if (column_constraints.data.IsEmpty())
    return Vector<LayoutUnit>();
  if (is_fixed_layout) {
    return SynchronizeAssignableTableInlineSizeAndColumnsFixed(
        assignable_table_inline_size, inline_border_spacing,
        column_constraints);
  } else {
    const NGTableTypes::Column* start_column = &column_constraints.data[0];
    const NGTableTypes::Column* end_column =
        start_column + column_constraints.data.size();
    return DistributeInlineSizeToComputedInlineSizeAuto(
        assignable_table_inline_size, inline_border_spacing, start_column,
        end_column);
  }
}

void NGTableAlgorithmHelpers::DistributeRowspanCellToRows(
    const NGTableTypes::RowspanCell& rowspan_cell,
    LayoutUnit border_block_spacing,
    NGTableTypes::Rows* rows) {
  DCHECK_GE(rowspan_cell.span, 0u);
  DistributeExcessBlockSizeToRows(
      rowspan_cell.start_row, rowspan_cell.span,
      rowspan_cell.cell_block_constraint.min_block_size,
      /* desired_block_size_is_rowspan */ true, border_block_spacing,
      kIndefiniteSize, rows);
}

// Legacy code ignores section block size.
void NGTableAlgorithmHelpers::DistributeSectionFixedBlockSizeToRows(
    const wtf_size_t start_row,
    const wtf_size_t rowspan,
    LayoutUnit section_fixed_block_size,
    LayoutUnit border_block_spacing,
    LayoutUnit percentage_resolution_block_size,
    NGTableTypes::Rows* rows) {
  DistributeExcessBlockSizeToRows(start_row, rowspan, section_fixed_block_size,
                                  /* desired_block_size_is_rowspan */ false,
                                  border_block_spacing,
                                  percentage_resolution_block_size, rows);
}

void NGTableAlgorithmHelpers::DistributeTableBlockSizeToSections(
    LayoutUnit border_block_spacing,
    LayoutUnit table_block_size,
    NGTableTypes::Sections* sections,
    NGTableTypes::Rows* rows) {
  if (sections->IsEmpty())
    return;
  // Redistribute table block size over sections algorithm:
  // 1. Compute section groups:
  //   Group 0: sections with 0-block size
  //   Group 1: sections with %-age block size not in Group 0
  //   Group 2: unconstrained tbody sections not in Group 0
  //   Group 3: all tbody sections not in Group 0
  //   Group 4: all sections not in Group 0
  //
  // 2. Percentage redistribution:
  //   Grow sections in group 1 up to their %ge block size
  //
  // 3. Final redistribution
  //   Pick first non-empty group between groups 4, 3, 2, and 0.
  //   Grow sections in picked group.
  //   Groups 4, 3, 2 grow proportiononaly to their block size.
  //   Group 0 grows evenly.
  unsigned block_space_count = sections->size() + 1;
  LayoutUnit undistributable_space = block_space_count * border_block_spacing;

  LayoutUnit distributable_table_block_size =
      std::max(LayoutUnit(), table_block_size - undistributable_space);
  bool has_growable_percent_sections = false;
  LayoutUnit desired_percentage_block_size_deficit;
  LayoutUnit total_group_block_sizes[5];
  unsigned number_of_empty_groups = 0;

  auto is_group_0 = [](auto& section) {
    return section.block_size == LayoutUnit();
  };
  auto is_group_1 = [](auto& section) {
    return section.percent.has_value() && section.percent != 0.0 &&
           section.block_size != LayoutUnit();
  };
  auto is_group_2 = [](auto& section) {
    return section.is_tbody && !section.is_constrained &&
           section.block_size != LayoutUnit();
  };
  auto is_group_3 = [](auto& section) {
    return section.is_tbody && section.block_size != LayoutUnit();
  };
  auto is_group_4 = [](auto& section) {
    return section.block_size != LayoutUnit();
  };

  auto update_block_sizes = [&total_group_block_sizes, &number_of_empty_groups,
                             &is_group_0, &is_group_2, &is_group_3,
                             &is_group_4](auto& section) {
    if (is_group_2(section))
      total_group_block_sizes[2] += section.block_size;
    if (is_group_3(section))
      total_group_block_sizes[3] += section.block_size;
    if (is_group_4(section))
      total_group_block_sizes[4] += section.block_size;
    if (is_group_0(section))
      number_of_empty_groups++;
  };

  for (NGTableTypes::Section& section : *sections) {
    section.needs_redistribution = false;
    update_block_sizes(section);
    if (is_group_1(section)) {
      has_growable_percent_sections = true;
      desired_percentage_block_size_deficit +=
          (LayoutUnit(*section.percent * distributable_table_block_size / 100) -
           section.block_size)
              .ClampNegativeToZero();
    }
  }
  LayoutUnit excess_block_size =
      distributable_table_block_size - total_group_block_sizes[4];
  if (excess_block_size <= LayoutUnit())
    return;

  // Step 1: Percentage redistribution: grow percentages to their maximum.
  if (has_growable_percent_sections) {
    // Because percentages will grow, need to recompute all the totals.
    total_group_block_sizes[2] = LayoutUnit();
    total_group_block_sizes[3] = LayoutUnit();
    total_group_block_sizes[4] = LayoutUnit();
    number_of_empty_groups = 0;
    float ratio = std::min(
        excess_block_size / desired_percentage_block_size_deficit.ToFloat(),
        1.0f);
    LayoutUnit remaining_deficit =
        LayoutUnit(ratio * desired_percentage_block_size_deficit);
    NGTableTypes::Section* last_section = nullptr;
    for (NGTableTypes::Section& section : *sections) {
      if (!is_group_1(section)) {
        update_block_sizes(section);
        continue;
      }
      LayoutUnit desired_block_size =
          LayoutUnit(*section.percent * distributable_table_block_size / 100);
      LayoutUnit block_size_deficit =
          (desired_block_size - section.block_size).ClampNegativeToZero();
      LayoutUnit grow_by = LayoutUnit(block_size_deficit * ratio);
      if (grow_by != LayoutUnit()) {
        section.block_size += grow_by;
        section.needs_redistribution = true;
        last_section = &section;
        remaining_deficit -= grow_by;
      }
      update_block_sizes(section);
    }
    if (last_section && remaining_deficit != LayoutUnit()) {
      last_section->block_size += remaining_deficit;
      if (is_group_4(*last_section))
        total_group_block_sizes[4] += remaining_deficit;
    }
  }
  excess_block_size =
      distributable_table_block_size - total_group_block_sizes[4];

  if (excess_block_size > LayoutUnit()) {
    // Step 2: distribute remaining block sizes to group 0, 2, 3, or 4.
    unsigned group_index;
    if (total_group_block_sizes[2] > LayoutUnit())
      group_index = 2;
    else if (total_group_block_sizes[3] > LayoutUnit())
      group_index = 3;
    else if (total_group_block_sizes[4] > LayoutUnit())
      group_index = 4;
    else
      group_index = 0;

    LayoutUnit remaining_deficit = excess_block_size;
    NGTableTypes::Section* last_section;
    for (NGTableTypes::Section& section : *sections) {
      if (group_index == 2 && !is_group_2(section))
        continue;
      if (group_index == 3 && !is_group_3(section))
        continue;
      if (group_index == 4 && !is_group_4(section))
        continue;
      LayoutUnit grow_by;
      if (group_index == 0) {
        grow_by =
            LayoutUnit(excess_block_size.ToFloat() / number_of_empty_groups);
      } else {
        grow_by = LayoutUnit(section.block_size.ToFloat() * excess_block_size /
                             total_group_block_sizes[group_index]);
      }
      if (grow_by > LayoutUnit()) {
        section.block_size += grow_by;
        section.needs_redistribution = true;
        remaining_deficit -= grow_by;
        last_section = &section;
      }
    }
    if (last_section && remaining_deficit != LayoutUnit()) {
      last_section->block_size += remaining_deficit;
    }
  }

  // Step 3: Propagate section expansion to rows.
  for (NGTableTypes::Section& section : *sections) {
    if (!section.needs_redistribution)
      continue;
    DistributeExcessBlockSizeToRows(
        section.start_row, section.rowspan, section.block_size,
        /* desired_block_size_is_rowspan */ false, border_block_spacing,
        section.block_size, rows);
  }
}

}  // namespace blink
