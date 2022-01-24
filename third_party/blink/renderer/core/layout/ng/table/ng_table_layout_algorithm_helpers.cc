// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_helpers.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_node.h"

namespace blink {

namespace {

// Implements spec distribution algorithm:
// https://www.w3.org/TR/css-tables-3/#width-distribution-algorithm
// |treat_target_size_as_constrained| constrained target can grow fixed-width
// columns. unconstrained target cannot grow fixed-width columns beyond
// specified size.
Vector<LayoutUnit> DistributeInlineSizeToComputedInlineSizeAuto(
    LayoutUnit target_inline_size,
    LayoutUnit inline_border_spacing,
    const NGTableTypes::Column* start_column,
    const NGTableTypes::Column* end_column,
    const bool treat_target_size_as_constrained) {
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

    if (column->is_mergeable) {
      ;  // Mergeable columns are ignored.
    } else if (column->percent) {
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
        if (column->is_mergeable)
          continue;
        *computed_size = column->min_inline_size.value_or(LayoutUnit());
      }
    } break;
    case kPercentageGuess: {
      // Percent columns grow in proportion to difference between their
      // percentage size and minimum size.
      // Auto/Fixed columns get min inline size.
      LayoutUnit percent_inline_size_increases =
          guess_size_total_increases[kPercentageGuess];
      LayoutUnit distributable_inline_size =
          target_inline_size - guess_sizes[kMinGuess];
      LayoutUnit rounding_error_inline_size = distributable_inline_size;
      LayoutUnit* computed_size = computed_sizes.begin();
      LayoutUnit* last_computed_size = nullptr;
      for (const NGTableTypes::Column* column = start_column;
           column != end_column; ++column, ++computed_size) {
        if (column->is_mergeable)
          continue;
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
        if (column->is_mergeable)
          continue;
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
        if (column->is_mergeable)
          continue;
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
          if (column->is_mergeable)
            continue;
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
      } else if (fixed_columns_count > 0 && treat_target_size_as_constrained) {
        // Grow fixed columns if available.
        LayoutUnit rounding_error_inline_size = distributable_inline_size;
        LayoutUnit* last_computed_size = nullptr;
        LayoutUnit* computed_size = computed_sizes.begin();
        for (const NGTableTypes::Column* column = start_column;
             column != end_column; ++column, ++computed_size) {
          if (column->is_mergeable)
            continue;
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
        // All remaining columns are percent.
        // They grow to max(col minimum, %ge size) + additional size
        // proportional to column percent.
        LayoutUnit rounding_error_inline_size = distributable_inline_size;
        LayoutUnit* last_computed_size = nullptr;
        LayoutUnit* computed_size = computed_sizes.begin();
        for (const NGTableTypes::Column* column = start_column;
             column != end_column; ++column, ++computed_size) {
          if (column->is_mergeable || !column->percent)
            continue;
          last_computed_size = computed_size;
          LayoutUnit percent_inline_size =
              column->ResolvePercentInlineSize(target_inline_size);
          LayoutUnit delta;
          if (total_percent != 0.0f) {
            delta = LayoutUnit(distributable_inline_size * *column->percent /
                               total_percent);
          } else {
            delta = LayoutUnit(distributable_inline_size.ToFloat() /
                               percent_columns_count);
          }
          rounding_error_inline_size -= delta;
          *computed_size = percent_inline_size + delta;
        }
        if (rounding_error_inline_size != LayoutUnit() && last_computed_size) {
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
  unsigned fixed_columns_count = 0;
  unsigned zero_inline_size_constrained_colums_count = 0;

  auto TreatAsFixed = [](const NGTableTypes::Column& column) {
    // Columns of width 0 are treated as auto by all browsers.
    return column.IsFixed() && column.max_inline_size != LayoutUnit();
  };

  auto IsZeroInlineSizeConstrained = [](const NGTableTypes::Column& column) {
    // Columns of width 0 are treated as auto by all browsers.
    return column.is_constrained && column.max_inline_size == LayoutUnit();
  };

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
      total_percent_inline_size +=
          column.ResolvePercentInlineSize(target_inline_size);
    } else if (TreatAsFixed(column)) {
      fixed_columns_count++;
      total_fixed_inline_size += column.max_inline_size.value_or(LayoutUnit());
    } else if (IsZeroInlineSizeConstrained(column)) {
      zero_inline_size_constrained_colums_count++;
    } else {
      auto_columns_count++;
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
      if (!TreatAsFixed(*column))
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
  // Distribute to auto, and zero inline size columns.
  LayoutUnit distributing_inline_size =
      target_inline_size - assigned_inline_size;
  LayoutUnit* column_size = column_sizes.begin();

  bool distribute_zero_inline_size =
      zero_inline_size_constrained_colums_count == all_columns_count;

  for (const NGTableTypes::Column* column = column_constraints.data.begin();
       column != column_constraints.data.end(); ++column, ++column_size) {
    if (column->percent || TreatAsFixed(*column))
      continue;
    // Zero-width columns only grow if all columns are zero-width.
    if (IsZeroInlineSizeConstrained(*column) && !distribute_zero_inline_size)
      continue;

    last_column_size = column_size;
    *column_size =
        LayoutUnit(distributing_inline_size /
                   float(distribute_zero_inline_size
                             ? zero_inline_size_constrained_colums_count
                             : auto_columns_count));
    assigned_inline_size += *column_size;
  }
  LayoutUnit delta = target_inline_size - assigned_inline_size;
  DCHECK(last_column_size);
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

  // Inline sizes for redistribution exclude border spacing.
  LayoutUnit total_inner_border_spacing;
  unsigned effective_span = 0;
  bool is_first_column = true;
  for (NGTableTypes::Column* column = start_column; column != end_column;
       ++column) {
    if (column->is_mergeable)
      continue;
    ++effective_span;
    if (!is_first_column)
      total_inner_border_spacing += inline_border_spacing;
    else
      is_first_column = false;
  }
  LayoutUnit colspan_cell_min_inline_size;
  LayoutUnit colspan_cell_max_inline_size;
  // Colspanned cells only distribute min inline size if constrained.
  if (colspan_cell.cell_inline_constraint.is_constrained) {
    colspan_cell_min_inline_size =
        (colspan_cell.cell_inline_constraint.min_inline_size -
         total_inner_border_spacing)
            .ClampNegativeToZero();
  }
  colspan_cell_max_inline_size =
      (colspan_cell.cell_inline_constraint.max_inline_size -
       total_inner_border_spacing)
          .ClampNegativeToZero();

  // Distribute min/max evenly between all cells.
  LayoutUnit rounding_error_min_inline_size = colspan_cell_min_inline_size;
  LayoutUnit rounding_error_max_inline_size = colspan_cell_max_inline_size;

  LayoutUnit new_min_size = LayoutUnit(colspan_cell_min_inline_size /
                                       static_cast<float>(effective_span));
  LayoutUnit new_max_size = LayoutUnit(colspan_cell_max_inline_size /
                                       static_cast<float>(effective_span));
  absl::optional<float> new_percent;
  if (colspan_cell.cell_inline_constraint.percent) {
    new_percent = *colspan_cell.cell_inline_constraint.percent / effective_span;
  }

  NGTableTypes::Column* last_column = nullptr;
  for (NGTableTypes::Column* column = start_column; column < end_column;
       ++column) {
    if (column->is_mergeable)
      continue;
    last_column = column;
    rounding_error_min_inline_size -= new_min_size;
    rounding_error_max_inline_size -= new_max_size;

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
    // Percentages only get distributed over auto columns.
    if (!column->percent && !column->is_constrained && new_percent) {
      column->percent = *new_percent;
    }
  }
  DCHECK(last_column);
  last_column->min_inline_size =
      *last_column->min_inline_size + rounding_error_min_inline_size;
  last_column->max_inline_size =
      *last_column->max_inline_size + rounding_error_max_inline_size;
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
  LayoutUnit total_inner_border_spacing;
  bool is_first_column = true;
  for (NGTableTypes::Column* column = start_column; column != end_column;
       ++column) {
    if (!column->is_mergeable) {
      if (!is_first_column)
        total_inner_border_spacing += inline_border_spacing;
      else
        is_first_column = false;
    }
  }

  LayoutUnit colspan_cell_min_inline_size =
      (colspan_cell.cell_inline_constraint.min_inline_size -
       total_inner_border_spacing)
          .ClampNegativeToZero();
  LayoutUnit colspan_cell_max_inline_size =
      (colspan_cell.cell_inline_constraint.max_inline_size -
       total_inner_border_spacing)
          .ClampNegativeToZero();
  absl::optional<float> colspan_cell_percent =
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
      if (column->is_mergeable)
        continue;
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
        if (column->percent || column->is_mergeable)
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
      DistributeInlineSizeToComputedInlineSizeAuto(
          colspan_cell_min_inline_size, inline_border_spacing, start_column,
          end_column, true);
  LayoutUnit* computed_size = computed_sizes.begin();
  for (NGTableTypes::Column* column = start_column; column != end_column;
       ++column, ++computed_size) {
    column->min_inline_size =
        std::max(*column->min_inline_size, *computed_size);
  }
  computed_sizes = DistributeInlineSizeToComputedInlineSizeAuto(
      colspan_cell_max_inline_size, inline_border_spacing, start_column,
      end_column, /* treat_target_size_as_constrained */
      colspan_cell.cell_inline_constraint.is_constrained);
  computed_size = computed_sizes.begin();
  for (NGTableTypes::Column* column = start_column; column != end_column;
       ++column, ++computed_size) {
    column->max_inline_size =
        std::max(std::max(*column->min_inline_size, *column->max_inline_size),
                 *computed_size);
  }
}

// Handles distribution of excess block size from: table, sections,
// rows, and rowspanned cells, to rows.
// Rowspanned cells distribute with slight differences from
// general distribution algorithm.
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
                                 const NGTableTypes::Row* row) {
    if (percentage_resolution_block_size == kIndefiniteSize)
      return LayoutUnit();
    DCHECK(row->percent);
    return (LayoutUnit(*row->percent * percentage_resolution_block_size / 100) -
            row->block_size)
        .ClampNegativeToZero();
  };

  auto IsUnconstrainedNonEmptyRow =
      [&percentage_resolution_block_size](const NGTableTypes::Row* row) {
        if (row->block_size == LayoutUnit())
          return false;
        if (row->percent && percentage_resolution_block_size == kIndefiniteSize)
          return true;
        return !row->is_constrained;
      };

  auto IsRowWithOriginatingRowspan =
      [&start_row,
       &desired_block_size_is_rowspan](const NGTableTypes::Row* row) {
        // Rowspans are treated specially only during rowspan distribution.
        return desired_block_size_is_rowspan && row != start_row &&
               row->has_rowspan_start;
      };

  auto IsEmptyRow =
      [&percentage_resolution_block_size](const NGTableTypes::Row* row) {
        bool is_percent = percentage_resolution_block_size != kIndefiniteSize &&
                          row->percent && *row->percent != 0;
        return row->block_size == LayoutUnit() && !is_percent;
      };

  unsigned percent_rows_with_deficit_count = 0;
  unsigned rows_with_originating_rowspan = 0;
  unsigned unconstrained_non_empty_row_count = 0;
  unsigned constrained_non_empty_row_count = 0;
  unsigned empty_row_count = 0;
  unsigned unconstrained_empty_row_count = 0;

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
    } else if (row->is_constrained && !IsEmptyRow(row)) {
      constrained_non_empty_row_count++;
    }
    if (IsEmptyRow(row)) {
      empty_row_count++;
      if (!row->is_constrained)
        unconstrained_empty_row_count++;
    }
  }

