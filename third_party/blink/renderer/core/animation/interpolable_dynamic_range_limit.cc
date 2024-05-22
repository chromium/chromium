// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_dynamic_range_limit.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"

namespace blink {

InterpolableDynamicRangeLimit::InterpolableDynamicRangeLimit(
    DynamicRangeLimit dynamic_range_limit)
    : dynamic_range_limit_(dynamic_range_limit) {}

// static
InterpolableDynamicRangeLimit* InterpolableDynamicRangeLimit::Create(
    DynamicRangeLimit dynamic_range_limit) {
  return MakeGarbageCollected<InterpolableDynamicRangeLimit>(
      dynamic_range_limit);
}

DynamicRangeLimit InterpolableDynamicRangeLimit::GetDynamicRangeLimit() const {
  return dynamic_range_limit_;
}

InterpolableDynamicRangeLimit* InterpolableDynamicRangeLimit::RawClone() const {
  return MakeGarbageCollected<InterpolableDynamicRangeLimit>(
      dynamic_range_limit_);
}

InterpolableDynamicRangeLimit* InterpolableDynamicRangeLimit::RawCloneAndZero()
    const {
  return MakeGarbageCollected<InterpolableDynamicRangeLimit>(
      DynamicRangeLimit(cc::PaintFlags::DynamicRangeLimit::kHigh));
}

bool InterpolableDynamicRangeLimit::Equals(
    const InterpolableValue& other) const {
  const InterpolableDynamicRangeLimit& other_palette =
      To<InterpolableDynamicRangeLimit>(other);
  return dynamic_range_limit_ == other_palette.dynamic_range_limit_;
}

void InterpolableDynamicRangeLimit::AssertCanInterpolateWith(
    const InterpolableValue& other) const {}

void InterpolableDynamicRangeLimit::Interpolate(
    const InterpolableValue& to,
    const double progress,
    InterpolableValue& result) const {
  const InterpolableDynamicRangeLimit& to_limit =
      To<InterpolableDynamicRangeLimit>(to);
  InterpolableDynamicRangeLimit& result_limit =
      To<InterpolableDynamicRangeLimit>(result);

  // Percentages are required to be in the range 0% to 100% for
  // dynamic-range-limit-mix().
  double normalized_progress = ClampTo<double>(progress, 0.0, 1.0);

  if (normalized_progress == 0 ||
      dynamic_range_limit_ == to_limit.dynamic_range_limit_) {
    result_limit.dynamic_range_limit_ = dynamic_range_limit_;
  } else if (normalized_progress == 1) {
    result_limit.dynamic_range_limit_ = to_limit.dynamic_range_limit_;
  } else {
    result_limit.dynamic_range_limit_.standard_mix =
        (1 - progress) * dynamic_range_limit_.standard_mix +
        progress * to_limit.dynamic_range_limit_.standard_mix;
    result_limit.dynamic_range_limit_.constrained_high_mix =
        (1 - progress) * dynamic_range_limit_.constrained_high_mix +
        progress * to_limit.dynamic_range_limit_.constrained_high_mix;
  }
}

}  // namespace blink
