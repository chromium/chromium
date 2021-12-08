// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_layout_algorithm.h"

#include <memory>
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/flexible_box_algorithm.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_button.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/ng/flex/layout_ng_flexible_box.h"
#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_child_iterator.h"
#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_item_iterator.h"
#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_line.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
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
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_node.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

NGFlexLayoutAlgorithm::NGFlexLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params,
    DevtoolsFlexInfo* layout_info_for_devtools)
    : NGLayoutAlgorithm(params),
      is_column_(Style().ResolvedIsColumnFlexDirection()),
      is_horizontal_flow_(FlexLayoutAlgorithm::IsHorizontalFlow(Style())),
      is_cross_size_definite_(IsContainerCrossSizeDefinite()),
      child_percentage_size_(
          CalculateChildPercentageSize(ConstraintSpace(),
                                       Node(),
                                       ChildAvailableSize())),
      algorithm_(&Style(),
                 MainAxisContentExtent(LayoutUnit::Max()),
                 child_percentage_size_,
                 &Node().GetDocument()),
      layout_info_for_devtools_(layout_info_for_devtools) {
  // TODO(almaher): Support multi-line column fragmentation.
  involved_in_block_fragmentation_ =
      InvolvedInBlockFragmentation(container_builder_) &&
      (!algorithm_.IsMultiline() || is_horizontal_flow_);
}

bool NGFlexLayoutAlgorithm::MainAxisIsInlineAxis(
    const NGBlockNode& child) const {
  return child.Style().IsHorizontalWritingMode() == is_horizontal_flow_;
}

