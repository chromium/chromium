// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_column_layout_algorithm.h"

#include <algorithm>
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_baseline.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

inline bool NeedsColumnBalancing(LayoutUnit block_size,
                                 const ComputedStyle& style) {
  return block_size == NGSizeIndefinite ||
         style.GetColumnFill() == EColumnFill::kBalance;
}

// Constrain a balanced column block size to not overflow the multicol
// container.
LayoutUnit ConstrainColumnBlockSize(LayoutUnit size,
                                    NGBlockNode node,
                                    const NGConstraintSpace& space) {
  // The {,max-}{height,width} properties are specified on the multicol
  // container, but here we're calculating the column block sizes inside the
  // multicol container, which isn't exactly the same. We may shrink the column
  // block size here, but we'll never stretch it, because the value passed is
  // the perfect balanced block size. Making it taller would only disrupt the
  // balanced output, for no reason. The only thing we need to worry about here
  // is to not overflow the multicol container.

  // First of all we need to convert the size to a value that can be compared
  // against the resolved properties on the multicol container. That means that
  // we have to convert the value from content-box to border-box.
  NGBoxStrut border_scrollbar_padding =
      CalculateBorderScrollbarPadding(space, node);
  LayoutUnit extra = border_scrollbar_padding.BlockSum();
  size += extra;

  const ComputedStyle& style = node.Style();
  LayoutUnit max = ResolveBlockLength(space, style, style.LogicalMaxHeight(),
                                      size, LengthResolveType::kMaxSize,
                                      LengthResolvePhase::kLayout);
  LayoutUnit extent = ResolveBlockLength(space, style, style.LogicalHeight(),
                                         size, LengthResolveType::kContentSize,
                                         LengthResolvePhase::kLayout);
  if (extent != NGSizeIndefinite) {
    // A specified height/width will just constrain the maximum length.
    max = std::min(max, extent);
  }

  // Constrain and convert the value back to content-box.
  size = std::min(size, max);
  return size - extra;
}

}  // namespace

NGColumnLayoutAlgorithm::NGColumnLayoutAlgorithm(
    NGBlockNode node,
    const NGConstraintSpace& space,
    const NGBreakToken* break_token)
    : NGLayoutAlgorithm(node, space, ToNGBlockBreakToken(break_token)) {}

