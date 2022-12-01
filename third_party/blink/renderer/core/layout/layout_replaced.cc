/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011-2012. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/layout/layout_replaced.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/html/html_dimension.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/box_layout_extra_input.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/layout_view_transition_content.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/replaced_painter.h"
#include "third_party/blink/renderer/core/style/basic_shapes.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

const int LayoutReplaced::kDefaultWidth = 300;
const int LayoutReplaced::kDefaultHeight = 150;

LayoutReplaced::LayoutReplaced(Element* element)
    : LayoutBox(element), intrinsic_size_(kDefaultWidth, kDefaultHeight) {
  // TODO(jchaffraix): We should not set this boolean for block-level
  // replaced elements (crbug.com/567964).
  SetIsAtomicInlineLevel(true);
}

LayoutReplaced::LayoutReplaced(Element* element,
                               const LayoutSize& intrinsic_size)
    : LayoutBox(element), intrinsic_size_(intrinsic_size) {
  // TODO(jchaffraix): We should not set this boolean for block-level
  // replaced elements (crbug.com/567964).
  SetIsAtomicInlineLevel(true);
}

LayoutReplaced::~LayoutReplaced() = default;

void LayoutReplaced::WillBeDestroyed() {
  NOT_DESTROYED();
  if (!DocumentBeingDestroyed() && Parent())
    Parent()->DirtyLinesFromChangedChild(this);

  LayoutBox::WillBeDestroyed();
}

void LayoutReplaced::StyleDidChange(StyleDifference diff,
                                    const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutBox::StyleDidChange(diff, old_style);

  // Replaced elements can have border-radius clips without clipping overflow;
  // the overflow clipping case is already covered in LayoutBox::StyleDidChange
  if (old_style && !old_style->RadiiEqual(StyleRef()))
    SetNeedsPaintPropertyUpdate();

  bool had_style = !!old_style;
  float old_zoom = had_style ? old_style->EffectiveZoom()
                             : ComputedStyleInitialValues::InitialZoom();
  if (Style() && StyleRef().EffectiveZoom() != old_zoom)
    IntrinsicSizeChanged();

  if ((IsLayoutImage() || IsVideo() || IsCanvas()) && !ClipsToContentBox() &&
      !StyleRef().ObjectPropertiesPreventReplacedOverflow()) {
    static constexpr const char kErrorMessage[] =
        "Specifying 'overflow: visible' on img, video and canvas tags may "
        "cause them to produce visual content outside of the element bounds. "
        "See "
        "https://github.com/WICG/view-transitions/blob/main/"
        "debugging_overflow_on_images.md for details.";
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning, kErrorMessage);
    constexpr bool kDiscardDuplicates = true;
    GetDocument().AddConsoleMessage(console_message, kDiscardDuplicates);
  }
}

void LayoutReplaced::UpdateLayout() {
  NOT_DESTROYED();
  DCHECK(NeedsLayout());

  PhysicalRect old_content_rect = ReplacedContentRect();

  if (!RuntimeEnabledFeatures::LayoutNGReplacedNoBoxSettersEnabled()) {
    UpdateLogicalWidth();
    UpdateLogicalHeight();
  }

  ClearLayoutOverflow();
  ClearSelfNeedsLayoutOverflowRecalc();
  ClearChildNeedsLayoutOverflowRecalc();

  if (!RuntimeEnabledFeatures::LayoutNGUnifyUpdateAfterLayoutEnabled())
    UpdateAfterLayout();

  ClearNeedsLayout();

  if (ReplacedContentRectFrom(SizeFromNG(), BorderPaddingFromNG()) !=
      old_content_rect)
    SetShouldDoFullPaintInvalidation();
}

void LayoutReplaced::IntrinsicSizeChanged() {
  NOT_DESTROYED();
  int scaled_width =
      static_cast<int>(kDefaultWidth * StyleRef().EffectiveZoom());
  int scaled_height =
      static_cast<int>(kDefaultHeight * StyleRef().EffectiveZoom());
  intrinsic_size_ = LayoutSize(scaled_width, scaled_height);
  SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kSizeChanged);
}

void LayoutReplaced::Paint(const PaintInfo& paint_info) const {
  NOT_DESTROYED();
  ReplacedPainter(*this).Paint(paint_info);
}

bool LayoutReplaced::HasReplacedLogicalHeight() const {
  NOT_DESTROYED();
  if (StyleRef().LogicalHeight().IsAuto())
    return StretchBlockSizeIfAuto();

  if (StyleRef().LogicalHeight().IsFixed())
    return true;

  if (StyleRef().LogicalHeight().IsPercentOrCalc()) {
    if (HasAutoHeightOrContainingBlockWithAutoHeight())
      return false;
    return true;
  }

  if (StyleRef().LogicalHeight().IsContentOrIntrinsicOrFillAvailable())
    return StyleRef().AspectRatio().IsAuto();

  return false;
}

bool LayoutReplaced::NeedsPreferredWidthsRecalculation() const {
  NOT_DESTROYED();
  // If the height is a percentage and the width is auto, then the
  // containingBlocks's height changing can cause this node to change it's
  // preferred width because it maintains aspect ratio.
  return HasRelativeLogicalHeight() && StyleRef().LogicalWidth().IsAuto();
}

static inline bool LayoutObjectHasIntrinsicAspectRatio(
    const LayoutObject* layout_object) {
  DCHECK(layout_object);
  return layout_object->IsImage() || layout_object->IsCanvas() ||
         IsA<LayoutVideo>(layout_object) ||
         IsA<LayoutViewTransitionContent>(layout_object);
}

void LayoutReplaced::RecalcVisualOverflow() {
  NOT_DESTROYED();
  ClearVisualOverflow();
  LayoutObject::RecalcVisualOverflow();
  AddVisualEffectOverflow();

  // Replaced elements clip the content to the element's content-box by default.
  // But if the CSS overflow property is respected, the content may paint
  // outside the element's bounds as ink overflow (with overflow:visible for
  // example). So we add |ReplacedContentRect()|, which provides the element's
  // painting rectangle relative to it's bounding box in its visual overflow if
  // the overflow property is respected.
  // Note that |overflow_| is meant to track the maximum potential ink overflow.
  // The actual painted overflow (based on the values for overflow,
  // overflow-clip-margin and paint containment) is computed in
  // LayoutBox::VisualOverflowRect.
  if (RespectsCSSOverflow())
    AddContentsVisualOverflow(ReplacedContentRect());
}

