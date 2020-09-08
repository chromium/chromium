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

#include "third_party/blink/renderer/core/layout/flexible_box_algorithm.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

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

FlexItem::FlexItem(const FlexLayoutAlgorithm* algorithm,
                   LayoutBox* box,
                   const ComputedStyle& style,
                   LayoutUnit flex_base_content_size,
                   MinMaxSizes min_max_main_sizes,
                   base::Optional<MinMaxSizes> min_max_cross_sizes,
                   LayoutUnit main_axis_border_padding,
                   LayoutUnit cross_axis_border_padding,
                   NGPhysicalBoxStrut physical_margins,
                   NGBoxStrut scrollbars)
    : algorithm(algorithm),
      line_number(0),
      box(box),
      style(style),
      flex_base_content_size(flex_base_content_size),
      min_max_main_sizes(min_max_main_sizes),
      min_max_cross_sizes(min_max_cross_sizes),
      hypothetical_main_content_size(
          min_max_main_sizes.ClampSizeToMinAndMax(flex_base_content_size)),
      main_axis_border_padding(main_axis_border_padding),
      cross_axis_border_padding(cross_axis_border_padding),
      physical_margins(physical_margins),
      scrollbars(scrollbars),
      frozen(false),
      needs_relayout_for_stretch(false),
      ng_input_node(/* LayoutBox* */ nullptr) {
  DCHECK_GE(min_max_main_sizes.max_size, LayoutUnit())
      << "Use LayoutUnit::Max() for no max size";
}

bool FlexItem::MainAxisIsInlineAxis() const {
  return algorithm->IsHorizontalFlow() == style.IsHorizontalWritingMode();
}

LayoutUnit FlexItem::FlowAwareMarginStart() const {
  if (algorithm->IsHorizontalFlow()) {
    return algorithm->IsLeftToRightFlow() ? physical_margins.left
                                          : physical_margins.right;
  }
  return algorithm->IsLeftToRightFlow() ? physical_margins.top
                                        : physical_margins.bottom;
}

LayoutUnit FlexItem::FlowAwareMarginEnd() const {
  if (algorithm->IsHorizontalFlow()) {
    return algorithm->IsLeftToRightFlow() ? physical_margins.right
                                          : physical_margins.left;
  }
  return algorithm->IsLeftToRightFlow() ? physical_margins.bottom
                                        : physical_margins.top;
}

LayoutUnit FlexItem::FlowAwareMarginBefore() const {
  switch (algorithm->GetTransformedWritingMode()) {
    case TransformedWritingMode::kTopToBottomWritingMode:
      return physical_margins.top;
    case TransformedWritingMode::kBottomToTopWritingMode:
      return physical_margins.bottom;
    case TransformedWritingMode::kLeftToRightWritingMode:
      return physical_margins.left;
    case TransformedWritingMode::kRightToLeftWritingMode:
      return physical_margins.right;
  }
  NOTREACHED();
  return LayoutUnit();
}

LayoutUnit FlexItem::MainAxisMarginExtent() const {
  return algorithm->IsHorizontalFlow() ? physical_margins.HorizontalSum()
                                       : physical_margins.VerticalSum();
}

LayoutUnit FlexItem::CrossAxisMarginExtent() const {
  return algorithm->IsHorizontalFlow() ? physical_margins.VerticalSum()
                                       : physical_margins.HorizontalSum();
}

LayoutUnit FlexItem::MarginBoxAscent() const {
  if (box) {
    LayoutUnit ascent(box->FirstLineBoxBaseline());
    if (ascent == -1)
      ascent = cross_axis_size;
    return ascent + FlowAwareMarginBefore();
  }

  DCHECK(layout_result);
  return FlowAwareMarginBefore() +
         NGBoxFragment(
             algorithm->StyleRef().GetWritingMode(),
             algorithm->StyleRef().Direction(),
             To<NGPhysicalBoxFragment>(layout_result->PhysicalFragment()))
             .BaselineOrSynthesize();
}

LayoutUnit FlexItem::AvailableAlignmentSpace() const {
  LayoutUnit cross_extent = CrossAxisMarginExtent() + cross_axis_size;
  return Line()->cross_axis_extent - cross_extent;
}

bool FlexItem::HasAutoMarginsInCrossAxis() const {
  if (algorithm->IsHorizontalFlow()) {
    return style.MarginTop().IsAuto() || style.MarginBottom().IsAuto();
  }
  return style.MarginLeft().IsAuto() || style.MarginRight().IsAuto();
}

