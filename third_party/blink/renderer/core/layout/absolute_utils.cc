// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/absolute_utils.h"

#include <algorithm>

#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/geometry/static_position.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

namespace {

using InsetBias = InsetModifiedContainingBlock::InsetBias;

StyleSelfAlignmentData AlignSelf(const ComputedStyle& style) {
  return RuntimeEnabledFeatures::LayoutAlignForPositionedEnabled()
             ? style.ResolvedAlignSelf(ItemPosition::kNormal)
             : StyleSelfAlignmentData(ItemPosition::kNormal,
                                      OverflowAlignment::kDefault);
}

StyleSelfAlignmentData JustifySelf(const ComputedStyle& style) {
  return RuntimeEnabledFeatures::LayoutAlignForPositionedEnabled()
             ? style.ResolvedJustifySelf(ItemPosition::kNormal)
             : StyleSelfAlignmentData(ItemPosition::kNormal,
                                      OverflowAlignment::kDefault);
}

inline InsetBias GetStaticPositionInsetBias(
    LogicalStaticPosition::InlineEdge inline_edge) {
  switch (inline_edge) {
    case LogicalStaticPosition::InlineEdge::kInlineStart:
      return InsetBias::kStart;
    case LogicalStaticPosition::InlineEdge::kInlineCenter:
      return InsetBias::kEqual;
    case LogicalStaticPosition::InlineEdge::kInlineEnd:
      return InsetBias::kEnd;
  }
}

inline InsetBias GetStaticPositionInsetBias(
    LogicalStaticPosition::BlockEdge block_edge) {
  switch (block_edge) {
    case LogicalStaticPosition::BlockEdge::kBlockStart:
      return InsetBias::kStart;
    case LogicalStaticPosition::BlockEdge::kBlockCenter:
      return InsetBias::kEqual;
    case LogicalStaticPosition::BlockEdge::kBlockEnd:
      return InsetBias::kEnd;
  }
}

InsetBias GetAlignmentInsetBias(
    const StyleSelfAlignmentData& alignment,
    WritingDirectionMode container_writing_direction,
    WritingDirectionMode self_writing_direction,
    bool is_justify_axis,
    absl::optional<InsetBias>* out_safe_inset_bias) {
  // `alignment` is in the writing-direction of the containing-block, vs. the
  // inset-bias which is relative to the writing-direction of the candidate.
  const LogicalToLogical bias(
      self_writing_direction, container_writing_direction, InsetBias::kStart,
      InsetBias::kEnd, InsetBias::kStart, InsetBias::kEnd);

  if (alignment.Overflow() == OverflowAlignment::kSafe) {
    *out_safe_inset_bias =
        is_justify_axis ? bias.InlineStart() : bias.BlockStart();
  }

  switch (alignment.GetPosition()) {
    case ItemPosition::kStart:
    case ItemPosition::kFlexStart:
    case ItemPosition::kBaseline:
    case ItemPosition::kStretch:
    case ItemPosition::kNormal:
    case ItemPosition::kAnchorCenter:
      return is_justify_axis ? bias.InlineStart() : bias.BlockStart();
    case ItemPosition::kCenter:
      return InsetBias::kEqual;
    case ItemPosition::kEnd:
    case ItemPosition::kFlexEnd:
    case ItemPosition::kLastBaseline:
      return is_justify_axis ? bias.InlineEnd() : bias.BlockEnd();
    case ItemPosition::kSelfStart:
      return InsetBias::kStart;
    case ItemPosition::kSelfEnd:
      return InsetBias::kEnd;
    case ItemPosition::kLeft:
      DCHECK(is_justify_axis);
      return container_writing_direction.IsLtr() ? bias.InlineStart()
                                                 : bias.InlineEnd();
    case ItemPosition::kRight:
      DCHECK(is_justify_axis);
      return container_writing_direction.IsRtl() ? bias.InlineStart()
                                                 : bias.InlineEnd();
    case ItemPosition::kLegacy:
    case ItemPosition::kAuto:
      NOTREACHED();
      return InsetBias::kStart;
  }
}

// Computes the inset modified containing block in one axis, accounting for
// insets and the static-position.
void ComputeUnclampedIMCBInOneAxis(
    const LayoutUnit available_size,
    const absl::optional<LayoutUnit>& inset_start,
    const absl::optional<LayoutUnit>& inset_end,
    const LayoutUnit static_position_offset,
    InsetBias static_position_inset_bias,
    InsetBias alignment_inset_bias,
    const absl::optional<InsetBias>& safe_alignment_inset_bias,
    LayoutUnit* imcb_start_out,
    LayoutUnit* imcb_end_out,
    InsetBias* imcb_inset_bias_out,
    absl::optional<InsetBias>* imcb_safe_inset_bias_out) {
  DCHECK_NE(available_size, kIndefiniteSize);
  if (!inset_start && !inset_end) {
    // If both our insets are auto, the available-space is defined by the
    // static-position.
    switch (static_position_inset_bias) {
      case InsetBias::kStart:
        // The available-space for the start static-position "grows" towards the
        // end edge.
        // |      *----------->|
        *imcb_start_out = static_position_offset;
        *imcb_end_out = LayoutUnit();
        break;
      case InsetBias::kEqual: {
        // The available-space for the center static-position "grows" towards
        // both edges (equally), and stops when it hits the first one.
        // |<-----*----->      |
        LayoutUnit half_imcb_size = std::min(
            static_position_offset, available_size - static_position_offset);
        *imcb_start_out = static_position_offset - half_imcb_size;
        *imcb_end_out =
            available_size - static_position_offset - half_imcb_size;
        break;
      }
      case InsetBias::kEnd:
        // The available-space for the end static-position "grows" towards the
        // start edge.
        // |<-----*            |
        *imcb_end_out = available_size - static_position_offset;
        *imcb_start_out = LayoutUnit();
        break;
    }
    *imcb_inset_bias_out = static_position_inset_bias;
  } else {
    // Otherwise we just resolve auto to 0.
    *imcb_start_out = inset_start.value_or(LayoutUnit());
    *imcb_end_out = inset_end.value_or(LayoutUnit());

    if (!inset_start.has_value() || !inset_end.has_value()) {
      // In the case that only one inset is auto, that is the weaker inset;
      *imcb_inset_bias_out =
          inset_start.has_value() ? InsetBias::kStart : InsetBias::kEnd;
    } else {
      // Both insets were set - use the alignment bias (defaults to the "start"
      // edge of the containing block if we have normal alignment).
      *imcb_inset_bias_out = alignment_inset_bias;
      *imcb_safe_inset_bias_out = safe_alignment_inset_bias;
    }
  }
}

InsetModifiedContainingBlock ComputeUnclampedIMCB(
    const LogicalSize& available_size,
    const LogicalOofInsets& insets,
    const LogicalStaticPosition& static_position,
    const ComputedStyle& style,
    WritingDirectionMode container_writing_direction,
    WritingDirectionMode self_writing_direction) {
  InsetModifiedContainingBlock imcb;
  imcb.available_size = available_size;
  imcb.has_auto_inline_inset = !insets.inline_start || !insets.inline_end;
  imcb.has_auto_block_inset = !insets.block_start || !insets.block_end;

  const bool is_parallel =
      IsParallelWritingMode(container_writing_direction.GetWritingMode(),
                            self_writing_direction.GetWritingMode());
  const auto inline_alignment =
      is_parallel ? JustifySelf(style) : AlignSelf(style);
  const auto block_alignment =
      is_parallel ? AlignSelf(style) : JustifySelf(style);

  absl::optional<InsetBias> safe_inline_alignment_inset_bias;
  const auto inline_alignment_inset_bias = GetAlignmentInsetBias(
      inline_alignment, container_writing_direction, self_writing_direction,
      /* is_justify_axis */ is_parallel, &safe_inline_alignment_inset_bias);
  absl::optional<InsetBias> safe_block_alignment_inset_bias;
  const auto block_alignment_inset_bias = GetAlignmentInsetBias(
      block_alignment, container_writing_direction, self_writing_direction,
      /* is_justify_axis */ !is_parallel, &safe_block_alignment_inset_bias);

  ComputeUnclampedIMCBInOneAxis(
      available_size.inline_size, insets.inline_start, insets.inline_end,
      static_position.offset.inline_offset,
      GetStaticPositionInsetBias(static_position.inline_edge),
      inline_alignment_inset_bias, safe_inline_alignment_inset_bias,
      &imcb.inline_start, &imcb.inline_end, &imcb.inline_inset_bias,
      &imcb.safe_inline_inset_bias);
  ComputeUnclampedIMCBInOneAxis(
      available_size.block_size, insets.block_start, insets.block_end,
      static_position.offset.block_offset,
      GetStaticPositionInsetBias(static_position.block_edge),
      block_alignment_inset_bias, safe_block_alignment_inset_bias,
      &imcb.block_start, &imcb.block_end, &imcb.block_inset_bias,
      &imcb.safe_block_inset_bias);
  return imcb;
}

// Absolutize margin values to pixels and resolve any auto margins.
// https://drafts.csswg.org/css-position-3/#abspos-margins
void ComputeMargins(const LayoutUnit margin_percentage_resolution_size,
                    const LayoutUnit imcb_size,
                    const Length& margin_start_length,
                    const Length& margin_end_length,
                    const LayoutUnit size,
                    bool has_auto_inset,
                    bool is_start_dominant,
                    bool is_block_direction,
                    LayoutUnit* margin_start_out,
                    LayoutUnit* margin_end_out) {
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
  // |margin_start| + |size| + |margin_end| = |imcb_size|
  if (!has_auto_inset) {
    // "If left, right, and width are not auto:"
    // Compute margins.
    const LayoutUnit free_space = imcb_size - size -
                                  margin_start.value_or(LayoutUnit()) -
                                  margin_end.value_or(LayoutUnit());

    if (!margin_start && !margin_end) {
      // When both margins are auto.
      if (free_space > LayoutUnit() || is_block_direction) {
        margin_start = free_space / 2;
        margin_end = free_space - *margin_start;
      } else {
        // Margins are negative.
        if (is_start_dominant) {
          margin_start = LayoutUnit();
          margin_end = free_space;
        } else {
          margin_start = free_space;
          margin_end = LayoutUnit();
        }
      }
    } else if (!margin_start) {
      margin_start = free_space;
    } else if (!margin_end) {
      margin_end = free_space;
    }
  }

  // Set any unknown margins.
  *margin_start_out = margin_start.value_or(LayoutUnit());
  *margin_end_out = margin_end.value_or(LayoutUnit());
}

void ResizeIMCBInOneAxis(const InsetBias inset_bias,
                         const LayoutUnit amount,
                         LayoutUnit* inset_start,
                         LayoutUnit* inset_end) {
  switch (inset_bias) {
    case InsetBias::kStart:
      *inset_end += amount;
      break;
    case InsetBias::kEnd:
      *inset_start += amount;
      break;
    case InsetBias::kEqual:
      *inset_start += amount / 2;
      *inset_end += amount / 2;
      break;
  }
}

// Align the margin box within the inset-modified containing block as defined by
// its self-alignment properties.
// https://drafts.csswg.org/css-position-3/#abspos-layout
void ComputeInsets(const LayoutUnit available_size,
                   LayoutUnit imcb_start,
                   LayoutUnit imcb_end,
                   const InsetBias imcb_inset_bias,
                   const absl::optional<InsetBias>& imcb_safe_inset_bias,
                   const LayoutUnit margin_start,
                   const LayoutUnit margin_end,
                   const LayoutUnit size,
                   LayoutUnit* inset_start_out,
                   LayoutUnit* inset_end_out) {
  DCHECK_NE(available_size, kIndefiniteSize);
  LayoutUnit free_space =
      available_size - imcb_start - imcb_end - margin_start - margin_end - size;
  InsetBias bias = imcb_inset_bias;
  if (imcb_safe_inset_bias && free_space < LayoutUnit()) {
    free_space = LayoutUnit();
    bias = *imcb_safe_inset_bias;
  }

  // Move the weaker inset edge to consume all the free space, so that:
  // `imcb_start` + `margin_start` + `size` + `margin_end` + `imcb_end` =
  // `available_size`
  ResizeIMCBInOneAxis(bias, free_space, &imcb_start, &imcb_end);

  *inset_start_out = imcb_start + margin_start;
  *inset_end_out = imcb_end + margin_end;
}

bool CanComputeBlockSizeWithoutLayout(
    const BlockNode& node,
    WritingDirectionMode container_writing_direction,
    ItemPosition block_alignment_position,
    bool has_auto_block_inset) {
  // Tables (even with an explicit size) apply a min-content constraint.
  if (node.IsTable()) {
    return false;
  }
  // Replaced elements always have their size computed ahead of time.
  if (node.IsReplaced()) {
    return true;
  }
  const auto& style = node.Style();
  if (style.LogicalHeight().IsContentOrIntrinsic() ||
      style.LogicalMinHeight().IsContentOrIntrinsic() ||
      style.LogicalMaxHeight().IsContentOrIntrinsic()) {
    return false;
  }
  if (style.LogicalHeight().IsAuto()) {
    // Any 'auto' inset will trigger shink-to-fit sizing.
    if (has_auto_block_inset) {
      return false;
    }
    if (block_alignment_position == ItemPosition::kStretch) {
      return true;
    }
    // Non-normal alignment will trigger shrink-to-fit sizing.
    if (block_alignment_position != ItemPosition::kNormal) {
      return false;
    }
  }
  return true;
}

}  // namespace

