// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_column_layout_algorithm.h"

#include <algorithm>
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_fragment_geometry.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_margin_strut.h"
#include "third_party/blink/renderer/core/layout/ng/list/ng_unpositioned_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_column_spanner_path.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

// An itinerary of multicol container parts to walk separately for layout. A
// part is either a chunk of regular column content, or a column spanner.
class MulticolPartWalker {
  STACK_ALLOCATED();

 public:
  // What to lay out or process next.
  struct Entry {
    STACK_ALLOCATED();

   public:
    Entry() = default;
    Entry(const NGBlockBreakToken* token, NGBlockNode spanner)
        : break_token(token), spanner(spanner) {}

    // The incoming break token for the content to process, or null if we're at
    // the start.
    const NGBlockBreakToken* break_token = nullptr;

    // The column spanner node to process, or null if we're dealing with regular
    // column content.
    NGBlockNode spanner = nullptr;
  };

  MulticolPartWalker(NGBlockNode multicol_container,
                     const NGBlockBreakToken* break_token)
      : multicol_container_(multicol_container),
        parent_break_token_(break_token),
        child_token_idx_(0) {
    UpdateCurrent();
    // The first entry in the first multicol fragment may be empty (that just
    // means that we haven't started yet), but if this happens anywhere else, it
    // means that we're finished. Nothing inside this multicol container left to
    // process.
    if (IsBreakInside(parent_break_token_) && !current_.break_token &&
        parent_break_token_->HasSeenAllChildren())
      is_finished_ = true;
  }

  Entry Current() const {
    DCHECK(!is_finished_);
    return current_;
  }

  bool IsFinished() const { return is_finished_; }

  // Move to the next part.
  void Next();

  // Move over to the specified spanner, and take it from there.
  void MoveToSpanner(NGBlockNode spanner,
                     const NGBlockBreakToken* next_column_token);

  // Push a break token for the column content to resume at.
  void AddNextColumnBreakToken(const NGBlockBreakToken& next_column_token);

  // If a column was added for an OOF before a spanner, we need to update the
  // column break token so that the content is resumed at the correct spot.
  void UpdateNextColumnBreakToken(
      const NGFragmentBuilder::ChildrenVector& children);

 private:
  void MoveToNext();
  void UpdateCurrent();

  Entry current_;
  NGBlockNode spanner_ = nullptr;
  NGBlockNode multicol_container_;
  const NGBlockBreakToken* parent_break_token_;
  const NGBlockBreakToken* next_column_token_ = nullptr;

  // An index into parent_break_token_'s ChildBreakTokens() vector. Used for
  // keeping track of the next child break token to inspect.
  wtf_size_t child_token_idx_;

  bool is_finished_ = false;
};

void MulticolPartWalker::Next() {
  if (is_finished_)
    return;
  MoveToNext();
  if (!is_finished_)
    UpdateCurrent();
}

void MulticolPartWalker::MoveToSpanner(
    NGBlockNode spanner,
    const NGBlockBreakToken* next_column_token) {
  *this = MulticolPartWalker(multicol_container_, nullptr);
  DCHECK(spanner.IsColumnSpanAll());
  spanner_ = spanner;
  next_column_token_ = next_column_token;
  UpdateCurrent();
}

void MulticolPartWalker::AddNextColumnBreakToken(
    const NGBlockBreakToken& next_column_token) {
  *this = MulticolPartWalker(multicol_container_, nullptr);
  next_column_token_ = &next_column_token;
  UpdateCurrent();
}

void MulticolPartWalker::UpdateNextColumnBreakToken(
    const NGFragmentBuilder::ChildrenVector& children) {
  if (children.empty())
    return;
  const blink::NGPhysicalFragment* last_child =
      children[children.size() - 1].fragment;
  if (!last_child->IsColumnBox())
    return;
  const auto* child_break_token =
      To<NGBlockBreakToken>(last_child->BreakToken());
  if (child_break_token && child_break_token != next_column_token_)
    next_column_token_ = child_break_token;
}

void MulticolPartWalker::UpdateCurrent() {
  DCHECK(!is_finished_);
  if (parent_break_token_) {
    const auto& child_break_tokens = parent_break_token_->ChildBreakTokens();
    if (child_token_idx_ < child_break_tokens.size()) {
      const auto* child_break_token =
          To<NGBlockBreakToken>(child_break_tokens[child_token_idx_].Get());
      if (child_break_token->InputNode() == multicol_container_) {
        current_.spanner = nullptr;
      } else {
        current_.spanner = To<NGBlockNode>(child_break_token->InputNode());
        DCHECK(current_.spanner.IsColumnSpanAll());
      }
      current_.break_token = child_break_token;
      return;
    }
  }

  if (spanner_) {
    current_ = Entry(/* break_token */ nullptr, spanner_);
    return;
  }

  if (next_column_token_) {
    current_ = Entry(next_column_token_, /* spanner */ nullptr);
    return;
  }

  // The current entry is empty. That's only the case when we're at the very
  // start of the multicol container, or if we're past all children.
  DCHECK(!is_finished_);
  DCHECK(!current_.spanner);
  DCHECK(!current_.break_token);
}

void MulticolPartWalker::MoveToNext() {
  if (parent_break_token_) {
    const auto& child_break_tokens = parent_break_token_->ChildBreakTokens();
    if (child_token_idx_ < child_break_tokens.size()) {
      child_token_idx_++;
      // If we have more incoming break tokens, we'll use that.
      if (child_token_idx_ < child_break_tokens.size())
        return;
      // We just ran out of break tokens. Fall through.
    }
  }

  if (spanner_) {
    NGLayoutInputNode next = spanner_.NextSibling();
    // Otherwise, if there's a next spanner, we'll use that.
    if (next && next.IsColumnSpanAll()) {
      spanner_ = To<NGBlockNode>(next);
      return;
    }
    spanner_ = nullptr;

    // Otherwise, if we have column content to resume at, use that.
    if (next_column_token_)
      return;
  }

  // Otherwise, we're done.
  is_finished_ = true;
}

NGBlockNode GetSpannerFromPath(const NGColumnSpannerPath* path) {
  while (path->Child())
    path = path->Child();
  DCHECK(path->BlockNode().IsColumnSpanAll());
  return path->BlockNode();
}

}  // namespace

NGColumnLayoutAlgorithm::NGColumnLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {
  // When a list item has multicol, |NGColumnLayoutAlgorithm| needs to keep
  // track of the list marker instead of the child layout algorithm. See
  // |NGBlockLayoutAlgorithm|.
  if (const NGBlockNode marker_node = Node().ListMarkerBlockNodeIfListItem()) {
    if (!marker_node.ListMarkerOccupiesWholeLine() &&
        (!BreakToken() || BreakToken()->HasUnpositionedListMarker())) {
      container_builder_.SetUnpositionedListMarker(
          NGUnpositionedListMarker(marker_node));
    }
  }
}

