// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/scroll_offset_range.h"

namespace blink {

namespace {

std::optional<LayoutUnit> Negate(const std::optional<LayoutUnit>& bound) {
  return bound ? std::optional<LayoutUnit>(-bound.value()) : std::nullopt;
}

}  // namespace

PhysicalScrollRange LogicalScrollRange::SlowToPhysical(
    WritingDirectionMode mode) const {
  switch (mode.GetWritingMode()) {
    case WritingMode::kHorizontalTb:
      DCHECK(!mode.IsLtr());  // LTR is in the fast code path.
      return PhysicalScrollRange{Negate(inline_max), Negate(inline_min),
                                 block_min, block_max};
    case WritingMode::kVerticalRl:
      if (mode.IsLtr()) {
        return PhysicalScrollRange{Negate(block_max), Negate(block_min),
                                   inline_min, inline_max};
      }
      return PhysicalScrollRange{Negate(block_max), Negate(block_min),
                                 Negate(inline_max), Negate(inline_min)};
    case WritingMode::kVerticalLr:
      if (mode.IsLtr()) {
        return PhysicalScrollRange{block_min, block_max, inline_min,
                                   inline_max};
      }
      return PhysicalScrollRange{block_min, block_max, Negate(inline_max),
                                 Negate(inline_min)};
    case WritingMode::kSidewaysLr:
    case WritingMode::kSidewaysRl:
      // Blink doesn't support these writing modes yet.
      NOTREACHED_IN_MIGRATION();
      return PhysicalScrollRange();
  }
}

}  // namespace blink
