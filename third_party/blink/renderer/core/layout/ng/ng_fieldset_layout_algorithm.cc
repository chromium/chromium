// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_fieldset_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/layout_fieldset.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"

namespace blink {

NGFieldsetLayoutAlgorithm::NGFieldsetLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params),
      border_padding_(params.fragment_geometry.border +
                      params.fragment_geometry.padding) {
  container_builder_.SetIsNewFormattingContext(
      params.space.IsNewFormattingContext());
  container_builder_.SetInitialFragmentGeometry(params.fragment_geometry);
}

scoped_refptr<const NGLayoutResult> NGFieldsetLayoutAlgorithm::Layout() {
  // TODO(mstensho): Support block fragmentation.
  DCHECK(!BreakToken());

  // Layout of a fieldset container consists of two parts: Create a child
  // fragment for the rendered legend (if any), and create a child fragment for
  // the fieldset contents anonymous box (if any). Fieldset scrollbars and
  // padding will not be applied to the fieldset container itself, but rather to
  // the fieldset contents anonymous child box. The reason for this is that the
  // rendered legend shouldn't be part of the scrollport; the legend is
  // essentially a part of the block-start border, and should not scroll along
  // with the actual fieldset contents. Since scrollbars are handled by the
  // anonymous child box, and since padding is inside the scrollport, padding
  // also needs to be handled by the anonymous child.
  NGBoxStrut borders = container_builder_.Borders();
  NGBoxStrut padding = container_builder_.Padding();
  LogicalSize border_box_size = container_builder_.InitialBorderBoxSize();
  const auto writing_mode = ConstraintSpace().GetWritingMode();
  LayoutUnit block_start_padding_edge =
      container_builder_.Borders().block_start;

  // TODO(vmpstr): Skip child (including legend) layout for fieldset elements.
  if (NGBlockNode legend = Node().GetRenderedLegend()) {
    // Lay out the legend. While the fieldset container normally ignores its
    // padding, the legend is laid out within what would have been the content
    // box had the fieldset been a regular block with no weirdness.
    LogicalSize content_box_size =
        ShrinkAvailableSize(border_box_size, border_padding_);
    auto legend_space =
        CreateConstraintSpaceForLegend(legend, content_box_size);
    auto result = legend.Layout(legend_space, BreakToken());
    const auto& physical_fragment = result->PhysicalFragment();
    NGBoxStrut legend_margins =
        ComputeMarginsFor(legend_space, legend.Style(), ConstraintSpace());
    // If the margin box of the legend is at least as tall as the fieldset
    // block-start border width, it will start at the block-start border edge of
    // the fieldset. As a paint effect, the block-start border will be pushed so
    // that the center of the border will be flush with the center of the
    // border-box of the legend.
    // TODO(mstensho): inline alignment
    LogicalOffset legend_offset = LogicalOffset(
        border_padding_.inline_start + legend_margins.inline_start,
        legend_margins.block_start);
    LayoutUnit legend_margin_box_block_size =
        NGFragment(writing_mode, physical_fragment).BlockSize() +
        legend_margins.BlockSum();
    LayoutUnit space_left = borders.block_start - legend_margin_box_block_size;
    if (space_left > LayoutUnit()) {
      // If the border is the larger one, though, it will stay put at the
      // border-box block-start edge of the fieldset. Then it's the legend that
      // needs to be pushed. We'll center the margin box in this case, to make
      // sure that both margins remain within the area occupied by the border
      // also after adjustment.
      legend_offset.block_offset += space_left / 2;
    } else {
      // If the legend is larger than the width of the fieldset block-start
      // border, the actual padding edge of the fieldset will be moved
      // accordingly. This will be the block-start offset for the fieldset
      // contents anonymous box.
      block_start_padding_edge = legend_margin_box_block_size;
    }

    container_builder_.AddChild(physical_fragment, legend_offset);
  }

  NGBoxStrut borders_with_legend = borders;
  borders_with_legend.block_start = block_start_padding_edge;
  LayoutUnit intrinsic_block_size = borders_with_legend.BlockSum();

  // Proceed with normal fieldset children (excluding the rendered legend). They
  // all live inside an anonymous child box of the fieldset container.
  if (auto fieldset_content = Node().GetFieldsetContent()) {
    LogicalSize adjusted_padding_box_size =
        ShrinkAvailableSize(border_box_size, borders_with_legend);
    auto child_space =
        CreateConstraintSpaceForFieldsetContent(adjusted_padding_box_size);
    auto result = fieldset_content.Layout(child_space, BreakToken());
    const auto& physical_fragment = result->PhysicalFragment();
    container_builder_.AddChild(physical_fragment,
                                borders_with_legend.StartOffset());

    intrinsic_block_size +=
        NGFragment(writing_mode, physical_fragment).BlockSize();
  } else {
    // There was no anonymous child to provide the padding, so we have to add it
    // ourselves.
    intrinsic_block_size += padding.BlockSum();
  }

  intrinsic_block_size =
      ClampIntrinsicBlockSize(Node(), border_padding_, intrinsic_block_size);

  // Recompute the block-axis size now that we know our content size.
  border_box_size.block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), border_padding_, intrinsic_block_size);

  // The above computation utility knows nothing about fieldset weirdness. The
  // legend may eat from the available content box block size. Make room for
  // that if necessary.
  // Note that in size containment, we have to consider sizing as if we have no
  // contents, with the conjecture being that legend is part of the contents.
  // Thus, only do this adjustment if we do not contain size.
  if (!Node().ShouldApplySizeContainment()) {
    LayoutUnit minimum_border_box_block_size =
        borders_with_legend.BlockSum() + padding.BlockSum();
    border_box_size.block_size =
        std::max(border_box_size.block_size, minimum_border_box_block_size);
  }

  container_builder_.SetIsFieldsetContainer();
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);
  container_builder_.SetBlockSize(border_box_size.block_size);

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), borders_with_legend,
                        &container_builder_)
      .Run();

  return container_builder_.ToBoxFragment();
}

