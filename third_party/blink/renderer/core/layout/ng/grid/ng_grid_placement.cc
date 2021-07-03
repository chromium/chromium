// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_placement.h"

namespace blink {

NGGridPlacement::NGGridPlacement(const ComputedStyle& grid_style,
                                 const wtf_size_t column_auto_repetitions,
                                 const wtf_size_t row_auto_repetitions,
                                 const wtf_size_t column_start_offset,
                                 const wtf_size_t row_start_offset)
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
      row_auto_repetitions_(row_auto_repetitions),
      column_start_offset_(column_start_offset),
      row_start_offset_(row_start_offset) {}

// https://drafts.csswg.org/css-grid/#auto-placement-algo
void NGGridPlacement::RunAutoPlacementAlgorithm(GridItems* grid_items) {
  DCHECK(grid_items);

  // Step 1. Position anything that’s not auto-placed; if no items need
  // auto-placement, then we are done.
  GridItemVector items_locked_to_major_axis;
  GridItemVector items_not_locked_to_major_axis;
  if (!PlaceNonAutoGridItems(grid_items, &items_locked_to_major_axis,
                             &items_not_locked_to_major_axis)) {
    return;
  }

  // Step 2. Process the items locked to the major axis.
  PlaceGridItemsLockedToMajorAxis(items_locked_to_major_axis, *grid_items);

  // Step 3. Determine the number of minor tracks in the implicit grid.
  // This is already accomplished within the |PlaceNonAutoGridItems| and
  // |PlaceGridItemsLockedToMajorAxis| methods; nothing else to do here.

  // Step 4. Position remaining grid items.
  AutoPlacementCursor placement_cursor;
  for (GridItemData* grid_item : items_not_locked_to_major_axis) {
    switch (grid_item->AutoPlacement(major_direction_)) {
      case AutoPlacementType::kBoth:
        PlaceAutoBothAxisGridItem(grid_item, &placement_cursor, *grid_items);
        break;
      case AutoPlacementType::kMajor:
        PlaceAutoMajorAxisGridItem(grid_item, &placement_cursor, *grid_items);
        break;
      case AutoPlacementType::kMinor:
        NOTREACHED() << "Minor axis placement should've already occurred.";
        break;
      case AutoPlacementType::kNotNeeded:
        break;
    }
  }
}

bool NGGridPlacement::PlaceNonAutoGridItems(
    GridItems* grid_items,
    GridItemVector* items_locked_to_major_axis,
    GridItemVector* items_not_locked_to_major_axis) {
  DCHECK(grid_items && items_locked_to_major_axis &&
         items_not_locked_to_major_axis);

  column_start_offset_ = row_start_offset_ = 0;
  for (auto& grid_item : grid_items->item_data) {
    const auto& item_style = grid_item.node.Style();

    GridSpan item_column_span =
        GridPositionsResolver::ResolveGridPositionsFromStyle(
            grid_style_, item_style, kForColumns,
            column_auto_repeat_track_count_);
    DCHECK(!item_column_span.IsTranslatedDefinite());
    grid_item.SetSpan(item_column_span, kForColumns);

    GridSpan item_row_span =
        GridPositionsResolver::ResolveGridPositionsFromStyle(
            grid_style_, item_style, kForRows, row_auto_repeat_track_count_);
    DCHECK(!item_row_span.IsTranslatedDefinite());
    grid_item.SetSpan(item_row_span, kForRows);

    // When we have negative indices that go beyond the start of the explicit
    // grid we need to prepend tracks to it; count how many tracks are needed by
    // checking the minimum negative start line of definite spans, the negative
    // of that minimum is the number of tracks we need to prepend.
    // Simplifying the logic above: maximize the negative start lines.
    if (item_column_span.IsUntranslatedDefinite()) {
      column_start_offset_ = std::max<int>(
          column_start_offset_, -item_column_span.UntranslatedStartLine());
    }
    if (item_row_span.IsUntranslatedDefinite()) {
      row_start_offset_ = std::max<int>(row_start_offset_,
                                        -item_row_span.UntranslatedStartLine());
    }
  }

  minor_max_end_line_ =
      (minor_direction_ == kForColumns)
          ? GridPositionsResolver::ExplicitGridColumnCount(
                grid_style_, column_auto_repeat_track_count_) +
                column_start_offset_
          : GridPositionsResolver::ExplicitGridRowCount(
                grid_style_, row_auto_repeat_track_count_) +
                row_start_offset_;

  bool has_auto_placed_items = false;
  for (auto& grid_item : grid_items->item_data) {
    GridSpan item_major_span = grid_item.Span(major_direction_);
    GridSpan item_minor_span = grid_item.Span(minor_direction_);

    const bool has_indefinite_major_span = item_major_span.IsIndefinite();
    const bool has_indefinite_minor_span = item_minor_span.IsIndefinite();

    if (!has_indefinite_major_span) {
      item_major_span.Translate(StartOffset(major_direction_));
      grid_item.SetSpan(item_major_span, major_direction_);
    }
    if (!has_indefinite_minor_span) {
      item_minor_span.Translate(StartOffset(minor_direction_));
      grid_item.SetSpan(item_minor_span, minor_direction_);
    }

    minor_max_end_line_ = std::max<wtf_size_t>(
        minor_max_end_line_, has_indefinite_minor_span
                                 ? item_minor_span.IndefiniteSpanSize()
                                 : item_minor_span.EndLine());

    if (has_indefinite_major_span || has_indefinite_minor_span) {
      if (has_indefinite_major_span)
        items_not_locked_to_major_axis->push_back(&grid_item);
      else
        items_locked_to_major_axis->push_back(&grid_item);
      has_auto_placed_items = true;
    }
  }
  return has_auto_placed_items;
}

void NGGridPlacement::PlaceGridItemsLockedToMajorAxis(
    const GridItemVector& items_locked_to_major_axis,
    const GridItems& placed_grid_items) {
  // Mapping between the major axis tracks and the last auto-placed item's end
  // line inserted on that track. This is needed to implement "sparse" packing
  // for grid items locked to a given major axis track.
  // See https://drafts.csswg.org/css-grid/#auto-placement-algo.
  HashMap<wtf_size_t, wtf_size_t, DefaultHash<wtf_size_t>::Hash,
          WTF::UnsignedWithZeroKeyHashTraits<wtf_size_t>>
      minor_cursors;

  for (GridItemData* grid_item : items_locked_to_major_axis) {
    DCHECK_EQ(grid_item->AutoPlacement(major_direction_),
              AutoPlacementType::kMinor);

    wtf_size_t minor_start;
    if (HasSparsePacking() &&
        minor_cursors.Contains(grid_item->StartLine(major_direction_))) {
      minor_start = minor_cursors.at(grid_item->StartLine(major_direction_));
    } else {
      minor_start = 0;
    }

    const wtf_size_t minor_span_size =
        grid_item->Span(minor_direction_).IndefiniteSpanSize();
    while (DoesItemOverlap(grid_item->StartLine(major_direction_),
                           grid_item->EndLine(major_direction_), minor_start,
                           minor_start + minor_span_size, placed_grid_items)) {
      ++minor_start;
    }

    wtf_size_t minor_end = minor_start + minor_span_size;
    if (HasSparsePacking())
      minor_cursors.Set(grid_item->StartLine(major_direction_), minor_end);
    minor_max_end_line_ = std::max(minor_max_end_line_, minor_end);

    // Update grid item placement for minor axis.
    GridSpan grid_item_span =
        GridSpan::TranslatedDefiniteGridSpan(minor_start, minor_end);
    grid_item->SetSpan(grid_item_span, minor_direction_);
  }
}

void NGGridPlacement::PlaceAutoMajorAxisGridItem(
    GridItemData* grid_item,
    AutoPlacementCursor* placement_cursor,
    const GridItems& placed_grid_items) {
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
  const wtf_size_t major_span_size =
      grid_item->Span(major_direction_).IndefiniteSpanSize();

  // Increment the cursor’s major position until a value is found where the
  // grid item does not overlap any occupied grid cells
  while (DoesItemOverlap(placement_cursor->major_position,
                         placement_cursor->major_position + major_span_size,
                         grid_item->StartLine(minor_direction_),
                         grid_item->EndLine(minor_direction_),
                         placed_grid_items)) {
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
    const GridItems& placed_grid_items) {
  DCHECK(grid_item);

  // For dense packing, set the cursor’s major and minor positions to the
  // start-most row and column lines in the implicit grid.
  if (!HasSparsePacking()) {
    placement_cursor->major_position = 0;
    placement_cursor->minor_position = 0;
  }

  const wtf_size_t major_span_size =
      grid_item->Span(major_direction_).IndefiniteSpanSize();
  const wtf_size_t minor_span_size =
      grid_item->Span(minor_direction_).IndefiniteSpanSize();

  // Check to see if there would be overlap if this item was placed at the
  // cursor. If overlap exists, increment minor position until no conflict
  // exists or the item would overflow the minor axis.
  while (DoesItemOverlap(placement_cursor->major_position,
                         placement_cursor->major_position + major_span_size,
                         placement_cursor->minor_position,
                         placement_cursor->minor_position + minor_span_size,
                         placed_grid_items)) {
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

bool NGGridPlacement::DoesItemOverlap(
    wtf_size_t major_start,
    wtf_size_t major_end,
    wtf_size_t minor_start,
    wtf_size_t minor_end,
    const GridItems& placed_grid_items) const {
  DCHECK_LE(major_start, major_end);
  DCHECK_LE(minor_start, minor_end);
  // TODO(janewman): Implement smarter collision detection, iterating over all
  // items is not ideal and has large performance implications.
  for (const auto& grid_item : placed_grid_items.item_data) {
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

wtf_size_t NGGridPlacement::AutoRepeatTrackCount(
    const GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns) ? column_auto_repeat_track_count_
                                          : row_auto_repeat_track_count_;
}

wtf_size_t NGGridPlacement::AutoRepetitions(
    const GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns) ? column_auto_repetitions_
                                          : row_auto_repetitions_;
}

wtf_size_t NGGridPlacement::StartOffset(
    const GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns) ? column_start_offset_
                                          : row_start_offset_;
}

bool NGGridPlacement::HasSparsePacking() const {
  return packing_behavior_ == PackingBehavior::kSparse;
}

namespace {

bool IsStartLineAuto(const GridTrackSizingDirection track_direction,
                     const ComputedStyle& out_of_flow_item_style) {
  return (track_direction == kForColumns)
             ? out_of_flow_item_style.GridColumnStart().IsAuto()
             : out_of_flow_item_style.GridRowStart().IsAuto();
}

bool IsEndLineAuto(const GridTrackSizingDirection track_direction,
                   const ComputedStyle& out_of_flow_item_style) {
  return (track_direction == kForColumns)
             ? out_of_flow_item_style.GridColumnEnd().IsAuto()
             : out_of_flow_item_style.GridRowEnd().IsAuto();
}

}  // namespace

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

  wtf_size_t start_offset = StartOffset(track_direction);
  int span_start_line = span.UntranslatedStartLine() + start_offset;
  int span_end_line = span.UntranslatedEndLine() + start_offset;

  if (span_start_line < 0 ||
      IsStartLineAuto(track_direction, out_of_flow_item_style) ||
      !track_collection.IsGridLineWithinImplicitGrid(span_start_line)) {
    *start_line = kNotFound;
  } else {
    *start_line = span_start_line;
  }
  if (span_end_line < 0 ||
      IsEndLineAuto(track_direction, out_of_flow_item_style) ||
      !track_collection.IsGridLineWithinImplicitGrid(span_end_line)) {
    *end_line = kNotFound;
  } else {
    *end_line = span_end_line;
  }
}

}  // namespace blink
