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
    std::optional<InsetBias>* out_safe_inset_bias) {
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

// Adjusts the insets so they will be equidistant from the center offset.
// |<-----*----->      |
void ResizeIMCBForCenterOffset(const LayoutUnit available_size,
                               const LayoutUnit offset,
                               LayoutUnit* inset_start,
                               LayoutUnit* inset_end) {
  const LayoutUnit half_imcb_size =
      std::min(offset - *inset_start, available_size - *inset_end - offset);
  *inset_start = offset - half_imcb_size;
  *inset_end = available_size - offset - half_imcb_size;
}

// Computes the inset modified containing block in one axis, accounting for
// insets and the static-position.
void ComputeUnclampedIMCBInOneAxis(
    const LayoutUnit available_size,
    const std::optional<LayoutUnit>& inset_start,
    const std::optional<LayoutUnit>& inset_end,
    const LayoutUnit static_position_offset,
    InsetBias static_position_inset_bias,
    InsetBias alignment_inset_bias,
    const std::optional<InsetBias>& safe_alignment_inset_bias,
    LayoutUnit* imcb_start_out,
    LayoutUnit* imcb_end_out,
    InsetBias* imcb_inset_bias_out,
    std::optional<InsetBias>* imcb_safe_inset_bias_out) {
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
        *imcb_start_out = LayoutUnit();
        *imcb_end_out = LayoutUnit();
        ResizeIMCBForCenterOffset(available_size, static_position_offset,
                                  imcb_start_out, imcb_end_out);
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
    const LogicalAlignment& alignment,
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

  std::optional<InsetBias> safe_inline_alignment_inset_bias;
  const auto inline_alignment_inset_bias = GetAlignmentInsetBias(
      alignment.inline_alignment, container_writing_direction,
      self_writing_direction,
      /* is_justify_axis */ is_parallel, &safe_inline_alignment_inset_bias);
  std::optional<InsetBias> safe_block_alignment_inset_bias;
  const auto block_alignment_inset_bias = GetAlignmentInsetBias(
      alignment.block_alignment, container_writing_direction,
      self_writing_direction,
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
void ComputeMargins(LogicalSize margin_percentage_resolution_size,
                    const LayoutUnit imcb_size,
                    const Length& margin_start_length,
                    const Length& margin_end_length,
                    const LayoutUnit size,
                    bool has_auto_inset,
                    bool is_start_dominant,
                    bool is_block_direction,
                    LayoutUnit* margin_start_out,
                    LayoutUnit* margin_end_out) {
  std::optional<LayoutUnit> margin_start;
  if (!margin_start_length.IsAuto()) {
    margin_start = MinimumValueForLength(
        margin_start_length, margin_percentage_resolution_size.inline_size);
  }
  std::optional<LayoutUnit> margin_end;
  if (!margin_end_length.IsAuto()) {
    margin_end = MinimumValueForLength(
        margin_end_length, margin_percentage_resolution_size.inline_size);
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

// Align the margin box within the inset-modified containing block as defined by
// its self-alignment properties.
// https://drafts.csswg.org/css-position-3/#abspos-layout
void ComputeInsets(const LayoutUnit available_size,
                   LayoutUnit imcb_start,
                   LayoutUnit imcb_end,
                   const InsetBias imcb_inset_bias,
                   const std::optional<InsetBias>& imcb_safe_inset_bias,
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
  if (style.LogicalHeight().HasContentOrIntrinsic() ||
      style.LogicalMinHeight().HasContentOrIntrinsic() ||
      style.LogicalMaxHeight().HasContentOrIntrinsic()) {
    return false;
  }
  if (style.LogicalHeight().HasAuto()) {
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
    const LogicalAlignment& alignment,
    WritingDirectionMode self_writing_direction) {
  bool force_x_insets_to_zero = false;
  bool force_y_insets_to_zero = false;
  std::optional<InsetAreaOffsets> offsets = style.InsetAreaOffsets();
  if (offsets.has_value()) {
    force_x_insets_to_zero = force_y_insets_to_zero = true;
  }
  if (alignment.inline_alignment.GetPosition() == ItemPosition::kAnchorCenter) {
    if (self_writing_direction.IsHorizontal()) {
      force_x_insets_to_zero = true;
    } else {
      force_y_insets_to_zero = true;
    }
  }
  if (alignment.block_alignment.GetPosition() == ItemPosition::kAnchorCenter) {
    if (self_writing_direction.IsHorizontal()) {
      force_y_insets_to_zero = true;
    } else {
      force_x_insets_to_zero = true;
    }
  }

  // Compute in physical, because anchors may be in different `writing-mode` or
  // `direction`.
  const PhysicalSize available_size = ToPhysicalSize(
      available_logical_size, self_writing_direction.GetWritingMode());
  std::optional<LayoutUnit> left;
  if (const Length& left_length = style.Left(); !left_length.IsAuto()) {
    left = MinimumValueForLength(left_length, available_size.width);
  } else if (force_x_insets_to_zero) {
    left = LayoutUnit();
  }
  std::optional<LayoutUnit> right;
  if (const Length& right_length = style.Right(); !right_length.IsAuto()) {
    right = MinimumValueForLength(right_length, available_size.width);
  } else if (force_x_insets_to_zero) {
    right = LayoutUnit();
  }

  std::optional<LayoutUnit> top;
  if (const Length& top_length = style.Top(); !top_length.IsAuto()) {
    top = MinimumValueForLength(top_length, available_size.height);
  } else if (force_y_insets_to_zero) {
    top = LayoutUnit();
  }
  std::optional<LayoutUnit> bottom;
  if (const Length& bottom_length = style.Bottom(); !bottom_length.IsAuto()) {
    bottom = MinimumValueForLength(bottom_length, available_size.height);
  } else if (force_y_insets_to_zero) {
    bottom = LayoutUnit();
  }

  // Convert the physical insets to logical.
  PhysicalToLogical<std::optional<LayoutUnit>&> insets(
      self_writing_direction, top, right, bottom, left);
  return {insets.InlineStart(), insets.InlineEnd(), insets.BlockStart(),
          insets.BlockEnd()};
}

LogicalAlignment ComputeAlignment(
    const ComputedStyle& style,
    WritingDirectionMode container_writing_direction,
    WritingDirectionMode self_writing_direction) {
  ItemPosition align_normal_behavior = ItemPosition::kNormal;
  ItemPosition justify_normal_behavior = ItemPosition::kNormal;
  const InsetArea inset_area = style.GetInsetArea().ToPhysical(
      container_writing_direction, self_writing_direction);
  if (!inset_area.IsNone()) {
    std::tie(align_normal_behavior, justify_normal_behavior) =
        inset_area.AlignJustifySelfFromPhysical(container_writing_direction);
  }
  const bool is_parallel =
      IsParallelWritingMode(container_writing_direction.GetWritingMode(),
                            self_writing_direction.GetWritingMode());
  return is_parallel
             ? LogicalAlignment{style.ResolvedJustifySelf(
                                    justify_normal_behavior),
                                style.ResolvedAlignSelf(align_normal_behavior)}
             : LogicalAlignment{
                   style.ResolvedAlignSelf(align_normal_behavior),
                   style.ResolvedJustifySelf(justify_normal_behavior)};
}

LogicalAnchorCenterPosition ComputeAnchorCenterPosition(
    const ComputedStyle& style,
    const LogicalAlignment& alignment,
    WritingDirectionMode writing_direction,
    LogicalSize available_logical_size) {
  // Compute in physical, because anchors may be in different writing-mode.
  const ItemPosition inline_position = alignment.inline_alignment.GetPosition();
  const ItemPosition block_position = alignment.block_alignment.GetPosition();

  const bool has_anchor_center_in_x =
      writing_direction.IsHorizontal()
          ? inline_position == ItemPosition::kAnchorCenter
          : block_position == ItemPosition::kAnchorCenter;
  const bool has_anchor_center_in_y =
      writing_direction.IsHorizontal()
          ? block_position == ItemPosition::kAnchorCenter
          : inline_position == ItemPosition::kAnchorCenter;

  const PhysicalSize available_size = ToPhysicalSize(
      available_logical_size, writing_direction.GetWritingMode());
  std::optional<LayoutUnit> left;
  std::optional<LayoutUnit> top;
  std::optional<LayoutUnit> right;
  std::optional<LayoutUnit> bottom;
  if (style.AnchorCenterOffset().has_value()) {
    if (has_anchor_center_in_x) {
      left = style.AnchorCenterOffset()->left;
      if (left) {
        right = available_size.width - *left;
      }
    }
    if (has_anchor_center_in_y) {
      top = style.AnchorCenterOffset()->top;
      if (top) {
        bottom = available_size.height - *top;
      }
    }
  }

  // Convert result back to logical against `writing_direction`.
  PhysicalToLogical converter(writing_direction, top, right, bottom, left);
  return LogicalAnchorCenterPosition{converter.InlineStart(),
                                     converter.BlockStart()};
}

InsetModifiedContainingBlock ComputeInsetModifiedContainingBlock(
    const BlockNode& node,
    const LogicalSize& available_size,
    const LogicalAlignment& alignment,
    const LogicalOofInsets& insets,
    const LogicalStaticPosition& static_position,
    const LogicalAnchorCenterPosition& anchor_center_position,
    WritingDirectionMode container_writing_direction,
    WritingDirectionMode self_writing_direction) {
  InsetModifiedContainingBlock imcb = ComputeUnclampedIMCB(
      available_size, alignment, insets, static_position, node.Style(),
      container_writing_direction, self_writing_direction);
  // `anchor-center` coerces the IMCB to be based on the anchor center position.
  if (anchor_center_position.inline_offset) {
    ResizeIMCBForCenterOffset(available_size.inline_size,
                              *anchor_center_position.inline_offset,
                              &imcb.inline_start, &imcb.inline_end);
    imcb.inline_inset_bias = InsetBias::kEqual;
    imcb.safe_inline_inset_bias = std::nullopt;
  }
  if (anchor_center_position.block_offset) {
    ResizeIMCBForCenterOffset(available_size.block_size,
                              *anchor_center_position.block_offset,
                              &imcb.block_start, &imcb.block_end);
    imcb.block_inset_bias = InsetBias::kEqual;
    imcb.safe_block_inset_bias = std::nullopt;
  }
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
    const LogicalAlignment& alignment,
    const LogicalOofInsets& insets,
    const LogicalStaticPosition& static_position,
    const ComputedStyle& style,
    WritingDirectionMode container_writing_direction,
    WritingDirectionMode self_writing_direction) {
  return ComputeUnclampedIMCB(
      available_size, alignment, insets, static_position, style,
      container_writing_direction, self_writing_direction);
}

bool ComputeOofInlineDimensions(
    const BlockNode& node,
    const ComputedStyle& style,
    const ConstraintSpace& space,
    const InsetModifiedContainingBlock& imcb,
    const LogicalAlignment& alignment,
    const BoxStrut& border_padding,
    const std::optional<LogicalSize>& replaced_size,
    WritingDirectionMode container_writing_direction,
    LogicalOofDimensions* dimensions) {
  DCHECK(dimensions);
  DCHECK_GE(imcb.InlineSize(), LayoutUnit());

  const auto alignment_position = alignment.inline_alignment.GetPosition();
  const auto block_alignment_position = alignment.block_alignment.GetPosition();

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
      ComputeOofBlockDimensions(node, style, space, imcb, alignment,
                                border_padding,
                                /* replaced_size */ std::nullopt,
                                container_writing_direction, dimensions);
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
    const Length& main_inline_length = style.LogicalWidth();

    const bool is_implicit_stretch =
        !imcb.has_auto_inline_inset &&
        alignment_position == ItemPosition::kNormal;
    const bool is_explicit_stretch =
        !imcb.has_auto_inline_inset &&
        alignment_position == ItemPosition::kStretch;
    const bool is_stretch = is_implicit_stretch || is_explicit_stretch;

    // If our block constraint is strong/explicit.
    const bool is_block_explicit =
        !style.LogicalHeight().HasAuto() ||
        (!imcb.has_auto_block_inset &&
         block_alignment_position == ItemPosition::kStretch);

    // Determine how "auto" should resolve.
    bool apply_automatic_min_size = false;
    const Length& auto_length = ([&]() {
      // Tables always shrink-to-fit unless explicitly asked to stretch.
      if (node.IsTable()) {
        return is_explicit_stretch ? Length::FillAvailable()
                                   : Length::FitContent();
      }
      // We'd like to apply the aspect-ratio.
      // The aspect-ratio applies from the block-axis if we can compute our
      // block-size without invoking layout, and either:
      //  - We aren't stretching our auto inline-size.
      //  - We are stretching our auto inline-size, but the block-size has a
      //    stronger (explicit) constraint, e.g:
      //    "height:10px" or "align-self:stretch".
      if (!style.AspectRatio().IsAuto() &&
          can_compute_block_size_without_layout &&
          (!is_stretch || (is_implicit_stretch && is_block_explicit))) {
        // See if we should apply the automatic minimum size.
        if (style.OverflowInlineDirection() == EOverflow::kVisible) {
          apply_automatic_min_size = true;
        }
        return Length::FitContent();
      }
      return is_stretch ? Length::FillAvailable() : Length::FitContent();
    })();

    const Length& min_inline_length =
        apply_automatic_min_size && style.LogicalMinWidth().HasAuto()
            ? Length::MinIntrinsic()
            : style.LogicalMinWidth();

    LayoutUnit main_inline_size = ResolveMainInlineLength(
        space, style, border_padding, MinMaxSizesFunc, main_inline_length,
        &auto_length, imcb.InlineSize());
    MinMaxSizes min_max_inline_sizes =
        ComputeMinMaxInlineSizes(space, node, border_padding, MinMaxSizesFunc,
                                 &min_inline_length, imcb.InlineSize());

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

  ComputeMargins(
      space.MarginPaddingPercentageResolutionSize(), imcb.InlineSize(),
      style.MarginInlineStart(), style.MarginInlineEnd(), inline_size,
      imcb.has_auto_inline_inset, is_margin_start_dominant, is_block_direction,
      &dimensions->margins.inline_start, &dimensions->margins.inline_end);

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
    const LogicalAlignment& alignment,
    const BoxStrut& border_padding,
    const std::optional<LogicalSize>& replaced_size,
    WritingDirectionMode container_writing_direction,
    LogicalOofDimensions* dimensions) {
  DCHECK(dimensions);
  DCHECK_GE(imcb.BlockSize(), LayoutUnit());

  const auto alignment_position = alignment.block_alignment.GetPosition();
  const LayoutResult* result = nullptr;

  MinMaxSizes min_max_block_sizes =
      ComputeMinMaxBlockSizes(space, style, border_padding, imcb.BlockSize());

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
    const Length& main_block_length = style.LogicalHeight();

    const bool is_table = node.IsTable();

    const bool is_implicit_stretch =
        !imcb.has_auto_block_inset &&
        alignment_position == ItemPosition::kNormal;
    const bool is_explicit_stretch =
        !imcb.has_auto_block_inset &&
        alignment_position == ItemPosition::kStretch;
    const bool is_stretch = is_implicit_stretch || is_explicit_stretch;

    // Determine how "auto" should resolve.
    const Length& auto_length = ([&]() {
      // Tables always shrink-to-fit unless explicitly asked to stretch.
      if (is_table) {
        return is_explicit_stretch ? Length::FillAvailable()
                                   : Length::FitContent();
      }
      if (!style.AspectRatio().IsAuto() &&
          dimensions->size.inline_size != kIndefiniteSize &&
          !is_explicit_stretch) {
        return Length::FitContent();
      }
      return is_stretch ? Length::FillAvailable() : Length::FitContent();
    })();

    const LayoutUnit main_block_size = ResolveMainBlockLength(
        space, style, border_padding, main_block_length, &auto_length,
        IntrinsicBlockSizeFunc, imcb.BlockSize());

    // Manually resolve any intrinsic/content min/max block-sizes.
    // TODO(crbug.com/1135207): |ComputeMinMaxBlockSizes()| should handle this.
    if (style.LogicalMinHeight().HasContentOrIntrinsic()) {
      min_max_block_sizes.min_size = IntrinsicBlockSizeFunc();
    }
    if (style.LogicalMaxHeight().HasContentOrIntrinsic()) {
      min_max_block_sizes.max_size = IntrinsicBlockSizeFunc();
    }
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

  ComputeMargins(
      space.MarginPaddingPercentageResolutionSize(), imcb.BlockSize(),
      style.MarginBlockStart(), style.MarginBlockEnd(), block_size,
      imcb.has_auto_block_inset, is_margin_start_dominant, is_block_direction,
      &dimensions->margins.block_start, &dimensions->margins.block_end);

  ComputeInsets(space.AvailableSize().block_size, imcb.block_start,
                imcb.block_end, imcb.block_inset_bias,
                imcb.safe_block_inset_bias, dimensions->margins.block_start,
                dimensions->margins.block_end, block_size,
                &dimensions->inset.block_start, &dimensions->inset.block_end);

  return result;
}

}  // namespace blink
