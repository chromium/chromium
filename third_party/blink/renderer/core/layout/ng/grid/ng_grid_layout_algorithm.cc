// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_placement.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_properties.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_disable_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

NGGridLayoutAlgorithm::NGGridLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());

  border_box_size_ = container_builder_.InitialBorderBoxSize();

  // At various stages of this algorithm we need to know if the grid
  // available-size. If it is initially indefinite, we need to know the min/max
  // sizes as well. Initialize all these to the same value.
  grid_available_size_ = grid_min_available_size_ = grid_max_available_size_ =
      ChildAvailableSize();

  // Firstly if block-size containment applies compute the block-size ignoring
  // children (just based on the row definitions).
  if (grid_available_size_.block_size == kIndefiniteSize &&
      Node().ShouldApplyBlockSizeContainment()) {
    // We always need a definite min block-size in order to run the track
    // sizing algorithm.
    grid_min_available_size_.block_size = BorderScrollbarPadding().BlockSum();
    contain_intrinsic_block_size_ = ComputeIntrinsicBlockSizeIgnoringChildren();

    // Resolve the block-size, and set the available sizes.
    const LayoutUnit block_size = ComputeBlockSizeForFragment(
        ConstraintSpace(), Style(), BorderPadding(),
        *contain_intrinsic_block_size_, border_box_size_.inline_size);

    grid_available_size_.block_size = grid_min_available_size_.block_size =
        grid_max_available_size_.block_size =
            (block_size - BorderScrollbarPadding().BlockSum())
                .ClampNegativeToZero();
  }

  // Next if our inline-size is indefinite, compute the min/max inline-sizes.
  if (grid_available_size_.inline_size == kIndefiniteSize) {
    const LayoutUnit border_scrollbar_padding =
        BorderScrollbarPadding().InlineSum();
    const MinMaxSizes sizes = ComputeMinMaxInlineSizes(
        ConstraintSpace(), Node(), container_builder_.BorderPadding(),
        [&border_scrollbar_padding](MinMaxSizesType) -> MinMaxSizesResult {
          // If we've reached here we are inside the ComputeMinMaxSizes pass,
          // and also have something like "min-width: min-content". This is
          // cyclic. Just return the border/scrollbar/padding as our
          // "intrinsic" size.
          return MinMaxSizesResult(
              {border_scrollbar_padding, border_scrollbar_padding},
              /* depends_on_block_constraints */ false);
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
        ConstraintSpace(), Style(), container_builder_.BorderPadding());

    grid_min_available_size_.block_size =
        (sizes.min_size - border_scrollbar_padding).ClampNegativeToZero();
    grid_max_available_size_.block_size =
        (sizes.max_size == LayoutUnit::Max())
            ? sizes.max_size
            : (sizes.max_size - border_scrollbar_padding).ClampNegativeToZero();
  }
}

namespace {

LayoutUnit ComputeSetSpanSize(const SetGeometry& set_geometry,
                              const GridItemIndices& set_indices) {
  DCHECK_LT(set_indices.end, set_geometry.sets.size());
  DCHECK_LE(set_indices.begin, set_indices.end);

  const LayoutUnit start_offset = set_geometry.sets[set_indices.begin].offset;
  const LayoutUnit end_offset = set_geometry.sets[set_indices.end].offset;
  DCHECK_GE(end_offset, start_offset);

  // The size of a set span is the end offset minus the start offset and gutter
  // size. It is floored at zero so that the size is not negative when the
  // gutter size is greater than the difference between the offsets.
  return (end_offset - start_offset - set_geometry.gutter_size)
      .ClampNegativeToZero();
}

bool HasBlockSizeDependentGridItem(const GridItems& grid_items) {
  for (const auto& grid_item : grid_items.item_data) {
    if (grid_item.is_sizing_dependent_on_block_size)
      return true;
  }
  return false;
}

bool MayChangeOrthogonalItemContributions(const GridItems& grid_items,
                                          const SetGeometry& old_row_geometry,
                                          const SetGeometry& new_row_geometry) {
  auto GridItemRowSpanSize = [](const GridItemData& grid_item,
                                const SetGeometry& set_geometry) -> LayoutUnit {
    const auto& set_indices = grid_item.SetIndices(kForRows);
    const wtf_size_t last_indefinite_index =
        set_geometry.last_indefinite_indices.IsEmpty()
            ? kNotFound
            : set_geometry.last_indefinite_indices[set_indices.end];

    return (last_indefinite_index == kNotFound ||
            last_indefinite_index < set_indices.begin)
               ? ComputeSetSpanSize(set_geometry, set_indices)
               : kIndefiniteSize;
  };

  for (const auto& grid_item : grid_items.item_data) {
    if (grid_item.is_sizing_dependent_on_block_size) {
      const auto old_size = GridItemRowSpanSize(grid_item, old_row_geometry);
      const auto new_size = GridItemRowSpanSize(grid_item, new_row_geometry);

      DCHECK_NE(new_size, kIndefiniteSize);
      if (old_size != new_size)
        return true;
    }
  }
  return false;
}

NGGridLayoutData BundleLayoutData(
    const NGGridLayoutAlgorithmTrackCollection& column_track_collection,
    const NGGridLayoutAlgorithmTrackCollection& row_track_collection,
    const NGGridGeometry& grid_geometry) {
  NGGridLayoutData layout_data;

  layout_data.column_geometry.gutter_size =
      grid_geometry.column_geometry.gutter_size;
  layout_data.column_geometry.track_count =
      column_track_collection.NonCollapsedTrackCount();
  layout_data.column_geometry.sets = grid_geometry.column_geometry.sets;
  layout_data.column_geometry.ranges = column_track_collection.Ranges();

  layout_data.row_geometry.gutter_size = grid_geometry.row_geometry.gutter_size;
  layout_data.row_geometry.track_count =
      row_track_collection.NonCollapsedTrackCount();
  layout_data.row_geometry.sets = grid_geometry.row_geometry.sets;
  layout_data.row_geometry.ranges = row_track_collection.Ranges();

  return layout_data;
}

NGGridProperties InitializeGridProperties(
    const GridItems& grid_items,
    const WritingMode container_writing_mode) {
  NGGridProperties grid_properties;

  for (const auto& grid_item : grid_items.item_data) {
    grid_properties.has_baseline_column |=
        grid_item.IsBaselineSpecifiedForDirection(kForColumns);
    grid_properties.has_baseline_row |=
        grid_item.IsBaselineSpecifiedForDirection(kForRows);
    grid_properties.has_orthogonal_item |= !IsParallelWritingMode(
        container_writing_mode, grid_item.node.Style().GetWritingMode());
  }
  return grid_properties;
}

void CacheGridTrackSpanProperties(
    const NGGridLayoutAlgorithmTrackCollection& track_collection,
    GridItems* grid_items,
    NGGridProperties* grid_properties = nullptr) {
  DCHECK(grid_items);
  const auto track_direction = track_collection.Direction();

  if (grid_properties) {
    auto& track_properties = (track_direction == kForColumns)
                                 ? grid_properties->column_properties
                                 : grid_properties->row_properties;

    auto CacheGridProperty =
        [&](const TrackSpanProperties::PropertyId property) {
          for (const auto& range : track_collection.Ranges()) {
            if (range.properties.HasProperty(property)) {
              track_properties.SetProperty(property);
              break;
            }
          }
        };

    track_properties.Reset();
    CacheGridProperty(TrackSpanProperties::kHasFlexibleTrack);
    CacheGridProperty(TrackSpanProperties::kHasIntrinsicTrack);
    CacheGridProperty(TrackSpanProperties::kHasNonDefiniteTrack);
    CacheGridProperty(TrackSpanProperties::kIsDependentOnAvailableSize);
  }

  auto CacheTrackSpanProperty =
      [&](GridItemData& grid_item, const wtf_size_t range_index,
          const TrackSpanProperties::PropertyId property) {
        if (track_collection.RangeHasTrackSpanProperty(range_index, property))
          grid_item.SetTrackSpanProperty(property, track_direction);
      };

  GridItemVector grid_items_spanning_multiple_ranges;
  for (auto& grid_item : grid_items->item_data) {
    GridItemIndices range_indices = grid_item.RangeIndices(track_direction);

    // If a grid item spans only one range, then we can just cache the track
    // span properties directly. On the contrary, if a grid item spans multiple
    // tracks, it is added to |grid_items_spanning_multiple_ranges| as we need
    // to do more work to cache its track span properties.
    // TODO(layout-dev): Investigate applying this concept to spans > 1.
    if (range_indices.begin == range_indices.end) {
      CacheTrackSpanProperty(grid_item, range_indices.begin,
                             TrackSpanProperties::kHasFlexibleTrack);
      CacheTrackSpanProperty(grid_item, range_indices.begin,
                             TrackSpanProperties::kHasIntrinsicTrack);
      CacheTrackSpanProperty(grid_item, range_indices.begin,
                             TrackSpanProperties::kHasAutoMinimumTrack);
      CacheTrackSpanProperty(grid_item, range_indices.begin,
                             TrackSpanProperties::kHasFixedMinimumTrack);
      CacheTrackSpanProperty(grid_item, range_indices.begin,
                             TrackSpanProperties::kHasFixedMaximumTrack);
    } else {
      grid_items_spanning_multiple_ranges.emplace_back(&grid_item);
    }
  }

  if (grid_items_spanning_multiple_ranges.IsEmpty())
    return;

  auto CompareGridItemsByStartLine = [track_direction](
                                         const GridItemData* lhs,
                                         const GridItemData* rhs) -> bool {
    return lhs->StartLine(track_direction) < rhs->StartLine(track_direction);
  };
  std::sort(grid_items_spanning_multiple_ranges.begin(),
            grid_items_spanning_multiple_ranges.end(),
            CompareGridItemsByStartLine);

  auto CacheTrackSpanPropertyForAllGridItems =
      [&](const TrackSpanProperties::PropertyId property) {
        // At this point we have the remaining grid items sorted by start line
        // in the respective direction; this is important since we'll process
        // both, the ranges in the track collection and the grid items,
        // incrementally.
        auto range_iterator = track_collection.RangeIterator();

        for (auto* grid_item : grid_items_spanning_multiple_ranges) {
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
                      grid_item->StartLine(track_direction) ||
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
              grid_item->EndLine(track_direction)) {
            grid_item->SetTrackSpanProperty(property, track_direction);
          }
        }
      };

  CacheTrackSpanPropertyForAllGridItems(TrackSpanProperties::kHasFlexibleTrack);
  CacheTrackSpanPropertyForAllGridItems(
      TrackSpanProperties::kHasIntrinsicTrack);
  CacheTrackSpanPropertyForAllGridItems(
      TrackSpanProperties::kHasAutoMinimumTrack);
  CacheTrackSpanPropertyForAllGridItems(
      TrackSpanProperties::kHasFixedMinimumTrack);
  CacheTrackSpanPropertyForAllGridItems(
      TrackSpanProperties::kHasFixedMaximumTrack);
}

}  // namespace

scoped_refptr<const NGLayoutResult> NGGridLayoutAlgorithm::Layout() {
  const auto result = LayoutInternal();
  if (result->Status() == NGLayoutResult::kDisableFragmentation) {
    DCHECK(ConstraintSpace().HasBlockFragmentation());
    return RelayoutWithoutFragmentation<NGGridLayoutAlgorithm>();
  }
  return result;
}

scoped_refptr<const NGLayoutResult> NGGridLayoutAlgorithm::LayoutInternal() {
  PaintLayerScrollableArea::DelayScrollOffsetClampScope delay_clamp_scope;
  const auto& container_style = Style();

  // Measure items.
  GridItems grid_items;
  NGGridPlacement grid_placement(container_style,
                                 ComputeAutomaticRepetitions(kForColumns),
                                 ComputeAutomaticRepetitions(kForRows));
  ConstructAndAppendGridItems(&grid_items, &grid_placement);

  // Build block track collections.
  NGGridBlockTrackCollection column_block_track_collection(kForColumns);
  NGGridBlockTrackCollection row_block_track_collection(kForRows);
  BuildBlockTrackCollections(grid_placement, &grid_items,
                             &column_block_track_collection,
                             &row_block_track_collection);

  NGGridLayoutAlgorithmTrackCollection column_track_collection;
  NGGridLayoutAlgorithmTrackCollection row_track_collection;
  LayoutUnit intrinsic_block_size;
  NGGridGeometry grid_geometry;

  if (IsResumingLayout(BreakToken())) {
    // TODO(layout-dev): When we support variable inline-size fragments we'll
    // need to re-run |ComputeGridGeometry| for the different inline-size.
    // When doing this, we'll need to make sure that we don't recalculate the
    // automatic repetitions (this depends on available size), as this might
    // change the grid structure significantly (e.g. pull a child up into the
    // first row).
    const auto& grid_data = BreakToken()->GridData();
    intrinsic_block_size = grid_data.intrinsic_block_size;
    grid_geometry = grid_data.grid_geometry;

    column_track_collection = NGGridLayoutAlgorithmTrackCollection(
        column_block_track_collection,
        /* is_available_size_indefinite */ false);
    row_track_collection = NGGridLayoutAlgorithmTrackCollection(
        row_block_track_collection, /* is_available_size_indefinite */ false);

    CacheGridTrackSpanProperties(column_track_collection, &grid_items);
    CacheGridTrackSpanProperties(row_track_collection, &grid_items);

    // Cache set indices for grid items.
    for (auto& grid_item : grid_items.item_data) {
      grid_item.ComputeSetIndices(column_track_collection);
      grid_item.ComputeSetIndices(row_track_collection);
    }
  } else {
    grid_geometry = ComputeGridGeometry(
        column_block_track_collection, row_block_track_collection, &grid_items,
        &intrinsic_block_size, &column_track_collection, &row_track_collection);
  }

  const auto& constraint_space = ConstraintSpace();
  if (UNLIKELY(InvolvedInBlockFragmentation(container_builder_))) {
    // Either retrieve all items offsets, or generate them using the
    // non-fragmented |PlaceGridItems| pass.
    Vector<GridItemOffsets> offsets;
    Vector<LayoutUnit> row_offset_adjustments;
    Vector<EBreakBetween> row_break_between;
    if (IsResumingLayout(BreakToken())) {
      offsets = BreakToken()->GridData().offsets;
      row_offset_adjustments = BreakToken()->GridData().row_offset_adjustments;
      row_break_between = BreakToken()->GridData().row_break_between;
    } else {
      row_offset_adjustments =
          Vector<LayoutUnit>(grid_geometry.row_geometry.sets.size());
      row_break_between = Vector<EBreakBetween>(
          grid_geometry.row_geometry.sets.size(), EBreakBetween::kAuto);
      PlaceGridItems(grid_items, grid_geometry, &offsets, &row_break_between);
    }

    PlaceGridItemsForFragmentation(
        grid_items, row_break_between, &grid_geometry, &offsets,
        &row_offset_adjustments, &intrinsic_block_size);

    container_builder_.SetGridBreakTokenData(
        std::make_unique<NGGridBreakTokenData>(
            grid_geometry, offsets, row_offset_adjustments, row_break_between,
            intrinsic_block_size));
  } else {
    PlaceGridItems(grid_items, grid_geometry);
  }

  auto layout_data = std::make_unique<NGGridLayoutData>(BundleLayoutData(
      column_track_collection, row_track_collection, grid_geometry));

  const auto& border_padding = BorderPadding();
  const LayoutUnit block_size = ComputeBlockSizeForFragment(
      constraint_space, container_style, border_padding, intrinsic_block_size,
      border_box_size_.inline_size);

  PlaceOutOfFlowItems(*layout_data, block_size);

  // Store layout data for use in computed style and devtools.
  container_builder_.TransferGridLayoutData(std::move(layout_data));

  // For scrollable overflow purposes grid is unique in that the "inflow-bounds"
  // are the size of the grid, and *not* where the inflow grid-items are placed.
  // Explicitly set the inflow-bounds to the grid size.
  const auto& node = Node();
  if (node.IsScrollContainer()) {
    LogicalRect inflow_bounds;
    inflow_bounds.offset = {grid_geometry.column_geometry.sets.front().offset,
                            grid_geometry.row_geometry.sets.front().offset};

    inflow_bounds.size = {grid_geometry.column_geometry.sets.back().offset -
                              grid_geometry.column_geometry.FinalGutterSize() -
                              inflow_bounds.offset.inline_offset,
                          grid_geometry.row_geometry.sets.back().offset -
                              grid_geometry.row_geometry.FinalGutterSize() -
                              inflow_bounds.offset.block_offset};

    container_builder_.SetInflowBounds(inflow_bounds);
  }
  container_builder_.SetMayHaveDescendantAboveBlockStart(false);

  // Grid is slightly different to other layout modes in that the contents of
  // the grid won't change if the initial block-size changes definiteness (for
  // example). We can safely mark ourselves as not having any children
  // dependent on the block constraints.
  container_builder_.SetHasDescendantThatDependsOnPercentageBlockSize(false);

  if (constraint_space.HasKnownFragmentainerBlockSize()) {
    // |FinishFragmentation| uses |NGBoxFragmentBuilder::IntrinsicBlockSize| to
    // determine the final size of this fragment. We don't have an accurate
    // "per-fragment" intrinsic block-size so just set it to the trailing
    // border-padding.
    container_builder_.SetIntrinsicBlockSize(border_padding.block_end);
  } else {
    container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);
  }
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  if (UNLIKELY(InvolvedInBlockFragmentation(container_builder_))) {
    auto status = FinishFragmentation(
        node, constraint_space, border_padding.block_end,
        FragmentainerSpaceAtBfcStart(constraint_space), &container_builder_);
    if (status == NGBreakStatus::kDisableFragmentation)
      return container_builder_.Abort(NGLayoutResult::kDisableFragmentation);
    DCHECK_EQ(status, NGBreakStatus::kContinue);
  } else {
#if DCHECK_IS_ON()
    // If we're not participating in a fragmentation context, no block
    // fragmentation related fields should have been set.
    container_builder_.CheckNoBlockFragmentation();
#endif
  }

  NGOutOfFlowLayoutPart(node, constraint_space, &container_builder_).Run();
  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGGridLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  const auto& node = Node();
  const LayoutUnit override_intrinsic_inline_size =
      node.OverrideIntrinsicContentInlineSize();

  if (override_intrinsic_inline_size != kIndefiniteSize) {
    const LayoutUnit size =
        BorderScrollbarPadding().InlineSum() + override_intrinsic_inline_size;
    return {{size, size}, /* depends_on_block_constraints */ false};
  }

  NGGridPlacement grid_placement(Style(),
                                 ComputeAutomaticRepetitions(kForColumns),
                                 ComputeAutomaticRepetitions(kForRows));

  // Measure items; if we have inline size containment ignore all children.
  GridItems grid_items;
  if (!node.ShouldApplyInlineSizeContainment())
    ConstructAndAppendGridItems(&grid_items, &grid_placement);

  // Build block track collections.
  NGGridBlockTrackCollection column_block_track_collection(kForColumns);
  NGGridBlockTrackCollection row_block_track_collection(kForRows);
  BuildBlockTrackCollections(grid_placement, &grid_items,
                             &column_block_track_collection,
                             &row_block_track_collection);

  const bool is_inline_available_size_indefinite =
      grid_available_size_.inline_size == kIndefiniteSize;
  const bool is_block_available_size_indefinite =
      grid_available_size_.block_size == kIndefiniteSize;

  // Build algorithm track collections from the block track collections.
  NGGridLayoutAlgorithmTrackCollection column_track_collection(
      column_block_track_collection, is_inline_available_size_indefinite);
  NGGridLayoutAlgorithmTrackCollection row_track_collection(
      row_block_track_collection, is_block_available_size_indefinite);

  NGGridProperties grid_properties =
      InitializeGridProperties(grid_items, Style().GetWritingMode());

  CacheGridTrackSpanProperties(column_track_collection, &grid_items,
                               &grid_properties);
  CacheGridTrackSpanProperties(row_track_collection, &grid_items,
                               &grid_properties);

  // Cache set indices for grid items.
  for (auto& grid_item : grid_items) {
    grid_item.ComputeSetIndices(column_track_collection);
    grid_item.ComputeSetIndices(row_track_collection);
  }

  bool depends_on_block_constraints = false;
  auto ComputeTotalColumnSize =
      [&](const SizingConstraint sizing_constraint) -> LayoutUnit {
    NGGridGeometry grid_geometry(
        InitializeTrackSizes(grid_properties, &column_track_collection),
        InitializeTrackSizes(grid_properties, &row_track_collection));

    bool needs_additional_pass = false;
    if (grid_properties.HasBaseline(kForColumns)) {
      CalculateAlignmentBaselines(kForColumns, sizing_constraint,
                                  &grid_geometry, &grid_items,
                                  &needs_additional_pass);
    }

    grid_geometry.column_geometry =
        ComputeUsedTrackSizes(grid_geometry, grid_properties, sizing_constraint,
                              &column_track_collection, &grid_items);

    if (needs_additional_pass || HasBlockSizeDependentGridItem(grid_items)) {
      // If we need to calculate the row geometry, we have a dependency on our
      // block constraints.
      depends_on_block_constraints = true;

      if (grid_properties.HasBaseline(kForRows)) {
        CalculateAlignmentBaselines(kForRows, sizing_constraint, &grid_geometry,
                                    &grid_items, &needs_additional_pass);
      }

      absl::optional<SetGeometry> initial_row_geometry;
      if (!needs_additional_pass)
        initial_row_geometry = grid_geometry.row_geometry;

      grid_geometry.row_geometry = ComputeUsedTrackSizes(
          grid_geometry, grid_properties, sizing_constraint,
          &row_track_collection, &grid_items);

      if (initial_row_geometry) {
        DCHECK(!needs_additional_pass);
        needs_additional_pass = MayChangeOrthogonalItemContributions(
            grid_items, *initial_row_geometry, grid_geometry.row_geometry);
      }

      if (needs_additional_pass) {
        if (grid_properties.HasBaseline(kForColumns)) {
          CalculateAlignmentBaselines(kForColumns, sizing_constraint,
                                      &grid_geometry, &grid_items);
        }
        grid_geometry.column_geometry =
            InitializeTrackSizes(grid_properties, &column_track_collection);
        grid_geometry.column_geometry = ComputeUsedTrackSizes(
            grid_geometry, grid_properties, sizing_constraint,
            &column_track_collection, &grid_items);
      }
    }
    return ComputeSetSpanSize(
        grid_geometry.column_geometry,
        /* set_indices */ {0, grid_geometry.column_geometry.sets.size() - 1});
  };

  MinMaxSizes sizes{ComputeTotalColumnSize(SizingConstraint::kMinContent),
                    ComputeTotalColumnSize(SizingConstraint::kMaxContent)};
  sizes += BorderScrollbarPadding().InlineSum();

  // TODO(crbug.com/1272533): This should be |depends_on_block_constraints|
  // (rather than false). However we need more cache slots to handle the
  // performance degredation we currently experience. See bug for more details.
  return MinMaxSizesResult(sizes, /* depends_on_block_constraints */ false);
}

