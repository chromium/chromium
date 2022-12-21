// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/grid_positions_resolver.h"

#include <algorithm>
#include "third_party/blink/renderer/core/style/computed_style.h"
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
    GridTrackSizingDirection track_direction,
    wtf_size_t last_line,
    wtf_size_t auto_repeat_tracks_count,
    bool is_subgridded_to_parent)
    : last_line_(last_line),
      auto_repeat_total_tracks_(auto_repeat_tracks_count) {
  const bool is_for_columns = track_direction == kForColumns;
  const ComputedGridTrackList& computed_grid_track_list =
      is_for_columns ? grid_container_style.GridTemplateColumns()
                     : grid_container_style.GridTemplateRows();
  is_standalone_grid_ =
      computed_grid_track_list.axis_type == GridAxisType::kStandaloneAxis;

  // Line names from the container style are valid when the grid axis type is a
  // standalone grid or the axis is a subgrid and the parent is a grid. See:
  // https://www.w3.org/TR/css-grid-2/#subgrid-listing
  bool are_named_lines_valid = true;
  if (RuntimeEnabledFeatures::LayoutNGSubgridEnabled()) {
    are_named_lines_valid = is_subgridded_to_parent || is_standalone_grid_;
  }

  const NamedGridLinesMap& grid_line_names =
      computed_grid_track_list.named_grid_lines;
  const NamedGridLinesMap& auto_repeat_grid_line_names =
      computed_grid_track_list.auto_repeat_named_grid_lines;
  const NamedGridLinesMap& implicit_grid_line_names =
      is_for_columns ? grid_container_style.ImplicitNamedGridColumnLines()
                     : grid_container_style.ImplicitNamedGridRowLines();

  if (!grid_line_names.empty() && are_named_lines_valid) {
    auto it = grid_line_names.find(named_line);
    named_lines_indexes_ = it == grid_line_names.end() ? nullptr : &it->value;
  }

  if (!auto_repeat_grid_line_names.empty() && are_named_lines_valid) {
    auto it = auto_repeat_grid_line_names.find(named_line);
    auto_repeat_named_lines_indexes_ =
        it == auto_repeat_grid_line_names.end() ? nullptr : &it->value;
  }

  if (!implicit_grid_line_names.empty()) {
    auto it = implicit_grid_line_names.find(named_line);
    implicit_named_lines_indexes_ =
        it == implicit_grid_line_names.end() ? nullptr : &it->value;
  }

  insertion_point_ = computed_grid_track_list.auto_repeat_insertion_point;
  auto_repeat_track_list_length_ =
      computed_grid_track_list.auto_repeat_track_sizes.size();
}

bool NamedLineCollection::HasExplicitNamedLines() {
  return named_lines_indexes_ || auto_repeat_named_lines_indexes_;
}

bool NamedLineCollection::HasNamedLines() {
  return HasExplicitNamedLines() || implicit_named_lines_indexes_;
}

bool NamedLineCollection::Contains(wtf_size_t line) {
  CHECK(HasNamedLines());

  if (line > last_line_) {
    return false;
  }

  auto find = [](const Vector<wtf_size_t>* indexes, wtf_size_t line) {
    return indexes && indexes->Find(line) != kNotFound;
  };

  if (find(implicit_named_lines_indexes_, line)) {
    return true;
  }

  if (auto_repeat_track_list_length_ == 0 || line < insertion_point_) {
    return find(named_lines_indexes_, line);
  }

  DCHECK(auto_repeat_total_tracks_);

  if (line > insertion_point_ + auto_repeat_total_tracks_) {
    return find(named_lines_indexes_, line - (auto_repeat_total_tracks_ - 1));
  }

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
      find(auto_repeat_named_lines_indexes_, auto_repeat_track_list_length_)) {
    return true;
  }
  return find(auto_repeat_named_lines_indexes_,
              auto_repeat_index_in_first_repetition);
}

wtf_size_t NamedLineCollection::FirstExplicitPosition() {
  DCHECK(HasExplicitNamedLines());

  wtf_size_t first_line = 0;

  // If it is an standalone grid and there is no auto repeat(), there must be
  // some named line outside, return the 1st one. Also return it if it precedes
  // the auto-repeat().
  if ((is_standalone_grid_ && auto_repeat_track_list_length_ == 0) ||
      (named_lines_indexes_ &&
       named_lines_indexes_->at(first_line) <= insertion_point_)) {
    return named_lines_indexes_->at(first_line);
  }

  // Return the 1st named line inside the auto repeat(), if any.
  if (auto_repeat_named_lines_indexes_) {
    return auto_repeat_named_lines_indexes_->at(first_line) + insertion_point_;
  }

  // The 1st named line must be after the auto repeat().
  return named_lines_indexes_->at(first_line) + auto_repeat_total_tracks_ - 1;
}

