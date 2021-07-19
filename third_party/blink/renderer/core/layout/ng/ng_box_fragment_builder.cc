// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"

namespace blink {

void NGBoxFragmentBuilder::AddBreakBeforeChild(
    NGLayoutInputNode child,
    absl::optional<NGBreakAppeal> appeal,
    bool is_forced_break) {
  // If there's a pre-set break token, we shouldn't be here.
  DCHECK(!break_token_);

  if (appeal) {
    break_appeal_ = *appeal;
    // If we're violating any orphans / widows or
    // break-{after,before,inside}:avoid requests, remember this. If we're
    // balancing columns, we may be able to stretch the columns to resolve the
    // situation. Note that we should consider handling kBreakAppealLastResort
    // as well here, but that's currently causing trouble for large leading
    // margins, which would cause taller columns than desirable in some cases.
    if (break_appeal_ == kBreakAppealViolatingBreakAvoid ||
        break_appeal_ == kBreakAppealViolatingOrphansAndWidows)
      has_violating_break_ = true;
  }
  if (is_forced_break) {
    SetHasForcedBreak();
    // A forced break is considered to always have perfect appeal; they should
    // never be weighed against other potential breakpoints.
    DCHECK(!appeal || *appeal == kBreakAppealPerfect);
  }

  DCHECK(has_block_fragmentation_);

  if (!has_inflow_child_break_inside_)
    has_inflow_child_break_inside_ = !child.IsFloatingOrOutOfFlowPositioned();
  if (!has_float_break_inside_)
    has_float_break_inside_ = child.IsFloating();

  if (auto* child_inline_node = DynamicTo<NGInlineNode>(child)) {
    if (!last_inline_break_token_) {
      // In some cases we may want to break before the first line in the
      // fragment. This happens if there's a tall float before the line, or, as
      // a last resort, when there are no better breakpoints to choose from, and
      // we're out of space. When laying out, we store the inline break token
      // from the last line added to the builder, but if we haven't added any
      // lines at all, we are still going to need a break token, so that the we
      // can tell where to resume in the inline formatting context in the next
      // fragmentainer.

      if (previous_break_token_) {
        // If there's an incoming break token, see if it has a child inline
        // break token, and use that one. We may be past floats or lines that
        // were laid out in earlier fragments.
        const auto& child_tokens = previous_break_token_->ChildBreakTokens();
        if (child_tokens.size()) {
          // If there is an inline break token, it will always be the last
          // child.
          last_inline_break_token_ =
              DynamicTo<NGInlineBreakToken>(child_tokens.back());
          if (last_inline_break_token_)
            return;
        }
      }

      // We're at the beginning of the inline formatting context.
      last_inline_break_token_ = NGInlineBreakToken::Create(
          *child_inline_node, /* style */ nullptr, /* item_index */ 0,
          /* text_offset */ 0, NGInlineBreakToken::kDefault);
    }
    return;
  }
  auto token = NGBlockBreakToken::CreateBreakBefore(child, is_forced_break);
  child_break_tokens_.push_back(token);
}

void NGBoxFragmentBuilder::AddResult(
    const NGLayoutResult& child_layout_result,
    const LogicalOffset offset,
    absl::optional<LogicalOffset> relative_offset,
    bool propagate_oof_descendants) {
  const auto& fragment = child_layout_result.PhysicalFragment();
  if (items_builder_) {
    if (const NGPhysicalLineBoxFragment* line =
            DynamicTo<NGPhysicalLineBoxFragment>(&fragment)) {
      items_builder_->AddLine(*line, offset);
      // TODO(kojii): We probably don't need to AddChild this line, but there
      // maybe OOF objects. Investigate how to handle them.
    }
  }

  const NGMarginStrut end_margin_strut = child_layout_result.EndMarginStrut();
  // No margins should pierce outside formatting-context roots.
  DCHECK(!fragment.IsFormattingContextRoot() || end_margin_strut.IsEmpty());

  absl::optional<LayoutUnit> adjustment_for_oof_propagation;
  if (propagate_oof_descendants)
    adjustment_for_oof_propagation = BlockOffsetAdjustmentForFragmentainer();

  AddChild(fragment, offset, /* inline_container */ nullptr, &end_margin_strut,
           child_layout_result.IsSelfCollapsing(), relative_offset,
           adjustment_for_oof_propagation);
  if (fragment.IsBox())
    PropagateBreak(child_layout_result);
}

void NGBoxFragmentBuilder::AddChild(
    const NGPhysicalFragment& child,
    const LogicalOffset& child_offset,
    const NGInlineContainer<LogicalOffset>* inline_container,
    const NGMarginStrut* margin_strut,
    bool is_self_collapsing,
    absl::optional<LogicalOffset> relative_offset,
    absl::optional<LayoutUnit> adjustment_for_oof_propagation) {
#if DCHECK_IS_ON()
  needs_inflow_bounds_explicitly_set_ = !!relative_offset;
  needs_may_have_descendant_above_block_start_explicitly_set_ =
      !!relative_offset;
#endif

  if (!relative_offset) {
    relative_offset = LogicalOffset();
    if (box_type_ != NGPhysicalBoxFragment::NGBoxType::kInlineBox) {
      if (child.IsCSSBox()) {
        // Apply the relative position offset.
        const auto& box_child = To<NGPhysicalBoxFragment>(child);
        if (box_child.Style().GetPosition() == EPosition::kRelative) {
          relative_offset = ComputeRelativeOffsetForBoxFragment(
              box_child, GetWritingDirection(), child_available_size_);
        }

        // The |may_have_descendant_above_block_start_| flag is used to
        // determine if a fragment can be re-used when preceding floats are
        // present. This is relatively rare, and is true if:
        //  - An inflow child is positioned above our block-start edge.
        //  - Any inflow descendants (within the same formatting-context) which
        //    *may* have a child positioned above our block-start edge.
        if ((child_offset.block_offset < LayoutUnit() &&
             !box_child.IsOutOfFlowPositioned()) ||
            (!box_child.IsFormattingContextRoot() &&
             box_child.MayHaveDescendantAboveBlockStart()))
          may_have_descendant_above_block_start_ = true;
      }

      // If we are a scroll container, we need to track the maximum bounds of
      // any inflow children (including line-boxes) to calculate the
      // layout-overflow.
      //
      // This is used for determining the "padding-box" of the scroll container
      // which is *sometimes* considered as part of the scrollable area. Inflow
      // children contribute to this area, out-of-flow positioned children
      // don't.
      //
      // Out-of-flow positioned children still contribute to the
      // layout-overflow, but just don't influence where this padding is.
      if (Node().IsScrollContainer() &&
          box_type_ != NGPhysicalBoxFragment::NGBoxType::kColumnBox &&
          !child.IsOutOfFlowPositioned()) {
        NGBoxStrut margins;
        if (child.IsCSSBox()) {
          margins = ComputeMarginsFor(child.Style(),
                                      child_available_size_.inline_size,
                                      GetWritingDirection());
        }

        // If we are in block-flow layout we use the end *margin-strut* as the
        // block-end "margin" (instead of just the block-end margin).
        if (margin_strut) {
          NGMarginStrut end_margin_strut = *margin_strut;
          end_margin_strut.Append(margins.block_end, /* is_quirky */ false);

          // Self-collapsing blocks are special, their end margin-strut is part
          // of their inflow position. To correctly determine the "end" margin,
          // we need to the "final" margin-strut from their end margin-strut.
          margins.block_end = is_self_collapsing
                                  ? end_margin_strut.Sum() - margin_strut->Sum()
                                  : end_margin_strut.Sum();
        }

        // Use the original offset (*without* relative-positioning applied).
        NGFragment fragment(GetWritingDirection(), child);
        LogicalRect bounds = {child_offset, fragment.Size()};

        // Margins affect the inflow-bounds in interesting ways.
        //
        // For the margin which is closest to the direction which we are
        // scrolling, we allow negative margins, but only up to the size of the
        // fragment. For the margin furthest away we disallow negative margins.
        if (!margins.IsEmpty()) {
          // Convert the physical overflow directions to logical.
          const bool has_top_overflow = Node().HasTopOverflow();
          const bool has_left_overflow = Node().HasLeftOverflow();
          PhysicalToLogical<bool> converter(
              GetWritingDirection(), has_top_overflow, !has_left_overflow,
              !has_top_overflow, has_left_overflow);

          if (converter.InlineStart()) {
            margins.inline_end = margins.inline_end.ClampNegativeToZero();
            margins.inline_start =
                std::max(margins.inline_start, -fragment.InlineSize());
          } else {
            margins.inline_start = margins.inline_start.ClampNegativeToZero();
            margins.inline_end =
                std::max(margins.inline_end, -fragment.InlineSize());
          }
          if (converter.BlockStart()) {
            margins.block_end = margins.block_end.ClampNegativeToZero();
            margins.block_start =
                std::max(margins.block_start, -fragment.BlockSize());
          } else {
            margins.block_start = margins.block_start.ClampNegativeToZero();
            margins.block_end =
                std::max(margins.block_end, -fragment.BlockSize());
          }

          // Shift the bounds by the (potentially clamped) margins.
          bounds.offset -= {margins.inline_start, margins.block_start};
          bounds.size.inline_size += margins.InlineSum();
          bounds.size.block_size += margins.BlockSum();

          // Our bounds size should never go negative.
          DCHECK_GE(bounds.size.inline_size, LayoutUnit());
          DCHECK_GE(bounds.size.block_size, LayoutUnit());
        }

        // Even an empty (0x0) fragment contributes to the inflow-bounds.
        if (!inflow_bounds_)
          inflow_bounds_ = bounds;
        else
          inflow_bounds_->UniteEvenIfEmpty(bounds);
      }
    }
  }

  DCHECK(relative_offset);
  PropagateChildData(child, child_offset, *relative_offset, inline_container,
                     adjustment_for_oof_propagation);
  AddChildInternal(&child, child_offset + *relative_offset);
}

void NGBoxFragmentBuilder::AddBreakToken(
    scoped_refptr<const NGBreakToken> token,
    bool is_in_parallel_flow) {
  // If there's a pre-set break token, we shouldn't be here.
  DCHECK(!break_token_);

  DCHECK(token.get());
  child_break_tokens_.push_back(std::move(token));
  has_inflow_child_break_inside_ |= !is_in_parallel_flow;
}

void NGBoxFragmentBuilder::AddOutOfFlowLegacyCandidate(
    NGBlockNode node,
    const NGLogicalStaticPosition& static_position,
    const LayoutInline* inline_container) {
  if (inline_container)
    inline_container = To<LayoutInline>(inline_container->ContinuationRoot());
  oof_positioned_candidates_.emplace_back(
      node, static_position,
      NGInlineContainer<LogicalOffset>(inline_container,
                                       /* relative_offset */ LogicalOffset()));
}

NGPhysicalFragment::NGBoxType NGBoxFragmentBuilder::BoxType() const {
  if (box_type_ != NGPhysicalFragment::NGBoxType::kNormalBox)
    return box_type_;

  // When implicit, compute from LayoutObject.
  DCHECK(layout_object_);
  if (layout_object_->IsFloating())
    return NGPhysicalFragment::NGBoxType::kFloating;
  if (layout_object_->IsOutOfFlowPositioned())
    return NGPhysicalFragment::NGBoxType::kOutOfFlowPositioned;
  if (layout_object_->IsRenderedLegend())
    return NGPhysicalFragment::NGBoxType::kRenderedLegend;
  if (layout_object_->IsInline()) {
    // Check |IsAtomicInlineLevel()| after |IsInline()| because |LayoutReplaced|
    // sets |IsAtomicInlineLevel()| even when it's block-level. crbug.com/567964
    if (layout_object_->IsAtomicInlineLevel())
      return NGPhysicalFragment::NGBoxType::kAtomicInline;
    return NGPhysicalFragment::NGBoxType::kInlineBox;
  }
  DCHECK(node_) << "Must call SetBoxType if there is no node";
  DCHECK_EQ(is_new_fc_, node_.CreatesNewFormattingContext())
      << "Forgot to call builder.SetIsNewFormattingContext";
  if (is_new_fc_)
    return NGPhysicalFragment::NGBoxType::kBlockFlowRoot;
  return NGPhysicalFragment::NGBoxType::kNormalBox;
}

EBreakBetween NGBoxFragmentBuilder::JoinedBreakBetweenValue(
    EBreakBetween break_before) const {
  return JoinFragmentainerBreakValues(previous_break_after_, break_before);
}

void NGBoxFragmentBuilder::MoveChildrenInBlockDirection(LayoutUnit delta) {
  DCHECK(is_new_fc_);
  DCHECK(!has_oof_candidate_that_needs_block_offset_adjustment_);
  DCHECK_NE(FragmentBlockSize(), kIndefiniteSize);
  DCHECK(oof_positioned_descendants_.IsEmpty());

  if (delta == LayoutUnit())
    return;

  if (baseline_)
    *baseline_ += delta;
  if (last_baseline_)
    *last_baseline_ += delta;

  if (inflow_bounds_)
    inflow_bounds_->offset.block_offset += delta;

  for (auto& child : children_)
    child.offset.block_offset += delta;

  for (auto& candidate : oof_positioned_candidates_)
    candidate.static_position.offset.block_offset += delta;
  for (auto& descendant : oof_positioned_fragmentainer_descendants_)
    descendant.static_position.offset.block_offset += delta;

  if (NGFragmentItemsBuilder* items_builder = ItemsBuilder())
    items_builder->MoveChildrenInBlockDirection(delta);
}

void NGBoxFragmentBuilder::PropagateBreak(
    const NGLayoutResult& child_layout_result) {
  if (LIKELY(!has_block_fragmentation_))
    return;
  if (!has_inflow_child_break_inside_ || !has_float_break_inside_) {
    // Figure out if this child break is in the same flow as this parent. If
    // it's an out-of-flow positioned box, it's not. If it's in a parallel flow,
    // it's also not.
    const auto& child_fragment =
        To<NGPhysicalBoxFragment>(child_layout_result.PhysicalFragment());
    if (const auto* token = child_fragment.BreakToken()) {
      if (!token->IsBlockType() ||
          !To<NGBlockBreakToken>(token)->IsAtBlockEnd()) {
        if (child_fragment.IsFloating())
          has_float_break_inside_ = true;
        else if (!child_fragment.IsOutOfFlowPositioned())
          has_inflow_child_break_inside_ = true;
      }
    }
  }
  if (child_layout_result.HasForcedBreak()) {
    SetHasForcedBreak();
  } else if (IsInitialColumnBalancingPass()) {
    PropagateTallestUnbreakableBlockSize(
        child_layout_result.TallestUnbreakableBlockSize());
  } else {
    PropagateSpaceShortage(child_layout_result.MinimalSpaceShortage());
  }

  if (!is_fragmentation_context_root_)
    has_violating_break_ |= child_layout_result.HasViolatingBreak();
}

scoped_refptr<const NGLayoutResult> NGBoxFragmentBuilder::ToBoxFragment(
    WritingMode block_or_line_writing_mode) {
#if DCHECK_IS_ON()
  if (ItemsBuilder()) {
    for (const ChildWithOffset& child : Children()) {
      DCHECK(child.fragment);
      const NGPhysicalFragment& fragment = *child.fragment;
      DCHECK(fragment.IsLineBox() ||
             // TODO(kojii): How to place floats and OOF is TBD.
             fragment.IsFloatingOrOutOfFlowPositioned());
    }
  }
#endif

  if (UNLIKELY(has_block_fragmentation_ && !break_token_ && node_)) {
    if (last_inline_break_token_)
      child_break_tokens_.push_back(std::move(last_inline_break_token_));
    if (DidBreakSelf() || HasChildBreakInside())
      break_token_ = NGBlockBreakToken::Create(*this);
  }

  if (!has_floating_descendants_for_paint_ && items_builder_) {
    has_floating_descendants_for_paint_ =
        items_builder_->HasFloatingDescendantsForPaint();
  }

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGPhysicalBoxFragment::Create(this, block_or_line_writing_mode);
  fragment->CheckType();

  return base::AdoptRef(
      new NGLayoutResult(NGLayoutResult::NGBoxFragmentBuilderPassKey(),
                         std::move(fragment), this));
}

LogicalOffset NGBoxFragmentBuilder::GetChildOffset(
    const LayoutObject* object) const {
  DCHECK(object);

  if (const NGFragmentItemsBuilder* items_builder = items_builder_) {
    if (auto offset = items_builder->LogicalOffsetFor(*object))
      return *offset;
    // Out-of-flow objects may be in |FragmentItems| or in |children_|.
  }

  for (const auto& child : children_) {
    if (child.fragment->GetLayoutObject() == object)
      return child.offset;

    // TODO(layout-dev): ikilpatrick thinks we may need to traverse
    // further than the initial line-box children for a nested inline
    // container. We could not come up with a testcase, it would be
    // something with split inlines, and nested oof/fixed descendants maybe.
    if (child.fragment->IsLineBox()) {
      const auto& line_box_fragment =
          To<NGPhysicalLineBoxFragment>(*child.fragment);
      for (const auto& line_box_child : line_box_fragment.Children()) {
        if (line_box_child->GetLayoutObject() == object) {
          return child.offset + line_box_child.Offset().ConvertToLogical(
                                    GetWritingDirection(),
                                    line_box_fragment.Size(),
                                    line_box_child->Size());
        }
      }
    }
  }
  NOTREACHED();
  return LogicalOffset();
}

void NGBoxFragmentBuilder::SetLastBaselineToBlockEndMarginEdgeIfNeeded() {
  if (ConstraintSpace()->BaselineAlgorithmType() !=
      NGBaselineAlgorithmType::kInlineBlock)
    return;

  if (!node_.UseBlockEndMarginEdgeForInlineBlockBaseline())
    return;

  // When overflow is present (within an atomic-inline baseline context) we
  // should always use the block-end margin edge as the baseline.
  NGBoxStrut margins = ComputeMarginsForSelf(*ConstraintSpace(), Style());
  SetLastBaseline(FragmentBlockSize() + margins.block_end);
}

void NGBoxFragmentBuilder::SetMathItalicCorrection(
    LayoutUnit italic_correction) {
  if (!math_data_)
    math_data_.emplace();
  math_data_->italic_correction_ = italic_correction;
}

void NGBoxFragmentBuilder::AdjustOffsetsForFragmentainerDescendant(
    NGLogicalOutOfFlowPositionedNode& descendant,
    bool only_fixedpos_containing_block) {
  if (!PreviousBreakToken())
    return;
  LayoutUnit previous_consumed_block_size =
      PreviousBreakToken()->ConsumedBlockSize();

  // If the containing block is fragmented, adjust the offset to be from the
  // first containing block fragment to the fragmentation context root. Also,
  // adjust the static position to be relative to the adjusted containing block
  // offset.
  if (!only_fixedpos_containing_block &&
      !descendant.containing_block.fragment) {
    descendant.containing_block.offset.block_offset -=
        previous_consumed_block_size;
    descendant.static_position.offset.block_offset +=
        previous_consumed_block_size;
  }

  // If the fixedpos containing block is fragmented, adjust the offset to be
  // from the first containing block fragment to the fragmentation context root.
  if (!descendant.fixedpos_containing_block.fragment &&
      node_.IsFixedContainer()) {
    descendant.fixedpos_containing_block.offset.block_offset -=
        previous_consumed_block_size;
  }
}

LayoutUnit NGBoxFragmentBuilder::BlockOffsetAdjustmentForFragmentainer(
    LayoutUnit fragmentainer_consumed_block_size) const {
  if (IsFragmentainerBoxType() && PreviousBreakToken())
    return PreviousBreakToken()->ConsumedBlockSize();
  return fragmentainer_consumed_block_size;
}

void NGBoxFragmentBuilder::
    AdjustFixedposContainingBlockForFragmentainerDescendants() {
  if (!HasOutOfFlowFragmentainerDescendants() || !PreviousBreakToken() ||
      !node_.IsFixedContainer())
    return;

  for (auto& descendant : oof_positioned_fragmentainer_descendants_) {
    AdjustOffsetsForFragmentainerDescendant(
        descendant, /* only_fixedpos_containing_block */ true);
  }
}

void NGBoxFragmentBuilder::AdjustFixedposContainingBlockForInnerMulticols() {
  if (!HasMulticolsWithPendingOOFs() || !PreviousBreakToken() ||
      !node_.IsFixedContainer())
    return;

  // If the fixedpos containing block is fragmented, adjust the offset to be
  // from the first containing block fragment to the fragmentation context root.
  // Also, update the multicol offset such that it is relative to the fixedpos
  // containing block.
  LayoutUnit previous_consumed_block_size =
      PreviousBreakToken()->ConsumedBlockSize();
  for (auto& multicol : multicols_with_pending_oofs_) {
    NGMulticolWithPendingOOFs<LogicalOffset>& value = multicol.value;
    if (!value.fixedpos_containing_block.fragment) {
      value.fixedpos_containing_block.offset.block_offset -=
          previous_consumed_block_size;
      value.multicol_offset.block_offset += previous_consumed_block_size;
    }
  }
}

#if DCHECK_IS_ON()

void NGBoxFragmentBuilder::CheckNoBlockFragmentation() const {
  DCHECK(!HasChildBreakInside());
  DCHECK(!HasInflowChildBreakInside());
  DCHECK(!DidBreakSelf());
  DCHECK(!has_forced_break_);
  DCHECK_EQ(consumed_block_size_, LayoutUnit());
  DCHECK_EQ(consumed_block_size_legacy_adjustment_, LayoutUnit());
  DCHECK_EQ(minimal_space_shortage_, LayoutUnit::Max());
  DCHECK(!initial_break_before_);
  DCHECK_EQ(previous_break_after_, EBreakBetween::kAuto);
}

#endif

}  // namespace blink
