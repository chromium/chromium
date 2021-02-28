// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_child_iterator.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_placement.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"

namespace blink {

NGGridLayoutAlgorithm::NGGridLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
  DCHECK(!params.break_token);

  border_box_size_ = container_builder_.InitialBorderBoxSize();

  // At various stages of this algorithm we need to know if the grid
  // available-size. If it is initially indefinite, we need to know the min/max
  // sizes as well. Initialize all these to the same value.
  grid_available_size_ = grid_min_available_size_ = grid_max_available_size_ =
      ChildAvailableSize();

  // Next if our inline-size is indefinite, compute the min/max inline-sizes.
  if (grid_available_size_.inline_size == kIndefiniteSize) {
    const LayoutUnit border_scrollbar_padding =
        BorderScrollbarPadding().InlineSum();
    const MinMaxSizes sizes = ComputeMinMaxInlineSizes(
        ConstraintSpace(), Style(), container_builder_.BorderPadding(),
        [&border_scrollbar_padding](MinMaxSizesType) -> MinMaxSizesResult {
          // If we've reached here we are inside the ComputeMinMaxSizes pass,
          // and also have something like "min-width: min-content". This is
          // cyclic. Just return the border/scrollbar/padding as our
          // "intrinsic" size.
          return MinMaxSizesResult(
              {border_scrollbar_padding, border_scrollbar_padding},
              /* depends_on_percentage_block_size */ false);
        });

    grid_min_available_size_.inline_size =
        (sizes.min_size - border_scrollbar_padding).ClampNegativeToZero();
    grid_max_available_size_.inline_size =
        (sizes.max_size == LayoutUnit::Max())
            ? sizes.max_size
            : (sizes.max_size - border_scrollbar_padding).ClampNegativeToZero();
  }

  // And similar for the min/max block-sizes.
  if (grid_available_size_.block_size == kIndefiniteSize) {
    const LayoutUnit border_scrollbar_padding =
        BorderScrollbarPadding().BlockSum();
    const MinMaxSizes sizes = ComputeMinMaxBlockSizes(
        ConstraintSpace(), Style(), container_builder_.BorderPadding(),
        kIndefiniteSize);

    grid_min_available_size_.block_size =
        (sizes.min_size - border_scrollbar_padding).ClampNegativeToZero();
    grid_max_available_size_.block_size =
        (sizes.max_size == LayoutUnit::Max())
            ? sizes.max_size
            : (sizes.max_size - border_scrollbar_padding).ClampNegativeToZero();
  }
}

scoped_refptr<const NGLayoutResult> NGGridLayoutAlgorithm::Layout() {
  PaintLayerScrollableArea::DelayScrollOffsetClampScope delay_clamp_scope;

  // Measure items.
  GridItems grid_items;
  Vector<GridItemData> out_of_flow_items;
  ConstructAndAppendGridItems(&grid_items, &out_of_flow_items);

  NGGridLayoutAlgorithmTrackCollection column_track_collection;
  NGGridLayoutAlgorithmTrackCollection row_track_collection;
  NGGridPlacement grid_placement(Style(),
                                 ComputeAutomaticRepetitions(kForColumns),
                                 ComputeAutomaticRepetitions(kForRows));

  BuildAlgorithmTrackCollections(&grid_items, &column_track_collection,
                                 &row_track_collection, &grid_placement);

  // Cache track span properties for grid items.
  CacheGridItemsTrackSpanProperties(column_track_collection, &grid_items);
  CacheGridItemsTrackSpanProperties(row_track_collection, &grid_items);

  // We perform the track sizing algorithm using two methods. First
  // |InitializeTrackSizes|, which we need to get an initial column and row set
  // geometry. Then |ComputeUsedTrackSizes|, to finalize the sizing algorithm
  // for both dimensions.
  GridGeometry grid_geometry = {InitializeTrackSizes(&column_track_collection),
                                InitializeTrackSizes(&row_track_collection)};

  // Cache set indices for grid items.
  for (auto& grid_item : grid_items.item_data) {
    grid_item.SetIndices(column_track_collection);
    grid_item.SetIndices(row_track_collection);
  }

  // Resolve inline size.
  ComputeUsedTrackSizes(SizingConstraint::kLayout, grid_geometry,
                        &column_track_collection, &grid_items);

  // Determine the final (used) column set geometry.
  grid_geometry.column_geometry = ComputeSetGeometry(
      column_track_collection, grid_available_size_.inline_size);

  // Resolve block size.
  ComputeUsedTrackSizes(SizingConstraint::kLayout, grid_geometry,
                        &row_track_collection, &grid_items);

  // Determine the final (used) row set geometry.
  grid_geometry.row_geometry =
      ComputeSetGeometry(row_track_collection, grid_available_size_.block_size);

  // Intrinsic block size is based on the final row offset. Because gutters are
  // included in row offsets, subtract out the final gutter (if there is one).
  LayoutUnit final_gutter = (grid_geometry.row_geometry.sets.size() == 1)
                                ? LayoutUnit()
                                : grid_geometry.row_geometry.gutter_size;
  LayoutUnit intrinsic_block_size =
      grid_geometry.row_geometry.sets.back().offset - final_gutter +
      BorderScrollbarPadding().block_end;

  intrinsic_block_size =
      ClampIntrinsicBlockSize(ConstraintSpace(), Node(),
                              BorderScrollbarPadding(), intrinsic_block_size);

  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), BorderPadding(), intrinsic_block_size,
      border_box_size_.inline_size);

  // If we had an indefinite available block-size, we now need to re-calculate
  // our grid-gap, and alignment using our new block-size.
  if (grid_available_size_.block_size == kIndefiniteSize) {
    const LayoutUnit resolved_available_block_size =
        (block_size - BorderScrollbarPadding().BlockSum())
            .ClampNegativeToZero();

    grid_geometry.row_geometry =
        ComputeSetGeometry(row_track_collection, resolved_available_block_size);
  }

  PlaceGridItems(grid_items, grid_geometry, block_size);

  PlaceOutOfFlowDescendants(column_track_collection, row_track_collection,
                            grid_geometry, grid_placement, block_size);

  for (auto& out_of_flow_item : out_of_flow_items) {
    out_of_flow_item.SetIndices(column_track_collection, &grid_placement);
    out_of_flow_item.SetIndices(row_track_collection, &grid_placement);
  }

  PlaceOutOfFlowItems(out_of_flow_items, grid_geometry, block_size);

  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();
  return container_builder_.ToBoxFragment();
}

namespace {

LayoutUnit ComputeTotalTrackSize(
    const NGGridLayoutAlgorithmTrackCollection& track_collection,
    const LayoutUnit grid_gap) {
  LayoutUnit total_track_size;
  for (auto set_iterator = track_collection.GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    const auto& set = set_iterator.CurrentSet();
    total_track_size += set.BaseSize() + set.TrackCount() * grid_gap;
  }
  // Clamp to zero to avoid a negative |grid_gap| when there are no tracks.
  total_track_size -= grid_gap;
  return total_track_size.ClampNegativeToZero();
}

}  // namespace

MinMaxSizesResult NGGridLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput& input) const {
  // TODO(janewman): Handle the cases typically done via:
  // CalculateMinMaxSizesIgnoringChildren.

  // Measure items.
  GridItems grid_items;
  ConstructAndAppendGridItems(&grid_items);

  NGGridLayoutAlgorithmTrackCollection column_track_collection_for_min_size;
  NGGridLayoutAlgorithmTrackCollection row_track_collection;
  NGGridPlacement grid_placement(Style(),
                                 ComputeAutomaticRepetitions(kForColumns),
                                 ComputeAutomaticRepetitions(kForRows));

  BuildAlgorithmTrackCollections(&grid_items,
                                 &column_track_collection_for_min_size,
                                 &row_track_collection, &grid_placement);

  // Cache track span properties for grid items.
  CacheGridItemsTrackSpanProperties(column_track_collection_for_min_size,
                                    &grid_items);

  // Cache set indices for grid items.
  for (auto& grid_item : grid_items) {
    grid_item.SetIndices(column_track_collection_for_min_size);
    grid_item.SetIndices(row_track_collection);
  }

  GridGeometry grid_geometry = {
      InitializeTrackSizes(&column_track_collection_for_min_size),
      InitializeTrackSizes(&row_track_collection)};

  // Before the track sizing algorithm, create a copy of the column collection;
  // one will be used to compute the min size and the other for the max size.
  NGGridLayoutAlgorithmTrackCollection column_track_collection_for_max_size =
      column_track_collection_for_min_size;

  // Resolve inline size under a 'min-content' constraint.
  ComputeUsedTrackSizes(SizingConstraint::kMinContent, grid_geometry,
                        &column_track_collection_for_min_size, &grid_items);
  // Resolve inline size under a 'max-content' constraint.
  ComputeUsedTrackSizes(SizingConstraint::kMaxContent, grid_geometry,
                        &column_track_collection_for_max_size, &grid_items);

  const LayoutUnit grid_gap = GridGap(kForColumns);
  MinMaxSizes sizes{
      ComputeTotalTrackSize(column_track_collection_for_min_size, grid_gap),
      ComputeTotalTrackSize(column_track_collection_for_max_size, grid_gap)};

  // TODO(janewman): Confirm that |input.percentage_resolution_block_size|
  // isn't used within grid layout.
  sizes += BorderScrollbarPadding().InlineSum();
  return MinMaxSizesResult(sizes, /* depends_on_percentage_block_size */ false);
}

NGGridLayoutAlgorithm::AutoPlacementType
NGGridLayoutAlgorithm::GridItemData::AutoPlacement(
    GridTrackSizingDirection flow_direction) const {
  bool is_major_indefinite = Span(flow_direction).IsIndefinite();
  bool is_minor_indefinite =
      Span(flow_direction == kForColumns ? kForRows : kForColumns)
          .IsIndefinite();

  if (is_minor_indefinite && is_major_indefinite)
    return AutoPlacementType::kBoth;
  else if (is_minor_indefinite)
    return AutoPlacementType::kMinor;
  else if (is_major_indefinite)
    return AutoPlacementType::kMajor;

  return AutoPlacementType::kNotNeeded;
}

const GridSpan& NGGridLayoutAlgorithm::GridItemData::Span(
    GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns) ? resolved_position.columns
                                          : resolved_position.rows;
}

void NGGridLayoutAlgorithm::GridItemData::SetSpan(
    const GridSpan& span,
    GridTrackSizingDirection track_direction) {
  if (track_direction == kForColumns)
    resolved_position.columns = span;
  else
    resolved_position.rows = span;
}

wtf_size_t NGGridLayoutAlgorithm::GridItemData::StartLine(
    GridTrackSizingDirection track_direction) const {
  const GridSpan& span = (track_direction == kForColumns)
                             ? resolved_position.columns
                             : resolved_position.rows;
  return span.StartLine();
}

wtf_size_t NGGridLayoutAlgorithm::GridItemData::EndLine(
    GridTrackSizingDirection track_direction) const {
  const GridSpan& span = (track_direction == kForColumns)
                             ? resolved_position.columns
                             : resolved_position.rows;
  return span.EndLine();
}

wtf_size_t NGGridLayoutAlgorithm::GridItemData::SpanSize(
    GridTrackSizingDirection track_direction) const {
  const GridSpan& span = (track_direction == kForColumns)
                             ? resolved_position.columns
                             : resolved_position.rows;
  return span.IntegerSpan();
}

const TrackSpanProperties&
NGGridLayoutAlgorithm::GridItemData::GetTrackSpanProperties(
    GridTrackSizingDirection track_direction) const {
  return track_direction == kForColumns ? column_span_properties
                                        : row_span_properties;
}

