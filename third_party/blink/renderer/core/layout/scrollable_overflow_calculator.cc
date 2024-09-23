// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/scrollable_overflow_calculator.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/logical_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_fragment.h"
#include "third_party/blink/renderer/core/style/style_overflow_clip_margin.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

// static
PhysicalRect
ScrollableOverflowCalculator::RecalculateScrollableOverflowForFragment(
    const PhysicalBoxFragment& fragment,
    bool has_block_fragmentation) {
  const BlockNode node(const_cast<LayoutBox*>(
      To<LayoutBox>(fragment.GetSelfOrContainerLayoutObject())));
  DCHECK(!node.IsReplaced() || node.IsMedia());

  const WritingDirectionMode writing_direction =
      node.Style().GetWritingDirection();

  ScrollableOverflowCalculator calculator(
      node, fragment.IsCSSBox(), has_block_fragmentation, fragment.Borders(),
      fragment.Scrollbar(), fragment.Padding(), fragment.Size(),
      writing_direction);

  if (const FragmentItems* items = fragment.Items()) {
    calculator.AddItems(fragment, *items);
  }

  for (const auto& child : fragment.PostLayoutChildren()) {
    const auto* box_fragment = DynamicTo<PhysicalBoxFragment>(*child.fragment);
    if (!box_fragment)
      continue;

    if (box_fragment->IsFragmentainerBox()) {
      // When this function is called nothing has updated the
      // scrollable-overflow of any fragmentainers (as they are not directly
      // associated with a layout-object). Recalculate their scrollable-overflow
      // directly.
      PhysicalRect child_overflow = RecalculateScrollableOverflowForFragment(
          *box_fragment, has_block_fragmentation);
      child_overflow.offset += child.offset;
      calculator.AddOverflow(child_overflow, /* child_is_fragmentainer */ true);
    } else {
      calculator.AddChild(*box_fragment, child.offset);
    }
  }

  if (fragment.TableCollapsedBorders())
    calculator.AddTableSelfRect();

  return calculator.Result(fragment.InflowBounds());
}

ScrollableOverflowCalculator::ScrollableOverflowCalculator(
    const BlockNode& node,
    bool is_css_box,
    bool has_block_fragmentation,
    const PhysicalBoxStrut& borders,
    const PhysicalBoxStrut& scrollbar,
    const PhysicalBoxStrut& padding,
    PhysicalSize size,
    WritingDirectionMode writing_direction)
    : node_(node),
      writing_direction_(writing_direction),
      is_scroll_container_(is_css_box && node_.IsScrollContainer()),
      is_view_(node_.IsView()),
      has_left_overflow_(is_css_box && node_.HasLeftOverflow()),
      has_top_overflow_(is_css_box && node_.HasTopOverflow()),
      has_non_visible_overflow_(is_css_box && node_.HasNonVisibleOverflow()),
      has_block_fragmentation_(has_block_fragmentation),
      padding_(padding),
      size_(size) {
  const auto border_scrollbar = borders + scrollbar;

  // TODO(layout-dev): This isn't correct for <fieldset> elements as we may
  // have a legend which is taller than the block-start border.
  padding_rect_ = {PhysicalOffset(border_scrollbar.left, border_scrollbar.top),
                   PhysicalSize((size_.width - border_scrollbar.HorizontalSum())
                                    .ClampNegativeToZero(),
                                (size_.height - border_scrollbar.VerticalSum())
                                    .ClampNegativeToZero())};
  scrollable_overflow_ = padding_rect_;
}

const PhysicalRect ScrollableOverflowCalculator::Result(
    const std::optional<PhysicalRect> inflow_bounds) {
  if (!inflow_bounds || !is_scroll_container_)
    return scrollable_overflow_;

  PhysicalOffset start_offset = inflow_bounds->MinXMinYCorner() -
                                PhysicalOffset(padding_.left, padding_.top);
  PhysicalOffset end_offset = inflow_bounds->MaxXMaxYCorner() +
                              PhysicalOffset(padding_.right, padding_.bottom);

  PhysicalRect inflow_overflow = {
      start_offset, PhysicalSize(end_offset.left - start_offset.left,
                                 end_offset.top - start_offset.top)};
  inflow_overflow = AdjustOverflowForScrollOrigin(inflow_overflow);

  scrollable_overflow_.UniteEvenIfEmpty(inflow_overflow);
  return scrollable_overflow_;
}