LogicalOofInsets ComputeOutOfFlowInsets(
    const ComputedStyle& style,
    const LogicalSize& available_logical_size,
    WritingDirectionMode container_writing_direction,
    WritingDirectionMode self_writing_direction,
    AnchorEvaluatorImpl* anchor_evaluator) {
  InsetArea inset_area;
  if (!style.GetInsetArea().IsNone() && anchor_evaluator->HasDefaultAnchor()) {
    // We only need to know if the inset-area is 'auto' or not below, but need
    // to consider writing direction as the inset-area falls back to 'auto' if
    // the axes are not orthogonal.
    inset_area = style.GetInsetArea().ToPhysical(container_writing_direction,
                                                 self_writing_direction);
  }
  // Compute in physical, because anchors may be in different `writing-mode` or
  // `direction`.
  const PhysicalSize available_size = ToPhysicalSize(
      available_logical_size, self_writing_direction.GetWritingMode());
  std::optional<LayoutUnit> left;
  if (const Length& left_length = style.UsedLeft(); !left_length.IsAuto()) {
    anchor_evaluator->SetAxis(/* is_y_axis */ false,
                              /* is_right_or_bottom */ false,
                              available_size.width);
    left = MinimumValueForLength(left_length, available_size.width,
                                 anchor_evaluator);
  } else if (!inset_area.IsNone()) {
    left = LayoutUnit();
  }
  std::optional<LayoutUnit> right;
  if (const Length& right_length = style.UsedRight(); !right_length.IsAuto()) {
    anchor_evaluator->SetAxis(/* is_y_axis */ false,
                              /* is_right_or_bottom */ true,
                              available_size.width);
    right = MinimumValueForLength(right_length, available_size.width,
                                  anchor_evaluator);
  } else if (!inset_area.IsNone()) {
    right = LayoutUnit();
  }

  std::optional<LayoutUnit> top;
  if (const Length& top_length = style.UsedTop(); !top_length.IsAuto()) {
    anchor_evaluator->SetAxis(/* is_y_axis */ true,
                              /* is_right_or_bottom */ false,
                              available_size.height);
    top = MinimumValueForLength(top_length, available_size.height,
                                anchor_evaluator);
  } else if (!inset_area.IsNone()) {
    top = LayoutUnit();
  }
  std::optional<LayoutUnit> bottom;
  if (const Length& bottom_length = style.UsedBottom();
      !bottom_length.IsAuto()) {
    anchor_evaluator->SetAxis(/* is_y_axis */ true,
                              /* is_right_or_bottom */ true,
                              available_size.height);
    bottom = MinimumValueForLength(bottom_length, available_size.height,
                                   anchor_evaluator);
  } else if (!inset_area.IsNone()) {
    bottom = LayoutUnit();
  }

  // Convert the physical insets to logical.
  PhysicalToLogical<absl::optional<LayoutUnit>&> insets(
      self_writing_direction, top, right, bottom, left);
  return {insets.InlineStart(), insets.InlineEnd(), insets.BlockStart(),
          insets.BlockEnd()};
}