base::Optional<MinMaxSize> NGFieldsetLayoutAlgorithm::ComputeMinMaxSize(
    const MinMaxSizeInput& input) const {
  MinMaxSize sizes;

  bool apply_size_containment = node_.ShouldApplySizeContainment();
  // TODO(crbug.com/1011842): Need to consider content-size here.
  if (apply_size_containment) {
    if (input.size_type == NGMinMaxSizeType::kContentBoxSize)
      return sizes;
  }

  // Size containment does not consider the legend for sizing.
  if (!apply_size_containment) {
    if (NGBlockNode legend = Node().GetRenderedLegend()) {
      sizes = ComputeMinAndMaxContentContribution(Style(), legend, input);
      sizes += ComputeMinMaxMargins(Style(), legend).InlineSum();
    }
  }

  // The fieldset content includes the fieldset padding (and any scrollbars),
  // while the legend is a regular child and doesn't. We may have a fieldset
  // without any content or legend, so add the padding here, on the outside.
  sizes += ComputePadding(ConstraintSpace(), node_.Style()).InlineSum();

  // Size containment does not consider the content for sizing.
  if (!apply_size_containment) {
    if (NGBlockNode content = Node().GetFieldsetContent()) {
      MinMaxSize content_minmax =
          ComputeMinAndMaxContentContribution(Style(), content, input);
      content_minmax += ComputeMinMaxMargins(Style(), content).InlineSum();
      sizes.Encompass(content_minmax);
    }
  }

  sizes += ComputeBorders(ConstraintSpace(), node_).InlineSum();
  return sizes;
}

const NGConstraintSpace
NGFieldsetLayoutAlgorithm::CreateConstraintSpaceForLegend(
    NGBlockNode legend,
    LogicalSize available_size) {
  NGConstraintSpaceBuilder builder(
      ConstraintSpace(), legend.Style().GetWritingMode(), /* is_new_fc */ true);
  SetOrthogonalFallbackInlineSizeIfNeeded(Style(), legend, &builder);

  builder.SetAvailableSize(available_size);
  LogicalSize percentage_size =
      CalculateChildPercentageSize(ConstraintSpace(), Node(), available_size);
  builder.SetPercentageResolutionSize(percentage_size);
  builder.SetIsShrinkToFit(legend.Style().LogicalWidth().IsAuto());
  builder.SetTextDirection(legend.Style().Direction());
  return builder.ToConstraintSpace();
}

const NGConstraintSpace
NGFieldsetLayoutAlgorithm::CreateConstraintSpaceForFieldsetContent(
    LogicalSize padding_box_size) {
  NGConstraintSpaceBuilder builder(ConstraintSpace(),
                                   ConstraintSpace().GetWritingMode(),
                                   /* is_new_fc */ true);
  builder.SetAvailableSize(padding_box_size);
  builder.SetPercentageResolutionSize(
      ConstraintSpace().PercentageResolutionSize());
  builder.SetIsFixedBlockSize(padding_box_size.block_size != kIndefiniteSize);
  return builder.ToConstraintSpace();
}

}  // namespace blink
