// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_absolute_utils.h"

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

bool AbsoluteHorizontalNeedsEstimate(const ComputedStyle& style) {
  Length width = style.Width();
  return width.IsIntrinsic() || style.MinWidth().IsIntrinsic() ||
         style.MaxWidth().IsIntrinsic() ||
         (width.IsAuto() && (style.Left().IsAuto() || style.Right().IsAuto()));
}

bool AbsoluteVerticalNeedsEstimate(const ComputedStyle& style) {
  Length height = style.Height();
  return height.IsIntrinsic() || style.MinHeight().IsIntrinsic() ||
         style.MaxHeight().IsIntrinsic() ||
         (height.IsAuto() && (style.Top().IsAuto() || style.Bottom().IsAuto()));
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

LayoutUnit ResolveWidth(const Length& width,
                        const NGConstraintSpace& space,
                        const ComputedStyle& style,
                        const base::Optional<MinMaxSize>& child_minmax,
                        LengthResolveType type) {
  if (space.GetWritingMode() == WritingMode::kHorizontalTb) {
    return ResolveInlineLength(space, style, child_minmax, width, type,
                               LengthResolvePhase::kLayout);
  }
  LayoutUnit computed_width =
      child_minmax.has_value() ? child_minmax->max_size : LayoutUnit();
  return ResolveBlockLength(space, style, width, computed_width, type,
                            LengthResolvePhase::kLayout);
}

LayoutUnit ResolveHeight(const Length& height,
                         const NGConstraintSpace& space,
                         const ComputedStyle& style,
                         const base::Optional<MinMaxSize>& child_minmax,
                         LengthResolveType type) {
  if (space.GetWritingMode() != WritingMode::kHorizontalTb) {
    return ResolveInlineLength(space, style, child_minmax, height, type,
                               LengthResolvePhase::kLayout);
  }
  LayoutUnit computed_height =
      child_minmax.has_value() ? child_minmax->max_size : LayoutUnit();
  return ResolveBlockLength(space, style, height, computed_height, type,
                            LengthResolvePhase::kLayout);
}

// Available size can is maximum length Element can have without overflowing
// container bounds. The position of Element's edges will determine
// how much space there is available.
LayoutUnit ComputeAvailableWidth(LayoutUnit container_width,
                                 const base::Optional<LayoutUnit>& left,
                                 const base::Optional<LayoutUnit>& right,
                                 const base::Optional<LayoutUnit>& margin_left,
                                 const base::Optional<LayoutUnit>& margin_right,
                                 const NGStaticPosition& static_position) {
  LayoutUnit available_width = container_width;
  DCHECK(!left || !right);
  if (!left && !right) {
    if (static_position.HasLeft())
      available_width -= static_position.Left();
    else
      available_width = static_position.Right();
  } else if (!right) {
    available_width -= *left;
  } else {  // !left
    available_width -= *right;
  }
  LayoutUnit margins = (margin_left ? margin_left.value() : LayoutUnit()) +
                       (margin_right ? margin_right.value() : LayoutUnit());
  return (available_width - margins).ClampNegativeToZero();
}

LayoutUnit ComputeAvailableHeight(
    LayoutUnit container_height,
    const base::Optional<LayoutUnit>& top,
    const base::Optional<LayoutUnit>& bottom,
    const base::Optional<LayoutUnit>& margin_top,
    const base::Optional<LayoutUnit>& margin_bottom,
    const NGStaticPosition& static_position) {
  LayoutUnit available_height = container_height;
  DCHECK(!top || !bottom);
  if (!top && !bottom) {
    if (static_position.HasTop())
      available_height -= static_position.Top();
    else
      available_height = static_position.Bottom();
  } else if (!bottom) {
    available_height -= *top;
  } else {  // !top
    available_height -= *bottom;
  }
  LayoutUnit margins = (margin_top ? margin_top.value() : LayoutUnit()) +
                       (margin_bottom ? margin_bottom.value() : LayoutUnit());
  return (available_height - margins).ClampNegativeToZero();
}

LayoutUnit HorizontalBorderPadding(const NGConstraintSpace& space,
                                   const ComputedStyle& style) {
  return ResolveMarginPaddingLength(space, style.PaddingLeft()) +
         ResolveMarginPaddingLength(space, style.PaddingRight()) +
         LayoutUnit(style.BorderLeftWidth()) +
         LayoutUnit(style.BorderRightWidth());
}

LayoutUnit VerticalBorderPadding(const NGConstraintSpace& space,
                                 const ComputedStyle& style) {
  return ResolveMarginPaddingLength(space, style.PaddingTop()) +
         ResolveMarginPaddingLength(space, style.PaddingBottom()) +
         LayoutUnit(style.BorderTopWidth()) +
         LayoutUnit(style.BorderBottomWidth());
}

// Implement absolute horizontal size resolution algorithm.
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-width
void ComputeAbsoluteHorizontal(const NGConstraintSpace& space,
                               const ComputedStyle& style,
                               const base::Optional<LayoutUnit>& incoming_width,
                               const NGStaticPosition& static_position,
                               const base::Optional<MinMaxSize>& child_minmax,
                               const WritingMode container_writing_mode,
                               const TextDirection container_direction,
                               NGAbsolutePhysicalPosition* position) {
  NGPhysicalSize percentage_physical =
      space.PercentageResolutionSize().ConvertToPhysical(
          space.GetWritingMode());
  base::Optional<LayoutUnit> margin_left;
  if (!style.MarginLeft().IsAuto())
    margin_left = ResolveMarginPaddingLength(space, style.MarginLeft());
  base::Optional<LayoutUnit> margin_right;
  if (!style.MarginRight().IsAuto())
    margin_right = ResolveMarginPaddingLength(space, style.MarginRight());
  base::Optional<LayoutUnit> left;
  if (!style.Left().IsAuto())
    left = ValueForLength(style.Left(), percentage_physical.width);
  base::Optional<LayoutUnit> right;
  if (!style.Right().IsAuto())
    right = ValueForLength(style.Right(), percentage_physical.width);
  base::Optional<LayoutUnit> width = incoming_width;
  NGPhysicalSize container_size =
      space.AvailableSize().ConvertToPhysical(space.GetWritingMode());
  DCHECK_NE(container_size.width, NGSizeIndefinite);

  // Solving the equation:
  // left + marginLeft + width + marginRight + right  = container width
  if (!left && !right && !width) {
    // Standard: "If all three of left, width, and right are auto:"
    if (!margin_left)
      margin_left = LayoutUnit();
    if (!margin_right)
      margin_right = LayoutUnit();
    DCHECK(child_minmax.has_value());

    width = child_minmax->ShrinkToFit(
        ComputeAvailableWidth(container_size.width, left, right, margin_left,
                              margin_right, static_position));
    if (IsLeftDominant(container_writing_mode, container_direction)) {
      left = static_position.LeftInset(container_size.width, *width,
                                       *margin_left, *margin_right);
    } else {
      right = static_position.RightInset(container_size.width, *width,
                                         *margin_left, *margin_right);
    }
  } else if (left && right && width) {
    // Standard: "If left, right, and width are not auto:"
    // Compute margins.
    LayoutUnit margin_space = container_size.width - *left - *right - *width;
    // When both margins are auto.
    if (!margin_left && !margin_right) {
      if (margin_space > 0) {
        margin_left = margin_space / 2;
        margin_right = margin_space / 2;
      } else {
        // Margins are negative.
        if (IsLeftDominant(container_writing_mode, container_direction)) {
          margin_left = LayoutUnit();
          margin_right = margin_space;
        } else {
          margin_right = LayoutUnit();
          margin_left = margin_space;
        }
      }
    } else if (!margin_left) {
      margin_left = margin_space - *margin_right;
    } else if (!margin_right) {
      margin_right = margin_space - *margin_left;
    } else {
      // Are values overconstrained?
      LayoutUnit margin_extra = margin_space - *margin_left - *margin_right;
      if (margin_extra) {
        // Relax the end.
        if (IsLeftDominant(container_writing_mode, container_direction))
          right = *right + margin_extra;
        else
          left = *left + margin_extra;
      }
    }
  }

  // Set unknown margins.
  if (!margin_left)
    margin_left = LayoutUnit();
  if (!margin_right)
    margin_right = LayoutUnit();

  // Rules 1 through 3, 2 out of 3 are unknown.
  if (!left && !width) {
    // Rule 1: left/width are unknown.
    DCHECK(right.has_value());
    DCHECK(child_minmax.has_value());
    width = child_minmax->ShrinkToFit(
        ComputeAvailableWidth(container_size.width, left, right, margin_left,
                              margin_right, static_position));
  } else if (!left && !right) {
    // Rule 2.
    DCHECK(width.has_value());
    if (IsLeftDominant(container_writing_mode, container_direction))
      left = static_position.LeftInset(container_size.width, *width,
                                       *margin_left, *margin_right);
    else
      right = static_position.RightInset(container_size.width, *width,
                                         *margin_left, *margin_right);
  } else if (!width && !right) {
    // Rule 3.
    DCHECK(child_minmax.has_value());
    width = child_minmax->ShrinkToFit(
        ComputeAvailableWidth(container_size.width, left, right, margin_left,
                              margin_right, static_position));
  }

  // Rules 4 through 6, 1 out of 3 are unknown.
  if (!left) {
    left =
        container_size.width - *width - *right - *margin_left - *margin_right;
  } else if (!right) {
    right =
        container_size.width - *width - *left - *margin_left - *margin_right;
  } else if (!width) {
    width =
        container_size.width - *left - *right - *margin_left - *margin_right;
  }

  // The DCHECK is useful, but only holds true when not saturated.
  if (!(left->MightBeSaturated() || right->MightBeSaturated() ||
        width->MightBeSaturated() || margin_left->MightBeSaturated() ||
        margin_right->MightBeSaturated()))
    DCHECK_EQ(container_size.width,
              *left + *right + *margin_left + *margin_right + *width);

  // If calculated width is outside of min/max constraints,
  // rerun the algorithm with constrained width.
  LayoutUnit min = ResolveWidth(style.MinWidth(), space, style, child_minmax,
                                LengthResolveType::kMinSize);
  LayoutUnit max = ResolveWidth(style.MaxWidth(), space, style, child_minmax,
                                LengthResolveType::kMaxSize);
  if (width != ConstrainByMinMax(*width, min, max)) {
    width = ConstrainByMinMax(*width, min, max);
    // Because this function only changes "width" when it's not already
    // set, it is safe to recursively call ourselves here because on the
    // second call it is guaranteed to be within min..max.
    ComputeAbsoluteHorizontal(space, style, width, static_position,
                              child_minmax, container_writing_mode,
                              container_direction, position);
    return;
  }

  // Negative widths are not allowed.
  width = std::max(*width, HorizontalBorderPadding(space, style));

  position->inset.left = *left + *margin_left;
  position->inset.right = *right + *margin_right;
  position->margins.left = *margin_left;
  position->margins.right = *margin_right;
  position->size.width = *width;
}

// Implements absolute vertical size resolution algorithm.
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-height
void ComputeAbsoluteVertical(const NGConstraintSpace& space,
                             const ComputedStyle& style,
                             const base::Optional<LayoutUnit>& incoming_height,
                             const NGStaticPosition& static_position,
                             const base::Optional<MinMaxSize>& child_minmax,
                             const WritingMode container_writing_mode,
                             const TextDirection container_direction,
                             NGAbsolutePhysicalPosition* position) {
  NGPhysicalSize percentage_physical =
      space.PercentageResolutionSize().ConvertToPhysical(
          space.GetWritingMode());

  base::Optional<LayoutUnit> margin_top;
  if (!style.MarginTop().IsAuto())
    margin_top = ResolveMarginPaddingLength(space, style.MarginTop());
  base::Optional<LayoutUnit> margin_bottom;
  if (!style.MarginBottom().IsAuto())
    margin_bottom = ResolveMarginPaddingLength(space, style.MarginBottom());
  base::Optional<LayoutUnit> top;
  if (!style.Top().IsAuto())
    top = ValueForLength(style.Top(), percentage_physical.height);
  base::Optional<LayoutUnit> bottom;
  if (!style.Bottom().IsAuto())
    bottom = ValueForLength(style.Bottom(), percentage_physical.height);
  LayoutUnit border_padding = VerticalBorderPadding(space, style);
  base::Optional<LayoutUnit> height = incoming_height;

  NGPhysicalSize container_size =
      space.AvailableSize().ConvertToPhysical(space.GetWritingMode());
  DCHECK_NE(container_size.height, NGSizeIndefinite);

  // Solving the equation:
  // top + marginTop + height + marginBottom + bottom
  // + border_padding = container height
  if (!top && !bottom && !height) {
    // Standard: "If all three of top, height, and bottom are auto:"
    if (!margin_top)
      margin_top = LayoutUnit();
    if (!margin_bottom)
      margin_bottom = LayoutUnit();
    DCHECK(child_minmax.has_value());
    height = child_minmax->ShrinkToFit(
        ComputeAvailableHeight(container_size.height, top, bottom, margin_top,
                               margin_bottom, static_position));
    if (IsTopDominant(container_writing_mode, container_direction)) {
      top = static_position.TopInset(container_size.height, *height,
                                     *margin_top, *margin_bottom);
    } else {
      bottom = static_position.BottomInset(container_size.height, *height,
                                           *margin_top, *margin_bottom);
    }
  } else if (top && bottom && height) {
    // Standard: "If top, bottom, and height are not auto:"
    // Compute margins.
    LayoutUnit margin_space = container_size.height - *top - *bottom - *height;
    if (!margin_top && !margin_bottom) {
      // When both margins are auto.
      margin_top = margin_space / 2;
      margin_bottom = margin_space - *margin_top;
    } else if (!margin_top) {
      margin_top = margin_space - *margin_bottom;
    } else if (!margin_bottom) {
      margin_bottom = margin_space - *margin_top;
    } else {
      // Since none of the margins are auto (and we have non-auto top, bottom
      // and height), we are over-constrained. Keep the dominant inset and
      // override the other.
      LayoutUnit margin_extra = margin_space - *margin_top - *margin_bottom;
      if (margin_extra) {
        if (IsTopDominant(container_writing_mode, container_direction))
          bottom = *bottom + margin_extra;
        else
          top = *top + margin_extra;
      }
    }
  }

  // Set unknown margins.
  if (!margin_top)
    margin_top = LayoutUnit();
  if (!margin_bottom)
    margin_bottom = LayoutUnit();

  // Rules 1 through 3, 2 out of 3 are unknown, fix 1.
  if (!top && !height) {
    // Rule 1.
    DCHECK(bottom.has_value());
    DCHECK(child_minmax.has_value());
    height = child_minmax->ShrinkToFit(
        ComputeAvailableHeight(container_size.height, top, bottom, margin_top,
                               margin_bottom, static_position));
  } else if (!top && !bottom) {
    // Rule 2.
    DCHECK(height.has_value());
    if (IsTopDominant(container_writing_mode, container_direction)) {
      top = static_position.TopInset(container_size.height, *height,
                                     *margin_top, *margin_bottom);
    } else {
      bottom = static_position.BottomInset(container_size.height, *height,
                                           *margin_top, *margin_bottom);
    }
  } else if (!height && !bottom) {
    // Rule 3.
    DCHECK(child_minmax.has_value());
    height = child_minmax->ShrinkToFit(
        ComputeAvailableHeight(container_size.height, top, bottom, margin_top,
                               margin_bottom, static_position));
  }

  // Rules 4 through 6, 1 out of 3 are unknown.
  if (!top) {
    top = container_size.height - *height - *bottom - *margin_top -
          *margin_bottom;
  } else if (!bottom) {
    bottom =
        container_size.height - *height - *top - *margin_top - *margin_bottom;
  } else if (!height) {
    height =
        container_size.height - *top - *bottom - *margin_top - *margin_bottom;
  }
  // The DCHECK is useful, but only holds true when not saturated.
  if (!(top->MightBeSaturated() || bottom->MightBeSaturated() ||
        height->MightBeSaturated() || margin_top->MightBeSaturated() ||
        margin_bottom->MightBeSaturated())) {
    DCHECK_EQ(container_size.height,
              *top + *bottom + *margin_top + *margin_bottom + *height);
  }
  // If calculated height is outside of min/max constraints,
  // rerun the algorithm with constrained width.
  LayoutUnit min = ResolveHeight(style.MinHeight(), space, style, child_minmax,
                                 LengthResolveType::kMinSize);
  LayoutUnit max = ResolveHeight(style.MaxHeight(), space, style, child_minmax,
                                 LengthResolveType::kMaxSize);
  if (height != ConstrainByMinMax(*height, min, max)) {
    height = ConstrainByMinMax(*height, min, max);
    // Because this function only changes "height" when it's not already
    // set, it is safe to recursively call ourselves here because on the
    // second call it is guaranteed to be within min..max.
    ComputeAbsoluteVertical(space, style, height, static_position, child_minmax,
                            container_writing_mode, container_direction,
                            position);
    return;
  }
  // Negative heights are not allowed.
  height = std::max(*height, border_padding);

  position->inset.top = *top + *margin_top;
  position->inset.bottom = *bottom + *margin_bottom;
  position->margins.top = *margin_top;
  position->margins.bottom = *margin_bottom;
  position->size.height = *height;
}

}  // namespace

