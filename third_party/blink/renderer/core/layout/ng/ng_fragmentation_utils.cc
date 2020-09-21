// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

// At a class A break point [1], the break value with the highest precedence
// wins. If the two values have the same precedence (e.g. "left" and "right"),
// the value specified on a latter object wins.
//
// [1] https://drafts.csswg.org/css-break/#possible-breaks
inline int FragmentainerBreakPrecedence(EBreakBetween break_value) {
  // "auto" has the lowest priority.
  // "avoid*" values win over "auto".
  // "avoid-page" wins over "avoid-column".
  // "avoid" wins over "avoid-page".
  // Forced break values win over "avoid".
  // Any forced page break value wins over "column" forced break.
  // More specific break values (left, right, recto, verso) wins over generic
  // "page" values.

  switch (break_value) {
    default:
      NOTREACHED();
      FALLTHROUGH;
    case EBreakBetween::kAuto:
      return 0;
    case EBreakBetween::kAvoidColumn:
      return 1;
    case EBreakBetween::kAvoidPage:
      return 2;
    case EBreakBetween::kAvoid:
      return 3;
    case EBreakBetween::kColumn:
      return 4;
    case EBreakBetween::kPage:
      return 5;
    case EBreakBetween::kLeft:
    case EBreakBetween::kRight:
    case EBreakBetween::kRecto:
    case EBreakBetween::kVerso:
      return 6;
  }
}

EBreakBetween JoinFragmentainerBreakValues(EBreakBetween first_value,
                                           EBreakBetween second_value) {
  if (FragmentainerBreakPrecedence(second_value) >=
      FragmentainerBreakPrecedence(first_value))
    return second_value;
  return first_value;
}

bool IsForcedBreakValue(const NGConstraintSpace& constraint_space,
                        EBreakBetween break_value) {
  if (break_value == EBreakBetween::kColumn)
    return constraint_space.BlockFragmentationType() == kFragmentColumn;
  // TODO(mstensho): The innermost fragmentation type doesn't tell us everything
  // here. We might want to force a break to the next page, even if we're in
  // multicol (printing multicol, for instance).
  if (break_value == EBreakBetween::kLeft ||
      break_value == EBreakBetween::kPage ||
      break_value == EBreakBetween::kRecto ||
      break_value == EBreakBetween::kRight ||
      break_value == EBreakBetween::kVerso)
    return constraint_space.BlockFragmentationType() == kFragmentPage;
  return false;
}

template <typename Property>
bool IsAvoidBreakValue(const NGConstraintSpace& constraint_space,
                       Property break_value) {
  if (break_value == Property::kAvoid)
    return constraint_space.HasBlockFragmentation();
  if (break_value == Property::kAvoidColumn)
    return constraint_space.BlockFragmentationType() == kFragmentColumn;
  // TODO(mstensho): The innermost fragmentation type doesn't tell us everything
  // here. We might want to avoid breaking to the next page, even if we're
  // in multicol (printing multicol, for instance).
  if (break_value == Property::kAvoidPage)
    return constraint_space.BlockFragmentationType() == kFragmentPage;
  return false;
}
// The properties break-after, break-before and break-inside may all specify
// avoid* values. break-after and break-before use EBreakBetween, and
// break-inside uses EBreakInside.
template bool CORE_TEMPLATE_EXPORT IsAvoidBreakValue(const NGConstraintSpace&,
                                                     EBreakBetween);
template bool CORE_TEMPLATE_EXPORT IsAvoidBreakValue(const NGConstraintSpace&,
                                                     EBreakInside);

EBreakBetween CalculateBreakBetweenValue(NGLayoutInputNode child,
                                         const NGLayoutResult& layout_result,
                                         const NGBoxFragmentBuilder& builder) {
  if (child.IsInline())
    return EBreakBetween::kAuto;
  EBreakBetween break_before = JoinFragmentainerBreakValues(
      child.Style().BreakBefore(), layout_result.InitialBreakBefore());
  return builder.JoinedBreakBetweenValue(break_before);
}

