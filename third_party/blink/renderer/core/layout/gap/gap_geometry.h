// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GAP_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GAP_GEOMETRY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/gap/main_gap.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/style/grid_enums.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Represents the direction in which a GapIntersecion is blocked. When
// considering column gaps, `kBefore` means a GapIntersection is blocked by a
// spanning item upwards and `kAfter` means it is blocked downwards. When
// considering row gaps, `kBefore` means a GapIntersection is blocked by a
// spanning item to the left and `kAfter` means it is blocked to the right.
enum class BlockedGapDirection {
  kBefore,
  kAfter,
};

// This bitmask indicates whether an intersection is blocked due to the presence
// of a spanning item in one or both directions. When considering column gaps,
// `kBlockedBefore` means the intersection is blocked by a spanning item
// upwards and `kBlockedAfter` means it is blocked downwards. When
// considering row gaps, `kBlockedBefore` means the intersection is blocked by a
// spanning item to the left and `kBlockedAfter` means it is blocked to the
// right.
class CORE_EXPORT BlockedStatus {
 public:
  enum BlockStatusId : unsigned {
    kNone = 0,
    kBlockedBefore = 1 << 0,
    kBlockedAfter = 1 << 1,
  };

  inline bool HasBlockedStatus(BlockStatusId status) const {
    return (status_ & status) != 0;
  }
  inline void SetBlockedStatus(BlockStatusId status) { status_ |= status; }

  inline bool operator&(BlockStatusId status) const {
    return HasBlockedStatus(status);
  }
  inline BlockedStatus& operator|=(BlockStatusId status) {
    SetBlockedStatus(status);
    return *this;
  }

 private:
  wtf_size_t status_{kNone};
};

// GapIntersection points are used to paint gap decorations. An intersection
// point occurs:
// 1. At the center of an intersection between a gap and the container edge.
// 2. At the center of an intersection between gaps in different directions.
// https://drafts.csswg.org/css-gaps-1/#layout-painting
class GapIntersection {
 public:
  GapIntersection() = default;
  GapIntersection(LayoutUnit inline_offset, LayoutUnit block_offset)
      : inline_offset(inline_offset), block_offset(block_offset) {}

  GapIntersection(LayoutUnit inline_offset,
                  LayoutUnit block_offset,
                  bool is_at_edge_of_container)
      : inline_offset(inline_offset),
        block_offset(block_offset),
        is_at_edge_of_container(is_at_edge_of_container) {}

  String ToString(bool verbose = false) const;

  LayoutUnit inline_offset;
  LayoutUnit block_offset;

  // Represents whether the intersection point is blocked before or after due to
  // the presence of a spanning item.
  bool is_blocked_before = false;
  bool is_blocked_after = false;

  bool is_at_edge_of_container = false;
};

using GapIntersectionList = Vector<GapIntersection>;
using MainGaps = Vector<MainGap>;
using CrossGaps = Vector<CrossGap>;

// Represents a range of tracks within a grid or columns within a multicol. Used
// to track areas inside a gap that are blocked by spanners.
struct TrackRange {
  wtf_size_t start;
  wtf_size_t end;
};

using TrackRanges = Vector<TrackRange>;

// Represents a mapping from gap indices to the ranges of tracks blocked within
// those gaps. For example, a gap with index 0 might map to a list of track
// ranges such as {[2, 4], 7, 9]}, indicating that tracks 2 through 4 and 7
// through 9 are blocked in gap[0].
using GapToTrackRangesMap =
    HashMap<wtf_size_t, TrackRanges, blink::IntWithZeroKeyHashTraits<int>>;

// Gap geometry is used to determine gap locations for the purpose of painting
// gap decorations.
//
// See third_party/blink/renderer/core/layout/gap/README.md for more.
class CORE_EXPORT GapGeometry : public GarbageCollected<GapGeometry> {
 public:
  enum ContainerType {
    kGrid,
    kFlex,
    kMultiColumn,
  };

  explicit GapGeometry(ContainerType container_type)
      : container_type_(container_type) {}