void LayoutReplaced::ComputeIntrinsicSizingInfoForReplacedContent(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  NOT_DESTROYED();
  // In cases where we apply size containment we don't need to compute sizing
  // information, since the final result does not depend on it.
  if (ShouldApplySizeContainment()) {
    // Reset the size in case it was already populated.
    intrinsic_sizing_info.size = gfx::SizeF();

    const StyleAspectRatio& aspect_ratio = StyleRef().AspectRatio();
    if (!aspect_ratio.IsAuto()) {
      intrinsic_sizing_info.aspect_ratio.set_width(
          aspect_ratio.GetRatio().width());
      intrinsic_sizing_info.aspect_ratio.set_height(
          aspect_ratio.GetRatio().height());
    }

    // If any of the dimensions are overridden, set those sizes.
    if (HasOverrideIntrinsicContentLogicalWidth()) {
      intrinsic_sizing_info.size.set_width(
          OverrideIntrinsicContentLogicalWidth().ToFloat());
    }
    if (HasOverrideIntrinsicContentLogicalHeight()) {
      intrinsic_sizing_info.size.set_height(
          OverrideIntrinsicContentLogicalHeight().ToFloat());
    }
    return;
  }

  // Size overrides only apply if there is size-containment, which is checked
  // above.
  DCHECK(!HasOverrideIntrinsicContentLogicalWidth());
  DCHECK(!HasOverrideIntrinsicContentLogicalHeight());

  ComputeIntrinsicSizingInfo(intrinsic_sizing_info);

  // Update our intrinsic size to match what was computed, so that
  // when we constrain the size, the correct intrinsic size will be
  // obtained for comparison against min and max widths.
  if (!intrinsic_sizing_info.aspect_ratio.IsEmpty() &&
      !intrinsic_sizing_info.size.IsEmpty()) {
    intrinsic_size_ = LayoutSize(intrinsic_sizing_info.size);
    if (!IsHorizontalWritingMode())
      intrinsic_size_ = intrinsic_size_.TransposedSize();
  }
}

gfx::SizeF LayoutReplaced::ConstrainIntrinsicSizeToMinMax(
    const IntrinsicSizingInfo& intrinsic_sizing_info) const {
  NOT_DESTROYED();
  // Constrain the intrinsic size along each axis according to minimum and
  // maximum width/heights along the opposite axis. So for example a maximum
  // width that shrinks our width will result in the height we compute here
  // having to shrink in order to preserve the aspect ratio. Because we compute
  // these values independently along each axis, the final returned size may in
  // fact not preserve the aspect ratio.
  // TODO(davve): Investigate using only the intrinsic aspect ratio here.
  gfx::SizeF constrained_size = intrinsic_sizing_info.size;
  if (!intrinsic_sizing_info.aspect_ratio.IsEmpty() &&
      !intrinsic_sizing_info.size.IsEmpty() &&
      StyleRef().LogicalWidth().IsAuto() &&
      StyleRef().LogicalHeight().IsAuto()) {
    // We can't multiply or divide by 'intrinsicSizingInfo.aspectRatio' here, it
    // breaks tests, like images/zoomed-img-size.html, which
    // can only be fixed once subpixel precision is available for things like
    // intrinsicWidth/Height - which include zoom!
    constrained_size.set_width(LayoutBox::ComputeReplacedLogicalHeight() *
                               intrinsic_sizing_info.size.width() /
                               intrinsic_sizing_info.size.height());
    constrained_size.set_height(LayoutBox::ComputeReplacedLogicalWidth() *
                                intrinsic_sizing_info.size.height() /
                                intrinsic_sizing_info.size.width());
  }
  return constrained_size;
}