  LayoutUnit distributable_block_size =
      (desired_block_size - border_block_spacing * (row_count - 1)) -
      total_block_size;
  if (distributable_block_size <= LayoutUnit())
    return;

  // Step 1: percentage rows grow to no more than their percentage size.
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
    last_row->block_size = std::max(last_row->block_size, LayoutUnit());
    distributed_block_size += remaining_deficit;
    // Rounding errors might cause us to distribute more than available length.
    distributed_block_size =
        std::min(distributed_block_size, distributable_block_size);
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
    last_row->block_size = std::max(last_row->block_size, LayoutUnit());
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
    last_row->block_size = std::max(last_row->block_size, LayoutUnit());
    return;
  }

  // Step 4: Empty row distribution
  // At this point all rows are empty and/or constrained.
  if (empty_row_count > 0) {
    if (desired_block_size_is_rowspan) {
      NGTableTypes::Row* last_row = nullptr;
      NGTableTypes::Row* row = start_row;
      // Rowspan distribution skips initial empty row if possible,
      // and distributes everything to the last empty row.
      if (empty_row_count != row_count)
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
      // Grow empty rows if either of these is true:
      // - all rows are empty.
      // - non-empty rows are all constrained.
      NGTableTypes::Row* last_row = nullptr;
      LayoutUnit remaining_deficit = distributable_block_size;
      // If there are constrained and unconstrained empty rows,
      // only unconstrained rows grow.
      bool grow_only_unconstrained = unconstrained_empty_row_count > 0;
      unsigned growing_rows_count = grow_only_unconstrained
                                        ? unconstrained_empty_row_count
                                        : empty_row_count;
      for (NGTableTypes::Row* row = start_row; row != end_row; ++row) {
        if (row->block_size != LayoutUnit())
          continue;
        if (grow_only_unconstrained && row->is_constrained)
          continue;
        last_row = row;
        LayoutUnit delta =
            LayoutUnit(distributable_block_size.ToFloat() / growing_rows_count);
        row->block_size = delta;
        remaining_deficit -= delta;
      }
      DCHECK(last_row);
      last_row->block_size += remaining_deficit;
      last_row->block_size = std::max(last_row->block_size, LayoutUnit());
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
  if (last_row) {
    last_row->block_size += remaining_deficit;
    last_row->block_size = std::max(last_row->block_size, LayoutUnit());
  }
}

}  // namespace

MinMaxSizes NGTableAlgorithmHelpers::ComputeGridInlineMinMax(
    const NGTableNode& node,
    const NGTableTypes::Columns& column_constraints,
    LayoutUnit undistributable_space,
    bool is_fixed_layout,
    bool is_layout_pass,
    bool skip_collapsed_columns) {
  MinMaxSizes min_max;
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
  LayoutUnit percent_max_size_estimate;
  // Sum of max_inline_sizes of non-percentage columns.
  LayoutUnit non_percent_max_size_sum;
  float percent_sum = 0;
  for (const NGTableTypes::Column& column : column_constraints.data) {
    if (skip_collapsed_columns && column.is_collapsed)
      continue;
    if (column.min_inline_size) {
      // In fixed layout, constrained cells minimum inline size is their
      // maximum.
      if (is_fixed_layout && column.IsFixed()) {
        min_max.min_size += *column.max_inline_size;
      } else {
        min_max.min_size += *column.min_inline_size;
      }
      if (column.percent && *column.percent > 0) {
        if (*column.max_inline_size > LayoutUnit()) {
          LayoutUnit estimate = LayoutUnit(
              100 / *column.percent *
              (*column.max_inline_size - column.percent_border_padding));
          percent_max_size_estimate =
              std::max(percent_max_size_estimate, estimate);
        }
      } else {
        non_percent_max_size_sum += *column.max_inline_size;
      }
    }
    if (column.max_inline_size)
      min_max.max_size += *column.max_inline_size;
    if (column.percent)
      percent_sum += *column.percent;
  }
  // Floating point math can cause total sum to be slightly above 100%.
  DCHECK_LE(percent_sum, 100.5f);
  percent_sum = std::min(percent_sum, 100.0f);

  // Table max inline size constraint can be computed from the total column
  // percentage combined with max_inline_size of non-percent columns.
  if (percent_sum > 0 && node.AllowColumnPercentages(is_layout_pass)) {
    LayoutUnit size_from_percent_and_fixed;
    DCHECK_GE(percent_sum, 0.0f);
    if (non_percent_max_size_sum != LayoutUnit()) {
      if (percent_sum == 100.0f) {
        size_from_percent_and_fixed = NGTableTypes::kTableMaxInlineSize;
      } else {
        size_from_percent_and_fixed =
            LayoutUnit((100 / (100 - percent_sum)) * non_percent_max_size_sum);
      }
    }
    min_max.max_size = std::max(min_max.max_size, size_from_percent_and_fixed);
    min_max.max_size = std::max(min_max.max_size, percent_max_size_estimate);
  }

  min_max.max_size = std::max(min_max.min_size, min_max.max_size);
  min_max += undistributable_space;
  return min_max;
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
        end_column, /* treat_target_size_as_constrained */ true);
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
  // Compute section size guesses:
  // min_guess_sum is sum of section sizes
  // percentage_guess_sum is sum of kMinGuess + percentage guesses

