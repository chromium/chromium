// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_absolute_utils.h"

#include <algorithm>
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_static_position.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

namespace {

// Tables require special handling. The width/height is always considered as
// 'auto', and the value for width/height is treated as an additional
// min-width/min-height.
bool IsTable(const ComputedStyle& style) {
  return style.Display() == EDisplay::kTable ||
         style.Display() == EDisplay::kInlineTable;
}

bool IsLogicalWidthTreatedAsAuto(const ComputedStyle& style) {
  return IsTable(style) || style.LogicalWidth().IsAuto();
}

bool IsLogicalHeightTreatAsAuto(const ComputedStyle& style) {
  return IsTable(style) || style.LogicalHeight().IsAuto();
}

// Dominant side:
// htb ltr => top left
// htb rtl => top right
// vlr ltr => top left
// vlr rtl => bottom left
// vrl ltr => top right
// vrl rtl => bottom right
bool IsLeftDominant(const WritingMode container_writing_mode,
                    const TextDirection container_direction) {
  return (container_writing_mode != WritingMode::kVerticalRl) &&
         !(container_writing_mode == WritingMode::kHorizontalTb &&
           container_direction == TextDirection::kRtl);
}

bool IsTopDominant(const WritingMode container_writing_mode,
                   const TextDirection container_direction) {
  return (container_writing_mode == WritingMode::kHorizontalTb) ||
         (container_direction != TextDirection::kRtl);
}

inline LayoutUnit StaticPositionStartInset(bool is_static_position_start,
                                           LayoutUnit static_position_offset,
                                           LayoutUnit size) {
  return is_static_position_start ? static_position_offset
                                  : static_position_offset - size;
}

inline LayoutUnit StaticPositionEndInset(bool is_static_position_start,
                                         LayoutUnit static_position_offset,
                                         LayoutUnit available_size,
                                         LayoutUnit size) {
  return available_size - static_position_offset -
         (is_static_position_start ? size : LayoutUnit());
}

LayoutUnit ComputeShrinkToFitSize(
    const base::Optional<MinMaxSize>& child_minmax,
    LayoutUnit computed_available_size,
    LayoutUnit margin_start,
    LayoutUnit margin_end) {
  return child_minmax->ShrinkToFit(
      (computed_available_size - margin_start - margin_end)
          .ClampNegativeToZero());
}

// Implement the absolute size resolution algorithm.
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-width
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-height
// |child_minmax| can have no value if an element is replaced, and has no
// intrinsic width or height, but has an aspect ratio.
void ComputeAbsoluteSize(const LayoutUnit border_padding_size,
                         const base::Optional<MinMaxSize>& child_minmax,
                         const LayoutUnit margin_percentage_resolution_size,
                         const LayoutUnit available_size,
                         const Length& margin_start_length,
                         const Length& margin_end_length,
                         const Length& inset_start_length,
                         const Length& inset_end_length,
                         const LayoutUnit min_size,
                         const LayoutUnit max_size,
                         const LayoutUnit static_position_offset,
                         bool is_static_position_start,
                         bool is_start_dominant,
                         bool is_block_direction,
                         base::Optional<LayoutUnit> size,
                         LayoutUnit* size_out,
                         LayoutUnit* inset_start_out,
                         LayoutUnit* inset_end_out,
                         LayoutUnit* margin_start_out,
                         LayoutUnit* margin_end_out) {
  DCHECK_NE(available_size, kIndefiniteSize);

  base::Optional<LayoutUnit> margin_start;
  if (!margin_start_length.IsAuto()) {
    margin_start = MinimumValueForLength(margin_start_length,
                                         margin_percentage_resolution_size);
  }
  base::Optional<LayoutUnit> margin_end;
  if (!margin_end_length.IsAuto()) {
    margin_end = MinimumValueForLength(margin_end_length,
                                       margin_percentage_resolution_size);
  }
  base::Optional<LayoutUnit> inset_start;
  if (!inset_start_length.IsAuto()) {
    inset_start = MinimumValueForLength(inset_start_length, available_size);
  }
  base::Optional<LayoutUnit> inset_end;
  if (!inset_end_length.IsAuto()) {
    inset_end = MinimumValueForLength(inset_end_length, available_size);
  }

  // Solving the equation:
  // |inset_start| + |margin_start| + |size| + |margin_end| + |inset_end| =
  // |available_size|
  if (!inset_start && !inset_end && !size) {
    // "If all three of left, width, and right are auto:"
    if (!margin_start)
      margin_start = LayoutUnit();
    if (!margin_end)
      margin_end = LayoutUnit();

    LayoutUnit computed_available_size =
        is_static_position_start ? available_size - static_position_offset
                                 : static_position_offset;
    size = ComputeShrinkToFitSize(child_minmax, computed_available_size,
                                  *margin_start, *margin_end);
    LayoutUnit margin_size = *size + *margin_start + *margin_end;
    if (is_start_dominant) {
      inset_start = StaticPositionStartInset(
          is_static_position_start, static_position_offset, margin_size);
    } else {
      inset_end = StaticPositionEndInset(is_static_position_start,
                                         static_position_offset, available_size,
                                         margin_size);
    }
  } else if (inset_start && inset_end && size) {
    // "If left, right, and width are not auto:"
    // Compute margins.
    LayoutUnit margin_space =
        available_size - *inset_start - *inset_end - *size;

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
    // Rule 1: left/width are unknown.
    DCHECK(inset_end.has_value());
    LayoutUnit computed_available_size = available_size - *inset_end;
    size = ComputeShrinkToFitSize(child_minmax, computed_available_size,
                                  *margin_start, *margin_end);
  } else if (!inset_start && !inset_end) {
    // Rule 2.
    DCHECK(size.has_value());
    LayoutUnit margin_size = *size + *margin_start + *margin_end;
    if (is_start_dominant) {
      inset_start = StaticPositionStartInset(
          is_static_position_start, static_position_offset, margin_size);
    } else {
      inset_end = StaticPositionEndInset(is_static_position_start,
                                         static_position_offset, available_size,
                                         margin_size);
    }
  } else if (!size && !inset_end) {
    // Rule 3.
    LayoutUnit computed_available_size = available_size - *inset_start;
    size = ComputeShrinkToFitSize(child_minmax, computed_available_size,
                                  *margin_start, *margin_end);
  }

  // Rules 4 through 6: 1 out of 3 are unknown.
  if (!inset_start) {
    inset_start =
        available_size - *size - *inset_end - *margin_start - *margin_end;
  } else if (!inset_end) {
    inset_end =
        available_size - *size - *inset_start - *margin_start - *margin_end;
  } else if (!size) {
    size = available_size - *inset_start - *inset_end - *margin_start -
           *margin_end;
  }

  // If calculated |size| is outside of min/max constraints, rerun the
  // algorithm with the constrained |size|.
  LayoutUnit constrained_size = ConstrainByMinMax(*size, min_size, max_size);
  if (size != constrained_size) {
    // Because this function only changes "size" when it's not already set, it
    // is safe to recursively call ourselves here because on the second call it
    // is guaranteed to be within |min_size| and |max_size|.
    ComputeAbsoluteSize(
        border_padding_size, child_minmax, margin_percentage_resolution_size,
        available_size, margin_start_length, margin_end_length,
        inset_start_length, inset_end_length, min_size, max_size,
        static_position_offset, is_static_position_start, is_start_dominant,
        is_block_direction, constrained_size, size_out, inset_start_out,
        inset_end_out, margin_start_out, margin_end_out);
    return;
  }

  // Negative sizes are not allowed.
  *size_out = std::max(*size, border_padding_size);
  *inset_start_out = *inset_start + *margin_start;
  *inset_end_out = *inset_end + *margin_end;
  *margin_start_out = *margin_start;
  *margin_end_out = *margin_end;
}

}  // namespace

