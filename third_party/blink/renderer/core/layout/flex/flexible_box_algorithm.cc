/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/flex/flexible_box_algorithm.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/flex/ng_flex_line.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {
namespace {

ItemPosition BoxAlignmentToItemPosition(EBoxAlignment alignment) {
  switch (alignment) {
    case EBoxAlignment::kBaseline:
      return ItemPosition::kBaseline;
    case EBoxAlignment::kCenter:
      return ItemPosition::kCenter;
    case EBoxAlignment::kStretch:
      return ItemPosition::kStretch;
    case EBoxAlignment::kStart:
      return ItemPosition::kFlexStart;
    case EBoxAlignment::kEnd:
      return ItemPosition::kFlexEnd;
  }
}

ContentPosition BoxPackToContentPosition(EBoxPack box_pack) {
  switch (box_pack) {
    case EBoxPack::kCenter:
      return ContentPosition::kCenter;
    case EBoxPack::kJustify:
      return ContentPosition::kFlexStart;
    case EBoxPack::kStart:
      return ContentPosition::kFlexStart;
    case EBoxPack::kEnd:
      return ContentPosition::kFlexEnd;
  }
}

ContentDistributionType BoxPackToContentDistribution(EBoxPack box_pack) {
  return box_pack == EBoxPack::kJustify ? ContentDistributionType::kSpaceBetween
                                        : ContentDistributionType::kDefault;
}

}  // namespace

// static
LayoutUnit FlexibleBoxAlgorithm::AlignmentOffset(
    LayoutUnit available_free_space,
    ItemPosition position,
    LayoutUnit baseline_offset,
    bool is_wrap_reverse) {
  switch (position) {
    case ItemPosition::kLegacy:
    case ItemPosition::kAuto:
    case ItemPosition::kNormal:
    case ItemPosition::kAnchorCenter:
      NOTREACHED();
    case ItemPosition::kSelfStart:
    case ItemPosition::kSelfEnd:
    case ItemPosition::kStart:
    case ItemPosition::kEnd:
    case ItemPosition::kLeft:
    case ItemPosition::kRight:
      NOTREACHED() << static_cast<int>(position)
                   << " AlignmentForChild should have transformed this "
                      "position value to something we handle below.";
    case ItemPosition::kStretch:
      // Actual stretching must be handled by the caller. Since wrap-reverse
      // flips cross start and cross end, stretch children should be aligned
      // with the cross end. This matters because applyStretchAlignment
      // doesn't always stretch or stretch fully (explicit cross size given, or
      // stretching constrained by max-height/max-width). For flex-start and
      // flex-end this is handled by alignmentForChild().
      if (is_wrap_reverse)
        return available_free_space;
      break;
    case ItemPosition::kFlexStart:
      break;
    case ItemPosition::kFlexEnd:
      return available_free_space;
    case ItemPosition::kCenter:
      return available_free_space / 2;
    case ItemPosition::kBaseline:
    case ItemPosition::kLastBaseline:
      return baseline_offset;
  }
  return LayoutUnit();
}

// static
LayoutUnit FlexibleBoxAlgorithm::GapBetweenItems(
    const ComputedStyle& style,
    LogicalSize percent_resolution_sizes) {
  if (IsColumnFlow(style)) {
    if (const std::optional<Length>& row_gap = style.RowGap()) {
      return MinimumValueForLength(
          *row_gap,
          percent_resolution_sizes.block_size.ClampIndefiniteToZero());
    }
    return LayoutUnit();
  }
  if (const std::optional<Length>& column_gap = style.ColumnGap()) {
    return MinimumValueForLength(
        *column_gap,
        percent_resolution_sizes.inline_size.ClampIndefiniteToZero());
  }
  return LayoutUnit();
}

// static
LayoutUnit FlexibleBoxAlgorithm::GapBetweenLines(
    const ComputedStyle& style,
    LogicalSize percent_resolution_sizes) {
  if (!IsColumnFlow(style)) {
    if (const std::optional<Length>& row_gap = style.RowGap()) {
      return MinimumValueForLength(
          *row_gap,
          percent_resolution_sizes.block_size.ClampIndefiniteToZero());
    }
    return LayoutUnit();
  }
  if (const std::optional<Length>& column_gap = style.ColumnGap()) {
    return MinimumValueForLength(
        *column_gap,
        percent_resolution_sizes.inline_size.ClampIndefiniteToZero());
  }
  return LayoutUnit();
}

FlexibleBoxAlgorithm::FlexibleBoxAlgorithm(const ComputedStyle* style,
                                           LayoutUnit line_break_length,
                                           LogicalSize percent_resolution_sizes,
                                           Document* document)
    : gap_between_items_(GapBetweenItems(*style, percent_resolution_sizes)),
      gap_between_lines_(GapBetweenLines(*style, percent_resolution_sizes)),
      line_break_length_(line_break_length),
      next_item_index_(0) {
  DCHECK_GE(gap_between_items_, 0);
  DCHECK_GE(gap_between_lines_, 0);
  const auto& row_gap = style->RowGap();
  const auto& column_gap = style->ColumnGap();
  if (row_gap || column_gap) {
    UseCounter::Count(document, WebFeature::kFlexGapSpecified);
    if (gap_between_items_ || gap_between_lines_)
      UseCounter::Count(document, WebFeature::kFlexGapPositive);
  }

  if (row_gap && row_gap->HasPercent()) {
    UseCounter::Count(document, WebFeature::kFlexRowGapPercent);
    if (percent_resolution_sizes.block_size == LayoutUnit(-1))
      UseCounter::Count(document, WebFeature::kFlexRowGapPercentIndefinite);
  }
}

FlexLine* FlexibleBoxAlgorithm::ComputeNextFlexLine(bool is_multi_line) {
  LayoutUnit sum_flex_base_size;
  LayoutUnit sum_hypothetical_main_size;

  bool line_has_in_flow_item = false;

  wtf_size_t start_index = next_item_index_;

  for (; next_item_index_ < all_items_.size(); ++next_item_index_) {
    FlexItem& flex_item = all_items_[next_item_index_];
    if (is_multi_line &&
        sum_hypothetical_main_size +
                flex_item.HypotheticalMainAxisMarginBoxSize() >
            line_break_length_ &&
        line_has_in_flow_item) {
      break;
    }
    line_has_in_flow_item = true;
    sum_flex_base_size +=
        flex_item.FlexBaseMarginBoxSize() + gap_between_items_;
    sum_hypothetical_main_size +=
        flex_item.HypotheticalMainAxisMarginBoxSize() + gap_between_items_;
  }
  if (line_has_in_flow_item) {
    // We added a gap after every item but there shouldn't be one after the last
    // item, so subtract it here.
    // Note: the two sums here can be negative because of negative margins.
    sum_hypothetical_main_size -= gap_between_items_;
    sum_flex_base_size -= gap_between_items_;
  }

  DCHECK(next_item_index_ > start_index ||
         next_item_index_ == all_items_.size());
  if (next_item_index_ > start_index) {
    return &flex_lines_.emplace_back(
        FlexItemVectorView(&all_items_, start_index, next_item_index_),
        sum_flex_base_size, sum_hypothetical_main_size);
  }
  return nullptr;
}

// static
bool FlexibleBoxAlgorithm::IsColumnFlow(const ComputedStyle& style) {
  return style.ResolvedIsColumnFlexDirection();
}

// static
bool FlexibleBoxAlgorithm::IsHorizontalFlow(const ComputedStyle& style) {
  if (style.IsHorizontalWritingMode())
    return !style.ResolvedIsColumnFlexDirection();
  return style.ResolvedIsColumnFlexDirection();
}

// static
const StyleContentAlignmentData&
FlexibleBoxAlgorithm::ContentAlignmentNormalBehavior() {
  // The justify-content property applies along the main axis, but since
  // flexing in the main axis is controlled by flex, stretch behaves as
  // flex-start (ignoring the specified fallback alignment, if any).
  // https://drafts.csswg.org/css-align/#distribution-flex
  static const StyleContentAlignmentData kNormalBehavior = {
      ContentPosition::kNormal, ContentDistributionType::kStretch};
  return kNormalBehavior;
}

// static
StyleContentAlignmentData FlexibleBoxAlgorithm::ResolvedJustifyContent(
    const ComputedStyle& style) {
  const bool is_webkit_box = style.IsDeprecatedWebkitBox();
  ContentPosition position;
  if (is_webkit_box) {
    position = BoxPackToContentPosition(style.BoxPack());
    // As row-reverse does layout in reverse, it effectively swaps end & start.
    // -webkit-box didn't do this (-webkit-box always did layout starting at 0,
    // and increasing).
    if (style.ResolvedIsRowReverseFlexDirection()) {
      if (position == ContentPosition::kFlexEnd)
        position = ContentPosition::kFlexStart;
      else if (position == ContentPosition::kFlexStart)
        position = ContentPosition::kFlexEnd;
    }
  } else {
    position =
        style.ResolvedJustifyContentPosition(ContentAlignmentNormalBehavior());
  }
  if (position == ContentPosition::kLeft ||
      position == ContentPosition::kRight) {
    if (IsColumnFlow(style)) {
      if (style.IsHorizontalWritingMode()) {
        // Main axis is perpendicular to both the physical left<->right and
        // inline start<->end axes, so kLeft and kRight behave as kStart.
        position = ContentPosition::kStart;
      } else if ((position == ContentPosition::kLeft &&
                  style.IsFlippedBlocksWritingMode()) ||
                 (position == ContentPosition::kRight &&
                  style.GetWritingDirection().BlockEnd() ==
                      PhysicalDirection::kRight)) {
        position = ContentPosition::kEnd;
      } else {
        position = ContentPosition::kStart;
      }
    } else if ((position == ContentPosition::kLeft &&
                !style.IsLeftToRightDirection()) ||
               (position == ContentPosition::kRight &&
                style.IsLeftToRightDirection())) {
      DCHECK(!FlexibleBoxAlgorithm::IsColumnFlow(style));
      position = ContentPosition::kEnd;
    } else {
      position = ContentPosition::kStart;
    }
  }
  DCHECK_NE(position, ContentPosition::kLeft);
  DCHECK_NE(position, ContentPosition::kRight);

  ContentDistributionType distribution =
      is_webkit_box ? BoxPackToContentDistribution(style.BoxPack())
                    : style.ResolvedJustifyContentDistribution(
                          ContentAlignmentNormalBehavior());
  OverflowAlignment overflow = style.JustifyContent().Overflow();
  if (is_webkit_box) {
    overflow = OverflowAlignment::kSafe;
  } else if (distribution == ContentDistributionType::kStretch) {
    // For flex, justify-content: stretch behaves as flex-start:
    // https://drafts.csswg.org/css-align/#distribution-flex
    position = ContentPosition::kFlexStart;
    distribution = ContentDistributionType::kDefault;
  }
  return StyleContentAlignmentData(position, distribution, overflow);
}

// static
StyleContentAlignmentData FlexibleBoxAlgorithm::ResolvedAlignContent(
    const ComputedStyle& style) {
  ContentPosition position =
      style.ResolvedAlignContentPosition(ContentAlignmentNormalBehavior());
  ContentDistributionType distribution =
      style.ResolvedAlignContentDistribution(ContentAlignmentNormalBehavior());
  OverflowAlignment overflow = style.AlignContent().Overflow();
  return StyleContentAlignmentData(position, distribution, overflow);
}

// static
ItemPosition FlexibleBoxAlgorithm::AlignmentForChild(
    const ComputedStyle& flexbox_style,
    const ComputedStyle& child_style) {
  ItemPosition align =
      flexbox_style.IsDeprecatedWebkitBox()
          ? BoxAlignmentToItemPosition(flexbox_style.BoxAlign())
          : child_style
                .ResolvedAlignSelf(
                    {ItemPosition::kStretch, OverflowAlignment::kDefault},
                    &flexbox_style)
                .GetPosition();
  DCHECK_NE(align, ItemPosition::kAuto);
  DCHECK_NE(align, ItemPosition::kNormal);
  DCHECK_NE(align, ItemPosition::kLeft) << "left, right are only for justify";
  DCHECK_NE(align, ItemPosition::kRight) << "left, right are only for justify";

  if (align == ItemPosition::kStart)
    return ItemPosition::kFlexStart;
  if (align == ItemPosition::kEnd)
    return ItemPosition::kFlexEnd;

  if (align == ItemPosition::kSelfStart || align == ItemPosition::kSelfEnd) {
    LogicalToPhysical<ItemPosition> physical(
        child_style.GetWritingDirection(), ItemPosition::kFlexStart,
        ItemPosition::kFlexEnd, ItemPosition::kFlexStart,
        ItemPosition::kFlexEnd);

    PhysicalToLogical<ItemPosition> logical(flexbox_style.GetWritingDirection(),
                                            physical.Top(), physical.Right(),
                                            physical.Bottom(), physical.Left());

    if (flexbox_style.ResolvedIsColumnFlexDirection()) {
      return align == ItemPosition::kSelfStart ? logical.InlineStart()
                                               : logical.InlineEnd();
    }
    return align == ItemPosition::kSelfStart ? logical.BlockStart()
                                             : logical.BlockEnd();
  }

  if (flexbox_style.FlexWrap() == EFlexWrap::kWrapReverse) {
    if (align == ItemPosition::kFlexStart) {
      align = ItemPosition::kFlexEnd;
    } else if (align == ItemPosition::kFlexEnd) {
      align = ItemPosition::kFlexStart;
    }
  }

  if (!child_style.HasOutOfFlowPosition()) {
    if (IsHorizontalFlow(flexbox_style)) {
      if (child_style.MarginTop().IsAuto() ||
          child_style.MarginBottom().IsAuto()) {
        align = ItemPosition::kFlexStart;
      }
    } else {
      if (child_style.MarginLeft().IsAuto() ||
          child_style.MarginRight().IsAuto()) {
        align = ItemPosition::kFlexStart;
      }
    }
  }

  return align;
}

// static
LayoutUnit FlexibleBoxAlgorithm::ContentDistributionSpaceBetweenChildren(
    LayoutUnit available_free_space,
    const StyleContentAlignmentData& data,
    unsigned number_of_items) {
  if (available_free_space > 0 && number_of_items > 1) {
    if (data.Distribution() == ContentDistributionType::kSpaceBetween)
      return available_free_space / (number_of_items - 1);
    if (data.Distribution() == ContentDistributionType::kSpaceAround ||
        data.Distribution() == ContentDistributionType::kStretch)
      return available_free_space / number_of_items;
    if (data.Distribution() == ContentDistributionType::kSpaceEvenly)
      return available_free_space / (number_of_items + 1);
  }
  return LayoutUnit();
}

const FlexItem& FlexibleBoxAlgorithm::FlexItemAtIndex(
    wtf_size_t item_index) const {
  return all_items_[item_index];
}

void FlexibleBoxAlgorithm::Trace(Visitor* visitor) const {
  visitor->Trace(all_items_);
}

}  // namespace blink
