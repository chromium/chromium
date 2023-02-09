// Copyright 2018 The Chromium Authors
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
#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_break_token_data.h"
#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_child_iterator.h"
#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_data.h"
#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_item_iterator.h"
#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_line.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/ng_baseline_utils.h"
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
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

class BaselineAccumulator {
  STACK_ALLOCATED();

 public:
  explicit BaselineAccumulator(const ComputedStyle& style)
      : font_baseline_(style.GetFontBaseline()) {}

  void AccumulateItem(const NGBoxFragment& fragment,
                      const LayoutUnit block_offset,
                      bool is_first_line,
                      bool is_last_line) {
    if (is_first_line) {
      if (!first_fallback_baseline_) {
        first_fallback_baseline_ =
            block_offset + fragment.FirstBaselineOrSynthesize(font_baseline_);
      }
    }

    if (is_last_line) {
      last_fallback_baseline_ =
          block_offset + fragment.LastBaselineOrSynthesize(font_baseline_);
    }
  }

  void AccumulateLine(const NGFlexLine& line,
                      bool is_first_line,
                      bool is_last_line) {
    if (is_first_line) {
      if (line.major_baseline != LayoutUnit::Min()) {
        first_major_baseline_ = line.cross_axis_offset + line.major_baseline;
      }
      if (line.minor_baseline != LayoutUnit::Min()) {
        first_minor_baseline_ =
            line.cross_axis_offset + line.line_cross_size - line.minor_baseline;
      }
    }

    if (is_last_line) {
      if (line.major_baseline != LayoutUnit::Min()) {
        last_major_baseline_ = line.cross_axis_offset + line.major_baseline;
      }
      if (line.minor_baseline != LayoutUnit::Min()) {
        last_minor_baseline_ =
            line.cross_axis_offset + line.line_cross_size - line.minor_baseline;
      }
    }
  }

  absl::optional<LayoutUnit> FirstBaseline() const {
    if (first_major_baseline_)
      return *first_major_baseline_;
    if (first_minor_baseline_)
      return *first_minor_baseline_;
    return first_fallback_baseline_;
  }
  absl::optional<LayoutUnit> LastBaseline() const {
    if (last_minor_baseline_)
      return *last_minor_baseline_;
    if (last_major_baseline_)
      return *last_major_baseline_;
    return last_fallback_baseline_;
  }

 private:
  FontBaseline font_baseline_;

  absl::optional<LayoutUnit> first_major_baseline_;
  absl::optional<LayoutUnit> first_minor_baseline_;
  absl::optional<LayoutUnit> first_fallback_baseline_;

  absl::optional<LayoutUnit> last_major_baseline_;
  absl::optional<LayoutUnit> last_minor_baseline_;
  absl::optional<LayoutUnit> last_fallback_baseline_;
};

bool ContainsNonWhitespace(const LayoutBox* box) {
  const LayoutObject* next = box;
  while ((next = next->NextInPreOrder(box))) {
    if (const auto* text = DynamicTo<LayoutText>(next)) {
      if (!text->GetText().ContainsOnlyWhitespaceOrEmpty())
        return true;
    }
  }
  return false;
}

}  // anonymous namespace

NGFlexLayoutAlgorithm::NGFlexLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params,
    const HashMap<wtf_size_t, LayoutUnit>* cross_size_adjustments)
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
      cross_size_adjustments_(cross_size_adjustments) {
  // TODO(layout-dev): Devtools support when there are multiple fragments.
  if (Node().GetLayoutBox()->NeedsDevtoolsInfo() &&
      !InvolvedInBlockFragmentation(container_builder_))
    layout_info_for_devtools_ = std::make_unique<DevtoolsFlexInfo>();
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

  if (alignment == ItemPosition::kFlexEnd ||
      alignment == ItemPosition::kLastBaseline)
    return AxisEdge::kEnd;

  if (alignment == ItemPosition::kCenter)
    return AxisEdge::kCenter;

  return AxisEdge::kStart;
}

}  // namespace

