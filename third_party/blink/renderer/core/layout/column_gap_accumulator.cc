// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/column_gap_accumulator.h"

#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"

namespace blink {

ColumnGapAccumulator::ColumnGapAccumulator(LayoutUnit column_gap_size,
                                           LayoutUnit row_gap_size,
                                           wtf_size_t specified_column_count,
                                           bool has_auto_column_count)
    : column_gap_size_(column_gap_size),
      row_gap_size_(row_gap_size),
      specified_column_count_(specified_column_count),
      has_auto_column_count_(has_auto_column_count),
      gap_geometry_(MakeGarbageCollected<GapGeometry>(
          GapGeometry::ContainerType::kMultiColumn)) {}

void ColumnGapAccumulator::AddMainGap(LayoutUnit block_offset,
                                      SpannerMainGapType gap_type) {
  // If the main gap is not a spanner main gap, it should be offset by half the
  // row gap size so that it's placed in the middle of the gap.
  if (gap_type == SpannerMainGapType::kNone) {
    block_offset += row_gap_size_ / 2;
  }

  gap_geometry_->AddMainGap(block_offset, gap_type);
}

void ColumnGapAccumulator::AddEndSpannerMainGapIfNeeded(
    LayoutUnit block_offset) {
  if (!LastMainGapIsStartSpanner()) {
    return;
  }

  AddMainGap(block_offset, SpannerMainGapType::kEnd);
}

void ColumnGapAccumulator::AddStartSpannerMainGapIfNeeded(
    LayoutUnit block_offset) {
  if (gap_geometry_->MainGapCount() > 0 &&
      gap_geometry_->GetMainGaps().back().IsStartSpannerMainGap()) {
    return;
  }

  AddMainGap(block_offset, SpannerMainGapType::kStart);

  // Spanner rows use kNotFound since they span the full specified column count.
  AddNumberOfColumnsForCurrentRow(kNotFound);
}

void ColumnGapAccumulator::AddCrossGap(LayoutUnit column_inline_start_offset) {
  LayoutUnit gap_center = column_inline_start_offset - (column_gap_size_ / 2);

  CHECK(first_column_offset_.has_value());

  gap_geometry_->AddCrossGap(
      LogicalOffset(gap_center, first_column_offset_.value().block_offset));
}

void ColumnGapAccumulator::AddNumberOfColumnsForCurrentRow(
    wtf_size_t cols_in_row) {
  if (!columns_per_row_.has_value()) {
    columns_per_row_ = Vector<wtf_size_t>();
  }
  columns_per_row_->push_back(cols_in_row);

  FinalizeMainGapSegmentStateForCurrentRow(cols_in_row);
}

void ColumnGapAccumulator::SetFirstColumnOffsetIfNeeded(LogicalOffset offset) {
  if (!first_column_offset_.has_value()) {
    first_column_offset_.emplace(offset);
  }
}

void ColumnGapAccumulator::UpdateMaxColumnsInRow(wtf_size_t count) {
  max_columns_in_row_ = std::max(max_columns_in_row_, count);
}

bool ColumnGapAccumulator::ShouldAddCrossGapAt(
    wtf_size_t column_index_in_row) const {
  return column_index_in_row >= max_columns_in_row_;
}

bool ColumnGapAccumulator::LastMainGapIsStartSpanner() const {
  return gap_geometry_->MainGapCount() > 0 &&
         gap_geometry_->GetMainGaps().back().IsStartSpannerMainGap();
}

const GapGeometry* ColumnGapAccumulator::BuildGapGeometry(
    const BoxFragmentBuilder& container_builder,
    LayoutUnit column_inline_size) {
  if ((gap_geometry_->CrossGapCount() == 0 &&
       gap_geometry_->MainGapCount() == 0) ||
      !first_column_offset_.has_value()) {
    return nullptr;
  }

  // In the case where we didn't create as many columns as specified in
  // `column-count`, we need to add a cross gap for each remaining column
  // that would have been created, until we reach the specified column count.
  for (wtf_size_t i = max_columns_in_row_; i < specified_column_count_; ++i) {
    LayoutUnit inline_offset;
    if (gap_geometry_->CrossGapCount() > 0) {
      inline_offset =
          gap_geometry_->GetCrossGaps().back().GetGapOffset().inline_offset +
          column_gap_size_ / 2 + column_inline_size + column_gap_size_;
    }
    AddCrossGap(inline_offset);
  }

  LayoutUnit content_inline_end =
      container_builder.FragmentInlineSize() -
      container_builder.BorderScrollbarPadding().inline_end;
  if (gap_geometry_->CrossGapCount() > 0) {
    content_inline_end = std::max(
        content_inline_end,
        gap_geometry_->GetCrossGaps().back().GetGapOffset().inline_offset);

    if (columns_per_row_.has_value()) {
      UpdateCrossGapSegmentStates();
    }

    gap_geometry_->SetInlineGapSize(column_gap_size_);
  }

  LayoutUnit content_block_end =
      container_builder.FragmentBlockSize() -
      container_builder.ApplicableBorders().block_end -
      container_builder.ApplicableScrollbar().block_end -
      container_builder.ApplicablePadding().block_end;
  if (gap_geometry_->MainGapCount() > 0) {
    // TODO(crbug.com/357648037): There is content beyond the last main gap,
    // so using this as the offset isn't right. The bug here is that if the
    // multicol container is overflowed, the column gaps in the last row will
    // be missing.
    content_block_end = std::max(
        content_block_end, gap_geometry_->GetMainGaps().back().GetGapOffset());
    gap_geometry_->SetBlockGapSize(row_gap_size_);
  }

  gap_geometry_->SetContentInlineOffsets(first_column_offset_->inline_offset,
                                         content_inline_end);
  gap_geometry_->SetContentBlockOffsets(first_column_offset_->block_offset,
                                        content_block_end);

  gap_geometry_->SetMainDirection(kForRows);

  gap_geometry_->Finalize();

  return gap_geometry_;
}

void ColumnGapAccumulator::FinalizeMainGapSegmentStateForCurrentRow(
    wtf_size_t cols_in_row) {
  // Now that we know the column count of the row, we can finalize the gap
  // segment state of the main gap directly above it (if any).
  if (cols_in_row == kNotFound || gap_geometry_->MainGapCount() == 0 ||
      gap_geometry_->GetMainGaps().back().IsSpannerMainGap()) {
    return;
  }

  // Walk back to the last column row (skipping any interleaved spanner rows).
  // The current entry is at the back; start one before it.
  CHECK_GE(columns_per_row_->size(), 2u);
  wtf_size_t prev_index = columns_per_row_->size() - 2;
  while ((*columns_per_row_)[prev_index] == kNotFound) {
    if (prev_index == 0) {
      // No preceding column row exists; the gap is between this row and a
      // spanner above. Nothing to do.
      return;
    }
    --prev_index;
  }

  const wtf_size_t cols_above = (*columns_per_row_)[prev_index];
  if (cols_above == cols_in_row) {
    return;
  }

  const wtf_size_t shorter_row_cols = std::min(cols_above, cols_in_row);
  const wtf_size_t longer_row_cols = std::max(cols_above, cols_in_row);
  const GapSegmentState state =
      (cols_above > cols_in_row)
          ? GapSegmentState(GapSegmentState::kEmptyAfter)
          : GapSegmentState(GapSegmentState::kEmptyBefore);
  gap_geometry_->MainGapAt(gap_geometry_->MainGapCount() - 1)
      .AddGapSegmentStateRange(
          GapSegmentStateRange(shorter_row_cols, longer_row_cols, state));
}

void ColumnGapAccumulator::UpdateCrossGapSegmentStates() {
  // Computes per-row segment states for each cross gap based on how many
  // columns are present in each row of the multicol container.
  for (wtf_size_t cross_gap_index = 0;
       cross_gap_index < gap_geometry_->CrossGapCount(); ++cross_gap_index) {
    CrossGap& cross_gap = gap_geometry_->CrossGapAt(cross_gap_index);
    for (wtf_size_t cols_in_row_index = 0;
         cols_in_row_index < columns_per_row_->size(); ++cols_in_row_index) {
      wtf_size_t cols_in_row = (*columns_per_row_)[cols_in_row_index];
      wtf_size_t segment_start = cols_in_row_index;
      wtf_size_t segment_end = cols_in_row_index + 1;

      // There are columns around this cross gap, so we don't mark it
      // empty on either side.
      if (cols_in_row != kNotFound && cross_gap_index + 1 < cols_in_row) {
        continue;
      }

      // If the cross gap index is greater than or equal to the number of
      // columns in the row, then the cross gap is outside of the specified
      // column count, and should be treated as blocked in the case where it's
      // not in between column content.
      bool is_cross_gap_outside_specified_column_count =
          cross_gap_index + 1 >= specified_column_count_ &&
          !has_auto_column_count_;

      GapSegmentState state;
      if (cols_in_row == kNotFound &&
          !is_cross_gap_outside_specified_column_count) {
        state = GapSegmentState(GapSegmentState::kBlocked);
      } else if (cross_gap_index + 1 == cols_in_row) {
        // The cross gap is after the last column in this row, so it's
        // empty on the right side.
        state = GapSegmentState(GapSegmentState::kEmptyAfter);
      } else {
        // Empty on both sides.
        state = GapSegmentState();
      }

      cross_gap.AddGapSegmentStateRange(
          GapSegmentStateRange(segment_start, segment_end, state));
    }
  }
}

}  // namespace blink
