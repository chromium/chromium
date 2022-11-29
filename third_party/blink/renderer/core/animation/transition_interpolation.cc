// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/transition_interpolation.h"

#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_value.h"

namespace blink {

void TransitionInterpolation::Interpolate(int iteration, double fraction) {
  if (!cached_fraction_ || *cached_fraction_ != fraction ||
      cached_iteration_ != iteration) {
    merge_.start_interpolable_value->Interpolate(
        *merge_.end_interpolable_value, fraction, *cached_interpolable_value_);
    cached_iteration_ = iteration;
    cached_fraction_.emplace(fraction);
  }
}

const InterpolableValue& TransitionInterpolation::CurrentInterpolableValue()
    const {
  return *cached_interpolable_value_;
}

const NonInterpolableValue*
TransitionInterpolation::CurrentNonInterpolableValue() const {
  return merge_.non_interpolable_value.get();
}

void TransitionInterpolation::Apply(
    InterpolationEnvironment& environment) const {
  type_.Apply(CurrentInterpolableValue(), CurrentNonInterpolableValue(),
              environment);
}

std::unique_ptr<TypedInterpolationValue>
TransitionInterpolation::GetInterpolatedValue() const {
  return std::make_unique<TypedInterpolationValue>(
      type_, CurrentInterpolableValue().Clone(), CurrentNonInterpolableValue());
}

}  // namespace blink