  // This copy-esque constructor allows creating a new GapGeometry
  // instance based on an existing one, while replacing the main gaps and
  // content block offsets. This is useful for fragmentation where most states
  // remain the same, but the content block offsets and main gaps may differ.
  GapGeometry(const GapGeometry& other,
              MainGaps&& main_gaps,
              LayoutUnit content_block_start,
              LayoutUnit content_block_end)
      : column_intersections_(other.column_intersections_),
        row_intersections_(other.row_intersections_),
        inline_gap_size_(other.inline_gap_size_),
        block_gap_size_(other.block_gap_size_),
        container_type_(other.container_type_),
        main_gaps_(std::move(main_gaps)),
        cross_gaps_(other.cross_gaps_),
        content_inline_start_(other.content_inline_start_),
        content_inline_end_(other.content_inline_end_),
        content_block_start_(content_block_start),
        content_block_end_(content_block_end),
        row_gaps_to_blocked_column_ranges_(
            other.row_gaps_to_blocked_column_ranges_),
        column_gaps_to_blocked_row_ranges_(
            other.column_gaps_to_blocked_row_ranges_),
        main_direction_(other.main_direction_),
        main_gap_running_index_(other.main_gap_running_index_) {}

  void Trace(Visitor* visitor) const {}

  void SetGapIntersections(GridTrackSizingDirection track_direction,
                           Vector<GapIntersectionList>&& intersection_list);

  const Vector<GapIntersectionList>& GetGapIntersections(
      GridTrackSizingDirection track_direction) const;

  // Computes the physical bounding rect for gap decorations ink overflow.
  PhysicalRect ComputeInkOverflowForGaps(WritingDirectionMode writing_direction,
                                         const PhysicalSize& container_size,
                                         LayoutUnit inline_thickness,
                                         LayoutUnit block_thickness) const;

  // Computes the physical bounding rect for gap decorations ink overflow.
  //
  // TODO(samomekarajr): Rename this to ComputeInkOverflowForGaps and remove the
  // other function during cleanup.
  PhysicalRect ComputeInkOverflowForGapsOptimized(
      WritingDirectionMode writing_direction,
      const PhysicalSize& container_size,
      LayoutUnit inline_thickness,
      LayoutUnit block_thickness) const;

  ContainerType GetContainerType() const { return container_type_; }

  void SetInlineGapSize(LayoutUnit size) { inline_gap_size_ = size; }
  LayoutUnit GetInlineGapSize() const { return inline_gap_size_; }

  void SetBlockGapSize(LayoutUnit size) { block_gap_size_ = size; }
  LayoutUnit GetBlockGapSize() const { return block_gap_size_; }

  String IntersectionsToString(GridTrackSizingDirection track_direction,
                               bool verbose = false) const;

  // TODO(crbug.com/436140061): These methods are being used to implement the
  // optimized version of GapDecorations. Once the optimized version is
  // implemented, we can remove all the other unused methods from the old
  // version.
  // See third_party/blink/renderer/core/layout/gap/README.md for more
  // information.

  void SetContentInlineOffsets(LayoutUnit start_offset, LayoutUnit end_offset);
  LayoutUnit GetContentInlineStart() const { return content_inline_start_; }
  LayoutUnit GetContentInlineEnd() const { return content_inline_end_; }

  void SetContentBlockOffsets(LayoutUnit start_offset, LayoutUnit end_offset);
  LayoutUnit GetContentBlockStart() const { return content_block_start_; }
  LayoutUnit GetContentBlockEnd() const { return content_block_end_; }

  void SetMainGaps(Vector<MainGap>&& main_gaps) {
    CHECK(!main_gaps.empty());
    main_gaps_ = std::move(main_gaps);
    main_gap_running_index_ = 0;
  }

  void SetCrossGaps(Vector<CrossGap>&& cross_gaps) {
    CHECK(!cross_gaps.empty());
    cross_gaps_ = std::move(cross_gaps);
  }

  void SetMainDirection(GridTrackSizingDirection direction) {
    main_direction_ = direction;
  }

  GridTrackSizingDirection GetMainDirection() const { return main_direction_; }

  bool IsMainDirection(GridTrackSizingDirection direction) const {
    return main_direction_ == direction;
  }

  void SetRowGapsToBlockedColumnRanges(
      GapToTrackRangesMap&& row_gaps_to_blocked_column_ranges) {
    row_gaps_to_blocked_column_ranges_ =
        std::move(row_gaps_to_blocked_column_ranges);
  }

  void SetColumnGapsToBlockedRowRanges(
      GapToTrackRangesMap&& column_gaps_to_blocked_row_ranges) {
    column_gaps_to_blocked_row_ranges_ =
        std::move(column_gaps_to_blocked_row_ranges);
  }

