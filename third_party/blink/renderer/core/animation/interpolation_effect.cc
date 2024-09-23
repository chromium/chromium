// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolation_effect.h"

namespace blink {

void InterpolationEffect::GetActiveInterpolations(
    double fraction,
    TimingFunction::LimitDirection limit_direction,
    HeapVector<Member<Interpolation>>& result) const {
  wtf_size_t existing_size = result.size();
  wtf_size_t result_index = 0;

  for (const auto& record : interpolations_) {
    Interpolation* interpolation = nullptr;
    if (record->is_static_) {
      // The local fraction is irrelevant since the result is constant valued.
      // The first sample will cache a value, which will be reused in
      // subsequent calls as long as the cache is not invalidated.
      interpolation = record->interpolation_;
      interpolation->Interpolate(0, 0);
    } else {
      if (fraction >= record->apply_from_ && fraction < record->apply_to_) {
        // TODO(kevers): There is room to expand the optimization to allow a
        // non-static property to have static records in the event of keyframe
        // pairs with identical values. We could then skip the local fraction
        // calculation and simply sample at 0. For this, we would still need
        // records for each keyframe pair.
        interpolation = record->interpolation_;
        double record_length = record->end_ - record->start_;
        double local_fraction =
            record_length ? (fraction - record->start_) / record_length : 0.0;
        if (record->easing_) {
          local_fraction =
              record->easing_->Evaluate(local_fraction, limit_direction);
        }
        interpolation->Interpolate(0, local_fraction);
      }
    }
    if (interpolation) {
      if (result_index < existing_size) {
        result[result_index++] = interpolation;
      } else {
        result.push_back(interpolation);
      }
    }
  }
  if (result_index < existing_size) {
    result.Shrink(result_index);
  }
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

void InterpolationEffect::AddStaticValuedInterpolation(
    const PropertyHandle& property,
    const Keyframe::PropertySpecificKeyframe& keyframe) {
  interpolations_.push_back(MakeGarbageCollected<InterpolationRecord>(
      keyframe.CreateInterpolation(property, keyframe)));
}

void InterpolationEffect::Trace(Visitor* visitor) const {
  visitor->Trace(interpolations_);
}

}  // namespace blink
