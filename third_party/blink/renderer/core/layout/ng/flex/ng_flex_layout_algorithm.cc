// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_layout_algorithm.h"

#include <memory>
#include "third_party/blink/renderer/core/layout/flexible_box_algorithm.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_button.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_child_iterator.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

NGFlexLayoutAlgorithm::NGFlexLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params),
      is_column_(Style().ResolvedIsColumnFlexDirection()),
      is_horizontal_flow_(FlexLayoutAlgorithm::IsHorizontalFlow(Style())),
      is_cross_size_definite_(IsContainerCrossSizeDefinite()) {
  container_builder_.SetIsNewFormattingContext(
      params.space.IsNewFormattingContext());

  border_box_size_ = container_builder_.InitialBorderBoxSize();
  child_percentage_size_ = CalculateChildPercentageSize(
      ConstraintSpace(), Node(), ChildAvailableSize());

  algorithm_.emplace(&Style(), MainAxisContentExtent(LayoutUnit::Max()),
                     child_percentage_size_, &Node().GetDocument());
}

bool NGFlexLayoutAlgorithm::MainAxisIsInlineAxis(
    const NGBlockNode& child) const {
  return child.Style().IsHorizontalWritingMode() ==
         FlexLayoutAlgorithm::IsHorizontalFlow(Style());
}

LayoutUnit NGFlexLayoutAlgorithm::MainAxisContentExtent(
    LayoutUnit sum_hypothetical_main_size) const {
  if (Style().ResolvedIsColumnFlexDirection()) {
    // Even though we only pass border_padding in the third parameter, the
    // return value includes scrollbar, so subtract scrollbar to get content
    // size.
    // We add |border_scrollbar_padding| to the fourth parameter because
    // |content_size| needs to be the size of the border box. We've overloaded
    // the term "content".
    const LayoutUnit border_scrollbar_padding =
        BorderScrollbarPadding().BlockSum();
    return ComputeBlockSizeForFragment(
               ConstraintSpace(), Style(), BorderPadding(),
               sum_hypothetical_main_size.ClampNegativeToZero() +
                   border_scrollbar_padding,
               border_box_size_.inline_size) -
           border_scrollbar_padding;
  }
  return ChildAvailableSize().inline_size;
}

namespace {

enum AxisEdge { kStart, kCenter, kEnd };

// Maps the resolved justify-content value to a static-position edge.
AxisEdge MainAxisStaticPositionEdge(const ComputedStyle& style,
                                    bool is_column) {
  const StyleContentAlignmentData justify =
      FlexLayoutAlgorithm::ResolvedJustifyContent(style);
  const ContentPosition content_position = justify.GetPosition();
  bool is_reverse_flex = is_column
                             ? style.ResolvedIsColumnReverseFlexDirection()
                             : style.ResolvedIsRowReverseFlexDirection();

  if (content_position == ContentPosition::kFlexEnd)
    return is_reverse_flex ? AxisEdge::kStart : AxisEdge::kEnd;

  if (content_position == ContentPosition::kCenter ||
      justify.Distribution() == ContentDistributionType::kSpaceAround ||
      justify.Distribution() == ContentDistributionType::kSpaceEvenly)
    return AxisEdge::kCenter;

  return is_reverse_flex ? AxisEdge::kEnd : AxisEdge::kStart;
}

// Maps the resolved alignment value to a static-position edge.
AxisEdge CrossAxisStaticPositionEdge(const ComputedStyle& style,
                                     const ComputedStyle& child_style) {
  ItemPosition alignment =
      FlexLayoutAlgorithm::AlignmentForChild(style, child_style);
  // AlignmentForChild already accounted for wrap-reverse for kFlexStart and
  // kFlexEnd, but not kStretch. kStretch is supposed to act like kFlexStart.
  if (style.FlexWrap() == EFlexWrap::kWrapReverse &&
      alignment == ItemPosition::kStretch) {
    return AxisEdge::kEnd;
  }

  if (alignment == ItemPosition::kFlexEnd)
    return AxisEdge::kEnd;

  if (alignment == ItemPosition::kCenter)
    return AxisEdge::kCenter;

  return AxisEdge::kStart;
}

}  // namespace

void NGFlexLayoutAlgorithm::HandleOutOfFlowPositioned(NGBlockNode child) {
  AxisEdge main_axis_edge = MainAxisStaticPositionEdge(Style(), is_column_);
  AxisEdge cross_axis_edge =
      CrossAxisStaticPositionEdge(Style(), child.Style());

  AxisEdge inline_axis_edge = is_column_ ? cross_axis_edge : main_axis_edge;
  AxisEdge block_axis_edge = is_column_ ? main_axis_edge : cross_axis_edge;

  using InlineEdge = NGLogicalStaticPosition::InlineEdge;
  using BlockEdge = NGLogicalStaticPosition::BlockEdge;

  InlineEdge inline_edge;
  BlockEdge block_edge;
  LogicalOffset offset = BorderScrollbarPadding().StartOffset();

  // Determine the static-position based off the axis-edge.
  if (inline_axis_edge == AxisEdge::kStart) {
    inline_edge = InlineEdge::kInlineStart;
  } else if (inline_axis_edge == AxisEdge::kCenter) {
    inline_edge = InlineEdge::kInlineCenter;
    offset.inline_offset += ChildAvailableSize().inline_size / 2;
  } else {
    inline_edge = InlineEdge::kInlineEnd;
    offset.inline_offset += ChildAvailableSize().inline_size;
  }

  // We may not know the final block-size of the fragment yet. This will be
  // adjusted within the |NGContainerFragmentBuilder| once set.
  if (block_axis_edge == AxisEdge::kStart) {
    block_edge = BlockEdge::kBlockStart;
  } else if (block_axis_edge == AxisEdge::kCenter) {
    block_edge = BlockEdge::kBlockCenter;
    offset.block_offset -= BorderScrollbarPadding().BlockSum() / 2;
  } else {
    block_edge = BlockEdge::kBlockEnd;
    offset.block_offset -= BorderScrollbarPadding().BlockSum();
  }

  container_builder_.AddOutOfFlowChildCandidate(child, offset, inline_edge,
                                                block_edge);
}

bool NGFlexLayoutAlgorithm::IsColumnContainerMainSizeDefinite() const {
  DCHECK(is_column_);
  // If this flex container is also a flex item, it might have a definite size
  // imposed on it by its parent flex container.
  // We can't rely on BlockLengthUnresolvable for this case because that
  // considers Auto as unresolvable even when the block size is fixed and
  // definite.
  if (ConstraintSpace().IsFixedBlockSize() &&
      !ConstraintSpace().IsFixedBlockSizeIndefinite())
    return true;
  Length main_size = Style().LogicalHeight();
  return !BlockLengthUnresolvable(ConstraintSpace(), main_size,
                                  LengthResolvePhase::kLayout);
}

bool NGFlexLayoutAlgorithm::IsContainerCrossSizeDefinite() const {
  // A column flexbox's cross axis is an inline size, so is definite.
  if (is_column_)
    return true;
  // If this flex container is also a flex item, it might have a definite size
  // imposed on it by its parent flex container.
  // TODO(dgrogan): Removing this check doesn't cause any tests to fail. Remove
  // it if unneeded or add a test that needs it.
  if (ConstraintSpace().IsFixedBlockSize() &&
      !ConstraintSpace().IsFixedBlockSizeIndefinite())
    return true;

  return !BlockLengthUnresolvable(ConstraintSpace(), Style().LogicalHeight(),
                                  LengthResolvePhase::kLayout);
}