void LayoutReplaced::ComputePositionedLogicalWidth(
    LogicalExtentComputedValues& computed_values) const {
  NOT_DESTROYED();
  // The following is based off of the W3C Working Draft from April 11, 2006 of
  // CSS 2.1: Section 10.3.8 "Absolutely positioned, replaced elements"
  // <http://www.w3.org/TR/2005/WD-CSS21-20050613/visudet.html#abs-replaced-width>
  // (block-style-comments in this function correspond to text from the spec and
  // the numbers correspond to numbers in spec).

  // We don't use containingBlock(), since we may be positioned by an enclosing
  // relative positioned inline.
  const auto* container_block = To<LayoutBoxModelObject>(Container());

  const LayoutUnit container_logical_width =
      ContainingBlockLogicalWidthForPositioned(container_block);
  const LayoutUnit container_relative_logical_width =
      ContainingBlockLogicalWidthForPositioned(container_block, false);

  // To match WinIE, in quirks mode use the parent's 'direction' property
  // instead of the the container block's.
  TextDirection container_direction = container_block->StyleRef().Direction();

  // Variables to solve.
  bool is_horizontal = IsHorizontalWritingMode();
  Length logical_left = StyleRef().LogicalLeft();
  Length logical_right = StyleRef().LogicalRight();
  Length margin_logical_left =
      is_horizontal ? StyleRef().MarginLeft() : StyleRef().MarginTop();
  Length margin_logical_right =
      is_horizontal ? StyleRef().MarginRight() : StyleRef().MarginBottom();
  LayoutUnit& margin_logical_left_alias = StyleRef().IsLeftToRightDirection()
                                              ? computed_values.margins_.start_
                                              : computed_values.margins_.end_;
  LayoutUnit& margin_logical_right_alias =
      StyleRef().IsLeftToRightDirection() ? computed_values.margins_.end_
                                          : computed_values.margins_.start_;

  // ---------------------------------------------------------------------------
  // 1. The used value of 'width' is determined as for inline replaced
  //  elements.
  // ---------------------------------------------------------------------------
  // NOTE: This value of width is final in that the min/max width calculations
  // are dealt with in computeReplacedWidth().  This means that the steps to
  // produce correct max/min in the non-replaced version, are not necessary.
  computed_values.extent_ =
      ComputeReplacedLogicalWidth() + BorderAndPaddingLogicalWidth();

  const LayoutUnit available_space =
      container_logical_width - computed_values.extent_;

  // ---------------------------------------------------------------------------
  // 2. If both 'left' and 'right' have the value 'auto', then if 'direction'
  //    of the containing block is 'ltr', set 'left' to the static position;
  //    else if 'direction' is 'rtl', set 'right' to the static position.
  // ---------------------------------------------------------------------------
  // see FIXME 1
  ComputeInlineStaticDistance(logical_left, logical_right, this,
                              container_block, container_logical_width);

  // ---------------------------------------------------------------------------
  // 3. If 'left' or 'right' are 'auto', replace any 'auto' on 'margin-left'
  //    or 'margin-right' with '0'.
  // ---------------------------------------------------------------------------
  if (logical_left.IsAuto() || logical_right.IsAuto()) {
    if (margin_logical_left.IsAuto())
      margin_logical_left = Length::Fixed(0);
    if (margin_logical_right.IsAuto())
      margin_logical_right = Length::Fixed(0);
  }

  // ---------------------------------------------------------------------------
  // 4. If at this point both 'margin-left' and 'margin-right' are still 'auto',
  //    solve the equation under the extra constraint that the two margins must
  //    get equal values, unless this would make them negative, in which case
  //    when the direction of the containing block is 'ltr' ('rtl'), set
  //    'margin-left' ('margin-right') to zero and solve for 'margin-right'
  //    ('margin-left').
  // ---------------------------------------------------------------------------
  LayoutUnit logical_left_value;
  LayoutUnit logical_right_value;

  if (margin_logical_left.IsAuto() && margin_logical_right.IsAuto()) {
    // 'left' and 'right' cannot be 'auto' due to step 3
    DCHECK(!(logical_left.IsAuto() && logical_right.IsAuto()));

    logical_left_value = ValueForLength(logical_left, container_logical_width);
    logical_right_value =
        ValueForLength(logical_right, container_logical_width);

    LayoutUnit difference =
        available_space - (logical_left_value + logical_right_value);
    if (difference > LayoutUnit()) {
      margin_logical_left_alias = difference / 2;  // split the difference
      margin_logical_right_alias =
          difference -
          margin_logical_left_alias;  // account for odd valued differences
    } else {
      // Use the containing block's direction rather than the parent block's
      // per CSS 2.1 reference test abspos-replaced-width-margin-000.
      if (container_direction == TextDirection::kLtr) {
        margin_logical_left_alias = LayoutUnit();
        margin_logical_right_alias = difference;  // will be negative
      } else {
        margin_logical_left_alias = difference;  // will be negative
        margin_logical_right_alias = LayoutUnit();
      }
    }

    // -------------------------------------------------------------------------
    // 5. If at this point there is an 'auto' left, solve the equation for that
    //    value.
    // -------------------------------------------------------------------------
  } else if (logical_left.IsAuto()) {
    margin_logical_left_alias =
        ValueForLength(margin_logical_left, container_relative_logical_width);
    margin_logical_right_alias =
        ValueForLength(margin_logical_right, container_relative_logical_width);
    logical_right_value =
        ValueForLength(logical_right, container_logical_width);

    // Solve for 'left'
    logical_left_value =
        available_space - (logical_right_value + margin_logical_left_alias +
                           margin_logical_right_alias);
  } else if (logical_right.IsAuto()) {
    margin_logical_left_alias =
        ValueForLength(margin_logical_left, container_relative_logical_width);
    margin_logical_right_alias =
        ValueForLength(margin_logical_right, container_relative_logical_width);
    logical_left_value = ValueForLength(logical_left, container_logical_width);

    // Solve for 'right'
    logical_right_value =
        available_space - (logical_left_value + margin_logical_left_alias +
                           margin_logical_right_alias);
  } else if (margin_logical_left.IsAuto()) {
    margin_logical_right_alias =
        ValueForLength(margin_logical_right, container_relative_logical_width);
    logical_left_value = ValueForLength(logical_left, container_logical_width);
    logical_right_value =
        ValueForLength(logical_right, container_logical_width);

    // Solve for 'margin-left'
    margin_logical_left_alias =
        available_space -
        (logical_left_value + logical_right_value + margin_logical_right_alias);
  } else if (margin_logical_right.IsAuto()) {
    margin_logical_left_alias =
        ValueForLength(margin_logical_left, container_relative_logical_width);
    logical_left_value = ValueForLength(logical_left, container_logical_width);
    logical_right_value =
        ValueForLength(logical_right, container_logical_width);

    // Solve for 'margin-right'
    margin_logical_right_alias =
        available_space -
        (logical_left_value + logical_right_value + margin_logical_left_alias);
  } else {
    // Nothing is 'auto', just calculate the values.
    margin_logical_left_alias =
        ValueForLength(margin_logical_left, container_relative_logical_width);
    margin_logical_right_alias =
        ValueForLength(margin_logical_right, container_relative_logical_width);
    logical_right_value =
        ValueForLength(logical_right, container_logical_width);
    logical_left_value = ValueForLength(logical_left, container_logical_width);
    // If the containing block is right-to-left, then push the left position as
    // far to the right as possible
    if (container_direction == TextDirection::kRtl) {
      int total_logical_width =
          (computed_values.extent_ + logical_left_value + logical_right_value +
           margin_logical_left_alias + margin_logical_right_alias)
              .ToInt();
      logical_left_value =
          container_logical_width - (total_logical_width - logical_left_value);
    }
  }

  // ---------------------------------------------------------------------------
  // 6. If at this point the values are over-constrained, ignore the value for
  //    either 'left' (in case the 'direction' property of the containing block
  //    is 'rtl') or 'right' (in case 'direction' is 'ltr') and solve for that
  //    value.
  // ---------------------------------------------------------------------------
  // NOTE: Constraints imposed by the width of the containing block and its
  // content have already been accounted for above.
  //
  // FIXME: Deal with differing writing modes here.  Our offset needs to be in
  // the containing block's coordinate space, so that
  // can make the result here rather complicated to compute.
  //
  // Use computed values to calculate the horizontal position.
  //
  // FIXME: This hack is needed to calculate the logical left position for a
  // 'rtl' relatively positioned, inline containing block because right now, it
  // is using the logical left position of the first line box when really it
  // should use the last line box. When this is fixed elsewhere, this block
  // should be removed.
  if (container_block->IsLayoutInline() &&
      !container_block->StyleRef().IsLeftToRightDirection()) {
    const auto* flow = To<LayoutInline>(container_block);
    InlineFlowBox* first_line = flow->FirstLineBox();
    InlineFlowBox* last_line = flow->LastLineBox();
    if (first_line && last_line && first_line != last_line) {
      computed_values.position_ =
          logical_left_value + margin_logical_left_alias +
          last_line->BorderLogicalLeft() +
          (last_line->LogicalLeft() - first_line->LogicalLeft());
      return;
    }
  }

  LayoutUnit logical_left_pos = logical_left_value + margin_logical_left_alias;
  ComputeLogicalLeftPositionedOffset(logical_left_pos, this,
                                     computed_values.extent_, container_block,
                                     container_logical_width);
  computed_values.position_ = logical_left_pos;
}

