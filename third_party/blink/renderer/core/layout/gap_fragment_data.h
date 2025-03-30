// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_FRAGMENT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_FRAGMENT_DATA_H_

#include "third_party/blink/renderer/core/style/grid_enums.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
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

  LayoutUnit inline_offset;
  LayoutUnit block_offset;

  // Represents whether the intersection point is blocked before or after due to
  // the presence of a spanning item. For flex, this is used to represent
  // whether the intersection point is "blocked" by the edge of the container.
  bool is_blocked_before = false;
  bool is_blocked_after = false;
};

using GapIntersectionList = Vector<GapIntersection>;

// Gap locations are used for painting gap decorations.
struct GapGeometry : public GarbageCollected<GapGeometry> {
  enum ContainerType {
    kGrid,
    kFlex,
  };

 public:
  explicit GapGeometry(ContainerType container_type)
      : container_type_(container_type) {}

  void Trace(Visitor* visitor) const {}

  void SetGapIntersections(GridTrackSizingDirection track_direction,
                           Vector<GapIntersectionList>&& intersection_list) {
    track_direction == kForColumns ? column_intersections_ = intersection_list
                                   : row_intersections_ = intersection_list;
  }

  // Marks the intersection point at [main_index][inner_index] in the specified
  // `track_direction` (kColumns or kRows) as blocked in the given
  // `blocked_direction` (`kBefore` or `kAfter`). This is necessary to avoid
  // painting gap decorations behind spanners when authors set the
  // `*-rule-break` property to 'spanning-item' or `intersection`.
  void MarkGapIntersectionBlocked(GridTrackSizingDirection track_direction,
                                  BlockedGapDirection blocked_direction,
                                  wtf_size_t main_index,
                                  wtf_size_t inner_index) {
    auto& intersections = track_direction == kForColumns ? column_intersections_
                                                         : row_intersections_;

    blocked_direction == BlockedGapDirection::kBefore
        ? intersections[main_index][inner_index].is_blocked_before = true
        : intersections[main_index][inner_index].is_blocked_after = true;
  }

  const Vector<GapIntersectionList>& GetGapIntersections(
      GridTrackSizingDirection track_direction) const {
    return track_direction == kForColumns ? column_intersections_
                                          : row_intersections_;
  }

  ContainerType GetContainerType() const { return container_type_; }

  void SetInlineGapSize(LayoutUnit size) { inline_gap_size_ = size; }
  LayoutUnit GetInlineGapSize() const { return inline_gap_size_; }

  void SetBlockGapSize(LayoutUnit size) { block_gap_size_ = size; }
  LayoutUnit GetBlockGapSize() const { return block_gap_size_; }

  bool IntersectionIncludesContentEdge(
      const wtf_size_t intersection_index,
      wtf_size_t num_intersections,
      const GapIntersection& intersection) const {
    // `GapIntersection` objects for flex mark intersections as blocked before
    // and after if they border a content edge.
    return container_type_ == ContainerType::kFlex
               ? (intersection.is_blocked_before ||
                  intersection.is_blocked_after)
               : (intersection_index == 0 ||
                  intersection_index == num_intersections - 1);
  }

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
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_FRAGMENT_DATA_H_