void NGGridLayoutAlgorithm::GridItemData::SetTrackSpanProperty(
    TrackSpanProperties::PropertyId property,
    GridTrackSizingDirection track_direction) {
  if (track_direction == kForColumns)
    column_span_properties.SetProperty(property);
  else
    row_span_properties.SetProperty(property);
}

bool NGGridLayoutAlgorithm::GridItemData::IsSpanningFlexibleTrack(
    GridTrackSizingDirection track_direction) const {
  return GetTrackSpanProperties(track_direction)
      .HasProperty(TrackSpanProperties::kHasFlexibleTrack);
}

bool NGGridLayoutAlgorithm::GridItemData::IsSpanningIntrinsicTrack(
    GridTrackSizingDirection track_direction) const {
  return GetTrackSpanProperties(track_direction)
      .HasProperty(TrackSpanProperties::kHasIntrinsicTrack);
}

NGGridLayoutAlgorithm::ItemSetIndices
NGGridLayoutAlgorithm::GridItemData::SetIndices(
    const NGGridLayoutAlgorithmTrackCollection& track_collection,
    const NGGridPlacement* grid_placement) {
  const GridTrackSizingDirection track_direction = track_collection.Direction();

  // If the set indices are already computed, we can just return them.
  base::Optional<ItemSetIndices>& cached_set_indices =
      (track_direction == kForColumns) ? column_set_indices : row_set_indices;
  if (cached_set_indices.has_value())
    return cached_set_indices.value();

  // We only calculate the indexes if:
  // 1. The item is in flow (it is a grid item) or
  // 2. The item is out of flow, but the line was not defined as 'auto' and
  // the line is within the bounds of the grid, since an out of flow item
  // cannot create grid lines.
  wtf_size_t start_line, end_line;
  if (item_type == ItemType::kInGridFlow) {
    start_line = StartLine(track_direction);
    end_line = EndLine(track_direction);

    DCHECK_NE(start_line, kNotFound);
    DCHECK_NE(end_line, kNotFound);
  } else {
    DCHECK(grid_placement);
    grid_placement->ResolveOutOfFlowItemGridLines(
        track_collection, node.Style(), &start_line, &end_line);
  }

  // TODO(ansollano): An out of flow item can have an index that is in the
  // middle of a range. Correctly handle this case.
  ItemSetIndices set_indices;
  if (start_line != kNotFound) {
    DCHECK(track_collection.IsGridLineWithinImplicitGrid(start_line));
    // If a start line of an out of flow item is the last line of the grid, then
    // the |set_indices.begin| is the number of sets in the collection.
    if (track_collection.EndLineOfImplicitGrid() == start_line) {
      DCHECK_EQ(item_type, ItemType::kOutOfFlow);
      set_indices.begin = track_collection.SetCount();
    } else {
      wtf_size_t first_spanned_range =
          track_collection.RangeIndexFromTrackNumber(start_line);
      set_indices.begin =
          track_collection.RangeStartingSetIndex(first_spanned_range);
    }
  }

  if (end_line != kNotFound) {
    DCHECK(track_collection.IsGridLineWithinImplicitGrid(end_line));
    // If an end line of an out of flow item is the first line of the grid, then
    // the |set_indices.end| is 0.
    if (!end_line) {
      DCHECK_EQ(item_type, ItemType::kOutOfFlow);
      set_indices.end = 0;
    } else {
      wtf_size_t last_spanned_range =
          track_collection.RangeIndexFromTrackNumber(end_line - 1);
      set_indices.end =
          track_collection.RangeStartingSetIndex(last_spanned_range) +
          track_collection.RangeSetCount(last_spanned_range);
    }
  }

#if DCHECK_IS_ON()
  if (set_indices.begin != kNotFound && set_indices.end != kNotFound) {
    DCHECK_LE(set_indices.end, track_collection.SetCount());
    DCHECK_LT(set_indices.begin, set_indices.end);
  } else if (set_indices.begin != kNotFound) {
    DCHECK_LE(set_indices.begin, track_collection.SetCount());
  } else if (set_indices.end != kNotFound) {
    DCHECK_LE(set_indices.end, track_collection.SetCount());
  }
#endif

  cached_set_indices = set_indices;
  return set_indices;
}

NGGridLayoutAlgorithm::GridItems::Iterator
NGGridLayoutAlgorithm::GridItems::begin() {
  return Iterator(&item_data, reordered_item_indices.begin());
}

NGGridLayoutAlgorithm::GridItems::Iterator
NGGridLayoutAlgorithm::GridItems::end() {
  return Iterator(&item_data, reordered_item_indices.end());
}

void NGGridLayoutAlgorithm::GridItems::Append(
    const GridItemData& new_item_data) {
  reordered_item_indices.push_back(item_data.size());
  item_data.emplace_back(new_item_data);
}

bool NGGridLayoutAlgorithm::GridItems::IsEmpty() const {
  return item_data.IsEmpty();
}

namespace {

// TODO(ethavar): Consider doing this in-place, as it's not used more than once.
// Returns an iterator for every |NGGridSet| contained within an item's span in
// the relevant track collection.
NGGridLayoutAlgorithmTrackCollection::SetIterator GetSetIteratorForItem(
    NGGridLayoutAlgorithm::GridItemData& item,
    NGGridLayoutAlgorithmTrackCollection& track_collection) {
  NGGridLayoutAlgorithm::ItemSetIndices set_indices =
      item.SetIndices(track_collection);
  return track_collection.GetSetIterator(set_indices.begin, set_indices.end);
}

}  // namespace

// TODO(ethavar): Current implementation of this method simply returns the
// preferred size of the grid item in the relevant direction. We should follow
// the definitions from https://drafts.csswg.org/css-grid-2/#algo-spanning-items
// (i.e. compute minimum, min-content, and max-content contributions).
LayoutUnit NGGridLayoutAlgorithm::ContributionSizeForGridItem(
    const GridGeometry& grid_geometry,
    const GridItemData& grid_item,
    GridTrackSizingDirection track_direction,
    GridItemContributionType contribution_type) const {
  const NGBlockNode& node = grid_item.node;
  const ComputedStyle& item_style = node.Style();

  // TODO(ikilpatrick): We'll need to record if any child used an indefinite
  // size for its contribution, such that we can then do the 2nd pass on the
  // track-sizing algorithm.
  LogicalRect unused;
  const NGConstraintSpace space = CreateConstraintSpace(
      grid_geometry, grid_item, NGCacheSlot::kMeasure, &unused);

  bool is_parallel_with_track_direction =
      (track_direction == kForColumns) ==
      IsParallelWritingMode(Style().GetWritingMode(),
                            item_style.GetWritingMode());

  auto MinMaxContentSizes = [&]() -> MinMaxSizes {
    DCHECK(is_parallel_with_track_direction);
    // TODO(ikilpatrick): kIndefiniteSize is incorrect for the %-block-size.
    // We'll want to determine this using the base or used track-sizes instead.
    // This should match the %-resolution sizes we use for layout during
    // measuring.
    MinMaxSizesInput input(kIndefiniteSize, MinMaxSizesType::kContent);
    return ComputeMinAndMaxContentContributionForSelf(node, input).sizes;
  };

  // This function will determine the correct block-size of a grid-item.
  // TODO(ikilpatrick): This should try and skip layout when possible. Notes:
  //  - We'll need to do a full layout for tables.
  //  - We'll need special logic for replaced elements.
  //  - We'll need to respect the aspect-ratio when appropriate.
  auto BlockSize = [&]() -> LayoutUnit {
    DCHECK(!is_parallel_with_track_direction);
    scoped_refptr<const NGLayoutResult> result = node.Layout(space);
    // We want to return the block-size in the *child's* writing-mode.
    return NGFragment(item_style.GetWritingDirection(),
                      result->PhysicalFragment())
        .BlockSize();
  };

  LayoutUnit contribution;
  switch (contribution_type) {
    case GridItemContributionType::kForContentBasedMinimums:
    case GridItemContributionType::kForIntrinsicMaximums:
      if (is_parallel_with_track_direction)
        contribution = MinMaxContentSizes().min_size;
      else
        contribution = BlockSize();
      break;
    case GridItemContributionType::kForIntrinsicMinimums: {
      // TODO(ikilpatrick): All of the below is incorrect for replaced elements.

      const Length& main_length = is_parallel_with_track_direction
                                      ? item_style.LogicalWidth()
                                      : item_style.LogicalHeight();

      // We could be clever is and make this an if-stmt, but each type has
      // subtle consequences. This forces us in the future when we add a new
      // length type to consider what the best thing is for grid.
      switch (main_length.GetType()) {
        case Length::kAuto:
        case Length::kFitContent:
        case Length::kFillAvailable:
        case Length::kPercent:
        case Length::kCalculated: {
          // All of the above lengths are considered auto if we are querying a
          // minimum contribution. They all require definite track-sizes to
          // determine their final size.

          // Scroll containers are "compressible", and we only consider their
          // min-size when determining their contribution.
          if (item_style.IsScrollContainer()) {
            const NGBoxStrut border_padding =
                ComputeBorders(space, node) + ComputePadding(space, item_style);

            // TODO(ikilpatrick): This block needs to respect the aspect-ratio,
            // and apply the transferred min/max sizes when appropriate. We do
            // this sometimes elsewhere so should unify and simplify this code.
            if (is_parallel_with_track_direction) {
              auto MinMaxSizesFunc =
                  [&](MinMaxSizesType type) -> MinMaxSizesResult {
                // TODO(ikilpatrick): Again, kIndefiniteSize here is incorrect,
                // and needs to use the base or resolved track sizes.
                MinMaxSizesInput input(kIndefiniteSize, type);
                return node.ComputeMinMaxSizes(item_style.GetWritingMode(),
                                               input, &space);
              };

              contribution = ResolveMinInlineLength(
                  space, item_style, border_padding, MinMaxSizesFunc,
                  item_style.LogicalMinWidth());
            } else {
              contribution =
                  ResolveMinBlockLength(space, item_style, border_padding,
                                        item_style.LogicalMinHeight());
            }
            break;
          }

          if (is_parallel_with_track_direction)
            contribution = MinMaxContentSizes().min_size;
          else
            contribution = BlockSize();
          break;
        }
        case Length::kMinContent:
        case Length::kMaxContent:
        case Length::kFixed: {
          // All of the above lengths are "definite" (non-auto), and don't need
          // the special min-size treatment above. (They will all end up being
          // the specified size).
          if (is_parallel_with_track_direction) {
            // TODO(ikilpatrick): This is incorrect for replaced elements.
            const NGBoxStrut border_padding =
                ComputeBorders(space, node) + ComputePadding(space, item_style);
            contribution =
                ComputeInlineSizeForFragment(space, node, border_padding);
          } else {
            contribution = BlockSize();
          }
          break;
        }
        case Length::kMinIntrinsic:
        case Length::kDeviceWidth:
        case Length::kDeviceHeight:
        case Length::kExtendToZoom:
        case Length::kNone:
          NOTREACHED();
          break;
      }
      break;
    }
    case GridItemContributionType::kForMaxContentMinimums:
    case GridItemContributionType::kForMaxContentMaximums:
      if (is_parallel_with_track_direction)
        contribution = MinMaxContentSizes().max_size;
      else
        contribution = BlockSize();
      break;
    case GridItemContributionType::kForFreeSpace:
      NOTREACHED() << "|kForFreeSpace| should only be used to distribute extra "
                      "space in maximize tracks and stretch auto tracks steps.";
      break;
  }

  const auto margins =
      ComputeMarginsFor(space, node.Style(), ConstraintSpace());

  DCHECK_NE(contribution, kIndefiniteSize);
  return contribution + ((track_direction == kForColumns) ? margins.InlineSum()
                                                          : margins.BlockSum());
}

