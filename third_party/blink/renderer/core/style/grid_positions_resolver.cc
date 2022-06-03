// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/grid_positions_resolver.h"

#include <algorithm>
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/style/grid_area.h"

namespace blink {

static inline GridTrackSizingDirection DirectionFromSide(
    GridPositionSide side) {
  return side == kColumnStartSide || side == kColumnEndSide ? kForColumns
                                                            : kForRows;
}

static inline String ImplicitNamedGridLineForSide(const String& line_name,
                                                  GridPositionSide side) {
  return line_name + ((side == kColumnStartSide || side == kRowStartSide)
                          ? "-start"
                          : "-end");
}

NamedLineCollection::NamedLineCollection(
    const ComputedStyle& grid_container_style,
    const String& named_line,
    GridTrackSizingDirection direction,
    wtf_size_t last_line,
    wtf_size_t auto_repeat_tracks_count)
    : last_line_(last_line),
      auto_repeat_total_tracks_(auto_repeat_tracks_count) {
  bool is_row_axis = direction == kForColumns;
  const NamedGridLinesMap& grid_line_names =
      is_row_axis ? grid_container_style.NamedGridColumnLines()
                  : grid_container_style.NamedGridRowLines();
  const NamedGridLinesMap& auto_repeat_grid_line_names =
      is_row_axis ? grid_container_style.AutoRepeatNamedGridColumnLines()
                  : grid_container_style.AutoRepeatNamedGridRowLines();
  const NamedGridLinesMap& implicit_grid_line_names =
      is_row_axis ? grid_container_style.ImplicitNamedGridColumnLines()
                  : grid_container_style.ImplicitNamedGridRowLines();

  if (!grid_line_names.IsEmpty()) {
    auto it = grid_line_names.find(named_line);
    named_lines_indexes_ = it == grid_line_names.end() ? nullptr : &it->value;
  }

  if (!auto_repeat_grid_line_names.IsEmpty()) {
    auto it = auto_repeat_grid_line_names.find(named_line);
    auto_repeat_named_lines_indexes_ =
        it == auto_repeat_grid_line_names.end() ? nullptr : &it->value;
  }

  if (!implicit_grid_line_names.IsEmpty()) {
    auto it = implicit_grid_line_names.find(named_line);
    implicit_named_lines_indexes_ =
        it == implicit_grid_line_names.end() ? nullptr : &it->value;
  }

  insertion_point_ =
      is_row_axis ? grid_container_style.GridAutoRepeatColumnsInsertionPoint()
                  : grid_container_style.GridAutoRepeatRowsInsertionPoint();

  auto_repeat_track_list_length_ =
      is_row_axis ? grid_container_style.GridAutoRepeatColumns().size()
                  : grid_container_style.GridAutoRepeatRows().size();
}

bool NamedLineCollection::HasExplicitNamedLines() {
  return named_lines_indexes_ || auto_repeat_named_lines_indexes_;
}

bool NamedLineCollection::HasNamedLines() {
  return HasExplicitNamedLines() || implicit_named_lines_indexes_;
}

bool NamedLineCollection::Contains(wtf_size_t line) {
  CHECK(HasNamedLines());

  if (line > last_line_)
    return false;

  auto find = [](const Vector<wtf_size_t>* indexes, wtf_size_t line) {
    return indexes && indexes->Find(line) != kNotFound;
  };

  if (find(implicit_named_lines_indexes_, line))
    return true;

  if (auto_repeat_track_list_length_ == 0 || line < insertion_point_)
    return find(named_lines_indexes_, line);

  DCHECK(auto_repeat_total_tracks_);

  if (line > insertion_point_ + auto_repeat_total_tracks_)
    return find(named_lines_indexes_, line - (auto_repeat_total_tracks_ - 1));

  if (line == insertion_point_) {
    return find(named_lines_indexes_, line) ||
           find(auto_repeat_named_lines_indexes_, 0);
  }

  if (line == insertion_point_ + auto_repeat_total_tracks_) {
    return find(auto_repeat_named_lines_indexes_,
                auto_repeat_track_list_length_) ||
           find(named_lines_indexes_, insertion_point_ + 1);
  }

  wtf_size_t auto_repeat_index_in_first_repetition =
      (line - insertion_point_) % auto_repeat_track_list_length_;
  if (!auto_repeat_index_in_first_repetition &&
      find(auto_repeat_named_lines_indexes_, auto_repeat_track_list_length_))
    return true;
  return find(auto_repeat_named_lines_indexes_,
              auto_repeat_index_in_first_repetition);
}

wtf_size_t NamedLineCollection::FirstExplicitPosition() {
  DCHECK(HasExplicitNamedLines());

  wtf_size_t first_line = 0;

  // If there is no auto repeat(), there must be some named line outside, return
  // the 1st one. Also return it if it precedes the auto-repeat().
  if (auto_repeat_track_list_length_ == 0 ||
      (named_lines_indexes_ &&
       named_lines_indexes_->at(first_line) <= insertion_point_))
    return named_lines_indexes_->at(first_line);

  // Return the 1st named line inside the auto repeat(), if any.
  if (auto_repeat_named_lines_indexes_)
    return auto_repeat_named_lines_indexes_->at(first_line) + insertion_point_;

  // The 1st named line must be after the auto repeat().
  return named_lines_indexes_->at(first_line) + auto_repeat_total_tracks_ - 1;
}

wtf_size_t NamedLineCollection::FirstPosition() {
  CHECK(HasNamedLines());

  wtf_size_t first_line = 0;

  if (!implicit_named_lines_indexes_)
    return FirstExplicitPosition();

  if (!HasExplicitNamedLines())
    return implicit_named_lines_indexes_->at(first_line);

  return std::min(FirstExplicitPosition(),
                  implicit_named_lines_indexes_->at(first_line));
}

GridPositionSide GridPositionsResolver::InitialPositionSide(
    GridTrackSizingDirection direction) {
  return (direction == kForColumns) ? kColumnStartSide : kRowStartSide;
}

GridPositionSide GridPositionsResolver::FinalPositionSide(
    GridTrackSizingDirection direction) {
  return (direction == kForColumns) ? kColumnEndSide : kRowEndSide;
}

static void InitialAndFinalPositionsFromStyle(
    const ComputedStyle& grid_item_style,
    GridTrackSizingDirection direction,
    GridPosition& initial_position,
    GridPosition& final_position) {
  initial_position = (direction == kForColumns)
                         ? grid_item_style.GridColumnStart()
                         : grid_item_style.GridRowStart();
  final_position = (direction == kForColumns) ? grid_item_style.GridColumnEnd()
                                              : grid_item_style.GridRowEnd();

  // We must handle the placement error handling code here instead of in the
  // StyleAdjuster because we don't want to overwrite the specified values.
  if (initial_position.IsSpan() && final_position.IsSpan())
    final_position.SetAutoPosition();

  // If the grid item has an automatic position and a grid span for a named line
  // in a given dimension, instead treat the grid span as one.
  if (initial_position.IsAuto() && final_position.IsSpan() &&
      !final_position.NamedGridLine().IsNull())
    final_position.SetSpanPosition(1, g_null_atom);
  if (final_position.IsAuto() && initial_position.IsSpan() &&
      !initial_position.NamedGridLine().IsNull())
    initial_position.SetSpanPosition(1, g_null_atom);
}

static wtf_size_t LookAheadForNamedGridLine(
    int start,
    wtf_size_t number_of_lines,
    wtf_size_t grid_last_line,
    NamedLineCollection& lines_collection) {
  DCHECK(number_of_lines);

  // Only implicit lines on the search direction are assumed to have the given
  // name, so we can start to look from first line.
  // See: https://drafts.csswg.org/css-grid/#grid-placement-span-int
  wtf_size_t end = std::max(start, 0);

  if (!lines_collection.HasNamedLines()) {
    end = std::max(end, grid_last_line + 1);
    return end + number_of_lines - 1;
  }

  for (; number_of_lines; ++end) {
    if (end > grid_last_line || lines_collection.Contains(end))
      number_of_lines--;
  }

  DCHECK(end);
  return end - 1;
}

static int LookBackForNamedGridLine(int end,
                                    wtf_size_t number_of_lines,
                                    int grid_last_line,
                                    NamedLineCollection& lines_collection) {
  DCHECK(number_of_lines);

  // Only implicit lines on the search direction are assumed to have the given
  // name, so we can start to look from last line.
  // See: https://drafts.csswg.org/css-grid/#grid-placement-span-int
  int start = std::min(end, grid_last_line);

  if (!lines_collection.HasNamedLines()) {
    start = std::min(start, -1);
    return start - number_of_lines + 1;
  }

  for (; number_of_lines; --start) {
    if (start < 0 || lines_collection.Contains(start))
      number_of_lines--;
  }

  return start + 1;
}

static GridSpan DefiniteGridSpanWithNamedSpanAgainstOpposite(
    int opposite_line,
    const GridPosition& position,
    GridPositionSide side,
    int last_line,
    NamedLineCollection& lines_collection) {
  int start, end;
  if (side == kRowStartSide || side == kColumnStartSide) {
    start = LookBackForNamedGridLine(opposite_line - 1, position.SpanPosition(),
                                     last_line, lines_collection);
    end = opposite_line;
  } else {
    start = opposite_line;
    end = LookAheadForNamedGridLine(opposite_line + 1, position.SpanPosition(),
                                    last_line, lines_collection);
  }

  return GridSpan::UntranslatedDefiniteGridSpan(start, end);
}

wtf_size_t GridPositionsResolver::ExplicitGridColumnCount(
    const ComputedStyle& grid_container_style,
    wtf_size_t auto_repeat_tracks_count) {
  return std::min<wtf_size_t>(
      std::max(
          grid_container_style.GridTemplateColumns().LegacyTrackList().size() +
              auto_repeat_tracks_count,
          grid_container_style.NamedGridAreaColumnCount()),
      kGridMaxTracks);
}

wtf_size_t GridPositionsResolver::ExplicitGridRowCount(
    const ComputedStyle& grid_container_style,
    wtf_size_t auto_repeat_tracks_count) {
  return std::min<wtf_size_t>(
      std::max(
          grid_container_style.GridTemplateRows().LegacyTrackList().size() +
              auto_repeat_tracks_count,
          grid_container_style.NamedGridAreaRowCount()),
      kGridMaxTracks);
}

static wtf_size_t ExplicitGridSizeForSide(
    const ComputedStyle& grid_container_style,
    GridPositionSide side,
    wtf_size_t auto_repeat_tracks_count) {
  return (side == kColumnStartSide || side == kColumnEndSide)
             ? GridPositionsResolver::ExplicitGridColumnCount(
                   grid_container_style, auto_repeat_tracks_count)
             : GridPositionsResolver::ExplicitGridRowCount(
                   grid_container_style, auto_repeat_tracks_count);
}

static GridSpan ResolveNamedGridLinePositionAgainstOppositePosition(
    const ComputedStyle& grid_container_style,
    int opposite_line,
    const GridPosition& position,
    wtf_size_t auto_repeat_tracks_count,
    GridPositionSide side) {
  DCHECK(position.IsSpan());
  DCHECK(!position.NamedGridLine().IsNull());
  // Negative positions are not allowed per the specification and should have
  // been handled during parsing.
  DCHECK_GT(position.SpanPosition(), 0);

  wtf_size_t last_line = ExplicitGridSizeForSide(grid_container_style, side,
                                                 auto_repeat_tracks_count);
  NamedLineCollection lines_collection(
      grid_container_style, position.NamedGridLine(), DirectionFromSide(side),
      last_line, auto_repeat_tracks_count);
  return DefiniteGridSpanWithNamedSpanAgainstOpposite(
      opposite_line, position, side, last_line, lines_collection);
}

static GridSpan DefiniteGridSpanWithSpanAgainstOpposite(
    int opposite_line,
    const GridPosition& position,
    GridPositionSide side) {
  wtf_size_t position_offset = position.SpanPosition();
  if (side == kColumnStartSide || side == kRowStartSide)
    return GridSpan::UntranslatedDefiniteGridSpan(
        opposite_line - position_offset, opposite_line);

  return GridSpan::UntranslatedDefiniteGridSpan(
      opposite_line, opposite_line + position_offset);
}

static GridSpan ResolveGridPositionAgainstOppositePosition(
    const ComputedStyle& grid_container_style,
    int opposite_line,
    const GridPosition& position,
    GridPositionSide side,
    wtf_size_t auto_repeat_tracks_count) {
  if (position.IsAuto()) {
    if (side == kColumnStartSide || side == kRowStartSide)
      return GridSpan::UntranslatedDefiniteGridSpan(opposite_line - 1,
                                                    opposite_line);
    return GridSpan::UntranslatedDefiniteGridSpan(opposite_line,
                                                  opposite_line + 1);
  }

  DCHECK(position.IsSpan());
  DCHECK_GT(position.SpanPosition(), 0);

  if (!position.NamedGridLine().IsNull()) {
    // span 2 'c' -> we need to find the appropriate grid line before / after
    // our opposite position.
    return ResolveNamedGridLinePositionAgainstOppositePosition(
        grid_container_style, opposite_line, position, auto_repeat_tracks_count,
        side);
  }

  return DefiniteGridSpanWithSpanAgainstOpposite(opposite_line, position, side);
}

static wtf_size_t SpanSizeFromPositions(const GridPosition& initial_position,
                                        const GridPosition& final_position) {
  // This method will only be used when both positions need to be resolved
  // against the opposite one.
  DCHECK(initial_position.ShouldBeResolvedAgainstOppositePosition() &&
         final_position.ShouldBeResolvedAgainstOppositePosition());

  if (initial_position.IsAuto() && final_position.IsAuto())
    return 1;

  const GridPosition& span_position =
      initial_position.IsSpan() ? initial_position : final_position;
  DCHECK(span_position.IsSpan() && span_position.SpanPosition());
  return span_position.SpanPosition();
}

wtf_size_t GridPositionsResolver::SpanSizeForAutoPlacedItem(
    const ComputedStyle& grid_item_style,
    GridTrackSizingDirection direction) {
  GridPosition initial_position, final_position;
  InitialAndFinalPositionsFromStyle(grid_item_style, direction,
                                    initial_position, final_position);
  return SpanSizeFromPositions(initial_position, final_position);
}

static int ResolveNamedGridLinePositionFromStyle(
    const ComputedStyle& grid_container_style,
    const GridPosition& position,
    GridPositionSide side,
    wtf_size_t auto_repeat_tracks_count) {
  DCHECK(!position.NamedGridLine().IsNull());

  wtf_size_t last_line = ExplicitGridSizeForSide(grid_container_style, side,
                                                 auto_repeat_tracks_count);
  NamedLineCollection lines_collection(
      grid_container_style, position.NamedGridLine(), DirectionFromSide(side),
      last_line, auto_repeat_tracks_count);

  if (position.IsPositive())
    return LookAheadForNamedGridLine(0, abs(position.IntegerPosition()),
                                     last_line, lines_collection);

  return LookBackForNamedGridLine(last_line, abs(position.IntegerPosition()),
                                  last_line, lines_collection);
}

static int ResolveGridPositionFromStyle(
    const ComputedStyle& grid_container_style,
    const GridPosition& position,
    GridPositionSide side,
    wtf_size_t auto_repeat_tracks_count) {
  switch (position.GetType()) {
    case kExplicitPosition: {
      DCHECK(position.IntegerPosition());

      if (!position.NamedGridLine().IsNull())
        return ResolveNamedGridLinePositionFromStyle(
            grid_container_style, position, side, auto_repeat_tracks_count);

      // Handle <integer> explicit position.
      if (position.IsPositive())
        return position.IntegerPosition() - 1;

      wtf_size_t resolved_position = abs(position.IntegerPosition()) - 1;
      wtf_size_t end_of_track = ExplicitGridSizeForSide(
          grid_container_style, side, auto_repeat_tracks_count);

      return end_of_track - resolved_position;
    }
    case kNamedGridAreaPosition: {
      // First attempt to match the grid area's edge to a named grid area: if
      // there is a named line with the name ''<custom-ident>-start (for
      // grid-*-start) / <custom-ident>-end'' (for grid-*-end), contributes the
      // first such line to the grid item's placement.
      String named_grid_line = position.NamedGridLine();
      DCHECK(!position.NamedGridLine().IsNull());

      wtf_size_t last_line = ExplicitGridSizeForSide(grid_container_style, side,
                                                     auto_repeat_tracks_count);
      NamedLineCollection implicit_lines(
          grid_container_style,
          ImplicitNamedGridLineForSide(named_grid_line, side),
          DirectionFromSide(side), last_line, auto_repeat_tracks_count);
      if (implicit_lines.HasNamedLines())
        return implicit_lines.FirstPosition();

      // Otherwise, if there is a named line with the specified name,
      // contributes the first such line to the grid item's placement.
      NamedLineCollection explicit_lines(grid_container_style, named_grid_line,
                                         DirectionFromSide(side), last_line,
                                         auto_repeat_tracks_count);
      if (explicit_lines.HasNamedLines())
        return explicit_lines.FirstPosition();

      // If none of the above works specs mandate to assume that all the lines
      // in the implicit grid have this name.
      return last_line + 1;
    }
    case kAutoPosition:
    case kSpanPosition:
      // 'auto' and span depend on the opposite position for resolution (e.g.
      // grid-row: auto / 1 or grid-column: span 3 / "myHeader").
      NOTREACHED();
      return 0;
  }
  NOTREACHED();
  return 0;
}

GridSpan GridPositionsResolver::ResolveGridPositionsFromStyle(
    const ComputedStyle& grid_container_style,
    const ComputedStyle& grid_item_style,
    GridTrackSizingDirection direction,
    wtf_size_t auto_repeat_tracks_count) {
  GridPosition initial_position, final_position;
  InitialAndFinalPositionsFromStyle(grid_item_style, direction,
                                    initial_position, final_position);

  GridPositionSide initial_side = InitialPositionSide(direction);
  GridPositionSide final_side = FinalPositionSide(direction);

  if (initial_position.ShouldBeResolvedAgainstOppositePosition() &&
      final_position.ShouldBeResolvedAgainstOppositePosition()) {
    // We can't get our grid positions without running the auto placement
    // algorithm.
    return GridSpan::IndefiniteGridSpan(
        SpanSizeFromPositions(initial_position, final_position));
  }

  if (initial_position.ShouldBeResolvedAgainstOppositePosition()) {
    // Infer the position from the final position ('auto / 1' or 'span 2 / 3'
    // case).
    int end_line =
        ResolveGridPositionFromStyle(grid_container_style, final_position,
                                     final_side, auto_repeat_tracks_count);
    return ResolveGridPositionAgainstOppositePosition(
        grid_container_style, end_line, initial_position, initial_side,
        auto_repeat_tracks_count);
  }

  if (final_position.ShouldBeResolvedAgainstOppositePosition()) {
    // Infer our position from the initial position ('1 / auto' or '3 / span 2'
    // case).
    int start_line =
        ResolveGridPositionFromStyle(grid_container_style, initial_position,
                                     initial_side, auto_repeat_tracks_count);
    return ResolveGridPositionAgainstOppositePosition(
        grid_container_style, start_line, final_position, final_side,
        auto_repeat_tracks_count);
  }

  int start_line =
      ResolveGridPositionFromStyle(grid_container_style, initial_position,
                                   initial_side, auto_repeat_tracks_count);
  int end_line =
      ResolveGridPositionFromStyle(grid_container_style, final_position,
                                   final_side, auto_repeat_tracks_count);

  if (end_line < start_line)
    std::swap(end_line, start_line);
  else if (end_line == start_line)
    end_line = start_line + 1;

  return GridSpan::UntranslatedDefiniteGridSpan(start_line, end_line);
}

}  // namespace blink
