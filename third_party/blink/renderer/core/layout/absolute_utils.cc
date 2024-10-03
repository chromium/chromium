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
    std::optional<InsetBias>* out_safe_inset_bias,
    std::optional<InsetBias>* out_default_inset_bias) {
  // `alignment` is in the writing-direction of the containing-block, vs. the
  // inset-bias which is relative to the writing-direction of the candidate.
  const LogicalToLogical bias(
      self_writing_direction, container_writing_direction, InsetBias::kStart,
      InsetBias::kEnd, InsetBias::kStart, InsetBias::kEnd);

  if (alignment.Overflow() == OverflowAlignment::kSafe) {
    *out_safe_inset_bias =
        is_justify_axis ? bias.InlineStart() : bias.BlockStart();
  }
  if (alignment.Overflow() == OverflowAlignment::kDefault &&
      alignment.GetPosition() != ItemPosition::kNormal) {
    *out_default_inset_bias =
        is_justify_axis ? bias.InlineStart() : bias.BlockStart();
  }

  switch (alignment.GetPosition()) {
    case ItemPosition::kStart:
    case ItemPosition::kFlexStart:
    case ItemPosition::kBaseline:
    case ItemPosition::kStretch:
    case ItemPosition::kNormal:
      return is_justify_axis ? bias.InlineStart() : bias.BlockStart();
    case ItemPosition::kAnchorCenter:
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
      NOTREACHED_IN_MIGRATION();
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

// Computes the inset modified containing block in one axis, accounting for
// insets and the static-position.
void ComputeUnclampedIMCBInOneAxis(
    const LayoutUnit available_size,
    const std::optional<LayoutUnit>& inset_start,
    const std::optional<LayoutUnit>& inset_end,
    const LayoutUnit static_position_offset,
    InsetBias static_position_inset_bias,
    InsetBias alignment_inset_bias,
    const std::optional<InsetBias>& safe_inset_bias,
    const std::optional<InsetBias>& default_inset_bias,
    LayoutUnit* imcb_start_out,
    LayoutUnit* imcb_end_out,
    InsetBias* imcb_inset_bias_out,
    std::optional<InsetBias>* safe_inset_bias_out,
    std::optional<InsetBias>* default_inset_bias_out) {
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
        const LayoutUnit half_size = std::min(
            static_position_offset, available_size - static_position_offset);
        *imcb_start_out = static_position_offset - half_size;
        *imcb_end_out = available_size - static_position_offset - half_size;
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
      *safe_inset_bias_out = safe_inset_bias;
      *default_inset_bias_out = default_inset_bias;
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

  std::optional<InsetBias> inline_safe_inset_bias;
  std::optional<InsetBias> inline_default_inset_bias;
  const auto inline_alignment_inset_bias = GetAlignmentInsetBias(
      alignment.inline_alignment, container_writing_direction,
      self_writing_direction,
      /* is_justify_axis */ is_parallel, &inline_safe_inset_bias,
      &inline_default_inset_bias);
  std::optional<InsetBias> block_safe_inset_bias;
  std::optional<InsetBias> block_default_inset_bias;
  const auto block_alignment_inset_bias =
      GetAlignmentInsetBias(alignment.block_alignment,
                            container_writing_direction, self_writing_direction,
                            /* is_justify_axis */ !is_parallel,
                            &block_safe_inset_bias, &block_default_inset_bias);

  ComputeUnclampedIMCBInOneAxis(
      available_size.inline_size, insets.inline_start, insets.inline_end,
      static_position.offset.inline_offset,
      GetStaticPositionInsetBias(static_position.inline_edge),
      inline_alignment_inset_bias, inline_safe_inset_bias,
      inline_default_inset_bias, &imcb.inline_start, &imcb.inline_end,
      &imcb.inline_inset_bias, &imcb.inline_safe_inset_bias,
      &imcb.inline_default_inset_bias);
  ComputeUnclampedIMCBInOneAxis(
      available_size.block_size, insets.block_start, insets.block_end,
      static_position.offset.block_offset,
      GetStaticPositionInsetBias(static_position.block_edge),
      block_alignment_inset_bias, block_safe_inset_bias,
      block_default_inset_bias, &imcb.block_start, &imcb.block_end,
      &imcb.block_inset_bias, &imcb.block_safe_inset_bias,
      &imcb.block_default_inset_bias);
  return imcb;
}

// Absolutize margin values to pixels and resolve any auto margins.
// https://drafts.csswg.org/css-position-3/#abspos-margins
bool ComputeMargins(LogicalSize margin_percentage_resolution_size,
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

  const bool apply_auto_margins =
      !has_auto_inset && (!margin_start || !margin_end);

  // Solving the equation:
  // |margin_start| + |size| + |margin_end| = |imcb_size|
  if (apply_auto_margins) {
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

  // Set any unknown margins, auto margins with any auto inset resolve to zero.
  *margin_start_out = margin_start.value_or(LayoutUnit());
  *margin_end_out = margin_end.value_or(LayoutUnit());

  return apply_auto_margins;
}

// Align the margin box within the inset-modified containing block as defined by
// its self-alignment properties.
// https://drafts.csswg.org/css-position-3/#abspos-layout
void ComputeInsets(const LayoutUnit available_size,
                   const LayoutUnit container_start,
                   const LayoutUnit container_end,
                   const LayoutUnit original_imcb_start,
                   const LayoutUnit original_imcb_end,
                   const InsetBias imcb_inset_bias,
                   const std::optional<InsetBias>& safe_inset_bias,
                   const std::optional<InsetBias>& default_inset_bias,
                   const LayoutUnit margin_start,
                   const LayoutUnit margin_end,
                   const LayoutUnit size,
                   const std::optional<LayoutUnit>& anchor_center_offset,
                   LayoutUnit* inset_start_out,
                   LayoutUnit* inset_end_out) {
  DCHECK_NE(available_size, kIndefiniteSize);

  LayoutUnit imcb_start = original_imcb_start;
  LayoutUnit imcb_end = original_imcb_end;

  // First if we have a valid anchor-center position, adjust the offsets so
  // that it is centered on that point.
  //
  // At this stage it doesn't matter what the resulting free-space is, just
  // that if we have safe alignment, we bias towards the safe inset.
  if (anchor_center_offset) {
    const LayoutUnit half_size =
        (safe_inset_bias.value_or(InsetBias::kStart) == InsetBias::kStart)
            ? *anchor_center_offset - imcb_start
            : available_size - *anchor_center_offset - imcb_end;
    imcb_start = *anchor_center_offset - half_size;
    imcb_end = available_size - *anchor_center_offset - half_size;
  }

  // Determine the free-space. If we have safe alignment specified, e.g.
  // "justify-self: safe start", clamp the free-space to zero and bias towards
  // the safe edge (may be end if RTL for example).
  LayoutUnit free_space =
      available_size - imcb_start - imcb_end - margin_start - size - margin_end;
  InsetBias bias = imcb_inset_bias;
  bool apply_safe_bias = safe_inset_bias && free_space < LayoutUnit();
  if (apply_safe_bias) {
    free_space = LayoutUnit();
    bias = *safe_inset_bias;
  }

  // Move the weaker inset edge to consume all the free space, so that:
  // `imcb_start` + `margin_start` + `size` + `margin_end` + `imcb_end` =
  // `available_size`
  ResizeIMCBInOneAxis(bias, free_space, &imcb_start, &imcb_end);

  // Finally consider the default alignment overflow behavior if applicable.
  // This only applies when both insets are specified, and we have non-normal
  // alignment.
  //
  // This will take the element, and shift it to be within the bounds of the
  // containing-block. It will prioritize the edge specified by
  // `default_inset_bias`.
  if (default_inset_bias && !apply_safe_bias) {
    // If the insets shifted the IMCB outside the containing-block, we consider
    // that to be the safe edge.
    auto adjust_start = [&]() {
      const LayoutUnit safe_start =
          std::min(original_imcb_start, -container_start);
      if (imcb_start < safe_start) {
        imcb_end += (imcb_start - safe_start);
        imcb_start = safe_start;
      }
    };
    auto adjust_end = [&]() {
      const LayoutUnit safe_end = std::min(original_imcb_end, -container_end);
      if (imcb_end < safe_end) {
        imcb_start += (imcb_end - safe_end);
        imcb_end = safe_end;
      }
    };
    if (*default_inset_bias == InsetBias::kStart) {
      adjust_end();
      adjust_start();
    } else {
      adjust_start();
      adjust_end();
    }
  }

  *inset_start_out = imcb_start + margin_start;
  *inset_end_out = imcb_end + margin_end;
}

bool CanComputeBlockSizeWithoutLayout(
    const BlockNode& node,
    WritingDirectionMode container_writing_direction,
    ItemPosition block_alignment_position,
    bool has_auto_block_inset,
    bool has_inline_size) {
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
    // Any 'auto' inset will trigger fit-content.
    if (has_auto_block_inset) {
      return false;
    }
    // Check for an explicit stretch.
    if (block_alignment_position == ItemPosition::kStretch) {
      return true;
    }
    // Non-normal alignment will trigger fit-content.
    if (block_alignment_position != ItemPosition::kNormal) {
      return false;
    }
    // An aspect-ratio (with a definite inline-size) will trigger fit-content.
    if (!style.AspectRatio().IsAuto() && has_inline_size) {
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
  std::optional<PositionAreaOffsets> offsets = style.PositionAreaOffsets();
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
    bool is_containing_block_scrollable,
    WritingDirectionMode container_writing_direction,
    WritingDirectionMode self_writing_direction) {
  StyleSelfAlignmentData align_normal_behavior(ItemPosition::kNormal,
                                               OverflowAlignment::kDefault);
  StyleSelfAlignmentData justify_normal_behavior(ItemPosition::kNormal,
                                                 OverflowAlignment::kDefault);
  const PositionArea position_area = style.GetPositionArea().ToPhysical(
      container_writing_direction, self_writing_direction);
  if (!position_area.IsNone()) {
    std::tie(align_normal_behavior, justify_normal_behavior) =
        position_area.AlignJustifySelfFromPhysical(
            container_writing_direction, is_containing_block_scrollable);
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
    WritingDirectionMode container_writing_direction,
    WritingDirectionMode self_writing_direction) {
  InsetModifiedContainingBlock imcb = ComputeUnclampedIMCB(
      available_size, alignment, insets, static_position, node.Style(),
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
    const LogicalAnchorCenterPosition& anchor_center_position,
    const LogicalAlignment& alignment,
    const BoxStrut& border_padding,
    const std::optional<LogicalSize>& replaced_size,
    const BoxStrut& container_insets,
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
                                       imcb.has_auto_block_inset,
                                       /* has_inline_size */ false);

  auto MinMaxSizesFunc = [&](SizeType type) -> MinMaxSizesResult {
    DCHECK(!node.IsReplaced());

    // Mark the inline calculations as being dependent on min/max sizes.
    depends_on_min_max_sizes = true;

    // If we can't compute our block-size without layout, we can use the
    // provided space to determine our min/max sizes.
    if (!can_compute_block_size_without_layout)
      return node.ComputeMinMaxSizes(style.GetWritingMode(), type, space);

    // Compute our block-size if we haven't already.
    if (dimensions->size.block_size == kIndefiniteSize) {
      ComputeOofBlockDimensions(
          node, style, space, imcb, anchor_center_position, alignment,
          border_padding,
          /* replaced_size */ std::nullopt, container_insets,
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
        return is_explicit_stretch ? Length::Stretch() : Length::FitContent();
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
      return is_stretch ? Length::Stretch() : Length::FitContent();
    })();

    const LayoutUnit main_inline_size = ResolveMainInlineLength(
        space, style, border_padding, MinMaxSizesFunc, main_inline_length,
        &auto_length, imcb.InlineSize());
    const MinMaxSizes min_max_inline_sizes = ComputeMinMaxInlineSizes(
        space, node, border_padding,
        apply_automatic_min_size ? &Length::MinIntrinsic() : nullptr,
        MinMaxSizesFunc, TransferredSizesMode::kNormal, imcb.InlineSize());

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

  const bool applied_auto_margins = ComputeMargins(
      space.MarginPaddingPercentageResolutionSize(), imcb.InlineSize(),
      style.MarginInlineStart(), style.MarginInlineEnd(), inline_size,
      imcb.has_auto_inline_inset, is_margin_start_dominant, is_block_direction,
      &dimensions->margins.inline_start, &dimensions->margins.inline_end);

  if (applied_auto_margins) {
    dimensions->inset.inline_start =
        imcb.inline_start + dimensions->margins.inline_start;
    dimensions->inset.inline_end =
        imcb.inline_end + dimensions->margins.inline_end;
  } else {
    ComputeInsets(
        space.AvailableSize().inline_size, container_insets.inline_start,
        container_insets.inline_end, imcb.inline_start, imcb.inline_end,
        imcb.inline_inset_bias, imcb.inline_safe_inset_bias,
        imcb.inline_default_inset_bias, dimensions->margins.inline_start,
        dimensions->margins.inline_end, inline_size,
        anchor_center_position.inline_offset, &dimensions->inset.inline_start,
        &dimensions->inset.inline_end);
  }

  return depends_on_min_max_sizes;
}

const LayoutResult* ComputeOofBlockDimensions(
    const BlockNode& node,
    const ComputedStyle& style,
    const ConstraintSpace& space,
    const InsetModifiedContainingBlock& imcb,
    const LogicalAnchorCenterPosition& anchor_center_position,
    const LogicalAlignment& alignment,
    const BoxStrut& border_padding,
    const std::optional<LogicalSize>& replaced_size,
    const BoxStrut& container_insets,
    WritingDirectionMode container_writing_direction,
    LogicalOofDimensions* dimensions) {
  DCHECK(dimensions);
  DCHECK_GE(imcb.BlockSize(), LayoutUnit());

  const auto alignment_position = alignment.block_alignment.GetPosition();

  const LayoutResult* result = nullptr;
  LayoutUnit block_size;
  if (replaced_size) {
    DCHECK(node.IsReplaced());
    block_size = replaced_size->block_size;
  } else if (CanComputeBlockSizeWithoutLayout(
                 node, container_writing_direction, alignment_position,
                 imcb.has_auto_block_inset,
                 /* has_inline_size */ dimensions->size.inline_size !=
                     kIndefiniteSize)) {
    DCHECK(!node.IsTable());

    // Nothing depends on our intrinsic-size, so we can safely use the initial
    // variant of these functions.
    const LayoutUnit main_block_size = ResolveMainBlockLength(
        space, style, border_padding, style.LogicalHeight(), &Length::Stretch(),
        kIndefiniteSize, imcb.BlockSize());
    const MinMaxSizes min_max_block_sizes =
        ComputeInitialMinMaxBlockSizes(space, node, border_padding);
    block_size = min_max_block_sizes.ClampSizeToMinAndMax(main_block_size);
  } else {
    DCHECK_NE(dimensions->size.inline_size, kIndefiniteSize);

    // Create a new space, setting the fixed inline-size.
    ConstraintSpaceBuilder builder(style.GetWritingMode(),
                                   style.GetWritingDirection(),
                                   /* is_new_fc */ true);
    builder.SetAvailableSize({dimensions->size.inline_size, imcb.BlockSize()});
    builder.SetIsFixedInlineSize(true);
    builder.SetPercentageResolutionSize(space.PercentageResolutionSize());

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
          space, node, /*fragmentainer_offset_delta=*/LayoutUnit(),
          space.FragmentainerBlockSize(),
          /*requires_content_before_breaking=*/false, &builder);
    }

    result = node.Layout(builder.ToConstraintSpace());
    block_size = LogicalFragment(style.GetWritingDirection(),
                                 result->GetPhysicalFragment())
                     .BlockSize();
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

  const bool applied_auto_margins = ComputeMargins(
      space.MarginPaddingPercentageResolutionSize(), imcb.BlockSize(),
      style.MarginBlockStart(), style.MarginBlockEnd(), block_size,
      imcb.has_auto_block_inset, is_margin_start_dominant, is_block_direction,
      &dimensions->margins.block_start, &dimensions->margins.block_end);

  if (applied_auto_margins) {
    dimensions->inset.block_start =
        imcb.block_start + dimensions->margins.block_start;
    dimensions->inset.block_end =
        imcb.block_end + dimensions->margins.block_end;
  } else {
    ComputeInsets(space.AvailableSize().block_size,
                  container_insets.block_start, container_insets.block_end,
                  imcb.block_start, imcb.block_end, imcb.block_inset_bias,
                  imcb.block_safe_inset_bias, imcb.block_default_inset_bias,
                  dimensions->margins.block_start,
                  dimensions->margins.block_end, block_size,
                  anchor_center_position.block_offset,
                  &dimensions->inset.block_start, &dimensions->inset.block_end);
  }
  return result;
}

}  // namespace blink
