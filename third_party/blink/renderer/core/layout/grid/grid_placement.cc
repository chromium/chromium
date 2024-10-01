// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid/grid_placement.h"

#include "third_party/blink/renderer/core/layout/grid/grid_item.h"

namespace blink {

namespace {

enum class AutoPlacementType { kNotNeeded, kMajor, kMinor, kBoth };

AutoPlacementType AutoPlacement(const GridArea& position,
                                GridTrackSizingDirection major_direction) {
  const GridTrackSizingDirection minor_direction =
      (major_direction == kForColumns) ? kForRows : kForColumns;
  DCHECK(!position.Span(major_direction).IsUntranslatedDefinite() &&
         !position.Span(minor_direction).IsUntranslatedDefinite());

  const bool is_major_indefinite =
      position.Span(major_direction).IsIndefinite();
  const bool is_minor_indefinite =
      position.Span(minor_direction).IsIndefinite();

  if (is_minor_indefinite && is_major_indefinite)
    return AutoPlacementType::kBoth;
  if (is_minor_indefinite)
    return AutoPlacementType::kMinor;
  if (is_major_indefinite)
    return AutoPlacementType::kMajor;
  return AutoPlacementType::kNotNeeded;
}

}  // namespace

GridPlacement::GridPlacement(const ComputedStyle& grid_style,
                             const GridLineResolver& line_resolver)
    : placement_data_(line_resolver),
      packing_behavior_(grid_style.IsGridAutoFlowAlgorithmSparse()
                            ? PackingBehavior::kSparse
                            : PackingBehavior::kDense),
      // The major direction is the one specified in the 'grid-auto-flow'
      // property (row or column), the minor direction is its opposite.
      major_direction_(grid_style.IsGridAutoFlowDirectionRow() ? kForRows
                                                               : kForColumns),
      minor_direction_(grid_style.IsGridAutoFlowDirectionRow() ? kForColumns
                                                               : kForRows) {}

// https://drafts.csswg.org/css-grid/#auto-placement-algo
GridPlacementData GridPlacement::RunAutoPlacementAlgorithm(
    const GridItems& grid_items) {
#if DCHECK_IS_ON()
  DCHECK(!auto_placement_algorithm_called_)
      << "Auto-placement algorithm should only be called once.";
  auto_placement_algorithm_called_ = true;
#endif

  auto FinalizeResolvedPositions = [&]() -> GridPlacementData {
    ClampGridItemsToFitSubgridArea(kForColumns);
    ClampGridItemsToFitSubgridArea(kForRows);
    return std::move(placement_data_);
  };

  // Step 1. Position anything that’s not auto-placed; if no items need
  // auto-placement, then we are done.
  PlacedGridItemsList placed_items;
  PositionVector positions_locked_to_major_axis;
  PositionVector positions_not_locked_to_major_axis;

  if (!PlaceNonAutoGridItems(grid_items, &placed_items,
                             &positions_locked_to_major_axis,
                             &positions_not_locked_to_major_axis)) {
    return FinalizeResolvedPositions();
  }

  placed_items.AppendCurrentItemsToOrderedList();

  // Step 2. Process the items locked to the major axis.
  PlaceGridItemsLockedToMajorAxis(positions_locked_to_major_axis,
                                  &placed_items);

  // Step 3. Determine the number of minor tracks in the implicit grid.
  // This is already accomplished within the |PlaceNonAutoGridItems| and
  // |PlaceGridItemsLockedToMajorAxis| methods; nothing else to do here.

  // Before performing auto placement, clamp items to the subgridded area so the
  // auto-placement algorithm is dealing with accurate positions.
  ClampGridItemsToFitSubgridArea(kForColumns);
  ClampGridItemsToFitSubgridArea(kForRows);

  // Step 4. Position remaining grid items.
  AutoPlacementCursor placement_cursor(placed_items.FirstPlacedItem());
  for (auto* position : positions_not_locked_to_major_axis) {
    switch (AutoPlacement(*position, major_direction_)) {
      case AutoPlacementType::kBoth:
        PlaceAutoBothAxisGridItem(position, &placed_items, &placement_cursor);
        break;
      case AutoPlacementType::kMajor:
        PlaceAutoMajorAxisGridItem(position, &placed_items, &placement_cursor);
        break;
      case AutoPlacementType::kMinor:
      case AutoPlacementType::kNotNeeded:
        NOTREACHED_IN_MIGRATION()
            << "Placement of non-auto placed items and items locked "
               "to a major axis should've already occurred.";
        break;
    }
    if (!HasSparsePacking()) {
      // For dense packing, set the cursor’s major and minor positions to the
      // start-most row and column lines in the implicit grid.
      placement_cursor = AutoPlacementCursor(placed_items.FirstPlacedItem());
    }
  }
  return FinalizeResolvedPositions();
}

bool GridPlacement::PlaceNonAutoGridItems(
    const GridItems& grid_items,
    PlacedGridItemsList* placed_items,
    PositionVector* positions_locked_to_major_axis,
    PositionVector* positions_not_locked_to_major_axis) {
  DCHECK(placed_items && positions_locked_to_major_axis &&
         positions_not_locked_to_major_axis);

  placement_data_.grid_item_positions.ReserveInitialCapacity(grid_items.Size());
  placement_data_.column_start_offset = placement_data_.row_start_offset = 0;

  for (const auto& grid_item : grid_items) {
    const auto& item_style = grid_item.node.Style();

    GridArea position;
    position.columns =
        placement_data_.line_resolver.ResolveGridPositionsFromStyle(
            item_style, kForColumns);
    DCHECK(!position.columns.IsTranslatedDefinite());

    position.rows = placement_data_.line_resolver.ResolveGridPositionsFromStyle(
        item_style, kForRows);
    DCHECK(!position.rows.IsTranslatedDefinite());

    // When we have negative indices that go beyond the start of the explicit
    // grid we need to prepend tracks to it; count how many tracks are needed by
    // checking the minimum negative start line of definite spans, the negative
    // of that minimum is the number of tracks we need to prepend.
    // Simplifying the logic above: maximize the negative value of start lines.
    if (position.columns.IsUntranslatedDefinite()) {
      placement_data_.column_start_offset =
          std::max<int>(placement_data_.column_start_offset,
                        -position.columns.UntranslatedStartLine());
    }

    if (position.rows.IsUntranslatedDefinite()) {
      placement_data_.row_start_offset =
          std::max<int>(placement_data_.row_start_offset,
                        -position.rows.UntranslatedStartLine());
    }
    placement_data_.grid_item_positions.emplace_back(position);
  }

  minor_max_end_line_ = IntrinsicEndLine(minor_direction_);

  placed_items->needs_to_sort_item_vector = false;
  auto& non_auto_placed_items = placed_items->item_vector;
  non_auto_placed_items.ReserveInitialCapacity(grid_items.Size());

  for (auto& position : placement_data_.grid_item_positions) {
    GridSpan item_major_span = position.Span(major_direction_);
    GridSpan item_minor_span = position.Span(minor_direction_);

    const bool has_indefinite_major_span = item_major_span.IsIndefinite();
    const bool has_indefinite_minor_span = item_minor_span.IsIndefinite();

    if (!has_indefinite_major_span) {
      item_major_span.Translate((major_direction_ == kForColumns)
                                    ? placement_data_.column_start_offset
                                    : placement_data_.row_start_offset);
      position.SetSpan(item_major_span, major_direction_);
    }

    if (!has_indefinite_minor_span) {
      item_minor_span.Translate((minor_direction_ == kForColumns)
                                    ? placement_data_.column_start_offset
                                    : placement_data_.row_start_offset);
      position.SetSpan(item_minor_span, minor_direction_);
    }

    minor_max_end_line_ = std::max<wtf_size_t>(
        minor_max_end_line_, has_indefinite_minor_span
                                 ? item_minor_span.IndefiniteSpanSize()
                                 : item_minor_span.EndLine());

    // Prevent intrinsic tracks from overflowing the subgrid.
    if (!placement_data_.HasStandaloneAxis(minor_direction_)) {
      ClampMinorMaxToSubgridArea();
    }

    if (!has_indefinite_major_span && !has_indefinite_minor_span) {
      auto placed_item = std::make_unique<PlacedGridItem>(
          position, major_direction_, minor_direction_);

      // We will need to sort the item vector if the new placed item should be
      // inserted to the ordered list before the last item in the vector.
      placed_items->needs_to_sort_item_vector |=
          !non_auto_placed_items.empty() &&
          *placed_item < *non_auto_placed_items.back();

      non_auto_placed_items.emplace_back(std::move(placed_item));
    } else {
      if (has_indefinite_major_span)
        positions_not_locked_to_major_axis->emplace_back(&position);
      else
        positions_locked_to_major_axis->emplace_back(&position);
    }
  }
  return !positions_not_locked_to_major_axis->empty() ||
         !positions_locked_to_major_axis->empty();
}

void GridPlacement::PlaceGridItemsLockedToMajorAxis(
    const PositionVector& positions_locked_to_major_axis,
    PlacedGridItemsList* placed_items) {
  DCHECK(placed_items);

  // Mapping between the major axis tracks and the last auto-placed item's end
  // line inserted on that track. This is needed to implement "sparse" packing
  // for grid items locked to a given major axis track.
  // See https://drafts.csswg.org/css-grid/#auto-placement-algo.
  HashMap<wtf_size_t, wtf_size_t, IntWithZeroKeyHashTraits<wtf_size_t>>
      minor_cursors;

  for (auto* position : positions_locked_to_major_axis) {
    DCHECK_EQ(AutoPlacement(*position, major_direction_),
              AutoPlacementType::kMinor);

    const wtf_size_t minor_span_size =
        position->Span(minor_direction_).IndefiniteSpanSize();
    const wtf_size_t major_start_line = position->StartLine(major_direction_);

    AutoPlacementCursor placement_cursor(placed_items->FirstPlacedItem());
    placement_cursor.MoveToMajorLine(major_start_line);
    if (HasSparsePacking() && minor_cursors.Contains(major_start_line))
      placement_cursor.MoveToMinorLine(minor_cursors.at(major_start_line));

    placement_cursor.MoveCursorToFitGridSpan(
        position->SpanSize(major_direction_), minor_span_size,
        minor_max_end_line_, CursorMovementBehavior::kForceMajorLine);

    wtf_size_t minor_end_line = placement_cursor.MinorLine() + minor_span_size;
    if (HasSparsePacking())
      minor_cursors.Set(major_start_line, minor_end_line);
    minor_max_end_line_ = std::max(minor_max_end_line_, minor_end_line);

    // Prevent intrinsic tracks from overflowing the subgrid.
    if (!placement_data_.HasStandaloneAxis(minor_direction_)) {
      ClampMinorMaxToSubgridArea();
    }

    // Update grid item placement for minor axis.
    GridSpan grid_item_span = GridSpan::TranslatedDefiniteGridSpan(
        placement_cursor.MinorLine(), minor_end_line);
    position->SetSpan(grid_item_span, minor_direction_);

    PlaceGridItemAtCursor(*position, placed_items, &placement_cursor);
  }
}

void GridPlacement::PlaceAutoMajorAxisGridItem(
    GridArea* position,
    PlacedGridItemsList* placed_items,
    AutoPlacementCursor* placement_cursor) const {
  DCHECK(position && placed_items && placement_cursor);
  const wtf_size_t major_span_size =
      position->Span(major_direction_).IndefiniteSpanSize();

  placement_cursor->MoveToMinorLine(position->StartLine(minor_direction_));
  placement_cursor->MoveCursorToFitGridSpan(
      major_span_size, position->SpanSize(minor_direction_),
      minor_max_end_line_, CursorMovementBehavior::kForceMinorLine);

  // Update grid item placement for major axis.
  GridSpan grid_item_span = GridSpan::TranslatedDefiniteGridSpan(
      placement_cursor->MajorLine(),
      placement_cursor->MajorLine() + major_span_size);
  position->SetSpan(grid_item_span, major_direction_);

  PlaceGridItemAtCursor(*position, placed_items, placement_cursor);
}

void GridPlacement::PlaceAutoBothAxisGridItem(
    GridArea* position,
    PlacedGridItemsList* placed_items,
    AutoPlacementCursor* placement_cursor) const {
  DCHECK(position && placed_items && placement_cursor);

  const wtf_size_t major_span_size =
      position->Span(major_direction_).IndefiniteSpanSize();
  const wtf_size_t minor_span_size =
      position->Span(minor_direction_).IndefiniteSpanSize();

  placement_cursor->MoveCursorToFitGridSpan(major_span_size, minor_span_size,
                                            minor_max_end_line_,
                                            CursorMovementBehavior::kAuto);

  // Update grid item placement for both axis.
  GridSpan grid_item_span = GridSpan::TranslatedDefiniteGridSpan(
      placement_cursor->MajorLine(),
      placement_cursor->MajorLine() + major_span_size);
  position->SetSpan(grid_item_span, major_direction_);

  grid_item_span = GridSpan::TranslatedDefiniteGridSpan(
      placement_cursor->MinorLine(),
      placement_cursor->MinorLine() + minor_span_size);
  position->SetSpan(grid_item_span, minor_direction_);

  PlaceGridItemAtCursor(*position, placed_items, placement_cursor);
}

void GridPlacement::PlaceGridItemAtCursor(
    const GridArea& position,
    PlacedGridItemsList* placed_items,
    AutoPlacementCursor* placement_cursor) const {
  DCHECK(placed_items && placement_cursor);

  auto new_placed_item = std::make_unique<PlacedGridItem>(
      position, major_direction_, minor_direction_);
  const auto* next_placed_item = placement_cursor->NextPlacedItem();

  placed_items->ordered_list.InsertAfter(
      new_placed_item.get(), next_placed_item
                                 ? next_placed_item->Prev()
                                 : placed_items->ordered_list.Tail());

  placement_cursor->InsertPlacedItemAtCurrentPosition(new_placed_item.get());
  placed_items->item_vector.emplace_back(std::move(new_placed_item));
}

void GridPlacement::ClampGridItemsToFitSubgridArea(
    GridTrackSizingDirection track_direction) {
  const wtf_size_t subgrid_span_size =
      placement_data_.SubgridSpanSize(track_direction);

  // If no subgrid span size was specified, then we should create implicit grid
  // lines for placement, so we don't need to clamp the resolved positions.
  if (subgrid_span_size == kNotFound)
    return;

  DCHECK_GT(subgrid_span_size, 0u);
  const int start_offset = placement_data_.StartOffset(track_direction);

  for (auto& resolved_position : placement_data_.grid_item_positions) {
    // This may be called before all positions are finalized. Any definite
    // positions need to be clamped, as their positions may be used to determine
    // relative positions of the positions that are still indefinite. While
    // clamping, these indefinite positions can be skipped.
    if ((track_direction == kForColumns &&
         !resolved_position.columns.IsTranslatedDefinite()) ||
        (track_direction == kForRows &&
         !resolved_position.rows.IsTranslatedDefinite())) {
      continue;
    }

    int start_line =
        resolved_position.StartLine(track_direction) - start_offset;
    int end_line = resolved_position.EndLine(track_direction) - start_offset;

    resolved_position.SetSpan(
        GridSpan::TranslatedDefiniteGridSpan(
            ClampTo<int>(start_line, 0, subgrid_span_size - 1),
            ClampTo<int>(end_line, 1, subgrid_span_size)),
        track_direction);
  }

  // At this point, any grid item placed on a implicit grid line before the
  // subgrid's explicit grid should be clamped to its first line.
  // As such, the start offset of the explicit grid should be 0.
  if (track_direction == kForColumns)
    placement_data_.column_start_offset = 0;
  else
    placement_data_.row_start_offset = 0;
}

void GridPlacement::ClampMinorMaxToSubgridArea() {
  DCHECK(!placement_data_.HasStandaloneAxis(minor_direction_));
  wtf_size_t subgrid_max_size = IntrinsicEndLine(minor_direction_);

  // `minor_max_end_line_` starts at `subgrid_max_size` and can only grow
  // larger.
  DCHECK_GE(minor_max_end_line_, subgrid_max_size);
  if (minor_max_end_line_ > subgrid_max_size) {
    minor_max_end_line_ = subgrid_max_size;
  }
}

bool GridPlacement::HasSparsePacking() const {
  return packing_behavior_ == PackingBehavior::kSparse;
}

wtf_size_t GridPlacement::IntrinsicEndLine(
    GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns)
             ? placement_data_.column_start_offset +
                   placement_data_.ExplicitGridTrackCount(kForColumns)
             : placement_data_.row_start_offset +
                   placement_data_.ExplicitGridTrackCount(kForRows);
}

