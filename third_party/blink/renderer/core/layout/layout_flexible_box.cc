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

#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"

#include <limits>
#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/flexible_box_algorithm.h"
#include "third_party/blink/renderer/core/layout/layout_state.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/paint/block_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

static bool HasAspectRatio(const LayoutBox& child) {
  return child.IsImage() || child.IsCanvas() || IsA<LayoutVideo>(child) ||
         !child.StyleRef().AspectRatio().IsAuto();
}

LayoutFlexibleBox::LayoutFlexibleBox(Element* element)
    : LayoutBlock(element),
      order_iterator_(this),
      number_of_in_flow_children_on_first_line_(-1),
      has_definite_height_(SizeDefiniteness::kUnknown),
      in_layout_(false) {
  DCHECK(!ChildrenInline());
}

LayoutFlexibleBox::~LayoutFlexibleBox() = default;

void LayoutFlexibleBox::Trace(Visitor* visitor) const {
  visitor->Trace(intrinsic_size_along_main_axis_);
  visitor->Trace(relaid_out_children_);
  visitor->Trace(order_iterator_);
  LayoutBlock::Trace(visitor);
}

bool LayoutFlexibleBox::IsChildAllowed(LayoutObject* object,
                                       const ComputedStyle& style) const {
  NOT_DESTROYED();
  const auto* select = DynamicTo<HTMLSelectElement>(GetNode());
  if (UNLIKELY(select && select->UsesMenuList())) {
    // For a size=1 <select>, we only render the active option label through the
    // InnerElement. We do not allow adding layout objects for options and
    // optgroups.
    return object->GetNode() == &select->InnerElement();
  }
  return LayoutBlock::IsChildAllowed(object, style);
}

MinMaxSizes LayoutFlexibleBox::ComputeIntrinsicLogicalWidths() const {
  NOT_DESTROYED();
  MinMaxSizes sizes;
  sizes +=
      BorderAndPaddingLogicalWidth() + ComputeLogicalScrollbars().InlineSum();

  if (HasOverrideIntrinsicContentLogicalWidth()) {
    sizes += OverrideIntrinsicContentLogicalWidth();
    return sizes;
  }
  LayoutUnit default_inline_size = DefaultIntrinsicContentInlineSize();
  if (default_inline_size != kIndefiniteSize) {
    sizes.max_size += default_inline_size;
    if (!StyleRef().LogicalWidth().IsPercentOrCalc())
      sizes.min_size = sizes.max_size;
    return sizes;
  }
  if (ShouldApplySizeContainment())
    return sizes;

  MinMaxSizes child_sizes;

  // FIXME: We're ignoring flex-basis here and we shouldn't. We can't start
  // honoring it though until the flex shorthand stops setting it to 0. See
  // https://bugs.webkit.org/show_bug.cgi?id=116117 and
  // https://crbug.com/240765.
  float previous_max_content_flex_fraction = -1;
  int number_of_items = 0;
  for (LayoutBox* child = FirstChildBox(); child;
       child = child->NextSiblingBox()) {
    if (child->IsOutOfFlowPositioned())
      continue;
    number_of_items++;

    LayoutUnit margin = MarginIntrinsicLogicalWidthForChild(*child);

    LayoutUnit min_preferred_logical_width;
    LayoutUnit max_preferred_logical_width;
    if (child->NeedsPreferredWidthsRecalculation())
      child->SetIntrinsicLogicalWidthsDirty();
    ComputeChildPreferredLogicalWidths(*child, min_preferred_logical_width,
                                       max_preferred_logical_width);
    DCHECK_GE(min_preferred_logical_width, LayoutUnit());
    DCHECK_GE(max_preferred_logical_width, LayoutUnit());
    min_preferred_logical_width += margin;
    max_preferred_logical_width += margin;
    if (!IsColumnFlow()) {
      child_sizes.max_size += max_preferred_logical_width;
      if (IsMultiline()) {
        // For multiline, the min preferred width is if you put a break between
        // each item.
        child_sizes.min_size =
            std::max(child_sizes.min_size, min_preferred_logical_width);
      } else {
        child_sizes.min_size += min_preferred_logical_width;
      }
    } else {
      child_sizes.min_size =
          std::max(min_preferred_logical_width, child_sizes.min_size);
      child_sizes.max_size =
          std::max(max_preferred_logical_width, child_sizes.max_size);
    }

    previous_max_content_flex_fraction = CountIntrinsicSizeForAlgorithmChange(
        max_preferred_logical_width, child, previous_max_content_flex_fraction);
  }

  if (!IsColumnFlow() && number_of_items > 0) {
    LayoutUnit gap_inline_size =
        (number_of_items - 1) *
        FlexLayoutAlgorithm::GapBetweenItems(
            StyleRef(),
            LogicalSize{ContentLogicalWidth(),
                        AvailableLogicalHeightForPercentageComputation()});
    child_sizes.max_size += gap_inline_size;
    if (!IsMultiline()) {
      child_sizes.min_size += gap_inline_size;
    }
  }

  child_sizes.max_size = std::max(child_sizes.min_size, child_sizes.max_size);

  // Due to negative margins, it is possible that we calculated a negative
  // intrinsic width. Make sure that we never return a negative width.
  child_sizes.min_size = std::max(LayoutUnit(), child_sizes.min_size);
  child_sizes.max_size = std::max(LayoutUnit(), child_sizes.max_size);

  sizes += child_sizes;
  return sizes;
}

float LayoutFlexibleBox::CountIntrinsicSizeForAlgorithmChange(
    LayoutUnit max_preferred_logical_width,
    LayoutBox* child,
    float previous_max_content_flex_fraction) const {
  NOT_DESTROYED();
  // Determine whether the new version of the intrinsic size algorithm of the
  // flexbox spec would produce a different result than our above algorithm.
  // The algorithm produces a different result iff the max-content flex
  // fraction (as defined in the new algorithm) is not identical for each flex
  // item.
  if (IsColumnFlow())
    return previous_max_content_flex_fraction;
  const Length& flex_basis = child->StyleRef().FlexBasis();
  float flex_grow = child->StyleRef().FlexGrow();
  // A flex-basis of auto will lead to a max-content flex fraction of zero, so
  // just like an inflexible item it would compute to a size of max-content, so
  // we ignore it here.
  if (flex_basis.IsAuto() || flex_grow == 0)
    return previous_max_content_flex_fraction;
  flex_grow = std::max(1.0f, flex_grow);
  float max_content_flex_fraction =
      max_preferred_logical_width.ToFloat() / flex_grow;
  if (previous_max_content_flex_fraction != -1 &&
      max_content_flex_fraction != previous_max_content_flex_fraction) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kFlexboxIntrinsicSizeAlgorithmIsDifferent);
  }
  return max_content_flex_fraction;
}

LayoutUnit LayoutFlexibleBox::SynthesizedBaselineFromBorderBox(
    const LayoutBox& box,
    LineDirectionMode direction) {
  return direction == kHorizontalLine ? box.Size().Height()
                                      : box.Size().Width();
}

LayoutUnit LayoutFlexibleBox::BaselinePosition(FontBaseline,
                                               bool,
                                               LineDirectionMode direction,
                                               LinePositionMode mode) const {
  NOT_DESTROYED();
  DCHECK_EQ(mode, kPositionOnContainingLine);
  // TODO(crbug.com/1131352): input[type=range] should not use
  // LayoutFlexibleBox. We should move out this code.
  if (const auto* input = DynamicTo<HTMLInputElement>(GetNode())) {
    if (input->type() == input_type_names::kRange) {
      return SynthesizedBaselineFromBorderBox(*this, direction) +
             MarginBefore();
    }
  }
  LayoutUnit baseline = FirstLineBoxBaseline();
  if (baseline == -1) {
    return SynthesizedBaselineFromBorderBox(*this, direction) +
           MarginLogicalHeight();
  }

  return BeforeMarginInLineDirection(direction) + baseline;
}

LayoutUnit LayoutFlexibleBox::FirstLineBoxBaseline() const {
  NOT_DESTROYED();
  if (IsWritingModeRoot() || number_of_in_flow_children_on_first_line_ <= 0 ||
      ShouldApplyLayoutContainment())
    return LayoutUnit(-1);
  LayoutBox* baseline_child = nullptr;
  int child_number = 0;
  for (LayoutBox* child = order_iterator_.First(); child;
       child = order_iterator_.Next()) {
    if (child->IsOutOfFlowPositioned())
      continue;
    if (FlexLayoutAlgorithm::AlignmentForChild(StyleRef(), child->StyleRef()) ==
            ItemPosition::kBaseline &&
        !HasAutoMarginsInCrossAxis(*child)) {
      baseline_child = child;
      break;
    }
    if (!baseline_child)
      baseline_child = child;

    ++child_number;
    if (child_number == number_of_in_flow_children_on_first_line_)
      break;
  }

  if (!baseline_child)
    return LayoutUnit(-1);

  if (!IsColumnFlow() && !MainAxisIsInlineAxis(*baseline_child)) {
    // TODO(cbiesinger): Should LogicalTop here be LogicalLeft?
    return CrossAxisExtentForChild(*baseline_child) +
           baseline_child->LogicalTop();
  }
  if (IsColumnFlow() && MainAxisIsInlineAxis(*baseline_child)) {
    return MainAxisExtentForChild(*baseline_child) +
           baseline_child->LogicalTop();
  }

  LayoutUnit baseline = baseline_child->FirstLineBoxBaseline();
  if (baseline == -1) {
    // FIXME: We should pass |direction| into firstLineBoxBaseline and stop
    // bailing out if we're a writing mode root. This would also fix some
    // cases where the flexbox is orthogonal to its container.
    LineDirectionMode direction =
        IsHorizontalWritingMode() ? kHorizontalLine : kVerticalLine;
    return SynthesizedBaselineFromBorderBox(*baseline_child, direction) +
           baseline_child->LogicalTop();
  }

  return baseline + baseline_child->LogicalTop();
}