void LayoutReplaced::ComputePositionedLogicalHeight(
    LogicalExtentComputedValues& computed_values) const {
  NOT_DESTROYED();
  // The following is based off of the W3C Working Draft from April 11, 2006 of
  // CSS 2.1: Section 10.6.5 "Absolutely positioned, replaced elements"
  // <http://www.w3.org/TR/2005/WD-CSS21-20050613/visudet.html#abs-replaced-height>
  // (block-style-comments in this function correspond to text from the spec and
  // the numbers correspond to numbers in spec)

  // We don't use containingBlock(), since we may be positioned by an enclosing
  // relpositioned inline.
  const auto* container_block = To<LayoutBoxModelObject>(Container());

  const LayoutUnit container_logical_height =
      ContainingBlockLogicalHeightForPositioned(container_block);
  const LayoutUnit container_relative_logical_width =
      ContainingBlockLogicalWidthForPositioned(container_block, false);

  // Variables to solve.
  Length margin_before = StyleRef().MarginBefore();
  Length margin_after = StyleRef().MarginAfter();
  LayoutUnit& margin_before_alias = computed_values.margins_.before_;
  LayoutUnit& margin_after_alias = computed_values.margins_.after_;

  Length logical_top = StyleRef().LogicalTop();
  Length logical_bottom = StyleRef().LogicalBottom();

  // ---------------------------------------------------------------------------
  // 1. The used value of 'height' is determined as for inline replaced
  //    elements.
  // ---------------------------------------------------------------------------
  // NOTE: This value of height is final in that the min/max height calculations
  // are dealt with in computeReplacedHeight().  This means that the steps to
  // produce correct max/min in the non-replaced version, are not necessary.
  computed_values.extent_ =
      ComputeReplacedLogicalHeight() + BorderAndPaddingLogicalHeight();
  const LayoutUnit available_space =
      container_logical_height - computed_values.extent_;

  // ---------------------------------------------------------------------------
  // 2. If both 'top' and 'bottom' have the value 'auto', replace 'top' with the
  //    element's static position.
  // ---------------------------------------------------------------------------
  // see FIXME 1
  ComputeBlockStaticDistance(logical_top, logical_bottom, this,
                             container_block);

  // ---------------------------------------------------------------------------
  // 3. If 'bottom' is 'auto', replace any 'auto' on 'margin-top' or
  //    'margin-bottom' with '0'.
  // ---------------------------------------------------------------------------
  // FIXME: The spec. says that this step should only be taken when bottom is
  // auto, but if only top is auto, this makes step 4 impossible.
  if (logical_top.IsAuto() || logical_bottom.IsAuto()) {
    if (margin_before.IsAuto())
      margin_before = Length::Fixed(0);
    if (margin_after.IsAuto())
      margin_after = Length::Fixed(0);
  }

  // ---------------------------------------------------------------------------
  // 4. If at this point both 'margin-top' and 'margin-bottom' are still 'auto',
  //    solve the equation under the extra constraint that the two margins must
  //    get equal values.
  // ---------------------------------------------------------------------------
  LayoutUnit logical_top_value;
  LayoutUnit logical_bottom_value;

  if (margin_before.IsAuto() && margin_after.IsAuto()) {
    // 'top' and 'bottom' cannot be 'auto' due to step 2 and 3 combined.
    DCHECK(!(logical_top.IsAuto() || logical_bottom.IsAuto()));

    logical_top_value = ValueForLength(logical_top, container_logical_height);
    logical_bottom_value =
        ValueForLength(logical_bottom, container_logical_height);

    LayoutUnit difference =
        available_space - (logical_top_value + logical_bottom_value);
    // NOTE: This may result in negative values.
    margin_before_alias = difference / 2;  // split the difference
    margin_after_alias =
        difference - margin_before_alias;  // account for odd valued differences

    // -------------------------------------------------------------------------
    // 5. If at this point there is only one 'auto' left, solve the equation
    //    for that value.
    // -------------------------------------------------------------------------
  } else if (logical_top.IsAuto()) {
    margin_before_alias =
        ValueForLength(margin_before, container_relative_logical_width);
    margin_after_alias =
        ValueForLength(margin_after, container_relative_logical_width);
    logical_bottom_value =
        ValueForLength(logical_bottom, container_logical_height);

    // Solve for 'top'
    logical_top_value =
        available_space -
        (logical_bottom_value + margin_before_alias + margin_after_alias);
  } else if (logical_bottom.IsAuto()) {
    margin_before_alias =
        ValueForLength(margin_before, container_relative_logical_width);
    margin_after_alias =
        ValueForLength(margin_after, container_relative_logical_width);
    logical_top_value = ValueForLength(logical_top, container_logical_height);

    // Solve for 'bottom'
    // NOTE: It is not necessary to solve for 'bottom' because we don't ever
    // use the value.
  } else if (margin_before.IsAuto()) {
    margin_after_alias =
        ValueForLength(margin_after, container_relative_logical_width);
    logical_top_value = ValueForLength(logical_top, container_logical_height);
    logical_bottom_value =
        ValueForLength(logical_bottom, container_logical_height);

    // Solve for 'margin-top'
    margin_before_alias =
        available_space -
        (logical_top_value + logical_bottom_value + margin_after_alias);
  } else if (margin_after.IsAuto()) {
    margin_before_alias =
        ValueForLength(margin_before, container_relative_logical_width);
    logical_top_value = ValueForLength(logical_top, container_logical_height);
    logical_bottom_value =
        ValueForLength(logical_bottom, container_logical_height);

    // Solve for 'margin-bottom'
    margin_after_alias =
        available_space -
        (logical_top_value + logical_bottom_value + margin_before_alias);
  } else {
    // Nothing is 'auto', just calculate the values.
    margin_before_alias =
        ValueForLength(margin_before, container_relative_logical_width);
    margin_after_alias =
        ValueForLength(margin_after, container_relative_logical_width);
    logical_top_value = ValueForLength(logical_top, container_logical_height);
    // NOTE: It is not necessary to solve for 'bottom' because we don't ever
    // use the value.
  }

  // ---------------------------------------------------------------------------
  // 6. If at this point the values are over-constrained, ignore the value for
  //    'bottom' and solve for that value.
  // ---------------------------------------------------------------------------
  // NOTE: It is not necessary to do this step because we don't end up using the
  // value of 'bottom' regardless of whether the values are over-constrained or
  // not.

  // Use computed values to calculate the vertical position.
  LayoutUnit logical_top_pos = logical_top_value + margin_before_alias;
  ComputeLogicalTopPositionedOffset(logical_top_pos, this,
                                    computed_values.extent_, container_block,
                                    container_logical_height);
  computed_values.position_ = logical_top_pos;
}

absl::optional<gfx::SizeF>
LayoutReplaced::ComputeObjectViewBoxSizeForIntrinsicSizing() const {
  if (IntrinsicWidthOverride() || IntrinsicHeightOverride())
    return absl::nullopt;

  if (auto view_box = ComputeObjectViewBoxRect())
    return static_cast<gfx::SizeF>(view_box->size);

  return absl::nullopt;
}

absl::optional<PhysicalRect> LayoutReplaced::ComputeObjectViewBoxRect(
    const LayoutSize* overridden_intrinsic_size) const {
  scoped_refptr<BasicShape> object_view_box = StyleRef().ObjectViewBox();
  if (LIKELY(!object_view_box))
    return absl::nullopt;

  const auto& intrinsic_size =
      overridden_intrinsic_size ? *overridden_intrinsic_size : intrinsic_size_;
  if (intrinsic_size.IsEmpty())
    return absl::nullopt;

  if (!CanApplyObjectViewBox())
    return absl::nullopt;

  DCHECK(object_view_box->GetType() == BasicShape::kBasicShapeRectType ||
         object_view_box->GetType() == BasicShape::kBasicShapeInsetType ||
         object_view_box->GetType() == BasicShape::kBasicShapeXYWHType);

  Path path;
  gfx::RectF bounding_box(0, 0, intrinsic_size.Width().ToFloat(),
                          intrinsic_size.Height().ToFloat());
  object_view_box->GetPath(path, bounding_box, 1.f);

  const PhysicalRect view_box_rect =
      PhysicalRect::EnclosingRect(path.BoundingRect());
  if (view_box_rect.IsEmpty())
    return absl::nullopt;

  const PhysicalRect intrinsic_rect(PhysicalOffset(), intrinsic_size);
  if (view_box_rect == intrinsic_rect)
    return absl::nullopt;

  return view_box_rect;
}

