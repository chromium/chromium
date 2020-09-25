// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_child_iterator.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/style/grid_positions_resolver.h"

namespace blink {

NGGridLayoutAlgorithm::NGGridLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params),
      state_(GridLayoutAlgorithmState::kMeasuringItems) {
  DCHECK(params.space.IsNewFormattingContext());
  DCHECK(!params.break_token);
  container_builder_.SetIsNewFormattingContext(true);

  border_box_size_ = container_builder_.InitialBorderBoxSize();
  child_percentage_size_ = CalculateChildPercentageSize(
      ConstraintSpace(), Node(), ChildAvailableSize());
}

scoped_refptr<const NGLayoutResult> NGGridLayoutAlgorithm::Layout() {
  // Proceed by algorithm state, as some scenarios will involve a non-linear
  // path through these steps (e.g. skipping or redoing some of them).
  while (state_ != GridLayoutAlgorithmState::kCompletedLayout) {
    switch (state_) {
      case GridLayoutAlgorithmState::kMeasuringItems: {
        SetSpecifiedTracks();
        DetermineExplicitTrackStarts();
        ConstructAndAppendGridItems();
        block_column_track_collection_.FinalizeRanges();
        block_row_track_collection_.FinalizeRanges();

        DCHECK_NE(child_percentage_size_.inline_size, kIndefiniteSize);
        algorithm_column_track_collection_ =
            NGGridLayoutAlgorithmTrackCollection(
                block_column_track_collection_,
                /* is_content_box_size_indefinite */ false);

        bool is_content_box_block_size_indefinite =
            child_percentage_size_.block_size == kIndefiniteSize;
        algorithm_row_track_collection_ = NGGridLayoutAlgorithmTrackCollection(
            block_row_track_collection_, is_content_box_block_size_indefinite);

        state_ = GridLayoutAlgorithmState::kResolvingInlineSize;
        break;
      }

      case GridLayoutAlgorithmState::kResolvingInlineSize:
        ComputeUsedTrackSizes(GridTrackSizingDirection::kForColumns);
        state_ = GridLayoutAlgorithmState::kResolvingBlockSize;
        break;

      case GridLayoutAlgorithmState::kResolvingBlockSize:
        ComputeUsedTrackSizes(GridTrackSizingDirection::kForRows);
        state_ = GridLayoutAlgorithmState::kPlacingGridItems;
        break;

      case GridLayoutAlgorithmState::kPlacingGridItems:
        PlaceGridItems();
        state_ = GridLayoutAlgorithmState::kCompletedLayout;
        break;

      case GridLayoutAlgorithmState::kCompletedLayout:
        NOTREACHED();
        break;
    }
  }

  // TODO(ansollan): Calculate the intrinsic-block-size from the tracks.
  LayoutUnit intrinsic_block_size = BorderScrollbarPadding().BlockSum();
  intrinsic_block_size =
      ClampIntrinsicBlockSize(ConstraintSpace(), Node(),
                              BorderScrollbarPadding(), intrinsic_block_size);
  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), BorderPadding(), intrinsic_block_size,
      border_box_size_.inline_size);

  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);
  container_builder_.SetFragmentsTotalBlockSize(block_size);
  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();
  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGGridLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput& input) const {
  return {MinMaxSizes(), /* depends_on_percentage_block_size */ true};
}

const NGGridLayoutAlgorithmTrackCollection&
NGGridLayoutAlgorithm::ColumnTrackCollection() const {
  return algorithm_column_track_collection_;
}

const NGGridLayoutAlgorithmTrackCollection&
NGGridLayoutAlgorithm::RowTrackCollection() const {
  return algorithm_row_track_collection_;
}

NGGridLayoutAlgorithm::GridItemData::GridItemData(const NGBlockNode node)
    : node(node) {}

wtf_size_t NGGridLayoutAlgorithm::GridItemData::StartLine(
    GridTrackSizingDirection track_direction) const {
  const GridSpan& span = (track_direction == kForColumns)
                             ? resolved_position.columns
                             : resolved_position.rows;
  DCHECK(span.IsTranslatedDefinite());
  return span.StartLine();
}

wtf_size_t NGGridLayoutAlgorithm::GridItemData::EndLine(
    GridTrackSizingDirection track_direction) const {
  const GridSpan& span = (track_direction == kForColumns)
                             ? resolved_position.columns
                             : resolved_position.rows;
  DCHECK(span.IsTranslatedDefinite());
  return span.EndLine();
}