const NGLayoutResult* NGColumnLayoutAlgorithm::Layout() {
  const LogicalSize border_box_size = container_builder_.InitialBorderBoxSize();
  // TODO(mstensho): This isn't the content-box size, as
  // |BorderScrollbarPadding()| has been adjusted for fragmentation. Verify
  // that this is the correct size.
  column_block_size_ =
      ShrinkLogicalSize(border_box_size, BorderScrollbarPadding()).block_size;

  DCHECK_GE(ChildAvailableSize().inline_size, LayoutUnit());
  column_inline_size_ =
      ResolveUsedColumnInlineSize(ChildAvailableSize().inline_size, Style());

  column_inline_progression_ =
      column_inline_size_ +
      ResolveUsedColumnGap(ChildAvailableSize().inline_size, Style());
  used_column_count_ =
      ResolveUsedColumnCount(ChildAvailableSize().inline_size, Style());

  // Write the column inline-size and count back to the legacy flow thread if
  // we're at the first fragment. TextAutosizer needs the inline-size, and the
  // legacy fragmentainer group machinery needs the count.
  if (!IsBreakInside(BreakToken()))
    node_.StoreColumnSizeAndCount(column_inline_size_, used_column_count_);

  // If we know the block-size of the fragmentainers in an outer fragmentation
  // context (if any), our columns may be constrained by that, meaning that we
  // may have to fragment earlier than what we would have otherwise, and, if
  // that's the case, that we may also not create overflowing columns (in the
  // inline axis), but rather finish the row and resume in the next row in the
  // next outer fragmentainer. Note that it is possible to be nested inside a
  // fragmentation context that doesn't know the block-size of its
  // fragmentainers. This would be in the first layout pass of an outer multicol
  // container, before any tentative column block-size has been calculated.
  is_constrained_by_outer_fragmentation_context_ =
      ConstraintSpace().HasKnownFragmentainerBlockSize();

  container_builder_.SetIsBlockFragmentationContextRoot();

  intrinsic_block_size_ = BorderScrollbarPadding().block_start;

  NGBreakStatus break_status = LayoutChildren();
  if (break_status == NGBreakStatus::kNeedsEarlierBreak) {
    // We need to discard this layout and do it again. We found an earlier break
    // point that's more appealing than the one we ran out of space at.
    return RelayoutAndBreakEarlier<NGColumnLayoutAlgorithm>(
        container_builder_.EarlyBreak());
  } else if (break_status == NGBreakStatus::kBrokeBefore) {
    // If we want to break before, make sure that we're actually at the start.
    DCHECK(!IsBreakInside(BreakToken()));

    return container_builder_.Abort(NGLayoutResult::kOutOfFragmentainerSpace);
  }

  intrinsic_block_size_ =
      std::max(intrinsic_block_size_, BorderScrollbarPadding().block_start);
  intrinsic_block_size_ += BorderScrollbarPadding().block_end;

  // Figure out how much space we've already been able to process in previous
  // fragments, if this multicol container participates in an outer
  // fragmentation context.
  LayoutUnit previously_consumed_block_size;
  if (const auto* token = BreakToken())
    previously_consumed_block_size = token->ConsumedBlockSize();

  intrinsic_block_size_ =
      ClampIntrinsicBlockSize(ConstraintSpace(), Node(), BreakToken(),
                              BorderScrollbarPadding(), intrinsic_block_size_);

  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), BorderPadding(),
      previously_consumed_block_size + intrinsic_block_size_,
      border_box_size.inline_size);

  container_builder_.SetFragmentsTotalBlockSize(block_size);
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size_);
  container_builder_.SetBlockOffsetForAdditionalColumns(
      CurrentContentBlockOffset(intrinsic_block_size_));

  PositionAnyUnclaimedListMarker();

  if (UNLIKELY(InvolvedInBlockFragmentation(container_builder_))) {
    // In addition to establishing one, we're nested inside another
    // fragmentation context.
    FinishFragmentation(Node(), ConstraintSpace(), BorderPadding().block_end,
                        FragmentainerSpaceLeft(ConstraintSpace()),
                        &container_builder_);

    // OOF positioned elements inside a nested fragmentation context are laid
    // out at the outermost context. If this multicol has OOF positioned
    // elements pending layout, store its node for later use.
    if (container_builder_.HasOutOfFlowFragmentainerDescendants()) {
      container_builder_.AddMulticolWithPendingOOFs(Node());
    }

    // Read the intrinsic block-size back, since it may have been reduced due to
    // fragmentation.
    intrinsic_block_size_ = container_builder_.IntrinsicBlockSize();
  } else {
#if DCHECK_IS_ON()
    // If we're not participating in a fragmentation context, no block
    // fragmentation related fields should have been set.
    container_builder_.CheckNoBlockFragmentation();
#endif
  }

  if (ConstraintSpace().IsTableCell()) {
    NGTableAlgorithmUtils::FinalizeTableCellLayout(intrinsic_block_size_,
                                                   &container_builder_);
  }

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();

  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGColumnLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  const LayoutUnit override_intrinsic_inline_size =
      Node().OverrideIntrinsicContentInlineSize();
  if (override_intrinsic_inline_size != kIndefiniteSize) {
    const LayoutUnit size =
        BorderScrollbarPadding().InlineSum() + override_intrinsic_inline_size;
    return {{size, size}, /* depends_on_block_constraints */ false};
  }

  // First calculate the min/max sizes of columns.
  NGConstraintSpace space = CreateConstraintSpaceForMinMax();
  NGFragmentGeometry fragment_geometry = CalculateInitialFragmentGeometry(
      space, Node(), /* break_token */ nullptr, /* is_intrinsic */ true);
  NGBlockLayoutAlgorithm algorithm({Node(), fragment_geometry, space});
  MinMaxSizesResult result =
      algorithm.ComputeMinMaxSizes(MinMaxSizesFloatInput());

  // How column-width affects min/max sizes is currently not defined in any
  // spec, but there used to be a definition, which everyone still follows to
  // some extent:
  // https://www.w3.org/TR/2016/WD-css-sizing-3-20160510/#multicol-intrinsic
  //
  // GitHub issue for getting this back into some spec:
  // https://github.com/w3c/csswg-drafts/issues/1742
  if (!Style().HasAutoColumnWidth()) {
    // One peculiarity in the (old and only) spec is that column-width may
    // shrink min intrinsic inline-size to become less than what the contents
    // require:
    //
    // "The min-content inline size of a multi-column element with a computed
    // column-width not auto is the smaller of its column-width and the largest
    // min-content inline-size contribution of its contents."
    const LayoutUnit column_width(Style().ColumnWidth());
    result.sizes.min_size = std::min(result.sizes.min_size, column_width);
    result.sizes.max_size = std::max(result.sizes.max_size, column_width);
    result.sizes.max_size =
        std::max(result.sizes.max_size, result.sizes.min_size);
  }

  // Now convert those column min/max values to multicol container min/max
  // values. We typically have multiple columns and also gaps between them.
  int column_count = Style().ColumnCount();
  DCHECK_GE(column_count, 1);
  LayoutUnit column_gap = ResolveUsedColumnGap(LayoutUnit(), Style());
  LayoutUnit gap_extra = column_gap * (column_count - 1);

  // Another peculiarity in the (old and only) spec (see above) is that
  // column-count (and therefore also column-gap) is ignored in intrinsic min
  // inline-size calculation, if column-width is specified.
  if (Style().HasAutoColumnWidth()) {
    result.sizes.min_size *= column_count;
    result.sizes.min_size += gap_extra;
  }
  result.sizes.max_size *= column_count;
  result.sizes.max_size += gap_extra;

  // The block layout algorithm skips spanners for min/max calculation (since
  // they shouldn't be part of the column-count multiplication above). Calculate
  // min/max inline-size for spanners now.
  if (!Node().ShouldApplyInlineSizeContainment())
    result.sizes.Encompass(ComputeSpannersMinMaxSizes(Node()).sizes);

  result.sizes += BorderScrollbarPadding().InlineSum();
  return result;
}

