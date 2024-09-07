// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/forms/fieldset_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/forms/fieldset_break_token_data.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/logical_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/space_utils.h"

namespace blink {

namespace {

enum class LegendBlockAlignment {
  kStart,
  kCenter,
  kEnd,
};

// Legends aren't inline-level. Yet they may be aligned within the fieldset
// block-start border using the text-align property (in addition to using auto
// margins).
inline LegendBlockAlignment ComputeLegendBlockAlignment(
    const ComputedStyle& legend_style,
    const ComputedStyle& fieldset_style) {
  bool start_auto =
      legend_style.MarginInlineStartUsing(fieldset_style).IsAuto();
  bool end_auto = legend_style.MarginInlineEndUsing(fieldset_style).IsAuto();
  if (start_auto || end_auto) {
    if (start_auto) {
      return end_auto ? LegendBlockAlignment::kCenter
                      : LegendBlockAlignment::kEnd;
    }
    return LegendBlockAlignment::kStart;
  }
  const bool is_ltr = fieldset_style.IsLeftToRightDirection();
  switch (legend_style.GetTextAlign()) {
    case ETextAlign::kLeft:
      return is_ltr ? LegendBlockAlignment::kStart : LegendBlockAlignment::kEnd;
    case ETextAlign::kRight:
      return is_ltr ? LegendBlockAlignment::kEnd : LegendBlockAlignment::kStart;
    case ETextAlign::kCenter:
      return LegendBlockAlignment::kCenter;
    default:
      return LegendBlockAlignment::kStart;
  }
}

}  // namespace

FieldsetLayoutAlgorithm::FieldsetLayoutAlgorithm(
    const LayoutAlgorithmParams& params)
    : LayoutAlgorithm(params),
      writing_direction_(GetConstraintSpace().GetWritingDirection()),
      consumed_block_size_(GetBreakToken()
                               ? GetBreakToken()->ConsumedBlockSize()
                               : LayoutUnit()) {
  DCHECK(params.fragment_geometry.scrollbar.IsEmpty());
  border_box_size_ = container_builder_.InitialBorderBoxSize();
}

const LayoutResult* FieldsetLayoutAlgorithm::Layout() {
  // Layout of a fieldset container consists of two parts: Create a child
  // fragment for the rendered legend (if any), and create a child fragment for
  // the fieldset contents anonymous box (if any).
  // Fieldset scrollbars and padding will not be applied to the fieldset
  // container itself, but rather to the fieldset contents anonymous child box.
  // The reason for this is that the rendered legend shouldn't be part of the
  // scrollport; the legend is essentially a part of the block-start border,
  // and should not scroll along with the actual fieldset contents. Since
  // scrollbars are handled by the anonymous child box, and since padding is
  // inside the scrollport, padding also needs to be handled by the anonymous
  // child.
  if (ShouldIncludeBlockStartBorderPadding(container_builder_)) {
    intrinsic_block_size_ = Borders().block_start;
  }

  if (InvolvedInBlockFragmentation(container_builder_)) {
    container_builder_.SetBreakTokenData(
        MakeGarbageCollected<FieldsetBreakTokenData>(
            container_builder_.GetBreakTokenData()));
  }

  BreakStatus break_status = LayoutChildren();
  if (break_status == BreakStatus::kNeedsEarlierBreak) {
    // We need to abort the layout. No fragment will be generated.
    return container_builder_.Abort(LayoutResult::kNeedsEarlierBreak);
  }

  intrinsic_block_size_ =
      ClampIntrinsicBlockSize(GetConstraintSpace(), Node(), GetBreakToken(),
                              Borders() + Scrollbar() + Padding(),
                              intrinsic_block_size_ + Borders().block_end);

  // Recompute the block-axis size now that we know our content size.
  border_box_size_.block_size =
      ComputeBlockSizeForFragment(GetConstraintSpace(), Node(), BorderPadding(),
                                  intrinsic_block_size_ + consumed_block_size_,
                                  border_box_size_.inline_size);

  // The above computation utility knows nothing about fieldset weirdness. The
  // legend may eat from the available content box block size. Make room for
  // that if necessary.
  // Note that in size containment, we have to consider sizing as if we have no
  // contents, with the conjecture being that legend is part of the contents.
  // Thus, only do this adjustment if we do not contain size.
  if (!Node().ShouldApplyBlockSizeContainment()) {
    border_box_size_.block_size =
        std::max(border_box_size_.block_size, minimum_border_box_block_size_);
  }

  // TODO(almaher): end border and padding may overflow the parent
  // fragmentainer, and we should avoid that.
  LayoutUnit all_fragments_block_size = border_box_size_.block_size;

  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size_);
  container_builder_.SetFragmentsTotalBlockSize(all_fragments_block_size);
  container_builder_.SetIsFieldsetContainer();