NGBreakAppeal CalculateBreakAppealBefore(const NGConstraintSpace& space,
                                         NGLayoutInputNode child,
                                         const NGLayoutResult& layout_result,
                                         const NGBoxFragmentBuilder& builder,
                                         bool has_container_separation) {
  if (!has_container_separation) {
    // This is not a valid break point. If there's no container separation, it
    // means that we're breaking before the first piece of in-flow content
    // inside this block, even if it's not a valid class C break point [1]. We
    // really don't want to break here, if we can find something better.
    //
    // [1] https://www.w3.org/TR/css-break-3/#possible-breaks
    return kBreakAppealLastResort;
  }

  EBreakBetween break_between =
      CalculateBreakBetweenValue(child, layout_result, builder);
  // If there's a break-{after,before}:avoid* involved at this breakpoint,
  // its appeal will decrease.
  if (IsAvoidBreakValue(space, break_between))
    return kBreakAppealViolatingBreakAvoid;

  return kBreakAppealPerfect;
}

NGBreakAppeal CalculateBreakAppealInside(const NGConstraintSpace& space,
                                         NGBlockNode child,
                                         const NGLayoutResult& layout_result) {
  if (layout_result.HasForcedBreak())
    return kBreakAppealPerfect;
  const auto& physical_fragment = layout_result.PhysicalFragment();
  NGBreakAppeal appeal;
  // If we actually broke, get the appeal from the break token. Otherwise, get
  // the early break appeal.
  if (const auto* block_break_token =
          DynamicTo<NGBlockBreakToken>(physical_fragment.BreakToken()))
    appeal = block_break_token->BreakAppeal();
  else
    appeal = layout_result.EarlyBreakAppeal();

  // We don't let break-inside:avoid affect the child's stored break appeal, but
  // we rather handle it now, on the outside. The reason is that we want to be
  // able to honor any 'avoid' values on break-before or break-after among the
  // children of the child, even if we need to disregrard a break-inside:avoid
  // rule on the child itself. This prevents us from violating more rules than
  // necessary: if we need to break inside the child (even if it should be
  // avoided), we'll at least break at the most appealing location inside.
  if (appeal > kBreakAppealViolatingBreakAvoid &&
      IsAvoidBreakValue(space, child.Style().BreakInside()))
    appeal = kBreakAppealViolatingBreakAvoid;
  return appeal;
}

void SetupSpaceBuilderForFragmentation(const NGConstraintSpace& parent_space,
                                       const NGLayoutInputNode& child,
                                       LayoutUnit fragmentainer_offset_delta,
                                       NGConstraintSpaceBuilder* builder,
                                       bool is_new_fc) {
  DCHECK(parent_space.HasBlockFragmentation());

  // If the child is truly unbreakable, it won't participate in block
  // fragmentation. If it's too tall to fit, it will either overflow the
  // fragmentainer or get brutally sliced into pieces (without looking for
  // allowed breakpoints, since there are none, by definition), depending on
  // fragmentation type (multicol vs. printing). We still need to perform block
  // fragmentation inside inline nodes, though: While the line box itself is
  // monolithic, there may be floats inside, which are fragmentable.
  if (child.IsMonolithic() && !child.IsInline())
    return;

  builder->SetFragmentainerBlockSize(parent_space.FragmentainerBlockSize());
  builder->SetFragmentainerOffsetAtBfc(parent_space.FragmentainerOffsetAtBfc() +
                                       fragmentainer_offset_delta);
  builder->SetFragmentationType(parent_space.BlockFragmentationType());

  if (parent_space.IsInColumnBfc() && !is_new_fc)
    builder->SetIsInColumnBfc();
}

void SetupFragmentBuilderForFragmentation(
    const NGConstraintSpace& space,
    const NGBlockBreakToken* previous_break_token,
    NGBoxFragmentBuilder* builder) {
  builder->SetHasBlockFragmentation();

  // The whereabouts of our container's so far best breakpoint is none of our
  // business, but we store its appeal, so that we don't look for breakpoints
  // with lower appeal than that.
  builder->SetBreakAppeal(space.EarlyBreakAppeal());

  if (space.IsInitialColumnBalancingPass())
    builder->SetIsInitialColumnBalancingPass();

  unsigned sequence_number = 0;
  if (previous_break_token && !previous_break_token->IsBreakBefore()) {
    sequence_number = previous_break_token->SequenceNumber() + 1;
    builder->SetIsFirstForNode(false);
  }
  builder->SetSequenceNumber(sequence_number);

  builder->AdjustBorderScrollbarPaddingForFragmentation(previous_break_token);
}

