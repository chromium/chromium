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
#include "third_party/blink/renderer/core/layout/min_max_size.h"

namespace blink {

FlexItem::FlexItem(LayoutBox* box,
                   LayoutUnit flex_base_content_size,
                   MinMaxSize min_max_sizes,
                   LayoutUnit main_axis_border_and_padding,
                   LayoutUnit main_axis_margin)
    : box(box),
      flex_base_content_size(flex_base_content_size),
      min_max_sizes(min_max_sizes),
      hypothetical_main_content_size(
          min_max_sizes.ClampSizeToMinAndMax(flex_base_content_size)),
      main_axis_border_and_padding(main_axis_border_and_padding),
      main_axis_margin(main_axis_margin),
      frozen(false),
      ng_input_node(/* LayoutBox* */ nullptr) {
  DCHECK(!box->IsOutOfFlowPositioned());
  DCHECK_GE(min_max_sizes.max_size, LayoutUnit())
      << "Use LayoutUnit::Max() for no max size";
}

bool FlexItem::HasOrthogonalFlow() const {
  return algorithm->IsHorizontalFlow() != box->IsHorizontalWritingMode();
}

LayoutUnit FlexItem::FlowAwareMarginStart() const {
  if (algorithm->IsHorizontalFlow()) {
    return algorithm->IsLeftToRightFlow() ? box->MarginLeft()
                                          : box->MarginRight();
  }
  return algorithm->IsLeftToRightFlow() ? box->MarginTop()
                                        : box->MarginBottom();
}

LayoutUnit FlexItem::FlowAwareMarginEnd() const {
  if (algorithm->IsHorizontalFlow()) {
    return algorithm->IsLeftToRightFlow() ? box->MarginRight()
                                          : box->MarginLeft();
  }
  return algorithm->IsLeftToRightFlow() ? box->MarginBottom()
                                        : box->MarginTop();
}

LayoutUnit FlexItem::FlowAwareMarginBefore() const {
  switch (algorithm->GetTransformedWritingMode()) {
    case TransformedWritingMode::kTopToBottomWritingMode:
      return box->MarginTop();
    case TransformedWritingMode::kBottomToTopWritingMode:
      return box->MarginBottom();
    case TransformedWritingMode::kLeftToRightWritingMode:
      return box->MarginLeft();
    case TransformedWritingMode::kRightToLeftWritingMode:
      return box->MarginRight();
  }
  NOTREACHED();
  return box->MarginTop();
}

LayoutUnit FlexItem::CrossAxisMarginExtent() const {
  return algorithm->IsHorizontalFlow() ? box->MarginHeight()
                                       : box->MarginWidth();
}

LayoutUnit FlexItem::MarginBoxAscent() const {
  LayoutUnit ascent(box->FirstLineBoxBaseline());
  if (ascent == -1)
    ascent = cross_axis_size;
  return ascent + FlowAwareMarginBefore();
}

LayoutUnit FlexItem::AvailableAlignmentSpace(
    LayoutUnit line_cross_axis_extent) const {
  LayoutUnit cross_extent = CrossAxisMarginExtent() + cross_axis_size;
  return line_cross_axis_extent - cross_extent;
}

bool FlexItem::HasAutoMarginsInCrossAxis() const {
  if (algorithm->IsHorizontalFlow()) {
    return box->StyleRef().MarginTop().IsAuto() ||
           box->StyleRef().MarginBottom().IsAuto();
  }
  return box->StyleRef().MarginLeft().IsAuto() ||
         box->StyleRef().MarginRight().IsAuto();
}

ItemPosition FlexItem::Alignment() const {
  return FlexLayoutAlgorithm::AlignmentForChild(*algorithm->Style(),
                                                box->StyleRef());
}

void FlexItem::UpdateAutoMarginsInMainAxis(LayoutUnit auto_margin_offset) {
  DCHECK_GE(auto_margin_offset, LayoutUnit());

  if (algorithm->IsHorizontalFlow()) {
    if (box->StyleRef().MarginLeft().IsAuto())
      box->SetMarginLeft(auto_margin_offset);
    if (box->StyleRef().MarginRight().IsAuto())
      box->SetMarginRight(auto_margin_offset);
  } else {
    if (box->StyleRef().MarginTop().IsAuto())
      box->SetMarginTop(auto_margin_offset);
    if (box->StyleRef().MarginBottom().IsAuto())
      box->SetMarginBottom(auto_margin_offset);
  }
}

void FlexLine::FreezeViolations(ViolationsVector& violations) {
  for (size_t i = 0; i < violations.size(); ++i) {
    DCHECK(!violations[i]->frozen) << i;
    LayoutBox* child = violations[i]->box;
    LayoutUnit child_size = violations[i]->flexed_content_size;
    remaining_free_space -= child_size - violations[i]->flex_base_content_size;
    total_flex_grow -= child->StyleRef().FlexGrow();
    total_flex_shrink -= child->StyleRef().FlexShrink();
    total_weighted_flex_shrink -=
        child->StyleRef().FlexShrink() * violations[i]->flex_base_content_size;
    // totalWeightedFlexShrink can be negative when we exceed the precision of
    // a double when we initially calcuate totalWeightedFlexShrink. We then
    // subtract each child's weighted flex shrink with full precision, now
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
  for (size_t i = 0; i < line_items.size(); ++i) {
    FlexItem& flex_item = line_items[i];
    LayoutBox* child = flex_item.box;
    DCHECK(!flex_item.box->IsOutOfFlowPositioned());
    DCHECK(!flex_item.frozen) << i;
    float flex_factor = (flex_sign == kPositiveFlexibility)
                            ? child->StyleRef().FlexGrow()
                            : child->StyleRef().FlexShrink();
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

  for (size_t i = 0; i < line_items.size(); ++i) {
    FlexItem& flex_item = line_items[i];
    LayoutBox* child = flex_item.box;

    // This check also covers out-of-flow children.
    if (flex_item.frozen)
      continue;

    LayoutUnit child_size = flex_item.flex_base_content_size;
    double extra_space = 0;
    if (remaining_free_space > 0 && total_flex_grow > 0 &&
        flex_sign == kPositiveFlexibility && std::isfinite(total_flex_grow)) {
      extra_space =
          remaining_free_space * child->StyleRef().FlexGrow() / total_flex_grow;
    } else if (remaining_free_space < 0 && total_weighted_flex_shrink > 0 &&
               flex_sign == kNegativeFlexibility &&
               std::isfinite(total_weighted_flex_shrink) &&
               child->StyleRef().FlexShrink()) {
      extra_space = remaining_free_space * child->StyleRef().FlexShrink() *
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
    LayoutBox* child = line_items[i].box;
    DCHECK(!child->IsOutOfFlowPositioned());
    if (is_horizontal) {
      if (child->StyleRef().MarginLeft().IsAuto())
        ++number_of_auto_margins;
      if (child->StyleRef().MarginRight().IsAuto())
        ++number_of_auto_margins;
    } else {
      if (child->StyleRef().MarginTop().IsAuto())
        ++number_of_auto_margins;
      if (child->StyleRef().MarginBottom().IsAuto())
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

void FlexLine::ComputeLineItemsPosition(LayoutUnit main_axis_offset,
                                        LayoutUnit& cross_axis_offset) {
  // Recalculate the remaining free space. The adjustment for flex factors
  // between 0..1 means we can't just use remainingFreeSpace here.
  remaining_free_space = container_main_inner_size;
  for (size_t i = 0; i < line_items.size(); ++i) {
    FlexItem& flex_item = line_items[i];
    DCHECK(!flex_item.box->IsOutOfFlowPositioned());
    remaining_free_space -= flex_item.FlexedMarginBoxSize();
  }

  const StyleContentAlignmentData justify_content =
      FlexLayoutAlgorithm::ResolvedJustifyContent(*algorithm->Style());

  LayoutUnit auto_margin_offset = ApplyMainAxisAutoMarginAdjustment();
  const LayoutUnit available_free_space = remaining_free_space;
  main_axis_offset += FlexLayoutAlgorithm::InitialContentPositionOffset(
      available_free_space, justify_content, line_items.size());
  LayoutUnit max_descent;  // Used when align-items: baseline.
  LayoutUnit max_child_cross_axis_extent;
  bool should_flip_main_axis = !algorithm->StyleRef().IsColumnFlexDirection() &&
                               !algorithm->IsLeftToRightFlow();
  for (size_t i = 0; i < line_items.size(); ++i) {
    FlexItem& flex_item = line_items[i];

    DCHECK(!flex_item.box->IsOutOfFlowPositioned());

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
      child_cross_axis_margin_box_extent = flex_item.cross_axis_intrinsic_size +
                                           flex_item.CrossAxisMarginExtent();
    }
    max_child_cross_axis_extent = std::max(max_child_cross_axis_extent,
                                           child_cross_axis_margin_box_extent);

    main_axis_offset += flex_item.FlowAwareMarginStart();

    LayoutUnit child_main_extent = flex_item.FlexedBorderBoxSize();
    // In an RTL column situation, this will apply the margin-right/margin-end
    // on the left. This will be fixed later in flipForRightToLeftColumn.
    flex_item.desired_location = LayoutPoint(
        should_flip_main_axis
            ? container_logical_width - main_axis_offset - child_main_extent
            : main_axis_offset,
        cross_axis_offset + flex_item.FlowAwareMarginBefore());
    main_axis_offset += child_main_extent + flex_item.FlowAwareMarginEnd();

    if (i != line_items.size() - 1) {
      // The last item does not get extra space added.
      main_axis_offset +=
          FlexLayoutAlgorithm::ContentDistributionSpaceBetweenChildren(
              available_free_space, justify_content, line_items.size());
    }
  }

  main_axis_extent = main_axis_offset;

  this->cross_axis_offset = cross_axis_offset;
  cross_axis_extent = max_child_cross_axis_extent;

  cross_axis_offset += max_child_cross_axis_extent;
}

FlexLayoutAlgorithm::FlexLayoutAlgorithm(const ComputedStyle* style,
                                         LayoutUnit line_break_length)
    : style_(style),
      line_break_length_(line_break_length),
      next_item_index_(0) {}

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
    const FlexItem& flex_item = all_items_[next_item_index_];
    DCHECK(!flex_item.box->IsOutOfFlowPositioned());
    if (IsMultiline() &&
        sum_hypothetical_main_size +
                flex_item.HypotheticalMainAxisMarginBoxSize() >
            line_break_length_ &&
        line_has_in_flow_item) {
      break;
    }
    line_has_in_flow_item = true;
    sum_flex_base_size += flex_item.FlexBaseMarginBoxSize();
    total_flex_grow += flex_item.box->StyleRef().FlexGrow();
    total_flex_shrink += flex_item.box->StyleRef().FlexShrink();
    total_weighted_flex_shrink += flex_item.box->StyleRef().FlexShrink() *
                                  flex_item.flex_base_content_size;
    sum_hypothetical_main_size += flex_item.HypotheticalMainAxisMarginBoxSize();
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
  return StyleRef().IsColumnFlexDirection();
}

bool FlexLayoutAlgorithm::IsHorizontalFlow(const ComputedStyle& style) {
  if (style.IsHorizontalWritingMode())
    return !style.IsColumnFlexDirection();
  return style.IsColumnFlexDirection();
}

bool FlexLayoutAlgorithm::IsLeftToRightFlow() const {
  if (style_->IsColumnFlexDirection()) {
    return blink::IsHorizontalWritingMode(style_->GetWritingMode()) ||
           IsFlippedLinesWritingMode(style_->GetWritingMode());
  }
  return style_->IsLeftToRightDirection() ^
         (style_->FlexDirection() == EFlexDirection::kRowReverse);
}

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
  Length min = IsHorizontalFlow() ? child.StyleRef().MinWidth()
                                  : child.StyleRef().MinHeight();
  return min.IsAuto() && !child.ShouldApplySizeContainment() &&
         MainAxisOverflowForChild(child) == EOverflow::kVisible;
}

TransformedWritingMode FlexLayoutAlgorithm::GetTransformedWritingMode() const {
  return GetTransformedWritingMode(*style_);
}

TransformedWritingMode FlexLayoutAlgorithm::GetTransformedWritingMode(
    const ComputedStyle& style) {
  WritingMode mode = style.GetWritingMode();
  if (!style.IsColumnFlexDirection()) {
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

StyleContentAlignmentData FlexLayoutAlgorithm::ResolvedJustifyContent(
    const ComputedStyle& style) {
  ContentPosition position =
      style.ResolvedJustifyContentPosition(ContentAlignmentNormalBehavior());
  ContentDistributionType distribution =
      style.ResolvedJustifyContentDistribution(
          ContentAlignmentNormalBehavior());
  OverflowAlignment overflow = style.JustifyContentOverflowAlignment();
  // For flex, justify-content: stretch behaves as flex-start:
  // https://drafts.csswg.org/css-align/#distribution-flex
  if (distribution == ContentDistributionType::kStretch) {
    position = ContentPosition::kFlexStart;
    distribution = ContentDistributionType::kDefault;
  }
  return StyleContentAlignmentData(position, distribution, overflow);
}

StyleContentAlignmentData FlexLayoutAlgorithm::ResolvedAlignContent(
    const ComputedStyle& style) {
  ContentPosition position =
      style.ResolvedAlignContentPosition(ContentAlignmentNormalBehavior());
  ContentDistributionType distribution =
      style.ResolvedAlignContentDistribution(ContentAlignmentNormalBehavior());
  OverflowAlignment overflow = style.AlignContentOverflowAlignment();
  return StyleContentAlignmentData(position, distribution, overflow);
}

ItemPosition FlexLayoutAlgorithm::AlignmentForChild(
    const ComputedStyle& flexbox_style,
    const ComputedStyle& child_style) {
  ItemPosition align =
      child_style.ResolvedAlignSelf(ItemPosition::kStretch, &flexbox_style)
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

LayoutUnit FlexLayoutAlgorithm::InitialContentPositionOffset(
    LayoutUnit available_free_space,
    const StyleContentAlignmentData& data,
    unsigned number_of_items) {
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

}  // namespace blink
