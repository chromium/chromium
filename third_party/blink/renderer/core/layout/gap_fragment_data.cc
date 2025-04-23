// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/gap_fragment_data.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

WTF::String GapIntersection::ToString(bool verbose) const {
  WTF::String str = WTF::String("(") + WTF::String(inline_offset.ToString()) +
                    WTF::String(", ") + WTF::String(block_offset.ToString());

  if (verbose) {
    str = str + WTF::String(" - is_blocked_before: ") +
          (is_blocked_before ? "true" : "false") +
          WTF::String(" - is_blocked_after: ") +
          (is_blocked_after ? "true" : "false") +
          WTF::String(" - is_at_edge_of_container: ") +
          (is_at_edge_of_container ? "true" : "false");
  }

  str = str + WTF::String(")");
  return str;
}

void GapGeometry::SetGapIntersections(
    GridTrackSizingDirection track_direction,
    Vector<GapIntersectionList>&& intersection_list) {
  track_direction == kForColumns ? column_intersections_ = intersection_list
                                 : row_intersections_ = intersection_list;
}

void GapGeometry::MarkGapIntersectionBlocked(
    GridTrackSizingDirection track_direction,
    BlockedGapDirection blocked_direction,
    wtf_size_t main_index,
    wtf_size_t inner_index) {
  auto& intersections = track_direction == kForColumns ? column_intersections_
                                                       : row_intersections_;

  blocked_direction == BlockedGapDirection::kBefore
      ? intersections[main_index][inner_index].is_blocked_before = true
      : intersections[main_index][inner_index].is_blocked_after = true;
}

const Vector<GapIntersectionList>& GapGeometry::GetGapIntersections(
    GridTrackSizingDirection track_direction) const {
  return track_direction == kForColumns ? column_intersections_
                                        : row_intersections_;
}

WTF::String GapGeometry::IntersectionsToString(
    GridTrackSizingDirection track_direction,
    bool verbose) const {
  const Vector<GapIntersectionList>* intersections =
      track_direction == kForColumns ? &column_intersections_
                                     : &row_intersections_;
  StringBuilder result;
  for (auto& intersection_list : *intersections) {
    result.Append("[");
    for (auto& intersection : intersection_list) {
      result.Append(intersection.ToString(verbose));
      result.Append(", ");
    }
    result.Append("]");
    result.Append("\n");
  }
  return result.ToString();
}

}  // namespace blink