bool NGFlexLayoutAlgorithm::DoesItemStretch(const NGBlockNode& child) const {
  if (!DoesItemCrossSizeComputeToAuto(child))
    return false;
  const ComputedStyle& child_style = child.Style();
  // https://drafts.csswg.org/css-flexbox/#valdef-align-items-stretch
  // If the cross size property of the flex item computes to auto, and neither
  // of the cross-axis margins are auto, the flex item is stretched.
  if (is_horizontal_flow_ &&
      (child_style.MarginTop().IsAuto() || child_style.MarginBottom().IsAuto()))
    return false;
  if (!is_horizontal_flow_ &&
      (child_style.MarginLeft().IsAuto() || child_style.MarginRight().IsAuto()))
    return false;
  return FlexLayoutAlgorithm::AlignmentForChild(Style(), child_style) ==
         ItemPosition::kStretch;
}

bool NGFlexLayoutAlgorithm::IsItemFlexBasisDefinite(
    const NGBlockNode& child) const {
  const Length& flex_basis = child.Style().FlexBasis();
  DCHECK(!flex_basis.IsAuto())
      << "This is never called with flex_basis.IsAuto, but it'd be trivial to "
         "support.";
  if (!is_column_)
    return true;
  return !BlockLengthUnresolvable(BuildSpaceForFlexBasis(child), flex_basis,
                                  LengthResolvePhase::kLayout);
}

// This behavior is under discussion: the item's pre-flexing main size
// definiteness may no longer imply post-flexing definiteness.
// TODO(dgrogan): Have https://crbug.com/1003506 and
// https://github.com/w3c/csswg-drafts/issues/4305 been resolved yet?
bool NGFlexLayoutAlgorithm::IsItemMainSizeDefinite(
    const NGBlockNode& child) const {
  DCHECK(is_column_)
      << "This method doesn't work with row flexboxes because we assume "
         "main size is block size when we call BlockLengthUnresolvable.";
  // Inline sizes are always definite.
  // TODO(dgrogan): The relevant tests, the last two cases in
  // css/css-flexbox/percentage-heights-003.html passed even without this, so it
  // may be untested or unnecessary.
  if (MainAxisIsInlineAxis(child))
    return true;
  // We need a constraint space for the child to determine resolvability and the
  // space for flex-basis is sufficient, even though it has some unnecessary
  // stuff (ShrinkToFit and fixed cross sizes).
  return !BlockLengthUnresolvable(BuildSpaceForFlexBasis(child),
                                  child.Style().LogicalHeight(),
                                  LengthResolvePhase::kLayout);
}

bool NGFlexLayoutAlgorithm::IsItemCrossAxisLengthDefinite(
    const NGBlockNode& child,
    const Length& length) const {
  // We don't consider inline value of 'auto' for the cross-axis min/main/max
  // size to be definite. Block value of 'auto' is always indefinite.
  if (length.IsAuto())
    return false;
  // But anything else in the inline direction is definite.
  if (!MainAxisIsInlineAxis(child))
    return true;
  // If we get here, cross axis is block axis.
  return !BlockLengthUnresolvable(BuildSpaceForFlexBasis(child), length,
                                  LengthResolvePhase::kLayout);
}

bool NGFlexLayoutAlgorithm::DoesItemCrossSizeComputeToAuto(
    const NGBlockNode& child) const {
  const ComputedStyle& child_style = child.Style();
  if (is_horizontal_flow_)
    return child_style.Height().IsAuto();
  return child_style.Width().IsAuto();
}

// This function is used to handle two requirements from the spec.
// (1) Calculating flex base size; case 3E at
// https://drafts.csswg.org/css-flexbox/#algo-main-item : If a cross size is
// needed to determine the main size (e.g. when the flex item’s main size is
// in its block axis) and the flex item’s cross size is auto and not
// definite, in this calculation use fit-content as the flex item’s cross size.
// The flex base size is the item’s resulting main size.
// (2) Cross size determination after main size has been calculated.
// https://drafts.csswg.org/css-flexbox/#algo-cross-item : Determine the
// hypothetical cross size of each item by performing layout with the used main
// size and the available space, treating auto as fit-content.
bool NGFlexLayoutAlgorithm::ShouldItemShrinkToFit(
    const NGBlockNode& child) const {
  if (MainAxisIsInlineAxis(child)) {
    // In this case, the cross size is in the item's block axis. The item's
    // block size is never needed to determine its inline size so don't use
    // fit-content.
    return false;
  }
  if (!child.Style().LogicalWidth().IsAuto()) {
    DCHECK(!DoesItemCrossSizeComputeToAuto(child));
    // The cross size (item's inline size) is already specified, so don't use
    // fit-content.
    return false;
  }
  DCHECK(DoesItemCrossSizeComputeToAuto(child));
  // If execution reaches here, the item's inline size is its cross size and
  // computes to auto. In that situation, we only don't use fit-content if the
  // item qualifies for the first case in
  // https://drafts.csswg.org/css-flexbox/#definite-sizes :
  // 1. If a single-line flex container has a definite cross size, the outer
  // cross size of any stretched flex items is the flex container’s inner cross
  // size (clamped to the flex item’s min and max cross size) and is considered
  // definite.
  if (WillChildCrossSizeBeContainerCrossSize(child))
    return false;
  return true;
}

bool NGFlexLayoutAlgorithm::WillChildCrossSizeBeContainerCrossSize(
    const NGBlockNode& child) const {
  return !algorithm_->IsMultiline() && is_cross_size_definite_ &&
         DoesItemStretch(child);
}

double NGFlexLayoutAlgorithm::GetMainOverCrossAspectRatio(
    const NGBlockNode& child) const {
  DCHECK(child.HasAspectRatio());
  LogicalSize aspect_ratio = child.GetAspectRatio();

  DCHECK_GT(aspect_ratio.inline_size, 0);
  DCHECK_GT(aspect_ratio.block_size, 0);

  double ratio =
      aspect_ratio.inline_size.ToDouble() / aspect_ratio.block_size.ToDouble();
  // Multiplying by ratio will take something in the item's block axis and
  // convert it to the inline axis. We want to convert from cross size to main
  // size. If block axis and cross axis are the same, then we already have what
  // we need. Otherwise we need to use the reciprocal.
  if (!MainAxisIsInlineAxis(child))
    ratio = 1 / ratio;
  return ratio;
}

LayoutUnit NGFlexLayoutAlgorithm::CalculateFixedCrossSize(
    const MinMaxSizes& cross_axis_min_max,
    const NGBoxStrut& margins) const {
  if (!is_column_)
    DCHECK_NE(ChildAvailableSize().block_size, kIndefiniteSize);
  LayoutUnit available_size = is_column_ ? ChildAvailableSize().inline_size
                                         : ChildAvailableSize().block_size;
  LayoutUnit margin_sum = is_column_ ? margins.InlineSum() : margins.BlockSum();
  return cross_axis_min_max.ClampSizeToMinAndMax(available_size - margin_sum);
}

