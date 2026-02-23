// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_COLUMN_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_COLUMN_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"
#include "third_party/blink/renderer/core/layout/gap/gap_utils.h"
#include "third_party/blink/renderer/core/layout/layout_algorithm.h"

namespace blink {

class BlockBreakToken;
class BlockNode;
class ColumnSpannerPath;
class ConstraintSpace;
enum class BreakStatus;
struct LogicalSize;
struct MarginStrut;

// Multicol layout algorithm.
//
// Establishes a fragmentation context and produces columns as child fragments.
// Each column is a fragmentainer that contains a portion of the fragmented
// content.
//
// Additionally column spanners are also added as child fragments, if they
// exist. They are taken out of the fragmentation context and become siblings of
// column fragments.
//
// The multicol spec has the concept of "lines" and "rows". A multicol container
// contains rows, which contain lines and/or spanners. Lines contain columns.
// This algorithm obviously needs to be aware of these concepts, but no
// fragments are created for rows or lines. It's just plain columns that are
// positioned relatively to the resulting multicol fragment.
//
// Gap decorations:
//
// This algorithm also sets up gap decorations, i.e. column rules and row rules.
// Example of a multicol container with a spanner, and a row gap.
// Gap intersections are given by `X`.
// +------------------------X-----------------------------------------+
// | +---------+           Column Gap     +---------+                 |
// | |         |                          |         |                 |
// | +---------+                          +---------+                 |
// |                                                                  |
// |------------------------X-----------------------------------------|
// |                    Spanner                                       |
// |------------------------X-----------------------------------------|
// |                                                                  |
// | +---------+           Column Gap     +---------+                 |
// | |         |                          |         |                 |
// | +---------+                          +---------+                 |
// X=========Row Gap========X=========================================X
// |                                                                  |
// | +---------+           Column Gap                                 |
// | |         |                                                      |
// | +---------+                                                      |
// +------------------------X-----------------------------------------+
//
// To populate the gap intersections, we build them out as we place each column
// in a row of columns.
//
// Each column in a line of columns, except for the first column, can be
// associated with the following gap intersections:
// * The column intersection of the column gap with the first or last edge of
//   the container (in the block direction).
// * The column intersection of the column gap with any spanner before the
//   column.
//
// See third_party/blink/renderer/core/layout/gap/README.md for more info.
class CORE_EXPORT ColumnLayoutAlgorithm
    : public LayoutAlgorithm<BlockNode, BoxFragmentBuilder, BlockBreakToken> {
 public:
  explicit ColumnLayoutAlgorithm(const LayoutAlgorithmParams& params);

  const LayoutResult* Layout();

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&);

  // Create an empty column fragment, modeled after an existing column. The
  // resulting column may then be used and mutated by the out-of-flow layout
  // code, to add out-of-flow descendants.
  static const PhysicalBoxFragment& CreateEmptyColumn(
      const BlockNode&,
      const ConstraintSpace& parent_space,
      const PhysicalBoxFragment& previous_column);

 private:
  MinMaxSizesResult ComputeSpannersMinMaxSizes(
      const BlockNode& search_parent) const;

  // Lay out as many children as we can. If |kNeedsEarlierBreak| is returned, it
  // means that we ran out of space at an unappealing location, and need to
  // relayout and break earlier (because we have a better breakpoint there). If
  // |kBrokeBefore| is returned, it means that we need to break before the
  // multicol container, and retry in the next fragmentainer.
  BreakStatus LayoutChildren();

  // Lay out and fragment content into columns. Keep going until done, out of
  // space in any outer fragmentation context, or until a column spanner is
  // found.
  const LayoutResult* LayoutFragmentationContext(
      const BlockBreakToken* next_column_token,
      MarginStrut*);

  // Lay out one line of columns. The layout result returned is for the last
  // column that was laid out. The lines themselves don't create fragments. If
  // we're in a nested fragmentation context, and a break is inserted before the
  // line, nullptr is returned.
  //
  // `line_offset` is the block-offset from the start of the multicol fragment.
  const LayoutResult* LayoutLine(const BlockBreakToken* next_column_token,
                                 LayoutUnit line_offset,
                                 LayoutUnit miminum_column_block_size,
                                 bool has_wrapped,
                                 MarginStrut*);

  // Lay out a column spanner. The return value will tell whether to break
  // before the spanner or not. If `BreakStatus::kContinue` is returned, and no
  // break token was set, it means that we can proceed to the next line of
  // columns.
  BreakStatus LayoutSpanner(BlockNode spanner_node,
                            const BlockBreakToken* break_token,
                            MarginStrut*);