// A grid position is defined as the intersection between a line from the major
// axis and another from the minor axis. Following the auto-placement algorithm
// convention, a position with lesser major axis line comes first; in case of
// ties, a position with lesser minor axis line comes first.
bool GridPlacement::GridPosition::operator<=(const GridPosition& other) const {
  return (major_line == other.major_line) ? minor_line <= other.minor_line
                                          : major_line < other.major_line;
}
bool GridPlacement::GridPosition::operator<(const GridPosition& other) const {
  return (major_line != other.major_line) ? major_line < other.major_line
                                          : minor_line < other.minor_line;
}

GridPlacement::PlacedGridItem::PlacedGridItem(
    const GridArea& position,
    GridTrackSizingDirection major_direction,
    GridTrackSizingDirection minor_direction)
    : start_{position.StartLine(major_direction),
             position.StartLine(minor_direction)},
      end_{position.EndLine(major_direction),
           position.EndLine(minor_direction)} {}

GridPlacement::GridPosition
GridPlacement::PlacedGridItem::EndOnPreviousMajorLine() const {
  DCHECK_GT(end_.major_line, 0u);
  return {end_.major_line - 1, end_.minor_line};
}

void GridPlacement::AutoPlacementCursor::MoveCursorToFitGridSpan(
    const wtf_size_t major_span_size,
    const wtf_size_t minor_span_size,
    const wtf_size_t minor_max_end_line,
    const CursorMovementBehavior movement_behavior) {
  DCHECK_LE(minor_span_size, minor_max_end_line);

  wtf_size_t next_minor_line;
  const bool allow_minor_line_movement =
      movement_behavior != CursorMovementBehavior::kForceMinorLine;

  // If we want to force the current major line, it's okay to place this grid
  // span beyond the implicit grid's maximum minor end line.
  const wtf_size_t minor_max_start_line =
      (movement_behavior == CursorMovementBehavior::kForceMajorLine)
          ? minor_max_end_line
          : minor_max_end_line - minor_span_size;

  auto NeedsToMoveToNextMajorLine = [&]() -> bool {
    // If we need to force the minor line, or the grid span would go beyond the
    // maximum minor end line, there is no point to keep looking for overlapping
    // items in the current major line, i.e. needs to move the major line.
    return next_minor_line > minor_max_start_line ||
           (!allow_minor_line_movement &&
            next_minor_line != current_position_.minor_line);
  };

  auto DoesCurrentPositionFitGridSpan = [&]() -> bool {
    if (NeedsToMoveToNextMajorLine()) {
      MoveToNextMajorLine(allow_minor_line_movement);
    } else {
      // If the minor line didn't move, it means there was no overlap with any
      // previously placed item, and we don't need to move any further.
      if (current_position_.minor_line == next_minor_line)
        return true;

      DCHECK_LT(current_position_.minor_line, next_minor_line);
      MoveToMinorLine(next_minor_line);
    }
    return false;
  };

  if (current_position_.minor_line > minor_max_start_line)
    MoveToNextMajorLine(allow_minor_line_movement);

  while (true) {
    UpdateItemsOverlappingMajorLine();
    next_minor_line = current_position_.minor_line;
    for (const auto* placed_item : items_overlapping_major_line_) {
      const wtf_size_t minor_span_end_line = next_minor_line + minor_span_size;
      const wtf_size_t item_minor_end_line = placed_item->MinorEndLine();

      // Since we know that this item will overlap with the current major line,
      // we only need to check if the minor span will overlap too.
      if (next_minor_line < item_minor_end_line &&
          placed_item->MinorStartLine() < minor_span_end_line) {
        next_minor_line = item_minor_end_line;
        if (NeedsToMoveToNextMajorLine())
          break;
      }
    }

    // If the next minor line was moved because it overlapped with a placed
    // item, we don't need to check for overlaps with the rest of the upcoming
    // placed items; keep looking for a position that doesn't overlap with the
    // set of items overlapping the current major line first.
    if (!DoesCurrentPositionFitGridSpan())
      continue;

    const auto* upcoming_item = next_placed_item_;
    while (upcoming_item) {
      const wtf_size_t major_span_end_line =
          current_position_.major_line + major_span_size;
      const wtf_size_t minor_span_end_line = next_minor_line + minor_span_size;
      const wtf_size_t item_minor_end_line = upcoming_item->MinorEndLine();

      // Check if the cursor would overlap the upcoming placed item.
      if (next_minor_line < item_minor_end_line &&
          current_position_.major_line < upcoming_item->MajorEndLine() &&
          upcoming_item->MajorStartLine() < major_span_end_line &&
          upcoming_item->MinorStartLine() < minor_span_end_line) {
        next_minor_line = item_minor_end_line;
        if (NeedsToMoveToNextMajorLine())
          break;
      }
      upcoming_item = upcoming_item->Next();
    }

    if (DoesCurrentPositionFitGridSpan()) {
      // No overlap with any placed item.
      break;
    }
  }
}