MinMaxSizesResult NGColumnLayoutAlgorithm::ComputeSpannersMinMaxSizes(
    const NGBlockNode& search_parent) const {
  MinMaxSizesResult result;
  for (NGLayoutInputNode child = search_parent.FirstChild(); child;
       child = child.NextSibling()) {
    const NGBlockNode* child_block = DynamicTo<NGBlockNode>(&child);
    if (!child_block)
      continue;
    MinMaxSizesResult child_result;
    if (!child_block->IsColumnSpanAll()) {
      // Spanners don't need to be a direct child of the multicol container, but
      // they need to be in its formatting context.
      if (child_block->CreatesNewFormattingContext())
        continue;
      child_result = ComputeSpannersMinMaxSizes(*child_block);
    } else {
      NGMinMaxConstraintSpaceBuilder builder(
          ConstraintSpace(), Style(), *child_block, /* is_new_fc */ true);
      builder.SetAvailableBlockSize(ChildAvailableSize().block_size);
      const NGConstraintSpace child_space = builder.ToConstraintSpace();
      child_result = ComputeMinAndMaxContentContribution(Style(), *child_block,
                                                         child_space);
    }
    result.sizes.Encompass(child_result.sizes);
  }
  return result;
}

NGBreakStatus NGColumnLayoutAlgorithm::LayoutChildren() {
  NGMarginStrut margin_strut;
  MulticolPartWalker walker(Node(), BreakToken());
  while (!walker.IsFinished()) {
    auto entry = walker.Current();
    const auto* child_break_token = To<NGBlockBreakToken>(entry.break_token);

    // If this is regular column content (i.e. not a spanner), or we're at the
    // very start, perform column layout. If we're at the very start, and even
    // if the child is a spanner (which means that we won't be able to lay out
    // any column content at all), we still need to enter here, because that's
    // how we create a break token for the column content to resume at. With no
    // break token, we wouldn't be able to resume layout after the any initial
    // spanners.
    if (!entry.spanner) {
      const NGLayoutResult* result =
          LayoutRow(child_break_token, &margin_strut);

      if (!result) {
        // Not enough outer fragmentainer space to produce any columns at all.

        if (intrinsic_block_size_) {
          // We have preceding initial border/padding, or a column spanner
          // (possibly preceded by other spanners or even column content). So we
          // need to break inside the multicol container. Stop walking the
          // children, but "continue" layout, so that we produce a
          // fragment. Note that we normally don't want to break right after
          // initial border/padding, but will do so as a last resort. It's up to
          // our containing block to decide what's best. If there's no incoming
          // break token, it means that we're at the very start of column
          // layout, and we need to create a break token before the first
          // column.
          if (!child_break_token) {
            container_builder_.AddBreakBeforeChild(
                Node(), kBreakAppealLastResort, /* is_forced_break */ false);
          }

          break;
        }
        // Otherwise we have nothing here, and need to break before the multicol
        // container. No fragment will be produced.
        DCHECK(!IsBreakInside(BreakToken()));
        return NGBreakStatus::kBrokeBefore;
      }

      walker.Next();

      const auto* next_column_token =
          To<NGBlockBreakToken>(result->PhysicalFragment().BreakToken());

      if (const NGColumnSpannerPath* path = result->ColumnSpannerPath()) {
        // We found a spanner, and if there's column content to resume at after
        // it, |next_column_token| will be set. Move the walker to the
        // spanner. We'll now walk that spanner and any sibling spanners, before
        // resuming at |next_column_token|.
        NGBlockNode spanner_node = GetSpannerFromPath(path);
        walker.MoveToSpanner(spanner_node, next_column_token);
        continue;
      }

      // If we didn't find a spanner, it either means that we're through
      // everything, or that column layout needs to continue from the next outer
      // fragmentainer.
      if (next_column_token)
        walker.AddNextColumnBreakToken(*next_column_token);

      break;
    }

    // Attempt to lay out one column spanner.

    NGBlockNode spanner_node = entry.spanner;

    // If this is the child we had previously determined to break before, do so
    // now and finish layout.
    if (early_break_ &&
        IsEarlyBreakTarget(*early_break_, container_builder_, spanner_node))
      break;

    // Handle any OOF fragmentainer descendants that were found before the
    // spanner.
    NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_)
        .HandleFragmentation();
    walker.UpdateNextColumnBreakToken(container_builder_.Children());

    NGBreakStatus break_status =
        LayoutSpanner(spanner_node, child_break_token, &margin_strut);

    walker.Next();

    if (break_status == NGBreakStatus::kNeedsEarlierBreak)
      return break_status;
    if (break_status == NGBreakStatus::kBrokeBefore ||
        container_builder_.HasInflowChildBreakInside()) {
      break;
    }
  }

  if (!walker.IsFinished() || container_builder_.HasInflowChildBreakInside()) {
    // We broke in the main flow. Let this multicol container take up any
    // remaining space.
    intrinsic_block_size_ = std::max(intrinsic_block_size_,
                                     FragmentainerSpaceLeft(ConstraintSpace()));

    // Go through any remaining parts that we didn't get to, and push them as
    // break tokens for the next (outer) fragmentainer to handle.
    for (; !walker.IsFinished(); walker.Next()) {
      auto entry = walker.Current();
      if (entry.break_token) {
        // Copy unhandled incoming break tokens, for the next (outer)
        // fragmentainer.
        container_builder_.AddBreakToken(entry.break_token);
      } else if (entry.spanner) {
        // Create break tokens for the spanners that were discovered (but not
        // handled) while laying out this (outer) fragmentainer, so that they
        // get resumed in the next one (or pushed again, if it won't fit there
        // either).
        container_builder_.AddBreakBeforeChild(
            entry.spanner, kBreakAppealPerfect, /* is_forced_break */ false);
      }
    }
  } else {
    // We've gone through all the content. This doesn't necessarily mean that
    // we're done fragmenting, since the multicol container may be taller than
    // what the content requires, which means that we might create more
    // (childless) fragments, if we're nested inside another fragmentation
    // context. In that case we must make sure to skip the contents when
    // resuming.
    container_builder_.SetHasSeenAllChildren();

    // TODO(mstensho): Truncate the child margin if it overflows the
    // fragmentainer, by using AdjustedMarginAfterFinalChildFragment().

    intrinsic_block_size_ += margin_strut.Sum();
  }

  return NGBreakStatus::kContinue;
}

struct ResultWithOffset {
  DISALLOW_NEW();

 public:
  Member<const NGLayoutResult> result;
  LogicalOffset offset;

  ResultWithOffset(const NGLayoutResult* result, LogicalOffset offset)
      : result(result), offset(offset) {}

  const NGPhysicalBoxFragment& Fragment() const {
    return To<NGPhysicalBoxFragment>(result->PhysicalFragment());
  }

  void Trace(Visitor* visitor) const { visitor->Trace(result); }
};

