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
  const auto& other_geometry = To<GridLanesGapGeometry>(other);
  return empty_openings_ == other_geometry.empty_openings_ &&
         cross_gap_decoration_ranges_ ==
             other_geometry.cross_gap_decoration_ranges_;
}

void GridLanesGapGeometry::SetEmptyOpenings(
    GridLanesEmptyOpenings empty_openings) {
  CHECK_EQ(empty_openings.size(), MainGapCount() + 1);
  empty_openings_ = std::move(empty_openings);
}

void GridLanesGapGeometry::AddCrossGapDecorationSlots(wtf_size_t gap_count) {
  CHECK_LE(cross_gap_decoration_ranges_.size(), MainGapCount());
  wtf_size_t start = 0;
  if (cross_gap_decoration_ranges_.empty()) {
    cross_gap_decoration_ranges_.ReserveInitialCapacity(MainGapCount() + 1);
  } else {
    const auto& previous = cross_gap_decoration_ranges_.back();
    start = previous.start + previous.count;
  }
  CHECK_LE(start, CrossGapCount());
  CHECK_LE(gap_count, CrossGapCount() - start);
  cross_gap_decoration_ranges_.emplace_back(start, gap_count);
}

GapGeometry::GapIndexRange GridLanesGapGeometry::PaintableCrossGapRangeForOwner(
    wtf_size_t owner_index) const {
  CHECK_LT(owner_index, cross_gap_decoration_ranges_.size());
  return cross_gap_decoration_ranges_[owner_index];
}

const Vector<TrackOpeningCoordinates>&
GridLanesGapGeometry::EmptyOpeningsForLane(wtf_size_t lane_index) const {
  return empty_openings_[lane_index];
}

void GridLanesGapGeometry::MarkEmptySegments(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    RuleVisibilityItems rule_visibility,
    Vector<GapIntersection>& intersections) const {
  CHECK(IsMainDirection(direction));

  if (rule_visibility == RuleVisibilityItems::kBetween ||
      rule_visibility == RuleVisibilityItems::kAll) {
    return;
  }
  base::span<const TrackOpeningCoordinates> before_openings =
      empty_openings_[gap_index];
  base::span<const TrackOpeningCoordinates> after_openings =
      empty_openings_[gap_index + 1];

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
                                gap_geometry.GetCrossGaps());
  }
  if (main_gap.HasCrossGapsAfter()) {
    after_ = CrossGapRunCursor(main_gap.GetCrossGapAfterStart(),
                               main_gap.GetCrossGapAfterEnd(),
                               gap_geometry.GetCrossGaps());
  }
  // The 2 accounts for the content-start and content-end intersections.
  intersection_capacity_ =
      2 + before_.RemainingCount() + after_.RemainingCount();
  occupancy_before_ = before_;
  occupancy_after_ = after_;
  SkipGapsAtOrBeforeContentStart();
}

void GridLanesMainGapSegmentWalker::CrossGapRunCursor::ConsumeOccupancy(
    const CrossGap& gap) {
  switch (gap.GetType()) {
    case CrossGap::Type::kItemStart:
    case CrossGap::Type::kOccupiedStart:
      ++occupied_item_count_;
      break;
    case CrossGap::Type::kItemEnd:
    case CrossGap::Type::kOccupiedEnd:
      CHECK_GT(occupied_item_count_, 0u);
      --occupied_item_count_;
      break;
    default:
      break;
  }
}

