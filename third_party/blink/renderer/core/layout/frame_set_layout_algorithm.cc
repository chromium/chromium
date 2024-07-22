// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/frame_set_layout_algorithm.h"

#include "third_party/blink/renderer/core/html/html_frame_set_element.h"
#include "third_party/blink/renderer/core/layout/disable_layout_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/frame_set_layout_data.h"

namespace blink {

namespace {

// This function never produces fractional values.
// LayoutUnit(int) produces fractional values if the argument is greater
// than LayoutUnit::kIntMax or smaller than LayoutUnit::kIntMin.
// FrameSetLayoutAlgorithm always requires integers.
LayoutUnit IntLayoutUnit(double value) {
  if (value >= LayoutUnit::kIntMax) {
    return LayoutUnit(LayoutUnit::kIntMax);
  }
  if (value <= LayoutUnit::kIntMin) {
    return LayoutUnit(LayoutUnit::kIntMin);
  }
  return LayoutUnit(floor(value));
}

// Adjusts proportionally the size with remaining size.
LayoutUnit AdjustSizeToRemainingSize(LayoutUnit current,
                                     LayoutUnit remaining,
                                     int64_t total) {
  // Performs the math operations step by step to avoid the overflow.
  base::CheckedNumeric<int64_t> temp_product = current.ToInt();
  temp_product *= remaining.ToInt();
  temp_product /= total;
  return LayoutUnit(base::checked_cast<int>(temp_product.ValueOrDie()));
}

}  // namespace

FrameSetLayoutAlgorithm::FrameSetLayoutAlgorithm(
    const LayoutAlgorithmParams& params)
    : LayoutAlgorithm<BlockNode, BoxFragmentBuilder, BlockBreakToken>(params) {}

const LayoutResult* FrameSetLayoutAlgorithm::Layout() {
  auto& frame_set = *To<HTMLFrameSetElement>(Node().GetDOMNode());
  auto layout_data = std::make_unique<FrameSetLayoutData>();
  layout_data->border_thickness = frame_set.Border(Style());
  layout_data->has_border_color = frame_set.HasBorderColor();
  layout_data->row_allow_border = frame_set.AllowBorderRows();
  layout_data->col_allow_border = frame_set.AllowBorderColumns();

  PhysicalSize size = ToPhysicalSize(container_builder_.Size(),
                                     GetConstraintSpace().GetWritingMode());
  const wtf_size_t row_count = frame_set.TotalRows();
  layout_data->row_sizes =
      LayoutAxis(row_count, frame_set.RowLengths(), frame_set.RowDeltas(),
                 size.height - (row_count - 1) * layout_data->border_thickness);
  const wtf_size_t col_count = frame_set.TotalCols();
  layout_data->col_sizes =
      LayoutAxis(col_count, frame_set.ColLengths(), frame_set.ColDeltas(),
                 size.width - (col_count - 1) * layout_data->border_thickness);

  LayoutChildren(*layout_data);

  container_builder_.TransferFrameSetLayoutData(std::move(layout_data));
  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult FrameSetLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  MinMaxSizes sizes;
  const auto& space = GetConstraintSpace();
  // This function needs to return a value which is >= border+padding in order
  // to pass a DCHECK in FlexLayoutAlgorithm::ConstructAndAppendFlexItems()
  // though <frameset> ignores border and padding.
  //
  // We can't use BorderPadding() here because FragmentGeometry for <frameset>
  // doesn't provide it.
  //
  // Test: external/wpt/css/css-flexbox/frameset-crash.html
  sizes += (ComputeBorders(space, Node()) + ComputePadding(space, Style()))
               .InlineSum();
  return MinMaxSizesResult(sizes, false);
}

// https://html.spec.whatwg.org/C/#convert-a-list-of-dimensions-to-a-list-of-pixel-values
Vector<LayoutUnit> FrameSetLayoutAlgorithm::LayoutAxis(
    wtf_size_t count,
    const Vector<HTMLDimension>& grid,
    const Vector<int>& deltas,
    LayoutUnit available_length) {
  DCHECK_GT(count, 0u);
  DCHECK_EQ(count, deltas.size());
  available_length = LayoutUnit(available_length.ToInt()).ClampNegativeToZero();
  Vector<LayoutUnit> sizes(count);

  if (grid.empty()) {
    sizes[0] = available_length;
    return sizes;
  }

  // First we need to investigate how many columns of each type we have and
  // how much space these columns are going to require.

  Vector<wtf_size_t, 4> fixed_indices;
  Vector<wtf_size_t, 4> percent_indices;
  Vector<wtf_size_t, 4> relative_indices;
  for (wtf_size_t i = 0; i < count; ++i) {
    if (grid[i].IsAbsolute())
      fixed_indices.push_back(i);
    else if (grid[i].IsPercentage())
      percent_indices.push_back(i);
    else if (grid[i].IsRelative())
      relative_indices.push_back(i);
  }

  int64_t total_relative = 0;
  int64_t total_fixed = 0;
  int64_t total_percent = 0;

  const float effective_zoom = Node().Style().EffectiveZoom();

  // Count the total length of all of the fixed columns/rows.
  for (auto i : fixed_indices) {
    sizes[i] =
        IntLayoutUnit(grid[i].Value() * effective_zoom).ClampNegativeToZero();
    DCHECK(sizes[i].IsInteger());
    total_fixed += sizes[i].ToInt();
  }

  // Count the total percentage of all of the percentage columns/rows.
  for (auto i : percent_indices) {
    sizes[i] = IntLayoutUnit(grid[i].Value() * available_length / 100.0)
                   .ClampNegativeToZero();
    DCHECK(sizes[i].IsInteger()) << sizes[i];
    total_percent += sizes[i].ToInt();
  }

  // Count the total relative of all the relative columns/rows.
  for (auto i : relative_indices)
    total_relative += ClampTo<int>(std::max(grid[i].Value(), 1.0));

  LayoutUnit remaining_length = available_length;

  // Fixed columns/rows are our first priority. If there is not enough space to
  // fit all fixed columns/rows we need to proportionally adjust their size.
  if (total_fixed > remaining_length.ToInt()) {
    LayoutUnit remaining_fixed = remaining_length;
    for (auto i : fixed_indices) {
      sizes[i] =
          AdjustSizeToRemainingSize(sizes[i], remaining_fixed, total_fixed);
      remaining_length -= sizes[i];
    }
  } else {
    remaining_length -= total_fixed;
  }

  // Percentage columns/rows are our second priority. Divide the remaining space
  // proportionally over all percentage columns/rows.
  // NOTE: the size of each column/row is not relative to 100%, but to the total
  // percentage. For example, if there are three columns, each of 75%, and the
  // available space is 300px, each column will become 100px in width.
  if (total_percent > remaining_length.ToInt()) {
    LayoutUnit remaining_percent = remaining_length;
    for (auto i : percent_indices) {
      sizes[i] =
          AdjustSizeToRemainingSize(sizes[i], remaining_percent, total_percent);
      remaining_length -= sizes[i];
    }
  } else {
    remaining_length -= total_percent;
  }

  // Relative columns/rows are our last priority. Divide the remaining space
  // proportionally over all relative columns/rows.
  // NOTE: the relative value of 0* is treated as 1*.
  if (!relative_indices.empty()) {
    wtf_size_t last_relative_index = WTF::kNotFound;
    int64_t remaining_relative = remaining_length.ToInt();
    for (auto i : relative_indices) {
      sizes[i] = IntLayoutUnit(
          (ClampTo<int>(std::max(grid[i].Value(), 1.)) * remaining_relative) /
          total_relative);
      remaining_length -= sizes[i];
      last_relative_index = i;
    }

    // If we could not evenly distribute the available space of all of the
    // relative columns/rows, the remainder will be added to the last column/
    // row. For example: if we have a space of 100px and three columns (*,*,*),
    // the remainder will be 1px and will be added to the last column: 33px,
    // 33px, 34px.
    if (remaining_length) {
      sizes[last_relative_index] += remaining_length;
      remaining_length = LayoutUnit();
    }
  }

  // If we still have some left over space we need to divide it over the already
  // existing columns/rows
  if (remaining_length) {
    // Our first priority is to spread if over the percentage columns. The
    // remaining space is spread evenly, for example: if we have a space of
    // 100px, the columns definition of 25%,25% used to result in two columns of
    // 25px. After this the columns will each be 50px in width.
    if (!percent_indices.empty() && total_percent) {
      LayoutUnit remaining_percent = remaining_length;
      for (auto i : percent_indices) {
        LayoutUnit change_percent = AdjustSizeToRemainingSize(
            sizes[i], remaining_percent, total_percent);
        sizes[i] += change_percent;
        remaining_length -= change_percent;
      }
    } else if (total_fixed) {
      // Our last priority is to spread the remaining space over the fixed
      // columns. For example if we have 100px of space and two column of each
      // 40px, both columns will become exactly 50px.
      LayoutUnit remaining_fixed = remaining_length;
      for (auto i : fixed_indices) {
        LayoutUnit change_fixed =
            AdjustSizeToRemainingSize(sizes[i], remaining_fixed, total_fixed);
        sizes[i] += change_fixed;
        remaining_length -= change_fixed;
      }
    }
  }

  // If we still have some left over space we probably ended up with a remainder
  // of a division. We cannot spread it evenly anymore. If we have any
  // percentage columns/rows simply spread the remainder equally over all
  // available percentage columns, regardless of their size.
  if (remaining_length && !percent_indices.empty()) {
    LayoutUnit remaining_percent = remaining_length;
    for (auto i : percent_indices) {
      int change_percent = (remaining_percent / percent_indices.size()).ToInt();
      sizes[i] += change_percent;
      remaining_length -= change_percent;
    }
  } else if (remaining_length && !fixed_indices.empty()) {
    // If we don't have any percentage columns/rows we only have fixed columns.
    // Spread the remainder equally over all fixed columns/rows.
    LayoutUnit remaining_fixed = remaining_length;
    for (auto i : fixed_indices) {
      int change_fixed = (remaining_fixed / fixed_indices.size()).ToInt();
      sizes[i] += change_fixed;
      remaining_length -= change_fixed;
    }
  }

  // Still some left over. Add it to the last column, because it is impossible
  // spread it evenly or equally.
  if (remaining_length)
    sizes[count - 1] += remaining_length;

  // Now we have the final layout, distribute the delta over it.
  bool worked = true;
  for (wtf_size_t i = 0; i < count; ++i) {
    if (sizes[i] && sizes[i] + deltas[i] <= 0)
      worked = false;
    sizes[i] += deltas[i];
  }
  // If the deltas broke something, undo them.
  if (!worked) {
    for (wtf_size_t i = 0; i < count; ++i)
      sizes[i] -= deltas[i];
  }

  return sizes;
}

void FrameSetLayoutAlgorithm::LayoutChildren(
    const FrameSetLayoutData& layout_data) {
  PhysicalOffset position;
  LayoutInputNode child = Node().FirstChild();
  if (!child)
    return;
  for (wtf_size_t row = 0; row < layout_data.row_sizes.size(); ++row) {
    position.left = LayoutUnit();
    const LayoutUnit row_size = layout_data.row_sizes[row];
    for (wtf_size_t col = 0; col < layout_data.col_sizes.size(); ++col) {
      const LayoutUnit col_size = layout_data.col_sizes[col];
      const LogicalSize available_size(
          Style().GetWritingDirection().IsHorizontal()
              ? LogicalSize(col_size, row_size)
              : LogicalSize(row_size, col_size));
      LayoutChild(child, available_size, position,
                  PhysicalSize(col_size, row_size));
      child = child.NextSibling();
      if (!child)
        return;
      position.left += col_size + layout_data.border_thickness;
    }
    position.top += row_size + layout_data.border_thickness;
  }
  // We have more children than what's defined by the frameset's grid. We want
  // those to generate fragments as well, so that LayoutBox traversal code can
  // generally assume that each box has at least one fragment. Give them zero
  // size and they'll show up nowhere.
  while (child) {
    LayoutChild(child, /* available_size */ LogicalSize(),
                /* position */ PhysicalOffset(),
                /* child_size */ PhysicalSize());
    child = child.NextSibling();
  }
}

void FrameSetLayoutAlgorithm::LayoutChild(const LayoutInputNode& child,
                                          LogicalSize available_size,
                                          PhysicalOffset position,
                                          PhysicalSize child_size) {
  const PhysicalSize frameset_size = ToPhysicalSize(
      container_builder_.Size(), GetConstraintSpace().GetWritingMode());
  const auto container_direction = Style().GetWritingDirection();
  const bool kNewFormattingContext = true;
  ConstraintSpaceBuilder space_builder(container_direction.GetWritingMode(),
                                       child.Style().GetWritingDirection(),
                                       kNewFormattingContext);
  space_builder.SetAvailableSize(available_size);
  space_builder.SetIsFixedInlineSize(true);
  space_builder.SetIsFixedBlockSize(true);
  const LayoutResult* result =
      To<BlockNode>(child).Layout(space_builder.ToConstraintSpace());
  container_builder_.AddResult(
      *result, position.ConvertToLogical(container_direction, frameset_size,
                                         child_size));
}

}  // namespace blink