void NGGridLayoutAlgorithm::ConstructAndAppendGridItems(
    GridItems* grid_items,
    Vector<GridItemData>* out_of_flow_items) const {
  DCHECK(grid_items);
  NGGridChildIterator iterator(Node());
  for (NGBlockNode child = iterator.NextChild(); child;
       child = iterator.NextChild()) {
    GridItemData grid_item(MeasureGridItem(child));
    // If |out_of_flow_items| is provided, store out-of-flow items separately,
    // as they do not contribute to track sizing or auto-placement.
    if (grid_item.item_type == ItemType::kInGridFlow)
      grid_items->Append(grid_item);
    else if (out_of_flow_items)
      out_of_flow_items->emplace_back(grid_item);
  }
}

// https://drafts.csswg.org/css-grid-2/#auto-repeat
wtf_size_t NGGridLayoutAlgorithm::ComputeAutomaticRepetitions(
    GridTrackSizingDirection track_direction) const {
  const NGGridTrackList& track_list =
      (track_direction == kForColumns)
          ? Style().GridTemplateColumns().NGTrackList()
          : Style().GridTemplateRows().NGTrackList();
  if (!track_list.HasAutoRepeater())
    return 0;

  LayoutUnit available_size = (track_direction == kForColumns)
                                  ? grid_available_size_.inline_size
                                  : grid_available_size_.block_size;
  LayoutUnit max_available_size = available_size;

  if (available_size == kIndefiniteSize) {
    max_available_size = (track_direction == kForColumns)
                             ? grid_max_available_size_.inline_size
                             : grid_max_available_size_.block_size;
    available_size = (track_direction == kForColumns)
                         ? grid_min_available_size_.inline_size
                         : grid_min_available_size_.block_size;
  }

  const LayoutUnit grid_gap = GridGap(track_direction, available_size);

  LayoutUnit auto_repeater_size;
  LayoutUnit non_auto_specified_size;
  for (wtf_size_t repeater_index = 0;
       repeater_index < track_list.RepeaterCount(); ++repeater_index) {
    const wtf_size_t repeater_track_count =
        track_list.RepeatSize(repeater_index);
    LayoutUnit repeater_size;
    for (wtf_size_t track_index = 0; track_index < repeater_track_count;
         ++track_index) {
      const GridTrackSize& track_size =
          track_list.RepeatTrackSize(repeater_index, track_index);
      base::Optional<LayoutUnit> fixed_min_track_breadth;
      base::Optional<LayoutUnit> fixed_max_track_breadth;
      if (track_size.HasFixedMaxTrackBreadth()) {
        fixed_max_track_breadth = MinimumValueForLength(
            track_size.MaxTrackBreadth().length(), available_size);
      }
      if (track_size.HasFixedMinTrackBreadth()) {
        fixed_min_track_breadth = MinimumValueForLength(
            track_size.MinTrackBreadth().length(), available_size);
      }
      LayoutUnit track_contribution;
      if (fixed_max_track_breadth && fixed_min_track_breadth) {
        track_contribution =
            std::max(*fixed_max_track_breadth, *fixed_min_track_breadth);
      } else if (fixed_max_track_breadth) {
        track_contribution = *fixed_max_track_breadth;
      } else if (fixed_min_track_breadth) {
        track_contribution = *fixed_min_track_breadth;
      }

      // For the purpose of finding the number of auto-repeated tracks in a
      // standalone axis, the UA must floor the track size to a UA-specified
      // value to avoid division by zero. It is suggested that this floor be
      // 1px.
      if (track_list.RepeatType(repeater_index) !=
          NGGridTrackRepeater::kNoAutoRepeat) {
        track_contribution = std::max(LayoutUnit(1), track_contribution);
      }

      repeater_size += track_contribution + grid_gap;
    }
    if (track_list.RepeatType(repeater_index) ==
        NGGridTrackRepeater::kNoAutoRepeat) {
      non_auto_specified_size +=
          repeater_size * track_list.RepeatCount(repeater_index, 0);
    } else {
      DCHECK_EQ(0, auto_repeater_size);
      auto_repeater_size = repeater_size;
    }
  }

  DCHECK_GT(auto_repeater_size, 0);

  // We can compute the number of repetitions by satisfying the expression
  // below. Notice that we subtract an extra |grid_gap| since it was included
  // in the contribution for the last set in the collection.
  //   available_size =
  //       (repetitions * auto_repeater_size) +
  //       non_auto_specified_size - grid_gap
  //
  // Solving for repetitions we have:
  //   repetitions =
  //       available_size - (non_auto_specified_size - grid_gap) /
  //       auto_repeater_size
  non_auto_specified_size -= grid_gap;

  // First we want to allow as many repetitions as possible, up to the max
  // available-size. Only do this if we have a definite max-size.
  // If a definite available-size was provided, |max_available_size| will be
  // set to that value.
  if (max_available_size != LayoutUnit::Max()) {
    // Use floor to ensure that the auto repeater sizes goes under the max
    // available-size.
    const int count = FloorToInt(
        (max_available_size - non_auto_specified_size) / auto_repeater_size);
    return (count <= 0) ? 1u : count;
  }

  // Next, consider the min available-size, which was already used to floor
  // |available_size|. Use ceil to ensure that the auto repeater size goes
  // above this min available-size.
  const int count = CeilToInt((available_size - non_auto_specified_size) /
                              auto_repeater_size);
  return (count <= 0) ? 1u : count;
}

namespace {

using AxisEdge = NGGridLayoutAlgorithm::AxisEdge;

// Given an |item_position| determines the correct |AxisEdge| alignment.
// Additionally will determine if the grid-item should be stretched with the
// |is_stretched| out-parameter.
AxisEdge AxisEdgeFromItemPosition(const ComputedStyle& container_style,
                                  const ComputedStyle& style,
                                  const ItemPosition item_position,
                                  bool is_inline_axis,
                                  bool* is_stretched) {
  DCHECK(is_stretched);
  *is_stretched = false;

  // Auto-margins take precedence over any alignment properties.
  if (style.MayHaveMargin()) {
    bool start_auto = is_inline_axis
                          ? style.MarginStartUsing(container_style).IsAuto()
                          : style.MarginBeforeUsing(container_style).IsAuto();
    bool end_auto = is_inline_axis
                        ? style.MarginEndUsing(container_style).IsAuto()
                        : style.MarginAfterUsing(container_style).IsAuto();

    if (start_auto && end_auto)
      return AxisEdge::kCenter;
    else if (start_auto)
      return AxisEdge::kEnd;
    else if (end_auto)
      return AxisEdge::kStart;
  }

  const auto container_writing_direction =
      container_style.GetWritingDirection();

  switch (item_position) {
    case ItemPosition::kSelfStart:
    case ItemPosition::kSelfEnd: {
      // In order to determine the correct "self" axis-edge without a
      // complicated set of if-branches we use two converters.

      // First use the grid-item's writing-direction to convert the logical
      // edge into the physical coordinate space.
      LogicalToPhysical<AxisEdge> physical(style.GetWritingDirection(),
                                           AxisEdge::kStart, AxisEdge::kEnd,
                                           AxisEdge::kStart, AxisEdge::kEnd);

      // Then use the container's writing-direction to convert the physical
      // edges, into our logical coordinate space.
      PhysicalToLogical<AxisEdge> logical(container_writing_direction,
                                          physical.Top(), physical.Right(),
                                          physical.Bottom(), physical.Left());

      if (is_inline_axis) {
        return item_position == ItemPosition::kSelfStart ? logical.InlineStart()
                                                         : logical.InlineEnd();
      }
      return item_position == ItemPosition::kSelfStart ? logical.BlockStart()
                                                       : logical.BlockEnd();
    }
    case ItemPosition::kCenter:
      return AxisEdge::kCenter;
    case ItemPosition::kFlexStart:
    case ItemPosition::kStart:
      return AxisEdge::kStart;
    case ItemPosition::kFlexEnd:
    case ItemPosition::kEnd:
      return AxisEdge::kEnd;
    case ItemPosition::kStretch:
      *is_stretched = true;
      return AxisEdge::kStart;
    case ItemPosition::kBaseline:
    case ItemPosition::kLastBaseline:
      return AxisEdge::kBaseline;
    case ItemPosition::kLeft:
      DCHECK(is_inline_axis);
      return container_writing_direction.IsLtr() ? AxisEdge::kStart
                                                 : AxisEdge::kEnd;
    case ItemPosition::kRight:
      DCHECK(is_inline_axis);
      return container_writing_direction.IsRtl() ? AxisEdge::kStart
                                                 : AxisEdge::kEnd;
    case ItemPosition::kLegacy:
    case ItemPosition::kAuto:
    case ItemPosition::kNormal:
      NOTREACHED();
      break;
  }

  NOTREACHED();
  return AxisEdge::kStart;
}

}  // namespace

NGGridLayoutAlgorithm::GridItemData NGGridLayoutAlgorithm::MeasureGridItem(
    const NGBlockNode node) const {
  const auto& container_style = Style();

  // Before we take track sizing into account for column width contributions,
  // have all child inline and min/max sizes measured for content-based width
  // resolution.
  GridItemData grid_item(node);
  const ComputedStyle& item_style = node.Style();

  const ItemPosition normal_behaviour =
      node.IsReplaced() ? ItemPosition::kStart : ItemPosition::kStretch;

  // Determine the alignment for the grid-item ahead of time (we may need to
  // know if it stretches ahead of time to correctly determine any block-axis
  // contribution).
  grid_item.inline_axis_alignment = AxisEdgeFromItemPosition(
      container_style, item_style,
      item_style.ResolvedJustifySelf(normal_behaviour, &container_style)
          .GetPosition(),
      /* is_inline_axis */ true, &grid_item.is_inline_axis_stretched);
  grid_item.block_axis_alignment = AxisEdgeFromItemPosition(
      container_style, item_style,
      item_style.ResolvedAlignSelf(normal_behaviour, &container_style)
          .GetPosition(),
      /* is_inline_axis */ false, &grid_item.is_block_axis_stretched);

  grid_item.item_type = node.IsOutOfFlowPositioned() ? ItemType::kOutOfFlow
                                                     : ItemType::kInGridFlow;

  return grid_item;
}

void NGGridLayoutAlgorithm::BuildBlockTrackCollections(
    GridItems* grid_items,
    NGGridBlockTrackCollection* column_track_collection,
    NGGridBlockTrackCollection* row_track_collection,
    NGGridPlacement* grid_placement) const {
  DCHECK(grid_items);
  DCHECK(column_track_collection);
  DCHECK(row_track_collection);
  DCHECK(grid_placement);
  const ComputedStyle& grid_style = Style();

  auto BuildBlockTrackCollection =
      [&](NGGridBlockTrackCollection* track_collection) {
        const GridTrackSizingDirection track_direction =
            track_collection->Direction();
        const wtf_size_t start_offset =
            grid_placement->StartOffset(track_direction);

        const NGGridTrackList& template_track_list =
            (track_direction == kForColumns)
                ? grid_style.GridTemplateColumns().NGTrackList()
                : grid_style.GridTemplateRows().NGTrackList();
        const NGGridTrackList& auto_track_list =
            (track_direction == kForColumns)
                ? grid_style.GridAutoColumns().NGTrackList()
                : grid_style.GridAutoRows().NGTrackList();

        track_collection->SetSpecifiedTracks(
            &template_track_list, &auto_track_list, start_offset,
            grid_placement->AutoRepetitions(track_collection->Direction()));
        EnsureTrackCoverageForGridItems(*grid_items, track_collection);
        track_collection->FinalizeRanges(start_offset);
      };

  grid_placement->RunAutoPlacementAlgorithm(grid_items);
  BuildBlockTrackCollection(column_track_collection);
  BuildBlockTrackCollection(row_track_collection);
}

