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

FlexItem::FlexItem(const FlexibleBoxAlgorithm* algorithm,
                   const ComputedStyle& style,
                   LayoutUnit flex_base_content_size,
                   MinMaxSizes min_max_main_sizes,
                   absl::optional<MinMaxSizes> min_max_cross_sizes,
                   LayoutUnit main_axis_border_padding,
                   LayoutUnit cross_axis_border_padding,
                   PhysicalBoxStrut physical_margins,
                   BoxStrut scrollbars,
                   WritingMode baseline_writing_mode,
                   BaselineGroup baseline_group,
                   bool depends_on_min_max_sizes)
    : algorithm_(algorithm),
      line_number_(0),
      style_(style),
      flex_base_content_size_(flex_base_content_size),
      min_max_main_sizes_(min_max_main_sizes),
      min_max_cross_sizes_(min_max_cross_sizes),
      hypothetical_main_content_size_(
          min_max_main_sizes.ClampSizeToMinAndMax(flex_base_content_size)),
      main_axis_border_padding_(main_axis_border_padding),
      cross_axis_border_padding_(cross_axis_border_padding),
      physical_margins_(physical_margins),
      scrollbars_(scrollbars),
      baseline_writing_direction_({baseline_writing_mode, TextDirection::kLtr}),
      baseline_group_(baseline_group),
      depends_on_min_max_sizes_(depends_on_min_max_sizes),
      frozen_(false),
      ng_input_node_(/* LayoutBox* */ nullptr) {
  DCHECK_GE(min_max_main_sizes.max_size, LayoutUnit())
      << "Use LayoutUnit::Max() for no max size";
}

bool FlexItem::MainAxisIsInlineAxis() const {
  return algorithm_->IsHorizontalFlow() == style_->IsHorizontalWritingMode();
}

LayoutUnit FlexItem::FlowAwareMarginStart() const {
  if (algorithm_->IsHorizontalFlow()) {
    return algorithm_->IsLeftToRightFlow() ? physical_margins_.left
                                           : physical_margins_.right;
  }
  return algorithm_->IsLeftToRightFlow() ? physical_margins_.top
                                         : physical_margins_.bottom;
}

LayoutUnit FlexItem::FlowAwareMarginEnd() const {
  if (algorithm_->IsHorizontalFlow()) {
    return algorithm_->IsLeftToRightFlow() ? physical_margins_.right
                                           : physical_margins_.left;
  }
  return algorithm_->IsLeftToRightFlow() ? physical_margins_.bottom
                                         : physical_margins_.top;
}

LayoutUnit FlexItem::FlowAwareMarginBefore() const {
  switch (algorithm_->GetTransformedWritingMode()) {
    case TransformedWritingMode::kTopToBottomWritingMode:
      return physical_margins_.top;
    case TransformedWritingMode::kBottomToTopWritingMode:
      return physical_margins_.bottom;
    case TransformedWritingMode::kLeftToRightWritingMode:
      return physical_margins_.left;
    case TransformedWritingMode::kRightToLeftWritingMode:
      return physical_margins_.right;
  }
  NOTREACHED();
  return LayoutUnit();
}

LayoutUnit FlexItem::FlowAwareMarginAfter() const {
  switch (algorithm_->GetTransformedWritingMode()) {
    case TransformedWritingMode::kTopToBottomWritingMode:
      return physical_margins_.bottom;
    case TransformedWritingMode::kBottomToTopWritingMode:
      return physical_margins_.top;
    case TransformedWritingMode::kLeftToRightWritingMode:
      return physical_margins_.right;
    case TransformedWritingMode::kRightToLeftWritingMode:
      return physical_margins_.left;
  }
  NOTREACHED();
  return LayoutUnit();
}

LayoutUnit FlexItem::MarginBlockEnd() const {
  BoxStrut margins = physical_margins_.ConvertToLogical(
      algorithm_->Style()->GetWritingDirection());
  return margins.block_end;
}

LayoutUnit FlexItem::MainAxisMarginExtent() const {
  return algorithm_->IsHorizontalFlow() ? physical_margins_.HorizontalSum()
                                        : physical_margins_.VerticalSum();
}

LayoutUnit FlexItem::CrossAxisMarginExtent() const {
  return algorithm_->IsHorizontalFlow() ? physical_margins_.VerticalSum()
                                        : physical_margins_.HorizontalSum();
}

LayoutUnit FlexItem::MarginBoxAscent(bool is_last_baseline,
                                     bool is_wrap_reverse) const {
  DCHECK(layout_result_);
  LogicalBoxFragment baseline_fragment(
      baseline_writing_direction_,
      To<PhysicalBoxFragment>(layout_result_->GetPhysicalFragment()));

  const auto font_baseline = algorithm_->StyleRef().GetFontBaseline();
  LayoutUnit baseline =
      is_last_baseline
          ? baseline_fragment.LastBaselineOrSynthesize(font_baseline)
          : baseline_fragment.FirstBaselineOrSynthesize(font_baseline);
  if (is_wrap_reverse != is_last_baseline)
    baseline = baseline_fragment.BlockSize() - baseline;

  return baseline_group_ == BaselineGroup::kMajor
             ? FlowAwareMarginBefore() + baseline
             : FlowAwareMarginAfter() + baseline;
}