  // Add another main gap, at the given offset. This is either the block-start
  // of a row gap, or before or after a spanner.
  void AddMainGap(LayoutUnit block_offset,
                  SpannerMainGapType gap_type = SpannerMainGapType::kNone);

  // Add a cross gap at the given inline offset of the current column.
  void AddCrossGap(LayoutUnit column_inline_start_offset);

  // Add an entry for the current row to `columns_per_row_` to store the number
  // of columns this row contains. Spanners are counted as rows and marked with
  // `kNotFound`.
  void AddNumberOfColumnsForCurrentRow(wtf_size_t cols_in_row);

  // Populates `range_of_cross_gaps_before_current_main_gap_` with
  // `CrossGapRanges` for each group of `CrossGap`s before each `MainGap`.
  // For each `MainGap` we say that the `CrossGaps` associated with it are any
  // that start before that main gap (and after a spanner). This information is
  // needed by Paint to calculate the intersection points of row gaps and column
  // gaps.
  void CommitRangeOfCrossGapsBeforeCurrentMainGap();

  // Updates the gap segment states for cross gaps based on the number of
  // columns in each segment row. This is used to determine which cross gaps
  // are blocked, empty on one side, or have columns on both sides.
  void UpdateCrossGapSegmentStates();

  // Attempt to position the list-item marker (if any) beside the child
  // fragment. This requires the fragment to have a baseline. If it doesn't,
  // we'll keep the unpositioned marker around, so that we can retry with a
  // later fragment (if any). If we reach the end of layout and still have an
  // unpositioned marker, it can be placed by calling
  // PositionAnyUnclaimedListMarker().
  void AttemptToPositionListMarker(const PhysicalBoxFragment& child_fragment,
                                   LayoutUnit block_offset);

  // At the end of layout, if no column or spanner were able to position the
  // list-item marker, position the marker at the beginning of the multicol
  // container.
  void PositionAnyUnclaimedListMarker();

  // Propagate the baseline from the given |child| if needed.
  void PropagateBaselineFromChild(const PhysicalBoxFragment& child,
                                  LayoutUnit block_offset);

  // Calculate the smallest possible block-size for columns, based on the
  // content. For column balancing this will be the initial size we'll try with
  // when actually lay out the columns (and then stretch the columns and re-lay
  // out until the desired result is achieved). For column-fill:auto and
  // unconstrained block-size, we also need to go through this, since we need to
  // know the column block-size before performing "real" layout, since all
  // columns in a line need to have the same block-size.
  LayoutUnit ResolveColumnAutoBlockSize(
      const LogicalSize& column_size,
      LayoutUnit line_offset,
      LayoutUnit available_outer_space,
      const BlockBreakToken* child_break_token,
      bool balance_columns);

  LayoutUnit ResolveColumnAutoBlockSizeInternal(
      const LogicalSize& column_size,
      LayoutUnit line_offset,
      LayoutUnit available_outer_space,
      const BlockBreakToken* child_break_token,
      bool balance_columns);

  LayoutUnit ConstrainColumnBlockSize(LayoutUnit size,
                                      LayoutUnit line_offset,
                                      LayoutUnit available_outer_space) const;
  LayoutUnit CurrentContentBlockOffset(
      LayoutUnit border_box_line_offset) const {
    return border_box_line_offset - BorderScrollbarPadding().block_start;
  }

  // Return the inline-size of one column.
  LayoutUnit ColumnInlineSize() const {
    return combined_column_inline_size_ / used_column_count_;
  }

  // Get the percentage resolution size to use for column content (i.e. not
  // spanners).
  LogicalSize ColumnPercentageResolutionSize() const {
    // Percentage block-size on children is resolved against the content-box of
    // the multicol container (just like in regular block layout), while
    // percentage inline-size is restricted by the columns.
    return LogicalSize(ColumnInlineSize(), ChildAvailableSize().block_size);
  }

  bool ShouldWrapColumns() const {
    if (Style().ColumnWrap() == EColumnWrap::kWrap) {
      return true;
    }
    if (Style().ColumnWrap() == EColumnWrap::kNowrap) {
      return false;
    }
    DCHECK_EQ(Style().ColumnWrap(), EColumnWrap::kAuto);
    return !Style().HasAutoColumnHeight();
  }

  // Return true if overflowing columns are created in the inline direction.
  // This is always the case if there's no column wrapping and no nested
  // fragmentation.
  //
  // Return false if overflowing columns are created inside a new line / row.
  // This happens when column wrapping is enabled, and also generally for nested
  // fragmentation, except if the block-size of the column is non-auto and known
  // to be less than or equal to the available space in the outer fragmentainer.
  // This is specified via `column_known_to_fit_in_outer_fragmentainer`.
  bool ColumnsOverflowInInlineDirection(
      bool column_known_to_fit_in_outer_fragmentainer) const;

