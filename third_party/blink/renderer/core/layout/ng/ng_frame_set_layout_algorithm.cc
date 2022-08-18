// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_frame_set_layout_algorithm.h"

#include "third_party/blink/renderer/core/html/html_frame_set_element.h"
#include "third_party/blink/renderer/core/layout/ng/frame_set_layout_data.h"

namespace blink {

namespace {

// This function never produces fractional values.
// LayoutUnit(int) produces fractional values if the argument is greater
// than kIntMaxForLayoutUnit or smaller than kIntMinForLayoutUnit.
// NGFrameSetLayoutAlgorithm always requires integers.
LayoutUnit IntLayoutUnit(double value) {
  if (value >= kIntMaxForLayoutUnit)
    return LayoutUnit(kIntMaxForLayoutUnit);
  if (value <= kIntMinForLayoutUnit)
    return LayoutUnit(kIntMinForLayoutUnit);
  return LayoutUnit(floor(value));
}

}  // namespace

NGFrameSetLayoutAlgorithm::NGFrameSetLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm<NGBlockNode, NGBoxFragmentBuilder, NGBlockBreakToken>(
          params) {}

const NGLayoutResult* NGFrameSetLayoutAlgorithm::Layout() {
  auto& frame_set = *To<HTMLFrameSetElement>(Node().GetDOMNode());
  auto layout_data = std::make_unique<FrameSetLayoutData>();
  layout_data->border_thickness = frame_set.Border(Style());
  layout_data->has_border_color = frame_set.HasBorderColor();
  layout_data->row_allow_border = frame_set.AllowBorderRows();
  layout_data->col_allow_border = frame_set.AllowBorderColumns();

  PhysicalSize size = ToPhysicalSize(container_builder_.Size(),
                                     ConstraintSpace().GetWritingMode());
  const wtf_size_t row_count = frame_set.TotalRows();
  layout_data->row_sizes =
      LayoutAxis(row_count, frame_set.RowLengths(), frame_set.RowDeltas(),
                 size.height - (row_count - 1) * layout_data->border_thickness);
  const wtf_size_t col_count = frame_set.TotalCols();
  layout_data->col_sizes =
      LayoutAxis(col_count, frame_set.ColLengths(), frame_set.ColDeltas(),
                 size.width - (col_count - 1) * layout_data->border_thickness);

  // TODO(crbug.com/1346221): Layout children.

  container_builder_.TransferFrameSetLayoutData(std::move(layout_data));
  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGFrameSetLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  return MinMaxSizesResult(MinMaxSizes(), false);
}

// https://html.spec.whatwg.org/C/#convert-a-list-of-dimensions-to-a-list-of-pixel-values
Vector<LayoutUnit> NGFrameSetLayoutAlgorithm::LayoutAxis(
    wtf_size_t count,
    const Vector<HTMLDimension>& grid,
    const Vector<int>& deltas,
    LayoutUnit available_length) {
  DCHECK_GT(count, 0u);
  DCHECK_EQ(count, deltas.size());
  available_length = LayoutUnit(available_length.ToInt()).ClampNegativeToZero();
  Vector<LayoutUnit> sizes(count);

  if (grid.IsEmpty()) {
    sizes[0] = available_length;
    return sizes;
  }

  [[maybe_unused]] int64_t total_relative = 0;
  [[maybe_unused]] int64_t total_fixed = 0;
  [[maybe_unused]] int64_t total_percent = 0;
  [[maybe_unused]] wtf_size_t count_relative = 0;
  [[maybe_unused]] wtf_size_t count_fixed = 0;
  [[maybe_unused]] wtf_size_t count_percent = 0;

  const float effective_zoom = Node().Style().EffectiveZoom();

  // First we need to investigate how many columns of each type we have and
  // how much space these columns are going to require.
  for (wtf_size_t i = 0; i < count; ++i) {
    // Count the total length of all of the fixed columns/rows -> total_fixed.
    // Count the number of columns/rows which are fixed -> count_fixed.
    if (grid[i].IsAbsolute()) {
      sizes[i] =
          IntLayoutUnit(grid[i].Value() * effective_zoom).ClampNegativeToZero();
      DCHECK(IsIntegerValue(sizes[i]));
      total_fixed += sizes[i].ToInt();
      ++count_fixed;
    }

    // Count the total percentage of all of the percentage columns/rows ->
    // total_percent. Count the number of columns/rows which are percentages ->
    // count_percent.
    if (grid[i].IsPercentage()) {
      sizes[i] = IntLayoutUnit(grid[i].Value() * available_length / 100.0)
                     .ClampNegativeToZero();
      DCHECK(IsIntegerValue(sizes[i])) << sizes[i];
      total_percent += sizes[i].ToInt();
      ++count_percent;
    }

    // Count the total relative of all the relative columns/rows ->
    // total_relative. Count the number of columns/rows which are relative ->
    // count_relative.
    if (grid[i].IsRelative()) {
      total_relative += ClampTo<int>(std::max(grid[i].Value(), 1.0));
      ++count_relative;
    }
  }

  return sizes;
}

}  // namespace blink