  const Vector<MainGap>& GetMainGaps() const { return main_gaps_; }

  const Vector<CrossGap>& GetCrossGaps() const { return cross_gaps_; }

  const GapToTrackRangesMap& GetRowGapsToBlockedColumnRanges() const {
    return row_gaps_to_blocked_column_ranges_;
  }

  const GapToTrackRangesMap& GetColumnGapsToBlockedRowRanges() const {
    return column_gaps_to_blocked_row_ranges_;
  }

  // Returns the offset of the gap at the specified `gap_index` in the given
  // `direction` (main or cross axis). For the main axis, it returns the offset
  // directly. For the cross axis, it returns either the inline or block offset
  // depending on the direction (columns or rows).
  LayoutUnit GetGapOffset(GridTrackSizingDirection direction,
                          wtf_size_t gap_index) const;

  // Gap Decorations are painted relative to intersection points within a gap.
  // This methods returns a Vector of ordered intersection offsets for the gap
  // at `gap_index`. The general pattern is: container content-start ->
  // MainxCross intersections -> container content-end. The middle intersections
  // depend on the container type and direction.
  Vector<LayoutUnit> GenerateIntersectionListForGap(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index) const;

  // Determines whether the intersection at `intersection_index` within
  // `gap_index` lies on a container edge. Typically, the first and last
  // intersections are edges, but for flex cross gaps, we must first check if
  // the gap itself is an edge gap before deciding whether the first or last
  // intersection is an edge intersection.
  bool IsEdgeIntersection(wtf_size_t gap_index,
                          wtf_size_t intersection_index,
                          wtf_size_t intersection_count,
                          bool is_main_gap) const;

  // Determines if a given track at `cross_index` is covered for gap at
  // `main_index`. For the given `track_direction`, this function looks up any
  // spanners associated with the gap at `main_index`. If no spanners exist, the
  // track is uncovered. Otherwise, it determines if `cross_index` falls within
  // any of the gap spanner ranges, indicating that the track is covered by a
  // spanning item.
  bool IsTrackCovered(GridTrackSizingDirection track_direction,
                      wtf_size_t main_index,
                      wtf_size_t cross_index) const;

  // Determines the blocked status of a specific intersection within a grid.
  // `primary_index` represents the gap index along the track direction and
  // `secondary_index` identifies the specific intersection within that gap.
  BlockedStatus GetIntersectionBlockedStatus(
      GridTrackSizingDirection track_direction,
      wtf_size_t primary_index,
      wtf_size_t secondary_index) const;

  blink::String ToString(bool verbose = false) const;

  const Vector<wtf_size_t>& GetSpannerMainGapsIndices() const {
    return spanner_main_gaps_indices_;
  }
  void SetSpannerMainGapsIndices(Vector<wtf_size_t>&& indices) {
    spanner_main_gaps_indices_ = std::move(indices);
  }

  bool IsMultiColSpanner(wtf_size_t gap_index,
                         GridTrackSizingDirection direction = kForRows) const;

 private:
  // Returns a list of intersection offsets for a main gap at `gap_index`. This
  // list includes:
  // - container content start
  // - Intersections with cross gaps (container-specific)
  // - container content end.
  // All offsets are in increasing order along `direction`.
  Vector<LayoutUnit> GenerateMainIntersectionList(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index) const;

  // Returns a list of intersection offsets for a cross gap. For grid
  // containers, this includes the container content edges and every main gap
  // offset. For flex containers, it includes the cross-gap start offset and its
  // computed end offset.
  Vector<LayoutUnit> GenerateCrossIntersectionList(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index) const;

  // Computes the end offset for a flex cross gap at `cross_gap_index`. The end
  // offset is either:
  // - The container's content end which occurs when the cross gap is at last
  // line, or
  // - The offset of the main gap where this cross gap ends (tracked by
  // `main_gap_running_index_`) which occurs when the cross gap occurs on any
  // line but the last.
  LayoutUnit ComputeEndOffsetForFlexCrossGap(wtf_size_t cross_gap_index,
                                             GridTrackSizingDirection direction,
                                             bool cross_gap_is_at_end) const;

  // TODO(samomekarajr): Potential optimization. This can be a single
  // Vector<GapIntersection> if we exclude intersection points at the edge of
  // the container. We can check the "blocked" status of edge intersection
  // points to determine if we should draw from edge of the container to that
  // intersection.
  Vector<GapIntersectionList> column_intersections_;
  Vector<GapIntersectionList> row_intersections_;