  if (InvolvedInBlockFragmentation(container_builder_)) [[unlikely]] {
    BreakStatus status = FinishFragmentation(&container_builder_);
    if (status == BreakStatus::kNeedsEarlierBreak) {
      // If we found a good break somewhere inside this block, re-layout and
      // break at that location.
      return RelayoutAndBreakEarlier<FieldsetLayoutAlgorithm>(
          container_builder_.GetEarlyBreak());
    } else if (status == BreakStatus::kDisableFragmentation) {
      return RelayoutWithoutFragmentation<FieldsetLayoutAlgorithm>();
    }
    DCHECK_EQ(status, BreakStatus::kContinue);
  } else {
#if DCHECK_IS_ON()
    // If we're not participating in a fragmentation context, no block
    // fragmentation related fields should have been set.
    container_builder_.CheckNoBlockFragmentation();
#endif
  }

  container_builder_.HandleOofsAndSpecialDescendants();

  const auto& style = Style();
  if (style.LogicalHeight().MayHavePercentDependence() ||
      style.LogicalMinHeight().MayHavePercentDependence() ||
      style.LogicalMaxHeight().MayHavePercentDependence()) {
    // The height of the fieldset content box depends on the percent-height of
    // the fieldset. So we should assume the fieldset has a percent-height
    // descendant.
    container_builder_.SetHasDescendantThatDependsOnPercentageBlockSize();
  }

  return container_builder_.ToBoxFragment();
}

BreakStatus FieldsetLayoutAlgorithm::LayoutChildren() {
  const BlockBreakToken* content_break_token = nullptr;
  bool has_seen_all_children = false;
  if (const auto* token = GetBreakToken()) {
    const auto child_tokens = token->ChildBreakTokens();
    if (base::checked_cast<wtf_size_t>(child_tokens.size())) {
      const BlockBreakToken* child_token =
          To<BlockBreakToken>(child_tokens[0].Get());
      if (child_token) {
        DCHECK(!child_token->InputNode().IsRenderedLegend());
        content_break_token = child_token;
      }
      // There shouldn't be any additional break tokens.
      DCHECK_EQ(child_tokens.size(), 1u);
    }
    if (token->HasSeenAllChildren()) {
      container_builder_.SetHasSeenAllChildren();
      has_seen_all_children = true;
    }
  }

  LogicalSize adjusted_padding_box_size =
      ShrinkLogicalSize(border_box_size_, Borders());

  BlockNode legend = Node().GetRenderedLegend();
  if (legend) {
    if (!IsBreakInside(GetBreakToken())) {
      LayoutLegend(legend);
    }
    LayoutUnit legend_size_contribution;
    if (IsBreakInside(GetBreakToken())) {
      const auto* token_data =
          To<FieldsetBreakTokenData>(GetBreakToken()->TokenData());
      legend_size_contribution = token_data->legend_block_size_contribution;
    } else {
      // We're at the first fragment. The current layout position
      // (intrinsic_block_size_) is at the outer block-end edge of the legend
      // or just after the block-start border, whichever is larger.
      legend_size_contribution = intrinsic_block_size_ - Borders().block_start;
    }

    if (InvolvedInBlockFragmentation(container_builder_)) {
      auto* token_data =
          To<FieldsetBreakTokenData>(container_builder_.GetBreakTokenData());
      token_data->legend_block_size_contribution = legend_size_contribution;
    }

    if (adjusted_padding_box_size.block_size != kIndefiniteSize) {
      DCHECK_NE(border_box_size_.block_size, kIndefiniteSize);
      adjusted_padding_box_size.block_size = std::max(
          adjusted_padding_box_size.block_size - legend_size_contribution,
          Padding().BlockSum());
    }

    // The legend may eat from the available content box block size. Calculate
    // the minimum block size needed to encompass the legend.
    if (!Node().ShouldApplyBlockSizeContainment()) {
      minimum_border_box_block_size_ =
          BorderPadding().BlockSum() + legend_size_contribution;
    }
  }

  // Proceed with normal fieldset children (excluding the rendered legend). They
  // all live inside an anonymous child box of the fieldset container.
  if (content_break_token || !has_seen_all_children) {
    BlockNode fieldset_content = Node().GetFieldsetContent();
    DCHECK(fieldset_content);
    BreakStatus break_status =
        LayoutFieldsetContent(fieldset_content, content_break_token,
                              adjusted_padding_box_size, !!legend);
    if (break_status == BreakStatus::kNeedsEarlierBreak) {
      return break_status;
    }
  }

  return BreakStatus::kContinue;
}

