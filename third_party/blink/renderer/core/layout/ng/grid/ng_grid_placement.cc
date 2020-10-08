// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_placement.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_child_iterator.h"

namespace blink {

NGGridPlacement::NGGridPlacement(
    const wtf_size_t row_auto_repeat,
    const wtf_size_t column_auto_repeat,
    const wtf_size_t row_explicit_start,
    const wtf_size_t column_explicit_start,
    const PackingBehavior packing_behavior,
    const GridTrackSizingDirection major_direction,
    const ComputedStyle& grid_style,
    wtf_size_t minor_max_end_line,
    NGGridBlockTrackCollection& row_collection,
    NGGridBlockTrackCollection& column_collection,
    Vector<NGGridLayoutAlgorithm::GridItemData>& items)
    : row_auto_repeat_(row_auto_repeat),
      column_auto_repeat_(column_auto_repeat),
      row_explicit_start_(row_explicit_start),
      column_explicit_start_(column_explicit_start),
      packing_behavior_(packing_behavior),
      major_direction_(major_direction),
      minor_direction_(major_direction == kForRows ? kForColumns : kForRows),
      grid_style_(grid_style),
      minor_max_end_line_(minor_max_end_line),
      row_collection_(row_collection),
      column_collection_(column_collection),
      items_(items)

{
  placement_cursor_major = ExplicitStart(major_direction_);
  placement_cursor_minor = ExplicitStart(minor_direction_);
}

// https://drafts.csswg.org/css-grid/#auto-placement-algo
void NGGridPlacement::RunAutoPlacementAlgorithm() {
  // Step 1. Position anything that’s not auto-positioned, if no items need
  // auto positioning, then we are done.
  if (!PlaceNonAutoGridItems())
    return;

  // Step 2. Process the items locked to the major axis.
  PlaceGridItemsLockedToMajorAxis();

  // Step 3. Determine the number of minor tracks in the implicit grid.
  starting_minor_line_ = ExplicitStart(minor_direction_);
  ending_minor_line_ = std::max(
      minor_max_end_line_,
      starting_minor_line_ +
          BlockCollection(minor_direction_).ExplicitTracks().TotalTrackCount());

  // Step 4. Position remaining grid items.
  for (NGGridLayoutAlgorithm::GridItemData* grid_item :
       items_not_locked_to_major_axis_) {
    DCHECK(grid_item);
    switch (packing_behavior_) {
      case PackingBehavior::kSparse:
        if (grid_item->AutoPlacement(major_direction_) ==
            NGGridLayoutAlgorithm::AutoPlacementType::kMajor) {
          PlaceAutoMajorAxisGridItem(*grid_item);
        } else if (grid_item->AutoPlacement(major_direction_) ==
                   NGGridLayoutAlgorithm::AutoPlacementType::kBoth) {
          PlaceAutoBothAxisGridItem(*grid_item);
        }
        break;
      case PackingBehavior::kDense:
        // TODO(janewman): implement auto placement for dense packing.
        break;
    }
  }
}

bool NGGridPlacement::PlaceNonAutoGridItems() {
  for (NGGridLayoutAlgorithm::GridItemData& grid_item : items_) {
    bool has_definite_major_placement =
        PlaceGridItem(major_direction_, grid_item);
    bool has_definite_minor_placement =
        PlaceGridItem(minor_direction_, grid_item);

    // If the item has definite positions on both axis then no auto placement is
    // needed.
    if (has_definite_major_placement && has_definite_minor_placement)
      continue;

    if (has_definite_major_placement)
      items_locked_to_major_axis_.push_back(&grid_item);
    else
      items_not_locked_to_major_axis_.push_back(&grid_item);
  }
  return !items_locked_to_major_axis_.IsEmpty() ||
         !items_not_locked_to_major_axis_.IsEmpty();
}

void NGGridPlacement::PlaceGridItemsLockedToMajorAxis() {
  // Mapping between the major axis tracks (rows or columns) and the last
  // auto-placed item's position inserted on that track. This is needed to
  // implement "sparse" packing for items locked to a given track.
  // See https://drafts.csswg.org/css-grid/#auto-placement-algo
  HashMap<wtf_size_t, wtf_size_t> minor_axis_cursors;
  for (NGGridLayoutAlgorithm::GridItemData* grid_item :
       items_locked_to_major_axis_) {
    DCHECK(grid_item);
    DCHECK_EQ(grid_item->AutoPlacement(major_direction_),
              NGGridLayoutAlgorithm::AutoPlacementType::kMinor);
    switch (packing_behavior_) {
      case PackingBehavior::kSparse: {
        wtf_size_t minor_axis_start;
        if (minor_axis_cursors.Contains(grid_item->StartLine(major_direction_) +
                                        1)) {
          minor_axis_start =
              minor_axis_cursors.at(grid_item->StartLine(major_direction_) + 1);
        } else {
          minor_axis_start = ExplicitStart(minor_direction_);
        }
        wtf_size_t minor_span_size =
            GridPositionsResolver::SpanSizeForAutoPlacedItem(
                grid_item->node.Style(), minor_direction_);
        while (DoesItemOverlap(grid_item->StartLine(major_direction_),
                               grid_item->EndLine(major_direction_),
                               minor_axis_start,
                               minor_axis_start + minor_span_size)) {
          minor_axis_start++;
        }
        wtf_size_t minor_axis_end = minor_axis_start + minor_span_size;
        minor_axis_cursors.Set(grid_item->StartLine(major_direction_) + 1,
                               minor_axis_start);
        minor_max_end_line_ =
            std::max<wtf_size_t>(minor_max_end_line_, minor_axis_end);

        // Update placement and ensure track coverage.
        UpdatePlacementAndEnsureTrackCoverage(
            GridSpan::TranslatedDefiniteGridSpan(minor_axis_start,
                                                 minor_axis_end),
            minor_direction_, *grid_item);
      } break;
      case PackingBehavior::kDense:
        // TODO(janewman): implement auto placement for dense packing.
        break;
    }
  }
}

void NGGridPlacement::PlaceAutoMajorAxisGridItem(
    NGGridLayoutAlgorithm::GridItemData& grid_item) {
  wtf_size_t major_span_size = GridPositionsResolver::SpanSizeForAutoPlacedItem(
      grid_item.node.Style(), major_direction_);
  // Set the minor position of the cursor to the grid item’s minor starting
  // line. If this is less than the previous column position of the cursor,
  // increment the major position by 1.
  if (grid_item.StartLine(minor_direction_) < placement_cursor_minor) {
    placement_cursor_major++;
  }

  placement_cursor_minor = grid_item.StartLine(minor_direction_);
  // Increment the cursor’s major position until a value is found where the grid
  // item does not overlap any occupied grid cells
  while (DoesItemOverlap(placement_cursor_major,
                         placement_cursor_major + major_span_size,
                         grid_item.StartLine(minor_direction_),
                         grid_item.EndLine(minor_direction_))) {
    placement_cursor_major++;
  }

  // Update item and track placement.
  UpdatePlacementAndEnsureTrackCoverage(
      GridSpan::TranslatedDefiniteGridSpan(
          placement_cursor_major, placement_cursor_major + major_span_size),
      major_direction_, grid_item);
}

void NGGridPlacement::PlaceAutoBothAxisGridItem(
    NGGridLayoutAlgorithm::GridItemData& grid_item) {
  wtf_size_t major_span_size = GridPositionsResolver::SpanSizeForAutoPlacedItem(
      grid_item.node.Style(), major_direction_);
  wtf_size_t minor_span_size = GridPositionsResolver::SpanSizeForAutoPlacedItem(
      grid_item.node.Style(), minor_direction_);
  // Check to see if there would be overlap if this item was placed at the
  // cursor. If overlap exists, increment minor position until no conflict
  // exists or the item would overflow the minor axis.
  while (DoesItemOverlap(
      placement_cursor_major, placement_cursor_major + major_span_size,
      placement_cursor_minor, placement_cursor_minor + minor_span_size)) {
    placement_cursor_minor++;
    if (placement_cursor_minor + minor_span_size > ending_minor_line_) {
      // If the cursor overflows the minor axis, increment cursor on the major
      // axis and start from the beginning.
      placement_cursor_major++;
      placement_cursor_minor = starting_minor_line_;
    }
  }

  UpdatePlacementAndEnsureTrackCoverage(
      GridSpan::TranslatedDefiniteGridSpan(
          placement_cursor_major, placement_cursor_major + major_span_size),
      major_direction_, grid_item);
  UpdatePlacementAndEnsureTrackCoverage(
      GridSpan::TranslatedDefiniteGridSpan(
          placement_cursor_minor, placement_cursor_minor + minor_span_size),
      minor_direction_, grid_item);
}

bool NGGridPlacement::PlaceGridItem(
    GridTrackSizingDirection direction,
    NGGridLayoutAlgorithm::NGGridLayoutAlgorithm::GridItemData& grid_item) {
  GridSpan span = GridPositionsResolver::ResolveGridPositionsFromStyle(
      grid_style_, grid_item.node.Style(), direction, AutoRepeat(direction));
  // Indefinite positions are resolved with the auto placement algorithm.
  if (span.IsIndefinite())
    return false;

  span.Translate(ExplicitStart(direction));
  UpdatePlacementAndEnsureTrackCoverage(span, direction, grid_item);
  return true;
}

void NGGridPlacement::UpdatePlacementAndEnsureTrackCoverage(
    GridSpan span,
    GridTrackSizingDirection track_direction,
    NGGridLayoutAlgorithm::NGGridLayoutAlgorithm::GridItemData& grid_item) {
  grid_item.SetSpan(span, track_direction);
  BlockCollection(track_direction)
      .EnsureTrackCoverage(span.StartLine(), span.IntegerSpan());
}

bool NGGridPlacement::DoesItemOverlap(wtf_size_t major_start,
                                      wtf_size_t major_end,
                                      wtf_size_t minor_start,
                                      wtf_size_t minor_end) const {
  DCHECK_LE(major_start, major_end);
  DCHECK_LE(minor_start, minor_end);
  // TODO(janewman): Implement smarter collision detection, iterating over all
  // items is not ideal and has large performance implications.
  for (const NGGridLayoutAlgorithm::GridItemData& grid_item : items_) {
    if (grid_item.Span(major_direction_).IsIndefinite())
      continue;
    // Only test against definite positions.
    // No collision if both start and end are on the same side of the item.
    wtf_size_t item_major_start = grid_item.StartLine(major_direction_);
    wtf_size_t item_major_end = grid_item.EndLine(major_direction_);
    if (major_end <= item_major_start)
      continue;
    if (major_start >= item_major_end)
      continue;

    // Do the same for the minor axis.
    if (grid_item.Span(minor_direction_).IsIndefinite())
      continue;
    wtf_size_t item_minor_start = grid_item.StartLine(minor_direction_);
    wtf_size_t item_minor_end = grid_item.EndLine(minor_direction_);
    if (minor_end <= item_minor_start)
      continue;
    if (minor_start >= item_minor_end)
      continue;

    return true;
  }
  return false;
}

wtf_size_t NGGridPlacement::AutoRepeat(GridTrackSizingDirection direction) {
  switch (direction) {
    case kForRows:
      return row_auto_repeat_;
    case kForColumns:
      return column_auto_repeat_;
  }
}

wtf_size_t NGGridPlacement::ExplicitStart(GridTrackSizingDirection direction) {
  switch (direction) {
    case kForRows:
      return row_explicit_start_;
    case kForColumns:
      return column_explicit_start_;
  }
}

NGGridBlockTrackCollection& NGGridPlacement::BlockCollection(
    GridTrackSizingDirection direction) {
  switch (direction) {
    case kForRows:
      return row_collection_;
    case kForColumns:
      return column_collection_;
  }
}

}  // namespace blink