void NGFlexLayoutAlgorithm::HandleOutOfFlowPositionedItems(
    HeapVector<Member<LayoutBox>>& oof_children) {
  if (oof_children.empty())
    return;

  HeapVector<Member<LayoutBox>> oofs;
  std::swap(oofs, oof_children);

  bool should_process_block_end = true;
  bool should_process_block_center = true;
  const LayoutUnit previous_consumed_block_size =
      BreakToken() ? BreakToken()->ConsumedBlockSize() : LayoutUnit();

  // We will attempt to add OOFs in the fragment in which their static
  // position belongs. However, the last fragment has the most up-to-date flex
  // size information (e.g. any expanded rows, etc), so for center aligned
  // items, we could end up with an incorrect static position.
  if (UNLIKELY(InvolvedInBlockFragmentation(container_builder_))) {
    should_process_block_end = !container_builder_.DidBreakSelf() &&
                               !container_builder_.ShouldBreakInside();
    if (should_process_block_end) {
      // Recompute the total block size in case |total_intrinsic_block_size_|
      // changed as a result of fragmentation.
      total_block_size_ = ComputeBlockSizeForFragment(
          ConstraintSpace(), Style(), BorderPadding(),
          total_intrinsic_block_size_, container_builder_.InlineSize());
    } else {
      LayoutUnit center = total_block_size_ / 2;
      should_process_block_center = center - previous_consumed_block_size <=
                                    FragmentainerCapacity(ConstraintSpace());
    }
  }

  using InlineEdge = NGLogicalStaticPosition::InlineEdge;
  using BlockEdge = NGLogicalStaticPosition::BlockEdge;

  NGBoxStrut border_scrollbar_padding = BorderScrollbarPadding();
  border_scrollbar_padding.block_start =
      OriginalBorderScrollbarPaddingBlockStart();

  LogicalSize total_fragment_size = {container_builder_.InlineSize(),
                                     total_block_size_};
  total_fragment_size =
      ShrinkLogicalSize(total_fragment_size, border_scrollbar_padding);

  for (LayoutBox* oof_child : oofs) {
    NGBlockNode child(oof_child);

    AxisEdge main_axis_edge = MainAxisStaticPositionEdge(Style(), is_column_);
    AxisEdge cross_axis_edge =
        CrossAxisStaticPositionEdge(Style(), child.Style());

    // This code block just collects UMA stats.
    if (!IsBreakInside(BreakToken())) {
      const auto& style = Style();
      const auto& child_style = child.Style();
      const PhysicalToLogical<Length> insets_in_flexbox_writing_mode(
          Style().GetWritingDirection(), child_style.Top(), child_style.Right(),
          child_style.Bottom(), child_style.Left());
      if (is_column_) {
        const ItemPosition normalized_alignment =
            FlexLayoutAlgorithm::AlignmentForChild(style, child_style);
        const ItemPosition default_justify_self_behavior =
            child.IsReplaced() ? ItemPosition::kStart : ItemPosition::kStretch;
        const ItemPosition normalized_justify =
            FlexLayoutAlgorithm::TranslateItemPosition(
                style, child_style,
                child_style.ResolvedJustifySelf(default_justify_self_behavior)
                    .GetPosition());

        const bool are_cross_axis_insets_auto =
            insets_in_flexbox_writing_mode.InlineStart().IsAuto() &&
            insets_in_flexbox_writing_mode.InlineEnd().IsAuto();

        if (normalized_alignment != normalized_justify &&
            are_cross_axis_insets_auto) {
          UseCounter::Count(Node().GetDocument(),
                            WebFeature::kFlexboxNewAbsPos);
        }
      }
      if (main_axis_edge != AxisEdge::kStart) {
        const bool are_main_axis_insets_auto =
            is_column_
                ? insets_in_flexbox_writing_mode.BlockStart().IsAuto() &&
                      insets_in_flexbox_writing_mode.BlockEnd().IsAuto()
                : insets_in_flexbox_writing_mode.InlineStart().IsAuto() &&
                      insets_in_flexbox_writing_mode.InlineEnd().IsAuto();
        if (are_main_axis_insets_auto) {
          UseCounter::Count(Node().GetDocument(),
                            WebFeature::kFlexboxAbsPosJustifyContent);
        }
      }
    }

    AxisEdge inline_axis_edge = is_column_ ? cross_axis_edge : main_axis_edge;
    AxisEdge block_axis_edge = is_column_ ? main_axis_edge : cross_axis_edge;

    InlineEdge inline_edge;
    BlockEdge block_edge;
    LogicalOffset offset = border_scrollbar_padding.StartOffset();

    // Determine the static-position based off the axis-edge.
    if (block_axis_edge == AxisEdge::kStart) {
      DCHECK(!IsBreakInside(BreakToken()));
      block_edge = BlockEdge::kBlockStart;
    } else if (block_axis_edge == AxisEdge::kCenter) {
      if (!should_process_block_center) {
        oof_children.emplace_back(oof_child);
        continue;
      }
      block_edge = BlockEdge::kBlockCenter;
      offset.block_offset += total_fragment_size.block_size / 2;
    } else {
      if (!should_process_block_end) {
        oof_children.emplace_back(oof_child);
        continue;
      }
      block_edge = BlockEdge::kBlockEnd;
      offset.block_offset += total_fragment_size.block_size;
    }

    if (inline_axis_edge == AxisEdge::kStart) {
      inline_edge = InlineEdge::kInlineStart;
    } else if (inline_axis_edge == AxisEdge::kCenter) {
      inline_edge = InlineEdge::kInlineCenter;
      offset.inline_offset += total_fragment_size.inline_size / 2;
    } else {
      inline_edge = InlineEdge::kInlineEnd;
      offset.inline_offset += total_fragment_size.inline_size;
    }

    // Make the child offset relative to our fragment.
    offset.block_offset -= previous_consumed_block_size;

    container_builder_.AddOutOfFlowChildCandidate(child, offset, inline_edge,
                                                  block_edge);
  }
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
  // Note: Unresolvable % cross size doesn't count as auto for stretchability.
  // As discussed in https://github.com/w3c/csswg-drafts/issues/4312.
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
  if (MainAxisIsInlineAxis(child))
    return !BlockLengthUnresolvable(BuildSpaceForFlexBasis(child), length);
  return !InlineLengthUnresolvable(BuildSpaceForFlexBasis(child), length);
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

NGConstraintSpace NGFlexLayoutAlgorithm::BuildSpaceForIntrinsicInlineSize(
    const NGBlockNode& child) const {
  NGMinMaxConstraintSpaceBuilder builder(ConstraintSpace(), Style(), child,
                                         /* is_new_fc */ true);
  builder.SetAvailableBlockSize(ChildAvailableSize().block_size);
  builder.SetPercentageResolutionBlockSize(child_percentage_size_.block_size);
  builder.SetReplacedPercentageResolutionBlockSize(
      child_percentage_size_.block_size);
  if (!is_column_ && WillChildCrossSizeBeContainerCrossSize(child))
    builder.SetBlockAutoBehavior(NGAutoBehavior::kStretchExplicit);
  return builder.ToConstraintSpace();
}

NGConstraintSpace NGFlexLayoutAlgorithm::BuildSpaceForIntrinsicBlockSize(
    const NGBlockNode& flex_item,
    absl::optional<LayoutUnit> override_inline_size) const {
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
  if (override_inline_size.has_value()) {
    LogicalSize available_size = ChildAvailableSize();
    available_size.inline_size = *override_inline_size;
    space_builder.SetIsFixedInlineSize(true);
    space_builder.SetAvailableSize(available_size);
  } else {
    space_builder.SetAvailableSize(ChildAvailableSize());
  }
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
    absl::optional<LayoutUnit> override_inline_size,
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

    if (override_inline_size) {
      DCHECK(!line_cross_size_for_stretch.has_value())
          << "We only override inline size when we are calculating intrinsic "
             "width of multiline column flexboxes, and we don't do any "
             "stretching during the intrinsic width calculation.";
      available_size.inline_size = *override_inline_size;
      space_builder.SetIsFixedInlineSize(true);
    }
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
    DCHECK(!override_inline_size.has_value());
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

void NGFlexLayoutAlgorithm::ConstructAndAppendFlexItems(
    Phase phase,
    HeapVector<Member<LayoutBox>>* oof_children) {
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

  bool is_wrap_reverse = Style().FlexWrap() == EFlexWrap::kWrapReverse;

  NGFlexChildIterator iterator(Node());
  for (NGBlockNode child = iterator.NextChild(); child;
       child = iterator.NextChild()) {
    if (child.IsOutOfFlowPositioned()) {
      if (phase == Phase::kLayout) {
        DCHECK(oof_children);
        oof_children->emplace_back(child.GetLayoutBox());
      }
      continue;
    }

    absl::optional<LayoutUnit> max_content_contribution;
    if (phase == Phase::kColumnWrapIntrinsicSize) {
      auto space = BuildSpaceForIntrinsicInlineSize(child);
      MinMaxSizesResult child_contributions =
          ComputeMinAndMaxContentContribution(Style(), child, space);
      max_content_contribution = child_contributions.sizes.max_size;
      NGBoxStrut child_margins =
          ComputeMarginsFor(space, child.Style(), ConstraintSpace());
      child_contributions.sizes += child_margins.InlineSum();

      largest_min_content_contribution_ =
          std::max(child_contributions.sizes.min_size,
                   largest_min_content_contribution_);
    }

    const ComputedStyle& child_style = child.Style();
    const auto child_writing_mode = child_style.GetWritingMode();

    if (is_alignment_behavior_change_possible &&
        all_items_match_container_alignment && phase == Phase::kLayout) {
      LayoutUnit item_offset = FlexItem::AlignmentOffset(
          kAvailableFreeSpace,
          FlexLayoutAlgorithm::AlignmentForChild(Style(), child_style),
          LayoutUnit(), /* is_wrap_reverse */ false,
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
        const auto child_space =
            BuildSpaceForIntrinsicBlockSize(child, max_content_contribution);
        min_max_sizes =
            child.ComputeMinMaxSizes(child_writing_mode, type, child_space);
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

    auto ComputeTransferredMainSize = [&]() -> LayoutUnit {
      DCHECK(!IsItemCrossAxisLengthDefinite(child, cross_axis_length) ||
             !WillChildCrossSizeBeContainerCrossSize(child))
          << "IsItemCrossAxisLengthDefinite and "
             "WillChildCrossSizeBeContainerCrossSize should be mutually "
             "exclusive.";
      LayoutUnit cross_size;
      Length cross_axis_length_to_resolve = Length::FitContent();
      if (IsItemCrossAxisLengthDefinite(child, cross_axis_length))
        cross_axis_length_to_resolve = cross_axis_length;
      else if (WillChildCrossSizeBeContainerCrossSize(child))
        cross_axis_length_to_resolve = Length::FillAvailable();
      if (MainAxisIsInlineAxis(child)) {
        cross_size = ResolveMainBlockLength(
            flex_basis_space, child_style, border_padding_in_child_writing_mode,
            cross_axis_length_to_resolve, kIndefiniteSize);
      } else {
        if (cross_axis_length_to_resolve.IsFitContent() &&
            flex_basis_space.AvailableSize().inline_size == kIndefiniteSize) {
          // TODO(dgrogan): Figure out if orthogonal items require a similar
          // branch in the MainAxisIsInlineAxis case just above this.
          DCHECK(phase == Phase::kColumnWrapIntrinsicSize);
          cross_size = *max_content_contribution;
        } else {
          cross_size = ResolveMainInlineLength(
              flex_basis_space, child_style,
              border_padding_in_child_writing_mode, MinMaxSizesFunc,
              cross_axis_length_to_resolve);
        }
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

    const NGLayoutResult* layout_result = nullptr;
    auto IntrinsicBlockSizeFunc =
        [&](MinMaxSizesType type = MinMaxSizesType::kIntrinsic) -> LayoutUnit {
      if (type == MinMaxSizesType::kContent && child.HasAspectRatio() &&
          !child.IsReplaced()) {
        // We don't enter here for replaced children because (a) this block
        // doesn't account for natural sizes so wouldn't work for replaced
        // elements, and (b) IntrinsicBlockSize() below already returns the
        // kContent block size for replaced elements.
        DCHECK(!AspectRatioProvidesMainSize(child))
            << "We only ever call IntrinsicBlockSizeFunc with kContent for "
               "determing flex base size in case E. If "
               "AspectRatioProvidesMainSize==true, we would have fallen into "
               "case B, not case E.";
        DCHECK(!MainAxisIsInlineAxis(child))
            << "We assume that the main axis is block axis in the call to "
               "BlockSum() below.";
        return AdjustMainSizeForAspectRatioCrossAxisMinAndMax(
            child, ComputeTransferredMainSize(),
            min_max_sizes_in_cross_axis_direction,
            border_padding_in_child_writing_mode);
      }
      if (!layout_result) {
        NGConstraintSpace child_space =
            BuildSpaceForIntrinsicBlockSize(child, max_content_contribution);
        layout_result = child.Layout(child_space, /* break_token */ nullptr);
        DCHECK(layout_result);
      }
      return layout_result->IntrinsicBlockSize();
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
        flex_base_border_box =
            IntrinsicBlockSizeFunc(MinMaxSizesType::kContent);
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
              BuildSpaceForIntrinsicBlockSize(*table_child,
                                              max_content_contribution));
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

      if (child.HasAspectRatio() && !MainAxisIsInlineAxis(child)) {
        content_size_suggestion =
            AdjustMainSizeForAspectRatioCrossAxisMinAndMax(
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
        if (!specified_length_in_main_axis.IsAuto() &&
            !InlineLengthUnresolvable(flex_basis_space,
                                      specified_length_in_main_axis)) {
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

    const NGBoxStrut scrollbars = ComputeScrollbarsForNonAnonymous(child);

    const auto container_writing_direction =
        ConstraintSpace().GetWritingDirection();
    bool is_last_baseline =
        FlexLayoutAlgorithm::AlignmentForChild(Style(), child_style) ==
        ItemPosition::kLastBaseline;
    const auto baseline_writing_mode = DetermineBaselineWritingMode(
        container_writing_direction, child_writing_mode,
        /* is_parallel_context */ !is_column_);
    const auto baseline_group = DetermineBaselineGroup(
        container_writing_direction, baseline_writing_mode,
        /* is_parallel_context */ !is_column_, is_last_baseline,
        /* is_flipped */ is_wrap_reverse);
    algorithm_
        .emplace_back(nullptr, child.Style(), flex_base_content_size,
                      min_max_sizes_in_main_axis_direction,
                      min_max_sizes_in_cross_axis_direction,
                      main_axis_border_padding, cross_axis_border_padding,
                      physical_child_margins, scrollbars, baseline_writing_mode,
                      baseline_group, min_max_sizes.has_value())
        .ng_input_node_ = child;
    // Save the layout result so that we can maybe reuse it later.
    if (layout_result) {
      DCHECK(!MainAxisIsInlineAxis(child));
      algorithm_.all_items_.back().layout_result_ = layout_result;
    }
    algorithm_.all_items_.back().max_content_contribution_ =
        max_content_contribution;
  }

  if (phase == Phase::kLayout && is_alignment_behavior_change_possible &&
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
NGFlexLayoutAlgorithm::AdjustMainSizeForAspectRatioCrossAxisMinAndMax(
    const NGBlockNode& child,
    LayoutUnit main_axis_size,
    const MinMaxSizes& cross_min_max,
    const NGBoxStrut& border_padding_in_child_writing_mode) {
  DCHECK(child.HasAspectRatio());
  auto transferred_min_max_func = MainAxisIsInlineAxis(child)
                                      ? ComputeTransferredMinMaxInlineSizes
                                      : ComputeTransferredMinMaxBlockSizes;
  auto min_max =
      transferred_min_max_func(child.GetAspectRatio(), cross_min_max,
                               border_padding_in_child_writing_mode,
                               child.Style().BoxSizingForAspectRatio());
  return min_max.ClampSizeToMinAndMax(main_axis_size);
}

const NGLayoutResult* NGFlexLayoutAlgorithm::Layout() {
  auto* result = LayoutInternal();
  switch (result->Status()) {
    case NGLayoutResult::kNeedsEarlierBreak:
      // If we found a good break somewhere inside this block, re-layout and
      // break at that location.
      return RelayoutAndBreakEarlierForFlex(result);
    case NGLayoutResult::kNeedsRelayoutWithNoChildScrollbarChanges:
      return RelayoutIgnoringChildScrollbarChanges();
    case NGLayoutResult::kDisableFragmentation:
      DCHECK(ConstraintSpace().HasBlockFragmentation());
      return RelayoutWithoutFragmentation<NGFlexLayoutAlgorithm>();
    case NGLayoutResult::kNeedsRelayoutWithRowCrossSizeChanges:
      return RelayoutWithNewRowSizes();
    default:
      return result;
  }
}

const NGLayoutResult*
NGFlexLayoutAlgorithm::RelayoutIgnoringChildScrollbarChanges() {
  DCHECK(!ignore_child_scrollbar_changes_);
  NGLayoutAlgorithmParams params(
      Node(), container_builder_.InitialFragmentGeometry(), ConstraintSpace(),
      BreakToken(), /* early_break */ nullptr);
  NGFlexLayoutAlgorithm algorithm(params);
  algorithm.ignore_child_scrollbar_changes_ = true;
  return algorithm.Layout();
}

const NGLayoutResult* NGFlexLayoutAlgorithm::RelayoutAndBreakEarlierForFlex(
    const NGLayoutResult* previous_result) {
  DCHECK(previous_result->GetEarlyBreak());
  NGLayoutAlgorithmParams params(
      Node(), container_builder_.InitialFragmentGeometry(), ConstraintSpace(),
      BreakToken(), previous_result->GetEarlyBreak(), &column_early_breaks_);
  NGFlexLayoutAlgorithm algorithm_with_break(params);
  algorithm_with_break.ignore_child_scrollbar_changes_ =
      ignore_child_scrollbar_changes_;
  return RelayoutAndBreakEarlier(&algorithm_with_break);
}

const NGLayoutResult* NGFlexLayoutAlgorithm::LayoutInternal() {
  // Freezing the scrollbars for the sub-tree shouldn't be strictly necessary,
  // but we do this just in case we trigger an unstable layout.
  absl::optional<PaintLayerScrollableArea::FreezeScrollbarsScope>
      freeze_scrollbars;
  if (ignore_child_scrollbar_changes_)
    freeze_scrollbars.emplace();

  PaintLayerScrollableArea::DelayScrollOffsetClampScope delay_clamp_scope;

  Vector<EBreakBetween> row_break_between_outputs;
  HeapVector<NGFlexLine> flex_line_outputs;
  HeapVector<Member<LayoutBox>> oof_children;
  bool broke_before_row = false;
  ClearCollectionScope<HeapVector<NGFlexLine>> scope(&flex_line_outputs);

  bool use_empty_line_block_size;
  if (IsBreakInside(BreakToken())) {
    const NGFlexBreakTokenData* flex_data =
        To<NGFlexBreakTokenData>(BreakToken()->TokenData());
    total_intrinsic_block_size_ = flex_data->intrinsic_block_size;
    flex_line_outputs = flex_data->flex_lines;
    row_break_between_outputs = flex_data->row_break_between;
    broke_before_row = flex_data->broke_before_row;
    oof_children = flex_data->oof_children;

    use_empty_line_block_size =
        flex_line_outputs.empty() && Node().HasLineIfEmpty();
  } else {
    PlaceFlexItems(&flex_line_outputs, &oof_children);

    use_empty_line_block_size =
        flex_line_outputs.empty() && Node().HasLineIfEmpty();
    CalculateTotalIntrinsicBlockSize(use_empty_line_block_size);
  }

  total_block_size_ = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), BorderPadding(), total_intrinsic_block_size_,
      container_builder_.InlineSize());

  if (!IsBreakInside(BreakToken())) {
    ApplyFinalAlignmentAndReversals(&flex_line_outputs);
    NGLayoutResult::EStatus status = GiveItemsFinalPositionAndSize(
        &flex_line_outputs, &row_break_between_outputs);
    if (status != NGLayoutResult::kSuccess)
      return container_builder_.Abort(status);
  }

  LayoutUnit previously_consumed_block_size;
  if (UNLIKELY(BreakToken()))
    previously_consumed_block_size = BreakToken()->ConsumedBlockSize();

  intrinsic_block_size_ = BorderScrollbarPadding().block_start;
  LayoutUnit block_size;
  if (UNLIKELY(InvolvedInBlockFragmentation(container_builder_))) {
    if (use_empty_line_block_size) {
      intrinsic_block_size_ =
          (total_intrinsic_block_size_ - BorderScrollbarPadding().block_end -
           previously_consumed_block_size)
              .ClampNegativeToZero();
    }

    NGLayoutResult::EStatus status =
        GiveItemsFinalPositionAndSizeForFragmentation(
            &flex_line_outputs, &row_break_between_outputs, &broke_before_row);
    if (status != NGLayoutResult::kSuccess)
      return container_builder_.Abort(status);

    intrinsic_block_size_ = ClampIntrinsicBlockSize(
        ConstraintSpace(), Node(), BreakToken(), BorderScrollbarPadding(),
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
  if (UNLIKELY(layout_info_for_devtools_)) {
    container_builder_.TransferFlexLayoutData(
        std::move(layout_info_for_devtools_));
  }

  if (UNLIKELY(InvolvedInBlockFragmentation(container_builder_))) {
    NGBreakStatus break_status = FinishFragmentation(
        Node(), ConstraintSpace(), BorderPadding().block_end,
        FragmentainerSpaceLeft(ConstraintSpace()), &container_builder_);
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

  HandleOutOfFlowPositionedItems(oof_children);

  // For rows, the break-before of the first row and the break-after of the
  // last row are propagated to the container. For columns, treat the set
  // of columns as a single row and propagate the combined break-before rules
  // for the first items in each column and break-after rules for last items in
  // each column.
  if (ConstraintSpace().ShouldPropagateChildBreakValues()) {
    DCHECK(!row_break_between_outputs.empty());
    container_builder_.SetInitialBreakBefore(row_break_between_outputs.front());
    container_builder_.SetPreviousBreakAfter(row_break_between_outputs.back());
  }

  if (ConstraintSpace().HasBlockFragmentation()) {
    container_builder_.SetBreakTokenData(
        MakeGarbageCollected<NGFlexBreakTokenData>(
            container_builder_.GetBreakTokenData(), flex_line_outputs,
            row_break_between_outputs, oof_children,
            total_intrinsic_block_size_, broke_before_row));
  }

#if DCHECK_IS_ON()
  if (!IsBreakInside(BreakToken()) && !cross_size_adjustments_)
    CheckFlexLines(flex_line_outputs);
#endif

  // Un-freeze descendant scrollbars before we run the OOF layout part.
  freeze_scrollbars.reset();
  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();

  return container_builder_.ToBoxFragment();
}

void NGFlexLayoutAlgorithm::PlaceFlexItems(
    HeapVector<NGFlexLine>* flex_line_outputs,
    HeapVector<Member<LayoutBox>>* oof_children,
    bool is_computing_multiline_column_intrinsic_size) {
  DCHECK(oof_children || is_computing_multiline_column_intrinsic_size);
  ConstructAndAppendFlexItems(is_computing_multiline_column_intrinsic_size
                                  ? Phase::kColumnWrapIntrinsicSize
                                  : Phase::kLayout,
                              oof_children);

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

  flex_line_outputs->reserve(algorithm_.NumItems());

  FlexLine* line;
  while ((
      line = algorithm_.ComputeNextFlexLine(container_builder_.InlineSize()))) {
    line->SetContainerMainInnerSize(
        MainAxisContentExtent(line->sum_hypothetical_main_size_));
    line->FreezeInflexibleItems();
    while (!line->ResolveFlexibleLengths()) {
      continue;
    }

    if (UNLIKELY(layout_info_for_devtools_))
      layout_info_for_devtools_->lines.push_back(DevtoolsFlexInfo::Line());

    flex_line_outputs->push_back(NGFlexLine(line->line_items_.size()));
    for (wtf_size_t i = 0; i < line->line_items_.size(); ++i) {
      FlexItem& flex_item = line->line_items_[i];
      NGFlexItem& flex_item_output = flex_line_outputs->back().line_items[i];

      flex_item.offset_ = &flex_item_output.offset;
      flex_item_output.ng_input_node = flex_item.ng_input_node_;
      flex_item_output.main_axis_final_size = flex_item.FlexedBorderBoxSize();

      NGConstraintSpace child_space = BuildSpaceForLayout(
          flex_item.ng_input_node_, flex_item.FlexedBorderBoxSize(),
          flex_item.max_content_contribution_);

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
    flex_line_outputs->back().cross_axis_offset = line->cross_axis_offset_;
    flex_line_outputs->back().major_baseline = line->max_major_ascent_;
    flex_line_outputs->back().minor_baseline = line->max_minor_ascent_;
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
      ConstraintSpace(), Node(), BreakToken(), BorderScrollbarPadding(),
      total_intrinsic_block_size_ + BorderScrollbarPadding().block_end);
}

void NGFlexLayoutAlgorithm::ApplyFinalAlignmentAndReversals(
    HeapVector<NGFlexLine>* flex_line_outputs) {
  auto& line_contexts = algorithm_.FlexLines();
  const LayoutUnit cross_axis_start_edge =
      line_contexts.empty() ? LayoutUnit()
                            : line_contexts[0].cross_axis_offset_;

  LayoutUnit final_content_main_size =
      (container_builder_.InlineSize() - BorderScrollbarPadding().InlineSum())
          .ClampNegativeToZero();
  LayoutUnit final_content_cross_size =
      (total_block_size_ - BorderScrollbarPadding().BlockSum())
          .ClampNegativeToZero();
  if (is_column_)
    std::swap(final_content_main_size, final_content_cross_size);

  if (!algorithm_.IsMultiline() && !line_contexts.empty()) {
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
                                  final_content_cross_size, flex_line_outputs);
    flex_line_outputs->Reverse();
  }

  if (Style().ResolvedIsColumnReverseFlexDirection()) {
    algorithm_.LayoutColumnReverse(final_content_main_size,
                                   BorderScrollbarPadding().block_start);
  }
  if (Style().ResolvedIsColumnReverseFlexDirection() ||
      Style().ResolvedIsRowReverseFlexDirection()) {
    for (auto& flex_line : *flex_line_outputs)
      flex_line.line_items.Reverse();
  }
}

NGLayoutResult::EStatus NGFlexLayoutAlgorithm::GiveItemsFinalPositionAndSize(
    HeapVector<NGFlexLine>* flex_line_outputs,
    Vector<EBreakBetween>* row_break_between_outputs) {
  DCHECK(!IsBreakInside(BreakToken()));
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

  bool should_propagate_row_break_values =
      ConstraintSpace().ShouldPropagateChildBreakValues();
  if (should_propagate_row_break_values) {
    DCHECK(row_break_between_outputs);
    // The last row break between will store the final break-after to be
    // propagated to the container.
    if (!is_column_) {
      *row_break_between_outputs = Vector<EBreakBetween>(
          flex_line_outputs->size() + 1, EBreakBetween::kAuto);
    } else {
      // For flex columns, we only need to store two values - one for
      // the break-before value of all combined columns, and the second for
      // for the break-after values for all combined columns.
      *row_break_between_outputs =
          Vector<EBreakBetween>(2, EBreakBetween::kAuto);
    }
  }

  BaselineAccumulator baseline_accumulator(Style());
  NGLayoutResult::EStatus status = NGLayoutResult::kSuccess;

  for (wtf_size_t flex_line_idx = 0; flex_line_idx < flex_line_outputs->size();
       ++flex_line_idx) {
    NGFlexLine& line_output = (*flex_line_outputs)[flex_line_idx];

    bool is_first_line = flex_line_idx == 0;
    bool is_last_line = flex_line_idx == flex_line_outputs->size() - 1;
    if (!InvolvedInBlockFragmentation(container_builder_) && !is_column_) {
      baseline_accumulator.AccumulateLine(line_output, is_first_line,
                                          is_last_line);
    }

    for (wtf_size_t flex_item_idx = 0;
         flex_item_idx < line_output.line_items.size(); ++flex_item_idx) {
      NGFlexItem& flex_item = line_output.line_items[flex_item_idx];
      FlexItem* item = algorithm_.FlexItemAtIndex(flex_line_idx, flex_item_idx);

      LogicalOffset offset = flex_item.offset.ToLogicalOffset(is_column_);

      const NGLayoutResult* layout_result = nullptr;
      if (DoesItemStretch(flex_item.ng_input_node)) {
        NGConstraintSpace child_space = BuildSpaceForLayout(
            flex_item.ng_input_node, flex_item.main_axis_final_size,
            /* override_inline_size */ absl::nullopt,
            line_output.line_cross_size);
        layout_result =
            flex_item.ng_input_node.Layout(child_space,
                                           /* break_token */ nullptr);
      } else {
        DCHECK(item);
        layout_result = item->layout_result_;
      }

      flex_item.has_descendant_that_depends_on_percentage_block_size =
          layout_result->HasDescendantThatDependsOnPercentageBlockSize();
      flex_item.margin_block_end = item->MarginBlockEnd();

      if (should_propagate_row_break_values) {
        const auto& item_style = flex_item.Style();
        auto item_break_before = JoinFragmentainerBreakValues(
            item_style.BreakBefore(), layout_result->InitialBreakBefore());
        auto item_break_after = JoinFragmentainerBreakValues(
            item_style.BreakAfter(), layout_result->FinalBreakAfter());

        // The break-before and break-after values of flex items in a flex row
        // are propagated to the row itself. Accumulate the BreakBetween values
        // for each row ahead of time so that they can be stored on the break
        // token for future use.
        //
        // https://drafts.csswg.org/css-flexbox-1/#pagination
        if (!is_column_) {
          (*row_break_between_outputs)[flex_line_idx] =
              JoinFragmentainerBreakValues(
                  (*row_break_between_outputs)[flex_line_idx],
                  item_break_before);
          (*row_break_between_outputs)[flex_line_idx + 1] =
              JoinFragmentainerBreakValues(
                  (*row_break_between_outputs)[flex_line_idx + 1],
                  item_break_after);
        } else {
          // Treat all columns as a "row" of columns, and accumulate the initial
          // and final break values for all columns, which will be propagated to
          // the container.
          if (flex_item_idx == 0) {
            (*row_break_between_outputs)[0] = JoinFragmentainerBreakValues(
                (*row_break_between_outputs)[0], item_break_before);
          }
          if (flex_item_idx == line_output.line_items.size() - 1) {
            (*row_break_between_outputs)[1] = JoinFragmentainerBreakValues(
                (*row_break_between_outputs)[1], item_break_after);
          }
        }
      }

      const auto& physical_fragment =
          To<NGPhysicalBoxFragment>(layout_result->PhysicalFragment());

      NGBoxFragment fragment(ConstraintSpace().GetWritingDirection(),
                             physical_fragment);
      if (!InvolvedInBlockFragmentation(container_builder_)) {
        container_builder_.AddResult(*layout_result, offset);
        baseline_accumulator.AccumulateItem(fragment, offset.block_offset,
                                            is_first_line, is_last_line);
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

  if (auto first_baseline = baseline_accumulator.FirstBaseline())
    container_builder_.SetFirstBaseline(*first_baseline);
  if (auto last_baseline = baseline_accumulator.LastBaseline())
    container_builder_.SetLastBaseline(*last_baseline);

  // TODO(crbug.com/1131352): Avoid control-specific handling.
  if (Node().IsButton()) {
    DCHECK(!InvolvedInBlockFragmentation(container_builder_));
    AdjustButtonBaseline(final_content_cross_size);
  } else if (Node().IsSlider()) {
    DCHECK(!InvolvedInBlockFragmentation(container_builder_));
    container_builder_.SetBaselines(BorderScrollbarPadding().BlockSum() +
                                    final_content_cross_size);
  }

  // Signal if we need to relayout with new child scrollbar information.
  return status;
}

NGLayoutResult::EStatus
NGFlexLayoutAlgorithm::GiveItemsFinalPositionAndSizeForFragmentation(
    HeapVector<NGFlexLine>* flex_line_outputs,
    Vector<EBreakBetween>* row_break_between_outputs,
    bool* broke_before_row) {
  DCHECK(InvolvedInBlockFragmentation(container_builder_));
  DCHECK(flex_line_outputs);
  DCHECK(row_break_between_outputs);
  DCHECK(broke_before_row);

  NGFlexItemIterator item_iterator(*flex_line_outputs, BreakToken(),
                                   is_column_);

  Vector<bool> has_inflow_child_break_inside_line(flex_line_outputs->size(),
                                                  false);
  bool needs_earlier_break_in_column = false;
  NGLayoutResult::EStatus status = NGLayoutResult::kSuccess;
  LayoutUnit fragmentainer_space = FragmentainerSpaceLeft(ConstraintSpace());

  HeapVector<NGFlexColumnBreakInfo> column_break_info;
  if (is_column_) {
    column_break_info =
        HeapVector<NGFlexColumnBreakInfo>(flex_line_outputs->size());
  }

  LayoutUnit previously_consumed_block_size;
  if (BreakToken())
    previously_consumed_block_size = BreakToken()->ConsumedBlockSize();

  BaselineAccumulator baseline_accumulator(Style());
  for (auto entry = item_iterator.NextItem(*broke_before_row);
       NGFlexItem* flex_item = entry.flex_item;
       entry = item_iterator.NextItem(*broke_before_row)) {
    wtf_size_t flex_item_idx = entry.flex_item_idx;
    wtf_size_t flex_line_idx = entry.flex_line_idx;
    NGFlexLine& line_output = (*flex_line_outputs)[flex_line_idx];
    const auto* item_break_token = To<NGBlockBreakToken>(entry.token);
    bool last_item_in_line = flex_item_idx == line_output.line_items.size() - 1;

    bool is_first_line = flex_line_idx == 0;
    bool is_last_line = flex_line_idx == flex_line_outputs->size() - 1;

    // A child break in a parallel flow doesn't affect whether we should
    // break here or not. But if the break happened in the same flow, we'll now
    // just finish layout of the fragment. No more siblings should be processed.
    if (!is_column_) {
      if (flex_line_idx != 0 &&
          has_inflow_child_break_inside_line[flex_line_idx - 1])
        break;
    } else {
      // If we are relaying out as a result of an early break, and we have early
      // breaks for more than one column, they will be stored in
      // |additional_early_breaks_|. Keep |early_break_| consistent with that of
      // the current column.
      if (additional_early_breaks_ &&
          flex_line_idx < additional_early_breaks_->size())
        early_break_ = (*additional_early_breaks_)[flex_line_idx];
      else if (early_break_ && flex_line_idx != 0)
        early_break_ = nullptr;

      if (has_inflow_child_break_inside_line[flex_line_idx]) {
        if (!last_item_in_line)
          item_iterator.NextLine();
        continue;
      }
    }

    LayoutUnit row_block_offset =
        !is_column_ ? line_output.cross_axis_offset : LayoutUnit();
    LogicalOffset original_offset =
        flex_item->offset.ToLogicalOffset(is_column_);
    LogicalOffset offset = original_offset;

    // If a row or item broke before, subsequent items and lines need to be
    // adjusted by the expansion amount.
    LayoutUnit individual_item_adjustment;
    if (item_break_token && item_break_token->IsBreakBefore()) {
      if (item_break_token->IsForcedBreak()) {
        // We had previously updated the adjustment to subtract out the total
        // consumed block size up to the break. Now add the total consumed
        // block size in the previous fragmentainer to get the total amount
        // the item or row expanded by. This allows for things like margins
        // and alignment offsets to not get sliced by a forced break.
        line_output.item_offset_adjustment += previously_consumed_block_size;
      } else if (!is_column_ && flex_item_idx == 0 && *broke_before_row) {
        LayoutUnit total_row_block_offset =
            row_block_offset + line_output.item_offset_adjustment;
        line_output.item_offset_adjustment +=
            previously_consumed_block_size - total_row_block_offset;
      } else {
        LayoutUnit total_item_block_offset =
            offset.block_offset + line_output.item_offset_adjustment;
        individual_item_adjustment =
            (previously_consumed_block_size - total_item_block_offset)
                .ClampNegativeToZero();
        // For items in a row, the offset adjustment due to a break before
        // should only apply to the item itself and not to the entire row.
        if (is_column_) {
          line_output.item_offset_adjustment += individual_item_adjustment;
        }
      }
    }

    if (IsBreakInside(item_break_token)) {
      offset.block_offset = LayoutUnit();
    } else if (IsBreakInside(BreakToken())) {
      LayoutUnit offset_adjustment =
          previously_consumed_block_size - line_output.item_offset_adjustment;
      offset.block_offset -= offset_adjustment;
      if (!is_column_) {
        offset.block_offset += individual_item_adjustment;
        row_block_offset -= offset_adjustment;
      }
    }

    const NGEarlyBreak* early_break_in_child = nullptr;
    if (UNLIKELY(early_break_)) {
      if (!is_column_)
        container_builder_.SetLineCount(flex_line_idx);
      if (IsEarlyBreakTarget(*early_break_, container_builder_,
                             flex_item->ng_input_node)) {
        container_builder_.AddBreakBeforeChild(flex_item->ng_input_node,
                                               kBreakAppealPerfect,
                                               /* is_forced_break */ false);
        if (early_break_->Type() == NGEarlyBreak::kLine)
          *broke_before_row = true;
        ConsumeRemainingFragmentainerSpace(previously_consumed_block_size,
                                           &line_output);
        // For column flex containers, continue to the next column. For rows,
        // continue until we've processed all items in the current row.
        has_inflow_child_break_inside_line[flex_line_idx] = true;
        if (is_column_) {
          if (!last_item_in_line)
            item_iterator.NextLine();
        } else if (last_item_in_line) {
          DCHECK_EQ(status, NGLayoutResult::kSuccess);
          break;
        }
        last_line_idx_to_process_first_child_ = flex_line_idx;
        continue;
      } else {
        early_break_in_child =
            EnterEarlyBreakInChild(flex_item->ng_input_node, *early_break_);
      }
    }

    // If we are re-laying out one or more rows with an updated cross-size,
    // adjust the row info to reflect this change (but only if this is the first
    // time we are processing the current row in this layout pass).
    if (UNLIKELY(cross_size_adjustments_)) {
      DCHECK(!is_column_);
      // Maps don't allow keys of 0, so adjust the index by 1.
      if (cross_size_adjustments_->Contains(flex_line_idx + 1) &&
          (last_line_idx_to_process_first_child_ == kNotFound ||
           last_line_idx_to_process_first_child_ < flex_line_idx)) {
        LayoutUnit row_block_size_adjustment =
            cross_size_adjustments_->find(flex_line_idx + 1)->value;
        line_output.line_cross_size += row_block_size_adjustment;

        // Adjust any subsequent row offsets to reflect the current row's new
        // size.
        AdjustOffsetForNextLine(flex_line_outputs, flex_line_idx,
                                row_block_size_adjustment);
      }
    }

    absl::optional<LayoutUnit> line_cross_size_for_stretch =
        DoesItemStretch(flex_item->ng_input_node)
            ? absl::optional<LayoutUnit>(line_output.line_cross_size)
            : absl::nullopt;

    // If an item broke, its offset may have expanded (as the result of a
    // current or previous break before), in which case, we shouldn't expand by
    // the total line cross size. Otherwise, we would continue to expand the row
    // past the block-size of its items.
    if (line_cross_size_for_stretch && !is_column_ && item_break_token) {
      LayoutUnit updated_cross_size_for_stretch =
          line_cross_size_for_stretch.value();
      updated_cross_size_for_stretch -=
          previously_consumed_block_size -
          (original_offset.block_offset + line_output.item_offset_adjustment) -
          item_break_token->ConsumedBlockSize();
      line_cross_size_for_stretch = updated_cross_size_for_stretch;
    }

    const bool min_block_size_should_encompass_intrinsic_size =
        MinBlockSizeShouldEncompassIntrinsicSize(*flex_item);
    NGConstraintSpace child_space = BuildSpaceForLayout(
        flex_item->ng_input_node, flex_item->main_axis_final_size,
        /* override_inline_size */ absl::nullopt, line_cross_size_for_stretch,
        offset.block_offset, min_block_size_should_encompass_intrinsic_size);
    const NGLayoutResult* layout_result = flex_item->ng_input_node.Layout(
        child_space, item_break_token, early_break_in_child);

    NGBreakStatus break_status = NGBreakStatus::kContinue;
    NGFlexColumnBreakInfo* current_column_break_info = nullptr;
    if (!early_break_ && ConstraintSpace().HasBlockFragmentation()) {
      bool has_container_separation = false;
      if (!is_column_) {
        has_container_separation =
            offset.block_offset > row_block_offset &&
            (!item_break_token || (*broke_before_row && flex_item_idx == 0 &&
                                   item_break_token->IsBreakBefore()));
        // Don't attempt to break before a row if the fist item is resuming
        // layout. In which case, the row should be resuming layout, as well.
        if (flex_item_idx == 0 &&
            (!item_break_token ||
             (item_break_token->IsBreakBefore() && *broke_before_row))) {
          // Rows have no layout result, so if the row breaks before, we
          // will break before the first item in the row instead.
          bool row_container_separation = has_processed_first_line_;
          bool is_first_for_row = !item_break_token || *broke_before_row;
          NGBreakStatus row_break_status = BreakBeforeRowIfNeeded(
              line_output, (*row_break_between_outputs)[flex_line_idx],
              flex_line_idx, flex_item->ng_input_node, row_container_separation,
              is_first_for_row);
          if (row_break_status == NGBreakStatus::kBrokeBefore) {
            ConsumeRemainingFragmentainerSpace(previously_consumed_block_size,
                                               &line_output);
            *broke_before_row = true;
            DCHECK_EQ(status, NGLayoutResult::kSuccess);
            break;
          }
          *broke_before_row = false;
          if (row_break_status == NGBreakStatus::kNeedsEarlierBreak) {
            status = NGLayoutResult::kNeedsEarlierBreak;
            break;
          }
          DCHECK_EQ(row_break_status, NGBreakStatus::kContinue);
        }
      } else {
        has_container_separation =
            !item_break_token &&
            ((last_line_idx_to_process_first_child_ != kNotFound &&
              last_line_idx_to_process_first_child_ >= flex_line_idx) ||
             offset.block_offset > LayoutUnit());

        // We may switch back and forth between columns, so we need to make sure
        // to use the break-after for the current column.
        if (flex_line_outputs->size() > 1) {
          current_column_break_info = &column_break_info[flex_line_idx];
          container_builder_.SetPreviousBreakAfter(
              current_column_break_info->break_after);
        }
      }
      break_status = BreakBeforeChildIfNeeded(
          ConstraintSpace(), flex_item->ng_input_node, *layout_result,
          ConstraintSpace().FragmentainerOffset() + offset.block_offset,
          has_container_separation, &container_builder_, !is_column_,
          current_column_break_info);
    }

    if (break_status == NGBreakStatus::kNeedsEarlierBreak) {
      if (current_column_break_info) {
        DCHECK(is_column_);
        DCHECK(current_column_break_info->early_break);
        if (!needs_earlier_break_in_column) {
          needs_earlier_break_in_column = true;
          container_builder_.SetEarlyBreak(
              current_column_break_info->early_break);
        }
        // Keep track of the early breaks for each column.
        AddColumnEarlyBreak(current_column_break_info->early_break,
                            flex_line_idx);
        if (!last_item_in_line)
          item_iterator.NextLine();
        continue;
      }
      status = NGLayoutResult::kNeedsEarlierBreak;
      break;
    }

    if (break_status == NGBreakStatus::kBrokeBefore) {
      ConsumeRemainingFragmentainerSpace(previously_consumed_block_size,
                                         &line_output,
                                         current_column_break_info);
      // For column flex containers, continue to the next column. For rows,
      // continue until we've processed all items in the current row.
      has_inflow_child_break_inside_line[flex_line_idx] = true;
      if (is_column_) {
        if (!last_item_in_line)
          item_iterator.NextLine();
      } else if (last_item_in_line) {
        DCHECK_EQ(status, NGLayoutResult::kSuccess);
        break;
      }
      last_line_idx_to_process_first_child_ = flex_line_idx;
      continue;
    }

    const auto& physical_fragment =
        To<NGPhysicalBoxFragment>(layout_result->PhysicalFragment());

    NGBoxFragment fragment(ConstraintSpace().GetWritingDirection(),
                           physical_fragment);

    bool is_at_block_end = !physical_fragment.BreakToken() ||
                           physical_fragment.BreakToken()->IsAtBlockEnd();
    LayoutUnit item_block_end = offset.block_offset + fragment.BlockSize();
    if (is_at_block_end) {
      // Only add the block-end margin if the item has reached the end of its
      // content. Then re-set it to avoid adding it more than once.
      item_block_end += flex_item->margin_block_end;
      flex_item->margin_block_end = LayoutUnit();
    } else {
      has_inflow_child_break_inside_line[flex_line_idx] = true;
    }

    // This item may have expanded due to fragmentation. Record how large the
    // shift was (if any). Only do this if the item has completed layout.
    if (is_column_) {
      flex_item->total_remaining_block_size -= fragment.BlockSize();
      if (flex_item->total_remaining_block_size < LayoutUnit() &&
          !physical_fragment.BreakToken()) {
        LayoutUnit expansion = -flex_item->total_remaining_block_size;
        line_output.item_offset_adjustment += expansion;
      }
    } else if (!cross_size_adjustments_ &&
               !flex_item
                    ->has_descendant_that_depends_on_percentage_block_size) {
      // For rows, keep track of any expansion past the block-end of each
      // row so that we can re-run layout with the new row block-size.
      LayoutUnit line_block_end =
          line_output.LineCrossEnd() - previously_consumed_block_size;
      if (line_block_end <= fragmentainer_space &&
          line_block_end >= LayoutUnit() &&
          previously_consumed_block_size != LayoutUnit::Max()) {
        LayoutUnit item_expansion;
        if (is_at_block_end) {
          item_expansion = item_block_end - line_block_end;
        } else {
          // We can't use the size of the fragment, as we don't
          // know how large the subsequent fragments will be (and how much
          // they'll expand the row).
          //
          // Instead of using the size of the fragment, expand the row to the
          // rest of the fragmentainer, with an additional epsilon. This epsilon
          // will ensure that we continue layout for children in this row in
          // the next fragmentainer. Without it we'd drop those subsequent
          // fragments.
          item_expansion = (fragmentainer_space - line_block_end).AddEpsilon();
        }

        // If the item expanded past the row, adjust any subsequent row offsets
        // to reflect the expansion.
        if (item_expansion > LayoutUnit()) {
          // Maps don't allow keys of 0, so adjust the index by 1.
          if (row_cross_size_updates_.empty() ||
              !row_cross_size_updates_.Contains(flex_line_idx + 1)) {
            row_cross_size_updates_.insert(flex_line_idx + 1, item_expansion);
            AdjustOffsetForNextLine(flex_line_outputs, flex_line_idx,
                                    item_expansion);
          } else {
            auto it = row_cross_size_updates_.find(flex_line_idx + 1);
            DCHECK_NE(it, row_cross_size_updates_.end());
            if (item_expansion > it->value) {
              AdjustOffsetForNextLine(flex_line_outputs, flex_line_idx,
                                      item_expansion - it->value);
              it->value = item_expansion;
            }
          }
        }
      }
    }

    if (current_column_break_info) {
      DCHECK(is_column_);
      current_column_break_info->column_intrinsic_block_size =
          std::max(item_block_end,
                   current_column_break_info->column_intrinsic_block_size);
    }

    intrinsic_block_size_ = std::max(item_block_end, intrinsic_block_size_);
    container_builder_.AddResult(*layout_result, offset,
                                 /* relative_offset */ absl::nullopt,
                                 /* inline_container */ nullptr,
                                 current_column_break_info
                                     ? &current_column_break_info->break_after
                                     : nullptr);
    baseline_accumulator.AccumulateItem(fragment, offset.block_offset,
                                        is_first_line, is_last_line);
    if (last_item_in_line) {
      if (!has_inflow_child_break_inside_line[flex_line_idx])
        line_output.has_seen_all_children = true;
      if (!has_processed_first_line_)
        has_processed_first_line_ = true;

      if (!physical_fragment.BreakToken() ||
          line_output.has_seen_all_children) {
        if (flex_line_idx < flex_line_outputs->size() - 1 && !is_column_ &&
            !item_iterator.HasMoreBreakTokens()) {
          // Add the offset adjustment of the current row to the next row so
          // that its items can also be adjusted by previous item expansion.
          // Only do this when the current row has completed layout and
          // the next row hasn't started layout yet.
          (*flex_line_outputs)[flex_line_idx + 1].item_offset_adjustment +=
              line_output.item_offset_adjustment;
        }
      }
    }
    last_line_idx_to_process_first_child_ = flex_line_idx;
  }

  if (needs_earlier_break_in_column ||
      status == NGLayoutResult::kNeedsEarlierBreak)
    return NGLayoutResult::kNeedsEarlierBreak;

  if (!row_cross_size_updates_.empty()) {
    DCHECK(!is_column_);
    return NGLayoutResult::kNeedsRelayoutWithRowCrossSizeChanges;
  }

  if (!container_builder_.HasInflowChildBreakInside() &&
      !item_iterator.NextItem(*broke_before_row).flex_item) {
    container_builder_.SetHasSeenAllChildren();
  }

  if (auto first_baseline = baseline_accumulator.FirstBaseline())
    container_builder_.SetFirstBaseline(*first_baseline);
  if (auto last_baseline = baseline_accumulator.LastBaseline())
    container_builder_.SetLastBaseline(*last_baseline);

  // Update the |total_intrinsic_block_size_| in case things expanded.
  total_intrinsic_block_size_ =
      std::max(total_intrinsic_block_size_,
               intrinsic_block_size_ + previously_consumed_block_size);

  return status;
}

NGLayoutResult::EStatus NGFlexLayoutAlgorithm::PropagateFlexItemInfo(
    FlexItem* flex_item,
    wtf_size_t flex_line_idx,
    LogicalOffset offset,
    PhysicalSize fragment_size) {
  DCHECK(flex_item);
  NGLayoutResult::EStatus status = NGLayoutResult::kSuccess;

  if (UNLIKELY(layout_info_for_devtools_)) {
    // If this is a "devtools layout", execution speed isn't critical but we
    // have to not adversely affect execution speed of a regular layout.
    PhysicalRect item_rect;
    item_rect.size = fragment_size;

    LogicalSize logical_flexbox_size =
        LogicalSize(container_builder_.InlineSize(), total_block_size_);
    PhysicalSize flexbox_size = ToPhysicalSize(
        logical_flexbox_size, ConstraintSpace().GetWritingMode());
    item_rect.offset = offset.ConvertToPhysical(
        ConstraintSpace().GetWritingDirection(), flexbox_size, item_rect.size);
    // devtools uses margin box.
    item_rect.Expand(flex_item->physical_margins_);
    DCHECK_GE(layout_info_for_devtools_->lines.size(), 1u);
    DevtoolsFlexInfo::Item item(
        item_rect, flex_item->MarginBoxAscent(
                       flex_item->Alignment() == ItemPosition::kLastBaseline,
                       Style().FlexWrap() == EFlexWrap::kWrapReverse));
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
      !container_builder_.FirstBaseline()) {
    // To ensure that we have a consistent baseline when we have no children,
    // even when we have the anonymous LayoutBlock child, we calculate the
    // baseline for the empty case manually here.
    container_builder_.SetBaselines(BorderScrollbarPadding().block_start +
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
          *Node().GetLayoutBox(), Style(), parent ? parent->Style() : nullptr))
    return;

  // The button should have at most one child.
  const NGContainerFragmentBuilder::ChildrenVector& children =
      container_builder_.Children();
  if (children.size() < 1) {
    const LayoutBlock* layout_block = To<LayoutBlock>(Node().GetLayoutBox());
    absl::optional<LayoutUnit> baseline = layout_block->BaselineForEmptyLine(
        layout_block->IsHorizontalWritingMode() ? kHorizontalLine
                                                : kVerticalLine);
    if (container_builder_.FirstBaseline() != baseline) {
      UseCounter::Count(Node().GetDocument(),
                        WebFeature::kWrongBaselineOfEmptyLineButton);
    }
    return;
  }
  DCHECK_EQ(children.size(), 1u);
  const NGLogicalLink& child = children[0];
  DCHECK(!child.fragment->IsLineBox());
  const NGConstraintSpace& space = ConstraintSpace();
  NGBoxFragment fragment(space.GetWritingDirection(),
                         To<NGPhysicalBoxFragment>(*child.fragment));
  absl::optional<LayoutUnit> child_baseline =
      space.BaselineAlgorithmType() == NGBaselineAlgorithmType::kDefault
          ? fragment.FirstBaseline()
          : fragment.LastBaseline();
  if (child_baseline)
    child_baseline = *child_baseline + child.offset.block_offset;
  if (container_builder_.FirstBaseline() != child_baseline) {
    UseCounter::Count(Node().GetDocument(),
                      WebFeature::kWrongBaselineOfMultiLineButton);
    String text = Node().GetDOMNode()->textContent();
    if (ContainsNonWhitespace(Node().GetLayoutBox())) {
      UseCounter::Count(
          Node().GetDocument(),
          WebFeature::kWrongBaselineOfMultiLineButtonWithNonSpace);
    }
  }
}

MinMaxSizesResult NGFlexLayoutAlgorithm::ComputeItemContributions(
    const NGConstraintSpace& space,
    const FlexItem& item) const {
  const NGBlockNode& child = item.ng_input_node_;
  const ComputedStyle& child_style = child.Style();
  MinMaxSizesResult item_contributions = child.ComputeMinMaxSizes(
      ConstraintSpace().GetWritingMode(), MinMaxSizesType::kContent, space);

  // This calculates the "preferred size" part of 9.9.3:
  // "... outer preferred size (its width/height as appropriate) if that is not
  // auto ..."
  const Length& preferred_main_axis_length =
      is_horizontal_flow_ ? child_style.Width() : child_style.Height();
  bool is_preferred_main_axis_length_auto = preferred_main_axis_length.IsAuto();
  // This block of if-statements that computes
  // |is_preferred_main_axis_length_auto| is fragile.
  // TODO(dgrogan/ikilpatrick): Figure out how to modify
  // CalculateInitialFragmentGeometry so that we can call it unconditionally and
  // it returns something usable here when the preferred main axis length is
  // auto. Any of these would be usable: -1, 0, or the child's min-content size.
  if (is_preferred_main_axis_length_auto) {
    if (AspectRatioProvidesMainSize(child))
      is_preferred_main_axis_length_auto = false;
  } else if (MainAxisIsInlineAxis(child)) {
    if (InlineLengthUnresolvable(space, preferred_main_axis_length))
      is_preferred_main_axis_length_auto = true;
  } else {
    if (BlockLengthUnresolvable(space, preferred_main_axis_length))
      is_preferred_main_axis_length_auto = true;
  }
  if (!is_preferred_main_axis_length_auto) {
    NGFragmentGeometry child_initial_geometry =
        CalculateInitialFragmentGeometry(space, child,
                                         /* break_token */ nullptr,
                                         /* is_intrinsic */ false);
    const LayoutUnit preferred_size =
        MainAxisIsInlineAxis(child)
            ? child_initial_geometry.border_box_size.inline_size
            : child_initial_geometry.border_box_size.block_size;

    item_contributions.sizes.Encompass(preferred_size);
  }

  if (algorithm_.IsMultiline()) {
    // This block implements the "capped" part at the end of 9.9.1:
    // "for a multi-line container... each item’s contribution is capped by
    // the item’s flex base size if the item is not growable, floored by the
    // item’s flex base size if the item is not shrinkable"
    const LayoutUnit flex_base_size_border_box =
        item.flex_base_content_size_ + item.main_axis_border_padding_;
    const ComputedStyle& parent_style = Style();
    if (child_style.ResolvedFlexGrow(parent_style) == 0.f) {
      item_contributions.sizes.min_size = std::min(
          item_contributions.sizes.min_size, flex_base_size_border_box);
    }
    if (child_style.ResolvedFlexShrink(parent_style) == 0.f) {
      item_contributions.sizes.min_size = std::max(
          item_contributions.sizes.min_size, flex_base_size_border_box);
    }
  }

  item_contributions.sizes.Constrain(item.min_max_main_sizes_.max_size +
                                     item.main_axis_border_padding_);
  item_contributions.sizes.Encompass(item.min_max_main_sizes_.min_size +
                                     item.main_axis_border_padding_);
  return item_contributions;
}

class FlexFractionParts {
 public:
  explicit FlexFractionParts(const ComputedStyle& parent_style)
      : parent_style_(parent_style) {}
  // After we find the largest flex fraction, this function calculates the
  // product from https://drafts.csswg.org/css-flexbox/#intrinsic-main-sizes,
  // step 4:
  // "Add each item’s flex base size to the product of its flex grow factor (or
  // scaled flex shrink factor, if the chosen max-content flex fraction was
  // negative) and the chosen flex fraction"
  LayoutUnit ApplyLargestFlexFractionToItem(const ComputedStyle& child_style,
                                            LayoutUnit flex_base_content_size) {
    if (chosen_flex_fraction_ == std::numeric_limits<float>::lowest()) {
      // All the items wanted to shrink from their flex basis to get to their
      // min-content size, but all had flex-shrink = 0.
      DCHECK_EQ(numerator_, LayoutUnit());
      return LayoutUnit();
    }

    if (chosen_flex_fraction_ > 0.f) {
      DCHECK(!denominator_pixels_part_.has_value());
      DCHECK_GT(sum_factors_less_than_one_adjustment_, 0.f)
          << "If all the flex grow factors were == 0, then "
             "chosen_flex_fraction_ can't be positive";
      return LayoutUnit(
          numerator_ *
          (child_style.ResolvedFlexGrow(parent_style_) /
           (sum_factors_less_than_one_adjustment_ * denominator_float_part_)));
    }
    if (chosen_flex_fraction_ < 0.f) {
      return LayoutUnit(
          flex_base_content_size.MulDiv(numerator_, *denominator_pixels_part_) *
          (sum_factors_less_than_one_adjustment_ *
           child_style.ResolvedFlexShrink(parent_style_) /
           denominator_float_part_));
    }
    // Control-flow reaches here if the item that contributed the chosen flex
    // fraction had (1) a flex base size smaller than max-content contribution
    // size and grow factor 0 (it wants to grow but it can't); OR (2) a flex
    // base size equal to its max-content contribution size.
    DCHECK(!denominator_pixels_part_.has_value())
        << "We're not supposed to get here if the chosen flex "
           "fraction was from an item with a negative desired flex fraction, "
           "which is the only time |denominator_pixels_part_| has a value.";
    return LayoutUnit();
  }

  // This function does 9.9.1 step 1 and 2: calculate the item's desired flex
  // fraction (step 1) and save it if it's the largest (step 2).
  void UpdateLargestFlexFraction(const FlexItem& item,
                                 LayoutUnit item_contribution) {
    const ComputedStyle& child_style = item.style_;

    const float flex_grow_factor = child_style.ResolvedFlexGrow(parent_style_);
    sum_flex_grow_factors_ += flex_grow_factor;
    const float flex_shrink_factor =
        child_style.ResolvedFlexShrink(parent_style_);
    sum_flex_shrink_factors_ += flex_shrink_factor;

    // |difference| is contribution - flex_basis, which can be negative because
    // item contributions can be smaller than the item's flex base size.
    LayoutUnit difference = item_contribution - item.flex_base_content_size_ -
                            item.main_axis_border_padding_;
    if (difference > LayoutUnit()) {
      // This item's contribution is greater than its flex basis.
      // If control flow ever reaches here for _any_ item, the largest flex
      // fraction will be positive and the container's max-content size (or
      // min-content size, whichever this object is associated with) will be
      // greater than the sum of the items' flex bases.
      float desired_flex_fraction = difference;
      if (flex_grow_factor >= 1.f)
        desired_flex_fraction /= flex_grow_factor;
      else
        desired_flex_fraction *= flex_grow_factor;
      if (desired_flex_fraction > chosen_flex_fraction_) {
        chosen_flex_fraction_ = desired_flex_fraction;
        numerator_ = difference;
        denominator_float_part_ =
            flex_grow_factor >= 1.f ? flex_grow_factor : 1 / flex_grow_factor;
        denominator_pixels_part_.reset();
      }
    } else if (difference < LayoutUnit()) {
      // If we end up here for _every_ item, the final largest flex fraction
      // will be negative and the container's max-content (or min-content) size
      // will be less than the sum of the items' flex bases, but still most
      // likely greater than the sum of each item's contribution.
      const float scaled_flex_shrink_factor =
          item.flex_base_content_size_ * flex_shrink_factor;
      if (scaled_flex_shrink_factor == 0.f) {
        // If the desired flex fraction is -infinity, it should never become the
        // chosen flex fraction.
        return;
      }
      const float desired_flex_fraction =
          difference / scaled_flex_shrink_factor;
      if (desired_flex_fraction > chosen_flex_fraction_) {
        chosen_flex_fraction_ = desired_flex_fraction;
        numerator_ = difference;
        denominator_float_part_ = flex_shrink_factor;
        denominator_pixels_part_ = item.flex_base_content_size_;
      }
    } else {
      // This item's flex basis was equal to its contribution size. If
      // every item enters either this block or the previous block then
      // the intrinsic size represented by this object (either min or max)
      // will be equal to the sum of the items' flex bases.
      DCHECK_EQ(difference, LayoutUnit());
      if (difference > chosen_flex_fraction_) {
        chosen_flex_fraction_ = 0.f;
        numerator_ = difference;
        denominator_float_part_ = 1.f;
        denominator_pixels_part_.reset();
      }
    }
  }

  // This function sets up sum_factors_less_than_one_adjustment_ to handle 9.9.1
  // step 3:
  // If the chosen flex fraction is positive, and the sum of the line’s
  // flex grow factors is less than 1, divide the chosen flex fraction by that
  // sum.
  // If the chosen flex fraction is negative, and the sum of the line’s
  // flex shrink factors is less than 1, multiply the chosen flex fraction by
  // that sum.
  void SetSumFactorsLessThanOneAdjustment() {
    if (chosen_flex_fraction_ > 0.f && sum_flex_grow_factors_ < 1.f) {
      DCHECK_GT(sum_flex_grow_factors_, 0.f)
          << "If all the flex grow factors were == 0, then "
             "chosen_flex_fraction can't be positive";
      sum_factors_less_than_one_adjustment_ = sum_flex_grow_factors_;
    } else if (chosen_flex_fraction_ < 0.f && sum_flex_shrink_factors_ < 1.f) {
      sum_factors_less_than_one_adjustment_ = sum_flex_shrink_factors_;
    }
  }

 private:
  float chosen_flex_fraction_ = std::numeric_limits<float>::lowest();

  // We have to store these individual components of the flex fraction so that
  // we can multiply them in an order that minimizes precision issues.
  LayoutUnit numerator_;
  float denominator_float_part_;
  // This optional field is filled when we use scaled flex shrink factor as
  // dictated in step 1 from
  // https://drafts.csswg.org/css-flexbox/#intrinsic-main-sizes.
  absl::optional<LayoutUnit> denominator_pixels_part_;
  float sum_factors_less_than_one_adjustment_ = 1.f;

  float sum_flex_shrink_factors_ = 0;
  float sum_flex_grow_factors_ = 0;
  const ComputedStyle& parent_style_;
};

MinMaxSizesResult
NGFlexLayoutAlgorithm::ComputeMinMaxSizeOfMultilineColumnContainer() {
  MinMaxSizes largest_inline_size_contributions;
  // The algorithm for determining the max-content width of a column-wrap
  // container is simply: Run layout on the container but give the items an
  // overridden available size, equal to the largest max-content width of any
  // item, when they are laid out. The container's max-content width is then
  // the farthest outer inline-end point of all the items.
  HeapVector<NGFlexLine> flex_line_outputs;
  PlaceFlexItems(&flex_line_outputs, /* oof_children */ nullptr,
                 /* is_computing_multiline_column_intrinsic_size */ true);
  largest_inline_size_contributions.min_size =
      largest_min_content_contribution_;
  if (!flex_line_outputs.empty()) {
    largest_inline_size_contributions.max_size =
        flex_line_outputs.back().line_cross_size +
        flex_line_outputs.back().cross_axis_offset -
        flex_line_outputs.front().cross_axis_offset;
  }

  DCHECK_GE(largest_inline_size_contributions.min_size, 0);
  DCHECK_LE(largest_inline_size_contributions.min_size,
            largest_inline_size_contributions.max_size);

  largest_inline_size_contributions += BorderScrollbarPadding().InlineSum();

  // This always depends on block constraints because if block constraints
  // change, this flexbox could get a different number of columns.
  return {largest_inline_size_contributions,
          /* depends_on_block_constraints */ true};
}

MinMaxSizesResult NGFlexLayoutAlgorithm::ComputeMinMaxSizeOfRowContainer() {
  // The goal of this algorithm is to find a container inline size such that
  // after running the flex algorithm, each item's final size will be at least
  // as large as its contribution. This is similar to regular non-flex
  // min/max-content sizing except we can't make everything exactly its
  // contribution size, just due to the inherent nature of the flex algorithm.
  // So the intrinsic size algorithm is designed to make the container larger
  // than the sum of the contributions in cases where not every item is going to
  // be at its exact contribution size after the main flex algorithm runs.

  MinMaxSizes container_sizes;
  bool depends_on_block_constraints = false;

  FlexFractionParts min_content_largest_fraction(Style());
  FlexFractionParts max_content_largest_fraction(Style());
  LayoutUnit largest_outer_min_content_contribution;

  // The intrinsic sizing algorithm uses lots of geometry and values from each
  // item (e.g. flex base size, used minimum and maximum sizes including
  // automatic minimum sizing), so re-use |ConstructAndAppendFlexItems| from the
  // layout algorithm, which calculates all that.
  ConstructAndAppendFlexItems(Phase::kRowIntrinsicSize);

  // First pass: look for the most restrictive items that will influence the
  // sizing of the rest.
  for (const FlexItem& item : algorithm_.all_items_) {
    const NGBlockNode& child = item.ng_input_node_;

    const NGConstraintSpace space = BuildSpaceForIntrinsicInlineSize(child);
    const MinMaxSizesResult min_max_content_contributions =
        ComputeItemContributions(space, item);
    depends_on_block_constraints |=
        min_max_content_contributions.depends_on_block_constraints;

    if (algorithm_.IsMultiline()) {
      const LayoutUnit main_axis_margins =
          is_horizontal_flow_ ? item.physical_margins_.HorizontalSum()
                              : item.physical_margins_.VerticalSum();
      largest_outer_min_content_contribution = std::max(
          largest_outer_min_content_contribution,
          min_max_content_contributions.sizes.min_size + main_axis_margins);
    } else {
      min_content_largest_fraction.UpdateLargestFlexFraction(
          item, min_max_content_contributions.sizes.min_size);
    }
    max_content_largest_fraction.UpdateLargestFlexFraction(
        item, min_max_content_contributions.sizes.max_size);
  }
  min_content_largest_fraction.SetSumFactorsLessThanOneAdjustment();
  max_content_largest_fraction.SetSumFactorsLessThanOneAdjustment();

  // Second pass: determine what each item's size will be when the container is
  // at either of its intrinsic sizes.
  for (const FlexItem& item : algorithm_.all_items_) {
    const ComputedStyle& child_style = item.style_;
    const LayoutUnit flex_base_size_border_box =
        item.flex_base_content_size_ + item.main_axis_border_padding_;
    MinMaxSizes item_final_contribution{flex_base_size_border_box,
                                        flex_base_size_border_box};
    if (!algorithm_.IsMultiline()) {
      item_final_contribution.min_size +=
          min_content_largest_fraction.ApplyLargestFlexFractionToItem(
              child_style, item.flex_base_content_size_);
    }
    item_final_contribution.max_size +=
        max_content_largest_fraction.ApplyLargestFlexFractionToItem(
            child_style, item.flex_base_content_size_);

    item_final_contribution.Constrain(item.min_max_main_sizes_.max_size +
                                      item.main_axis_border_padding_);
    item_final_contribution.Encompass(item.min_max_main_sizes_.min_size +
                                      item.main_axis_border_padding_);

    container_sizes += item_final_contribution;

    const LayoutUnit main_axis_margins =
        is_horizontal_flow_ ? item.physical_margins_.HorizontalSum()
                            : item.physical_margins_.VerticalSum();
    container_sizes += main_axis_margins;
  }

  if (algorithm_.IsMultiline()) {
    container_sizes.min_size = largest_outer_min_content_contribution;
  } else {
    DCHECK_EQ(largest_outer_min_content_contribution, LayoutUnit())
        << "largest_outer_min_content_contribution is not filled in for "
           "singleline containers.";
    const LayoutUnit gap_inline_size =
        (algorithm_.NumItems() - 1) * algorithm_.gap_between_items_;
    container_sizes += gap_inline_size;
  }

  // Due to negative margins, it is possible that we calculated a negative
  // intrinsic width. Make sure that we never return a negative width.
  container_sizes.Encompass(LayoutUnit());
  container_sizes += BorderScrollbarPadding().InlineSum();
  DCHECK_GE(container_sizes.max_size, container_sizes.min_size);
  return MinMaxSizesResult(container_sizes, depends_on_block_constraints);
}

MinMaxSizesResult NGFlexLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  if (auto result = CalculateMinMaxSizesIgnoringChildren(
          Node(), BorderScrollbarPadding()))
    return *result;

  if (RuntimeEnabledFeatures::NewFlexboxSizingEnabled()) {
    // TODO(crbug.com/240765): Implement all the cases here.
    if (is_column_) {
      if (algorithm_.IsMultiline()) {
        return ComputeMinMaxSizeOfMultilineColumnContainer();
      } else {
        // singleline column flexbox
      }
    } else {
      return ComputeMinMaxSizeOfRowContainer();
    }
  }

  MinMaxSizes sizes;
  bool depends_on_block_constraints = false;

  int number_of_items = 0;
  NGFlexChildIterator iterator(Node());
  for (NGBlockNode child = iterator.NextChild(); child;
       child = iterator.NextChild()) {
    if (child.IsOutOfFlowPositioned())
      continue;
    number_of_items++;

    const NGConstraintSpace space = BuildSpaceForIntrinsicInlineSize(child);
    MinMaxSizesResult child_result =
        ComputeMinAndMaxContentContribution(Style(), child, space);
    NGBoxStrut child_margins =
        ComputeMarginsFor(space, child.Style(), ConstraintSpace());
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

LayoutUnit NGFlexLayoutAlgorithm::FragmentainerSpaceAvailable(
    LayoutUnit block_offset) const {
  return (FragmentainerSpaceLeft(ConstraintSpace()) - block_offset)
      .ClampNegativeToZero();
}

void NGFlexLayoutAlgorithm::ConsumeRemainingFragmentainerSpace(
    LayoutUnit previously_consumed_block_size,
    NGFlexLine* flex_line,
    const NGFlexColumnBreakInfo* column_break_info) {
  if (To<NGBlockBreakToken>(container_builder_.LastChildBreakToken())
          ->IsForcedBreak()) {
    // This will be further adjusted by the total consumed block size once we
    // handle the break before in the next fragmentainer. This ensures that the
    // expansion is properly handled in the column balancing pass.
    LayoutUnit intrinsic_block_size = intrinsic_block_size_;
    if (column_break_info) {
      DCHECK(is_column_);
      intrinsic_block_size = column_break_info->column_intrinsic_block_size;
    }
    flex_line->item_offset_adjustment -=
        intrinsic_block_size + previously_consumed_block_size;
  }

  if (!ConstraintSpace().HasKnownFragmentainerBlockSize())
    return;
  // The remaining part of the fragmentainer (the unusable space for child
  // content, due to the break) should still be occupied by this container.
  intrinsic_block_size_ += FragmentainerSpaceAvailable(intrinsic_block_size_);
}

NGBreakStatus NGFlexLayoutAlgorithm::BreakBeforeRowIfNeeded(
    const NGFlexLine& row,
    EBreakBetween row_break_between,
    wtf_size_t row_index,
    NGLayoutInputNode child,
    bool has_container_separation,
    bool is_first_for_row) {
  DCHECK(!is_column_);
  DCHECK(InvolvedInBlockFragmentation(container_builder_));

  LayoutUnit fragmentainer_block_offset =
      ConstraintSpace().FragmentainerOffset() + row.cross_axis_offset;
  if (BreakToken())
    fragmentainer_block_offset -= BreakToken()->ConsumedBlockSize();

  if (has_container_separation) {
    if (IsForcedBreakValue(ConstraintSpace(), row_break_between)) {
      BreakBeforeChild(ConstraintSpace(), child, /* layout_result */ nullptr,
                       fragmentainer_block_offset, kBreakAppealPerfect,
                       /* is_forced_break */ true, &container_builder_,
                       row.line_cross_size);
      return NGBreakStatus::kBrokeBefore;
    }
  }

  bool breakable_at_start_of_container = IsBreakableAtStartOfResumedContainer(
      ConstraintSpace(), container_builder_, is_first_for_row);
  NGBreakAppeal appeal_before = CalculateBreakAppealBefore(
      ConstraintSpace(), NGLayoutResult::EStatus::kSuccess, row_break_between,
      has_container_separation, breakable_at_start_of_container);

  // Attempt to move past the break point, and if we can do that, also assess
  // the appeal of breaking there, even if we didn't.
  if (MovePastRowBreakPoint(
          appeal_before, fragmentainer_block_offset, row.line_cross_size,
          row_index, has_container_separation, breakable_at_start_of_container))
    return NGBreakStatus::kContinue;

  // We're out of space. Figure out where to insert a soft break. It will either
  // be before this row, or before an earlier sibling, if there's a more
  // appealing breakpoint there.
  if (!AttemptSoftBreak(ConstraintSpace(), child, /* layout_result */ nullptr,
                        fragmentainer_block_offset, appeal_before,
                        &container_builder_, row.line_cross_size))
    return NGBreakStatus::kNeedsEarlierBreak;

  return NGBreakStatus::kBrokeBefore;
}

bool NGFlexLayoutAlgorithm::MovePastRowBreakPoint(
    NGBreakAppeal appeal_before,
    LayoutUnit fragmentainer_block_offset,
    LayoutUnit row_block_size,
    wtf_size_t row_index,
    bool has_container_separation,
    bool breakable_at_start_of_container) {
  if (!ConstraintSpace().HasKnownFragmentainerBlockSize()) {
    // We only care about soft breaks if we have a fragmentainer block-size.
    // During column balancing this may be unknown.
    return true;
  }

  LayoutUnit space_left =
      FragmentainerCapacity(ConstraintSpace()) - fragmentainer_block_offset;

  // If the row starts past the end of the fragmentainer, we must break before
  // it.
  bool must_break_before = false;
  if (space_left < LayoutUnit()) {
    must_break_before = true;
  } else if (space_left == LayoutUnit()) {
    // If the row starts exactly at the end, we'll allow the row here if the
    // row has zero block-size. Otherwise we have to break before it.
    must_break_before = row_block_size != LayoutUnit();
  }
  if (must_break_before) {
#if DCHECK_IS_ON()
    bool refuse_break_before =
        space_left >= FragmentainerCapacity(ConstraintSpace());
    DCHECK(!refuse_break_before);
#endif
    return false;
  }

  // Update the early break in case breaking before the row ends up being the
  // most appealing spot to break.
  if ((has_container_separation || breakable_at_start_of_container) &&
      (!container_builder_.HasEarlyBreak() ||
       appeal_before >= container_builder_.EarlyBreak().BreakAppeal())) {
    container_builder_.SetEarlyBreak(
        MakeGarbageCollected<NGEarlyBreak>(row_index, appeal_before));
  }

  // Avoiding breaks inside a row will be handled at the item level.
  return true;
}

void NGFlexLayoutAlgorithm::AddColumnEarlyBreak(NGEarlyBreak* breakpoint,
                                                wtf_size_t index) {
  DCHECK(is_column_);
  while (column_early_breaks_.size() <= index)
    column_early_breaks_.push_back(nullptr);
  column_early_breaks_[index] = breakpoint;
}

void NGFlexLayoutAlgorithm::AdjustOffsetForNextLine(
    HeapVector<NGFlexLine>* flex_line_outputs,
    wtf_size_t flex_line_idx,
    LayoutUnit item_expansion) const {
  DCHECK_LT(flex_line_idx, flex_line_outputs->size());
  if (flex_line_idx == flex_line_outputs->size() - 1)
    return;
  (*flex_line_outputs)[flex_line_idx + 1].item_offset_adjustment +=
      item_expansion;
}

const NGLayoutResult* NGFlexLayoutAlgorithm::RelayoutWithNewRowSizes() {
  // We shouldn't update the row cross-sizes more than once per fragmentainer.
  DCHECK(!cross_size_adjustments_);

  // There should be no more than two row expansions per fragmentainer.
  DCHECK(!row_cross_size_updates_.empty());
  DCHECK_LE(row_cross_size_updates_.size(), 2u);

  NGLayoutAlgorithmParams params(
      Node(), container_builder_.InitialFragmentGeometry(), ConstraintSpace(),
      BreakToken(), early_break_, additional_early_breaks_);
  NGFlexLayoutAlgorithm algorithm_with_row_cross_sizes(
      params, &row_cross_size_updates_);
  auto& new_builder = algorithm_with_row_cross_sizes.container_builder_;
  new_builder.SetBoxType(container_builder_.BoxType());
  algorithm_with_row_cross_sizes.ignore_child_scrollbar_changes_ =
      ignore_child_scrollbar_changes_;

  // We may have aborted layout due to an early break previously. Ensure that
  // the builder detects the correct space shortage, if so.
  if (early_break_) {
    new_builder.PropagateSpaceShortage(
        container_builder_.MinimalSpaceShortage());
  }
  return algorithm_with_row_cross_sizes.Layout();
}

// We are interested in cases where the flex item *may* expand due to
// fragmentation (lines pushed down by a fragmentation line, etc).
bool NGFlexLayoutAlgorithm::MinBlockSizeShouldEncompassIntrinsicSize(
    const NGFlexItem& item) const {
  // If this item has (any) descendant that is percentage based, we can end
  // up in a situation where we'll constantly try and expand the row. E.g.
  // <div style="display: flex;">
  //   <div style="min-height: 100px;">
  //     <div style="height: 200%;"></div>
  //   </div>
  // </div>
  if (item.has_descendant_that_depends_on_percentage_block_size)
    return false;

  if (item.ng_input_node.IsMonolithic())
    return false;

  const auto& item_style = item.ng_input_node.Style();

  // NOTE: We currently assume that writing-mode roots are monolithic, but
  // this may change in the future.
  DCHECK_EQ(ConstraintSpace().GetWritingDirection().GetWritingMode(),
            item_style.GetWritingMode());

  if (is_column_) {
    bool can_shrink = item_style.ResolvedFlexShrink(Style()) != 0.f &&
                      IsColumnContainerMainSizeDefinite();

    // Only allow growth if the item can't shrink and the flex-basis is
    // content-based.
    if (!IsUsedFlexBasisDefinite(item.ng_input_node) && !can_shrink)
      return true;

    // Only allow growth if the item's block-size is auto and either the item
    // can't shrink or its min-height is auto.
    if (item_style.LogicalHeight().IsAutoOrContentOrIntrinsic() &&
        (!can_shrink || algorithm_.ShouldApplyMinSizeAutoForChild(
                            *item.ng_input_node.GetLayoutBox())))
      return true;
  } else {
    // Don't grow if the item's block-size should be the same as its container.
    if (WillChildCrossSizeBeContainerCrossSize(item.ng_input_node) &&
        !Style().LogicalHeight().IsAutoOrContentOrIntrinsic()) {
      return false;
    }

    // Only allow growth if the item's cross size is auto.
    if (DoesItemCrossSizeComputeToAuto(item.ng_input_node))
      return true;
  }
  return false;
}

#if DCHECK_IS_ON()
void NGFlexLayoutAlgorithm::CheckFlexLines(
    HeapVector<NGFlexLine>& flex_line_outputs) const {
  const Vector<FlexLine>& flex_lines = algorithm_.flex_lines_;

  // Re-reverse the order of the lines and items to match those stored in
  // |algorithm_|.
  if (Style().FlexWrap() == EFlexWrap::kWrapReverse)
    flex_line_outputs.Reverse();

  if (Style().ResolvedIsColumnReverseFlexDirection() ||
      Style().ResolvedIsRowReverseFlexDirection()) {
    for (auto& flex_line : flex_line_outputs)
      flex_line.line_items.Reverse();
  }

  DCHECK_EQ(flex_line_outputs.size(), flex_lines.size());
  for (wtf_size_t i = 0; i < flex_line_outputs.size(); i++) {
    const FlexLine& flex_line = flex_lines[i];
    const NGFlexLine& flex_line_output = flex_line_outputs[i];

    DCHECK_EQ(flex_line_output.line_cross_size, flex_line.cross_axis_extent_);
    DCHECK_EQ(flex_line_output.cross_axis_offset, flex_line.cross_axis_offset_);
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