LayoutUnit LayoutFlexibleBox::InlineBlockBaseline(
    LineDirectionMode direction) const {
  NOT_DESTROYED();
  return FirstLineBoxBaseline();
}

bool LayoutFlexibleBox::HasTopOverflow() const {
  NOT_DESTROYED();
  if (IsHorizontalWritingMode())
    return StyleRef().ResolvedIsColumnReverseFlexDirection();
  return StyleRef().IsLeftToRightDirection() ==
         StyleRef().ResolvedIsRowReverseFlexDirection();
}

bool LayoutFlexibleBox::HasLeftOverflow() const {
  NOT_DESTROYED();
  if (IsHorizontalWritingMode()) {
    return StyleRef().IsLeftToRightDirection() ==
           StyleRef().ResolvedIsRowReverseFlexDirection();
  }
  return (StyleRef().GetWritingMode() == WritingMode::kVerticalLr) ==
         StyleRef().ResolvedIsColumnReverseFlexDirection();
}

void LayoutFlexibleBox::MergeAnonymousFlexItems(LayoutObject* remove_child) {
  NOT_DESTROYED();
  // When we remove a flex item, and the previous and next siblings of the item
  // are text nodes wrapped in anonymous flex items, the adjacent text nodes
  // need to be merged into the same flex item.
  LayoutObject* prev = remove_child->PreviousSibling();
  if (!prev || !prev->IsAnonymousBlock())
    return;
  LayoutObject* next = remove_child->NextSibling();
  if (!next || !next->IsAnonymousBlock())
    return;
  To<LayoutBoxModelObject>(next)->MoveAllChildrenTo(
      To<LayoutBoxModelObject>(prev));
  To<LayoutBlockFlow>(next)->DeleteLineBoxTree();
  next->Destroy();
  intrinsic_size_along_main_axis_.erase(next);
}

void LayoutFlexibleBox::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  if (!DocumentBeingDestroyed() &&
      !StyleRef().IsDeprecatedFlexboxUsingFlexLayout()) {
    MergeAnonymousFlexItems(child);
  }

  LayoutBlock::RemoveChild(child);
  intrinsic_size_along_main_axis_.erase(child);
}

bool LayoutFlexibleBox::HitTestChildren(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset,
    HitTestPhase phase) {
  NOT_DESTROYED();
  if (phase != HitTestPhase::kForeground)
    return false;

  PhysicalOffset scrolled_offset = accumulated_offset;
  if (IsScrollContainer())
    scrolled_offset -= PhysicalOffset(PixelSnappedScrolledContentOffset());

  for (LayoutBox* child = LastChildBox(); child;
       child = child->PreviousSiblingBox()) {
    if (child->HasSelfPaintingLayer())
      continue;

    PhysicalOffset child_accumulated_offset =
        scrolled_offset + child->PhysicalLocation(this);
    bool child_hit = child->HitTestAllPhases(result, hit_test_location,
                                             child_accumulated_offset);
    if (child_hit) {
      UpdateHitTestResult(result,
                          hit_test_location.Point() - accumulated_offset);
      return true;
    }
  }
  return false;
}

void LayoutFlexibleBox::StyleDidChange(StyleDifference diff,
                                       const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutBlock::StyleDidChange(diff, old_style);

  if (old_style &&
      old_style->ResolvedAlignItems(SelfAlignmentNormalBehavior())
              .GetPosition() == ItemPosition::kStretch &&
      diff.NeedsFullLayout()) {
    // Flex items that were previously stretching need to be relayed out so we
    // can compute new available cross axis space. This is only necessary for
    // stretching since other alignment values don't change the size of the
    // box.
    for (LayoutBox* child = FirstChildBox(); child;
         child = child->NextSiblingBox()) {
      ItemPosition previous_alignment =
          child->StyleRef()
              .ResolvedAlignSelf(SelfAlignmentNormalBehavior(), old_style)
              .GetPosition();
      if (previous_alignment == ItemPosition::kStretch &&
          previous_alignment !=
              child->StyleRef()
                  .ResolvedAlignSelf(SelfAlignmentNormalBehavior(), Style())
                  .GetPosition())
        child->SetChildNeedsLayout(kMarkOnlyThis);
    }
  }
}

void LayoutFlexibleBox::UpdateBlockLayout(bool relayout_children) {
  NOT_DESTROYED();
  DCHECK(NeedsLayout());

  if (!relayout_children && SimplifiedLayout())
    return;

  relaid_out_children_.clear();
  base::AutoReset<bool> reset1(&in_layout_, true);
  DCHECK_EQ(has_definite_height_, SizeDefiniteness::kUnknown);

  if (UpdateLogicalWidthAndColumnWidth())
    relayout_children = true;

  SubtreeLayoutScope layout_scope(*this);
  LayoutUnit previous_height = LogicalHeight();
  SetLogicalHeight(BorderAndPaddingLogicalHeight() +
                   ComputeLogicalScrollbars().BlockSum());

  PaintLayerScrollableArea::DelayScrollOffsetClampScope delay_clamp_scope;

  {
    TextAutosizer::LayoutScope text_autosizer_layout_scope(this, &layout_scope);
    LayoutState state(*this);

    number_of_in_flow_children_on_first_line_ = -1;

    PrepareOrderIteratorAndMargins();

    LayoutFlexItems(relayout_children, layout_scope);
    if (PaintLayerScrollableArea::PreventRelayoutScope::RelayoutNeeded()) {
      // Recompute the logical width, because children may have added or removed
      // scrollbars.
      UpdateLogicalWidthAndColumnWidth();
      PaintLayerScrollableArea::FreezeScrollbarsScope freeze_scrollbars_scope;
      PrepareOrderIteratorAndMargins();
      LayoutFlexItems(true, layout_scope);
      PaintLayerScrollableArea::PreventRelayoutScope::ResetRelayoutNeeded();
    }

    if (LogicalHeight() != previous_height)
      relayout_children = true;

    LayoutPositionedObjects(relayout_children || IsDocumentElement());

    // FIXME: css3/flexbox/repaint-rtl-column.html seems to issue paint
    // invalidations for more overflow than it needs to.
    ComputeLayoutOverflow(ClientLogicalBottomAfterRepositioning());
  }

  // We have to reset this, because changes to our ancestors' style can affect
  // this value. Also, this needs to be before we call updateAfterLayout, as
  // that function may re-enter this one.
  has_definite_height_ = SizeDefiniteness::kUnknown;

  // Update our scroll information if we're overflow:auto/scroll/hidden now
  // that we know if we overflow or not.
  UpdateAfterLayout();

  ClearNeedsLayout();
}

void LayoutFlexibleBox::PaintChildren(const PaintInfo& paint_info,
                                      const PhysicalOffset&) const {
  NOT_DESTROYED();
  BlockPainter(*this).PaintChildrenAtomically(this->GetOrderIterator(),
                                              paint_info);
}

void LayoutFlexibleBox::RepositionLogicalHeightDependentFlexItems(
    FlexLayoutAlgorithm& algorithm) {
  NOT_DESTROYED();
  Vector<FlexLine>& line_contexts = algorithm.FlexLines();
  LayoutUnit cross_axis_start_edge = line_contexts.empty()
                                         ? LayoutUnit()
                                         : line_contexts[0].cross_axis_offset_;
  // If we have a single line flexbox, the line height is all the available
  // space. For flex-direction: row, this means we need to use the height, so
  // we do this after calling updateLogicalHeight.
  if (!IsMultiline() && !line_contexts.empty()) {
    line_contexts[0].cross_axis_extent_ = CrossAxisContentExtent();
  }

  AlignFlexLines(algorithm);

  AlignChildren(algorithm);

  if (StyleRef().FlexWrap() == EFlexWrap::kWrapReverse) {
    algorithm.FlipForWrapReverse(cross_axis_start_edge,
                                 CrossAxisContentExtent());
    for (FlexLine& line_context : line_contexts) {
      for (FlexItem& flex_item : line_context.line_items_) {
        ResetAlignmentForChild(*flex_item.box_,
                               flex_item.offset_->cross_axis_offset);
      }
    }
  }

  // direction:rtl + flex-direction:column means the cross-axis direction is
  // flipped.
  FlipForRightToLeftColumn(line_contexts);
}

DISABLE_CFI_PERF
LayoutUnit LayoutFlexibleBox::ClientLogicalBottomAfterRepositioning() {
  NOT_DESTROYED();
  LayoutUnit max_child_logical_bottom;
  for (LayoutBox* child = FirstChildBox(); child;
       child = child->NextSiblingBox()) {
    if (child->IsOutOfFlowPositioned())
      continue;
    LayoutUnit child_logical_bottom = LogicalTopForChild(*child) +
                                      LogicalHeightForChild(*child) +
                                      MarginAfterForChild(*child);
    max_child_logical_bottom =
        std::max(max_child_logical_bottom, child_logical_bottom);
  }
  return std::max(ClientLogicalBottom(),
                  max_child_logical_bottom + PaddingAfter());
}

bool LayoutFlexibleBox::MainAxisIsInlineAxis(const LayoutBox& child) const {
  NOT_DESTROYED();
  // If we have a horizontal flow, that means the main size is the width.
  // That's the inline size for horizontal writing modes, and the block
  // size in vertical writing modes. For a vertical flow, main size is the
  // height, so it's the inverse. So we need the inline size if we have a
  // horizontal flow and horizontal writing mode, or vertical flow and vertical
  // writing mode. Otherwise we need the block size.
  return IsHorizontalFlow() == child.IsHorizontalWritingMode();
}

bool LayoutFlexibleBox::IsColumnFlow() const {
  NOT_DESTROYED();
  return StyleRef().ResolvedIsColumnFlexDirection();
}