ItemPosition FlexItem::Alignment() const {
  return FlexLayoutAlgorithm::AlignmentForChild(*algorithm->Style(), style);
}

void FlexItem::UpdateAutoMarginsInMainAxis(LayoutUnit auto_margin_offset) {
  DCHECK_GE(auto_margin_offset, LayoutUnit());

  if (algorithm->IsHorizontalFlow()) {
    if (style.MarginLeft().IsAuto())
      physical_margins.left = auto_margin_offset;
    if (style.MarginRight().IsAuto())
      physical_margins.right = auto_margin_offset;
  } else {
    if (style.MarginTop().IsAuto())
      physical_margins.top = auto_margin_offset;
    if (style.MarginBottom().IsAuto())
      physical_margins.bottom = auto_margin_offset;
  }
}

bool FlexItem::UpdateAutoMarginsInCrossAxis(
    LayoutUnit available_alignment_space) {
  DCHECK_GE(available_alignment_space, LayoutUnit());

  bool is_horizontal = algorithm->IsHorizontalFlow();
  const Length& top_or_left =
      is_horizontal ? style.MarginTop() : style.MarginLeft();
  const Length& bottom_or_right =
      is_horizontal ? style.MarginBottom() : style.MarginRight();
  if (top_or_left.IsAuto() && bottom_or_right.IsAuto()) {
    desired_location.Move(LayoutUnit(), available_alignment_space / 2);
    if (is_horizontal) {
      physical_margins.top = available_alignment_space / 2;
      physical_margins.bottom = available_alignment_space / 2;
    } else {
      physical_margins.left = available_alignment_space / 2;
      physical_margins.right = available_alignment_space / 2;
    }
    return true;
  }
  bool should_adjust_top_or_left = true;
  if (algorithm->IsColumnFlow() && !style.IsLeftToRightDirection()) {
    // For column flows, only make this adjustment if topOrLeft corresponds to
    // the "before" margin, so that flipForRightToLeftColumn will do the right
    // thing.
    should_adjust_top_or_left = false;
  }
  if (!algorithm->IsColumnFlow() && style.IsFlippedBlocksWritingMode()) {
    // If we are a flipped writing mode, we need to adjust the opposite side.
    // This is only needed for row flows because this only affects the
    // block-direction axis.
    should_adjust_top_or_left = false;
  }

  if (top_or_left.IsAuto()) {
    if (should_adjust_top_or_left)
      desired_location.Move(LayoutUnit(), available_alignment_space);

    if (is_horizontal)
      physical_margins.top = available_alignment_space;
    else
      physical_margins.left = available_alignment_space;
    return true;
  }
  if (bottom_or_right.IsAuto()) {
    if (!should_adjust_top_or_left)
      desired_location.Move(LayoutUnit(), available_alignment_space);

    if (is_horizontal)
      physical_margins.bottom = available_alignment_space;
    else
      physical_margins.right = available_alignment_space;
    return true;
  }
  return false;
}

void FlexItem::ComputeStretchedSize() {
  DCHECK_EQ(Alignment(), ItemPosition::kStretch);
  LayoutUnit stretched_size =
      std::max(cross_axis_border_padding,
               Line()->cross_axis_extent - CrossAxisMarginExtent());
  if (box) {
    if (MainAxisIsInlineAxis() && style.LogicalHeight().IsAuto()) {
      cross_axis_size = box->ConstrainLogicalHeightByMinMax(
          stretched_size, box->IntrinsicContentLogicalHeight());
    } else if (!MainAxisIsInlineAxis() && style.LogicalWidth().IsAuto()) {
      const LayoutFlexibleBox* flexbox = ToLayoutFlexibleBox(box->Parent());
      cross_axis_size = box->ConstrainLogicalWidthByMinMax(
          stretched_size, flexbox->CrossAxisContentExtent(), flexbox);
    }
    return;
  }

  if ((MainAxisIsInlineAxis() && style.LogicalHeight().IsAuto()) ||
      (!MainAxisIsInlineAxis() && style.LogicalWidth().IsAuto()))
    cross_axis_size = min_max_cross_sizes->ClampSizeToMinAndMax(stretched_size);
}

