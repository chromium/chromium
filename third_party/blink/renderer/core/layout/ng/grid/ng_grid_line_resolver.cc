// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_line_resolver.h"

#include <algorithm>
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_named_line_collection.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/grid_area.h"
#include "third_party/blink/renderer/core/style/grid_position.h"

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

static void InitialAndFinalPositionsFromStyle(
    const ComputedStyle& grid_item_style,
    GridTrackSizingDirection track_direction,
    GridPosition& initial_position,
    GridPosition& final_position) {
  const bool is_for_columns = track_direction == kForColumns;
  initial_position = is_for_columns ? grid_item_style.GridColumnStart()
                                    : grid_item_style.GridRowStart();
  final_position = is_for_columns ? grid_item_style.GridColumnEnd()
                                  : grid_item_style.GridRowEnd();

  // We must handle the placement error handling code here instead of in the
  // StyleAdjuster because we don't want to overwrite the specified values.
  if (initial_position.IsSpan() && final_position.IsSpan())
    final_position.SetAutoPosition();

  // If the grid item has an automatic position and a grid span for a named line
  // in a given dimension, instead treat the grid span as one.
  if (initial_position.IsAuto() && final_position.IsSpan() &&
      !final_position.NamedGridLine().IsNull()) {
    final_position.SetSpanPosition(1, g_null_atom);
  }
  if (final_position.IsAuto() && initial_position.IsSpan() &&
      !initial_position.NamedGridLine().IsNull()) {
    initial_position.SetSpanPosition(1, g_null_atom);
  }
}