const NGLayoutResult* NGColumnLayoutAlgorithm::LayoutRow(
    const NGBlockBreakToken* next_column_token,
    NGMarginStrut* margin_strut) {
  LogicalSize column_size(column_inline_size_, column_block_size_);

  // Calculate the block-offset by including any trailing margin from a previous
  // adjacent column spanner. We will not reset the margin strut just yet, as we
  // first need to figure out if there's any content at all inside the columns.
  // If there isn't, it should be possible to collapse the margin through the
  // row (and as far as the spec is concerned, the row won't even exist then).
  LayoutUnit row_offset = intrinsic_block_size_ + margin_strut->Sum();

  // If block-size is non-auto, subtract the space for content we've consumed in
  // previous fragments. This is necessary when we're nested inside another
  // fragmentation context.
  if (column_size.block_size != kIndefiniteSize) {
    if (BreakToken() && is_constrained_by_outer_fragmentation_context_)
      column_size.block_size -= BreakToken()->ConsumedBlockSize();

    // Subtract the space already taken in the current fragment (spanners and
    // earlier column rows).
    column_size.block_size -= CurrentContentBlockOffset(row_offset);

    column_size.block_size = column_size.block_size.ClampNegativeToZero();
  }

  bool may_resume_in_next_outer_fragmentainer = false;
  LayoutUnit available_outer_space = kIndefiniteSize;
  if (is_constrained_by_outer_fragmentation_context_) {
    available_outer_space =
        UnclampedFragmentainerSpaceLeft(ConstraintSpace()) - row_offset;

    if (available_outer_space <= LayoutUnit()) {
      if (available_outer_space < LayoutUnit()) {
        // We're past the end of the outer fragmentainer (typically due to a
        // margin). Nothing will fit here, not even zero-size content. If we
        // haven't produced any fragments yet, and aborting is allowed, we'll
        // retry in the next outer fragmentainer. Otherwise, we need to continue
        // (once we have started laying out, we cannot skip any fragmentainers)
        // with no available size.
        if (ConstraintSpace().IsInsideBalancedColumns() &&
            !container_builder_.IsInitialColumnBalancingPass())
          container_builder_.PropagateSpaceShortage(-available_outer_space);
        if (!IsBreakInside(BreakToken()) && MayAbortOnInsufficientSpace())
          return nullptr;
        available_outer_space = LayoutUnit();
      }

      // We are out of space, but we're exactly at the end of the outer
      // fragmentainer. If none of our contents take up space, we're going to
      // fit, otherwise not. Lay out and find out.
    }

    // Determine if we should resume layout in the next outer fragmentation
    // context if we run out of space in the current one. This is always the
    // thing to do except when block-size is non-auto and short enough to fit in
    // the current outer fragmentainer. In such cases we'll allow inner columns
    // to overflow its outer fragmentainer (since the inner multicol is too
    // short to reach the outer fragmentation line).
    if (column_size.block_size == kIndefiniteSize ||
        column_size.block_size > available_outer_space)
      may_resume_in_next_outer_fragmentainer = true;
  }

  bool shrink_to_fit_column_block_size = false;

  // If column-fill is 'balance', we should of course balance. Additionally, we
  // need to do it if we're *inside* another multicol container that's
  // performing its initial column balancing pass. Otherwise we might report a
  // taller block-size that we eventually end up with, resulting in the outer
  // columns to be overstretched.
  bool balance_columns = Style().GetColumnFill() == EColumnFill::kBalance ||
                         (ConstraintSpace().HasBlockFragmentation() &&
                          !ConstraintSpace().HasKnownFragmentainerBlockSize());

  // If columns are to be balanced, we need to examine the contents of the
  // multicol container to figure out a good initial (minimal) column
  // block-size. We also need to do this if column-fill is 'auto' and the
  // block-size is unconstrained.
  bool has_content_based_block_size =
      balance_columns || (column_size.block_size == kIndefiniteSize &&
                          !is_constrained_by_outer_fragmentation_context_);

  if (has_content_based_block_size) {
    column_size.block_size = ResolveColumnAutoBlockSize(
        column_size, row_offset, next_column_token, balance_columns);
  } else if (available_outer_space != kIndefiniteSize) {
    // Finally, resolve any remaining auto block-size, and make sure that we
    // don't take up more space than there's room for in the outer fragmentation
    // context.
    if (column_size.block_size > available_outer_space ||
        column_size.block_size == kIndefiniteSize) {
      // If the block-size of the inner multicol is unconstrained, we'll let the
      // outer fragmentainer context constrain it. However, if the inner
      // multicol only has content for one column (in the current row), and only
      // fills it partially, we need to shrink its block-size, to make room for
      // any content that follows the inner multicol, rather than eating the
      // entire fragmentainer.
      if (column_size.block_size == kIndefiniteSize)
        shrink_to_fit_column_block_size = true;
      column_size.block_size = available_outer_space;
    }
  }

  DCHECK_GE(column_size.block_size, LayoutUnit());

  // New column fragments won't be added to the fragment builder right away,
  // since we may need to delete them and try again with a different block-size
  // (colum balancing). Keep them in this list, and add them to the fragment
  // builder when we have the final column fragments. Or clear the list and
  // retry otherwise.
  HeapVector<ResultWithOffset, 16> new_columns;

  bool is_empty_spanner_parent = false;

  // Avoid suboptimal breaks (and overflow from monolithic content) inside a
  // nested multicol container if we can. If this multicol container may
  // continue in the next outer fragmentainer, and we have already made some
  // progress (either inside the multicol container itself (spanners or
  // block-start border/padding), or in the outer fragmentation context), it may
  // be better to push some of the content to the next outer fragmentainer and
  // retry there.
  bool may_have_more_space_in_next_outer_fragmentainer = false;
  if (may_resume_in_next_outer_fragmentainer && !IsBreakInside(BreakToken())) {
    if (intrinsic_block_size_)
      may_have_more_space_in_next_outer_fragmentainer = true;
    else if (!ConstraintSpace().IsAtFragmentainerStart())
      may_have_more_space_in_next_outer_fragmentainer = true;
  }

  const NGLayoutResult* result = nullptr;
  absl::optional<NGBreakAppeal> min_break_appeal;
  LayoutUnit intrinsic_block_size_contribution;

  do {
    const NGBlockBreakToken* column_break_token = next_column_token;

    bool allow_discard_start_margin =
        column_break_token && !column_break_token->IsCausedByColumnSpanner();
    bool has_violating_break = false;
    bool has_oof_fragmentainer_descendants = false;

    LayoutUnit column_inline_offset(BorderScrollbarPadding().inline_start);
    int actual_column_count = 0;
    int forced_break_count = 0;

    // Each column should calculate their own minimal space shortage. Find the
    // lowest value of those. This will serve as the column stretch amount, if
    // we determine that stretching them is necessary and possible (column
    // balancing).
    LayoutUnit minimal_space_shortage = kIndefiniteSize;

    min_break_appeal = absl::nullopt;
    intrinsic_block_size_contribution = LayoutUnit();

    do {
      // Lay out one column. Each column will become a fragment.
      NGConstraintSpace child_space = CreateConstraintSpaceForFragmentainer(
          ConstraintSpace(), kFragmentColumn, column_size,
          ColumnPercentageResolutionSize(), allow_discard_start_margin,
          balance_columns, min_break_appeal.value_or(kBreakAppealLastResort));

      NGFragmentGeometry fragment_geometry =
          CalculateInitialFragmentGeometry(child_space, Node(), BreakToken());

      NGLayoutAlgorithmParams params(Node(), fragment_geometry, child_space,
                                     column_break_token);
      params.column_spanner_path = spanner_path_;

      NGBlockLayoutAlgorithm child_algorithm(params);
      child_algorithm.SetBoxType(NGPhysicalFragment::kColumnBox);
      result = child_algorithm.Layout();
      const auto& column =
          To<NGPhysicalBoxFragment>(result->PhysicalFragment());
      intrinsic_block_size_contribution = column_size.block_size;
      if (shrink_to_fit_column_block_size) {
        // Shrink-to-fit the row block-size contribution from the first column
        // if we're nested inside another fragmentation context. The column
        // block-size that we use in auto-filled (non-balanced) inner multicol
        // containers with unconstrained block-size is set to the available
        // block-size in the outer fragmentation context. If we end up with just
        // one inner column in this row, we should shrink the inner multicol
        // container fragment, so that it doesn't take up the entire outer
        // fragmentainer needlessly. So clamp it to the total block-size of the
        // contents in the column (including overflow).
        //
        // TODO(layout-dev): It would be slightly nicer if we actually shrunk
        // the block-size of the column fragment (in
        // FinishFragmentationForFragmentainer()) instead of just cropping the
        // block-size of the multicol container here, but that would cause
        // trouble for out-of-flow positioned descendants that extend past the
        // end of in-flow content, which benefit from "full" column block-size.
        intrinsic_block_size_contribution =
            std::min(intrinsic_block_size_contribution,
                     result->BlockSizeForFragmentation());
        shrink_to_fit_column_block_size = false;
      }

      if (!has_oof_fragmentainer_descendants && balance_columns &&
          NGFragmentedOutOfFlowData::
              HasOutOfFlowPositionedFragmentainerDescendants(column))
        has_oof_fragmentainer_descendants = true;

      // Add the new column fragment to the list, but don't commit anything to
      // the fragment builder until we know whether these are the final columns.
      LogicalOffset logical_offset(column_inline_offset, row_offset);
      new_columns.emplace_back(result, logical_offset);

      absl::optional<LayoutUnit> space_shortage =
          result->MinimalSpaceShortage();
      UpdateMinimalSpaceShortage(space_shortage, &minimal_space_shortage);
      actual_column_count++;

      if (result->ColumnSpannerPath()) {
        is_empty_spanner_parent = result->IsEmptySpannerParent();
        break;
      }

      has_violating_break |= result->BreakAppeal() != kBreakAppealPerfect;
      column_inline_offset += column_inline_progression_;

      if (result->HasForcedBreak())
        forced_break_count++;

      column_break_token = column.BreakToken();

      // If we're participating in an outer fragmentation context, we'll only
      // allow as many columns as the used value of column-count, so that we
      // don't overflow in the inline direction. There's one important
      // exception: If we have determined that this is going to be the last
      // fragment for this multicol container in the outer fragmentation
      // context, we'll just allow as many columns as needed (and let them
      // overflow in the inline direction, if necessary). We're not going to
      // progress into a next outer fragmentainer if the (remaining part of the)
      // multicol container fits block-wise in the current outer fragmentainer.
      if (may_resume_in_next_outer_fragmentainer && column_break_token &&
          actual_column_count >= used_column_count_)
        break;

      if (may_have_more_space_in_next_outer_fragmentainer) {
        // If the outer fragmentainer already has content progress (before this
        // row), we are in a situation where there may be more space for us
        // (block-size) in the next outer fragmentainer. This means that it may
        // be possible to avoid suboptimal breaks if we push content to a column
        // row in the next outer fragmentainer. Therefore, avoid breaks with
        // lower appeal than what we've seen so far. Anything that would cause
        // "too severe" breaking violations will be pushed to the next outer
        // fragmentainer.
        min_break_appeal =
            std::min(min_break_appeal.value_or(kBreakAppealPerfect),
                     result->BreakAppeal());

        // Avoid creating rows that are too short to hold monolithic content.
        // Bail if possible, discarding all columns. Note that this is safe to
        // do even if we're column-balancing, because we attempt to make room
        // for all monolithic content already in the initial column balancing
        // pass (and if that fails, there's no way it's going to fit), by
        // checking TallestUnbreakableBlockSize() from the layout results.
        LayoutUnit block_end_overflow =
            NGBoxFragment(ConstraintSpace().GetWritingDirection(), column)
                .BlockEndLayoutOverflow();
        if (row_offset + block_end_overflow >
            FragmentainerSpaceLeft(ConstraintSpace())) {
          if (ConstraintSpace().IsInsideBalancedColumns() &&
              !container_builder_.IsInitialColumnBalancingPass())
            container_builder_.PropagateSpaceShortage(minimal_space_shortage);
          if (MayAbortOnInsufficientSpace())
            return nullptr;
        }
      }
      allow_discard_start_margin = true;
    } while (column_break_token);

    if (!balance_columns) {
      if (result->ColumnSpannerPath()) {
        // We always have to balance columns preceding a spanner, so if we
        // didn't do that initially, switch over to column balancing mode now,
        // and lay out again.
        balance_columns = true;
        new_columns.clear();
        column_size.block_size = ResolveColumnAutoBlockSize(
            column_size, row_offset, next_column_token, balance_columns);
        continue;
      }

      // Balancing not enabled. We're done.
      break;
    }

    // Any OOFs contained within this multicol get laid out once all columns
    // complete layout. However, OOFs should affect column balancing. Pass the
    // current set of columns into NGOutOfFlowLayoutPart to determine if OOF
    // layout will affect column balancing in any way (without actually adding
    // the OOF results to the builder - this will be handled at a later point).
    if (has_oof_fragmentainer_descendants) {
      // If, for example, the columns get split by a column spanner, the offset
      // of an OOF's containing block will be relative to the first
      // fragmentainer in the first row. However, we are only concerned about
      // the current row of columns, so we should adjust the containing block
      // offsets to be relative to the first column in the current row.
      LayoutUnit containing_block_adjustment = -TotalColumnBlockSize();

      NGOutOfFlowLayoutPart::ColumnBalancingInfo column_balancing_info;
      for (wtf_size_t i = 0; i < new_columns.size(); i++) {
        auto& new_column = new_columns[i];
        column_balancing_info.columns.push_back(
            NGLogicalLink{&new_column.Fragment(), new_column.offset});

        // Because the current set of columns haven't been added to the builder
        // yet, any OOF descendants won't have been propagated up yet. Instead,
        // propagate any OOF descendants up to |column_balancing_info| so that
        // they can be passed into NGOutOfFlowLayoutPart (without affecting the
        // builder).
        container_builder_.PropagateOOFFragmentainerDescendants(
            new_column.Fragment(), new_column.offset,
            /* relative_offset */ LogicalOffset(), containing_block_adjustment,
            /* containing_block */ nullptr,
            /* fixedpos_containing_block */ nullptr,
            &column_balancing_info.out_of_flow_fragmentainer_descendants);
      }
      DCHECK(column_balancing_info.HasOutOfFlowFragmentainerDescendants());

      NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_)
          .HandleFragmentation(&column_balancing_info);
      actual_column_count += column_balancing_info.num_new_columns;
      if (column_balancing_info.minimal_space_shortage > LayoutUnit()) {
        UpdateMinimalSpaceShortage(column_balancing_info.minimal_space_shortage,
                                   &minimal_space_shortage);
      }
      if (!has_violating_break)
        has_violating_break = column_balancing_info.has_violating_break;
    }

    // We're balancing columns. Check if the column block-size that we laid out
    // with was satisfactory. If not, stretch and retry, if possible.
    //
    // If we didn't break at any undesirable location and actual column count
    // wasn't larger than what we have room for, we're done IF we're also out of
    // content (no break token; in nested multicol situations there are cases
    // where we only allow as many columns as we have room for, as additional
    // columns normally need to continue in the next outer fragmentainer). If we
    // have made the columns tall enough to bump into a spanner, it also means
    // we need to stop to lay out the spanner(s), and resume column layout
    // afterwards.
    if (!has_violating_break && actual_column_count <= used_column_count_ &&
        (!column_break_token || result->ColumnSpannerPath()))
      break;

    // Attempt to stretch the columns.
    LayoutUnit new_column_block_size;
    if (used_column_count_ <= forced_break_count + 1) {
      // If we have no soft break opportunities (because forced breaks cause too
      // many breaks already), there's no stretch amount that could prevent the
      // columns from overflowing. Give up, unless we're nested inside another
      // fragmentation context, in which case we'll stretch the columns to take
      // up all the space inside the multicol container fragment. A box is
      // required to use all the remaining fragmentainer space when something
      // inside breaks; see https://www.w3.org/TR/css-break-3/#box-splitting
      if (!is_constrained_by_outer_fragmentation_context_)
        break;
      // We'll get properly constrained right below. Rely on that, rather than
      // calculating the exact amount here (we could check the available outer
      // fragmentainer size and subtract the row offset and stuff, but that's
      // duplicated logic). We'll use as much as we're allowed to.
      new_column_block_size = LayoutUnit::Max();
    } else {
      new_column_block_size = column_size.block_size;
      if (minimal_space_shortage > LayoutUnit())
        new_column_block_size += minimal_space_shortage;
    }
    new_column_block_size =
        ConstrainColumnBlockSize(new_column_block_size, row_offset);

    // Give up if we cannot get taller columns. The multicol container may have
    // a specified block-size preventing taller columns, for instance.
    DCHECK_GE(new_column_block_size, column_size.block_size);
    if (new_column_block_size <= column_size.block_size) {
      if (ConstraintSpace().IsInsideBalancedColumns()) {
        // If we're doing nested column balancing, propagate any space shortage
        // to the outer multicol container, so that the outer multicol container
        // can attempt to stretch, so that this inner one may fit as well.
        if (!container_builder_.IsInitialColumnBalancingPass())
          container_builder_.PropagateSpaceShortage(minimal_space_shortage);
      }
      break;
    }

    // Remove column fragments and re-attempt layout with taller columns.
    new_columns.clear();
    column_size.block_size = new_column_block_size;
  } while (true);

  // If we just have one empty fragmentainer, we need to keep the trailing
  // margin from any previous column spanner, and also make sure that we don't
  // incorrectly consider this to be a class A breakpoint. A fragmentainer may
  // end up empty if there's no in-flow content at all inside the multicol
  // container, if the multicol container starts with a spanner, or if the
  // only in-flow content is empty as a result of a nested OOF positioned
  // element whose containing block lives outside this multicol.
  //
  // If the size of the fragment is non-zero, we shouldn't consider it to be
  // empty (even if there's nothing inside). This happens with contenteditable,
  // which in some cases makes room for a line box that isn't there.
  bool is_empty =
      !column_size.block_size && new_columns.size() == 1 &&
      (new_columns[0].Fragment().Children().empty() || is_empty_spanner_parent);

  if (!is_empty) {
    has_processed_first_child_ = true;
    container_builder_.SetPreviousBreakAfter(EBreakBetween::kAuto);

    const auto& first_column =
        To<NGPhysicalBoxFragment>(new_columns[0].Fragment());

    // Only the first column in a row may attempt to place any unpositioned
    // list-item. This matches the behavior in Gecko, and also to some extent
    // with how baselines are propagated inside a multicol container.
    AttemptToPositionListMarker(first_column, row_offset);

    // We're adding a row with content. We can update the intrinsic block-size
    // (which will also be used as layout position for subsequent content), and
    // reset the margin strut (it has already been incorporated into the
    // offset).
    intrinsic_block_size_ = row_offset + intrinsic_block_size_contribution;
    *margin_strut = NGMarginStrut();
  }

  // Commit all column fragments to the fragment builder.
  for (auto result_with_offset : new_columns) {
    const NGPhysicalBoxFragment& column = result_with_offset.Fragment();
    container_builder_.AddChild(column, result_with_offset.offset);
    PropagateBaselineFromChild(column, result_with_offset.offset.block_offset);
  }

  if (min_break_appeal)
    container_builder_.ClampBreakAppeal(*min_break_appeal);

  return result;
}

