// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid/grid_named_line_collection.h"

#include <algorithm>
#include "third_party/blink/renderer/core/style/computed_grid_track_list.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/grid_area.h"

namespace blink {

GridNamedLineCollection::GridNamedLineCollection(
    const String& named_line,
    GridTrackSizingDirection track_direction,
    const NamedGridLinesMap& implicit_grid_line_names,
    const NamedGridLinesMap& explicit_grid_line_names,
    const ComputedGridTrackList& computed_grid_track_list,
    wtf_size_t last_line,
    wtf_size_t auto_repeat_tracks_count,
    bool is_subgridded_to_parent)
    : last_line_(last_line),
      auto_repeat_total_tracks_(auto_repeat_tracks_count) {
  is_standalone_grid_ =
      computed_grid_track_list.axis_type == GridAxisType::kStandaloneAxis;

  // Line names from the container style are valid when the grid axis type is a
  // standalone grid or the axis is a subgrid and the parent is a grid. See:
  // https://www.w3.org/TR/css-grid-2/#subgrid-listing
  bool are_named_lines_valid = is_subgridded_to_parent || is_standalone_grid_;

  const NamedGridLinesMap& auto_repeat_grid_line_names =
      computed_grid_track_list.auto_repeat_named_grid_lines;

  if (!explicit_grid_line_names.empty() && are_named_lines_valid) {
    auto it = explicit_grid_line_names.find(named_line);
    named_lines_indexes_ =
        (it == explicit_grid_line_names.end()) ? nullptr : &it->value;
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
      computed_grid_track_list.track_list.AutoRepeatTrackCount();

  // For standalone grids, auto repeaters guarantee a minimum of one repeat,
  // but subgrids have a minimum of zero repeats. This can present issues, as
  // various parts of the code expect each track specified to produce at least
  // one grid track. To work around this, indices are incremented after a
  // collapsed track by one in `Contains`. Keep `last_line_` in sync with this
  // behavior.
  if (HasCollapsedAutoRepeat()) {
    DCHECK(!is_standalone_grid_);
    ++last_line_;
  }
}

bool GridNamedLineCollection::HasExplicitNamedLines() const {
  return named_lines_indexes_ || auto_repeat_named_lines_indexes_;
}

bool GridNamedLineCollection::HasCollapsedAutoRepeat() const {
  // Collapsed repeaters are only possible for subgrids, as standalone grids
  // guarantee a minimum of one repeat for auto repeaters.
  if (is_standalone_grid_) {
    return false;
  }

  // A collapsed auto repeater occurs when the author specifies auto repeat
  // tracks, but they were collapsed to zero repeats.
  return auto_repeat_track_list_length_ && !auto_repeat_total_tracks_;
}

bool GridNamedLineCollection::HasNamedLines() const {
  return HasExplicitNamedLines() || implicit_named_lines_indexes_;
}

bool GridNamedLineCollection::Contains(wtf_size_t line) const {
  CHECK(HasNamedLines());

  if (line > last_line_)
    return false;

  // If there's a collapsed auto repeater, the subsequent track indices will be
  // one index too high, so we can account for that after the fact by
  // incrementing `line` by one if it's at or after the insertion point.
  // Collapsed auto repeaters are only possible for subgrids, as standalone
  // grids guarantee a minimum of one repeat. The following methods expect each
  // line name to consume at least one track:
  //    `GridLineResolver::LookAheadForNamedGridLine`
  //    `GridLineResolver::LookBackForNamedGridLine`
  const bool has_collapsed_auto_repeat = HasCollapsedAutoRepeat();
  if (has_collapsed_auto_repeat && line >= insertion_point_) {
    DCHECK(!is_standalone_grid_);
    ++line;

    // The constructor should have updated `last_line_` in anticipation of this
    // scenario.
    DCHECK_LE(line, last_line_);
  }

  auto find = [](const Vector<wtf_size_t>* indexes, wtf_size_t line) {
    return indexes && indexes->Find(line) != kNotFound;
  };

  // First search implicit indices, as they have the highest precedence.
  if (find(implicit_named_lines_indexes_, line))
    return true;

  // This is the standard path for non-auto repeaters. We can also always go
  // down this path and skip auto-repeat logic if the auto repeat track list
  // length is 0 (possible for both standalone grids and subgrids), or if it has
  // a collapsed auto repeat (only possible for subgrids).
  if (auto_repeat_track_list_length_ == 0 || has_collapsed_auto_repeat ||
      line < insertion_point_) {
    return find(named_lines_indexes_, line);
  }

  // Search named lines after auto repetitions.
  if (line > insertion_point_ + auto_repeat_total_tracks_) {
    return find(named_lines_indexes_, line - (auto_repeat_total_tracks_ - 1));
  }

  // Subgrids are allowed to have an auto repeat count of zero.
  if (auto_repeat_total_tracks_ == 0) {
    DCHECK(!is_standalone_grid_);
    return false;
  }

  // Search the line name at the insertion point. This line and any of the
  // subsequent lines are of equal precedence and won't overlap, so it's safe
  // to do them in any order.
  if (line == insertion_point_) {
    return find(named_lines_indexes_, line) ||
           find(auto_repeat_named_lines_indexes_, 0);
  }

  // Search the final auto repetition line name.
  if (line == insertion_point_ + auto_repeat_total_tracks_) {
    return find(auto_repeat_named_lines_indexes_,
                auto_repeat_track_list_length_) ||
           find(named_lines_indexes_, insertion_point_ + 1);
  }

  // Search repeated line names.
  wtf_size_t auto_repeat_index_in_first_repetition =
      (line - insertion_point_) % auto_repeat_track_list_length_;
  if (!auto_repeat_index_in_first_repetition &&
      find(auto_repeat_named_lines_indexes_, auto_repeat_track_list_length_)) {
    return true;
  }
  return find(auto_repeat_named_lines_indexes_,
              auto_repeat_index_in_first_repetition);
}

wtf_size_t GridNamedLineCollection::FirstExplicitPosition() const {
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
  if (auto_repeat_named_lines_indexes_)
    return auto_repeat_named_lines_indexes_->at(first_line) + insertion_point_;

  // The 1st named line must be after the auto repeat().
  // TODO(kschmi) Remove this offset when `auto_repeat_total_tracks_` is
  // correct for subgrids.
  const wtf_size_t auto_repeat_counted_tracks =
      auto_repeat_total_tracks_ ? auto_repeat_total_tracks_ - 1 : 0;
  return named_lines_indexes_->at(first_line) + auto_repeat_counted_tracks;
}

wtf_size_t GridNamedLineCollection::FirstPosition() const {
  CHECK(HasNamedLines());

  if (!implicit_named_lines_indexes_)
    return FirstExplicitPosition();

  wtf_size_t first_line = 0;
  if (!HasExplicitNamedLines())
    return implicit_named_lines_indexes_->at(first_line);

  return std::min(FirstExplicitPosition(),
                  implicit_named_lines_indexes_->at(first_line));
}

}  // namespace blink
