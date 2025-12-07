// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_GAP_DATA_LIST_INTERPOLATION_FUNCTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_GAP_DATA_LIST_INTERPOLATION_FUNCTIONS_H_

#include <memory>

#include "third_party/blink/renderer/core/animation/interpolation_value.h"

namespace blink {

class CORE_EXPORT GapDataListInterpolationFunctions {
  STACK_ALLOCATED();

 public:
  // Used to determine if we will use kLowestCommonMultiple or kEqual when
  // interpolating lists. When the gap data list is simple, we can use the
  // lowest common multiple. When there is an auto repeater, we segment the list
  // into three segments:
  // - `leading_values`, the list of values before the auto repeater (after
  // expanding any integer repeaters)
  // - `auto_repeater`, the auto repeater itself
  // - `trailing_values`, the list of values after the auto repeater (after
  // expanding any integer repeaters)
  enum class GapDataListPattern { kSimple, kSegmented };

  struct GapSegmentsData {
    // Without an auto repeater, the pattern is kSimple.
    GapDataListPattern pattern = GapDataListPattern::kSimple;

    // When kSimple, everything is in `leading` and no auto exists.
    wtf_size_t leading_count = 0;
    wtf_size_t trailing_count = 0;
  };

  static GapSegmentsData CreateGapSegmentsData(const InterpolableList& list);

  // Validate both sides share the same segment shape:
  // - both have or both don't have auto
  // - same number of leading items (if we have auto)
  // - same number of trailing items (if we have auto)
  static bool GapSegmentShapesMatch(const GapSegmentsData& a,
                                    const GapSegmentsData& b);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_GAP_DATA_LIST_INTERPOLATION_FUNCTIONS_H_