bool IsNodeFullyGrown(NGBlockNode node,
                      const NGConstraintSpace& space,
                      LayoutUnit current_total_block_size,
                      const NGBoxStrut& border_padding,
                      LayoutUnit inline_size) {
  // Pass an "infinite" intrinsic size to see how the block-size is
  // constrained. If it doesn't affect the block size, it means that the node
  // cannot grow any further.
  LayoutUnit max_block_size = ComputeBlockSizeForFragment(
      space, node.Style(), border_padding, LayoutUnit::Max(), inline_size);
  DCHECK_GE(max_block_size, current_total_block_size);
  return max_block_size == current_total_block_size;
}

bool FinishFragmentation(NGBlockNode node,
                         const NGConstraintSpace& space,
                         const NGBlockBreakToken* previous_break_token,
                         const NGBoxStrut& border_padding,
                         LayoutUnit space_left,
                         NGBoxFragmentBuilder* builder) {
  LayoutUnit previously_consumed_block_size;
  if (previous_break_token && !previous_break_token->IsBreakBefore())
    previously_consumed_block_size = previous_break_token->ConsumedBlockSize();

  LayoutUnit fragments_total_block_size = builder->FragmentsTotalBlockSize();
  LayoutUnit desired_block_size =
      fragments_total_block_size - previously_consumed_block_size;
  DCHECK_GE(desired_block_size, LayoutUnit());
  LayoutUnit intrinsic_block_size = builder->IntrinsicBlockSize();

  LayoutUnit final_block_size = desired_block_size;
  if (builder->FoundColumnSpanner()) {
    // There's a column spanner (or more) inside. This means that layout got
    // interrupted and thus hasn't reached the end of this block yet. We're
    // going to resume inside this block when done with the spanner(s). This is
    // true even if there is no column content siblings after the spanner(s).
    //
    // <div style="columns:2;">
    //   <div id="container" style="height:100px;">
    //     <div id="child" style="height:20px;"></div>
    //     <div style="column-span:all;"></div>
    //   </div>
    // </div>
    //
    // We'll create fragments for #container both before and after the spanner.
    // Before the spanner we'll create one for each column, each 10px tall
    // (height of #child divided into 2 columns). After the spanner, there's no
    // more content, but the specified height is 100px, so distribute what we
    // haven't already consumed (100px - 20px = 80px) over two columns. We get
    // two fragments for #container after the spanner, each 40px tall.
    final_block_size = std::min(final_block_size, intrinsic_block_size) -
                       border_padding.block_end;
    builder->SetDidBreakSelf();
  } else if (space_left != kIndefiniteSize && desired_block_size > space_left) {
    // We're taller than what we have room for. We don't want to use more than
    // |space_left|, but if the intrinsic block-size is larger than that, it
    // means that there's something unbreakable (monolithic) inside (or we'd
    // already have broken inside). We'll allow this to overflow the
    // fragmentainer.
    //
    // TODO(mstensho): This is desired behavior for multicol, but not ideal for
    // printing, where we'd prefer the unbreakable content to be sliced into
    // different pages, lest it be clipped and lost.
    //
    // There is a last-resort breakpoint before trailing border and padding, so
    // first check if we can break there and still make progress.
    DCHECK_GE(intrinsic_block_size, border_padding.block_end);
    DCHECK_GE(desired_block_size, border_padding.block_end);

    LayoutUnit subtractable_border_padding;
    if (desired_block_size > border_padding.block_end)
      subtractable_border_padding = border_padding.block_end;

    final_block_size =
        std::min(desired_block_size - subtractable_border_padding,
                 std::max(space_left,
                          intrinsic_block_size - subtractable_border_padding));

    // We'll only need to break inside if we need more space after any
    // unbreakable content that we may have forcefully fitted here.
    if (final_block_size < desired_block_size)
      builder->SetDidBreakSelf();
  }

  LogicalBoxSides sides;
  if (previously_consumed_block_size)
    sides.block_start = false;
  if (builder->DidBreakSelf())
    sides.block_end = false;
  builder->SetSidesToInclude(sides);

  builder->SetConsumedBlockSize(previously_consumed_block_size +
                                final_block_size);
  builder->SetFragmentBlockSize(final_block_size);

  if (builder->FoundColumnSpanner())
    return true;

  if (space_left == kIndefiniteSize) {
    // We don't know how space is available (initial column balancing pass), so
    // we won't break.
    builder->SetIsAtBlockEnd();
    return true;
  }

  if (builder->HasChildBreakInside()) {
    // We broke before or inside one of our children. Even if we fit within the
    // remaining space, and even if the child involved in the break were to be
    // in a parallel flow, we still need to prepare a break token for this node,
    // so that we can resume layout of its broken or unstarted children in the
    // next fragmentainer.
    //
    // If we're at the end of the node, we need to mark the outgoing break token
    // as such. This is a way for the parent algorithm to determine whether we
    // need to insert a break there, or whether we may continue with any sibling
    // content. If we are allowed to continue, while there's still child content
    // left to be laid out, said content ends up in a parallel flow.
    // https://www.w3.org/TR/css-break-3/#parallel-flows
    //
    // TODO(mstensho): The spec actually says that we enter a parallel flow once
    // we're past the block-end *content edge*, but here we're checking against
    // the *border edge* instead. Does it matter?
    if (previous_break_token && previous_break_token->IsAtBlockEnd()) {
      builder->SetIsAtBlockEnd();
      // We entered layout already at the end of the block (but with overflowing
      // children). So we should take up no more space on our own.
      DCHECK_EQ(desired_block_size, LayoutUnit());
    } else if (desired_block_size <= space_left) {
      // We have room for the calculated block-size in the current
      // fragmentainer, but we need to figure out whether this node is going to
      // produce more non-zero block-size fragments or not.
      //
      // If the block-size is constrained / fixed (in which case
      // IsNodeFullyGrown() will return true now), we know that we're at the
      // end. If block-size is unconstrained (or at least allowed to grow a bit
      // more), we're only at the end if no in-flow content inside broke.
      if (!builder->HasInflowChildBreakInside() ||
          IsNodeFullyGrown(node, space, fragments_total_block_size,
                           border_padding,
                           builder->InitialBorderBoxSize().inline_size))
        builder->SetIsAtBlockEnd();

      // If we're going to break just because of floats or out-of-flow child
      // breaks, no break appeal will have been recorded so far, since we only
      // update the appeal at same-flow breakpoints, and since we start off by
      // assuming the lowest appeal, upgrade it now. There's nothing here that
      // makes breaking inside less appealing than perfect.
      if (!builder->HasInflowChildBreakInside())
        builder->SetBreakAppeal(kBreakAppealPerfect);
    }
    return true;
  }

  if (desired_block_size > space_left) {
    // No child inside broke, but we're too tall to fit.
    NGBreakAppeal break_appeal = kBreakAppealPerfect;
    if (!previously_consumed_block_size) {
      // This is the first fragment generated for the node. Avoid breaking
      // inside block-start border, scrollbar and padding, if possible. No valid
      // breakpoints there.
      const NGFragmentGeometry& geometry = builder->InitialFragmentGeometry();
      LayoutUnit block_start_unbreakable_space =
          geometry.border.block_start + geometry.scrollbar.block_start +
          geometry.padding.block_start;
      if (space_left < block_start_unbreakable_space)
        break_appeal = kBreakAppealLastResort;
    }
    if (space.BlockFragmentationType() == kFragmentColumn &&
        !space.IsInitialColumnBalancingPass())
      builder->PropagateSpaceShortage(desired_block_size - space_left);
    if (desired_block_size <= intrinsic_block_size) {
      // We only want to break inside if there's a valid class C breakpoint [1].
      // That is, we need a non-zero gap between the last child (outer block-end
      // edge) and this container (inner block-end edge). We've just found that
      // not to be the case. If we have found a better early break, we should
      // break there. Otherwise mark the break as unappealing, as breaking here
      // means that we're going to break inside the block-end padding or border,
      // or right before them. No valid breakpoints there.
      //
      // [1] https://www.w3.org/TR/css-break-3/#possible-breaks
      if (builder->HasEarlyBreak())
        return false;
      break_appeal = kBreakAppealLastResort;
    }
    builder->SetBreakAppeal(break_appeal);
    return true;
  }

  // The end of the block fits in the current fragmentainer.
  builder->SetIsAtBlockEnd();
  return true;
}

