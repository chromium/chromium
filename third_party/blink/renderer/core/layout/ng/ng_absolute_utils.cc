// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_absolute_utils.h"

#include <algorithm>
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_static_position.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

namespace {

// Dominant side:
// htb ltr => top left
// htb rtl => top right
// vlr ltr => top left
// vlr rtl => bottom left
// vrl ltr => top right
// vrl rtl => bottom right
bool IsLeftDominant(const WritingDirectionMode writing_direction) {
  return (writing_direction.GetWritingMode() != WritingMode::kVerticalRl) &&
         !(writing_direction.IsHorizontal() && writing_direction.IsRtl());
}

bool IsTopDominant(const WritingDirectionMode writing_direction) {
  return writing_direction.IsHorizontal() || writing_direction.IsLtr();
}

// A direction agnostic version of |NGLogicalStaticPosition::InlineEdge|, and
// |NGLogicalStaticPosition::BlockEdge|.
enum StaticPositionEdge { kStart, kCenter, kEnd };

inline StaticPositionEdge GetStaticPositionEdge(
    NGLogicalStaticPosition::InlineEdge inline_edge) {
  switch (inline_edge) {
    case NGLogicalStaticPosition::InlineEdge::kInlineStart:
      return kStart;
    case NGLogicalStaticPosition::InlineEdge::kInlineCenter:
      return kCenter;
    case NGLogicalStaticPosition::InlineEdge::kInlineEnd:
      return kEnd;
  }
}

inline StaticPositionEdge GetStaticPositionEdge(
    NGLogicalStaticPosition::BlockEdge block_edge) {
  switch (block_edge) {
    case NGLogicalStaticPosition::BlockEdge::kBlockStart:
      return kStart;
    case NGLogicalStaticPosition::BlockEdge::kBlockCenter:
      return kCenter;
    case NGLogicalStaticPosition::BlockEdge::kBlockEnd:
      return kEnd;
  }
}

inline LayoutUnit StaticPositionStartInset(StaticPositionEdge edge,
                                           LayoutUnit static_position_offset,
                                           LayoutUnit size) {
  switch (edge) {
    case kStart:
      return static_position_offset;
    case kCenter:
      return static_position_offset - (size / 2);
    case kEnd:
      return static_position_offset - size;
  }
}

inline LayoutUnit StaticPositionEndInset(StaticPositionEdge edge,
                                         LayoutUnit static_position_offset,
                                         LayoutUnit available_size,
                                         LayoutUnit size) {
  switch (edge) {
    case kStart:
      return available_size - static_position_offset - size;
    case kCenter:
      return available_size - static_position_offset - (size / 2);
    case kEnd:
      return available_size - static_position_offset;
  }
}

// Computes the available-space as an (offset, size) pair, accounting for insets
// and the static-position.
std::pair<LayoutUnit, LayoutUnit> ComputeAvailableSpaceInOneAxis(
    const LayoutUnit available_size,
    const absl::optional<LayoutUnit>& inset_start,
    const absl::optional<LayoutUnit>& inset_end,
    const LayoutUnit static_position_offset,
    StaticPositionEdge static_position_edge) {
  DCHECK_NE(available_size, kIndefiniteSize);
  LayoutUnit computed_offset;
  LayoutUnit computed_available_size;

  if (!inset_start && !inset_end) {
    // If both our insets are auto, the available-space is defined by the
    // static-position.
    switch (static_position_edge) {
      case kStart:
        // The available-space for the start static-position "grows" towards the
        // end edge.
        // |      *----------->|
        computed_offset = static_position_offset;
        computed_available_size = available_size - static_position_offset;
        break;
      case kCenter: {
        // The available-space for the center static-position "grows" towards
        // both edges (equally), and stops when it hits the first one.
        // |<-----*----->      |
        LayoutUnit half_computed_available_size = std::min(
            static_position_offset, available_size - static_position_offset);
        computed_offset = static_position_offset - half_computed_available_size;
        computed_available_size = 2 * half_computed_available_size;
        break;
      }
      case kEnd:
        // The available-space for the end static-position "grows" towards the
        // start edge.
        // |<-----*            |
        computed_offset = LayoutUnit();
        computed_available_size = static_position_offset;
        break;
    }
  } else {
    // Otherwise we just subtract the insets.
    computed_available_size = available_size -
                              inset_start.value_or(LayoutUnit()) -
                              inset_end.value_or(LayoutUnit());
    computed_offset = inset_start.value_or(LayoutUnit());
  }

  return std::make_pair(computed_offset, computed_available_size);
}

// Computes the insets, and margins if necessary.
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-width
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-height
void ComputeInsets(const LayoutUnit margin_percentage_resolution_size,
                   const LayoutUnit available_size,
                   const LayoutUnit computed_available_size,
                   const Length& margin_start_length,
                   const Length& margin_end_length,
                   absl::optional<LayoutUnit> inset_start,
                   absl::optional<LayoutUnit> inset_end,
                   const LayoutUnit static_position_offset,
                   StaticPositionEdge static_position_edge,
                   bool is_start_dominant,
                   bool is_block_direction,
                   LayoutUnit size,
                   LayoutUnit* inset_start_out,
                   LayoutUnit* inset_end_out,
                   LayoutUnit* margin_start_out,
                   LayoutUnit* margin_end_out) {
  DCHECK_NE(available_size, kIndefiniteSize);

  absl::optional<LayoutUnit> margin_start;
  if (!margin_start_length.IsAuto()) {
    margin_start = MinimumValueForLength(margin_start_length,
                                         margin_percentage_resolution_size);
  }
  absl::optional<LayoutUnit> margin_end;
  if (!margin_end_length.IsAuto()) {
    margin_end = MinimumValueForLength(margin_end_length,
                                       margin_percentage_resolution_size);
  }

  // Solving the equation:
  // |inset_start| + |margin_start| + |size| + |margin_end| + |inset_end| =
  // |available_size|
  if (inset_start && inset_end) {
    // "If left, right, and width are not auto:"
    // Compute margins.
    LayoutUnit margin_space = computed_available_size - size;

    if (!margin_start && !margin_end) {
      // When both margins are auto.
      if (margin_space > 0 || is_block_direction) {
        margin_start = margin_space / 2;
        margin_end = margin_space - *margin_start;
      } else {
        // Margins are negative.
        if (is_start_dominant) {
          margin_start = LayoutUnit();
          margin_end = margin_space;
        } else {
          margin_start = margin_space;
          margin_end = LayoutUnit();
        }
      }
    } else if (!margin_start) {
      margin_start = margin_space - *margin_end;
    } else if (!margin_end) {
      margin_end = margin_space - *margin_start;
    } else {
      // Are the values over-constrained?
      LayoutUnit margin_extra = margin_space - *margin_start - *margin_end;
      if (margin_extra) {
        // Relax the end.
        if (is_start_dominant)
          inset_end = *inset_end + margin_extra;
        else
          inset_start = *inset_start + margin_extra;
      }
    }
  }

  // Set any unknown margins.
  if (!margin_start)
    margin_start = LayoutUnit();
  if (!margin_end)
    margin_end = LayoutUnit();

  if (!inset_start && !inset_end) {
    LayoutUnit margin_size = size + *margin_start + *margin_end;
    if (is_start_dominant) {
      inset_start = StaticPositionStartInset(
          static_position_edge, static_position_offset, margin_size);
    } else {
      inset_end =
          StaticPositionEndInset(static_position_edge, static_position_offset,
                                 available_size, margin_size);
    }
  }

  if (!inset_start) {
    inset_start =
        available_size - size - *inset_end - *margin_start - *margin_end;
  } else if (!inset_end) {
    inset_end =
        available_size - size - *inset_start - *margin_start - *margin_end;
  }

  *inset_start_out = *inset_start + *margin_start;
  *inset_end_out = *inset_end + *margin_end;
  *margin_start_out = *margin_start;
  *margin_end_out = *margin_end;
}

bool CanComputeBlockSizeWithoutLayout(const NGBlockNode& node) {
  if (node.IsTable())
    return false;
  if (node.IsReplaced())
    return true;
  const auto& style = node.Style();
  return !style.LogicalHeight().IsContentOrIntrinsic() &&
         !style.LogicalMinHeight().IsContentOrIntrinsic() &&
         !style.LogicalMaxHeight().IsContentOrIntrinsic() &&
         (!style.LogicalHeight().IsAuto() ||
          (!style.LogicalTop().IsAuto() && !style.LogicalBottom().IsAuto()));
}

}  // namespace