bool AbsoluteNeedsChildInlineSize(const ComputedStyle& style) {
  bool is_logical_width_intrinsic =
      !IsTable(style) && style.LogicalWidth().IsIntrinsic();
  return is_logical_width_intrinsic || style.LogicalMinWidth().IsIntrinsic() ||
         style.LogicalMaxWidth().IsIntrinsic() ||
         (IsLogicalWidthTreatedAsAuto(style) &&
          (style.LogicalLeft().IsAuto() || style.LogicalRight().IsAuto()));
}

bool AbsoluteNeedsChildBlockSize(const ComputedStyle& style) {
  bool is_logical_height_intrinsic =
      !IsTable(style) && style.LogicalHeight().IsIntrinsic();
  return is_logical_height_intrinsic ||
         style.LogicalMinHeight().IsIntrinsic() ||
         style.LogicalMaxHeight().IsIntrinsic() ||
         (IsLogicalHeightTreatAsAuto(style) &&
          (style.LogicalTop().IsAuto() || style.LogicalBottom().IsAuto()));
}

base::Optional<LayoutUnit> ComputeAbsoluteDialogYPosition(
    const LayoutObject& dialog,
    LayoutUnit height) {
  auto* dialog_node = DynamicTo<HTMLDialogElement>(dialog.GetNode());
  if (!dialog_node)
    return base::nullopt;

  // This code implements <dialog> static-position spec.
  //
  // https://html.spec.whatwg.org/C/#the-dialog-element
  if (dialog_node->GetCenteringMode() == HTMLDialogElement::kNotCentered)
    return base::nullopt;

  bool can_center_dialog =
      (dialog.Style()->GetPosition() == EPosition::kAbsolute ||
       dialog.Style()->GetPosition() == EPosition::kFixed) &&
      dialog.Style()->HasAutoTopAndBottom();

  if (dialog_node->GetCenteringMode() == HTMLDialogElement::kCentered) {
    if (can_center_dialog)
      return dialog_node->CenteredPosition();
    return base::nullopt;
  }

  DCHECK_EQ(dialog_node->GetCenteringMode(),
            HTMLDialogElement::kNeedsCentering);
  if (!can_center_dialog) {
    dialog_node->SetNotCentered();
    return base::nullopt;
  }

  auto* scrollable_area = dialog.GetDocument().View()->LayoutViewport();
  LayoutUnit top =
      LayoutUnit((dialog.Style()->GetPosition() == EPosition::kFixed)
                     ? 0
                     : scrollable_area->ScrollOffsetInt().Height());

  int visible_height = dialog.GetDocument().View()->Height();
  if (height < visible_height)
    top += (visible_height - height) / 2;
  dialog_node->SetCentered(top);
  return top;
}