const TrackSpanProperties& GridItemData::GetTrackSpanProperties(
    const GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns) ? column_span_properties
                                          : row_span_properties;
}

void GridItemData::SetTrackSpanProperty(
    const TrackSpanProperties::PropertyId property,
    const GridTrackSizingDirection track_direction) {
  if (track_direction == kForColumns)
    column_span_properties.SetProperty(property);
  else
    row_span_properties.SetProperty(property);
}

bool GridItemData::IsSpanningFlexibleTrack(
    const GridTrackSizingDirection track_direction) const {
  return GetTrackSpanProperties(track_direction)
      .HasProperty(TrackSpanProperties::kHasFlexibleTrack);
}

bool GridItemData::IsSpanningIntrinsicTrack(
    const GridTrackSizingDirection track_direction) const {
  return GetTrackSpanProperties(track_direction)
      .HasProperty(TrackSpanProperties::kHasIntrinsicTrack);
}

bool GridItemData::IsSpanningAutoMinimumTrack(
    const GridTrackSizingDirection track_direction) const {
  return GetTrackSpanProperties(track_direction)
      .HasProperty(TrackSpanProperties::kHasAutoMinimumTrack);
}

bool GridItemData::IsSpanningFixedMinimumTrack(
    const GridTrackSizingDirection track_direction) const {
  return GetTrackSpanProperties(track_direction)
      .HasProperty(TrackSpanProperties::kHasFixedMinimumTrack);
}

bool GridItemData::IsSpanningFixedMaximumTrack(
    const GridTrackSizingDirection track_direction) const {
  return GetTrackSpanProperties(track_direction)
      .HasProperty(TrackSpanProperties::kHasFixedMaximumTrack);
}

bool GridItemData::IsBaselineAlignedForDirection(
    const GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns)
             ? InlineAxisAlignment() == AxisEdge::kBaseline
             : BlockAxisAlignment() == AxisEdge::kBaseline;
}

bool GridItemData::IsBaselineSpecifiedForDirection(
    const GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns)
             ? inline_axis_alignment == AxisEdge::kBaseline
             : block_axis_alignment == AxisEdge::kBaseline;
}

void GridItemData::SetAlignmentFallback(
    const GridTrackSizingDirection track_direction,
    const ComputedStyle& container_style,
    const bool has_synthesized_baseline) {
  // Alignment fallback is only possible when baseline alignment is specified.
  if (!IsBaselineSpecifiedForDirection(track_direction))
    return;

  auto CanParticipateInBaselineAlignment =
      [&](const ComputedStyle& container_style,
          const GridTrackSizingDirection track_direction) -> bool {
    // "If baseline alignment is specified on a grid item whose size in that
    // axis depends on the size of an intrinsically-sized track (whose size is
    // therefore dependent on both the item’s size and baseline alignment,
    // creating a cyclic dependency), that item does not participate in
    // baseline alignment, and instead uses its fallback alignment as if that
    // were originally specified. For this purpose, <flex> track sizes count
    // as “intrinsically-sized” when the grid container has an indefinite size
    // in the relevant axis."
    // https://drafts.csswg.org/css-grid-2/#row-align
    if (has_synthesized_baseline &&
        (IsSpanningIntrinsicTrack(track_direction) ||
         IsSpanningFlexibleTrack(track_direction))) {
      // Parallel grid items with a synthesized baseline support baseline
      // alignment only of the height doesn't depend on the track size.
      const auto& item_style = node.Style();
      const bool is_parallel_to_baseline_axis =
          (track_direction == kForRows) ==
          IsParallelWritingMode(container_style.GetWritingMode(),
                                item_style.GetWritingMode());
      if (is_parallel_to_baseline_axis) {
        const bool logical_height_depends_on_container =
            item_style.LogicalHeight().IsPercentOrCalc() ||
            item_style.LogicalMinHeight().IsPercentOrCalc() ||
            item_style.LogicalMaxHeight().IsPercentOrCalc() ||
            item_style.LogicalHeight().IsAuto();
        return !logical_height_depends_on_container;
      } else {
        // Orthogonal items with synthesized baselines never support baseline
        // alignment when they span intrinsic or flex tracks.
        return false;
      }
    }
    return true;
  };

  // Set fallback alignment to start edges if an item requests baseline
  // alignment but does not meet requirements for it.
  if (!CanParticipateInBaselineAlignment(container_style, track_direction)) {
    if (track_direction == kForColumns &&
        inline_axis_alignment == AxisEdge::kBaseline) {
      inline_axis_alignment_fallback = AxisEdge::kStart;
    } else if (track_direction == kForRows &&
               block_axis_alignment == AxisEdge::kBaseline) {
      block_axis_alignment_fallback = AxisEdge::kStart;
    }
  } else {
    // Reset the alignment fallback if eligibility has changed.
    if (track_direction == kForColumns &&
        inline_axis_alignment_fallback.has_value()) {
      inline_axis_alignment_fallback.reset();
    } else if (track_direction == kForRows &&
               block_axis_alignment_fallback.has_value()) {
      block_axis_alignment_fallback.reset();
    }
  }
}

void GridItemData::ComputeSetIndices(
    const NGGridLayoutAlgorithmTrackCollection& track_collection) {
  DCHECK(!IsOutOfFlow());
  GridItemIndices range_indices = RangeIndices(track_collection.Direction());

#if DCHECK_IS_ON()
  const wtf_size_t start_line = StartLine(track_collection.Direction());
  const wtf_size_t end_line = EndLine(track_collection.Direction());
  DCHECK_LE(end_line, track_collection.EndLineOfImplicitGrid());
  DCHECK_LT(start_line, end_line);

  // Check the range index caching was correct by running a binary search.
  DCHECK_EQ(track_collection.RangeIndexFromTrackNumber(start_line),
            range_indices.begin);
  DCHECK_EQ(track_collection.RangeIndexFromTrackNumber(end_line - 1),
            range_indices.end);
#endif

  auto& set_indices =
      track_collection.IsForColumns() ? column_set_indices : row_set_indices;
  set_indices.begin =
      track_collection.RangeStartingSetIndex(range_indices.begin);
  set_indices.end = track_collection.RangeStartingSetIndex(range_indices.end) +
                    track_collection.RangeSetCount(range_indices.end);

  DCHECK_LE(set_indices.end, track_collection.SetCount());
  DCHECK_LT(set_indices.begin, set_indices.end);
}

const GridItemIndices& GridItemData::SetIndices(
    const GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns) ? column_set_indices
                                          : row_set_indices;
}

GridItemIndices& GridItemData::RangeIndices(
    const GridTrackSizingDirection track_direction) {
  return (track_direction == kForColumns) ? column_range_indices
                                          : row_range_indices;
}

void GridItemData::ComputeOutOfFlowItemPlacement(
    const NGGridLayoutAlgorithmTrackCollection& track_collection,
    const NGGridPlacement& grid_placement) {
  DCHECK(IsOutOfFlow());

  auto& start_offset = track_collection.IsForColumns()
                           ? column_placement.offset_in_range.begin
                           : row_placement.offset_in_range.begin;
  auto& end_offset = track_collection.IsForColumns()
                         ? column_placement.offset_in_range.end
                         : row_placement.offset_in_range.end;

  if (IsGridContainingBlock()) {
    grid_placement.ResolveOutOfFlowItemGridLines(track_collection, node.Style(),
                                                 &start_offset, &end_offset);
  } else {
    start_offset = kNotFound;
    end_offset = kNotFound;
  }

#if DCHECK_IS_ON()
  if (start_offset != kNotFound && end_offset != kNotFound) {
    DCHECK_LE(end_offset, track_collection.EndLineOfImplicitGrid());
    DCHECK_LT(start_offset, end_offset);
  } else if (start_offset != kNotFound) {
    DCHECK_LE(start_offset, track_collection.EndLineOfImplicitGrid());
  } else if (end_offset != kNotFound) {
    DCHECK_LE(end_offset, track_collection.EndLineOfImplicitGrid());
  }
#endif

  // We only calculate the range placement if the line was not defined as 'auto'
  // and it is within the bounds of the grid, since an out of flow item cannot
  // create grid lines.
  const wtf_size_t range_count = track_collection.RangeCount();
  auto& start_range_index = track_collection.IsForColumns()
                                ? column_placement.range_index.begin
                                : row_placement.range_index.begin;
  if (start_offset != kNotFound) {
    if (!range_count) {
      // An undefined and empty grid has a single start/end grid line and no
      // ranges. Therefore, if the start offset isn't 'auto', the only valid
      // offset is zero.
      DCHECK_EQ(start_offset, 0u);
      start_range_index = 0;
    } else {
      // If the start line of an out of flow item is the last line of the grid,
      // we can just subtract one unit to the range count.
      start_range_index =
          (start_offset < track_collection.EndLineOfImplicitGrid())
              ? track_collection.RangeIndexFromTrackNumber(start_offset)
              : range_count - 1;
      start_offset -= track_collection.RangeTrackNumber(start_range_index);
    }
  }

  auto& end_range_index = track_collection.IsForColumns()
                              ? column_placement.range_index.end
                              : row_placement.range_index.end;
  if (end_offset != kNotFound) {
    if (!range_count) {
      // Similarly to the start offset, if we have an undefined, empty grid and
      // the end offset isn't 'auto', the only valid offset is zero.
      DCHECK_EQ(end_offset, 0u);
      end_range_index = 0;
    } else {
      // If the end line of an out of flow item is the first line of the grid,
      // then |last_spanned_range| is set to zero.
      end_range_index =
          end_offset
              ? track_collection.RangeIndexFromTrackNumber(end_offset - 1)
              : 0;
      end_offset -= track_collection.RangeTrackNumber(end_range_index);
    }
  }
}

void GridItems::Append(const GridItemData& new_item_data) {
  reordered_item_indices.push_back(item_data.size());
  item_data.emplace_back(new_item_data);
}

void GridItems::ReserveCapacity(wtf_size_t capacity) {
  reordered_item_indices.ReserveCapacity(capacity);
  item_data.ReserveCapacity(capacity);
}

namespace {

LayoutUnit GetLogicalBaseline(const NGBoxFragment& fragment,
                              const GridTrackSizingDirection track_direction,
                              const WritingMode writing_mode) {
  const auto child_writing_mode =
      fragment.GetWritingDirection().GetWritingMode();
  const bool is_for_columns = (track_direction == kForColumns);

  // TODO(kschmi): Reconcile this with layout experts to see if this makes
  // sense. Some of the entries here are non-intuitive.
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      switch (child_writing_mode) {
        case WritingMode::kHorizontalTb:
          return is_for_columns
                     ? LayoutUnit()
                     : fragment.Baseline().value_or(fragment.BlockSize());
        case WritingMode::kVerticalLr:
          return is_for_columns ? fragment.Baseline().value_or(LayoutUnit())
                                : fragment.InlineSize();
        case WritingMode::kVerticalRl:
          return is_for_columns ? (fragment.BlockSize() -
                                   fragment.Baseline().value_or(LayoutUnit()))
                                : fragment.InlineSize();
        default:
          NOTREACHED();
          return LayoutUnit();
      }
    case WritingMode::kVerticalLr:
      switch (child_writing_mode) {
        case WritingMode::kHorizontalTb:
          return is_for_columns
                     ? fragment.Baseline().value_or(fragment.BlockSize())
                     : LayoutUnit();
        case WritingMode::kVerticalLr:
          return is_for_columns ? fragment.InlineSize()
                                : fragment.Baseline().value_or(LayoutUnit());
        case WritingMode::kVerticalRl:
          return is_for_columns ? fragment.InlineSize()
                                : (fragment.BlockSize() -
                                   fragment.Baseline().value_or(LayoutUnit()));
        default:
          NOTREACHED();
          return LayoutUnit();
      }
    case WritingMode::kVerticalRl:
      switch (child_writing_mode) {
        case WritingMode::kHorizontalTb:
          return is_for_columns
                     ? fragment.Baseline().value_or(fragment.BlockSize())
                     : fragment.InlineSize();
        case WritingMode::kVerticalLr:
          return is_for_columns
                     ? fragment.InlineSize()
                     : (fragment.BlockSize() -
                        fragment.Baseline().value_or(fragment.BlockSize()));
        case WritingMode::kVerticalRl:
          return is_for_columns
                     ? fragment.InlineSize()
                     : fragment.Baseline().value_or(fragment.BlockSize());
        default:
          NOTREACHED();
          return LayoutUnit();
      }
    default:
      NOTREACHED();
      return LayoutUnit();
  }
}

bool HasSynthesizedBaseline(const GridTrackSizingDirection track_direction,
                            const NGBoxFragment& fragment,
                            const WritingMode writing_mode) {
  const auto child_writing_mode =
      fragment.GetWritingDirection().GetWritingMode();
  const bool is_for_columns = (track_direction == kForColumns);

  // TODO(kschmi): Reconcile this with layout experts to see if this makes
  // sense. Some of the entries here are non-intuitive.
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      switch (child_writing_mode) {
        case WritingMode::kHorizontalTb:
          return is_for_columns ? true : !fragment.Baseline().has_value();
        case WritingMode::kVerticalLr:
          return is_for_columns ? !fragment.Baseline().has_value() : true;
        case WritingMode::kVerticalRl:
          return is_for_columns ? (!fragment.Baseline().has_value()) : true;
        default:
          NOTREACHED();
          return false;
      }
    case WritingMode::kVerticalLr:
      switch (child_writing_mode) {
        case WritingMode::kHorizontalTb:
          return is_for_columns ? !fragment.Baseline().has_value() : true;
        case WritingMode::kVerticalLr:
          return is_for_columns ? true : !fragment.Baseline().has_value();
        case WritingMode::kVerticalRl:
          return is_for_columns ? true : !fragment.Baseline().has_value();
        default:
          NOTREACHED();
          return false;
      }
    case WritingMode::kVerticalRl:
      switch (child_writing_mode) {
        case WritingMode::kHorizontalTb:
          return is_for_columns ? !fragment.Baseline().has_value() : true;
        case WritingMode::kVerticalLr:
          return is_for_columns ? true : !fragment.Baseline().has_value();
        case WritingMode::kVerticalRl:
          return is_for_columns ? true : !fragment.Baseline().has_value();
        default:
          NOTREACHED();
          return false;
      }
    default:
      NOTREACHED();
      return false;
  }
}

}  // namespace

