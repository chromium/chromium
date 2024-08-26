// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {
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
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
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

bool ShouldCloneBlockStartBorderPadding(const BoxFragmentBuilder& builder) {
  if (builder.Node().Style().BoxDecorationBreak() !=
      EBoxDecorationBreak::kClone) {
    return false;
  }
  const BlockBreakToken* previous_break_token = builder.PreviousBreakToken();
  if (!previous_break_token) {
    return true;
  }
  if (previous_break_token->MonolithicOverflow()) {
    LayoutUnit space_left =
        FragmentainerSpaceLeft(builder, /*is_for_children=*/false);
    if (space_left < builder.BorderScrollbarPadding().BlockSum()) {
      return false;
    }
  }
  return !previous_break_token->IsAtBlockEnd();
}

}  // anonymous namespace

EBreakBetween JoinFragmentainerBreakValues(EBreakBetween first_value,
                                           EBreakBetween second_value) {
  if (FragmentainerBreakPrecedence(second_value) >=
      FragmentainerBreakPrecedence(first_value))
    return second_value;
  return first_value;
}

bool IsForcedBreakValue(const ConstraintSpace& constraint_space,
                        EBreakBetween break_value) {
  if (constraint_space.ShouldIgnoreForcedBreaks())
    return false;
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
bool IsAvoidBreakValue(const ConstraintSpace& constraint_space,
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
template bool CORE_TEMPLATE_EXPORT IsAvoidBreakValue(const ConstraintSpace&,
                                                     EBreakBetween);
template bool CORE_TEMPLATE_EXPORT IsAvoidBreakValue(const ConstraintSpace&,
                                                     EBreakInside);

EBreakBetween CalculateBreakBetweenValue(LayoutInputNode child,
                                         const LayoutResult& layout_result,
                                         const BoxFragmentBuilder& builder) {
  if (child.IsInline())
    return EBreakBetween::kAuto;

  // Since it's not an inline node, if we have a fragment at all, it has to be a
  // box fragment.
  const PhysicalBoxFragment* box_fragment = nullptr;
  if (layout_result.Status() == LayoutResult::kSuccess) {
    box_fragment =
        &To<PhysicalBoxFragment>(layout_result.GetPhysicalFragment());
    if (!box_fragment->IsFirstForNode()) {
      // If the node is resumed after a break, we are not *before* it anymore,
      // so ignore values. We normally don't even consider breaking before a
      // resumed node, since there normally is no container separation. The
      // normal place to resume is at the very start of the fragmentainer -
      // cannot break there!  However, there are cases where a node is resumed
      // at a location past the start of the fragmentainer, e.g. when printing
      // monolithic overflowing content.
      return EBreakBetween::kAuto;
    }
  }

  EBreakBetween break_before = JoinFragmentainerBreakValues(
      child.Style().BreakBefore(), layout_result.InitialBreakBefore());
  break_before = builder.JoinedBreakBetweenValue(break_before);
  const auto& space = builder.GetConstraintSpace();
  if (space.IsPaginated() && box_fragment &&
      !IsForcedBreakValue(builder.GetConstraintSpace(), break_before)) {
    AtomicString current_name = builder.PageName();
    if (current_name == g_null_atom) {
      current_name = space.PageName();
    }
    // If the page name propagated from the child differs from what we already
    // have, we need to break before the child.
    if (box_fragment->PageName() != current_name) {
      return EBreakBetween::kPage;
    }
  }
  return break_before;
}

bool IsBreakableAtStartOfResumedContainer(
    const ConstraintSpace& space,
    const LayoutResult& child_layout_result,
    const BoxFragmentBuilder& builder) {
  if (child_layout_result.Status() != LayoutResult::kSuccess) {
    return false;
  }
  bool is_first_for_node = true;
  if (const auto* box_fragment = DynamicTo<PhysicalBoxFragment>(
          child_layout_result.GetPhysicalFragment())) {
    is_first_for_node = box_fragment->IsFirstForNode();
  }
  return IsBreakableAtStartOfResumedContainer(space, builder,
                                              is_first_for_node);
}

bool IsBreakableAtStartOfResumedContainer(const ConstraintSpace& space,
                                          const BoxFragmentBuilder& builder,
                                          bool is_first_for_node) {
  return space.MinBreakAppeal() != kBreakAppealLastResort &&
         IsBreakInside(builder.PreviousBreakToken()) && is_first_for_node;
}

BreakAppeal CalculateBreakAppealBefore(const ConstraintSpace& space,
                                       LayoutInputNode child,
                                       const LayoutResult& layout_result,
                                       const BoxFragmentBuilder& builder,
                                       bool has_container_separation) {
  bool breakable_at_start_of_container =
      IsBreakableAtStartOfResumedContainer(space, layout_result, builder);
  EBreakBetween break_between =
      CalculateBreakBetweenValue(child, layout_result, builder);
  return CalculateBreakAppealBefore(space, layout_result.Status(),
                                    break_between, has_container_separation,
                                    breakable_at_start_of_container);
}

BreakAppeal CalculateBreakAppealBefore(
    const ConstraintSpace& space,
    LayoutResult::EStatus layout_result_status,
    EBreakBetween break_between,
    bool has_container_separation,
    bool breakable_at_start_of_container) {
  DCHECK(layout_result_status == LayoutResult::kSuccess ||
         layout_result_status == LayoutResult::kOutOfFragmentainerSpace);
  BreakAppeal break_appeal = kBreakAppealPerfect;
  if (!has_container_separation &&
      layout_result_status == LayoutResult::kSuccess) {
    if (!breakable_at_start_of_container) {
      // This is not a valid break point. If there's no container separation, it
      // means that we're breaking before the first piece of in-flow content
      // inside this block, even if it's not a valid class C break point [1]. We
      // really don't want to break here, if we can find something better.
      //
      // [1] https://www.w3.org/TR/css-break-3/#possible-breaks
      return kBreakAppealLastResort;
    }

    // This is the first child after a break. We are normally not allowed to
    // break before those, but in this case we will allow it, to prevent
    // suboptimal breaks that might otherwise occur further ahead in the
    // fragmentainer. If necessary, we'll push this child (and all subsequent
    // content) past all the columns in the current row all the way to the the
    // next row in the next outer fragmentainer, where there may be more space,
    // in order to avoid suboptimal breaks.
    break_appeal = space.MinBreakAppeal();
  }

  if (IsAvoidBreakValue(space, break_between)) {
    // If there's a break-{after,before}:avoid* involved at this breakpoint, its
    // appeal will decrease.
    break_appeal = std::min(break_appeal, kBreakAppealViolatingBreakAvoid);
  }
  return break_appeal;
}

BreakAppeal CalculateBreakAppealInside(
    const ConstraintSpace& space,
    const LayoutResult& layout_result,
    std::optional<BreakAppeal> hypothetical_appeal) {
  if (layout_result.HasForcedBreak())
    return kBreakAppealPerfect;
  const auto& physical_fragment = layout_result.GetPhysicalFragment();
  const auto* break_token =
      DynamicTo<BlockBreakToken>(physical_fragment.GetBreakToken());
  BreakAppeal appeal;
  bool consider_break_inside_avoidance;
  if (hypothetical_appeal) {
    // The hypothetical appeal of breaking inside should only be considered if
    // we haven't actually broken.
    DCHECK(!break_token);
    appeal = *hypothetical_appeal;
    consider_break_inside_avoidance = true;
  } else {
    appeal = layout_result.GetBreakAppeal();
    consider_break_inside_avoidance = IsBreakInside(break_token);
  }

  // We don't let break-inside:avoid affect the child's stored break appeal, but
  // we rather handle it now, on the outside. The reason is that we want to be
  // able to honor any 'avoid' values on break-before or break-after among the
  // children of the child, even if we need to disregrard a break-inside:avoid
  // rule on the child itself. This prevents us from violating more rules than
  // necessary: if we need to break inside the child (even if it should be
  // avoided), we'll at least break at the most appealing location inside.
  if (consider_break_inside_avoidance &&
      appeal > kBreakAppealViolatingBreakAvoid &&
      IsAvoidBreakValue(space, physical_fragment.Style().BreakInside()))
    appeal = kBreakAppealViolatingBreakAvoid;
  return appeal;
}

LogicalSize FragmentainerLogicalCapacity(
    const PhysicalBoxFragment& fragmentainer) {
  DCHECK(fragmentainer.IsFragmentainerBox());
  LogicalSize logical_size =
      WritingModeConverter(fragmentainer.Style().GetWritingDirection())
          .ToLogical(fragmentainer.Size());
  // TODO(layout-dev): This should really be checking if there are any
  // descendants that take up block space rather than if it has overflow. In
  // other words, we would still want to clamp a zero height fragmentainer if
  // it had content with zero inline size and non-zero block size. This would
  // likely require us to store an extra flag on PhysicalBoxFragment.
  if (fragmentainer.HasScrollableOverflow()) {
    // Don't clamp the fragmentainer to a block size of 1 if it is truly a
    // zero-height column.
    logical_size.block_size =
        ClampedToValidFragmentainerCapacity(logical_size.block_size);
  }
  return logical_size;
}

LogicalOffset GetFragmentainerProgression(const BoxFragmentBuilder& builder,
                                          FragmentationType type) {
  if (type == kFragmentColumn) {
    LayoutUnit column_inline_progression = ColumnInlineProgression(
        builder.ChildAvailableSize().inline_size, builder.Style());
    return LogicalOffset(column_inline_progression, LayoutUnit());
  }
  DCHECK_EQ(type, kFragmentPage);
  return LogicalOffset(LayoutUnit(), builder.ChildAvailableSize().block_size);
}

void SetupSpaceBuilderForFragmentation(const ConstraintSpace& parent_space,
                                       const LayoutInputNode& child,
                                       LayoutUnit fragmentainer_offset,
                                       LayoutUnit fragmentainer_block_size,
                                       bool requires_content_before_breaking,
                                       ConstraintSpaceBuilder* builder) {
  DCHECK(parent_space.HasBlockFragmentation());

  // If the child is truly unbreakable, it won't participate in block
  // fragmentation. If it's too tall to fit, it will either overflow the
  // fragmentainer or get brutally sliced into pieces (without looking for
  // allowed breakpoints, since there are none, by definition), depending on
  // fragmentation type (multicol vs. printing). We still need to perform block
  // fragmentation inside inline nodes, though: While the line box itself is
  // monolithic, there may be floats inside, which are fragmentable.
  if (child.IsMonolithic() && !child.IsInline()) {
    builder->SetShouldPropagateChildBreakValues(false);
    return;
  }

  builder->SetFragmentainerBlockSize(fragmentainer_block_size);
  builder->SetFragmentainerOffset(fragmentainer_offset);
  if (fragmentainer_offset <= LayoutUnit())
    builder->SetIsAtFragmentainerStart();
  builder->SetFragmentationType(parent_space.BlockFragmentationType());
  builder->SetShouldPropagateChildBreakValues();
  DCHECK(!requires_content_before_breaking ||
         !parent_space.IsInitialColumnBalancingPass());
  builder->SetRequiresContentBeforeBreaking(requires_content_before_breaking);

  if (parent_space.IsInsideBalancedColumns())
    builder->SetIsInsideBalancedColumns();

  // We lack the required machinery to resume layout inside out-of-flow
  // positioned elements during regular layout. OOFs are handled by regular
  // layout during the initial column balacning pass, while it's handled
  // specially during actual layout - at the outermost fragmentation context in
  // OutOfFlowLayoutPart (so this is only an issue when calculating the
  // initial column block-size). So just disallow breaks (we only need to worry
  // about forced breaks, as soft breaks are impossible in the initial column
  // balancing pass). This might result in over-stretched columns in some
  // strange cases, but probably something we can live with.
  if ((parent_space.IsInitialColumnBalancingPass() &&
       child.IsOutOfFlowPositioned()) ||
      parent_space.ShouldIgnoreForcedBreaks())
    builder->SetShouldIgnoreForcedBreaks();

  builder->SetMinBreakAppeal(parent_space.MinBreakAppeal());

  if (parent_space.IsPaginated()) {
    if (AtomicString page_name = child.PageName())
      builder->SetPageName(page_name);
    else
      builder->SetPageName(parent_space.PageName());
  }
}

void SetupSpaceBuilderForFragmentation(
    const BoxFragmentBuilder& parent_fragment_builder,
    const LayoutInputNode& child,
    LayoutUnit fragmentainer_offset_delta,
    ConstraintSpaceBuilder* builder) {
  LayoutUnit fragmentainer_block_size =
      FragmentainerCapacity(parent_fragment_builder, /*is_for_children=*/true);
  LayoutUnit fragmentainer_block_offset =
      FragmentainerOffset(parent_fragment_builder, /*is_for_children=*/true) +
      fragmentainer_offset_delta;
  return SetupSpaceBuilderForFragmentation(
      parent_fragment_builder.GetConstraintSpace(), child,
      fragmentainer_block_offset, fragmentainer_block_size,
      parent_fragment_builder.RequiresContentBeforeBreaking(), builder);
}

void SetupFragmentBuilderForFragmentation(
    const ConstraintSpace& space,
    const LayoutInputNode& node,
    const BlockBreakToken* previous_break_token,
    BoxFragmentBuilder* builder) {
  // When resuming layout after a break, we may not be allowed to break again
  // (because of clipped overflow). In such situations, we should not call
  // SetHasBlockFragmentation(), but we still need to resume layout correctly,
  // based on the previous break token.
  DCHECK(space.HasBlockFragmentation() || previous_break_token);
  // If the node itself is monolithic, we shouldn't be here.
  DCHECK(!node.IsMonolithic() || space.IsAnonymous());
  // If we turn off fragmentation on a non-monolithic node, we need to treat the
  // resulting fragment as monolithic. This matters when it comes to determining
  // the containing block of out-of-flow positioned descendants. In order to
  // match the behavior in OOF layout, however, the fragment should only become
  // monolithic when fragmentation is forced off at the first fragment. If we
  // reach the end of the visible area after the containing block has inserted a
  // break, it should not be set as monolithic. (How can we be monolithic, if we
  // create more than one fragment, anyway?) An OOF fragment will always become
  // a direct child of the fragmentainer if the containing block generates more
  // than one fragment. The monolithicness flag is ultimately checked by
  // pre-paint, in order to know where in the tree to look for the OOF fragment
  // (direct fragmentainer child vs. child of the actual containing block).
  builder->SetIsMonolithic(!space.IsAnonymous() &&
                           space.IsBlockFragmentationForcedOff() &&
                           !IsBreakInside(previous_break_token));

  if (space.HasBlockFragmentation())
    builder->SetHasBlockFragmentation();

  if (space.IsInitialColumnBalancingPass())
    builder->SetIsInitialColumnBalancingPass();

  unsigned sequence_number = 0;
  if (previous_break_token && !previous_break_token->IsBreakBefore()) {
    sequence_number = previous_break_token->SequenceNumber() + 1;
    builder->SetIsFirstForNode(false);
  }

  LayoutUnit space_left =
      FragmentainerSpaceLeft(*builder, /*is_for_children=*/false);

  // If box decorations are to be cloned, both block-start and block-end should
  // obviosuly be present in every fragment, but whether block-end decorations
  // count as being cloned or not depends on whether the fragment currently
  // being built is known to be the last fragment. If it is, block-end box
  // decorations will behave as normally, so that child content may overflow it.
  bool clone_box_start_decorations =
      ShouldCloneBlockStartBorderPadding(*builder);
  bool clone_box_end_decorations = clone_box_start_decorations;

  if (clone_box_start_decorations) {
    // Include border/padding from previous fragments. When resolving the
    // block-size for this fragment, we need the total space used by
    // decorations.
    builder->UpdateBorderPaddingForClonedBoxDecorations();
  }

  if (space.HasBlockFragmentation() && !space.IsAnonymous() &&
      !space.IsInitialColumnBalancingPass()) {
    bool requires_content_before_breaking =
        space.RequiresContentBeforeBreaking();
    // We're now going to figure out if the (remainder of the) node is
    // guaranteed to fit in the fragmentainer, and make some decisions based on
    // that. We'll skip this for tables, because table sizing is complicated,
    // since captions are not part of the "table box", and any specified
    // block-size pertains to the table box, while the captions are on the
    // outside of the "table box", but still part of the fragment.
    if (!node.IsTable() &&
        builder->InitialBorderBoxSize().inline_size != kIndefiniteSize) {
      // Pass an "infinite" intrinsic size to see how the block-size is
      // constrained. If it doesn't affect the block size, it means that we can
      // tell before layout how much more space this node needs.
      LayoutUnit max_block_size = ComputeBlockSizeForFragment(
          space, To<BlockNode>(node), builder->BorderPadding(),
          LayoutUnit::Max(), builder->InitialBorderBoxSize().inline_size);
      DCHECK(space.HasKnownFragmentainerBlockSize());

      // If max_block_size is "infinite", we can't tell for sure that it's going
      // to fit. The calculation below will normally detect that, but it's going
      // to be incorrect when we have reached the point where space left
      // incorrectly seems to be enough to contain the remaining fragment when
      // subtracting previously consumed block-size from its max size.
      if (max_block_size != LayoutUnit::Max()) {
        LayoutUnit previously_consumed_block_size;
        if (previous_break_token) {
          previously_consumed_block_size =
              previous_break_token->ConsumedBlockSize();
        }

        if (max_block_size - previously_consumed_block_size <= space_left) {
          builder->SetIsKnownToFitInFragmentainer(true);
          clone_box_end_decorations = false;
          if (builder->MustStayInCurrentFragmentainer())
            requires_content_before_breaking = true;
        }
      }
    }

    if (clone_box_end_decorations) {
      builder->SetShouldCloneBoxEndDecorations(true);

      // If block-end border+padding is cloned, they should be repeated in every
      // fragment, so breaking before them would be wrong and make no sense.
      builder->SetShouldPreventBreakBeforeBlockEndDecorations(true);
    }

    builder->SetRequiresContentBeforeBreaking(requires_content_before_breaking);
  }
  builder->SetSequenceNumber(sequence_number);

  if (IsBreakInside(previous_break_token) && !clone_box_start_decorations) {
    // When resuming after a fragmentation break in the slicing box decoration
    // break model, block-start border and padding are omitted. Don't omit it
    // here for tables, though. The table box (which contains the border) might
    // not start in the first fragment, if there are preceding captions, so the
    // table algorithm needs to handle this logic on its own.
    if (!node.IsTable()) {
      builder->ClearBorderScrollbarPaddingBlockStart();
    }
  }

  if (builder->IsInitialColumnBalancingPass()) {
    const BoxStrut& unbreakable = builder->BorderScrollbarPadding();
    builder->PropagateTallestUnbreakableBlockSize(unbreakable.block_start);
    builder->PropagateTallestUnbreakableBlockSize(unbreakable.block_end);
  }
}

bool ShouldIncludeBlockStartBorderPadding(const BoxFragmentBuilder& builder) {
  return !IsBreakInside(builder.PreviousBreakToken()) ||
         ShouldCloneBlockStartBorderPadding(builder);
}

bool ShouldIncludeBlockEndBorderPadding(const BoxFragmentBuilder& builder) {
  if (builder.PreviousBreakToken() &&
      builder.PreviousBreakToken()->IsAtBlockEnd()) {
    // Past the block-end, and therefore past block-end border+padding.
    return false;
  }
  if (!builder.ShouldBreakInside() || builder.IsKnownToFitInFragmentainer() ||
      builder.ShouldCloneBoxEndDecorations()) {
    return true;
  }

  // We're going to break inside.
  if (builder.GetConstraintSpace().IsNewFormattingContext()) {
    return false;
  }
  // Not being a formatting context root, only in-flow child breaks will have an
  // effect on where the block ends.
  return !builder.HasInflowChildBreakInside();
}

BreakStatus FinishFragmentation(BoxFragmentBuilder* builder) {
  const BlockNode& node = builder->Node();
  const ConstraintSpace& space = builder->GetConstraintSpace();
  LayoutUnit space_left = FragmentainerSpaceLeft(*builder,
                                                 /*is_for_children=*/false);
  const BlockBreakToken* previous_break_token = builder->PreviousBreakToken();
  LayoutUnit previously_consumed_block_size;
  if (previous_break_token && !previous_break_token->IsBreakBefore())
    previously_consumed_block_size = previous_break_token->ConsumedBlockSize();
  bool is_past_end =
      previous_break_token && previous_break_token->IsAtBlockEnd();

  LayoutUnit fragments_total_block_size = builder->FragmentsTotalBlockSize();
  LayoutUnit desired_block_size =
      fragments_total_block_size - previously_consumed_block_size;

  // Consumed block-size stored in the break tokens is always stretched to the
  // fragmentainers. If this wasn't also the case for all previous fragments
  // (because we reached the end of the node and were overflowing), we may end
  // up with negative values here.
  desired_block_size = desired_block_size.ClampNegativeToZero();

  LayoutUnit desired_intrinsic_block_size = builder->IntrinsicBlockSize();

  LayoutUnit final_block_size = desired_block_size;

  LayoutUnit trailing_border_padding =
      builder->BorderScrollbarPadding().block_end;
  LayoutUnit subtractable_border_padding;
  if (!builder->ShouldPreventBreakBeforeBlockEndDecorations()) {
    if (desired_block_size > trailing_border_padding ||
        (previous_break_token && previous_break_token->MonolithicOverflow())) {
      // There is a last-resort breakpoint before trailing border and padding,
      // if progress can still be guaranteed.
      //
      // Note that we're always guaranteed progress if there's incoming
      // monolithic overflow. We're going to move past monolithic overflow, and
      // just add as many fragments we need in order to get past the overflow.
      subtractable_border_padding = trailing_border_padding;
    }
  }

  if (space_left != kIndefiniteSize) {
    // If intrinsic block-size is larger than space left, it means that we have
    // some tall unbreakable child content (otherwise it would already have
    // broken to stay within the limits). In such cases, this fragment will be
    // allowed to take up more space (within applicable constraints) in a
    // similarly unbreakable manner, to encompass the unbreakable content. This
    // effectively increases the fragmentainer space available, as far as this
    // node is concerned.
    space_left = std::max(
        space_left, desired_intrinsic_block_size - subtractable_border_padding);
  }

  if (space.IsPaginated()) {
    // Descendants take precedence, but if none of them propagated a page name,
    // use the one specified on this element (or on something in the ancestry)
    // now, if any.
    builder->SetPageNameIfNeeded(space.PageName());
  }

  if (builder->FoundColumnSpanner())
    builder->SetDidBreakSelf();

  if (is_past_end) {
    final_block_size = LayoutUnit();
  } else if (builder->FoundColumnSpanner()) {
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
    final_block_size =
        std::min(final_block_size, desired_intrinsic_block_size) -
        trailing_border_padding;

    // TODO(crbug.com/1381327): We shouldn't get negative sizes here, but this
    // happens if we have incorrectly added trailing border/padding of a
    // block-size-restricted container (of a spanner) in a previous fragment, so
    // that we're past the block-end border edge, in which case
    // desired_block_size will be zero (because of an overly large
    // previously_consumed_block_size) - so that subtracting
    // trailing_border_padding here might result in a negative value. Note that
    // the code block right below has some subtractable_border_padding logic
    // that could have saved us here, but it still wouldn't be correct. We
    // should never add block-end border/padding if we're interrupted by as
    // spanner. So just clamp to zero, to avoid DCHECK failures.
    final_block_size = final_block_size.ClampNegativeToZero();
  } else if (space_left != kIndefiniteSize && desired_block_size > space_left &&
             space.HasBlockFragmentation()) {
    // We're taller than what we have room for. We don't want to use more than
    // |space_left|, but if the intrinsic block-size is larger than that, it
    // means that there's something unbreakable (monolithic) inside (or we'd
    // already have broken inside). We'll allow this to overflow the
    // fragmentainer.
    DCHECK_GE(desired_intrinsic_block_size, trailing_border_padding);
    DCHECK_GE(desired_block_size, trailing_border_padding);

    LayoutUnit modified_intrinsic_block_size = std::max(
        space_left, desired_intrinsic_block_size - subtractable_border_padding);
    builder->SetIntrinsicBlockSize(modified_intrinsic_block_size);
    final_block_size =
        std::min(desired_block_size - subtractable_border_padding,
                 modified_intrinsic_block_size);

    // We'll only need to break inside if we need more space after any
    // unbreakable content that we may have forcefully fitted here.
    if (final_block_size < desired_block_size)
      builder->SetDidBreakSelf();
  }

  LogicalBoxSides sides;
  // If this isn't the first fragment, omit the block-start border, if in the
  // slicing box decoration break model.
  if (previously_consumed_block_size &&
      (node.Style().BoxDecorationBreak() == EBoxDecorationBreak::kSlice ||
       is_past_end)) {
    sides.block_start = false;
  }
  // If this isn't the last fragment with same-flow content, omit the block-end
  // border. If something overflows the node, we'll keep on creating empty
  // fragments to contain the overflow (which establishes a parallel flow), but
  // those fragments should make no room (nor paint) block-end border/paddding.
  if ((builder->DidBreakSelf() && !builder->ShouldCloneBoxEndDecorations()) ||
      is_past_end) {
    sides.block_end = false;
  }
  builder->SetSidesToInclude(sides);

  builder->SetConsumedBlockSize(previously_consumed_block_size +
                                final_block_size);
  builder->SetFragmentBlockSize(final_block_size);

  if (builder->FoundColumnSpanner() || !space.HasBlockFragmentation())
    return BreakStatus::kContinue;

  bool was_broken_by_child = builder->HasInflowChildBreakInside();
  if (!was_broken_by_child && space.IsNewFormattingContext())
    was_broken_by_child = builder->GetExclusionSpace().HasFragmentainerBreak();

  if (space_left == kIndefiniteSize) {
    // We don't know how space is available (initial column balancing pass), so
    // we won't break.
    if (!was_broken_by_child)
      builder->SetIsAtBlockEnd();
    return BreakStatus::kContinue;
  }

  if (!final_block_size && previous_break_token &&
      previous_break_token->MonolithicOverflow()) {
    // See if we've now managed to move past previous fragmentainer overflow, or
    // if we need to steer clear of at least some of it in the next
    // fragmentainer as well. This only happens when printing monolithic
    // content.
    LayoutUnit remaining_overflow =
        previous_break_token->MonolithicOverflow() -
        FragmentainerCapacity(*builder, /*is_for_children=*/false);
    if (remaining_overflow > LayoutUnit()) {
      builder->ReserveSpaceForMonolithicOverflow(remaining_overflow);
    }
  }

  if (builder->ShouldBreakInside()) {
    // We need to break before or inside one of our children (or have already
    // done so). Even if we fit within the remaining space, and even if the
    // child involved in the break were to be in a parallel flow, we still need
    // to prepare a break token for this node, so that we can resume layout of
    // its broken or unstarted children in the next fragmentainer.
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
    if (is_past_end) {
      builder->SetIsAtBlockEnd();
      // We entered layout already at the end of the block (but with overflowing
      // children). So we should take up no more space on our own.
      DCHECK_EQ(final_block_size, LayoutUnit());
    } else if (desired_block_size <= space_left) {
      // We have room for the calculated block-size in the current
      // fragmentainer, but we need to figure out whether this node is going to
      // produce more non-zero block-size fragments or not.
      //
      // If the block-size is constrained / fixed (in which case
      // IsKnownToFitInFragmentainer() will return true now), we know that we're
      // at the end. If block-size is unconstrained (or at least allowed to grow
      // a bit more), we're only at the end if no in-flow content inside broke.
      if (!was_broken_by_child || builder->IsKnownToFitInFragmentainer()) {
        if (node.HasNonVisibleBlockOverflow() && builder->ShouldBreakInside()) {
          // We have reached the end of a fragmentable node that clips overflow
          // in the block direction. If something broke inside at this point, we
          // need to relayout without fragmentation, so that we don't generate
          // any additional fragments (apart from the one we're working on) from
          // this node. We don't want any zero-sized clipped fragments that
          // contribute to superfluous fragmentainers.
          return BreakStatus::kDisableFragmentation;
        }

        builder->SetIsAtBlockEnd();
      }
    }

    if (builder->IsAtBlockEnd()) {
      // This node is to be resumed in the next fragmentainer. Make sure that
      // consumed block-size includes the entire remainder of the fragmentainer.
      // The fragment will normally take up all that space, but not if we've
      // reached the end of the node (and we are breaking because of
      // overflow). We include the entire fragmentainer in consumed block-size
      // in order to write offsets correctly back to legacy layout.
      builder->SetConsumedBlockSize(previously_consumed_block_size +
                                    std::max(final_block_size, space_left));
    } else {
      // If we're not at the end, it means that block-end border and shadow
      // should be omitted, unless box decorations are to be cloned.
      if (!builder->ShouldCloneBoxEndDecorations()) {
        sides.block_end = false;
        builder->SetSidesToInclude(sides);
      }
    }

    return BreakStatus::kContinue;
  }

  if (desired_block_size > space_left) {
    // No child inside broke, but we're too tall to fit.
    if (!previously_consumed_block_size) {
      // This is the first fragment generated for the node. Avoid breaking
      // inside block-start border, scrollbar and padding, if possible. No valid
      // breakpoints there.
      const FragmentGeometry& geometry = builder->InitialFragmentGeometry();
      LayoutUnit block_start_unbreakable_space =
          geometry.border.block_start + geometry.scrollbar.block_start +
          geometry.padding.block_start;
      if (space_left < block_start_unbreakable_space)
        builder->ClampBreakAppeal(kBreakAppealLastResort);
    }
    if (space.BlockFragmentationType() == kFragmentColumn &&
        !space.IsInitialColumnBalancingPass())
      builder->PropagateSpaceShortage(desired_block_size - space_left);
    if (desired_block_size <= desired_intrinsic_block_size) {
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
        return BreakStatus::kNeedsEarlierBreak;
      builder->ClampBreakAppeal(kBreakAppealLastResort);
    }
    return BreakStatus::kContinue;
  }

  // The end of the block fits in the current fragmentainer.
  builder->SetIsAtBlockEnd();
  return BreakStatus::kContinue;
}

BreakStatus FinishFragmentationForFragmentainer(BoxFragmentBuilder* builder) {
  const ConstraintSpace& space = builder->GetConstraintSpace();
  DCHECK(builder->IsFragmentainerBoxType());
  const BlockBreakToken* previous_break_token = builder->PreviousBreakToken();
  LayoutUnit consumed_block_size =
      previous_break_token ? previous_break_token->ConsumedBlockSize()
                           : LayoutUnit();
  if (space.HasKnownFragmentainerBlockSize()) {
    // Just copy the block-size from the constraint space. Calculating the
    // size the regular way would cause some problems with overflow. For one,
    // we don't want to produce a break token if there's no child content that
    // requires it. When we lay out, we use FragmentainerCapacity(), so this
    // is what we need to add to consumed block-size for the next break
    // token. The fragment block-size itself will be based directly on the
    // fragmentainer size from the constraint space, though.
    LayoutUnit block_size = space.FragmentainerBlockSize();
    LayoutUnit fragmentainer_capacity =
        FragmentainerCapacity(*builder, /*is_for_children=*/false);
    builder->SetFragmentBlockSize(block_size);
    consumed_block_size += fragmentainer_capacity;
    builder->SetConsumedBlockSize(consumed_block_size);

    // We clamp the fragmentainer block size from 0 to 1 for legacy write-back
    // if there is content that overflows the zero-height fragmentainer.
    // Set the consumed block size adjustment for legacy if this results
    // in a different consumed block size than is used for NG layout.
    LayoutUnit consumed_block_size_for_legacy =
        previous_break_token
            ? previous_break_token->ConsumedBlockSizeForLegacy()
            : LayoutUnit();
    LayoutUnit legacy_fragmentainer_block_size =
        (builder->IntrinsicBlockSize() > LayoutUnit()) ? fragmentainer_capacity
                                                       : block_size;
    LayoutUnit consumed_block_size_legacy_adjustment =
        consumed_block_size_for_legacy + legacy_fragmentainer_block_size -
        consumed_block_size;
    builder->SetConsumedBlockSizeLegacyAdjustment(
        consumed_block_size_legacy_adjustment);

    if (previous_break_token && previous_break_token->MonolithicOverflow()) {
      // Add pages as long as there's monolithic overflow that requires it.
      LayoutUnit remaining_overflow =
          previous_break_token->MonolithicOverflow() -
          FragmentainerCapacity(*builder, /*is_for_children=*/false);
      if (remaining_overflow > LayoutUnit()) {
        builder->ReserveSpaceForMonolithicOverflow(remaining_overflow);
      }
    }
  } else {
    LayoutUnit fragments_total_block_size = builder->FragmentsTotalBlockSize();
    // Just pass the value through. This is a fragmentainer, and fragmentainers
    // don't have previously consumed block-size baked in, unlike any other
    // fragments.
    builder->SetFragmentBlockSize(fragments_total_block_size);
    builder->SetConsumedBlockSize(fragments_total_block_size +
                                  consumed_block_size);
  }
  if (builder->IsEmptySpannerParent() &&
      builder->HasOutOfFlowFragmentainerDescendants())
    builder->SetIsEmptySpannerParent(false);

  return BreakStatus::kContinue;
}

bool HasBreakOpportunityBeforeNextChild(
    const PhysicalFragment& child_fragment,
    const BreakToken* incoming_child_break_token) {
  // Once we have added a child, there'll be a valid class A/B breakpoint [1]
  // before consecutive siblings, which implies that we have container
  // separation, which means that we may break before such siblings. Exclude
  // children in parallel flows, since they shouldn't affect this flow.
  //
  // [1] https://www.w3.org/TR/css-break-3/#possible-breaks
  if (IsA<PhysicalBoxFragment>(&child_fragment)) {
    const auto* block_break_token =
        To<BlockBreakToken>(incoming_child_break_token);
    return !block_break_token || !block_break_token->IsAtBlockEnd();
  }

  // Only establish a valid break opportunity after a line box if it has
  // non-zero height. When there's a block inside an inline, a zero-height line
  // may be created before and after the block, but for the sake of
  // fragmentation, pretend that they're not there.
  DCHECK(child_fragment.IsLineBox());
  LogicalFragment fragment(child_fragment.Style().GetWritingDirection(),
                           child_fragment);
  return fragment.BlockSize() != LayoutUnit();
}

BreakStatus BreakBeforeChildIfNeeded(
    const ConstraintSpace& space,
    LayoutInputNode child,
    const LayoutResult& layout_result,
    LayoutUnit fragmentainer_block_offset,
    LayoutUnit fragmentainer_block_size,
    bool has_container_separation,
    BoxFragmentBuilder* builder,
    bool is_row_item,
    FlexColumnBreakInfo* flex_column_break_info) {
  DCHECK(space.HasBlockFragmentation());

  // Break-before and break-after are handled at the row level.
  if (has_container_separation && !is_row_item) {
    EBreakBetween break_between =
        CalculateBreakBetweenValue(child, layout_result, *builder);
    if (IsForcedBreakValue(space, break_between)) {
      BreakBeforeChild(space, child, &layout_result, fragmentainer_block_offset,
                       fragmentainer_block_size, kBreakAppealPerfect,
                       /*is_forced_break=*/true, builder);
      return BreakStatus::kBrokeBefore;
    }
  }

  BreakAppeal appeal_before = CalculateBreakAppealBefore(
      space, child, layout_result, *builder, has_container_separation);

  // Attempt to move past the break point, and if we can do that, also assess
  // the appeal of breaking there, even if we didn't.
  if (MovePastBreakpoint(space, child, layout_result,
                         fragmentainer_block_offset, fragmentainer_block_size,
                         appeal_before, builder, is_row_item,
                         flex_column_break_info)) {
    return BreakStatus::kContinue;
  }

  // Breaking inside the child isn't appealing, and we're out of space. Figure
  // out where to insert a soft break. It will either be before this child, or
  // before an earlier sibling, if there's a more appealing breakpoint there.
  if (!AttemptSoftBreak(
          space, child, &layout_result, fragmentainer_block_offset,
          fragmentainer_block_size, appeal_before, builder,
          /*block_size_override=*/std::nullopt, flex_column_break_info)) {
    return BreakStatus::kNeedsEarlierBreak;
  }

  return BreakStatus::kBrokeBefore;
}

void BreakBeforeChild(const ConstraintSpace& space,
                      LayoutInputNode child,
                      const LayoutResult* layout_result,
                      LayoutUnit fragmentainer_block_offset,
                      LayoutUnit fragmentainer_block_size,
                      std::optional<BreakAppeal> appeal,
                      bool is_forced_break,
                      BoxFragmentBuilder* builder,
                      std::optional<LayoutUnit> block_size_override) {
#if DCHECK_IS_ON()
  DCHECK(layout_result || block_size_override);
  if (layout_result && layout_result->Status() == LayoutResult::kSuccess) {
    // In order to successfully break before a node, this has to be its first
    // fragment.
    const auto& physical_fragment = layout_result->GetPhysicalFragment();
    DCHECK(!physical_fragment.IsBox() ||
           To<PhysicalBoxFragment>(physical_fragment).IsFirstForNode());
  }
#endif

  if (space.HasKnownFragmentainerBlockSize()) {
    PropagateSpaceShortage(space, layout_result, fragmentainer_block_offset,
                           fragmentainer_block_size, builder,
                           block_size_override);
  }

  if (layout_result && space.ShouldPropagateChildBreakValues() &&
      !is_forced_break)
    builder->PropagateChildBreakValues(*layout_result);

  // We'll drop the fragment (if any) on the floor and retry at the start of the
  // next fragmentainer.
  builder->AddBreakBeforeChild(child, appeal, is_forced_break);
}

void PropagateSpaceShortage(const ConstraintSpace& space,
                            const LayoutResult* layout_result,
                            LayoutUnit fragmentainer_block_offset,
                            LayoutUnit fragmentainer_block_size,
                            FragmentBuilder* builder,
                            std::optional<LayoutUnit> block_size_override) {
  // Only multicol cares about space shortage.
  if (space.BlockFragmentationType() != kFragmentColumn)
    return;

  LayoutUnit space_shortage =
      CalculateSpaceShortage(space, layout_result, fragmentainer_block_offset,
                             fragmentainer_block_size, block_size_override);

  // TODO(mstensho): Turn this into a DCHECK, when the engine is ready for
  // it. Space shortage should really be positive here, or we might ultimately
  // fail to stretch the columns (column balancing).
  if (space_shortage > LayoutUnit())
    builder->PropagateSpaceShortage(space_shortage);
}

LayoutUnit CalculateSpaceShortage(
    const ConstraintSpace& space,
    const LayoutResult* layout_result,
    LayoutUnit fragmentainer_block_offset,
    LayoutUnit fragmentainer_block_size,
    std::optional<LayoutUnit> block_size_override) {
  // Space shortage is only reported for soft breaks, and they can only exist if
  // we know the fragmentainer block-size.
  DCHECK(space.HasKnownFragmentainerBlockSize());
  DCHECK(layout_result || block_size_override);

  // Only multicol cares about space shortage.
  DCHECK_EQ(space.BlockFragmentationType(), kFragmentColumn);

  LayoutUnit space_shortage;
  if (block_size_override) {
    space_shortage = fragmentainer_block_offset + block_size_override.value() -
                     fragmentainer_block_size;
  } else if (!layout_result->MinimalSpaceShortage()) {
    // Calculate space shortage: Figure out how much more space would have been
    // sufficient to make the child fragment fit right here in the current
    // fragmentainer. If layout aborted, though, we can't calculate anything.
    if (layout_result->Status() != LayoutResult::kSuccess) {
      return kIndefiniteSize;
    }
    LogicalFragment fragment(space.GetWritingDirection(),
                             layout_result->GetPhysicalFragment());
    space_shortage = fragmentainer_block_offset + fragment.BlockSize() -
                     fragmentainer_block_size;
  } else {
    // However, if space shortage was reported inside the child, use that. If we
    // broke inside the child, we didn't complete layout, so calculating space
    // shortage for the child as a whole would be impossible and pointless.
    space_shortage = *layout_result->MinimalSpaceShortage();
  }
  return space_shortage;
}

void UpdateMinimalSpaceShortage(std::optional<LayoutUnit> new_space_shortage,
                                LayoutUnit* minimal_space_shortage) {
  DCHECK(minimal_space_shortage);
  if (!new_space_shortage || *new_space_shortage <= LayoutUnit())
    return;
  if (*minimal_space_shortage == kIndefiniteSize) {
    *minimal_space_shortage = *new_space_shortage;
  } else {
    *minimal_space_shortage =
        std::min(*minimal_space_shortage, *new_space_shortage);
  }
}

bool MovePastBreakpoint(const ConstraintSpace& space,
                        LayoutInputNode child,
                        const LayoutResult& layout_result,
                        LayoutUnit fragmentainer_block_offset,
                        LayoutUnit fragmentainer_block_size,
                        BreakAppeal appeal_before,
                        BoxFragmentBuilder* builder,
                        bool is_row_item,
                        FlexColumnBreakInfo* flex_column_break_info) {
  if (layout_result.Status() != LayoutResult::kSuccess) {
    // Layout aborted - no fragment was produced. There's nothing to move
    // past. We need to break before.
    DCHECK_EQ(layout_result.Status(), LayoutResult::kOutOfFragmentainerSpace);
    // The only case where this should happen is with BR clear=all.
    DCHECK(child.IsInline());
    return false;
  }

  if (child.IsBlock()) {
    const auto& box_fragment =
        To<PhysicalBoxFragment>(layout_result.GetPhysicalFragment());

    // If we're at a resumed fragment, don't break before it. Once we've found
    // room for the first fragment, we cannot skip fragmentainers afterwards. We
    // might be out of space at a subsequent fragment e.g. if all space is taken
    // up by a float that got pushed ahead from a previous fragmentainer, but we
    // still need to allow this fragment here. Inserting a break before on a
    // node that has already started producing fragments would result in
    // restarting layout from scratch once we find room for a fragment
    // again. Preventing breaking here should have no visual effect, since the
    // block-size of the fragment will typically be 0 anyway.
    if (!box_fragment.IsFirstForNode())
      return true;

    // If clearance forces the child to the next fragmentainer, we cannot move
    // past the breakpoint, but rather retry in the next fragmentainer.
    if (builder && builder->GetExclusionSpace().NeedsClearancePastFragmentainer(
                       child.Style().Clear(space.Direction()))) {
      return false;
    }
  }

  if (!space.HasKnownFragmentainerBlockSize() &&
      space.IsInitialColumnBalancingPass() && builder) {
    if (layout_result.GetPhysicalFragment().IsMonolithic() ||
        (child.IsBlock() &&
         IsAvoidBreakValue(space, child.Style().BreakInside()))) {
      // If this is the initial column balancing pass, attempt to make the
      // column block-size at least as large as the tallest piece of monolithic
      // content and/or block with break-inside:avoid.
      LayoutUnit block_size =
          BlockSizeForFragmentation(layout_result, space.GetWritingDirection());
      PropagateUnbreakableBlockSize(block_size, fragmentainer_block_offset,
                                    builder);
    }
  }

  bool move_past =
      MovePastBreakpoint(space, layout_result, fragmentainer_block_offset,
                         fragmentainer_block_size, appeal_before, builder,
                         is_row_item, flex_column_break_info);

  if (move_past && builder && child.IsBlock() && !is_row_item) {
    // We're tentatively not going to break before this child, but we'll check
    // the appeal of breaking there anyway. It may be the best breakpoint we'll
    // ever find. (Note that we only do this for block children, since, when it
    // comes to inline layout, we first need to lay out all the line boxes, so
    // that we know what do to in order to honor orphans and widows, if at all
    // possible. We also only do this for non-row items since items in a row
    // will be parallel to one another.)
    UpdateEarlyBreakAtBlockChild(space, To<BlockNode>(child), layout_result,
                                 appeal_before, builder,
                                 flex_column_break_info);
  }

  return move_past;
}

bool MovePastBreakpoint(const ConstraintSpace& space,
                        const LayoutResult& layout_result,
                        LayoutUnit fragmentainer_block_offset,
                        LayoutUnit fragmentainer_block_size,
                        BreakAppeal appeal_before,
                        BoxFragmentBuilder* builder,
                        bool is_row_item,
                        FlexColumnBreakInfo* flex_column_break_info) {
  DCHECK_EQ(layout_result.Status(), LayoutResult::kSuccess);

  if (!space.HasKnownFragmentainerBlockSize()) {
    // We only care about soft breaks if we have a fragmentainer block-size.
    // During column balancing this may be unknown.
    return true;
  }

  const auto& physical_fragment = layout_result.GetPhysicalFragment();
  LogicalFragment fragment(space.GetWritingDirection(), physical_fragment);
  const auto* break_token =
      DynamicTo<BlockBreakToken>(physical_fragment.GetBreakToken());

  LayoutUnit space_left = fragmentainer_block_size - fragmentainer_block_offset;

  // If we haven't used any space at all in the fragmentainer yet, we cannot
  // break before this child, or there'd be no progress. We'd risk creating an
  // infinite number of fragmentainers without putting any content into them. If
  // we have set a minimum break appeal (better than kBreakAppealLastResort),
  // though, we might have to allow breaking here.
  bool refuse_break_before = space_left >= fragmentainer_block_size &&
                             (!builder || !IsBreakableAtStartOfResumedContainer(
                                              space, layout_result, *builder));

  // If the child starts past the end of the fragmentainer (probably due to a
  // block-start margin), we must break before it.
  bool must_break_before = false;
  if (space_left < LayoutUnit()) {
    must_break_before = true;
  } else if (space_left == LayoutUnit()) {
    // If the child starts exactly at the end, we'll allow the child here if the
    // fragment contains the block-end of the child, or if it's a column
    // spanner. Otherwise we have to break before it. We don't want empty
    // fragments with nothing useful inside, if it's to be resumed in the next
    // fragmentainer.
    must_break_before = !layout_result.GetColumnSpannerPath() &&
                        IsBreakInside(break_token) &&
                        !break_token->IsAtBlockEnd();
  }
  if (must_break_before) {
    DCHECK(!refuse_break_before);
    return false;
  }

  LayoutUnit block_size =
      BlockSizeForFragmentation(layout_result, space.GetWritingDirection());
  BreakAppeal appeal_inside = CalculateBreakAppealInside(space, layout_result);

  // If breaking before is impossible, we have to move past.
  bool move_past = refuse_break_before;

  if (!move_past) {
    if (block_size <= space_left) {
      if (IsBreakInside(break_token) || appeal_inside < kBreakAppealPerfect) {
        // The block child broke inside, either in this fragmentation context,
        // or in an inner one. We now need to decide whether to keep that break,
        // or if it would be better to break before it. Allow breaking inside if
        // it has the same appeal or higher than breaking before or breaking
        // earlier.
        if (appeal_inside >= appeal_before) {
          if (flex_column_break_info) {
            if (!flex_column_break_info->early_break ||
                appeal_inside >=
                    flex_column_break_info->early_break->GetBreakAppeal()) {
              move_past = true;
            }
          } else if (!builder || !builder->HasEarlyBreak() ||
                     appeal_inside >=
                         builder->GetEarlyBreak().GetBreakAppeal()) {
            move_past = true;
          }
        }
      } else {
        move_past = true;
      }
    } else if (appeal_before == kBreakAppealLastResort && builder &&
               builder->RequiresContentBeforeBreaking()) {
      // The fragment doesn't fit, but we need to force it to stay here anyway.
      builder->SetIsBlockSizeForFragmentationClamped();
      move_past = true;
    }
  }

  if (move_past) {
    if (builder) {
      if (block_size > space_left) {
        // We're moving past the breakpoint even if the child doesn't fit. This
        // may happen with monolithic content at the beginning of the
        // fragmentainer. Report space shortage.
        PropagateSpaceShortage(space, &layout_result,
                               fragmentainer_block_offset,
                               fragmentainer_block_size, builder);
      }
    }
    return true;
  }

  // We don't want to break inside, so we should attempt to break before.
  return false;
}

void UpdateEarlyBreakAtBlockChild(const ConstraintSpace& space,
                                  BlockNode child,
                                  const LayoutResult& layout_result,
                                  BreakAppeal appeal_before,
                                  BoxFragmentBuilder* builder,
                                  FlexColumnBreakInfo* flex_column_break_info) {
  // We may need to create early-breaks even if we have broken inside the child,
  // in case it establishes a parallel flow, in which case a break inside won't
  // help honor any break avoidance requests that come after this child. But
  // breaking *before* the child might help.
  const auto* break_token =
      To<BlockBreakToken>(layout_result.GetPhysicalFragment().GetBreakToken());
  // See if there's a good breakpoint inside the child.
  BreakAppeal appeal_inside = kBreakAppealLastResort;
  if (const auto* breakpoint = layout_result.GetEarlyBreak()) {
    // If the child broke inside, it shouldn't have any early-break.
    DCHECK(!IsBreakInside(break_token));

    appeal_inside = CalculateBreakAppealInside(space, layout_result,
                                               breakpoint->GetBreakAppeal());
    if (flex_column_break_info) {
      if (!flex_column_break_info->early_break ||
          flex_column_break_info->early_break->GetBreakAppeal() <=
              breakpoint->GetBreakAppeal()) {
        // Found a good breakpoint inside the child. Add the child to the early
        // break chain for the current column.
        auto* parent_break =
            MakeGarbageCollected<EarlyBreak>(child, appeal_inside, breakpoint);
        flex_column_break_info->early_break = parent_break;
      }
    } else if (!builder->HasEarlyBreak() ||
               builder->GetEarlyBreak().GetBreakAppeal() <=
                   breakpoint->GetBreakAppeal()) {
      // Found a good breakpoint inside the child. Add the child to the early
      // break container chain, and store it.
      auto* parent_break =
          MakeGarbageCollected<EarlyBreak>(child, appeal_inside, breakpoint);
      builder->SetEarlyBreak(parent_break);
    }
  }

  // Breaking before isn't better if it's less appealing than what we already
  // have (obviously), and also not if it has the same appeal as the break
  // location inside the child that we just found (when the appeal is the same,
  // whatever takes us further wins).
  if (appeal_before <= appeal_inside)
    return;

  if (flex_column_break_info) {
    if (flex_column_break_info->early_break &&
        flex_column_break_info->early_break->GetBreakAppeal() > appeal_before) {
      return;
    }
    flex_column_break_info->early_break =
        MakeGarbageCollected<EarlyBreak>(child, appeal_before);
    return;
  }

  if (builder->HasEarlyBreak() &&
      builder->GetEarlyBreak().GetBreakAppeal() > appeal_before) {
    return;
  }

  builder->SetEarlyBreak(
      MakeGarbageCollected<EarlyBreak>(child, appeal_before));
}

bool AttemptSoftBreak(const ConstraintSpace& space,
                      LayoutInputNode child,
                      const LayoutResult* layout_result,
                      LayoutUnit fragmentainer_block_offset,
                      LayoutUnit fragmentainer_block_size,
                      BreakAppeal appeal_before,
                      BoxFragmentBuilder* builder,
                      std::optional<LayoutUnit> block_size_override,
                      FlexColumnBreakInfo* flex_column_break_info) {
  DCHECK(layout_result || block_size_override);
  // If there's a breakpoint with higher appeal among earlier siblings, we need
  // to abort and re-layout to that breakpoint.
  bool found_earlier_break = false;
  if (flex_column_break_info) {
    found_earlier_break =
        flex_column_break_info->early_break &&
        flex_column_break_info->early_break->GetBreakAppeal() > appeal_before;
  } else {
    found_earlier_break =
        builder->HasEarlyBreak() &&
        builder->GetEarlyBreak().GetBreakAppeal() > appeal_before;
  }
  if (found_earlier_break) {
    // Found a better place to break. Before aborting, calculate and report
    // space shortage from where we'd actually break.
    PropagateSpaceShortage(space, layout_result, fragmentainer_block_offset,
                           fragmentainer_block_size, builder,
                           block_size_override);
    return false;
  }

  // Break before the child. Note that there may be a better break further up
  // with higher appeal (but it's too early to tell), in which case this
  // breakpoint will be replaced.
  BreakBeforeChild(space, child, layout_result, fragmentainer_block_offset,
                   fragmentainer_block_size, appeal_before,
                   /* is_forced_break */ false, builder, block_size_override);
  return true;
}

const EarlyBreak* EnterEarlyBreakInChild(const BlockNode& child,
                                         const EarlyBreak& early_break) {
  if (early_break.Type() != EarlyBreak::kBlock ||
      early_break.GetBlockNode() != child) {
    return nullptr;
  }

  // If there's no break inside, we should already have broken before the child.
  DCHECK(early_break.BreakInside());
  return early_break.BreakInside();
}

bool IsEarlyBreakTarget(const EarlyBreak& early_break,
                        const BoxFragmentBuilder& builder,
                        const LayoutInputNode& child) {
  if (early_break.Type() == EarlyBreak::kLine) {
    DCHECK(child.IsInline() || child.IsFlexItem());
    return early_break.LineNumber() == builder.LineCount();
  }
  return early_break.IsBreakBefore() && early_break.GetBlockNode() == child;
}

ConstraintSpace CreateConstraintSpaceForFragmentainer(
    const ConstraintSpace& parent_space,
    FragmentationType fragmentation_type,
    LogicalSize fragmentainer_size,
    LogicalSize percentage_resolution_size,
    bool balance_columns,
    BreakAppeal min_break_appeal) {
  ConstraintSpaceBuilder space_builder(
      parent_space, parent_space.GetWritingDirection(), /* is_new_fc */ true);
  space_builder.SetAvailableSize(fragmentainer_size);
  space_builder.SetPercentageResolutionSize(percentage_resolution_size);
  space_builder.SetInlineAutoBehavior(AutoSizeBehavior::kStretchImplicit);
  space_builder.SetFragmentationType(fragmentation_type);
  space_builder.SetShouldPropagateChildBreakValues();
  space_builder.SetFragmentainerBlockSize(fragmentainer_size.block_size);
  space_builder.SetIsAnonymous(true);
  if (fragmentation_type == kFragmentColumn) {
    space_builder.SetIsInColumnBfc();
  }
  if (balance_columns) {
    DCHECK_EQ(fragmentation_type, kFragmentColumn);
    space_builder.SetIsInsideBalancedColumns();
  }
  space_builder.SetMinBreakAppeal(min_break_appeal);
  space_builder.SetBaselineAlgorithmType(
      parent_space.GetBaselineAlgorithmType());

  return space_builder.ToConstraintSpace();
}

BoxFragmentBuilder CreateContainerBuilderForMulticol(
    const BlockNode& multicol,
    const ConstraintSpace& space,
    const FragmentGeometry& fragment_geometry) {
  const ComputedStyle* style = &multicol.Style();
  BoxFragmentBuilder multicol_container_builder(
      multicol, style, space, style->GetWritingDirection(),
      /*previous_break_token=*/nullptr);
  multicol_container_builder.SetIsNewFormattingContext(true);
  multicol_container_builder.SetInitialFragmentGeometry(fragment_geometry);
  multicol_container_builder.SetIsBlockFragmentationContextRoot();

  return multicol_container_builder;
}

ConstraintSpace CreateConstraintSpaceForMulticol(const BlockNode& multicol) {
  WritingDirectionMode writing_direction_mode =
      multicol.Style().GetWritingDirection();
  ConstraintSpaceBuilder space_builder(writing_direction_mode.GetWritingMode(),
                                       writing_direction_mode,
                                       /* is_new_fc */ true);
  // This constraint space isn't going to be used for actual sizing. Yet,
  // someone will use it for initial geometry calculation, and if the multicol
  // has percentage sizes, DCHECKs will fail if we don't set any available size
  // at all.
  space_builder.SetAvailableSize(LogicalSize());
  return space_builder.ToConstraintSpace();
}

const BlockBreakToken* FindPreviousBreakToken(
    const PhysicalBoxFragment& fragment) {
  const LayoutBox* box = To<LayoutBox>(fragment.GetLayoutObject());
  DCHECK(box);
  DCHECK_GE(box->PhysicalFragmentCount(), 1u);

  // Bail early if this is the first fragment. There'll be no previous break
  // token then.
  if (fragment.IsFirstForNode())
    return nullptr;

  // If this isn't the first fragment, it means that there has to be multiple
  // fragments.
  DCHECK_GT(box->PhysicalFragmentCount(), 1u);

  const PhysicalBoxFragment* previous_fragment;
  if (const BlockBreakToken* break_token = fragment.GetBreakToken()) {
    // The sequence number of the outgoing break token is the same as the index
    // of this fragment.
    DCHECK_GE(break_token->SequenceNumber(), 1u);
    previous_fragment =
        box->GetPhysicalFragment(break_token->SequenceNumber() - 1);
  } else {
    // This is the last fragment, so its incoming break token will be the
    // outgoing one from the penultimate fragment.
    previous_fragment =
        box->GetPhysicalFragment(box->PhysicalFragmentCount() - 2);
  }
  return previous_fragment->GetBreakToken();
}

wtf_size_t BoxFragmentIndex(const PhysicalBoxFragment& fragment) {
  DCHECK(!fragment.IsInlineBox());
  const BlockBreakToken* token = FindPreviousBreakToken(fragment);
  return token ? token->SequenceNumber() + 1 : 0;
}

wtf_size_t PreviousInnerFragmentainerIndex(
    const PhysicalBoxFragment& fragment) {
  // This should be a fragmentation context root, typically a multicol
  // container.
  DCHECK(fragment.IsFragmentationContextRoot());

  const LayoutBox* box = To<LayoutBox>(fragment.GetLayoutObject());
  DCHECK_GE(box->PhysicalFragmentCount(), 1u);
  if (box->PhysicalFragmentCount() == 1)
    return 0;

  wtf_size_t idx = 0;
  // Walk the list of fragments generated by the node, until we reach the
  // specified one. Note that some fragments may not contain any fragmentainers
  // at all, if all the space is taken up by column spanners, for instance.
  for (const PhysicalBoxFragment& walker : box->PhysicalFragments()) {
    if (&walker == &fragment)
      return idx;
    // Find the last fragmentainer inside this fragment.
    auto children = walker.Children();
    for (auto& child : base::Reversed(children)) {
      if (!child->IsFragmentainerBox()) {
        // Not a fragmentainer (could be a spanner, OOF, etc.)
        continue;
      }
      const auto* token = To<BlockBreakToken>(child->GetBreakToken());
      idx = token->SequenceNumber() + 1;
      break;
    }
  }

  NOTREACHED_IN_MIGRATION();
  return idx;
}

PhysicalOffset OffsetInStitchedFragments(
    const PhysicalBoxFragment& fragment,
    PhysicalSize* out_stitched_fragments_size) {
  auto writing_direction = fragment.Style().GetWritingDirection();
  LayoutUnit stitched_block_size;
  LayoutUnit fragment_block_offset;
  const LayoutBox* layout_box = To<LayoutBox>(fragment.GetLayoutObject());
  const auto& first_fragment = *layout_box->GetPhysicalFragment(0);
  if (first_fragment.GetBreakToken() &&
      first_fragment.GetBreakToken()->IsRepeated()) {
    // Repeated content isn't stitched.
    stitched_block_size =
        LogicalFragment(writing_direction, first_fragment).BlockSize();
  } else {
    if (const auto* previous_break_token = FindPreviousBreakToken(fragment)) {
      fragment_block_offset = previous_break_token->ConsumedBlockSize();
    }
    if (fragment.IsOnlyForNode()) {
      stitched_block_size =
          LogicalFragment(writing_direction, fragment).BlockSize();
    } else {
      wtf_size_t idx = layout_box->PhysicalFragmentCount();
      DCHECK_GT(idx, 1u);
      idx--;
      // Calculating the stitched size is straight-forward if the node isn't
      // overflowed: Just add the consumed block-size of the last break token
      // and the block-size of the last fragment. If it is overflowed, on the
      // other hand, we need to search backwards until we find the end of the
      // block-end border edge.
      while (idx) {
        const PhysicalBoxFragment* walker =
            layout_box->GetPhysicalFragment(idx);
        stitched_block_size =
            LogicalFragment(writing_direction, *walker).BlockSize();

        // Look at the preceding break token.
        idx--;
        const BlockBreakToken* break_token =
            layout_box->GetPhysicalFragment(idx)->GetBreakToken();
        if (!break_token->IsAtBlockEnd()) {
          stitched_block_size += break_token->ConsumedBlockSize();
          break;
        }
      }
    }
  }
  LogicalSize stitched_fragments_logical_size(
      LogicalFragment(writing_direction, fragment).InlineSize(),
      stitched_block_size);
  PhysicalSize stitched_fragments_physical_size(ToPhysicalSize(
      stitched_fragments_logical_size, writing_direction.GetWritingMode()));
  if (out_stitched_fragments_size)
    *out_stitched_fragments_size = stitched_fragments_physical_size;
  LogicalOffset offset_in_stitched_box(LayoutUnit(), fragment_block_offset);
  WritingModeConverter converter(writing_direction,
                                 stitched_fragments_physical_size);
  return converter.ToPhysical(offset_in_stitched_box, fragment.Size());
}

LayoutUnit BlockSizeForFragmentation(
    const LayoutResult& result,
    WritingDirectionMode container_writing_direction) {
  LayoutUnit block_size = result.BlockSizeForFragmentation();
  if (block_size == kIndefiniteSize) {
    // Just use the border-box size of the fragment if block-size for
    // fragmentation hasn't been calculated. This happens for line boxes and any
    // other kind of monolithic content.
    WritingMode writing_mode = container_writing_direction.GetWritingMode();
    LogicalSize logical_size =
        result.GetPhysicalFragment().Size().ConvertToLogical(writing_mode);
    block_size = logical_size.block_size;
  }

  // Ruby annotations do not take up space in the line box, so we need this to
  // make sure that we don't let them cross the fragmentation line without
  // noticing.
  block_size += result.AnnotationBlockOffsetAdjustment();
  LayoutUnit annotation_overflow = result.AnnotationOverflow();
  if (annotation_overflow > LayoutUnit())
    block_size += annotation_overflow;

  return block_size;
}

bool CanPaintMultipleFragments(const PhysicalBoxFragment& fragment) {
  if (!fragment.IsCSSBox())
    return true;
  DCHECK(fragment.GetLayoutObject());
  return CanPaintMultipleFragments(*fragment.GetLayoutObject());
}

bool CanPaintMultipleFragments(const LayoutObject& layout_object) {
  const auto* layout_box = DynamicTo<LayoutBox>(&layout_object);
  // Only certain LayoutBox types are problematic.
  if (!layout_box)
    return true;

  DCHECK(!layout_box->IsFragmentLessBox());

  // If the object isn't monolithic, we're good.
  if (!layout_box->IsMonolithic()) {
    return true;
  }

  // There seems to be many issues preventing us from allowing repeated
  // scrollable containers, so we need to disallow them (unless we're printing,
  // in which case they're not really scrollable). Should we be able to fix all
  // the issues some day (after removing the legacy layout code), we could
  // change this policy. But for now we need to forbid this, which also means
  // that we cannot paint repeated text input form elements (because they use
  // scrollable containers internally) (if it makes sense at all to repeat form
  // elements...).
  if (layout_box->IsScrollContainer() &&
      !layout_object.GetDocument().Printing())
    return false;

  // It's somewhat problematic and strange to repeat most kinds of
  // LayoutReplaced (how would that make sense for iframes, for instance?). For
  // now, just allow regular images and SVGs. We may consider expanding this
  // list in the future. One reason for being extra strict for the time being is
  // legacy layout / paint code, but it may be that it doesn't make a lot of
  // sense to repeat too many types of replaced content, even if we should
  // become technically capable of doing it.
  if (layout_box->IsLayoutReplaced()) {
    if (layout_box->IsLayoutImage() && !layout_box->IsMedia())
      return true;
    if (layout_box->IsSVGRoot())
      return true;
    return false;
  }

  if (auto* element = DynamicTo<Element>(layout_box->GetNode())) {
    // We're already able to support *some* types of form controls, but for now,
    // just disallow everything. Does it even make sense to allow repeated form
    // controls?
    if (element->IsFormControlElement())
      return false;
  }

  return true;
}

}  // namespace blink
