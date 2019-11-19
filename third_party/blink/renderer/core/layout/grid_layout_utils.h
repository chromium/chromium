// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LAYOUT_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LAYOUT_UTILS_H_

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

enum GridAxis { kGridRowAxis, kGridColumnAxis };

class LayoutGrid;

class GridLayoutUtils {
  STATIC_ONLY(GridLayoutUtils);

 public:
  static LayoutUnit MarginLogicalWidthForChild(const LayoutGrid&,
                                               const LayoutBox&);
  static LayoutUnit MarginLogicalHeightForChild(const LayoutGrid&,
                                                const LayoutBox&);
  static bool IsOrthogonalChild(const LayoutGrid&, const LayoutBox&);
  static GridTrackSizingDirection FlowAwareDirectionForChild(
      const LayoutGrid&,
      const LayoutBox&,
      GridTrackSizingDirection);
  static bool HasOverrideContainingBlockContentSizeForChild(
      const LayoutBox&,
      GridTrackSizingDirection);
  static LayoutUnit OverrideContainingBlockContentSizeForChild(
      const LayoutBox&,
      GridTrackSizingDirection);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LAYOUT_UTILS_H_