  // if table_block_size <= min_guess_sum, there is nothing to distribute.

  // 1. if table_block_size > min_guess_sum distribute size to
  //    percentage sections.
  //    Sections grow in proportion to difference between their percentage
  //    size and min size.
  //
  // 2. if table_block_size > percentage_guess_sum distribute size to
  //    eligible sections.
  //    Eligible sections:
  //      if TBODY sections exist, only TBODY sections are eligible.
  //      otherwise, all sections are eligible.
  //
  //    - grow auto eligible sections in proportion to their size
  //    - grow fixed eligible sections in proportion to their size
  //    - grow percentage eligible sections in proportion to their size

  unsigned block_space_count = sections->size() + 1;
  LayoutUnit undistributable_space = block_space_count * border_block_spacing;

  LayoutUnit distributable_table_block_size =
      std::max(LayoutUnit(), table_block_size - undistributable_space);

  auto ComputePercentageSize = [&distributable_table_block_size](
                                   auto& section) {
    DCHECK(section.percent.has_value());
    return std::max(
        section.block_size,
        LayoutUnit(*section.percent * distributable_table_block_size / 100));
  };

  LayoutUnit auto_sections_size;
  LayoutUnit fixed_sections_size;
  LayoutUnit percent_sections_size;
  LayoutUnit tbody_auto_sections_size;
  LayoutUnit tbody_fixed_sections_size;
  LayoutUnit tbody_percent_sections_size;
  LayoutUnit minimum_size_guess;
  LayoutUnit percent_size_guess;

