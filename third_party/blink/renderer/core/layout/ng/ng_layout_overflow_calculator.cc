// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_overflow_calculator.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text_combine.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/style/style_overflow_clip_margin.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect_outsets.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

// static
PhysicalRect NGLayoutOverflowCalculator::RecalculateLayoutOverflowForFragment(
    const NGPhysicalBoxFragment& fragment,
    bool has_block_fragmentation) {
  DCHECK(!fragment.IsLegacyLayoutRoot() ||
         fragment.GetLayoutObject()->IsMedia());
  const NGBlockNode node(const_cast<LayoutBox*>(
      To<LayoutBox>(fragment.GetSelfOrContainerLayoutObject())));
  const WritingDirectionMode writing_direction =
      node.Style().GetWritingDirection();

  // TODO(ikilpatrick): The final computed scrollbars for a fragment should
  // likely live on the NGPhysicalBoxFragment.
  NGPhysicalBoxStrut scrollbar;
  if (fragment.IsCSSBox()) {
    scrollbar = ComputeScrollbarsForNonAnonymous(node).ConvertToPhysical(
        writing_direction);
  }

  NGLayoutOverflowCalculator calculator(
      node, fragment.IsCSSBox(), has_block_fragmentation, fragment.Borders(),
      scrollbar, fragment.Padding(), fragment.Size(), writing_direction);

  if (const NGFragmentItems* items = fragment.Items())
    calculator.AddItems(fragment, *items);

  for (const auto& child : fragment.PostLayoutChildren()) {
    const auto* box_fragment =
        DynamicTo<NGPhysicalBoxFragment>(*child.fragment);
    if (!box_fragment)
      continue;

    if (box_fragment->IsFragmentainerBox()) {
      // When this function is called nothing has updated the layout-overflow
      // of any fragmentainers (as they are not directly associated with a
      // layout-object). Recalculate their layout-overflow directly.
      PhysicalRect child_overflow = RecalculateLayoutOverflowForFragment(
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

NGLayoutOverflowCalculator::NGLayoutOverflowCalculator(
    const NGBlockNode& node,
    bool is_css_box,
    bool has_block_fragmentation,
    const NGPhysicalBoxStrut& borders,
    const NGPhysicalBoxStrut& scrollbar,
    const NGPhysicalBoxStrut& padding,
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
  layout_overflow_ = padding_rect_;
}

const PhysicalRect NGLayoutOverflowCalculator::Result(
    const absl::optional<PhysicalRect> inflow_bounds) {
  if (!inflow_bounds || !is_scroll_container_)
    return layout_overflow_;

  PhysicalOffset start_offset = inflow_bounds->MinXMinYCorner() -
                                PhysicalOffset(padding_.left, padding_.top);
  PhysicalOffset end_offset = inflow_bounds->MaxXMaxYCorner() +
                              PhysicalOffset(padding_.right, padding_.bottom);

  PhysicalRect inflow_overflow = {
      start_offset, PhysicalSize(end_offset.left - start_offset.left,
                                 end_offset.top - start_offset.top)};
  inflow_overflow = AdjustOverflowForScrollOrigin(inflow_overflow);

  layout_overflow_.UniteEvenIfEmpty(inflow_overflow);
  return layout_overflow_;
}

void NGLayoutOverflowCalculator::AddTableSelfRect() {
  AddOverflow({PhysicalOffset(), size_});
}

template <typename Items>
void NGLayoutOverflowCalculator::AddItemsInternal(
    const LayoutObject* layout_object,
    const Items& items) {
  bool has_hanging = false;
  PhysicalRect line_rect;

  // |LayoutNGTextCombine| doesn't not cause layout overflow because combined
  // text fits in 1em by using width variant font or scaling.
  if (UNLIKELY(IsA<LayoutNGTextCombine>(layout_object)))
    return;

  for (const auto& item : items) {
    if (const auto* line_box = item->LineBoxFragment()) {
      has_hanging = line_box->HasHanging();
      line_rect = item->RectInContainerFragment();

      if (line_rect.IsEmpty())
        continue;

      layout_overflow_.UniteEvenIfEmpty(line_rect);
      continue;
    }

    if (item->IsText()) {
      PhysicalRect child_overflow = item->RectInContainerFragment();

      // Adjust the text's overflow if the line-box has hanging.
      if (UNLIKELY(has_hanging))
        child_overflow = AdjustOverflowForHanging(line_rect, child_overflow);

      AddOverflow(child_overflow);
      continue;
    }

    if (const auto* child_box_fragment = item->BoxFragment()) {
      // Use the default box-fragment overflow logic.
      PhysicalRect child_overflow =
          LayoutOverflowForPropagation(*child_box_fragment);
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

void NGLayoutOverflowCalculator::AddItems(
    const LayoutObject* layout_object,
    const NGFragmentItemsBuilder::ItemWithOffsetList& items) {
  AddItemsInternal(layout_object, items);
}

void NGLayoutOverflowCalculator::AddItems(
    const NGPhysicalBoxFragment& box_fragment,
    const NGFragmentItems& items) {
  AddItemsInternal(box_fragment.GetLayoutObject(), items.Items());
}

PhysicalRect NGLayoutOverflowCalculator::AdjustOverflowForHanging(
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

PhysicalRect NGLayoutOverflowCalculator::AdjustOverflowForScrollOrigin(
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

PhysicalRect NGLayoutOverflowCalculator::LayoutOverflowForPropagation(
    const NGPhysicalBoxFragment& child_fragment) {
  // If the fragment is anonymous, just return its layout-overflow (don't apply
  // any incorrect transforms, etc).
  if (!child_fragment.IsCSSBox())
    return child_fragment.LayoutOverflow();

  PhysicalRect overflow = {{}, child_fragment.Size()};

  // Collapsed table rows/sections set IsHiddenForPaint flag.
  bool ignore_layout_overflow =
      child_fragment.ShouldApplyLayoutContainment() ||
      child_fragment.IsInlineBox() ||
      (child_fragment.ShouldClipOverflowAlongBothAxis() &&
       !child_fragment.ShouldApplyOverflowClipMargin()) ||
      child_fragment.IsHiddenForPaint();

  if (!ignore_layout_overflow) {
    PhysicalRect child_overflow = child_fragment.LayoutOverflow();
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
  if (absl::optional<gfx::Transform> transform =
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
