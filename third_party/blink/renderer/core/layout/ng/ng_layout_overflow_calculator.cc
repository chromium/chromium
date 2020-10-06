// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_overflow_calculator.h"

#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"

namespace blink {

// static
PhysicalRect NGLayoutOverflowCalculator::RecalculateLayoutOverflowForFragment(
    const NGPhysicalBoxFragment& fragment) {
  DCHECK(!fragment.IsLegacyLayoutRoot());
  const NGBlockNode node(const_cast<LayoutBox*>(
      ToLayoutBox(fragment.GetSelfOrContainerLayoutObject())));
  const WritingDirectionMode writing_direction =
      node.Style().GetWritingDirection();

  // TODO(ikilpatrick): The final computed scrollbars for a fragment should
  // likely live on the NGPhysicalBoxFragment.
  NGPhysicalBoxStrut scrollbar;
  if (fragment.IsCSSBox()) {
    scrollbar = ComputeScrollbarsForNonAnonymous(node).ConvertToPhysical(
        writing_direction.GetWritingMode(), writing_direction.Direction());
  }

  NGLayoutOverflowCalculator calculator(
      node, fragment.IsCSSBox(), fragment.Borders(), scrollbar,
      fragment.Padding(), fragment.Size(), writing_direction);

  if (const NGFragmentItems* items = fragment.Items())
    calculator.AddItems(*items);

  for (const auto& child : fragment.PostLayoutChildren()) {
    const auto* box_fragment =
        DynamicTo<NGPhysicalBoxFragment>(*child.fragment);
    if (!box_fragment)
      continue;

    if (box_fragment->IsFragmentainerBox()) {
      // When this function is called nothing has updated the layout-overflow
      // of any fragmentainers (as they are not directly associated with a
      // layout-object). Recalculate their layout-overflow directly.
      PhysicalRect child_overflow =
          RecalculateLayoutOverflowForFragment(*box_fragment);
      child_overflow.offset += child.offset;
      calculator.AddOverflow(child_overflow);
    } else {
      calculator.AddChild(*box_fragment, child.offset);
    }
  }

  return calculator.Result(fragment.InflowBounds());
}

NGLayoutOverflowCalculator::NGLayoutOverflowCalculator(
    const NGBlockNode& node,
    bool is_css_box,
    const NGPhysicalBoxStrut& borders,
    const NGPhysicalBoxStrut& scrollbar,
    const NGPhysicalBoxStrut& padding,
    PhysicalSize size,
    WritingDirectionMode writing_direction)
    : node_(node),
      writing_direction_(writing_direction),
      is_scroll_container_(is_css_box && node_.IsScrollContainer()),
      has_left_overflow_(is_css_box && node_.HasLeftOverflow()),
      has_top_overflow_(is_css_box && node_.HasTopOverflow()),
      has_non_visible_overflow_(is_css_box && node_.HasNonVisibleOverflow()),
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
    const base::Optional<PhysicalRect> inflow_bounds) {
  // Adjust the layout-overflow if we have "overflow: clip" present.
  if (!is_scroll_container_ && has_non_visible_overflow_) {
    const OverflowClipAxes overflow_clip_axes = node_.GetOverflowClipAxes();
    if (overflow_clip_axes & kOverflowClipX) {
      layout_overflow_.offset.left = padding_rect_.offset.left;
      layout_overflow_.size.width = padding_rect_.size.width;
    }
    if (overflow_clip_axes & kOverflowClipY) {
      layout_overflow_.offset.top = padding_rect_.offset.top;
      layout_overflow_.size.height = padding_rect_.size.height;
    }
  }

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

  PhysicalRect normal_overflow = layout_overflow_;
  normal_overflow.UniteEvenIfEmpty(inflow_overflow);

  if (node_.IsInlineFormattingContextRoot())
    return normal_overflow;

  WritingModeConverter converter(writing_direction_, size_);

  LogicalRect block_end_padding_rect = {
      LogicalOffset(converter.ToLogical(padding_rect_).offset.inline_offset,
                    converter.ToLogical(*inflow_bounds).BlockEndOffset()),
      LogicalSize(LayoutUnit(),
                  padding_
                      .ConvertToLogical(writing_direction_.GetWritingMode(),
                                        writing_direction_.Direction())
                      .block_end)};

  PhysicalRect alternate_overflow = layout_overflow_;
  alternate_overflow.UniteEvenIfEmpty(AdjustOverflowForScrollOrigin(
      converter.ToPhysical(block_end_padding_rect)));

  // We'd like everything to be |normal_overflow|, lets see what the impact
  // would be.
  if (node_.Style().OverflowInlineDirection() == EOverflow::kAuto ||
      node_.Style().OverflowInlineDirection() == EOverflow::kScroll) {
    if (alternate_overflow.size.width != normal_overflow.size.width) {
      if (alternate_overflow.size.width != padding_rect_.size.width) {
        UseCounter::Count(
            node_.GetDocument(),
            node_.IsFlexibleBox()
                ? WebFeature::kNewLayoutOverflowDifferentAndAlreadyScrollsFlex
                : WebFeature::
                      kNewLayoutOverflowDifferentAndAlreadyScrollsBlock);
      } else {
        UseCounter::Count(node_.GetDocument(),
                          node_.IsFlexibleBox()
                              ? WebFeature::kNewLayoutOverflowDifferentFlex
                              : WebFeature::kNewLayoutOverflowDifferentBlock);
      }
    }
  }

  return alternate_overflow;
}

template <typename Items>
void NGLayoutOverflowCalculator::AddItemsInternal(const Items& items) {
  bool has_hanging = false;
  PhysicalRect line_rect;

  for (const auto& item : items) {
    if (const auto* line_box = item->LineBoxFragment()) {
      has_hanging = line_box->HasHanging();
      line_rect = item->RectInContainerBlock();

      if (line_rect.IsEmpty())
        continue;

      // Currently line-boxes don't contribute overflow in the block-axis. This
      // was added for web-compat reasons.
      PhysicalRect child_overflow = line_rect;
      if (writing_direction_.IsHorizontal())
        child_overflow.size.height = LayoutUnit();
      else
        child_overflow.size.width = LayoutUnit();

      layout_overflow_.UniteEvenIfEmpty(child_overflow);
      continue;
    }

    if (item->IsText()) {
      PhysicalRect child_overflow = item->RectInContainerBlock();

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
      child_overflow.offset += item->OffsetInContainerBlock();

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
    const NGFragmentItemsBuilder::ItemWithOffsetList& items) {
  AddItemsInternal(items);
}

void NGLayoutOverflowCalculator::AddItems(const NGFragmentItems& items) {
  AddItemsInternal(items.Items());
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

  // Children with overflow clip (e.g. a scrollable child) don't propagate any
  // layout overflow.
  PhysicalRect overflow = {{}, child_fragment.Size()};
  if (!child_fragment.ShouldApplyLayoutContainment() &&
      !child_fragment.ShouldClipOverflowAlongBothAxis() &&
      !child_fragment.IsInlineBox())
    overflow.UniteEvenIfEmpty(child_fragment.LayoutOverflow());

  // Apply any transforms to the overflow.
  if (base::Optional<TransformationMatrix> transform =
          node_.GetTransformForChildFragment(child_fragment, size_)) {
    overflow =
        PhysicalRect::EnclosingRect(transform->MapRect(FloatRect(overflow)));
  }

  return overflow;
}

}  // namespace blink
