// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_line_resolver.h"

#include <algorithm>
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_data.h"
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

NGGridLineResolver::NGGridLineResolver(
    const ComputedStyle& grid_style,
    const NGGridLineResolver& parent_line_resolver,
    GridArea subgrid_area)
    : style_(&grid_style),
      subgridded_columns_merged_explicit_grid_line_names_(
          grid_style.GridTemplateColumns().named_grid_lines),
      subgridded_rows_merged_explicit_grid_line_names_(
          grid_style.GridTemplateRows().named_grid_lines),
      subgridded_columns_merged_implicit_grid_line_names_(
          grid_style.ImplicitNamedGridColumnLines()),
      subgridded_rows_merged_implicit_grid_line_names_(
          grid_style.ImplicitNamedGridRowLines()) {
  if (subgrid_area.columns.IsTranslatedDefinite())
    subgridded_column_span_size_ = subgrid_area.SpanSize(kForColumns);
  if (subgrid_area.rows.IsTranslatedDefinite())
    subgridded_row_span_size_ = subgrid_area.SpanSize(kForRows);

  auto MergeNamedGridLinesWithParent =
      [](NamedGridLinesMap& subgrid_map, const NamedGridLinesMap& parent_map,
         GridSpan subgrid_span, bool is_opposite_direction_to_parent) -> void {
    // Update `subgrid_map` to a merged map from a parent grid or subgrid map
    // (`parent_map`). The map is a key-value store with keys as the line name
    // and the value as an array of ascending indices.
    for (auto& pair : parent_map) {
      // TODO(kschmi) : Benchmark whether this is faster with an std::map, which
      // would eliminate the need for sorting and removing duplicates below.
      // Perf will vary based on the number of named lines defined.
      Vector<wtf_size_t, 16> merged_list;
      for (const auto& position : pair.value) {
        // Filter out parent named lines that are out of the subgrid range. Also
        // offset entries by `subgrid_start_line` before inserting them into the
        // merged map so they are all relative to offset 0. These are already in
        // ascending order so there's no need to sort.
        if (subgrid_span.Contains(position)) {
          if (is_opposite_direction_to_parent) {
            merged_list.push_back(subgrid_span.EndLine() - position);
          } else {
            merged_list.push_back(position - subgrid_span.StartLine());
          }
        }
      }

      // If there's a name collision, merge the values and sort. These are from
      // the subgrid and not the parent container, so they are already relative
      // to index 0 and don't need to be offset.
      const auto& existing_entry = subgrid_map.find(pair.key);
      if (existing_entry != subgrid_map.end()) {
        for (auto& value : existing_entry->value)
          merged_list.push_back(value);

        std::sort(merged_list.begin(), merged_list.end());

        // Remove any duplicated entries in the sorted list. Duplicates can
        // occur when the parent grid and the subgrid define line names with the
        // same name at the same index. It doesn't matter which one takes
        // precedence (grid vs subgrid), as long as there is only a single entry
        // per index. Without this call, there will be bugs when we need to
        // iterate over the nth entry with a given name (e.g. "a 5") - the
        // duplicate will make all entries past it off-by-one.
        // `std::unique` doesn't change the size of the vector (it just moves
        // duplicates to the end), so we need to erase the duplicates via the
        // iterator returned.
        merged_list.erase(std::unique(merged_list.begin(), merged_list.end()),
                          merged_list.end());
      }

      // Override the existing subgrid's line names map with the new merged list
      // for this particular line name entry. If `merged_list` list is empty,
      // (this can happen when all entries for a particular line name are out
      // of the subgrid range), erase the entry entirely, as
      // `NGGridNamedLineCollection` doesn't support named line entries without
      // values.
      if (merged_list.empty())
        subgrid_map.erase(pair.key);
      else
        subgrid_map.Set(pair.key, merged_list);
    }
  };
  auto MergeImplicitLinesWithParent = [&](NamedGridLinesMap& subgrid_map,
                                          const NamedGridLinesMap& parent_map,
                                          GridSpan subgrid_span) -> void {
    const wtf_size_t subgrid_span_size = subgrid_span.IntegerSpan();
    // First, clamp the existing `subgrid_map` to the subgrid range before
    // merging. These are positive and relative to index 0, so we only need to
    // clamp values above `subgrid_span_size`.
    for (auto& pair : subgrid_map) {
      WTF::Vector<wtf_size_t> clamped_list;
      for (const auto& position : pair.value) {
        if (position > subgrid_span_size)
          clamped_list.push_back(subgrid_span_size);
        else
          clamped_list.push_back(position);
      }
      subgrid_map.Set(pair.key, clamped_list);
    }

    // Update `subgrid_map` to a merged map from a parent grid or subgrid map
    // (`parent_map`). The map is a key-value store with keys as the implicit
    // line name and the value as an array of ascending indices.
    for (auto& pair : parent_map) {
      WTF::Vector<wtf_size_t> merged_list;
      for (const auto& position : pair.value) {
        auto IsGridAreaInSubgridRange = [&]() -> bool {
          // Returns true if a given position is within either the implicit
          // -start or -end line (or both) to comply with this part of the spec:
          //
          // "Note: If a named grid area only partially overlaps the subgrid,
          // its implicitly-assigned line names will be assigned to the first
          // and/or last line of the subgrid such that a named grid area exists
          // representing that partially overlapped area of the subgrid".
          // https://www.w3.org/TR/css-grid-2/#subgrid-area-inheritance
          //
          // TODO(kschmi): Performance can be optimized here by storing
          // additional data on the style object for implicit lines that
          // correlate matched implicit -start/-end pairs. Another
          // option is to sort and iterate through adjacent -start/-end lines.
          auto IsPositionWithinGridArea =
              [&](const String& initial_suffix,
                  const String& opposing_suffix) -> bool {
            if (subgrid_span.Contains(position))
              return true;
            if (pair.key.EndsWith(initial_suffix)) {
              // If the initial suffix is not in range, return true if the
              // implicit line with the opposing suffix is within range.
              auto line_name_without_initial_suffix = pair.key.Substring(
                  0, pair.key.length() - initial_suffix.length());
              const auto opposite_line_name =
                  line_name_without_initial_suffix + opposing_suffix;

              const auto& opposite_line_entry =
                  parent_map.find(opposite_line_name);
              if (opposite_line_entry != parent_map.end()) {
                for (auto& opposite_position : opposite_line_entry->value) {
                  if (subgrid_span.Contains(opposite_position))
                    return true;
                }
              }
            }
            return false;
          };
          const String start_suffix("-start");
          const String end_suffix("-end");
          return IsPositionWithinGridArea(start_suffix, end_suffix) ||
                 IsPositionWithinGridArea(end_suffix, start_suffix);
        };

        // Implicit entries within the subgrid span can get inserted directly
        // (minus the subgrid start position, because they are relative to
        // the parent grid). For partially overlapping entries, snap to
        // either 0 or `subgrid_span_size`.
        const wtf_size_t subgrid_start_line = subgrid_span.StartLine();
        if (subgrid_span.Contains(position)) {
          merged_list.push_back(position - subgrid_span.StartLine());
        } else if (IsGridAreaInSubgridRange()) {
          // Clamp the parent's start/end positions if a parent grid-area
          // partially overlaps the subgrid.
          if (position < subgrid_start_line)
            merged_list.push_back(0);
          else if (position > (subgrid_start_line + subgrid_span_size))
            merged_list.push_back(subgrid_span_size);
        }

        // If there's a name collision, merge the values and sort. These are
        // from the subgrid and not the parent, so they are already relative to
        // index 0 and don't need to be offset.
        const auto& existing_entry = subgrid_map.find(pair.key);
        if (existing_entry != subgrid_map.end()) {
          for (auto& value : existing_entry->value)
            merged_list.push_back(value);
          std::sort(merged_list.begin(), merged_list.end());
        }

        // Override the existing subgrid's line names map with the new merged
        // list for this particular line name entry. If `merged_list` list is
        // empty, (this can happen when all entries for a particular line name
        // are out of the subgrid range), erase the entry entirely, as
        // `NGGridNamedLineCollection` doesn't support named line entries
        // without values.
        if (merged_list.empty())
          subgrid_map.erase(pair.key);
        else
          subgrid_map.Set(pair.key, merged_list);
      }
    }
  };

  // TODO(kschmi) - Account for orthogonal writing modes and swap rows/columns.
  const bool is_opposite_direction_to_parent =
      (grid_style.Direction() != parent_line_resolver.style_->Direction());

  if (subgrid_area.columns.IsTranslatedDefinite()) {
    MergeNamedGridLinesWithParent(
        *subgridded_columns_merged_explicit_grid_line_names_,
        parent_line_resolver.ExplicitNamedLinesMap(kForColumns),
        subgrid_area.columns, is_opposite_direction_to_parent);
    MergeImplicitLinesWithParent(
        *subgridded_columns_merged_implicit_grid_line_names_,
        parent_line_resolver.ImplicitNamedLinesMap(kForColumns),
        subgrid_area.columns);
  }
  if (subgrid_area.rows.IsTranslatedDefinite()) {
    MergeNamedGridLinesWithParent(
        *subgridded_rows_merged_explicit_grid_line_names_,
        parent_line_resolver.ExplicitNamedLinesMap(kForRows), subgrid_area.rows,
        is_opposite_direction_to_parent);
    MergeImplicitLinesWithParent(
        *subgridded_rows_merged_implicit_grid_line_names_,
        parent_line_resolver.ImplicitNamedLinesMap(kForRows),
        subgrid_area.rows);
  }
}

