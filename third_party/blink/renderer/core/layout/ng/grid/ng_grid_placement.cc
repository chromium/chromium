// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_placement.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_child_iterator.h"

namespace blink {

NGGridPlacement::NGGridPlacement(const ComputedStyle& grid_style,
                                 const wtf_size_t column_auto_repetitions,
                                 const wtf_size_t row_auto_repetitions)
    : grid_style_(grid_style),
      packing_behavior_(grid_style.IsGridAutoFlowAlgorithmSparse()
                            ? PackingBehavior::kSparse
                            : PackingBehavior::kDense),
      // The major direction is the one specified in the 'grid-auto-flow'
      // property (row or column), the minor direction is its opposite.
      major_direction_(grid_style.IsGridAutoFlowDirectionRow() ? kForRows
                                                               : kForColumns),
      minor_direction_(grid_style.IsGridAutoFlowDirectionRow() ? kForColumns
                                                               : kForRows),
      column_auto_repeat_track_count_(
          grid_style.GridTemplateColumns().NGTrackList().AutoRepeatSize() *
          column_auto_repetitions),
      row_auto_repeat_track_count_(
          grid_style.GridTemplateRows().NGTrackList().AutoRepeatSize() *
          row_auto_repetitions),
      column_auto_repetitions_(column_auto_repetitions),
      row_auto_repetitions_(row_auto_repetitions) {}

// https://drafts.csswg.org/css-grid/#auto-placement-algo
void NGGridPlacement::RunAutoPlacementAlgorithm(
    Vector<GridItemData>* grid_items) {
  DCHECK(grid_items);
  minor_max_end_line_ = (minor_direction_ == kForColumns)
                            ? GridPositionsResolver::ExplicitGridColumnCount(
                                  grid_style_, column_auto_repeat_track_count_)
                            : GridPositionsResolver::ExplicitGridRowCount(
                                  grid_style_, row_auto_repeat_track_count_);

  // We need these variables in order to use |GridPositionsResolver|.
  column_start_offset_ = DetermineTrackStartOffset(*grid_items, kForColumns);
  row_start_offset_ = DetermineTrackStartOffset(*grid_items, kForRows);

  // Step 1. Position anything that’s not auto-placed; if no items need
  // auto-placement, then we are done.
  if (!PlaceNonAutoGridItems(grid_items))
    return;

  // Step 2. Process the items locked to the major axis.
  PlaceGridItemsLockedToMajorAxis(grid_items);

  // Step 3. Determine the number of minor tracks in the implicit grid.
  // This is already accomplished within the |PlaceNonAutoGridItems| and
  // |PlaceGridItemsLockedToMajorAxis| methods; nothing else to do here.

  // Step 4. Position remaining grid items.
  AutoPlacementCursor placement_cursor;
  for (GridItemData& grid_item : *grid_items) {
    switch (grid_item.AutoPlacement(major_direction_)) {
      case AutoPlacementType::kBoth:
        PlaceAutoBothAxisGridItem(&grid_item, &placement_cursor, *grid_items);
        break;
      case AutoPlacementType::kMajor:
        PlaceAutoMajorAxisGridItem(&grid_item, &placement_cursor, *grid_items);
        break;
      case AutoPlacementType::kMinor:
        NOTREACHED() << "Minor axis placement should've already occurred.";
        break;
      case AutoPlacementType::kNotNeeded:
        break;
    }
  }
}

wtf_size_t NGGridPlacement::DetermineTrackStartOffset(
    const Vector<GridItemData>& grid_items,
    GridTrackSizingDirection track_direction) const {
  wtf_size_t track_start_offset = 0;

  for (const GridItemData& grid_item : grid_items) {
    GridSpan grid_item_span =
        GridPositionsResolver::ResolveGridPositionsFromStyle(
            grid_style_, grid_item.node.Style(), track_direction,
            AutoRepeatTrackCount(track_direction));

    if (!grid_item_span.IsIndefinite()) {
      DCHECK(grid_item_span.IsUntranslatedDefinite());
      track_start_offset = std::max<int>(
          track_start_offset, -grid_item_span.UntranslatedStartLine());
    }
  }
  return track_start_offset;
}

bool NGGridPlacement::PlaceNonAutoGridItems(Vector<GridItemData>* grid_items) {
  DCHECK(grid_items);
  bool has_auto_placed_items = false;

  for (GridItemData& grid_item : *grid_items) {
    bool has_definite_major_placement =
        PlaceGridItem(&grid_item, major_direction_);
    bool has_definite_minor_placement =
        PlaceGridItem(&grid_item, minor_direction_);

    if (has_definite_minor_placement) {
      minor_max_end_line_ =
          std::max(minor_max_end_line_, grid_item.EndLine(minor_direction_));
    } else {
      has_auto_placed_items = true;
      minor_max_end_line_ = std::max<wtf_size_t>(
          minor_max_end_line_, GridPositionsResolver::SpanSizeForAutoPlacedItem(
                                   grid_item.node.Style(), minor_direction_));
    }
    has_auto_placed_items |= !has_definite_major_placement;
  }
  return has_auto_placed_items;
}

void NGGridPlacement::PlaceGridItemsLockedToMajorAxis(
    Vector<GridItemData>* grid_items) {
  DCHECK(grid_items);

  // Mapping between the major axis tracks and the last auto-placed item's end
  // line inserted on that track. This is needed to implement "sparse" packing
  // for grid items locked to a given major axis track.
  // See https://drafts.csswg.org/css-grid/#auto-placement-algo.
  HashMap<wtf_size_t, wtf_size_t> minor_cursors;

  for (GridItemData& grid_item : *grid_items) {
    // Only consider grid items that require minor axis auto-placement.
    if (grid_item.AutoPlacement(major_direction_) != AutoPlacementType::kMinor)
      continue;

    wtf_size_t minor_start;
    if (HasSparsePacking() &&
        minor_cursors.Contains(grid_item.StartLine(major_direction_) + 1)) {
      minor_start = minor_cursors.at(grid_item.StartLine(major_direction_) + 1);
    } else {
      minor_start = 0;
    }

    wtf_size_t minor_span_size =
        GridPositionsResolver::SpanSizeForAutoPlacedItem(grid_item.node.Style(),
                                                         minor_direction_);
    while (DoesItemOverlap(grid_item.StartLine(major_direction_),
                           grid_item.EndLine(major_direction_), minor_start,
                           minor_start + minor_span_size, *grid_items)) {
      ++minor_start;
    }

    wtf_size_t minor_end = minor_start + minor_span_size;
    if (HasSparsePacking())
      minor_cursors.Set(grid_item.StartLine(major_direction_) + 1, minor_end);
    minor_max_end_line_ = std::max(minor_max_end_line_, minor_end);

    // Update grid item placement for minor axis.
    GridSpan grid_item_span =
        GridSpan::TranslatedDefiniteGridSpan(minor_start, minor_end);
    grid_item.SetSpan(grid_item_span, minor_direction_);
  }
}

void NGGridPlacement::PlaceAutoMajorAxisGridItem(
    GridItemData* grid_item,
    AutoPlacementCursor* placement_cursor,
    const Vector<GridItemData>& grid_items) {
  DCHECK(grid_item);

  if (HasSparsePacking()) {
    // For sparse packing, set the minor position of the cursor to the grid
    // item’s minor starting line. If this is less than the previous column
    // position of the cursor, increment the major position by 1.
    if (grid_item->StartLine(minor_direction_) <
        placement_cursor->minor_position) {
      ++placement_cursor->major_position;
    }
  } else {
    // Otherwise, for dense packing, reset the auto-placement cursor's major
    // position to the start-most major line in the implicit grid.
    placement_cursor->major_position = 0;
  }

  placement_cursor->minor_position = grid_item->StartLine(minor_direction_);
  wtf_size_t major_span_size = GridPositionsResolver::SpanSizeForAutoPlacedItem(
      grid_item->node.Style(), major_direction_);

  // Increment the cursor’s major position until a value is found where the
  // grid item does not overlap any occupied grid cells
  while (DoesItemOverlap(placement_cursor->major_position,
                         placement_cursor->major_position + major_span_size,
                         grid_item->StartLine(minor_direction_),
                         grid_item->EndLine(minor_direction_), grid_items)) {
    ++placement_cursor->major_position;
  }

  // Update grid item placement for major axis.
  GridSpan grid_item_span = GridSpan::TranslatedDefiniteGridSpan(
      placement_cursor->major_position,
      placement_cursor->major_position + major_span_size);
  grid_item->SetSpan(grid_item_span, major_direction_);
}

void NGGridPlacement::PlaceAutoBothAxisGridItem(
    GridItemData* grid_item,
    AutoPlacementCursor* placement_cursor,
    const Vector<GridItemData>& grid_items) {
  DCHECK(grid_item);

  // For dense packing, set the cursor’s major and minor positions to the
  // start-most row and column lines in the implicit grid.
  if (!HasSparsePacking()) {
    placement_cursor->major_position = 0;
    placement_cursor->minor_position = 0;
  }

  wtf_size_t major_span_size = GridPositionsResolver::SpanSizeForAutoPlacedItem(
      grid_item->node.Style(), major_direction_);
  wtf_size_t minor_span_size = GridPositionsResolver::SpanSizeForAutoPlacedItem(
      grid_item->node.Style(), minor_direction_);

  // Check to see if there would be overlap if this item was placed at the
  // cursor. If overlap exists, increment minor position until no conflict
  // exists or the item would overflow the minor axis.
  while (DoesItemOverlap(placement_cursor->major_position,
                         placement_cursor->major_position + major_span_size,
                         placement_cursor->minor_position,
                         placement_cursor->minor_position + minor_span_size,
                         grid_items)) {
    ++placement_cursor->minor_position;
    if (placement_cursor->minor_position + minor_span_size >
        minor_max_end_line_) {
      // If the cursor overflows the minor axis, increment cursor on the major
      // axis and start from the beginning.
      ++placement_cursor->major_position;
      placement_cursor->minor_position = 0;
    }
  }

  // Update grid item placement for both axis.
  GridSpan grid_item_span = GridSpan::TranslatedDefiniteGridSpan(
      placement_cursor->major_position,
      placement_cursor->major_position + major_span_size);
  grid_item->SetSpan(grid_item_span, major_direction_);

  grid_item_span = GridSpan::TranslatedDefiniteGridSpan(
      placement_cursor->minor_position,
      placement_cursor->minor_position + minor_span_size);
  grid_item->SetSpan(grid_item_span, minor_direction_);
}

bool NGGridPlacement::PlaceGridItem(
    GridItemData* grid_item,
    GridTrackSizingDirection track_direction) const {
  DCHECK(grid_item);
  GridSpan span = GridPositionsResolver::ResolveGridPositionsFromStyle(
      grid_style_, grid_item->node.Style(), track_direction,
      AutoRepeatTrackCount(track_direction));

  if (span.IsIndefinite()) {
    DCHECK(grid_item->Span(track_direction).IsIndefinite());
    return false;
  }

  DCHECK(span.IsUntranslatedDefinite());
  span.Translate(StartOffset(track_direction));
  grid_item->SetSpan(span, track_direction);
  return true;
}

bool NGGridPlacement::DoesItemOverlap(
    wtf_size_t major_start,
    wtf_size_t major_end,
    wtf_size_t minor_start,
    wtf_size_t minor_end,
    const Vector<GridItemData>& grid_items) const {
  DCHECK_LE(major_start, major_end);
  DCHECK_LE(minor_start, minor_end);
  // TODO(janewman): Implement smarter collision detection, iterating over all
  // items is not ideal and has large performance implications.
  for (const GridItemData& grid_item : grid_items) {
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

wtf_size_t NGGridPlacement::AutoRepetitions(
    GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns) ? column_auto_repetitions_
                                          : row_auto_repetitions_;
}

wtf_size_t NGGridPlacement::StartOffset(
    GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns) ? column_start_offset_
                                          : row_start_offset_;
}

wtf_size_t NGGridPlacement::AutoRepeatTrackCount(
    GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns) ? column_auto_repeat_track_count_
                                          : row_auto_repeat_track_count_;
}