// static
LayoutUnit FlexItem::AlignmentOffset(LayoutUnit available_free_space,
                                     ItemPosition position,
                                     LayoutUnit ascent,
                                     LayoutUnit max_ascent,
                                     bool is_wrap_reverse,
                                     bool is_deprecated_webkit_box) {
  switch (position) {
    case ItemPosition::kLegacy:
    case ItemPosition::kAuto:
    case ItemPosition::kNormal:
      NOTREACHED();
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
    case ItemPosition::kCenter: {
      const LayoutUnit result = (available_free_space / 2);
      return is_deprecated_webkit_box ? result.ClampNegativeToZero() : result;
    }
    case ItemPosition::kBaseline:
      // FIXME: If we get here in columns, we want the use the descent, except
      // we currently can't get the ascent/descent of orthogonal children.
      // https://bugs.webkit.org/show_bug.cgi?id=98076
      return max_ascent - ascent;
    case ItemPosition::kLastBaseline:
    case ItemPosition::kSelfStart:
    case ItemPosition::kSelfEnd:
    case ItemPosition::kStart:
    case ItemPosition::kEnd:
    case ItemPosition::kLeft:
    case ItemPosition::kRight:
      // TODO(jfernandez): Implement these (https://crbug.com/722287).
      break;
  }
  return LayoutUnit();
}
void FlexLine::FreezeViolations(ViolationsVector& violations) {
  const ComputedStyle& flex_box_style = algorithm->StyleRef();
  for (size_t i = 0; i < violations.size(); ++i) {
    DCHECK(!violations[i]->frozen) << i;
    const ComputedStyle& child_style = violations[i]->style;
    LayoutUnit child_size = violations[i]->flexed_content_size;
    remaining_free_space -= child_size - violations[i]->flex_base_content_size;
    total_flex_grow -= child_style.ResolvedFlexGrow(flex_box_style);
    const float flex_shrink = child_style.ResolvedFlexShrink(flex_box_style);
    total_flex_shrink -= flex_shrink;
    total_weighted_flex_shrink -=
        flex_shrink * violations[i]->flex_base_content_size;
    // total_weighted_flex_shrink can be negative when we exceed the precision
    // of a double when we initially calculate total_weighted_flex_shrink. We
    // then subtract each child's weighted flex shrink with full precision, now
    // leading to a negative result. See
    // css3/flexbox/large-flex-shrink-assert.html
    total_weighted_flex_shrink = std::max(total_weighted_flex_shrink, 0.0);
    violations[i]->frozen = true;
  }
}

void FlexLine::FreezeInflexibleItems() {
  // Per https://drafts.csswg.org/css-flexbox/#resolve-flexible-lengths step 2,
  // we freeze all items with a flex factor of 0 as well as those with a min/max
  // size violation.
  FlexSign flex_sign = Sign();
  remaining_free_space = container_main_inner_size - sum_flex_base_size;

  ViolationsVector new_inflexible_items;
  const ComputedStyle& flex_box_style = algorithm->StyleRef();
  for (size_t i = 0; i < line_items.size(); ++i) {
    FlexItem& flex_item = line_items[i];
    DCHECK(!flex_item.frozen) << i;
    float flex_factor =
        (flex_sign == kPositiveFlexibility)
            ? flex_item.style.ResolvedFlexGrow(flex_box_style)
            : flex_item.style.ResolvedFlexShrink(flex_box_style);
    if (flex_factor == 0 ||
        (flex_sign == kPositiveFlexibility &&
         flex_item.flex_base_content_size >
             flex_item.hypothetical_main_content_size) ||
        (flex_sign == kNegativeFlexibility &&
         flex_item.flex_base_content_size <
             flex_item.hypothetical_main_content_size)) {
      flex_item.flexed_content_size = flex_item.hypothetical_main_content_size;
      new_inflexible_items.push_back(&flex_item);
    }
  }
  FreezeViolations(new_inflexible_items);
  initial_free_space = remaining_free_space;
}