LayoutUnit FlexItem::AvailableAlignmentSpace() const {
  LayoutUnit cross_extent = CrossAxisMarginExtent() + cross_axis_size_;
  return Line()->cross_axis_extent_ - cross_extent;
}

ItemPosition FlexItem::Alignment() const {
  return FlexibleBoxAlgorithm::AlignmentForChild(*algorithm_->Style(), *style_);
}

void FlexItem::UpdateAutoMarginsInMainAxis(LayoutUnit auto_margin_offset) {
  DCHECK_GE(auto_margin_offset, LayoutUnit());

  if (algorithm_->IsHorizontalFlow()) {
    if (style_->MarginLeft().IsAuto()) {
      physical_margins_.left = auto_margin_offset;
    }
    if (style_->MarginRight().IsAuto()) {
      physical_margins_.right = auto_margin_offset;
    }
  } else {
    if (style_->MarginTop().IsAuto()) {
      physical_margins_.top = auto_margin_offset;
    }
    if (style_->MarginBottom().IsAuto()) {
      physical_margins_.bottom = auto_margin_offset;
    }
  }
}

bool FlexItem::UpdateAutoMarginsInCrossAxis(
    LayoutUnit available_alignment_space) {
  DCHECK_GE(available_alignment_space, LayoutUnit());

  bool is_horizontal = algorithm_->IsHorizontalFlow();
  const Length& top_or_left =
      is_horizontal ? style_->MarginTop() : style_->MarginLeft();
  const Length& bottom_or_right =
      is_horizontal ? style_->MarginBottom() : style_->MarginRight();
  if (top_or_left.IsAuto() && bottom_or_right.IsAuto()) {
    offset_->cross_axis_offset += available_alignment_space / 2;
    if (is_horizontal) {
      physical_margins_.top = available_alignment_space / 2;
      physical_margins_.bottom = available_alignment_space / 2;
    } else {
      physical_margins_.left = available_alignment_space / 2;
      physical_margins_.right = available_alignment_space / 2;
    }
    return true;
  }
  bool should_adjust_top_or_left = true;
  if (algorithm_->IsColumnFlow() && !style_->IsLeftToRightDirection()) {
    // For column flows, only make this adjustment if topOrLeft corresponds to
    // the "before" margin, so that flipForRightToLeftColumn will do the right
    // thing.
    should_adjust_top_or_left = false;
  }
  if (!algorithm_->IsColumnFlow() && style_->IsFlippedBlocksWritingMode()) {
    // If we are a flipped writing mode, we need to adjust the opposite side.
    // This is only needed for row flows because this only affects the
    // block-direction axis.
    should_adjust_top_or_left = false;
  }

  if (top_or_left.IsAuto()) {
    if (should_adjust_top_or_left)
      offset_->cross_axis_offset += available_alignment_space;

    if (is_horizontal)
      physical_margins_.top = available_alignment_space;
    else
      physical_margins_.left = available_alignment_space;
    return true;
  }
  if (bottom_or_right.IsAuto()) {
    if (!should_adjust_top_or_left)
      offset_->cross_axis_offset += available_alignment_space;

    if (is_horizontal)
      physical_margins_.bottom = available_alignment_space;
    else
      physical_margins_.right = available_alignment_space;
    return true;
  }
  return false;
}

void FlexItem::ComputeStretchedSize() {
  DCHECK_EQ(Alignment(), ItemPosition::kStretch);
  LayoutUnit stretched_size =
      std::max(cross_axis_border_padding_,
               Line()->cross_axis_extent_ - CrossAxisMarginExtent());

  if ((MainAxisIsInlineAxis() && style_->LogicalHeight().IsAuto()) ||
      (!MainAxisIsInlineAxis() && style_->LogicalWidth().IsAuto())) {
    cross_axis_size_ =
        min_max_cross_sizes_->ClampSizeToMinAndMax(stretched_size);
  }
}

void FlexItem::Trace(Visitor* visitor) const {
  visitor->Trace(style_);
  visitor->Trace(ng_input_node_);
  visitor->Trace(layout_result_);
}