bool NGGridLineResolver::operator==(const NGGridLineResolver& other) const {
  // This should only compare input data for placement. |style_| isn't
  // applicable since we shouldn't compare line resolvers of different nodes,
  // and the named line maps are a product of the computed style and the inputs.
  return column_auto_repetitions_ == other.column_auto_repetitions_ &&
         row_auto_repetitions_ == other.row_auto_repetitions_ &&
         subgridded_column_span_size_ == other.subgridded_column_span_size_ &&
         subgridded_row_span_size_ == other.subgridded_row_span_size_;
}

void NGGridLineResolver::InitialAndFinalPositionsFromStyle(
    const ComputedStyle& grid_item_style,
    GridTrackSizingDirection track_direction,
    GridPosition& initial_position,
    GridPosition& final_position) const {
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

wtf_size_t NGGridLineResolver::LookAheadForNamedGridLine(
    int start,
    wtf_size_t number_of_lines,
    wtf_size_t grid_last_line,
    NGGridNamedLineCollection& lines_collection) const {
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

int NGGridLineResolver::LookBackForNamedGridLine(
    int end,
    wtf_size_t number_of_lines,
    int grid_last_line,
    NGGridNamedLineCollection& lines_collection) const {
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

GridSpan NGGridLineResolver::DefiniteGridSpanWithNamedSpanAgainstOpposite(
    int opposite_line,
    const GridPosition& position,
    GridPositionSide side,
    int last_line,
    NGGridNamedLineCollection& lines_collection) const {
  int start, end;
  const int span_position = position.SpanPosition();
  if (side == kRowStartSide || side == kColumnStartSide) {
    start = LookBackForNamedGridLine(opposite_line - 1, span_position,
                                     last_line, lines_collection);
    end = opposite_line;
  } else {
    start = opposite_line;
    end = LookAheadForNamedGridLine(opposite_line + 1, span_position, last_line,
                                    lines_collection);
  }

  return GridSpan::UntranslatedDefiniteGridSpan(start, end);
}

bool NGGridLineResolver::IsSubgridded(
    GridTrackSizingDirection track_direction) const {
  // The merged explicit line names only exist when a direction is subgridded.
  const auto& merged_explicit_grid_line_names =
      (track_direction == kForColumns)
          ? subgridded_columns_merged_explicit_grid_line_names_
          : subgridded_rows_merged_explicit_grid_line_names_;

  return merged_explicit_grid_line_names.has_value();
}

wtf_size_t NGGridLineResolver::ExplicitGridColumnCount() const {
  if (subgridded_column_span_size_ != kNotFound)
    return subgridded_column_span_size_;

  return std::min<wtf_size_t>(std::max(style_->GridTemplateColumns()
                                               .track_sizes.NGTrackList()
                                               .TrackCountWithoutAutoRepeat() +
                                           AutoRepeatTrackCount(kForColumns),
                                       style_->NamedGridAreaColumnCount()),
                              kGridMaxTracks);
}

wtf_size_t NGGridLineResolver::ExplicitGridRowCount() const {
  if (subgridded_row_span_size_ != kNotFound)
    return subgridded_row_span_size_;

  return std::min<wtf_size_t>(std::max(style_->GridTemplateRows()
                                               .track_sizes.NGTrackList()
                                               .TrackCountWithoutAutoRepeat() +
                                           AutoRepeatTrackCount(kForRows),
                                       style_->NamedGridAreaRowCount()),
                              kGridMaxTracks);
}

wtf_size_t NGGridLineResolver::ExplicitGridTrackCount(
    GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns) ? ExplicitGridColumnCount()
                                          : ExplicitGridRowCount();
}

wtf_size_t NGGridLineResolver::AutoRepetitions(
    GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns) ? column_auto_repetitions_
                                          : row_auto_repetitions_;
}

wtf_size_t NGGridLineResolver::AutoRepeatTrackCount(
    GridTrackSizingDirection track_direction) const {
  return AutoRepetitions(track_direction) *
         ComputedGridTrackList(track_direction)
             .TrackList()
             .AutoRepeatTrackCount();
}

wtf_size_t NGGridLineResolver::SubgridSpanSize(
    GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns) ? subgridded_column_span_size_
                                          : subgridded_row_span_size_;
}