bool LayoutFlexibleBox::IsHorizontalFlow() const {
  NOT_DESTROYED();
  if (IsHorizontalWritingMode())
    return !IsColumnFlow();
  return IsColumnFlow();
}

bool LayoutFlexibleBox::IsLeftToRightFlow() const {
  NOT_DESTROYED();
  if (IsColumnFlow()) {
    return blink::IsHorizontalWritingMode(StyleRef().GetWritingMode()) ||
           IsFlippedLinesWritingMode(StyleRef().GetWritingMode());
  }
  return StyleRef().IsLeftToRightDirection() ^
         StyleRef().ResolvedIsRowReverseFlexDirection();
}

bool LayoutFlexibleBox::IsMultiline() const {
  NOT_DESTROYED();
  return StyleRef().FlexWrap() != EFlexWrap::kNowrap;
}

Length LayoutFlexibleBox::FlexBasisForChild(const LayoutBox& child) const {
  NOT_DESTROYED();
  Length flex_length = child.StyleRef().FlexBasis();
  if (flex_length.IsAuto()) {
    flex_length = IsHorizontalFlow() ? child.StyleRef().Width()
                                     : child.StyleRef().Height();
  }
  return flex_length;
}

LayoutUnit LayoutFlexibleBox::CrossAxisExtentForChild(
    const LayoutBox& child) const {
  NOT_DESTROYED();
  return IsHorizontalFlow() ? child.Size().Height() : child.Size().Width();
}

LayoutUnit LayoutFlexibleBox::ChildUnstretchedLogicalHeight(
    const LayoutBox& child) const {
  NOT_DESTROYED();
  // This should only be called if the logical height is the cross size
  DCHECK(MainAxisIsInlineAxis(child));
  if (NeedToStretchChildLogicalHeight(child)) {
    AutoClearOverrideLogicalHeight clear(const_cast<LayoutBox*>(&child));

    LayoutUnit child_intrinsic_content_logical_height;
    // If we have size containment specified, and are not overriding the
    // intrinsic content height, then the height is LayoutUnit(). In all other
    // cases, this if-condition will pass and set the intrinsic height.
    if (!child.ShouldApplySizeContainment() ||
        child.HasOverrideIntrinsicContentLogicalHeight()) {
      child_intrinsic_content_logical_height =
          child.IntrinsicContentLogicalHeight();
    }

    LayoutUnit child_intrinsic_logical_height =
        child_intrinsic_content_logical_height +
        child.ComputeLogicalScrollbars().BlockSum() +
        child.BorderAndPaddingLogicalHeight();
    LogicalExtentComputedValues values;
    child.ComputeLogicalHeight(child_intrinsic_logical_height, LayoutUnit(),
                               values);
    return values.extent_;
  }
  return child.LogicalHeight();
}

DISABLE_CFI_PERF
LayoutUnit LayoutFlexibleBox::ChildUnstretchedLogicalWidth(
    const LayoutBox& child) const {
  NOT_DESTROYED();
  // This should only be called if the logical width is the cross size
  DCHECK(!MainAxisIsInlineAxis(child));

  // We compute the width as if we were unstretched. Only the main axis
  // override size is set at this point.
  // However, if our cross axis length is definite we don't need to recompute
  // and can just return the already-set logical width.
  if (!CrossAxisLengthIsDefinite(child, child.StyleRef().LogicalWidth())) {
    AutoClearOverrideLogicalWidth clear(const_cast<LayoutBox*>(&child));

    LogicalExtentComputedValues values;
    child.ComputeLogicalWidth(values);
    return values.extent_;
  }

  return child.LogicalWidth();
}

LayoutUnit LayoutFlexibleBox::CrossAxisUnstretchedExtentForChild(
    const LayoutBox& child) const {
  NOT_DESTROYED();
  return MainAxisIsInlineAxis(child) ? ChildUnstretchedLogicalHeight(child)
                                     : ChildUnstretchedLogicalWidth(child);
}

LayoutUnit LayoutFlexibleBox::MainAxisExtentForChild(
    const LayoutBox& child) const {
  NOT_DESTROYED();
  return IsHorizontalFlow() ? child.Size().Width() : child.Size().Height();
}

LayoutUnit LayoutFlexibleBox::MainAxisContentExtentForChild(
    const LayoutBox& child) const {
  NOT_DESTROYED();
  return IsHorizontalFlow() ? child.ContentWidth() : child.ContentHeight();
}

LayoutUnit LayoutFlexibleBox::MainAxisContentExtentForChildIncludingScrollbar(
    const LayoutBox& child) const {
  NOT_DESTROYED();
  return IsHorizontalFlow()
             ? child.ContentWidth() + child.ComputeScrollbars().HorizontalSum()
             : child.ContentHeight() + child.ComputeScrollbars().VerticalSum();
}

LayoutUnit LayoutFlexibleBox::CrossAxisExtent() const {
  NOT_DESTROYED();
  return IsHorizontalFlow() ? Size().Height() : Size().Width();
}

LayoutUnit LayoutFlexibleBox::CrossAxisContentExtent() const {
  NOT_DESTROYED();
  return IsHorizontalFlow() ? ContentHeight() : ContentWidth();
}

LayoutUnit LayoutFlexibleBox::MainAxisContentExtent(
    LayoutUnit content_logical_height) {
  NOT_DESTROYED();
  if (IsColumnFlow()) {
    LogicalExtentComputedValues computed_values;
    LayoutUnit border_padding_and_scrollbar =
        BorderAndPaddingLogicalHeight() + ComputeLogicalScrollbars().BlockSum();
    LayoutUnit border_box_logical_height =
        content_logical_height + border_padding_and_scrollbar;
    ComputeLogicalHeight(border_box_logical_height, LogicalTop(),
                         computed_values);
    if (computed_values.extent_ == LayoutUnit::Max())
      return computed_values.extent_;
    return std::max(LayoutUnit(),
                    computed_values.extent_ - border_padding_and_scrollbar);
  }
  return ContentLogicalWidth();
}

LayoutUnit LayoutFlexibleBox::ComputeMainAxisExtentForChild(
    const LayoutBox& child,
    SizeType size_type,
    const Length& size,
    LayoutUnit border_and_padding) const {
  NOT_DESTROYED();
  if (!MainAxisIsInlineAxis(child)) {
    // We don't have to check for "auto" here - computeContentLogicalHeight
    // will just return -1 for that case anyway. It's safe to access
    // scrollbarLogicalHeight here because ComputeNextFlexLine will have
    // already forced layout on the child. We previously layed out the child
    // if necessary (see ComputeNextFlexLine and the call to
    // childHasIntrinsicMainAxisSize) so we can be sure that the two height
    // calls here will return up-to-date data.
    LayoutUnit logical_height = child.ComputeContentLogicalHeight(
        size_type, size, child.IntrinsicContentLogicalHeight());
    if (logical_height == -1)
      return logical_height;
    return logical_height + child.ComputeLogicalScrollbars().BlockSum();
  }
  // computeLogicalWidth always re-computes the intrinsic widths. However, when
  // our logical width is auto, we can just use our cached value. So let's do
  // that here. (Compare code in LayoutBlock::computePreferredLogicalWidths)
  if (child.StyleRef().LogicalWidth().IsAuto() && !HasAspectRatio(child)) {
    if (size.IsMinContent() || size.IsMinIntrinsic())
      return child.PreferredLogicalWidths().min_size - border_and_padding;
    if (size.IsMaxContent())
      return child.PreferredLogicalWidths().max_size - border_and_padding;
  }
  return child.ComputeLogicalWidthUsing(size_type, size, ContentLogicalWidth(),
                                        this) -
         border_and_padding;
}

LayoutUnit LayoutFlexibleBox::ContentInsetRight() const {
  NOT_DESTROYED();
  return BorderRight() + PaddingRight() + ComputeScrollbars().right;
}

LayoutUnit LayoutFlexibleBox::ContentInsetBottom() const {
  NOT_DESTROYED();
  return BorderBottom() + PaddingBottom() + ComputeScrollbars().bottom;
}

LayoutUnit LayoutFlexibleBox::FlowAwareContentInsetStart() const {
  NOT_DESTROYED();
  if (IsHorizontalFlow())
    return IsLeftToRightFlow() ? ContentLeft() : ContentInsetRight();
  return IsLeftToRightFlow() ? ContentTop() : ContentInsetBottom();
}

LayoutUnit LayoutFlexibleBox::FlowAwareContentInsetEnd() const {
  NOT_DESTROYED();
  if (IsHorizontalFlow())
    return IsLeftToRightFlow() ? ContentInsetRight() : ContentLeft();
  return IsLeftToRightFlow() ? ContentInsetBottom() : ContentTop();
}

LayoutUnit LayoutFlexibleBox::FlowAwareContentInsetBefore() const {
  NOT_DESTROYED();
  switch (FlexLayoutAlgorithm::GetTransformedWritingMode(StyleRef())) {
    case TransformedWritingMode::kTopToBottomWritingMode:
      return ContentTop();
    case TransformedWritingMode::kBottomToTopWritingMode:
      return ContentInsetBottom();
    case TransformedWritingMode::kLeftToRightWritingMode:
      return ContentLeft();
    case TransformedWritingMode::kRightToLeftWritingMode:
      return ContentInsetRight();
  }
  NOTREACHED();
}

DISABLE_CFI_PERF
LayoutUnit LayoutFlexibleBox::FlowAwareContentInsetAfter() const {
  NOT_DESTROYED();
  switch (FlexLayoutAlgorithm::GetTransformedWritingMode(StyleRef())) {
    case TransformedWritingMode::kTopToBottomWritingMode:
      return ContentInsetBottom();
    case TransformedWritingMode::kBottomToTopWritingMode:
      return ContentTop();
    case TransformedWritingMode::kLeftToRightWritingMode:
      return ContentInsetRight();
    case TransformedWritingMode::kRightToLeftWritingMode:
      return ContentLeft();
  }
  NOTREACHED();
}

