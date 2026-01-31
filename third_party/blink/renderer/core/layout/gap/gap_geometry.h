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

class ComputedStyle;

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

using MainGaps = Vector<MainGap>;
using CrossGaps = Vector<CrossGap>;

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
              MainGaps&& new_main_gaps,
              LayoutUnit new_content_block_start,
              LayoutUnit new_content_block_end)
      : inline_gap_size_(other.inline_gap_size_),
        block_gap_size_(other.block_gap_size_),
        container_type_(other.container_type_),
        main_gaps_(std::move(new_main_gaps)),
        cross_gaps_(other.cross_gaps_),
        content_inline_start_(other.content_inline_start_),
        content_inline_end_(other.content_inline_end_),
        content_block_start_(new_content_block_start),
        content_block_end_(new_content_block_end),
        main_direction_(other.main_direction_),
        main_gap_running_index_(other.main_gap_running_index_) {}

  void Trace(Visitor* visitor) const {}

  // Computes the physical bounding rect for gap decorations ink overflow.
  PhysicalRect ComputeInkOverflowForGaps(WritingDirectionMode writing_direction,
                                         const PhysicalSize& container_size,
                                         LayoutUnit inline_thickness,
                                         LayoutUnit block_thickness) const;

  ContainerType GetContainerType() const { return container_type_; }

  void SetInlineGapSize(LayoutUnit size) { inline_gap_size_ = size; }
  LayoutUnit GetInlineGapSize() const { return inline_gap_size_; }

  void SetBlockGapSize(LayoutUnit size) { block_gap_size_ = size; }
  LayoutUnit GetBlockGapSize() const { return block_gap_size_; }


  void SetContentInlineOffsets(LayoutUnit start_offset, LayoutUnit end_offset);
  LayoutUnit GetContentInlineStart() const { return content_inline_start_; }
  LayoutUnit GetContentInlineEnd() const { return content_inline_end_; }

  void SetContentBlockOffsets(LayoutUnit start_offset, LayoutUnit end_offset);
  LayoutUnit GetContentBlockStart() const { return content_block_start_; }
  LayoutUnit GetContentBlockEnd() const { return content_block_end_; }

  void SetMainGaps(Vector<MainGap>&& main_gaps) {
    CHECK(!main_gaps.empty());
    main_gaps_ = std::move(main_gaps);

    // The `main_gap_running_index_` should be the first main_gap index that has
    // cross gaps before it.
    main_gap_running_index_ = 0;
    while (main_gap_running_index_ < main_gaps_.size() &&
           !main_gaps_[main_gap_running_index_].HasCrossGapsBefore()) {
      ++main_gap_running_index_;
    }
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

  const Vector<MainGap>& GetMainGaps() const { return main_gaps_; }

  const Vector<CrossGap>& GetCrossGaps() const { return cross_gaps_; }

  // Returns the center offset of the gap at the specified `gap_index` in the
  // given `direction` (main or cross axis). For the main axis, it returns the
  // offset directly. For the cross axis, it returns either the inline or block
  // offset depending on the direction (columns or rows).
  LayoutUnit GetGapCenterOffset(GridTrackSizingDirection direction,
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
                          bool is_main_gap,
                          const Vector<LayoutUnit>& intersections) const;

  // Returns the `GapSegmentState` for the intersection at `secondary_index`
  // within the gap at `primary_index` in the `track_direction`.
  GapSegmentState GetIntersectionGapSegmentState(
      GridTrackSizingDirection track_direction,
      wtf_size_t primary_index,
      wtf_size_t secondary_index) const;

  // Determines if a given track at `secondary_index` is covered for gap at
  // `primary_index`. For the given `track_direction`, this function looks up
  // any spanners associated with the gap at `primary_index`. If no spanners
  // exist, the track is uncovered. Otherwise, it determines if
  // `secondary_index` falls within any of the gap spanner ranges, indicating
  // that the track is covered by a spanning item.
  bool IsTrackCovered(GridTrackSizingDirection track_direction,
                      wtf_size_t primary_index,
                      wtf_size_t secondary_index) const;

  // Adjusts ranges of cross gaps for this GapGeometry such that they are
  // fragmentation-aware and relative to the current fragment.
  // `last_track_in_previous_fragment` points to the last track that has been
  // fully processed in the previous fragment. `first_track_in_next_fragment`
  // points to the first track that has not been fully processed in the current
  // fragment. If a track starts in the current fragment but continues to
  // subsequent fragments, it is considered "unprocessed".
  void AdjustCrossGapsRangesForFragmentation(
      wtf_size_t last_track_in_previous_fragment,
      wtf_size_t first_track_in_next_fragment,
      Vector<wtf_size_t>& column_gaps_segment_ranges_start_indices);

  // Determines the blocked status of a specific intersection within a grid.
  // `primary_index` represents the gap index along the track direction and
  // `secondary_index` identifies the specific intersection within that gap.
  BlockedStatus GetIntersectionBlockedStatus(
      GridTrackSizingDirection track_direction,
      wtf_size_t primary_index,
      wtf_size_t secondary_index,
      const Vector<LayoutUnit>& intersections) const;

  blink::String ToString(bool verbose = false) const;

  bool IsMultiColSpanner(wtf_size_t gap_index,
                         GridTrackSizingDirection direction = kForRows) const;

  LayoutUnit ComputeInsetEnd(const ComputedStyle& style,
                             wtf_size_t gap_index,
                             wtf_size_t intersection_index,
                             const Vector<LayoutUnit>& intersections,
                             bool is_column_gap,
                             bool is_main,
                             LayoutUnit cross_width) const;

  LayoutUnit ComputeInsetStart(const ComputedStyle& style,
                               wtf_size_t gap_index,
                               wtf_size_t intersection_index,
                               const Vector<LayoutUnit>& intersections,
                               bool is_column_gap,
                               bool is_main,
                               LayoutUnit cross_width) const;

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

  // Fills `intersections` for a flex main gap at `gap_index`, which includes:
  // 1. Cross gaps that appear before the main gap
  // 2. Cross gaps that appear after the main gap
  void GenerateMainIntersectionListForFlex(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      Vector<LayoutUnit>& intersections) const;

  // Returns a list of intersection offsets for a cross gap. For grid
  // containers, this includes the container content edges and every main gap
  // offset. For flex containers, it includes the cross-gap start offset and its
  // computed end offset.
  Vector<LayoutUnit> GenerateCrossIntersectionList(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index) const;

  // Fills `intersections` for a grid cross gap at `gap_index`, which includes:
  // 1. The content-start edge
  // 2. The start offset of every main gap
  // 3. The content-end edge
  void GenerateCrossIntersectionListForGrid(
      GridTrackSizingDirection direction,
      Vector<LayoutUnit>& intersections) const;

  // Fills `intersections` for a flex cross gap at `gap_index`, which includes:
  // 1. The gap's start offset
  // 2. Its computed end offset (either a main gap or the container's
  // content-end edge)
  void GenerateCrossIntersectionListForFlex(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      Vector<LayoutUnit>& intersections) const;

  // Fills `intersections` for a multicol cross gap at `gap_index`, which includes:
  // 1. The start block offset of the cross gap.
  // 2. The offset of any main gaps that intersect this cross gap.
  void GenerateCrossIntersectionListForMulticol(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      Vector<LayoutUnit>& intersections) const;

  // Computes the end offset for a flex or multicol cross gap at
  // `cross_gap_index`. The end offset is either:
  // - The container's content end which occurs when the cross gap is at last
  // line, or
  // - The offset of the main gap where this cross gap ends (tracked by
  // `main_gap_running_index_`) which occurs when the cross gap occurs on any
  // line but the last.
  LayoutUnit ComputeEndOffsetForFlexOrMulticolCrossGap(
      wtf_size_t cross_gap_index,
      GridTrackSizingDirection direction,
      bool cross_gap_is_at_end) const;

  // In multicol, the intersections of a given `CrossGap` will be spanner
  // adjacent if and only if there are 3 intersections in the gap, and we are at
  // the middle intersection. This is because all multicol `CrossGap` will have
  // only 2 intersections, except if they are adjacent to a spanner, in which
  // case they will have 3 intersections: One at the start of the gap, one at
  // the start of the spanner, and one at the end of the spanner. The middle
  // intersection is the one that is spanner adjacent.
  bool MulticolCrossGapIntersectionsEndAtSpanner(
      wtf_size_t intersection_index,
      const Vector<LayoutUnit>& intersections) const;

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
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GAP_GEOMETRY_H_