NGGridLayoutAlgorithm::ReorderedGridItems::Iterator::Iterator(
    Vector<wtf_size_t>::const_iterator current_index,
    Vector<GridItemData>* items)
    : current_index_(current_index), items_(items) {}

bool NGGridLayoutAlgorithm::ReorderedGridItems::Iterator::operator!=(
    const Iterator& other) const {
  return items_ != other.items_ || current_index_ != other.current_index_;
}

NGGridLayoutAlgorithm::GridItemData&
NGGridLayoutAlgorithm::ReorderedGridItems::Iterator::operator*() {
  DCHECK_LT(*current_index_, items_->size());
  return items_->at(*current_index_);
}

NGGridLayoutAlgorithm::ReorderedGridItems::Iterator&
NGGridLayoutAlgorithm::ReorderedGridItems::Iterator::operator++() {
  ++current_index_;
  return *this;
}

NGGridLayoutAlgorithm::ReorderedGridItems::ReorderedGridItems(
    const Vector<wtf_size_t>& reordered_item_indices,
    Vector<GridItemData>& items)
    : reordered_item_indices_(reordered_item_indices), items_(items) {}

NGGridLayoutAlgorithm::ReorderedGridItems::Iterator
NGGridLayoutAlgorithm::ReorderedGridItems::begin() {
  return Iterator(reordered_item_indices_.begin(), &items_);
}

NGGridLayoutAlgorithm::ReorderedGridItems::Iterator
NGGridLayoutAlgorithm::ReorderedGridItems::end() {
  return Iterator(reordered_item_indices_.end(), &items_);
}

NGGridLayoutAlgorithm::ReorderedGridItems
NGGridLayoutAlgorithm::GetReorderedGridItems() {
  return ReorderedGridItems(reordered_item_indices_, items_);
}

NGGridLayoutAlgorithmTrackCollection& NGGridLayoutAlgorithm::TrackCollection(
    GridTrackSizingDirection track_direction) {
  return (track_direction == kForColumns) ? algorithm_column_track_collection_
                                          : algorithm_row_track_collection_;
}

void NGGridLayoutAlgorithm::ConstructAndAppendGridItems() {
  NGGridChildIterator iterator(Node());
  for (NGBlockNode child = iterator.NextChild(); child;
       child = iterator.NextChild()) {
    GridItemData grid_item = MeasureGridItem(child);
    EnsureTrackCoverageForGridItem(kForColumns, grid_item);
    EnsureTrackCoverageForGridItem(kForRows, grid_item);
    items_.emplace_back(grid_item);
  }

  // Fill grid item indices vector in document order.
  reordered_item_indices_.ReserveInitialCapacity(items_.size());
  for (wtf_size_t i = 0; i < items_.size(); ++i)
    reordered_item_indices_.push_back(i);
}

NGGridLayoutAlgorithm::GridItemData NGGridLayoutAlgorithm::MeasureGridItem(
    const NGBlockNode node) {
  // Before we take track sizing into account for column width contributions,
  // have all child inline and min/max sizes measured for content-based width
  // resolution.
  GridItemData grid_item(node);
  const ComputedStyle& child_style = node.Style();
  bool is_orthogonal_flow_root = !IsParallelWritingMode(
      ConstraintSpace().GetWritingMode(), child_style.GetWritingMode());
  NGConstraintSpace constraint_space = BuildSpaceForGridItem(node);

  // Children with orthogonal writing modes require a full layout pass to
  // determine inline size.
  if (is_orthogonal_flow_root) {
    scoped_refptr<const NGLayoutResult> result = node.Layout(constraint_space);
    grid_item.inline_size = NGFragment(ConstraintSpace().GetWritingMode(),
                                       result->PhysicalFragment())
                                .InlineSize();
  } else {
    NGBoxStrut border_padding_in_child_writing_mode =
        ComputeBorders(constraint_space, node) +
        ComputePadding(constraint_space, child_style);
    grid_item.inline_size = ComputeInlineSizeForFragment(
        constraint_space, node, border_padding_in_child_writing_mode);
  }

  grid_item.margins =
      ComputeMarginsFor(constraint_space, child_style, ConstraintSpace());
  grid_item.min_max_sizes =
      node.ComputeMinMaxSizes(
              ConstraintSpace().GetWritingMode(),
              MinMaxSizesInput(child_percentage_size_.block_size,
                               MinMaxSizesType::kContent),
              &constraint_space)
          .sizes;
  return grid_item;
}

