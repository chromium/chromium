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

LayoutUnit ComputeShrinkToFitSize(
    bool is_table,
    const base::Optional<MinMaxSizes>& min_max_sizes,
    LayoutUnit available_size,
    LayoutUnit computed_available_size,
    LayoutUnit margin_start,
    LayoutUnit margin_end) {
  // The available-size given to tables isn't allowed to exceed the
  // available-size of the containing-block.
  if (is_table)
    computed_available_size = std::min(computed_available_size, available_size);
  return min_max_sizes->ShrinkToFit(
      (computed_available_size - margin_start - margin_end)
          .ClampNegativeToZero());
}

// Implement the absolute size resolution algorithm.
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-width
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-height
// |min_max_sizes| can have no value if an element is replaced, and has no
// intrinsic width or height, but has an aspect ratio.
void ComputeAbsoluteSize(const LayoutUnit border_padding_size,
                         const base::Optional<MinMaxSizes>& min_max_sizes,
                         const LayoutUnit margin_percentage_resolution_size,
                         const LayoutUnit available_size,
                         const Length& margin_start_length,
                         const Length& margin_end_length,
                         const Length& inset_start_length,
                         const Length& inset_end_length,
                         const LayoutUnit min_size,
                         const LayoutUnit max_size,
                         const LayoutUnit static_position_offset,
                         StaticPositionEdge static_position_edge,
                         bool is_start_dominant,
                         bool is_block_direction,
                         bool is_table,
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
        // |<-----*---->       |
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
    size = ComputeShrinkToFitSize(is_table, min_max_sizes, available_size,
                                  computed_available_size, *margin_start,
                                  *margin_end);
    LayoutUnit margin_size = *size + *margin_start + *margin_end;
    if (is_start_dominant) {
      inset_start = StaticPositionStartInset(
          static_position_edge, static_position_offset, margin_size);
    } else {
      inset_end =
          StaticPositionEndInset(static_position_edge, static_position_offset,
                                 available_size, margin_size);
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
    size = ComputeShrinkToFitSize(is_table, min_max_sizes, available_size,
                                  computed_available_size, *margin_start,
                                  *margin_end);
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
    LayoutUnit computed_available_size = available_size - *inset_start;
    size = ComputeShrinkToFitSize(is_table, min_max_sizes, available_size,
                                  computed_available_size, *margin_start,
                                  *margin_end);
  }

  // Rules 4 through 6: 1 out of 3 are unknown.
  if (!inset_start) {
    inset_start =
        available_size - *size - *inset_end - *margin_start - *margin_end;
  } else if (!inset_end) {
    inset_end =
        available_size - *size - *inset_start - *margin_start - *margin_end;
  } else if (!size) {
    LayoutUnit computed_available_size =
        available_size - *inset_start - *inset_end;
    if (is_table) {
      size = ComputeShrinkToFitSize(is_table, min_max_sizes, available_size,
                                    computed_available_size, *margin_start,
                                    *margin_end);
    } else {
      size = computed_available_size - *margin_start - *margin_end;
    }
  }

  // If calculated |size| is outside of min/max constraints, rerun the
  // algorithm with the constrained |size|.
  LayoutUnit constrained_size = ConstrainByMinMax(*size, min_size, max_size);
  if (size != constrained_size) {
    // Because this function only changes "size" when it's not already set, it
    // is safe to recursively call ourselves here because on the second call it
    // is guaranteed to be within |min_size| and |max_size|.
    ComputeAbsoluteSize(
        border_padding_size, min_max_sizes, margin_percentage_resolution_size,
        available_size, margin_start_length, margin_end_length,
        inset_start_length, inset_end_length, min_size, max_size,
        static_position_offset, static_position_edge, is_start_dominant,
        is_block_direction, is_table, constrained_size, size_out,
        inset_start_out, inset_end_out, margin_start_out, margin_end_out);
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

bool AbsoluteNeedsChildInlineSize(const ComputedStyle& style) {
  if (style.IsDisplayTableBox())
    return true;
  return style.LogicalWidth().IsIntrinsic() ||
         style.LogicalMinWidth().IsIntrinsic() ||
         style.LogicalMaxWidth().IsIntrinsic() ||
         (style.LogicalWidth().IsAuto() &&
          (style.LogicalLeft().IsAuto() || style.LogicalRight().IsAuto()));
}

bool AbsoluteNeedsChildBlockSize(const ComputedStyle& style) {
  if (style.IsDisplayTableBox())
    return true;
  return style.LogicalHeight().IsIntrinsic() ||
         style.LogicalMinHeight().IsIntrinsic() ||
         style.LogicalMaxHeight().IsIntrinsic() ||
         (style.LogicalHeight().IsAuto() &&
          (style.LogicalTop().IsAuto() || style.LogicalBottom().IsAuto()));
}

bool IsInlineSizeComputableFromBlockSize(const ComputedStyle& style) {
  DCHECK(style.HasOutOfFlowPosition());
  if (style.AspectRatio().IsAuto())
    return false;
  // An explicit block size should take precedence over specified insets.
  bool have_inline_size =
      style.LogicalWidth().IsFixed() || style.LogicalWidth().IsPercentOrCalc();
  bool have_block_size = style.LogicalHeight().IsFixed() ||
                         style.LogicalHeight().IsPercentOrCalc();
  if (have_inline_size)
    return false;
  if (have_block_size)
    return true;
  // If we have block insets but no inline insets, we compute based on the
  // insets.
  return !AbsoluteNeedsChildBlockSize(style) &&
         AbsoluteNeedsChildInlineSize(style);
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

  auto& document = dialog.GetDocument();
  auto* scrollable_area = document.View()->LayoutViewport();
  LayoutUnit top =
      LayoutUnit((dialog.Style()->GetPosition() == EPosition::kFixed)
                     ? 0
                     : scrollable_area->ScrollOffsetInt().Height());

  if (top)
    UseCounter::Count(document, WebFeature::kDialogWithNonZeroScrollOffset);

  int visible_height = document.View()->Height();
  if (height < visible_height)
    top += (visible_height - height) / 2;
  else if (height > visible_height)
    UseCounter::Count(document, WebFeature::kDialogHeightLargerThanViewport);
  dialog_node->SetCentered(top);
  return top;
}

void ComputeOutOfFlowInlineDimensions(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const NGLogicalStaticPosition& static_position,
    const base::Optional<MinMaxSizes>& minmax_content_sizes,
    const base::Optional<MinMaxSizes>& minmax_intrinsic_sizes_for_ar,
    const base::Optional<LogicalSize>& replaced_size,
    const WritingDirectionMode container_writing_direction,
    NGLogicalOutOfFlowDimensions* dimensions) {
  DCHECK(dimensions);

  Length min_inline_length = style.LogicalMinWidth();
  base::Optional<MinMaxSizes> min_size_minmax = minmax_content_sizes;
  // We don't need to check for IsInlineSizeComputableFromBlockSize; this is
  // done by the caller.
  if (minmax_intrinsic_sizes_for_ar) {
    min_inline_length = Length::MinIntrinsic();
    min_size_minmax = minmax_intrinsic_sizes_for_ar;
  }
  LayoutUnit min_inline_size =
      ResolveMinInlineLength(space, style, border_padding, min_size_minmax,
                             min_inline_length, LengthResolvePhase::kLayout);
  LayoutUnit max_inline_size = ResolveMaxInlineLength(
      space, style, border_padding, minmax_content_sizes,
      style.LogicalMaxWidth(), LengthResolvePhase::kLayout);

  // This implements the transferred min/max sizes per
  // https://drafts.csswg.org/css-sizing-4/#aspect-ratio
  if (!style.AspectRatio().IsAuto() &&
      dimensions->size.block_size == kIndefiniteSize) {
    MinMaxSizes sizes = ComputeMinMaxInlineSizesFromAspectRatio(
        space, style, border_padding, LengthResolvePhase::kLayout);
    min_inline_size = std::max(sizes.min_size, min_inline_size);
    max_inline_size = std::min(sizes.max_size, max_inline_size);
  }

  // Tables are never allowed to go below their min-content size.
  const bool is_table = style.IsDisplayTableBox();
  if (is_table)
    min_inline_size = std::max(min_inline_size, minmax_content_sizes->min_size);

  base::Optional<LayoutUnit> inline_size;
  if (!style.LogicalWidth().IsAuto()) {
    inline_size =
        ResolveMainInlineLength(space, style, border_padding,
                                minmax_content_sizes, style.LogicalWidth());
  } else if (replaced_size.has_value()) {
    inline_size = replaced_size->inline_size;
  } else if (IsInlineSizeComputableFromBlockSize(style)) {
    DCHECK(minmax_content_sizes.has_value());
    inline_size = minmax_content_sizes->min_size;
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
      border_padding.InlineSum(), minmax_content_sizes,
      space.PercentageResolutionInlineSizeForParentWritingMode(),
      space.AvailableSize().inline_size, style.MarginStart(), style.MarginEnd(),
      style.LogicalInlineStart(), style.LogicalInlineEnd(), min_inline_size,
      max_inline_size, static_position.offset.inline_offset,
      GetStaticPositionEdge(static_position.inline_edge), is_start_dominant,
      false /* is_block_direction */, is_table, inline_size,
      &dimensions->size.inline_size, &dimensions->inset.inline_start,
      &dimensions->inset.inline_end, &dimensions->margins.inline_start,
      &dimensions->margins.inline_end);
}

void ComputeOutOfFlowBlockDimensions(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const NGLogicalStaticPosition& static_position,
    const base::Optional<LayoutUnit>& child_block_size,
    const base::Optional<LogicalSize>& replaced_size,
    const WritingDirectionMode container_writing_direction,
    NGLogicalOutOfFlowDimensions* dimensions) {
  // After partial size has been computed, child block size is either unknown,
  // or fully computed, there is no minmax. To express this, a 'fixed' minmax
  // is created where min and max are the same.
  base::Optional<MinMaxSizes> min_max_sizes;
  if (child_block_size.has_value()) {
    min_max_sizes = MinMaxSizes{*child_block_size, *child_block_size};
  }

  LayoutUnit child_block_size_or_indefinite =
      child_block_size.value_or(kIndefiniteSize);

  LayoutUnit min_block_size = ResolveMinBlockLength(
      space, style, border_padding, style.LogicalMinHeight(),
      LengthResolvePhase::kLayout);
  LayoutUnit max_block_size = ResolveMaxBlockLength(
      space, style, border_padding, style.LogicalMaxHeight(),
      LengthResolvePhase::kLayout);

  // Tables are never allowed to go below their "auto" block-size.
  const bool is_table = style.IsDisplayTableBox();
  if (is_table)
    min_block_size = std::max(min_block_size, min_max_sizes->min_size);

  base::Optional<LayoutUnit> block_size;
  if (!style.LogicalHeight().IsAuto()) {
    block_size = ResolveMainBlockLength(
        space, style, border_padding, style.LogicalHeight(),
        child_block_size_or_indefinite, LengthResolvePhase::kLayout);
  } else if (replaced_size.has_value()) {
    block_size = replaced_size->block_size;
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
      border_padding.BlockSum(), min_max_sizes,
      space.PercentageResolutionInlineSizeForParentWritingMode(),
      space.AvailableSize().block_size, style.MarginBefore(),
      style.MarginAfter(), style.LogicalTop(), style.LogicalBottom(),
      min_block_size, max_block_size, static_position.offset.block_offset,
      GetStaticPositionEdge(static_position.block_edge), is_start_dominant,
      true /* is_block_direction */, is_table, block_size,
      &dimensions->size.block_size, &dimensions->inset.block_start,
      &dimensions->inset.block_end, &dimensions->margins.block_start,
      &dimensions->margins.block_end);
}

}  // namespace blink