NGBreakStatus NGColumnLayoutAlgorithm::LayoutSpanner(
    NGBlockNode spanner_node,
    const NGBlockBreakToken* break_token,
    NGMarginStrut* margin_strut) {
  spanner_path_ = nullptr;
  const ComputedStyle& spanner_style = spanner_node.Style();
  NGBoxStrut margins =
      ComputeMarginsFor(spanner_style, ChildAvailableSize().inline_size,
                        ConstraintSpace().GetWritingDirection());
  AdjustMarginsForFragmentation(break_token, &margins);

  // Collapse the block-start margin of this spanner with the block-end margin
  // of an immediately preceding spanner, if any.
  margin_strut->Append(margins.block_start, /* is_quirky */ false);

  LayoutUnit block_offset = intrinsic_block_size_ + margin_strut->Sum();
  auto spanner_space =
      CreateConstraintSpaceForSpanner(spanner_node, block_offset);

  const NGEarlyBreak* early_break_in_child = nullptr;
  if (UNLIKELY(early_break_))
    early_break_in_child = EnterEarlyBreakInChild(spanner_node, *early_break_);

  auto* result =
      spanner_node.Layout(spanner_space, break_token, early_break_in_child);

  if (ConstraintSpace().HasBlockFragmentation() && !early_break_) {
    // We're nested inside another fragmentation context. Examine this break
    // point, and determine whether we should break.

    LayoutUnit fragmentainer_block_offset =
        ConstraintSpace().FragmentainerOffset() + block_offset;

    NGBreakStatus break_status = BreakBeforeChildIfNeeded(
        ConstraintSpace(), spanner_node, *result, fragmentainer_block_offset,
        has_processed_first_child_, &container_builder_);

    if (break_status != NGBreakStatus::kContinue) {
      // We need to break, either before the spanner, or even earlier.
      return break_status;
    }
  }

  const auto& spanner_fragment =
      To<NGPhysicalBoxFragment>(result->PhysicalFragment());
  NGFragment logical_fragment(ConstraintSpace().GetWritingDirection(),
                              spanner_fragment);

  ResolveInlineAutoMargins(spanner_style, Style(),
                           ChildAvailableSize().inline_size,
                           logical_fragment.InlineSize(), &margins);

  LogicalOffset offset(
      BorderScrollbarPadding().inline_start + margins.inline_start,
      block_offset);
  container_builder_.AddResult(*result, offset);

  // According to the spec, the first spanner that has a baseline contributes
  // with its baseline to the multicol container. This is in contrast to column
  // content, where only the first column may contribute with a baseline.
  PropagateBaselineFromChild(spanner_fragment, offset.block_offset);

  AttemptToPositionListMarker(spanner_fragment, block_offset);

  *margin_strut = NGMarginStrut();
  margin_strut->Append(margins.block_end, /* is_quirky */ false);

  intrinsic_block_size_ = offset.block_offset + logical_fragment.BlockSize();
  has_processed_first_child_ = true;

  return NGBreakStatus::kContinue;
}