void GridPlacement::AutoPlacementCursor::UpdateItemsOverlappingMajorLine() {
  DCHECK(std::is_heap(items_overlapping_major_line_.begin(),
                      items_overlapping_major_line_.end(),
                      ComparePlacedGridItemsByEnd));

  while (!items_overlapping_major_line_.empty()) {
    // Notice that the |EndOnPreviousMajorLine| of an item "A" is the first
    // position such that any upcoming grid position (located at a greater
    // major/minor position) is guaranteed to not overlap with "A".
    auto last_overlapping_position =
        items_overlapping_major_line_.front()->EndOnPreviousMajorLine();

    // We cannot discard any items since they're still overlapping.
    if (current_position_ < last_overlapping_position)
      break;

    // When we are located at the major line right before the current item's
    // major end line, we want to ensure that we move to the next major line
    // since it won't be considered overlapping in |MoveToNextMajorLine| now
    // that we moved past the item's |EndOnPreviousMajorLine|.
    if (current_position_.major_line == last_overlapping_position.major_line)
      should_move_to_next_item_major_end_line_ = false;

    std::pop_heap(items_overlapping_major_line_.begin(),
                  items_overlapping_major_line_.end(),
                  ComparePlacedGridItemsByEnd);
    items_overlapping_major_line_.pop_back();
  }

  while (next_placed_item_ && next_placed_item_->Start() <= current_position_) {
    auto last_overlapping_position =
        next_placed_item_->EndOnPreviousMajorLine();

    // If the current position's major line overlaps the next placed item, we
    // should retry the auto-placement algorithm on the next major line before
    // trying to skip to the nearest major end line of an overlapping item.
    if (current_position_.major_line <= last_overlapping_position.major_line)
      should_move_to_next_item_major_end_line_ = false;

    if (current_position_ < last_overlapping_position) {
      items_overlapping_major_line_.emplace_back(next_placed_item_);
      std::push_heap(items_overlapping_major_line_.begin(),
                     items_overlapping_major_line_.end(),
                     ComparePlacedGridItemsByEnd);
    }
    next_placed_item_ = next_placed_item_->Next();
  }
}

