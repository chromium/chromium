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

void SetupFragmentation(const NGConstraintSpace& parent_space,
                        LayoutUnit fragmentainer_offset_delta,
                        NGConstraintSpaceBuilder* builder,
                        bool is_new_fc) {
  DCHECK(parent_space.HasBlockFragmentation());

  builder->SetFragmentainerBlockSize(parent_space.FragmentainerBlockSize());
  builder->SetFragmentainerOffsetAtBfc(parent_space.FragmentainerOffsetAtBfc() +
                                       fragmentainer_offset_delta);
  builder->SetFragmentationType(parent_space.BlockFragmentationType());

  if (parent_space.IsInColumnBfc() && !is_new_fc)
    builder->SetIsInColumnBfc();
}

void FinishFragmentation(const NGConstraintSpace& space,
                         LayoutUnit block_size,
                         LayoutUnit intrinsic_block_size,
                         LayoutUnit previously_consumed_block_size,
                         LayoutUnit space_left,
                         NGBoxFragmentBuilder* builder) {
  if (builder->DidBreak()) {
    // One of our children broke. Even if we fit within the remaining space, we
    // need to prepare a break token.
    builder->SetConsumedBlockSize(std::min(space_left, block_size) +
                                  previously_consumed_block_size);
    builder->SetBlockSize(std::min(space_left, block_size));
    builder->SetIntrinsicBlockSize(space_left);
    return;
  }

  if (block_size > space_left) {
    // Need a break inside this block.
    builder->SetConsumedBlockSize(space_left + previously_consumed_block_size);
    builder->SetDidBreak();
    builder->SetBreakAppeal(kBreakAppealPerfect);
    builder->SetBlockSize(space_left);
    builder->SetIntrinsicBlockSize(space_left);
    if (space.BlockFragmentationType() == kFragmentColumn &&
        !space.IsInitialColumnBalancingPass())
      builder->PropagateSpaceShortage(block_size - space_left);
    return;
  }

  // The end of the block fits in the current fragmentainer.
  builder->SetConsumedBlockSize(previously_consumed_block_size + block_size);
  builder->SetBlockSize(block_size);
  builder->SetIntrinsicBlockSize(intrinsic_block_size);
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
                      NGBreakAppeal appeal,
                      bool is_forced_break,
                      NGBoxFragmentBuilder* builder) {
#if DCHECK_IS_ON()
  // In order to successfully break before a node, this has to be its first
  // fragment.
  const auto& physical_fragment = layout_result.PhysicalFragment();
  DCHECK(!physical_fragment.IsBox() ||
         To<NGPhysicalBoxFragment>(physical_fragment).IsFirstForNode());
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
    // sufficient to make the child fit right here in the current fragment.
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
  const auto& physical_fragment = layout_result.PhysicalFragment();
  NGFragment fragment(space.GetWritingMode(), physical_fragment);

  if (!space.HasKnownFragmentainerBlockSize()) {
    if (space.IsInitialColumnBalancingPass()) {
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

  if (IsA<NGBlockBreakToken>(physical_fragment.BreakToken())) {
    // The block child broke inside. We now need to decide whether to keep that
    // break, or if it would be better to break before it.
    NGBreakAppeal appeal_inside = CalculateBreakAppealInside(
        space, To<NGBlockNode>(child), layout_result);
    // Allow breaking inside if it has the same appeal or higher than breaking
    // before or breaking earlier.
    if (appeal_inside >= appeal_before &&
        (!builder->HasEarlyBreak() ||
         appeal_inside >= builder->BreakAppeal())) {
      builder->SetBreakAppeal(appeal_inside);
      return true;
    }
  } else {
    LayoutUnit space_left =
        space.FragmentainerBlockSize() - fragmentainer_block_offset;
    bool need_break;
    if (child.IsMonolithic()) {
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
    if (need_break) {
      // If we haven't used any space at all in the fragmentainer yet, though,
      // we cannot break even if we really want to, or there'd be no progress.
      // We'd end up creating an infinite number of fragmentainers without
      // putting any content into them.
      if (space_left >= space.FragmentainerBlockSize())
        need_break = false;
    }

    if (!need_break) {
      if (child.IsBlock()) {
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

}  // namespace blink