bool FlexLine::ResolveFlexibleLengths() {
  LayoutUnit total_violation;
  LayoutUnit used_free_space;
  ViolationsVector min_violations;
  ViolationsVector max_violations;

  FlexSign flex_sign = Sign();
  double sum_flex_factors =
      (flex_sign == kPositiveFlexibility) ? total_flex_grow : total_flex_shrink;
  if (sum_flex_factors > 0 && sum_flex_factors < 1) {
    LayoutUnit fractional(initial_free_space * sum_flex_factors);
    if (fractional.Abs() < remaining_free_space.Abs())
      remaining_free_space = fractional;
  }

  const ComputedStyle& flex_box_style = algorithm->StyleRef();
  for (size_t i = 0; i < line_items.size(); ++i) {
    FlexItem& flex_item = line_items[i];

    // This check also covers out-of-flow children.
    if (flex_item.frozen)
      continue;

    LayoutUnit child_size = flex_item.flex_base_content_size;
    double extra_space = 0;
    if (remaining_free_space > 0 && total_flex_grow > 0 &&
        flex_sign == kPositiveFlexibility && std::isfinite(total_flex_grow)) {
      extra_space = remaining_free_space *
                    flex_item.style.ResolvedFlexGrow(flex_box_style) /
                    total_flex_grow;
    } else if (remaining_free_space < 0 && total_weighted_flex_shrink > 0 &&
               flex_sign == kNegativeFlexibility &&
               std::isfinite(total_weighted_flex_shrink) &&
               flex_item.style.ResolvedFlexShrink(flex_box_style)) {
      extra_space = remaining_free_space *
                    flex_item.style.ResolvedFlexShrink(flex_box_style) *
                    flex_item.flex_base_content_size /
                    total_weighted_flex_shrink;
    }
    if (std::isfinite(extra_space))
      child_size += LayoutUnit::FromFloatRound(extra_space);

    LayoutUnit adjusted_child_size = flex_item.ClampSizeToMinAndMax(child_size);
    DCHECK_GE(adjusted_child_size, 0);
    flex_item.flexed_content_size = adjusted_child_size;
    used_free_space += adjusted_child_size - flex_item.flex_base_content_size;

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
    remaining_free_space -= used_free_space;
  }

  return !total_violation;
}