LayoutUnit NGGridLayoutAlgorithm::Baseline(
    const NGGridGeometry& grid_geometry,
    const GridItemData& grid_item,
    const GridTrackSizingDirection track_direction) const {
  // "If a box spans multiple shared alignment contexts, then it participates
  // in first/last baseline alignment within its start-most/end-most shared
  // alignment context along that axis", so we only need to look at the first
  // index for baseline/first-baseline support.
  // https://www.w3.org/TR/css-align-3/#baseline-sharing-group
  if (track_direction == kForColumns) {
    const wtf_size_t set_index = grid_item.column_set_indices.begin;
    return (grid_item.column_baseline_type == BaselineType::kMajor)
               ? grid_geometry.major_inline_baselines[set_index]
               : grid_geometry.minor_inline_baselines[set_index];
  } else {
    const wtf_size_t set_index = grid_item.row_set_indices.begin;
    return (grid_item.row_baseline_type == BaselineType::kMajor)
               ? grid_geometry.major_block_baselines[set_index]
               : grid_geometry.minor_block_baselines[set_index];
  }
}

NGGridGeometry NGGridLayoutAlgorithm::ComputeGridGeometry(
    const NGGridBlockTrackCollection& column_block_track_collection,
    const NGGridBlockTrackCollection& row_block_track_collection,
    GridItems* grid_items,
    LayoutUnit* intrinsic_block_size,
    NGGridLayoutAlgorithmTrackCollection* column_track_collection,
    NGGridLayoutAlgorithmTrackCollection* row_track_collection) {
  DCHECK(grid_items && intrinsic_block_size && column_track_collection &&
         row_track_collection);

  DCHECK_NE(grid_available_size_.inline_size, kIndefiniteSize);
  const bool is_block_available_size_indefinite =
      grid_available_size_.block_size == kIndefiniteSize;

  // Build algorithm row track collection from the block track collection.
  *column_track_collection = NGGridLayoutAlgorithmTrackCollection(
      column_block_track_collection, /* is_available_size_indefinite */ false);
  *row_track_collection = NGGridLayoutAlgorithmTrackCollection(
      row_block_track_collection, is_block_available_size_indefinite);

  const auto& container_style = Style();
  NGGridProperties grid_properties =
      InitializeGridProperties(*grid_items, container_style.GetWritingMode());

  CacheGridTrackSpanProperties(*column_track_collection, grid_items,
                               &grid_properties);
  CacheGridTrackSpanProperties(*row_track_collection, grid_items,
                               &grid_properties);

  // Cache set indices for grid items.
  for (auto& grid_item : grid_items->item_data) {
    grid_item.ComputeSetIndices(*column_track_collection);
    grid_item.ComputeSetIndices(*row_track_collection);
  }

  NGGridGeometry grid_geometry;
  auto ComputeGrid = [&]() {
    // We perform the track sizing algorithm using two methods. First
    // |InitializeTrackSizes|, which we need to get an initial column and row
    // set geometry. Then |ComputeUsedTrackSizes|, to finalize the sizing
    // algorithm for both dimensions.
    grid_geometry = NGGridGeometry(
        InitializeTrackSizes(grid_properties, column_track_collection),
        InitializeTrackSizes(grid_properties, row_track_collection));

    // Store column baselines, as these contributions can influence column
    // sizing.
    bool needs_additional_pass = false;
    if (grid_properties.HasBaseline(kForColumns)) {
      CalculateAlignmentBaselines(kForColumns, SizingConstraint::kLayout,
                                  &grid_geometry, grid_items,
                                  &needs_additional_pass);
    }

    // Resolve inline size.
    grid_geometry.column_geometry = ComputeUsedTrackSizes(
        grid_geometry, grid_properties, SizingConstraint::kLayout,
        column_track_collection, grid_items);

    if (grid_properties.HasBaseline(kForRows)) {
      CalculateAlignmentBaselines(kForRows, SizingConstraint::kLayout,
                                  &grid_geometry, grid_items,
                                  &needs_additional_pass);
    }

    absl::optional<SetGeometry> initial_row_geometry;
    if (!needs_additional_pass && HasBlockSizeDependentGridItem(*grid_items))
      initial_row_geometry = grid_geometry.row_geometry;

    // Resolve block size.
    grid_geometry.row_geometry = ComputeUsedTrackSizes(
        grid_geometry, grid_properties, SizingConstraint::kLayout,
        row_track_collection, grid_items);

    if (initial_row_geometry) {
      DCHECK(!needs_additional_pass);
      needs_additional_pass = MayChangeOrthogonalItemContributions(
          *grid_items, *initial_row_geometry, grid_geometry.row_geometry);
    }

    // If we had an orthogonal item which may have depended on the resolved row
    // tracks, re-run the track sizing algorithm for both dimensions.
    if (needs_additional_pass) {
      if (grid_properties.HasBaseline(kForColumns)) {
        CalculateAlignmentBaselines(kForColumns, SizingConstraint::kLayout,
                                    &grid_geometry, grid_items);
      }

      grid_geometry.column_geometry =
          InitializeTrackSizes(grid_properties, column_track_collection);
      grid_geometry.column_geometry = ComputeUsedTrackSizes(
          grid_geometry, grid_properties, SizingConstraint::kLayout,
          column_track_collection, grid_items);

      if (grid_properties.HasBaseline(kForRows)) {
        CalculateAlignmentBaselines(kForRows, SizingConstraint::kLayout,
                                    &grid_geometry, grid_items);
      }

      grid_geometry.row_geometry =
          InitializeTrackSizes(grid_properties, row_track_collection);
      grid_geometry.row_geometry = ComputeUsedTrackSizes(
          grid_geometry, grid_properties, SizingConstraint::kLayout,
          row_track_collection, grid_items);
    }

    if (grid_properties.HasBaseline(kForColumns)) {
      CalculateAlignmentBaselines(kForColumns, SizingConstraint::kLayout,
                                  &grid_geometry, grid_items);
    }
    if (grid_properties.HasBaseline(kForRows)) {
      CalculateAlignmentBaselines(kForRows, SizingConstraint::kLayout,
                                  &grid_geometry, grid_items);
    }
  };

  ComputeGrid();

  if (contain_intrinsic_block_size_) {
    *intrinsic_block_size = *contain_intrinsic_block_size_;
  } else {
    // Intrinsic block size is based on the final row offset. Because gutters
    // are included in row offsets, subtract out the final gutter (if present).
    *intrinsic_block_size = grid_geometry.row_geometry.sets.back().offset -
                            grid_geometry.row_geometry.FinalGutterSize() +
                            BorderScrollbarPadding().block_end;

    // TODO(layout-dev): This isn't great but matches legacy. Ideally this
    // would only apply when we have only flexible track(s).
    if (grid_items->IsEmpty() && Node().HasLineIfEmpty()) {
      *intrinsic_block_size = std::max(
          *intrinsic_block_size, BorderScrollbarPadding().BlockSum() +
                                     Node().EmptyLineBlockSize(BreakToken()));
    }

    *intrinsic_block_size = ClampIntrinsicBlockSize(ConstraintSpace(), Node(),
                                                    BorderScrollbarPadding(),
                                                    *intrinsic_block_size);
  }

  if (grid_available_size_.block_size == kIndefiniteSize) {
    const LayoutUnit block_size = ComputeBlockSizeForFragment(
        ConstraintSpace(), container_style, BorderPadding(),
        *intrinsic_block_size, border_box_size_.inline_size);

    grid_available_size_.block_size = grid_min_available_size_.block_size =
        grid_max_available_size_.block_size =
            (block_size - BorderScrollbarPadding().BlockSum())
                .ClampNegativeToZero();

    // Re-compute the row geometry now that we have the resolved available
    // block-size. "align-content: space-evenly" etc, require the resolved size.
    if (container_style.AlignContent() !=
        ComputedStyleInitialValues::InitialAlignContent()) {
      grid_geometry.row_geometry = ComputeSetGeometry(*row_track_collection);
    }

    // If we have any rows, gaps which will resolve differently if we have a
    // definite |grid_available_size_| re-compute the grid using the
    // |block_size| calculated above.
    bool should_recompute_grid =
        (container_style.RowGap() &&
         container_style.RowGap()->IsPercentOrCalc()) ||
        grid_properties.IsDependentOnAvailableSize(kForRows);

    // If we are a flex-item, we may have our initial block-size forced to be
    // indefinite, however grid layout always re-computes the grid using the
    // final "used" block-size.
    // We can detect this case by checking if computing our block-size (with an
    // indefinite intrinsic size) is definite.
    //
    // TODO(layout-dev): A small optimization here would be to do this only if
    // we have 'auto' tracks which fill the remaining available space.
    if (ConstraintSpace().IsInitialBlockSizeIndefinite()) {
      should_recompute_grid |=
          ComputeBlockSizeForFragment(
              ConstraintSpace(), container_style, BorderPadding(),
              /* intrinsic_block_size */ kIndefiniteSize,
              border_box_size_.inline_size) != kIndefiniteSize;
    }

    if (should_recompute_grid) {
      DCHECK_NE(grid_available_size_.block_size, kIndefiniteSize);

      *row_track_collection = NGGridLayoutAlgorithmTrackCollection(
          row_block_track_collection, /* is_available_size_indefinite */ false);
      CacheGridTrackSpanProperties(*row_track_collection, grid_items,
                                   &grid_properties);
      ComputeGrid();
    }
  }
  return grid_geometry;
}

LayoutUnit NGGridLayoutAlgorithm::ComputeIntrinsicBlockSizeIgnoringChildren()
    const {
  DCHECK(Node().ShouldApplyBlockSizeContainment());

  // First check 'contain-intrinsic-size'.
  const LayoutUnit override_intrinsic_block_size =
      Node().OverrideIntrinsicContentBlockSize();
  if (override_intrinsic_block_size != kIndefiniteSize)
    return BorderScrollbarPadding().BlockSum() + override_intrinsic_block_size;

  // Don't append any children for this calculation.
  GridItems grid_items;
  NGGridPlacement grid_placement(Style(),
                                 ComputeAutomaticRepetitions(kForColumns),
                                 ComputeAutomaticRepetitions(kForRows));

  // Build block track collections.
  NGGridBlockTrackCollection column_block_track_collection(kForColumns);
  NGGridBlockTrackCollection row_block_track_collection(kForRows);
  BuildBlockTrackCollections(grid_placement, &grid_items,
                             &column_block_track_collection,
                             &row_block_track_collection);

  const bool is_block_available_size_indefinite =
      grid_available_size_.block_size == kIndefiniteSize;

  // Build algorithm row track collection from the block track collection.
  NGGridLayoutAlgorithmTrackCollection row_track_collection(
      row_block_track_collection, is_block_available_size_indefinite);

  NGGridProperties grid_properties =
      InitializeGridProperties(grid_items, Style().GetWritingMode());

  NGGridGeometry grid_geometry(
      SetGeometry(),
      InitializeTrackSizes(grid_properties, &row_track_collection));

  // Resolve the rows.
  grid_geometry.row_geometry = ComputeUsedTrackSizes(
      grid_geometry, grid_properties, SizingConstraint::kLayout,
      &row_track_collection, &grid_items);

  return grid_geometry.row_geometry.sets.back().offset -
         grid_geometry.row_geometry.FinalGutterSize() +
         BorderScrollbarPadding().block_end;
}

namespace {

scoped_refptr<const NGLayoutResult> LayoutNodeForMeasure(
    const NGBlockNode& node,
    const NGConstraintSpace& constraint_space,
    const SizingConstraint sizing_constraint) {
  // Disable side effects during MinMax computation to avoid potential "MinMax
  // after layout" crashes. This is not necessary during the layout pass, and
  // would have a negative impact on performance if used there.
  absl::optional<NGDisableSideEffectsScope> disable_side_effects;
  if (sizing_constraint != SizingConstraint::kLayout &&
      !node.GetLayoutBox()->NeedsLayout()) {
    disable_side_effects.emplace();
  }
  return node.Layout(constraint_space);
}

}  // namespace