NGBreakStatus BreakBeforeChildIfNeeded(const NGConstraintSpace& space,
                                       NGLayoutInputNode child,
                                       const NGLayoutResult& layout_result,
                                       LayoutUnit fragmentainer_block_offset,
                                       bool has_container_separation,
                                       NGBoxFragmentBuilder* builder) {
  DCHECK(space.HasBlockFragmentation());

  if (has_container_separation) {
    EBreakBetween break_between =
        CalculateBreakBetweenValue(child, layout_result, *builder);
    if (IsForcedBreakValue(space, break_between)) {
      BreakBeforeChild(space, child, layout_result, fragmentainer_block_offset,
                       kBreakAppealPerfect, /* is_forced_break */ true,
                       builder);
      return NGBreakStatus::kBrokeBefore;
    }
  }

  NGBreakAppeal appeal_before = CalculateBreakAppealBefore(
      space, child, layout_result, *builder, has_container_separation);

  // Attempt to move past the break point, and if we can do that, also assess
  // the appeal of breaking there, even if we didn't.
  if (MovePastBreakpoint(space, child, layout_result,
                         fragmentainer_block_offset, appeal_before, builder))
    return NGBreakStatus::kContinue;

  // Breaking inside the child isn't appealing, and we're out of space. Figure
  // out where to insert a soft break. It will either be before this child, or
  // before an earlier sibling, if there's a more appealing breakpoint there.
  if (!AttemptSoftBreak(space, child, layout_result, fragmentainer_block_offset,
                        appeal_before, builder))
    return NGBreakStatus::kNeedsEarlierBreak;

  return NGBreakStatus::kBrokeBefore;
}

