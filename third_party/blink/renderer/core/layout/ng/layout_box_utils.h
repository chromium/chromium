// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_BOX_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_BOX_UTILS_H_

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class LayoutBox;
class LayoutBlock;
class LayoutPoint;
class NGBlockBreakToken;
class NGPhysicalBoxFragment;
struct PhysicalOffset;

// This static class should be used for querying information from a |LayoutBox|,
// or providing information to it.
class LayoutBoxUtils {
  STATIC_ONLY(LayoutBoxUtils);

 public:
  // Returns the available logical width/height for |box| accounting for:
  //  - Orthogonal writing modes.
  //  - Any containing block override sizes set.
  static LayoutUnit AvailableLogicalHeight(const LayoutBox& box,
                                           const LayoutBlock* cb);

  static bool SkipContainingBlockForPercentHeightCalculation(
      const LayoutBlock* cb);

  static LayoutUnit InlineSize(const LayoutBox& box);

  // The total block size of all fragments.
  static LayoutUnit TotalBlockSize(const LayoutBox& box);

  // Convert a physical offset for a physical fragment to a physical legacy
  // LayoutPoint, to be used in LayoutBox. There are special considerations for
  // vertical-rl writing-mode, and also for block fragmentation (the
  // block-offset should include consumed space in previous fragments).
  static LayoutPoint ComputeLocation(
      const NGPhysicalBoxFragment& child_fragment,
      PhysicalOffset offset,
      const NGPhysicalBoxFragment& container_fragment,
      const NGBlockBreakToken* previous_container_break_token);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_BOX_UTILS_H_