LayoutUnit LayoutFlexibleBox::CrossAxisScrollbarExtent() const {
  NOT_DESTROYED();
  return IsHorizontalFlow() ? ComputeScrollbars().HorizontalSum()
                            : ComputeScrollbars().VerticalSum();
}

LayoutUnit LayoutFlexibleBox::CrossAxisScrollbarExtentForChild(
    const LayoutBox& child) const {
  NOT_DESTROYED();
  return IsHorizontalFlow() ? child.ComputeScrollbars().HorizontalSum()
                            : child.ComputeScrollbars().VerticalSum();
}

FlexOffset LayoutFlexibleBox::FlowAwareLocationForChild(
    const LayoutBox& child) const {
  NOT_DESTROYED();
  LayoutPoint location = IsHorizontalFlow()
                             ? child.Location()
                             : child.Location().TransposedPoint();
  return FlexOffset(location.X(), location.Y());
}

bool LayoutFlexibleBox::UseChildAspectRatio(const LayoutBox& child) const {
  NOT_DESTROYED();
  if (!HasAspectRatio(child))
    return false;
  if (child.StyleRef().AspectRatio().IsAuto() &&
      child.IntrinsicSize().Height() == 0) {
    // We can't compute a ratio in this case.
    return false;
  }
  const Length& cross_size =
      IsHorizontalFlow() ? child.StyleRef().Height() : child.StyleRef().Width();
  return CrossAxisLengthIsDefinite(child, cross_size);
}

LayoutUnit LayoutFlexibleBox::ComputeMainSizeFromAspectRatioUsing(
    const LayoutBox& child,
    const Length& cross_size_length,
    LayoutUnit main_axis_border_and_padding,
    LayoutUnit cross_axis_border_and_padding) const {
  NOT_DESTROYED();
  DCHECK(HasAspectRatio(child));

  LayoutUnit cross_size;
  if (cross_size_length.IsFixed()) {
    cross_size = LayoutUnit(cross_size_length.Value());
  } else {
    DCHECK(cross_size_length.IsPercentOrCalc());
    cross_size = MainAxisIsInlineAxis(child)
                     ? child.ComputePercentageLogicalHeight(cross_size_length)
                     : AdjustBorderBoxLogicalWidthForBoxSizing(
                           ValueForLength(cross_size_length, ContentWidth()));
  }

  LayoutSize aspect_ratio = child.IntrinsicSize();
  EAspectRatioType ar_type = child.StyleRef().AspectRatio().GetType();
  LayoutUnit border_and_padding;
  if (ar_type == EAspectRatioType::kRatio ||
      (ar_type == EAspectRatioType::kAutoAndRatio && aspect_ratio.IsEmpty())) {
    aspect_ratio = LayoutSize(child.StyleRef().AspectRatio().GetRatio());
    if (child.StyleRef().BoxSizingForAspectRatio() == EBoxSizing::kContentBox) {
      cross_size -= cross_axis_border_and_padding;
      border_and_padding = main_axis_border_and_padding;
    }
  }
  // TODO(cbiesinger): box sizing?
  double ratio =
      aspect_ratio.Width().ToFloat() / aspect_ratio.Height().ToFloat();
  if (IsHorizontalFlow())
    return LayoutUnit(cross_size * ratio) + border_and_padding;
  return LayoutUnit(cross_size / ratio) + border_and_padding;
}

void LayoutFlexibleBox::SetFlowAwareLocationForChild(
    LayoutBox& child,
    const FlexOffset& flex_offset) {
  NOT_DESTROYED();
  if (IsHorizontalFlow()) {
    child.SetLocationAndUpdateOverflowControlsIfNeeded(LayoutPoint(
        flex_offset.main_axis_offset, flex_offset.cross_axis_offset));
  } else {
    child.SetLocationAndUpdateOverflowControlsIfNeeded(LayoutPoint(
        flex_offset.cross_axis_offset, flex_offset.main_axis_offset));
  }
}

bool LayoutFlexibleBox::MainAxisLengthIsDefinite(const LayoutBox& child,
                                                 const Length& flex_basis,
                                                 bool add_to_cb) const {
  NOT_DESTROYED();
  // 'content' isn't actually supported in legacy flex. Checking IsContent() and
  // returning false on the next line prevents a DCHECK though.
  if (flex_basis.IsAuto() || flex_basis.IsContent())
    return false;
  if (IsColumnFlow() && flex_basis.IsContentOrIntrinsicOrFillAvailable())
    return false;
  if (flex_basis.IsPercentOrCalc()) {
    if (!IsColumnFlow() || has_definite_height_ == SizeDefiniteness::kDefinite)
      return true;
    if (has_definite_height_ == SizeDefiniteness::kIndefinite)
      return false;
    if (child.HasOverrideContainingBlockContentLogicalHeight()) {
      // We don't want to cache this. To be a bit more efficient, just check
      // whether the override height is -1 or not and return the value based on
      // that.
      DCHECK(!add_to_cb);
      LayoutUnit override_height =
          child.OverrideContainingBlockContentLogicalHeight();
      return override_height == LayoutUnit(-1) ? false : true;
    }
    LayoutBlock* cb = nullptr;
    bool definite =
        child.ContainingBlockLogicalHeightForPercentageResolution(&cb) != -1;
    if (add_to_cb)
      cb->AddPercentHeightDescendant(const_cast<LayoutBox*>(&child));
    if (in_layout_) {
      // We can reach this code even while we're not laying ourselves out, such
      // as from mainSizeForPercentageResolution.
      has_definite_height_ = definite ? SizeDefiniteness::kDefinite
                                      : SizeDefiniteness::kIndefinite;
    }
    return definite;
  }
  return true;
}

bool LayoutFlexibleBox::CrossAxisLengthIsDefinite(const LayoutBox& child,
                                                  const Length& length) const {
  NOT_DESTROYED();
  if (length.IsAuto())
    return false;
  if (length.IsPercentOrCalc()) {
    if (!MainAxisIsInlineAxis(child) ||
        has_definite_height_ == SizeDefiniteness::kDefinite)
      return true;
    if (has_definite_height_ == SizeDefiniteness::kIndefinite)
      return false;
    bool definite =
        child.ContainingBlockLogicalHeightForPercentageResolution() != -1;
    has_definite_height_ =
        definite ? SizeDefiniteness::kDefinite : SizeDefiniteness::kIndefinite;
    return definite;
  }
  // TODO(cbiesinger): Eventually we should support other types of sizes here.
  // Requires updating computeMainSizeFromAspectRatioUsing.
  return length.IsFixed();
}

void LayoutFlexibleBox::CacheChildMainSize(const LayoutBox& child) {
  NOT_DESTROYED();
  DCHECK(!child.SelfNeedsLayout());
  DCHECK(!child.NeedsLayout() || child.ChildLayoutBlockedByDisplayLock());
  LayoutUnit main_size;
  if (MainAxisIsInlineAxis(child)) {
    main_size = child.PreferredLogicalWidths().max_size;
  } else {
    if (FlexBasisForChild(child).IsPercentOrCalc() &&
        !MainAxisLengthIsDefinite(child, FlexBasisForChild(child))) {
      main_size = child.IntrinsicContentLogicalHeight() +
                  child.BorderAndPaddingLogicalHeight() +
                  child.ComputeLogicalScrollbars().BlockSum();
    } else {
      main_size = child.LogicalHeight();
    }
  }
  intrinsic_size_along_main_axis_.Set(&child, main_size);
  relaid_out_children_.insert(&child);
}

void LayoutFlexibleBox::ClearCachedMainSizeForChild(const LayoutBox& child) {
  NOT_DESTROYED();
  intrinsic_size_along_main_axis_.erase(&child);
}

bool LayoutFlexibleBox::CanAvoidLayoutForNGChild(const LayoutBox& child) const {
  NOT_DESTROYED();
  if (!child.IsLayoutNGObject())
    return false;

  // If the last layout was done with a different override size, or different
  // definite-ness, we need to force-relayout so that percentage sizes are
  // resolved correctly.
  const NGLayoutResult* cached_layout_result =
      child.GetSingleCachedLayoutResult();
  if (!cached_layout_result)
    return false;

  const NGConstraintSpace& old_space =
      cached_layout_result->GetConstraintSpaceForCaching();
  if (old_space.IsFixedInlineSize() != child.HasOverrideLogicalWidth())
    return false;
  if (old_space.IsFixedBlockSize() != child.HasOverrideLogicalHeight())
    return false;
  if (!old_space.IsInitialBlockSizeIndefinite() !=
      UseOverrideLogicalHeightForPerentageResolution(child))
    return false;
  if (child.HasOverrideLogicalWidth() &&
      old_space.AvailableSize().inline_size != child.OverrideLogicalWidth())
    return false;
  if (child.HasOverrideLogicalHeight() &&
      old_space.AvailableSize().block_size != child.OverrideLogicalHeight())
    return false;
  return true;
}

