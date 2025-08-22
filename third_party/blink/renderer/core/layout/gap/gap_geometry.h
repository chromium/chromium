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

  WTF::String ToString(bool verbose = false) const;

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

using TrackRanges = std::unique_ptr<Vector<TrackRange>>;

// Represents a mapping from gap indices to the ranges of tracks blocked within
// those gaps. For example, a gap with index 0 might map to a list of track
// ranges such as {[2, 4], 7, 9]}, indicating that tracks 2 through 4 and 7
// through 9 are blocked in gap[0].
using GapToTrackRangesMap =
    WTF::HashMap<wtf_size_t, TrackRanges, blink::IntWithZeroKeyHashTraits<int>>;

// Gap locations are used for painting gap decorations.
class CORE_EXPORT GapGeometry : public GarbageCollected<GapGeometry> {
 public:
  enum ContainerType {
    kGrid,
    kFlex,
    kMultiColumn,
  };

  explicit GapGeometry(ContainerType container_type)
      : container_type_(container_type) {}

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

  ContainerType GetContainerType() const { return container_type_; }

  void SetInlineGapSize(LayoutUnit size) { inline_gap_size_ = size; }
  LayoutUnit GetInlineGapSize() const { return inline_gap_size_; }

  void SetBlockGapSize(LayoutUnit size) { block_gap_size_ = size; }
  LayoutUnit GetBlockGapSize() const { return block_gap_size_; }

  WTF::String IntersectionsToString(GridTrackSizingDirection track_direction,
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
    main_gaps_ = std::move(main_gaps);
  }

  void SetCrossGaps(Vector<CrossGap>&& cross_gaps) {
    cross_gaps_ = std::move(cross_gaps);
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

  blink::String ToString(bool verbose = false) const;

 private:
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
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GAP_GEOMETRY_H_