void BreakBeforeChild(const NGConstraintSpace& space,
                      NGLayoutInputNode child,
                      const NGLayoutResult& layout_result,
                      LayoutUnit fragmentainer_block_offset,
                      base::Optional<NGBreakAppeal> appeal,
                      bool is_forced_break,
                      NGBoxFragmentBuilder* builder) {
#if DCHECK_IS_ON()
  if (layout_result.Status() == NGLayoutResult::kSuccess) {
    // In order to successfully break before a node, this has to be its first
    // fragment.
    const auto& physical_fragment = layout_result.PhysicalFragment();
    DCHECK(!physical_fragment.IsBox() ||
           To<NGPhysicalBoxFragment>(physical_fragment).IsFirstForNode());
  }
#endif

  // Report space shortage. Note that we're not doing this for line boxes here
  // (only blocks), because line boxes need handle it in their own way (due to
  // how we implement widows).
  if (child.IsBlock() && space.HasKnownFragmentainerBlockSize()) {
    PropagateSpaceShortage(space, layout_result, fragmentainer_block_offset,
                           builder);
  }

  // If the fragmentainer block-size is unknown, we have no reason to insert
  // soft breaks.
  DCHECK(is_forced_break || space.HasKnownFragmentainerBlockSize());

  // We'll drop the fragment (if any) on the floor and retry at the start of the
  // next fragmentainer.
  builder->AddBreakBeforeChild(child, appeal, is_forced_break);
}

void PropagateSpaceShortage(const NGConstraintSpace& space,
                            const NGLayoutResult& layout_result,
                            LayoutUnit fragmentainer_block_offset,
                            NGBoxFragmentBuilder* builder) {
  // Space shortage is only reported for soft breaks, and they can only exist if
  // we know the fragmentainer block-size.
  DCHECK(space.HasKnownFragmentainerBlockSize());

  // Only multicol cares about space shortage.
  if (space.BlockFragmentationType() != kFragmentColumn)
    return;

  LayoutUnit space_shortage;
  if (layout_result.MinimalSpaceShortage() == LayoutUnit::Max()) {
    // Calculate space shortage: Figure out how much more space would have been
    // sufficient to make the child fragment fit right here in the current
    // fragmentainer. If layout aborted, though, we can't propagate anything.
    if (layout_result.Status() != NGLayoutResult::kSuccess)
      return;
    NGFragment fragment(space.GetWritingMode(),
                        layout_result.PhysicalFragment());
    space_shortage = fragmentainer_block_offset + fragment.BlockSize() -
                     space.FragmentainerBlockSize();
  } else {
    // However, if space shortage was reported inside the child, use that. If we
    // broke inside the child, we didn't complete layout, so calculating space
    // shortage for the child as a whole would be impossible and pointless.
    space_shortage = layout_result.MinimalSpaceShortage();
  }

  // TODO(mstensho): Turn this into a DCHECK, when the engine is ready for
  // it. Space shortage should really be positive here, or we might ultimately
  // fail to stretch the columns (column balancing).
  if (space_shortage > LayoutUnit())
    builder->PropagateSpaceShortage(space_shortage);
}