void FieldsetLayoutAlgorithm::LayoutLegend(BlockNode& legend) {
  // Lay out the legend. While the fieldset container normally ignores its
  // padding, the legend is laid out within what would have been the content
  // box had the fieldset been a regular block with no weirdness.
  LogicalSize percentage_size = CalculateChildPercentageSize(
      GetConstraintSpace(), Node(), ChildAvailableSize());
  BoxStrut legend_margins =
      ComputeMarginsFor(legend.Style(), percentage_size.inline_size,
                        GetConstraintSpace().GetWritingDirection());

  auto legend_space = CreateConstraintSpaceForLegend(
      legend, ChildAvailableSize(), percentage_size);
  const LayoutResult* result = legend.Layout(legend_space, GetBreakToken());

  // Legends are monolithic, so abortions are not expected.
  DCHECK_EQ(result->Status(), LayoutResult::kSuccess);

  const auto& physical_fragment = result->GetPhysicalFragment();

  LayoutUnit legend_border_box_block_size =
      LogicalFragment(writing_direction_, physical_fragment).BlockSize();
  LayoutUnit legend_margin_box_block_size = legend_margins.block_start +
                                            legend_border_box_block_size +
                                            legend_margins.block_end;

  LayoutUnit space_left = Borders().block_start - legend_border_box_block_size;
  LayoutUnit block_offset;
  if (space_left > LayoutUnit()) {
    // https://html.spec.whatwg.org/C/#the-fieldset-and-legend-elements
    // * The element is expected to be positioned in the block-flow direction
    //   such that its border box is centered over the border on the
    //   block-start side of the fieldset element.
    block_offset += space_left / 2;
  }
  // If the border is smaller than the block end offset of the legend margin
  // box, intrinsic_block_size_ should now be based on the the block end
  // offset of the legend margin box instead of the border.
  LayoutUnit legend_margin_end_offset =
      block_offset + legend_margin_box_block_size - legend_margins.block_start;
  if (legend_margin_end_offset > Borders().block_start)
    intrinsic_block_size_ = legend_margin_end_offset;

  // If the margin box of the legend is at least as tall as the fieldset
  // block-start border width, it will start at the block-start border edge
  // of the fieldset. As a paint effect, the block-start border will be
  // pushed so that the center of the border will be flush with the center
  // of the border-box of the legend.

  LayoutUnit legend_border_box_inline_size =
      LogicalFragment(writing_direction_, result->GetPhysicalFragment())
          .InlineSize();

  // Padding is mostly ignored for the fieldset container, but rather set on the
  // anonymous fieldset content wrapper child (which is reflected in the
  // BorderScrollbarPadding() of the builders). However, legends should honor
  // it. Scrollbars should never occur at the inline-start, so no need to add
  // that.
  LayoutUnit legend_inline_start =
      Borders().inline_start + Scrollbar().inline_start +
      Padding().inline_start + legend_margins.inline_start;

  const LayoutUnit available_space =
      ChildAvailableSize().inline_size - legend_border_box_inline_size;
  if (available_space > LayoutUnit()) {
    auto alignment = ComputeLegendBlockAlignment(legend.Style(), Style());
    if (alignment == LegendBlockAlignment::kCenter)
      legend_inline_start += available_space / 2;
    else if (alignment == LegendBlockAlignment::kEnd)
      legend_inline_start += available_space - legend_margins.inline_end;
  }

  LogicalOffset legend_offset = {legend_inline_start, block_offset};

  container_builder_.AddResult(*result, legend_offset);
}

