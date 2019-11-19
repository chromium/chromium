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
#include "third_party/blink/renderer/core/layout/min_max_size.h"

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

FlexItem::FlexItem(LayoutBox* box,
                   LayoutUnit flex_base_content_size,
                   MinMaxSize min_max_sizes,
                   base::Optional<MinMaxSize> min_max_cross_axis_sizes,
                   LayoutUnit main_axis_border_padding,
                   LayoutUnit main_axis_margin)
    : algorithm(nullptr),
      line_number(0),
      box(box),
      flex_base_content_size(flex_base_content_size),
      min_max_sizes(min_max_sizes),
      min_max_cross_sizes(min_max_cross_axis_sizes),
      hypothetical_main_content_size(
          min_max_sizes.ClampSizeToMinAndMax(flex_base_content_size)),
      main_axis_border_padding(main_axis_border_padding),
      main_axis_margin(main_axis_margin),
      frozen(false),
      needs_relayout_for_stretch(false),
      ng_input_node(/* LayoutBox* */ nullptr) {
  DCHECK(!box->IsOutOfFlowPositioned());
  DCHECK_GE(min_max_sizes.max_size, LayoutUnit())
      << "Use LayoutUnit::Max() for no max size";
}

bool FlexItem::MainAxisIsInlineAxis() const {
  return algorithm->IsHorizontalFlow() == box->IsHorizontalWritingMode();
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

LayoutUnit FlexItem::AvailableAlignmentSpace() const {
  LayoutUnit cross_extent = CrossAxisMarginExtent() + cross_axis_size;
  return Line()->cross_axis_extent - cross_extent;
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

bool FlexItem::UpdateAutoMarginsInCrossAxis(
    LayoutUnit available_alignment_space) {
  DCHECK(!box->IsOutOfFlowPositioned());
  DCHECK_GE(available_alignment_space, LayoutUnit());

  bool is_horizontal = algorithm->IsHorizontalFlow();
  const Length& top_or_left = is_horizontal ? box->StyleRef().MarginTop()
                                            : box->StyleRef().MarginLeft();
  const Length& bottom_or_right = is_horizontal ? box->StyleRef().MarginBottom()
                                                : box->StyleRef().MarginRight();
  if (top_or_left.IsAuto() && bottom_or_right.IsAuto()) {
    desired_location.Move(LayoutUnit(), available_alignment_space / 2);
    if (is_horizontal) {
      box->SetMarginTop(available_alignment_space / 2);
      box->SetMarginBottom(available_alignment_space / 2);
    } else {
      box->SetMarginLeft(available_alignment_space / 2);
      box->SetMarginRight(available_alignment_space / 2);
    }
    return true;
  }
  bool should_adjust_top_or_left = true;
  if (algorithm->IsColumnFlow() && !box->StyleRef().IsLeftToRightDirection()) {
    // For column flows, only make this adjustment if topOrLeft corresponds to
    // the "before" margin, so that flipForRightToLeftColumn will do the right
    // thing.
    should_adjust_top_or_left = false;
  }
  if (!algorithm->IsColumnFlow() &&
      box->StyleRef().IsFlippedBlocksWritingMode()) {
    // If we are a flipped writing mode, we need to adjust the opposite side.
    // This is only needed for row flows because this only affects the
    // block-direction axis.
    should_adjust_top_or_left = false;
  }

  if (top_or_left.IsAuto()) {
    if (should_adjust_top_or_left)
      desired_location.Move(LayoutUnit(), available_alignment_space);

    if (is_horizontal)
      box->SetMarginTop(available_alignment_space);
    else
      box->SetMarginLeft(available_alignment_space);
    return true;
  }
  if (bottom_or_right.IsAuto()) {
    if (!should_adjust_top_or_left)
      desired_location.Move(LayoutUnit(), available_alignment_space);

    if (is_horizontal)
      box->SetMarginBottom(available_alignment_space);
    else
      box->SetMarginRight(available_alignment_space);
    return true;
  }
  return false;
}

void FlexItem::ComputeStretchedSize() {
  DCHECK_EQ(Alignment(), ItemPosition::kStretch);
  if (MainAxisIsInlineAxis() && box->StyleRef().LogicalHeight().IsAuto()) {
    LayoutUnit stretched_logical_height =
        std::max(box->BorderAndPaddingLogicalHeight(),
                 Line()->cross_axis_extent - CrossAxisMarginExtent());
    cross_axis_size = box->ConstrainLogicalHeightByMinMax(
        stretched_logical_height, box->IntrinsicContentLogicalHeight());
  } else if (!MainAxisIsInlineAxis() &&
             box->StyleRef().LogicalWidth().IsAuto()) {
    LayoutUnit child_width =
        (Line()->cross_axis_extent - CrossAxisMarginExtent())
            .ClampNegativeToZero();
    if (LayoutFlexibleBox* flexbox = ToLayoutFlexibleBoxOrNull(box->Parent())) {
      cross_axis_size = box->ConstrainLogicalWidthByMinMax(
          child_width, flexbox->CrossAxisContentExtent(), flexbox);
    } else {
      DCHECK(box->Parent()->IsLayoutNGFlexibleBox());
      cross_axis_size = min_max_cross_sizes->ClampSizeToMinAndMax(child_width);
    }
  }
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
    LayoutBox* child = violations[i]->box;
    LayoutUnit child_size = violations[i]->flexed_content_size;
    remaining_free_space -= child_size - violations[i]->flex_base_content_size;
    total_flex_grow -= child->StyleRef().ResolvedFlexGrow(flex_box_style);
    const float flex_shrink =
        child->StyleRef().ResolvedFlexShrink(flex_box_style);
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
    LayoutBox* child = flex_item.box;
    DCHECK(!flex_item.box->IsOutOfFlowPositioned());
    DCHECK(!flex_item.frozen) << i;
    float flex_factor =
        (flex_sign == kPositiveFlexibility)
            ? child->StyleRef().ResolvedFlexGrow(flex_box_style)
            : child->StyleRef().ResolvedFlexShrink(flex_box_style);
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
    LayoutBox* child = flex_item.box;

    // This check also covers out-of-flow children.
    if (flex_item.frozen)
      continue;

    LayoutUnit child_size = flex_item.flex_base_content_size;
    double extra_space = 0;
    if (remaining_free_space > 0 && total_flex_grow > 0 &&
        flex_sign == kPositiveFlexibility && std::isfinite(total_flex_grow)) {
      extra_space = remaining_free_space *
                    child->StyleRef().ResolvedFlexGrow(flex_box_style) /
                    total_flex_grow;
    } else if (remaining_free_space < 0 && total_weighted_flex_shrink > 0 &&
               flex_sign == kNegativeFlexibility &&
               std::isfinite(total_weighted_flex_shrink) &&
               child->StyleRef().ResolvedFlexShrink(flex_box_style)) {
      extra_space = remaining_free_space *
                    child->StyleRef().ResolvedFlexShrink(flex_box_style) *
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
  this->main_axis_offset = main_axis_offset;
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
  LayoutUnit initial_position =
      FlexLayoutAlgorithm::InitialContentPositionOffset(
          algorithm->StyleRef(), available_free_space, justify_content,
          line_items.size());

  main_axis_offset += initial_position;
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
  LayoutUnit width_for_rtl = container_logical_width;
  // -webkit-box always started layout at an origin of 0, regardless of
  // direction.
  if (should_flip_main_axis && algorithm->StyleRef().IsDeprecatedWebkitBox())
    width_for_rtl = sum_hypothetical_main_size;
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
    flex_item.desired_location =
        LayoutPoint(should_flip_main_axis
                        ? width_for_rtl - main_axis_offset - child_main_extent
                        : main_axis_offset,
                    cross_axis_offset + flex_item.FlowAwareMarginBefore());
    main_axis_offset += child_main_extent + flex_item.FlowAwareMarginEnd();

    if (i != line_items.size() - 1) {
      // The last item does not get extra space added.
      LayoutUnit space_between =
          FlexLayoutAlgorithm::ContentDistributionSpaceBetweenChildren(
              available_free_space, justify_content, line_items.size());
      main_axis_offset += space_between;
      sum_justify_adjustments += space_between;
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
    FlexItem& flex_item = all_items_[next_item_index_];
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
    total_flex_grow += flex_item.box->StyleRef().ResolvedFlexGrow(StyleRef());
    const float flex_shrink =
        flex_item.box->StyleRef().ResolvedFlexShrink(StyleRef());
    total_flex_shrink += flex_shrink;
    total_weighted_flex_shrink +=
        flex_shrink * flex_item.flex_base_content_size;
    sum_hypothetical_main_size += flex_item.HypotheticalMainAxisMarginBoxSize();
    flex_item.line_number = flex_lines_.size();
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
  return StyleRef().ResolvedIsColumnFlexDirection();
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
  if (!min.IsAuto())
    return false;

  // webkit-box treats min-size: auto as 0.
  if (StyleRef().IsDeprecatedWebkitBox())
    return false;

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
         flex_lines_.front().cross_axis_offset;
}

void FlexLayoutAlgorithm::AlignFlexLines(LayoutUnit cross_axis_content_extent) {
  const StyleContentAlignmentData align_content = ResolvedAlignContent(*style_);
  if (align_content.GetPosition() == ContentPosition::kFlexStart)
    return;
  if (flex_lines_.IsEmpty() || !IsMultiline())
    return;
  LayoutUnit available_cross_axis_space = cross_axis_content_extent;
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

    line_offset += ContentDistributionSpaceBetweenChildren(
        available_cross_axis_space, align_content, flex_lines_.size());
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
      DCHECK(!flex_item.box->IsOutOfFlowPositioned());

      if (flex_item.UpdateAutoMarginsInCrossAxis(
              std::max(LayoutUnit(), flex_item.AvailableAlignmentSpace()))) {
        continue;
      }

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
    // -webkit-box treats end as start for horizontal rtl.
    if (position == ContentPosition::kFlexEnd &&
        !style.ResolvedIsColumnReverseFlexDirection() &&
        !style.IsLeftToRightDirection()) {
      position = ContentPosition::kFlexStart;
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
  if (available_free_space <= 0 && style.IsDeprecatedWebkitBox() &&
      style.ResolvedIsColumnFlexDirection()) {
    // -webkit-box with vertical orientation and no available spaces positions
    // relative to the start regardless of ContentPosition.
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
      // We passed 0 as the initial main_axis offset to ComputeLineItemsPosition
      // for ColumnReverse containers so here we have to add the
      // border_scrollbar_padding of the container.
      flex_item.desired_location.SetX(
          main_axis_content_size + border_scrollbar_padding_before -
          flex_item.desired_location.X() - item_main_size -
          flex_item.box->MarginAfter(Style()) +
          flex_item.box->MarginBefore(Style()));
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