static wtf_size_t LookAheadForNamedGridLine(
    int start,
    wtf_size_t number_of_lines,
    wtf_size_t grid_last_line,
    NGGridNamedLineCollection& lines_collection) {
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

static int LookBackForNamedGridLine(
    int end,
    wtf_size_t number_of_lines,
    int grid_last_line,
    NGGridNamedLineCollection& lines_collection) {
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
    NGGridNamedLineCollection& lines_collection) {
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

wtf_size_t NGGridLineResolver::ExplicitGridColumnCount(
    const ComputedStyle& grid_container_style,
    wtf_size_t auto_repeat_tracks_count,
    wtf_size_t subgrid_span_size) {
  if (subgrid_span_size != kNotFound)
    return subgrid_span_size;

  const auto& track_list =
      grid_container_style.GridTemplateColumns().track_sizes;
  const wtf_size_t total_track_count =
      track_list.NGTrackList().TrackCountWithoutAutoRepeat();

  return std::min<wtf_size_t>(
      std::max(total_track_count + auto_repeat_tracks_count,
               grid_container_style.NamedGridAreaColumnCount()),
      kGridMaxTracks);
}

wtf_size_t NGGridLineResolver::ExplicitGridRowCount(
    const ComputedStyle& grid_container_style,
    wtf_size_t auto_repeat_tracks_count,
    wtf_size_t subgrid_span_size) {
  if (subgrid_span_size != kNotFound)
    return subgrid_span_size;

  const auto& track_list = grid_container_style.GridTemplateRows().track_sizes;
  const wtf_size_t total_track_count =
      track_list.NGTrackList().TrackCountWithoutAutoRepeat();

  return std::min<wtf_size_t>(
      std::max(total_track_count + auto_repeat_tracks_count,
               grid_container_style.NamedGridAreaRowCount()),
      kGridMaxTracks);
}

static wtf_size_t ExplicitGridSizeForSide(
    const ComputedStyle& grid_container_style,
    GridPositionSide side,
    wtf_size_t auto_repeat_tracks_count,
    wtf_size_t subgrid_span_size) {
  return (side == kColumnStartSide || side == kColumnEndSide)
             ? NGGridLineResolver::ExplicitGridColumnCount(
                   grid_container_style, auto_repeat_tracks_count,
                   subgrid_span_size)
             : NGGridLineResolver::ExplicitGridRowCount(
                   grid_container_style, auto_repeat_tracks_count,
                   subgrid_span_size);
}

static GridSpan ResolveNamedGridLinePositionAgainstOppositePosition(
    const ComputedStyle& grid_container_style,
    int opposite_line,
    const GridPosition& position,
    wtf_size_t auto_repeat_tracks_count,
    GridPositionSide side,
    wtf_size_t subgrid_span_size) {
  DCHECK(position.IsSpan());
  DCHECK(!position.NamedGridLine().IsNull());
  // Negative positions are not allowed per the specification and should have
  // been handled during parsing.
  DCHECK_GT(position.SpanPosition(), 0);

  wtf_size_t last_line = ExplicitGridSizeForSide(
      grid_container_style, side, auto_repeat_tracks_count, subgrid_span_size);
  NGGridNamedLineCollection lines_collection(
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
  if (side == kColumnStartSide || side == kRowStartSide) {
    return GridSpan::UntranslatedDefiniteGridSpan(
        opposite_line - position_offset, opposite_line);
  }

  return GridSpan::UntranslatedDefiniteGridSpan(
      opposite_line, opposite_line + position_offset);
}

static GridSpan ResolveGridPositionAgainstOppositePosition(
    const ComputedStyle& grid_container_style,
    int opposite_line,
    const GridPosition& position,
    GridPositionSide side,
    wtf_size_t auto_repeat_tracks_count,
    wtf_size_t subgrid_span_size) {
  if (position.IsAuto()) {
    if (side == kColumnStartSide || side == kRowStartSide) {
      return GridSpan::UntranslatedDefiniteGridSpan(opposite_line - 1,
                                                    opposite_line);
    }
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
        side, subgrid_span_size);
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

wtf_size_t NGGridLineResolver::SpanSizeForAutoPlacedItem(
    const ComputedStyle& grid_item_style,
    GridTrackSizingDirection track_direction) {
  GridPosition initial_position, final_position;
  InitialAndFinalPositionsFromStyle(grid_item_style, track_direction,
                                    initial_position, final_position);
  return SpanSizeFromPositions(initial_position, final_position);
}

static int ResolveNamedGridLinePositionFromStyle(
    const ComputedStyle& grid_container_style,
    const GridPosition& position,
    GridPositionSide side,
    wtf_size_t auto_repeat_tracks_count,
    wtf_size_t subgrid_span_size) {
  DCHECK(!position.NamedGridLine().IsNull());

  wtf_size_t last_line = ExplicitGridSizeForSide(
      grid_container_style, side, auto_repeat_tracks_count, subgrid_span_size);
  NGGridNamedLineCollection lines_collection(
      grid_container_style, position.NamedGridLine(), DirectionFromSide(side),
      last_line, auto_repeat_tracks_count);

  if (position.IsPositive()) {
    return LookAheadForNamedGridLine(0, abs(position.IntegerPosition()),
                                     last_line, lines_collection);
  }

  return LookBackForNamedGridLine(last_line, abs(position.IntegerPosition()),
                                  last_line, lines_collection);
}

static int ResolveGridPositionFromStyle(
    const ComputedStyle& grid_container_style,
    const GridPosition& position,
    GridPositionSide side,
    wtf_size_t auto_repeat_tracks_count,
    bool is_parent_grid_container,
    wtf_size_t subgrid_span_size) {
  switch (position.GetType()) {
    case kExplicitPosition: {
      DCHECK(position.IntegerPosition());

      if (!position.NamedGridLine().IsNull()) {
        return ResolveNamedGridLinePositionFromStyle(
            grid_container_style, position, side, auto_repeat_tracks_count,
            subgrid_span_size);
      }

      // Handle <integer> explicit position.
      if (position.IsPositive())
        return position.IntegerPosition() - 1;

      wtf_size_t resolved_position = abs(position.IntegerPosition()) - 1;
      wtf_size_t end_of_track =
          ExplicitGridSizeForSide(grid_container_style, side,
                                  auto_repeat_tracks_count, subgrid_span_size);

      return end_of_track - resolved_position;
    }
    case kNamedGridAreaPosition: {
      // First attempt to match the grid area's edge to a named grid area: if
      // there is a named line with the name ''<custom-ident>-start (for
      // grid-*-start) / <custom-ident>-end'' (for grid-*-end), contributes the
      // first such line to the grid item's placement.
      String named_grid_line = position.NamedGridLine();
      DCHECK(!position.NamedGridLine().IsNull());

      wtf_size_t last_line =
          ExplicitGridSizeForSide(grid_container_style, side,
                                  auto_repeat_tracks_count, subgrid_span_size);
      NGGridNamedLineCollection implicit_lines(
          grid_container_style,
          ImplicitNamedGridLineForSide(named_grid_line, side),
          DirectionFromSide(side), last_line, auto_repeat_tracks_count);
      if (implicit_lines.HasNamedLines())
        return implicit_lines.FirstPosition();

      // Otherwise, if there is a named line with the specified name,
      // contributes the first such line to the grid item's placement.
      NGGridNamedLineCollection explicit_lines(
          grid_container_style, named_grid_line, DirectionFromSide(side),
          last_line, auto_repeat_tracks_count, is_parent_grid_container);
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

GridSpan NGGridLineResolver::ResolveGridPositionsFromStyle(
    const ComputedStyle& grid_container_style,
    const ComputedStyle& grid_item_style,
    GridTrackSizingDirection track_direction,
    wtf_size_t auto_repeat_tracks_count,
    bool is_parent_grid_container,
    wtf_size_t subgrid_span_size) {
  GridPosition initial_position, final_position;
  InitialAndFinalPositionsFromStyle(grid_item_style, track_direction,
                                    initial_position, final_position);

  const bool initial_should_be_resolved_against_opposite_position =
      initial_position.ShouldBeResolvedAgainstOppositePosition();
  const bool final_should_be_resolved_against_opposite_position =
      final_position.ShouldBeResolvedAgainstOppositePosition();

  if (initial_should_be_resolved_against_opposite_position &&
      final_should_be_resolved_against_opposite_position) {
    // We can't get our grid positions without running the auto placement
    // algorithm.
    return GridSpan::IndefiniteGridSpan(
        SpanSizeFromPositions(initial_position, final_position));
  }

  const GridPositionSide initial_side =
      (track_direction == kForColumns) ? kColumnStartSide : kRowStartSide;
  const GridPositionSide final_side =
      (track_direction == kForColumns) ? kColumnEndSide : kRowEndSide;

  if (initial_should_be_resolved_against_opposite_position) {
    // Infer the position from the final_position position ('auto / 1' or 'span
    // 2 / 3' case).
    int end_line = ResolveGridPositionFromStyle(
        grid_container_style, final_position, final_side,
        auto_repeat_tracks_count, is_parent_grid_container, subgrid_span_size);
    return ResolveGridPositionAgainstOppositePosition(
        grid_container_style, end_line, initial_position, initial_side,
        auto_repeat_tracks_count, subgrid_span_size);
  }

  if (final_should_be_resolved_against_opposite_position) {
    // Infer our position from the initial_position position ('1 / auto' or '3 /
    // span 2' case).
    int start_line = ResolveGridPositionFromStyle(
        grid_container_style, initial_position, initial_side,
        auto_repeat_tracks_count, is_parent_grid_container, subgrid_span_size);
    return ResolveGridPositionAgainstOppositePosition(
        grid_container_style, start_line, final_position, final_side,
        auto_repeat_tracks_count, subgrid_span_size);
  }

  int start_line = ResolveGridPositionFromStyle(
      grid_container_style, initial_position, initial_side,
      auto_repeat_tracks_count, is_parent_grid_container, subgrid_span_size);
  int end_line = ResolveGridPositionFromStyle(
      grid_container_style, final_position, final_side,
      auto_repeat_tracks_count, is_parent_grid_container, subgrid_span_size);

  if (end_line < start_line)
    std::swap(end_line, start_line);
  else if (end_line == start_line)
    end_line = start_line + 1;

  return GridSpan::UntranslatedDefiniteGridSpan(start_line, end_line);
}

}  // namespace blink
