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
    LayoutUnit border_scrollbar_padding_inline_start,
    std::optional<GapGeometry::PlacementReversal> placement_reversal)
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
  gap_geometry_->ResizeFlexCrossGapSizes(num_lines);
  gap_geometry_->SetMainDirection(is_column_ ? kForColumns : kForRows);
  if (is_column_) {
    // Entries use global-line indexing. Pre-size the vector so lines skipped
    // by this fragment still have entries, even when the fragment has no row
    // gaps at all.
    row_gap_break_token_data_.resize(num_lines);
  }

  // `ApplyReversals` puts `flex_lines` in geometric order before this
  // accumulator is constructed. Record the reversal so paint can assign gap
  // decoration values in placement order.
  if (placement_reversal) {
    gap_geometry_->SetGapPlacementReversal(*placement_reversal);
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

  return gap_geometry_;
}

Vector<FlexRowGapBreakTokenData>
FlexGapAccumulator::FinalizeRowGapBreakTokenData() {
  // Column flex needs every global-line entry, including skipped lines and
  // fragments whose row-gap counts are all zero, so later fragments can resume
  // each line's stitched pattern.
  if (is_column_) {
    return std::move(row_gap_break_token_data_);
  }

  // Row flex stores at most one entry. Discarding a faux main gap can leave
  // that entry with a zero count; omit it because there is no row gap to
  // stitch.
  if (!row_gap_break_token_data_.empty() &&
      row_gap_break_token_data_[0].row_gap_count == 0u) {
    return {};
  }

  return std::move(row_gap_break_token_data_);
}

void FlexGapAccumulator::InitializeFragmentedColumnGapGeometry(
    const FlexLineVector& flex_lines) {
  CHECK(is_column_);
  CHECK(!flex_lines.empty());
  CHECK_EQ(gap_geometry_->MainGapCount(), 0u);

  content_cross_start_ = flex_lines.front().cross_axis_offset;
  const FlexLine& last_line = flex_lines.back();
  content_cross_end_ = last_line.cross_axis_offset + last_line.line_cross_size;
  content_main_start_ = border_scrollbar_padding_block_start_;

  for (wtf_size_t i = 0; i + 1 < flex_lines.size(); ++i) {
    const LayoutUnit preceding_end =
        flex_lines[i].cross_axis_offset + flex_lines[i].line_cross_size;
    PopulateMainGapForFirstItem(preceding_end);
  }
}