  ConstraintSpace CreateConstraintSpaceForBalancing(
      const LogicalSize& column_size) const;
  ConstraintSpace CreateConstraintSpaceForSpanner(
      const BlockNode& spanner,
      LayoutUnit block_offset) const;
  ConstraintSpace CreateConstraintSpaceForMinMax() const;

  // The sum of all the current column children's block-sizes, as if they were
  // stacked, including any block-size that is added as a result of
  // ClampedToValidFragmentainerCapacity().
  LayoutUnit TotalColumnBlockSize() const;

  // Return true if row height is constrained by something (e.g. non-auto
  // column-height, or auto column-height and non-auto block-size).
  bool HasRowHeight() const {
    return !Style().HasAutoColumnHeight() ||
           remaining_content_block_size_ > LayoutUnit();
  }

  // Return the height of one row, if constrained (either by non-auto
  // `column-height`, or by auto `column-height` combined with non-auto
  // `block-size`). This function may not be called if row height is
  // unconstrained (if `HasRowHeight()` is false).
  LayoutUnit RowHeight() const {
    if (Style().HasAutoColumnHeight()) {
      DCHECK_GT(remaining_content_block_size_, LayoutUnit());
      return remaining_content_block_size_;
    }
    return LayoutUnit(Style().ColumnHeight());
  }

  // Convert a line offset (which is relative to the block-start of the multicol
  // fragment under construction) to an offset relatively to the start of the
  // current row.
  LayoutUnit OffsetInCurrentRow(LayoutUnit line_offset) const;

  // Return the remaining available space in the current row at the specified
  // line offset. Note that the offset may be inside a row gap, in which case a
  // negative value is returned (since we're past the end of the row).
  LayoutUnit RemainingRowHeightAtOffset(LayoutUnit line_offset) const;

  // Return the offset from the specified line offset to the start of the next
  // row, including any row-gap.
  LayoutUnit OffsetToNextRow(LayoutUnit line_offset) const;

  // Return true if column wrapping is enabled and the specified line offset is
  // past the start of the current row.
  bool IsPastStartInWrappingRow(LayoutUnit line_offset) const;

  const ColumnSpannerPath* spanner_path_ = nullptr;

  int used_column_count_;

  // The total inline-size of all columns (used `column-count`) if placed next
  // to each other, without any column gaps. This is used to avoid rounding
  // errors that would occur when dividing available size by the number of
  // columns.
  LayoutUnit combined_column_inline_size_;

  // The remaining space available to columns in the multicol container, if
  // block-size isn't auto.
  LayoutUnit remaining_content_block_size_;

  LayoutUnit intrinsic_block_size_;
  LayoutUnit tallest_unbreakable_block_size_;
  bool is_constrained_by_outer_fragmentation_context_ = false;

  LayoutUnit column_gap_size_;

  // The offset from the inline-start of the first column in the fragment, to
  // the inline-start of the first (imaginary or real) column that has (or would
  // have) overflowed in the inline direction.
  LayoutUnit inline_stride_;

  LayoutUnit row_gap_size_;

  // One entry for each row gap, and one entry between column content and
  // spanners. There is no gap between column content and spanners, but column
  // gaps need to be interrupted, since they shouldn't necessarily overlap with
  // spanners.
  Vector<MainGap> main_gaps_;

  // One entry for each column gap.
  Vector<CrossGap> cross_gaps_;

  // Offset to the first column (in the first row), from the start border edge
  // of the resulting multicol fragment. Will only be set if needed, i.e. for
  // gap decorations.
  std::optional<LogicalOffset> first_column_offset_;

  // Tracks the maximum number of columns in any row.
  wtf_size_t max_columns_in_row_ = 0;

  // This will be set during (outer) block fragmentation once we've processed
  // the first piece of content of the multicol container. It is used to check
  // if we're at a valid class A  breakpoint (between block-level siblings).
  bool has_processed_first_child_ = false;

  // This is the number of columns in each row, where the index in the vector
  // indicates the index of the row of columns. Keep in mind, that this used
  // used for gap decorations, which treats the area behind a spanner as a
  // segment. Therefore, this vector also includes spanners as "rows" but marked
  // as having `kNotFound` columns.
  // TODO(crbug.com/440123087): Since the number of optionals for gap
  // decorations has grown, explore encapsulating the logic in an `Accumulator`
  // class similar to `Flex` and `Grid`.
  std::optional<Vector<wtf_size_t>> columns_per_row_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_COLUMN_LAYOUT_ALGORITHM_H_
