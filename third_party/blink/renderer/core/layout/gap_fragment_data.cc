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

PhysicalRect GapGeometry::ComputeInkOverflowForGaps(
    WritingDirectionMode writing_direction,
    const PhysicalSize& container_size,
    LayoutUnit inline_thickness,
    LayoutUnit block_thickness) const {
  // One of the two intersection lists must be non-empty. If both are empty,
  // it means there are no gaps in the container, hence we wouldn't have a
  // gap geometry.
  CHECK(!row_intersections_.empty() || !column_intersections_.empty());

  LayoutUnit inline_start;
  LayoutUnit inline_size;
  LayoutUnit block_start;
  LayoutUnit block_size;

  // To determine the inline bounds, we'd typically use the rows intersections
  // but in the case where there are no row intersections (i.e. no row gaps) we
  // fallback to using the column intersections.
  if (row_intersections_.empty()) {
    inline_start = column_intersections_.front().front().inline_offset;
    inline_size = column_intersections_.back().back().inline_offset -
                  column_intersections_.front().front().inline_offset;
  } else {
    inline_start = row_intersections_.front().front().inline_offset;
    inline_size = row_intersections_.back().back().inline_offset -
                  row_intersections_.front().front().inline_offset;
  }

  // Similarly, to determine the block bounds, we'd typically use the columns
  // intersections but in the case where there are no column
  // intersections (i.e. no column gaps) we fallback to using the row
  // intersections.
  if (column_intersections_.empty()) {
    block_start = row_intersections_.front().front().block_offset;
    block_size = row_intersections_.back().back().block_offset -
                 row_intersections_.front().front().block_offset;
  } else {
    block_start = column_intersections_.front().front().block_offset;
    block_size = column_intersections_.back().back().block_offset -
                 column_intersections_.front().front().block_offset;
  }

  // Inflate the bounds to account for the gap decorations thickness.
  inline_start -= inline_thickness / 2;
  inline_size += inline_thickness;
  block_start -= block_thickness / 2;
  block_size += block_thickness;

  LogicalRect logical_rect(inline_size, block_start, inline_size, block_size);
  WritingModeConverter converter(writing_direction, container_size);
  PhysicalRect physical_rect = converter.ToPhysical(logical_rect);

  return physical_rect;
}

}  // namespace blink