void NGGridLayoutAlgorithm::BuildAlgorithmTrackCollections(
    GridItems* grid_items,
    NGGridLayoutAlgorithmTrackCollection* column_track_collection,
    NGGridLayoutAlgorithmTrackCollection* row_track_collection,
    NGGridPlacement* grid_placement) const {
  DCHECK(grid_items);
  DCHECK(column_track_collection);
  DCHECK(row_track_collection);
  DCHECK(grid_placement);

  // Build block track collections.
  NGGridBlockTrackCollection column_block_track_collection(kForColumns);
  NGGridBlockTrackCollection row_block_track_collection(kForRows);
  BuildBlockTrackCollections(grid_items, &column_block_track_collection,
                             &row_block_track_collection, grid_placement);

  // Build algorithm track collections from the block track collections.
  *column_track_collection = NGGridLayoutAlgorithmTrackCollection(
      column_block_track_collection,
      grid_available_size_.inline_size == kIndefiniteSize);

  *row_track_collection = NGGridLayoutAlgorithmTrackCollection(
      row_block_track_collection,
      grid_available_size_.block_size == kIndefiniteSize);
}

void NGGridLayoutAlgorithm::EnsureTrackCoverageForGridItems(
    const GridItems& grid_items,
    NGGridBlockTrackCollection* track_collection) const {
  DCHECK(track_collection);
  const GridTrackSizingDirection track_direction =
      track_collection->Direction();
  for (const auto& grid_item : grid_items.item_data) {
    track_collection->EnsureTrackCoverage(grid_item.StartLine(track_direction),
                                          grid_item.SpanSize(track_direction));
  }
}

void NGGridLayoutAlgorithm::CacheGridItemsTrackSpanProperties(
    const NGGridLayoutAlgorithmTrackCollection& track_collection,
    GridItems* grid_items) const {
  DCHECK(grid_items);
  const GridTrackSizingDirection track_direction = track_collection.Direction();

  auto CompareGridItemsByStartLine = [grid_items, track_direction](
                                         wtf_size_t a, wtf_size_t b) -> bool {
    return grid_items->item_data[a].StartLine(track_direction) <
           grid_items->item_data[b].StartLine(track_direction);
  };
  std::sort(grid_items->reordered_item_indices.begin(),
            grid_items->reordered_item_indices.end(),
            CompareGridItemsByStartLine);

  auto CacheTrackSpanPropertyForAllGridItems =
      [&](TrackSpanProperties::PropertyId property) {
        // At this point we have the grid items sorted by their start line in
        // the respective direction; this is important since we'll process both,
        // the ranges in the track collection and the grid items, incrementally.
        auto range_iterator = track_collection.RangeIterator();

        for (auto& grid_item : *grid_items) {
          // We want to find the first range in the collection that:
          //   - Spans tracks located AFTER the start line of the current grid
          //   item; this can be done by checking that the last track number of
          //   the current range is NOT less than the current grid item's start
          //   line. Furthermore, since grid items are sorted by start line, if
          //   at any point a range is located BEFORE the current grid item's
          //   start line, the same range will also be located BEFORE any
          //   subsequent item's start line.
          //   - Contains a track that fulfills the specified property.
          while (!range_iterator.IsAtEnd() &&
                 (range_iterator.RangeTrackEnd() <
                      grid_item.StartLine(track_direction) ||
                  !track_collection.RangeHasTrackSpanProperty(
                      range_iterator.RangeIndex(), property))) {
            range_iterator.MoveToNextRange();
          }

          // Since we discarded every range in the track collection, any
          // following grid item cannot fulfill the property.
          if (range_iterator.IsAtEnd())
            break;

          // Notice that, from the way we build the ranges of a track collection
          // (see |NGGridBlockTrackCollection::EnsureTrackCoverage|), any given
          // range must either be completely contained or excluded from a grid
          // item's span. Thus, if the current range's last track is also
          // located BEFORE the item's end line, then this range, including a
          // track that fulfills the specified property, is completely contained
          // within this item's boundaries. Otherwise, this and every subsequent
          // range are excluded from the grid item's span, meaning that such
          // item cannot satisfy the property we are looking for.
          if (range_iterator.RangeTrackEnd() <
              grid_item.EndLine(track_direction)) {
            grid_item.SetTrackSpanProperty(property, track_direction);
          }
        }
      };

  CacheTrackSpanPropertyForAllGridItems(TrackSpanProperties::kHasFlexibleTrack);
  CacheTrackSpanPropertyForAllGridItems(
      TrackSpanProperties::kHasIntrinsicTrack);
}

// https://drafts.csswg.org/css-grid-2/#algo-init
NGGridLayoutAlgorithm::SetGeometry NGGridLayoutAlgorithm::InitializeTrackSizes(
    NGGridLayoutAlgorithmTrackCollection* track_collection) const {
  DCHECK(track_collection);
  const GridTrackSizingDirection track_direction =
      track_collection->Direction();
  LayoutUnit available_size = (track_direction == kForColumns)
                                  ? grid_available_size_.inline_size
                                  : grid_available_size_.block_size;

  LayoutUnit set_offset = (track_direction == kForColumns)
                              ? BorderScrollbarPadding().inline_start
                              : BorderScrollbarPadding().block_start;
  wtf_size_t last_indefinite_index = kNotFound;
  wtf_size_t index = 0u;
  Vector<SetOffsetData> sets;
  sets.ReserveInitialCapacity(track_collection->SetCount() + 1);
  sets.emplace_back(set_offset, last_indefinite_index);

  const LayoutUnit grid_gap = GridGap(track_direction);

  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    auto& current_set = set_iterator.CurrentSet();
    const GridTrackSize& track_size = current_set.TrackSize();

    if (track_size.IsFitContent()) {
      // Indefinite lengths cannot occur, as they must be normalized to 'auto'.
      DCHECK(!track_size.FitContentTrackBreadth().HasPercentage() ||
             available_size != kIndefiniteSize);
      current_set.SetFitContentLimit(MinimumValueForLength(
          track_size.FitContentTrackBreadth().length(), available_size));
    }

    if (track_size.HasFixedMinTrackBreadth()) {
      DCHECK(!track_size.MinTrackBreadth().HasPercentage() ||
             available_size != kIndefiniteSize);

      // A fixed sizing function: Resolve to an absolute length and use that
      // size as the track’s initial base size.
      LayoutUnit fixed_min_breadth = MinimumValueForLength(
          track_size.MinTrackBreadth().length(), available_size);
      current_set.SetBaseSize(fixed_min_breadth * current_set.TrackCount());
    } else {
      // An intrinsic sizing function: Use an initial base size of zero.
      DCHECK(track_size.HasIntrinsicMinTrackBreadth());
      current_set.SetBaseSize(LayoutUnit());
    }

    // Note that, since |NGGridSet| initializes its growth limit as indefinite,
    // an intrinsic or flexible sizing function needs no further resolution.
    if (track_size.HasFixedMaxTrackBreadth()) {
      DCHECK(!track_size.MaxTrackBreadth().HasPercentage() ||
             available_size != kIndefiniteSize);

      // A fixed sizing function: Resolve to an absolute length and use that
      // size as the track’s initial growth limit; if the growth limit is less
      // than the base size, increase the growth limit to match the base size.
      LayoutUnit fixed_max_breadth = MinimumValueForLength(
          track_size.MaxTrackBreadth().length(), available_size);
      current_set.SetGrowthLimit(
          std::max(current_set.BaseSize(),
                   fixed_max_breadth * current_set.TrackCount()));
    }

    DCHECK_NE(track_size.GetType(), kLengthTrackSizing);

    // TODO(ikilpatrick): If all of are our row tracks are "inflexible" (they
    // all have fixed min/max track breadths which are the same), we need to
    // also apply 'align-content' upfront to ensure that orthogonal children
    // have the correct available-size given.

    // For the purposes of our "base" row set geometry, we only use any fixed
    // max-track breadth. We use this for sizing any orthogonal, (or
    // %-block-size) children.
    if (track_direction == kForRows && track_size.HasFixedMaxTrackBreadth()) {
      set_offset +=
          current_set.GrowthLimit() + current_set.TrackCount() * grid_gap;
    } else {
      last_indefinite_index = index;
    }

    sets.emplace_back(set_offset, last_indefinite_index);
    ++index;
  }

  return {sets, grid_gap};
}

// https://drafts.csswg.org/css-grid-2/#algo-track-sizing
void NGGridLayoutAlgorithm::ComputeUsedTrackSizes(
    SizingConstraint sizing_constraint,
    const GridGeometry& grid_geometry,
    NGGridLayoutAlgorithmTrackCollection* track_collection,
    GridItems* grid_items) const {
  DCHECK(track_collection);
  DCHECK(grid_items);

  // 2. Resolve intrinsic track sizing functions to absolute lengths.
  ResolveIntrinsicTrackSizes(grid_geometry, track_collection, grid_items);

  // 3. If the free space is positive, distribute it equally to the base sizes
  // of all tracks, freezing tracks as they reach their growth limits (and
  // continuing to grow the unfrozen tracks as needed).
  MaximizeTracks(sizing_constraint, track_collection);

  // TODO(janewman): 4. Expand Flexible Tracks

  // 5. Stretch 'auto' Tracks
  StretchAutoTracks(sizing_constraint, track_collection);
}

