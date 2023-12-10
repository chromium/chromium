// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BOX_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BOX_UTILS_H_

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class BlockBreakToken;
class LayoutBox;
class LayoutBlock;
class LayoutPoint;
class PhysicalBoxFragment;
struct PhysicalOffset;

// This static class should be used for querying information from a |LayoutBox|,
// or providing information to it.
class LayoutBoxUtils {
  STATIC_ONLY(LayoutBoxUtils);

 public:
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
      const PhysicalBoxFragment& child_fragment,
      PhysicalOffset offset,
      const PhysicalBoxFragment& container_fragment,
      const BlockBreakToken* previous_container_break_token);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BOX_UTILS_H_
