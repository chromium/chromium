// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_COLUMN_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_COLUMN_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"

namespace blink {

enum class NGBreakStatus;
class NGBlockNode;
class NGBlockBreakToken;
class NGColumnSpannerPath;
class NGConstraintSpace;
struct LogicalSize;
struct NGMarginStrut;

class CORE_EXPORT NGColumnLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  explicit NGColumnLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

  const NGLayoutResult* Layout() override;

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&) override;

 private:
  MinMaxSizesResult ComputeSpannersMinMaxSizes(
      const NGBlockNode& search_parent) const;

  // Lay out as many children as we can. If |kNeedsEarlierBreak| is returned, it
  // means that we ran out of space at an unappealing location, and need to
  // relayout and break earlier (because we have a better breakpoint there). If
  // |kBrokeBefore| is returned, it means that we need to break before the
  // multicol container, and retry in the next fragmentainer.
  NGBreakStatus LayoutChildren();

  // Lay out one row of columns. The layout result returned is for the last
  // column that was laid out. The rows themselves don't create fragments. If
  // we're in a nested fragmentation context and completely out of outer
  // fragmentainer space, nullptr will be returned.
  const NGLayoutResult* LayoutRow(const NGBlockBreakToken* next_column_token,
                                  NGMarginStrut*);

  // Lay out a column spanner. The return value will tell whether to break
  // before the spanner or not. If |NGBreakStatus::kContinue| is returned, and
  // no break token was set, it means that we can proceed to the next row of
  // columns.
  NGBreakStatus LayoutSpanner(NGBlockNode spanner_node,
                              const NGBlockBreakToken* break_token,
                              NGMarginStrut*);

  // Attempt to position the list-item marker (if any) beside the child
  // fragment. This requires the fragment to have a baseline. If it doesn't,
  // we'll keep the unpositioned marker around, so that we can retry with a
  // later fragment (if any). If we reach the end of layout and still have an
  // unpositioned marker, it can be placed by calling
  // PositionAnyUnclaimedListMarker().
  void AttemptToPositionListMarker(const NGPhysicalBoxFragment& child_fragment,
                                   LayoutUnit block_offset);

  // At the end of layout, if no column or spanner were able to position the
  // list-item marker, position the marker at the beginning of the multicol
  // container.
  void PositionAnyUnclaimedListMarker();

  // Propagate the baseline from the given |child| if needed.
  void PropagateBaselineFromChild(const NGPhysicalBoxFragment& child,
                                  LayoutUnit block_offset);

  // Calculate the smallest possible block-size for columns, based on the
  // content. For column balancing this will be the initial size we'll try with
  // when actually lay out the columns (and then stretch the columns and re-lay
  // out until the desired result is achieved). For column-fill:auto and
  // unconstrained block-size, we also need to go through this, since we need to
  // know the column block-size before performing "real" layout, since all
  // columns in a row need to have the same block-size.
  LayoutUnit ResolveColumnAutoBlockSize(
      const LogicalSize& column_size,
      LayoutUnit row_offset,
      const NGBlockBreakToken* child_break_token,
      bool balance_columns);

  LayoutUnit ResolveColumnAutoBlockSizeInternal(
      const LogicalSize& column_size,
      LayoutUnit row_offset,
      const NGBlockBreakToken* child_break_token,
      bool balance_columns);

  LayoutUnit ConstrainColumnBlockSize(LayoutUnit size,
                                      LayoutUnit row_offset) const;
  LayoutUnit CurrentContentBlockOffset(LayoutUnit border_box_row_offset) const {
    return border_box_row_offset - BorderScrollbarPadding().block_start;
  }

  // Get the percentage resolution size to use for column content (i.e. not
  // spanners).
  LogicalSize ColumnPercentageResolutionSize() const {
    // Percentage block-size on children is resolved against the content-box of
    // the multicol container (just like in regular block layout), while
    // percentage inline-size is restricted by the columns.
    return LogicalSize(column_inline_size_, ChildAvailableSize().block_size);
  }

  NGConstraintSpace CreateConstraintSpaceForBalancing(
      const LogicalSize& column_size) const;
  NGConstraintSpace CreateConstraintSpaceForSpanner(
      const NGBlockNode& spanner,
      LayoutUnit block_offset) const;
  NGConstraintSpace CreateConstraintSpaceForMinMax() const;

  // If this is a nested multicol container, and there's no room for anything in
  // the current outer fragmentainer, we're normally allowed to abort (typically
  // with NGLayoutResult::kOutOfFragmentainerSpace), and retry in the next outer
  // fragmentainer. This is not the case for out-of-flow positioned multicol
  // containers, though, as we're not allowed to insert a soft break before an
  // out-of-flow positioned node. Our implementation requires that an OOF start
  // in the fragmentainer where it would "naturally" occur. This is also not the
  // case for floated multicols since float margins are treated as monolithic
  // [1]. Given this, the margin of the float wouldn't get truncated after a
  // break, which could lead to an infinite loop.
  //
  // [1] https://codereview.chromium.org/2479483002
  bool MayAbortOnInsufficientSpace() const {
    DCHECK(is_constrained_by_outer_fragmentation_context_);
    return !Node().IsFloatingOrOutOfFlowPositioned();
  }

  // The sum of all the current column children's block-sizes, as if they were
  // stacked, including any block-size that is added as a result of
  // ClampedToValidFragmentainerCapacity().
  LayoutUnit TotalColumnBlockSize() const;

  const NGColumnSpannerPath* spanner_path_ = nullptr;

  int used_column_count_;
  LayoutUnit column_inline_size_;
  LayoutUnit column_inline_progression_;
  LayoutUnit column_block_size_;
  LayoutUnit intrinsic_block_size_;
  LayoutUnit tallest_unbreakable_block_size_;
  bool is_constrained_by_outer_fragmentation_context_ = false;

  // This will be set during (outer) block fragmentation once we've processed
  // the first piece of content of the multicol container. It is used to check
  // if we're at a valid class A  breakpoint (between block-level siblings).
  bool has_processed_first_child_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_COLUMN_LAYOUT_ALGORITHM_H_