bool GridLanesMainGapSegmentWalker::CrossGapRunCursor::IsEmpty(
    const CrossGaps& cross_gaps,
    GridTrackSizingDirection direction,
    LayoutUnit start,
    LayoutUnit end) {
  CHECK_LT(start, end);
  bool is_empty = occupied_item_count_ == 0;
  while (!AtEnd()) {
    const auto& gap = cross_gaps[CrossGapIndex()];
    const LayoutUnit offset = gap.GetGapOffset(direction);
    if (offset >= end) {
      break;
    }

    ConsumeOccupancy(gap);
    Advance(cross_gaps);

    const bool currently_empty = occupied_item_count_ == 0;
    if (offset <= start) {
      is_empty = currently_empty;
    } else if (!currently_empty) {
      is_empty = false;
    }
  }
  return is_empty;
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

CrossGap::Type GridLanesMainGapSegmentWalker::CrossGapTypeAtOffset(
    LayoutUnit offset) const {
  CrossGap::Type type = CrossGap::Type::kOccupiedEnd;
  for (const auto* run : {&before_, &after_}) {
    if (run->AtEnd() || CrossGapOffset(run->CrossGapIndex()) != offset) {
      continue;
    }
    const auto candidate =
        gap_geometry_.GetCrossGaps()[run->CrossGapIndex()].GetType();
    if (candidate == CrossGap::Type::kGutter) {
      return candidate;
    }
    // Lower enum values have precedence at coincident offsets.
    type = std::min(type, candidate);
  }
  return type;
}

bool GridLanesMainGapSegmentWalker::IsInsideGutter(LayoutUnit offset) const {
  const LayoutUnit half_width = cross_gap_width_ / 2;
  if (previous_gutter_offset_ &&
      offset - *previous_gutter_offset_ <= half_width) {
    return true;
  }

  LayoutUnit next_gutter_offset = content_end_;
  for (const auto* run : {&before_, &after_}) {
    if (run->HasGutter()) {
      next_gutter_offset =
          std::min(next_gutter_offset, CrossGapOffset(run->NextGutterIndex()));
    }
  }
  return next_gutter_offset < content_end_ &&
         next_gutter_offset - offset <= half_width;
}

void GridLanesMainGapSegmentWalker::MarkEmptySides(
    Vector<GapIntersection>& intersections) {
  const auto& cross_gaps = gap_geometry_.GetCrossGaps();
  const LayoutUnit half_width = cross_gap_width_ / 2;
  LayoutUnit segment_start = intersections.front().GetOffset();

  for (wtf_size_t i = 0; i + 1 < intersections.size(); ++i) {
    auto& intersection = intersections[i];
    const auto& next_intersection = intersections[i + 1];

    // Only interior real gutters trim the segment. Artificial endpoints and
    // content edges have no crossing width.
    const LayoutUnit next_half_width =
        i + 2 < intersections.size() && !next_intersection.IsArtificial()
            ? half_width
            : LayoutUnit();
    const LayoutUnit segment_end =
        next_intersection.GetOffset() - next_half_width;

    // Check the final opening/closing pair, not each pair of gutter centers.
    // A long overlap window can have a positive-length trimmed interior.
    if (segment_start < segment_end) {
      GapSegmentState state = intersection.SegmentState();
      if (occupancy_before_.IsEmpty(cross_gaps, cross_direction_, segment_start,
                                    segment_end)) {
        state |= GapSegmentState::kEmptyBefore;
      }
      if (occupancy_after_.IsEmpty(cross_gaps, cross_direction_, segment_start,
                                   segment_end)) {
        state |= GapSegmentState::kEmptyAfter;
      }
      intersection.SetSegmentState(state);
    }

    segment_start = next_intersection.GetOffset() + next_half_width;
  }
}

void GridLanesMainGapSegmentWalker::SkipGapsAtOrBeforeContentStart() {
  SkipRunAtOrBeforeContentStart(before_);
  SkipRunAtOrBeforeContentStart(after_);
}

void GridLanesMainGapSegmentWalker::SkipRunAtOrBeforeContentStart(
    CrossGapRunCursor& run) {
  while (!run.AtEnd() &&
         CrossGapOffset(run.CrossGapIndex()) <= content_start_) {
    run.Advance(gap_geometry_.GetCrossGaps());
  }
}

void GridLanesMainGapSegmentWalker::ConsumeRunAtOffset(CrossGapRunCursor& run,
                                                       LayoutUnit offset) {
  while (!run.AtEnd() && CrossGapOffset(run.CrossGapIndex()) == offset) {
    run.Advance(gap_geometry_.GetCrossGaps());
  }
  CHECK(run.AtEnd() || CrossGapOffset(run.CrossGapIndex()) > offset);
}

std::optional<const GridLanesMainGapSegmentWalker::Segment>
GridLanesMainGapSegmentWalker::Next() {
  if (finished_) {
    return std::nullopt;
  }

  while (true) {
    const LayoutUnit offset = NextCrossGapOffset();
    if (offset >= content_end_) {
      finished_ = true;
      return Segment{content_end_};
    }

    const CrossGap::Type type = CrossGapTypeAtOffset(offset);
    const bool is_gutter = type == CrossGap::Type::kGutter;
    const bool occupancy_only = type == CrossGap::Type::kOccupiedStart ||
                                type == CrossGap::Type::kOccupiedEnd;
    const bool skip_boundary =
        occupancy_only || (!is_gutter && IsInsideGutter(offset));
    ConsumeRunAtOffset(before_, offset);
    ConsumeRunAtOffset(after_, offset);
    if (skip_boundary) {
      continue;
    }

    // Only genuine gutters participate in overlap windows. Touching gutter
    // edges, artificial boundaries, and content edges do not extend a window.
    const bool continues_overlap =
        is_gutter && previous_intersection_is_gutter_ &&
        offset - *previous_gutter_offset_ < cross_gap_width_;
    previous_intersection_is_gutter_ = is_gutter;
    if (is_gutter) {
      previous_gutter_offset_ = offset;
    }
    return Segment{offset, type, continues_overlap};
  }
}

void GridLanesGapGeometry::GenerateMainIntersectionList(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    RuleVisibilityItems rule_visibility,
    GapSegmentStateCursor& cursor,
    Vector<GapIntersection>& intersections) const {
  GridLanesMainGapSegmentWalker walker(*this, gap_index);
  const bool needs_item_occupancy =
      rule_visibility == RuleVisibilityItems::kBetween;
  intersections.reserve(walker.IntersectionCapacity());
  const LayoutUnit content_start = direction == kForColumns
                                       ? GetContentBlockStart()
                                       : GetContentInlineStart();
  intersections.emplace_back(content_start, cursor.GetNextGapSegmentState());

  // Segments are initially generated using the center of each crossing
  // gutter. Intersections from overlapping gutters are then reduced to an
  // opening/closing pair. Advance the state cursor for every intersection
  // produced by the walker, including omitted item endpoints and those not
  // kept in the pair, so spanner states remain aligned with the segments.
  while (auto segment = walker.Next()) {
    const GapSegmentState outgoing_state = cursor.GetNextGapSegmentState();
    GapIntersectionKind kind = GapIntersectionKind::kNormal;
    if (segment->end_type == CrossGap::Type::kSpannerBoundary) {
      kind = GapIntersectionKind::kSpannerBoundary;
    } else if (segment->end_type == CrossGap::Type::kItemStart ||
               segment->end_type == CrossGap::Type::kItemEnd) {
      if (!needs_item_occupancy) {
        continue;
      }
      kind = GapIntersectionKind::kItemBoundary;
    }
    if (!segment->continues_overlap_window) {
      intersections.emplace_back(segment->end_offset, outgoing_state,
                                 OverlapWindowState::kNone, kind);
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
  if (needs_item_occupancy) {
    walker.MarkEmptySides(intersections);
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