bool MovePastBreakpoint(const NGConstraintSpace& space,
                        NGLayoutInputNode child,
                        const NGLayoutResult& layout_result,
                        LayoutUnit fragmentainer_block_offset,
                        NGBreakAppeal appeal_before,
                        NGBoxFragmentBuilder* builder) {
  if (layout_result.Status() != NGLayoutResult::kSuccess) {
    // Layout aborted - no fragment was produced. There's nothing to move
    // past. We need to break before.
    DCHECK_EQ(layout_result.Status(), NGLayoutResult::kOutOfFragmentainerSpace);
    return false;
  }

  const auto& physical_fragment = layout_result.PhysicalFragment();
  NGFragment fragment(space.GetWritingMode(), physical_fragment);

  if (!space.HasKnownFragmentainerBlockSize()) {
    if (space.IsInitialColumnBalancingPass() && builder) {
      if (child.IsMonolithic() ||
          (child.IsBlock() &&
           IsAvoidBreakValue(space, child.Style().BreakInside()))) {
        // If this is the initial column balancing pass, attempt to make the
        // column block-size at least as large as the tallest piece of
        // monolithic content and/or block with break-inside:avoid.
        PropagateUnbreakableBlockSize(fragment.BlockSize(),
                                      fragmentainer_block_offset, builder);
      }
    }
    // We only care about soft breaks if we have a fragmentainer block-size.
    // During column balancing this may be unknown.
    return true;
  }

  LayoutUnit space_left =
      space.FragmentainerBlockSize() - fragmentainer_block_offset;

  // If we haven't used any space at all in the fragmentainer yet, we cannot
  // break before this child, or there'd be no progress. We'd risk creating an
  // infinite number of fragmentainers without putting any content into them.
  bool refuse_break_before = space_left >= space.FragmentainerBlockSize();

  // If the child starts past the end of the fragmentainer (probably due to a
  // block-start margin), we must break before it.
  bool must_break_before = space_left < LayoutUnit();
  if (must_break_before) {
    DCHECK(!refuse_break_before);
    return false;
  }

  if (IsA<NGBlockBreakToken>(physical_fragment.BreakToken())) {
    // The block child broke inside. We now need to decide whether to keep that
    // break, or if it would be better to break before it.
    NGBreakAppeal appeal_inside = CalculateBreakAppealInside(
        space, To<NGBlockNode>(child), layout_result);
    // Allow breaking inside if it has the same appeal or higher than breaking
    // before or breaking earlier. Also, if breaking before is impossible, break
    // inside regardless of appeal.
    bool want_break_inside = refuse_break_before;
    if (!want_break_inside && appeal_inside >= appeal_before) {
      if (!builder || !builder->HasEarlyBreak() ||
          appeal_inside >= builder->BreakAppeal())
        want_break_inside = true;
    }
    if (want_break_inside) {
      if (builder)
        builder->SetBreakAppeal(appeal_inside);
      return true;
    }
  } else {
    bool need_break;
    if (refuse_break_before) {
      need_break = false;
    } else if (child.IsMonolithic()) {
      // If the monolithic piece of content (e.g. a line, or block-level
      // replaced content) doesn't fit, we need a break.
      need_break = fragment.BlockSize() > space_left;
    } else {
      // If the block-offset is past the fragmentainer boundary (or exactly at
      // the boundary), no part of the fragment is going to fit in the current
      // fragmentainer. Fragments may be pushed past the fragmentainer boundary
      // by margins. We shouldn't break before a zero-size block that's exactly
      // at a fragmentainer boundary, though.
      need_break = space_left < LayoutUnit() ||
                   (space_left == LayoutUnit() && fragment.BlockSize());
    }

    if (!need_break) {
      if (child.IsBlock() && builder) {
        // If this doesn't happen, though, we're tentatively not going to break
        // before or inside this child, but we'll check the appeal of breaking
        // there anyway. It may be the best breakpoint we'll ever find. (Note
        // that we only do this for block children, since, when it comes to
        // inline layout, we first need to lay out all the line boxes, so that
        // we know what do to in order to honor orphans and widows, if at all
        // possible.)
        UpdateEarlyBreakAtBlockChild(space, To<NGBlockNode>(child),
                                     layout_result, appeal_before, builder);
      }
      return true;
    }
  }

  // We don't want to break inside, so we should attempt to break before.
  return false;
}