NGConstraintSpace NGGridLayoutAlgorithm::BuildSpaceForGridItem(
    const NGBlockNode node) const {
  NGConstraintSpaceBuilder space_builder(ConstraintSpace(),
                                         node.Style().GetWritingMode(),
                                         node.CreatesNewFormattingContext());

  space_builder.SetCacheSlot(NGCacheSlot::kMeasure);
  space_builder.SetIsPaintedAtomically(true);
  space_builder.SetAvailableSize(ChildAvailableSize());
  space_builder.SetPercentageResolutionSize(child_percentage_size_);
  space_builder.SetTextDirection(node.Style().Direction());
  space_builder.SetIsShrinkToFit(node.Style().LogicalWidth().IsAuto());
  return space_builder.ToConstraintSpace();
}

void NGGridLayoutAlgorithm::SetSpecifiedTracks() {
  const ComputedStyle& grid_style = Style();
  // TODO(kschmi): Auto track repeat count should be based on the number of
  // children, rather than specified auto-column/track. Temporarily assign them
  // to zero here to avoid DCHECK's until we implement this logic.
  automatic_column_repetitions_ = 0;
  automatic_row_repetitions_ = 0;

  // TODO(janewman): We need to implement calculation for track auto repeat
  // count so this can be used outside of testing.
  block_column_track_collection_.SetSpecifiedTracks(
      &grid_style.GridTemplateColumns().NGTrackList(),
      &grid_style.GridAutoColumns().NGTrackList(),
      automatic_column_repetitions_);

  block_row_track_collection_.SetSpecifiedTracks(
      &grid_style.GridTemplateRows().NGTrackList(),
      &grid_style.GridAutoRows().NGTrackList(), automatic_row_repetitions_);
}

void NGGridLayoutAlgorithm::EnsureTrackCoverageForGridItem(
    GridTrackSizingDirection track_direction,
    GridItemData& grid_item) {
  GridSpan span = GridPositionsResolver::ResolveGridPositionsFromStyle(
      Style(), grid_item.node.Style(), track_direction,
      AutoRepeatCountForDirection(track_direction));

  // TODO(janewman): indefinite positions should be resolved with the auto
  // placement algorithm.
  if (span.IsIndefinite())
    return;

  if (track_direction == kForColumns) {
    span.Translate(explicit_column_start_);
    grid_item.resolved_position.columns = span;
    block_column_track_collection_.EnsureTrackCoverage(span.StartLine(),
                                                       span.IntegerSpan());
  } else {
    span.Translate(explicit_row_start_);
    grid_item.resolved_position.rows = span;
    block_row_track_collection_.EnsureTrackCoverage(span.StartLine(),
                                                    span.IntegerSpan());
  }
}

void NGGridLayoutAlgorithm::DetermineExplicitTrackStarts() {
  DCHECK_EQ(0u, explicit_column_start_);
  DCHECK_EQ(0u, explicit_row_start_);

  NGGridChildIterator iterator(Node());
  for (NGBlockNode child = iterator.NextChild(); child;
       child = iterator.NextChild()) {
    GridSpan column_span = GridPositionsResolver::ResolveGridPositionsFromStyle(
        Style(), child.Style(), kForColumns,
        AutoRepeatCountForDirection(kForColumns));
    GridSpan row_span = GridPositionsResolver::ResolveGridPositionsFromStyle(
        Style(), child.Style(), kForRows,
        AutoRepeatCountForDirection(kForRows));
    if (!column_span.IsIndefinite()) {
      explicit_column_start_ = std::max<int>(
          explicit_column_start_, -column_span.UntranslatedStartLine());
    }
    if (!row_span.IsIndefinite()) {
      explicit_row_start_ =
          std::max<int>(explicit_row_start_, -row_span.UntranslatedStartLine());
    }
  }
}