scoped_refptr<NGLayoutResult> NGColumnLayoutAlgorithm::Layout() {
  NGBoxStrut borders = ComputeBorders(ConstraintSpace(), Node());
  NGBoxStrut scrollbars = Node().GetScrollbarSizes();
  NGBoxStrut padding = ComputePadding(ConstraintSpace(), Style()) +
                       ComputeIntrinsicPadding(ConstraintSpace(), Node());
  NGBoxStrut border_scrollbar_padding = borders + scrollbars + padding;
  NGLogicalSize border_box_size =
      CalculateBorderBoxSize(ConstraintSpace(), Node());
  NGLogicalSize content_box_size =
      ShrinkAvailableSize(border_box_size, border_scrollbar_padding);
  NGLogicalSize column_size = CalculateColumnSize(content_box_size);

  WritingMode writing_mode = ConstraintSpace().GetWritingMode();
  LayoutUnit column_block_offset(border_scrollbar_padding.block_start);
  LayoutUnit column_inline_progression =
      column_size.inline_size +
      ResolveUsedColumnGap(content_box_size.inline_size, Style());
  int used_column_count =
      ResolveUsedColumnCount(content_box_size.inline_size, Style());

  do {
    scoped_refptr<const NGBlockBreakToken> break_token = BreakToken();
    LayoutUnit intrinsic_block_size;
    LayoutUnit column_inline_offset(border_scrollbar_padding.inline_start);
    int actual_column_count = 0;
    int forced_break_count = 0;

    // Each column should calculate their own minimal space shortage. Find the
    // lowest value of those. This will serve as the column stretch amount, if
    // we determine that stretching them is necessary and possible (column
    // balancing).
    LayoutUnit minimal_space_shortage(LayoutUnit::Max());

    // Allow any block-start margins at the start of the first column.
    bool separate_leading_margins = true;

    do {
      // Lay out one column. Each column will become a fragment.
      NGConstraintSpace child_space = CreateConstraintSpaceForColumns(
          column_size, separate_leading_margins);

      NGBlockLayoutAlgorithm child_algorithm(Node(), child_space,
                                             break_token.get());
      child_algorithm.SetBoxType(NGPhysicalFragment::kColumnBox);
      scoped_refptr<NGLayoutResult> result = child_algorithm.Layout();
      scoped_refptr<const NGPhysicalBoxFragment> column(
          ToNGPhysicalBoxFragment(result->PhysicalFragment().get()));

      NGLogicalOffset logical_offset(column_inline_offset, column_block_offset);
      container_builder_.AddChild(*result, logical_offset);

      LayoutUnit space_shortage = result->MinimalSpaceShortage();
      if (space_shortage > LayoutUnit()) {
        minimal_space_shortage =
            std::min(minimal_space_shortage, space_shortage);
      }
      actual_column_count++;
      if (result->HasForcedBreak()) {
        forced_break_count++;
        separate_leading_margins = true;
      } else {
        separate_leading_margins = false;
      }

      LayoutUnit block_size =
          NGBoxFragment(writing_mode, ConstraintSpace().Direction(), *column)
              .BlockSize();
      intrinsic_block_size =
          std::max(intrinsic_block_size, column_block_offset + block_size);

      column_inline_offset += column_inline_progression;
      break_token = ToNGBlockBreakToken(column->BreakToken());
    } while (break_token && !break_token->IsFinished());

    // If we overflowed (actual column count larger than what we have room for),
    // and we're supposed to calculate the column lengths automatically (column
    // balancing), see if we're able to stretch them.
    //
    // We can only stretch the columns if we have at least one column that could
    // take more content, and we also need to know the stretch amount (minimal
    // space shortage). We need at least one soft break opportunity to do
    // this. If forced breaks cause too many breaks, there's no stretch amount
    // that could prevent the actual column count from overflowing.
    if (actual_column_count > used_column_count &&
        actual_column_count > forced_break_count + 1 &&
        minimal_space_shortage != LayoutUnit::Max()) {
      LayoutUnit new_column_block_size =
          StretchColumnBlockSize(minimal_space_shortage, column_size.block_size,
                                 content_box_size.block_size);

      DCHECK_GE(new_column_block_size, column_size.block_size);
      if (new_column_block_size > column_size.block_size) {
        // Re-attempt layout with taller columns.
        column_size.block_size = new_column_block_size;
        container_builder_.RemoveChildren();
        continue;
      }
    }
    container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);
    break;
  } while (true);

  NGOutOfFlowLayoutPart(&container_builder_, Node().IsAbsoluteContainer(),
                        Node().IsFixedContainer(), borders + scrollbars,
                        ConstraintSpace(), Style())
      .Run();

  // TODO(mstensho): Propagate baselines.

  if (border_box_size.block_size == NGSizeIndefinite) {
    // Get the block size from the columns if it's auto.
    border_box_size.block_size =
        column_size.block_size + border_scrollbar_padding.BlockSum();
  }
  container_builder_.SetInlineSize(border_box_size.inline_size);
  container_builder_.SetBlockSize(border_box_size.block_size);
  container_builder_.SetBorders(ComputeBorders(ConstraintSpace(), Style()));
  container_builder_.SetPadding(ComputePadding(ConstraintSpace(), Style()));

  return container_builder_.ToBoxFragment();
}

base::Optional<MinMaxSize> NGColumnLayoutAlgorithm::ComputeMinMaxSize(
    const MinMaxSizeInput& input) const {
  // First calculate the min/max sizes of columns.
  NGBlockLayoutAlgorithm algorithm(Node(), ConstraintSpace());
  MinMaxSizeInput child_input(input);
  child_input.size_type = NGMinMaxSizeType::kContentBoxSize;
  base::Optional<MinMaxSize> min_max_sizes =
      algorithm.ComputeMinMaxSize(child_input);
  DCHECK(min_max_sizes.has_value());
  MinMaxSize sizes = min_max_sizes.value();

  // If column-width is non-auto, pick the larger of that and intrinsic column
  // width.
  if (!Style().HasAutoColumnWidth()) {
    sizes.min_size =
        std::max(sizes.min_size, LayoutUnit(Style().ColumnWidth()));
    sizes.max_size = std::max(sizes.max_size, sizes.min_size);
  }

  // Now convert those column min/max values to multicol container min/max
  // values. We typically have multiple columns and also gaps between them.
  int column_count = Style().ColumnCount();
  DCHECK_GE(column_count, 1);
  sizes.min_size *= column_count;
  sizes.max_size *= column_count;
  LayoutUnit column_gap = ResolveUsedColumnGap(LayoutUnit(), Style());
  sizes += column_gap * (column_count - 1);

  if (input.size_type == NGMinMaxSizeType::kBorderBoxSize) {
    LayoutUnit border_scrollbar_padding =
        CalculateBorderScrollbarPadding(ConstraintSpace(), node_).InlineSum();
    sizes += border_scrollbar_padding;
  }

  return sizes;
}