void UpdateEarlyBreakAtBlockChild(const NGConstraintSpace& space,
                                  NGBlockNode child,
                                  const NGLayoutResult& layout_result,
                                  NGBreakAppeal appeal_before,
                                  NGBoxFragmentBuilder* builder) {
  // If the child already broke, it's a little too late to look for breakpoints.
  DCHECK(!layout_result.PhysicalFragment().BreakToken());

  // See if there's a good breakpoint inside the child.
  NGBreakAppeal appeal_inside = kBreakAppealLastResort;
  if (scoped_refptr<const NGEarlyBreak> breakpoint =
          layout_result.GetEarlyBreak()) {
    appeal_inside = CalculateBreakAppealInside(space, child, layout_result);
    if (builder->BreakAppeal() <= appeal_inside) {
      // Found a good breakpoint inside the child. Add the child to the early
      // break container chain, and store it.
      auto parent_break = base::AdoptRef(new NGEarlyBreak(child, breakpoint));
      builder->SetEarlyBreak(parent_break, appeal_inside);
    }
  }

  // Breaking before isn't better if it's less appealing than what we already
  // have (obviously), and also not if it has the same appeal as the break
  // location inside the child that we just found (when the appeal is the same,
  // whatever takes us further wins).
  if (appeal_before < builder->BreakAppeal() || appeal_before == appeal_inside)
    return;

  builder->SetEarlyBreak(base::AdoptRef(new NGEarlyBreak(child)),
                         appeal_before);
}

bool AttemptSoftBreak(const NGConstraintSpace& space,
                      NGLayoutInputNode child,
                      const NGLayoutResult& layout_result,
                      LayoutUnit fragmentainer_block_offset,
                      NGBreakAppeal appeal_before,
                      NGBoxFragmentBuilder* builder) {
  // if there's a breakpoint with higher appeal among earlier siblings, we need
  // to abort and re-layout to that breakpoint.
  if (builder->HasEarlyBreak() && builder->BreakAppeal() > appeal_before) {
    // Found a better place to break. Before aborting, calculate and report
    // space shortage from where we'd actually break.
    PropagateSpaceShortage(space, layout_result, fragmentainer_block_offset,
                           builder);
    return false;
  }

  // Break before the child. Note that there may be a better break further up
  // with higher appeal (but it's too early to tell), in which case this
  // breakpoint will be replaced.
  BreakBeforeChild(space, child, layout_result, fragmentainer_block_offset,
                   appeal_before, /* is_forced_break */ false, builder);
  return true;
}

NGConstraintSpace CreateConstraintSpaceForColumns(
    const NGConstraintSpace& parent_space,
    LogicalSize column_size,
    LogicalSize percentage_resolution_size,
    bool is_first_fragmentainer,
    bool balance_columns) {
  NGConstraintSpaceBuilder space_builder(
      parent_space, parent_space.GetWritingMode(), /* is_new_fc */ true);
  space_builder.SetAvailableSize(column_size);
  space_builder.SetPercentageResolutionSize(percentage_resolution_size);

  // To ensure progression, we need something larger than 0 here. The spec
  // actually says that fragmentainers have to accept at least 1px of content.
  // See https://www.w3.org/TR/css-break-3/#breaking-rules
  LayoutUnit column_block_size =
      std::max(column_size.block_size, LayoutUnit(1));

  space_builder.SetFragmentationType(kFragmentColumn);
  space_builder.SetFragmentainerBlockSize(column_block_size);
  space_builder.SetIsAnonymous(true);
  space_builder.SetIsInColumnBfc();
  if (balance_columns)
    space_builder.SetIsInsideBalancedColumns();
  if (!is_first_fragmentainer) {
    // Margins at fragmentainer boundaries should be eaten and truncated to
    // zero. Note that this doesn't apply to margins at forced breaks, but we'll
    // deal with those when we get to them. Set up a margin strut that eats all
    // leading adjacent margins.
    space_builder.SetDiscardingMarginStrut();
  }

  return space_builder.ToConstraintSpace();
}

}  // namespace blink
