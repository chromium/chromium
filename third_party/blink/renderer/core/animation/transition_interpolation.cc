// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/transition_interpolation.h"
#include <memory>

#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_value.h"

namespace blink {

void TransitionInterpolation::Interpolate(int iteration, double fraction) {
  if (!cached_fraction_ || *cached_fraction_ != fraction ||
      cached_iteration_ != iteration) {
    if (merge_) {
      merge_.start_interpolable_value->Interpolate(
          *merge_.end_interpolable_value, fraction,
          *cached_interpolable_value_);
    }
    cached_iteration_ = iteration;
    cached_fraction_.emplace(fraction);
  }
}

const InterpolableValue& TransitionInterpolation::CurrentInterpolableValue()
    const {
  if (merge_) {
    return *cached_interpolable_value_;
  }
  return cached_fraction_ < 0.5 ? *start_.interpolable_value
                                : *end_.interpolable_value;
}

const NonInterpolableValue*
TransitionInterpolation::CurrentNonInterpolableValue() const {
  if (merge_) {
    return merge_.non_interpolable_value.get();
  }
  return cached_fraction_ < 0.5 ? start_.non_interpolable_value.get()
                                : end_.non_interpolable_value.get();
}

void TransitionInterpolation::Apply(
    InterpolationEnvironment& environment) const {
  type_.Apply(CurrentInterpolableValue(), CurrentNonInterpolableValue(),
              environment);
}

TypedInterpolationValue* TransitionInterpolation::GetInterpolatedValue() const {
  return MakeGarbageCollected<TypedInterpolationValue>(
      type_, CurrentInterpolableValue().Clone(), CurrentNonInterpolableValue());
}

}  // namespace blink