NGConstraintSpace NGFlexLayoutAlgorithm::BuildSpaceForIntrinsicBlockSize(
    const NGBlockNode& flex_item,
    const NGPhysicalBoxStrut& physical_margins = NGPhysicalBoxStrut(),
    const MinMaxSizes& cross_axis_min_max = MinMaxSizes{
        kIndefiniteSize, kIndefiniteSize}) const {
  const ComputedStyle& child_style = flex_item.Style();
  NGConstraintSpaceBuilder space_builder(ConstraintSpace(),
                                         child_style.GetWritingMode(),
                                         /* is_new_fc */ true);
  SetOrthogonalFallbackInlineSizeIfNeeded(Style(), flex_item, &space_builder);
  space_builder.SetCacheSlot(NGCacheSlot::kMeasure);
  space_builder.SetIsPaintedAtomically(true);

  NGBoxStrut margins = physical_margins.ConvertToLogical(
      ConstraintSpace().GetWritingMode(), Style().Direction());
  LogicalSize child_available_size = ChildAvailableSize();
  if (ShouldItemShrinkToFit(flex_item)) {
    space_builder.SetIsShrinkToFit(true);
  } else if (cross_axis_min_max.min_size != kIndefiniteSize &&
             WillChildCrossSizeBeContainerCrossSize(flex_item)) {
    LayoutUnit cross_size =
        CalculateFixedCrossSize(cross_axis_min_max, margins);
    if (is_column_) {
      space_builder.SetIsFixedInlineSize(true);
      child_available_size.inline_size = cross_size;
    } else {
      space_builder.SetIsFixedBlockSize(true);
      child_available_size.block_size = cross_size;
    }
  }

  space_builder.SetNeedsBaseline(
      ConstraintSpace().NeedsBaseline() ||
      FlexLayoutAlgorithm::AlignmentForChild(Style(), child_style) ==
          ItemPosition::kBaseline);

  // For determining the intrinsic block-size we make %-block-sizes resolve
  // against an indefinite size.
  LogicalSize child_percentage_size = child_percentage_size_;
  if (is_column_)
    child_percentage_size.block_size = kIndefiniteSize;

  space_builder.SetAvailableSize(child_available_size);
  space_builder.SetPercentageResolutionSize(child_percentage_size);
  // TODO(dgrogan): The SetReplacedPercentageResolutionSize calls in this file
  // may be untested. Write a test or determine why they're unnecessary.
  space_builder.SetReplacedPercentageResolutionSize(child_percentage_size);
  space_builder.SetTextDirection(child_style.Direction());
  return space_builder.ToConstraintSpace();
}

NGConstraintSpace NGFlexLayoutAlgorithm::BuildSpaceForFlexBasis(
    const NGBlockNode& flex_item) const {
  NGConstraintSpaceBuilder space_builder(ConstraintSpace(),
                                         flex_item.Style().GetWritingMode(),
                                         /* is_new_fc */ true);
  SetOrthogonalFallbackInlineSizeIfNeeded(Style(), flex_item, &space_builder);

  // This space is only used for resolving lengths, not for layout. We only
  // need the available and percentage sizes.
  space_builder.SetAvailableSize(ChildAvailableSize());
  space_builder.SetPercentageResolutionSize(child_percentage_size_);
  space_builder.SetReplacedPercentageResolutionSize(child_percentage_size_);
  return space_builder.ToConstraintSpace();
}

