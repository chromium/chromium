// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GRID_LANES_GAP_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GRID_LANES_GAP_GEOMETRY_H_

#include <algorithm>
#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/track_opening.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class GridLanesGapGeometry;

// Empty openings of each non-collapsed grid-lanes lane, indexed by lane, in
// logical stacking-axis coordinates.
using GridLanesEmptyOpenings = Vector<Vector<TrackOpeningCoordinates>>;

// Records the start or end of an item's final border box in a lane.
// Offsets are relative to the container fragment along the stacking axis.
struct GridLanesOccupancyEvent {
  // Orders item events at coincident offsets.
  enum class Kind : uint8_t {
    kItemStart,
    kItemEnd,
    kOccupiedStart,
    kOccupiedEnd,
  };

  LayoutUnit offset;
  Kind kind;

  bool operator==(const GridLanesOccupancyEvent&) const = default;
};

using GridLanesItemOccupancy = Vector<Vector<GridLanesOccupancyEvent>>;

// Walks the gap decoration segments for a `MainGap` in a grid-lanes container.
// Segments are formed by the intersections of the `MainGap` with the
// `CrossGaps` (or items) in the adjacent lanes.
//
// This is different from `GapSegmentStateCursor`, which does not generate
// segment geometry. That cursor only walks stored `GapSegmentStateRange`s in
// the segment order produced by this class and returns the state for each
// index.
class GridLanesMainGapSegmentWalker {
  STACK_ALLOCATED();

 public:
  struct Segment {
    LayoutUnit end_offset;
    CrossGap::Type end_type = CrossGap::Type::kGutter;
    // Whether the crossing gutter at `end_offset` overlaps the preceding
    // crossing gutter. False for the first gutter, since there is no previous
    // gutter to overlap. It is also false when the gutters' edges only touch,
    // when there is space between them, or when `end_offset` marks the end of
    // the container's content.
    //
    // For more about overlap windows see:
    // https://www.w3.org/TR/css-gaps-1/#segment-endpoints
    bool continues_overlap_window = false;
  };

  // Merges real gutters and artificial boundaries from the adjacent lanes.
  GridLanesMainGapSegmentWalker(const GridLanesGapGeometry& gap_geometry,
                                wtf_size_t main_gap_index);

  // Returns the next endpoint, skipping covered artificial boundaries.
  std::optional<const Segment> Next();
  wtf_size_t IntersectionCapacity() const { return intersection_capacity_; }

 private:
  // Forward cursor over one side's contiguous `CrossGap` run.
  class CrossGapRunCursor {
   public:
    CrossGapRunCursor() = default;
    CrossGapRunCursor(wtf_size_t start,
                      wtf_size_t inclusive_end,
                      const CrossGaps& cross_gaps)
        : current_(start), next_gutter_(start) {
      CHECK_LE(start, inclusive_end);
      CHECK_LT(inclusive_end, cross_gaps.size());
      end_ = inclusive_end + 1;
      SkipArtificialGaps(cross_gaps);
    }

    bool AtEnd() const { return current_ == end_; }
    wtf_size_t RemainingCount() const { return end_ - current_; }
    wtf_size_t CrossGapIndex() const {
      CHECK(!AtEnd());
      return current_;
    }
    bool HasGutter() const { return next_gutter_ < end_; }
    wtf_size_t NextGutterIndex() const {
      CHECK(HasGutter());
      return next_gutter_;
    }
    // Advance the geometry cursor while keeping genuine-gutter lookahead.
    void Advance(const CrossGaps& cross_gaps) {
      CHECK(!AtEnd());
      ++current_;
      next_gutter_ = std::max(next_gutter_, current_);
      SkipArtificialGaps(cross_gaps);
    }

   private:
    // Keep a separate lookahead for genuine gutters so artificial boundaries
    // cannot create or interrupt gutter overlap windows.
    void SkipArtificialGaps(const CrossGaps& cross_gaps) {
      while (next_gutter_ < end_ && cross_gaps[next_gutter_].IsArtificial()) {
        ++next_gutter_;
      }
    }

    wtf_size_t current_ = 0;
    wtf_size_t end_ = 0;
    wtf_size_t next_gutter_ = 0;
  };

  LayoutUnit CrossGapOffset(wtf_size_t index) const;
  // Returns the next crossing offset from either lane, limited to content-end.
  // Does not advance either cursor.
  LayoutUnit NextCrossGapOffset() const;
  // Real gutters take precedence over coincident artificial boundaries.
  CrossGap::Type CrossGapTypeAtOffset(LayoutUnit offset) const;
  // Artificial boundaries inside a gutter do not split its intersection.
  bool IsInsideGutter(LayoutUnit offset) const;
  void SkipGapsAtOrBeforeContentStart();
  void SkipRunAtOrBeforeContentStart(CrossGapRunCursor& run);
  void ConsumeRunAtOffset(CrossGapRunCursor& run, LayoutUnit offset);

  const GridLanesGapGeometry& gap_geometry_;
  GridTrackSizingDirection cross_direction_;
  LayoutUnit content_start_;
  LayoutUnit content_end_;
  LayoutUnit cross_gap_width_;
  CrossGapRunCursor before_;
  CrossGapRunCursor after_;
  wtf_size_t intersection_capacity_ = 0;