DISABLE_CFI_PERF
LayoutUnit LayoutFlexibleBox::ComputeInnerFlexBaseSizeForChild(
    LayoutBox& child,
    LayoutUnit main_axis_border_and_padding,
    LayoutUnit cross_axis_border_and_padding,
    ChildLayoutType child_layout_type) {
  NOT_DESTROYED();
  if (child.IsImage() || IsA<LayoutVideo>(child) || child.IsCanvas())
    UseCounter::Count(GetDocument(), WebFeature::kAspectRatioFlexItem);

  Length flex_basis = FlexBasisForChild(child);
  // -webkit-box sizes as fit-content instead of max-content.
  if (flex_basis.IsAuto() &&
      (StyleRef().IsDeprecatedWebkitBox() &&
       (StyleRef().BoxOrient() == EBoxOrient::kHorizontal ||
        StyleRef().BoxAlign() != EBoxAlignment::kStretch))) {
    flex_basis = Length(Length::kFitContent);
  }
  if (MainAxisLengthIsDefinite(child, flex_basis)) {
    return std::max(LayoutUnit(), ComputeMainAxisExtentForChild(
                                      child, kMainOrPreferredSize, flex_basis,
                                      main_axis_border_and_padding));
  }

  if (UseChildAspectRatio(child)) {
    const Length& cross_size_length = IsHorizontalFlow()
                                          ? child.StyleRef().Height()
                                          : child.StyleRef().Width();
    LayoutUnit result = ComputeMainSizeFromAspectRatioUsing(
        child, cross_size_length, main_axis_border_and_padding,
        cross_axis_border_and_padding);
    result = AdjustChildSizeForAspectRatioCrossAxisMinAndMax(
        child, result, main_axis_border_and_padding,
        cross_axis_border_and_padding);
    return result - main_axis_border_and_padding;
  }

  // The flex basis is indefinite (=auto), so we need to compute the actual
  // width of the child. For the logical width axis we just use the preferred
  // width; for the height we need to lay out the child.
  LayoutUnit main_axis_extent;
  if (MainAxisIsInlineAxis(child)) {
    // We don't need to add ComputeLogicalScrollbars().InlineSum() here because
    // the preferred width includes the scrollbar, even for overflow: auto.
    main_axis_extent = child.PreferredLogicalWidths().max_size;
  } else {
    // The needed value here is the logical height. This value does not include
    // the border/scrollbar/padding size, so we have to add the scrollbar.
    if (child.HasOverrideIntrinsicContentLogicalHeight()) {
      return child.OverrideIntrinsicContentLogicalHeight() +
             LayoutUnit(child.ComputeLogicalScrollbars().BlockSum());
    }
    if (child.ShouldApplySizeContainment())
      return LayoutUnit(child.ComputeLogicalScrollbars().BlockSum());

    if (child_layout_type == kNeverLayout)
      return LayoutUnit();

    DCHECK(!child.NeedsLayout());
    DCHECK(intrinsic_size_along_main_axis_.Contains(&child));
    main_axis_extent = intrinsic_size_along_main_axis_.at(&child);
  }
  DCHECK_GE(main_axis_extent - main_axis_border_and_padding, LayoutUnit())
      << main_axis_extent << " - " << main_axis_border_and_padding;
  return main_axis_extent - main_axis_border_and_padding;
}

void LayoutFlexibleBox::LayoutFlexItems(bool relayout_children,
                                        SubtreeLayoutScope& layout_scope) {
  NOT_DESTROYED();
  PaintLayerScrollableArea::PreventRelayoutScope prevent_relayout_scope(
      layout_scope);

  // Set up our list of flex items. All of the rest of the algorithm should
  // work off this list of a subset.
  ChildLayoutType layout_type =
      relayout_children ? kForceLayout : kLayoutIfNeeded;
  const LayoutUnit line_break_length = MainAxisContentExtent(LayoutUnit::Max());
  FlexLayoutAlgorithm flex_algorithm(
      Style(), line_break_length,
      LogicalSize{ContentLogicalWidth(),
                  AvailableLogicalHeightForPercentageComputation()},
      &GetDocument());
  order_iterator_.First();
  for (LayoutBox* child = order_iterator_.CurrentChild(); child;
       child = order_iterator_.Next()) {
    if (child->IsOutOfFlowPositioned()) {
      // Out-of-flow children are not flex items, so we skip them here.
      PrepareChildForPositionedLayout(*child);
      continue;
    }

    ConstructAndAppendFlexItem(&flex_algorithm, *child, layout_type);
  }
  // Because we set the override containing block logical height to -1 in
  // ConstructAndAppendFlexItem, any value we may have cached for definiteness
  // is incorrect; just reset it here.
  has_definite_height_ = SizeDefiniteness::kUnknown;

  LayoutUnit cross_axis_offset = FlowAwareContentInsetBefore();
  LayoutUnit logical_width = LogicalWidth();
  FlexLine* current_line;
  Vector<FlexOffset> item_offsets(flex_algorithm.NumItems());
  FlexOffset* current_item_offset = item_offsets.begin();
  while ((current_line = flex_algorithm.ComputeNextFlexLine(logical_width))) {
    DCHECK_GE(current_line->line_items_.size(), 0ULL);
    current_line->SetContainerMainInnerSize(
        MainAxisContentExtent(current_line->sum_hypothetical_main_size_));
    current_line->FreezeInflexibleItems();

    while (!current_line->ResolveFlexibleLengths()) {
      DCHECK_GE(current_line->total_flex_grow_, 0);
      DCHECK_GE(current_line->total_weighted_flex_shrink_, 0);
    }

    LayoutLineItems(current_line, relayout_children, layout_scope,
                    &current_item_offset);

    current_line->ComputeLineItemsPosition(FlowAwareContentInsetStart(),
                                           FlowAwareContentInsetEnd(),
                                           cross_axis_offset);
    ApplyLineItemsPosition(current_line);
    if (number_of_in_flow_children_on_first_line_ == -1) {
      number_of_in_flow_children_on_first_line_ =
          current_line->line_items_.size();
    }
  }
  if (HasLineIfEmpty()) {
    // Even if ComputeNextFlexLine returns true, the flexbox might not have
    // a line because all our children might be out of flow positioned.
    // Instead of just checking if we have a line, make sure the flexbox
    // has at least a line's worth of height to cover this case.
    LayoutUnit min_height = MinimumLogicalHeightForEmptyLine();
    if (Size().Height() < min_height)
      SetLogicalHeight(min_height);
  }
  if (!IsColumnFlow()) {
    SetLogicalHeight(LogicalHeight() +
                     flex_algorithm.gap_between_lines_ *
                         (flex_algorithm.FlexLines().size() - 1));
  }
  UpdateLogicalHeight();
  if (!HasOverrideLogicalHeight() && IsColumnFlow()) {
    SetIntrinsicContentLogicalHeight(
        flex_algorithm.IntrinsicContentBlockSize());
  }
  RepositionLogicalHeightDependentFlexItems(flex_algorithm);
}

bool LayoutFlexibleBox::HasAutoMarginsInCrossAxis(
    const LayoutBox& child) const {
  NOT_DESTROYED();
  if (IsHorizontalFlow()) {
    return child.StyleRef().MarginTop().IsAuto() ||
           child.StyleRef().MarginBottom().IsAuto();
  }
  return child.StyleRef().MarginLeft().IsAuto() ||
         child.StyleRef().MarginRight().IsAuto();
}

LayoutUnit LayoutFlexibleBox::ComputeChildMarginValue(const Length& margin) {
  NOT_DESTROYED();
  // When resolving the margins, we use the content size for resolving percent
  // and calc (for percents in calc expressions) margins. Fortunately, percent
  // margins are always computed with respect to the block's width, even for
  // margin-top and margin-bottom.
  LayoutUnit available_size = ContentLogicalWidth();
  return MinimumValueForLength(margin, available_size);
}

void LayoutFlexibleBox::PrepareOrderIteratorAndMargins() {
  NOT_DESTROYED();
  OrderIteratorPopulator populator(order_iterator_);

  for (LayoutBox* child = FirstChildBox(); child;
       child = child->NextSiblingBox()) {
    populator.CollectChild(child);

    if (child->IsOutOfFlowPositioned())
      continue;

    // Before running the flex algorithm, 'auto' has a margin of 0.
    const ComputedStyle& style = child->StyleRef();
    child->SetMarginTop(ComputeChildMarginValue(style.MarginTop()));
    child->SetMarginRight(ComputeChildMarginValue(style.MarginRight()));
    child->SetMarginBottom(ComputeChildMarginValue(style.MarginBottom()));
    child->SetMarginLeft(ComputeChildMarginValue(style.MarginLeft()));
  }
}

DISABLE_CFI_PERF
MinMaxSizes LayoutFlexibleBox::ComputeMinAndMaxSizesForChild(
    const FlexLayoutAlgorithm& algorithm,
    const LayoutBox& child,
    LayoutUnit border_and_padding,
    LayoutUnit cross_axis_border_and_padding) const {
  NOT_DESTROYED();
  MinMaxSizes sizes{LayoutUnit(), LayoutUnit::Max()};

  const Length& max = IsHorizontalFlow() ? child.StyleRef().MaxWidth()
                                         : child.StyleRef().MaxHeight();
  if (!max.IsNone()) {
    sizes.max_size =
        ComputeMainAxisExtentForChild(child, kMaxSize, max, border_and_padding);
    if (sizes.max_size == -1)
      sizes.max_size = LayoutUnit::Max();
    DCHECK_GE(sizes.max_size, LayoutUnit());
  }

  const Length& min = IsHorizontalFlow() ? child.StyleRef().MinWidth()
                                         : child.StyleRef().MinHeight();
  if (!min.IsAuto()) {
    sizes.min_size =
        ComputeMainAxisExtentForChild(child, kMinSize, min, border_and_padding);
    // computeMainAxisExtentForChild can return -1 when the child has a
    // percentage min size, but we have an indefinite size in that axis.
    sizes.min_size = std::max(LayoutUnit(), sizes.min_size);
  } else if (algorithm.ShouldApplyMinSizeAutoForChild(child)) {
    LayoutUnit content_size = ComputeMainAxisExtentForChild(
        child, kMinSize, Length::MinContent(), border_and_padding);
    DCHECK_GE(content_size, LayoutUnit());
    if (HasAspectRatio(child) && child.IntrinsicSize().Height() > 0) {
      content_size = AdjustChildSizeForAspectRatioCrossAxisMinAndMax(
          child, content_size, border_and_padding,
          cross_axis_border_and_padding);
    }
    if (child.IsTable() && !IsColumnFlow()) {
      // Avoid resolving minimum size to something narrower than the minimum
      // preferred logical width of the table.
      sizes.min_size = content_size;
    } else {
      if (sizes.max_size != -1 && content_size > sizes.max_size)
        content_size = sizes.max_size;

      const Length& main_size = IsHorizontalFlow() ? child.StyleRef().Width()
                                                   : child.StyleRef().Height();
      if (MainAxisLengthIsDefinite(child, main_size)) {
        LayoutUnit resolved_main_size = ComputeMainAxisExtentForChild(
            child, kMainOrPreferredSize, main_size, border_and_padding);
        DCHECK_GE(resolved_main_size, LayoutUnit());
        LayoutUnit specified_size =
            sizes.max_size != -1 ? std::min(resolved_main_size, sizes.max_size)
                                 : resolved_main_size;

        sizes.min_size = std::min(specified_size, content_size);
      } else if (child.IsLayoutReplaced() && UseChildAspectRatio(child)) {
        const Length& cross_size_length = IsHorizontalFlow()
                                              ? child.StyleRef().Height()
                                              : child.StyleRef().Width();
        LayoutUnit transferred_size = ComputeMainSizeFromAspectRatioUsing(
            child, cross_size_length, border_and_padding,
            cross_axis_border_and_padding);
        transferred_size = AdjustChildSizeForAspectRatioCrossAxisMinAndMax(
            child, transferred_size, border_and_padding,
            cross_axis_border_and_padding);
        sizes.min_size = std::min(transferred_size, content_size);
      } else {
        sizes.min_size = content_size;
      }
    }
  }
  DCHECK_GE(sizes.min_size, LayoutUnit());
  return sizes;
}