wtf_size_t NamedLineCollection::FirstPosition() {
  CHECK(HasNamedLines());

  if (!implicit_named_lines_indexes_) {
    return FirstExplicitPosition();
  }

  wtf_size_t first_line = 0;
  if (!HasExplicitNamedLines()) {
    return implicit_named_lines_indexes_->at(first_line);
  }

  return std::min(FirstExplicitPosition(),
                  implicit_named_lines_indexes_->at(first_line));
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
  if (initial_position.IsSpan() && final_position.IsSpan()) {
    final_position.SetAutoPosition();
  }

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
    if (end > grid_last_line || lines_collection.Contains(end)) {
      number_of_lines--;
    }
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
    if (start < 0 || lines_collection.Contains(start)) {
      number_of_lines--;
    }
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
    wtf_size_t auto_repeat_tracks_count,
    wtf_size_t subgrid_span_size) {
  if (subgrid_span_size != kNotFound) {
    return subgrid_span_size;
  }

  const auto& track_list =
      grid_container_style.GridTemplateColumns().track_sizes;
  const wtf_size_t total_track_count = track_list.LegacyTrackList().size();

  return std::min<wtf_size_t>(
      std::max(total_track_count + auto_repeat_tracks_count,
               grid_container_style.NamedGridAreaColumnCount()),
      kGridMaxTracks);
}

wtf_size_t GridPositionsResolver::ExplicitGridRowCount(
    const ComputedStyle& grid_container_style,
    wtf_size_t auto_repeat_tracks_count,
    wtf_size_t subgrid_span_size) {
  if (subgrid_span_size != kNotFound) {
    return subgrid_span_size;
  }

  const auto& track_list = grid_container_style.GridTemplateRows().track_sizes;
  const wtf_size_t total_track_count = track_list.LegacyTrackList().size();

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
             ? GridPositionsResolver::ExplicitGridColumnCount(
                   grid_container_style, auto_repeat_tracks_count,
                   subgrid_span_size)
             : GridPositionsResolver::ExplicitGridRowCount(
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

  if (initial_position.IsAuto() && final_position.IsAuto()) {
    return 1;
  }

  const GridPosition& span_position =
      initial_position.IsSpan() ? initial_position : final_position;
  DCHECK(span_position.IsSpan() && span_position.SpanPosition());
  return span_position.SpanPosition();
}

wtf_size_t GridPositionsResolver::SpanSizeForAutoPlacedItem(
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
  NamedLineCollection lines_collection(
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
    bool is_subgridded_to_parent,
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
      if (position.IsPositive()) {
        return position.IntegerPosition() - 1;
      }

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
      NamedLineCollection implicit_lines(
          grid_container_style,
          ImplicitNamedGridLineForSide(named_grid_line, side),
          DirectionFromSide(side), last_line, auto_repeat_tracks_count);
      if (implicit_lines.HasNamedLines()) {
        return implicit_lines.FirstPosition();
      }

      // Otherwise, if there is a named line with the specified name,
      // contributes the first such line to the grid item's placement.
      NamedLineCollection explicit_lines(
          grid_container_style, named_grid_line, DirectionFromSide(side),
          last_line, auto_repeat_tracks_count, is_subgridded_to_parent);
      if (explicit_lines.HasNamedLines()) {
        return explicit_lines.FirstPosition();
      }

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
    GridTrackSizingDirection track_direction,
    wtf_size_t auto_repeat_tracks_count,
    bool is_subgridded_to_parent,
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
        auto_repeat_tracks_count, is_subgridded_to_parent, subgrid_span_size);
    return ResolveGridPositionAgainstOppositePosition(
        grid_container_style, end_line, initial_position, initial_side,
        auto_repeat_tracks_count, subgrid_span_size);
  }

  if (final_should_be_resolved_against_opposite_position) {
    // Infer our position from the initial_position position ('1 / auto' or '3 /
    // span 2' case).
    int start_line = ResolveGridPositionFromStyle(
        grid_container_style, initial_position, initial_side,
        auto_repeat_tracks_count, is_subgridded_to_parent, subgrid_span_size);
    return ResolveGridPositionAgainstOppositePosition(
        grid_container_style, start_line, final_position, final_side,
        auto_repeat_tracks_count, subgrid_span_size);
  }

  int start_line = ResolveGridPositionFromStyle(
      grid_container_style, initial_position, initial_side,
      auto_repeat_tracks_count, is_subgridded_to_parent, subgrid_span_size);
  int end_line = ResolveGridPositionFromStyle(
      grid_container_style, final_position, final_side,
      auto_repeat_tracks_count, is_subgridded_to_parent, subgrid_span_size);

  if (end_line < start_line) {
    std::swap(end_line, start_line);
  } else if (end_line == start_line) {
    end_line = start_line + 1;
  }

  return GridSpan::UntranslatedDefiniteGridSpan(start_line, end_line);
}

}  // namespace blink
