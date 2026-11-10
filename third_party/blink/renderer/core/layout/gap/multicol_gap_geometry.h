// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_MULTICOL_GAP_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_MULTICOL_GAP_GEOMETRY_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CORE_EXPORT MulticolGapGeometry final : public GapGeometry {
 public:
  MulticolGapGeometry() : GapGeometry(ContainerType::kMultiColumn) {}

 private:
  // Multicol has no subclass fields.
  bool HasEqualSubclassFields(const GapGeometry& other) const override {
    return true;
  }

  // Fills `intersections` for a multicol main gap. Columns align, so every main
  // gap intersects every cross gap, and `gap_index` is not used. The base class
  // skips spanner main gaps before it calls this method.
  void GenerateMainIntersectionList(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      RuleVisibilityItems rule_visibility,
      GapSegmentStateCursor& cursor,
      Vector<GapIntersection>& intersections) const override {
    GenerateMainIntersectionListForGridAndMulticol(direction, cursor,
                                                   intersections);
  }

  // Fills `intersections` for a multicol cross gap at `gap_index`, which
  // includes:
  // 1. The start block offset of the cross gap.
  // 2. The offset of any main gaps that intersect this cross gap.
  // 3. The content-end edge.
  // Multicol cross gaps have no owner, so `cross_gap_owner_index` must be
  // `std::nullopt`.
  void GenerateCrossIntersectionList(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      std::optional<wtf_size_t> cross_gap_owner_index,
      GapSegmentStateCursor& cursor,
      Vector<GapIntersection>& intersections) const override;
};

template <>
struct DowncastTraits<MulticolGapGeometry> {
  static bool AllowFrom(const GapGeometry& gap_geometry) {
    return gap_geometry.GetContainerType() ==
           GapGeometry::ContainerType::kMultiColumn;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_MULTICOL_GAP_GEOMETRY_H_