BreakStatus FieldsetLayoutAlgorithm::LayoutFieldsetContent(
    BlockNode& fieldset_content,
    const BlockBreakToken* content_break_token,
    LogicalSize adjusted_padding_box_size,
    bool has_legend) {
  const EarlyBreak* early_break_in_child = nullptr;
  if (early_break_) [[unlikely]] {
    if (IsEarlyBreakTarget(*early_break_, container_builder_,
                           fieldset_content)) {
      container_builder_.AddBreakBeforeChild(fieldset_content,
                                             kBreakAppealPerfect,
                                             /* is_forced_break */ false);
      ConsumeRemainingFragmentainerSpace();
      return BreakStatus::kContinue;
    } else {
      early_break_in_child =
          EnterEarlyBreakInChild(fieldset_content, *early_break_);
    }
  }

  const LayoutResult* result = nullptr;
  bool is_past_end = GetBreakToken() && GetBreakToken()->IsAtBlockEnd();

  LayoutUnit max_content_block_size = LayoutUnit::Max();
  if (adjusted_padding_box_size.block_size == kIndefiniteSize) {
    max_content_block_size = ResolveInitialMaxBlockLength(
        GetConstraintSpace(), Style(), BorderPadding(),
        Style().LogicalMaxHeight());
  }

  // If we are past the block-end and had previously laid out the content with a
  // block-size limitation, skip the normal layout call and apply the block-size
  // limitation for all future fragments.
  if (!is_past_end || max_content_block_size == LayoutUnit::Max()) {
    auto child_space = CreateConstraintSpaceForFieldsetContent(
        fieldset_content, adjusted_padding_box_size, intrinsic_block_size_);
    result = fieldset_content.Layout(child_space, content_break_token,
                                     early_break_in_child);
  }

  // If the following conditions meet, the content should be laid out with
  // a block-size limitation:
  // - The FIELDSET block-size is indefinite.
  // - It has max-block-size.
  // - The intrinsic block-size of the content is larger than the
  //   max-block-size.
  if (max_content_block_size != LayoutUnit::Max() &&
      (!result || result->Status() == LayoutResult::kSuccess)) {
    DCHECK_EQ(adjusted_padding_box_size.block_size, kIndefiniteSize);
    if (max_content_block_size > Padding().BlockSum()) {
      // intrinsic_block_size_ is
      // max(Borders().block_start, legend margin box block size).
      max_content_block_size =
          std::max(max_content_block_size -
                       (intrinsic_block_size_ + Borders().block_end),
                   Padding().BlockSum());
    }

    if (result) {
      const auto& fragment = result->GetPhysicalFragment();
      LayoutUnit total_block_size =
          LogicalFragment(writing_direction_, fragment).BlockSize();
      if (content_break_token)
        total_block_size += content_break_token->ConsumedBlockSize();
      if (total_block_size >= max_content_block_size)
        result = nullptr;
    } else {
      DCHECK(is_past_end);
    }

    if (!result) {
      adjusted_padding_box_size.block_size = max_content_block_size;
      auto adjusted_child_space = CreateConstraintSpaceForFieldsetContent(
          fieldset_content, adjusted_padding_box_size, intrinsic_block_size_);
      result = fieldset_content.Layout(
          adjusted_child_space, content_break_token, early_break_in_child);
    }
  }
  DCHECK(result);

  BreakStatus break_status = BreakStatus::kContinue;
  if (GetConstraintSpace().HasBlockFragmentation() && !early_break_) {
    break_status = BreakBeforeChildIfNeeded(
        fieldset_content, *result,
        FragmentainerOffsetForChildren() + intrinsic_block_size_,
        /*has_container_separation=*/false);
  }

  if (break_status == BreakStatus::kContinue) {
    DCHECK_EQ(result->Status(), LayoutResult::kSuccess);
    LogicalOffset offset(Borders().inline_start, intrinsic_block_size_);
    container_builder_.AddResult(*result, offset);

    const auto& fragment =
        To<PhysicalBoxFragment>(result->GetPhysicalFragment());
    if (auto first_baseline = fragment.FirstBaseline()) {
      container_builder_.SetFirstBaseline(offset.block_offset +
                                          *first_baseline);
    }
    if (auto last_baseline = fragment.LastBaseline())
      container_builder_.SetLastBaseline(offset.block_offset + *last_baseline);
    if (fragment.UseLastBaselineForInlineBaseline())
      container_builder_.SetUseLastBaselineForInlineBaseline();

    intrinsic_block_size_ +=
        LogicalFragment(writing_direction_, fragment).BlockSize();
    container_builder_.SetHasSeenAllChildren();
  } else if (break_status == BreakStatus::kBrokeBefore) {
    ConsumeRemainingFragmentainerSpace();
  }

  return break_status;
}

LayoutUnit FieldsetLayoutAlgorithm::FragmentainerSpaceAvailable() const {
  // The legend may have extended past the end of the fragmentainer. Clamp to
  // zero if this is the case.
  return std::max(LayoutUnit(),
                  FragmentainerSpaceLeftForChildren() - intrinsic_block_size_);
}

void FieldsetLayoutAlgorithm::ConsumeRemainingFragmentainerSpace() {
  if (GetConstraintSpace().HasKnownFragmentainerBlockSize()) {
    // The remaining part of the fragmentainer (the unusable space for child
    // content, due to the break) should still be occupied by this container.
    intrinsic_block_size_ += FragmentainerSpaceAvailable();
  }
}

