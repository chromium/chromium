// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NON_OVERFLOWING_SCROLL_RANGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NON_OVERFLOWING_SCROLL_RANGE_H_

#include "third_party/blink/renderer/core/layout/geometry/scroll_offset_range.h"

namespace blink {

// Helper structure for CSS anchor positioning's fallback positioning. Each
// fallback position has a corresponding `NonOverflowingScrollRange`. See
// https://drafts.csswg.org/css-anchor-position-1/#fallback-apply
struct NonOverflowingScrollRange {
  DISALLOW_NEW();

  // The range of the snapshotted scroll offset within which this fallback
  // position's margin box doesn't overflow the scroll-adjusted inset-modified
  // containing block rect.
  PhysicalScrollRange containing_block_range;

  // This range is set only if `position-fallback-bounds` is not `normal`,
  // in which case it's the range for *the difference* between
  // A. The snapshotted scroll offset, which is the offset applied to the margin
  //    box, and
  // B. The scroll offset applied to the additional fallback-bounds rect, if any
  // So that when (A - B) is in this range, this fallback position's margin box
  // doesn't overflow the additional fallback-bounds rect.
  PhysicalScrollRange additional_bounds_range;

  // Checks if the given scroll offsets are within the scroll ranges, i.e., if
  // the fallback position's margin box overflows the bounds.
  bool Contains(const gfx::Vector2dF& anchor_scroll_offset,
                const gfx::Vector2dF& additional_bounds_scroll_offset) const {
    return containing_block_range.Contains(anchor_scroll_offset) &&
           additional_bounds_range.Contains(anchor_scroll_offset -
                                            additional_bounds_scroll_offset);
  }

  bool operator==(const NonOverflowingScrollRange& other) const {
    return containing_block_range == other.containing_block_range &&
           additional_bounds_range == other.additional_bounds_range;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NON_OVERFLOWING_SCROLL_RANGE_H_