bool LayoutFlexibleBox::CrossSizeIsDefiniteForPercentageResolution(
    const LayoutBox& child) const {
  NOT_DESTROYED();
  DCHECK(MainAxisIsInlineAxis(child));
  if (FlexLayoutAlgorithm::AlignmentForChild(StyleRef(), child.StyleRef()) !=
      ItemPosition::kStretch)
    return false;

  // Here we implement https://drafts.csswg.org/css-flexbox/#algo-stretch
  if (child.HasOverrideLogicalHeight())
    return true;

  // We don't currently implement the optimization from
  // https://drafts.csswg.org/css-flexbox/#definite-sizes case 1. While that
  // could speed up a specialized case, it requires determining if we have a
  // definite size, which itself is not cheap. We can consider implementing it
  // at a later time. (The correctness is ensured by redoing layout in
  // applyStretchAlignmentToChild)
  return false;
}

bool LayoutFlexibleBox::MainSizeIsDefiniteForPercentageResolution(
    const LayoutBox& child) const {
  NOT_DESTROYED();
  DCHECK(!MainAxisIsInlineAxis(child));
  // This function implements section 9.8. Definite and Indefinite Sizes, case
  // 2) of the flexbox spec.
  // We need to check for the flexbox to have a definite main size.
  // We make up a percentage to check whether we have a definite size.
  if (!MainAxisLengthIsDefinite(child, Length::Percent(0), false))
    return false;

  return child.HasOverrideLogicalHeight();
}

bool LayoutFlexibleBox::UseOverrideLogicalHeightForPerentageResolution(
    const LayoutBox& child) const {
  NOT_DESTROYED();
  if (MainAxisIsInlineAxis(child))
    return CrossSizeIsDefiniteForPercentageResolution(child);
  return MainSizeIsDefiniteForPercentageResolution(child);
}

LayoutUnit LayoutFlexibleBox::AdjustChildSizeForAspectRatioCrossAxisMinAndMax(
    const LayoutBox& child,
    LayoutUnit child_size,
    LayoutUnit main_axis_border_and_padding,
    LayoutUnit cross_axis_border_and_padding) const {
  NOT_DESTROYED();
  const Length& cross_min = IsHorizontalFlow() ? child.StyleRef().MinHeight()
                                               : child.StyleRef().MinWidth();
  const Length& cross_max = IsHorizontalFlow() ? child.StyleRef().MaxHeight()
                                               : child.StyleRef().MaxWidth();

  if (CrossAxisLengthIsDefinite(child, cross_max)) {
    LayoutUnit max_value = ComputeMainSizeFromAspectRatioUsing(
        child, cross_max, main_axis_border_and_padding,
        cross_axis_border_and_padding);
    child_size = std::min(max_value, child_size);
  }

  if (CrossAxisLengthIsDefinite(child, cross_min)) {
    LayoutUnit min_value = ComputeMainSizeFromAspectRatioUsing(
        child, cross_min, main_axis_border_and_padding,
        cross_axis_border_and_padding);
    child_size = std::max(min_value, child_size);
  }

  return child_size;
}

DISABLE_CFI_PERF
void LayoutFlexibleBox::ConstructAndAppendFlexItem(
    FlexLayoutAlgorithm* algorithm,
    LayoutBox& child,
    ChildLayoutType layout_type) {
  NOT_DESTROYED();
  if (layout_type != kNeverLayout &&
      ChildHasIntrinsicMainAxisSize(*algorithm, child)) {
    // If this condition is true, then ComputeMainAxisExtentForChild will call
    // child.IntrinsicContentLogicalHeight() and
    // child.ScrollbarLogicalHeight(), so if the child has intrinsic
    // min/max/preferred size, run layout on it now to make sure its logical
    // height and scroll bars are up to date.
    // For column flow flex containers, we even need to do this for children
    // that don't need layout, if there's a chance that the logical width of
    // the flex container has changed (because that may affect the intrinsic
    // height of the child).
    UpdateBlockChildDirtyBitsBeforeLayout(layout_type == kForceLayout, child);
    if (child.NeedsLayout() || layout_type == kForceLayout ||
        !intrinsic_size_along_main_axis_.Contains(&child)) {
      // Don't resolve percentages in children. This is especially important
      // for the min-height calculation, where we want percentages to be
      // treated as auto. For flex-basis itself, this is not a problem because
      // by definition we have an indefinite flex basis here and thus
      // percentages should not resolve.
      if (IsHorizontalWritingMode() == child.IsHorizontalWritingMode())
        child.SetOverrideContainingBlockContentLogicalHeight(LayoutUnit(-1));
      else
        child.SetOverrideContainingBlockContentLogicalWidth(LayoutUnit(-1));
      child.ClearOverrideSize();
      child.ForceLayout();
      CacheChildMainSize(child);
      child.ClearOverrideContainingBlockContentSize();
    }
  }

  LayoutUnit main_axis_border_padding = IsHorizontalFlow()
                                            ? child.BorderAndPaddingWidth()
                                            : child.BorderAndPaddingHeight();
  LayoutUnit cross_axis_border_padding = IsHorizontalFlow()
                                             ? child.BorderAndPaddingHeight()
                                             : child.BorderAndPaddingWidth();

  LayoutUnit child_inner_flex_base_size = ComputeInnerFlexBaseSizeForChild(
      child, main_axis_border_padding, cross_axis_border_padding, layout_type);

  MinMaxSizes sizes = ComputeMinAndMaxSizesForChild(
      *algorithm, child, main_axis_border_padding, cross_axis_border_padding);

  NGPhysicalBoxStrut physical_margins(child.MarginTop(), child.MarginRight(),
                                      child.MarginBottom(), child.MarginLeft());
  algorithm->emplace_back(
      &child, child.StyleRef(), child_inner_flex_base_size, sizes,
      /* min_max_cross_sizes */ absl::nullopt, main_axis_border_padding,
      cross_axis_border_padding, physical_margins, /* unused */ NGBoxStrut(),
      StyleRef().GetWritingMode());
}

void LayoutFlexibleBox::SetOverrideMainAxisContentSizeForChild(FlexItem& item) {
  NOT_DESTROYED();
  if (MainAxisIsInlineAxis(*item.box_)) {
    item.box_->SetOverrideLogicalWidth(item.FlexedBorderBoxSize());
  } else {
    item.box_->SetOverrideLogicalHeight(item.FlexedBorderBoxSize());
  }
}

namespace {

LayoutUnit MainAxisStaticPositionCommon(const LayoutBox& child,
                                        LayoutBox* parent,
                                        LayoutUnit available_space) {
  LayoutUnit offset = FlexLayoutAlgorithm::InitialContentPositionOffset(
      parent->StyleRef(), available_space,
      FlexLayoutAlgorithm::ResolvedJustifyContent(parent->StyleRef()), 1);
  if (parent->StyleRef().ResolvedIsRowReverseFlexDirection() ||
      parent->StyleRef().ResolvedIsColumnReverseFlexDirection())
    offset = available_space - offset;
  return offset;
}

LayoutUnit StaticMainAxisPositionForNGPositionedChild(const LayoutBox& child,
                                                      LayoutBox* parent) {
  const LayoutUnit available_space =
      FlexLayoutAlgorithm::IsHorizontalFlow(parent->StyleRef())
          ? parent->ContentWidth() - child.Size().Width()
          : parent->ContentHeight() - child.Size().Height();
  return MainAxisStaticPositionCommon(child, parent, available_space);
}

LayoutUnit CrossAxisStaticPositionCommon(const LayoutBox& child,
                                         LayoutBox* parent,
                                         LayoutUnit available_space) {
  return FlexItem::AlignmentOffset(
      available_space,
      FlexLayoutAlgorithm::AlignmentForChild(parent->StyleRef(),
                                             child.StyleRef()),
      LayoutUnit(), parent->StyleRef().FlexWrap() == EFlexWrap::kWrapReverse,
      parent->StyleRef().IsDeprecatedWebkitBox());
}

LayoutUnit StaticCrossAxisPositionForNGPositionedChild(const LayoutBox& child,
                                                       LayoutBox* parent) {
  const LayoutUnit available_space =
      FlexLayoutAlgorithm::IsHorizontalFlow(parent->StyleRef())
          ? parent->ContentHeight() - child.Size().Height()
          : parent->ContentWidth() - child.Size().Width();
  return CrossAxisStaticPositionCommon(child, parent, available_space);
}

LayoutUnit StaticInlinePositionForNGPositionedChild(const LayoutBox& child,
                                                    LayoutBlock* parent) {
  const LayoutUnit start_offset = parent->StartOffsetForContent();
  if (parent->StyleRef().IsDeprecatedWebkitBox())
    return start_offset;
  return start_offset +
         (parent->StyleRef().ResolvedIsColumnFlexDirection()
              ? StaticCrossAxisPositionForNGPositionedChild(child, parent)
              : StaticMainAxisPositionForNGPositionedChild(child, parent));
}

LayoutUnit StaticBlockPositionForNGPositionedChild(const LayoutBox& child,
                                                   LayoutBlock* parent) {
  return parent->BorderAndPaddingBefore() +
         (parent->StyleRef().ResolvedIsColumnFlexDirection()
              ? StaticMainAxisPositionForNGPositionedChild(child, parent)
              : StaticCrossAxisPositionForNGPositionedChild(child, parent));
}

}  // namespace