LayoutUnit NGGridLayoutAlgorithm::ContributionSizeForGridItem(
    const NGGridGeometry& grid_geometry,
    const SizingConstraint sizing_constraint,
    const GridTrackSizingDirection track_direction,
    const GridItemContributionType contribution_type,
    GridItemData* grid_item) const {
  DCHECK(grid_item);

  const NGBlockNode& node = grid_item->node;
  const ComputedStyle& item_style = node.Style();

  const bool is_for_columns = track_direction == kForColumns;
  const bool is_parallel = IsParallelWritingMode(Style().GetWritingMode(),
                                                 item_style.GetWritingMode());
  const bool is_parallel_with_track_direction = is_for_columns == is_parallel;

  // TODO(ikilpatrick): We'll need to record if any child used an indefinite
  // size for its contribution, such that we can then do the 2nd pass on the
  // track-sizing algorithm.
  const auto space = CreateConstraintSpaceForMeasure(*grid_item, grid_geometry,
                                                     track_direction);
  const auto margins = ComputeMarginsFor(space, item_style, ConstraintSpace());

  auto MinMaxContentSizes = [&]() -> MinMaxSizes {
    const auto result = ComputeMinAndMaxContentContributionForSelf(node, space);

    // The min/max contribution may depend on the block-size of the grid-area:
    // <div style="display: inline-grid; grid-template-columns: auto auto;">
    //   <div style="height: 100%">
    //     <img style="height: 50%;" />
    //   </div>
    //   <div>
    //     <div style="height: 100px;"></div>
    //   </div>
    // </div>
    // Mark ourselves as requiring an additional pass to re-resolve the column
    // tracks for this case.
    if (is_parallel && result.depends_on_block_constraints)
      grid_item->is_sizing_dependent_on_block_size = true;
    return result.sizes;
  };

  // This function will determine the correct block-size of a grid-item.
  // TODO(ikilpatrick): This should try and skip layout when possible. Notes:
  //  - We'll need to do a full layout for tables.
  //  - We'll need special logic for replaced elements.
  //  - We'll need to respect the aspect-ratio when appropriate.
  LayoutUnit baseline_shim;
  auto BlockContributionSize = [&]() -> LayoutUnit {
    DCHECK(!is_parallel_with_track_direction);

    // TODO(ikilpatrick): This check is potentially too broad, i.e. a fixed
    // inline size with no %-padding doesn't need the additional pass.
    if (is_for_columns)
      grid_item->is_sizing_dependent_on_block_size = true;

    scoped_refptr<const NGLayoutResult> result;
    if (space.AvailableSize().inline_size == kIndefiniteSize) {
      // The only case where we will have an indefinite block size is for the
      // first column resolution step; after that we will always have the used
      // sizes of the previous step for the orthogonal direction.
      DCHECK(is_for_columns);

      // If we are orthogonal grid-item, resolving against an indefinite size,
      // set our inline-size to our max content-contribution size.
      const auto fallback_space = CreateConstraintSpaceForMeasure(
          *grid_item, grid_geometry, track_direction,
          /* opt_fixed_block_size */ MinMaxContentSizes().max_size);

      result = LayoutNodeForMeasure(node, fallback_space, sizing_constraint);
    } else {
      result = LayoutNodeForMeasure(node, space, sizing_constraint);
    }

    NGBoxFragment fragment(
        item_style.GetWritingDirection(),
        To<NGPhysicalBoxFragment>(result->PhysicalFragment()));

    if (grid_item->IsBaselineAlignedForDirection(track_direction)) {
      LayoutUnit track_baseline =
          Baseline(grid_geometry, *grid_item, track_direction);

      // The item's baseline alignment impacts the item's contribution as the
      // difference between the track's baseline and the item's baseline.
      if (track_baseline != LayoutUnit::Min()) {
        baseline_shim =
            track_baseline -
            GetLogicalBaseline(
                fragment, track_direction,
                ConstraintSpace().GetWritingDirection().GetWritingMode());

        // Subtract out the start margin so it doesn't get added a second time
        // at the end of |NGGridLayoutAlgorithm::ContributionSizeForGridItem|.
        baseline_shim -=
            is_for_columns ? margins.inline_start : margins.block_start;
      }
    }
    return fragment.BlockSize() + baseline_shim;
  };

  const LayoutUnit margin_sum =
      is_for_columns ? margins.InlineSum() : margins.BlockSum();

  LayoutUnit contribution;
  switch (contribution_type) {
    case GridItemContributionType::kForContentBasedMinimums:
    case GridItemContributionType::kForIntrinsicMaximums:
      contribution = is_parallel_with_track_direction
                         ? MinMaxContentSizes().min_size
                         : BlockContributionSize();
      break;
    case GridItemContributionType::kForIntrinsicMinimums: {
      // TODO(ikilpatrick): All of the below is incorrect for replaced elements.
      const Length& main_length = is_parallel_with_track_direction
                                      ? item_style.LogicalWidth()
                                      : item_style.LogicalHeight();
      const Length& min_length = is_parallel_with_track_direction
                                     ? item_style.LogicalMinWidth()
                                     : item_style.LogicalMinHeight();

      // We could be clever is and make this an if-stmt, but each type has
      // subtle consequences. This forces us in the future when we add a new
      // length type to consider what the best thing is for grid.
      switch (main_length.GetType()) {
        case Length::kAuto:
        case Length::kFitContent:
        case Length::kFillAvailable:
        case Length::kPercent:
        case Length::kCalculated: {
          const NGBoxStrut border_padding =
              ComputeBorders(space, node) + ComputePadding(space, item_style);

          // All of the above lengths are considered 'auto' if we are querying a
          // minimum contribution. They all require definite track sizes to
          // determine their final size.
          //
          // From https://drafts.csswg.org/css-grid/#min-size-auto:
          //   To provide a more reasonable default minimum size for grid items,
          //   the used value of its automatic minimum size in a given axis is
          //   the content-based minimum size if all of the following are true:
          //     - it is not a scroll container
          //     - it spans at least one track in that axis whose min track
          //     sizing function is 'auto'
          //     - if it spans more than one track in that axis, none of those
          //     tracks are flexible
          //   Otherwise, the automatic minimum size is zero, as usual.
          //
          // Start by resolving the cases where |min_length| is non-auto or its
          // automatic minimum size should be zero.
          if (!min_length.IsAuto() || item_style.IsScrollContainer() ||
              !grid_item->IsSpanningAutoMinimumTrack(track_direction) ||
              (grid_item->IsSpanningFlexibleTrack(track_direction) &&
               grid_item->SpanSize(track_direction) > 1)) {
            // TODO(ikilpatrick): This block needs to respect the aspect-ratio,
            // and apply the transferred min/max sizes when appropriate. We do
            // this sometimes elsewhere so should unify and simplify this code.
            if (is_parallel_with_track_direction) {
              auto MinMaxSizesFunc =
                  [&](MinMaxSizesType type) -> MinMaxSizesResult {
                return node.ComputeMinMaxSizes(item_style.GetWritingMode(),
                                               type, space);
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

          // Resolve the content-based minimum size.
          contribution = is_parallel_with_track_direction
                             ? MinMaxContentSizes().min_size
                             : BlockContributionSize();

          const auto& set_geometry = is_for_columns
                                         ? grid_geometry.column_geometry
                                         : grid_geometry.row_geometry;
          const auto& set_indices = grid_item->SetIndices(track_direction);
          const wtf_size_t last_indefinite_index =
              set_geometry.last_indefinite_indices[set_indices.end];

          if (last_indefinite_index == kNotFound ||
              last_indefinite_index < set_indices.begin) {
            // Further clamp the minimum size to less than or equal to the
            // stretch fit into the grid area’s maximum size in that dimension,
            // as represented by the sum of those grid tracks’ max track sizing
            // functions plus any intervening fixed gutters.
            LayoutUnit spanned_tracks_definite_max_size =
                ComputeSetSpanSize(set_geometry, set_indices);
            DCHECK_GE(spanned_tracks_definite_max_size, 0);

            const LayoutUnit border_padding_sum =
                is_parallel_with_track_direction ? border_padding.InlineSum()
                                                 : border_padding.BlockSum();
            DCHECK_GE(contribution, baseline_shim + border_padding_sum);

            // The stretch fit into a given size is that size, minus the box’s
            // computed margins, border, and padding in the given dimension,
            // flooring at zero so that the inner size is not negative.
            spanned_tracks_definite_max_size =
                (spanned_tracks_definite_max_size - baseline_shim - margin_sum -
                 border_padding_sum)
                    .ClampNegativeToZero();

            // Add the baseline shim, border, and padding (margins will be added
            // later) back to the contribution, since we don't want the outer
            // size of the minimum size to overflow its grid area; these are
            // already accounted for in the current value of |contribution|.
            contribution =
                std::min(contribution, spanned_tracks_definite_max_size +
                                           baseline_shim + border_padding_sum);
          }
          break;
        }
        case Length::kMinContent:
        case Length::kMaxContent:
        case Length::kFixed: {
          // All of the above lengths are "definite" (non-auto), and don't need
          // the special min-size treatment above. (They will all end up being
          // the specified size).
          if (is_parallel_with_track_direction) {
            contribution = main_length.IsMaxContent()
                               ? MinMaxContentSizes().max_size
                               : MinMaxContentSizes().min_size;
          } else {
            contribution = BlockContributionSize();
          }
          break;
        }
        case Length::kMinIntrinsic:
        case Length::kDeviceWidth:
        case Length::kDeviceHeight:
        case Length::kExtendToZoom:
        case Length::kContent:
        case Length::kNone:
          NOTREACHED();
          break;
      }
      break;
    }
    case GridItemContributionType::kForMaxContentMinimums:
    case GridItemContributionType::kForMaxContentMaximums:
      contribution = is_parallel_with_track_direction
                         ? MinMaxContentSizes().max_size
                         : BlockContributionSize();
      break;
    case GridItemContributionType::kForFreeSpace:
      NOTREACHED() << "|kForFreeSpace| should only be used to distribute extra "
                      "space in maximize tracks and stretch auto tracks steps.";
      break;
  }
  return (contribution + margin_sum).ClampNegativeToZero();
}

void NGGridLayoutAlgorithm::ConstructAndAppendGridItems(
    GridItems* grid_items,
    NGGridPlacement* grid_placement) const {
  DCHECK(grid_items && grid_placement && grid_items->IsEmpty());

  const auto& node = Node();
  if (auto previous_capacity = node.CachedGridItemCount())
    grid_items->ReserveCapacity(*previous_capacity);

  const auto& container_style = Style();
  const auto container_writing_mode = ConstraintSpace().GetWritingMode();
  const int initial_order = ComputedStyleInitialValues::InitialOrder();
  bool should_sort_grid_items_by_order_property = false;

  for (auto child = node.FirstChild(); child; child = child.NextSibling()) {
    auto grid_item = InitializeGridItem(To<NGBlockNode>(child), container_style,
                                        container_writing_mode);

    // Order all of our in-flow children by their order property.
    if (!grid_item.IsOutOfFlow()) {
      should_sort_grid_items_by_order_property |=
          child.Style().Order() != initial_order;
      grid_items->Append(grid_item);
    }
  }

  // We only need to sort this when we encounter a non-initial order property.
  auto CompareItemsByOrderProperty = [](const GridItemData& lhs,
                                        const GridItemData& rhs) {
    return lhs.node.Style().Order() < rhs.node.Style().Order();
  };
  if (should_sort_grid_items_by_order_property) {
    std::stable_sort(grid_items->item_data.begin(), grid_items->item_data.end(),
                     CompareItemsByOrderProperty);
  }

  const auto& grid_item_positions =
      node.ResolveGridItemPositions(*grid_items, grid_placement);

  auto* resolved_position = grid_item_positions.begin();
  for (auto& grid_item : grid_items->item_data)
    grid_item.resolved_position = *(resolved_position++);
}

// https://drafts.csswg.org/css-grid-2/#auto-repeat
wtf_size_t NGGridLayoutAlgorithm::ComputeAutomaticRepetitions(
    const GridTrackSizingDirection track_direction) const {
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

  const LayoutUnit grid_gap = GridGap(track_direction);

  LayoutUnit auto_repeater_size;
  LayoutUnit non_auto_specified_size;
  for (wtf_size_t repeater_index = 0;
       repeater_index < track_list.RepeaterCount(); ++repeater_index) {
    const wtf_size_t repeater_track_count =
        track_list.RepeatSize(repeater_index);
    const auto repeat_type = track_list.RepeatType(repeater_index);
    const bool is_auto_repeater =
        repeat_type == NGGridTrackRepeater::kAutoFill ||
        repeat_type == NGGridTrackRepeater::kAutoFit;
    LayoutUnit repeater_size;
    for (wtf_size_t track_index = 0; track_index < repeater_track_count;
         ++track_index) {
      const GridTrackSize& track_size =
          track_list.RepeatTrackSize(repeater_index, track_index);
      absl::optional<LayoutUnit> fixed_min_track_breadth;
      absl::optional<LayoutUnit> fixed_max_track_breadth;
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
      if (is_auto_repeater)
        track_contribution = std::max(LayoutUnit(1), track_contribution);

      repeater_size += track_contribution + grid_gap;
    }
    if (!is_auto_repeater) {
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

// Given an |item_style| determines the correct |AxisEdge| alignment.
// Additionally will determine:
//  - The behavior of 'auto' via the |auto_behavior| out-parameter.
//  - If the alignment is safe via the |is_overflow_safe| out-parameter.
AxisEdge AxisEdgeFromItemPosition(const bool is_inline_axis,
                                  const bool is_replaced,
                                  const bool is_out_of_flow,
                                  const ComputedStyle& item_style,
                                  const ComputedStyle& container_style,
                                  NGAutoBehavior* auto_behavior,
                                  bool* is_overflow_safe) {
  DCHECK(auto_behavior && is_overflow_safe);

  const auto& alignment = is_inline_axis
                              ? item_style.ResolvedJustifySelf(
                                    ItemPosition::kNormal, &container_style)
                              : item_style.ResolvedAlignSelf(
                                    ItemPosition::kNormal, &container_style);

  *auto_behavior = NGAutoBehavior::kFitContent;
  *is_overflow_safe = alignment.Overflow() == OverflowAlignment::kSafe;

  // Auto-margins take precedence over any alignment properties.
  if (item_style.MayHaveMargin() && !is_out_of_flow) {
    const bool is_start_auto =
        is_inline_axis ? item_style.MarginStartUsing(container_style).IsAuto()
                       : item_style.MarginBeforeUsing(container_style).IsAuto();
    const bool is_end_auto =
        is_inline_axis ? item_style.MarginEndUsing(container_style).IsAuto()
                       : item_style.MarginAfterUsing(container_style).IsAuto();

    // 'auto' margin alignment is always "safe".
    if (is_start_auto || is_end_auto)
      *is_overflow_safe = true;

    if (is_start_auto && is_end_auto)
      return AxisEdge::kCenter;
    else if (is_start_auto)
      return AxisEdge::kEnd;
    else if (is_end_auto)
      return AxisEdge::kStart;
  }

  const auto container_writing_direction =
      container_style.GetWritingDirection();
  const auto item_position = alignment.GetPosition();

  switch (item_position) {
    case ItemPosition::kSelfStart:
    case ItemPosition::kSelfEnd: {
      // In order to determine the correct "self" axis-edge without a
      // complicated set of if-branches we use two converters.

      // First use the grid-item's writing-direction to convert the logical
      // edge into the physical coordinate space.
      LogicalToPhysical<AxisEdge> physical(item_style.GetWritingDirection(),
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
      *auto_behavior = NGAutoBehavior::kStretchExplicit;
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
    case ItemPosition::kNormal:
      *auto_behavior = is_replaced ? NGAutoBehavior::kFitContent
                                   : NGAutoBehavior::kStretchImplicit;
      return AxisEdge::kStart;
    case ItemPosition::kLegacy:
    case ItemPosition::kAuto:
      NOTREACHED();
      return AxisEdge::kStart;
  }
}

// Determines whether the track direction, grid container writing mode, and
// grid item writing mode are part of the same alignment context as specified in
// https://www.w3.org/TR/css-align-3/#baseline-sharing-group
// In particular, 'Boxes share an alignment context, along a particular axis,
// and established by a particular box, when they are grid items in the same
// row, along the grid’s row (inline) axis, established by the grid container.'
//
// TODO(kschmi): Some of these conditions are non-intuitive, so investigate
// whether these conditions are correct or if the test expectations are off.
BaselineType DetermineBaselineType(
    const GridTrackSizingDirection track_direction,
    const WritingMode container_writing_mode,
    const WritingMode child_writing_mode) {
  bool is_major = false;
  switch (container_writing_mode) {
    case WritingMode::kHorizontalTb:
      is_major = (track_direction == kForRows)
                     ? true
                     : (child_writing_mode == WritingMode::kVerticalLr ||
                        child_writing_mode == WritingMode::kHorizontalTb);
      break;
    case WritingMode::kVerticalLr:
      is_major = (track_direction == kForRows)
                     ? (child_writing_mode == WritingMode::kVerticalLr ||
                        child_writing_mode == WritingMode::kHorizontalTb)
                     : true;
      break;
    case WritingMode::kVerticalRl:
      is_major = (track_direction == kForRows)
                     ? (child_writing_mode == WritingMode::kVerticalRl ||
                        child_writing_mode == WritingMode::kHorizontalTb)
                     : true;
      break;
    default:
      is_major = true;
      break;
  }
  return is_major ? BaselineType::kMajor : BaselineType::kMinor;
}

}  // namespace

// static
GridItemData NGGridLayoutAlgorithm::InitializeGridItem(
    const NGBlockNode node,
    const ComputedStyle& container_style,
    const WritingMode container_writing_mode) {
  GridItemData grid_item(node);

  const auto& item_style = node.Style();
  const bool is_replaced = node.IsReplaced();
  const bool is_out_of_flow = grid_item.IsOutOfFlow();

  // Determine the alignment for the grid item ahead of time (we may need to
  // know if it stretches to correctly determine any block axis contribution).
  bool is_overflow_safe;
  grid_item.inline_axis_alignment = AxisEdgeFromItemPosition(
      /* is_inline_axis */ true, is_replaced, is_out_of_flow, item_style,
      container_style, &grid_item.inline_auto_behavior, &is_overflow_safe);
  grid_item.is_inline_axis_overflow_safe = is_overflow_safe;

  grid_item.block_axis_alignment = AxisEdgeFromItemPosition(
      /* is_inline_axis */ false, is_replaced, is_out_of_flow, item_style,
      container_style, &grid_item.block_auto_behavior, &is_overflow_safe);
  grid_item.is_block_axis_overflow_safe = is_overflow_safe;

  const auto item_writing_mode =
      item_style.GetWritingDirection().GetWritingMode();
  grid_item.row_baseline_type = DetermineBaselineType(
      kForRows, container_writing_mode, item_writing_mode);
  grid_item.column_baseline_type = DetermineBaselineType(
      kForColumns, container_writing_mode, item_writing_mode);

  return grid_item;
}

void NGGridLayoutAlgorithm::BuildBlockTrackCollections(
    const NGGridPlacement& grid_placement,
    GridItems* grid_items,
    NGGridBlockTrackCollection* column_track_collection,
    NGGridBlockTrackCollection* row_track_collection) const {
  DCHECK(grid_items && column_track_collection && row_track_collection);
  const ComputedStyle& grid_style = Style();

  auto BuildBlockTrackCollection =
      [&](NGGridBlockTrackCollection* track_collection) {
        const auto track_direction = track_collection->Direction();
        const wtf_size_t start_offset =
            grid_placement.StartOffset(track_direction);

        const NGGridTrackList& template_track_list =
            (track_direction == kForColumns)
                ? grid_style.GridTemplateColumns().NGTrackList()
                : grid_style.GridTemplateRows().NGTrackList();
        const NGGridTrackList& auto_track_list =
            (track_direction == kForColumns)
                ? grid_style.GridAutoColumns().NGTrackList()
                : grid_style.GridAutoRows().NGTrackList();
        const wtf_size_t named_grid_area_track_count =
            (track_direction == kForColumns)
                ? grid_style.NamedGridAreaColumnCount()
                : grid_style.NamedGridAreaRowCount();

        track_collection->SetSpecifiedTracks(
            &template_track_list, &auto_track_list, start_offset,
            grid_placement.AutoRepetitions(track_direction),
            named_grid_area_track_count);
        EnsureTrackCoverageForGridItems(grid_items, track_collection);
        track_collection->FinalizeRanges(start_offset);
      };

  BuildBlockTrackCollection(column_track_collection);
  BuildBlockTrackCollection(row_track_collection);
}

void NGGridLayoutAlgorithm::EnsureTrackCoverageForGridItems(
    GridItems* grid_items,
    NGGridBlockTrackCollection* track_collection) const {
  DCHECK(grid_items && track_collection);

  const auto track_direction = track_collection->Direction();
  for (auto& grid_item : grid_items->item_data) {
    track_collection->EnsureTrackCoverage(
        grid_item.StartLine(track_direction),
        grid_item.SpanSize(track_direction),
        &grid_item.RangeIndices(track_direction).begin,
        &grid_item.RangeIndices(track_direction).end);
  }
}

void NGGridLayoutAlgorithm::CalculateAlignmentBaselines(
    const GridTrackSizingDirection track_direction,
    const SizingConstraint sizing_constraint,
    NGGridGeometry* grid_geometry,
    GridItems* grid_items,
    bool* needs_additional_pass) const {
  DCHECK(grid_geometry && grid_items);

  // Reset existing baselines from geometry so they are clean with each call to
  // this method. Use 'WTF::Vector::Fill()' over 'WTF::Vector::clear()', as
  // 'clear' will reset the capacity to zero and require re-allocations.
  if (track_direction == kForColumns) {
    grid_geometry->major_inline_baselines.Fill(LayoutUnit::Min());
    grid_geometry->minor_inline_baselines.Fill(LayoutUnit::Min());
  } else {
    grid_geometry->major_block_baselines.Fill(LayoutUnit::Min());
    grid_geometry->minor_block_baselines.Fill(LayoutUnit::Min());
  }

  auto UpdateBaseline = [&](const GridItemData& grid_item,
                            LayoutUnit candidate_baseline) {
    // "If a box spans multiple shared alignment contexts, then it participates
    // in first/last baseline alignment within its start-most/end-most shared
    // alignment context along that axis", so we only need to look at the first
    // index for baseline/first-baseline support.
    // https://www.w3.org/TR/css-align-3/#baseline-sharing-group
    LayoutUnit* track_baseline;
    if (track_direction == kForColumns) {
      const wtf_size_t set_index = grid_item.column_set_indices.begin;
      track_baseline = (grid_item.column_baseline_type == BaselineType::kMajor)
                           ? &grid_geometry->major_inline_baselines[set_index]
                           : &grid_geometry->minor_inline_baselines[set_index];
    } else {
      const wtf_size_t set_index = grid_item.row_set_indices.begin;
      track_baseline = (grid_item.row_baseline_type == BaselineType::kMajor)
                           ? &grid_geometry->major_block_baselines[set_index]
                           : &grid_geometry->minor_block_baselines[set_index];
    }
    *track_baseline = std::max(*track_baseline, candidate_baseline);
  };

  for (auto& grid_item : grid_items->item_data) {
    if (!grid_item.IsBaselineSpecifiedForDirection(track_direction))
      continue;

    LogicalRect unused_grid_area;
    const auto space = CreateConstraintSpaceForLayout(*grid_geometry, grid_item,
                                                      &unused_grid_area);

    // We cannot apply some of the baseline alignment rules for synthesized
    // baselines until layout has been performed. However, layout cannot
    // be performed in certain scenarios. So force an additional pass in
    // these cases and skip layout for now.
    const auto& item_style = grid_item.node.Style();
    if (InlineLengthUnresolvable(space, item_style.LogicalWidth()) ||
        InlineLengthUnresolvable(space, item_style.LogicalMinWidth()) ||
        InlineLengthUnresolvable(space, item_style.LogicalMaxWidth())) {
      if (needs_additional_pass)
        *needs_additional_pass = true;
      continue;
    }

    const auto result =
        LayoutNodeForMeasure(grid_item.node, space, sizing_constraint);

    NGBoxFragment fragment(
        item_style.GetWritingDirection(),
        To<NGPhysicalBoxFragment>(result->PhysicalFragment()));

    const auto& container_space = ConstraintSpace();
    const auto container_writing_mode =
        container_space.GetWritingDirection().GetWritingMode();

    grid_item.SetAlignmentFallback(
        track_direction, Style(),
        HasSynthesizedBaseline(track_direction, fragment,
                               container_writing_mode));

    const auto margins = ComputeMarginsFor(space, item_style, container_space);
    LayoutUnit baseline =
        ((track_direction == kForColumns) ? margins.inline_start
                                          : margins.block_start) +
        GetLogicalBaseline(fragment, track_direction, container_writing_mode);

    // TODO(kschmi): The IsReplaced() check here is a bit strange, but is
    // necessary to pass some of the tests. Follow-up to see if there's
    // a better solution.
    if (grid_item.IsBaselineAlignedForDirection(track_direction) ||
        grid_item.node.IsReplaced()) {
      UpdateBaseline(grid_item, baseline);
    }
  }
}

// https://drafts.csswg.org/css-grid-2/#algo-init
SetGeometry NGGridLayoutAlgorithm::InitializeTrackSizes(
    const NGGridProperties& grid_properties,
    NGGridLayoutAlgorithmTrackCollection* track_collection) const {
  DCHECK(track_collection);

  const auto track_direction = track_collection->Direction();
  const wtf_size_t set_count = track_collection->SetCount() + 1;

  LayoutUnit available_size = (track_direction == kForColumns)
                                  ? grid_available_size_.inline_size
                                  : grid_available_size_.block_size;

  TrackGeometry track_geometry;
  track_geometry.start_offset = (track_direction == kForColumns)
                                    ? BorderScrollbarPadding().inline_start
                                    : BorderScrollbarPadding().block_start;
  track_geometry.gutter_size = GridGap(track_direction);
  SetGeometry set_geometry(track_geometry, set_count);

  // The initial last indefinite index is always kNotFound.
  wtf_size_t last_indefinite_index = kNotFound;
  set_geometry.last_indefinite_indices.ReserveInitialCapacity(set_count);
  set_geometry.last_indefinite_indices.emplace_back(last_indefinite_index);

  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    auto& current_set = set_iterator.CurrentSet();
    const auto& track_size = current_set.TrackSize();
    DCHECK_NE(track_size.GetType(), kLengthTrackSizing);

    if (track_size.IsFitContent()) {
      // Indefinite lengths cannot occur, as they must be normalized to 'auto'.
      DCHECK(!track_size.FitContentTrackBreadth().HasPercentage() ||
             available_size != kIndefiniteSize);

      LayoutUnit fit_content_argument = MinimumValueForLength(
          track_size.FitContentTrackBreadth().length(), available_size);
      current_set.SetFitContentLimit(fit_content_argument *
                                     current_set.TrackCount());
    }

    if (track_size.HasFixedMaxTrackBreadth()) {
      DCHECK(!track_size.MaxTrackBreadth().HasPercentage() ||
             available_size != kIndefiniteSize);

      // A fixed sizing function: Resolve to an absolute length and use that
      // size as the track’s initial growth limit; if the growth limit is less
      // than the base size, increase the growth limit to match the base size.
      LayoutUnit fixed_max_breadth = MinimumValueForLength(
          track_size.MaxTrackBreadth().length(), available_size);
      current_set.InitGrowthLimit(fixed_max_breadth * current_set.TrackCount());

      // For the purposes of our "base" row set geometry, we only use any
      // definite max track sizing functions. We will use this value later to
      // measure orthogonal (or %-block-size) grid item contributions.
      track_geometry.start_offset +=
          (fixed_max_breadth + set_geometry.gutter_size) *
          current_set.TrackCount();
    } else {
      // An intrinsic or flexible sizing function: Use an initial growth limit
      // of infinity.
      current_set.InitGrowthLimit(kIndefiniteSize);
      last_indefinite_index = set_geometry.sets.size() - 1;
    }

    if (track_size.HasFixedMinTrackBreadth()) {
      DCHECK(!track_size.MinTrackBreadth().HasPercentage() ||
             available_size != kIndefiniteSize);

      // A fixed sizing function: Resolve to an absolute length and use that
      // size as the track’s initial base size.
      LayoutUnit fixed_min_breadth = MinimumValueForLength(
          track_size.MinTrackBreadth().length(), available_size);
      current_set.InitBaseSize(fixed_min_breadth * current_set.TrackCount());
    } else {
      // An intrinsic sizing function: Use an initial base size of zero.
      DCHECK(track_size.HasIntrinsicMinTrackBreadth());
      current_set.InitBaseSize(LayoutUnit());
    }

    set_geometry.sets.emplace_back(track_geometry.start_offset,
                                   current_set.TrackCount());
    set_geometry.last_indefinite_indices.emplace_back(last_indefinite_index);
  }

  // If all of our tracks have a definite size upfront, we use
  // |ComputeSetGeometry| which will apply alignment (if present).
  return grid_properties.IsSpanningOnlyDefiniteTracks(track_direction)
             ? ComputeSetGeometry(*track_collection)
             : set_geometry;
}

// https://drafts.csswg.org/css-grid-2/#algo-track-sizing
SetGeometry NGGridLayoutAlgorithm::ComputeUsedTrackSizes(
    const NGGridGeometry& grid_geometry,
    const NGGridProperties& grid_properties,
    const SizingConstraint sizing_constraint,
    NGGridLayoutAlgorithmTrackCollection* track_collection,
    GridItems* grid_items) const {
  DCHECK(track_collection && grid_items);

  // 2. Resolve intrinsic track sizing functions to absolute lengths.
  if (grid_properties.HasIntrinsicTrack(track_collection->Direction())) {
    ResolveIntrinsicTrackSizes(grid_geometry, sizing_constraint,
                               track_collection, grid_items);
  }

  // If any track still has an infinite growth limit (i.e. it had no items
  // placed in it), set its growth limit to its base size before maximizing.
  track_collection->SetAllGrowthLimitsToBaseSize();

  // 3. If the free space is positive, distribute it equally to the base sizes
  // of all tracks, freezing tracks as they reach their growth limits (and
  // continuing to grow the unfrozen tracks as needed).
  MaximizeTracks(sizing_constraint, track_collection);

  // 4. This step sizes flexible tracks using the largest value it can assign to
  // an 'fr' without exceeding the available space.
  if (grid_properties.HasFlexibleTrack(track_collection->Direction())) {
    ExpandFlexibleTracks(grid_geometry, sizing_constraint, track_collection,
                         grid_items);
  }

  // 5. Stretch tracks with an 'auto' max track sizing function.
  StretchAutoTracks(sizing_constraint, track_collection);

  return ComputeSetGeometry(*track_collection);
}

// Helpers for the track sizing algorithm.
namespace {

using ClampedDouble = base::ClampedNumeric<double>;
using GridSetVector = Vector<NGGridSet*, 16>;
using SetIterator = NGGridLayoutAlgorithmTrackCollection::SetIterator;

const double kDoubleEpsilon = std::numeric_limits<float>::epsilon();

SetIterator GetSetIteratorForItem(
    const GridItemData& grid_item,
    NGGridLayoutAlgorithmTrackCollection& track_collection) {
  const auto& set_indices = grid_item.SetIndices(track_collection.Direction());
  return track_collection.GetSetIterator(set_indices.begin, set_indices.end);
}

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
  LayoutUnit planned_increase = set.PlannedIncrease();
  set.SetInfinitelyGrowable(false);

  // Only grow sets that accommodated a grid item.
  if (planned_increase == kIndefiniteSize)
    return;

  switch (contribution_type) {
    case GridItemContributionType::kForIntrinsicMinimums:
    case GridItemContributionType::kForContentBasedMinimums:
    case GridItemContributionType::kForMaxContentMinimums:
      set.SetBaseSize(set.BaseSize() + planned_increase);
      return;
    case GridItemContributionType::kForIntrinsicMaximums:
      // Mark any tracks whose growth limit changed from infinite to finite in
      // this step as infinitely growable for the next step.
      set.SetInfinitelyGrowable(set.GrowthLimit() == kIndefiniteSize);
      set.SetGrowthLimit(DefiniteGrowthLimit(set) + planned_increase);
      return;
    case GridItemContributionType::kForMaxContentMaximums:
      set.SetGrowthLimit(DefiniteGrowthLimit(set) + planned_increase);
      return;
    case GridItemContributionType::kForFreeSpace:
      NOTREACHED();
      return;
  }
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
      if (growth_limit == kIndefiniteSize)
        return kIndefiniteSize;

      LayoutUnit increased_base_size =
          set.BaseSize() + set.ItemIncurredIncrease();
      DCHECK_LE(increased_base_size, growth_limit);
      return growth_limit - increased_base_size;
    }
    case GridItemContributionType::kForIntrinsicMaximums:
    case GridItemContributionType::kForMaxContentMaximums: {
      if (infinitely_growable_behavior ==
              InfinitelyGrowableBehavior::kEnforce &&
          set.GrowthLimit() != kIndefiniteSize && !set.IsInfinitelyGrowable()) {
        // For growth limits, the potential is infinite if its value is infinite
        // too or if the set is marked as infinitely growable; otherwise, zero.
        return LayoutUnit();
      }

      LayoutUnit fit_content_limit = set.FitContentLimit();
      DCHECK(fit_content_limit >= 0 || fit_content_limit == kIndefiniteSize);

      // The max track sizing function of a 'fit-content' track is treated as
      // 'max-content' until it reaches the limit specified as the 'fit-content'
      // argument, after which it is treated as having a fixed sizing function
      // of that argument (with a growth potential of zero).
      if (fit_content_limit != kIndefiniteSize) {
        LayoutUnit growth_potential = fit_content_limit -
                                      DefiniteGrowthLimit(set) -
                                      set.ItemIncurredIncrease();
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

template <typename T>
bool AreEqual(T a, T b) {
  return a == b;
}

template <>
bool AreEqual<double>(double a, double b) {
  return std::abs(a - b) < kDoubleEpsilon;
}

// Follow the definitions from https://drafts.csswg.org/css-grid-2/#extra-space;
// notice that this method replaces the notion of "tracks" with "sets".
template <bool is_equal_distribution>
void DistributeExtraSpaceToSets(LayoutUnit extra_space,
                                double flex_factor_sum,
                                GridItemContributionType contribution_type,
                                GridSetVector* sets_to_grow,
                                GridSetVector* sets_to_grow_beyond_limit) {
  DCHECK(extra_space && sets_to_grow);

  if (extra_space == kIndefiniteSize) {
    // Infinite extra space should only happen when distributing free space at
    // the maximize tracks step; in such case, we can simplify this method by
    // "filling" every track base size up to their growth limit.
    DCHECK_EQ(contribution_type, GridItemContributionType::kForFreeSpace);
    for (auto* set : *sets_to_grow) {
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

  wtf_size_t growable_track_count = 0;
  for (auto* set : *sets_to_grow) {
    set->SetItemIncurredIncrease(LayoutUnit());

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

  using ShareRatioType = typename std::conditional<is_equal_distribution,
                                                   wtf_size_t, double>::type;
  DCHECK(is_equal_distribution ||
         !AreEqual<ShareRatioType>(flex_factor_sum, 0));
  ShareRatioType share_ratio_sum =
      is_equal_distribution ? growable_track_count : flex_factor_sum;
  const bool is_flex_factor_sum_overflowing_limits =
      share_ratio_sum >= std::numeric_limits<wtf_size_t>::max();

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
    auto CompareSetsByGrowthPotential = [contribution_type](NGGridSet* a,
                                                            NGGridSet* b) {
      LayoutUnit growth_potential_a = GrowthPotentialForSet(
          *a, contribution_type, InfinitelyGrowableBehavior::kIgnore);
      LayoutUnit growth_potential_b = GrowthPotentialForSet(
          *b, contribution_type, InfinitelyGrowableBehavior::kIgnore);

      if (growth_potential_a == kIndefiniteSize ||
          growth_potential_b == kIndefiniteSize) {
        // At this point we know that there is at least one set with infinite
        // growth potential; if |a| has a definite value, then |b| must have
        // infinite growth potential, and thus, |a| < |b|.
        return growth_potential_a != kIndefiniteSize;
      }
      // Straightforward comparison of definite growth potentials.
      return growth_potential_a < growth_potential_b;
    };
    // If we only have flex growth potential, there's no need to sort because
    // flex growth potentials are infinite.
    if (AreEqual<double>(flex_factor_sum, 0)) {
      DCHECK(is_equal_distribution);
      std::sort(sets_to_grow->begin(), sets_to_grow->end(),
                CompareSetsByGrowthPotential);
    }
  }

  auto ExtraSpaceShare = [&](const NGGridSet& set,
                             LayoutUnit growth_potential) -> LayoutUnit {
    DCHECK(growth_potential >= 0 || growth_potential == kIndefiniteSize);

    // If this set won't take a share of the extra space, e.g. has zero growth
    // potential, exit so that this set is filtered out of |share_ratio_sum|.
    if (!growth_potential)
      return LayoutUnit();

    wtf_size_t set_track_count = set.TrackCount();
    DCHECK_LE(set_track_count, growable_track_count);

    ShareRatioType set_share_ratio =
        is_equal_distribution ? set_track_count : set.FlexFactor();

    // Since |share_ratio_sum| can be greater than the wtf_size_t limit, cap the
    // value of |set_share_ratio| to prevent overflows.
    if (set_share_ratio > share_ratio_sum) {
      DCHECK(is_flex_factor_sum_overflowing_limits);
      set_share_ratio = share_ratio_sum;
    }

    LayoutUnit extra_space_share;
    if (AreEqual(set_share_ratio, share_ratio_sum)) {
      // If this set's share ratio and the remaining ratio sum are the same, it
      // means that this set will receive all of the remaining space. Hence, we
      // can optimize a little by directly using the extra space as this set's
      // share and break early by decreasing the remaining growable track count
      // to 0 (even if there are further growable tracks, since the share ratio
      // sum will be reduced to 0, their space share will also be 0).
      set_track_count = growable_track_count;
      extra_space_share = extra_space;
    } else {
      DCHECK(!AreEqual<ShareRatioType>(share_ratio_sum, 0));
      DCHECK_LT(set_share_ratio, share_ratio_sum);

      extra_space_share = LayoutUnit::FromRawValue(
          (extra_space.RawValue() * set_share_ratio) / share_ratio_sum);
    }

    if (growth_potential != kIndefiniteSize)
      extra_space_share = std::min(extra_space_share, growth_potential);
    DCHECK_LE(extra_space_share, extra_space);

    growable_track_count -= set_track_count;
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
  for (auto* set : *sets_to_grow) {
    // Break early if there are no further tracks to grow.
    if (!growable_track_count)
      break;
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
    for (auto* set : *sets_to_grow_beyond_limit) {
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

    // If we reached this point, we must have exhausted every growable track up
    // to their limits, meaning |growable_track_count| should be 0 and we need
    // to recompute it considering their 'fit-content' limits instead.
    DCHECK_EQ(growable_track_count, 0u);

    for (auto* set : *sets_to_grow_beyond_limit) {
      if (BeyondLimitsGrowthPotential(*set))
        growable_track_count += set->TrackCount();
    }

    // In |IncreaseTrackSizesToAccommodateGridItems| we guaranteed that, when
    // dealing with flexible tracks, there shouldn't be any set to grow beyond
    // limits. Thus, the only way to reach the section below is when we are
    // distributing space equally among sets.
    DCHECK(is_equal_distribution);
    share_ratio_sum = growable_track_count;

    for (auto* set : *sets_to_grow_beyond_limit) {
      // Break early if there are no further tracks to grow.
      if (!growable_track_count)
        break;
      set->SetItemIncurredIncrease(
          set->ItemIncurredIncrease() +
          ExtraSpaceShare(*set, BeyondLimitsGrowthPotential(*set)));
    }
  }
}

void DistributeExtraSpaceToSetsEqually(
    LayoutUnit extra_space,
    GridItemContributionType contribution_type,
    GridSetVector* sets_to_grow,
    GridSetVector* sets_to_grow_beyond_limit = nullptr) {
  DistributeExtraSpaceToSets</* is_equal_distribution */ true>(
      extra_space, /* flex_factor_sum */ 0, contribution_type, sets_to_grow,
      sets_to_grow_beyond_limit);
}

void DistributeExtraSpaceToWeightedSets(
    LayoutUnit extra_space,
    const double flex_factor_sum,
    GridItemContributionType contribution_type,
    GridSetVector* sets_to_grow) {
  DistributeExtraSpaceToSets</* is_equal_distribution */ false>(
      extra_space, flex_factor_sum, contribution_type, sets_to_grow,
      /* sets_to_grow_beyond_limit */ nullptr);
}

}  // namespace

void NGGridLayoutAlgorithm::IncreaseTrackSizesToAccommodateGridItems(
    GridItems::Iterator group_begin,
    GridItems::Iterator group_end,
    const NGGridGeometry& grid_geometry,
    const bool is_group_spanning_flex_track,
    const SizingConstraint sizing_constraint,
    const GridItemContributionType contribution_type,
    NGGridLayoutAlgorithmTrackCollection* track_collection) const {
  DCHECK(track_collection);
  const auto track_direction = track_collection->Direction();

  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    set_iterator.CurrentSet().SetPlannedIncrease(kIndefiniteSize);
  }

  GridSetVector sets_to_grow;
  GridSetVector sets_to_grow_beyond_limit;
  for (auto it = group_begin; it != group_end; ++it) {
    GridItemData& grid_item = *it;

    // When the grid items of this group are not spanning a flexible track, we
    // can skip the current item if it doesn't span an intrinsic track.
    if (!grid_item.IsSpanningIntrinsicTrack(track_direction) &&
        !is_group_spanning_flex_track) {
      continue;
    }

    sets_to_grow.Shrink(0);
    sets_to_grow_beyond_limit.Shrink(0);

    LayoutUnit spanned_tracks_size =
        GridGap(track_direction) * (grid_item.SpanSize(track_direction) - 1);

    ClampedDouble flex_factor_sum = 0;
    for (auto set_iterator =
             GetSetIteratorForItem(grid_item, *track_collection);
         !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
      auto& current_set = set_iterator.CurrentSet();
      spanned_tracks_size +=
          AffectedSizeForContribution(current_set, contribution_type);

      if (is_group_spanning_flex_track &&
          !current_set.TrackSize().HasFlexMaxTrackBreadth()) {
        // From https://drafts.csswg.org/css-grid-2/#algo-spanning-flex-items:
        //   Distributing space only to flexible tracks (i.e. treating all other
        //   tracks as having a fixed sizing function).
        continue;
      }

      if (IsContributionAppliedToSet(current_set, contribution_type)) {
        if (current_set.PlannedIncrease() == kIndefiniteSize)
          current_set.SetPlannedIncrease(LayoutUnit());

        if (is_group_spanning_flex_track)
          flex_factor_sum += current_set.FlexFactor();

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
        grid_geometry, sizing_constraint, track_direction, contribution_type,
        &grid_item);
    extra_space = (extra_space - spanned_tracks_size).ClampNegativeToZero();

    if (!extra_space)
      continue;

    // From https://drafts.csswg.org/css-grid-2/#algo-spanning-flex-items:
    //   If the sum of the flexible sizing functions of all flexible tracks
    //   spanned by the item is greater than zero, distributing space to such
    //   tracks according to the ratios of their flexible sizing functions
    //   rather than distributing space equally.
    if (!is_group_spanning_flex_track || AreEqual<double>(flex_factor_sum, 0)) {
      DistributeExtraSpaceToSetsEqually(
          extra_space, contribution_type, &sets_to_grow,
          sets_to_grow_beyond_limit.IsEmpty() ? &sets_to_grow
                                              : &sets_to_grow_beyond_limit);
    } else {
      // 'fr' units are only allowed as a maximum in track definitions, meaning
      // that no set has an intrinsic max track sizing function that would allow
      // it to grow beyond limits (see |ShouldUsedSizeGrowBeyondLimit|).
      DCHECK(sets_to_grow_beyond_limit.IsEmpty());
      DistributeExtraSpaceToWeightedSets(extra_space, flex_factor_sum,
                                         contribution_type, &sets_to_grow);
    }

    // For each affected track, if the track's item-incurred increase is larger
    // than its planned increase, set the planned increase to that value.
    for (auto* set : sets_to_grow) {
      DCHECK_NE(set->ItemIncurredIncrease(), kIndefiniteSize);
      DCHECK_NE(set->PlannedIncrease(), kIndefiniteSize);
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
    const NGGridGeometry& grid_geometry,
    const SizingConstraint sizing_constraint,
    NGGridLayoutAlgorithmTrackCollection* track_collection,
    GridItems* grid_items) const {
  DCHECK(track_collection && grid_items);
  const auto track_direction = track_collection->Direction();

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
        current_group_begin, current_group_end, grid_geometry,
        /* is_group_spanning_flex_track */ false, sizing_constraint,
        GridItemContributionType::kForIntrinsicMinimums, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        current_group_begin, current_group_end, grid_geometry,
        /* is_group_spanning_flex_track */ false, sizing_constraint,
        GridItemContributionType::kForContentBasedMinimums, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        current_group_begin, current_group_end, grid_geometry,
        /* is_group_spanning_flex_track */ false, sizing_constraint,
        GridItemContributionType::kForMaxContentMinimums, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        current_group_begin, current_group_end, grid_geometry,
        /* is_group_spanning_flex_track */ false, sizing_constraint,
        GridItemContributionType::kForIntrinsicMaximums, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        current_group_begin, current_group_end, grid_geometry,
        /* is_group_spanning_flex_track */ false, sizing_constraint,
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
        current_group_begin, grid_items->end(), grid_geometry,
        /* is_group_spanning_flex_track */ true, sizing_constraint,
        GridItemContributionType::kForIntrinsicMinimums, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        current_group_begin, grid_items->end(), grid_geometry,
        /* is_group_spanning_flex_track */ true, sizing_constraint,
        GridItemContributionType::kForContentBasedMinimums, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        current_group_begin, grid_items->end(), grid_geometry,
        /* is_group_spanning_flex_track */ true, sizing_constraint,
        GridItemContributionType::kForMaxContentMinimums, track_collection);
  }
}

namespace {

void GrowSetsByItemIncurredIncrease(GridSetVector* sets_to_grow) {
  for (auto* set : *sets_to_grow)
    set->SetBaseSize(set->BaseSize() + set->ItemIncurredIncrease());
}

LayoutUnit ComputeTotalTrackSize(
    const NGGridLayoutAlgorithmTrackCollection& track_collection,
    const LayoutUnit grid_gap) {
  LayoutUnit total_track_size;
  for (auto set_iterator = track_collection.GetConstSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    const auto& set = set_iterator.CurrentSet();
    total_track_size += set.BaseSize() + set.TrackCount() * grid_gap;
  }
  // Clamp to zero to avoid a negative |grid_gap| when there are no tracks.
  total_track_size -= grid_gap;
  return total_track_size.ClampNegativeToZero();
}

}  // namespace

// https://drafts.csswg.org/css-grid-2/#algo-grow-tracks
void NGGridLayoutAlgorithm::MaximizeTracks(
    const SizingConstraint sizing_constraint,
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

  DistributeExtraSpaceToSetsEqually(
      free_space, GridItemContributionType::kForFreeSpace, &sets_to_grow);
  GrowSetsByItemIncurredIncrease(&sets_to_grow);

  // TODO(ethavar): If this would cause the grid to be larger than the grid
  // container’s inner size as limited by its 'max-width/height', then redo this
  // step, treating the available grid space as equal to the grid container’s
  // inner size when it’s sized to its 'max-width/height'.
}

// https://drafts.csswg.org/css-grid-2/#algo-stretch
void NGGridLayoutAlgorithm::StretchAutoTracks(
    const SizingConstraint sizing_constraint,
    NGGridLayoutAlgorithmTrackCollection* track_collection) const {
  const auto track_direction = track_collection->Direction();

  // Stretching auto tracks should only occur if we have a "stretch" (or
  // default) content distribution.
  const auto& content_alignment = (track_direction == kForColumns)
                                      ? Style().JustifyContent()
                                      : Style().AlignContent();

  if (content_alignment.Distribution() != ContentDistributionType::kStretch &&
      (content_alignment.Distribution() != ContentDistributionType::kDefault ||
       content_alignment.GetPosition() != ContentPosition::kNormal)) {
    return;
  }

  // Expand tracks that have an 'auto' max track sizing function by dividing any
  // remaining positive, definite free space equally amongst them.
  GridSetVector sets_to_grow;
  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    auto& set = set_iterator.CurrentSet();
    if (set.TrackSize().HasAutoMaxTrackBreadth() &&
        !set.TrackSize().IsFitContent()) {
      sets_to_grow.push_back(&set);
    }
  }

  if (sets_to_grow.IsEmpty())
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
    free_space -=
        ComputeTotalTrackSize(*track_collection, GridGap(track_direction));
  }

  if (free_space <= 0)
    return;

  DistributeExtraSpaceToSetsEqually(free_space,
                                    GridItemContributionType::kForFreeSpace,
                                    &sets_to_grow, &sets_to_grow);
  GrowSetsByItemIncurredIncrease(&sets_to_grow);
}

// https://drafts.csswg.org/css-grid-2/#algo-flex-tracks
void NGGridLayoutAlgorithm::ExpandFlexibleTracks(
    const NGGridGeometry& grid_geometry,
    const SizingConstraint sizing_constraint,
    NGGridLayoutAlgorithmTrackCollection* track_collection,
    GridItems* grid_items) const {
  DCHECK(track_collection && grid_items);
  LayoutUnit free_space =
      DetermineFreeSpace(sizing_constraint, *track_collection);

  // If the free space is zero or if sizing the grid container under a
  // min-content constraint, the used flex fraction is zero.
  if (!free_space)
    return;

  const auto track_direction = track_collection->Direction();
  const LayoutUnit gutter_size =
      grid_geometry.Geometry(track_direction).gutter_size;

  // https://drafts.csswg.org/css-grid-2/#algo-find-fr-size
  GridSetVector flexible_sets;
  auto FindFrSize = [&](SetIterator set_iterator,
                        LayoutUnit leftover_space) -> double {
    ClampedDouble flex_factor_sum = 0;
    wtf_size_t total_track_count = 0;
    flexible_sets.Shrink(0);

    while (!set_iterator.IsAtEnd()) {
      auto& set = set_iterator.CurrentSet();
      if (set.TrackSize().HasFlexMaxTrackBreadth() &&
          !AreEqual<double>(set.FlexFactor(), 0)) {
        flex_factor_sum += set.FlexFactor();
        flexible_sets.push_back(&set);
      } else {
        leftover_space -= set.BaseSize();
      }
      total_track_count += set.TrackCount();
      set_iterator.MoveToNextSet();
    }

    // Remove the gutters between spanned tracks.
    leftover_space -= gutter_size * (total_track_count - 1);

    if (leftover_space < 0 || flexible_sets.IsEmpty())
      return 0;

    // From css-grid-2 spec: "If the product of the hypothetical fr size and
    // a flexible track’s flex factor is less than the track’s base size,
    // restart this algorithm treating all such tracks as inflexible."
    //
    // We will process the same algorithm a bit different; since we define the
    // hypothetical fr size as the leftover space divided by the flex factor
    // sum, we can reinterpret the statement above as follows:
    //
    //   (leftover space / flex factor sum) * flexible set's flex factor <
    //       flexible set's base size
    //
    // Reordering the terms of such expression we get:
    //
    //   leftover space / flex factor sum <
    //       flexible set's base size / flexible set's flex factor
    //
    // The term on the right is constant for every flexible set, while the term
    // on the left changes whenever we restart the algorithm treating some of
    // those sets as inflexible. Note that, if the expression above is false for
    // a given set, any other set with a lesser (base size / flex factor) ratio
    // will also fail such expression.
    //
    // Based on this observation, we can process the sets in non-increasing
    // ratio, when the current set does not fulfill the expression, no further
    // set will fulfill it either (and we can return the hypothetical fr size).
    // Otherwise, determine which sets should be treated as inflexible, exclude
    // them from the leftover space and flex factor sum computation, and keep
    // checking the condition for sets with lesser ratios.
    auto CompareSetsByBaseSizeFlexFactorRatio = [](NGGridSet* a,
                                                   NGGridSet* b) -> bool {
      // Avoid divisions by reordering the terms of the comparison.
      return a->BaseSize().RawValue() * b->FlexFactor() >
             b->BaseSize().RawValue() * a->FlexFactor();
    };
    std::sort(flexible_sets.begin(), flexible_sets.end(),
              CompareSetsByBaseSizeFlexFactorRatio);

    GridSetVector::iterator current_set = flexible_sets.begin();
    while (leftover_space > 0 && current_set != flexible_sets.end()) {
      flex_factor_sum = base::ClampMax(flex_factor_sum, 1);

      GridSetVector::iterator next_set = current_set;
      while (next_set != flexible_sets.end() &&
             (*next_set)->FlexFactor() * leftover_space.RawValue() <
                 (*next_set)->BaseSize().RawValue() * flex_factor_sum) {
        ++next_set;
      }

      // Any upcoming flexible set will receive a share of free space of at
      // least their base size; return the current hypothetical fr size.
      if (current_set == next_set) {
        DCHECK(!AreEqual<double>(flex_factor_sum, 0));
        return leftover_space.RawValue() / flex_factor_sum;
      }

      // Otherwise, treat all those sets that does not receive a share of free
      // space of at least their base size as inflexible, effectively excluding
      // them from the leftover space and flex factor sum computation.
      for (GridSetVector::iterator it = current_set; it != next_set; ++it) {
        flex_factor_sum -= (*it)->FlexFactor();
        leftover_space -= (*it)->BaseSize();
      }
      current_set = next_set;
    }
    return 0;
  };

  double fr_size = 0;
  if (free_space != kIndefiniteSize) {
    // Otherwise, if the free space is a definite length, the used flex fraction
    // is the result of finding the size of an fr using all of the grid tracks
    // and a space to fill of the available grid space.
    fr_size = FindFrSize(track_collection->GetSetIterator(),
                         (track_direction == kForColumns)
                             ? grid_available_size_.inline_size
                             : grid_available_size_.block_size);
  } else {
    // Otherwise, if the free space is an indefinite length, the used flex
    // fraction is the maximum of:
    //   - For each grid item that crosses a flexible track, the result of
    //   finding the size of an fr using all the grid tracks that the item
    //   crosses and a space to fill of the item’s max-content contribution.
    for (auto& grid_item : grid_items->item_data) {
      if (grid_item.IsSpanningFlexibleTrack(track_direction)) {
        double grid_item_fr_size = FindFrSize(
            GetSetIteratorForItem(grid_item, *track_collection),
            ContributionSizeForGridItem(
                grid_geometry, sizing_constraint, track_direction,
                GridItemContributionType::kForMaxContentMaximums, &grid_item));
        fr_size = std::max(grid_item_fr_size, fr_size);
      }
    }

    //   - For each flexible track, if the flexible track’s flex factor is
    //   greater than one, the result of dividing the track’s base size by its
    //   flex factor; otherwise, the track’s base size.
    for (auto set_iterator = track_collection->GetConstSetIterator();
         !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
      auto& set = set_iterator.CurrentSet();
      if (!set.TrackSize().HasFlexMaxTrackBreadth())
        continue;

      DCHECK_GT(set.TrackCount(), 0u);
      double set_flex_factor =
          base::ClampMax(set.FlexFactor(), set.TrackCount());
      fr_size = std::max(set.BaseSize().RawValue() / set_flex_factor, fr_size);
    }
  }

  // Notice that the fr size multiplied by a set's flex factor can result in a
  // non-integer size; since we floor the expanded size to fit in a LayoutUnit,
  // when multiple sets lose the fractional part of the computation we may not
  // distribute the entire free space. We fix this issue by accumulating the
  // leftover fractional part from every flexible set.
  double leftover_size = 0;

  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    auto& set = set_iterator.CurrentSet();
    if (!set.TrackSize().HasFlexMaxTrackBreadth())
      continue;

    const ClampedDouble fr_share = fr_size * set.FlexFactor() + leftover_size;
    // Add an epsilon to round up values very close to the next integer.
    const LayoutUnit expanded_size =
        LayoutUnit::FromRawValue(fr_share + kDoubleEpsilon);

    if (!expanded_size.MightBeSaturated() && expanded_size >= set.BaseSize()) {
      set.SetBaseSize(expanded_size);
      // The epsilon added above might make |expanded_size| greater than
      // |fr_share|, in that case avoid a negative leftover by flooring to 0.
      leftover_size = base::ClampMax(fr_share - expanded_size.RawValue(), 0);
    }
  }

  // TODO(ethavar): If using this flex fraction would cause the grid to be
  // smaller than the grid container’s min-width/height (or larger than the grid
  // container’s max-width/height), then redo this step, treating the free space
  // as definite and the available grid space as equal to the grid container’s
  // inner size when it’s sized to its min-width/height (max-width/height).
}

namespace {

TrackGeometry ComputeFirstTrackInCollectionGeometry(
    const ComputedStyle& style,
    const StyleContentAlignmentData& content_alignment,
    const NGGridLayoutAlgorithmTrackCollection& track_collection,
    LayoutUnit available_size,
    LayoutUnit start_border_scrollbar_padding,
    LayoutUnit grid_gap) {
  const OverflowAlignment overflow = content_alignment.Overflow();
  // Determining the free-space is typically unnecessary, i.e. if there is
  // default alignment. Only compute this on-demand.
  auto FreeSpace = [&]() -> LayoutUnit {
    LayoutUnit free_space =
        available_size - ComputeTotalTrackSize(track_collection, grid_gap);
    // If overflow is 'safe', we have to make sure we don't overflow the
    // 'start' edge (potentially cause some data loss as the overflow is
    // unreachable).
    return (overflow == OverflowAlignment::kSafe)
               ? free_space.ClampNegativeToZero()
               : free_space;
  };
  // The default alignment, perform adjustments on top of this.
  TrackGeometry geometry = {start_border_scrollbar_padding, grid_gap};

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
      const wtf_size_t track_count = track_collection.NonCollapsedTrackCount();
      const LayoutUnit free_space = FreeSpace();
      if (track_count < 2 || free_space < LayoutUnit())
        return geometry;

      geometry.gutter_size += free_space / (track_count - 1);
      return geometry;
    }
    case ContentDistributionType::kSpaceAround: {
      // Default behavior for 'space-around' is to center content.
      const wtf_size_t track_count = track_collection.NonCollapsedTrackCount();
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
      // Default behavior for 'space-evenly' is to center content.
      const wtf_size_t track_count = track_collection.NonCollapsedTrackCount();
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
SetGeometry NGGridLayoutAlgorithm::ComputeSetGeometry(
    const NGGridLayoutAlgorithmTrackCollection& track_collection) const {
  const LayoutUnit available_size = track_collection.IsForColumns()
                                        ? grid_available_size_.inline_size
                                        : grid_available_size_.block_size;
  TrackGeometry track_geometry =
      track_collection.IsForColumns()
          ? ComputeFirstTrackInCollectionGeometry(
                Style(), Style().JustifyContent(), track_collection,
                available_size, BorderScrollbarPadding().inline_start,
                GridGap(kForColumns))
          : ComputeFirstTrackInCollectionGeometry(
                Style(), Style().AlignContent(), track_collection,
                available_size, BorderScrollbarPadding().block_start,
                GridGap(kForRows));

  SetGeometry set_geometry(track_geometry, track_collection.SetCount() + 1);
  for (auto set_iterator = track_collection.GetConstSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    const auto& set = set_iterator.CurrentSet();
    track_geometry.start_offset +=
        set.BaseSize() + set.TrackCount() * set_geometry.gutter_size;
    set_geometry.sets.emplace_back(track_geometry.start_offset,
                                   set.TrackCount());
  }
  return set_geometry;
}

LayoutUnit NGGridLayoutAlgorithm::GridGap(
    const GridTrackSizingDirection track_direction) const {
  const absl::optional<Length>& gap =
      (track_direction == kForColumns) ? Style().ColumnGap() : Style().RowGap();

  if (!gap)
    return LayoutUnit();

  LayoutUnit available_size =
      ((track_direction == kForColumns) ? grid_available_size_.inline_size
                                        : grid_available_size_.block_size)
          .ClampIndefiniteToZero();

  return MinimumValueForLength(*gap, available_size);
}

// TODO(ikilpatrick): Determine if other uses of this method need to respect
// |grid_min_available_size_| similar to |StretchAutoTracks|.
LayoutUnit NGGridLayoutAlgorithm::DetermineFreeSpace(
    SizingConstraint sizing_constraint,
    const NGGridLayoutAlgorithmTrackCollection& track_collection) const {
  const auto track_direction = track_collection.Direction();

  // https://drafts.csswg.org/css-sizing-3/#auto-box-sizes: both min-content and
  // max-content block sizes are the size of the content after layout.
  if (track_direction == kForRows)
    sizing_constraint = SizingConstraint::kLayout;

  switch (sizing_constraint) {
    case SizingConstraint::kLayout: {
      LayoutUnit free_space = (track_direction == kForColumns)
                                  ? grid_available_size_.inline_size
                                  : grid_available_size_.block_size;
      if (free_space != kIndefiniteSize) {
        free_space -=
            ComputeTotalTrackSize(track_collection, GridGap(track_direction));
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
                           LayoutUnit baseline_offset,
                           AxisEdge axis_edge,
                           bool is_overflow_safe) {
  LayoutUnit free_space = container_size - size - margin_start - margin_end;
  // If overflow is 'safe', we have to make sure we don't overflow the
  // 'start' edge (potentially cause some data loss as the overflow is
  // unreachable).
  if (is_overflow_safe)
    free_space = free_space.ClampNegativeToZero();
  switch (axis_edge) {
    case AxisEdge::kStart:
      return margin_start;
    case AxisEdge::kCenter:
      return margin_start + (free_space / 2);
    case AxisEdge::kEnd:
      return margin_start + free_space;
    case AxisEdge::kBaseline:
      return baseline_offset;
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
    case AxisEdge::kBaseline:
      *inline_edge = InlineEdge::kInlineStart;
      break;
    case AxisEdge::kCenter:
      *inline_edge = InlineEdge::kInlineCenter;
      offset->inline_offset += container_size.inline_size / 2;
      break;
    case AxisEdge::kEnd:
      *inline_edge = InlineEdge::kInlineEnd;
      offset->inline_offset += container_size.inline_size;
      break;
  }

  switch (block_axis_edge) {
    case AxisEdge::kStart:
    case AxisEdge::kBaseline:
      *block_edge = BlockEdge::kBlockStart;
      break;
    case AxisEdge::kCenter:
      *block_edge = BlockEdge::kBlockCenter;
      offset->block_offset += container_size.block_size / 2;
      break;
    case AxisEdge::kEnd:
      *block_edge = BlockEdge::kBlockEnd;
      offset->block_offset += container_size.block_size;
      break;
  }
}

}  // namespace

const NGConstraintSpace NGGridLayoutAlgorithm::CreateConstraintSpace(
    const GridItemData& grid_item,
    const LogicalSize& containing_grid_area_size,
    NGCacheSlot cache_slot,
    absl::optional<LayoutUnit> opt_fixed_block_size,
    absl::optional<LayoutUnit> opt_fragment_relative_block_offset,
    bool opt_min_block_size_should_encompass_intrinsic_size) const {
  NGConstraintSpaceBuilder builder(
      ConstraintSpace(), grid_item.node.Style().GetWritingDirection(),
      /* is_new_fc */ true, /* adjust_inline_size_if_needed */ false);

  builder.SetCacheSlot(cache_slot);
  builder.SetIsPaintedAtomically(true);
  if (opt_fixed_block_size) {
    builder.SetAvailableSize(
        {containing_grid_area_size.inline_size, *opt_fixed_block_size});
    builder.SetIsFixedBlockSize(true);
  } else {
    builder.SetAvailableSize(containing_grid_area_size);
  }
  builder.SetPercentageResolutionSize(containing_grid_area_size);
  builder.SetInlineAutoBehavior(grid_item.inline_auto_behavior);
  builder.SetBlockAutoBehavior(grid_item.block_auto_behavior);

  if (ConstraintSpace().HasBlockFragmentation() &&
      opt_fragment_relative_block_offset) {
    if (opt_min_block_size_should_encompass_intrinsic_size)
      builder.SetMinBlockSizeShouldEncompassIntrinsicSize();
    SetupSpaceBuilderForFragmentation(
        ConstraintSpace(), grid_item.node, *opt_fragment_relative_block_offset,
        &builder,
        /* is_new_fc */ true,
        container_builder_.RequiresContentBeforeBreaking());
  }

  return builder.ToConstraintSpace();
}

const NGConstraintSpace NGGridLayoutAlgorithm::CreateConstraintSpaceForLayout(
    const NGGridGeometry& grid_geometry,
    const GridItemData& grid_item,
    LogicalRect* containing_grid_area,
    absl::optional<LayoutUnit> opt_fragment_relative_block_offset,
    bool opt_min_block_size_should_encompass_intrinsic_size) const {
  ComputeGridItemOffsetAndSize(grid_item, grid_geometry.column_geometry,
                               kForColumns,
                               &containing_grid_area->offset.inline_offset,
                               &containing_grid_area->size.inline_size);
  ComputeGridItemOffsetAndSize(grid_item, grid_geometry.row_geometry, kForRows,
                               &containing_grid_area->offset.block_offset,
                               &containing_grid_area->size.block_size);
  return CreateConstraintSpace(
      grid_item, containing_grid_area->size, NGCacheSlot::kLayout,
      /* opt_fixed_block_size */ absl::nullopt,
      opt_fragment_relative_block_offset,
      opt_min_block_size_should_encompass_intrinsic_size);
}

const NGConstraintSpace NGGridLayoutAlgorithm::CreateConstraintSpaceForMeasure(
    const GridItemData& grid_item,
    const NGGridGeometry& grid_geometry,
    const GridTrackSizingDirection track_direction,
    absl::optional<LayoutUnit> opt_fixed_block_size) const {
  LogicalOffset unused_offset;
  LogicalSize containing_grid_area_size(kIndefiniteSize, kIndefiniteSize);

  if (track_direction == kForColumns) {
    ComputeGridItemOffsetAndSize(grid_item, grid_geometry.row_geometry,
                                 kForRows, &unused_offset.block_offset,
                                 &containing_grid_area_size.block_size);
  } else {
    ComputeGridItemOffsetAndSize(grid_item, grid_geometry.column_geometry,
                                 kForColumns, &unused_offset.inline_offset,
                                 &containing_grid_area_size.inline_size);
  }
  return CreateConstraintSpace(grid_item, containing_grid_area_size,
                               NGCacheSlot::kMeasure, opt_fixed_block_size);
}

namespace {

// Determining the grid's baseline is prioritized based on grid order (as
// opposed to DOM order). The baseline of the grid is determined by the first
// grid item with baseline alignment in the first row. If no items have
// baseline alignment, fall back to the first item in row-major order.
class BaselineAccumulator {
  STACK_ALLOCATED();

 public:
  void Accumulate(const GridItemData& grid_item,
                  const NGBoxFragment& fragment,
                  const LayoutUnit block_offset) {
    // Compares GridArea objects in row-major grid order for baseline
    // precedence. Returns 'true' if |a| < |b| and 'false' otherwise.
    auto IsBeforeInGridOrder = [&](const GridArea& a,
                                   const GridArea& b) -> bool {
      // Do not consider items that span tracks for container baselines.
      if (a.rows.IntegerSpan() > 1 || a.columns.IntegerSpan() > 1 ||
          b.rows.IntegerSpan() > 1 || b.columns.IntegerSpan() > 1) {
        return false;
      }
      return (a.rows < b.rows) || (a.rows == b.rows && (a.columns < b.columns));
    };

    const LayoutUnit baseline =
        block_offset + fragment.Baseline().value_or(fragment.BlockSize());
    if (grid_item.IsBaselineSpecifiedForDirection(kForRows)) {
      if (!alignment_baseline_ ||
          IsBeforeInGridOrder(grid_item.resolved_position,
                              alignment_baseline_->resolved_position)) {
        alignment_baseline_.emplace(grid_item.resolved_position, baseline);
      }
    } else if (!fallback_baseline_ ||
               IsBeforeInGridOrder(grid_item.resolved_position,
                                   fallback_baseline_->resolved_position)) {
      fallback_baseline_.emplace(grid_item.resolved_position, baseline);
    }
  }

  absl::optional<LayoutUnit> Baseline() const {
    if (alignment_baseline_)
      return alignment_baseline_->baseline;
    if (fallback_baseline_)
      return fallback_baseline_->baseline;
    return absl::nullopt;
  }

 private:
  struct PositionAndBaseline {
    PositionAndBaseline(const GridArea& resolved_position, LayoutUnit baseline)
        : resolved_position(resolved_position), baseline(baseline) {}
    GridArea resolved_position;
    LayoutUnit baseline;
  };

  absl::optional<PositionAndBaseline> alignment_baseline_;
  absl::optional<PositionAndBaseline> fallback_baseline_;
};

}  // namespace

void NGGridLayoutAlgorithm::PlaceGridItems(
    const GridItems& grid_items,
    const NGGridGeometry& grid_geometry,
    Vector<GridItemOffsets>* out_offsets,
    Vector<EBreakBetween>* out_row_break_between) {
  const auto& container_space = ConstraintSpace();
  const auto container_writing_direction =
      container_space.GetWritingDirection();

  BaselineAccumulator baseline_accumulator;

  for (const auto& grid_item : grid_items.item_data) {
    LogicalRect containing_grid_area;
    const NGConstraintSpace space = CreateConstraintSpaceForLayout(
        grid_geometry, grid_item, &containing_grid_area);

    const auto& item_style = grid_item.node.Style();
    const auto margins = ComputeMarginsFor(space, item_style, container_space);

    auto result = grid_item.node.Layout(space);
    const auto& physical_fragment =
        To<NGPhysicalBoxFragment>(result->PhysicalFragment());
    NGBoxFragment logical_fragment(item_style.GetWritingDirection(),
                                   physical_fragment);

    auto BaselineOffset =
        [&](const GridTrackSizingDirection track_direction) -> LayoutUnit {
      if (grid_item.IsBaselineAlignedForDirection(track_direction)) {
        // The baseline offset is the difference between the grid item's
        // baseline and its track baseline.
        const LayoutUnit item_baseline = GetLogicalBaseline(
            logical_fragment, track_direction,
            ConstraintSpace().GetWritingDirection().GetWritingMode());
        LayoutUnit track_baseline =
            Baseline(grid_geometry, grid_item, track_direction);
        return (track_baseline != LayoutUnit::Min())
                   ? (track_baseline - item_baseline)
                   : item_baseline;
      }
      return (track_direction == kForColumns) ? margins.inline_start
                                              : margins.block_start;
    };

    LayoutUnit inline_baseline_offset = BaselineOffset(kForColumns);
    LayoutUnit block_baseline_offset = BaselineOffset(kForRows);

    // Apply the grid-item's alignment (if any).
    NGBoxFragment fragment(container_writing_direction, physical_fragment);
    containing_grid_area.offset += LogicalOffset(
        AlignmentOffset(containing_grid_area.size.inline_size,
                        fragment.InlineSize(), margins.inline_start,
                        margins.inline_end, inline_baseline_offset,
                        grid_item.InlineAxisAlignment(),
                        grid_item.is_inline_axis_overflow_safe),
        AlignmentOffset(containing_grid_area.size.block_size,
                        fragment.BlockSize(), margins.block_start,
                        margins.block_end, block_baseline_offset,
                        grid_item.BlockAxisAlignment(),
                        grid_item.is_block_axis_overflow_safe));

    // Grid is special in that %-based offsets resolve against the grid-area.
    // Determine the relative offset here (instead of in the builder). This is
    // safe as grid *also* has special inflow-bounds logic (otherwise this
    // wouldn't work).
    absl::optional<LogicalOffset> relative_offset = LogicalOffset();
    if (item_style.GetPosition() == EPosition::kRelative) {
      *relative_offset += ComputeRelativeOffsetForBoxFragment(
          physical_fragment, container_writing_direction,
          containing_grid_area.size);
    }

    NGBlockNode(grid_item.node).StoreMargins(container_space, margins);

    // If |out_offsets| is present we just want to record the initial position
    // of all the children for the purposes of fragmentation. Don't add these
    // to the builder.
    if (out_offsets) {
      out_offsets->emplace_back(containing_grid_area.offset, *relative_offset);
    } else {
      container_builder_.AddResult(*result, containing_grid_area.offset,
                                   relative_offset);
      baseline_accumulator.Accumulate(grid_item, fragment,
                                      containing_grid_area.offset.block_offset);
    }

    if (out_row_break_between) {
      auto item_break_before = JoinFragmentainerBreakValues(
          item_style.BreakBefore(), result->InitialBreakBefore());
      auto item_break_after = JoinFragmentainerBreakValues(
          item_style.BreakAfter(), result->FinalBreakAfter());

      const auto& set_indices = grid_item.SetIndices(kForRows);
      (*out_row_break_between)[set_indices.begin] =
          JoinFragmentainerBreakValues(
              (*out_row_break_between)[set_indices.begin], item_break_before);
      (*out_row_break_between)[set_indices.end] = JoinFragmentainerBreakValues(
          (*out_row_break_between)[set_indices.end], item_break_after);
    }
  }

  // Propagate the baseline from the appropriate child.
  if (auto baseline = baseline_accumulator.Baseline())
    container_builder_.SetBaseline(*baseline);
}

void NGGridLayoutAlgorithm::PlaceGridItemsForFragmentation(
    const GridItems& grid_items,
    const Vector<EBreakBetween>& row_break_between,
    NGGridGeometry* grid_geometry,
    Vector<GridItemOffsets>* offsets,
    Vector<LayoutUnit>* row_offset_adjustments,
    LayoutUnit* intrinsic_block_size) {
  DCHECK(grid_geometry && offsets && row_offset_adjustments &&
         intrinsic_block_size);

  // TODO(ikilpatrick): Update SetHasSeenAllChildren and early exit if true.
  const auto container_writing_direction =
      ConstraintSpace().GetWritingDirection();

  // The following roughly comes from:
  // https://drafts.csswg.org/css-grid-1/#fragmentation-alg
  //
  // We are interested in cases where the grid-item *may* expand due to
  // fragmentation (lines pushed down by a fragmentation line, etc).
  auto MinBlockSizeShouldEncompassIntrinsicSize =
      [&](const GridItemData& grid_item) -> bool {
    if (grid_item.node.IsMonolithic())
      return false;

    const auto& item_style = grid_item.node.Style();

    // NOTE: We currently assume that writing-mode roots are monolithic, but
    // this may change in the future.
    DCHECK_EQ(container_writing_direction.GetWritingMode(),
              item_style.GetWritingMode());

    // Only allow growth on "auto" block-size items, (a fixed block-size item
    // can't grow).
    if (!item_style.LogicalHeight().IsAutoOrContentOrIntrinsic())
      return false;

    // Only allow growth on items which only span a single row.
    if (grid_item.SpanSize(kForRows) > 1)
      return false;

    // If we have a fixed maximum track, we assume that we've hit this maximum,
    // and as such shouldn't grow.
    if (grid_item.IsSpanningFixedMaximumTrack(kForRows) &&
        !grid_item.IsSpanningIntrinsicTrack(kForRows))
      return false;

    return !grid_item.IsSpanningFixedMinimumTrack(kForRows) ||
           Style().LogicalHeight().IsAutoOrContentOrIntrinsic();
  };

  struct ResultAndOffsets {
    ResultAndOffsets(scoped_refptr<const NGLayoutResult> result,
                     LogicalOffset offset,
                     LogicalOffset relative_offset)
        : result(std::move(result)),
          offset(offset),
          relative_offset(relative_offset) {}
    scoped_refptr<const NGLayoutResult> result;
    LogicalOffset offset;
    LogicalOffset relative_offset;
  };

  Vector<ResultAndOffsets> result_and_offsets;
  BaselineAccumulator baseline_accumulator;
  LayoutUnit max_row_expansion;
  wtf_size_t expansion_row_set_index;
  wtf_size_t breakpoint_row_set_index;
  bool has_subsequent_children;

  const LayoutUnit fragmentainer_space =
      FragmentainerSpaceAtBfcStart(ConstraintSpace());
  const LayoutUnit previous_consumed_block_size =
      BreakToken() ? BreakToken()->ConsumedBlockSize() : LayoutUnit();
  base::span<const Member<const NGBreakToken>> child_break_tokens;
  if (BreakToken())
    child_break_tokens = BreakToken()->ChildBreakTokens();

  auto PlaceItems = [&]() {
    // Reset our state.
    result_and_offsets = Vector<ResultAndOffsets>();
    baseline_accumulator = BaselineAccumulator();
    max_row_expansion = LayoutUnit();
    expansion_row_set_index = kNotFound;
    breakpoint_row_set_index = kNotFound;
    has_subsequent_children = false;

    auto child_break_token_it = child_break_tokens.begin();
    auto* offsets_it = offsets->begin();

    for (const auto& grid_item : grid_items.item_data) {
      // Grab the offsets and break-token (if present) for this child.
      const GridItemOffsets item_offsets = *(offsets_it++);
      const NGBlockBreakToken* break_token = nullptr;
      if (child_break_token_it != child_break_tokens.end()) {
        if ((*child_break_token_it)->InputNode() == grid_item.node)
          break_token = To<NGBlockBreakToken>((child_break_token_it++)->Get());
      }

      const LayoutUnit fragment_relative_block_offset =
          break_token
              ? LayoutUnit()
              : item_offsets.offset.block_offset - previous_consumed_block_size;
      const bool min_block_size_should_encompass_intrinsic_size =
          MinBlockSizeShouldEncompassIntrinsicSize(grid_item);
      LogicalRect grid_area;
      const auto space = CreateConstraintSpaceForLayout(
          *grid_geometry, grid_item, &grid_area, fragment_relative_block_offset,
          min_block_size_should_encompass_intrinsic_size);

      // Make the grid area relative to this fragment.
      const auto item_row_set_index = grid_item.SetIndices(kForRows).begin;
      grid_area.offset.block_offset +=
          (*row_offset_adjustments)[item_row_set_index] -
          previous_consumed_block_size;

      // Check to see if this child should be placed within this fragmentainer.
      // We base this calculation on the grid-area rather than the offset.
      // The row can either be:
      //  - Above, we've handled it already in a previous fragment.
      //  - Below, we'll handle it within a subsequent fragment.
      //
      // NOTE: Basing this calculation of the row position has the effect that
      // a child with a negative margin will be placed in the fragmentainer
      // with its row, but placed above the block-start edge of the
      // fragmentainer.
      if (fragmentainer_space != kIndefiniteSize &&
          grid_area.offset.block_offset >= fragmentainer_space) {
        has_subsequent_children = true;
        continue;
      }
      if (grid_area.offset.block_offset < LayoutUnit() && !break_token)
        continue;

      auto result = grid_item.node.Layout(space, break_token);
      result_and_offsets.emplace_back(
          result,
          LogicalOffset(item_offsets.offset.inline_offset,
                        fragment_relative_block_offset),
          item_offsets.relative_offset);

      const NGBoxFragment fragment(
          container_writing_direction,
          To<NGPhysicalBoxFragment>(result->PhysicalFragment()));
      baseline_accumulator.Accumulate(grid_item, fragment,
                                      fragment_relative_block_offset);

      // If the row has container separation we are able to push it into the
      // next fragmentainer. If it doesn't we, need to take the current
      // breakpoint (even if it is undesirable).
      const bool row_has_container_separation =
          grid_area.offset.block_offset > LayoutUnit();

      if (row_has_container_separation &&
          item_row_set_index < breakpoint_row_set_index) {
        // The row may have a forced break, move it to the next fragmentainer.
        if (IsForcedBreakValue(ConstraintSpace(),
                               row_break_between[item_row_set_index])) {
          container_builder_.SetHasForcedBreak();
          breakpoint_row_set_index = item_row_set_index;
          continue;
        }

        // TODO(ikilpatrick): Implement a grid specific version of
        // |CalculateBreakAppealBefore| to pass into |MovePastBreakpoint|.
        if (!MovePastBreakpoint(ConstraintSpace(), grid_item.node, *result,
                                fragment_relative_block_offset,
                                kBreakAppealPerfect, /* builder */ nullptr)) {
          // TODO(ikilpatrick): We may have break-before:avoid on this row, we
          // should search upwards (ensuring that we are still in this
          // fragmentainer), for the first row with the highest break appeal.
          breakpoint_row_set_index = item_row_set_index;
          continue;
        }
      }

      // This item may want to expand due to fragmentation. Record how much we
      // should grow the row by (if applicable).
      // Only do this if this row (currently) ends (but does not start) in this
      // fragment.
      if (min_block_size_should_encompass_intrinsic_size &&
          grid_area.offset.block_offset < LayoutUnit() &&
          fragmentainer_space != kIndefiniteSize &&
          grid_area.BlockEndOffset() <= fragmentainer_space) {
        if (expansion_row_set_index == kNotFound)
          expansion_row_set_index = item_row_set_index;
        else
          DCHECK_EQ(expansion_row_set_index, item_row_set_index);

        LayoutUnit item_expansion;
        if (result->PhysicalFragment().BreakToken()) {
          // This item may have a break, and will want to expand into the next
          // fragmentainer, (causing the row to expand into the next
          // fragmentainer). We can't use the size of the fragment, as we don't
          // know how large the subsequent fragments will be (and how much
          // they'll expand the row).
          //
          // Instead of using the size of the fragment, expand the row to the
          // rest of the fragmentainer, with an additional epsilon. This epsilon
          // will ensure that we continue layout for children in this row in
          // the next fragmentainer. Without it we'd drop those subsequent
          // fragments.
          item_expansion =
              (fragmentainer_space - grid_area.BlockEndOffset()).AddEpsilon();
        } else {
          item_expansion = fragment.BlockSize() - grid_area.BlockEndOffset();
        }

        max_row_expansion = std::max(max_row_expansion, item_expansion);
      }
    }
  };

  // This will adjust the pre-computed item-offset (for all items starting at
  // |row_index| and below) by |delta|.
  auto AdjustItemOffsets = [&](wtf_size_t row_index, LayoutUnit delta) {
    auto* items_it = grid_items.item_data.begin();
    for (auto& offset : *offsets) {
      if ((items_it++)->SetIndices(kForRows).begin < row_index)
        continue;
      offset.offset.block_offset += delta;
    }
  };

  // Adjust our grid break-token data to accommodate the larger item in the row.
  // Returns true if this function adjusted the break-token data in any way.
  auto ExpandRow = [&]() -> bool {
    if (max_row_expansion == LayoutUnit())
      return false;
    DCHECK_GT(max_row_expansion, LayoutUnit());

    *intrinsic_block_size += max_row_expansion;
    AdjustItemOffsets(expansion_row_set_index + 1, max_row_expansion);

    // Expand the positions of all the sets by the increase.
    auto* it =
        grid_geometry->row_geometry.sets.begin() + expansion_row_set_index + 1;
    while (it != grid_geometry->row_geometry.sets.end())
      (it++)->offset += max_row_expansion;
    return true;
  };

  // Shifts the row where we wish to take a breakpoint (indicated by
  // |breakpoint_row_set_index|) into the next fragmentainer.
  // Returns true if this function adjusted the break-token data in any way.
  auto ShiftBreakpointIntoNextFragmentainer = [&]() -> bool {
    if (breakpoint_row_set_index == kNotFound)
      return false;
    DCHECK_NE(fragmentainer_space, kIndefiniteSize);

    const LayoutUnit fragment_relative_row_offset =
        grid_geometry->row_geometry.sets[breakpoint_row_set_index].offset +
        (*row_offset_adjustments)[breakpoint_row_set_index] -
        previous_consumed_block_size;
    const LayoutUnit row_offset_delta =
        fragmentainer_space - fragment_relative_row_offset;

    // An expansion may have occurred in |ExpandRow| which already pushed this
    // row into the next fragmentainer.
    if (row_offset_delta <= LayoutUnit())
      return false;

    *intrinsic_block_size += row_offset_delta;
    AdjustItemOffsets(breakpoint_row_set_index, row_offset_delta);

    auto* it = row_offset_adjustments->begin() + breakpoint_row_set_index;
    while (it != row_offset_adjustments->end())
      *(it++) += row_offset_delta;
    return true;
  };

  PlaceItems();

  // See if we need to expand any rows, and if so re-run |PlaceItems()|. Only
  // allow row expansion once.
  if (ExpandRow())
    PlaceItems();

  // See if we need to take a row break-point, and if-so re-run |PlaceItems()|.
  // We only need to do this once.
  if (ShiftBreakpointIntoNextFragmentainer())
    PlaceItems();

  if (has_subsequent_children)
    container_builder_.SetHasSubsequentChildren();

  // Add all the results into the builder.
  for (auto& result_and_offset : result_and_offsets) {
    container_builder_.AddResult(*result_and_offset.result,
                                 result_and_offset.offset,
                                 result_and_offset.relative_offset);
  }

  // Propagate the baseline from the appropriate child.
  if (auto baseline = baseline_accumulator.Baseline())
    container_builder_.SetBaseline(*baseline);
}

void NGGridLayoutAlgorithm::PlaceOutOfFlowItems(
    const NGGridLayoutData& layout_data,
    const LayoutUnit block_size) {
  const auto& node = Node();
  const auto& container_style = Style();

  const LogicalSize fragment_size(container_builder_.InlineSize(), block_size);
  const auto default_containing_block_size =
      ShrinkLogicalSize(fragment_size, BorderScrollbarPadding());

  // TODO(ikilpatrick): Only add candidates within this fragment.
  for (auto child = node.FirstChild(); child; child = child.NextSibling()) {
    if (!child.IsOutOfFlowPositioned())
      continue;

    auto out_of_flow_item =
        InitializeGridItem(To<NGBlockNode>(child), container_style,
                           ConstraintSpace().GetWritingMode());
    absl::optional<LogicalRect> containing_block_rect;

    if (out_of_flow_item.IsGridContainingBlock()) {
      containing_block_rect = ComputeOutOfFlowItemContainingRect(
          container_style, node.CachedPlacementData(), layout_data,
          container_builder_.Borders(), border_box_size_, block_size,
          &out_of_flow_item);
    }

    auto child_offset = containing_block_rect
                            ? containing_block_rect->offset
                            : BorderScrollbarPadding().StartOffset();
    const auto containing_block_size = containing_block_rect
                                           ? containing_block_rect->size
                                           : default_containing_block_size;

    NGLogicalStaticPosition::InlineEdge inline_edge;
    NGLogicalStaticPosition::BlockEdge block_edge;

    AlignmentOffsetForOutOfFlow(out_of_flow_item.InlineAxisAlignment(),
                                out_of_flow_item.BlockAxisAlignment(),
                                containing_block_size, &inline_edge,
                                &block_edge, &child_offset);

    container_builder_.AddOutOfFlowChildCandidate(
        out_of_flow_item.node, child_offset, inline_edge, block_edge,
        /* needs_block_offset_adjustment */ false);
  }
}

namespace {

Vector<std::div_t> ComputeTrackSizesInRange(
    const SetGeometry& set_geometry,
    const wtf_size_t range_starting_set_index,
    const wtf_size_t range_set_count) {
  Vector<std::div_t> track_sizes;
  track_sizes.ReserveInitialCapacity(range_set_count);
  const wtf_size_t ending_set_index =
      range_starting_set_index + range_set_count;

  for (wtf_size_t set_index = range_starting_set_index;
       set_index < ending_set_index; ++set_index) {
    // Set information is stored as offsets. To determine the size of a single
    // track in a givent set, first determine the total size the set takes up
    // by finding the difference between the offsets and subtracting the gutter
    // size for each track in the set.
    const wtf_size_t set_track_count =
        set_geometry.sets[set_index + 1].track_count;
    DCHECK_GE(set_track_count, 1u);
    LayoutUnit set_size = set_geometry.sets[set_index + 1].offset -
                          set_geometry.sets[set_index].offset -
                          set_geometry.gutter_size * set_track_count;

    // Once we have determined the size of the set, we can find the size of a
    // given track by dividing the |set_size| by the |set_track_count|.
    track_sizes.emplace_back(std::div(set_size.RawValue(), set_track_count));
  }
  return track_sizes;
}

// For out of flow items that are located in the middle of a range, computes
// the extra offset relative to the start of its containing range.
LayoutUnit ComputeTrackOffsetInRange(const SetGeometry& set_geometry,
                                     const wtf_size_t range_starting_set_index,
                                     const wtf_size_t range_set_count,
                                     const wtf_size_t offset_in_range) {
  if (!range_set_count || !offset_in_range)
    return LayoutUnit();

  // To compute the index offset, we have to determine the size of the
  // tracks within the grid item's span.
  Vector<std::div_t> track_sizes = ComputeTrackSizesInRange(
      set_geometry, range_starting_set_index, range_set_count);

  // Calculate how many sets there are from the start of the range to the
  // |offset_in_range|. This division can produce a remainder, which would
  // mean that not all of the sets are repeated the same amount of times from
  // the start to the |offset_in_range|.
  const wtf_size_t floor_set_track_count = offset_in_range / range_set_count;
  const wtf_size_t remaining_track_count = offset_in_range % range_set_count;

  // Iterate over the sets and add the sizes of the tracks to |index_offset|.
  LayoutUnit index_offset = set_geometry.gutter_size * offset_in_range;
  for (wtf_size_t track_index = 0; track_index < track_sizes.size();
       ++track_index) {
    // If we have a remainder from the |floor_set_track_count|, we have to
    // consider it to get the correct offset.
    int set_count =
        floor_set_track_count + ((remaining_track_count > track_index) ? 1 : 0);
    index_offset += LayoutUnit::FromRawValue(
        (set_count * track_sizes[track_index].quot) +
        std::min(set_count, track_sizes[track_index].rem));
  }
  return index_offset;
}

template <bool snap_to_end_of_track>
LayoutUnit TrackOffset(
    const NGGridLayoutAlgorithmTrackCollection& track_collection,
    const SetGeometry& set_geometry,
    const wtf_size_t range_index,
    const wtf_size_t offset_in_range) {
  const wtf_size_t range_starting_set_index =
      track_collection.RangeStartingSetIndex(range_index);
  const wtf_size_t range_track_count =
      track_collection.RangeTrackCount(range_index);
  const wtf_size_t range_set_count =
      track_collection.RangeSetCount(range_index);

  LayoutUnit track_offset;
  if (offset_in_range == range_track_count) {
    DCHECK(snap_to_end_of_track);
    track_offset =
        set_geometry.sets[range_starting_set_index + range_set_count].offset;
  } else {
    DCHECK_LT(offset_in_range, range_track_count);
    DCHECK(offset_in_range || !snap_to_end_of_track);
    // If an out of flow item starts/ends in the middle of a range, compute and
    // add the extra offset to the start offset of the range.
    track_offset =
        set_geometry.sets[range_starting_set_index].offset +
        ComputeTrackOffsetInRange(set_geometry, range_starting_set_index,
                                  range_set_count, offset_in_range);
  }

  // |track_offset| includes the gutter size at the end of the last track,
  // when we snap to the end of last track such gutter size should be removed.
  // However, only snap if this range is not collapsed or if it can snap to the
  // end of the last track in the previous range of the collection.
  if (snap_to_end_of_track && (range_set_count || range_index))
    track_offset -= set_geometry.gutter_size;
  return track_offset;
}

LayoutUnit TrackStartOffset(
    const NGGridLayoutAlgorithmTrackCollection& track_collection,
    const SetGeometry& set_geometry,
    const wtf_size_t range_index,
    const wtf_size_t offset_in_range) {
  if (!track_collection.RangeCount()) {
    // If the start line of an out of flow item is not 'auto' in an empty and
    // undefined grid, start offset is the start border scrollbar padding.
    DCHECK_EQ(range_index, 0u);
    DCHECK_EQ(offset_in_range, 0u);
    return set_geometry.sets[0].offset;
  }

  const wtf_size_t range_track_count =
      track_collection.RangeTrackCount(range_index);

  if (offset_in_range == range_track_count &&
      range_index == track_collection.RangeCount() - 1) {
    // The only case where we allow the offset to be equal to the number of
    // tracks in the range is for the last range in the collection, which should
    // match the end line of the implicit grid; snap to the track end instead.
    return TrackOffset</* snap_to_end_of_track */ true>(
        track_collection, set_geometry, range_index, offset_in_range);
  }

  DCHECK_LT(offset_in_range, range_track_count);
  return TrackOffset</* snap_to_end_of_track */ false>(
      track_collection, set_geometry, range_index, offset_in_range);
}

LayoutUnit TrackEndOffset(
    const NGGridLayoutAlgorithmTrackCollection& track_collection,
    const SetGeometry& set_geometry,
    const wtf_size_t range_index,
    const wtf_size_t offset_in_range) {
  if (!track_collection.RangeCount()) {
    // If the end line of an out of flow item is not 'auto' in an empty and
    // undefined grid, end offset is the start border scrollbar padding.
    DCHECK_EQ(range_index, 0u);
    DCHECK_EQ(offset_in_range, 0u);
    return set_geometry.sets[0].offset;
  }

  if (!offset_in_range && !range_index) {
    // Only allow the offset to be 0 for the first range in the collection,
    // which is the start line of the implicit grid; don't snap to the end.
    return TrackOffset</* snap_to_end_of_track */ false>(
        track_collection, set_geometry, range_index, offset_in_range);
  }

  DCHECK_GT(offset_in_range, 0u);
  return TrackOffset</* snap_to_end_of_track */ true>(
      track_collection, set_geometry, range_index, offset_in_range);
}

void ComputeOutOfFlowOffsetAndSize(
    const GridItemData& out_of_flow_item,
    const SetGeometry& set_geometry,
    const NGGridLayoutAlgorithmTrackCollection& track_collection,
    const NGBoxStrut& borders,
    const LogicalSize& border_box_size,
    const LayoutUnit block_size,
    LayoutUnit* start_offset,
    LayoutUnit* size) {
  DCHECK(start_offset && size && out_of_flow_item.IsOutOfFlow());
  OutOfFlowItemPlacement item_placement;
  LayoutUnit end_offset;

  // The default padding box value for |size| is used for out of flow items in
  // which both the start line and end line are defined as 'auto'.
  if (track_collection.IsForColumns()) {
    item_placement = out_of_flow_item.column_placement;
    *start_offset = borders.inline_start;
    end_offset = border_box_size.inline_size - borders.inline_end;
  } else {
    item_placement = out_of_flow_item.row_placement;
    *start_offset = borders.block_start;
    end_offset = ((border_box_size.block_size == kIndefiniteSize)
                      ? block_size
                      : border_box_size.block_size) -
                 borders.block_end;
  }

  // If the start line is defined, the size will be calculated by subtracting
  // the offset at |start_index|; otherwise, use the computed border start.
  if (item_placement.range_index.begin != kNotFound) {
    DCHECK_NE(item_placement.offset_in_range.begin, kNotFound);

    *start_offset = TrackStartOffset(track_collection, set_geometry,
                                     item_placement.range_index.begin,
                                     item_placement.offset_in_range.begin);
  }

  // If the end line is defined, the offset (which can be the offset at the
  // start index or the start border) and the added grid gap after the spanned
  // tracks are subtracted from the offset at the end index.
  if (item_placement.range_index.end != kNotFound) {
    DCHECK_NE(item_placement.offset_in_range.end, kNotFound);

    end_offset = TrackEndOffset(track_collection, set_geometry,
                                item_placement.range_index.end,
                                item_placement.offset_in_range.end);
    *size = end_offset - *start_offset;
  } else {
    // By the time we call this method, we shouldn't have indefinite tracks.
    DCHECK(set_geometry.last_indefinite_indices.IsEmpty());

    // |start_offset| can be greater than |end_offset| if the track sizes from
    // the grid overflow the container's respective size.
    *size = (end_offset - *start_offset).ClampNegativeToZero();
  }
  DCHECK(*size >= 0 || *size == kIndefiniteSize);
}

}  // namespace

void NGGridLayoutAlgorithm::ComputeGridItemOffsetAndSize(
    const GridItemData& grid_item,
    const SetGeometry& set_geometry,
    const GridTrackSizingDirection track_direction,
    LayoutUnit* start_offset,
    LayoutUnit* size) const {
  DCHECK(start_offset && size && !grid_item.IsOutOfFlow());

  const auto& set_indices = grid_item.SetIndices(track_direction);
  DCHECK_LT(set_indices.end, set_geometry.sets.size());
  DCHECK_LT(set_indices.begin, set_indices.end);

  *start_offset = set_geometry.sets[set_indices.begin].offset;
  *size = kIndefiniteSize;

  // If we are measuring a grid item we might not yet have determined the final
  // used sizes for all sets; |last_indefinite_index| is the last set which has
  // an indefinite used size, if |set_indices.begin| is greater, then all the
  // sets between it and |set_indices.end| are definite.
  const wtf_size_t last_indefinite_index =
      set_geometry.last_indefinite_indices.IsEmpty()
          ? kNotFound
          : set_geometry.last_indefinite_indices[set_indices.end];
  if (last_indefinite_index == kNotFound ||
      set_indices.begin > last_indefinite_index) {
    *size = ComputeSetSpanSize(set_geometry, set_indices);
    if (size->MightBeSaturated())
      *size = LayoutUnit();
  }
  DCHECK(!size->MightBeSaturated());
  DCHECK(*size >= 0 || *size == kIndefiniteSize);
}

// static
LogicalRect NGGridLayoutAlgorithm::ComputeOutOfFlowItemContainingRect(
    const ComputedStyle& container_style,
    const NGGridPlacementData& placement_data,
    const NGGridLayoutData& layout_data,
    const NGBoxStrut& borders,
    const LogicalSize& border_box_size,
    const LayoutUnit block_size,
    GridItemData* out_of_flow_item) {
  DCHECK(out_of_flow_item && out_of_flow_item->IsOutOfFlow());

  NGGridPlacement grid_placement(container_style, placement_data);
  NGGridLayoutAlgorithmTrackCollection column_track_collection(
      layout_data.column_geometry.ranges, kForColumns);
  NGGridLayoutAlgorithmTrackCollection row_track_collection(
      layout_data.row_geometry.ranges, kForRows);

  out_of_flow_item->ComputeOutOfFlowItemPlacement(column_track_collection,
                                                  grid_placement);
  out_of_flow_item->ComputeOutOfFlowItemPlacement(row_track_collection,
                                                  grid_placement);

  SetGeometry column_geometry(layout_data.column_geometry.sets,
                              layout_data.column_geometry.gutter_size);
  SetGeometry row_geometry(layout_data.row_geometry.sets,
                           layout_data.row_geometry.gutter_size);

  LogicalRect containing_rect;
  ComputeOutOfFlowOffsetAndSize(
      *out_of_flow_item, column_geometry, column_track_collection, borders,
      border_box_size, block_size, &containing_rect.offset.inline_offset,
      &containing_rect.size.inline_size);

  ComputeOutOfFlowOffsetAndSize(
      *out_of_flow_item, row_geometry, row_track_collection, borders,
      border_box_size, block_size, &containing_rect.offset.block_offset,
      &containing_rect.size.block_size);

  return containing_rect;
}

}  // namespace blink