NGLogicalOutOfFlowPosition ComputePartialAbsoluteWithChildInlineSize(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const NGLogicalStaticPosition& static_position,
    const base::Optional<MinMaxSize>& child_minmax,
    const base::Optional<LogicalSize>& replaced_size,
    const WritingMode container_writing_mode,
    const TextDirection container_direction) {
  NGLogicalOutOfFlowPosition position;

  base::Optional<LayoutUnit> inline_size;
  if (!IsLogicalWidthTreatedAsAuto(style)) {
    inline_size = ResolveMainInlineLength(space, style, border_padding,
                                          child_minmax, style.LogicalWidth());
  } else if (replaced_size.has_value()) {
    inline_size = replaced_size->inline_size;
  }

  LayoutUnit min_inline_size = ResolveMinInlineLength(
      space, style, border_padding, child_minmax, style.LogicalMinWidth(),
      LengthResolvePhase::kLayout);
  LayoutUnit max_inline_size = ResolveMaxInlineLength(
      space, style, border_padding, child_minmax, style.LogicalMaxWidth(),
      LengthResolvePhase::kLayout);

  // Tables use the inline-size as a minimum.
  if (IsTable(style) && !style.LogicalWidth().IsAuto()) {
    min_inline_size =
        std::max(min_inline_size,
                 ResolveMainInlineLength(space, style, border_padding,
                                         child_minmax, style.LogicalWidth()));
  }

  bool is_start_dominant;
  if (style.GetWritingMode() == WritingMode::kHorizontalTb) {
    is_start_dominant =
        IsLeftDominant(container_writing_mode, container_direction) ==
        IsLeftDominant(style.GetWritingMode(), style.Direction());
  } else {
    is_start_dominant =
        IsTopDominant(container_writing_mode, container_direction) ==
        IsTopDominant(style.GetWritingMode(), style.Direction());
  }

  ComputeAbsoluteSize(
      border_padding.InlineSum(), child_minmax,
      space.PercentageResolutionInlineSizeForParentWritingMode(),
      space.AvailableSize().inline_size, style.MarginStart(), style.MarginEnd(),
      style.LogicalInlineStart(), style.LogicalInlineEnd(), min_inline_size,
      max_inline_size, static_position.offset.inline_offset,
      static_position.inline_edge ==
          NGLogicalStaticPosition::InlineEdge::kInlineStart,
      is_start_dominant, false /* is_block_direction */, inline_size,
      &position.size.inline_size, &position.inset.inline_start,
      &position.inset.inline_end, &position.margins.inline_start,
      &position.margins.inline_end);

  return position;
}