InsetModifiedContainingBlock ComputeInsetModifiedContainingBlock(
    const BlockNode& node,
    const LogicalSize& available_size,
    const LogicalOofInsets& insets,
    const LogicalStaticPosition& static_position,
    WritingDirectionMode container_writing_direction,
    WritingDirectionMode self_writing_direction) {
  InsetModifiedContainingBlock imcb = ComputeUnclampedIMCB(
      available_size, insets, static_position, node.Style(),
      container_writing_direction, self_writing_direction);
  // Clamp any negative size to 0.
  if (imcb.InlineSize() < LayoutUnit()) {
    ResizeIMCBInOneAxis(imcb.inline_inset_bias, imcb.InlineSize(),
                        &imcb.inline_start, &imcb.inline_end);
  }
  if (imcb.BlockSize() < LayoutUnit()) {
    ResizeIMCBInOneAxis(imcb.block_inset_bias, imcb.BlockSize(),
                        &imcb.block_start, &imcb.block_end);
  }
  if (node.IsTable()) {
    // Tables should not be larger than the container.
    if (imcb.InlineSize() > available_size.inline_size) {
      ResizeIMCBInOneAxis(imcb.inline_inset_bias,
                          imcb.InlineSize() - available_size.inline_size,
                          &imcb.inline_start, &imcb.inline_end);
    }
    if (imcb.BlockSize() > available_size.block_size) {
      ResizeIMCBInOneAxis(imcb.block_inset_bias,
                          imcb.BlockSize() - available_size.block_size,
                          &imcb.block_start, &imcb.block_end);
    }
  }
  return imcb;
}