void NGGridLayoutAlgorithm::DetermineGridItemsSpanningIntrinsicOrFlexTracks(
    GridTrackSizingDirection track_direction) {
  auto CompareGridItemsByStartLine =
      [this, track_direction](wtf_size_t index_a, wtf_size_t index_b) -> bool {
    return items_[index_a].StartLine(track_direction) <
           items_[index_b].StartLine(track_direction);
  };
  std::sort(reordered_item_indices_.begin(), reordered_item_indices_.end(),
            CompareGridItemsByStartLine);

  // At this point we have the grid items sorted by their start line in the
  // respective direction; this is important since we'll process both, the
  // ranges in the track collection and the grid items, incrementally.
  const auto& track_collection = TrackCollection(track_direction);
  auto range_spanning_flex_track_iterator = track_collection.RangeIterator();
  auto range_spanning_intrinsic_track_iterator =
      track_collection.RangeIterator();

  for (GridItemData& grid_item : GetReorderedGridItems()) {
    // We want to find the first range in the collection that:
    //   - Spans tracks located AFTER the start line of the current grid item;
    //   this can be done by checking that the last track number of the current
    //   range is NOT less than the current grid item's start line. Furthermore,
    //   since grid items are sorted by start line, if at any point a range is
    //   located BEFORE the current grid item's start line, the same range will
    //   also be located BEFORE any subsequent item's start line.
    //   - Contains a track with an intrinsic/flexible sizing function.
    while (!range_spanning_intrinsic_track_iterator.IsAtEnd() &&
           (range_spanning_intrinsic_track_iterator.RangeTrackEnd() <
                grid_item.StartLine(track_direction) ||
            !track_collection.IsRangeSpanningIntrinsicTrack(
                range_spanning_intrinsic_track_iterator.RangeIndex()))) {
      range_spanning_intrinsic_track_iterator.MoveToNextRange();
    }
    while (!range_spanning_flex_track_iterator.IsAtEnd() &&
           (range_spanning_flex_track_iterator.RangeTrackEnd() <
                grid_item.StartLine(track_direction) ||
            !track_collection.IsRangeSpanningFlexTrack(
                range_spanning_flex_track_iterator.RangeIndex()))) {
      range_spanning_flex_track_iterator.MoveToNextRange();
    }

    // Notice that, from the way we build the ranges of a track collection (see
    // |NGGridBlockTrackCollection::EnsureTrackCoverage|), any given range must
    // either be completely contained or excluded from a grid item's span. Thus,
    // if the current range's last track is also located BEFORE the item's end
    // line, then this range, including the intrinsic/flexible track it spans,
    // is completely contained within this grid item's boundaries.
    // Otherwise, this and any subsequent range are excluded from this item's
    // span, meaning that it does not span an intrinsic/flexible track.
    grid_item.is_spanning_intrinsic_track =
        !range_spanning_intrinsic_track_iterator.IsAtEnd() &&
        range_spanning_intrinsic_track_iterator.RangeTrackEnd() <
            grid_item.EndLine(track_direction);
    grid_item.is_spanning_flex_track =
        !range_spanning_flex_track_iterator.IsAtEnd() &&
        range_spanning_flex_track_iterator.RangeTrackEnd() <
            grid_item.EndLine(track_direction);
  }
}

// https://drafts.csswg.org/css-grid-1/#algo-track-sizing
void NGGridLayoutAlgorithm::ComputeUsedTrackSizes(
    GridTrackSizingDirection track_direction) {
  NGGridLayoutAlgorithmTrackCollection& track_collection =
      TrackCollection(track_direction);
  LayoutUnit content_box_size = (track_direction == kForColumns)
                                    ? child_percentage_size_.inline_size
                                    : child_percentage_size_.block_size;

  // 1. Initialize track sizes (https://drafts.csswg.org/css-grid-1/#algo-init).
  for (auto set_iterator = track_collection.GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    NGGridSet& current_set = set_iterator.CurrentSet();
    const GridTrackSize& track_size = current_set.TrackSize();

    if (track_size.HasFixedMinTrackBreadth()) {
      // Indefinite lengths cannot occur, as they’re treated as 'auto'.
      DCHECK(!track_size.MinTrackBreadth().HasPercentage() ||
             content_box_size != kIndefiniteSize);

      // A fixed sizing function: Resolve to an absolute length and use that
      // size as the track’s initial base size.
      LayoutUnit fixed_min_breadth = ValueForLength(
          track_size.MinTrackBreadth().length(), content_box_size);
      current_set.SetBaseSize(fixed_min_breadth * current_set.TrackCount());
    } else {
      // An intrinsic sizing function: Use an initial base size of zero.
      DCHECK(track_size.HasIntrinsicMinTrackBreadth());
      current_set.SetBaseSize(LayoutUnit());
    }

    if (track_size.HasFixedMaxTrackBreadth()) {
      DCHECK(!track_size.MaxTrackBreadth().HasPercentage() ||
             content_box_size != kIndefiniteSize);

      // A fixed sizing function: Resolve to an absolute length and use that
      // size as the track’s initial growth limit; if the growth limit is less
      // than the base size, increase the growth limit to match the base size.
      LayoutUnit fixed_max_breadth = ValueForLength(
          track_size.MaxTrackBreadth().length(), content_box_size);
      current_set.SetGrowthLimit(
          std::max(current_set.BaseSize(),
                   fixed_max_breadth * current_set.TrackCount()));
    } else {
      // An intrinsic or flexible sizing function: Use an initial growth limit
      // of infinity.
      current_set.SetGrowthLimit(kIndefiniteSize);
    }
  }
}

