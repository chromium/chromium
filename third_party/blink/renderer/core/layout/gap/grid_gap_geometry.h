// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GRID_GAP_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GRID_GAP_GEOMETRY_H_

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
};

template <>
struct DowncastTraits<GridGapGeometry> {
  static bool AllowFrom(const GapGeometry& gap_geometry) {
    return gap_geometry.GetContainerType() == GapGeometry::ContainerType::kGrid;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GRID_GAP_GEOMETRY_H_
