// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_COLUMN_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_COLUMN_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"

namespace blink {

enum class NGBreakStatus;
class NGBlockNode;
class NGBlockBreakToken;
class NGConstraintSpace;
struct LogicalSize;
struct NGMarginStrut;

class CORE_EXPORT NGColumnLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  NGColumnLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

  scoped_refptr<const NGLayoutResult> Layout() override;

  base::Optional<MinMaxSize> ComputeMinMaxSize(
      const MinMaxSizeInput&) const override;

 private:
  // Lay out as many children as we can. If false is returned, it means that we
  // ran out of space at an unappealing location, and need to relayout and break
  // earlier (because we have a better breakpoint there).
  bool LayoutChildren();

  // Lay out one row of columns. The layout result returned is for the last
  // column that was laid out. The rows themselves don't create fragments.
  scoped_refptr<const NGLayoutResult> LayoutRow(
      const NGBlockBreakToken* next_column_token,
      NGMarginStrut*);

  // Lay out a column spanner. The return value will tell whether to break
  // before the spanner or not. If we're not to break before the spanner, but
  // rather inside, |spanner_break_token| will be set, so that we know where to
  // resume in the next outer fragmentainer. If |NGBreakStatus::kContinue| is
  // returned, and no break token was set, it means that we can proceed to the
  // next row of columns.
  NGBreakStatus LayoutSpanner(NGBlockNode spanner_node,
                              const NGBlockBreakToken* break_token,
                              NGMarginStrut*,
                              scoped_refptr<const NGBlockBreakToken>*);

  LayoutUnit CalculateBalancedColumnBlockSize(
      const LogicalSize& column_size,
      const NGBlockBreakToken* child_break_token);

  // Stretch the column length. We do this during column balancing, when we
  // discover that the current length isn't large enough to fit all content.
  LayoutUnit StretchColumnBlockSize(LayoutUnit minimal_space_shortage,
                                    LayoutUnit current_column_size) const;

  LayoutUnit ConstrainColumnBlockSize(LayoutUnit size) const;
  LayoutUnit CurrentContentBlockOffset() const {
    return intrinsic_block_size_ - border_scrollbar_padding_.block_start;
  }

  // Finalize layout after breaking before a spanner.
  void FinishAfterBreakBeforeSpanner(
      scoped_refptr<const NGBlockBreakToken> next_column_token);

  // Lay out again, this time with a predefined good breakpoint that we
  // discovered in the first pass. This happens when we run out of space in a
  // fragmentainer at an less-than-ideal location, due to breaking restrictions,
  // such as break-before:avoid or break-after:avoid.
  scoped_refptr<const NGLayoutResult> RelayoutAndBreakEarlier();

  NGConstraintSpace CreateConstraintSpaceForColumns(
      const LogicalSize& column_size,
      bool is_first_fragmentainer,
      bool balance_columns) const;
  NGConstraintSpace CreateConstraintSpaceForBalancing(
      const LogicalSize& column_size) const;
  NGConstraintSpace CreateConstraintSpaceForSpanner(
      LayoutUnit block_offset) const;
  NGConstraintSpace CreateConstraintSpaceForMinMax() const;

  // When set, this will specify where to break before or inside.
  const NGEarlyBreak* early_break_ = nullptr;

  // Border + padding sum, resolved from the node's computed style.
  const NGBoxStrut border_padding_;

  // Border + scrollbar + padding sum for the fragment to be generated (most
  // importantly, for non-first fragments, leading block border + scrollbar +
  // padding is zero).
  NGBoxStrut border_scrollbar_padding_;

  LogicalSize content_box_size_;
  int used_column_count_;
  LayoutUnit column_inline_size_;
  LayoutUnit column_inline_progression_;
  LayoutUnit intrinsic_block_size_;
  bool is_constrained_by_outer_fragmentation_context_ = false;

  // This will be set during (outer) block fragmentation once we've processed
  // the first piece of content of the multicol container. It is used to check
  // if we're at a valid class A  breakpoint (between block-level siblings).
  bool has_processed_first_child_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_COLUMN_LAYOUT_ALGORITHM_H_
