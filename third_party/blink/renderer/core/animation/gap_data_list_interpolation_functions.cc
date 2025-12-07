// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/gap_data_list_interpolation_functions.h"

#include "third_party/blink/renderer/core/animation/interpolable_gap_data_auto_repeater.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/list_interpolation_functions.h"

namespace blink {

GapDataListInterpolationFunctions::GapSegmentsData
GapDataListInterpolationFunctions::CreateGapSegmentsData(
    const InterpolableList& list) {
  GapDataListInterpolationFunctions::GapSegmentsData segments;

  wtf_size_t auto_index = kNotFound;

  // First we loop through a list to identify the index of where we have an auto
  // node.
  for (wtf_size_t i = 0; i < list.length(); ++i) {
    if (DynamicTo<InterpolableGapLengthAutoRepeater>(list.Get(i))) {
      auto_index = i;
      segments.pattern =
          GapDataListInterpolationFunctions::GapDataListPattern::kSegmented;
      break;
    }
  }

  segments.leading_count = auto_index == kNotFound ? list.length() : auto_index;

  if (auto_index != kNotFound) {
    segments.trailing_count = list.length() - (auto_index + 1);
  }

  return segments;
}

bool GapDataListInterpolationFunctions::GapSegmentShapesMatch(
    const GapDataListInterpolationFunctions::GapSegmentsData& a,
    const GapDataListInterpolationFunctions::GapSegmentsData& b) {
  // If one side has an auto repeater and the other doesn't, we don't
  // interpolate.
  if (a.pattern != b.pattern) {
    return false;
  }

  // When the pattern is `kSimple` it means we don't have an auto repeater, so
  // we'll just do LowestCommonMultiple interpolation.
  if (a.pattern == GapDataListPattern::kSimple) {
    return true;
  }

  return a.trailing_count == b.trailing_count &&
         a.leading_count == b.leading_count;
}

}  // namespace blink