String NGAbsolutePhysicalPosition::ToString() const {
  return String::Format("INSET(LRTB):%d,%d,%d,%d SIZE:%dx%d",
                        inset.left.ToInt(), inset.right.ToInt(),
                        inset.top.ToInt(), inset.bottom.ToInt(),
                        size.width.ToInt(), size.height.ToInt());
}

bool AbsoluteNeedsChildBlockSize(const ComputedStyle& style) {
  if (style.IsHorizontalWritingMode())
    return AbsoluteVerticalNeedsEstimate(style);
  else
    return AbsoluteHorizontalNeedsEstimate(style);
}

bool AbsoluteNeedsChildInlineSize(const ComputedStyle& style) {
  if (style.IsHorizontalWritingMode())
    return AbsoluteHorizontalNeedsEstimate(style);
  else
    return AbsoluteVerticalNeedsEstimate(style);
}

base::Optional<LayoutUnit> ComputeAbsoluteDialogYPosition(
    const LayoutObject& dialog,
    LayoutUnit height) {
  if (!IsHTMLDialogElement(dialog.GetNode()))
    return base::nullopt;

  // This code implements <dialog> static position spec.
  // //
  // https://html.spec.whatwg.org/multipage/interactive-elements.html#the-dialog-element
  HTMLDialogElement* dialog_node = ToHTMLDialogElement(dialog.GetNode());
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

NGAbsolutePhysicalPosition ComputePartialAbsoluteWithChildInlineSize(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGStaticPosition& static_position,
    const base::Optional<MinMaxSize>& child_minmax,
    const base::Optional<NGLogicalSize>& replaced_size,
    const WritingMode container_writing_mode,
    const TextDirection container_direction) {
  NGAbsolutePhysicalPosition position;
  if (style.IsHorizontalWritingMode()) {
    base::Optional<LayoutUnit> width;
    if (!style.Width().IsAuto()) {
      width = ResolveWidth(style.Width(), space, style, child_minmax,
                           LengthResolveType::kContentSize);
    } else if (replaced_size.has_value()) {
      width = replaced_size.value().inline_size;
    }
    ComputeAbsoluteHorizontal(space, style, width, static_position,
                              child_minmax, container_writing_mode,
                              container_direction, &position);
  } else {
    base::Optional<LayoutUnit> height;
    if (!style.Height().IsAuto()) {
      height = ResolveHeight(style.Height(), space, style, child_minmax,
                             LengthResolveType::kContentSize);
    } else if (replaced_size.has_value()) {
      height = replaced_size.value().inline_size;
    }
    ComputeAbsoluteVertical(space, style, height, static_position, child_minmax,
                            container_writing_mode, container_direction,
                            &position);
  }
  return position;
}

void ComputeFullAbsoluteWithChildBlockSize(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGStaticPosition& static_position,
    const base::Optional<LayoutUnit>& child_block_size,
    const base::Optional<NGLogicalSize>& replaced_size,
    const WritingMode container_writing_mode,
    const TextDirection container_direction,
    NGAbsolutePhysicalPosition* position) {
  // After partial size has been computed, child block size is either
  // unknown, or fully computed, there is no minmax.
  // To express this, a 'fixed' minmax is created where
  // min and max are the same.
  base::Optional<MinMaxSize> child_minmax;
  if (child_block_size.has_value()) {
    child_minmax = MinMaxSize{*child_block_size, *child_block_size};
  }
  if (style.IsHorizontalWritingMode()) {
    base::Optional<LayoutUnit> height;
    if (!style.Height().IsAuto()) {
      height = ResolveHeight(style.Height(), space, style, child_minmax,
                             LengthResolveType::kContentSize);
    } else if (replaced_size.has_value()) {
      height = replaced_size.value().block_size;
    }
    ComputeAbsoluteVertical(space, style, height, static_position, child_minmax,
                            container_writing_mode, container_direction,
                            position);
  } else {
    base::Optional<LayoutUnit> width;
    if (!style.Width().IsAuto()) {
      width = ResolveWidth(style.Width(), space, style, child_minmax,
                           LengthResolveType::kContentSize);
    } else if (replaced_size.has_value()) {
      width = replaced_size.value().block_size;
    }
    ComputeAbsoluteHorizontal(space, style, width, static_position,
                              child_minmax, container_writing_mode,
                              container_direction, position);
  }
}

}  // namespace blink