LayoutUnit NGFlexLayoutAlgorithm::MainAxisContentExtent(
    LayoutUnit sum_hypothetical_main_size) const {
  if (is_column_) {
    // Even though we only pass border_padding in the third parameter, the
    // return value includes scrollbar, so subtract scrollbar to get content
    // size.
    // We add |border_scrollbar_padding| to the fourth parameter because
    // |content_size| needs to be the size of the border box. We've overloaded
    // the term "content".
    const LayoutUnit border_scrollbar_padding =
        BorderScrollbarPadding().BlockSum();
    absl::optional<LayoutUnit> inline_size;
    if (container_builder_.InlineSize() != kIndefiniteSize)
      inline_size = container_builder_.InlineSize();
    return ComputeBlockSizeForFragment(
               ConstraintSpace(), Style(), BorderPadding(),
               sum_hypothetical_main_size.ClampNegativeToZero() +
                   border_scrollbar_padding,
               inline_size) -
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

  DCHECK_NE(content_position, ContentPosition::kLeft);
  DCHECK_NE(content_position, ContentPosition::kRight);
  if (content_position == ContentPosition::kFlexEnd)
    return is_reverse_flex ? AxisEdge::kStart : AxisEdge::kEnd;

  if (content_position == ContentPosition::kCenter ||
      justify.Distribution() == ContentDistributionType::kSpaceAround ||
      justify.Distribution() == ContentDistributionType::kSpaceEvenly)
    return AxisEdge::kCenter;

  if (content_position == ContentPosition::kStart)
    return AxisEdge::kStart;
  if (content_position == ContentPosition::kEnd)
    return AxisEdge::kEnd;

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

// We are interested in cases where the flex item *may* expand due to
// fragmentation (lines pushed down by a fragmentation line, etc).
bool MinBlockSizeShouldEncompassIntrinsicSize(const NGFlexItem& item) {
  if (item.ng_input_node.IsMonolithic())
    return false;

  // TODO(almaher): Figure out which cases this should be true. (Should this
  // only be true when min-block-size is auto in the case of |is_column_|?)
  const auto& item_style = item.ng_input_node.Style();
  return item_style.LogicalHeight().IsAutoOrContentOrIntrinsic();
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
  return ChildAvailableSize().block_size != kIndefiniteSize;
}

bool NGFlexLayoutAlgorithm::IsContainerCrossSizeDefinite() const {
  // A column flexbox's cross axis is an inline size, so is definite.
  if (is_column_)
    return true;

  return ChildAvailableSize().block_size != kIndefiniteSize;
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

bool NGFlexLayoutAlgorithm::IsUsedFlexBasisDefinite(
    const NGBlockNode& child,
    Length* out_flex_basis = nullptr) const {
  const Length& flex_basis = GetUsedFlexBasis(child);
  if (out_flex_basis)
    *out_flex_basis = flex_basis;
  if (flex_basis.IsAuto() || flex_basis.IsContent())
    return false;
  const NGConstraintSpace& space = BuildSpaceForFlexBasis(child);
  if (MainAxisIsInlineAxis(child))
    return !InlineLengthUnresolvable(space, flex_basis);
  return !BlockLengthUnresolvable(space, flex_basis);
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
  return !BlockLengthUnresolvable(BuildSpaceForFlexBasis(child), length);
}

bool NGFlexLayoutAlgorithm::DoesItemCrossSizeComputeToAuto(
    const NGBlockNode& child) const {
  const ComputedStyle& child_style = child.Style();
  if (is_horizontal_flow_)
    return child_style.Height().IsAuto();
  return child_style.Width().IsAuto();
}

bool NGFlexLayoutAlgorithm::AspectRatioProvidesMainSize(
    const NGBlockNode& child) const {
  const Length& cross_axis_length =
      is_horizontal_flow_ ? child.Style().Height() : child.Style().Width();
  return child.HasAspectRatio() &&
         (IsItemCrossAxisLengthDefinite(child, cross_axis_length) ||
          WillChildCrossSizeBeContainerCrossSize(child));
}

bool NGFlexLayoutAlgorithm::WillChildCrossSizeBeContainerCrossSize(
    const NGBlockNode& child) const {
  return !algorithm_.IsMultiline() && is_cross_size_definite_ &&
         DoesItemStretch(child);
}

NGConstraintSpace NGFlexLayoutAlgorithm::BuildSpaceForIntrinsicBlockSize(
    const NGBlockNode& flex_item) const {
  const ComputedStyle& child_style = flex_item.Style();
  NGConstraintSpaceBuilder space_builder(ConstraintSpace(),
                                         child_style.GetWritingDirection(),
                                         /* is_new_fc */ true);
  SetOrthogonalFallbackInlineSizeIfNeeded(Style(), flex_item, &space_builder);
  space_builder.SetCacheSlot(NGCacheSlot::kMeasure);
  space_builder.SetIsPaintedAtomically(true);

  if (WillChildCrossSizeBeContainerCrossSize(flex_item)) {
    if (is_column_)
      space_builder.SetInlineAutoBehavior(NGAutoBehavior::kStretchExplicit);
    else
      space_builder.SetBlockAutoBehavior(NGAutoBehavior::kStretchExplicit);
  }

  // For determining the intrinsic block-size we make %-block-sizes resolve
  // against an indefinite size.
  LogicalSize child_percentage_size = child_percentage_size_;
  if (is_column_) {
    child_percentage_size.block_size = kIndefiniteSize;
    space_builder.SetIsInitialBlockSizeIndefinite(true);
  }

  space_builder.SetAvailableSize(ChildAvailableSize());
  space_builder.SetPercentageResolutionSize(child_percentage_size);
  // TODO(dgrogan): The SetReplacedPercentageResolutionSize calls in this file
  // may be untested. Write a test or determine why they're unnecessary.
  space_builder.SetReplacedPercentageResolutionSize(child_percentage_size);
  return space_builder.ToConstraintSpace();
}

NGConstraintSpace NGFlexLayoutAlgorithm::BuildSpaceForFlexBasis(
    const NGBlockNode& flex_item) const {
  NGConstraintSpaceBuilder space_builder(
      ConstraintSpace(), flex_item.Style().GetWritingDirection(),
      /* is_new_fc */ true);
  SetOrthogonalFallbackInlineSizeIfNeeded(Style(), flex_item, &space_builder);

  // This space is only used for resolving lengths, not for layout. We only
  // need the available and percentage sizes.
  space_builder.SetAvailableSize(ChildAvailableSize());
  space_builder.SetPercentageResolutionSize(child_percentage_size_);
  space_builder.SetReplacedPercentageResolutionSize(child_percentage_size_);
  return space_builder.ToConstraintSpace();
}

// This can return an indefinite Length.
Length NGFlexLayoutAlgorithm::GetUsedFlexBasis(const NGBlockNode& child) const {
  const ComputedStyle& child_style = child.Style();
  const Length& specified_length_in_main_axis =
      is_horizontal_flow_ ? child_style.Width() : child_style.Height();
  const Length& specified_flex_basis = child_style.FlexBasis();

  if (specified_flex_basis.IsAuto()) {
    if (specified_length_in_main_axis.IsAuto() &&
        Style().IsDeprecatedWebkitBox() &&
        (Style().BoxOrient() == EBoxOrient::kHorizontal ||
         Style().BoxAlign() != EBoxAlignment::kStretch)) {
      // 'auto' for items within a -webkit-box resolve as 'fit-content'.
      return Length::FitContent();
    }
    return specified_length_in_main_axis;
  }
  return specified_flex_basis;
}

NGConstraintSpace NGFlexLayoutAlgorithm::BuildSpaceForLayout(
    const NGBlockNode& flex_item_node,
    LayoutUnit item_main_axis_final_size,
    absl::optional<LayoutUnit> line_cross_size_for_stretch,
    absl::optional<LayoutUnit> block_offset_for_fragmentation,
    bool min_block_size_should_encompass_intrinsic_size) const {
  const ComputedStyle& child_style = flex_item_node.Style();
  NGConstraintSpaceBuilder space_builder(ConstraintSpace(),
                                         child_style.GetWritingDirection(),
                                         /* is_new_fc */ true);
  SetOrthogonalFallbackInlineSizeIfNeeded(Style(), flex_item_node,
                                          &space_builder);
  space_builder.SetIsPaintedAtomically(true);

  LogicalSize available_size;
  if (is_column_) {
    available_size.inline_size = line_cross_size_for_stretch
                                     ? *line_cross_size_for_stretch
                                     : ChildAvailableSize().inline_size;
    available_size.block_size = item_main_axis_final_size;
    space_builder.SetIsFixedBlockSize(true);
    if (line_cross_size_for_stretch ||
        WillChildCrossSizeBeContainerCrossSize(flex_item_node))
      space_builder.SetInlineAutoBehavior(NGAutoBehavior::kStretchExplicit);
    // https://drafts.csswg.org/css-flexbox/#definite-sizes
    // If the flex container has a definite main size, a flex item's
    // post-flexing main size is treated as definite, even though it can
    // rely on the indefinite sizes of any flex items in the same line.
    if (!IsColumnContainerMainSizeDefinite() &&
        !IsUsedFlexBasisDefinite(flex_item_node) &&
        !AspectRatioProvidesMainSize(flex_item_node)) {
      space_builder.SetIsInitialBlockSizeIndefinite(true);
    }
  } else {
    available_size.inline_size = item_main_axis_final_size;
    available_size.block_size = line_cross_size_for_stretch
                                    ? *line_cross_size_for_stretch
                                    : ChildAvailableSize().block_size;
    space_builder.SetIsFixedInlineSize(true);
    if (line_cross_size_for_stretch ||
        WillChildCrossSizeBeContainerCrossSize(flex_item_node))
      space_builder.SetBlockAutoBehavior(NGAutoBehavior::kStretchExplicit);
  }
  if (!line_cross_size_for_stretch && DoesItemStretch(flex_item_node)) {
    // For the first layout pass of stretched items, the goal is to determine
    // the post-flexed, pre-stretched cross-axis size. Stretched items will
    // later get a final layout with a potentially different cross size so use
    // the "measure" slot for this layout. We will use the "layout" cache slot
    // for the item's final layout.
    //
    // Setting the "measure" cache slot on the space writes the result
    // into both the "measure" and "layout" cache slots. So the stretch
    // layout will reuse this "measure" result if it can.
    space_builder.SetCacheSlot(NGCacheSlot::kMeasure);
  } else if (block_offset_for_fragmentation &&
             ConstraintSpace().HasBlockFragmentation()) {
    if (min_block_size_should_encompass_intrinsic_size)
      space_builder.SetMinBlockSizeShouldEncompassIntrinsicSize();
    SetupSpaceBuilderForFragmentation(
        ConstraintSpace(), flex_item_node, *block_offset_for_fragmentation,
        &space_builder,
        /* is_new_fc */ true,
        container_builder_.RequiresContentBeforeBreaking());
  }

  space_builder.SetAvailableSize(available_size);
  space_builder.SetPercentageResolutionSize(child_percentage_size_);
  space_builder.SetReplacedPercentageResolutionSize(child_percentage_size_);

  // For a button child, we need the baseline type same as the container's
  // baseline type for UseCounter. For example, if the container's display
  // property is 'inline-block', we need the last-line baseline of the
  // child. See the bottom of GiveItemsFinalPositionAndSize().
  if (Node().IsButton()) {
    space_builder.SetBaselineAlgorithmType(
        ConstraintSpace().BaselineAlgorithmType());
  }

  return space_builder.ToConstraintSpace();
}

void NGFlexLayoutAlgorithm::ConstructAndAppendFlexItems() {
  NGFlexChildIterator iterator(Node());

  // This block sets up data collection for
  // https://github.com/w3c/csswg-drafts/issues/3052
  bool all_items_have_non_auto_cross_sizes = true;
  bool all_items_match_container_alignment = true;
  const StyleContentAlignmentData align_content =
      algorithm_.ResolvedAlignContent(Style());
  bool is_alignment_behavior_change_possible =
      !algorithm_.IsMultiline() &&
      align_content.Distribution() != ContentDistributionType::kStretch &&
      align_content.GetPosition() != ContentPosition::kBaseline;
  const LayoutUnit kAvailableFreeSpace(100);
  LayoutUnit line_offset = FlexLayoutAlgorithm::InitialContentPositionOffset(
      Style(), kAvailableFreeSpace, align_content,
      /* number_of_items */ 1,
      /* is_reversed */ false);

  for (NGBlockNode child = iterator.NextChild(); child;
       child = iterator.NextChild()) {
    if (child.IsOutOfFlowPositioned() && !IsResumingLayout(BreakToken())) {
      // TODO(almaher): OOF static position and alignment when fragmenting. The
      // static position may get adjusted once the final container block-size is
      // known. However, we would want to use the total block-size rather than
      // the block-size of the first fragment.
      HandleOutOfFlowPositioned(child);
      continue;
    }

    const ComputedStyle& child_style = child.Style();
    if (is_alignment_behavior_change_possible &&
        all_items_match_container_alignment) {
      LayoutUnit item_offset = FlexItem::AlignmentOffset(
          kAvailableFreeSpace,
          FlexLayoutAlgorithm::AlignmentForChild(Style(), child_style),
          LayoutUnit(), LayoutUnit(), /* is_wrap_reverse */ false,
          Style().IsDeprecatedWebkitBox());
      all_items_match_container_alignment = (item_offset == line_offset);
    }

    NGConstraintSpace flex_basis_space = BuildSpaceForFlexBasis(child);

    NGPhysicalBoxStrut physical_child_margins =
        ComputePhysicalMargins(flex_basis_space, child_style);

    NGBoxStrut border_padding_in_child_writing_mode =
        ComputeBorders(flex_basis_space, child) +
        ComputePadding(flex_basis_space, child_style);

    NGPhysicalBoxStrut physical_border_padding(
        border_padding_in_child_writing_mode.ConvertToPhysical(
            child_style.GetWritingDirection()));

    LayoutUnit main_axis_border_padding =
        is_horizontal_flow_ ? physical_border_padding.HorizontalSum()
                            : physical_border_padding.VerticalSum();
    LayoutUnit cross_axis_border_padding =
        is_horizontal_flow_ ? physical_border_padding.VerticalSum()
                            : physical_border_padding.HorizontalSum();

    const Length& cross_axis_length =
        is_horizontal_flow_ ? child.Style().Height() : child.Style().Width();
    all_items_have_non_auto_cross_sizes &= !cross_axis_length.IsAuto();

    absl::optional<MinMaxSizesResult> min_max_sizes;
    auto MinMaxSizesFunc = [&](MinMaxSizesType type) -> MinMaxSizesResult {
      if (!min_max_sizes) {
        // We want the child's intrinsic inline sizes in its writing mode, so
        // pass child's writing mode as the first parameter, which is nominally
        // |container_writing_mode|.
        const auto child_space = BuildSpaceForIntrinsicBlockSize(child);
        min_max_sizes = child.ComputeMinMaxSizes(child_style.GetWritingMode(),
                                                 type, child_space);
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
    if (MainAxisIsInlineAxis(child)) {
      min_max_sizes_in_main_axis_direction.max_size = ResolveMaxInlineLength(
          flex_basis_space, child_style, border_padding_in_child_writing_mode,
          MinMaxSizesFunc, max_property_in_main_axis);
      min_max_sizes_in_cross_axis_direction = ComputeMinMaxBlockSizes(
          flex_basis_space, child_style, border_padding_in_child_writing_mode);
    } else {
      min_max_sizes_in_main_axis_direction.max_size = ResolveMaxBlockLength(
          flex_basis_space, child_style, border_padding_in_child_writing_mode,
          max_property_in_main_axis);
      min_max_sizes_in_cross_axis_direction = ComputeMinMaxInlineSizes(
          flex_basis_space, child, border_padding_in_child_writing_mode,
          MinMaxSizesFunc);
    }

    scoped_refptr<const NGLayoutResult> layout_result;
    auto IntrinsicBlockSizeFunc = [&]() -> LayoutUnit {
      if (!layout_result) {
        NGConstraintSpace child_space = BuildSpaceForIntrinsicBlockSize(child);
        layout_result = child.Layout(child_space, /* break_token */ nullptr);
        DCHECK(layout_result);
      }
      // TODO(crbug.com/1261306): This value does not account for any
      // min/main/max sizes transferred through the preferred aspect ratio, if
      // it exists. But we use this value in places where the flex spec calls
      // for 'min-content' and 'max-content', which are supposed to obey some
      // transferred sizes.
      return layout_result->IntrinsicBlockSize();
    };

    auto ComputeTransferredMainSize = [&]() -> LayoutUnit {
      DCHECK_NE(IsItemCrossAxisLengthDefinite(child, cross_axis_length),
                WillChildCrossSizeBeContainerCrossSize(child));
      LayoutUnit cross_size;
      Length cross_axis_length_to_resolve = cross_axis_length;
      if (WillChildCrossSizeBeContainerCrossSize(child))
        cross_axis_length_to_resolve = Length::FillAvailable();
      if (MainAxisIsInlineAxis(child)) {
        cross_size = ResolveMainBlockLength(
            flex_basis_space, child_style, border_padding_in_child_writing_mode,
            cross_axis_length_to_resolve, kIndefiniteSize);
      } else {
        cross_size = ResolveMainInlineLength(
            flex_basis_space, child_style, border_padding_in_child_writing_mode,
            MinMaxSizesFunc, cross_axis_length_to_resolve);
      }
      DCHECK_GE(cross_size, LayoutUnit());
      cross_size = min_max_sizes_in_cross_axis_direction.ClampSizeToMinAndMax(
          cross_size);
      if (MainAxisIsInlineAxis(child)) {
        return InlineSizeFromAspectRatio(
            border_padding_in_child_writing_mode, child.GetAspectRatio(),
            child_style.BoxSizingForAspectRatio(), cross_size);
      }
      return BlockSizeFromAspectRatio(
          border_padding_in_child_writing_mode, child.GetAspectRatio(),
          child_style.BoxSizingForAspectRatio(), cross_size);
    };

    Length flex_basis_length;
    LayoutUnit flex_base_border_box;
    if (is_column_ && child_style.FlexBasis().IsPercentOrCalc())
      has_column_percent_flex_basis_ = true;
    if (!IsUsedFlexBasisDefinite(child, &flex_basis_length)) {
      // This block means that the used flex-basis is 'content'. In here we
      // implement parts B,C,D,E of 9.2.3
      // https://drafts.csswg.org/css-flexbox/#algo-main-item
      if (AspectRatioProvidesMainSize(child)) {
        // This is Part B of 9.2.3
        // https://drafts.csswg.org/css-flexbox/#algo-main-item It requires that
        // the item has a definite cross size.
        flex_base_border_box = ComputeTransferredMainSize();
      } else if (MainAxisIsInlineAxis(child)) {
        // We're now in parts C, D, and E for what are usually (horizontal-tb
        // containers AND children) row flex containers. I _think_ the C and D
        // cases are correctly handled by this code, which was originally
        // written for case E.
        flex_base_border_box =
            MinMaxSizesFunc(MinMaxSizesType::kContent).sizes.max_size;
      } else {
        // Parts C, D, and E for what are usually column flex containers.
        flex_base_border_box = IntrinsicBlockSizeFunc();
      }
    } else {
      DCHECK(!flex_basis_length.IsAuto());
      DCHECK(!flex_basis_length.IsContent());
      // Part A of 9.2.3 https://drafts.csswg.org/css-flexbox/#algo-main-item
      if (MainAxisIsInlineAxis(child)) {
        flex_base_border_box = ResolveMainInlineLength(
            flex_basis_space, child_style, border_padding_in_child_writing_mode,
            MinMaxSizesFunc, flex_basis_length);
      } else {
        // Flex container's main axis is in child's block direction. Child's
        // flex basis is in child's block direction.
        flex_base_border_box = ResolveMainBlockLength(
            flex_basis_space, child_style, border_padding_in_child_writing_mode,
            flex_basis_length, IntrinsicBlockSizeFunc);
        if (const NGTableNode* table_child = DynamicTo<NGTableNode>(&child)) {
          // (1) A table interprets forced block size as the height of its
          // captions + rows.
          // (2) The specified height of a table only applies to the rows.
          // (3) So when we read the specified height here, we have to add the
          // height of the captions before sending it through the flexing
          // algorithm, which will eventually lead to a forced block size.
          LayoutUnit caption_block_size = table_child->ComputeCaptionBlockSize(
              BuildSpaceForIntrinsicBlockSize(*table_child));
          flex_base_border_box += caption_block_size;
        }
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
    if (algorithm_.ShouldApplyMinSizeAutoForChild(*child.GetLayoutBox())) {
      LayoutUnit content_size_suggestion;
      if (MainAxisIsInlineAxis(child)) {
        content_size_suggestion =
            MinMaxSizesFunc(MinMaxSizesType::kContent).sizes.min_size;
      } else {
        content_size_suggestion = IntrinsicBlockSizeFunc();
      }
      DCHECK_GE(content_size_suggestion, main_axis_border_padding);

      // TODO(crbug.com/1252693):
      // This code block is needed because
      // IntrinsicBlockSizeFunc incorrectly ignores the inline min/max
      // constraints for aspect-ratio items. So we apply those constraints here
      // in AdjustChildSizeForAspectRatioCrossAxisMinAndMax. Once 1252693 is
      // fixed, we can delete this entire code block.
      if (child.HasAspectRatio() && !MainAxisIsInlineAxis(child)) {
        content_size_suggestion =
            AdjustChildSizeForAspectRatioCrossAxisMinAndMax(
                child, content_size_suggestion,
                min_max_sizes_in_cross_axis_direction,
                border_padding_in_child_writing_mode);
      }

      LayoutUnit specified_size_suggestion = LayoutUnit::Max();
      const Length& specified_length_in_main_axis =
          is_horizontal_flow_ ? child_style.Width() : child_style.Height();
      // If the item’s computed main size property is definite, then the
      // specified size suggestion is that size.
      if (MainAxisIsInlineAxis(child)) {
        if (!specified_length_in_main_axis.IsAuto()) {
          // Note: we may have already resolved specified_length_in_main_axis
          // when calculating flex basis. Reusing that in the current code
          // structure is a lot of work, so just recalculate here.
          specified_size_suggestion = ResolveMainInlineLength(
              flex_basis_space, child_style,
              border_padding_in_child_writing_mode, MinMaxSizesFunc,
              specified_length_in_main_axis);
        }
      } else if (!BlockLengthUnresolvable(flex_basis_space,
                                          specified_length_in_main_axis)) {
        specified_size_suggestion = ResolveMainBlockLength(
            flex_basis_space, child_style, border_padding_in_child_writing_mode,
            specified_length_in_main_axis, IntrinsicBlockSizeFunc);
        DCHECK_NE(specified_size_suggestion, kIndefiniteSize);
      }

      LayoutUnit transferred_size_suggestion = LayoutUnit::Max();
      if (specified_size_suggestion == LayoutUnit::Max() &&
          child.IsReplaced() && AspectRatioProvidesMainSize(child)) {
        transferred_size_suggestion = ComputeTransferredMainSize();
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
          MinMaxSizesFunc, min);
    } else {
      min_max_sizes_in_main_axis_direction.min_size =
          ResolveMinBlockLength(flex_basis_space, child_style,
                                border_padding_in_child_writing_mode, min);
    }
    // Flex needs to never give a table a flexed main size that is less than its
    // min-content size, so floor the min main-axis size by min-content size.
    if (child.IsTable()) {
      if (MainAxisIsInlineAxis(child)) {
        min_max_sizes_in_main_axis_direction.Encompass(
            MinMaxSizesFunc(MinMaxSizesType::kContent).sizes.min_size);
      } else {
        min_max_sizes_in_main_axis_direction.Encompass(
            IntrinsicBlockSizeFunc());
      }
    }

    min_max_sizes_in_main_axis_direction -= main_axis_border_padding;
    DCHECK_GE(min_max_sizes_in_main_axis_direction.min_size, 0);
    DCHECK_GE(min_max_sizes_in_main_axis_direction.max_size, 0);

    NGBoxStrut scrollbars = ComputeScrollbarsForNonAnonymous(child);
    algorithm_
        .emplace_back(nullptr, child.Style(), flex_base_content_size,
                      min_max_sizes_in_main_axis_direction,
                      min_max_sizes_in_cross_axis_direction,
                      main_axis_border_padding, cross_axis_border_padding,
                      physical_child_margins, scrollbars,
                      min_max_sizes.has_value())
        .ng_input_node_ = child;
    // Save the layout result so that we can maybe reuse it later.
    if (layout_result) {
      DCHECK(!MainAxisIsInlineAxis(child));
      algorithm_.all_items_.back().layout_result_ = layout_result;
    }
  }

  if (is_alignment_behavior_change_possible &&
      algorithm_.all_items_.size() > 0 &&
      !all_items_match_container_alignment) {
    if (algorithm_.IsColumnFlow()) {
      if (all_items_have_non_auto_cross_sizes) {
        UseCounter::Count(node_.GetDocument(),
                          WebFeature::kFlexboxAlignSingleLineDifference);
      }
    } else if (is_cross_size_definite_) {
      UseCounter::Count(node_.GetDocument(),
                        WebFeature::kFlexboxAlignSingleLineDifference);
    }
  }
}

LayoutUnit
NGFlexLayoutAlgorithm::AdjustChildSizeForAspectRatioCrossAxisMinAndMax(
    const NGBlockNode& child,
    LayoutUnit content_size_suggestion,
    const MinMaxSizes& cross_min_max,
    const NGBoxStrut& border_padding_in_child_writing_mode) {
  DCHECK(child.HasAspectRatio());

  // Clamp content_suggestion by any definite min and max cross size properties
  // converted through the aspect ratio.
  if (MainAxisIsInlineAxis(child)) {
    auto min_max = ComputeTransferredMinMaxInlineSizes(
        child.GetAspectRatio(), cross_min_max,
        border_padding_in_child_writing_mode,
        child.Style().BoxSizingForAspectRatio());
    return min_max.ClampSizeToMinAndMax(content_size_suggestion);
  }
  auto min_max = ComputeTransferredMinMaxBlockSizes(
      child.GetAspectRatio(), cross_min_max,
      border_padding_in_child_writing_mode,
      child.Style().BoxSizingForAspectRatio());
  return min_max.ClampSizeToMinAndMax(content_size_suggestion);
}

scoped_refptr<const NGLayoutResult> NGFlexLayoutAlgorithm::Layout() {
  auto result = LayoutInternal();
  switch (result->Status()) {
    case NGLayoutResult::kNeedsEarlierBreak:
      // If we found a good break somewhere inside this block, re-layout and
      // break at that location.
      DCHECK(result->GetEarlyBreak());
      return RelayoutAndBreakEarlier<NGFlexLayoutAlgorithm>(
          *result->GetEarlyBreak());
    case NGLayoutResult::kNeedsRelayoutWithNoChildScrollbarChanges:
      return RelayoutIgnoringChildScrollbarChanges();
    case NGLayoutResult::kDisableFragmentation:
      DCHECK(ConstraintSpace().HasBlockFragmentation());
      return RelayoutWithoutFragmentation<NGFlexLayoutAlgorithm>();
    default:
      return result;
  }
}

scoped_refptr<const NGLayoutResult>
NGFlexLayoutAlgorithm::RelayoutIgnoringChildScrollbarChanges() {
  DCHECK(!ignore_child_scrollbar_changes_);
  DCHECK(!layout_info_for_devtools_);
  DCHECK(!DevtoolsReadonlyLayoutScope::InDevtoolsLayout());
  NGLayoutAlgorithmParams params(
      Node(), container_builder_.InitialFragmentGeometry(), ConstraintSpace(),
      BreakToken(), /* early_break */ nullptr);
  NGFlexLayoutAlgorithm algorithm(params);
  algorithm.ignore_child_scrollbar_changes_ = true;
  return algorithm.Layout();
}

scoped_refptr<const NGLayoutResult> NGFlexLayoutAlgorithm::LayoutInternal() {
  // Freezing the scrollbars for the sub-tree shouldn't be strictly necessary,
  // but we do this just in case we trigger an unstable layout.
  absl::optional<PaintLayerScrollableArea::FreezeScrollbarsScope>
      freeze_scrollbars;
  if (ignore_child_scrollbar_changes_)
    freeze_scrollbars.emplace();

  PaintLayerScrollableArea::DelayScrollOffsetClampScope delay_clamp_scope;

  Vector<NGFlexLine> flex_line_outputs;
  bool use_empty_line_block_size;
  if (IsResumingLayout(BreakToken())) {
    auto& flex_data = BreakToken()->FlexData();
    total_intrinsic_block_size_ = flex_data.intrinsic_block_size;
    flex_line_outputs = flex_data.flex_lines;

    use_empty_line_block_size =
        flex_line_outputs.IsEmpty() && Node().HasLineIfEmpty();
  } else {
    PlaceFlexItems(&flex_line_outputs);

    use_empty_line_block_size =
        flex_line_outputs.IsEmpty() && Node().HasLineIfEmpty();
    CalculateTotalIntrinsicBlockSize(use_empty_line_block_size);
  }

  total_block_size_ = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), BorderPadding(), total_intrinsic_block_size_,
      container_builder_.InlineSize());

  if (!IsResumingLayout(BreakToken())) {
    ApplyFinalAlignmentAndReversals(&flex_line_outputs);
    NGLayoutResult::EStatus status =
        GiveItemsFinalPositionAndSize(&flex_line_outputs);
    if (status != NGLayoutResult::kSuccess)
      return container_builder_.Abort(status);
  }

  LayoutUnit previously_consumed_block_size;
  if (UNLIKELY(BreakToken()))
    previously_consumed_block_size = BreakToken()->ConsumedBlockSize();

  intrinsic_block_size_ = BorderScrollbarPadding().block_start;
  if (use_empty_line_block_size &&
      InvolvedInBlockFragmentation(container_builder_)) {
    intrinsic_block_size_ =
        (total_intrinsic_block_size_ - BorderScrollbarPadding().block_end -
         previously_consumed_block_size)
            .ClampNegativeToZero();
  }

  if (involved_in_block_fragmentation_) {
    NGLayoutResult::EStatus status =
        GiveItemsFinalPositionAndSizeForFragmentation(&flex_line_outputs);
    if (status != NGLayoutResult::kSuccess)
      return container_builder_.Abort(status);
  }

  LayoutUnit block_size;
  if (involved_in_block_fragmentation_ ||
      (use_empty_line_block_size &&
       InvolvedInBlockFragmentation(container_builder_))) {
    intrinsic_block_size_ = ClampIntrinsicBlockSize(
        ConstraintSpace(), Node(), BorderScrollbarPadding(),
        intrinsic_block_size_ + BorderScrollbarPadding().block_end);

    block_size = ComputeBlockSizeForFragment(
        ConstraintSpace(), Style(), BorderPadding(),
        previously_consumed_block_size + intrinsic_block_size_,
        container_builder_.InlineSize());
  } else {
    intrinsic_block_size_ = total_intrinsic_block_size_;
    block_size = total_block_size_;
  }

  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size_);
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  if (has_column_percent_flex_basis_)
    container_builder_.SetHasDescendantThatDependsOnPercentageBlockSize(true);

  if (UNLIKELY(InvolvedInBlockFragmentation(container_builder_))) {
    NGBreakStatus break_status = FinishFragmentation(
        Node(), ConstraintSpace(), BorderPadding().block_end,
        FragmentainerSpaceAtBfcStart(ConstraintSpace()), &container_builder_);
    if (break_status != NGBreakStatus::kContinue) {
      if (break_status == NGBreakStatus::kNeedsEarlierBreak)
        return container_builder_.Abort(NGLayoutResult::kNeedsEarlierBreak);
      DCHECK_EQ(break_status, NGBreakStatus::kDisableFragmentation);
      return container_builder_.Abort(NGLayoutResult::kDisableFragmentation);
    }
  } else {
#if DCHECK_IS_ON()
    // If we're not participating in a fragmentation context, no block
    // fragmentation related fields should have been set.
    container_builder_.CheckNoBlockFragmentation();
#endif
  }

#if DCHECK_IS_ON()
  if (!IsResumingLayout(BreakToken()))
    CheckFlexLines(flex_line_outputs);
#endif

  if (ConstraintSpace().HasBlockFragmentation()) {
    container_builder_.SetFlexBreakTokenData(
        std::make_unique<NGFlexBreakTokenData>(flex_line_outputs,
                                               total_intrinsic_block_size_));
  }

  // Un-freeze descendant scrollbars before we run the OOF layout part.
  freeze_scrollbars.reset();
  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();

  return container_builder_.ToBoxFragment();
}

void NGFlexLayoutAlgorithm::PlaceFlexItems(
    Vector<NGFlexLine>* flex_line_outputs) {
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

  flex_line_outputs->ReserveCapacity(algorithm_.NumItems());

  FlexLine* line;
  while ((
      line = algorithm_.ComputeNextFlexLine(container_builder_.InlineSize()))) {
    line->SetContainerMainInnerSize(
        MainAxisContentExtent(line->sum_hypothetical_main_size_));
    line->FreezeInflexibleItems();
    while (!line->ResolveFlexibleLengths()) {
      continue;
    }

    // TODO(almaher): How should devtools be handled for multiple fragments?
    if (UNLIKELY(layout_info_for_devtools_ && !IsResumingLayout(BreakToken())))
      layout_info_for_devtools_->lines.push_back(DevtoolsFlexInfo::Line());

    flex_line_outputs->push_back(NGFlexLine(line->line_items_.size()));
    for (wtf_size_t i = 0; i < line->line_items_.size(); ++i) {
      FlexItem& flex_item = line->line_items_[i];
      NGFlexItem& flex_item_output = flex_line_outputs->back().line_items[i];

      flex_item.offset_ = &flex_item_output.offset;
      flex_item_output.ng_input_node = flex_item.ng_input_node_;
      flex_item_output.main_axis_final_size = flex_item.FlexedBorderBoxSize();

      NGConstraintSpace child_space = BuildSpaceForLayout(
          flex_item.ng_input_node_, flex_item.FlexedBorderBoxSize());

      // We need to get the item's cross axis size given its new main size. If
      // the new main size is the item's inline size, then we have to do a
      // layout to get its new block size. But if the new main size is the
      // item's block size, we can skip layout in some cases and just calculate
      // the inline size from the constraint space.
      // Even when we only need inline size, we have to lay out the item if:
      //  * this is the item's last chance to layout (i.e. doesn't stretch), OR
      //  * the item has not yet been laid out. (ComputeLineItemsPosition
      //    relies on the fragment's baseline, which comes from the post-layout
      //    fragment)
      if (DoesItemStretch(flex_item.ng_input_node_) &&
          flex_item.layout_result_) {
        DCHECK(!MainAxisIsInlineAxis(flex_item.ng_input_node_));
        NGBoxStrut border =
            ComputeBorders(child_space, flex_item.ng_input_node_);
        NGBoxStrut padding =
            ComputePadding(child_space, flex_item.ng_input_node_.Style());
        if (flex_item.ng_input_node_.IsReplaced()) {
          LogicalSize logical_border_box_size = ComputeReplacedSize(
              flex_item.ng_input_node_, child_space, border + padding);
          flex_item.cross_axis_size_ = logical_border_box_size.inline_size;
        } else {
          flex_item.cross_axis_size_ = ComputeInlineSizeForFragment(
              child_space, flex_item.ng_input_node_, border + padding);
        }
      } else {
        DCHECK((child_space.CacheSlot() == NGCacheSlot::kLayout) ||
               !flex_item.layout_result_)
            << "If we already have a 'measure' result from "
               "ConstructAndAppendFlexItems, we don't want to evict it.";
        flex_item.layout_result_ = flex_item.ng_input_node_.Layout(
            child_space, nullptr /*break token*/);
        // TODO(layout-dev): Handle abortions caused by block fragmentation.
        DCHECK_EQ(flex_item.layout_result_->Status(), NGLayoutResult::kSuccess);
        flex_item.cross_axis_size_ =
            is_horizontal_flow_
                ? flex_item.layout_result_->PhysicalFragment().Size().height
                : flex_item.layout_result_->PhysicalFragment().Size().width;
      }
    }
    // cross_axis_offset is updated in each iteration of the loop, for passing
    // in to the next iteration.
    line->ComputeLineItemsPosition(main_axis_start_offset, main_axis_end_offset,
                                   cross_axis_offset);
    flex_line_outputs->back().line_cross_size = line->cross_axis_extent_;
  }
}

void NGFlexLayoutAlgorithm::CalculateTotalIntrinsicBlockSize(
    bool use_empty_line_block_size) {
  total_intrinsic_block_size_ = BorderScrollbarPadding().block_start;

  if (use_empty_line_block_size)
    total_intrinsic_block_size_ += Node().EmptyLineBlockSize(BreakToken());
  else
    total_intrinsic_block_size_ += algorithm_.IntrinsicContentBlockSize();

  total_intrinsic_block_size_ = ClampIntrinsicBlockSize(
      ConstraintSpace(), Node(), BorderScrollbarPadding(),
      total_intrinsic_block_size_ + BorderScrollbarPadding().block_end);
}

void NGFlexLayoutAlgorithm::ApplyFinalAlignmentAndReversals(
    Vector<NGFlexLine>* flex_line_outputs) {
  Vector<FlexLine>& line_contexts = algorithm_.FlexLines();
  const LayoutUnit cross_axis_start_edge =
      line_contexts.IsEmpty() ? LayoutUnit()
                              : line_contexts[0].cross_axis_offset_;

  LayoutUnit final_content_main_size =
      (container_builder_.InlineSize() - BorderScrollbarPadding().InlineSum())
          .ClampNegativeToZero();
  LayoutUnit final_content_cross_size =
      (total_block_size_ - BorderScrollbarPadding().BlockSum())
          .ClampNegativeToZero();
  if (is_column_)
    std::swap(final_content_main_size, final_content_cross_size);

  if (!algorithm_.IsMultiline() && !line_contexts.IsEmpty()) {
    line_contexts[0].cross_axis_extent_ = final_content_cross_size;
    (*flex_line_outputs)[0].line_cross_size = final_content_cross_size;
  }

  algorithm_.AlignFlexLines(final_content_cross_size, flex_line_outputs);

  algorithm_.AlignChildren();

  if (Style().FlexWrap() == EFlexWrap::kWrapReverse) {
    // flex-wrap: wrap-reverse reverses the order of the lines in the container;
    // FlipForWrapReverse recalculates each item's cross axis position. We have
    // to do that after AlignChildren sets an initial cross axis position.
    algorithm_.FlipForWrapReverse(cross_axis_start_edge,
                                  final_content_cross_size);
  }

  if (Style().ResolvedIsColumnReverseFlexDirection()) {
    algorithm_.LayoutColumnReverse(final_content_main_size,
                                   BorderScrollbarPadding().block_start);
  }
}

NGLayoutResult::EStatus NGFlexLayoutAlgorithm::GiveItemsFinalPositionAndSize(
    Vector<NGFlexLine>* flex_line_outputs) {
  DCHECK(!IsResumingLayout(BreakToken()));
  LayoutUnit final_content_cross_size;
  if (is_column_) {
    final_content_cross_size =
        (container_builder_.InlineSize() - BorderScrollbarPadding().InlineSum())
            .ClampNegativeToZero();
  } else {
    final_content_cross_size =
        (total_block_size_ - BorderScrollbarPadding().BlockSum())
            .ClampNegativeToZero();
  }

  absl::optional<LayoutUnit> fallback_baseline;
  NGLayoutResult::EStatus status = NGLayoutResult::kSuccess;
  for (wtf_size_t flex_line_idx = 0; flex_line_idx < flex_line_outputs->size();
       ++flex_line_idx) {
    NGFlexLine& line_output = (*flex_line_outputs)[flex_line_idx];
    for (wtf_size_t flex_item_idx = 0;
         flex_item_idx < line_output.line_items.size(); ++flex_item_idx) {
      NGFlexItem& flex_item = line_output.line_items[flex_item_idx];
      FlexItem* item = algorithm_.FlexItemAtIndex(flex_line_idx, flex_item_idx);

      LogicalOffset offset = flex_item.offset.ToLogicalOffset(is_column_);

      scoped_refptr<const NGLayoutResult> layout_result;
      if (DoesItemStretch(flex_item.ng_input_node)) {
        NGConstraintSpace child_space = BuildSpaceForLayout(
            flex_item.ng_input_node, flex_item.main_axis_final_size,
            line_output.line_cross_size);
        layout_result =
            flex_item.ng_input_node.Layout(child_space,
                                           /* break_token */ nullptr);
      } else {
        DCHECK(item);
        layout_result = item->layout_result_;
      }

      const auto& physical_fragment =
          To<NGPhysicalBoxFragment>(layout_result->PhysicalFragment());

      NGBoxFragment fragment(ConstraintSpace().GetWritingDirection(),
                             physical_fragment);
      if (!involved_in_block_fragmentation_) {
        container_builder_.AddResult(*layout_result, offset);

        // Only propagate baselines from children on the first flex-line.
        if (&line_output == flex_line_outputs->begin()) {
          PropagateBaselineFromChild(flex_item.Style(), fragment,
                                     offset.block_offset, &fallback_baseline);
        }
      } else {
        flex_item.total_remaining_block_size = fragment.BlockSize();
      }

      if (PropagateFlexItemInfo(item, flex_line_idx, offset,
                                physical_fragment.Size()) ==
          NGLayoutResult::kNeedsRelayoutWithNoChildScrollbarChanges) {
        status = NGLayoutResult::kNeedsRelayoutWithNoChildScrollbarChanges;
      }
    }
  }

  // Set the baseline to the fallback, if we didn't find any children with
  // baseline alignment.
  if (!involved_in_block_fragmentation_ && !container_builder_.Baseline() &&
      fallback_baseline)
    container_builder_.SetBaseline(*fallback_baseline);

  // TODO(crbug.com/1131352): Avoid control-specific handling.
  if (Node().IsButton()) {
    DCHECK(!involved_in_block_fragmentation_);
    AdjustButtonBaseline(final_content_cross_size);
  } else if (Node().IsSlider()) {
    DCHECK(!involved_in_block_fragmentation_);
    container_builder_.SetBaseline(BorderScrollbarPadding().BlockSum() +
                                   final_content_cross_size);
  }

  // Signal if we need to relayout with new child scrollbar information.
  return status;
}

NGLayoutResult::EStatus
NGFlexLayoutAlgorithm::GiveItemsFinalPositionAndSizeForFragmentation(
    Vector<NGFlexLine>* flex_line_outputs) {
  DCHECK(involved_in_block_fragmentation_);

  absl::optional<LayoutUnit> fallback_baseline;
  NGFlexItemIterator item_iterator(*flex_line_outputs, BreakToken());

  for (auto entry = item_iterator.NextItem();
       NGFlexItem* flex_item = entry.flex_item;
       entry = item_iterator.NextItem()) {
    wtf_size_t flex_item_idx = entry.flex_item_idx;
    wtf_size_t flex_line_idx = entry.flex_line_idx;
    NGFlexLine& line_output = (*flex_line_outputs)[flex_line_idx];
    const NGBreakToken* item_break_token = entry.token;

    const NGEarlyBreak* early_break_in_child = nullptr;
    if (UNLIKELY(early_break_)) {
      if (IsEarlyBreakTarget(*early_break_, container_builder_,
                             flex_item->ng_input_node)) {
        container_builder_.AddBreakBeforeChild(flex_item->ng_input_node,
                                               kBreakAppealPerfect,
                                               /* is_forced_break */ false);
        ConsumeRemainingFragmentainerSpace(line_output);
        return NGLayoutResult::kSuccess;
      } else {
        early_break_in_child =
            EnterEarlyBreakInChild(flex_item->ng_input_node, *early_break_);
      }
    }

    // A child break in a parallel flow doesn't affect whether we should
    // break here or not.
    // TODO(almaher): Once we add support for row break tokens, set
    // |has_inflow_child_break_inside_| to false when adding item break tokens.
    if (container_builder_.HasInflowChildBreakInside() &&
        (!is_horizontal_flow_ ||
         last_line_idx_to_process_first_child_ != flex_line_idx)) {
      // But if the break happened in the same flow, we'll now just finish
      // layout of the fragment. No more siblings should be processed.
      break;
    }

    LogicalOffset offset = flex_item->offset.ToLogicalOffset(is_column_);

    if (item_break_token) {
      offset.block_offset = LayoutUnit();
    } else if (IsResumingLayout(BreakToken())) {
      LayoutUnit updated_block_offset = offset.block_offset -
                                        BreakToken()->ConsumedBlockSize() +
                                        line_output.item_offset_adjustment;
      DCHECK_GE(updated_block_offset, LayoutUnit());
      offset.block_offset = updated_block_offset;
    }

    absl::optional<LayoutUnit> line_cross_size_for_stretch =
        DoesItemStretch(flex_item->ng_input_node)
            ? absl::optional<LayoutUnit>(line_output.line_cross_size)
            : absl::nullopt;
    const bool min_block_size_should_encompass_intrinsic_size =
        MinBlockSizeShouldEncompassIntrinsicSize(*flex_item);

    NGConstraintSpace child_space = BuildSpaceForLayout(
        flex_item->ng_input_node, flex_item->main_axis_final_size,
        line_cross_size_for_stretch, offset.block_offset,
        min_block_size_should_encompass_intrinsic_size);
    scoped_refptr<const NGLayoutResult> layout_result =
        flex_item->ng_input_node.Layout(child_space,
                                        To<NGBlockBreakToken>(item_break_token),
                                        early_break_in_child);

    // TODO(almaher): Special break behavior will be needed for row flex
    // containers.
    NGBreakStatus break_status = NGBreakStatus::kContinue;
    if (!early_break_ && ConstraintSpace().HasBlockFragmentation()) {
      bool has_container_separation = false;
      if (is_horizontal_flow_) {
        has_container_separation = has_processed_first_line_;
      } else {
        has_container_separation =
            last_line_idx_to_process_first_child_ == flex_line_idx;
      }
      break_status = BreakBeforeChildIfNeeded(
          ConstraintSpace(), flex_item->ng_input_node, *layout_result,
          ConstraintSpace().FragmentainerOffsetAtBfc() + offset.block_offset,
          has_container_separation, &container_builder_);
    }

    if (break_status == NGBreakStatus::kBrokeBefore) {
      ConsumeRemainingFragmentainerSpace(line_output);
      // If we broke before an item in a row container, make sure that all
      // items in that row have been processed before returning.
      if (!is_horizontal_flow_ ||
          flex_item_idx == line_output.line_items.size() - 1) {
        return NGLayoutResult::kSuccess;
      }
      last_line_idx_to_process_first_child_ = flex_line_idx;
      continue;
    }
    if (break_status == NGBreakStatus::kNeedsEarlierBreak) {
      return NGLayoutResult::kNeedsEarlierBreak;
    }

    const auto& physical_fragment =
        To<NGPhysicalBoxFragment>(layout_result->PhysicalFragment());

    NGBoxFragment fragment(ConstraintSpace().GetWritingDirection(),
                           physical_fragment);
    flex_item->total_remaining_block_size -= fragment.BlockSize();

    // This item may have expanded due to fragmentation. Record how large the
    // shift was (if any). Only do this if the item has completed layout.
    if (min_block_size_should_encompass_intrinsic_size &&
        !physical_fragment.BreakToken() &&
        flex_item->total_remaining_block_size < LayoutUnit()) {
      // TODO(almaher): Special logic will likely be needed for row
      // expansion.
      LayoutUnit expansion = -flex_item->total_remaining_block_size;
      line_output.item_offset_adjustment += expansion;
      total_intrinsic_block_size_ += expansion;
    }

    // TODO(almaher): What to do in the case where the line extends past
    // the last item? Should that be included when fragmenting?
    intrinsic_block_size_ +=
        (offset.block_offset + fragment.BlockSize() - intrinsic_block_size_)
            .ClampNegativeToZero();

    container_builder_.AddResult(*layout_result, offset);

    // Only propagate baselines from children on the first flex-line.
    if (&line_output == flex_line_outputs->begin()) {
      // TODO(almaher): How will this work with fragmentation?
      PropagateBaselineFromChild(flex_item->Style(), fragment,
                                 offset.block_offset, &fallback_baseline);
    }
    if (!has_processed_first_line_ &&
        flex_item_idx == line_output.line_items.size() - 1) {
      has_processed_first_line_ = true;
    }
    last_line_idx_to_process_first_child_ = flex_line_idx;
  }

  if (!container_builder_.HasInflowChildBreakInside() &&
      !item_iterator.NextItem().flex_item) {
    container_builder_.SetHasSeenAllChildren();
  }

  // Set the baseline to the fallback, if we didn't find any children with
  // baseline alignment.
  if (!container_builder_.Baseline() && fallback_baseline)
    container_builder_.SetBaseline(*fallback_baseline);

  return NGLayoutResult::kSuccess;
}

NGLayoutResult::EStatus NGFlexLayoutAlgorithm::PropagateFlexItemInfo(
    FlexItem* flex_item,
    wtf_size_t flex_line_idx,
    LogicalOffset offset,
    PhysicalSize fragment_size) {
  DCHECK(flex_item);
  NGLayoutResult::EStatus status = NGLayoutResult::kSuccess;

  // TODO(almaher): How should devtools be handled for multiple fragments?
  if (UNLIKELY(layout_info_for_devtools_)) {
    // If this is a "devtools layout", execution speed isn't critical but we
    // have to not adversely affect execution speed of a regular layout.
    PhysicalRect item_rect;
    item_rect.size = fragment_size;

    // TODO(almaher): Is using |total_block_size_| correct in the case of
    // fragmentation?
    LogicalSize logical_flexbox_size =
        LogicalSize(container_builder_.InlineSize(), total_block_size_);
    PhysicalSize flexbox_size = ToPhysicalSize(
        logical_flexbox_size, ConstraintSpace().GetWritingMode());
    item_rect.offset = offset.ConvertToPhysical(
        ConstraintSpace().GetWritingDirection(), flexbox_size, item_rect.size);
    // devtools uses margin box.
    item_rect.Expand(flex_item->physical_margins_);
    DCHECK_GE(layout_info_for_devtools_->lines.size(), 1u);
    DevtoolsFlexInfo::Item item;
    item.rect = item_rect;
    item.baseline = flex_item->MarginBoxAscent();
    layout_info_for_devtools_->lines[flex_line_idx].items.push_back(item);
  }

  flex_item->ng_input_node_.StoreMargins(flex_item->physical_margins_);

  // Detect if the flex-item had its scrollbar state change. If so we need
  // to relayout as the input to the flex algorithm is incorrect.
  if (!ignore_child_scrollbar_changes_) {
    if (flex_item->scrollbars_ !=
        ComputeScrollbarsForNonAnonymous(flex_item->ng_input_node_))
      status = NGLayoutResult::kNeedsRelayoutWithNoChildScrollbarChanges;

    // The flex-item scrollbars may not have changed, but an descendant's
    // scrollbars might have causing the min/max sizes to be incorrect.
    if (flex_item->depends_on_min_max_sizes_ &&
        flex_item->ng_input_node_.GetLayoutBox()->IntrinsicLogicalWidthsDirty())
      status = NGLayoutResult::kNeedsRelayoutWithNoChildScrollbarChanges;
  } else {
    DCHECK_EQ(flex_item->scrollbars_,
              ComputeScrollbarsForNonAnonymous(flex_item->ng_input_node_));
  }
  return status;
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
    absl::optional<LayoutUnit> baseline = layout_block->BaselineForEmptyLine(
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
  NGBoxFragment fragment(space.GetWritingDirection(),
                         To<NGPhysicalBoxFragment>(*child.fragment));
  absl::optional<LayoutUnit> child_baseline =
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
    const ComputedStyle& flex_item_style,
    const NGBoxFragment& fragment,
    LayoutUnit block_offset,
    absl::optional<LayoutUnit>* fallback_baseline) {
  // Check if we've already found an appropriate baseline.
  if (container_builder_.Baseline())
    return;

  const auto baseline_type = Style().GetFontBaseline();
  const LayoutUnit baseline_offset =
      block_offset + (Node().IsButton()
                          ? fragment.FirstBaselineOrSynthesize(baseline_type)
                          : fragment.BaselineOrSynthesize(baseline_type));

  // We prefer a baseline from a child with baseline alignment, and no
  // auto-margins in the cross axis (even if we have to synthesize the
  // baseline).
  if (FlexLayoutAlgorithm::AlignmentForChild(Style(), flex_item_style) ==
          ItemPosition::kBaseline &&
      !FlexItem::HasAutoMarginsInCrossAxis(flex_item_style, &algorithm_)) {
    container_builder_.SetBaseline(baseline_offset);
    return;
  }

  // Set the fallback baseline if it doesn't have a value yet.
  *fallback_baseline = fallback_baseline->value_or(baseline_offset);
}

MinMaxSizesResult NGFlexLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  if (auto result = CalculateMinMaxSizesIgnoringChildren(
          Node(), BorderScrollbarPadding()))
    return *result;

  MinMaxSizes sizes;
  bool depends_on_block_constraints = false;

  int number_of_items = 0;
  NGFlexChildIterator iterator(Node());
  for (NGBlockNode child = iterator.NextChild(); child;
       child = iterator.NextChild()) {
    if (child.IsOutOfFlowPositioned())
      continue;
    number_of_items++;

    NGMinMaxConstraintSpaceBuilder builder(ConstraintSpace(), Style(), child,
                                           /* is_new_fc */ true);
    builder.SetAvailableBlockSize(ChildAvailableSize().block_size);
    builder.SetPercentageResolutionBlockSize(child_percentage_size_.block_size);
    builder.SetReplacedPercentageResolutionBlockSize(
        child_percentage_size_.block_size);
    if (!is_column_ && WillChildCrossSizeBeContainerCrossSize(child))
      builder.SetBlockAutoBehavior(NGAutoBehavior::kStretchExplicit);
    const auto space = builder.ToConstraintSpace();

    MinMaxSizesResult child_result =
        ComputeMinAndMaxContentContribution(Style(), child, space);
    NGBoxStrut child_margins = ComputeMinMaxMargins(Style(), child);
    child_result.sizes += child_margins.InlineSum();

    depends_on_block_constraints |= child_result.depends_on_block_constraints;
    if (is_column_) {
      sizes.min_size = std::max(sizes.min_size, child_result.sizes.min_size);
      sizes.max_size = std::max(sizes.max_size, child_result.sizes.max_size);
    } else {
      sizes.max_size += child_result.sizes.max_size;
      if (algorithm_.IsMultiline()) {
        sizes.min_size = std::max(sizes.min_size, child_result.sizes.min_size);
      } else {
        sizes.min_size += child_result.sizes.min_size;
      }
    }
  }
  if (!is_column_) {
    LayoutUnit gap_inline_size =
        (number_of_items - 1) * algorithm_.gap_between_items_;
    sizes.max_size += gap_inline_size;
    if (!algorithm_.IsMultiline()) {
      sizes.min_size += gap_inline_size;
    }
  }
  sizes.max_size = std::max(sizes.max_size, sizes.min_size);

  // Due to negative margins, it is possible that we calculated a negative
  // intrinsic width. Make sure that we never return a negative width.
  sizes.Encompass(LayoutUnit());
  sizes += BorderScrollbarPadding().InlineSum();
  return MinMaxSizesResult(sizes, depends_on_block_constraints);
}

LayoutUnit NGFlexLayoutAlgorithm::FragmentainerSpaceAvailable() const {
  return (FragmentainerSpaceAtBfcStart(ConstraintSpace()) -
          intrinsic_block_size_)
      .ClampNegativeToZero();
}

void NGFlexLayoutAlgorithm::ConsumeRemainingFragmentainerSpace(
    NGFlexLine& flex_line) {
  if (ConstraintSpace().HasKnownFragmentainerBlockSize()) {
    // The remaining part of the fragmentainer (the unusable space for child
    // content, due to the break) should still be occupied by this container.
    LayoutUnit expansion = FragmentainerSpaceAvailable();
    intrinsic_block_size_ += expansion;
    total_intrinsic_block_size_ += expansion;
    flex_line.item_offset_adjustment += expansion;
  }
}

#if DCHECK_IS_ON()
void NGFlexLayoutAlgorithm::CheckFlexLines(
    const Vector<NGFlexLine>& flex_line_outputs) const {
  const Vector<FlexLine>& flex_lines = algorithm_.flex_lines_;

  DCHECK_EQ(flex_line_outputs.size(), flex_lines.size());
  for (wtf_size_t i = 0; i < flex_line_outputs.size(); i++) {
    const FlexLine& flex_line = flex_lines[i];
    const NGFlexLine& flex_line_output = flex_line_outputs[i];

    DCHECK_EQ(flex_line_output.line_cross_size, flex_line.cross_axis_extent_);
    DCHECK_EQ(flex_line_output.line_items.size(), flex_line.line_items_.size());

    for (wtf_size_t j = 0; j < flex_line_output.line_items.size(); j++) {
      const FlexItem& flex_item = flex_line.line_items_[j];
      const NGFlexItem& flex_item_output = flex_line_output.line_items[j];

      DCHECK_EQ(flex_item_output.offset, *flex_item.offset_);
      DCHECK_EQ(flex_item_output.ng_input_node, flex_item.ng_input_node_);
      DCHECK_EQ(flex_item_output.main_axis_final_size,
                flex_item.FlexedBorderBoxSize());
    }
  }
}
#endif

}  // namespace blink