  std::optional<LayoutUnit> previous_gutter_offset_;
  bool previous_intersection_is_gutter_ = false;
  bool finished_ = false;
};

// Gap geometry for grid-lanes, including artificial cross gaps.
//
// Real cross gaps mark placement gutters between items in a lane.
// Self-alignment can move an item's final border-box edges away from those
// gutters. Artificial cross gaps record these edges for items with available
// stacking-axis alignment space. Spanners also add boundaries one stacking gap
// before and after their final border boxes to preserve clearance.
//
// Artificial cross gaps subdivide grid-axis rules for item visibility and
// spanner blocking. They are not gutters: they never paint stacking-axis rules
// or consume decoration-list values. Actual item occupancy is stored in
// separate compact events, including edges that do not create intersections.
//
// At artificial intersections, cap insets apply with zero crossing width.
// These boundaries do not force an intersection break on an otherwise visible,
// unblocked rule.
class CORE_EXPORT GridLanesGapGeometry final : public GapGeometry {
 public:
  GridLanesGapGeometry() : GapGeometry(ContainerType::kGridLanes) {}

  // Stores finalized empty openings grouped by non-collapsed lane.
  void SetEmptyOpenings(GridLanesEmptyOpenings empty_openings);

  void SetItemOccupancy(GridLanesItemOccupancy item_occupancy);
  const Vector<GridLanesOccupancyEvent>& ItemOccupancyForLane(
      wtf_size_t lane_index) const;

  // Records one lane's slots in the paintable cross-gap sequence, in lane
  // order. Artificial boundaries do not consume slots.
  void AddCrossGapDecorationSlots(wtf_size_t gap_count);

  // Returns the empty openings of a lane, as offsets along the stacking axis.
  // `lane_index` counts only non-collapsed lanes.
  const Vector<TrackOpeningCoordinates>& EmptyOpeningsForLane(
      wtf_size_t lane_index) const;

  // A lane boundary separates adjacent non-collapsed tracks. Maps a lane
  // boundary to its grid-axis offset:
  //   lane boundary 0            -> grid-axis content start
  //   lane boundary lane_count   -> grid-axis content end
  //   lane boundary i (else)     -> main_gaps_[i - 1] center
  // where `lane_count == MainGapCount() + 1`.
  // Interior lane boundaries correspond to `MainGap`s; the outer lane
  // boundaries are content edges.
  LayoutUnit GridAxisOffsetForLaneBoundary(wtf_size_t lane_boundary) const;

 private:
  // Compares placement openings, actual occupancy, and decoration-slot ranges.
  bool HasEqualSubclassFields(const GapGeometry& other) const override;

  // Each cross gap belongs to the lane that contains it.
  bool CrossGapsHaveOwners() const override { return true; }

  // Layout stores lane-local slot ranges so assignment is constant-time.
  GapIndexRange PaintableCrossGapRangeForOwner(
      wtf_size_t owner_index) const override;

  // Fills `intersections` for a grid-lanes main gap at `gap_index`. The
  // intersections come from the cross gaps of the lanes on both sides of the
  // main gap. Item endpoints and actual occupancy are used only for `between`.
  // Overlapping crossing gutters become one overlap window.
  void GenerateMainIntersectionList(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      RuleVisibilityItems rule_visibility,
      GapSegmentStateCursor& cursor,
      Vector<GapIntersection>& intersections) const override;

  // Marks each `around` segment of a grid-lanes main gap, between two adjacent
  // intersections, as empty before or after when the lane on that side is empty
  // along the whole segment. The segment is bounded by the edges of the
  // crossing gutters, and its state is stored on the intersection that starts
  // it, as for grid. This way `rule-visibility-items` applies to each segment
  // before `rule-break` joins them.
  void MarkEmptySegments(GridTrackSizingDirection direction,
                         wtf_size_t gap_index,
                         RuleVisibilityItems rule_visibility,
                         Vector<GapIntersection>& intersections) const override;

  // Checks actual occupancy after overlapping gutters have been reduced.
  void MarkEmptySides(wtf_size_t main_gap_index,
                      Vector<GapIntersection>& intersections) const;

  // Fills `intersections` for a grid-lanes `CrossGap` in the lane at
  // `cross_gap_owner_index`, which includes:
  // 1. The gap's start offset
  // 2. Its computed end offset at the end of the lane (which could be a
  // `MainGap`)
  // `cross_gap_owner_index` must have a value. The intersections depend only
  // on the lane, so `direction`, `gap_index`, and `cursor` are not used.
  void GenerateCrossIntersectionList(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      std::optional<wtf_size_t> cross_gap_owner_index,
      GapSegmentStateCursor& cursor,
      Vector<GapIntersection>& intersections) const override;

  // `around` retains placement openings.
  GridLanesEmptyOpenings empty_openings_;

  // Final border-box occupancy, indexed by non-collapsed lane.
  GridLanesItemOccupancy item_occupancy_;

  // Restarts color/style/width list assignment for each lane without counting
  // artificial gaps as decoration slots.
  Vector<GapIndexRange> cross_gap_decoration_ranges_;
};

template <>
struct DowncastTraits<GridLanesGapGeometry> {
  static bool AllowFrom(const GapGeometry& gap_geometry) {
    return gap_geometry.GetContainerType() ==
           GapGeometry::ContainerType::kGridLanes;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GRID_LANES_GAP_GEOMETRY_H_