void ScrollableOverflowCalculator::AddTableSelfRect() {
  AddOverflow({PhysicalOffset(), size_});
}

template <typename Items>
void ScrollableOverflowCalculator::AddItemsInternal(
    const LayoutObject* layout_object,
    const Items& items) {
  bool has_hanging = false;
  PhysicalRect line_rect;

  // |LayoutTextCombine| doesn't not cause scrollable overflow because
  // combined text fits in 1em by using width variant font or scaling.
  if (IsA<LayoutTextCombine>(layout_object)) [[unlikely]] {
    return;
  }

  for (const auto& item : items) {
    if (item->IsHiddenForPaint()) {
      continue;
    }

    if (const auto* line_box = item->LineBoxFragment()) {
      has_hanging = line_box->HasHanging();
      line_rect = item->RectInContainerFragment();

      if (line_rect.IsEmpty())
        continue;

      scrollable_overflow_.UniteEvenIfEmpty(line_rect);
      continue;
    }

    if (item->IsText()) {
      PhysicalRect child_overflow = item->RectInContainerFragment();

      // Adjust the text's overflow if the line-box has hanging.
      if (has_hanging) [[unlikely]] {
        child_overflow = AdjustOverflowForHanging(line_rect, child_overflow);
      }

      AddOverflow(child_overflow);
      continue;
    }

    if (const auto* child_box_fragment = item->BoxFragment()) {
      // Use the default box-fragment overflow logic.
      PhysicalRect child_overflow =
          ScrollableOverflowForPropagation(*child_box_fragment);
      child_overflow.offset += item->OffsetInContainerFragment();

      // Only inline-boxes (not atomic-inlines) should be adjusted if the
      // line-box has hanging.
      if (child_box_fragment->IsInlineBox() && has_hanging)
        child_overflow = AdjustOverflowForHanging(line_rect, child_overflow);

      AddOverflow(child_overflow);
      continue;
    }
  }
}

void ScrollableOverflowCalculator::AddItems(
    const LayoutObject* layout_object,
    const FragmentItemsBuilder::ItemWithOffsetList& items) {
  AddItemsInternal(layout_object, items);
}

void ScrollableOverflowCalculator::AddItems(
    const PhysicalBoxFragment& box_fragment,
    const FragmentItems& items) {
  AddItemsInternal(box_fragment.GetLayoutObject(), items.Items());
}

PhysicalRect ScrollableOverflowCalculator::AdjustOverflowForHanging(
    const PhysicalRect& line_rect,
    PhysicalRect overflow) {
  if (writing_direction_.IsHorizontal()) {
    if (overflow.offset.left < line_rect.offset.left)
      overflow.offset.left = line_rect.offset.left;
    if (overflow.Right() > line_rect.Right())
      overflow.ShiftRightEdgeTo(line_rect.Right());
  } else {
    if (overflow.offset.top < line_rect.offset.top)
      overflow.offset.top = line_rect.offset.top;
    if (overflow.Bottom() > line_rect.Bottom())
      overflow.ShiftBottomEdgeTo(line_rect.Bottom());
  }

  return overflow;
}

PhysicalRect ScrollableOverflowCalculator::AdjustOverflowForScrollOrigin(
    const PhysicalRect& overflow) {
  LayoutUnit left_offset =
      has_left_overflow_
          ? std::min(padding_rect_.Right(), overflow.offset.left)
          : std::max(padding_rect_.offset.left, overflow.offset.left);

  LayoutUnit right_offset =
      has_left_overflow_
          ? std::min(padding_rect_.Right(), overflow.Right())
          : std::max(padding_rect_.offset.left, overflow.Right());

  LayoutUnit top_offset =
      has_top_overflow_
          ? std::min(padding_rect_.Bottom(), overflow.offset.top)
          : std::max(padding_rect_.offset.top, overflow.offset.top);

  LayoutUnit bottom_offset =
      has_top_overflow_ ? std::min(padding_rect_.Bottom(), overflow.Bottom())
                        : std::max(padding_rect_.offset.top, overflow.Bottom());

  return {PhysicalOffset(left_offset, top_offset),
          PhysicalSize(right_offset - left_offset, bottom_offset - top_offset)};
}