void ComputeFullAbsoluteWithChildBlockSize(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const NGLogicalStaticPosition& static_position,
    const base::Optional<LayoutUnit>& child_block_size,
    const base::Optional<LogicalSize>& replaced_size,
    const WritingMode container_writing_mode,
    const TextDirection container_direction,
    NGLogicalOutOfFlowPosition* position) {
  // After partial size has been computed, child block size is either unknown,
  // or fully computed, there is no minmax. To express this, a 'fixed' minmax
  // is created where min and max are the same.
  base::Optional<MinMaxSize> child_minmax;
  if (child_block_size.has_value()) {
    child_minmax = MinMaxSize{*child_block_size, *child_block_size};
  }

  LayoutUnit child_block_size_or_indefinite =
      child_block_size.value_or(kIndefiniteSize);

  base::Optional<LayoutUnit> block_size;
  if (!IsLogicalHeightTreatAsAuto(style)) {
    block_size = ResolveMainBlockLength(
        space, style, border_padding, style.LogicalHeight(),
        child_block_size_or_indefinite, LengthResolvePhase::kLayout);
  } else if (replaced_size.has_value()) {
    block_size = replaced_size->block_size;
  }

  LayoutUnit min_block_size = ResolveMinBlockLength(
      space, style, border_padding, style.LogicalMinHeight(),
      child_block_size_or_indefinite, LengthResolvePhase::kLayout);
  LayoutUnit max_block_size = ResolveMaxBlockLength(
      space, style, border_padding, style.LogicalMaxHeight(),
      child_block_size_or_indefinite, LengthResolvePhase::kLayout);

  bool is_start_dominant;
  if (style.GetWritingMode() == WritingMode::kHorizontalTb) {
    is_start_dominant =
        IsTopDominant(container_writing_mode, container_direction) ==
        IsTopDominant(style.GetWritingMode(), style.Direction());
  } else {
    is_start_dominant =
        IsLeftDominant(container_writing_mode, container_direction) ==
        IsLeftDominant(style.GetWritingMode(), style.Direction());
  }

  ComputeAbsoluteSize(
      border_padding.BlockSum(), child_minmax,
      space.PercentageResolutionInlineSizeForParentWritingMode(),
      space.AvailableSize().block_size, style.MarginBefore(),
      style.MarginAfter(), style.LogicalTop(), style.LogicalBottom(),
      min_block_size, max_block_size, static_position.offset.block_offset,
      static_position.block_edge ==
          NGLogicalStaticPosition::BlockEdge::kBlockStart,
      is_start_dominant, true /* is_block_direction */, block_size,
      &position->size.block_size, &position->inset.block_start,
      &position->inset.block_end, &position->margins.block_start,
      &position->margins.block_end);
}

}  // namespace blink
