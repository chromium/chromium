// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PRE_PAINT_SUBTREE_WALK_REASONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PRE_PAINT_SUBTREE_WALK_REASONS_H_

#include "base/containers/enum_set.h"

namespace blink {

// PrePaintTreeWalk needs to walk the whole subtree of a LayoutObject on which a
// certain flag changed.
enum class PrePaintSubtreeWalkReason {
  kMinValue,
  kEffectiveAllowedTouchAction = kMinValue,
  kBlockingWheelEventHandler,
  kSoftNavigationContext,
  kContainerTimingContext,
  kMaxValue = kContainerTimingContext,
};

inline constexpr unsigned kPrePaintSubtreeWalkReasonBits =
    static_cast<unsigned>(PrePaintSubtreeWalkReason::kMaxValue) + 1;

using PrePaintSubtreeWalkReasons = base::EnumSet<PrePaintSubtreeWalkReason>;

// Returns reasons that apply across frame boundaries.
inline PrePaintSubtreeWalkReasons CrossFramePrePaintSubtreeWalkReasons(
    PrePaintSubtreeWalkReasons reasons) {
  return base::Intersection(
      reasons, {PrePaintSubtreeWalkReason::kEffectiveAllowedTouchAction,
                PrePaintSubtreeWalkReason::kBlockingWheelEventHandler});
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PRE_PAINT_SUBTREE_WALK_REASONS_H_