bool NGGridLineResolver::HasStandaloneAxis(
    GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns)
             ? subgridded_column_span_size_ == kNotFound
             : subgridded_row_span_size_ == kNotFound;
}

wtf_size_t NGGridLineResolver::ExplicitGridSizeForSide(
    GridPositionSide side) const {
  return (side == kColumnStartSide || side == kColumnEndSide)
             ? ExplicitGridColumnCount()
             : ExplicitGridRowCount();
}

GridSpan
NGGridLineResolver::ResolveNamedGridLinePositionAgainstOppositePosition(
    int opposite_line,
    const GridPosition& position,
    GridPositionSide side) const {
  DCHECK(position.IsSpan());
  DCHECK(!position.NamedGridLine().IsNull());
  // Negative positions are not allowed per the specification and should have
  // been handled during parsing.
  DCHECK_GT(position.SpanPosition(), 0);

  GridTrackSizingDirection track_direction = DirectionFromSide(side);
  const auto& implicit_grid_line_names = ImplicitNamedLinesMap(track_direction);
  const auto& explicit_grid_line_names = ExplicitNamedLinesMap(track_direction);
  const auto& computed_grid_track_list = ComputedGridTrackList(track_direction);
  const auto& auto_repeat_tracks_count = AutoRepeatTrackCount(track_direction);

  wtf_size_t last_line = ExplicitGridSizeForSide(side);

  NGGridNamedLineCollection lines_collection(
      position.NamedGridLine(), track_direction, implicit_grid_line_names,
      explicit_grid_line_names, computed_grid_track_list, last_line,
      auto_repeat_tracks_count, IsSubgridded(track_direction));
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

const NamedGridLinesMap& NGGridLineResolver::ImplicitNamedLinesMap(
    GridTrackSizingDirection track_direction) const {
  const auto& subgrid_merged_implicit_grid_line_names =
      (track_direction == kForColumns)
          ? subgridded_columns_merged_implicit_grid_line_names_
          : subgridded_rows_merged_implicit_grid_line_names_;

  const auto& implicit_lines_map_from_style =
      (track_direction == kForColumns) ? style_->ImplicitNamedGridColumnLines()
                                       : style_->ImplicitNamedGridRowLines();

  return subgrid_merged_implicit_grid_line_names
             ? *subgrid_merged_implicit_grid_line_names
             : implicit_lines_map_from_style;
}

const NamedGridLinesMap& NGGridLineResolver::ExplicitNamedLinesMap(
    GridTrackSizingDirection track_direction) const {
  const auto& subgrid_merged_grid_line_names =
      (track_direction == kForColumns)
          ? subgridded_columns_merged_explicit_grid_line_names_
          : subgridded_rows_merged_explicit_grid_line_names_;

  return subgrid_merged_grid_line_names
             ? *subgrid_merged_grid_line_names
             : ComputedGridTrackList(track_direction).named_grid_lines;
}

const blink::ComputedGridTrackList& NGGridLineResolver::ComputedGridTrackList(
    GridTrackSizingDirection track_direction) const {
  // TODO(kschmi): Refactor so this isn't necessary and handle auto-repeats
  // for subgrids.
  return (track_direction == kForColumns) ? style_->GridTemplateColumns()
                                          : style_->GridTemplateRows();
}

GridSpan NGGridLineResolver::ResolveGridPositionAgainstOppositePosition(
    int opposite_line,
    const GridPosition& position,
    GridPositionSide side) const {
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
    return ResolveNamedGridLinePositionAgainstOppositePosition(opposite_line,
                                                               position, side);
  }

  return DefiniteGridSpanWithSpanAgainstOpposite(opposite_line, position, side);
}