void NGFlexLayoutAlgorithm::ConstructAndAppendFlexItems() {
  NGFlexChildIterator iterator(Node());
  for (NGBlockNode child = iterator.NextChild(); child;
       child = iterator.NextChild()) {
    if (child.IsOutOfFlowPositioned()) {
      HandleOutOfFlowPositioned(child);
      continue;
    }

    const ComputedStyle& child_style = child.Style();
    NGConstraintSpace flex_basis_space = BuildSpaceForFlexBasis(child);

    NGPhysicalBoxStrut physical_child_margins =
        ComputePhysicalMargins(flex_basis_space, child_style);

    NGBoxStrut border_padding_in_child_writing_mode =
        ComputeBorders(flex_basis_space, child) +
        ComputePadding(flex_basis_space, child_style);

    NGPhysicalBoxStrut physical_border_padding(
        border_padding_in_child_writing_mode.ConvertToPhysical(
            child_style.GetWritingMode(), child_style.Direction()));

    LayoutUnit main_axis_border_padding =
        is_horizontal_flow_ ? physical_border_padding.HorizontalSum()
                            : physical_border_padding.VerticalSum();
    LayoutUnit cross_axis_border_padding =
        is_horizontal_flow_ ? physical_border_padding.VerticalSum()
                            : physical_border_padding.HorizontalSum();

    base::Optional<MinMaxSizesResult> min_max_sizes;
    auto MinMaxSizesFunc = [&](MinMaxSizesType type) -> MinMaxSizesResult {
      if (!min_max_sizes) {
        // We want the child's intrinsic inline sizes in its writing mode, so
        // pass child's writing mode as the first parameter, which is nominally
        // |container_writing_mode|.
        NGConstraintSpace child_space = BuildSpaceForIntrinsicBlockSize(child);
        MinMaxSizesInput input(ChildAvailableSize().block_size, type);
        min_max_sizes = child.ComputeMinMaxSizes(child_style.GetWritingMode(),
                                                 input, &child_space);
      }
      return *min_max_sizes;
    };

    MinMaxSizes min_max_sizes_in_main_axis_direction{main_axis_border_padding,
                                                     LayoutUnit::Max()};
    MinMaxSizes min_max_sizes_in_cross_axis_direction{LayoutUnit(),
                                                      LayoutUnit::Max()};
    const Length& max_property_in_main_axis = is_horizontal_flow_
                                                  ? child.Style().MaxWidth()
                                                  : child.Style().MaxHeight();
    const Length& max_property_in_cross_axis = is_horizontal_flow_
                                                   ? child.Style().MaxHeight()
                                                   : child.Style().MaxWidth();
    const Length& min_property_in_cross_axis = is_horizontal_flow_
                                                   ? child.Style().MinHeight()
                                                   : child.Style().MinWidth();
    if (MainAxisIsInlineAxis(child)) {
      min_max_sizes_in_main_axis_direction.max_size = ResolveMaxInlineLength(
          flex_basis_space, child_style, border_padding_in_child_writing_mode,
          MinMaxSizesFunc, max_property_in_main_axis,
          LengthResolvePhase::kLayout);
      min_max_sizes_in_cross_axis_direction.max_size = ResolveMaxBlockLength(
          flex_basis_space, child_style, border_padding_in_child_writing_mode,
          max_property_in_cross_axis, LengthResolvePhase::kLayout);
      min_max_sizes_in_cross_axis_direction.min_size = ResolveMinBlockLength(
          flex_basis_space, child_style, border_padding_in_child_writing_mode,
          min_property_in_cross_axis, LengthResolvePhase::kLayout);
    } else {
      min_max_sizes_in_main_axis_direction.max_size = ResolveMaxBlockLength(
          flex_basis_space, child_style, border_padding_in_child_writing_mode,
          max_property_in_main_axis, LengthResolvePhase::kLayout);
      min_max_sizes_in_cross_axis_direction.max_size = ResolveMaxInlineLength(
          flex_basis_space, child_style, border_padding_in_child_writing_mode,
          MinMaxSizesFunc, max_property_in_cross_axis,
          LengthResolvePhase::kLayout);
      min_max_sizes_in_cross_axis_direction.min_size = ResolveMinInlineLength(
          flex_basis_space, child_style, border_padding_in_child_writing_mode,
          MinMaxSizesFunc, min_property_in_cross_axis,
          LengthResolvePhase::kLayout);
    }

    base::Optional<LayoutUnit> intrinsic_block_size;
    auto IntrinsicBlockSizeFunc = [&]() -> LayoutUnit {
      if (!intrinsic_block_size) {
        NGConstraintSpace child_space = BuildSpaceForIntrinsicBlockSize(
            child, physical_child_margins,
            min_max_sizes_in_cross_axis_direction);
        scoped_refptr<const NGLayoutResult> layout_result =
            child.Layout(child_space, /* break_token */ nullptr);
        intrinsic_block_size = layout_result->IntrinsicBlockSize();
      }
      return *intrinsic_block_size;
    };

    // The logic that calculates flex_base_border_box assumes that the used
    // value of the flex-basis property is either definite or 'content'.
    LayoutUnit flex_base_border_box;
    const Length& specified_length_in_main_axis =
        is_horizontal_flow_ ? child_style.Width() : child_style.Height();
    const Length& flex_basis = child_style.FlexBasis();
    Length length_to_resolve = Length::Auto();
    if (flex_basis.IsAuto()) {
      if (!is_column_ || IsItemMainSizeDefinite(child))
        length_to_resolve = specified_length_in_main_axis;
    } else if (IsItemFlexBasisDefinite(child)) {
      length_to_resolve = flex_basis;
    }

    if (length_to_resolve.IsAuto()) {
      // This block means that the used flex-basis is 'content'. In here we
      // implement parts B,C,D,E of 9.2.3
      // https://drafts.csswg.org/css-flexbox/#algo-main-item
      const Length& cross_axis_length =
          is_horizontal_flow_ ? child.Style().Height() : child.Style().Width();
      // This check should use HasAspectRatio() instead of Style().
      // AspectRatio(), but to avoid introducing a behavior change we only
      // do this for the aspect-ratio property for now until FlexNG ships.
      bool use_container_cross_size_for_aspect_ratio =
          (child.Style().AspectRatio() ||
           (child.HasAspectRatio() &&
            RuntimeEnabledFeatures::FlexAspectRatioEnabled())) &&
          WillChildCrossSizeBeContainerCrossSize(child);
      if (use_container_cross_size_for_aspect_ratio ||
          (child.HasAspectRatio() &&
           (IsItemCrossAxisLengthDefinite(child, cross_axis_length)))) {
        // This is Part B of 9.2.3
        // https://drafts.csswg.org/css-flexbox/#algo-main-item It requires that
        // the item has a definite cross size.
        LayoutUnit cross_size;
        if (use_container_cross_size_for_aspect_ratio) {
          NGBoxStrut margins = physical_child_margins.ConvertToLogical(
              ConstraintSpace().GetWritingMode(), Style().Direction());
          cross_size = CalculateFixedCrossSize(
              min_max_sizes_in_cross_axis_direction, margins);
        } else if (MainAxisIsInlineAxis(child)) {
          cross_size = ResolveMainBlockLength(
              flex_basis_space, child_style,
              border_padding_in_child_writing_mode, cross_axis_length,
              kIndefiniteSize, LengthResolvePhase::kLayout);
        } else {
          cross_size =
              ResolveMainInlineLength(flex_basis_space, child_style,
                                      border_padding_in_child_writing_mode,
                                      MinMaxSizesFunc, cross_axis_length);
        }
        cross_size = min_max_sizes_in_cross_axis_direction.ClampSizeToMinAndMax(
            cross_size);
        flex_base_border_box =
            LayoutUnit((cross_size - cross_axis_border_padding) *
                           GetMainOverCrossAspectRatio(child) +
                       main_axis_border_padding);
      } else if (MainAxisIsInlineAxis(child)) {
        // We're now in parts C, D, and E for what are usually (horizontal-tb
        // containers AND children) row flex containers. I _think_ the C and D
        // cases are correctly handled by this code, which was originally
        // written for case E.
        if (child.HasAspectRatio()) {
          // Legacy uses child.PreferredLogicalWidths() for this case, which
          // is not exactly correct.
          // TODO(dgrogan): Replace with a variant of ComputeReplacedSize that
          // ignores min-width, width, max-width.
          MinMaxSizesInput input(child_percentage_size_.block_size,
                                 MinMaxSizesType::kContent);
          flex_base_border_box =
              ComputeMinAndMaxContentContribution(Style(), child, input)
                  .sizes.max_size;
        } else {
          flex_base_border_box =
              MinMaxSizesFunc(MinMaxSizesType::kContent).sizes.max_size;
        }
      } else {
        // Parts C, D, and E for what are usually column flex containers.
        //
        // This is the post-layout height for aspect-ratio items, which matches
        // legacy but isn't always correct.
        // TODO(dgrogan): Replace with a variant of ComputeReplacedSize that
        // ignores min-height, height, max-height.
        flex_base_border_box = IntrinsicBlockSizeFunc();
      }
    } else {
      // Part A of 9.2.3 https://drafts.csswg.org/css-flexbox/#algo-main-item
      if (MainAxisIsInlineAxis(child)) {
        flex_base_border_box = ResolveMainInlineLength(
            flex_basis_space, child_style, border_padding_in_child_writing_mode,
            MinMaxSizesFunc, length_to_resolve);
      } else {
        // Flex container's main axis is in child's block direction. Child's
        // flex basis is in child's block direction.
        flex_base_border_box = ResolveMainBlockLength(
            flex_basis_space, child_style, border_padding_in_child_writing_mode,
            length_to_resolve, IntrinsicBlockSizeFunc,
            LengthResolvePhase::kLayout);
      }
    }

    // Spec calls this "flex base size"
    // https://www.w3.org/TR/css-flexbox-1/#algo-main-item
    // Blink's FlexibleBoxAlgorithm expects it to be content + scrollbar widths,
    // but no padding or border.
    DCHECK_GE(flex_base_border_box, main_axis_border_padding);
    LayoutUnit flex_base_content_size =
        flex_base_border_box - main_axis_border_padding;

    const Length& min = is_horizontal_flow_ ? child.Style().MinWidth()
                                            : child.Style().MinHeight();
    if (algorithm_->ShouldApplyMinSizeAutoForChild(*child.GetLayoutBox())) {
      LayoutUnit content_size_suggestion;
      if (MainAxisIsInlineAxis(child)) {
        content_size_suggestion =
            MinMaxSizesFunc(MinMaxSizesType::kContent).sizes.min_size;
      } else {
        LayoutUnit intrinsic_block_size;
        if (child.IsReplaced()) {
          base::Optional<LayoutUnit> computed_inline_size;
          base::Optional<LayoutUnit> computed_block_size;
          child.IntrinsicSize(&computed_inline_size, &computed_block_size);

          // The 150 is for replaced elements that have no size, which SVG
          // can have (maybe others?).
          intrinsic_block_size =
              computed_block_size.value_or(LayoutUnit(150)) +
              border_padding_in_child_writing_mode.BlockSum();
        } else {
          intrinsic_block_size = IntrinsicBlockSizeFunc();
        }
        content_size_suggestion = intrinsic_block_size;
      }
      DCHECK_GE(content_size_suggestion, main_axis_border_padding);

      if (child.HasAspectRatio()) {
        content_size_suggestion =
            AdjustChildSizeForAspectRatioCrossAxisMinAndMax(
                child, content_size_suggestion,
                min_max_sizes_in_cross_axis_direction.min_size,
                min_max_sizes_in_cross_axis_direction.max_size,
                main_axis_border_padding, cross_axis_border_padding);
      }

      LayoutUnit specified_size_suggestion = LayoutUnit::Max();
      // If the item’s computed main size property is definite, then the
      // specified size suggestion is that size.
      if (MainAxisIsInlineAxis(child)) {
        if (!specified_length_in_main_axis.IsAuto()) {
          // TODO(dgrogan): Optimization opportunity: we may have already
          // resolved specified_length_in_main_axis in the flex basis
          // calculation. Reuse that if possible.
          specified_size_suggestion = ResolveMainInlineLength(
              flex_basis_space, child_style,
              border_padding_in_child_writing_mode, MinMaxSizesFunc,
              specified_length_in_main_axis);
        }
      } else if (!BlockLengthUnresolvable(flex_basis_space,
                                          specified_length_in_main_axis,
                                          LengthResolvePhase::kLayout)) {
        specified_size_suggestion = ResolveMainBlockLength(
            flex_basis_space, child_style, border_padding_in_child_writing_mode,
            specified_length_in_main_axis, IntrinsicBlockSizeFunc,
            LengthResolvePhase::kLayout);
        DCHECK_NE(specified_size_suggestion, kIndefiniteSize);
      }

      LayoutUnit transferred_size_suggestion = LayoutUnit::Max();
      if (specified_size_suggestion == LayoutUnit::Max() &&
          child.HasAspectRatio()) {
        const Length& cross_axis_length =
            is_horizontal_flow_ ? child_style.Height() : child_style.Width();
        if (IsItemCrossAxisLengthDefinite(child, cross_axis_length)) {
          LayoutUnit cross_axis_size;
          if (MainAxisIsInlineAxis(child)) {
            cross_axis_size = ResolveMainBlockLength(
                flex_basis_space, child_style,
                border_padding_in_child_writing_mode, cross_axis_length,
                kIndefiniteSize, LengthResolvePhase::kLayout);
            DCHECK_NE(cross_axis_size, kIndefiniteSize);
          } else {
            cross_axis_size =
                ResolveMainInlineLength(flex_basis_space, child_style,
                                        border_padding_in_child_writing_mode,
                                        MinMaxSizesFunc, cross_axis_length);
          }
          double ratio = GetMainOverCrossAspectRatio(child);
          transferred_size_suggestion = LayoutUnit(
              main_axis_border_padding +
              ratio *
                  (min_max_sizes_in_cross_axis_direction.ClampSizeToMinAndMax(
                       cross_axis_size) -
                   cross_axis_border_padding));
        }
      }

      DCHECK(specified_size_suggestion == LayoutUnit::Max() ||
             transferred_size_suggestion == LayoutUnit::Max());

      min_max_sizes_in_main_axis_direction.min_size =
          std::min({specified_size_suggestion, content_size_suggestion,
                    transferred_size_suggestion,
                    min_max_sizes_in_main_axis_direction.max_size});
    } else if (MainAxisIsInlineAxis(child)) {
      min_max_sizes_in_main_axis_direction.min_size = ResolveMinInlineLength(
          flex_basis_space, child_style, border_padding_in_child_writing_mode,
          MinMaxSizesFunc, min, LengthResolvePhase::kLayout);
    } else {
      min_max_sizes_in_main_axis_direction.min_size = ResolveMinBlockLength(
          flex_basis_space, child_style, border_padding_in_child_writing_mode,
          min, LengthResolvePhase::kLayout);
    }
    // Flex needs to never give a table a flexed main size that is less than its
    // min-content size, so floor min-width by min-content size.
    // TODO(dgrogan): This should probably apply to column flexboxes also,
    // but that's not what legacy does.
    if (child.IsTable() && !is_column_) {
      MinMaxSizes table_intrinsic_widths =
          child
              .ComputeMinMaxSizes(
                  ConstraintSpace().GetWritingMode(),
                  MinMaxSizesInput(child_percentage_size_.block_size,
                                   MinMaxSizesType::kContent))
              .sizes;

      min_max_sizes_in_main_axis_direction.Encompass(
          table_intrinsic_widths.min_size);
    }

    min_max_sizes_in_main_axis_direction -= main_axis_border_padding;
    DCHECK_GE(min_max_sizes_in_main_axis_direction.min_size, 0);
    DCHECK_GE(min_max_sizes_in_main_axis_direction.max_size, 0);

    NGBoxStrut scrollbars = ComputeScrollbarsForNonAnonymous(child);
    algorithm_
        ->emplace_back(nullptr, child.Style(), flex_base_content_size,
                       min_max_sizes_in_main_axis_direction,
                       min_max_sizes_in_cross_axis_direction,
                       main_axis_border_padding, cross_axis_border_padding,
                       physical_child_margins, scrollbars)
        .ng_input_node = child;
  }
}