void GridPlacement::AutoPlacementCursor::MoveToMajorLine(
    const wtf_size_t major_line) {
  DCHECK_LE(current_position_.major_line, major_line);
  current_position_.major_line = major_line;
}

void GridPlacement::AutoPlacementCursor::MoveToMinorLine(
    const wtf_size_t minor_line) {
  // Since the auto-placement cursor only moves forward to the next minor line,
  // if the cursor is located at a position after the minor line we want to
  // force, cycle back to such minor line in the next major line.
  if (minor_line < current_position_.minor_line)
    ++current_position_.major_line;
  current_position_.minor_line = minor_line;
}

void GridPlacement::AutoPlacementCursor::MoveToNextMajorLine(
    bool allow_minor_line_movement) {
  ++current_position_.major_line;

  if (should_move_to_next_item_major_end_line_ &&
      !items_overlapping_major_line_.empty()) {
    DCHECK_GE(items_overlapping_major_line_.front()->MajorEndLine(),
              current_position_.major_line);
    current_position_.major_line =
        items_overlapping_major_line_.front()->MajorEndLine();
  }

  if (allow_minor_line_movement)
    current_position_.minor_line = 0;
  should_move_to_next_item_major_end_line_ = true;
}

void GridPlacement::AutoPlacementCursor::InsertPlacedItemAtCurrentPosition(
    const PlacedGridItem* new_placed_item) {
  // This update must happen after the doubly linked list already updated its
  // element links to keep the necessary order for the cursor's logic.
#if DCHECK_IS_ON()
  if (next_placed_item_) {
    DCHECK_EQ(next_placed_item_->Prev(), new_placed_item);
    DCHECK(*new_placed_item < *next_placed_item_);
  }
#endif
  DCHECK_EQ(new_placed_item->Next(), next_placed_item_);
  next_placed_item_ = new_placed_item;

  MoveToMinorLine(new_placed_item->MinorEndLine());
  UpdateItemsOverlappingMajorLine();
}