  unsigned auto_sections_count = 0;
  unsigned fixed_sections_count = 0;
  unsigned percent_sections_count = 0;
  unsigned tbody_auto_sections_count = 0;
  unsigned tbody_fixed_sections_count = 0;
  unsigned tbody_percent_sections_count = 0;

  for (const NGTableTypes::Section& section : *sections) {
    minimum_size_guess += section.block_size;
    if (section.percent.has_value())
      percent_size_guess += ComputePercentageSize(section);
    else
      percent_size_guess += section.block_size;

    if (section.is_constrained) {
      if (section.percent.has_value()) {
        percent_sections_count++;
        if (section.is_tbody)
          tbody_percent_sections_count++;
      } else {
        fixed_sections_count++;
        fixed_sections_size += section.block_size;
        if (section.is_tbody) {
          tbody_fixed_sections_size += section.block_size;
          tbody_fixed_sections_count++;
        }
      }
    } else {
      auto_sections_count++;
      auto_sections_size += section.block_size;
      if (section.is_tbody) {
        tbody_auto_sections_count++;
        tbody_auto_sections_size += section.block_size;
      }
    }
  }

  if (distributable_table_block_size <= minimum_size_guess)
    return;

  LayoutUnit current_sections_size = minimum_size_guess;

  // Distribute to percent sections.
  if (percent_sections_count > 0 && percent_size_guess > minimum_size_guess) {
    LayoutUnit distributable_size =
        std::min(percent_size_guess, distributable_table_block_size) -
        minimum_size_guess;
    DCHECK_GE(distributable_size, LayoutUnit());
    LayoutUnit percent_minimum_difference =
        percent_size_guess - minimum_size_guess;

    LayoutUnit rounding_error_tally = distributable_size;
    NGTableTypes::Section* last_section = nullptr;
    for (NGTableTypes::Section& section : *sections) {
      if (!section.percent)
        continue;
      LayoutUnit delta = LayoutUnit(
          distributable_size *
          (ComputePercentageSize(section).ToFloat() - section.block_size) /
          percent_minimum_difference);
      section.block_size += delta;
      section.needs_redistribution = true;
      rounding_error_tally -= delta;
      current_sections_size += delta;
      last_section = &section;
      percent_sections_size += section.block_size;
      if (section.is_tbody)
        tbody_percent_sections_size += section.block_size;
    }
    DCHECK_LT(rounding_error_tally,
              LayoutUnit(1));  // DO NOT CHECK IN, cluster fuzz magnet
    DCHECK(last_section);
    last_section->block_size += rounding_error_tally;
    percent_sections_size += rounding_error_tally;
    current_sections_size += rounding_error_tally;
    if (last_section->is_tbody)
      percent_sections_size += rounding_error_tally;
  }

