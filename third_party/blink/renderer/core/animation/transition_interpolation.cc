// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/transition_interpolation.h"

#include "third_party/blink/renderer/core/animation/css_interpolation_types_map.h"
#include "third_party/blink/renderer/core/animation/interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/interpolation_type.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"

namespace blink {

void TransitionInterpolation::Interpolate(int iteration, double fraction) {
  if (cached_fraction_ != fraction || cached_iteration_ != iteration) {
    if (fraction != 0 && fraction != 1) {
      merge_.start_interpolable_value->Interpolate(
          *merge_.end_interpolable_value, fraction,
          *cached_interpolable_value_);
    }
    cached_iteration_ = iteration;
    cached_fraction_ = fraction;
  }
}

const InterpolableValue& TransitionInterpolation::CurrentInterpolableValue()
    const {
  if (cached_fraction_ == 0) {
    return *start_.interpolable_value;
  }
  if (cached_fraction_ == 1) {
    return *end_.interpolable_value;
  }
  return *cached_interpolable_value_;
}

const NonInterpolableValue*
TransitionInterpolation::CurrentNonInterpolableValue() const {
  if (cached_fraction_ == 0) {
    return start_.non_interpolable_value.get();
  }
  if (cached_fraction_ == 1) {
    return end_.non_interpolable_value.get();
  }
  return merge_.non_interpolable_value.get();
}

void TransitionInterpolation::Apply(StyleResolverState& state) const {
  CSSInterpolationTypesMap map(state.GetDocument().GetPropertyRegistry(),
                               state.GetDocument());
  CSSInterpolationEnvironment environment(map, state, nullptr);
  type_.Apply(CurrentInterpolableValue(), CurrentNonInterpolableValue(),
              environment);
}

std::unique_ptr<TypedInterpolationValue>
TransitionInterpolation::GetInterpolatedValue() const {
  return std::make_unique<TypedInterpolationValue>(
      type_, CurrentInterpolableValue().Clone(), CurrentNonInterpolableValue());
}

}  // namespace blink