MinMaxSizesResult FieldsetLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  MinMaxSizesResult result;

  bool has_inline_size_containment = Node().ShouldApplyInlineSizeContainment();
  if (has_inline_size_containment) {
    // Size containment does not consider the legend for sizing.
    //
    // Add borders, scrollbar and padding separately, since padding for most
    // purposes are ignored on fieldset containers, and are therefore not
    // included in BorderScrollbarPadding().
    std::optional<MinMaxSizesResult> result_without_children =
        CalculateMinMaxSizesIgnoringChildren(
            Node(), Borders() + Scrollbar() + Padding());
    if (result_without_children)
      return *result_without_children;
  } else {
    if (BlockNode legend = Node().GetRenderedLegend()) {
      MinMaxConstraintSpaceBuilder builder(GetConstraintSpace(), Style(),
                                           legend,
                                           /* is_new_fc */ true);
      builder.SetAvailableBlockSize(kIndefiniteSize);
      const auto space = builder.ToConstraintSpace();

      result = ComputeMinAndMaxContentContribution(Style(), legend, space);
      result.sizes +=
          ComputeMarginsFor(space, legend.Style(), GetConstraintSpace())
              .InlineSum();
    }
  }

  // The fieldset content includes the fieldset padding (and any scrollbars),
  // while the legend is a regular child and doesn't. We may have a fieldset
  // without any content or legend, so add the padding here, on the outside.
  result.sizes += ComputePadding(GetConstraintSpace(), Style()).InlineSum();

  // Size containment does not consider the content for sizing.
  if (!has_inline_size_containment) {
    BlockNode content = Node().GetFieldsetContent();
    DCHECK(content);
    MinMaxConstraintSpaceBuilder builder(GetConstraintSpace(), Style(), content,
                                         /* is_new_fc */ true);
    builder.SetAvailableBlockSize(kIndefiniteSize);
    const auto space = builder.ToConstraintSpace();

    MinMaxSizesResult content_result =
        ComputeMinAndMaxContentContribution(Style(), content, space);
    content_result.sizes +=
        ComputeMarginsFor(space, content.Style(), GetConstraintSpace())
            .InlineSum();
    result.sizes.Encompass(content_result.sizes);
    result.depends_on_block_constraints |=
        content_result.depends_on_block_constraints;
  }

  result.sizes += ComputeBorders(GetConstraintSpace(), Node()).InlineSum();
  return result;
}

const ConstraintSpace FieldsetLayoutAlgorithm::CreateConstraintSpaceForLegend(
    BlockNode legend,
    LogicalSize available_size,
    LogicalSize percentage_size) {
  ConstraintSpaceBuilder builder(GetConstraintSpace(),
                                 legend.Style().GetWritingDirection(),
                                 /* is_new_fc */ true);
  SetOrthogonalFallbackInlineSizeIfNeeded(Style(), legend, &builder);

  builder.SetAvailableSize(available_size);
  builder.SetPercentageResolutionSize(percentage_size);
  return builder.ToConstraintSpace();
}

const ConstraintSpace
FieldsetLayoutAlgorithm::CreateConstraintSpaceForFieldsetContent(
    BlockNode fieldset_content,
    LogicalSize padding_box_size,
    LayoutUnit block_offset) {
  DCHECK(fieldset_content.CreatesNewFormattingContext());
  ConstraintSpaceBuilder builder(GetConstraintSpace(),
                                 fieldset_content.Style().GetWritingDirection(),
                                 /* is_new_fc */ true);
  builder.SetAvailableSize(padding_box_size);
  builder.SetInlineAutoBehavior(AutoSizeBehavior::kStretchImplicit);
  // We pass the container's PercentageResolutionSize because percentage
  // padding for the fieldset content should be computed as they are in
  // the container.
  //
  // https://html.spec.whatwg.org/C/#anonymous-fieldset-content-box
  // > * For the purpose of calculating percentage padding, act as if the
  // >   padding was calculated for the fieldset element.
  builder.SetPercentageResolutionSize(
      GetConstraintSpace().PercentageResolutionSize());
  builder.SetIsFixedBlockSize(padding_box_size.block_size != kIndefiniteSize);
  builder.SetBaselineAlgorithmType(
      GetConstraintSpace().GetBaselineAlgorithmType());

  if (GetConstraintSpace().HasBlockFragmentation()) {
    SetupSpaceBuilderForFragmentation(container_builder_, fieldset_content,
                                      block_offset, &builder);
  }
  return builder.ToConstraintSpace();
}

}  // namespace blink