void NGColumnLayoutAlgorithm::AttemptToPositionListMarker(
    const NGPhysicalBoxFragment& child_fragment,
    LayoutUnit block_offset) {
  const auto marker = container_builder_.UnpositionedListMarker();
  if (!marker)
    return;
  DCHECK(Node().IsListItem());

  FontBaseline baseline_type = Style().GetFontBaseline();
  auto baseline = marker.ContentAlignmentBaseline(
      ConstraintSpace(), baseline_type, child_fragment);
  if (!baseline)
    return;

  const NGLayoutResult* layout_result = marker.Layout(
      ConstraintSpace(), container_builder_.Style(), baseline_type);
  DCHECK(layout_result);

  // TODO(layout-dev): AddToBox() may increase the specified block-offset, which
  // is bad, since it means that we may need to refragment. For now we'll just
  // ignore the adjustment (which is also bad, of course).
  marker.AddToBox(ConstraintSpace(), baseline_type, child_fragment,
                  BorderScrollbarPadding(), *layout_result, *baseline,
                  &block_offset, &container_builder_);

  container_builder_.ClearUnpositionedListMarker();
}

void NGColumnLayoutAlgorithm::PositionAnyUnclaimedListMarker() {
  if (!Node().IsListItem())
    return;
  const auto marker = container_builder_.UnpositionedListMarker();
  if (!marker)
    return;

  // Lay out the list marker.
  FontBaseline baseline_type = Style().GetFontBaseline();
  const NGLayoutResult* layout_result =
      marker.Layout(ConstraintSpace(), Style(), baseline_type);
  DCHECK(layout_result);
  // Position the list marker without aligning with line boxes.
  marker.AddToBoxWithoutLineBoxes(ConstraintSpace(), baseline_type,
                                  *layout_result, &container_builder_,
                                  &intrinsic_block_size_);
  container_builder_.ClearUnpositionedListMarker();
}