LayoutUnit
NGFlexLayoutAlgorithm::AdjustChildSizeForAspectRatioCrossAxisMinAndMax(
    const NGBlockNode& child,
    LayoutUnit content_size_suggestion,
    LayoutUnit cross_min,
    LayoutUnit cross_max,
    LayoutUnit main_axis_border_padding,
    LayoutUnit cross_axis_border_padding) {
  DCHECK(child.HasAspectRatio());

  double ratio = GetMainOverCrossAspectRatio(child);
  // Clamp content_suggestion by any definite min and max cross size properties
  // converted through the aspect ratio.
  const Length& cross_max_length = is_horizontal_flow_
                                       ? child.Style().MaxHeight()
                                       : child.Style().MaxWidth();
  DCHECK_GE(cross_max, cross_axis_border_padding);
  // TODO(dgrogan): No tests fail if we unconditionally apply max_main_length.
  // Either add a test that needs it or remove it.
  if (IsItemCrossAxisLengthDefinite(child, cross_max_length)) {
    LayoutUnit max_main_length =
        main_axis_border_padding +
        LayoutUnit((cross_max - cross_axis_border_padding) * ratio);
    content_size_suggestion =
        std::min(max_main_length, content_size_suggestion);
  }

  const Length& cross_min_length = is_horizontal_flow_
                                       ? child.Style().MinHeight()
                                       : child.Style().MinWidth();
  DCHECK_GE(cross_min, cross_axis_border_padding);
  // TODO(dgrogan): Same as above with min_main_length here -- it may be
  // unneeded or untested.
  if (IsItemCrossAxisLengthDefinite(child, cross_min_length)) {
    LayoutUnit min_main_length =
        main_axis_border_padding +
        LayoutUnit((cross_min - cross_axis_border_padding) * ratio);
    content_size_suggestion =
        std::max(min_main_length, content_size_suggestion);
  }
  return content_size_suggestion;
}

