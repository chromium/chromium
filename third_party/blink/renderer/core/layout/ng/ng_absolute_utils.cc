// Copyright 2016 The Chromium Authors. All rights reserved.
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

// Implement the absolute size resolution algorithm.
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-width
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-height
template <typename MinMaxSizesFunc>
void ComputeAbsoluteSize(const LayoutUnit border_padding,
                         const MinMaxSizesFunc& min_max_sizes_func,
                         const LayoutUnit margin_percentage_resolution_size,
                         const LayoutUnit available_size,
                         const Length& margin_start_length,
                         const Length& margin_end_length,
                         const Length& inset_start_length,
                         const Length& inset_end_length,
                         const MinMaxSizes& min_max_length_sizes,
                         const LayoutUnit static_position_offset,
                         StaticPositionEdge static_position_edge,
                         bool is_start_dominant,
                         bool is_block_direction,
                         bool is_table,
                         bool is_shrink_to_fit,
                         absl::optional<LayoutUnit> size,
                         LayoutUnit* size_out,
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
  absl::optional<LayoutUnit> inset_start;
  if (!inset_start_length.IsAuto()) {
    inset_start = MinimumValueForLength(inset_start_length, available_size);
  }
  absl::optional<LayoutUnit> inset_end;
  if (!inset_end_length.IsAuto()) {
    inset_end = MinimumValueForLength(inset_end_length, available_size);
  }

  auto ClampToMinMaxLengthSizes = [&](LayoutUnit size) -> LayoutUnit {
    return std::max(border_padding,
                    min_max_length_sizes.ClampSizeToMinAndMax(size));
  };

  auto ComputeShrinkToFitSize = [&](LayoutUnit computed_available_size,
                                    LayoutUnit margin) {
    // The available-size given to tables isn't allowed to exceed the
    // available-size of the containing-block.
    if (is_table) {
      computed_available_size =
          std::min(computed_available_size, available_size);
    }
    return ClampToMinMaxLengthSizes(
        min_max_sizes_func(MinMaxSizesType::kContent)
            .sizes.ShrinkToFit(computed_available_size - margin));
  };

  if (size)
    size = ClampToMinMaxLengthSizes(*size);

  // Solving the equation:
  // |inset_start| + |margin_start| + |size| + |margin_end| + |inset_end| =
  // |available_size|
  if (!inset_start && !inset_end && !size) {
    // "If all three of left, width, and right are auto:"
    if (!margin_start)
      margin_start = LayoutUnit();
    if (!margin_end)
      margin_end = LayoutUnit();

    LayoutUnit computed_available_size;
    switch (static_position_edge) {
      case kStart:
        // The available-size for the start static-position "grows" towards the
        // end edge.
        // |      *----------->|
        computed_available_size = available_size - static_position_offset;
        break;
      case kCenter:
        // The available-size for the center static-position "grows" towards
        // both edges (equally), and stops when it hits the first one.
        // |<-----*----->      |
        computed_available_size =
            2 * std::min(static_position_offset,
                         available_size - static_position_offset);
        break;
      case kEnd:
        // The available-size for the end static-position "grows" towards the
        // start edge.
        // |<-----*            |
        computed_available_size = static_position_offset;
        break;
    }
    size = ComputeShrinkToFitSize(computed_available_size,
                                  *margin_start + *margin_end);
    LayoutUnit margin_size = *size + *margin_start + *margin_end;
    if (is_start_dominant) {
      inset_start = StaticPositionStartInset(
          static_position_edge, static_position_offset, margin_size);
    } else {
      inset_end =
          StaticPositionEndInset(static_position_edge, static_position_offset,
                                 available_size, margin_size);
    }
  } else if (inset_start && inset_end) {
    LayoutUnit computed_available_size =
        available_size - *inset_start - *inset_end;

    if (!size) {
      const LayoutUnit margin = margin_start.value_or(LayoutUnit()) +
                                margin_end.value_or(LayoutUnit());
      size = is_shrink_to_fit
                 ? ComputeShrinkToFitSize(computed_available_size, margin)
                 : ClampToMinMaxLengthSizes(computed_available_size - margin);
    }

    // "If left, right, and width are not auto:"
    // Compute margins.
    LayoutUnit margin_space = computed_available_size - *size;

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

  // Rules 1 through 3: 2 out of 3 are unknown.
  if (!inset_start && !size) {
    // Rule 1.
    DCHECK(inset_end.has_value());
    LayoutUnit computed_available_size = available_size - *inset_end;
    size = ComputeShrinkToFitSize(computed_available_size,
                                  *margin_start + *margin_end);
  } else if (!inset_start && !inset_end) {
    // Rule 2.
    DCHECK(size.has_value());
    LayoutUnit margin_size = *size + *margin_start + *margin_end;
    if (is_start_dominant) {
      inset_start = StaticPositionStartInset(
          static_position_edge, static_position_offset, margin_size);
    } else {
      inset_end =
          StaticPositionEndInset(static_position_edge, static_position_offset,
                                 available_size, margin_size);
    }
  } else if (!size && !inset_end) {
    // Rule 3.
    DCHECK(inset_start.has_value());
    LayoutUnit computed_available_size = available_size - *inset_start;
    size = ComputeShrinkToFitSize(computed_available_size,
                                  *margin_start + *margin_end);
  }

  // Rules 4 through 6: 1 out of 3 are unknown.
  if (!inset_start) {
    inset_start =
        available_size - *size - *inset_end - *margin_start - *margin_end;
  } else if (!inset_end) {
    inset_end =
        available_size - *size - *inset_start - *margin_start - *margin_end;
  } else if (!size) {
    NOTREACHED();
  }

  DCHECK_GE(*size, border_padding);
  *size_out = *size;
  *inset_start_out = *inset_start + *margin_start;
  *inset_end_out = *inset_end + *margin_end;
  *margin_start_out = *margin_start;
  *margin_end_out = *margin_end;
}

}  // namespace

