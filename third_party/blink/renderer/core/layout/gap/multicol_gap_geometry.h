// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_MULTICOL_GAP_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_MULTICOL_GAP_GEOMETRY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CORE_EXPORT MulticolGapGeometry final : public GapGeometry {
 public:
  MulticolGapGeometry() : GapGeometry(ContainerType::kMultiColumn) {}

 private:
  // TODO(javiercon): Remove this friend declaration when the methods that
  // `GapGeometry` calls can be made virtual.
  friend class GapGeometry;

  // Fills `intersections` for a multicol cross gap at `gap_index`, which
  // includes:
  // 1. The start block offset of the cross gap.
  // 2. The offset of any main gaps that intersect this cross gap.
  void GenerateCrossIntersectionListForMulticol(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      Vector<GapIntersection>& intersections,
      GapSegmentStateCursor& cursor) const;
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