scoped_refptr<const NGLayoutResult> NGFlexLayoutAlgorithm::Layout() {
  if (auto result = LayoutInternal())
    return result;

  // We may have aborted layout due to a child changing scrollbars, relayout
  // with the new scrollbar information.
  return RelayoutIgnoringChildScrollbarChanges();
}

scoped_refptr<const NGLayoutResult>
NGFlexLayoutAlgorithm::RelayoutIgnoringChildScrollbarChanges() {
  DCHECK(!ignore_child_scrollbar_changes_);
  // Freezing the scrollbars for the sub-tree shouldn't be strictly necessary,
  // but we do this just in case we trigger an unstable layout.
  PaintLayerScrollableArea::FreezeScrollbarsScope freeze_scrollbars;
  NGLayoutAlgorithmParams params(
      Node(), container_builder_.InitialFragmentGeometry(), ConstraintSpace(),
      BreakToken(), /* early_break */ nullptr);
  NGFlexLayoutAlgorithm algorithm(params);
  algorithm.ignore_child_scrollbar_changes_ = true;
  return algorithm.Layout();
}

scoped_refptr<const NGLayoutResult> NGFlexLayoutAlgorithm::LayoutInternal() {
  PaintLayerScrollableArea::DelayScrollOffsetClampScope delay_clamp_scope;
  ConstructAndAppendFlexItems();

  LayoutUnit main_axis_start_offset;
  LayoutUnit main_axis_end_offset;
  LayoutUnit cross_axis_offset = BorderScrollbarPadding().block_start;
  if (is_column_) {
    const bool is_column_reverse =
        Style().ResolvedIsColumnReverseFlexDirection();
    main_axis_start_offset =
        is_column_reverse ? LayoutUnit() : BorderScrollbarPadding().block_start;
    main_axis_end_offset =
        is_column_reverse ? LayoutUnit() : BorderScrollbarPadding().block_end;
    cross_axis_offset = BorderScrollbarPadding().inline_start;
  } else if (Style().ResolvedIsRowReverseFlexDirection()) {
    main_axis_start_offset = BorderScrollbarPadding().inline_end;
    main_axis_end_offset = BorderScrollbarPadding().inline_start;
  } else {
    main_axis_start_offset = BorderScrollbarPadding().inline_start;
    main_axis_end_offset = BorderScrollbarPadding().inline_end;
  }
  FlexLine* line;
  while (
      (line = algorithm_->ComputeNextFlexLine(border_box_size_.inline_size))) {
    line->SetContainerMainInnerSize(
        MainAxisContentExtent(line->sum_hypothetical_main_size));
    line->FreezeInflexibleItems();
    while (!line->ResolveFlexibleLengths()) {
      continue;
    }
    for (wtf_size_t i = 0; i < line->line_items.size(); ++i) {
      FlexItem& flex_item = line->line_items[i];

      const ComputedStyle& child_style = flex_item.ng_input_node.Style();
      NGConstraintSpaceBuilder space_builder(ConstraintSpace(),
                                             child_style.GetWritingMode(),
                                             /* is_new_fc */ true);
      SetOrthogonalFallbackInlineSizeIfNeeded(Style(), flex_item.ng_input_node,
                                              &space_builder);
      space_builder.SetTextDirection(child_style.Direction());
      space_builder.SetIsPaintedAtomically(true);

      LogicalSize available_size;
      NGBoxStrut margins = flex_item.physical_margins.ConvertToLogical(
          ConstraintSpace().GetWritingMode(), Style().Direction());
      LayoutUnit fixed_aspect_ratio_cross_size = kIndefiniteSize;
      if (RuntimeEnabledFeatures::FlexAspectRatioEnabled() &&
          flex_item.ng_input_node.HasAspectRatio() &&
          flex_item.ng_input_node.IsReplaced()) {
        // This code derives the cross axis size from the flexed main size and
        // the aspect ratio. We can delete this code when
        // NGReplacedLayoutAlgorithm exists, because it will do this for us.
        NGConstraintSpace flex_basis_space =
            BuildSpaceForFlexBasis(flex_item.ng_input_node);
        const Length& cross_axis_length =
            is_horizontal_flow_ ? child_style.Height() : child_style.Width();
        // Only derive the cross axis size from the aspect ratio if the computed
        // cross axis length might be indefinite. The item's cross axis length
        // might still be definite if it is stretched, but that is checked in
        // the |WillChildCrossSizeBeContainerCrossSize| calls below.
        if (cross_axis_length.IsAuto() ||
            (MainAxisIsInlineAxis(flex_item.ng_input_node) &&
             BlockLengthUnresolvable(flex_basis_space, cross_axis_length,
                                     LengthResolvePhase::kLayout))) {
          fixed_aspect_ratio_cross_size =
              flex_item.min_max_cross_sizes->ClampSizeToMinAndMax(
                  flex_item.cross_axis_border_padding +
                  LayoutUnit(
                      flex_item.flexed_content_size /
                      GetMainOverCrossAspectRatio(flex_item.ng_input_node)));
        }
      }
      if (is_column_) {
        available_size.inline_size = ChildAvailableSize().inline_size;
        available_size.block_size =
            flex_item.flexed_content_size + flex_item.main_axis_border_padding;
        space_builder.SetIsFixedBlockSize(true);
        if (WillChildCrossSizeBeContainerCrossSize(flex_item.ng_input_node)) {
          space_builder.SetIsFixedInlineSize(true);
          available_size.inline_size = CalculateFixedCrossSize(
              flex_item.min_max_cross_sizes.value(), margins);
        } else if (fixed_aspect_ratio_cross_size != kIndefiniteSize) {
          space_builder.SetIsFixedInlineSize(true);
          available_size.inline_size = fixed_aspect_ratio_cross_size;
        }
        // https://drafts.csswg.org/css-flexbox/#definite-sizes
        // If the flex container has a definite main size, a flex item's
        // post-flexing main size is treated as definite, even though it can
        // rely on the indefinite sizes of any flex items in the same line.
        if (!IsColumnContainerMainSizeDefinite() &&
            !IsItemMainSizeDefinite(flex_item.ng_input_node)) {
          space_builder.SetIsFixedBlockSizeIndefinite(true);
        }
      } else {
        available_size.inline_size =
            flex_item.flexed_content_size + flex_item.main_axis_border_padding;
        available_size.block_size = ChildAvailableSize().block_size;
        space_builder.SetIsFixedInlineSize(true);
        if (WillChildCrossSizeBeContainerCrossSize(flex_item.ng_input_node)) {
          space_builder.SetIsFixedBlockSize(true);
          available_size.block_size = CalculateFixedCrossSize(
              flex_item.min_max_cross_sizes.value(), margins);
        } else if (fixed_aspect_ratio_cross_size != kIndefiniteSize) {
          space_builder.SetIsFixedBlockSize(true);
          available_size.block_size = fixed_aspect_ratio_cross_size;
        } else if (DoesItemStretch(flex_item.ng_input_node)) {
          // If we are in a row flexbox, and we don't have a fixed block-size
          // (yet), use the "measure" cache slot. This will be the first
          // layout, and we will use the "layout" cache slot if this gets
          // stretched later.
          //
          // Setting the "measure" cache slot on the space writes the result
          // into both the "measure", and "layout" cache slots. If a subsequent
          // "layout" occurs, it'll just get written into the "layout" slot.
          space_builder.SetCacheSlot(NGCacheSlot::kMeasure);
        }
      }

      space_builder.SetNeedsBaseline(
          ConstraintSpace().NeedsBaseline() ||
          FlexLayoutAlgorithm::AlignmentForChild(Style(), child_style) ==
              ItemPosition::kBaseline);

      space_builder.SetAvailableSize(available_size);
      space_builder.SetPercentageResolutionSize(child_percentage_size_);
      space_builder.SetReplacedPercentageResolutionSize(child_percentage_size_);

      // https://drafts.csswg.org/css-flexbox/#algo-cross-item
      // Determine the hypothetical cross size of each item by performing layout
      // with the used main size and the available space, treating auto as
      // fit-content.
      if (ShouldItemShrinkToFit(flex_item.ng_input_node))
        space_builder.SetIsShrinkToFit(true);

      // For a button child, we need the baseline type same as the container's
      // baseline type for UseCounter. For example, if the container's display
      // property is 'inline-block', we need the last-line baseline of the
      // child. See the bottom of GiveLinesAndItemsFinalPositionAndSize().
      if (Node().IsButton()) {
        space_builder.SetBaselineAlgorithmType(
            ConstraintSpace().BaselineAlgorithmType());
      }

      NGConstraintSpace child_space = space_builder.ToConstraintSpace();
      flex_item.layout_result =
          flex_item.ng_input_node.Layout(child_space, nullptr /*break token*/);

      // TODO(layout-dev): Handle abortions caused by block fragmentation.
      DCHECK_EQ(flex_item.layout_result->Status(), NGLayoutResult::kSuccess);

      flex_item.cross_axis_size =
          is_horizontal_flow_
              ? flex_item.layout_result->PhysicalFragment().Size().height
              : flex_item.layout_result->PhysicalFragment().Size().width;
    }
    // cross_axis_offset is updated in each iteration of the loop, for passing
    // in to the next iteration.
    line->ComputeLineItemsPosition(main_axis_start_offset, main_axis_end_offset,
                                   cross_axis_offset);
  }

  LayoutUnit intrinsic_block_size = BorderScrollbarPadding().BlockSum();

  if (algorithm_->FlexLines().IsEmpty() && Node().HasLineIfEmpty()) {
    intrinsic_block_size += Node().GetLayoutBox()->LogicalHeightForEmptyLine();
  } else {
    intrinsic_block_size += algorithm_->IntrinsicContentBlockSize();
  }

  intrinsic_block_size =
      ClampIntrinsicBlockSize(ConstraintSpace(), Node(),
                              BorderScrollbarPadding(), intrinsic_block_size);
  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), BorderPadding(), intrinsic_block_size,
      border_box_size_.inline_size);

  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  bool success = GiveLinesAndItemsFinalPositionAndSize();
  if (!success)
    return nullptr;

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();

  return container_builder_.ToBoxFragment();
}

