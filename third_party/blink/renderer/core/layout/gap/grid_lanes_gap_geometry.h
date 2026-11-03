// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GRID_LANES_GAP_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GRID_LANES_GAP_GEOMETRY_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/track_opening.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class GridLanesGapGeometry;

// Empty openings of each non-collapsed grid-lanes lane, indexed by lane, in
// logical stacking-axis coordinates. An empty opening has no item and no item
// that could align into it.
using GridLanesEmptyOpenings = Vector<Vector<TrackOpeningCoordinates>>;

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
    wtf_size_t before_occupant_index;
    wtf_size_t after_occupant_index;
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

  GridLanesMainGapSegmentWalker(const GridLanesGapGeometry& gap_geometry,
                                wtf_size_t main_gap_index);

  std::optional<const Segment> Next();
  wtf_size_t IntersectionCapacity() const { return intersection_capacity_; }

 private:
  // Forward cursor over one side's contiguous `CrossGap` run.
  class CrossGapRunCursor {
   public:
    CrossGapRunCursor() = default;
    CrossGapRunCursor(wtf_size_t start,
                      wtf_size_t inclusive_end,
                      wtf_size_t cross_gap_count)
        : start_(start), current_(start) {
      CHECK_LE(start, inclusive_end);
      CHECK_LT(inclusive_end, cross_gap_count);
      end_ = inclusive_end + 1;
    }

    bool AtEnd() const { return current_ == end_; }
    wtf_size_t Size() const { return end_ - start_; }
    wtf_size_t CrossGapIndex() const {
      CHECK(!AtEnd());
      return current_;
    }
    wtf_size_t ConsumedCount() const { return current_ - start_; }
    void Advance() {
      CHECK(!AtEnd());
      ++current_;
    }

   private:
    wtf_size_t start_ = 0;
    wtf_size_t current_ = 0;
    wtf_size_t end_ = 0;
  };

  LayoutUnit CrossGapOffset(wtf_size_t index) const;
  // Returns the next crossing offset from either lane, limited to content-end.
  // Does not advance either cursor.
  LayoutUnit NextCrossGapOffset() const;
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

  // Whether the next crossing gutter overlaps the current one.
  bool next_gutter_overlaps_current_ = false;
  bool finished_ = false;
};

class CORE_EXPORT GridLanesGapGeometry final : public GapGeometry {
 public:
  GridLanesGapGeometry() : GapGeometry(ContainerType::kGridLanes) {}

  // Stores finalized empty openings grouped by non-collapsed lane.
  void SetGridLanesEmptyOpenings(GridLanesEmptyOpenings empty_openings);

  // Returns the empty openings of a lane, as offsets along the stacking axis.
  // `lane_index` counts only non-collapsed lanes.
  const Vector<TrackOpeningCoordinates>& EmptyOpeningsForLane(
      wtf_size_t lane_index) const;

  // Marks each segment of a grid-lanes main gap, between two adjacent
  // intersections, as empty before or after when the lane on that side is empty
  // along the whole segment. The segment is bounded by the edges of the
  // crossing gutters, and its state is stored on the intersection that starts
  // it, as for grid. This way `rule-visibility-items` applies to each segment
  // before `rule-break` joins them.
  void MarkGridLanesEmptySegments(GridTrackSizingDirection track_direction,
                                  wtf_size_t gap_index,
                                  Vector<GapIntersection>& intersections) const;

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
  // TODO(javiercon): Remove this friend declaration when the methods that
  // `GapGeometry` calls can be made virtual.
  friend class GapGeometry;

  bool HasEqualSubclassFields(const GridLanesGapGeometry& other) const {
    return grid_lanes_empty_openings_ == other.grid_lanes_empty_openings_;
  }

  // Fills `intersections` for a grid-lanes main gap at `gap_index`.
  void GenerateMainIntersectionListForGridLanes(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      Vector<GapIntersection>& intersections,
      GapSegmentStateCursor& cursor) const;

  // Fills `intersections` for a grid-lanes `CrossGap` at `lane`, which
  // includes:
  // 1. The gap's start offset
  // 2. Its computed end offset at the end of `lane` (which could be a
  // `MainGap`)
  void GenerateCrossIntersectionListForGridLanes(
      wtf_size_t lane,
      Vector<GapIntersection>& intersections) const;

  // Empty openings per lane, used to determine visibility.
  GridLanesEmptyOpenings grid_lanes_empty_openings_;
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