bool LayoutFlexibleBox::SetStaticPositionForChildInFlexNGContainer(
    LayoutBox& child,
    LayoutBlock* parent) {
  const ComputedStyle& style = parent->StyleRef();
  bool position_changed = false;
  PaintLayer* child_layer = child.Layer();
  if (child.StyleRef().HasStaticInlinePosition(
          style.IsHorizontalWritingMode())) {
    LayoutUnit inline_position =
        StaticInlinePositionForNGPositionedChild(child, parent);
    if (child_layer->StaticInlinePosition() != inline_position) {
      child_layer->SetStaticInlinePosition(inline_position);
      position_changed = true;
    }
  }
  if (child.StyleRef().HasStaticBlockPosition(
          style.IsHorizontalWritingMode())) {
    LayoutUnit block_position =
        StaticBlockPositionForNGPositionedChild(child, parent);
    if (child_layer->StaticBlockPosition() != block_position) {
      child_layer->SetStaticBlockPosition(block_position);
      position_changed = true;
    }
  }
  return position_changed;
}

LayoutUnit LayoutFlexibleBox::StaticMainAxisPositionForPositionedChild(
    const LayoutBox& child) {
  NOT_DESTROYED();
  const LayoutUnit available_space =
      MainAxisContentExtent(ContentLogicalHeight()) -
      MainAxisExtentForChild(child);

  return MainAxisStaticPositionCommon(child, this, available_space);
}

LayoutUnit LayoutFlexibleBox::StaticCrossAxisPositionForPositionedChild(
    const LayoutBox& child) {
  NOT_DESTROYED();
  LayoutUnit available_space =
      CrossAxisContentExtent() - CrossAxisExtentForChild(child);
  return CrossAxisStaticPositionCommon(child, this, available_space);
}

LayoutUnit LayoutFlexibleBox::StaticInlinePositionForPositionedChild(
    const LayoutBox& child) {
  NOT_DESTROYED();
  const LayoutUnit start_offset = StartOffsetForContent();
  if (StyleRef().IsDeprecatedWebkitBox())
    return start_offset;
  return start_offset + (IsColumnFlow()
                             ? StaticCrossAxisPositionForPositionedChild(child)
                             : StaticMainAxisPositionForPositionedChild(child));
}

LayoutUnit LayoutFlexibleBox::StaticBlockPositionForPositionedChild(
    const LayoutBox& child) {
  NOT_DESTROYED();
  return BorderAndPaddingBefore() +
         (IsColumnFlow() ? StaticMainAxisPositionForPositionedChild(child)
                         : StaticCrossAxisPositionForPositionedChild(child));
}

bool LayoutFlexibleBox::SetStaticPositionForPositionedLayout(LayoutBox& child) {
  NOT_DESTROYED();
  bool position_changed = false;
  PaintLayer* child_layer = child.Layer();
  if (child.StyleRef().HasStaticInlinePosition(
          StyleRef().IsHorizontalWritingMode())) {
    LayoutUnit inline_position = StaticInlinePositionForPositionedChild(child);
    if (child_layer->StaticInlinePosition() != inline_position) {
      child_layer->SetStaticInlinePosition(inline_position);
      position_changed = true;
    }
  }
  if (child.StyleRef().HasStaticBlockPosition(
          StyleRef().IsHorizontalWritingMode())) {
    LayoutUnit block_position = StaticBlockPositionForPositionedChild(child);
    if (child_layer->StaticBlockPosition() != block_position) {
      child_layer->SetStaticBlockPosition(block_position);
      position_changed = true;
    }
  }
  return position_changed;
}

void LayoutFlexibleBox::PrepareChildForPositionedLayout(LayoutBox& child) {
  NOT_DESTROYED();
  DCHECK(child.IsOutOfFlowPositioned());
  child.ContainingBlock()->InsertPositionedObject(&child);
  PaintLayer* child_layer = child.Layer();
  LayoutUnit static_inline_position = FlowAwareContentInsetStart();
  if (child_layer->StaticInlinePosition() != static_inline_position) {
    child_layer->SetStaticInlinePosition(static_inline_position);
    if (child.StyleRef().HasStaticInlinePosition(
            StyleRef().IsHorizontalWritingMode()))
      child.SetChildNeedsLayout(kMarkOnlyThis);
  }

  LayoutUnit static_block_position = FlowAwareContentInsetBefore();
  if (child_layer->StaticBlockPosition() != static_block_position) {
    child_layer->SetStaticBlockPosition(static_block_position);
    if (child.StyleRef().HasStaticBlockPosition(
            StyleRef().IsHorizontalWritingMode()))
      child.SetChildNeedsLayout(kMarkOnlyThis);
  }
}

void LayoutFlexibleBox::ResetAutoMarginsAndLogicalTopInCrossAxis(
    LayoutBox& child) {
  NOT_DESTROYED();
  if (HasAutoMarginsInCrossAxis(child)) {
    child.UpdateLogicalHeight();
    if (IsHorizontalFlow()) {
      if (child.StyleRef().MarginTop().IsAuto())
        child.SetMarginTop(LayoutUnit());
      if (child.StyleRef().MarginBottom().IsAuto())
        child.SetMarginBottom(LayoutUnit());
    } else {
      if (child.StyleRef().MarginLeft().IsAuto())
        child.SetMarginLeft(LayoutUnit());
      if (child.StyleRef().MarginRight().IsAuto())
        child.SetMarginRight(LayoutUnit());
    }
  }
}

bool LayoutFlexibleBox::NeedToStretchChildLogicalHeight(
    const LayoutBox& child) const {
  NOT_DESTROYED();
  // This function is a little bit magical. It relies on the fact that blocks
  // intrinsically "stretch" themselves in their inline axis, i.e. a <div> has
  // an implicit width: 100%. So the child will automatically stretch if our
  // cross axis is the child's inline axis. That's the case if:
  // - We are horizontal and the child is in vertical writing mode
  // - We are vertical and the child is in horizontal writing mode
  // Otherwise, we need to stretch if the cross axis size is auto.
  if (FlexLayoutAlgorithm::AlignmentForChild(StyleRef(), child.StyleRef()) !=
      ItemPosition::kStretch)
    return false;

  if (IsHorizontalFlow() != child.StyleRef().IsHorizontalWritingMode())
    return false;

  return child.StyleRef().LogicalHeight().IsAuto();
}

bool LayoutFlexibleBox::ChildHasIntrinsicMainAxisSize(
    const FlexLayoutAlgorithm& algorithm,
    const LayoutBox& child) const {
  NOT_DESTROYED();
  bool result = false;
  if (!MainAxisIsInlineAxis(child) && !child.ShouldApplySizeContainment()) {
    Length child_flex_basis = FlexBasisForChild(child);
    const Length& child_min_size = IsHorizontalFlow()
                                       ? child.StyleRef().MinWidth()
                                       : child.StyleRef().MinHeight();
    const Length& child_max_size = IsHorizontalFlow()
                                       ? child.StyleRef().MaxWidth()
                                       : child.StyleRef().MaxHeight();
    if (!MainAxisLengthIsDefinite(child, child_flex_basis) ||
        child_min_size.IsContentOrIntrinsic() ||
        child_max_size.IsContentOrIntrinsic()) {
      result = true;
    } else if (algorithm.ShouldApplyMinSizeAutoForChild(child)) {
      result = true;
    }
  }
  return result;
}

EOverflow LayoutFlexibleBox::CrossAxisOverflowForChild(
    const LayoutBox& child) const {
  NOT_DESTROYED();
  if (IsHorizontalFlow())
    return child.StyleRef().OverflowY();
  return child.StyleRef().OverflowX();
}

