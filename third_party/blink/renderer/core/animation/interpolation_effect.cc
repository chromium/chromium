// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolation_effect.h"

namespace blink {

void InterpolationEffect::GetActiveInterpolations(
    double fraction,
    HeapVector<Member<Interpolation>>& result) const {
  wtf_size_t existing_size = result.size();
  wtf_size_t result_index = 0;

  for (const auto& record : interpolations_) {
    if (fraction >= record->apply_from_ && fraction < record->apply_to_) {
      Interpolation* interpolation = record->interpolation_;
      double record_length = record->end_ - record->start_;
      double local_fraction =
          record_length ? (fraction - record->start_) / record_length : 0.0;
      if (record->easing_) {
        local_fraction = record->easing_->Evaluate(
            local_fraction, TimingFunction::LimitDirection::RIGHT);
      }
      interpolation->Interpolate(0, local_fraction);
      if (result_index < existing_size)
        result[result_index++] = interpolation;
      else
        result.push_back(interpolation);
    }
  }
  if (result_index < existing_size)
    result.Shrink(result_index);
}

void InterpolationEffect::AddInterpolationsFromKeyframes(
    const PropertyHandle& property,
    const Keyframe::PropertySpecificKeyframe& keyframe_a,
    const Keyframe::PropertySpecificKeyframe& keyframe_b,
    double apply_from,
    double apply_to) {
  AddInterpolation(keyframe_a.CreateInterpolation(property, keyframe_b),
                   &keyframe_a.Easing(), keyframe_a.Offset(),
                   keyframe_b.Offset(), apply_from, apply_to);
}

void InterpolationEffect::Trace(Visitor* visitor) {
  visitor->Trace(interpolations_);
}

}  // namespace blink