// Helpers for the track sizing algorithm.
namespace {

using GridItemContributionType =
    NGGridLayoutAlgorithm::GridItemContributionType;
using GridSetVector = Vector<NGGridSet*, 16>;

LayoutUnit DefiniteGrowthLimit(const NGGridSet& set) {
  LayoutUnit growth_limit = set.GrowthLimit();
  // For infinite growth limits, substitute the track’s base size.
  return (growth_limit == kIndefiniteSize) ? set.BaseSize() : growth_limit;
}

// Returns the corresponding size to be increased by accommodating a grid item's
// contribution; for intrinsic min track sizing functions, return the base size.
// For intrinsic max track sizing functions, return the growth limit.
LayoutUnit AffectedSizeForContribution(
    const NGGridSet& set,
    GridItemContributionType contribution_type) {
  switch (contribution_type) {
    case GridItemContributionType::kForIntrinsicMinimums:
    case GridItemContributionType::kForContentBasedMinimums:
    case GridItemContributionType::kForMaxContentMinimums:
      return set.BaseSize();
    case GridItemContributionType::kForIntrinsicMaximums:
    case GridItemContributionType::kForMaxContentMaximums:
      return DefiniteGrowthLimit(set);
    case GridItemContributionType::kForFreeSpace:
      NOTREACHED();
      return LayoutUnit();
  }
}

void GrowAffectedSizeByPlannedIncrease(
    NGGridSet& set,
    GridItemContributionType contribution_type) {
  bool did_growth_limit_become_finite = false;
  switch (contribution_type) {
    case GridItemContributionType::kForIntrinsicMinimums:
    case GridItemContributionType::kForContentBasedMinimums:
    case GridItemContributionType::kForMaxContentMinimums:
      set.SetBaseSize(set.BaseSize() + set.PlannedIncrease());
      break;
    case GridItemContributionType::kForIntrinsicMaximums:
    case GridItemContributionType::kForMaxContentMaximums:
      did_growth_limit_become_finite = (set.GrowthLimit() == kIndefiniteSize);
      set.SetGrowthLimit(DefiniteGrowthLimit(set) + set.PlannedIncrease());
      break;
    case GridItemContributionType::kForFreeSpace:
      NOTREACHED();
      break;
  }

  // Mark any tracks whose growth limit changed from infinite to finite in this
  // step as infinitely growable for the next step.
  if (contribution_type == GridItemContributionType::kForIntrinsicMaximums)
    set.SetInfinitelyGrowable(did_growth_limit_become_finite);
  else
    set.SetInfinitelyGrowable(false);
}

// Returns true if a set should increase its used size according to the steps in
// https://drafts.csswg.org/css-grid-2/#algo-spanning-items; false otherwise.
bool IsContributionAppliedToSet(const NGGridSet& set,
                                GridItemContributionType contribution_type) {
  switch (contribution_type) {
    case GridItemContributionType::kForIntrinsicMinimums:
      return set.TrackSize().HasIntrinsicMinTrackBreadth();
    case GridItemContributionType::kForContentBasedMinimums:
      return set.TrackSize().HasMinOrMaxContentMinTrackBreadth();
    case GridItemContributionType::kForMaxContentMinimums:
      // TODO(ethavar): Check if the grid container is being sized under a
      // 'max-content' constraint to consider 'auto' min track sizing functions,
      // see https://drafts.csswg.org/css-grid-2/#track-size-max-content-min.
      return set.TrackSize().HasMaxContentMinTrackBreadth();
    case GridItemContributionType::kForIntrinsicMaximums:
      return set.TrackSize().HasIntrinsicMaxTrackBreadth();
    case GridItemContributionType::kForMaxContentMaximums:
      return set.TrackSize().HasMaxContentOrAutoMaxTrackBreadth();
    case GridItemContributionType::kForFreeSpace:
      return true;
  }
}

// https://drafts.csswg.org/css-grid-2/#extra-space
// Returns true if a set's used size should be consider to grow beyond its limit
// (see the "Distribute space beyond limits" section); otherwise, false.
// Note that we will deliberately return false in cases where we don't have a
// collection of tracks different than "all affected tracks".
bool ShouldUsedSizeGrowBeyondLimit(const NGGridSet& set,
                                   GridItemContributionType contribution_type) {
  switch (contribution_type) {
    case GridItemContributionType::kForIntrinsicMinimums:
    case GridItemContributionType::kForContentBasedMinimums:
      return set.TrackSize().HasIntrinsicMaxTrackBreadth();
    case GridItemContributionType::kForMaxContentMinimums:
      return set.TrackSize().HasMaxContentOrAutoMaxTrackBreadth();
    case GridItemContributionType::kForIntrinsicMaximums:
    case GridItemContributionType::kForMaxContentMaximums:
    case GridItemContributionType::kForFreeSpace:
      return false;
  }
}

bool IsDistributionForGrowthLimits(GridItemContributionType contribution_type) {
  switch (contribution_type) {
    case GridItemContributionType::kForIntrinsicMinimums:
    case GridItemContributionType::kForContentBasedMinimums:
    case GridItemContributionType::kForMaxContentMinimums:
    case GridItemContributionType::kForFreeSpace:
      return false;
    case GridItemContributionType::kForIntrinsicMaximums:
    case GridItemContributionType::kForMaxContentMaximums:
      return true;
  }
}

enum class InfinitelyGrowableBehavior { kEnforce, kIgnore };

// We define growth potential = limit - affected size; for base sizes, the limit
// is its growth limit. For growth limits, the limit is infinity if it is marked
// as "infinitely growable", and equal to the growth limit otherwise.
LayoutUnit GrowthPotentialForSet(
    const NGGridSet& set,
    GridItemContributionType contribution_type,
    InfinitelyGrowableBehavior infinitely_growable_behavior =
        InfinitelyGrowableBehavior::kEnforce) {
  switch (contribution_type) {
    case GridItemContributionType::kForIntrinsicMinimums:
    case GridItemContributionType::kForContentBasedMinimums:
    case GridItemContributionType::kForMaxContentMinimums: {
      LayoutUnit growth_limit = set.GrowthLimit();
      return (growth_limit == kIndefiniteSize) ? kIndefiniteSize
                                               : growth_limit - set.BaseSize();
    }
    case GridItemContributionType::kForIntrinsicMaximums:
    case GridItemContributionType::kForMaxContentMaximums: {
      if (infinitely_growable_behavior ==
              InfinitelyGrowableBehavior::kEnforce &&
          !set.IsInfinitelyGrowable()) {
        // If the affected size was a growth limit and the track is not marked
        // infinitely growable, then the item-incurred increase will be zero.
        return LayoutUnit();
      }

      LayoutUnit fit_content_limit = set.FitContentLimit();
      DCHECK(fit_content_limit >= 0 || fit_content_limit == kIndefiniteSize);

      // The max track sizing function of a 'fit-content' track is treated as
      // 'max-content' until it reaches the limit specified as the 'fit-content'
      // argument, after which it is treated as having a fixed sizing function
      // of that argument (with a growth potential of zero).
      if (fit_content_limit != kIndefiniteSize) {
        LayoutUnit growth_potential =
            fit_content_limit - DefiniteGrowthLimit(set);
        return growth_potential.ClampNegativeToZero();
      }
      // Otherwise, this set has infinite growth potential.
      return kIndefiniteSize;
    }
    case GridItemContributionType::kForFreeSpace: {
      LayoutUnit growth_limit = set.GrowthLimit();
      DCHECK_NE(growth_limit, kIndefiniteSize);
      return growth_limit - set.BaseSize();
    }
  }
}

// Follow the definitions from https://drafts.csswg.org/css-grid-2/#extra-space;
// notice that this method replaces the notion of "tracks" with "sets".
void DistributeExtraSpaceToSets(
    LayoutUnit extra_space,
    bool is_equal_distribution,
    GridItemContributionType contribution_type,
    GridSetVector* sets_to_grow,
    GridSetVector* sets_to_grow_beyond_limit = nullptr) {
  DCHECK(extra_space && sets_to_grow);

  if (extra_space == kIndefiniteSize) {
    // Infinite extra space should only happen when distributing free space at
    // the maximize tracks step; in such case, we can simplify this method by
    // "filling" every track base size up to their growth limit.
    DCHECK_EQ(contribution_type, GridItemContributionType::kForFreeSpace);
    for (NGGridSet* set : *sets_to_grow) {
      set->SetItemIncurredIncrease(
          GrowthPotentialForSet(*set, contribution_type));
    }
    return;
  }

  DCHECK_GT(extra_space, 0);
#if DCHECK_IS_ON()
  if (IsDistributionForGrowthLimits(contribution_type))
    DCHECK_EQ(sets_to_grow, sets_to_grow_beyond_limit);
#endif

  auto ShareRatio =
      [&is_equal_distribution](const NGGridSet& set) -> wtf_size_t {
    if (is_equal_distribution)
      return set.TrackCount();

    // From https://drafts.csswg.org/css-grid-2/#algo-spanning-flex-items:
    //   If the sum of the flexible sizing functions of all flexible tracks
    //   spanned by the item is greater than zero, distributing space to such
    //   tracks according to the ratios of their flexible sizing functions
    //   rather than distributing space equally.
    //
    // Since we will use the flex factor to compute proportions out of a
    // |LayoutUnit|, let the class round the floating point value.
    DCHECK(set.TrackSize().HasFlexMaxTrackBreadth());
    LayoutUnit flex_factor = LayoutUnit::FromDoubleRound(
        set.TrackSize().MaxTrackBreadth().Flex() * set.TrackCount());
    return flex_factor.RawValue();
  };

  wtf_size_t share_ratio_sum = 0;
  wtf_size_t growable_track_count = 0;
  for (NGGridSet* set : *sets_to_grow) {
    set->SetItemIncurredIncrease(LayoutUnit());
    share_ratio_sum += ShareRatio(*set);

    // From the first note in https://drafts.csswg.org/css-grid-2/#extra-space:
    //   If the affected size was a growth limit and the track is not marked
    //   "infinitely growable", then each item-incurred increase will be zero.
    //
    // When distributing space to growth limits, we need to increase each track
    // up to its 'fit-content' limit. However, because of the note above, first
    // we should only grow tracks marked as "infinitely growable" up to limits
    // and then grow all affected tracks beyond limits.
    //
    // We can correctly resolve every scenario by doing a single sort of
    // |sets_to_grow|, purposely ignoring the "infinitely growable" flag, then
    // filtering out sets that won't take a share of the extra space at each
    // step; for base sizes this is not required, but if there are no tracks
    // with growth potential > 0, we can optimize by not sorting the sets.
    if (GrowthPotentialForSet(*set, contribution_type))
      growable_track_count += set->TrackCount();
  }

  // If the sum of share ratios is zero, default to distribute equally.
  if (!share_ratio_sum)
    is_equal_distribution = true;

  if (is_equal_distribution) {
    // Distribute space equally among tracks with growth potential > 0.
    share_ratio_sum = growable_track_count;
  } else {
    // Otherwise, distribute space according to their flex factors; compute
    // |share_ratio_sum| now filtering out sets with no growth potential.
    share_ratio_sum = 0;
    for (NGGridSet* set : *sets_to_grow) {
      if (GrowthPotentialForSet(*set, contribution_type))
        share_ratio_sum += ShareRatio(*set);
    }
  }

  // We will sort the tracks by growth potential in non-decreasing order to
  // distribute space up to limits; notice that if we start distributing space
  // equally among all tracks we will eventually reach the limit of a track or
  // run out of space to distribute. If the former scenario happens, it should
  // be easy to see that the group of tracks that will reach its limit first
  // will be that with the least growth potential. Otherwise, if tracks in such
  // group does not reach their limit, every upcoming track with greater growth
  // potential must be able to increase its size by the same amount.
  if (growable_track_count ||
      IsDistributionForGrowthLimits(contribution_type)) {
    auto CompareSetsByGrowthPotential = [contribution_type](NGGridSet* set_a,
                                                            NGGridSet* set_b) {
      LayoutUnit growth_potential_a = GrowthPotentialForSet(
          *set_a, contribution_type, InfinitelyGrowableBehavior::kIgnore);
      LayoutUnit growth_potential_b = GrowthPotentialForSet(
          *set_b, contribution_type, InfinitelyGrowableBehavior::kIgnore);

      if (growth_potential_a == kIndefiniteSize ||
          growth_potential_b == kIndefiniteSize) {
        // At this point we know that there is at least one set with infinite
        // growth potential; if |set_a| has a definite value, then |set_b| must
        // have infinite growth potential, and thus, |set_a| < |set_b|.
        return growth_potential_a != kIndefiniteSize;
      }
      // Straightforward comparison of definite growth potentials.
      return growth_potential_a < growth_potential_b;
    };
    std::sort(sets_to_grow->begin(), sets_to_grow->end(),
              CompareSetsByGrowthPotential);
  }

  auto ExtraSpaceShare = [&extra_space, &ShareRatio, &share_ratio_sum](
                             const NGGridSet& set,
                             LayoutUnit growth_potential) -> LayoutUnit {
    DCHECK(growth_potential >= 0 || growth_potential == kIndefiniteSize);

    // If this set won't take a share of the extra space, e.g. it has zero
    // growth potential or the remaining share ratio sum is zero, exit early so
    // that this set's share ratio is filtered out of |share_ratio_sum|.
    if (!share_ratio_sum || !growth_potential)
      return LayoutUnit();

    wtf_size_t set_share_ratio = ShareRatio(set);
    DCHECK_LE(set_share_ratio, share_ratio_sum);

    LayoutUnit extra_space_share =
        (extra_space * set_share_ratio) / share_ratio_sum;
    if (growth_potential != kIndefiniteSize)
      extra_space_share = std::min(extra_space_share, growth_potential);
    DCHECK_LE(extra_space_share, extra_space);

    share_ratio_sum -= set_share_ratio;
    extra_space -= extra_space_share;
    return extra_space_share;
  };

  // Distribute space up to limits:
  //   - For base sizes, grow the base size up to the growth limit.
  //   - For growth limits, the only case where a growth limit should grow at
  //   this step is when its set has already been marked "infinitely growable".
  //   Increase the growth limit up to the 'fit-content' argument (if any); note
  //   that these arguments could prevent this step to fulfill the entirety of
  //   the extra space and further distribution would be needed.
  for (NGGridSet* set : *sets_to_grow) {
    set->SetItemIncurredIncrease(
        ExtraSpaceShare(*set, GrowthPotentialForSet(*set, contribution_type)));
  }

  // Distribute space beyond limits:
  //   - For base sizes, every affected track can grow indefinitely.
  //   - For growth limits, grow tracks up to their 'fit-content' argument.
  if (sets_to_grow_beyond_limit && extra_space) {
#if DCHECK_IS_ON()
    // We expect |sets_to_grow_beyond_limit| to be ordered by growth potential
    // for the following section of the algorithm to work.
    //
    // For base sizes, since going beyond limits should only happen after we
    // grow every track up to their growth limits, it should be easy to see that
    // every growth potential is now zero, so they're already ordered.
    //
    // Now let's consider growth limits: we forced the sets to be sorted by
    // growth potential ignoring the "infinitely growable" flag, meaning that
    // ultimately they will be sorted by remaining space to their 'fit-content'
    // parameter (if it exists, infinite otherwise). If we ended up here, we
    // must have filled the sets marked as "infinitely growable" up to their
    // 'fit-content' parameter; therefore, if we only consider sets with
    // remaining space to their 'fit-content' limit in the following
    // distribution step, they should still be ordered.
    LayoutUnit previous_growable_potential;
    for (NGGridSet* set : *sets_to_grow_beyond_limit) {
      LayoutUnit growth_potential = GrowthPotentialForSet(
          *set, contribution_type, InfinitelyGrowableBehavior::kIgnore);
      if (growth_potential) {
        if (previous_growable_potential == kIndefiniteSize) {
          DCHECK_EQ(growth_potential, kIndefiniteSize);
        } else {
          DCHECK(growth_potential >= previous_growable_potential ||
                 growth_potential == kIndefiniteSize);
        }
        previous_growable_potential = growth_potential;
      }
    }
#endif

    auto BeyondLimitsGrowthPotential =
        [contribution_type](const NGGridSet& set) -> LayoutUnit {
      // For growth limits, ignore the "infinitely growable" flag and grow all
      // affected tracks up to their 'fit-content' argument (note that
      // |GrowthPotentialForSet| already accounts for it).
      return !IsDistributionForGrowthLimits(contribution_type)
                 ? kIndefiniteSize
                 : GrowthPotentialForSet(set, contribution_type,
                                         InfinitelyGrowableBehavior::kIgnore);
    };

    share_ratio_sum = 0;
    for (NGGridSet* set : *sets_to_grow_beyond_limit) {
      if (BeyondLimitsGrowthPotential(*set))
        share_ratio_sum += ShareRatio(*set);
    }
    for (NGGridSet* set : *sets_to_grow_beyond_limit) {
      set->SetItemIncurredIncrease(
          set->ItemIncurredIncrease() +
          ExtraSpaceShare(*set, BeyondLimitsGrowthPotential(*set)));
    }
  }
}

}  // namespace

