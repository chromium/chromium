// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_named_line_collection.h"

#include <algorithm>
#include "third_party/blink/renderer/core/style/computed_grid_track_list.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/grid_area.h"
#include "third_party/blink/renderer/core/style/grid_positions_resolver.h"

namespace blink {

NGGridNamedLineCollection::NGGridNamedLineCollection(
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
  bool are_named_lines_valid = true;
  if (RuntimeEnabledFeatures::LayoutNGSubgridEnabled())
    are_named_lines_valid = is_subgridded_to_parent || is_standalone_grid_;

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
      computed_grid_track_list.TrackList().AutoRepeatTrackCount();
}

bool NGGridNamedLineCollection::HasExplicitNamedLines() {
  return named_lines_indexes_ || auto_repeat_named_lines_indexes_;
}

bool NGGridNamedLineCollection::HasNamedLines() {
  return HasExplicitNamedLines() || implicit_named_lines_indexes_;
}

bool NGGridNamedLineCollection::Contains(wtf_size_t line) {
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
      find(auto_repeat_named_lines_indexes_, auto_repeat_track_list_length_)) {
    return true;
  }
  return find(auto_repeat_named_lines_indexes_,
              auto_repeat_index_in_first_repetition);
}

wtf_size_t NGGridNamedLineCollection::FirstExplicitPosition() {
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

wtf_size_t NGGridNamedLineCollection::FirstPosition() {
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