NGLogicalOutOfFlowInsets ComputeOutOfFlowInsets(
    const ComputedStyle& style,
    const LogicalSize& available_logical_size,
    NGAnchorEvaluatorImpl* anchor_evaluator) {
  // Compute in physical, because anchors may be in different `writing-mode` or
  // `direction`.
  const WritingDirectionMode writing_direction = style.GetWritingDirection();
  const PhysicalSize available_size = ToPhysicalSize(
      available_logical_size, writing_direction.GetWritingMode());
  absl::optional<LayoutUnit> left;
  if (const Length& left_length = style.Left(); !left_length.IsAuto()) {
    anchor_evaluator->SetAxis(/* is_y_axis */ false,
                              /* is_right_or_bottom */ false,
                              available_size.width);
    left = MinimumValueForLength(left_length, available_size.width,
                                 anchor_evaluator);
  }
  absl::optional<LayoutUnit> right;
  if (const Length& right_length = style.Right(); !right_length.IsAuto()) {
    anchor_evaluator->SetAxis(/* is_y_axis */ false,
                              /* is_right_or_bottom */ true,
                              available_size.width);
    right = MinimumValueForLength(right_length, available_size.width,
                                  anchor_evaluator);
  }

  absl::optional<LayoutUnit> top;
  if (const Length& top_length = style.Top(); !top_length.IsAuto()) {
    anchor_evaluator->SetAxis(/* is_y_axis */ true,
                              /* is_right_or_bottom */ false,
                              available_size.height);
    top = MinimumValueForLength(top_length, available_size.height,
                                anchor_evaluator);
  }
  absl::optional<LayoutUnit> bottom;
  if (const Length& bottom_length = style.Bottom(); !bottom_length.IsAuto()) {
    anchor_evaluator->SetAxis(/* is_y_axis */ true,
                              /* is_right_or_bottom */ true,
                              available_size.height);
    bottom = MinimumValueForLength(bottom_length, available_size.height,
                                   anchor_evaluator);
  }

  // Convert the physical insets to logical.
  PhysicalToLogical<absl::optional<LayoutUnit>&> insets(writing_direction, top,
                                                        right, bottom, left);
  return {insets.InlineStart(), insets.InlineEnd(), insets.BlockStart(),
          insets.BlockEnd()};
}