void NGGridLayoutAlgorithm::IncreaseTrackSizesToAccommodateGridItems(
    const GridGeometry& grid_geometry,
    GridItems::Iterator group_begin,
    GridItems::Iterator group_end,
    const bool is_group_spanning_flex_track,
    GridItemContributionType contribution_type,
    NGGridLayoutAlgorithmTrackCollection* track_collection) const {
  DCHECK(track_collection);
  const GridTrackSizingDirection track_direction =
      track_collection->Direction();

  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    set_iterator.CurrentSet().SetPlannedIncrease(LayoutUnit());
  }

  GridSetVector sets_to_grow;
  GridSetVector sets_to_grow_beyond_limit;
  for (auto grid_item = group_begin; grid_item != group_end; ++grid_item) {
    // When the grid items of this group are not spanning a flexible track, we
    // can skip the current item if it doesn't span an intrinsic track.
    if (!grid_item->IsSpanningIntrinsicTrack(track_direction) &&
        !is_group_spanning_flex_track) {
      continue;
    }

    sets_to_grow.Shrink(0);
    sets_to_grow_beyond_limit.Shrink(0);

    // TODO(ansollan): If the grid is auto-sized and has a calc or percent row
    // gap, then the gap can't be calculated on the first pass as we wouldn't
    // know our block size.
    LayoutUnit spanned_tracks_size =
        GridGap(track_direction) * (grid_item->SpanSize(track_direction) - 1);

    for (auto set_iterator =
             GetSetIteratorForItem(*grid_item, *track_collection);
         !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
      NGGridSet& current_set = set_iterator.CurrentSet();
      spanned_tracks_size +=
          AffectedSizeForContribution(current_set, contribution_type);

      // From https://drafts.csswg.org/css-grid-2/#algo-spanning-flex-items:
      //   Distributing space only to flexible tracks (i.e. treating all other
      //   tracks as having a fixed sizing function).
      if (is_group_spanning_flex_track &&
          !current_set.TrackSize().HasFlexMaxTrackBreadth()) {
        continue;
      }

      if (IsContributionAppliedToSet(current_set, contribution_type)) {
        sets_to_grow.push_back(&current_set);
        if (ShouldUsedSizeGrowBeyondLimit(current_set, contribution_type))
          sets_to_grow_beyond_limit.push_back(&current_set);
      }
    }

    if (sets_to_grow.IsEmpty())
      continue;

    // Subtract the corresponding size (base size or growth limit) of every
    // spanned track from the grid item's size contribution to find the item's
    // remaining size contribution. For infinite growth limits, substitute with
    // the track's base size. This is the space to distribute, floor it at zero.
    LayoutUnit extra_space = ContributionSizeForGridItem(
        grid_geometry, *grid_item, track_direction, contribution_type);
    extra_space -= spanned_tracks_size;

    if (extra_space <= 0)
      continue;

    DistributeExtraSpaceToSets(
        extra_space.ClampNegativeToZero(),
        /* is_equal_distribution = */ !is_group_spanning_flex_track,
        contribution_type, &sets_to_grow,
        sets_to_grow_beyond_limit.IsEmpty() ? &sets_to_grow
                                            : &sets_to_grow_beyond_limit);

    // For each affected track, if the track's item-incurred increase is larger
    // than its planned increase, set the planned increase to that value.
    for (NGGridSet* set : sets_to_grow) {
      set->SetPlannedIncrease(
          std::max(set->ItemIncurredIncrease(), set->PlannedIncrease()));
    }
  }

  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    GrowAffectedSizeByPlannedIncrease(set_iterator.CurrentSet(),
                                      contribution_type);
  }
}

// https://drafts.csswg.org/css-grid-2/#algo-content
void NGGridLayoutAlgorithm::ResolveIntrinsicTrackSizes(
    const GridGeometry& grid_geometry,
    NGGridLayoutAlgorithmTrackCollection* track_collection,
    GridItems* grid_items) const {
  DCHECK(track_collection && grid_items);
  const GridTrackSizingDirection track_direction =
      track_collection->Direction();

  // Reorder grid items to process them as follows:
  //   - First, consider items spanning a single non-flexible track.
  //   - Next, consider items with span size of 2 not spanning a flexible track.
  //   - Repeat incrementally for items with greater span sizes until all items
  //   not spanning a flexible track have been considered.
  //   - Finally, consider all items spanning a flexible track.
  auto CompareGridItemsForIntrinsicTrackResolution =
      [grid_items, track_direction](wtf_size_t a, wtf_size_t b) -> bool {
    if (grid_items->item_data[a].IsSpanningFlexibleTrack(track_direction) ||
        grid_items->item_data[b].IsSpanningFlexibleTrack(track_direction)) {
      // Ignore span sizes if one of the items spans a track with a flexible
      // sizing function; items not spanning such tracks should come first.
      return !grid_items->item_data[a].IsSpanningFlexibleTrack(track_direction);
    }
    return grid_items->item_data[a].SpanSize(track_direction) <
           grid_items->item_data[b].SpanSize(track_direction);
  };
  std::sort(grid_items->reordered_item_indices.begin(),
            grid_items->reordered_item_indices.end(),
            CompareGridItemsForIntrinsicTrackResolution);

  // First, process the items that don't span a flexible track.
  auto current_group_begin = grid_items->begin();
  while (current_group_begin != grid_items->end() &&
         !current_group_begin->IsSpanningFlexibleTrack(track_direction)) {
    // Each iteration considers all items with the same span size.
    wtf_size_t current_group_span_size =
        current_group_begin->SpanSize(track_direction);

    auto current_group_end = current_group_begin;
    do {
      DCHECK(!current_group_end->IsSpanningFlexibleTrack(track_direction));
      ++current_group_end;
    } while (current_group_end != grid_items->end() &&
             !current_group_end->IsSpanningFlexibleTrack(track_direction) &&
             current_group_end->SpanSize(track_direction) ==
                 current_group_span_size);

    IncreaseTrackSizesToAccommodateGridItems(
        grid_geometry, current_group_begin, current_group_end,
        /* is_group_spanning_flex_track */ false,
        GridItemContributionType::kForIntrinsicMinimums, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        grid_geometry, current_group_begin, current_group_end,
        /* is_group_spanning_flex_track */ false,
        GridItemContributionType::kForContentBasedMinimums, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        grid_geometry, current_group_begin, current_group_end,
        /* is_group_spanning_flex_track */ false,
        GridItemContributionType::kForMaxContentMinimums, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        grid_geometry, current_group_begin, current_group_end,
        /* is_group_spanning_flex_track */ false,
        GridItemContributionType::kForIntrinsicMaximums, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        grid_geometry, current_group_begin, current_group_end,
        /* is_group_spanning_flex_track */ false,
        GridItemContributionType::kForMaxContentMaximums, track_collection);

    // Move to the next group with greater span size.
    current_group_begin = current_group_end;
  }

  // From https://drafts.csswg.org/css-grid-2/#algo-spanning-flex-items:
  //   Increase sizes to accommodate spanning items crossing flexible tracks:
  //   Next, repeat the previous step instead considering (together, rather than
  //   grouped by span size) all items that do span a track with a flexible
  //   sizing function...
#if DCHECK_IS_ON()
  // Every grid item of the remaining group should span a flexible track.
  for (auto it = current_group_begin; it != grid_items->end(); ++it)
    DCHECK(it->IsSpanningFlexibleTrack(track_direction));
#endif

  // Now, process items spanning flexible tracks (if any).
  if (current_group_begin != grid_items->end()) {
    // We can safely skip contributions for maximums since a <flex> definition
    // does not have an intrinsic max track sizing function.
    IncreaseTrackSizesToAccommodateGridItems(
        grid_geometry, current_group_begin, grid_items->end(),
        /* is_group_spanning_flex_track */ true,
        GridItemContributionType::kForIntrinsicMinimums, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        grid_geometry, current_group_begin, grid_items->end(),
        /* is_group_spanning_flex_track */ true,
        GridItemContributionType::kForContentBasedMinimums, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        grid_geometry, current_group_begin, grid_items->end(),
        /* is_group_spanning_flex_track */ true,
        GridItemContributionType::kForMaxContentMinimums, track_collection);
  }

  // If any track still has an infinite growth limit (i.e. it had no items
  // placed in it), set its growth limit to its base size.
  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    auto& set = set_iterator.CurrentSet();
    if (set.GrowthLimit() == kIndefiniteSize)
      set.SetGrowthLimit(set.BaseSize());
  }
}