void NGColumnLayoutAlgorithm::PropagateBaselineFromChild(
    const NGPhysicalBoxFragment& child,
    LayoutUnit block_offset) {
  NGBoxFragment fragment(ConstraintSpace().GetWritingDirection(), child);

  // The first-baseline is the highest first-baseline of all fragments.
  if (auto first_baseline = fragment.FirstBaseline()) {
    LayoutUnit baseline = std::min(
        block_offset + *first_baseline,
        container_builder_.FirstBaseline().value_or(LayoutUnit::Max()));
    container_builder_.SetFirstBaseline(baseline);
  }

  // The last-baseline is the lowest last-baseline of all fragments.
  if (auto last_baseline = fragment.LastBaseline()) {
    LayoutUnit baseline =
        std::max(block_offset + *last_baseline,
                 container_builder_.LastBaseline().value_or(LayoutUnit::Min()));
    container_builder_.SetLastBaseline(baseline);
  }
  container_builder_.SetUseLastBaselineForInlineBaseline();
}

LayoutUnit NGColumnLayoutAlgorithm::ResolveColumnAutoBlockSize(
    const LogicalSize& column_size,
    LayoutUnit row_offset,
    const NGBlockBreakToken* child_break_token,
    bool balance_columns) {
  spanner_path_ = nullptr;
  return ResolveColumnAutoBlockSizeInternal(column_size, row_offset,
                                            child_break_token, balance_columns);
}

LayoutUnit NGColumnLayoutAlgorithm::ResolveColumnAutoBlockSizeInternal(
    const LogicalSize& column_size,
    LayoutUnit row_offset,
    const NGBlockBreakToken* child_break_token,
    bool balance_columns) {
  // To calculate a balanced column size for one row of columns, we need to
  // figure out how tall our content is. To do that we need to lay out. Create a
  // special constraint space for column balancing, without allowing soft
  // breaks. It will make us lay out all the multicol content as one single tall
  // strip (unless there are forced breaks). When we're done with this layout
  // pass, we can examine the result and calculate an ideal column block-size.
  NGConstraintSpace space = CreateConstraintSpaceForBalancing(column_size);
  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, Node(), BreakToken());

  // A run of content without explicit (forced) breaks; i.e. the content portion
  // between two explicit breaks, between fragmentation context start and an
  // explicit break, between an explicit break and fragmentation context end,
  // or, in cases when there are no explicit breaks at all: between
  // fragmentation context start and end. We need to know where the explicit
  // breaks are, in order to figure out where the implicit breaks will end up,
  // so that we get the columns properly balanced. A content run starts out as
  // representing one single column, and we'll add as many additional implicit
  // breaks as needed into the content runs that are the tallest ones
  // (ColumnBlockSize()).
  struct ContentRun {
    ContentRun(LayoutUnit content_block_size)
        : content_block_size(content_block_size) {}

    // Return the column block-size that this content run would require,
    // considering the implicit breaks we have assumed so far.
    LayoutUnit ColumnBlockSize() const {
      // Some extra care is required for the division here. We want the
      // resulting LayoutUnit value to be large enough to prevent overflowing
      // columns. Use floating point to get higher precision than
      // LayoutUnit. Then convert it to a LayoutUnit, but round it up to the
      // nearest value that LayoutUnit is able to represent.
      return LayoutUnit::FromFloatCeil(
          float(content_block_size) / float(implicit_breaks_assumed_count + 1));
    }

    LayoutUnit content_block_size;

    // The number of implicit breaks assumed to exist in this content run.
    int implicit_breaks_assumed_count = 0;
  };

  class ContentRuns final {
   public:
    // When we have "inserted" (assumed) enough implicit column breaks, this
    // method returns the block-size of the tallest column.
    LayoutUnit TallestColumnBlockSize() const {
      return TallestRun()->ColumnBlockSize();
    }

    LayoutUnit TallestContentBlockSize() const {
      return tallest_content_block_size_;
    }

    void AddRun(LayoutUnit content_block_size) {
      runs_.emplace_back(content_block_size);
      tallest_content_block_size_ =
          std::max(tallest_content_block_size_, content_block_size);
    }

    void DistributeImplicitBreaks(int used_column_count) {
      for (int columns_found = runs_.size(); columns_found < used_column_count;
           ++columns_found) {
        // The tallest content run (with all assumed implicit breaks added so
        // far taken into account) is where we assume the next implicit break.
        ++TallestRun()->implicit_breaks_assumed_count;
      }
    }

   private:
    ContentRun* TallestRun() const {
      DCHECK(!runs_.empty());
      auto* const it = std::max_element(
          runs_.begin(), runs_.end(),
          [](const ContentRun& run1, const ContentRun& run2) {
            return run1.ColumnBlockSize() < run2.ColumnBlockSize();
          });
      DCHECK(it != runs_.end());
      return const_cast<ContentRun*>(it);
    }

    Vector<ContentRun, 1> runs_;
    LayoutUnit tallest_content_block_size_;
  };

  // First split into content runs at explicit (forced) breaks.
  ContentRuns content_runs;
  const NGBlockBreakToken* break_token = child_break_token;
  tallest_unbreakable_block_size_ = LayoutUnit();
  int forced_break_count = 0;
  do {
    NGLayoutAlgorithmParams params(Node(), fragment_geometry, space,
                                   break_token);
    params.column_spanner_path = spanner_path_;
    NGBlockLayoutAlgorithm balancing_algorithm(params);
    balancing_algorithm.SetBoxType(NGPhysicalFragment::kColumnBox);
    const NGLayoutResult* result = balancing_algorithm.Layout();

    // This algorithm should never abort.
    DCHECK_EQ(result->Status(), NGLayoutResult::kSuccess);

    const NGPhysicalBoxFragment& fragment =
        To<NGPhysicalBoxFragment>(result->PhysicalFragment());

    // Add a content run, as long as we have soft break opportunities. Ignore
    // content that's doomed to end up in overflowing columns (because of too
    // many forced breaks).
    if (forced_break_count < used_column_count_) {
      LayoutUnit column_block_size = BlockSizeForFragmentation(
          *result, ConstraintSpace().GetWritingDirection());

      // Encompass the block-size of the (single-strip column) fragment, to
      // account for any trailing margins. We let them affect the column
      // block-size, for compatibility reasons, if nothing else. The initial
      // column balancing pass (i.e. here) is our opportunity to do that fairly
      // easily. But note that this doesn't guarantee that no margins will ever
      // get truncated. To avoid that we'd need to add some sort of mechanism
      // that is invoked in *every* column balancing layout pass, where we'd
      // essentially have to treat every margin as unbreakable (which kind of
      // sounds both bad and difficult).
      //
      // We might want to revisit this approach, if it's worth it: Maybe it's
      // better to not make any room at all for margins that might end up
      // getting truncated. After all, they don't really require any space, so
      // what we're doing currently might be seen as unnecessary (and slightly
      // unpredictable) column over-stretching.
      NGFragment logical_fragment(ConstraintSpace().GetWritingDirection(),
                                  fragment);
      column_block_size =
          std::max(column_block_size, logical_fragment.BlockSize());
      content_runs.AddRun(column_block_size);
    }

    tallest_unbreakable_block_size_ = std::max(
        tallest_unbreakable_block_size_, result->TallestUnbreakableBlockSize());

    // Stop when we reach a spanner. That's where this row of columns will end.
    // When laying out a row of columns, we'll pass in the spanner path, so that
    // the block layout algorithms can tell whether a node contains the spanner.
    if (const NGColumnSpannerPath* spanner_path = result->ColumnSpannerPath()) {
      bool knew_about_spanner = !!spanner_path_;
      spanner_path_ = spanner_path;
      if (forced_break_count && !knew_about_spanner) {
        // We may incorrectly have entered parallel flows, because we didn't
        // know about the spanner. Try again.
        return ResolveColumnAutoBlockSizeInternal(
            column_size, row_offset, child_break_token, balance_columns);
      }
      break;
    }

    if (result->HasForcedBreak())
      forced_break_count++;

    break_token = fragment.BreakToken();
  } while (break_token);

  if (ConstraintSpace().IsInitialColumnBalancingPass()) {
    // Nested column balancing. Our outer fragmentation context is in its
    // initial balancing pass, so it also wants to know the largest unbreakable
    // block-size.
    container_builder_.PropagateTallestUnbreakableBlockSize(
        tallest_unbreakable_block_size_);
  }

  // We now have an estimated minimal block-size for the columns. Roughly
  // speaking, this is the block-size that the columns will need if we are
  // allowed to break freely at any offset. This is normally not the case,
  // though, since there will typically be unbreakable pieces of content, such
  // as replaced content, lines of text, and other things. We need to actually
  // lay out into columns to figure out if they are tall enough or not (and
  // stretch and retry if not). Also honor {,min-,max-}block-size properties
  // before returning, and also try to not become shorter than the tallest piece
  // of unbreakable content.
  if (tallest_unbreakable_block_size_ >=
      content_runs.TallestContentBlockSize()) {
    return ConstrainColumnBlockSize(tallest_unbreakable_block_size_,
                                    row_offset);
  }

  if (balance_columns) {
    // We should create as many columns as specified by column-count.
    content_runs.DistributeImplicitBreaks(used_column_count_);
  }
  return ConstrainColumnBlockSize(content_runs.TallestColumnBlockSize(),
                                  row_offset);
}