InsetModifiedContainingBlock ComputeIMCBForPositionFallback(
    const LogicalSize& available_size,
    const LogicalOofInsets& insets,
    const LogicalStaticPosition& static_position,
    const ComputedStyle& style,
    WritingDirectionMode container_writing_direction,
    WritingDirectionMode self_writing_direction) {
  return ComputeUnclampedIMCB(available_size, insets, static_position, style,
                              container_writing_direction,
                              self_writing_direction);
}

bool ComputeOofInlineDimensions(
    const BlockNode& node,
    const ComputedStyle& style,
    const ConstraintSpace& space,
    const InsetModifiedContainingBlock& imcb,
    const BoxStrut& border_padding,
    const absl::optional<LogicalSize>& replaced_size,
    WritingDirectionMode container_writing_direction,
    const AnchorEvaluatorImpl* anchor_evaluator,
    LogicalOofDimensions* dimensions) {
  DCHECK(dimensions);
  DCHECK_GE(imcb.InlineSize(), LayoutUnit());

  const bool is_justify_axis = IsParallelWritingMode(
      container_writing_direction.GetWritingMode(), style.GetWritingMode());
  const auto alignment_position =
      (is_justify_axis ? JustifySelf(style) : AlignSelf(style)).GetPosition();
  const auto block_alignment_position =
      (is_justify_axis ? AlignSelf(style) : JustifySelf(style)).GetPosition();

  bool depends_on_min_max_sizes = false;
  const bool can_compute_block_size_without_layout =
      CanComputeBlockSizeWithoutLayout(node, container_writing_direction,
                                       block_alignment_position,
                                       imcb.has_auto_block_inset);

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
      ComputeOofBlockDimensions(node, style, space, imcb, border_padding,
                                /* replaced_size */ absl::nullopt,
                                container_writing_direction, anchor_evaluator,
                                dimensions);
    }

    // Create a new space, setting the fixed block-size.
    ConstraintSpaceBuilder builder(style.GetWritingMode(),
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

    const bool is_implicit_stretch =
        !imcb.has_auto_inline_inset &&
        alignment_position == ItemPosition::kNormal;
    const bool is_explicit_stretch =
        !imcb.has_auto_inline_inset &&
        alignment_position == ItemPosition::kStretch;
    const bool is_stretch = is_implicit_stretch || is_explicit_stretch;

    // If our block constraint is strong/explicit.
    const bool is_block_explicit =
        !style.LogicalHeight().IsAuto() ||
        (!imcb.has_auto_block_inset &&
         block_alignment_position == ItemPosition::kStretch);

    // Determine how "auto" should resolve.
    if (main_inline_length.IsAuto()) {
      if (node.IsTable()) {
        // Tables always shrink-to-fit unless explicitly asked to stretch.
        main_inline_length = is_explicit_stretch ? Length::FillAvailable()
                                                 : Length::FitContent();
      } else if (!style.AspectRatio().IsAuto() &&
                 can_compute_block_size_without_layout &&
                 (!is_stretch || (is_implicit_stretch && is_block_explicit))) {
        // We'd like to apply the aspect-ratio.
        // The aspect-ratio applies from the block-axis if we can compute our
        // block-size without invoking layout, and either:
        //  - We aren't stretching our auto inline-size.
        //  - We are stretching our auto inline-size, but the block-size has a
        //    stronger (explicit) constraint, e.g:
        //    "height:10px" or "align-self:stretch".
        main_inline_length = Length::FitContent();

        // Apply the automatic minimum size.
        if (style.OverflowInlineDirection() == EOverflow::kVisible &&
            min_inline_length.IsAuto())
          min_inline_length = Length::MinIntrinsic();
      } else {
        main_inline_length =
            is_stretch ? Length::FillAvailable() : Length::FitContent();
      }
    }

    LayoutUnit main_inline_size = ResolveMainInlineLength(
        space, style, border_padding, MinMaxSizesFunc, main_inline_length,
        imcb.InlineSize(), anchor_evaluator);
    MinMaxSizes min_max_inline_sizes = ComputeMinMaxInlineSizes(
        space, node, border_padding, MinMaxSizesFunc, &min_inline_length,
        imcb.InlineSize(), anchor_evaluator);

    inline_size = min_max_inline_sizes.ClampSizeToMinAndMax(main_inline_size);
  }

  dimensions->size.inline_size = inline_size;

  // Determines if the "start" sides of margins match.
  const bool is_margin_start_dominant =
      LogicalToLogical(container_writing_direction, style.GetWritingDirection(),
                       /* inline_start */ true, /* inline_end */ false,
                       /* block_start */ true, /* block_end */ false)
          .InlineStart();

  // Determines if this is the block axis in the containing block.
  const bool is_block_direction = !IsParallelWritingMode(
      container_writing_direction.GetWritingMode(), style.GetWritingMode());

  ComputeMargins(space.PercentageResolutionInlineSizeForParentWritingMode(),
                 imcb.InlineSize(), style.MarginInlineStart(),
                 style.MarginInlineEnd(), inline_size,
                 imcb.has_auto_inline_inset, is_margin_start_dominant,
                 is_block_direction, &dimensions->margins.inline_start,
                 &dimensions->margins.inline_end);

  ComputeInsets(space.AvailableSize().inline_size, imcb.inline_start,
                imcb.inline_end, imcb.inline_inset_bias,
                imcb.safe_inline_inset_bias, dimensions->margins.inline_start,
                dimensions->margins.inline_end, inline_size,
                &dimensions->inset.inline_start, &dimensions->inset.inline_end);

  return depends_on_min_max_sizes;
}