LogicalRect ComputeOutOfFlowAvailableRect(
    const NGBlockNode& node,
    const NGConstraintSpace& space,
    const NGLogicalOutOfFlowInsets& insets,
    const NGLogicalStaticPosition& static_position) {
  return ComputeOutOfFlowAvailableRect(node, space.AvailableSize(), insets,
                                       static_position);
}

LogicalRect ComputeOutOfFlowAvailableRect(
    const NGBlockNode& node,
    const LogicalSize& available_size,
    const NGLogicalOutOfFlowInsets& insets,
    const NGLogicalStaticPosition& static_position) {
  LayoutUnit inline_offset, inline_size;
  std::tie(inline_offset, inline_size) = ComputeAvailableSpaceInOneAxis(
      available_size.inline_size, insets.inline_start, insets.inline_end,
      static_position.offset.inline_offset,
      GetStaticPositionEdge(static_position.inline_edge));
  LayoutUnit block_offset, block_size;
  std::tie(block_offset, block_size) = ComputeAvailableSpaceInOneAxis(
      available_size.block_size, insets.block_start, insets.block_end,
      static_position.offset.block_offset,
      GetStaticPositionEdge(static_position.block_edge));
  return LogicalRect(inline_offset, block_offset, inline_size, block_size);
}

bool ComputeOutOfFlowInlineDimensions(
    const NGBlockNode& node,
    const ComputedStyle& style,
    const NGConstraintSpace& space,
    const NGLogicalOutOfFlowInsets& insets,
    const NGBoxStrut& border_padding,
    const NGLogicalStaticPosition& static_position,
    LogicalSize computed_available_size,
    const absl::optional<LogicalSize>& replaced_size,
    const WritingDirectionMode container_writing_direction,
    const Length::AnchorEvaluator* anchor_evaluator,
    NGLogicalOutOfFlowDimensions* dimensions) {
  DCHECK(dimensions);
  bool depends_on_min_max_sizes = false;

  const bool is_table = node.IsTable();
  const bool can_compute_block_size_without_layout =
      CanComputeBlockSizeWithoutLayout(node);

  if (is_table) {
    computed_available_size.inline_size = std::min(
        computed_available_size.inline_size, space.AvailableSize().inline_size);
    DCHECK_GE(computed_available_size.inline_size, LayoutUnit());
  }

  auto MinMaxSizesFunc = [&](MinMaxSizesType type) -> MinMaxSizesResult {
    DCHECK(!node.IsReplaced());

    // Mark the inline calculations as being dependent on min/max sizes.
    depends_on_min_max_sizes = true;

    // If we can't compute our block-size without layout, we can use the
    // provided space to determine our min/max sizes.
    if (!can_compute_block_size_without_layout)
      return node.ComputeMinMaxSizes(style.GetWritingMode(), type, space);

    // Compute our block-size if we haven't already.
    if (dimensions->size.block_size == kIndefiniteSize) {
      ComputeOutOfFlowBlockDimensions(
          node, style, space, insets, border_padding, static_position,
          computed_available_size,
          /* replaced_size */ absl::nullopt, container_writing_direction,
          anchor_evaluator, dimensions);
    }

    // Create a new space, setting the fixed block-size.
    NGConstraintSpaceBuilder builder(style.GetWritingMode(),
                                     style.GetWritingDirection(),
                                     /* is_new_fc */ true);
    builder.SetAvailableSize(
        {space.AvailableSize().inline_size, dimensions->size.block_size});
    builder.SetIsFixedBlockSize(true);
    builder.SetPercentageResolutionSize(space.PercentageResolutionSize());
    return node.ComputeMinMaxSizes(style.GetWritingMode(), type,
                                   builder.ToConstraintSpace());
  };

  LayoutUnit inline_size;
  if (replaced_size) {
    DCHECK(node.IsReplaced());
    inline_size = replaced_size->inline_size;
  } else {
    Length main_inline_length = style.LogicalWidth();
    Length min_inline_length = style.LogicalMinWidth();

    const bool stretch_inline_size =
        !style.LogicalLeft().IsAuto() && !style.LogicalRight().IsAuto();

    // Determine how "auto" should resolve.
    if (main_inline_length.IsAuto()) {
      if (is_table) {
        // Tables always shrink-to-fit.
        main_inline_length = Length::FitContent();
      } else if (!style.AspectRatio().IsAuto() &&
                 can_compute_block_size_without_layout &&
                 (!stretch_inline_size || !style.LogicalHeight().IsAuto())) {
        // We'd like to apply the aspect-ratio.
        // The aspect-ratio applies from the block-axis if we can compute our
        // block-size without invoking layout, and either:
        //  - We aren't stretching our auto inline-size.
        //  - We are stretching our auto inline-size, but the block-size isn't
        //  auto.
        main_inline_length = Length::FitContent();

        // Apply the automatic minimum size.
        if (style.OverflowInlineDirection() == EOverflow::kVisible &&
            min_inline_length.IsAuto())
          min_inline_length = Length::MinIntrinsic();
      } else {
        main_inline_length = stretch_inline_size ? Length::FillAvailable()
                                                 : Length::FitContent();
      }
    }

    LayoutUnit main_inline_size = ResolveMainInlineLength(
        space, style, border_padding, MinMaxSizesFunc, main_inline_length,
        computed_available_size.inline_size, anchor_evaluator);
    MinMaxSizes min_max_inline_sizes = ComputeMinMaxInlineSizes(
        space, node, border_padding, MinMaxSizesFunc, &min_inline_length,
        computed_available_size.inline_size, anchor_evaluator);

    inline_size = min_max_inline_sizes.ClampSizeToMinAndMax(main_inline_size);
  }

  dimensions->size.inline_size = inline_size;

  const auto writing_direction = style.GetWritingDirection();
  bool is_start_dominant;
  if (writing_direction.IsHorizontal()) {
    is_start_dominant = IsLeftDominant(container_writing_direction) ==
                        IsLeftDominant(writing_direction);
  } else {
    is_start_dominant = IsTopDominant(container_writing_direction) ==
                        IsTopDominant(writing_direction);
  }

  ComputeInsets(
      space.PercentageResolutionInlineSizeForParentWritingMode(),
      space.AvailableSize().inline_size, computed_available_size.inline_size,
      style.MarginStart(), style.MarginEnd(), insets.inline_start,
      insets.inline_end, static_position.offset.inline_offset,
      GetStaticPositionEdge(static_position.inline_edge), is_start_dominant,
      false /* is_block_direction */, inline_size,
      &dimensions->inset.inline_start, &dimensions->inset.inline_end,
      &dimensions->margins.inline_start, &dimensions->margins.inline_end);

  return depends_on_min_max_sizes;
}