// https://drafts.csswg.org/css-grid-2/#algo-grow-tracks
void NGGridLayoutAlgorithm::MaximizeTracks(
    SizingConstraint sizing_constraint,
    NGGridLayoutAlgorithmTrackCollection* track_collection) const {
  const LayoutUnit free_space =
      DetermineFreeSpace(sizing_constraint, *track_collection);
  if (!free_space)
    return;

  GridSetVector sets_to_grow;
  sets_to_grow.ReserveInitialCapacity(track_collection->SetCount());
  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    sets_to_grow.push_back(&set_iterator.CurrentSet());
  }

  DistributeExtraSpaceToSets(free_space, /* is_equal_distribution */ true,
                             GridItemContributionType::kForFreeSpace,
                             &sets_to_grow);

  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    auto& set = set_iterator.CurrentSet();
    set.SetBaseSize(set.BaseSize() + set.ItemIncurredIncrease());
  }

  // TODO(ethavar): If this would cause the grid to be larger than the grid
  // container’s inner size as limited by its 'max-width/height', then redo this
  // step, treating the available grid space as equal to the grid container’s
  // inner size when it’s sized to its 'max-width/height'.
}

// https://drafts.csswg.org/css-grid-2/#algo-stretch
void NGGridLayoutAlgorithm::StretchAutoTracks(
    SizingConstraint sizing_constraint,
    NGGridLayoutAlgorithmTrackCollection* track_collection) const {
  const GridTrackSizingDirection track_direction =
      track_collection->Direction();

  // Stretching auto tracks should only occur if we have a "stretch" (or
  // default) content distribution.
  const auto& content_alignment = (track_direction == kForColumns)
                                      ? Style().JustifyContent()
                                      : Style().AlignContent();
  bool has_stretch_distribution =
      content_alignment.Distribution() == ContentDistributionType::kStretch ||
      (content_alignment.GetPosition() == ContentPosition::kNormal &&
       content_alignment.Distribution() == ContentDistributionType::kDefault);
  if (!has_stretch_distribution)
    return;

  LayoutUnit free_space =
      DetermineFreeSpace(sizing_constraint, *track_collection);

  // If the free space is indefinite, but the grid container has a definite
  // min-width/height, use that size to calculate the free space for this step
  // instead.
  if (free_space == kIndefiniteSize) {
    free_space = (track_direction == kForColumns)
                     ? grid_min_available_size_.inline_size
                     : grid_min_available_size_.block_size;
    DCHECK_NE(free_space, kIndefiniteSize);
    free_space -= ComputeTotalTrackSize(*track_collection,
                                        GridGap(track_direction, free_space));
  }

  if (free_space <= 0)
    return;

  // Expand tracks that have an 'auto' max track sizing function by dividing any
  // remaining positive, definite free space equally amongst them.
  GridSetVector sets_to_grow;
  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    auto& set = set_iterator.CurrentSet();
    if (set.TrackSize().HasAutoMaxTrackBreadth())
      sets_to_grow.push_back(&set);
  }

  if (sets_to_grow.IsEmpty())
    return;

  DistributeExtraSpaceToSets(free_space, /* is_equal_distribution */ true,
                             GridItemContributionType::kForFreeSpace,
                             &sets_to_grow, &sets_to_grow);

  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    auto& set = set_iterator.CurrentSet();
    set.SetBaseSize(set.BaseSize() + set.ItemIncurredIncrease());
  }
}

namespace {

// Contains the information about where the grid tracks start, and the
// gutter-size between them, taking into account the content alignment
// properties.
struct TrackAlignmentGeometry {
  LayoutUnit start_offset;
  LayoutUnit gutter_size;
};

TrackAlignmentGeometry ComputeTrackAlignmentGeometry(
    const ComputedStyle& style,
    const StyleContentAlignmentData& content_alignment,
    const NGGridLayoutAlgorithmTrackCollection& track_collection,
    LayoutUnit available_size,
    LayoutUnit start_border_scrollbar_padding,
    LayoutUnit grid_gap) {
  // Determining the free-space is typically unnecessary, i.e. if there is
  // default alignment. Only compute this on-demand.
  auto FreeSpace = [&track_collection, &available_size,
                    &grid_gap]() -> LayoutUnit {
    return available_size - ComputeTotalTrackSize(track_collection, grid_gap);
  };

  // The default alignment, perform adjustments on top of this.
  TrackAlignmentGeometry geometry = {start_border_scrollbar_padding, grid_gap};

  // If we have an indefinite |available_size| we can't perform any alignment,
  // just return the default alignment.
  if (available_size == kIndefiniteSize)
    return geometry;

  // TODO(ikilpatrick): 'space-between', 'space-around', and 'space-evenly' all
  // divide by the free-space, and may have a non-zero modulo. Investigate if
  // this should be distributed between the tracks.
  switch (content_alignment.Distribution()) {
    case ContentDistributionType::kSpaceBetween: {
      // Default behavior for 'space-between' is to start align content.
      const wtf_size_t track_count = track_collection.EndLineOfImplicitGrid();
      const LayoutUnit free_space = FreeSpace();
      if (track_count < 2 || free_space < LayoutUnit())
        return geometry;

      geometry.gutter_size += free_space / (track_count - 1);
      return geometry;
    }
    case ContentDistributionType::kSpaceAround: {
      // Default behaviour for 'space-around' is to center content.
      const wtf_size_t track_count = track_collection.EndLineOfImplicitGrid();
      const LayoutUnit free_space = FreeSpace();
      if (track_count < 1 || free_space < LayoutUnit()) {
        geometry.start_offset += free_space / 2;
        return geometry;
      }

      LayoutUnit track_space = free_space / track_count;
      geometry.start_offset += track_space / 2;
      geometry.gutter_size += track_space;
      return geometry;
    }
    case ContentDistributionType::kSpaceEvenly: {
      // Default behaviour for 'space-evenly' is to center content.
      const wtf_size_t track_count = track_collection.EndLineOfImplicitGrid();
      const LayoutUnit free_space = FreeSpace();
      if (free_space < LayoutUnit()) {
        geometry.start_offset += free_space / 2;
        return geometry;
      }

      LayoutUnit track_space = free_space / (track_count + 1);
      geometry.start_offset += track_space;
      geometry.gutter_size += track_space;
      return geometry;
    }
    case ContentDistributionType::kStretch:
    case ContentDistributionType::kDefault:
      break;
  }

  switch (content_alignment.GetPosition()) {
    case ContentPosition::kLeft: {
      DCHECK(track_collection.IsForColumns());
      if (IsLtr(style.Direction()))
        return geometry;

      geometry.start_offset += FreeSpace();
      return geometry;
    }
    case ContentPosition::kRight: {
      DCHECK(track_collection.IsForColumns());
      if (IsRtl(style.Direction()))
        return geometry;

      geometry.start_offset += FreeSpace();
      return geometry;
      break;
    }
    case ContentPosition::kCenter: {
      geometry.start_offset += FreeSpace() / 2;
      return geometry;
    }
    case ContentPosition::kEnd:
    case ContentPosition::kFlexEnd: {
      geometry.start_offset += FreeSpace();
      return geometry;
    }
    case ContentPosition::kStart:
    case ContentPosition::kFlexStart:
    case ContentPosition::kNormal:
    case ContentPosition::kBaseline:
    case ContentPosition::kLastBaseline:
      return geometry;
  }
}

}  // namespace

// Calculates the offsets for all sets.
NGGridLayoutAlgorithm::SetGeometry NGGridLayoutAlgorithm::ComputeSetGeometry(
    const NGGridLayoutAlgorithmTrackCollection& track_collection,
    const LayoutUnit available_size) const {
  const TrackAlignmentGeometry track_alignment_geometry =
      track_collection.IsForColumns()
          ? ComputeTrackAlignmentGeometry(Style(), Style().JustifyContent(),
                                          track_collection, available_size,
                                          BorderScrollbarPadding().inline_start,
                                          GridGap(kForColumns, available_size))
          : ComputeTrackAlignmentGeometry(Style(), Style().AlignContent(),
                                          track_collection, available_size,
                                          BorderScrollbarPadding().block_start,
                                          GridGap(kForRows, available_size));

  LayoutUnit set_offset = track_alignment_geometry.start_offset;
  Vector<SetOffsetData> sets;
  sets.ReserveInitialCapacity(track_collection.SetCount() + 1);
  sets.emplace_back(set_offset, /* last_indefinite_index */ kNotFound);

  for (auto set_iterator = track_collection.GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    const auto& set = set_iterator.CurrentSet();
    set_offset += set.BaseSize() +
                  set.TrackCount() * track_alignment_geometry.gutter_size;
    sets.emplace_back(set_offset, /* last_indefinite_index */ kNotFound);
  }
  return {sets, track_alignment_geometry.gutter_size};
}

LayoutUnit NGGridLayoutAlgorithm::GridGap(
    GridTrackSizingDirection track_direction,
    LayoutUnit available_size) const {
  const base::Optional<Length>& gap =
      track_direction == kForColumns ? Style().ColumnGap() : Style().RowGap();

  if (!gap)
    return LayoutUnit();

  // TODO(ansollan): Update behavior based on outcome of working group
  // discussions. See https://github.com/w3c/csswg-drafts/issues/5566.
  if (available_size == kIndefiniteSize)
    available_size = LayoutUnit();

  return MinimumValueForLength(*gap, available_size);
}

// TODO(ikilpatrick): Determine if other uses of this method need to respect
// |grid_min_available_size_| similar to |StretchAutoTracks|.
LayoutUnit NGGridLayoutAlgorithm::DetermineFreeSpace(
    SizingConstraint sizing_constraint,
    const NGGridLayoutAlgorithmTrackCollection& track_collection) const {
  switch (sizing_constraint) {
    case SizingConstraint::kLayout: {
      const GridTrackSizingDirection track_direction =
          track_collection.Direction();
      LayoutUnit free_space = (track_direction == kForColumns)
                                  ? grid_available_size_.inline_size
                                  : grid_available_size_.block_size;
      if (free_space != kIndefiniteSize) {
        free_space -= ComputeTotalTrackSize(
            track_collection, GridGap(track_direction, free_space));
        // If tracks consume more space than the grid container has available,
        // clamp the free space to zero as there's no more room left to grow.
        free_space = free_space.ClampNegativeToZero();
      }
      return free_space;
    }
    case SizingConstraint::kMaxContent:
      // If sizing under a max-content constraint, the free space is infinite.
      return kIndefiniteSize;
    case SizingConstraint::kMinContent:
      // If sizing under a min-content constraint, the free space is zero.
      return LayoutUnit();
  }
}

namespace {

// Returns the alignment offset for either the inline or block direction.
LayoutUnit AlignmentOffset(LayoutUnit container_size,
                           LayoutUnit size,
                           LayoutUnit margin_start,
                           LayoutUnit margin_end,
                           AxisEdge axis_edge) {
  switch (axis_edge) {
    case AxisEdge::kStart:
      return margin_start;
    case AxisEdge::kCenter:
      return (container_size - size + margin_start - margin_end) / 2;
    case AxisEdge::kEnd:
      return container_size - margin_end - size;
    case AxisEdge::kBaseline:
      // TODO(kschmi): Implement baseline alignment.
      return margin_start;
  }
  NOTREACHED();
  return LayoutUnit();
}

void AlignmentOffsetForOutOfFlow(
    const AxisEdge inline_axis_edge,
    const AxisEdge block_axis_edge,
    const LogicalSize container_size,
    NGLogicalStaticPosition::InlineEdge* inline_edge,
    NGLogicalStaticPosition::BlockEdge* block_edge,
    LogicalOffset* offset) {
  using InlineEdge = NGLogicalStaticPosition::InlineEdge;
  using BlockEdge = NGLogicalStaticPosition::BlockEdge;

  switch (inline_axis_edge) {
    case AxisEdge::kStart:
      *inline_edge = InlineEdge::kInlineStart;
      break;
    case AxisEdge::kCenter:
      *inline_edge = InlineEdge::kInlineCenter;
      offset->inline_offset += container_size.inline_size / 2;
      break;
    default:
      *inline_edge = InlineEdge::kInlineEnd;
      offset->inline_offset += container_size.inline_size;
      break;
  }

  switch (block_axis_edge) {
    case AxisEdge::kStart:
      *block_edge = BlockEdge::kBlockStart;
      break;
    case AxisEdge::kCenter:
      *block_edge = BlockEdge::kBlockCenter;
      offset->block_offset += container_size.block_size / 2;
      break;
    default:
      *block_edge = BlockEdge::kBlockEnd;
      offset->block_offset += container_size.block_size;
      break;
  }
}

}  // namespace

