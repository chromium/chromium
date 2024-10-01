// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/column_layout_algorithm.h"

#include <algorithm>

#include "base/not_fatal_until.h"
#include "third_party/blink/renderer/core/layout/block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/block_layout_algorithm_utils.h"
#include "third_party/blink/renderer/core/layout/column_spanner_path.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/geometry/fragment_geometry.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/margin_strut.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/list/unpositioned_list_marker.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/simplified_oof_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/table/table_layout_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

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
    Entry(const BlockBreakToken* token, BlockNode spanner)
        : break_token(token), spanner(spanner) {}

    // The incoming break token for the content to process, or null if we're at
    // the start.
    const BlockBreakToken* break_token = nullptr;

    // The column spanner node to process, or null if we're dealing with regular
    // column content.
    BlockNode spanner = nullptr;
  };

  MulticolPartWalker(BlockNode multicol_container,
                     const BlockBreakToken* break_token)
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
  void MoveToSpanner(BlockNode spanner,
                     const BlockBreakToken* next_column_token);

  // Push a break token for the column content to resume at.
  void AddNextColumnBreakToken(const BlockBreakToken& next_column_token);

  // If a column was added for an OOF before a spanner, we need to update the
  // column break token so that the content is resumed at the correct spot.
  void UpdateNextColumnBreakToken(
      const FragmentBuilder::ChildrenVector& children);

 private:
  void MoveToNext();
  void UpdateCurrent();

  Entry current_;
  BlockNode spanner_ = nullptr;
  BlockNode multicol_container_;
  const BlockBreakToken* parent_break_token_;
  const BlockBreakToken* next_column_token_ = nullptr;

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
    BlockNode spanner,
    const BlockBreakToken* next_column_token) {
  *this = MulticolPartWalker(multicol_container_, nullptr);
  DCHECK(spanner.IsColumnSpanAll());
  spanner_ = spanner;
  next_column_token_ = next_column_token;
  UpdateCurrent();
}

void MulticolPartWalker::AddNextColumnBreakToken(
    const BlockBreakToken& next_column_token) {
  *this = MulticolPartWalker(multicol_container_, nullptr);
  next_column_token_ = &next_column_token;
  UpdateCurrent();
}

void MulticolPartWalker::UpdateNextColumnBreakToken(
    const FragmentBuilder::ChildrenVector& children) {
  if (children.empty())
    return;
  const blink::PhysicalFragment* last_child =
      children[children.size() - 1].fragment;
  if (!last_child->IsColumnBox())
    return;
  const auto* child_break_token =
      To<BlockBreakToken>(last_child->GetBreakToken());
  if (child_break_token && child_break_token != next_column_token_)
    next_column_token_ = child_break_token;
}