NGLogicalSize NGColumnLayoutAlgorithm::CalculateColumnSize(
    const NGLogicalSize& content_box_size) {
  NGLogicalSize column_size = content_box_size;
  DCHECK_GE(column_size.inline_size, LayoutUnit());
  column_size.inline_size =
      ResolveUsedColumnInlineSize(column_size.inline_size, Style());

  if (NeedsColumnBalancing(column_size.block_size, Style())) {
    int used_count =
        ResolveUsedColumnCount(content_box_size.inline_size, Style());
    column_size.block_size =
        CalculateBalancedColumnBlockSize(column_size, used_count);
  }

  return column_size;
}

LayoutUnit NGColumnLayoutAlgorithm::CalculateBalancedColumnBlockSize(
    const NGLogicalSize& column_size,
    int column_count) {
  // To calculate a balanced column size, we need to figure out how tall our
  // content is. To do that we need to lay out. Create a special constraint
  // space for column balancing, without splitting into fragmentainers. It will
  // make us lay out all the multicol content as one single tall strip. When
  // we're done with this layout pass, we can examine the result and calculate
  // an ideal column block size.
  NGConstraintSpace space = CreateConstaintSpaceForBalancing(column_size);
  NGBlockLayoutAlgorithm balancing_algorithm(Node(), space);
  scoped_refptr<NGLayoutResult> result = balancing_algorithm.Layout();

  // TODO(mstensho): This is where the fun begins. We need to examine the entire
  // fragment tree, not just the root.
  NGFragment fragment(space.GetWritingMode(), *result->PhysicalFragment());
  LayoutUnit single_strip_block_size = fragment.BlockSize();

  // Some extra care is required the division here. We want a the resulting
  // LayoutUnit value to be large enough to prevent overflowing columns. Use
  // floating point to get higher precision than LayoutUnit. Then convert it to
  // a LayoutUnit, but round it up to the nearest value that LayoutUnit is able
  // to represent.
  LayoutUnit block_size = LayoutUnit::FromFloatCeil(
      single_strip_block_size.ToFloat() / static_cast<float>(column_count));

  // Finally, honor {,min-,max-}{height,width} properties.
  return ConstrainColumnBlockSize(block_size, Node(), ConstraintSpace());
}

LayoutUnit NGColumnLayoutAlgorithm::StretchColumnBlockSize(
    LayoutUnit minimal_space_shortage,
    LayoutUnit current_column_size,
    LayoutUnit container_content_box_block_size) const {
  if (!NeedsColumnBalancing(container_content_box_block_size, Style()))
    return current_column_size;
  LayoutUnit length = current_column_size + minimal_space_shortage;
  // Honor {,min-,max-}{height,width} properties.
  return ConstrainColumnBlockSize(length, Node(), ConstraintSpace());
}

NGConstraintSpace NGColumnLayoutAlgorithm::CreateConstraintSpaceForColumns(
    const NGLogicalSize& column_size,
    bool separate_leading_margins) const {
  NGConstraintSpaceBuilder space_builder(ConstraintSpace());
  space_builder.SetAvailableSize(column_size);
  space_builder.SetPercentageResolutionSize(column_size);

  if (NGBaseline::ShouldPropagateBaselines(Node()))
    space_builder.AddBaselineRequests(ConstraintSpace().BaselineRequests());

  // To ensure progression, we need something larger than 0 here. The spec
  // actually says that fragmentainers have to accept at least 1px of content.
  // See https://www.w3.org/TR/css-break-3/#breaking-rules
  LayoutUnit column_block_size =
      std::max(column_size.block_size, LayoutUnit(1));

  space_builder.SetFragmentationType(kFragmentColumn);
  space_builder.SetFragmentainerBlockSize(column_block_size);
  space_builder.SetFragmentainerSpaceAtBfcStart(column_block_size);
  space_builder.SetIsNewFormattingContext(true);
  space_builder.SetIsAnonymous(true);
  space_builder.SetSeparateLeadingFragmentainerMargins(
      separate_leading_margins);

  return space_builder.ToConstraintSpace(Style().GetWritingMode());
}

NGConstraintSpace NGColumnLayoutAlgorithm::CreateConstaintSpaceForBalancing(
    const NGLogicalSize& column_size) const {
  NGConstraintSpaceBuilder space_builder(ConstraintSpace());
  space_builder.SetAvailableSize({column_size.inline_size, NGSizeIndefinite});
  space_builder.SetPercentageResolutionSize(column_size);
  space_builder.SetIsNewFormattingContext(true);
  space_builder.SetIsAnonymous(true);
  space_builder.SetIsIntermediateLayout(true);

  return space_builder.ToConstraintSpace(Style().GetWritingMode());
}

}  // namespace Blink
