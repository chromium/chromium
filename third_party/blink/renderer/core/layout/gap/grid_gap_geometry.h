// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GRID_GAP_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GRID_GAP_GEOMETRY_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CORE_EXPORT GridGapGeometry final : public GapGeometry {
 public:
  GridGapGeometry() : GapGeometry(ContainerType::kGrid) {}

  // Creates the gap geometry for one fragment of a fragmented grid, from the
  // gap geometry of the full grid.
  GridGapGeometry(const GridGapGeometry& other,
                  MainGaps&& new_main_gaps,
                  LayoutUnit new_content_block_start,
                  LayoutUnit new_content_block_end)
      : GapGeometry(other,
                    std::move(new_main_gaps),
                    new_content_block_start,
                    new_content_block_end) {}

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

 private:
  // Grid has no subclass fields.
  bool HasEqualSubclassFields(const GapGeometry& other) const override {
    return true;
  }

  // Fills `intersections` for a grid main gap. Grid tracks align, so every main
  // gap intersects every cross gap, and `gap_index` is not used.
  void GenerateMainIntersectionList(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      RuleVisibilityItems rule_visibility,
      GapSegmentStateCursor& cursor,
      Vector<GapIntersection>& intersections) const override {
    GenerateMainIntersectionListForGridAndMulticol(direction, cursor,
                                                   intersections);
  }

  // Fills `intersections` for a grid cross gap, which includes:
  // 1. The content-start edge
  // 2. The start offset of every main gap
  // 3. The content-end edge
  // Grid tracks align, so every cross gap intersects every main gap, and
  // `gap_index` is not used. Grid cross gaps have no owner, so
  // `cross_gap_owner_index` must be `std::nullopt`.
  void GenerateCrossIntersectionList(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      std::optional<wtf_size_t> cross_gap_owner_index,
      GapSegmentStateCursor& cursor,
      Vector<GapIntersection>& intersections) const override;
};

template <>
struct DowncastTraits<GridGapGeometry> {
  static bool AllowFrom(const GapGeometry& gap_geometry) {
    return gap_geometry.GetContainerType() == GapGeometry::ContainerType::kGrid;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GRID_GAP_GEOMETRY_H_
