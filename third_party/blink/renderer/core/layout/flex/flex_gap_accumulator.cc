// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/flex/flex_gap_accumulator.h"

#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/flex/flex_line.h"
#include "third_party/blink/renderer/core/layout/gap/cross_gap.h"
#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"
#include "third_party/blink/renderer/core/layout/gap/main_gap.h"

namespace blink {

FlexGapAccumulator::FlexGapAccumulator(
    LayoutUnit gap_between_items,
    LayoutUnit effective_gap_between_lines,
    wtf_size_t num_lines,
    wtf_size_t num_flex_items,
    bool is_column,
    LayoutUnit border_scrollbar_padding_block_start,
    LayoutUnit border_scrollbar_padding_inline_start)
    : gap_between_items_(gap_between_items),
      effective_gap_between_lines_(effective_gap_between_lines),
      is_column_(is_column),
      gap_geometry_(
          MakeGarbageCollected<GapGeometry>(GapGeometry::ContainerType::kFlex)),
      border_scrollbar_padding_block_start_(
          border_scrollbar_padding_block_start),
      border_scrollbar_padding_inline_start_(
          border_scrollbar_padding_inline_start) {
  gap_geometry_->ReserveCrossGaps(num_flex_items);
  if (num_lines > 0) {
    gap_geometry_->ReserveMainGaps(num_lines - 1);
  }
}

const GapGeometry* FlexGapAccumulator::BuildGapGeometry(
    const BoxFragmentBuilder& container_builder) {
  if (gap_geometry_->MainGapCount() == 0 &&
      gap_geometry_->CrossGapCount() == 0) {
    // `GapGeometry` requires at least one axis to be valid.
    return nullptr;
  }

  if (is_column_) {
    FinalizeContentMainEndForColumnFlex(container_builder);
  }

  if (is_column_) {
    // In a column flex container, the main axis gaps become the "columns" and
    // the cross axis gaps become the "rows".
    gap_geometry_->SetInlineGapSize(effective_gap_between_lines_);
    gap_geometry_->SetBlockGapSize(gap_between_items_);
    gap_geometry_->SetMainDirection(kForColumns);
  } else {
    gap_geometry_->SetBlockGapSize(effective_gap_between_lines_);
    gap_geometry_->SetInlineGapSize(gap_between_items_);
  }

  LayoutUnit content_inline_start =
      is_column_ ? content_cross_start_ : content_main_start_;
  LayoutUnit content_inline_end =
      is_column_ ? content_cross_end_ : content_main_end_;
  LayoutUnit content_block_start =
      is_column_ ? content_main_start_ : content_cross_start_;
  LayoutUnit content_block_end =
      is_column_ ? content_main_end_ : content_cross_end_;

  gap_geometry_->SetContentInlineOffsets(content_inline_start,
                                         content_inline_end);
  gap_geometry_->SetContentBlockOffsets(content_block_start, content_block_end);

  gap_geometry_->Finalize();

  return gap_geometry_;
}

void FlexGapAccumulator::BuildGapsForCurrentItem(const FlexLine& flex_line,
                                                 wtf_size_t flex_line_index,
                                                 LogicalOffset item_offset,
                                                 bool is_first_item,
                                                 bool is_last_item,
                                                 bool is_last_line,
                                                 LayoutUnit line_cross_start,
                                                 LayoutUnit line_cross_end,
                                                 LayoutUnit container_main_end,
                                                 bool in_fragmentation) {
  if (first_flex_line_processed_index_ == kNotFound) {
    first_flex_line_processed_index_ = flex_line_index;
  }

  wtf_size_t fragment_relative_line_index =
      flex_line_index - first_flex_line_processed_index_;

  const bool need_to_add_main_gap =
      (gap_geometry_->MainGapCount() == 0 ||
       gap_geometry_->MainGapCount() - 1 < fragment_relative_line_index) &&
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

  // TODO(490343456): There is a bug in the flex layout
  // algorithm that can cause the size of a line to be 0. In such cases we make
  // sure to not create a main gap, while the underlying bug and behavior is
  // being investigated. However, there are also legitimate cases where we can
  // have a line of size 0.
  //
  // We need to make sure we populate the `cross_gap_sizes_` for all lines.
  // In fragmentation scenarios, we may have some lines where we are not
  // processing the first item or the last item in the flex line (e.g. the nth
  // item in a line gets fragmented and finished in a separate fragment, but its
  // the only item in that line that got fragmented). In such cases we simply
  // need to populate the `cross_gap_sizes_` whenever we don't yet have an entry
  // for that line. This suffices for all other cases as well, since in
  // fragmentation scenarios, the flex line already has
  // `effective_gap_between_items` computed.
  if (in_fragmentation) {
    if (gap_geometry_->GetFlexCrossGapSizeCount() ==
        fragment_relative_line_index) {
      gap_geometry_->AddFlexCrossGapSize(flex_line.effective_gap_between_items);
    }
  } else {
    // For non-fragmentation scenarios, we need to wait to populate the
    // `cross_gap_sizes_` until we see the second item in a line, by which the
    // flex line would have the `effective_gap_between_items` computed. We need
    // the `is_last_item` check to handle the case where we have a single item
    // in a line.
    if ((!is_first_item || is_last_item) &&
        gap_geometry_->GetFlexCrossGapSizeCount() ==
            fragment_relative_line_index) {
      gap_geometry_->AddFlexCrossGapSize(flex_line.effective_gap_between_items);
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
      main_offset - (flex_line.effective_gap_between_items / 2);

  PopulateCrossGapForCurrentItem(flex_line, fragment_relative_line_index,
                                 is_first_line, is_last_line, single_line,
                                 main_intersection_offset, line_cross_start);

  if (is_last_item) {
    const LayoutUnit last_gap_offset =
        is_column_
            ? gap_geometry_->GetCrossGaps().back().GetGapOffset().block_offset
            : gap_geometry_->GetCrossGaps().back().GetGapOffset().inline_offset;
    content_main_end_ = std::max(last_gap_offset, container_main_end);
  }
}

void FlexGapAccumulator::PopulateMainGapForFirstItem(LayoutUnit cross_end) {
  LayoutUnit gap_offset = cross_end + (effective_gap_between_lines_ / 2);
  gap_geometry_->AddMainGap(gap_offset);
}

void FlexGapAccumulator::HandleCrossGapRangesForCurrentItem(
    wtf_size_t flex_line_index,
    wtf_size_t cross_gap_index) {
  if (gap_geometry_->MainGapCount() == 0) {
    return;
  }

  if (flex_line_index < gap_geometry_->MainGapCount()) {
    gap_geometry_->MainGapAt(flex_line_index)
        .IncrementRangeOfCrossGapsBefore(cross_gap_index);
  }

  if (flex_line_index > 0 &&
      flex_line_index - 1 < gap_geometry_->MainGapCount()) {
    // We increment the `RangeOfCrossGapsAfter` for the previous line, since
    // the CrossGaps that start at this line fall "after" the previous line.
    gap_geometry_->MainGapAt(flex_line_index - 1)
        .IncrementRangeOfCrossGapsAfter(cross_gap_index);
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
    // minus half of the effective gap size.
    cross_intersection_offset -= effective_gap_between_lines_ / 2;
    edge_state = CrossGap::EdgeIntersectionState::kEnd;
  } else {
    // Middle line, so the cross gap will start at midpoint between the start
    // of this line and the end of the previous line.
    cross_intersection_offset =
        cross_start - (effective_gap_between_lines_ / 2);
  }

  LogicalOffset logical_offset(
      is_column_ ? cross_intersection_offset : main_intersection_offset,
      is_column_ ? main_intersection_offset : cross_intersection_offset);
  gap_geometry_->AddCrossGap(logical_offset, edge_state);

  HandleCrossGapRangesForCurrentItem(flex_line_index,
                                     gap_geometry_->CrossGapCount() - 1);
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
  if (gap_geometry_->MainGapCount() == 0) {
    return;
  }

  const MainGap& last_main_gap = gap_geometry_->GetMainGaps().back();
  wtf_size_t affected_cross_gaps_start_index =
      last_main_gap.HasCrossGapsBefore()
          ? last_main_gap.GetCrossGapBeforeStart()
          : kNotFound;
  wtf_size_t affected_cross_gaps_end_index =
      last_main_gap.HasCrossGapsBefore() ? last_main_gap.GetCrossGapBeforeEnd()
                                         : kNotFound;
  // Since we are removing the last `MainGap`, we must update the
  // `content_cross_end_` to be just before the last `MainGap`.
  content_cross_end_ = new_cross_end.value_or(last_main_gap.GetGapOffset() -
                                              effective_gap_between_lines_ / 2);

  gap_geometry_->RemoveLastMainGap();

  // Since we have removed the last `MainGap`, we must also update the edge
  // intersection state of all the `CrossGap`s associated with that main gap,
  // since now we know that they will be adjacent to the end of the container.
  for (wtf_size_t i = affected_cross_gaps_start_index;
       i != kNotFound && i <= affected_cross_gaps_end_index; ++i) {
    CrossGap& cross_gap = gap_geometry_->CrossGapAt(i);
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