wtf_size_t NGGridLineResolver::SpanSizeFromPositions(
    const GridPosition& initial_position,
    const GridPosition& final_position) const {
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
    GridTrackSizingDirection track_direction) const {
  GridPosition initial_position, final_position;
  InitialAndFinalPositionsFromStyle(grid_item_style, track_direction,
                                    initial_position, final_position);
  return SpanSizeFromPositions(initial_position, final_position);
}

int NGGridLineResolver::ResolveNamedGridLinePosition(
    const GridPosition& position,
    GridPositionSide side) const {
  DCHECK(!position.NamedGridLine().IsNull());

  wtf_size_t last_line = ExplicitGridSizeForSide(side);
  GridTrackSizingDirection track_direction = DirectionFromSide(side);
  const auto& implicit_grid_line_names = ImplicitNamedLinesMap(track_direction);
  const auto& explicit_grid_line_names = ExplicitNamedLinesMap(track_direction);
  const auto& track_list = ComputedGridTrackList(track_direction);
  const auto& auto_repeat_tracks_count = AutoRepeatTrackCount(track_direction);

  NGGridNamedLineCollection lines_collection(
      position.NamedGridLine(), track_direction, implicit_grid_line_names,
      explicit_grid_line_names, track_list, last_line, auto_repeat_tracks_count,
      IsSubgridded(track_direction));

  if (position.IsPositive()) {
    return LookAheadForNamedGridLine(0, abs(position.IntegerPosition()),
                                     last_line, lines_collection);
  }

  return LookBackForNamedGridLine(last_line, abs(position.IntegerPosition()),
                                  last_line, lines_collection);
}