// Constrain a balanced column block size to not overflow the multicol
// container.
LayoutUnit NGColumnLayoutAlgorithm::ConstrainColumnBlockSize(
    LayoutUnit size,
    LayoutUnit row_offset) const {
  // Avoid becoming shorter than the tallest piece of unbreakable content.
  size = std::max(size, tallest_unbreakable_block_size_);

  if (is_constrained_by_outer_fragmentation_context_) {
    // Don't become too tall to fit in the outer fragmentation context.
    LayoutUnit available_outer_space =
        UnclampedFragmentainerSpaceLeft(ConstraintSpace()) - row_offset;
    size = std::min(size, available_outer_space.ClampNegativeToZero());
  }

  // Table-cell sizing is special. The aspects of specified block-size (and its
  // min/max variants) that are actually honored by table cells is taken care of
  // in the table layout algorithm. A constraint space with fixed block-size
  // will be passed from the table layout algorithm if necessary. Leave it
  // alone.
  if (ConstraintSpace().IsTableCell())
    return size;

  // The {,min-,max-}block-size properties are specified on the multicol
  // container, but here we're calculating the column block sizes inside the
  // multicol container, which isn't exactly the same. We may shrink the column
  // block size here, but we'll never stretch them, because the value passed is
  // the perfect balanced block size. Making it taller would only disrupt the
  // balanced output, for no reason. The only thing we need to worry about here
  // is to not overflow the multicol container.
  //
  // First of all we need to convert the size to a value that can be compared
  // against the resolved properties on the multicol container. That means that
  // we have to convert the value from content-box to border-box.
  LayoutUnit extra = BorderScrollbarPadding().BlockSum();
  size += extra;

  const ComputedStyle& style = Style();
  LayoutUnit max = ResolveMaxBlockLength(
      ConstraintSpace(), style, BorderPadding(), style.LogicalMaxHeight());
  LayoutUnit extent = kIndefiniteSize;
  if (!style.LogicalHeight().IsAuto()) {
    extent = ResolveMainBlockLength(ConstraintSpace(), style, BorderPadding(),
                                    style.LogicalHeight(), kIndefiniteSize);
    // A specified block-size will just constrain the maximum length.
    if (extent != kIndefiniteSize)
      max = std::min(max, extent);
  }

  // A specified min-block-size may increase the maximum length.
  LayoutUnit min = ResolveMinBlockLength(
      ConstraintSpace(), style, BorderPadding(), style.LogicalMinHeight());
  max = std::max(max, min);

  if (max != LayoutUnit::Max()) {
    // If this multicol container is nested inside another fragmentation
    // context, we need to subtract the space consumed in previous fragments.
    if (BreakToken())
      max -= BreakToken()->ConsumedBlockSize();

    // We may already have used some of the available space in earlier column
    // rows or spanners.
    max -= CurrentContentBlockOffset(row_offset);
  }

  // Constrain and convert the value back to content-box.
  size = std::min(size, max);
  return (size - extra).ClampNegativeToZero();
}

NGConstraintSpace NGColumnLayoutAlgorithm::CreateConstraintSpaceForBalancing(
    const LogicalSize& column_size) const {
  NGConstraintSpaceBuilder space_builder(
      ConstraintSpace(), Style().GetWritingDirection(), /* is_new_fc */ true);
  space_builder.SetFragmentationType(kFragmentColumn);
  space_builder.SetShouldPropagateChildBreakValues();
  space_builder.SetAvailableSize({column_size.inline_size, kIndefiniteSize});
  space_builder.SetInlineAutoBehavior(NGAutoBehavior::kStretchImplicit);
  space_builder.SetPercentageResolutionSize(ColumnPercentageResolutionSize());
  space_builder.SetIsAnonymous(true);
  space_builder.SetIsInColumnBfc();
  space_builder.SetIsInsideBalancedColumns();

  return space_builder.ToConstraintSpace();
}

NGConstraintSpace NGColumnLayoutAlgorithm::CreateConstraintSpaceForSpanner(
    const NGBlockNode& spanner,
    LayoutUnit block_offset) const {
  auto child_writing_direction = spanner.Style().GetWritingDirection();
  NGConstraintSpaceBuilder space_builder(
      ConstraintSpace(), child_writing_direction, /* is_new_fc */ true);
  if (!IsParallelWritingMode(ConstraintSpace().GetWritingMode(),
                             child_writing_direction.GetWritingMode()))
    SetOrthogonalFallbackInlineSizeIfNeeded(Style(), spanner, &space_builder);
  else if (ShouldBlockContainerChildStretchAutoInlineSize(spanner))
    space_builder.SetInlineAutoBehavior(NGAutoBehavior::kStretchImplicit);
  space_builder.SetAvailableSize(ChildAvailableSize());
  space_builder.SetPercentageResolutionSize(ChildAvailableSize());

  space_builder.SetBaselineAlgorithmType(
      ConstraintSpace().BaselineAlgorithmType());

  if (ConstraintSpace().HasBlockFragmentation()) {
    SetupSpaceBuilderForFragmentation(
        ConstraintSpace(), spanner, block_offset, &space_builder,
        /* is_new_fc */ true,
        container_builder_.RequiresContentBeforeBreaking());
  }

  return space_builder.ToConstraintSpace();
}

NGConstraintSpace NGColumnLayoutAlgorithm::CreateConstraintSpaceForMinMax()
    const {
  NGConstraintSpaceBuilder space_builder(
      ConstraintSpace(), Style().GetWritingDirection(), /* is_new_fc */ true);
  space_builder.SetIsAnonymous(true);
  space_builder.SetIsInColumnBfc();

  return space_builder.ToConstraintSpace();
}

LayoutUnit NGColumnLayoutAlgorithm::TotalColumnBlockSize() const {
  LayoutUnit total_block_size;
  WritingMode writing_mode = Style().GetWritingMode();
  for (auto& child : container_builder_.Children()) {
    if (child.fragment->IsFragmentainerBox()) {
      LayoutUnit fragmentainer_block_size =
          child.fragment->Size().ConvertToLogical(writing_mode).block_size;
      total_block_size +=
          ClampedToValidFragmentainerCapacity(fragmentainer_block_size);
    }
  }
  return total_block_size;
}

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::ResultWithOffset)
