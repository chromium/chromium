// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/gap/grid_lanes_gap_geometry.h"

#include <algorithm>

namespace blink {

namespace {

// Returns whether a lane is empty along a whole segment, meaning the segment
// lies inside one of the lane's empty openings. A segment that only partly
// overlaps empty space is not empty.
bool ConsumeOpeningsAndCheckEmpty(
    base::span<const TrackOpeningCoordinates>& remaining_empty_openings,
    LayoutUnit start,
    LayoutUnit end) {
  CHECK_LT(start, end);

  // Segments are checked in increasing order. An opening that ends before
  // this segment starts cannot contain this or any later segment, so skip it
  // for good.
  while (!remaining_empty_openings.empty() &&
         remaining_empty_openings.front().end_position <= start) {
    remaining_empty_openings = remaining_empty_openings.subspan(1u);
  }

  // A lane with no empty openings left is occupied along this segment.
  if (remaining_empty_openings.empty()) {
    return false;
  }
  const TrackOpeningCoordinates& opening = remaining_empty_openings.front();
  return opening.start_position <= start && end <= opening.end_position;
}

}  // namespace

bool GridLanesGapGeometry::HasEqualSubclassFields(
    const GapGeometry& other) const {
  return empty_openings_ == To<GridLanesGapGeometry>(other).empty_openings_;
}

void GridLanesGapGeometry::SetEmptyOpenings(
    GridLanesEmptyOpenings empty_openings) {
  CHECK_EQ(empty_openings.size(), MainGapCount() + 1);
  empty_openings_ = std::move(empty_openings);
}

const Vector<TrackOpeningCoordinates>&
GridLanesGapGeometry::EmptyOpeningsForLane(wtf_size_t lane_index) const {
  return empty_openings_[lane_index];
}

void GridLanesGapGeometry::MarkEmptySegments(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    Vector<GapIntersection>& intersections) const {
  CHECK(IsMainDirection(direction));

  base::span<const TrackOpeningCoordinates> before_openings =
      EmptyOpeningsForLane(gap_index);
  base::span<const TrackOpeningCoordinates> after_openings =
      EmptyOpeningsForLane(gap_index + 1);

  // A lane without empty openings is occupied along every segment, so there is
  // nothing to mark on its side.
  const bool check_before = !before_openings.empty();
  const bool check_after = !after_openings.empty();
  if (!check_before && !check_after) {
    return;
  }

  LayoutUnit segment_start =
      intersections[0].GetOffset() +
      GetCrossWidthForIntersection(direction, gap_index,
                                   /*intersection_index=*/0,
                                   /*is_main_gap=*/true, intersections) /
          2;
  for (wtf_size_t i = 0; i + 1 < intersections.size(); ++i) {
    const LayoutUnit next_half_cross_width =
        GetCrossWidthForIntersection(direction, gap_index, i + 1,
                                     /*is_main_gap=*/true, intersections) /
        2;
    const LayoutUnit segment_end =
        intersections[i + 1].GetOffset() - next_half_cross_width;

    // Where the crossing gutters of the adjacent lanes overlap, the space
    // between the two intersections has no positive length. It is not a real
    // segment, so it is not marked, and it cannot hide a rule.
    if (segment_end > segment_start) {
      GapSegmentState segment_state = intersections[i].SegmentState();
      if (check_before && ConsumeOpeningsAndCheckEmpty(
                              before_openings, segment_start, segment_end)) {
        segment_state |= GapSegmentState::kEmptyBefore;
      }
      if (check_after && ConsumeOpeningsAndCheckEmpty(
                             after_openings, segment_start, segment_end)) {
        segment_state |= GapSegmentState::kEmptyAfter;
      }
      intersections[i].SetSegmentState(segment_state);
    }

    segment_start = intersections[i + 1].GetOffset() + next_half_cross_width;
  }
}

GridLanesMainGapSegmentWalker::GridLanesMainGapSegmentWalker(
    const GridLanesGapGeometry& gap_geometry,
    wtf_size_t main_gap_index)
    : gap_geometry_(gap_geometry),
      cross_direction_(OppositeDirection(gap_geometry.GetMainDirection())) {
  if (cross_direction_ == kForRows) {
    content_start_ = gap_geometry.GetContentBlockStart();
    content_end_ = gap_geometry.GetContentBlockEnd();
    cross_gap_width_ = gap_geometry.GetBlockGapSize();
  } else {
    content_start_ = gap_geometry.GetContentInlineStart();
    content_end_ = gap_geometry.GetContentInlineEnd();
    cross_gap_width_ = gap_geometry.GetInlineGapSize();
  }

  CHECK_LT(main_gap_index, gap_geometry.MainGapCount());
  const MainGap& main_gap = gap_geometry.MainGapAt(main_gap_index);
  if (main_gap.HasCrossGapsBefore()) {
    before_ = CrossGapRunCursor(main_gap.GetCrossGapBeforeStart(),
                                main_gap.GetCrossGapBeforeEnd(),
                                gap_geometry.CrossGapCount());
  }
  if (main_gap.HasCrossGapsAfter()) {
    after_ = CrossGapRunCursor(main_gap.GetCrossGapAfterStart(),
                               main_gap.GetCrossGapAfterEnd(),
                               gap_geometry.CrossGapCount());
  }
  // The 2 accounts for the content-start and content-end intersections.
  intersection_capacity_ = 2 + before_.Size() + after_.Size();
  SkipGapsAtOrBeforeContentStart();
}

LayoutUnit GridLanesMainGapSegmentWalker::CrossGapOffset(
    wtf_size_t index) const {
  CHECK_LT(index, gap_geometry_.CrossGapCount());
  return gap_geometry_.GetCrossGaps()[index].GetGapOffset(cross_direction_);
}

LayoutUnit GridLanesMainGapSegmentWalker::NextCrossGapOffset() const {
  LayoutUnit offset = content_end_;
  if (!before_.AtEnd()) {
    offset = std::min(offset, CrossGapOffset(before_.CrossGapIndex()));
  }
  if (!after_.AtEnd()) {
    offset = std::min(offset, CrossGapOffset(after_.CrossGapIndex()));
  }
  return offset;
}

void GridLanesMainGapSegmentWalker::SkipGapsAtOrBeforeContentStart() {
  SkipRunAtOrBeforeContentStart(before_);
  SkipRunAtOrBeforeContentStart(after_);
}

void GridLanesMainGapSegmentWalker::SkipRunAtOrBeforeContentStart(
    CrossGapRunCursor& run) {
  while (!run.AtEnd() &&
         CrossGapOffset(run.CrossGapIndex()) <= content_start_) {
    run.Advance();
  }
}

void GridLanesMainGapSegmentWalker::ConsumeRunAtOffset(CrossGapRunCursor& run,
                                                       LayoutUnit offset) {
  while (!run.AtEnd() && CrossGapOffset(run.CrossGapIndex()) == offset) {
    run.Advance();
  }
  CHECK(run.AtEnd() || CrossGapOffset(run.CrossGapIndex()) > offset);
}

std::optional<const GridLanesMainGapSegmentWalker::Segment>
GridLanesMainGapSegmentWalker::Next() {
  if (finished_) {
    return std::nullopt;
  }

  Segment segment{content_end_, before_.ConsumedCount(),
                  after_.ConsumedCount()};
  const LayoutUnit offset = NextCrossGapOffset();
  if (offset >= content_end_) {
    finished_ = true;
    return segment;
  }

  ConsumeRunAtOffset(before_, offset);
  ConsumeRunAtOffset(after_, offset);

  segment.end_offset = offset;
  segment.continues_overlap_window = next_gutter_overlaps_current_;

  // Touching gutters and content boundaries do not extend the overlap window.
  const LayoutUnit next_offset = NextCrossGapOffset();
  next_gutter_overlaps_current_ =
      next_offset < content_end_ && next_offset - offset < cross_gap_width_;
  return segment;
}

void GridLanesGapGeometry::GenerateMainIntersectionList(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    GapSegmentStateCursor& cursor,
    Vector<GapIntersection>& intersections) const {
  GridLanesMainGapSegmentWalker walker(*this, gap_index);
  intersections.reserve(walker.IntersectionCapacity());
  const LayoutUnit content_start = direction == kForColumns
                                       ? GetContentBlockStart()
                                       : GetContentInlineStart();
  intersections.emplace_back(content_start, cursor.GetNextGapSegmentState());

  // Segments are initially generated using the center of each crossing
  // gutter. Intersections from overlapping gutters are then reduced to an
  // opening/closing pair. Advance the state cursor for every intersection
  // produced by the walker, including those not kept in the pair, so spanner
  // states remain aligned with the correct segments.
  while (auto segment = walker.Next()) {
    const GapSegmentState outgoing_state = cursor.GetNextGapSegmentState();
    if (!segment->continues_overlap_window) {
      intersections.emplace_back(segment->end_offset, outgoing_state,
                                 OverlapWindowState::kNone);
      continue;
    }

    // An existing window is stored as an opening/closing pair. If the last
    // intersection closes a window, extend that window to the new gutter.
    if (intersections.back().IsOverlapWindowClose()) {
      CHECK_GE(intersections.size(), 3u);
      GapIntersection& opening = intersections[intersections.size() - 2];
      CHECK(opening.IsOverlapWindowOpen());
      GapIntersection& closing = intersections.back();

      // Moving the closing intersection to the new gutter puts the segment
      // after its current position inside the window. Add that segment's
      // blocked state to the opening before updating the closing position.
      GapSegmentState interior_state = opening.SegmentState();
      interior_state |= closing.SegmentState();
      opening.SetSegmentState(interior_state);
      closing.SetOffset(segment->end_offset);
      closing.SetSegmentState(outgoing_state);
      continue;
    }

    // The first overlapping pair establishes the window. Subsequent centers
    // move only its closing point, but retain both the interior blocked
    // state and the outgoing state separately.
    intersections.back().SetOverlapState(OverlapWindowState::kWindowOpen);
    intersections.emplace_back(segment->end_offset, outgoing_state,
                               OverlapWindowState::kWindowClose);
  }
}

LayoutUnit GridLanesGapGeometry::GridAxisOffsetForLaneBoundary(
    wtf_size_t lane_boundary) const {
  // A lane boundary separates adjacent non-collapsed tracks. Interior lane
  // boundaries correspond to `MainGap`s; the outer lane boundaries are content
  // edges.
  const bool grid_axis_is_inline = GetMainDirection() == kForColumns;
  const LayoutUnit content_start =
      grid_axis_is_inline ? GetContentInlineStart() : GetContentBlockStart();
  const LayoutUnit content_end =
      grid_axis_is_inline ? GetContentInlineEnd() : GetContentBlockEnd();
  const wtf_size_t lane_count = MainGapCount() + 1;

  if (lane_boundary == 0) {
    return content_start;
  }
  if (lane_boundary == lane_count) {
    return content_end;
  }
  return MainGapAt(lane_boundary - 1).GetGapOffset();
}

void GridLanesGapGeometry::GenerateCrossIntersectionList(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    std::optional<wtf_size_t> cross_gap_owner_index,
    GapSegmentStateCursor& cursor,
    Vector<GapIntersection>& intersections) const {
  CHECK(cross_gap_owner_index);
  const wtf_size_t lane = *cross_gap_owner_index;

  // A grid-lanes `CrossGap` is one stacking-axis gutter confined to a single
  // lane, so it generates exactly two intersections: one at each of the lane's
  // grid-axis boundaries. It never crosses a grid-axis gutter.
  const wtf_size_t lane_count = MainGapCount() + 1;
  CHECK_LT(lane, lane_count);
  CHECK(intersections.empty());

  intersections.reserve(2);

  // The start intersection borders the preceding `MainGap` or content edge.
  GapIntersection start(GridAxisOffsetForLaneBoundary(lane));
  if (lane > 0) {
    start.SetMainGapIndex(lane - 1);
  }
  intersections.push_back(start);

  // The end intersection borders the following `MainGap` or content edge.
  GapIntersection end(GridAxisOffsetForLaneBoundary(lane + 1));
  if (lane + 1 < lane_count) {
    end.SetMainGapIndex(lane);
  }
  intersections.push_back(end);
}

}  // namespace blink