int NGGridLineResolver::ResolveGridPosition(const GridPosition& position,
                                            GridPositionSide side) const {
  auto track_direction = DirectionFromSide(side);
  const auto& auto_repeat_tracks_count = AutoRepeatTrackCount(track_direction);

  switch (position.GetType()) {
    case kExplicitPosition: {
      DCHECK(position.IntegerPosition());

      if (!position.NamedGridLine().IsNull()) {
        return ResolveNamedGridLinePosition(position, side);
      }

      // Handle <integer> explicit position.
      if (position.IsPositive())
        return position.IntegerPosition() - 1;

      wtf_size_t resolved_position = abs(position.IntegerPosition()) - 1;
      wtf_size_t end_of_track = ExplicitGridSizeForSide(side);

      return end_of_track - resolved_position;
    }
    case kNamedGridAreaPosition: {
      // First attempt to match the grid area's edge to a named grid area: if
      // there is a named line with the name ''<custom-ident>-start (for
      // grid-*-start) / <custom-ident>-end'' (for grid-*-end), contributes the
      // first such line to the grid item's placement.
      String named_grid_line = position.NamedGridLine();
      DCHECK(!position.NamedGridLine().IsNull());

      wtf_size_t last_line = ExplicitGridSizeForSide(side);

      const auto& implicit_grid_line_names =
          ImplicitNamedLinesMap(track_direction);
      const auto& explicit_grid_line_names =
          ExplicitNamedLinesMap(track_direction);
      const auto& track_list = ComputedGridTrackList(track_direction);

      NGGridNamedLineCollection implicit_lines(
          ImplicitNamedGridLineForSide(named_grid_line, side), track_direction,
          implicit_grid_line_names, explicit_grid_line_names, track_list,
          last_line, auto_repeat_tracks_count, IsSubgridded(track_direction));
      if (implicit_lines.HasNamedLines())
        return implicit_lines.FirstPosition();

      // Otherwise, if there is a named line with the specified name,
      // contributes the first such line to the grid item's placement.
      NGGridNamedLineCollection explicit_lines(
          named_grid_line, track_direction, implicit_grid_line_names,
          explicit_grid_line_names, track_list, last_line,
          auto_repeat_tracks_count, IsSubgridded(track_direction));
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
    const ComputedStyle& grid_item_style,
    GridTrackSizingDirection track_direction) const {
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
    int end_line = ResolveGridPosition(final_position, final_side);
    return ResolveGridPositionAgainstOppositePosition(
        end_line, initial_position, initial_side);
  }

  if (final_should_be_resolved_against_opposite_position) {
    // Infer our position from the initial_position position ('1 / auto' or '3 /
    // span 2' case).
    int start_line = ResolveGridPosition(initial_position, initial_side);
    return ResolveGridPositionAgainstOppositePosition(
        start_line, final_position, final_side);
  }

  int start_line = ResolveGridPosition(initial_position, initial_side);
  int end_line = ResolveGridPosition(final_position, final_side);

  if (end_line < start_line)
    std::swap(end_line, start_line);
  else if (end_line == start_line)
    end_line = start_line + 1;

  return GridSpan::UntranslatedDefiniteGridSpan(start_line, end_line);
}

}  // namespace blink