// static
LayoutUnit FlexItem::AlignmentOffset(LayoutUnit available_free_space,
                                     ItemPosition position,
                                     LayoutUnit baseline_offset,
                                     bool is_wrap_reverse,
                                     bool is_deprecated_webkit_box) {
  switch (position) {
    case ItemPosition::kLegacy:
    case ItemPosition::kAuto:
    case ItemPosition::kNormal:
    case ItemPosition::kAnchorCenter:
      NOTREACHED();
      break;
    case ItemPosition::kSelfStart:
    case ItemPosition::kSelfEnd:
    case ItemPosition::kStart:
    case ItemPosition::kEnd:
    case ItemPosition::kLeft:
    case ItemPosition::kRight:
      NOTREACHED() << static_cast<int>(position)
                   << " AlignmentForChild should have transformed this "
                      "position value to something we handle below.";
      break;
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

void FlexLine::FreezeViolations(ViolationsVector& violations) {
  const ComputedStyle& flex_box_style = algorithm_->StyleRef();
  for (wtf_size_t i = 0; i < violations.size(); ++i) {
    DCHECK(!violations[i]->frozen_) << i;
    const ComputedStyle& child_style = *violations[i]->style_;
    LayoutUnit child_size = violations[i]->flexed_content_size_;
    remaining_free_space_ -=
        child_size - violations[i]->flex_base_content_size_;
    total_flex_grow_ -= child_style.ResolvedFlexGrow(flex_box_style);
    const float flex_shrink = child_style.ResolvedFlexShrink(flex_box_style);
    total_flex_shrink_ -= flex_shrink;
    total_weighted_flex_shrink_ -=
        flex_shrink * violations[i]->flex_base_content_size_;
    // total_weighted_flex_shrink can be negative when we exceed the precision
    // of a double when we initially calculate total_weighted_flex_shrink. We
    // then subtract each child's weighted flex shrink with full precision, now
    // leading to a negative result. See
    // css3/flexbox/large-flex-shrink-assert.html
    total_weighted_flex_shrink_ = std::max(total_weighted_flex_shrink_, 0.0);
    violations[i]->frozen_ = true;
  }
}

void FlexLine::FreezeInflexibleItems() {
  // Per https://drafts.csswg.org/css-flexbox/#resolve-flexible-lengths step 2,
  // we freeze all items with a flex factor of 0 as well as those with a min/max
  // size violation.
  FlexSign flex_sign = Sign();
  remaining_free_space_ = container_main_inner_size_ - sum_flex_base_size_;

  ViolationsVector new_inflexible_items;
  const ComputedStyle& flex_box_style = algorithm_->StyleRef();
  for (wtf_size_t i = 0; i < line_items_.size(); ++i) {
    FlexItem& flex_item = line_items_[i];
    DCHECK(!flex_item.frozen_) << i;
    float flex_factor =
        (flex_sign == kPositiveFlexibility)
            ? flex_item.style_->ResolvedFlexGrow(flex_box_style)
            : flex_item.style_->ResolvedFlexShrink(flex_box_style);
    if (flex_factor == 0 ||
        (flex_sign == kPositiveFlexibility &&
         flex_item.flex_base_content_size_ >
             flex_item.hypothetical_main_content_size_) ||
        (flex_sign == kNegativeFlexibility &&
         flex_item.flex_base_content_size_ <
             flex_item.hypothetical_main_content_size_)) {
      flex_item.flexed_content_size_ =
          flex_item.hypothetical_main_content_size_;
      new_inflexible_items.push_back(&flex_item);
    }
  }
  FreezeViolations(new_inflexible_items);
  initial_free_space_ = remaining_free_space_;
}

bool FlexLine::ResolveFlexibleLengths() {
  LayoutUnit total_violation;
  LayoutUnit used_free_space;
  ViolationsVector min_violations;
  ViolationsVector max_violations;

  FlexSign flex_sign = Sign();
  double sum_flex_factors = (flex_sign == kPositiveFlexibility)
                                ? total_flex_grow_
                                : total_flex_shrink_;
  if (sum_flex_factors > 0 && sum_flex_factors < 1) {
    LayoutUnit fractional(initial_free_space_ * sum_flex_factors);
    if (fractional.Abs() < remaining_free_space_.Abs())
      remaining_free_space_ = fractional;
  }

  const ComputedStyle& flex_box_style = algorithm_->StyleRef();
  for (wtf_size_t i = 0; i < line_items_.size(); ++i) {
    FlexItem& flex_item = line_items_[i];

    // This check also covers out-of-flow children.
    if (flex_item.frozen_)
      continue;

    LayoutUnit child_size = flex_item.flex_base_content_size_;
    double extra_space = 0;
    if (remaining_free_space_ > 0 && total_flex_grow_ > 0 &&
        flex_sign == kPositiveFlexibility && std::isfinite(total_flex_grow_)) {
      extra_space = remaining_free_space_ *
                    flex_item.style_->ResolvedFlexGrow(flex_box_style) /
                    total_flex_grow_;
    } else if (remaining_free_space_ < 0 && total_weighted_flex_shrink_ > 0 &&
               flex_sign == kNegativeFlexibility &&
               std::isfinite(total_weighted_flex_shrink_) &&
               flex_item.style_->ResolvedFlexShrink(flex_box_style)) {
      extra_space = remaining_free_space_ *
                    flex_item.style_->ResolvedFlexShrink(flex_box_style) *
                    flex_item.flex_base_content_size_ /
                    total_weighted_flex_shrink_;
    }
    if (std::isfinite(extra_space))
      child_size += LayoutUnit::FromFloatRound(extra_space);

    LayoutUnit adjusted_child_size = flex_item.ClampSizeToMinAndMax(child_size);
    DCHECK_GE(adjusted_child_size, 0);
    flex_item.flexed_content_size_ = adjusted_child_size;
    used_free_space += adjusted_child_size - flex_item.flex_base_content_size_;

    LayoutUnit violation = adjusted_child_size - child_size;
    if (violation > 0)
      min_violations.push_back(&flex_item);
    else if (violation < 0)
      max_violations.push_back(&flex_item);
    total_violation += violation;
  }

  if (total_violation) {
    FreezeViolations(total_violation < 0 ? max_violations : min_violations);
  } else {
    remaining_free_space_ -= used_free_space;
  }

  return !total_violation;
}

LayoutUnit FlexLine::ApplyMainAxisAutoMarginAdjustment() {
  if (remaining_free_space_ <= LayoutUnit())
    return LayoutUnit();

  int number_of_auto_margins = 0;
  bool is_horizontal = algorithm_->IsHorizontalFlow();
  for (const auto& line_item : line_items_) {
    const ComputedStyle& style = *line_item.style_;
    if (is_horizontal) {
      if (style.MarginLeft().IsAuto())
        ++number_of_auto_margins;
      if (style.MarginRight().IsAuto())
        ++number_of_auto_margins;
    } else {
      if (style.MarginTop().IsAuto())
        ++number_of_auto_margins;
      if (style.MarginBottom().IsAuto())
        ++number_of_auto_margins;
    }
  }
  if (!number_of_auto_margins)
    return LayoutUnit();

  LayoutUnit size_of_auto_margin =
      remaining_free_space_ / number_of_auto_margins;
  remaining_free_space_ = LayoutUnit();
  return size_of_auto_margin;
}

void FlexLine::ComputeLineItemsPosition(LayoutUnit main_axis_start_offset,
                                        LayoutUnit main_axis_end_offset,
                                        LayoutUnit& cross_axis_offset) {
  const auto& style = algorithm_->StyleRef();
  const bool is_webkit_box = style.IsDeprecatedWebkitBox();
  const bool is_wrap_reverse = style.FlexWrap() == EFlexWrap::kWrapReverse;

  main_axis_offset_ = main_axis_start_offset;
  // Recalculate the remaining free space. The adjustment for flex factors
  // between 0..1 means we can't just use remainingFreeSpace here.
  LayoutUnit total_item_size;
  for (wtf_size_t i = 0; i < line_items_.size(); ++i)
    total_item_size += line_items_[i].FlexedMarginBoxSize();
  remaining_free_space_ =
      container_main_inner_size_ - total_item_size -
      (line_items_.size() - 1) * algorithm_->gap_between_items_;

  const StyleContentAlignmentData justify_content =
      FlexibleBoxAlgorithm::ResolvedJustifyContent(style);

  const LayoutUnit auto_margin_offset = ApplyMainAxisAutoMarginAdjustment();
  const LayoutUnit available_free_space = remaining_free_space_;
  const bool is_reversed = style.ResolvedIsRowReverseFlexDirection() ||
                           style.ResolvedIsColumnReverseFlexDirection();
  const LayoutUnit initial_position =
      FlexibleBoxAlgorithm::InitialContentPositionOffset(
          style, available_free_space, justify_content, line_items_.size(),
          is_reversed);
  LayoutUnit main_axis_offset = initial_position + main_axis_start_offset;

  // When a -webkit-box has negative available-space it always places that
  // overflow to the line-right. (Even if we have "direction: rtl" or
  // "-webkit-box-direction: reverse"). In the future it will hopefully be
  // possible to remove this quirk.
  if (is_webkit_box && available_free_space < 0 &&
      (style.ResolvedIsRowReverseFlexDirection() ==
       style.IsLeftToRightDirection())) {
    main_axis_offset += available_free_space;
  }

  LayoutUnit max_major_descent;
  LayoutUnit max_minor_descent;

  LayoutUnit max_child_cross_axis_extent;
  for (wtf_size_t i = 0; i < line_items_.size(); ++i) {
    FlexItem& flex_item = line_items_[i];

    flex_item.UpdateAutoMarginsInMainAxis(auto_margin_offset);

    LayoutUnit child_cross_axis_margin_box_extent;
    const auto alignment = flex_item.Alignment();
    // TODO(crbug.com/1272533): We may not have a layout-result during min/max
    // calculations. This is incorrect, and should be re-enabled once we have
    // more cache slots.
    if (flex_item.layout_result_ &&
        (alignment == ItemPosition::kBaseline ||
         alignment == ItemPosition::kLastBaseline)) {
      LayoutUnit ascent = flex_item.MarginBoxAscent(
          alignment == ItemPosition::kLastBaseline, is_wrap_reverse);
      LayoutUnit descent =
          (flex_item.CrossAxisMarginExtent() + flex_item.cross_axis_size_) -
          ascent;
      if (flex_item.baseline_group_ == BaselineGroup::kMajor) {
        max_major_ascent_ = std::max(max_major_ascent_, ascent);
        max_major_descent = std::max(max_major_descent, descent);
        child_cross_axis_margin_box_extent =
            max_major_ascent_ + max_major_descent;
      } else {
        max_minor_ascent_ = std::max(max_minor_ascent_, ascent);
        max_minor_descent = std::max(max_minor_descent, descent);
        child_cross_axis_margin_box_extent =
            max_minor_ascent_ + max_minor_descent;
      }
    } else {
      child_cross_axis_margin_box_extent =
          flex_item.cross_axis_size_ + flex_item.CrossAxisMarginExtent();
    }
    max_child_cross_axis_extent = std::max(max_child_cross_axis_extent,
                                           child_cross_axis_margin_box_extent);

    main_axis_offset += flex_item.FlowAwareMarginStart();

    LayoutUnit child_main_extent = flex_item.FlexedBorderBoxSize();
    // In an RTL column situation, this will apply the margin-right/margin-end
    // on the left. This will be fixed later in
    // LayoutFlexibleBox::FlipForRightToLeftColumn.
    *flex_item.offset_ = FlexOffset(
        style.ResolvedIsRowReverseFlexDirection()
            ? container_logical_width_ - main_axis_offset - child_main_extent
            : main_axis_offset,
        cross_axis_offset + flex_item.FlowAwareMarginBefore());
    main_axis_offset += child_main_extent + flex_item.FlowAwareMarginEnd();

    if (i != line_items_.size() - 1) {
      // The last item does not get extra space added.
      LayoutUnit space_between =
          FlexibleBoxAlgorithm::ContentDistributionSpaceBetweenChildren(
              available_free_space, justify_content, line_items_.size());
      main_axis_offset += space_between + algorithm_->gap_between_items_;
    }
  }

  main_axis_extent_ = main_axis_offset;

  cross_axis_offset_ = cross_axis_offset;
  cross_axis_extent_ = max_child_cross_axis_extent;

  cross_axis_offset += max_child_cross_axis_extent;
}

// static
LayoutUnit FlexibleBoxAlgorithm::GapBetweenItems(
    const ComputedStyle& style,
    LogicalSize percent_resolution_sizes) {
  if (IsColumnFlow(style)) {
    if (const absl::optional<Length>& row_gap = style.RowGap()) {
      return MinimumValueForLength(
          *row_gap,
          percent_resolution_sizes.block_size.ClampIndefiniteToZero());
    }
    return LayoutUnit();
  }
  if (const absl::optional<Length>& column_gap = style.ColumnGap()) {
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
    if (const absl::optional<Length>& row_gap = style.RowGap()) {
      return MinimumValueForLength(
          *row_gap,
          percent_resolution_sizes.block_size.ClampIndefiniteToZero());
    }
    return LayoutUnit();
  }
  if (const absl::optional<Length>& column_gap = style.ColumnGap()) {
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
      style_(style),
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

  if (row_gap && row_gap->IsPercentOrCalc()) {
    UseCounter::Count(document, WebFeature::kFlexRowGapPercent);
    if (percent_resolution_sizes.block_size == LayoutUnit(-1))
      UseCounter::Count(document, WebFeature::kFlexRowGapPercentIndefinite);
  }
}

FlexLine* FlexibleBoxAlgorithm::ComputeNextFlexLine(
    LayoutUnit container_logical_width) {
  LayoutUnit sum_flex_base_size;
  double total_flex_grow = 0;
  double total_flex_shrink = 0;
  double total_weighted_flex_shrink = 0;
  LayoutUnit sum_hypothetical_main_size;

  bool line_has_in_flow_item = false;

  wtf_size_t start_index = next_item_index_;

  for (; next_item_index_ < all_items_.size(); ++next_item_index_) {
    FlexItem& flex_item = all_items_[next_item_index_];
    if (IsMultiline() &&
        sum_hypothetical_main_size +
                flex_item.HypotheticalMainAxisMarginBoxSize() >
            line_break_length_ &&
        line_has_in_flow_item) {
      break;
    }
    line_has_in_flow_item = true;
    sum_flex_base_size +=
        flex_item.FlexBaseMarginBoxSize() + gap_between_items_;
    total_flex_grow += flex_item.style_->ResolvedFlexGrow(StyleRef());
    const float flex_shrink = flex_item.style_->ResolvedFlexShrink(StyleRef());
    total_flex_shrink += flex_shrink;
    total_weighted_flex_shrink +=
        flex_shrink * flex_item.flex_base_content_size_;
    sum_hypothetical_main_size +=
        flex_item.HypotheticalMainAxisMarginBoxSize() + gap_between_items_;
    flex_item.line_number_ = flex_lines_.size();
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
        this, FlexItemVectorView(&all_items_, start_index, next_item_index_),
        container_logical_width, sum_flex_base_size, total_flex_grow,
        total_flex_shrink, total_weighted_flex_shrink,
        sum_hypothetical_main_size);
  }
  return nullptr;
}

bool FlexibleBoxAlgorithm::IsHorizontalFlow() const {
  return IsHorizontalFlow(*style_);
}

bool FlexibleBoxAlgorithm::IsColumnFlow() const {
  return IsColumnFlow(*style_);
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

bool FlexibleBoxAlgorithm::IsLeftToRightFlow() const {
  if (style_->ResolvedIsColumnFlexDirection()) {
    return blink::IsHorizontalWritingMode(style_->GetWritingMode()) ||
           IsFlippedLinesWritingMode(style_->GetWritingMode());
  }
  return style_->IsLeftToRightDirection() ^
         style_->ResolvedIsRowReverseFlexDirection();
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

bool FlexibleBoxAlgorithm::ShouldApplyMinSizeAutoForChild(
    const LayoutBox& child) const {
  // See: https://drafts.csswg.org/css-flexbox/#min-size-auto
  const Length& min = IsHorizontalFlow() ? child.StyleRef().UsedMinWidth()
                                         : child.StyleRef().UsedMinHeight();
  bool main_axis_is_childs_block_axis =
      IsHorizontalFlow() != child.StyleRef().IsHorizontalWritingMode();
  bool intrinsic_in_childs_block_axis =
      main_axis_is_childs_block_axis &&
      (min.IsMinContent() || min.IsMaxContent() || min.IsMinIntrinsic() ||
       min.IsFitContent());
  if (!min.IsAuto() && !intrinsic_in_childs_block_axis)
    return false;

  // webkit-box treats min-size: auto as 0.
  if (StyleRef().IsDeprecatedWebkitBox())
    return false;

  if (child.ShouldApplySizeContainment())
    return false;

  // For replaced elements treat 'clip' similar to 'visible'.
  return MainAxisOverflowForChild(child) == EOverflow::kVisible ||
         (child.IsLayoutReplaced() &&
          MainAxisOverflowForChild(child) == EOverflow::kClip);
}

LayoutUnit FlexibleBoxAlgorithm::IntrinsicContentBlockSize() const {
  if (flex_lines_.empty())
    return LayoutUnit();

  if (IsColumnFlow()) {
    LayoutUnit max_size;
    for (const FlexLine& line : flex_lines_)
      max_size = std::max(line.sum_hypothetical_main_size_, max_size);
    return max_size;
  }

  const FlexLine& last_line = flex_lines_.back();
  // Subtract the first line's offset to remove border/padding
  return last_line.cross_axis_offset_ + last_line.cross_axis_extent_ -
         flex_lines_.front().cross_axis_offset_ +
         (flex_lines_.size() - 1) * gap_between_lines_;
}

void FlexibleBoxAlgorithm::AlignFlexLines(
    LayoutUnit cross_axis_content_extent,
    HeapVector<NGFlexLine>* flex_line_outputs) {
  const StyleContentAlignmentData align_content = ResolvedAlignContent(*style_);
  if (align_content.GetPosition() == ContentPosition::kFlexStart &&
      gap_between_lines_ == 0) {
    return;
  }
  if (flex_lines_.empty() || !IsMultiline())
    return;
  LayoutUnit available_cross_axis_space =
      cross_axis_content_extent - (flex_lines_.size() - 1) * gap_between_lines_;
  for (const FlexLine& line : flex_lines_)
    available_cross_axis_space -= line.cross_axis_extent_;

  const bool is_reversed = StyleRef().FlexWrap() == EFlexWrap::kWrapReverse;
  LayoutUnit line_offset = InitialContentPositionOffset(
      StyleRef(), available_cross_axis_space, align_content, flex_lines_.size(),
      is_reversed);
  for (wtf_size_t i = 0; i < flex_lines_.size(); i++) {
    FlexLine& line_context = flex_lines_[i];
    line_context.cross_axis_offset_ += line_offset;
    if (flex_line_outputs) {
      (*flex_line_outputs)[i].cross_axis_offset =
          line_context.cross_axis_offset_;
    }

    for (FlexItem& flex_item : line_context.line_items_) {
      flex_item.offset_->cross_axis_offset += line_offset;
    }
    if (align_content.Distribution() == ContentDistributionType::kStretch &&
        available_cross_axis_space > 0) {
      line_context.cross_axis_extent_ +=
          available_cross_axis_space /
          static_cast<unsigned>(flex_lines_.size());
      if (flex_line_outputs) {
        (*flex_line_outputs)[i].line_cross_size =
            line_context.cross_axis_extent_;
      }
    }

    line_offset +=
        ContentDistributionSpaceBetweenChildren(
            available_cross_axis_space, align_content, flex_lines_.size()) +
        gap_between_lines_;
  }
}

void FlexibleBoxAlgorithm::AlignChildren() {
  const bool is_webkit_box = StyleRef().IsDeprecatedWebkitBox();
  const bool is_wrap_reverse = StyleRef().FlexWrap() == EFlexWrap::kWrapReverse;
  for (FlexLine& line_context : flex_lines_) {
    for (FlexItem& flex_item : line_context.line_items_) {
      if (flex_item.UpdateAutoMarginsInCrossAxis(
              flex_item.AvailableAlignmentSpace().ClampNegativeToZero()))
        continue;

      ItemPosition position = flex_item.Alignment();
      if (position == ItemPosition::kStretch) {
        flex_item.ComputeStretchedSize();
      }
      LayoutUnit available_space = flex_item.AvailableAlignmentSpace();
      if (!is_webkit_box &&
          flex_item.style_
                  ->ResolvedAlignSelf(ItemPosition::kStretch, &StyleRef())
                  .Overflow() == OverflowAlignment::kSafe) {
        available_space = available_space.ClampNegativeToZero();
      }
      LayoutUnit baseline_offset;
      if (position == ItemPosition::kBaseline ||
          position == ItemPosition::kLastBaseline) {
        bool is_major = flex_item.baseline_group_ == BaselineGroup::kMajor;
        LayoutUnit ascent = flex_item.MarginBoxAscent(
            position == ItemPosition::kLastBaseline, is_wrap_reverse);
        LayoutUnit max_ascent = is_major ? line_context.max_major_ascent_
                                         : line_context.max_minor_ascent_;

        LayoutUnit baseline_delta = max_ascent - ascent;
        baseline_offset =
            is_major ? baseline_delta : available_space - baseline_delta;
      }
      LayoutUnit offset =
          FlexItem::AlignmentOffset(available_space, position, baseline_offset,
                                    is_wrap_reverse, is_webkit_box);
      flex_item.offset_->cross_axis_offset += offset;
    }
  }
}

void FlexibleBoxAlgorithm::FlipForWrapReverse(
    LayoutUnit cross_axis_start_edge,
    LayoutUnit cross_axis_content_size,
    HeapVector<NGFlexLine>* flex_line_outputs) {
  DCHECK_EQ(Style()->FlexWrap(), EFlexWrap::kWrapReverse);
  for (wtf_size_t i = 0; i < flex_lines_.size(); i++) {
    FlexLine& line_context = flex_lines_[i];
    LayoutUnit original_offset =
        line_context.cross_axis_offset_ - cross_axis_start_edge;
    LayoutUnit new_offset = cross_axis_content_size - original_offset -
                            line_context.cross_axis_extent_;
    LayoutUnit delta = new_offset - original_offset;
    if (flex_line_outputs) {
      line_context.cross_axis_offset_ += delta;
      (*flex_line_outputs)[i].cross_axis_offset += delta;
    }
    for (FlexItem& flex_item : line_context.line_items_)
      flex_item.offset_->cross_axis_offset += delta;
  }
}

TransformedWritingMode FlexibleBoxAlgorithm::GetTransformedWritingMode() const {
  return GetTransformedWritingMode(*style_);
}

// static
TransformedWritingMode FlexibleBoxAlgorithm::GetTransformedWritingMode(
    const ComputedStyle& style) {
  WritingMode mode = style.GetWritingMode();
  if (!style.ResolvedIsColumnFlexDirection()) {
    static_assert(
        static_cast<TransformedWritingMode>(WritingMode::kHorizontalTb) ==
                TransformedWritingMode::kTopToBottomWritingMode &&
            static_cast<TransformedWritingMode>(WritingMode::kVerticalLr) ==
                TransformedWritingMode::kLeftToRightWritingMode &&
            static_cast<TransformedWritingMode>(WritingMode::kVerticalRl) ==
                TransformedWritingMode::kRightToLeftWritingMode,
        "WritingMode and TransformedWritingMode must match values.");
    return static_cast<TransformedWritingMode>(mode);
  }

  switch (mode) {
    case WritingMode::kHorizontalTb:
      return style.IsLeftToRightDirection()
                 ? TransformedWritingMode::kLeftToRightWritingMode
                 : TransformedWritingMode::kRightToLeftWritingMode;
    case WritingMode::kVerticalLr:
    case WritingMode::kVerticalRl:
      return style.IsLeftToRightDirection()
                 ? TransformedWritingMode::kTopToBottomWritingMode
                 : TransformedWritingMode::kBottomToTopWritingMode;
    // TODO(layout-dev): Sideways-lr and sideways-rl are not yet supported.
    default:
      break;
  }
  NOTREACHED();
  return TransformedWritingMode::kTopToBottomWritingMode;
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
                  style.IsFlippedLinesWritingMode())) {
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
  // For flex, justify-content: stretch behaves as flex-start:
  // https://drafts.csswg.org/css-align/#distribution-flex
  if (!is_webkit_box && distribution == ContentDistributionType::kStretch) {
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
                .ResolvedAlignSelf(ItemPosition::kStretch, &flexbox_style)
                .GetPosition();
  return TranslateItemPosition(flexbox_style, child_style, align);
}

ItemPosition FlexibleBoxAlgorithm::TranslateItemPosition(
    const ComputedStyle& flexbox_style,
    const ComputedStyle& child_style,
    ItemPosition align) {
  DCHECK_NE(align, ItemPosition::kAuto);
  DCHECK_NE(align, ItemPosition::kNormal);

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

  if (align == ItemPosition::kLeft || align == ItemPosition::kRight) {
    DCHECK_EQ(
        align,
        child_style.ResolvedJustifySelf(ItemPosition::kStretch).GetPosition())
        << "justify-self is the only way that we can get a left or right "
           "ItemPosition";
    DCHECK(IsColumnFlow(flexbox_style))
        << "We can also only get left or right ItemPositions when checking "
           "compat data for column flexboxes. The rest of this logic assumes a "
           "column flexbox.";
    switch (flexbox_style.GetWritingMode()) {
      case WritingMode::kHorizontalTb:
      case WritingMode::kVerticalLr:
        return align == ItemPosition::kLeft ? ItemPosition::kFlexStart
                                            : ItemPosition::kFlexEnd;
      case WritingMode::kVerticalRl:
        return align == ItemPosition::kLeft ? ItemPosition::kFlexEnd
                                            : ItemPosition::kFlexStart;
      case WritingMode::kSidewaysLr:
      case WritingMode::kSidewaysRl:
        return ItemPosition::kFlexStart;
    }
  }

  if (align == ItemPosition::kBaseline) {
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

  if (flexbox_style.FlexWrap() == EFlexWrap::kWrapReverse) {
    if (align == ItemPosition::kFlexStart)
      align = ItemPosition::kFlexEnd;
    else if (align == ItemPosition::kFlexEnd)
      align = ItemPosition::kFlexStart;
  }

  return align;
}

// static
LayoutUnit FlexibleBoxAlgorithm::InitialContentPositionOffset(
    const ComputedStyle& style,
    LayoutUnit available_free_space,
    const StyleContentAlignmentData& data,
    unsigned number_of_items,
    bool is_reversed) {
  // Safe-alignment with negative free-space does nothing.
  if (available_free_space <= LayoutUnit() &&
      (style.IsDeprecatedWebkitBox() ||
       data.Overflow() == OverflowAlignment::kSafe)) {
    return LayoutUnit();
  }

  ContentPosition position = data.GetPosition();
  DCHECK_NE(position, ContentPosition::kLeft)
      << "ResolvedJustifyContent was supposed to translate this to kStart/End";
  DCHECK_NE(position, ContentPosition::kRight)
      << "ResolvedJustifyContent was supposed to translate this to kStart/End";
  if (position == ContentPosition::kFlexEnd ||
      (position == ContentPosition::kEnd && !is_reversed) ||
      (position == ContentPosition::kStart && is_reversed)) {
    return available_free_space;
  }
  if (data.GetPosition() == ContentPosition::kCenter)
    return available_free_space / 2;
  if (data.Distribution() == ContentDistributionType::kSpaceAround) {
    if (available_free_space > 0 && number_of_items)
      return available_free_space / (2 * number_of_items);

    return available_free_space / 2;
  }
  if (data.Distribution() == ContentDistributionType::kSpaceEvenly) {
    if (available_free_space > 0 && number_of_items)
      return available_free_space / (number_of_items + 1);
    // Fallback to 'center'
    return available_free_space / 2;
  }
  return LayoutUnit();
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

EOverflow FlexibleBoxAlgorithm::MainAxisOverflowForChild(
    const LayoutBox& child) const {
  if (IsHorizontalFlow())
    return child.StyleRef().OverflowX();
  return child.StyleRef().OverflowY();
}

// Above, we calculated the positions of items in a column reverse container as
// if they were in a column. Now that we know the block size of the container we
// can flip the position of every item.
void FlexibleBoxAlgorithm::LayoutColumnReverse(
    LayoutUnit main_axis_content_size,
    LayoutUnit border_scrollbar_padding_before) {
  DCHECK(IsColumnFlow());
  DCHECK(Style()->ResolvedIsColumnReverseFlexDirection());
  for (FlexLine& line_context : FlexLines()) {
    for (wtf_size_t child_number = 0;
         child_number < line_context.line_items_.size(); ++child_number) {
      FlexItem& flex_item = line_context.line_items_[child_number];
      LayoutUnit item_main_size = flex_item.FlexedBorderBoxSize();

      BoxStrut margins = flex_item.physical_margins_.ConvertToLogical(
          Style()->GetWritingDirection());

      // We passed 0 as the initial main_axis offset to ComputeLineItemsPosition
      // for ColumnReverse containers so here we have to add the
      // border_scrollbar_padding of the container.
      flex_item.offset_->main_axis_offset =
          main_axis_content_size + border_scrollbar_padding_before -
          flex_item.offset_->main_axis_offset - item_main_size -
          margins.block_end + margins.block_start;
    }
  }
}

FlexItem* FlexibleBoxAlgorithm::FlexItemAtIndex(wtf_size_t line_index,
                                                wtf_size_t item_index) const {
  DCHECK_LT(line_index, flex_lines_.size());
  if (StyleRef().FlexWrap() == EFlexWrap::kWrapReverse)
    line_index = flex_lines_.size() - line_index - 1;

  DCHECK_LT(item_index, flex_lines_[line_index].line_items_.size());
  if (Style()->ResolvedIsColumnReverseFlexDirection() ||
      Style()->ResolvedIsRowReverseFlexDirection())
    item_index = flex_lines_[line_index].line_items_.size() - item_index - 1;
  return const_cast<FlexItem*>(
      &flex_lines_[line_index].line_items_[item_index]);
}

void FlexibleBoxAlgorithm::Trace(Visitor* visitor) const {
  visitor->Trace(style_);
  visitor->Trace(all_items_);
}

}  // namespace blink