PhysicalRect LayoutReplaced::ComputeReplacedContentRect(
    const LayoutSize size,
    const NGPhysicalBoxStrut& border_padding,
    const LayoutSize* overridden_intrinsic_size) const {
  // |intrinsic_size| provides the size of the embedded content rendered in the
  // replaced element. This is the reference size that object-view-box applies
  // to.
  // If present, object-view-box changes the notion of embedded content used for
  // painting the element and applying rest of the object* properties. The
  // following cases are possible:
  //
  // - object-view-box is a subset of the embedded content. For example,
  // [0,0 50x50] on an image with bounds 100x100.
  //
  // - object-view-box is a superset of the embedded content. For example,
  // [-10, -10, 120x120] on an image with bounds 100x100.
  //
  // - object-view-box intersects with the embedded content. For example,
  // [-10, -10, 50x50] on an image with bounds 100x100.
  //
  // - object-view-box has no intersection with the embedded content. For
  // example, [-50, -50, 50x50] on any image.
  //
  // The image is scaled (by object-fit) and positioned (by object-position)
  // assuming the embedded content to be provided by the box identified by
  // object-view-box.
  //
  // Regions outside the image bounds (but within object-view-box) paint
  // transparent pixels. Regions outside object-view-box (but within image
  // bounds) are scaled as defined by object-fit above and treated as ink
  // overflow.
  const auto& intrinsic_size_for_object_view_box =
      overridden_intrinsic_size ? *overridden_intrinsic_size : intrinsic_size_;
  const auto view_box =
      ComputeObjectViewBoxRect(&intrinsic_size_for_object_view_box);

  // If no view box override was applied, then we don't need to adjust the
  // view-box paint rect.
  if (!view_box) {
    return ComputeObjectFitAndPositionRect(size, border_padding,
                                           overridden_intrinsic_size);
  }

  // Compute the paint rect based on bounds provided by the view box.
  DCHECK(!view_box->IsEmpty());
  const LayoutSize view_box_size(view_box->Width(), view_box->Height());
  const auto view_box_paint_rect =
      ComputeObjectFitAndPositionRect(size, border_padding, &view_box_size);
  if (view_box_paint_rect.IsEmpty())
    return view_box_paint_rect;

  // Scale the original image bounds by the scale applied to the view box.
  auto scaled_width = intrinsic_size_for_object_view_box.Width().MulDiv(
      view_box_paint_rect.Width(), view_box->Width());
  auto scaled_height = intrinsic_size_for_object_view_box.Height().MulDiv(
      view_box_paint_rect.Height(), view_box->Height());
  const PhysicalSize scaled_image_size(scaled_width, scaled_height);

  // Scale the offset from the image origin by the scale applied to the view
  // box.
  auto scaled_x_offset =
      view_box->X().MulDiv(view_box_paint_rect.Width(), view_box->Width());
  auto scaled_y_offset =
      view_box->Y().MulDiv(view_box_paint_rect.Height(), view_box->Height());
  const PhysicalOffset scaled_offset(scaled_x_offset, scaled_y_offset);

  return PhysicalRect(view_box_paint_rect.offset - scaled_offset,
                      scaled_image_size);
}

PhysicalRect LayoutReplaced::ComputeObjectFitAndPositionRect(
    const LayoutSize size,
    const NGPhysicalBoxStrut& border_padding,
    const LayoutSize* overridden_intrinsic_size) const {
  NOT_DESTROYED();
  PhysicalRect content_rect = PhysicalContentBoxRectFrom(size, border_padding);
  EObjectFit object_fit = StyleRef().GetObjectFit();

  if (object_fit == EObjectFit::kFill &&
      StyleRef().ObjectPosition() ==
          ComputedStyleInitialValues::InitialObjectPosition()) {
    return content_rect;
  }

  // TODO(davve): intrinsicSize doubles as both intrinsic size and intrinsic
  // ratio. In the case of SVG images this isn't correct since they can have
  // intrinsic ratio but no intrinsic size. In order to maintain aspect ratio,
  // the intrinsic size for SVG might be faked from the aspect ratio,
  // see SVGImage::containerSize().
  PhysicalSize intrinsic_size(
      overridden_intrinsic_size ? *overridden_intrinsic_size : IntrinsicSize());
  if (intrinsic_size.IsEmpty())
    return content_rect;

  PhysicalSize scaled_intrinsic_size(intrinsic_size);
  PhysicalRect final_rect = content_rect;
  switch (object_fit) {
    case EObjectFit::kScaleDown:
      // Srcset images have an intrinsic size depending on their destination,
      // but with object-fit: scale-down they need to use the underlying image
      // src's size. So revert back to the original size in that case.
      if (auto* image = DynamicTo<LayoutImage>(this)) {
        scaled_intrinsic_size.Scale(1.0 / image->ImageDevicePixelRatio());
      }
      [[fallthrough]];
    case EObjectFit::kContain:
    case EObjectFit::kCover:
      final_rect.size = final_rect.size.FitToAspectRatio(
          intrinsic_size, object_fit == EObjectFit::kCover
                              ? kAspectRatioFitGrow
                              : kAspectRatioFitShrink);
      if (object_fit != EObjectFit::kScaleDown ||
          final_rect.Width() <= scaled_intrinsic_size.width)
        break;
      [[fallthrough]];
    case EObjectFit::kNone:
      final_rect.size = scaled_intrinsic_size;
      break;
    case EObjectFit::kFill:
      break;
    default:
      NOTREACHED();
  }

  LayoutUnit x_offset =
      MinimumValueForLength(StyleRef().ObjectPosition().X(),
                            content_rect.Width() - final_rect.Width());
  LayoutUnit y_offset =
      MinimumValueForLength(StyleRef().ObjectPosition().Y(),
                            content_rect.Height() - final_rect.Height());
  final_rect.Move(PhysicalOffset(x_offset, y_offset));

  return final_rect;
}

PhysicalRect LayoutReplaced::ReplacedContentRect() const {
  NOT_DESTROYED();
  // This function should compute the result with old geometry even if a
  // BoxLayoutExtraInput exists.
  return ReplacedContentRectFrom(
      Size(), NGPhysicalBoxStrut(BorderTop() + PaddingTop(),
                                 BorderRight() + PaddingRight(),
                                 BorderBottom() + PaddingBottom(),
                                 BorderLeft() + PaddingLeft()));
}

PhysicalRect LayoutReplaced::ReplacedContentRectFrom(
    const LayoutSize size,
    const NGPhysicalBoxStrut& border_padding) const {
  NOT_DESTROYED();
  return ComputeReplacedContentRect(size, border_padding);
}

LayoutSize LayoutReplaced::SizeFromNG() const {
  if (!RuntimeEnabledFeatures::LayoutNGReplacedNoBoxSettersEnabled() ||
      !GetBoxLayoutExtraInput()) {
    return Size();
  }
  LayoutSize new_size(OverrideLogicalWidth(), OverrideLogicalHeight());
  if (!StyleRef().IsHorizontalWritingMode())
    new_size = new_size.TransposedSize();
  return new_size;
}