const NGConstraintSpace NGGridLayoutAlgorithm::CreateConstraintSpace(
    const GridGeometry& grid_geometry,
    const GridItemData& grid_item,
    NGCacheSlot cache_slot,
    LogicalRect* rect) const {
  DCHECK(rect);

  ComputeOffsetAndSize(grid_item, grid_geometry.column_geometry, kForColumns,
                       kIndefiniteSize, &rect->offset.inline_offset,
                       &rect->size.inline_size);
  ComputeOffsetAndSize(grid_item, grid_geometry.row_geometry, kForRows,
                       kIndefiniteSize, &rect->offset.block_offset,
                       &rect->size.block_size);

  NGConstraintSpaceBuilder builder(ConstraintSpace(),
                                   grid_item.node.Style().GetWritingDirection(),
                                   /* is_new_fc */ true);
  SetOrthogonalFallbackInlineSizeIfNeeded(Style(), grid_item.node, &builder);
  builder.SetCacheSlot(cache_slot);
  builder.SetIsPaintedAtomically(true);
  builder.SetAvailableSize(rect->size);
  builder.SetPercentageResolutionSize(rect->size);
  builder.SetStretchInlineSizeIfAuto(grid_item.is_inline_axis_stretched &&
                                     rect->size.inline_size != kIndefiniteSize);
  builder.SetStretchBlockSizeIfAuto(grid_item.is_block_axis_stretched &&
                                    rect->size.block_size != kIndefiniteSize);
  return builder.ToConstraintSpace();
}

void NGGridLayoutAlgorithm::PlaceGridItems(const GridItems& grid_items,
                                           const GridGeometry& grid_geometry,
                                           LayoutUnit block_size) {
  // |grid_items| is in DOM order to ensure proper painting order, but
  // determining the grid's baseline is prioritized based on grid order. The
  // baseline of the grid is determined by the first grid item with baseline
  // alignment in the first row. If no items have baseline alignment, fall back
  // to the first item in row-major order.
  struct PositionAndBaseline {
    PositionAndBaseline(const GridArea& resolved_position, LayoutUnit baseline)
        : resolved_position(resolved_position), baseline(baseline) {}
    GridArea resolved_position;
    LayoutUnit baseline;
  };
  base::Optional<PositionAndBaseline> alignment_baseline;
  base::Optional<PositionAndBaseline> fallback_baseline;

  for (const auto& grid_item : grid_items.item_data) {
    DCHECK(grid_item.column_set_indices.has_value());
    DCHECK(grid_item.row_set_indices.has_value());

    LogicalRect containing_grid_area;
    const NGConstraintSpace space = CreateConstraintSpace(
        grid_geometry, grid_item, NGCacheSlot::kLayout, &containing_grid_area);

    scoped_refptr<const NGLayoutResult> result = grid_item.node.Layout(space);
    const auto& physical_fragment =
        To<NGPhysicalBoxFragment>(result->PhysicalFragment());

    const auto margins =
        ComputeMarginsFor(space, grid_item.node.Style(), ConstraintSpace());

    // Apply the grid-item's alignment (if any).
    NGBoxFragment fragment(ConstraintSpace().GetWritingDirection(),
                           physical_fragment);
    containing_grid_area.offset += LogicalOffset(
        AlignmentOffset(containing_grid_area.size.inline_size,
                        fragment.InlineSize(), margins.inline_start,
                        margins.inline_end, grid_item.inline_axis_alignment),
        AlignmentOffset(containing_grid_area.size.block_size,
                        fragment.BlockSize(), margins.block_start,
                        margins.block_end, grid_item.block_axis_alignment));

    container_builder_.AddChild(physical_fragment, containing_grid_area.offset);
    NGBlockNode(grid_item.node).StoreMargins(ConstraintSpace(), margins);

    // Compares GridArea objects in row-major grid order for baseline
    // precedence. Returns 'true' if |a| < |b| and 'false' otherwise.
    auto IsBeforeInGridOrder = [&](const GridArea& a,
                                   const GridArea& b) -> bool {
      return (a.rows < b.rows) || (a.rows == b.rows && (a.columns < b.columns));
    };

    LayoutUnit baseline = fragment.BaselineOrSynthesize() +
                          containing_grid_area.offset.block_offset;
    if (grid_item.block_axis_alignment == AxisEdge::kBaseline) {
      if (!alignment_baseline ||
          IsBeforeInGridOrder(grid_item.resolved_position,
                              alignment_baseline->resolved_position)) {
        alignment_baseline.emplace(grid_item.resolved_position, baseline);
      }
    } else if (!fallback_baseline ||
               IsBeforeInGridOrder(grid_item.resolved_position,
                                   fallback_baseline->resolved_position)) {
      fallback_baseline.emplace(grid_item.resolved_position, baseline);
    }
  }

  // Propagate the baseline from the appropriate child.
  // TODO(kschmi): Synthesize baseline from alignment context if no grid items.
  if (!grid_items.IsEmpty()) {
    if (alignment_baseline &&
        (!fallback_baseline || alignment_baseline->resolved_position.rows <=
                                   fallback_baseline->resolved_position.rows)) {
      container_builder_.SetBaseline(alignment_baseline->baseline);
    } else {
      DCHECK(fallback_baseline);
      container_builder_.SetBaseline(fallback_baseline->baseline);
    }
  }
}

void NGGridLayoutAlgorithm::PlaceOutOfFlowItems(
    const Vector<GridItemData>& out_of_flow_items,
    const GridGeometry& grid_geometry,
    LayoutUnit block_size) {
  for (const GridItemData& out_of_flow_item : out_of_flow_items) {
    DCHECK(out_of_flow_item.column_set_indices.has_value());
    DCHECK(out_of_flow_item.row_set_indices.has_value());

    LogicalRect containing_block_rect = ComputeContainingGridAreaRect(
        grid_geometry, out_of_flow_item, block_size);
    NGLogicalStaticPosition::InlineEdge inline_edge;
    NGLogicalStaticPosition::BlockEdge block_edge;
    LogicalOffset child_offset = containing_block_rect.offset;
    AlignmentOffsetForOutOfFlow(out_of_flow_item.inline_axis_alignment,
                                out_of_flow_item.block_axis_alignment,
                                containing_block_rect.size, &inline_edge,
                                &block_edge, &child_offset);

    container_builder_.AddOutOfFlowChildCandidate(
        out_of_flow_item.node, child_offset, inline_edge, block_edge,
        /* needs_block_offset_adjustment */ false, containing_block_rect);
  }
}

void NGGridLayoutAlgorithm::PlaceOutOfFlowDescendants(
    const NGGridLayoutAlgorithmTrackCollection& column_track_collection,
    const NGGridLayoutAlgorithmTrackCollection& row_track_collection,
    const GridGeometry& grid_geometry,
    const NGGridPlacement& grid_placement,
    LayoutUnit block_size) {
  // At this point, we'll have a list of OOF candidates from any inflow children
  // of the grid (which have been propagated up). These might have an assigned
  // 'grid-area', so we need to assign their correct 'containing block rect'.
  Vector<NGLogicalOutOfFlowPositionedNode>* out_of_flow_descendants =
      container_builder_.MutableOutOfFlowPositionedCandidates();
  DCHECK(out_of_flow_descendants);

  for (auto& out_of_flow_descendant : *out_of_flow_descendants) {
    // TODO(ansollan): We don't need all parameters from |GridItemData| for out
    // of flow items. Implement a reduced version in |MeasureGridItem| or only
    // fill what is needed here.
    GridItemData out_of_flow_item =
        MeasureGridItem(out_of_flow_descendant.node);

    out_of_flow_item.SetIndices(column_track_collection, &grid_placement);
    out_of_flow_item.SetIndices(row_track_collection, &grid_placement);

    out_of_flow_descendant.containing_block_rect =
        ComputeContainingGridAreaRect(grid_geometry, out_of_flow_item,
                                      block_size);
  }
}

LogicalRect NGGridLayoutAlgorithm::ComputeContainingGridAreaRect(
    const GridGeometry& grid_geometry,
    const GridItemData& item,
    LayoutUnit block_size) {
  LogicalRect rect;
  ComputeOffsetAndSize(item, grid_geometry.column_geometry, kForColumns,
                       block_size, &rect.offset.inline_offset,
                       &rect.size.inline_size);
  ComputeOffsetAndSize(item, grid_geometry.row_geometry, kForRows, block_size,
                       &rect.offset.block_offset, &rect.size.block_size);
  return rect;
}

void NGGridLayoutAlgorithm::ComputeOffsetAndSize(
    const GridItemData& item,
    const SetGeometry& set_geometry,
    const GridTrackSizingDirection track_direction,
    LayoutUnit block_size,
    LayoutUnit* start_offset,
    LayoutUnit* size) const {
  wtf_size_t start_index, end_index;
  LayoutUnit border;
  // The default padding box value of the |size| will only be used in out of
  // flow items in which both the start line and end line are defined as 'auto'.
  if (track_direction == kForColumns) {
    start_index = item.column_set_indices->begin;
    end_index = item.column_set_indices->end;
    border = container_builder_.Borders().inline_start;
    *size =
        border_box_size_.inline_size - container_builder_.Borders().InlineSum();
  } else {
    start_index = item.row_set_indices->begin;
    end_index = item.row_set_indices->end;
    border = container_builder_.Borders().block_start;
    *size = border_box_size_.block_size == kIndefiniteSize
                ? block_size
                : border_box_size_.block_size;
    *size -= container_builder_.Borders().BlockSum();
  }
  *start_offset = border;
  LayoutUnit end_offset = border;
  // If the start line is defined, the size is calculated by subtracting the
  // offset at start index. Additionally, the start border is removed from the
  // cumulated offset because it was already accounted for in the previous value
  // of the size.
  if (start_index != kNotFound) {
    *start_offset = set_geometry.sets[start_index].offset;
    *size -= (*start_offset - end_offset);
  }
  // If the end line is defined, the offset (which can be the offset at the
  // start index or the start border) and the added grid gap after the spanned
  // tracks are subtracted from the offset at the end index.
  if (end_index != kNotFound) {
    // If we are measuring a grid-item we might not yet have determined the
    // final (used) sizes for all of out sets. |last_indefinite_index| is used
    // to track what sets have indefinite/definite sizes.
    //
    // |last_indefinite_index| is the last set seen which was indefinite. If
    // our |start_index| is greater than this, all the sets between this and
    // our |end_index| are definite.
    const wtf_size_t last_indefinite_index =
        set_geometry.sets[end_index].last_indefinite_index;
    end_offset = set_geometry.sets[end_index].offset;
    if (last_indefinite_index == kNotFound ||
        start_index > last_indefinite_index) {
      *size = end_offset - *start_offset - set_geometry.gutter_size;
    } else {
      *size = kIndefiniteSize;
    }
  }

#if DCHECK_IS_ON()
  if (start_index != kNotFound && end_index != kNotFound) {
    DCHECK_LT(start_index, end_index);
    DCHECK_LT(end_index, set_geometry.sets.size());
    DCHECK(*size >= 0 || *size == kIndefiniteSize);
  } else {
    // Only out of flow items can have an undefined ('auto') value for the start
    // and/or end |set_indices|.
    DCHECK_EQ(item.item_type, ItemType::kOutOfFlow);
  }
#endif
}
}  // namespace blink
