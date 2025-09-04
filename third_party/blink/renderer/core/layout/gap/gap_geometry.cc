// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"

#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder_stream.h"

namespace blink {

String GapIntersection::ToString(bool verbose) const {
  if (verbose) {
    return StrCat(
        {"(", inline_offset.ToString(), ", ", block_offset.ToString(),
         " - is_blocked_before: ", is_blocked_before ? "true" : "false",
         " - is_blocked_after: ", is_blocked_after ? "true" : "false",
         " - is_at_edge_of_container: ",
         is_at_edge_of_container ? "true" : "false", ")"});
  }
  return StrCat(
      {"(", inline_offset.ToString(), ", ", block_offset.ToString(), ")"});
}

void GapGeometry::SetGapIntersections(
    GridTrackSizingDirection track_direction,
    Vector<GapIntersectionList>&& intersection_list) {
  track_direction == kForColumns ? column_intersections_ = intersection_list
                                 : row_intersections_ = intersection_list;
}

const Vector<GapIntersectionList>& GapGeometry::GetGapIntersections(
    GridTrackSizingDirection track_direction) const {
  return track_direction == kForColumns ? column_intersections_
                                        : row_intersections_;
}

String GapGeometry::IntersectionsToString(
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
  return result.ReleaseString();
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

String GapGeometry::ToString(bool verbose) const {
  StringBuilder builder;
  builder << "MainGaps: [";
  for (const auto& main_gap : main_gaps_) {
    builder << main_gap.ToString(verbose) << ", ";
  }
  builder << "] ";
  builder << "CrossGaps: [";
  for (const auto& cross_gap : cross_gaps_) {
    builder << cross_gap.ToString(verbose) << ", ";
  }
  builder << "] ";
  return builder.ReleaseString();
}

void GapGeometry::SetContentInlineOffsets(LayoutUnit start_offset,
                                          LayoutUnit end_offset) {
  content_inline_start_ = start_offset;
  content_inline_end_ = end_offset;
}

void GapGeometry::SetContentBlockOffsets(LayoutUnit start_offset,
                                         LayoutUnit end_offset) {
  content_block_start_ = start_offset;
  content_block_end_ = end_offset;
}

LayoutUnit GapGeometry::GetGapOffset(GridTrackSizingDirection direction,
                                     wtf_size_t gap_index) const {
  if (IsMainDirection(direction)) {
    return GetMainGaps()[gap_index].GetGapOffset();
  } else {
    return direction == kForColumns
               ? GetCrossGaps()[gap_index].GetGapOffset().inline_offset
               : GetCrossGaps()[gap_index].GetGapOffset().block_offset;
  }
}

Vector<LayoutUnit> GapGeometry::GenerateIntersectionListForGap(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index) const {
  if (IsMainDirection(direction)) {
    return GenerateMainIntersectionList(direction, gap_index);
  }
  return GenerateCrossIntersectionList(direction, gap_index);
}

Vector<LayoutUnit> GapGeometry::GenerateMainIntersectionList(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index) const {
  Vector<LayoutUnit> intersections;
  LayoutUnit content_start =
      direction == kForColumns ? content_block_start_ : content_inline_start_;
  intersections.push_back(content_start);

  switch (GetContainerType()) {
    case ContainerType::kGrid: {
      // For grid containers:
      // - The main axis is rows, and cross gaps correspond to column gaps.
      // - Intersections occur at the inline offset of each column gap.
      CHECK_EQ(GetMainDirection(), kForRows);
      for (const auto& cross_gap : GetCrossGaps()) {
        intersections.push_back(cross_gap.GetGapOffset().inline_offset);
      }
      break;
    }
    case ContainerType::kFlex: {
      MainGap main_gap = GetMainGaps()[gap_index];
      // For a flex main gap:
      // - We need to include all cross gaps that intersect this main gap.
      // - Flex main gaps have two disjoint sets of cross gaps:
      // 1. Cross gaps that appear before the main gap
      // 2. Cross gaps that appear after the main gap
      // - We gather both sets and then sort them along the main axis to
      // maintain a monotonic order.
      //
      // See third_party/blink/renderer/core/layout/gap/README.md for more.
      CrossGaps cross_gaps;
      // TODO(samomekarajr): Can do merge two sorted lists here instead. This
      // will be be more efficient and avoid the extra copy loop below since we
      // would be merging directly into `intersections`.
      if (main_gap.HasCrossGapsBefore()) {
        for (wtf_size_t i = main_gap.GetCrossGapBeforeStart();
             i <= main_gap.GetCrossGapBeforeEnd(); ++i) {
          cross_gaps.push_back(GetCrossGaps()[i]);
        }
      }

      if (main_gap.HasCrossGapsAfter()) {
        for (wtf_size_t i = main_gap.GetCrossGapAfterStart();
             i <= main_gap.GetCrossGapAfterEnd(); ++i) {
          cross_gaps.push_back(GetCrossGaps()[i]);
        }
      }

      std::sort(cross_gaps.begin(), cross_gaps.end(),
                [direction](const CrossGap& a, const CrossGap& b) {
                  return direction == kForColumns
                             ? a.GetGapOffset().inline_offset <
                                   b.GetGapOffset().inline_offset
                             : a.GetGapOffset().block_offset <
                                   b.GetGapOffset().block_offset;
                });

      // Copy merged and sorted values into `intersections`.
      for (const auto& cross_gap : cross_gaps) {
        LogicalOffset cross_gap_start = cross_gap.GetGapOffset();
        LayoutUnit offset = direction == kForColumns
                                ? cross_gap_start.inline_offset
                                : cross_gap_start.block_offset;
        intersections.push_back(offset);
      }

      break;
    }
    case ContainerType::kMultiColumn:
      // TODO(samomekarajr): Implement MultiColumn case.
      break;
  }

  LayoutUnit content_end =
      direction == kForColumns ? content_block_end_ : content_inline_end_;
  intersections.push_back(content_end);

  return intersections;
}

Vector<LayoutUnit> GapGeometry::GenerateCrossIntersectionList(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index) const {
  Vector<LayoutUnit> intersections;
  switch (GetContainerType()) {
    case ContainerType::kGrid: {
      // For a grid cross gap:
      // - Intersections include:
      // 1. The content-start edge
      // 2. The start offset of every main gap
      // 3. The content-end edge
      // - This works because grid main and cross gaps are aligned.
      intersections.ReserveInitialCapacity(main_gaps_.size() + 2);
      LayoutUnit content_start = direction == kForColumns
                                     ? content_block_start_
                                     : content_inline_start_;

      intersections.push_back(content_start);

      for (const auto& main_gap : GetMainGaps()) {
        intersections.push_back(main_gap.GetGapOffset());
      }

      LayoutUnit content_end =
          direction == kForColumns ? content_block_end_ : content_inline_end_;

      intersections.push_back(content_end);
      break;
    }
    case ContainerType::kFlex: {
      // For a flex cross gap:
      // - There are exactly two intersections:
      // 1. The gap's start offset
      // 2. Its computed end offset (either a main gap or the container's
      // content-end edge)
      //
      // See third_party/blink/renderer/core/layout/gap/README.md for more.
      intersections.ReserveInitialCapacity(2);
      CrossGap cross_gap = GetCrossGaps()[gap_index];
      LayoutUnit offset = direction == kForColumns
                              ? cross_gap.GetGapOffset().block_offset
                              : cross_gap.GetGapOffset().inline_offset;
      intersections.push_back(offset);
      LayoutUnit end_offset_for_flex_cross_gap =
          ComputeEndOffsetForFlexCrossGap(gap_index, direction,
                                          cross_gap.EndsAtEdge());
      intersections.push_back(end_offset_for_flex_cross_gap);
      break;
    }
    case ContainerType::kMultiColumn:
      // TODO(samomekarajr): Implement MultiColumn case.
      break;
  }

  return intersections;
}

LayoutUnit GapGeometry::ComputeEndOffsetForFlexCrossGap(
    wtf_size_t cross_gap_index,
    GridTrackSizingDirection direction,
    bool cross_gap_is_at_end) const {
  if (main_gap_running_index_ == kNotFound || cross_gap_is_at_end) {
    // If the cross gap is an end-edge gap, its end offset is the container's
    // content end.
    return direction == kForRows ? content_inline_end_ : content_block_end_;
  }

  // Determine whether the current cross gap falls before the main gap
  // currently being tracked.
  const MainGaps& main_gaps = GetMainGaps();
  CHECK_LT(main_gap_running_index_, main_gaps.size());
  wtf_size_t last_cross_before_index =
      main_gaps[main_gap_running_index_].GetCrossGapBeforeEnd();

  // If the cross gap does not fall before the currently tracked main gap,
  // advance `main_gap_running_index_` to the next main gap.
  if (cross_gap_index > last_cross_before_index) {
    ++main_gap_running_index_;
  }

  CHECK_LT(main_gap_running_index_, main_gaps.size());
  return main_gaps[main_gap_running_index_].GetGapOffset();
}

bool GapGeometry::IsEdgeIntersection(wtf_size_t gap_index,
                                     wtf_size_t intersection_index,
                                     wtf_size_t intersection_count,
                                     bool is_main_gap) const {
  DCHECK_GT(intersection_count, 0u);
  const wtf_size_t last_intersection_index = intersection_count - 1;
  // For flex main-axis gaps and grid, the first and last intersections are
  // considered edges.
  if (is_main_gap || GetContainerType() == ContainerType::kGrid) {
    return intersection_index == 0 ||
           intersection_index == last_intersection_index;
  }

  if (GetContainerType() == ContainerType::kFlex) {
    DCHECK(!is_main_gap);
    // For flex cross-axis gaps:
    // - First, determine the edge state of the gap (start, end, or both).
    // - Based on this state, decide which intersections qualify as edges:
    //     * kBoth: Both first and last intersections are edges.
    //     * kStart: Only the first intersection is an edge.
    //     * kEnd: Only the last intersection is an edge.
    //
    // TODO(samomekarajr): Introducing the edge state to main_gap, can avoid the
    // special logic for flex cross gaps here. We can simply check the edge
    // state of the gap to determine if the first and/or last intersection are
    // edges.
    CrossGap::EdgeIntersectionState cross_gap_edge_state =
        GetCrossGaps()[gap_index].GetEdgeIntersectionState();
    if (cross_gap_edge_state == CrossGap::EdgeIntersectionState::kBoth) {
      return intersection_index == 0 ||
             intersection_index == last_intersection_index;
    } else if (cross_gap_edge_state ==
               CrossGap::EdgeIntersectionState::kStart) {
      return intersection_index == 0;
    } else if (cross_gap_edge_state == CrossGap::EdgeIntersectionState::kEnd) {
      return intersection_index == last_intersection_index;
    }
  }

  return false;
}

}  // namespace blink