void GridPlacement::PlacedGridItemsList::AppendCurrentItemsToOrderedList() {
  DCHECK(ordered_list.empty());

  auto ComparePlacedGridItemPointers =
      [](const std::unique_ptr<PlacedGridItem>& lhs,
         const std::unique_ptr<PlacedGridItem>& rhs) { return *lhs < *rhs; };

  if (needs_to_sort_item_vector) {
    std::sort(item_vector.begin(), item_vector.end(),
              ComparePlacedGridItemPointers);
  }
  DCHECK(std::is_sorted(item_vector.begin(), item_vector.end(),
                        ComparePlacedGridItemPointers));

  for (auto& placed_item : item_vector)
    ordered_list.Append(placed_item.get());
}

// static
void GridPlacement::ResolveOutOfFlowItemGridLines(
    const GridLayoutTrackCollection& track_collection,
    const GridLineResolver& line_resolver,
    const ComputedStyle& grid_style,
    const ComputedStyle& item_style,
    wtf_size_t start_offset,
    wtf_size_t* start_line,
    wtf_size_t* end_line) {
  DCHECK(start_line && end_line);

  *start_line = kNotFound;
  *end_line = kNotFound;

  const auto track_direction = track_collection.Direction();
  const bool is_for_columns = track_direction == kForColumns;

  const auto span =
      line_resolver.ResolveGridPositionsFromStyle(item_style, track_direction);

  if (span.IsIndefinite())
    return;

  const int span_start_line = span.UntranslatedStartLine() + start_offset;
  const int span_end_line = span.UntranslatedEndLine() + start_offset;

  if (span_start_line >= 0 &&
      (is_for_columns ? !item_style.GridColumnStart().IsAuto()
                      : !item_style.GridRowStart().IsAuto()) &&
      track_collection.IsGridLineWithinImplicitGrid(span_start_line)) {
    *start_line = span_start_line;
  }

  if (span_end_line >= 0 &&
      (is_for_columns ? !item_style.GridColumnEnd().IsAuto()
                      : !item_style.GridRowEnd().IsAuto()) &&
      track_collection.IsGridLineWithinImplicitGrid(span_end_line)) {
    *end_line = span_end_line;
  }
}

}  // namespace blink