DISABLE_CFI_PERF
void LayoutFlexibleBox::LayoutLineItems(FlexLine* current_line,
                                        bool relayout_children,
                                        SubtreeLayoutScope& layout_scope,
                                        FlexOffset** current_item_offset) {
  NOT_DESTROYED();
  for (wtf_size_t i = 0; i < current_line->line_items_.size(); ++i) {
    FlexItem& flex_item = current_line->line_items_[i];
    LayoutBox* child = flex_item.box_;

    DCHECK(current_item_offset);
    flex_item.offset_ = *current_item_offset;
    (*current_item_offset)++;

    DCHECK(!flex_item.box_->IsOutOfFlowPositioned());

    child->SetShouldCheckForPaintInvalidation();

    SetOverrideMainAxisContentSizeForChild(flex_item);
    // The flexed content size and the override size include the scrollbar
    // width, so we need to compare to the size including the scrollbar.
    if (flex_item.flexed_content_size_ !=
        MainAxisContentExtentForChildIncludingScrollbar(*child)) {
      child->SetSelfNeedsLayoutForAvailableSpace(true);
    } else {
      // To avoid double applying margin changes in
      // updateAutoMarginsInCrossAxis, we reset the margins here.
      ResetAutoMarginsAndLogicalTopInCrossAxis(*child);
    }
    // We may have already forced relayout for orthogonal flowing children in
    // computeInnerFlexBaseSizeForChild.
    bool force_child_relayout =
        relayout_children && !relaid_out_children_.Contains(child);
    // TODO(dgrogan): Broaden the NG part of this check once NG types other
    // than Mixin derivatives are cached.
    auto* child_layout_block = DynamicTo<LayoutBlock>(child);
    if (child_layout_block &&
        child_layout_block->HasPercentHeightDescendants() &&
        !CanAvoidLayoutForNGChild(*child)) {
      // Have to force another relayout even though the child is sized
      // correctly, because its descendants are not sized correctly yet. Our
      // previous layout of the child was done without an override height set.
      // So, redo it here.
      force_child_relayout = true;
    }
    UpdateBlockChildDirtyBitsBeforeLayout(force_child_relayout, *child);
    if (!child->NeedsLayout())
      MarkChildForPaginationRelayoutIfNeeded(*child, layout_scope);
    if (child->NeedsLayout()) {
      relaid_out_children_.insert(child);
      // It is very important that we only clear the cross axis override size
      // if we are in fact going to lay out the child. Otherwise, the cross
      // axis size and the actual laid out size get out of sync, which will
      // cause problems if we later lay out the child in simplified layout,
      // which does not go through regular flex layout and therefore would
      // not reset the cross axis size.
      if (MainAxisIsInlineAxis(*child))
        child->ClearOverrideLogicalHeight();
      else
        child->ClearOverrideLogicalWidth();
    }
    child->LayoutIfNeeded();

    // This shouldn't be necessary, because we set the override size to be
    // the flexed_content_size and so the result should in fact be that size.
    // But it turns out that tables ignore the override size, and so we have
    // to re-check the size so that we place the flex item correctly.
    flex_item.flexed_content_size_ =
        MainAxisExtentForChild(*child) - flex_item.main_axis_border_padding_;
    flex_item.cross_axis_size_ = CrossAxisUnstretchedExtentForChild(*child);
  }
}

void LayoutFlexibleBox::ApplyLineItemsPosition(FlexLine* current_line) {
  NOT_DESTROYED();
  bool is_paginated = View()->GetLayoutState()->IsPaginated();
  for (wtf_size_t i = 0; i < current_line->line_items_.size(); ++i) {
    const FlexItem& flex_item = current_line->line_items_[i];
    LayoutBox* child = flex_item.box_;
    SetFlowAwareLocationForChild(*child, *flex_item.offset_);
    child->SetMargin(flex_item.physical_margins_);

    if (is_paginated)
      UpdateFragmentationInfoForChild(*child);
  }

  if (IsColumnFlow()) {
    SetLogicalHeight(std::max(LogicalHeight(), current_line->main_axis_extent_ +
                                                   FlowAwareContentInsetEnd()));
  } else {
    SetLogicalHeight(
        std::max(LogicalHeight(), current_line->cross_axis_offset_ +
                                      FlowAwareContentInsetAfter() +
                                      current_line->cross_axis_extent_));
  }

  if (StyleRef().ResolvedIsColumnReverseFlexDirection()) {
    // We have to do an extra pass for column-reverse to reposition the flex
    // items since the start depends on the height of the flexbox, which we
    // only know after we've positioned all the flex items.
    UpdateLogicalHeight();
    LayoutColumnReverse(current_line->line_items_,
                        current_line->cross_axis_offset_,
                        current_line->remaining_free_space_);
  }
}

void LayoutFlexibleBox::LayoutColumnReverse(FlexItemVectorView& children,
                                            LayoutUnit cross_axis_offset,
                                            LayoutUnit available_free_space) {
  NOT_DESTROYED();
  const StyleContentAlignmentData justify_content =
      FlexLayoutAlgorithm::ResolvedJustifyContent(StyleRef());

  // This is similar to the logic in FlexLine::ComputeLineItemsPosition, except
  // we place the children starting from the end of the flexbox.
  LayoutUnit main_axis_offset = LogicalHeight() - FlowAwareContentInsetEnd();
  main_axis_offset -= FlexLayoutAlgorithm::InitialContentPositionOffset(
      StyleRef(), available_free_space, justify_content, children.size());

  for (wtf_size_t i = 0; i < children.size(); ++i) {
    FlexItem& flex_item = children[i];
    LayoutBox* child = flex_item.box_;

    DCHECK(!child->IsOutOfFlowPositioned());

    main_axis_offset -=
        MainAxisExtentForChild(*child) + flex_item.FlowAwareMarginEnd();

    SetFlowAwareLocationForChild(
        *child,
        FlexOffset(main_axis_offset,
                   cross_axis_offset + flex_item.FlowAwareMarginBefore()));

    main_axis_offset -= flex_item.FlowAwareMarginStart();

    main_axis_offset -=
        FlexLayoutAlgorithm::ContentDistributionSpaceBetweenChildren(
            available_free_space, justify_content, children.size());
  }
}

void LayoutFlexibleBox::AlignFlexLines(FlexLayoutAlgorithm& algorithm) {
  NOT_DESTROYED();
  Vector<FlexLine>& line_contexts = algorithm.FlexLines();
  const StyleContentAlignmentData align_content =
      FlexLayoutAlgorithm::ResolvedAlignContent(StyleRef());
  if (align_content.GetPosition() == ContentPosition::kFlexStart &&
      algorithm.gap_between_lines_ == 0) {
    return;
  }

  if (IsMultiline() && !line_contexts.empty()) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kFlexboxSingleLineAlignContent);
  }

  algorithm.AlignFlexLines(CrossAxisContentExtent());
  for (unsigned line_number = 0; line_number < line_contexts.size();
       ++line_number) {
    FlexLine& line_context = line_contexts[line_number];
    for (FlexItem& flex_item : line_context.line_items_) {
      ResetAlignmentForChild(*flex_item.box_,
                             flex_item.offset_->cross_axis_offset);
    }
  }
}

void LayoutFlexibleBox::ResetAlignmentForChild(
    LayoutBox& child,
    LayoutUnit new_cross_axis_position) {
  NOT_DESTROYED();
  SetFlowAwareLocationForChild(
      child, {FlowAwareLocationForChild(child).main_axis_offset,
              new_cross_axis_position});
}

void LayoutFlexibleBox::AlignChildren(FlexLayoutAlgorithm& algorithm) {
  NOT_DESTROYED();
  Vector<FlexLine>& line_contexts = algorithm.FlexLines();

  algorithm.AlignChildren();
  for (unsigned line_number = 0; line_number < line_contexts.size();
       ++line_number) {
    FlexLine& line_context = line_contexts[line_number];
    for (FlexItem& flex_item : line_context.line_items_) {
      if (flex_item.needs_relayout_for_stretch_) {
        DCHECK(flex_item.Alignment() == ItemPosition::kStretch);
        ApplyStretchAlignmentToChild(flex_item);
        flex_item.needs_relayout_for_stretch_ = false;
      }
      ResetAlignmentForChild(*flex_item.box_,
                             flex_item.offset_->cross_axis_offset);
      flex_item.box_->SetMargin(flex_item.physical_margins_);
    }
  }
}

void LayoutFlexibleBox::ApplyStretchAlignmentToChild(FlexItem& flex_item) {
  NOT_DESTROYED();
  LayoutBox& child = *flex_item.box_;
  if (flex_item.MainAxisIsInlineAxis() &&
      child.StyleRef().LogicalHeight().IsAuto()) {
    // FIXME: Can avoid laying out here in some cases. See
    // https://webkit.org/b/87905.
    bool child_needs_relayout =
        flex_item.cross_axis_size_ != child.LogicalHeight();
    child.SetOverrideLogicalHeight(flex_item.cross_axis_size_);

    auto* child_block = DynamicTo<LayoutBlock>(child);
    if (child_block && child_block->HasPercentHeightDescendants() &&
        !CanAvoidLayoutForNGChild(child)) {
      // Have to force another relayout even though the child is sized
      // correctly, because its descendants are not sized correctly yet. Our
      // previous layout of the child was done without an override height set.
      // So, redo it here.
      child_needs_relayout |= relaid_out_children_.Contains(&child);
    }
    if (child_needs_relayout)
      child.ForceLayout();
  } else if (!flex_item.MainAxisIsInlineAxis() &&
             child.StyleRef().LogicalWidth().IsAuto()) {
    if (flex_item.cross_axis_size_ != child.LogicalWidth()) {
      child.SetOverrideLogicalWidth(flex_item.cross_axis_size_);
      child.ForceLayout();
    }
  }
}

void LayoutFlexibleBox::FlipForRightToLeftColumn(
    const Vector<FlexLine>& line_contexts) {
  NOT_DESTROYED();
  if (StyleRef().IsLeftToRightDirection() || !IsColumnFlow())
    return;

  LayoutUnit cross_extent = CrossAxisExtent();
  for (const FlexLine& line_context : line_contexts) {
    for (const FlexItem& flex_item : line_context.line_items_) {
      DCHECK(!flex_item.box_->IsOutOfFlowPositioned());

      FlexOffset offset = FlowAwareLocationForChild(*flex_item.box_);
      // For vertical flows, setFlowAwareLocationForChild will transpose x and
      // y, so using the y axis for a column cross axis extent is correct.
      offset.cross_axis_offset =
          cross_extent - flex_item.cross_axis_size_ - offset.cross_axis_offset;
      SetFlowAwareLocationForChild(*flex_item.box_, offset);
    }
  }
}

}  // namespace blink