LayoutUnit FlexLine::ApplyMainAxisAutoMarginAdjustment() {
  if (remaining_free_space <= LayoutUnit())
    return LayoutUnit();

  int number_of_auto_margins = 0;
  bool is_horizontal = algorithm->IsHorizontalFlow();
  for (size_t i = 0; i < line_items.size(); ++i) {
    const ComputedStyle& style = line_items[i].style;
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
      remaining_free_space / number_of_auto_margins;
  remaining_free_space = LayoutUnit();
  return size_of_auto_margin;
}

void FlexLine::ComputeLineItemsPosition(LayoutUnit main_axis_start_offset,
                                        LayoutUnit main_axis_end_offset,
                                        LayoutUnit& cross_axis_offset) {
  this->main_axis_offset = main_axis_start_offset;
  // Recalculate the remaining free space. The adjustment for flex factors
  // between 0..1 means we can't just use remainingFreeSpace here.
  LayoutUnit total_item_size;
  for (size_t i = 0; i < line_items.size(); ++i)
    total_item_size += line_items[i].FlexedMarginBoxSize();
  remaining_free_space =
      container_main_inner_size - total_item_size -
      (line_items.size() - 1) * algorithm->gap_between_items_;

  const StyleContentAlignmentData justify_content =
      FlexLayoutAlgorithm::ResolvedJustifyContent(*algorithm->Style());

  LayoutUnit auto_margin_offset = ApplyMainAxisAutoMarginAdjustment();
  const LayoutUnit available_free_space = remaining_free_space;
  LayoutUnit initial_position =
      FlexLayoutAlgorithm::InitialContentPositionOffset(
          algorithm->StyleRef(), available_free_space, justify_content,
          line_items.size());

  LayoutUnit main_axis_offset = initial_position;
  sum_justify_adjustments += initial_position;
  LayoutUnit max_descent;  // Used when align-items: baseline.
  LayoutUnit max_child_cross_axis_extent;
  bool should_flip_main_axis;
  if (algorithm->IsNGFlexBox()) {
    should_flip_main_axis =
        algorithm->StyleRef().ResolvedIsRowReverseFlexDirection();
  } else {
    should_flip_main_axis =
        !algorithm->StyleRef().ResolvedIsColumnFlexDirection() &&
        !algorithm->IsLeftToRightFlow();
  }
  LayoutUnit width_when_flipped = container_logical_width;
  LayoutUnit flipped_offset;
  // -webkit-box, always did layout starting at 0. ltr and reverse were handled
  // by reversing the order of iteration. OTOH, flex always iterates in order
  // and flips the main coordinate. The following gives the same behavior for
  // -webkit-box while using the same iteration order as flex does by changing
  // how the flipped coordinate is calculated.
  if (should_flip_main_axis && algorithm->StyleRef().IsDeprecatedWebkitBox()) {
    // -webkit-box only distributed space when > 0.
    width_when_flipped =
        total_item_size + available_free_space.ClampNegativeToZero();
    flipped_offset = main_axis_end_offset;
  } else {
    main_axis_offset += main_axis_start_offset;
  }
  for (size_t i = 0; i < line_items.size(); ++i) {
    FlexItem& flex_item = line_items[i];

    flex_item.UpdateAutoMarginsInMainAxis(auto_margin_offset);

    LayoutUnit child_cross_axis_margin_box_extent;
    if (flex_item.Alignment() == ItemPosition::kBaseline &&
        !flex_item.HasAutoMarginsInCrossAxis()) {
      LayoutUnit ascent = flex_item.MarginBoxAscent();
      LayoutUnit descent =
          (flex_item.CrossAxisMarginExtent() + flex_item.cross_axis_size) -
          ascent;

      max_ascent = std::max(max_ascent, ascent);
      max_descent = std::max(max_descent, descent);

      child_cross_axis_margin_box_extent = max_ascent + max_descent;
    } else {
      child_cross_axis_margin_box_extent =
          flex_item.cross_axis_size + flex_item.CrossAxisMarginExtent();
    }
    max_child_cross_axis_extent = std::max(max_child_cross_axis_extent,
                                           child_cross_axis_margin_box_extent);

    main_axis_offset += flex_item.FlowAwareMarginStart();

    LayoutUnit child_main_extent = flex_item.FlexedBorderBoxSize();
    // In an RTL column situation, this will apply the margin-right/margin-end
    // on the left. This will be fixed later in
    // LayoutFlexibleBox::FlipForRightToLeftColumn.
    flex_item.desired_location = LayoutPoint(
        should_flip_main_axis ? width_when_flipped - main_axis_offset -
                                    child_main_extent + flipped_offset
                              : main_axis_offset,
        cross_axis_offset + flex_item.FlowAwareMarginBefore());
    main_axis_offset += child_main_extent + flex_item.FlowAwareMarginEnd();

    if (i != line_items.size() - 1) {
      // The last item does not get extra space added.
      LayoutUnit space_between =
          FlexLayoutAlgorithm::ContentDistributionSpaceBetweenChildren(
              available_free_space, justify_content, line_items.size());
      main_axis_offset += space_between + algorithm->gap_between_items_;
      // The gap is included in the intrinsic content block size, so don't add
      // it to sum_justify_adjustments.
      sum_justify_adjustments += space_between;
    }
  }

  main_axis_extent = main_axis_offset;

  this->cross_axis_offset = cross_axis_offset;
  cross_axis_extent = max_child_cross_axis_extent;

  cross_axis_offset += max_child_cross_axis_extent;
}

// static
LayoutUnit FlexLayoutAlgorithm::GapBetweenItems(
    const ComputedStyle& style,
    LogicalSize percent_resolution_sizes) {
  DCHECK_GE(percent_resolution_sizes.inline_size, 0);
  if (IsColumnFlow(style)) {
    if (const base::Optional<Length>& row_gap = style.RowGap()) {
      return MinimumValueForLength(
          *row_gap, percent_resolution_sizes.block_size.ClampNegativeToZero());
    }
    return LayoutUnit();
  }
  if (const base::Optional<Length>& column_gap = style.ColumnGap()) {
    return MinimumValueForLength(*column_gap,
                                 percent_resolution_sizes.inline_size);
  }
  return LayoutUnit();
}

// static
LayoutUnit FlexLayoutAlgorithm::GapBetweenLines(
    const ComputedStyle& style,
    LogicalSize percent_resolution_sizes) {
  DCHECK_GE(percent_resolution_sizes.inline_size, 0);
  if (!IsColumnFlow(style)) {
    if (const base::Optional<Length>& row_gap = style.RowGap()) {
      return MinimumValueForLength(
          *row_gap, percent_resolution_sizes.block_size.ClampNegativeToZero());
    }
    return LayoutUnit();
  }
  if (const base::Optional<Length>& column_gap = style.ColumnGap()) {
    return MinimumValueForLength(*column_gap,
                                 percent_resolution_sizes.inline_size);
  }
  return LayoutUnit();
}

FlexLayoutAlgorithm::FlexLayoutAlgorithm(const ComputedStyle* style,
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

FlexLine* FlexLayoutAlgorithm::ComputeNextFlexLine(
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
    total_flex_grow += flex_item.style.ResolvedFlexGrow(StyleRef());
    const float flex_shrink = flex_item.style.ResolvedFlexShrink(StyleRef());
    total_flex_shrink += flex_shrink;
    total_weighted_flex_shrink +=
        flex_shrink * flex_item.flex_base_content_size;
    sum_hypothetical_main_size +=
        flex_item.HypotheticalMainAxisMarginBoxSize() + gap_between_items_;
    flex_item.line_number = flex_lines_.size();
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

bool FlexLayoutAlgorithm::IsHorizontalFlow() const {
  return IsHorizontalFlow(*style_);
}

bool FlexLayoutAlgorithm::IsColumnFlow() const {
  return IsColumnFlow(*style_);
}

// static
bool FlexLayoutAlgorithm::IsColumnFlow(const ComputedStyle& style) {
  return style.ResolvedIsColumnFlexDirection();
}

// static
bool FlexLayoutAlgorithm::IsHorizontalFlow(const ComputedStyle& style) {
  if (style.IsHorizontalWritingMode())
    return !style.ResolvedIsColumnFlexDirection();
  return style.ResolvedIsColumnFlexDirection();
}

bool FlexLayoutAlgorithm::IsLeftToRightFlow() const {
  if (style_->ResolvedIsColumnFlexDirection()) {
    return blink::IsHorizontalWritingMode(style_->GetWritingMode()) ||
           IsFlippedLinesWritingMode(style_->GetWritingMode());
  }
  return style_->IsLeftToRightDirection() ^
         style_->ResolvedIsRowReverseFlexDirection();
}

// static
const StyleContentAlignmentData&
FlexLayoutAlgorithm::ContentAlignmentNormalBehavior() {
  // The justify-content property applies along the main axis, but since
  // flexing in the main axis is controlled by flex, stretch behaves as
  // flex-start (ignoring the specified fallback alignment, if any).
  // https://drafts.csswg.org/css-align/#distribution-flex
  static const StyleContentAlignmentData kNormalBehavior = {
      ContentPosition::kNormal, ContentDistributionType::kStretch};
  return kNormalBehavior;
}

bool FlexLayoutAlgorithm::ShouldApplyMinSizeAutoForChild(
    const LayoutBox& child) const {
  // css-flexbox section 4.5
  const Length& min = IsHorizontalFlow() ? child.StyleRef().MinWidth()
                                         : child.StyleRef().MinHeight();
  bool main_axis_is_childs_block_axis =
      IsHorizontalFlow() != child.StyleRef().IsHorizontalWritingMode();
  // TODO(dgrogan) Move this closer to ng_flex_layout_algorithm.cc:616.
  if (child.IsTable() && !main_axis_is_childs_block_axis)
    return true;
  bool intrinsic_in_childs_block_axis =
      main_axis_is_childs_block_axis &&
      (min.IsMinContent() || min.IsMaxContent() || min.IsMinIntrinsic() ||
       min.IsFitContent());
  if (!min.IsAuto() && !intrinsic_in_childs_block_axis)
    return false;

  // webkit-box treats min-size: auto as 0.
  if (StyleRef().IsDeprecatedWebkitBox())
    return false;

  // TODO(dgrogan): MainAxisOverflowForChild == kClip also qualifies, not just
  // kVisible.
  return !child.ShouldApplySizeContainment() &&
         MainAxisOverflowForChild(child) == EOverflow::kVisible;
}

LayoutUnit FlexLayoutAlgorithm::IntrinsicContentBlockSize() const {
  if (flex_lines_.IsEmpty())
    return LayoutUnit();

  if (IsColumnFlow()) {
    LayoutUnit max_size;
    for (const FlexLine& line : flex_lines_) {
      // Subtract main_axis_offset to remove border/padding
      max_size = std::max(line.main_axis_extent - line.sum_justify_adjustments -
                              line.main_axis_offset,
                          max_size);
    }
    return max_size;
  }

  const FlexLine& last_line = flex_lines_.back();
  // Subtract the first line's offset to remove border/padding
  return last_line.cross_axis_offset + last_line.cross_axis_extent -
         flex_lines_.front().cross_axis_offset +
         (flex_lines_.size() - 1) * gap_between_lines_;
}

void FlexLayoutAlgorithm::AlignFlexLines(LayoutUnit cross_axis_content_extent) {
  const StyleContentAlignmentData align_content = ResolvedAlignContent(*style_);
  if (align_content.GetPosition() == ContentPosition::kFlexStart &&
      gap_between_lines_ == 0) {
    return;
  }
  if (flex_lines_.IsEmpty() || !IsMultiline())
    return;
  LayoutUnit available_cross_axis_space =
      cross_axis_content_extent - (flex_lines_.size() - 1) * gap_between_lines_;
  for (const FlexLine& line : flex_lines_)
    available_cross_axis_space -= line.cross_axis_extent;

  LayoutUnit line_offset =
      InitialContentPositionOffset(StyleRef(), available_cross_axis_space,
                                   align_content, flex_lines_.size());
  for (FlexLine& line_context : flex_lines_) {
    line_context.cross_axis_offset += line_offset;

    for (FlexItem& flex_item : line_context.line_items) {
      flex_item.desired_location.SetY(flex_item.desired_location.Y() +
                                      line_offset);
    }
    if (align_content.Distribution() == ContentDistributionType::kStretch &&
        available_cross_axis_space > 0) {
      line_context.cross_axis_extent +=
          available_cross_axis_space /
          static_cast<unsigned>(flex_lines_.size());
    }

    line_offset +=
        ContentDistributionSpaceBetweenChildren(
            available_cross_axis_space, align_content, flex_lines_.size()) +
        gap_between_lines_;
  }
}

void FlexLayoutAlgorithm::AlignChildren() {
  // Keep track of the space between the baseline edge and the after edge of
  // the box for each line.
  Vector<LayoutUnit> min_margin_after_baselines;

  for (FlexLine& line_context : flex_lines_) {
    LayoutUnit min_margin_after_baseline = LayoutUnit::Max();
    LayoutUnit max_ascent = line_context.max_ascent;

    for (FlexItem& flex_item : line_context.line_items) {
      if (flex_item.UpdateAutoMarginsInCrossAxis(
              flex_item.AvailableAlignmentSpace().ClampNegativeToZero()))
        continue;

      ItemPosition position = flex_item.Alignment();
      if (position == ItemPosition::kStretch) {
        flex_item.ComputeStretchedSize();
        flex_item.needs_relayout_for_stretch = true;
      }
      LayoutUnit available_space = flex_item.AvailableAlignmentSpace();
      LayoutUnit offset = FlexItem::AlignmentOffset(
          available_space, position, flex_item.MarginBoxAscent(), max_ascent,
          StyleRef().FlexWrap() == EFlexWrap::kWrapReverse,
          StyleRef().IsDeprecatedWebkitBox());
      flex_item.desired_location.Move(LayoutUnit(), offset);
      if (position == ItemPosition::kBaseline &&
          StyleRef().FlexWrap() == EFlexWrap::kWrapReverse) {
        min_margin_after_baseline =
            std::min(min_margin_after_baseline,
                     flex_item.AvailableAlignmentSpace() - offset);
      }
    }
    min_margin_after_baselines.push_back(min_margin_after_baseline);
  }

  if (StyleRef().FlexWrap() != EFlexWrap::kWrapReverse)
    return;

  // wrap-reverse flips the cross axis start and end. For baseline alignment,
  // this means we need to align the after edge of baseline elements with the
  // after edge of the flex line.
  wtf_size_t line_number = 0;
  for (FlexLine& line_context : flex_lines_) {
    LayoutUnit min_margin_after_baseline =
        min_margin_after_baselines[line_number++];
    for (FlexItem& flex_item : line_context.line_items) {
      if (flex_item.Alignment() == ItemPosition::kBaseline &&
          !flex_item.HasAutoMarginsInCrossAxis() && min_margin_after_baseline) {
        flex_item.desired_location.Move(LayoutUnit(),
                                        min_margin_after_baseline);
      }
    }
  }
}

void FlexLayoutAlgorithm::FlipForWrapReverse(
    LayoutUnit cross_axis_start_edge,
    LayoutUnit cross_axis_content_size) {
  DCHECK_EQ(Style()->FlexWrap(), EFlexWrap::kWrapReverse);
  for (FlexLine& line_context : flex_lines_) {
    LayoutUnit original_offset =
        line_context.cross_axis_offset - cross_axis_start_edge;
    LayoutUnit new_offset = cross_axis_content_size - original_offset -
                            line_context.cross_axis_extent;
    LayoutUnit wrap_reverse_difference = new_offset - original_offset;
    for (FlexItem& flex_item : line_context.line_items)
      flex_item.desired_location.Move(LayoutUnit(), wrap_reverse_difference);
  }
}

TransformedWritingMode FlexLayoutAlgorithm::GetTransformedWritingMode() const {
  return GetTransformedWritingMode(*style_);
}

// static
TransformedWritingMode FlexLayoutAlgorithm::GetTransformedWritingMode(
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
StyleContentAlignmentData FlexLayoutAlgorithm::ResolvedJustifyContent(
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
  ContentDistributionType distribution =
      is_webkit_box ? BoxPackToContentDistribution(style.BoxPack())
                    : style.ResolvedJustifyContentDistribution(
                          ContentAlignmentNormalBehavior());
  OverflowAlignment overflow = style.JustifyContentOverflowAlignment();
  // For flex, justify-content: stretch behaves as flex-start:
  // https://drafts.csswg.org/css-align/#distribution-flex
  if (!is_webkit_box && distribution == ContentDistributionType::kStretch) {
    position = ContentPosition::kFlexStart;
    distribution = ContentDistributionType::kDefault;
  }
  return StyleContentAlignmentData(position, distribution, overflow);
}

// static
StyleContentAlignmentData FlexLayoutAlgorithm::ResolvedAlignContent(
    const ComputedStyle& style) {
  ContentPosition position =
      style.ResolvedAlignContentPosition(ContentAlignmentNormalBehavior());
  ContentDistributionType distribution =
      style.ResolvedAlignContentDistribution(ContentAlignmentNormalBehavior());
  OverflowAlignment overflow = style.AlignContentOverflowAlignment();
  return StyleContentAlignmentData(position, distribution, overflow);
}

// static
ItemPosition FlexLayoutAlgorithm::AlignmentForChild(
    const ComputedStyle& flexbox_style,
    const ComputedStyle& child_style) {
  ItemPosition align =
      flexbox_style.IsDeprecatedWebkitBox()
          ? BoxAlignmentToItemPosition(flexbox_style.BoxAlign())
          : child_style
                .ResolvedAlignSelf(ItemPosition::kStretch, &flexbox_style)
                .GetPosition();
  DCHECK_NE(align, ItemPosition::kAuto);
  DCHECK_NE(align, ItemPosition::kNormal);

  if (align == ItemPosition::kBaseline &&
      IsHorizontalFlow(flexbox_style) != child_style.IsHorizontalWritingMode())
    align = ItemPosition::kFlexStart;

  if (flexbox_style.FlexWrap() == EFlexWrap::kWrapReverse) {
    if (align == ItemPosition::kFlexStart)
      align = ItemPosition::kFlexEnd;
    else if (align == ItemPosition::kFlexEnd)
      align = ItemPosition::kFlexStart;
  }

  return align;
}

// static
LayoutUnit FlexLayoutAlgorithm::InitialContentPositionOffset(
    const ComputedStyle& style,
    LayoutUnit available_free_space,
    const StyleContentAlignmentData& data,
    unsigned number_of_items) {
  if (available_free_space <= 0 && style.IsDeprecatedWebkitBox()) {
    // -webkit-box only considers |available_free_space| if > 0.
    return LayoutUnit();
  }
  if (data.GetPosition() == ContentPosition::kFlexEnd)
    return available_free_space;
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
LayoutUnit FlexLayoutAlgorithm::ContentDistributionSpaceBetweenChildren(
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

EOverflow FlexLayoutAlgorithm::MainAxisOverflowForChild(
    const LayoutBox& child) const {
  if (IsHorizontalFlow())
    return child.StyleRef().OverflowX();
  return child.StyleRef().OverflowY();
}

// Above, we calculated the positions of items in a column reverse container as
// if they were in a column. Now that we know the block size of the container we
// can flip the position of every item.
void FlexLayoutAlgorithm::LayoutColumnReverse(
    LayoutUnit main_axis_content_size,
    LayoutUnit border_scrollbar_padding_before) {
  DCHECK(IsColumnFlow());
  DCHECK(Style()->ResolvedIsColumnReverseFlexDirection());
  DCHECK(all_items_.IsEmpty() || IsNGFlexBox())
      << "This method relies on NG having passed in 0 for initial main axis "
         "offset for column-reverse flex boxes. That needs to be fixed if this "
         "method is to be used in legacy.";
  for (FlexLine& line_context : FlexLines()) {
    for (wtf_size_t child_number = 0;
         child_number < line_context.line_items.size(); ++child_number) {
      FlexItem& flex_item = line_context.line_items[child_number];
      LayoutUnit item_main_size = flex_item.FlexedBorderBoxSize();

      NGBoxStrut margins = flex_item.physical_margins.ConvertToLogical(
          Style()->GetWritingMode(), Style()->Direction());

      // We passed 0 as the initial main_axis offset to ComputeLineItemsPosition
      // for ColumnReverse containers so here we have to add the
      // border_scrollbar_padding of the container.
      flex_item.desired_location.SetX(
          main_axis_content_size + border_scrollbar_padding_before -
          flex_item.desired_location.X() - item_main_size - margins.block_end +
          margins.block_start);
    }
  }
}

bool FlexLayoutAlgorithm::IsNGFlexBox() const {
  DCHECK(!all_items_.IsEmpty())
      << "You can't call IsNGFlexBox before adding items.";
  // The FlexItems created by legacy will have an empty ng_input_node. An NG
  // FlexItem's ng_input_node will have a LayoutBox.
  return all_items_.at(0).ng_input_node.GetLayoutBox();
}

}  // namespace blink
