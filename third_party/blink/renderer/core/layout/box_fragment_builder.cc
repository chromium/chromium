// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"

#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/break_token.h"
#include "third_party/blink/renderer/core/layout/column_spanner_path.h"
#include "third_party/blink/renderer/core/layout/exclusions/exclusion_space.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/inline/inline_break_token.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/inline/physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/positioned_float.h"
#include "third_party/blink/renderer/core/layout/relative_utils.h"

namespace blink {

void BoxFragmentBuilder::UpdateBorderPaddingForClonedBoxDecorations() {
  const BlockBreakToken* break_token = PreviousBreakToken();
  if (!IsBreakInside(break_token)) {
    return;
  }
  // BorderPadding() is used for resolving the box size, and it needs to include
  // the border/padding space taken up by this new fragment (to be created),
  // plus all preceding ones.
  int fragment_count_including_this = break_token->SequenceNumber() + 2;
  border_padding_.block_start *= fragment_count_including_this;
  border_padding_.block_end *= fragment_count_including_this;
}

const LayoutResult& BoxFragmentBuilder::LayoutResultForPropagation(
    const LayoutResult& layout_result) const {
  if (layout_result.Status() != LayoutResult::kSuccess) {
    return layout_result;
  }
  const auto& fragment = layout_result.GetPhysicalFragment();
  if (fragment.IsBox()) {
    return layout_result;
  }

  const auto* line = DynamicTo<PhysicalLineBoxFragment>(&fragment);
  if (!line || !line->IsBlockInInline() || !items_builder_) {
    return layout_result;
  }

  const auto& line_items = items_builder_->GetLogicalLineItems(*line);
  DCHECK(line_items.BlockInInlineLayoutResult());
  return *line_items.BlockInInlineLayoutResult();
}

void BoxFragmentBuilder::AddBreakBeforeChild(LayoutInputNode child,
                                             std::optional<BreakAppeal> appeal,
                                             bool is_forced_break) {
  // If there's a pre-set break token, we shouldn't be here.
  DCHECK(!break_token_);

  if (is_forced_break) {
    SetHasForcedBreak();
    // A forced break is considered to always have perfect appeal; they should
    // never be weighed against other potential breakpoints.
    DCHECK(!appeal || *appeal == kBreakAppealPerfect);
  } else if (appeal) {
    ClampBreakAppeal(*appeal);
  }

  DCHECK(has_block_fragmentation_);

  if (!has_inflow_child_break_inside_)
    has_inflow_child_break_inside_ = !child.IsFloatingOrOutOfFlowPositioned();

  if (auto* child_inline_node = DynamicTo<InlineNode>(child)) {
    if (!last_inline_break_token_) {
      // In some cases we may want to break before the first line in the
      // fragment. This happens if there's a tall float before the line, or, as
      // a last resort, when there are no better breakpoints to choose from, and
      // we're out of space. When laying out, we store the inline break token
      // from the last line added to the builder, but if we haven't added any
      // lines at all, we are still going to need a break token, so that the we
      // can tell where to resume in the inline formatting context in the next
      // fragmentainer.

      if (PreviousBreakToken()) {
        // If there's an incoming break token, see if it has a child inline
        // break token, and use that one. We may be past floats or lines that
        // were laid out in earlier fragments.
        const auto& child_tokens = PreviousBreakToken()->ChildBreakTokens();
        if (child_tokens.size()) {
          // If there is an inline break token, it will always be the last
          // child.
          last_inline_break_token_ =
              DynamicTo<InlineBreakToken>(child_tokens.back().Get());
          if (last_inline_break_token_)
            return;
        }
      }

      // We're at the beginning of the inline formatting context.
      last_inline_break_token_ = InlineBreakToken::Create(
          *child_inline_node, /* style */ nullptr, InlineItemTextIndex(),
          InlineBreakToken::kDefault);
    }
    return;
  }
  auto* token = BlockBreakToken::CreateBreakBefore(child, is_forced_break);
  child_break_tokens_.push_back(token);
}

void BoxFragmentBuilder::AddResult(
    const LayoutResult& child_layout_result,
    const LogicalOffset offset,
    std::optional<const BoxStrut> margins,
    std::optional<LogicalOffset> relative_offset,
    const OofInlineContainer<LogicalOffset>* inline_container) {
  const auto& fragment = child_layout_result.GetPhysicalFragment();

  // We'll normally propagate info from child_layout_result here, but if that's
  // a line box with a block inside, we'll use the result for that block
  // instead. The fact that we create a line box at all in such cases is just an
  // implementation detail -- anything of interest is stored on the child block
  // fragment.
  const LayoutResult* result_for_propagation = &child_layout_result;

  if (!fragment.IsBox() && items_builder_) {
    if (const auto* line = DynamicTo<PhysicalLineBoxFragment>(&fragment)) {
      if (line->IsBlockInInline() && has_block_fragmentation_) [[unlikely]] {
        // If this line box contains a block-in-inline, propagate break data
        // from the block-in-inline.
        const auto& line_items = items_builder_->GetLogicalLineItems(*line);
        result_for_propagation = line_items.BlockInInlineLayoutResult();
        DCHECK(result_for_propagation);
      }

      items_builder_->AddLine(*line, offset);
      // TODO(kojii): We probably don't need to AddChild this line, but there
      // maybe OOF objects. Investigate how to handle them.
    }
  }

  const MarginStrut end_margin_strut = child_layout_result.EndMarginStrut();
  // No margins should pierce outside formatting-context roots.
  DCHECK(!fragment.IsFormattingContextRoot() || end_margin_strut.IsEmpty());

  AddChild(fragment, offset, &end_margin_strut,
           child_layout_result.IsSelfCollapsing(), relative_offset,
           inline_container);
  if (margins) {
    const auto& box_fragment = To<PhysicalBoxFragment>(fragment);
    if (!margins->IsEmpty() || !box_fragment.Margins().IsZero()) {
      box_fragment.GetMutableForContainerLayout().SetMargins(
          margins->ConvertToPhysical(GetWritingDirection()));
    }
  }

  if (has_block_fragmentation_) [[unlikely]] {
    PropagateBreakInfo(*result_for_propagation, offset);
  }
  if (GetConstraintSpace().ShouldPropagateChildBreakValues()) [[unlikely]] {
    PropagateChildBreakValues(*result_for_propagation);
  }

  PropagateFromLayoutResult(*result_for_propagation);
}

void BoxFragmentBuilder::AddResult(const LayoutResult& child_layout_result,
                                   const LogicalOffset offset) {
  AddResult(child_layout_result, offset, std::nullopt, std::nullopt, nullptr);
}

void BoxFragmentBuilder::AddChild(
    const PhysicalFragment& child,
    const LogicalOffset& child_offset,
    const MarginStrut* margin_strut,
    bool is_self_collapsing,
    std::optional<LogicalOffset> relative_offset,
    const OofInlineContainer<LogicalOffset>* inline_container) {
#if DCHECK_IS_ON()
  needs_inflow_bounds_explicitly_set_ = !!relative_offset;
  needs_may_have_descendant_above_block_start_explicitly_set_ =
      !!relative_offset;
#endif

  if (!relative_offset) {
    relative_offset = LogicalOffset();
    if (box_type_ != PhysicalFragment::BoxType::kInlineBox) {
      if (child.IsLineBox()) {
        if (child.MayHaveDescendantAboveBlockStart()) [[unlikely]] {
          may_have_descendant_above_block_start_ = true;
        }
      } else if (child.IsCSSBox()) {
        // Apply the relative position offset.
        const auto& box_child = To<PhysicalBoxFragment>(child);
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
      // scrollable-overflow.
      //
      // This is used for determining the "padding-box" of the scroll container
      // which is *sometimes* considered as part of the scrollable area. Inflow
      // children contribute to this area, out-of-flow positioned children
      // don't.
      //
      // Out-of-flow positioned children still contribute to the
      // scrollable-overflow, but just don't influence where this padding is.
      if (Node().IsScrollContainer() && !IsFragmentainerBoxType() &&
          !child.IsOutOfFlowPositioned()) {
        BoxStrut margins;
        if (child.IsCSSBox()) {
          margins = ComputeMarginsFor(child.Style(),
                                      child_available_size_.inline_size,
                                      GetWritingDirection());
        }

        // If we are in block-flow layout we use the end *margin-strut* as the
        // block-end "margin" (instead of just the block-end margin).
        if (margin_strut) {
          MarginStrut end_margin_strut = *margin_strut;
          end_margin_strut.Append(margins.block_end, /* is_quirky */ false);

          // Self-collapsing blocks are special, their end margin-strut is part
          // of their inflow position. To correctly determine the "end" margin,
          // we need to the "final" margin-strut from their end margin-strut.
          margins.block_end = is_self_collapsing
                                  ? end_margin_strut.Sum() - margin_strut->Sum()
                                  : end_margin_strut.Sum();
        }

        // Use the original offset (*without* relative-positioning applied).
        LogicalFragment fragment(GetWritingDirection(), child);
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
  PropagateFromFragment(child, child_offset, *relative_offset,
                        inline_container);
  AddChildInternal(&child, child_offset + *relative_offset);

  // We have got some content, so follow normal breaking rules from now on.
  SetRequiresContentBeforeBreaking(false);
}

void BoxFragmentBuilder::AddBreakToken(const BreakToken* token,
                                       bool is_in_parallel_flow) {
  // If there's a pre-set break token, we shouldn't be here.
  DCHECK(!break_token_);

  DCHECK(token);
  child_break_tokens_.push_back(token);
  has_inflow_child_break_inside_ |= !is_in_parallel_flow;
}

EBreakBetween BoxFragmentBuilder::JoinedBreakBetweenValue(
    EBreakBetween break_before) const {
  return JoinFragmentainerBreakValues(previous_break_after_, break_before);
}

void BoxFragmentBuilder::MoveChildrenInBlockDirection(LayoutUnit delta) {
  DCHECK(is_new_fc_);
  DCHECK_NE(FragmentBlockSize(), kIndefiniteSize);
  DCHECK(oof_positioned_descendants_.empty());

  has_moved_children_in_block_direction_ = true;

  if (delta == LayoutUnit())
    return;

  if (first_baseline_)
    *first_baseline_ += delta;
  if (last_baseline_)
    *last_baseline_ += delta;

  if (inflow_bounds_)
    inflow_bounds_->offset.block_offset += delta;

  for (auto& child : children_)
    child.offset.block_offset += delta;

  for (auto& candidate : oof_positioned_candidates_)
    candidate.static_position.offset.block_offset += delta;
  for (auto& descendant : oof_positioned_fragmentainer_descendants_) {
    // If we have already returned past (above) the containing block of the OOF
    // (but not all the way the outermost fragmentainer), the containing block
    // is affected by this shift that we just decided to make. This shift wasn't
    // known at the time of normal propagation. So shift accordingly now.
    descendant.containing_block.IncreaseBlockOffset(delta);
    descendant.fixedpos_containing_block.IncreaseBlockOffset(delta);
  }

  if (FragmentItemsBuilder* items_builder = ItemsBuilder()) {
    items_builder->MoveChildrenInBlockDirection(delta);
  }
}

void BoxFragmentBuilder::PropagateBreakInfo(
    const LayoutResult& child_layout_result,
    LogicalOffset offset) {
  DCHECK(has_block_fragmentation_);

  // Include the bounds of this child (in the block direction).
  LayoutUnit block_end_in_container =
      offset.block_offset -
      child_layout_result.AnnotationBlockOffsetAdjustment() +
      BlockSizeForFragmentation(child_layout_result, writing_direction_);

  block_size_for_fragmentation_ =
      std::max(block_size_for_fragmentation_, block_end_in_container);

  if (GetConstraintSpace().RequiresContentBeforeBreaking()) {
    if (child_layout_result.IsBlockSizeForFragmentationClamped())
      is_block_size_for_fragmentation_clamped_ = true;
  }

  const auto& child_fragment = child_layout_result.GetPhysicalFragment();
  const auto* child_box_fragment =
      DynamicTo<PhysicalBoxFragment>(child_fragment);
  const BlockBreakToken* token =
      child_box_fragment ? child_box_fragment->GetBreakToken() : nullptr;

  // Figure out if this child break is in the same flow as this parent. If it's
  // an out-of-flow positioned box, it's not. If it's in a parallel flow, it's
  // also not.
  bool child_is_in_same_flow =
      ((!token || !token->IsAtBlockEnd()) &&
       !child_fragment.IsFloatingOrOutOfFlowPositioned()) ||
      child_layout_result.ShouldForceSameFragmentationFlow();

  // If we're paginated, monolithic overflow will be placed on subsequent pages,
  // even though there are no fragments for the content there. We need to be
  // aware of such overflow when laying out subsequent pages, so that we can
  // move past it, rather than overlapping with it. This approach works (kind
  // of) because in our implementation, pages are stacked in the block
  // direction, so that the block-start offset of the next page is the same as
  // the block-end offset of the preceding page.
  //
  // We need to reserve space for monolithic overflow caused by any child that
  // is in the same flow as its parent, so that subsequent content in this flow
  // gets pushed below the monolithic overflow. If we're at the root, even
  // include content from parallel flows, since we want to encompass everything
  // in that case, in order to create enough pages for it.
  //
  // Some children disable this monolithic overflow propagation, if they are
  // out-of-flow and inside another out-of-flow (so that the containing block
  // chain is broken), and the outer out-of-flow has clipped overflow.
  //
  // TODO(mstensho): Figure out if the !IsFragmentainerBoxType() condition below
  // makes any sense.
  if (GetConstraintSpace().IsPaginated() &&
      ((child_is_in_same_flow && !IsFragmentainerBoxType()) ||
       Node().IsPaginatedRoot()) &&
      (!child_box_fragment ||
       !child_box_fragment->IsMonolithicOverflowPropagationDisabled())) {
    DCHECK(GetConstraintSpace().HasKnownFragmentainerBlockSize());
    // Include overflow inside monolithic content if this is for a page
    // fragment. Otherwise just use the fragment size.
    LayoutUnit block_size;
    if (Node().IsPaginatedRoot() &&
        !child_fragment.HasNonVisibleBlockOverflow()) {
      // The root node is guaranteed to be block-level, so there should be a
      // child box fragment here.
      DCHECK(child_box_fragment);

      LogicalBoxFragment logical_fragment(
          child_box_fragment->Style().GetWritingDirection(),
          *child_box_fragment);
      block_size = logical_fragment.BlockEndScrollableOverflow();
    } else {
      LogicalFragment logical_fragment(
          child_fragment.Style().GetWritingDirection(), child_fragment);
      block_size = logical_fragment.BlockSize();
    }
    LayoutUnit fragment_block_end = offset.block_offset + block_size;
    LayoutUnit fragmentainer_overflow =
        fragment_block_end -
        FragmentainerSpaceLeft(*this, /*is_for_children=*/false);
    if (fragmentainer_overflow > LayoutUnit()) {
      // This child overflows the page, because there's something monolithic
      // inside.
      ReserveSpaceForMonolithicOverflow(fragmentainer_overflow);
    }
  }

  if (IsBreakInside(token)) {
    if (child_is_in_same_flow) {
      has_inflow_child_break_inside_ = true;
    }

    // Downgrade the appeal of breaking inside this container, if the break
    // inside the child is less appealing than what we've found so far.
    BreakAppeal appeal_inside =
        CalculateBreakAppealInside(GetConstraintSpace(), child_layout_result);
    ClampBreakAppeal(appeal_inside);
  }

  if (IsInitialColumnBalancingPass()) {
    PropagateTallestUnbreakableBlockSize(
        child_layout_result.TallestUnbreakableBlockSize());
  }

  if (child_layout_result.HasForcedBreak())
    SetHasForcedBreak();
  else if (!IsInitialColumnBalancingPass())
    PropagateSpaceShortage(child_layout_result.MinimalSpaceShortage());

  if (!child_box_fragment) {
    return;
  }

  // If a spanner was found inside the child, we need to finish up and propagate
  // the spanner to the column layout algorithm, so that it can take care of it.
  if (GetConstraintSpace().IsInColumnBfc()) [[unlikely]] {
    if (const auto* child_spanner_path =
            child_layout_result.GetColumnSpannerPath()) {
      DCHECK(HasInflowChildBreakInside() ||
             !child_layout_result.GetPhysicalFragment().IsBox());
      const auto* spanner_path =
          MakeGarbageCollected<ColumnSpannerPath>(Node(), child_spanner_path);
      SetColumnSpannerPath(spanner_path);
      SetIsEmptySpannerParent(child_layout_result.IsEmptySpannerParent());
    }
  } else {
    DCHECK(!child_layout_result.GetColumnSpannerPath());
  }

  if (!child_box_fragment->IsFragmentainerBox() &&
      !HasOutOfFlowInFragmentainerSubtree()) {
    SetHasOutOfFlowInFragmentainerSubtree(
        child_box_fragment->HasOutOfFlowInFragmentainerSubtree());
  }
}

void BoxFragmentBuilder::PropagateChildBreakValues(
    const LayoutResult& child_layout_result) {
  if (child_layout_result.Status() != LayoutResult::kSuccess) {
    return;
  }

  // Propagate from regular in-flow child blocks, and also from page areas and
  // page border boxes (need to do this for page* boxes in order to propagate
  // page names).
  const auto& fragment = child_layout_result.GetPhysicalFragment();
  if (fragment.IsInline() || !fragment.IsBox() || fragment.IsColumnBox() ||
      fragment.IsFloatingOrOutOfFlowPositioned()) {
    return;
  }

  const ComputedStyle& child_style = fragment.Style();

  // We need to propagate the initial break-before value up our container
  // chain, until we reach a container that's not a first child. If we get all
  // the way to the root of the fragmentation context without finding any such
  // container, we have no valid class A break point, and if a forced break
  // was requested, none will be inserted.
  EBreakBetween break_before = JoinFragmentainerBreakValues(
      child_layout_result.InitialBreakBefore(), child_style.BreakBefore());
  SetInitialBreakBeforeIfNeeded(break_before);

  // We also need to store the previous break-after value we've seen, since it
  // will serve as input to the next breakpoint (where we will combine the
  // break-after value of the previous child and the break-before value of the
  // next child, to figure out what to do at the breakpoint). The break-after
  // value of the last child will also be propagated up our container chain,
  // until we reach a container that's not a last child. This will be the
  // class A break point that it affects.
  EBreakBetween break_after = JoinFragmentainerBreakValues(
      child_layout_result.FinalBreakAfter(), child_style.BreakAfter());
  SetPreviousBreakAfter(break_after);

  SetPageNameIfNeeded(To<PhysicalBoxFragment>(fragment).PageName());
}

void BoxFragmentBuilder::HandleOofsAndSpecialDescendants() {
  OutOfFlowLayoutPart(this).Run();
  if (Style().ScrollMarkerGroup() != EScrollMarkerGroup::kNone &&
      !GetConstraintSpace().IsAnonymous()) {
    Node().HandleScrollMarkerGroup();
  }
}

const LayoutResult* BoxFragmentBuilder::ToBoxFragment(
    WritingMode block_or_line_writing_mode) {
#if DCHECK_IS_ON()
  if (ItemsBuilder()) {
    for (const LogicalFragmentLink& child : Children()) {
      DCHECK(child.fragment);
      const PhysicalFragment& fragment = *child.fragment;
      DCHECK(fragment.IsLineBox() ||
             // TODO(kojii): How to place floats and OOF is TBD.
             fragment.IsFloatingOrOutOfFlowPositioned());
    }
  }
#endif

  if (box_type_ == PhysicalFragment::kNormalBox && node_ &&
      node_.IsBlockInInline()) [[unlikely]] {
    SetIsBlockInInline();
  }

  if (has_block_fragmentation_ && node_) [[unlikely]] {
    if (PreviousBreakToken() && PreviousBreakToken()->IsAtBlockEnd()) {
      // Avoid trailing margin propagation from a node that just has overflowing
      // content here in the current fragmentainer. It's in a parallel flow. If
      // we don't prevent such propagation, the trailing margin may push down
      // subsequent nodes that are being resumed after a break, rather than
      // resuming at the block-start of the fragmentainer.
      end_margin_strut_ = MarginStrut();
    }

    if (!break_token_) {
      if (last_inline_break_token_)
        child_break_tokens_.push_back(std::move(last_inline_break_token_));
      if (DidBreakSelf() || ShouldBreakInside())
        break_token_ = BlockBreakToken::Create(this);
    }

    // Make some final adjustments to block-size for fragmentation, unless this
    // is a fragmentainer (so that we only include the block-size propagated
    // from children in that case).
    if (!PhysicalFragment::IsFragmentainerBoxType(box_type_)) {
      OverflowClipAxes block_axis = GetWritingDirection().IsHorizontal()
                                        ? kOverflowClipY
                                        : kOverflowClipX;
      if ((To<BlockNode>(node_).GetOverflowClipAxes() & block_axis) ||
          is_block_size_for_fragmentation_clamped_) {
        // If block-axis overflow is clipped, ignore child overflow and just use
        // the border-box size of the fragment itself. Also do this if the node
        // was forced to stay in the current fragmentainer. We'll ignore
        // overflow in such cases, because children are allowed to overflow
        // without affecting fragmentation then.
        block_size_for_fragmentation_ = FragmentBlockSize();
      } else {
        // Include the border-box size of the fragment itself.
        block_size_for_fragmentation_ =
            std::max(block_size_for_fragmentation_, FragmentBlockSize());
      }

      // If the node fits inside the current fragmentainer, any break inside it
      // will establish a parallel flow, which means that breaking early inside
      // it isn't going to help honor any break avoidance requests on content
      // that comes after this node. So don't propagate it.
      if (IsKnownToFitInFragmentainer())
        early_break_ = nullptr;
    }
  }

  const PhysicalBoxFragment* fragment =
      PhysicalBoxFragment::Create(this, block_or_line_writing_mode);
  fragment->CheckType();

  return MakeGarbageCollected<LayoutResult>(
      LayoutResult::BoxFragmentBuilderPassKey(), std::move(fragment), this);
}

void BoxFragmentBuilder::AdjustFragmentainerDescendant(
    LogicalOofNodeForFragmentation& descendant,
    bool only_fixedpos_containing_block) {
  LayoutUnit previous_consumed_block_size;
  if (PreviousBreakToken())
    previous_consumed_block_size = PreviousBreakToken()->ConsumedBlockSize();

  // If the containing block is fragmented, adjust the offset to be from the
  // first containing block fragment to the fragmentation context root. Also,
  // adjust the static position to be relative to the adjusted containing block
  // offset.
  if (!only_fixedpos_containing_block &&
      !descendant.containing_block.Fragment()) {
    descendant.containing_block.IncreaseBlockOffset(
        -previous_consumed_block_size);
    descendant.static_position.offset.block_offset +=
        previous_consumed_block_size;
  }

  // If the fixedpos containing block is fragmented, adjust the offset to be
  // from the first containing block fragment to the fragmentation context root.
  if (!descendant.fixedpos_containing_block.Fragment() &&
      (node_.IsFixedContainer() ||
       descendant.fixedpos_inline_container.container)) {
    descendant.fixedpos_containing_block.IncreaseBlockOffset(
        -previous_consumed_block_size);
  }
}

void BoxFragmentBuilder::
    AdjustFixedposContainingBlockForFragmentainerDescendants() {
  if (!HasOutOfFlowFragmentainerDescendants())
    return;

  for (auto& descendant : oof_positioned_fragmentainer_descendants_) {
    AdjustFragmentainerDescendant(descendant,
                                  /* only_fixedpos_containing_block */ true);
  }
}

void BoxFragmentBuilder::AdjustFixedposContainingBlockForInnerMulticols() {
  if (!HasMulticolsWithPendingOOFs() || !PreviousBreakToken())
    return;

  // If the fixedpos containing block is fragmented, adjust the offset to be
  // from the first containing block fragment to the fragmentation context root.
  // Also, update the multicol offset such that it is relative to the fixedpos
  // containing block.
  LayoutUnit previous_consumed_block_size =
      PreviousBreakToken()->ConsumedBlockSize();
  for (auto& multicol : multicols_with_pending_oofs_) {
    MulticolWithPendingOofs<LogicalOffset>& value = *multicol.value;
    if (!value.fixedpos_containing_block.Fragment() &&
        (node_.IsFixedContainer() ||
         value.fixedpos_inline_container.container)) {
      value.fixedpos_containing_block.IncreaseBlockOffset(
          -previous_consumed_block_size);
      value.multicol_offset.block_offset += previous_consumed_block_size;
    }
  }
}

#if DCHECK_IS_ON()

void BoxFragmentBuilder::CheckNoBlockFragmentation() const {
  DCHECK(!ShouldBreakInside());
  DCHECK(!HasInflowChildBreakInside());
  DCHECK(!DidBreakSelf());
  DCHECK(!has_forced_break_);
  DCHECK(GetConstraintSpace().ShouldRepeat() || !HasBreakTokenData());
  DCHECK_EQ(minimal_space_shortage_, kIndefiniteSize);
  if (!GetConstraintSpace().ShouldPropagateChildBreakValues()) {
    DCHECK(!initial_break_before_);
    DCHECK_EQ(previous_break_after_, EBreakBetween::kAuto);
  }
}

#endif

}  // namespace blink