NGPhysicalBoxStrut LayoutReplaced::BorderPaddingFromNG() const {
  if (RuntimeEnabledFeatures::LayoutNGReplacedNoBoxSettersEnabled() &&
      GetBoxLayoutExtraInput()) {
    return GetBoxLayoutExtraInput()->border_padding_for_replaced;
  }
  return NGPhysicalBoxStrut(
      BorderTop() + PaddingTop(), BorderRight() + PaddingRight(),
      BorderBottom() + PaddingBottom(), BorderLeft() + PaddingLeft());
}

PhysicalRect LayoutReplaced::PhysicalContentBoxRectFromNG() const {
  NOT_DESTROYED();
  return PhysicalContentBoxRectFrom(SizeFromNG(), BorderPaddingFromNG());
}

PhysicalRect LayoutReplaced::PhysicalContentBoxRectFrom(
    const LayoutSize size,
    const NGPhysicalBoxStrut& border_padding) const {
  NOT_DESTROYED();
  return PhysicalRect(
      border_padding.left, border_padding.top,
      (size.Width() - border_padding.HorizontalSum()).ClampNegativeToZero(),
      (size.Height() - border_padding.VerticalSum()).ClampNegativeToZero());
}

PhysicalRect LayoutReplaced::PreSnappedRectForPersistentSizing(
    const PhysicalRect& rect) {
  return PhysicalRect(rect.offset, PhysicalSize(ToRoundedSize(rect.size)));
}

void LayoutReplaced::ComputeIntrinsicSizingInfo(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  NOT_DESTROYED();
  DCHECK(!ShouldApplySizeContainment());

  auto view_box_size = ComputeObjectViewBoxSizeForIntrinsicSizing();
  if (view_box_size) {
    intrinsic_sizing_info.size = *view_box_size;
    if (!IsHorizontalWritingMode())
      intrinsic_sizing_info.size.Transpose();
  } else {
    intrinsic_sizing_info.size = gfx::SizeF(IntrinsicLogicalWidth().ToFloat(),
                                            IntrinsicLogicalHeight().ToFloat());
  }

  const StyleAspectRatio& aspect_ratio = StyleRef().AspectRatio();
  if (!aspect_ratio.IsAuto()) {
    intrinsic_sizing_info.aspect_ratio.set_width(
        aspect_ratio.GetRatio().width());
    intrinsic_sizing_info.aspect_ratio.set_height(
        aspect_ratio.GetRatio().height());
    if (!IsHorizontalWritingMode())
      intrinsic_sizing_info.aspect_ratio.Transpose();
  }
  if (aspect_ratio.GetType() == EAspectRatioType::kRatio)
    return;
  // Otherwise, let the intrinsic aspect ratio take precedence, below.

  // Figure out if we need to compute an intrinsic ratio.
  if (!LayoutObjectHasIntrinsicAspectRatio(this))
    return;

  if (!intrinsic_sizing_info.size.IsEmpty())
    intrinsic_sizing_info.aspect_ratio = intrinsic_sizing_info.size;
}

LayoutUnit LayoutReplaced::ComputeConstrainedLogicalWidth(
    ShouldComputePreferred should_compute_preferred) const {
  NOT_DESTROYED();
  if (should_compute_preferred == kComputePreferred)
    return ComputeReplacedLogicalWidthRespectingMinMaxWidth(LayoutUnit(),
                                                            kComputePreferred);
  // The aforementioned 'constraint equation' used for block-level, non-replaced
  // elements in normal flow:
  // 'margin-left' + 'border-left-width' + 'padding-left' + 'width' +
  // 'padding-right' + 'border-right-width' + 'margin-right' = width of
  // containing block
  LayoutUnit logical_width = ContainingBlockLogicalWidthForContent();

  // This solves above equation for 'width' (== logicalWidth).
  LayoutUnit margin_start =
      MinimumValueForLength(StyleRef().MarginStart(), logical_width);
  LayoutUnit margin_end =
      MinimumValueForLength(StyleRef().MarginEnd(), logical_width);
  logical_width = (logical_width - (margin_start + margin_end +
                                    (Size().Width() - ClientWidth())))
                      .ClampNegativeToZero();
  return ComputeReplacedLogicalWidthRespectingMinMaxWidth(
      logical_width, should_compute_preferred);
}