void NGFlexLayoutAlgorithm::ApplyStretchAlignmentToChild(FlexItem& flex_item) {
  const ComputedStyle& child_style = flex_item.ng_input_node.Style();
  NGConstraintSpaceBuilder space_builder(ConstraintSpace(),
                                         child_style.GetWritingMode(),
                                         /* is_new_fc */ true);
  SetOrthogonalFallbackInlineSizeIfNeeded(Style(), flex_item.ng_input_node,
                                          &space_builder);
  space_builder.SetIsPaintedAtomically(true);

  LogicalSize available_size(
      flex_item.flexed_content_size + flex_item.main_axis_border_padding,
      flex_item.cross_axis_size);
  if (is_column_) {
    available_size.Transpose();
    if (!IsColumnContainerMainSizeDefinite() &&
        !IsItemMainSizeDefinite(flex_item.ng_input_node)) {
      space_builder.SetIsFixedBlockSizeIndefinite(true);
    }
  }

  space_builder.SetNeedsBaseline(
      ConstraintSpace().NeedsBaseline() ||
      FlexLayoutAlgorithm::AlignmentForChild(Style(), child_style) ==
          ItemPosition::kBaseline);

  space_builder.SetTextDirection(child_style.Direction());
  space_builder.SetAvailableSize(available_size);
  space_builder.SetPercentageResolutionSize(child_percentage_size_);
  space_builder.SetReplacedPercentageResolutionSize(child_percentage_size_);
  space_builder.SetIsFixedInlineSize(true);
  space_builder.SetIsFixedBlockSize(true);
  NGConstraintSpace child_space = space_builder.ToConstraintSpace();
  flex_item.layout_result =
      flex_item.ng_input_node.Layout(child_space, /* break_token */ nullptr);
}

bool NGFlexLayoutAlgorithm::GiveLinesAndItemsFinalPositionAndSize() {
  Vector<FlexLine>& line_contexts = algorithm_->FlexLines();
  const LayoutUnit cross_axis_start_edge =
      line_contexts.IsEmpty() ? LayoutUnit()
                              : line_contexts[0].cross_axis_offset;

  LayoutUnit final_content_main_size =
      container_builder_.InlineSize() - BorderScrollbarPadding().InlineSum();
  LayoutUnit final_content_cross_size = container_builder_.FragmentBlockSize() -
                                        BorderScrollbarPadding().BlockSum();
  if (is_column_)
    std::swap(final_content_main_size, final_content_cross_size);

  if (!algorithm_->IsMultiline() && !line_contexts.IsEmpty())
    line_contexts[0].cross_axis_extent = final_content_cross_size;

  algorithm_->AlignFlexLines(final_content_cross_size);

  algorithm_->AlignChildren();

  if (Style().FlexWrap() == EFlexWrap::kWrapReverse) {
    // flex-wrap: wrap-reverse reverses the order of the lines in the container;
    // FlipForWrapReverse recalculates each item's cross axis position. We have
    // to do that after AlignChildren sets an initial cross axis position.
    algorithm_->FlipForWrapReverse(cross_axis_start_edge,
                                   final_content_cross_size);
  }

  if (Style().ResolvedIsColumnReverseFlexDirection()) {
    algorithm_->LayoutColumnReverse(final_content_main_size,
                                    BorderScrollbarPadding().block_start);
  }

  base::Optional<LayoutUnit> fallback_baseline;

  bool success = true;
  LayoutUnit overflow_block_size;
  for (FlexLine& line_context : line_contexts) {
    for (wtf_size_t child_number = 0;
         child_number < line_context.line_items.size(); ++child_number) {
      FlexItem& flex_item = line_context.line_items[child_number];

      if (DoesItemStretch(flex_item.ng_input_node))
        ApplyStretchAlignmentToChild(flex_item);

      const auto& physical_fragment = To<NGPhysicalBoxFragment>(
          flex_item.layout_result->PhysicalFragment());

      // flex_item.desired_location stores the main axis offset in X and the
      // cross axis offset in Y. But AddChild wants offset from parent
      // rectangle, so we have to transpose for columns. AddChild takes care of
      // any writing mode differences though.
      LayoutPoint location = is_column_
                                 ? flex_item.desired_location.TransposedPoint()
                                 : flex_item.desired_location;

      NGBoxFragment fragment(ConstraintSpace().GetWritingMode(),
                             ConstraintSpace().Direction(), physical_fragment);
      // Only propagate baselines from children on the first flex-line.
      if (&line_context == line_contexts.begin()) {
        PropagateBaselineFromChild(flex_item, fragment, location.Y(),
                                   &fallback_baseline);
      }

      container_builder_.AddChild(physical_fragment,
                                  {location.X(), location.Y()});

      flex_item.ng_input_node.StoreMargins(flex_item.physical_margins);

      LayoutUnit margin_block_end =
          flex_item.physical_margins
              .ConvertToLogical(ConstraintSpace().GetWritingMode(),
                                ConstraintSpace().Direction())
              .block_end;
      overflow_block_size =
          std::max(overflow_block_size,
                   location.Y() + fragment.BlockSize() + margin_block_end);

      // Detect if the flex-item had its scrollbar state change. If so we need
      // to relayout as the input to the flex algorithm is incorrect.
      if (!ignore_child_scrollbar_changes_) {
        if (flex_item.scrollbars !=
            ComputeScrollbarsForNonAnonymous(flex_item.ng_input_node))
          success = false;
      } else {
        DCHECK_EQ(flex_item.scrollbars,
                  ComputeScrollbarsForNonAnonymous(flex_item.ng_input_node));
      }
    }
  }

  container_builder_.SetOverflowBlockSize(overflow_block_size +
                                          BorderScrollbarPadding().block_end);

  // Set the baseline to the fallback, if we didn't find any children with
  // baseline alignment.
  if (!container_builder_.Baseline() && fallback_baseline)
    container_builder_.SetBaseline(*fallback_baseline);

  if (Node().IsButton())
    AdjustButtonBaseline(final_content_cross_size);

  // Signal if we need to relayout with new child scrollbar information.
  return success;
}