  // In flex it refers to the gap between flex items, and in grid it
  // refers to the column gutter size.
  LayoutUnit inline_gap_size_;
  // In flex it refers to the gap between flex lines, and in grid it
  // refers to the row gutter size.
  LayoutUnit block_gap_size_;

  ContainerType container_type_;

  // TODO(crbug.com/436140061): These properties are being used to implement the
  // optimized version of GapDecorations. Once the optimized version is
  // implemented, we can remove all the other unused properties from the old
  // version.
  // See third_party/blink/renderer/core/layout/gap/README.md for more
  // information.

  // In order to correctly get the start and end intersections for a `CrossGap`,
  // (i.e. stopping at spanners), we use two indices (`main_gap_running_index_`
  // and `next_spanner_main_gap_index_`), to help us in doing this in constant
  // time (for most cases). `next_spanner_main_gap_index_` is an index of
  // `spanner_main_gaps_indices_`, which in turn is a vector of indices for
  // spanner main gaps in `main_gaps_`, and it is used to track the next spanner
  // main gap (the end offset/intersection of the cross gap we want to paint).
  // `main_gap_running_index_` is an index of `main_gaps_`, and we use
  // `spanner_main_gap_indices_` along with `next_spanner_main_gap_index_` to
  // move `main_gap_running_index_` forward as we progress.
  //
  // After we are done painting a cross gap that goes from one spanner to the
  // next (or to the end of the container), we advance the indices to point
  // towards the next spanner main gap that we will paint up until.
  void AdvanceMulticolRunningIndices(bool& should_add_content_end) const;

  MainGaps main_gaps_;
  CrossGaps cross_gaps_;

  // These represent the offsets of the content where the gaps begin and end.
  // We use separate LayoutUnits instead of LogicalOffsets, since these are more
  // like "ranges" rather than points since we care about how "long" the content
  // is in each direction, not the exact coordinate of where it starts.
  LayoutUnit content_inline_start_;
  LayoutUnit content_inline_end_;
  LayoutUnit content_block_start_;
  LayoutUnit content_block_end_;

  // Maintains the portions of each gap that are blocked by spanning items. A
  // gap normally represents a continuous range of intersections (e.g. tracks
  // 1–N), but a spanning item may block part of that range, resulting in one or
  // more sub-ranges.
  GapToTrackRangesMap row_gaps_to_blocked_column_ranges_;
  GapToTrackRangesMap column_gaps_to_blocked_row_ranges_;

  // TODO(samomekarajr): Consider making this type a display agnostic type that
  // uses inline/block rather than rows/columns.
  GridTrackSizingDirection main_direction_ = kForRows;

  // In flex, cross gaps (except those at the last flex line) terminate
  // at a main gap. Main gaps already track their adjacent cross gaps (before
  // and after). The `main_gap_running_index_` tracks which main gap a sequence
  // of cross gaps belongs to. This allows us to determine the correct end
  // offset for cross gaps in flex.
  //
  // This is made be mutable because GapGeometry is treated as const during
  // Paint, but `ComputeEndOffsetForFlexCrossGap()` (called at paint time)
  // updates this index as part of its calculation. Making this mutable allows
  // us to maintain necessary state without breaking const-correctness for the
  // overall GapGeometry object.
  //
  // TODO(samomekarajr): Explore removing this in favour of having this state
  // live at the parent paint call and passing in as an input/output param.
  mutable wtf_size_t main_gap_running_index_ = kNotFound;
  // For multicol, for a given cross gap, we need to track the index the next
  // spanner main gap, since this will be the end of that cross gap. This is a
  // running index (in `spanner_main_gaps_indices_`) that gets updated as we
  // progress, using the `spanner_main_gaps_indices_`.
  //
  // This must be mutable for the same reasons that `main_gap_running_index_` is
  // mutable.
  mutable wtf_size_t next_spanner_main_gap_index_ = 0;
  // These are the indices (in `main_gaps_`) of the main gaps that are spanners.
  // Only used for multicol.
  //
  // This must be mutable for the same reasons that `main_gap_running_index_` is
  // mutable.
  mutable Vector<wtf_size_t> spanner_main_gaps_indices_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GAP_GEOMETRY_H_