LayoutUnit LayoutReplaced::ComputeReplacedLogicalWidth(
    ShouldComputePreferred should_compute_preferred) const {
  NOT_DESTROYED();
  if (!StyleRef().LogicalWidth().IsAuto() || StretchInlineSizeIfAuto()) {
    return ComputeReplacedLogicalWidthRespectingMinMaxWidth(
        ComputeReplacedLogicalWidthUsing(kMainOrPreferredSize,
                                         StyleRef().LogicalWidth()),
        should_compute_preferred);
  }

  // 10.3.2 Inline, replaced elements:
  // http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width
  IntrinsicSizingInfo intrinsic_sizing_info;
  ComputeIntrinsicSizingInfoForReplacedContent(intrinsic_sizing_info);

  gfx::SizeF constrained_size =
      ConstrainIntrinsicSizeToMinMax(intrinsic_sizing_info);

  if (StyleRef().LogicalWidth().IsAuto()) {
    bool computed_height_is_auto =
        StyleRef().LogicalHeight().IsAuto() && !StretchBlockSizeIfAuto();

    // If 'height' and 'width' both have computed values of 'auto' and the
    // element also has an intrinsic width, then that intrinsic width is the
    // used value of 'width'.
    if (computed_height_is_auto && intrinsic_sizing_info.has_width) {
      return ComputeReplacedLogicalWidthRespectingMinMaxWidth(
          LayoutUnit(constrained_size.width()), should_compute_preferred);
    }

    if (!intrinsic_sizing_info.aspect_ratio.IsEmpty()) {
      // If 'height' and 'width' both have computed values of 'auto' and the
      // element has no intrinsic width, but does have an intrinsic height and
      // intrinsic ratio; or if 'width' has a computed value of 'auto', 'height'
      // has some other computed value, and the element does have an intrinsic
      // ratio; then the used value of 'width' is: (used height) * (intrinsic
      // ratio).
      if ((computed_height_is_auto && !intrinsic_sizing_info.has_width &&
           intrinsic_sizing_info.has_height) ||
          !computed_height_is_auto) {
        LayoutUnit estimated_used_width =
            intrinsic_sizing_info.has_width
                ? LayoutUnit(constrained_size.width())
                : ComputeConstrainedLogicalWidth(should_compute_preferred);
        LayoutUnit logical_height =
            ComputeReplacedLogicalHeight(estimated_used_width);
        NGBoxStrut border_padding(BorderStart() + ComputedCSSPaddingStart(),
                                  BorderEnd() + ComputedCSSPaddingEnd(),
                                  BorderBefore() + ComputedCSSPaddingBefore(),
                                  BorderAfter() + ComputedCSSPaddingAfter());
        // Because ComputeReplacedLogicalHeight returns a content size, we need
        // to add border + padding for InlineSizeFromAspectRatio.
        EBoxSizing box_sizing = EBoxSizing::kContentBox;
        logical_height += border_padding.BlockSum();
        if (StyleRef().AspectRatio().GetType() == EAspectRatioType::kRatio)
          box_sizing = StyleRef().BoxSizing();
        double aspect_ratio = intrinsic_sizing_info.aspect_ratio.width() /
                              intrinsic_sizing_info.aspect_ratio.height();
        return ComputeReplacedLogicalWidthRespectingMinMaxWidth(
            InlineSizeFromAspectRatio(border_padding, aspect_ratio, box_sizing,
                                      logical_height) -
                border_padding.InlineSum(),
            should_compute_preferred);
      }

      // If 'height' and 'width' both have computed values of 'auto' and the
      // element has an intrinsic ratio but no intrinsic height or width, then
      // the used value of 'width' is undefined in CSS 2.1. However, it is
      // suggested that, if the containing block's width does not itself depend
      // on the replaced element's width, then the used value of 'width' is
      // calculated from the constraint equation used for block-level,
      // non-replaced elements in normal flow.
      if (computed_height_is_auto && !intrinsic_sizing_info.has_width &&
          !intrinsic_sizing_info.has_height)
        return ComputeConstrainedLogicalWidth(should_compute_preferred);
    }

    // Otherwise, if 'width' has a computed value of 'auto', and the element has
    // an intrinsic width, then that intrinsic width is the used value of
    // 'width'.
    if (intrinsic_sizing_info.has_width) {
      return ComputeReplacedLogicalWidthRespectingMinMaxWidth(
          LayoutUnit(constrained_size.width()), should_compute_preferred);
    }

    // Otherwise, if 'width' has a computed value of 'auto', but none of the
    // conditions above are met, then the used value of 'width' becomes 300px.
    // If 300px is too wide to fit the device, UAs should use the width of the
    // largest rectangle that has a 2:1 ratio and fits the device instead.
    // Note: We fall through and instead return intrinsicLogicalWidth() here -
    // to preserve existing WebKit behavior, which might or might not be
    // correct, or desired.
    // Changing this to return cDefaultWidth, will affect lots of test results.
    // Eg. some tests assume that a blank <img> tag (which implies
    // width/height=auto) has no intrinsic size, which is wrong per CSS 2.1, but
    // matches our behavior since a long time.
  }

  return ComputeReplacedLogicalWidthRespectingMinMaxWidth(
      IntrinsicLogicalWidth(), should_compute_preferred);
}

LayoutUnit LayoutReplaced::ComputeReplacedLogicalHeight(
    LayoutUnit estimated_used_width) const {
  NOT_DESTROYED();
  // 10.5 Content height: the 'height' property:
  // http://www.w3.org/TR/CSS21/visudet.html#propdef-height
  if (HasReplacedLogicalHeight()) {
    return ComputeReplacedLogicalHeightRespectingMinMaxHeight(
        ComputeReplacedLogicalHeightUsing(kMainOrPreferredSize,
                                          StyleRef().LogicalHeight()));
  }

  // 10.6.2 Inline, replaced elements:
  // http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-height
  IntrinsicSizingInfo intrinsic_sizing_info;
  ComputeIntrinsicSizingInfoForReplacedContent(intrinsic_sizing_info);

  gfx::SizeF constrained_size =
      ConstrainIntrinsicSizeToMinMax(intrinsic_sizing_info);

  bool width_is_auto = StyleRef().LogicalWidth().IsAuto();

  // If 'height' and 'width' both have computed values of 'auto' and the element
  // also has an intrinsic height, then that intrinsic height is the used value
  // of 'height'.
  if (width_is_auto && intrinsic_sizing_info.has_height) {
    return ComputeReplacedLogicalHeightRespectingMinMaxHeight(
        LayoutUnit(constrained_size.height()));
  }

  absl::optional<double> aspect_ratio;
  if (!intrinsic_sizing_info.aspect_ratio.IsEmpty()) {
    aspect_ratio = intrinsic_sizing_info.aspect_ratio.height() /
                   intrinsic_sizing_info.aspect_ratio.width();
  } else if (!StyleRef().AspectRatio().IsAuto() &&
             !intrinsic_sizing_info.has_height &&
             intrinsic_sizing_info.has_width) {
    aspect_ratio = StyleRef().AspectRatio().GetRatio().height() /
                   StyleRef().AspectRatio().GetRatio().width();
  }
  // Otherwise, if 'height' has a computed value of 'auto', and the element has
  // an intrinsic ratio then the used value of 'height' is:
  // (used width) / (intrinsic ratio)
  if (aspect_ratio) {
    LayoutUnit used_width =
        estimated_used_width ? estimated_used_width : AvailableLogicalWidth();
    NGBoxStrut border_padding(BorderStart() + ComputedCSSPaddingStart(),
                              BorderEnd() + ComputedCSSPaddingEnd(),
                              BorderBefore() + ComputedCSSPaddingBefore(),
                              BorderAfter() + ComputedCSSPaddingAfter());
    // Because used_size contains a content size, we need
    // to add border + padding for BlockSizeFromAspectRatio.
    EBoxSizing box_sizing = EBoxSizing::kContentBox;
    used_width += border_padding.InlineSum();
    if (StyleRef().AspectRatio().GetType() == EAspectRatioType::kRatio)
      box_sizing = StyleRef().BoxSizing();
    return ComputeReplacedLogicalHeightRespectingMinMaxHeight(
        BlockSizeFromAspectRatio(border_padding, *aspect_ratio, box_sizing,
                                 used_width) -
        border_padding.BlockSum());
  }

  // Otherwise, if 'height' has a computed value of 'auto', and the element has
  // an intrinsic height, then that intrinsic height is the used value of
  // 'height'.
  if (intrinsic_sizing_info.has_height) {
    return ComputeReplacedLogicalHeightRespectingMinMaxHeight(
        LayoutUnit(constrained_size.height()));
  }

  // Otherwise, if 'height' has a computed value of 'auto', but none of the
  // conditions above are met, then the used value of 'height' must be set to
  // the height of the largest rectangle that has a 2:1 ratio, has a height not
  // greater than 150px, and has a width not greater than the device width.
  return ComputeReplacedLogicalHeightRespectingMinMaxHeight(
      IntrinsicLogicalHeight());
}

MinMaxSizes LayoutReplaced::ComputeIntrinsicLogicalWidths() const {
  NOT_DESTROYED();
  MinMaxSizes sizes;
  sizes += BorderAndPaddingLogicalWidth() + IntrinsicLogicalWidth();
  return sizes;
}