bool NGGridPlacement::HasSparsePacking() const {
  return packing_behavior_ == PackingBehavior::kSparse;
}

void NGGridPlacement::ResolveOutOfFlowItemGridLines(
    const NGGridLayoutAlgorithmTrackCollection& track_collection,
    const ComputedStyle& out_of_flow_item_style,
    wtf_size_t* start_line,
    wtf_size_t* end_line) const {
  DCHECK(start_line);
  DCHECK(end_line);

  const GridTrackSizingDirection track_direction = track_collection.Direction();
  GridSpan span = GridPositionsResolver::ResolveGridPositionsFromStyle(
      grid_style_, out_of_flow_item_style, track_direction,
      AutoRepeatTrackCount(track_direction));

  if (span.IsIndefinite()) {
    *start_line = kNotFound;
    *end_line = kNotFound;
    return;
  }

  if (span.UntranslatedStartLine() > -1) {
    // TODO(ansollan): Handle out of flow positioned items with negative
    // indexes.
    span.Translate(StartOffset(track_direction));
  }

  *start_line = span.StartLine();
  *end_line = span.EndLine();

  bool is_start_line_auto =
      (track_direction == kForColumns)
          ? out_of_flow_item_style.GridColumnStart().IsAuto()
          : out_of_flow_item_style.GridRowStart().IsAuto();
  if (is_start_line_auto || !track_collection.IsTrackWithinBounds(*start_line))
    *start_line = kNotFound;

  bool is_end_line_auto = (track_direction == kForColumns)
                              ? out_of_flow_item_style.GridColumnEnd().IsAuto()
                              : out_of_flow_item_style.GridRowEnd().IsAuto();
  if (is_end_line_auto || !track_collection.IsTrackWithinBounds(*end_line - 1))
    *end_line = kNotFound;
}

}  // namespace blink