const NGLayoutResult* ComputeOutOfFlowBlockDimensions(
    const NGBlockNode& node,
    const ComputedStyle& style,
    const NGConstraintSpace& space,
    const NGLogicalOutOfFlowInsets& insets,
    const NGBoxStrut& border_padding,
    const NGLogicalStaticPosition& static_position,
    LogicalSize computed_available_size,
    const absl::optional<LogicalSize>& replaced_size,
    const WritingDirectionMode container_writing_direction,
    const Length::AnchorEvaluator* anchor_evaluator,
    NGLogicalOutOfFlowDimensions* dimensions) {
  DCHECK(dimensions);

  const NGLayoutResult* result = nullptr;

  const bool is_table = node.IsTable();
  if (is_table) {
    computed_available_size.block_size = std::min(
        computed_available_size.block_size, space.AvailableSize().block_size);
    DCHECK_GE(computed_available_size.block_size, LayoutUnit());
  }

  MinMaxSizes min_max_block_sizes = ComputeMinMaxBlockSizes(
      space, style, border_padding, computed_available_size.block_size,
      anchor_evaluator);

  auto IntrinsicBlockSizeFunc = [&]() -> LayoutUnit {
    DCHECK(!node.IsReplaced());
    DCHECK_NE(dimensions->size.inline_size, kIndefiniteSize);

    if (!result) {
      // Create a new space, setting the fixed block-size.
      NGConstraintSpaceBuilder builder(style.GetWritingMode(),
                                       style.GetWritingDirection(),
                                       /* is_new_fc */ true);
      builder.SetAvailableSize(
          {dimensions->size.inline_size, space.AvailableSize().block_size});
      builder.SetIsFixedInlineSize(true);
      builder.SetPercentageResolutionSize(space.PercentageResolutionSize());
      // Use the computed |MinMaxSizes| because |node.Layout()| can't resolve
      // the `anchor-size()` function.
      builder.SetOverrideMinMaxBlockSizes(min_max_block_sizes);

      if (space.IsInitialColumnBalancingPass()) {
        // The |fragmentainer_offset_delta| will not make a difference in the
        // initial column balancing pass.
        SetupSpaceBuilderForFragmentation(
            space, node, /* fragmentainer_offset_delta */ LayoutUnit(),
            &builder, /* is_new_fc */ true,
            /* requires_content_before_breaking */ false);
      }
      result = node.Layout(builder.ToConstraintSpace());
    }

    return NGFragment(style.GetWritingDirection(), result->PhysicalFragment())
        .BlockSize();
  };

  LayoutUnit block_size;
  if (replaced_size) {
    DCHECK(node.IsReplaced());
    block_size = replaced_size->block_size;
  } else {
    Length main_block_length = style.LogicalHeight();

    const bool stretch_block_size =
        !style.LogicalTop().IsAuto() && !style.LogicalBottom().IsAuto();

    // Determine how "auto" should resolve.
    if (main_block_length.IsAuto()) {
      if (is_table) {
        // Tables always shrink-to-fit.
        main_block_length = Length::FitContent();
      } else if (!style.AspectRatio().IsAuto() &&
                 dimensions->size.inline_size != kIndefiniteSize) {
        main_block_length = Length::FitContent();
      } else {
        main_block_length =
            stretch_block_size ? Length::FillAvailable() : Length::FitContent();
      }
    }

    LayoutUnit main_block_size = ResolveMainBlockLength(
        space, style, border_padding, main_block_length, IntrinsicBlockSizeFunc,
        computed_available_size.block_size, anchor_evaluator);

    // Manually resolve any intrinsic/content min/max block-sizes.
    // TODO(crbug.com/1135207): |ComputeMinMaxBlockSizes()| should handle this.
    if (style.LogicalMinHeight().IsContentOrIntrinsic())
      min_max_block_sizes.min_size = IntrinsicBlockSizeFunc();
    if (style.LogicalMaxHeight().IsContentOrIntrinsic())
      min_max_block_sizes.max_size = IntrinsicBlockSizeFunc();
    min_max_block_sizes.max_size =
        std::max(min_max_block_sizes.max_size, min_max_block_sizes.min_size);

    // Tables are never allowed to go below their "auto" block-size.
    if (is_table)
      min_max_block_sizes.Encompass(IntrinsicBlockSizeFunc());

    block_size = min_max_block_sizes.ClampSizeToMinAndMax(main_block_size);
  }

  dimensions->size.block_size = block_size;

  const auto writing_direction = style.GetWritingDirection();
  bool is_start_dominant;
  if (writing_direction.IsHorizontal()) {
    is_start_dominant = IsTopDominant(container_writing_direction) ==
                        IsTopDominant(writing_direction);
  } else {
    is_start_dominant = IsLeftDominant(container_writing_direction) ==
                        IsLeftDominant(writing_direction);
  }

  ComputeInsets(
      space.PercentageResolutionInlineSizeForParentWritingMode(),
      space.AvailableSize().block_size, computed_available_size.block_size,
      style.MarginBefore(), style.MarginAfter(), insets.block_start,
      insets.block_end, static_position.offset.block_offset,
      GetStaticPositionEdge(static_position.block_edge), is_start_dominant,
      true /* is_block_direction */, block_size, &dimensions->inset.block_start,
      &dimensions->inset.block_end, &dimensions->margins.block_start,
      &dimensions->margins.block_end);

  return result;
}