MinMaxSizes LayoutReplaced::PreferredLogicalWidths() const {
  NOT_DESTROYED();
  MinMaxSizes sizes;

  // We cannot resolve some logical width here (i.e. percent, fill-available or
  // fit-content) as the available logical width may not be set on our
  // containing block.
  const Length& logical_width = StyleRef().LogicalWidth();
  if (logical_width.IsPercentOrCalc() || logical_width.IsFillAvailable() ||
      logical_width.IsFitContent()) {
    sizes = IntrinsicLogicalWidths();
    sizes -= BorderAndPaddingLogicalWidth();
  } else {
    sizes = ComputeReplacedLogicalWidth(kComputePreferred);
  }

  const ComputedStyle& style_to_use = StyleRef();
  if (style_to_use.LogicalWidth().IsPercentOrCalc() ||
      style_to_use.LogicalMaxWidth().IsPercentOrCalc())
    sizes.min_size = LayoutUnit();

  if (style_to_use.LogicalMinWidth().IsFixed() &&
      style_to_use.LogicalMinWidth().Value() > 0) {
    sizes.Encompass(AdjustContentBoxLogicalWidthForBoxSizing(
        style_to_use.LogicalMinWidth().Value()));
  }

  if (style_to_use.LogicalMaxWidth().IsFixed()) {
    sizes.Constrain(AdjustContentBoxLogicalWidthForBoxSizing(
        style_to_use.LogicalMaxWidth().Value()));
  }

  sizes += BorderAndPaddingLogicalWidth();
  return sizes;
}

static std::pair<LayoutUnit, LayoutUnit> SelectionTopAndBottom(
    const LayoutReplaced& layout_replaced) {
  // TODO(layout-dev): This code is buggy if the replaced element is relative
  // positioned.

  // The fallback answer when we can't find the containing line box of
  // |layout_replaced|.
  const std::pair<LayoutUnit, LayoutUnit> fallback(
      layout_replaced.LogicalTop(), layout_replaced.LogicalBottom());

  if (layout_replaced.IsInline() &&
      layout_replaced.IsInLayoutNGInlineFormattingContext()) {
    // Step 1: Find the line box containing |layout_replaced|.
    NGInlineCursor line_box;
    line_box.MoveTo(layout_replaced);
    if (!line_box)
      return fallback;
    line_box.MoveToContainingLine();
    if (!line_box)
      return fallback;

    // Step 2: Return the logical top and bottom of the line box.
    // TODO(layout-dev): Use selection top & bottom instead of line's, or decide
    // if we still want to distinguish line and selection heights in NG.
    const ComputedStyle& line_style = line_box.Current().Style();
    const auto writing_direction = line_style.GetWritingDirection();
    const WritingModeConverter converter(writing_direction,
                                         line_box.ContainerFragment().Size());
    PhysicalRect physical_rect = line_box.Current().RectInContainerFragment();
    // The caller expects it to be in the "stitched" coordinate space.
    physical_rect.offset +=
        OffsetInStitchedFragments(line_box.ContainerFragment());
    const LogicalRect logical_rect = converter.ToLogical(physical_rect);
    return {logical_rect.offset.block_offset, logical_rect.BlockEndOffset()};
  }

  InlineBox* box = layout_replaced.InlineBoxWrapper();
  RootInlineBox* root_box = box ? &box->Root() : nullptr;
  if (!root_box)
    return fallback;

  return {root_box->SelectionTop(), root_box->SelectionBottom()};
}

PositionWithAffinity LayoutReplaced::PositionForPoint(
    const PhysicalOffset& point) const {
  NOT_DESTROYED();

  auto [top, bottom] = SelectionTopAndBottom(*this);

  LayoutPoint flipped_point_in_container =
      LocationContainer()->FlipForWritingMode(point + PhysicalLocation());
  LayoutUnit block_direction_position = IsHorizontalWritingMode()
                                            ? flipped_point_in_container.Y()
                                            : flipped_point_in_container.X();
  LayoutUnit line_direction_position = IsHorizontalWritingMode()
                                           ? flipped_point_in_container.X()
                                           : flipped_point_in_container.Y();

  if (block_direction_position < top)
    return PositionBeforeThis();  // coordinates are above

  if (block_direction_position >= bottom)
    return PositionBeforeThis();  // coordinates are below

  if (GetNode()) {
    const bool is_at_left_side =
        line_direction_position <= LogicalLeft() + (LogicalWidth() / 2);
    const bool is_at_start = is_at_left_side == IsLtr(ResolvedDirection());
    if (is_at_start)
      return PositionBeforeThis();
    return PositionAfterThis();
  }

  return LayoutBox::PositionForPoint(point);
}

PhysicalRect LayoutReplaced::LocalSelectionVisualRect() const {
  NOT_DESTROYED();
  if (GetSelectionState() == SelectionState::kNone ||
      GetSelectionState() == SelectionState::kContain) {
    return PhysicalRect();
  }

  if (IsInline() && IsInLayoutNGInlineFormattingContext()) {
    PhysicalRect rect;
    NGInlineCursor cursor;
    cursor.MoveTo(*this);
    for (; cursor; cursor.MoveToNextForSameLayoutObject())
      rect.Unite(cursor.CurrentLocalSelectionRectForReplaced());
    return rect;
  }

  if (!InlineBoxWrapper()) {
    // We're a block-level replaced element.  Just return our own dimensions.
    return PhysicalRect(PhysicalOffset(), Size());
  }

  RootInlineBox& root = InlineBoxWrapper()->Root();
  LayoutUnit new_logical_top =
      root.Block().StyleRef().IsFlippedBlocksWritingMode()
          ? InlineBoxWrapper()->LogicalBottom() - root.SelectionBottom()
          : root.SelectionTop() - InlineBoxWrapper()->LogicalTop();
  if (root.Block().StyleRef().IsHorizontalWritingMode()) {
    return PhysicalRect(LayoutUnit(), new_logical_top, Size().Width(),
                        root.SelectionHeight());
  }
  return PhysicalRect(new_logical_top, LayoutUnit(), root.SelectionHeight(),
                      Size().Height());
}

bool LayoutReplaced::RespectsCSSOverflow() const {
  const Element* element = DynamicTo<Element>(GetNode());
  return element && element->IsReplacedElementRespectingCSSOverflow();
}

bool LayoutReplaced::ClipsToContentBox() const {
  if (!RespectsCSSOverflow()) {
    // If an svg is clipped, it is guaranteed to be clipped to the element's
    // content box.
    if (IsSVGRoot())
      return GetOverflowClipAxes() == kOverflowClipBothAxis;
    return true;
  }

  // TODO(khushalsagar): There can be more cases where the content clips to
  // content box. For instance, when padding is 0 and the reference box is the
  // padding box.
  const auto& overflow_clip_margin = StyleRef().OverflowClipMargin();
  return GetOverflowClipAxes() == kOverflowClipBothAxis &&
         overflow_clip_margin &&
         overflow_clip_margin->GetReferenceBox() ==
             StyleOverflowClipMargin::ReferenceBox::kContentBox &&
         !overflow_clip_margin->GetMargin();
}

}  // namespace blink