// NOTE: Out-of-flow positioned tables require special handling:
//  - The specified inline-size/block-size is always considered as 'auto', and
//    instead treated as an additional "min" constraint.
//  - They can't be "stretched" by inset constraints, ("left: 0; right: 0;"),
//    instead they always perform shrink-to-fit sizing within this
//    available-size, (and this is why we always compute the min/max content
//    sizes for them).
//  - When performing shrink-to-fit sizing, the given available size can never
//    exceed the available-size of the containing-block (e.g.  with insets
//    similar to: "left: -100px; right: -100px").

namespace {

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

bool ComputeOutOfFlowInlineDimensions(
    const NGBlockNode& node,
    const NGConstraintSpace& space,
    const NGBoxStrut& border_padding,
    const NGLogicalStaticPosition& static_position,
    const absl::optional<LogicalSize>& replaced_size,
    const WritingDirectionMode container_writing_direction,
    NGLogicalOutOfFlowDimensions* dimensions) {
  DCHECK(dimensions);
  bool depends_on_min_max_sizes = false;

  const auto& style = node.Style();
  const bool is_table = node.IsTable();
  const bool can_compute_block_size_without_layout =
      CanComputeBlockSizeWithoutLayout(node);
  bool is_shrink_to_fit = is_table;

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
      ComputeOutOfFlowBlockDimensions(node, space, border_padding,
                                      static_position,
                                      /* replaced_size */ absl::nullopt,
                                      container_writing_direction, dimensions);
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

  Length min_inline_length = style.LogicalMinWidth();
  absl::optional<LayoutUnit> inline_size;
  if (replaced_size) {
    DCHECK(node.IsReplaced());
    inline_size = replaced_size->inline_size;
  } else if (!style.LogicalWidth().IsAuto()) {
    inline_size = ResolveMainInlineLength(
        space, style, border_padding, MinMaxSizesFunc, style.LogicalWidth());
  } else if (!style.AspectRatio().IsAuto()) {
    const bool stretch_inline_size = !node.IsTable() &&
                                     !style.LogicalLeft().IsAuto() &&
                                     !style.LogicalRight().IsAuto();

    // The aspect-ratio applies from the block-axis if:
    //  - Our auto inline-size would have stretched but we have an explicit
    //    block-size.
    //  - Our auto inline-size doesn't stretch but we can compute our
    //    block-size without layout.
    if ((stretch_inline_size &&
         !style.LogicalHeight().IsAutoOrContentOrIntrinsic()) ||
        (!stretch_inline_size && can_compute_block_size_without_layout)) {
      is_shrink_to_fit = true;

      // Apply the automatic minimum size.
      if (style.OverflowInlineDirection() == EOverflow::kVisible &&
          min_inline_length.IsAuto())
        min_inline_length = Length::MinIntrinsic();
    }
  }

  MinMaxSizes min_max_length_sizes;
  if (replaced_size) {
    // Replaced elements have their final size computed upfront, not by
    // |ComputeAbsoluteSize| which only does the positioning. As such we set
    // the length sizes to their respective "initial" values to avoid
    // re-computing them.
    min_max_length_sizes = {LayoutUnit(), LayoutUnit::Max()};
  } else {
    min_max_length_sizes = ComputeMinMaxInlineSizes(
        space, node, border_padding, MinMaxSizesFunc, &min_inline_length);
  }

  const auto writing_direction = style.GetWritingDirection();
  bool is_start_dominant;
  if (writing_direction.IsHorizontal()) {
    is_start_dominant = IsLeftDominant(container_writing_direction) ==
                        IsLeftDominant(writing_direction);
  } else {
    is_start_dominant = IsTopDominant(container_writing_direction) ==
                        IsTopDominant(writing_direction);
  }

  ComputeAbsoluteSize(
      border_padding.InlineSum(), MinMaxSizesFunc,
      space.PercentageResolutionInlineSizeForParentWritingMode(),
      space.AvailableSize().inline_size, style.MarginStart(), style.MarginEnd(),
      style.LogicalInlineStart(), style.LogicalInlineEnd(),
      min_max_length_sizes, static_position.offset.inline_offset,
      GetStaticPositionEdge(static_position.inline_edge), is_start_dominant,
      false /* is_block_direction */, is_table, is_shrink_to_fit, inline_size,
      &dimensions->size.inline_size, &dimensions->inset.inline_start,
      &dimensions->inset.inline_end, &dimensions->margins.inline_start,
      &dimensions->margins.inline_end);

  return depends_on_min_max_sizes;
}

const NGLayoutResult* ComputeOutOfFlowBlockDimensions(
    const NGBlockNode& node,
    const NGConstraintSpace& space,
    const NGBoxStrut& border_padding,
    const NGLogicalStaticPosition& static_position,
    const absl::optional<LogicalSize>& replaced_size,
    const WritingDirectionMode container_writing_direction,
    NGLogicalOutOfFlowDimensions* dimensions) {
  DCHECK(dimensions);

  const NGLayoutResult* result = nullptr;

  const auto& style = node.Style();
  const bool is_table = node.IsTable();
  bool is_shrink_to_fit = is_table;

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

  // There isn't two separate "min/max" values, instead we represent this
  // concept as a min/max size whose values are the same.
  auto MinMaxSizesFunc = [&](MinMaxSizesType) -> MinMaxSizesResult {
    DCHECK(!node.IsReplaced());

    MinMaxSizes sizes;
    sizes = IntrinsicBlockSizeFunc();

    // |depends_on_block_constraints| doesn't matter in this context.
    return MinMaxSizesResult(sizes, /* depends_on_block_constraints */ false);
  };

  absl::optional<LayoutUnit> block_size;
  if (replaced_size) {
    DCHECK(node.IsReplaced());
    block_size = replaced_size->block_size;
  } else if (!style.LogicalHeight().IsAuto()) {
    block_size =
        ResolveMainBlockLength(space, style, border_padding,
                               style.LogicalHeight(), IntrinsicBlockSizeFunc);
  } else if (!style.AspectRatio().IsAuto() &&
             dimensions->size.inline_size != kIndefiniteSize) {
    // If an aspect-ratio applied, size the child to the intrinsic size.
    is_shrink_to_fit = true;
  }

  MinMaxSizes min_max_length_sizes;
  if (replaced_size) {
    // See comment in |ComputeOutOfFlowInlineDimensions|.
    min_max_length_sizes = {LayoutUnit(), LayoutUnit::Max()};
  } else {
    min_max_length_sizes =
        ComputeMinMaxBlockSizes(space, style, border_padding);

    // Manually resolve any intrinsic/content min/max block-sizes.
    // TODO(crbug.com/1135207): |ComputeMinMaxBlockSizes()| should handle this.
    if (style.LogicalMinHeight().IsContentOrIntrinsic())
      min_max_length_sizes.min_size = IntrinsicBlockSizeFunc();
    if (style.LogicalMaxHeight().IsContentOrIntrinsic())
      min_max_length_sizes.max_size = IntrinsicBlockSizeFunc();
    min_max_length_sizes.max_size =
        std::max(min_max_length_sizes.max_size, min_max_length_sizes.min_size);

    // Tables are never allowed to go below their "auto" block-size.
    if (is_table)
      min_max_length_sizes.Encompass(IntrinsicBlockSizeFunc());
  }

  const auto writing_direction = style.GetWritingDirection();
  bool is_start_dominant;
  if (writing_direction.IsHorizontal()) {
    is_start_dominant = IsTopDominant(container_writing_direction) ==
                        IsTopDominant(writing_direction);
  } else {
    is_start_dominant = IsLeftDominant(container_writing_direction) ==
                        IsLeftDominant(writing_direction);
  }

  ComputeAbsoluteSize(
      border_padding.BlockSum(), MinMaxSizesFunc,
      space.PercentageResolutionInlineSizeForParentWritingMode(),
      space.AvailableSize().block_size, style.MarginBefore(),
      style.MarginAfter(), style.LogicalTop(), style.LogicalBottom(),
      min_max_length_sizes, static_position.offset.block_offset,
      GetStaticPositionEdge(static_position.block_edge), is_start_dominant,
      true /* is_block_direction */, is_table, is_shrink_to_fit, block_size,
      &dimensions->size.block_size, &dimensions->inset.block_start,
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
