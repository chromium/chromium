// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/flex/flex_gap_accumulator.h"

#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/flex/flex_line.h"
#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"

namespace blink {

const GapGeometry* FlexGapAccumulator::BuildGapGeometry(
    const BoxFragmentBuilder& container_builder) {
  const bool has_valid_main_axis_gaps =
      !main_gaps_.empty() && gap_between_lines_ > LayoutUnit();
  const bool has_valid_cross_axis_gaps =
      !cross_gaps_.empty() && gap_between_items_ > LayoutUnit();
  if (!has_valid_main_axis_gaps && !has_valid_cross_axis_gaps) {
    // `GapGeometry` requires at least one axis to be valid.
    return nullptr;
  }

  if (is_column_) {
    FinalizeContentMainEndForColumnFlex(container_builder);
  }

  GapGeometry* gap_geometry =
      MakeGarbageCollected<GapGeometry>(GapGeometry::ContainerType::kFlex);

  if (is_column_) {
    // In a column flex container, the main axis gaps become the "columns" and
    // the cross axis gaps become the "rows".
    if (gap_between_lines_ > LayoutUnit()) {
      gap_geometry->SetInlineGapSize(gap_between_lines_);
    }
    if (gap_between_items_ > LayoutUnit()) {
      gap_geometry->SetBlockGapSize(gap_between_items_);
    }

    gap_geometry->SetMainDirection(kForColumns);
  } else {
    if (gap_between_lines_ > LayoutUnit()) {
      gap_geometry->SetBlockGapSize(gap_between_lines_);
    }
    if (gap_between_items_ > LayoutUnit()) {
      gap_geometry->SetInlineGapSize(gap_between_items_);
    }
  }

  // TODO(crbug.com/440123087): Risky since they could in theory be used after
  // moved. Clean up to not move members. Change members to unique_ptrs
  if (!cross_gaps_.empty()) {
    gap_geometry->SetCrossGaps(std::move(cross_gaps_));
  }

  if (!main_gaps_.empty()) {
    gap_geometry->SetMainGaps(std::move(main_gaps_));
  }

  LayoutUnit content_inline_start =
      is_column_ ? content_cross_start_ : content_main_start_;
  LayoutUnit content_inline_end =
      is_column_ ? content_cross_end_ : content_main_end_;
  LayoutUnit content_block_start =
      is_column_ ? content_main_start_ : content_cross_start_;
  LayoutUnit content_block_end =
      is_column_ ? content_main_end_ : content_cross_end_;

  gap_geometry->SetContentInlineOffsets(content_inline_start,
                                        content_inline_end);
  gap_geometry->SetContentBlockOffsets(content_block_start, content_block_end);

  return gap_geometry;
}

void FlexGapAccumulator::BuildGapsForCurrentItem(
    const FlexLine& flex_line,
    wtf_size_t flex_line_index,
    LogicalOffset item_offset,
    bool is_first_item,
    bool is_last_item,
    bool is_last_line,
    LayoutUnit line_cross_start,
    LayoutUnit line_cross_end,
    LayoutUnit container_main_end) {
  if (first_flex_line_processed_index_ == kNotFound) {
    first_flex_line_processed_index_ = flex_line_index;
  }

  wtf_size_t fragment_relative_line_index =
      flex_line_index - first_flex_line_processed_index_;

  const bool need_to_add_main_gap =
      (main_gaps_.empty() ||
       main_gaps_.size() - 1 < fragment_relative_line_index) &&
      !is_last_line;
  const bool is_first_line = fragment_relative_line_index == 0;
  const bool single_line = is_first_line && is_last_line;

  if (single_line && is_first_item) {
    CHECK(!need_to_add_main_gap);
    SetContentStartOffsetsIfNeeded(item_offset, line_cross_start);
  }

  if (is_last_line && is_first_item) {
    content_cross_end_ = line_cross_end;
  }

  if (need_to_add_main_gap) {
    // We set the `MainGap` start offset when we process the first item in a
    // line, and nothing else. The last line does not have any `MainGap`s.
    SetContentStartOffsetsIfNeeded(item_offset, line_cross_start);
    PopulateMainGapForFirstItem(line_cross_end);

    if (is_last_item) {
      content_main_end_ = container_main_end;
    }
  }

  // The first item in any line doesn't have any `CrossGap` associated with
  // it, so we return early.
  if (is_first_item) {
    return;
  }

  const LayoutUnit main_offset =
      is_column_ ? item_offset.block_offset : item_offset.inline_offset;
  const LayoutUnit main_intersection_offset =
      main_offset - (gap_between_items_ / 2);

  PopulateCrossGapForCurrentItem(flex_line, fragment_relative_line_index,
                                 is_first_line, is_last_line, single_line,
                                 main_intersection_offset, line_cross_start);

  if (is_last_item) {
    const LayoutUnit last_gap_offset =
        is_column_ ? cross_gaps_.back().GetGapOffset().block_offset
                   : cross_gaps_.back().GetGapOffset().inline_offset;
    content_main_end_ = std::max(last_gap_offset, container_main_end);
  }
}

void FlexGapAccumulator::PopulateMainGapForFirstItem(LayoutUnit cross_end) {
  LayoutUnit gap_offset = cross_end + (gap_between_lines_ / 2);
  main_gaps_.emplace_back(gap_offset);
}

void FlexGapAccumulator::HandleCrossGapRangesForCurrentItem(
    wtf_size_t flex_line_index,
    wtf_size_t cross_gap_index) {
  if (main_gaps_.empty()) {
    return;
  }

  if (flex_line_index < main_gaps_.size()) {
    main_gaps_[flex_line_index].IncrementRangeOfCrossGapsBefore(
        cross_gap_index);
  }

  if (flex_line_index > 0) {
    CHECK_LE(flex_line_index - 1, main_gaps_.size());
    // We increment the `RangeOfCrossGapsAfter` for the previous line, since
    // the CrossGaps that start at this line fall "after" the previous line.
    main_gaps_[flex_line_index - 1].IncrementRangeOfCrossGapsAfter(
        cross_gap_index);
  }
}

void FlexGapAccumulator::PopulateCrossGapForCurrentItem(
    const FlexLine& flex_line,
    wtf_size_t flex_line_index,
    bool is_first_line,
    bool is_last_line,
    bool single_line,
    LayoutUnit main_intersection_offset,
    LayoutUnit cross_start) {
  // If we are in the first or last flex line, our the `CrossGap` associated
  // with this item will start at the point given by
  // `main_intersection_offset`, and the either cross axis of the line or the
  // cross axis offset of the line minus half of the gap size.
  //
  // If we are in the middle flex line, the `CrossGap` associated with this
  // item will start at the point given by `main_intersection_offset`, and the
  // midpoint between the start of the line and the end of the last line.

  LayoutUnit cross_intersection_offset = cross_start;
  CrossGap::EdgeIntersectionState edge_state =
      CrossGap::EdgeIntersectionState::kNone;

  if (single_line) {
    // If there is only one line, the cross gap will start and end at the
    // content edge.
    edge_state = CrossGap::EdgeIntersectionState::kBoth;
  } else if (is_first_line) {
    // First line, so the cross gap starts at the content edge.
    edge_state = CrossGap::EdgeIntersectionState::kStart;
  } else if (is_last_line) {
    // If there is more than one flex line, and the current line is the last
    // line, the cross offset will be the cross axis offset of the line
    // minus half of the gap size.
    cross_intersection_offset -= gap_between_lines_ / 2;
    edge_state = CrossGap::EdgeIntersectionState::kEnd;
  } else {
    // Middle line, so the cross gap will start at midpoint between the start
    // of this line and the end of the previous line.
    cross_intersection_offset =
        flex_line.cross_axis_offset - (gap_between_lines_ / 2);
  }

  LogicalOffset logical_offset(
      is_column_ ? cross_intersection_offset : main_intersection_offset,
      is_column_ ? main_intersection_offset : cross_intersection_offset);
  CrossGap cross_gap(logical_offset, edge_state);

  cross_gaps_.push_back(cross_gap);
  HandleCrossGapRangesForCurrentItem(flex_line_index, cross_gaps_.size() - 1);
}

void FlexGapAccumulator::FinalizeContentMainEndForColumnFlex(
    const BoxFragmentBuilder& container_builder) {
  CHECK(is_column_);
  LayoutUnit applicable_border_scrollbar_padding_block_end =
      container_builder.ApplicableBorders().block_end +
      container_builder.ApplicableScrollbar().block_end +
      container_builder.ApplicablePadding().block_end;

  SetContentMainEnd(container_builder.FragmentBlockSize() -
                    applicable_border_scrollbar_padding_block_end);
}

void FlexGapAccumulator::SuppressLastMainGap(
    std::optional<LayoutUnit> new_cross_end) {
  if (main_gaps_.empty()) {
    return;
  }

  wtf_size_t affected_cross_gaps_start_index =
      main_gaps_.back().HasCrossGapsBefore()
          ? main_gaps_.back().GetCrossGapBeforeStart()
          : kNotFound;
  wtf_size_t affected_cross_gaps_end_index =
      main_gaps_.back().HasCrossGapsBefore()
          ? main_gaps_.back().GetCrossGapBeforeEnd()
          : kNotFound;
  // Since we are removing the last `MainGap`, we must update the
  // `content_cross_end_` to be just before the last `MainGap`.
  content_cross_end_ =
      new_cross_end.has_value()
          ? new_cross_end.value()
          : main_gaps_.back().GetGapOffset() - (gap_between_lines_ / 2);

  main_gaps_.pop_back();

  // Since we have removed the last `MainGap`, we must also update the edge
  // intersection state of all the `CrossGap`s associated with that main gap,
  // since now we know that they will be adjacent to the end of the container.
  for (wtf_size_t i = affected_cross_gaps_start_index;
       i != kNotFound && i <= affected_cross_gaps_end_index; ++i) {
    CrossGap& cross_gap = cross_gaps_[i];
    CrossGap::EdgeIntersectionState edge_state =
        cross_gap.GetEdgeIntersectionState();
    if (edge_state == CrossGap::EdgeIntersectionState::kStart) {
      cross_gap.SetEdgeIntersectionState(
          CrossGap::EdgeIntersectionState::kBoth);
    } else if (edge_state == CrossGap::EdgeIntersectionState::kNone) {
      cross_gap.SetEdgeIntersectionState(CrossGap::EdgeIntersectionState::kEnd);
    }
  }
}

void FlexGapAccumulator::SetContentStartOffsetsIfNeeded(
    LogicalOffset offset,
    LayoutUnit line_cross_start) {
  if (content_main_start_ != LayoutUnit::Max() &&
      content_cross_start_ != LayoutUnit::Max()) {
    return;
  }

  content_cross_start_ = line_cross_start;
  content_main_start_ = is_column_ ? border_scrollbar_padding_block_start_
                                   : border_scrollbar_padding_inline_start_;
  const LayoutUnit main_offset =
      is_column_ ? offset.block_offset : offset.inline_offset;
  content_main_start_ = std::min(content_main_start_, main_offset);
}
}  // namespace blink
