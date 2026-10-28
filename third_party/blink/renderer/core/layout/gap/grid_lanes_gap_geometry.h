// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GRID_LANES_GAP_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GRID_LANES_GAP_GEOMETRY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CORE_EXPORT GridLanesGapGeometry final : public GapGeometry {
 public:
  GridLanesGapGeometry() : GapGeometry(ContainerType::kGridLanes) {}
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