PhysicalRect ScrollableOverflowCalculator::ScrollableOverflowForPropagation(
    const PhysicalBoxFragment& child_fragment) {
  if (child_fragment.IsHiddenForPaint()) {
    return {};
  }

  // If the fragment is anonymous, just return its scrollable-overflow (don't
  // apply any incorrect transforms, etc).
  if (!child_fragment.IsCSSBox())
    return child_fragment.ScrollableOverflow();

  PhysicalRect overflow = {{}, child_fragment.Size()};

  bool ignore_scrollable_overflow =
      child_fragment.ShouldApplyLayoutContainment() ||
      child_fragment.IsInlineBox() ||
      (child_fragment.ShouldClipOverflowAlongBothAxis() &&
       !child_fragment.ShouldApplyOverflowClipMargin());

  if (!ignore_scrollable_overflow) {
    PhysicalRect child_overflow = child_fragment.ScrollableOverflow();
    if (child_fragment.HasNonVisibleOverflow()) {
      const OverflowClipAxes overflow_clip_axes =
          child_fragment.GetOverflowClipAxes();
      if (child_fragment.ShouldApplyOverflowClipMargin()) {
        // ShouldApplyOverflowClipMargin should only be true if we're clipping
        // overflow in both axes.
        DCHECK_EQ(overflow_clip_axes, kOverflowClipBothAxis);
        PhysicalRect child_overflow_rect({}, child_fragment.Size());
        child_overflow_rect.Expand(child_fragment.OverflowClipMarginOutsets());
        child_overflow.Intersect(child_overflow_rect);
      } else {
        if (overflow_clip_axes & kOverflowClipX) {
          child_overflow.offset.left = LayoutUnit();
          child_overflow.size.width = child_fragment.Size().width;
        }
        if (overflow_clip_axes & kOverflowClipY) {
          child_overflow.offset.top = LayoutUnit();
          child_overflow.size.height = child_fragment.Size().height;
        }
      }
    }
    overflow.UniteEvenIfEmpty(child_overflow);
  }

  // Apply any transforms to the overflow.
  if (std::optional<gfx::Transform> transform =
          node_.GetTransformForChildFragment(child_fragment, size_)) {
    overflow =
        PhysicalRect::EnclosingRect(transform->MapRect(gfx::RectF(overflow)));
  }

  if (has_block_fragmentation_ && child_fragment.IsOutOfFlowPositioned()) {
    // If the containing block of an out-of-flow positioned box is inside a
    // clipped-overflow container inside a fragmentation context, we shouldn't
    // propagate overflow. Nothing will be painted on the outside of the clipped
    // ancestor anyway, and we don't need to worry about scrollable area
    // contribution, since scrollable containers are monolithic.
    LayoutObject::AncestorSkipInfo skip_info(node_.GetLayoutBox());
    OverflowClipAxes clipped_axes = kNoOverflowClip;
    for (const LayoutObject* walker =
             child_fragment.GetLayoutObject()->ContainingBlock(&skip_info);
         walker != node_.GetLayoutBox() && !skip_info.AncestorSkipped();
         walker = walker->ContainingBlock(&skip_info)) {
      if (OverflowClipAxes axes_to_clip = walker->GetOverflowClipAxes()) {
        // Shrink the overflow rectangle to be at most 1px large along the axes
        // to be clipped. Unconditionally setting it to 0 would prevent us from
        // propagating overflow along any non-clipped axis.
        if (axes_to_clip & kOverflowClipX) {
          overflow.offset.left = LayoutUnit();
          overflow.size.width = std::min(overflow.size.width, LayoutUnit(1));
        }
        if (axes_to_clip & kOverflowClipY) {
          overflow.offset.top = LayoutUnit();
          overflow.size.height = std::min(overflow.size.height, LayoutUnit(1));
        }
        clipped_axes |= axes_to_clip;
        if (clipped_axes == kOverflowClipBothAxis) {
          break;
        }
      }
    }
  }

  return overflow;
}

}  // namespace blink