void AdjustOffsetForSplitInline(const NGBlockNode& node,
                                const NGBoxFragmentBuilder* container_builder,
                                LogicalOffset& offset) {
  // Special case: oof css container is a split inline.
  // When css container spans multiple anonymous blocks, its dimensions can
  // only be computed by a block that is an ancestor of all fragments
  // generated by css container. That block is parent of anonymous
  // containing block. That is why instead of OOF being placed by its
  // anonymous container, they get placed by anonymous container's parent.
  // This is different from all other OOF blocks, and requires special
  // handling in several places in the OOF code.
  // There is an exception to special case: if anonymous block is Legacy, we
  // cannot do the fancy multiple anonymous block traversal, and we handle
  // it like regular blocks.
  //
  // Detailed example:
  //
  // If Layout tree looks like this:
  // LayoutNGBlockFlow#container
  //   LayoutNGBlockFlow (anonymous#1)
  //     LayoutInline#1 (relative)
  //   LayoutNGBlockFlow (anonymous#2 relative)
  //     LayoutNGBlockFlow#oof (positioned)
  //   LayoutNGBlockFlow (anonymous#3)
  //     LayoutInline#3 (continuation)
  //
  // The containing block geometry is defined by split inlines,
  // LayoutInline#1, LayoutInline#3.
  // Css container anonymous#2 does not have information needed
  // to compute containing block geometry.
  // Therefore, #oof cannot be placed by anonymous#2. NG handles this case
  // by placing #oof in parent of anonymous (#container).
  //
  // But, PaintPropertyTreeBuilder expects #oof.Location() to be wrt css
  // container, #anonymous2. This is why the code below adjusts the legacy
  // offset from being wrt #container to being wrt #anonymous2.
  const LayoutObject* container = node.GetLayoutBox()->Container();
  if (container->IsAnonymousBlock()) {
    LogicalOffset container_offset =
        container_builder->GetChildOffset(container);
    offset -= container_offset;
  } else if (container->IsLayoutInline() &&
             container->ContainingBlock()->IsAnonymousBlock()) {
    // Location of OOF with inline container, and anonymous containing block
    // is wrt container.
    LogicalOffset container_offset =
        container_builder->GetChildOffset(container->ContainingBlock());
    offset -= container_offset;
  }
}

}  // namespace blink