  // Distribute remaining sizes.
  bool has_tbody = tbody_auto_sections_count > 0 ||
                   tbody_fixed_sections_count > 0 ||
                   tbody_percent_sections_count > 0;
  LayoutUnit distributable_size =
      distributable_table_block_size - current_sections_size;
  if (distributable_size > LayoutUnit()) {
    LayoutUnit rounding_error_tally = distributable_size;
    if ((tbody_auto_sections_count > 0) ||
        (!has_tbody && auto_sections_count > 0)) {
      // Distribute to auto sections.
      // Sections grow by ratio of their size / total auto sizes.
      NGTableTypes::Section* last_section = nullptr;
      LayoutUnit total_auto_size =
          has_tbody ? tbody_auto_sections_size : auto_sections_size;
      for (NGTableTypes::Section& section : *sections) {
        if (section.is_constrained || (section.is_tbody != has_tbody))
          continue;
        LayoutUnit delta;
        if (total_auto_size > LayoutUnit()) {
          delta = LayoutUnit(distributable_size.ToFloat() * section.block_size /
                             total_auto_size);
        } else {
          delta = LayoutUnit(
              distributable_size.ToFloat() /
              (has_tbody ? tbody_auto_sections_count : auto_sections_count));
        }
        section.block_size += delta;
        section.needs_redistribution = true;
        rounding_error_tally -= delta;
        last_section = &section;
      }
      DCHECK(last_section);
      last_section->block_size += rounding_error_tally;
    } else if ((tbody_fixed_sections_count > 0) ||
               (!has_tbody && fixed_sections_count > 0)) {
      // Distribute to fixed sections.
      // Sections grow  by ration of their size / total fixed sizes.
      NGTableTypes::Section* last_section = nullptr;
      LayoutUnit total_fixed_size =
          has_tbody ? tbody_fixed_sections_size : fixed_sections_size;
      for (NGTableTypes::Section& section : *sections) {
        if (!section.is_constrained || section.percent.has_value())
          continue;
        if (section.is_tbody != has_tbody)
          continue;
        LayoutUnit delta;
        if (total_fixed_size > LayoutUnit()) {
          delta = LayoutUnit(distributable_size.ToFloat() * section.block_size /
                             total_fixed_size);
        } else {
          delta = LayoutUnit(
              distributable_size.ToFloat() /
              (has_tbody ? tbody_fixed_sections_count : fixed_sections_count));
        }
        section.block_size += delta;
        section.needs_redistribution = true;
        rounding_error_tally -= delta;
        last_section = &section;
      }
      DCHECK(last_section);
      last_section->block_size += rounding_error_tally;
    } else {
      DCHECK((tbody_percent_sections_count > 0) ||
             (!has_tbody && percent_sections_count > 0));
      // Distribute to percentage sections.
      NGTableTypes::Section* last_section = nullptr;
      LayoutUnit total_percent_size =
          has_tbody ? tbody_percent_sections_size : percent_sections_size;
      for (NGTableTypes::Section& section : *sections) {
        if (!section.percent.has_value())
          continue;
        if (section.is_tbody != has_tbody)
          continue;
        LayoutUnit delta;
        if (total_percent_size > LayoutUnit()) {
          delta = LayoutUnit(distributable_size.ToFloat() * section.block_size /
                             total_percent_size);
        } else {
          delta = LayoutUnit(distributable_size.ToFloat() /
                             (has_tbody ? tbody_percent_sections_count
                                        : percent_sections_count));
        }
        section.block_size += delta;
        section.needs_redistribution = true;
        rounding_error_tally -= delta;
        last_section = &section;
      }
      DCHECK(last_section);
      last_section->block_size += rounding_error_tally;
    }
  }
  // Propagate new section sizes to rows.
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