void MulticolPartWalker::UpdateCurrent() {
  DCHECK(!is_finished_);
  if (parent_break_token_) {
    const auto& child_break_tokens = parent_break_token_->ChildBreakTokens();
    if (child_token_idx_ < child_break_tokens.size()) {
      const auto* child_break_token =
          To<BlockBreakToken>(child_break_tokens[child_token_idx_].Get());
      if (child_break_token->InputNode() == multicol_container_) {
        current_.spanner = nullptr;
      } else {
        current_.spanner = To<BlockNode>(child_break_token->InputNode());
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
    LayoutInputNode next = spanner_.NextSibling();
    // Otherwise, if there's a next spanner, we'll use that.
    if (next && next.IsColumnSpanAll()) {
      spanner_ = To<BlockNode>(next);
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

BlockNode GetSpannerFromPath(const ColumnSpannerPath* path) {
  while (path->Child())
    path = path->Child();
  DCHECK(path->GetBlockNode().IsColumnSpanAll());
  return path->GetBlockNode();
}

}  // namespace

ColumnLayoutAlgorithm::ColumnLayoutAlgorithm(
    const LayoutAlgorithmParams& params)
    : LayoutAlgorithm(params) {
  // When a list item has multicol, |ColumnLayoutAlgorithm| needs to keep
  // track of the list marker instead of the child layout algorithm. See
  // |BlockLayoutAlgorithm|.
  if (const BlockNode marker_node = Node().ListMarkerBlockNodeIfListItem()) {
    if (!marker_node.ListMarkerOccupiesWholeLine() &&
        (!GetBreakToken() || GetBreakToken()->HasUnpositionedListMarker())) {
      container_builder_.SetUnpositionedListMarker(
          UnpositionedListMarker(marker_node));
    }
  }
}

const LayoutResult* ColumnLayoutAlgorithm::Layout() {
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
  if (!IsBreakInside(GetBreakToken())) {
    node_.StoreColumnSizeAndCount(column_inline_size_, used_column_count_);

    StyleEngine& style_engine = Node().GetDocument().GetStyleEngine();
    style_engine.SetInScrollMarkersAttachment(true);
    To<Element>(Node().EnclosingDOMNode())->ClearColumnPseudoElements();
    style_engine.SetInScrollMarkersAttachment(false);
  }

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
      GetConstraintSpace().HasKnownFragmentainerBlockSize();

  container_builder_.SetIsBlockFragmentationContextRoot();

  intrinsic_block_size_ = BorderScrollbarPadding().block_start;

  BreakStatus break_status = LayoutChildren();
  if (break_status == BreakStatus::kNeedsEarlierBreak) {
    // We need to discard this layout and do it again. We found an earlier break
    // point that's more appealing than the one we ran out of space at.
    return RelayoutAndBreakEarlier<ColumnLayoutAlgorithm>(
        container_builder_.GetEarlyBreak());
  }
  DCHECK_EQ(break_status, BreakStatus::kContinue);

  intrinsic_block_size_ =
      std::max(intrinsic_block_size_, BorderScrollbarPadding().block_start);
  intrinsic_block_size_ += BorderScrollbarPadding().block_end;

  // Figure out how much space we've already been able to process in previous
  // fragments, if this multicol container participates in an outer
  // fragmentation context.
  LayoutUnit previously_consumed_block_size;
  if (const auto* token = GetBreakToken()) {
    previously_consumed_block_size = token->ConsumedBlockSize();
  }

  const LayoutUnit unconstrained_intrinsic_block_size = intrinsic_block_size_;
  intrinsic_block_size_ =
      ClampIntrinsicBlockSize(GetConstraintSpace(), Node(), GetBreakToken(),
                              BorderScrollbarPadding(), intrinsic_block_size_);

  LayoutUnit block_size = ComputeBlockSizeForFragment(
      GetConstraintSpace(), Node(), BorderPadding(),
      previously_consumed_block_size + intrinsic_block_size_,
      border_box_size.inline_size);

  container_builder_.SetFragmentsTotalBlockSize(block_size);
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size_);
  container_builder_.SetBlockOffsetForAdditionalColumns(
      CurrentContentBlockOffset(intrinsic_block_size_));

  PositionAnyUnclaimedListMarker();

  if (InvolvedInBlockFragmentation(container_builder_)) [[unlikely]] {
    // In addition to establishing one, we're nested inside another
    // fragmentation context.
    FinishFragmentation(&container_builder_);

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

  if (GetConstraintSpace().IsTableCell()) {
    FinalizeTableCellLayout(unconstrained_intrinsic_block_size,
                            &container_builder_);
  } else {
    AlignBlockContent(Style(), GetBreakToken(),
                      unconstrained_intrinsic_block_size, container_builder_);
  }

  container_builder_.HandleOofsAndSpecialDescendants();

  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult ColumnLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  const LayoutUnit override_intrinsic_inline_size =
      Node().OverrideIntrinsicContentInlineSize();
  if (override_intrinsic_inline_size != kIndefiniteSize) {
    const LayoutUnit size =
        BorderScrollbarPadding().InlineSum() + override_intrinsic_inline_size;
    return {{size, size}, /* depends_on_block_constraints */ false};
  }

  // First calculate the min/max sizes of columns.
  ConstraintSpace space = CreateConstraintSpaceForMinMax();
  FragmentGeometry fragment_geometry = CalculateInitialFragmentGeometry(
      space, Node(), /* break_token */ nullptr, /* is_intrinsic */ true);
  BlockLayoutAlgorithm algorithm({Node(), fragment_geometry, space});
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

const PhysicalBoxFragment& ColumnLayoutAlgorithm::CreateEmptyColumn(
    const BlockNode& node,
    const ConstraintSpace& parent_space,
    const PhysicalBoxFragment& previous_column) {
  WritingMode writing_mode = parent_space.GetWritingMode();
  DCHECK(previous_column.IsColumnBox());
  const BlockBreakToken* break_token = previous_column.GetBreakToken();
  LogicalSize column_size =
      previous_column.Size().ConvertToLogical(writing_mode);
  ConstraintSpace child_space = CreateConstraintSpaceForFragmentainer(
      parent_space, kFragmentColumn, column_size,
      /*percentage_resolution_size=*/column_size, /*balance_columns=*/false,
      kBreakAppealLastResort);
  FragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(child_space, node, break_token);
  LayoutAlgorithmParams params(node, fragment_geometry, child_space,
                               break_token);
  SimplifiedOofLayoutAlgorithm child_algorithm(params, previous_column);
  child_algorithm.ResumeColumnLayout(break_token);
  return To<PhysicalBoxFragment>(
      child_algorithm.Layout()->GetPhysicalFragment());
}

MinMaxSizesResult ColumnLayoutAlgorithm::ComputeSpannersMinMaxSizes(
    const BlockNode& search_parent) const {
  MinMaxSizesResult result;
  for (LayoutInputNode child = search_parent.FirstChild(); child;
       child = child.NextSibling()) {
    const BlockNode* child_block = DynamicTo<BlockNode>(&child);
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
      MinMaxConstraintSpaceBuilder builder(GetConstraintSpace(), Style(),
                                           *child_block, /* is_new_fc */ true);
      builder.SetAvailableBlockSize(ChildAvailableSize().block_size);
      const ConstraintSpace child_space = builder.ToConstraintSpace();
      child_result = ComputeMinAndMaxContentContribution(Style(), *child_block,
                                                         child_space);
    }
    result.sizes.Encompass(child_result.sizes);
  }
  return result;
}

BreakStatus ColumnLayoutAlgorithm::LayoutChildren() {
  MarginStrut margin_strut;
  MulticolPartWalker walker(Node(), GetBreakToken());
  while (!walker.IsFinished()) {
    auto entry = walker.Current();
    const auto* child_break_token = To<BlockBreakToken>(entry.break_token);

    // If this is regular column content (i.e. not a spanner), or we're at the
    // very start, perform column layout. If we're at the very start, and even
    // if the child is a spanner (which means that we won't be able to lay out
    // any column content at all), we still need to enter here, because that's
    // how we create a break token for the column content to resume at. With no
    // break token, we wouldn't be able to resume layout after the any initial
    // spanners.
    if (!entry.spanner) {
      const LayoutResult* result =
          LayoutRow(child_break_token, LayoutUnit(), &margin_strut);

      if (!result) {
        // An outer fragmentainer break was inserted before this row.
        DCHECK(GetConstraintSpace().HasBlockFragmentation());
        break;
      }

      walker.Next();

      const auto* next_column_token =
          To<BlockBreakToken>(result->GetPhysicalFragment().GetBreakToken());

      if (const auto* path = result->GetColumnSpannerPath()) {
        // We found a spanner, and if there's column content to resume at after
        // it, |next_column_token| will be set. Move the walker to the
        // spanner. We'll now walk that spanner and any sibling spanners, before
        // resuming at |next_column_token|.
        BlockNode spanner_node = GetSpannerFromPath(path);
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

    BlockNode spanner_node = entry.spanner;

    // If this is the child we had previously determined to break before, do so
    // now and finish layout.
    if (early_break_ &&
        IsEarlyBreakTarget(*early_break_, container_builder_, spanner_node))
      break;

    // Handle any OOF fragmentainer descendants that were found before the
    // spanner.
    OutOfFlowLayoutPart(&container_builder_).HandleFragmentation();
    walker.UpdateNextColumnBreakToken(container_builder_.Children());

    BreakStatus break_status =
        LayoutSpanner(spanner_node, child_break_token, &margin_strut);

    walker.Next();

    if (break_status == BreakStatus::kNeedsEarlierBreak) {
      return break_status;
    }
    if (break_status == BreakStatus::kBrokeBefore ||
        container_builder_.HasInflowChildBreakInside()) {
      break;
    }
  }

  if (!walker.IsFinished() || container_builder_.HasInflowChildBreakInside()) {
    // We broke in the main flow. Let this multicol container take up any
    // remaining space.
    intrinsic_block_size_ =
        std::max(intrinsic_block_size_, FragmentainerSpaceLeftForChildren());

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

  return BreakStatus::kContinue;
}

struct ResultWithOffset {
  DISALLOW_NEW();

 public:
  Member<const LayoutResult> result;
  LogicalOffset offset;

  ResultWithOffset(const LayoutResult* result, LogicalOffset offset)
      : result(result), offset(offset) {}

  const PhysicalBoxFragment& Fragment() const {
    return To<PhysicalBoxFragment>(result->GetPhysicalFragment());
  }

  void Trace(Visitor* visitor) const { visitor->Trace(result); }
};

const LayoutResult* ColumnLayoutAlgorithm::LayoutRow(
    const BlockBreakToken* next_column_token,
    LayoutUnit minimum_column_block_size,
    MarginStrut* margin_strut) {
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
    if (GetBreakToken() && is_constrained_by_outer_fragmentation_context_) {
      column_size.block_size -= GetBreakToken()->ConsumedBlockSize();
    }

    // Subtract the space already taken in the current fragment (spanners and
    // earlier column rows).
    column_size.block_size -= CurrentContentBlockOffset(row_offset);

    column_size.block_size = column_size.block_size.ClampNegativeToZero();
  }

  bool may_resume_in_next_outer_fragmentainer = false;
  LayoutUnit available_outer_space = kIndefiniteSize;
  if (is_constrained_by_outer_fragmentation_context_) {
    available_outer_space =
        std::max(minimum_column_block_size,
                 FragmentainerSpaceLeftForChildren() - row_offset);
    DCHECK_GE(available_outer_space, LayoutUnit());

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
  bool balance_columns =
      Style().GetColumnFill() == EColumnFill::kBalance ||
      (GetConstraintSpace().HasBlockFragmentation() &&
       !GetConstraintSpace().HasKnownFragmentainerBlockSize());

  // If columns are to be balanced, we need to examine the contents of the
  // multicol container to figure out a good initial (minimal) column
  // block-size. We also need to do this if column-fill is 'auto' and the
  // block-size is unconstrained.
  bool has_content_based_block_size =
      balance_columns || (column_size.block_size == kIndefiniteSize &&
                          !is_constrained_by_outer_fragmentation_context_);

  if (has_content_based_block_size) {
    column_size.block_size = ResolveColumnAutoBlockSize(
        column_size, row_offset, available_outer_space, next_column_token,
        balance_columns);
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
  if (may_resume_in_next_outer_fragmentainer &&
      !IsBreakInside(GetBreakToken())) {
    if (intrinsic_block_size_) {
      may_have_more_space_in_next_outer_fragmentainer = true;
    } else if (!GetConstraintSpace().IsAtFragmentainerStart()) {
      may_have_more_space_in_next_outer_fragmentainer = true;
    }
  }

  const LayoutResult* result = nullptr;
  std::optional<BreakAppeal> min_break_appeal;
  LayoutUnit intrinsic_block_size_contribution;

  do {
    const BlockBreakToken* column_break_token = next_column_token;
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

    min_break_appeal = std::nullopt;
    intrinsic_block_size_contribution = LayoutUnit();

    do {
      // Lay out one column. Each column will become a fragment.
      ConstraintSpace child_space = CreateConstraintSpaceForFragmentainer(
          GetConstraintSpace(), kFragmentColumn, column_size,
          ColumnPercentageResolutionSize(), balance_columns,
          min_break_appeal.value_or(kBreakAppealLastResort));

      FragmentGeometry fragment_geometry = CalculateInitialFragmentGeometry(
          child_space, Node(), GetBreakToken());

      LayoutAlgorithmParams params(Node(), fragment_geometry, child_space,
                                   column_break_token);
      params.column_spanner_path = spanner_path_;

      BlockLayoutAlgorithm child_algorithm(params);
      child_algorithm.SetBoxType(PhysicalFragment::kColumnBox);
      result = child_algorithm.Layout();
      const auto& column =
          To<PhysicalBoxFragment>(result->GetPhysicalFragment());
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
          FragmentedOofData::HasOutOfFlowPositionedFragmentainerDescendants(
              column)) {
        has_oof_fragmentainer_descendants = true;
      }

      // Add the new column fragment to the list, but don't commit anything to
      // the fragment builder until we know whether these are the final columns.
      LogicalOffset logical_offset(column_inline_offset, row_offset);
      new_columns.emplace_back(result, logical_offset);

      std::optional<LayoutUnit> space_shortage = result->MinimalSpaceShortage();
      UpdateMinimalSpaceShortage(space_shortage, &minimal_space_shortage);
      actual_column_count++;

      if (result->GetColumnSpannerPath()) {
        is_empty_spanner_parent = result->IsEmptySpannerParent();
        break;
      }

      has_violating_break |= result->GetBreakAppeal() != kBreakAppealPerfect;
      column_inline_offset += column_inline_progression_;

      if (result->HasForcedBreak())
        forced_break_count++;

      column_break_token = column.GetBreakToken();

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
                     result->GetBreakAppeal());

        LayoutUnit block_end_overflow =
            LogicalBoxFragment(GetConstraintSpace().GetWritingDirection(),
                               column)
                .BlockEndScrollableOverflow();
        if (row_offset + block_end_overflow >
            FragmentainerSpaceLeftForChildren()) {
          if (GetConstraintSpace().IsInsideBalancedColumns() &&
              !container_builder_.IsInitialColumnBalancingPass()) {
            container_builder_.PropagateSpaceShortage(minimal_space_shortage);
          }
          if (!minimum_column_block_size &&
              block_end_overflow > column_size.block_size) {
            // We're inside nested block fragmentation, and the column was
            // overflowed by content taller than what there is room for in the
            // outer fragmentainer. Try row layout again, but this time force
            // the columns to be this tall as well, to encompass overflow. It's
            // generally undesirable to overflow the outer fragmentainer, but
            // it's up to the parent algorithms to decide.
            DCHECK_GT(block_end_overflow, LayoutUnit());
            minimum_column_block_size = block_end_overflow;
            // TODO(mstensho): Consider refactoring this, rather than calling
            // ourselves recursively.
            return LayoutRow(next_column_token, minimum_column_block_size,
                             margin_strut);
          }
        }
      }
    } while (column_break_token);

    if (!balance_columns) {
      if (result->GetColumnSpannerPath()) {
        // We always have to balance columns preceding a spanner, so if we
        // didn't do that initially, switch over to column balancing mode now,
        // and lay out again.
        balance_columns = true;
        new_columns.clear();
        column_size.block_size = ResolveColumnAutoBlockSize(
            column_size, row_offset, available_outer_space, next_column_token,
            balance_columns);
        continue;
      }

      // Balancing not enabled. We're done.
      break;
    }

    // Any OOFs contained within this multicol get laid out once all columns
    // complete layout. However, OOFs should affect column balancing. Pass the
    // current set of columns into OutOfFlowLayoutPart to determine if OOF
    // layout will affect column balancing in any way (without actually adding
    // the OOF results to the builder - this will be handled at a later point).
    if (has_oof_fragmentainer_descendants) {
      // If, for example, the columns get split by a column spanner, the offset
      // of an OOF's containing block will be relative to the first
      // fragmentainer in the first row. However, we are only concerned about
      // the current row of columns, so we should adjust the containing block
      // offsets to be relative to the first column in the current row.
      LayoutUnit containing_block_adjustment = -TotalColumnBlockSize();

      OutOfFlowLayoutPart::ColumnBalancingInfo column_balancing_info;
      FragmentBuilder::ChildrenVector columns;
      for (wtf_size_t i = 0; i < new_columns.size(); i++) {
        auto& new_column = new_columns[i];
        columns.push_back(
            LogicalFragmentLink{&new_column.Fragment(), new_column.offset});

        // Because the current set of columns haven't been added to the builder
        // yet, any OOF descendants won't have been propagated up yet. Instead,
        // propagate any OOF descendants up to |column_balancing_info| so that
        // they can be passed into OutOfFlowLayoutPart (without affecting the
        // builder).
        container_builder_.PropagateOOFFragmentainerDescendants(
            new_column.Fragment(), new_column.offset,
            /* relative_offset */ LogicalOffset(), containing_block_adjustment,
            /* containing_block */ nullptr,
            /* fixedpos_containing_block */ nullptr,
            &column_balancing_info.out_of_flow_fragmentainer_descendants);
      }
      DCHECK(column_balancing_info.HasOutOfFlowFragmentainerDescendants());

      OutOfFlowLayoutPart oof_part(&container_builder_);
      oof_part.SetColumnBalancingInfo(&column_balancing_info, &columns);
      oof_part.HandleFragmentation();
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
        (!column_break_token || result->GetColumnSpannerPath())) {
      break;
    }

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
    new_column_block_size = ConstrainColumnBlockSize(
        new_column_block_size, row_offset, available_outer_space);

    // Give up if we cannot get taller columns. The multicol container may have
    // a specified block-size preventing taller columns, for instance.
    DCHECK_GE(new_column_block_size, column_size.block_size);
    if (new_column_block_size <= column_size.block_size) {
      if (GetConstraintSpace().IsInsideBalancedColumns()) {
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

  if (GetConstraintSpace().HasBlockFragmentation() &&
      row_offset > LayoutUnit()) {
    // If we have container separation, breaking before this row is fine.
    LayoutUnit fragmentainer_block_offset =
        FragmentainerOffsetForChildren() + row_offset;
    // TODO(layout-dev): Consider adjusting break appeal based on the preceding
    // column spanner (if any), e.g. if it has break-after:avoid, so that we can
    // support early-breaks.
    if (!MovePastBreakpoint(*result, fragmentainer_block_offset,
                            kBreakAppealPerfect)) {
      // This row didn't fit nicely in the outer fragmentation context. Breaking
      // before is better.
      if (!next_column_token) {
        // We haven't made any progress in the fragmentation context at all, but
        // when there's preceding initial multicol border/padding, we may want
        // to insert a last-resort break here.
        container_builder_.AddBreakBeforeChild(Node(), kBreakAppealLastResort,
                                               /* is_forced_break */ false);
      }
      return nullptr;
    }
  }

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
        To<PhysicalBoxFragment>(new_columns[0].Fragment());

    // Only the first column in a row may attempt to place any unpositioned
    // list-item. This matches the behavior in Gecko, and also to some extent
    // with how baselines are propagated inside a multicol container.
    AttemptToPositionListMarker(first_column, row_offset);

    // We're adding a row with content. We can update the intrinsic block-size
    // (which will also be used as layout position for subsequent content), and
    // reset the margin strut (it has already been incorporated into the
    // offset).
    intrinsic_block_size_ = row_offset + intrinsic_block_size_contribution;
    *margin_strut = MarginStrut();
  }

  Element* element = To<Element>(Node().EnclosingDOMNode());
  bool create_column_pseudo =
      element->CachedStyleForPseudoElement(kPseudoIdColumn);

  // Commit all column fragments to the fragment builder.
  for (auto result_with_offset : new_columns) {
    const PhysicalBoxFragment& column = result_with_offset.Fragment();
    container_builder_.AddChild(column, result_with_offset.offset);
    PropagateBaselineFromChild(column, result_with_offset.offset.block_offset);

    if (create_column_pseudo) {
      // Create a ::column pseudo element, and, if needed, also a
      // ::column::scroll-marker pseudo element child of ::column.
      LogicalRect column_logical_rect(result_with_offset.offset, column_size);
      const WritingModeConverter converter(
          GetConstraintSpace().GetWritingDirection(),
          LogicalSize(ChildAvailableSize().inline_size, column_block_size_));
      element->CreateColumnPseudoElement(
          converter.ToPhysical(column_logical_rect));
    }
  }

  if (min_break_appeal)
    container_builder_.ClampBreakAppeal(*min_break_appeal);

  return result;
}

BreakStatus ColumnLayoutAlgorithm::LayoutSpanner(
    BlockNode spanner_node,
    const BlockBreakToken* break_token,
    MarginStrut* margin_strut) {
  spanner_path_ = nullptr;
  const ComputedStyle& spanner_style = spanner_node.Style();
  BoxStrut margins =
      ComputeMarginsFor(spanner_style, ChildAvailableSize().inline_size,
                        GetConstraintSpace().GetWritingDirection());
  AdjustMarginsForFragmentation(break_token, &margins);

  // Collapse the block-start margin of this spanner with the block-end margin
  // of an immediately preceding spanner, if any.
  margin_strut->Append(margins.block_start, /* is_quirky */ false);

  LayoutUnit block_offset = intrinsic_block_size_ + margin_strut->Sum();
  auto spanner_space =
      CreateConstraintSpaceForSpanner(spanner_node, block_offset);

  const EarlyBreak* early_break_in_child = nullptr;
  if (early_break_) [[unlikely]] {
    early_break_in_child = EnterEarlyBreakInChild(spanner_node, *early_break_);
  }

  auto* result =
      spanner_node.Layout(spanner_space, break_token, early_break_in_child);

  if (GetConstraintSpace().HasBlockFragmentation() && !early_break_) {
    // We're nested inside another fragmentation context. Examine this break
    // point, and determine whether we should break.

    LayoutUnit fragmentainer_block_offset =
        FragmentainerOffsetForChildren() + block_offset;

    BreakStatus break_status = BreakBeforeChildIfNeeded(
        spanner_node, *result, fragmentainer_block_offset,
        has_processed_first_child_);

    if (break_status != BreakStatus::kContinue) {
      // We need to break, either before the spanner, or even earlier.
      return break_status;
    }
  }

  const auto& spanner_fragment =
      To<PhysicalBoxFragment>(result->GetPhysicalFragment());
  LogicalFragment logical_fragment(GetConstraintSpace().GetWritingDirection(),
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

  *margin_strut = MarginStrut();
  margin_strut->Append(margins.block_end, /* is_quirky */ false);

  intrinsic_block_size_ = offset.block_offset + logical_fragment.BlockSize();
  has_processed_first_child_ = true;

  return BreakStatus::kContinue;
}

void ColumnLayoutAlgorithm::AttemptToPositionListMarker(
    const PhysicalBoxFragment& child_fragment,
    LayoutUnit block_offset) {
  const auto marker = container_builder_.GetUnpositionedListMarker();
  if (!marker)
    return;
  DCHECK(Node().IsListItem());

  FontBaseline baseline_type = Style().GetFontBaseline();
  auto baseline = marker.ContentAlignmentBaseline(
      GetConstraintSpace(), baseline_type, child_fragment);
  if (!baseline)
    return;

  const LayoutResult* layout_result = marker.Layout(
      GetConstraintSpace(), container_builder_.Style(), baseline_type);
  DCHECK(layout_result);

  // TODO(layout-dev): AddToBox() may increase the specified block-offset, which
  // is bad, since it means that we may need to refragment. For now we'll just
  // ignore the adjustment (which is also bad, of course).
  marker.AddToBox(GetConstraintSpace(), baseline_type, child_fragment,
                  BorderScrollbarPadding(), *layout_result, *baseline,
                  &block_offset, &container_builder_);

  container_builder_.ClearUnpositionedListMarker();
}

void ColumnLayoutAlgorithm::PositionAnyUnclaimedListMarker() {
  if (!Node().IsListItem())
    return;
  const auto marker = container_builder_.GetUnpositionedListMarker();
  if (!marker)
    return;

  // Lay out the list marker.
  FontBaseline baseline_type = Style().GetFontBaseline();
  const LayoutResult* layout_result =
      marker.Layout(GetConstraintSpace(), Style(), baseline_type);
  DCHECK(layout_result);
  // Position the list marker without aligning with line boxes.
  marker.AddToBoxWithoutLineBoxes(GetConstraintSpace(), baseline_type,
                                  *layout_result, &container_builder_,
                                  &intrinsic_block_size_);
  container_builder_.ClearUnpositionedListMarker();
}

void ColumnLayoutAlgorithm::PropagateBaselineFromChild(
    const PhysicalBoxFragment& child,
    LayoutUnit block_offset) {
  LogicalBoxFragment fragment(GetConstraintSpace().GetWritingDirection(),
                              child);

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

LayoutUnit ColumnLayoutAlgorithm::ResolveColumnAutoBlockSize(
    const LogicalSize& column_size,
    LayoutUnit row_offset,
    LayoutUnit available_outer_space,
    const BlockBreakToken* child_break_token,
    bool balance_columns) {
  spanner_path_ = nullptr;
  return ResolveColumnAutoBlockSizeInternal(column_size, row_offset,
                                            available_outer_space,
                                            child_break_token, balance_columns);
}

LayoutUnit ColumnLayoutAlgorithm::ResolveColumnAutoBlockSizeInternal(
    const LogicalSize& column_size,
    LayoutUnit row_offset,
    LayoutUnit available_outer_space,
    const BlockBreakToken* child_break_token,
    bool balance_columns) {
  // To calculate a balanced column size for one row of columns, we need to
  // figure out how tall our content is. To do that we need to lay out. Create a
  // special constraint space for column balancing, without allowing soft
  // breaks. It will make us lay out all the multicol content as one single tall
  // strip (unless there are forced breaks). When we're done with this layout
  // pass, we can examine the result and calculate an ideal column block-size.
  ConstraintSpace space = CreateConstraintSpaceForBalancing(column_size);
  FragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, Node(), GetBreakToken());

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
      auto const it = std::max_element(
          runs_.begin(), runs_.end(),
          [](const ContentRun& run1, const ContentRun& run2) {
            return run1.ColumnBlockSize() < run2.ColumnBlockSize();
          });
      CHECK(it != runs_.end(), base::NotFatalUntil::M130);
      return const_cast<ContentRun*>(&*it);
    }

    Vector<ContentRun, 1> runs_;
    LayoutUnit tallest_content_block_size_;
  };

  // First split into content runs at explicit (forced) breaks.
  ContentRuns content_runs;
  const BlockBreakToken* break_token = child_break_token;
  tallest_unbreakable_block_size_ = LayoutUnit();
  int forced_break_count = 0;
  do {
    LayoutAlgorithmParams params(Node(), fragment_geometry, space, break_token);
    params.column_spanner_path = spanner_path_;
    BlockLayoutAlgorithm balancing_algorithm(params);
    balancing_algorithm.SetBoxType(PhysicalFragment::kColumnBox);
    const LayoutResult* result = balancing_algorithm.Layout();

    // This algorithm should never abort.
    DCHECK_EQ(result->Status(), LayoutResult::kSuccess);

    const auto& fragment =
        To<PhysicalBoxFragment>(result->GetPhysicalFragment());

    // Add a content run, as long as we have soft break opportunities. Ignore
    // content that's doomed to end up in overflowing columns (because of too
    // many forced breaks).
    if (forced_break_count < used_column_count_) {
      LayoutUnit column_block_size = BlockSizeForFragmentation(
          *result, GetConstraintSpace().GetWritingDirection());

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
      LogicalFragment logical_fragment(
          GetConstraintSpace().GetWritingDirection(), fragment);
      column_block_size =
          std::max(column_block_size, logical_fragment.BlockSize());
      content_runs.AddRun(column_block_size);
    }

    tallest_unbreakable_block_size_ = std::max(
        tallest_unbreakable_block_size_, result->TallestUnbreakableBlockSize());

    // Stop when we reach a spanner. That's where this row of columns will end.
    // When laying out a row of columns, we'll pass in the spanner path, so that
    // the block layout algorithms can tell whether a node contains the spanner.
    if (const auto* spanner_path = result->GetColumnSpannerPath()) {
      bool knew_about_spanner = !!spanner_path_;
      spanner_path_ = spanner_path;
      if (forced_break_count && !knew_about_spanner) {
        // We may incorrectly have entered parallel flows, because we didn't
        // know about the spanner. Try again.
        return ResolveColumnAutoBlockSizeInternal(
            column_size, row_offset, available_outer_space, child_break_token,
            balance_columns);
      }
      break;
    }

    if (result->HasForcedBreak())
      forced_break_count++;

    break_token = fragment.GetBreakToken();
  } while (break_token);

  if (GetConstraintSpace().IsInitialColumnBalancingPass()) {
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
    return ConstrainColumnBlockSize(tallest_unbreakable_block_size_, row_offset,
                                    available_outer_space);
  }

  if (balance_columns) {
    // We should create as many columns as specified by column-count.
    content_runs.DistributeImplicitBreaks(used_column_count_);
  }
  return ConstrainColumnBlockSize(content_runs.TallestColumnBlockSize(),
                                  row_offset, available_outer_space);
}

// Constrain a balanced column block size to not overflow the multicol
// container.
LayoutUnit ColumnLayoutAlgorithm::ConstrainColumnBlockSize(
    LayoutUnit size,
    LayoutUnit row_offset,
    LayoutUnit available_outer_space) const {
  // Avoid becoming shorter than the tallest piece of unbreakable content.
  size = std::max(size, tallest_unbreakable_block_size_);

  if (is_constrained_by_outer_fragmentation_context_) {
    // Don't become too tall to fit in the outer fragmentation context.
    size = std::min(size, available_outer_space.ClampNegativeToZero());
  }

  const ConstraintSpace& space = GetConstraintSpace();
  const ComputedStyle& style = Style();

  // Table-cell sizing is special. The aspects of specified block-size (and its
  // min/max variants) that are actually honored by table cells is taken care of
  // in the table layout algorithm. A constraint space with fixed block-size
  // will be passed from the table layout algorithm if necessary. Leave it
  // alone.
  if (space.IsTableCell()) {
    return size;
  }

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

  LayoutUnit max = ResolveInitialMaxBlockLength(space, style, BorderPadding(),
                                                style.LogicalMaxHeight());
  LayoutUnit extent = kIndefiniteSize;

  const Length& block_length = style.LogicalHeight();
  const Length& auto_length = space.IsBlockAutoBehaviorStretch()
                                  ? Length::Stretch()
                                  : Length::FitContent();

  extent = ResolveMainBlockLength(space, style, BorderPadding(), block_length,
                                  &auto_length, kIndefiniteSize);
  // A specified block-size will just constrain the maximum length.
  if (extent != kIndefiniteSize) {
    max = std::min(max, extent);
  }

  // A specified min-block-size may increase the maximum length.
  LayoutUnit min = ResolveInitialMinBlockLength(space, style, BorderPadding(),
                                                style.LogicalMinHeight());
  max = std::max(max, min);

  if (max != LayoutUnit::Max()) {
    // If this multicol container is nested inside another fragmentation
    // context, we need to subtract the space consumed in previous fragments.
    if (GetBreakToken()) {
      max -= GetBreakToken()->ConsumedBlockSize();
    }

    // We may already have used some of the available space in earlier column
    // rows or spanners.
    max -= CurrentContentBlockOffset(row_offset);
  }

  // Constrain and convert the value back to content-box.
  size = std::min(size, max);
  return (size - extra).ClampNegativeToZero();
}

ConstraintSpace ColumnLayoutAlgorithm::CreateConstraintSpaceForBalancing(
    const LogicalSize& column_size) const {
  ConstraintSpaceBuilder space_builder(GetConstraintSpace(),
                                       Style().GetWritingDirection(),
                                       /* is_new_fc */ true);
  space_builder.SetFragmentationType(kFragmentColumn);
  space_builder.SetShouldPropagateChildBreakValues();
  space_builder.SetAvailableSize({column_size.inline_size, kIndefiniteSize});
  space_builder.SetInlineAutoBehavior(AutoSizeBehavior::kStretchImplicit);
  space_builder.SetPercentageResolutionSize(ColumnPercentageResolutionSize());
  space_builder.SetIsAnonymous(true);
  space_builder.SetIsInColumnBfc();
  space_builder.SetIsInsideBalancedColumns();

  return space_builder.ToConstraintSpace();
}

ConstraintSpace ColumnLayoutAlgorithm::CreateConstraintSpaceForSpanner(
    const BlockNode& spanner,
    LayoutUnit block_offset) const {
  auto child_writing_direction = spanner.Style().GetWritingDirection();
  ConstraintSpaceBuilder space_builder(
      GetConstraintSpace(), child_writing_direction, /* is_new_fc */ true);
  if (!IsParallelWritingMode(GetConstraintSpace().GetWritingMode(),
                             child_writing_direction.GetWritingMode())) {
    SetOrthogonalFallbackInlineSizeIfNeeded(Style(), spanner, &space_builder);
  } else if (ShouldBlockContainerChildStretchAutoInlineSize(spanner)) {
    space_builder.SetInlineAutoBehavior(AutoSizeBehavior::kStretchImplicit);
  }
  space_builder.SetAvailableSize(ChildAvailableSize());
  space_builder.SetPercentageResolutionSize(ChildAvailableSize());

  space_builder.SetBaselineAlgorithmType(
      GetConstraintSpace().GetBaselineAlgorithmType());

  if (GetConstraintSpace().HasBlockFragmentation()) {
    SetupSpaceBuilderForFragmentation(container_builder_, spanner, block_offset,
                                      &space_builder);
  }

  return space_builder.ToConstraintSpace();
}

ConstraintSpace ColumnLayoutAlgorithm::CreateConstraintSpaceForMinMax() const {
  ConstraintSpaceBuilder space_builder(GetConstraintSpace(),
                                       Style().GetWritingDirection(),
                                       /* is_new_fc */ true);
  space_builder.SetIsAnonymous(true);
  space_builder.SetIsInColumnBfc();

  return space_builder.ToConstraintSpace();
}

LayoutUnit ColumnLayoutAlgorithm::TotalColumnBlockSize() const {
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