void NGGridLayoutAlgorithm::SetAutomaticTrackRepetitionsForTesting(
    wtf_size_t auto_column,
    wtf_size_t auto_row) {
  automatic_column_repetitions_ = auto_column;
  automatic_row_repetitions_ = auto_row;
}

wtf_size_t NGGridLayoutAlgorithm::AutoRepeatCountForDirection(
    GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns) ? automatic_column_repetitions_
                                          : automatic_row_repetitions_;
}

void NGGridLayoutAlgorithm::PlaceGridItems() {
  NGGridChildIterator iterator(Node());
  LayoutUnit current_inline_offset, current_block_offset;

  for (auto row_set_iterator = TrackCollection(kForRows).GetSetIterator();
       !row_set_iterator.IsAtEnd(); row_set_iterator.MoveToNextSet()) {
    LayoutUnit row_base_size = row_set_iterator.CurrentSet().BaseSize();
    current_inline_offset = LayoutUnit();

    for (auto column_set_iterator =
             TrackCollection(kForColumns).GetSetIterator();
         !column_set_iterator.IsAtEnd(); column_set_iterator.MoveToNextSet()) {
      LayoutUnit column_base_size = column_set_iterator.CurrentSet().BaseSize();

      const NGBlockNode child_node = iterator.NextChild();
      if (!child_node)
        return;  // TODO(kschmi): DCHECK when auto rows/columns are implemented.

      if (child_node.IsOutOfFlowPositioned()) {
        // TODO(kschmi): Pass correct static positioned offset in.
        container_builder_.AddOutOfFlowChildCandidate(child_node,
                                                      LogicalOffset());
        continue;
      }

      // Layout child nodes based on constraint space from grid row/column
      // definitions and the inline and block offsets being accumulated.
      NGConstraintSpaceBuilder space_builder(
          ConstraintSpace(), child_node.Style().GetWritingMode(),
          /* is_new_fc */ true);
      space_builder.SetIsPaintedAtomically(true);
      space_builder.SetAvailableSize(
          LogicalSize(column_base_size, row_base_size));
      space_builder.SetPercentageResolutionSize(
          LogicalSize(column_base_size, row_base_size));
      space_builder.SetTextDirection(child_node.Style().Direction());
      space_builder.SetIsShrinkToFit(
          child_node.Style().LogicalWidth().IsAuto());
      NGConstraintSpace constraint_space = space_builder.ToConstraintSpace();
      scoped_refptr<const NGLayoutResult> result =
          child_node.Layout(constraint_space);

      container_builder_.AddChild(
          result->PhysicalFragment(),
          {current_inline_offset, current_block_offset});

      // TODO(kschmi): row-gap and column-gap should be accounted for in
      // inline and block positioning.
      current_inline_offset += column_base_size;
    }
    current_block_offset += row_base_size;
  }

  // TODO(kschmi): There should not be any remaining children, as grid auto
  // rows and columns should be expanded to handle all children. However, as
  // that functionality isn't implemented yet, it is currently possible to
  // have more children than available rows and columns. For now, place these
  // children at (0, 0). This should be turned into an assert that no children
  // remain in the iterator after the above loops have completed iterating over
  // rows and columns.
  while (const NGBlockNode child_node = iterator.NextChild()) {
    NGConstraintSpace constraint_space = BuildSpaceForGridItem(child_node);
    scoped_refptr<const NGLayoutResult> result =
        child_node.Layout(constraint_space);
    container_builder_.AddChild(result->PhysicalFragment(),
                                {LayoutUnit(), LayoutUnit()});
  }
}

}  // namespace blink