const LayoutResult* ComputeOofBlockDimensions(
    const BlockNode& node,
    const ComputedStyle& style,
    const ConstraintSpace& space,
    const InsetModifiedContainingBlock& imcb,
    const BoxStrut& border_padding,
    const absl::optional<LogicalSize>& replaced_size,
    WritingDirectionMode container_writing_direction,
    const AnchorEvaluatorImpl* anchor_evaluator,
    LogicalOofDimensions* dimensions) {
  DCHECK(dimensions);
  DCHECK_GE(imcb.BlockSize(), LayoutUnit());

  const bool is_justify_axis = !IsParallelWritingMode(
      container_writing_direction.GetWritingMode(), style.GetWritingMode());
  const auto alignment_position =
      (is_justify_axis ? JustifySelf(style) : AlignSelf(style)).GetPosition();

  const LayoutResult* result = nullptr;

  MinMaxSizes min_max_block_sizes = ComputeMinMaxBlockSizes(
      space, style, border_padding, imcb.BlockSize(), anchor_evaluator);

  auto IntrinsicBlockSizeFunc = [&]() -> LayoutUnit {
    DCHECK(!node.IsReplaced());
    DCHECK_NE(dimensions->size.inline_size, kIndefiniteSize);

    if (!result) {
      // Create a new space, setting the fixed block-size.
      ConstraintSpaceBuilder builder(style.GetWritingMode(),
                                     style.GetWritingDirection(),
                                     /* is_new_fc */ true);
      builder.SetAvailableSize(
          {dimensions->size.inline_size, imcb.BlockSize()});
      builder.SetIsFixedInlineSize(true);
      builder.SetPercentageResolutionSize(space.PercentageResolutionSize());

      // Use the computed |MinMaxSizes| because |node.Layout()| can't resolve
      // the `anchor-size()` function.
      builder.SetOverrideMinMaxBlockSizes(min_max_block_sizes);

      // Tables need to know about the explicit stretch constraint to produce
      // the correct result.
      if (!imcb.has_auto_block_inset &&
          alignment_position == ItemPosition::kStretch) {
        builder.SetBlockAutoBehavior(AutoSizeBehavior::kStretchExplicit);
      }

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

    return LogicalFragment(style.GetWritingDirection(),
                           result->GetPhysicalFragment())
        .BlockSize();
  };

  LayoutUnit block_size;
  if (replaced_size) {
    DCHECK(node.IsReplaced());
    block_size = replaced_size->block_size;
  } else {
    Length main_block_length = style.LogicalHeight();

    const bool is_table = node.IsTable();

    const bool is_implicit_stretch =
        !imcb.has_auto_block_inset &&
        alignment_position == ItemPosition::kNormal;
    const bool is_explicit_stretch =
        !imcb.has_auto_block_inset &&
        alignment_position == ItemPosition::kStretch;
    const bool is_stretch = is_implicit_stretch || is_explicit_stretch;

    // Determine how "auto" should resolve.
    if (main_block_length.IsAuto()) {
      if (is_table) {
        // Tables always shrink-to-fit unless explicitly asked to stretch.
        main_block_length = is_explicit_stretch ? Length::FillAvailable()
                                                : Length::FitContent();
      } else if (!style.AspectRatio().IsAuto() &&
                 dimensions->size.inline_size != kIndefiniteSize &&
                 !is_explicit_stretch) {
        main_block_length = Length::FitContent();
      } else {
        main_block_length =
            is_stretch ? Length::FillAvailable() : Length::FitContent();
      }
    }

    LayoutUnit main_block_size = ResolveMainBlockLength(
        space, style, border_padding, main_block_length, IntrinsicBlockSizeFunc,
        imcb.BlockSize(), anchor_evaluator);

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

  // Determines if the "start" sides of margins match.
  const bool is_margin_start_dominant =
      LogicalToLogical(container_writing_direction, style.GetWritingDirection(),
                       /* inline_start */ true, /* inline_end */ false,
                       /* block_start */ true, /* block_end */ false)
          .BlockStart();

  // Determines if this is the block axis in the containing block.
  const bool is_block_direction = IsParallelWritingMode(
      container_writing_direction.GetWritingMode(), style.GetWritingMode());

  ComputeMargins(space.PercentageResolutionInlineSizeForParentWritingMode(),
                 imcb.BlockSize(), style.MarginBlockStart(),
                 style.MarginBlockEnd(), block_size, imcb.has_auto_block_inset,
                 is_margin_start_dominant, is_block_direction,
                 &dimensions->margins.block_start,
                 &dimensions->margins.block_end);

  ComputeInsets(space.AvailableSize().block_size, imcb.block_start,
                imcb.block_end, imcb.block_inset_bias,
                imcb.safe_block_inset_bias, dimensions->margins.block_start,
                dimensions->margins.block_end, block_size,
                &dimensions->inset.block_start, &dimensions->inset.block_end);

  return result;
}

void AdjustOffsetForSplitInline(const BlockNode& node,
                                const BoxFragmentBuilder* container_builder,
                                LogicalOffset& offset) {
  DCHECK(!RuntimeEnabledFeatures::LayoutNewContainingBlockEnabled());
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

  // The container_builder for LayoutViewTransitionRoot does not have any
  // children.
  if (container->IsAnonymousBlock() && !container->IsViewTransitionRoot()) {
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