void NGFlexLayoutAlgorithm::AdjustButtonBaseline(
    LayoutUnit final_content_cross_size) {
  // See LayoutButton::BaselinePosition()
  if (!Node().HasLineIfEmpty() && !Node().ShouldApplyLayoutContainment() &&
      !container_builder_.Baseline()) {
    // To ensure that we have a consistent baseline when we have no children,
    // even when we have the anonymous LayoutBlock child, we calculate the
    // baseline for the empty case manually here.
    container_builder_.SetBaseline(BorderScrollbarPadding().block_start +
                                   final_content_cross_size);
    return;
  }

  // Apply flexbox's baseline as is.  That is to say, the baseline of the
  // first line.
  // However, we count the differences between it and the last-line baseline
  // of the anonymous block. crbug.com/690036.
  // We also have a difference in empty buttons. See crbug.com/304848.

  const LayoutObject* parent = Node().GetLayoutBox()->Parent();
  if (!LayoutButton::ShouldCountWrongBaseline(
          Style(), parent ? parent->Style() : nullptr))
    return;

  // The button should have at most one child.
  const NGContainerFragmentBuilder::ChildrenVector& children =
      container_builder_.Children();
  if (children.size() < 1) {
    const LayoutBlock* layout_block = To<LayoutBlock>(Node().GetLayoutBox());
    base::Optional<LayoutUnit> baseline = layout_block->BaselineForEmptyLine(
        layout_block->IsHorizontalWritingMode() ? kHorizontalLine
                                                : kVerticalLine);
    if (container_builder_.Baseline() != baseline) {
      UseCounter::Count(Node().GetDocument(),
                        WebFeature::kWrongBaselineOfEmptyLineButton);
    }
    return;
  }
  DCHECK_EQ(children.size(), 1u);
  const NGContainerFragmentBuilder::ChildWithOffset& child = children[0];
  DCHECK(!child.fragment->IsLineBox());
  const NGConstraintSpace& space = ConstraintSpace();
  NGBoxFragment fragment(space.GetWritingMode(), space.Direction(),
                         To<NGPhysicalBoxFragment>(*child.fragment));
  base::Optional<LayoutUnit> child_baseline =
      space.BaselineAlgorithmType() == NGBaselineAlgorithmType::kFirstLine
          ? fragment.FirstBaseline()
          : fragment.Baseline();
  if (child_baseline)
    child_baseline = *child_baseline + child.offset.block_offset;
  if (container_builder_.Baseline() != child_baseline) {
    UseCounter::Count(Node().GetDocument(),
                      WebFeature::kWrongBaselineOfMultiLineButton);
  }
}

void NGFlexLayoutAlgorithm::PropagateBaselineFromChild(
    const FlexItem& flex_item,
    const NGBoxFragment& fragment,
    LayoutUnit block_offset,
    base::Optional<LayoutUnit>* fallback_baseline) {
  // Check if we've already found an appropriate baseline.
  if (container_builder_.Baseline())
    return;

  LayoutUnit baseline_offset =
      block_offset + (Node().IsButton() ? fragment.FirstBaselineOrSynthesize()
                                        : fragment.BaselineOrSynthesize());

  // We prefer a baseline from a child with baseline alignment, and no
  // auto-margins in the cross axis (even if we have to synthesize the
  // baseline).
  if (FlexLayoutAlgorithm::AlignmentForChild(Style(), flex_item.style) ==
          ItemPosition::kBaseline &&
      !flex_item.HasAutoMarginsInCrossAxis()) {
    container_builder_.SetBaseline(baseline_offset);
    return;
  }

  // Set the fallback baseline if it doesn't have a value yet.
  *fallback_baseline = fallback_baseline->value_or(baseline_offset);
}

MinMaxSizesResult NGFlexLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput& child_input) const {
  if (auto result = CalculateMinMaxSizesIgnoringChildren(
          Node(), BorderScrollbarPadding()))
    return *result;

  MinMaxSizes sizes;
  bool depends_on_percentage_block_size = false;

  int number_of_items = 0;
  NGFlexChildIterator iterator(Node());
  for (NGBlockNode child = iterator.NextChild(); child;
       child = iterator.NextChild()) {
    if (child.IsOutOfFlowPositioned())
      continue;
    number_of_items++;

    MinMaxSizesResult child_result =
        ComputeMinAndMaxContentContribution(Style(), child, child_input);
    NGBoxStrut child_margins = ComputeMinMaxMargins(Style(), child);
    child_result.sizes += child_margins.InlineSum();

    depends_on_percentage_block_size |=
        child_result.depends_on_percentage_block_size;
    if (is_column_) {
      sizes.min_size = std::max(sizes.min_size, child_result.sizes.min_size);
      sizes.max_size = std::max(sizes.max_size, child_result.sizes.max_size);
    } else {
      sizes.max_size += child_result.sizes.max_size;
      if (algorithm_->IsMultiline()) {
        sizes.min_size = std::max(sizes.min_size, child_result.sizes.min_size);
      } else {
        sizes.min_size += child_result.sizes.min_size;
      }
    }
  }
  if (!is_column_) {
    LayoutUnit gap_inline_size =
        (number_of_items - 1) * algorithm_->gap_between_items_;
    sizes.max_size += gap_inline_size;
    if (!algorithm_->IsMultiline()) {
      sizes.min_size += gap_inline_size;
    }
  }
  sizes.max_size = std::max(sizes.max_size, sizes.min_size);

  // Due to negative margins, it is possible that we calculated a negative
  // intrinsic width. Make sure that we never return a negative width.
  sizes.Encompass(LayoutUnit());
  sizes += BorderScrollbarPadding().InlineSum();
  return {sizes, depends_on_percentage_block_size};
}

}  // namespace blink