void FlexGapAccumulator::BuildGapsForCurrentItem(
    const FlexLineVector& flex_lines,
    wtf_size_t global_line_index,
    wtf_size_t item_index_in_line,
    LogicalOffset item_offset,
    bool is_first_item,
    bool is_last_item,
    bool is_last_line,
    LayoutUnit line_cross_start,
    LayoutUnit line_cross_end,
    LayoutUnit container_main_end,
    bool in_fragmentation) {
  const FlexLine& flex_line = flex_lines[global_line_index];
  const bool is_fragmented_column = is_column_ && in_fragmentation;

  // Geometry uses global flex-line slots for column flex and
  // fragment-relative slots for row flex.
  wtf_size_t fragment_relative_line_index = global_line_index;
  if (!is_column_) {
    if (first_row_flex_line_index_ == kNotFound) {
      first_row_flex_line_index_ = global_line_index;
    }
    fragment_relative_line_index -= first_row_flex_line_index_;
  }

  // In a fragmented column flex we populate the `MainGaps` ahead of time since
  // they exist in every fragment, so we skip adding them here.
  const bool need_to_add_main_gap =
      !is_fragmented_column &&
      (gap_geometry_->MainGapCount() == 0 ||
       gap_geometry_->MainGapCount() - 1 < fragment_relative_line_index) &&
      !is_last_line;
  const bool is_first_line = fragment_relative_line_index == 0;
  const bool single_line = is_first_line && is_last_line;

  if (single_line && is_first_item) {
    CHECK(!need_to_add_main_gap);
    SetContentStartOffsetsIfNeeded(item_offset, line_cross_start);
  }

  if (!is_fragmented_column && is_last_line && is_first_item) {
    content_cross_end_ = line_cross_end;
  }

  if (need_to_add_main_gap) {
    // We set the `MainGap` start offset when we process the first item in a
    // line, and nothing else. The last line does not have any `MainGap`s.
    SetContentStartOffsetsIfNeeded(item_offset, line_cross_start);
    PopulateMainGapForFirstItem(line_cross_end);
    // For row flex containers, row gaps are the gaps between its flex lines. We
    // store a single entry for the whole fragment and build up its
    // `row_gap_count` as each main gap is placed.
    if (!is_column_) {
      // For row flex, count this main gap in the fragment's single entry. The
      // first main gap creates the entry; later ones add to it.
      if (row_gap_break_token_data_.empty()) {
        row_gap_break_token_data_.push_back(
            FlexRowGapBreakTokenData{global_line_index,
                                     /*row_gap_count=*/1u});
      } else {
        IncrementRowGapCount(/*row_gap_data_index=*/0);
      }
    }

    if (is_last_item) {
      content_main_end_ = container_main_end;
    }
  }

  // When we're not fragmenting, wait for the second item (or the last item of a
  // single-item line) so `effective_gap_between_items` is final before writing.
  // When fragmenting, the effective gap was computed during the initial layout
  // pass, so it can be written for every item.
  const bool can_write_cross_gap_size =
      in_fragmentation || !is_first_item || is_last_item;
  if (can_write_cross_gap_size) {
    gap_geometry_->SetFlexCrossGapSize(fragment_relative_line_index,
                                       flex_line.effective_gap_between_items);
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

  PopulateCrossGapForCurrentItem(
      flex_line, global_line_index, fragment_relative_line_index, is_first_line,
      is_last_line, single_line, main_intersection_offset, line_cross_start);
  // Store this `CrossGap`'s index in the full gap-decoration value list.
  const GridTrackSizingDirection cross_direction =
      is_column_ ? kForRows : kForColumns;
  if (in_fragmentation &&
      gap_geometry_->NeedsDecorationValueAssignmentMapping(cross_direction)) {
    RecordFragmentedFlexCrossGapDecorationIndex(flex_lines, global_line_index,
                                                item_index_in_line);
  }

  if (is_last_item) {
    const LayoutUnit last_gap_offset =
        is_column_
            ? gap_geometry_->GetCrossGaps().back().GetGapOffset().block_offset
            : gap_geometry_->GetCrossGaps().back().GetGapOffset().inline_offset;
    content_main_end_ = std::max(last_gap_offset, container_main_end);
  }
}

void FlexGapAccumulator::RecordFragmentedFlexCrossGapDecorationIndex(
    const FlexLineVector& flex_lines,
    wtf_size_t global_line_index,
    wtf_size_t item_index_in_line) {
  CHECK_GT(item_index_in_line, 0u);
  if (fragmented_flex_line_gap_ranges_.empty()) {
    // Record each flex line's first `CrossGap` index and number of `CrossGap`s
    // in the full flexbox.
    fragmented_flex_line_gap_ranges_.ReserveInitialCapacity(flex_lines.size());
    wtf_size_t line_start = 0;
    for (const FlexLine& line : flex_lines) {
      const wtf_size_t line_gap_count =
          line.item_indices.empty() ? 0u : line.item_indices.size() - 1;
      fragmented_flex_line_gap_ranges_.emplace_back(
          GapGeometry::GapIndexRange{line_start, line_gap_count});
      line_start += line_gap_count;
    }
  }
  const GapGeometry::GapIndexRange line_range =
      fragmented_flex_line_gap_ranges_[global_line_index];
  const GapGeometry::GapIndexRange last_line_range =
      fragmented_flex_line_gap_ranges_.back();
  const wtf_size_t gap_slot_count =
      last_line_range.start + last_line_range.count;

  // Convert the line-local gap index to its index in the full flexbox.
  const wtf_size_t stitched_gap_index =
      line_range.start + item_index_in_line - 1;

  // Find which gap-decoration value belongs to this gap.
  const wtf_size_t decoration_value_index =
      gap_geometry_->DecorationValueIndexForCrossGap(
          stitched_gap_index, line_range, gap_slot_count);
  gap_geometry_->AddFragmentedFlexCrossGapDecorationIndex(
      decoration_value_index, gap_slot_count);
}

void FlexGapAccumulator::PopulateMainGapForFirstItem(LayoutUnit cross_end) {
  LayoutUnit gap_offset = cross_end + (effective_gap_between_lines_ / 2);
  gap_geometry_->AddMainGap(gap_offset);
}

void FlexGapAccumulator::HandleCrossGapRangesForCurrentItem(
    wtf_size_t fragment_relative_line_index,
    wtf_size_t cross_gap_index) {
  if (gap_geometry_->MainGapCount() == 0) {
    return;
  }

  if (fragment_relative_line_index < gap_geometry_->MainGapCount()) {
    gap_geometry_->MainGapAt(fragment_relative_line_index)
        .IncrementRangeOfCrossGapsBefore(cross_gap_index);
  }

  if (fragment_relative_line_index > 0 &&
      fragment_relative_line_index - 1 < gap_geometry_->MainGapCount()) {
    // We increment the `RangeOfCrossGapsAfter` for the previous line, since
    // the CrossGaps that start at this line fall "after" the previous line.
    gap_geometry_->MainGapAt(fragment_relative_line_index - 1)
        .IncrementRangeOfCrossGapsAfter(cross_gap_index);
  }
}

void FlexGapAccumulator::PopulateCrossGapForCurrentItem(
    const FlexLine& flex_line,
    wtf_size_t global_line_index,
    wtf_size_t fragment_relative_line_index,
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

  // For column flex containers, a line's cross gaps are its row gaps. We store
  // one entry per flex line and build up each line's `row_gap_count` as its
  // cross gaps are placed.
  if (is_column_) {
    IncrementRowGapCount(global_line_index);
  }

  HandleCrossGapRangesForCurrentItem(fragment_relative_line_index,
                                     gap_geometry_->CrossGapCount() - 1);
}

void FlexGapAccumulator::IncrementRowGapCount(wtf_size_t row_gap_data_index) {
  DCHECK_LT(row_gap_data_index, row_gap_break_token_data_.size());
  ++row_gap_break_token_data_[row_gap_data_index].row_gap_count;
}

void FlexGapAccumulator::DecrementRowGapCount() {
  CHECK(!is_column_);
  if (gap_geometry_->MainGapCount() == 0 || row_gap_break_token_data_.empty()) {
    return;
  }

  DCHECK_EQ(row_gap_break_token_data_.size(), 1u);
  DCHECK_GT(row_gap_break_token_data_[0].row_gap_count, 0u);
  --row_gap_break_token_data_[0].row_gap_count;
}

void FlexGapAccumulator::CalculateColumnFlexLineRowGapStart(
    const FlexLineVector& flex_lines,
    wtf_size_t global_line_index,
    const FlexGapBreakTokenData* previous_gap_data) {
  CHECK(is_column_);
  DCHECK_LT(global_line_index, row_gap_break_token_data_.size());

  // The very first line of the first fragment starts at 0, so no adjustment to
  // the gap start offset is needed.
  if (global_line_index == 0 && !previous_gap_data) {
    CHECK_EQ(row_gap_break_token_data_[global_line_index].first_row_gap_index,
             0u);
    return;
  }

  wtf_size_t previous_start;
  wtf_size_t previous_gap_count = 0;

  // For the first fragment, we get the `previous_start` and
  // `previous_gap_count` using the number of items in the previous flex line.
  if (!previous_gap_data) {
    const wtf_size_t num_items_in_previous_line =
        flex_lines[global_line_index - 1].item_indices.size();
    previous_start =
        row_gap_break_token_data_[global_line_index - 1].first_row_gap_index;
    if (num_items_in_previous_line > 1) {
      previous_gap_count = num_items_in_previous_line - 1;
    }
  } else {
    // For subsequent fragments, we get the `previous_start` and
    // `previous_gap_count` from the previous fragment's row gaps info.
    const Vector<FlexRowGapBreakTokenData>& previous_row_gap_data =
        previous_gap_data->gap_data_for_rows;
    DCHECK_LT(global_line_index, previous_row_gap_data.size());
    previous_start =
        previous_row_gap_data[global_line_index].first_row_gap_index;
    previous_gap_count = previous_row_gap_data[global_line_index].row_gap_count;
  }

  row_gap_break_token_data_[global_line_index].first_row_gap_index =
      previous_start + previous_gap_count;
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
