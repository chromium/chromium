// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_flex_layout_algorithm.h"

#include <memory>
#include "third_party/blink/renderer/core/layout/flexible_box_algorithm.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

NGFlexLayoutAlgorithm::NGFlexLayoutAlgorithm(NGBlockNode node,
                                             const NGConstraintSpace& space,
                                             const NGBreakToken* break_token)
    : NGLayoutAlgorithm(node, space, ToNGBlockBreakToken(break_token)) {}

scoped_refptr<NGLayoutResult> NGFlexLayoutAlgorithm::Layout() {
  DCHECK(!Style().IsColumnFlexDirection())
      << "Column flexboxes aren't supported yet";
  DCHECK(!NeedMinMaxSize(ConstraintSpace(), Style()))
      << "Don't support that yet";

  NGLogicalSize flex_container_border_box_size =
      CalculateBorderBoxSize(ConstraintSpace(), Node());
  NGLogicalSize flex_container_content_box_size = ShrinkAvailableSize(
      flex_container_border_box_size,
      CalculateBorderScrollbarPadding(ConstraintSpace(), Node()));
  LayoutUnit flex_container_border_box_inline_size =
      flex_container_border_box_size.inline_size;
  LayoutUnit flex_container_content_inline_size =
      flex_container_content_box_size.inline_size;

  FlexLayoutAlgorithm algorithm(&Style(), flex_container_content_inline_size);
  for (NGLayoutInputNode generic_child = Node().FirstChild(); generic_child;
       generic_child = generic_child.NextSibling()) {
    NGBlockNode child = ToNGBlockNode(generic_child);
    if (child.IsOutOfFlowPositioned())
      continue;

    NGConstraintSpace child_space =
        NGConstraintSpaceBuilder(ConstraintSpace())
            .SetAvailableSize(flex_container_content_box_size)
            .SetPercentageResolutionSize(flex_container_content_box_size)
            .ToConstraintSpace(child.Style().GetWritingMode());

    LayoutUnit main_axis_border_and_padding =
        ComputeBorders(child_space, child.Style()).InlineSum() +
        ComputePadding(child_space, child.Style()).InlineSum();
    // ComputeMinMaxSize will layout the child if it has an orthogonal writing
    // mode. MinMaxSize will be in the container's inline direction.
    MinMaxSizeInput zero_input;
    MinMaxSize min_max_sizes_border_box = child.ComputeMinMaxSize(
        ConstraintSpace().GetWritingMode(), zero_input, &child_space);

    LayoutUnit flex_base_border_box;
    if (child.Style().FlexBasis().IsAuto() && child.Style().Width().IsAuto()) {
      flex_base_border_box = min_max_sizes_border_box.max_size;
    } else {
      Length length_to_resolve = child.Style().FlexBasis();
      if (length_to_resolve.IsAuto())
        length_to_resolve = child.Style().Width();
      DCHECK(!length_to_resolve.IsAuto());

      // TODO(dgrogan): Use ResolveBlockLength here for column flex boxes.

      flex_base_border_box = ResolveInlineLength(
          child_space, child.Style(), min_max_sizes_border_box,
          length_to_resolve, LengthResolveType::kContentSize,
          LengthResolvePhase::kLayout);
    }

    // Spec calls this "flex base size"
    // https://www.w3.org/TR/css-flexbox-1/#algo-main-item
    // Blink's FlexibleBoxAlgorithm expects it to be content + scrollbar widths,
    // but no padding or border.
    LayoutUnit flex_base_content_size =
        flex_base_border_box - main_axis_border_and_padding;

    LayoutUnit main_axis_margin =
        ComputeMarginsForSelf(child_space, child.Style()).InlineSum();

    // TODO(dgrogan): When child has a min/max-{width,height} set, call
    // Resolve{Inline,Block}Length here with child's style and constraint space.
    // Pass kMinSize, kMaxSize as appropriate.
    // Further, min-width:auto has special meaning for flex items. We'll need to
    // calculate that here by either extracting the logic from legacy or
    // reimplementing. When resolved, pass it here.
    // https://www.w3.org/TR/css-flexbox-1/#min-size-auto
    MinMaxSize min_max_sizes_in_main_axis_direction{LayoutUnit(),
                                                    LayoutUnit::Max()};
    algorithm
        .emplace_back(child.GetLayoutBox(), flex_base_content_size,
                      min_max_sizes_in_main_axis_direction,
                      main_axis_border_and_padding, main_axis_margin)
        .ng_input_node = child;
  }

  NGBoxStrut borders_scrollbar_padding =
      CalculateBorderScrollbarPadding(ConstraintSpace(), Node());
  LayoutUnit main_axis_offset = borders_scrollbar_padding.inline_start;
  LayoutUnit cross_axis_offset = borders_scrollbar_padding.block_start;
  FlexLine* line;
  while ((line = algorithm.ComputeNextFlexLine(
              flex_container_border_box_inline_size))) {
    // TODO(dgrogan): This parameter is more complicated for columns.
    line->SetContainerMainInnerSize(flex_container_content_inline_size);
    line->FreezeInflexibleItems();
    while (!line->ResolveFlexibleLengths()) {
      continue;
    }
    for (wtf_size_t i = 0; i < line->line_items.size(); ++i) {
      FlexItem& flex_item = line->line_items[i];
      NGConstraintSpaceBuilder space_builder(ConstraintSpace());
      NGLogicalSize available_size(flex_item.flexed_content_size +
                                       flex_item.main_axis_border_and_padding,
                                   flex_container_content_box_size.block_size);
      space_builder.SetAvailableSize(available_size);
      space_builder.SetPercentageResolutionSize(
          flex_container_content_box_size);
      space_builder.SetIsFixedSizeInline(true);
      NGConstraintSpace child_space = space_builder.ToConstraintSpace(
          flex_item.box->Style()->GetWritingMode());
      flex_item.layout_result =
          ToNGBlockNode(flex_item.ng_input_node)
              .Layout(child_space, nullptr /*break token*/);
      flex_item.cross_axis_size =
          flex_item.layout_result->PhysicalFragment()->Size().height;
      // TODO(dgrogan): Port logic from
      // LayoutFlexibleBox::CrossAxisIntrinsicExtentForChild?
      flex_item.cross_axis_intrinsic_size = flex_item.cross_axis_size;
    }
    // cross_axis_offset is updated in each iteration of the loop, for passing
    // in to the next iteration.
    line->ComputeLineItemsPosition(main_axis_offset, cross_axis_offset);

    for (wtf_size_t i = 0; i < line->line_items.size(); ++i) {
      FlexItem& flex_item = line->line_items[i];
      container_builder_.AddChild(
          *flex_item.layout_result,
          {flex_item.desired_location.X(), flex_item.desired_location.Y()});
    }

    // TODO(dgrogan): For column flex containers, keep track of tallest flex
    // line and pass to ComputeBlockSizeForFragment as content_size.
  }
  LayoutUnit intrinsic_block_content_size = cross_axis_offset;
  LayoutUnit intrinsic_block_size =
      intrinsic_block_content_size + borders_scrollbar_padding.BlockSum();
  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), intrinsic_block_size);
  container_builder_.SetBlockSize(block_size);
  container_builder_.SetInlineSize(flex_container_border_box_inline_size);
  container_builder_.SetBorders(ComputeBorders(ConstraintSpace(), Style()));
  container_builder_.SetPadding(ComputePadding(ConstraintSpace(), Style()));
  return container_builder_.ToBoxFragment();
}

base::Optional<MinMaxSize> NGFlexLayoutAlgorithm::ComputeMinMaxSize(
    const MinMaxSizeInput& input) const {
  // TODO(dgrogan): Implement this.
  return base::nullopt;
}

}  // namespace blink
