// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/compositor_animation_curve.h"

#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/typed_interpolation_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"

namespace blink {

bool CompositorAnimationCurve::PopulateKeyframes(Animation* animation,
                                                 ValueFilter value_filter) {
  const KeyframeEffect* effect = To<KeyframeEffect>(animation->effect());
  const KeyframeEffectModelBase* model = effect->Model();
  Element* element = effect->EffectTarget();
  const PropertySpecificKeyframeVector* frames =
      model->GetPropertySpecificKeyframes(PropertyHandle(PropertyName()));
  for (const auto& frame : *frames) {
    double offset = frame->Offset();
    const TimingFunction& timing_function = frame->Easing();
    if (frame->IsCSSPropertySpecificKeyframe()) {
      const CSSValue* css_value =
          To<CSSPropertySpecificKeyframe>(frame.Get())->Value();
      const CSSValue* computed_value = StyleResolver::ComputeValue(
          effect->EffectTarget(), property_name_, *css_value);
      // TODO(crbug.com/41491098): Conditionally store the css_value as well as
      // the resolved value if style dependent (e.g. contains var substitution).
      // This allows the snapshot update for the curve to be handled outside of
      // CSSAniamtions::CalculateCompositorAnimationUpdate, which is called too
      // late in the pipeline.
      if (!value_filter(element, computed_value, nullptr)) {
        return false;
      }
      AddKeyframe(offset, timing_function, computed_value);
    } else {
      DCHECK(frame->IsTransitionPropertySpecificKeyframe());
      const auto* transition_keyframe =
          To<TransitionKeyframe::PropertySpecificKeyframe>(frame.Get());
      const TypedInterpolationValue* interpolation_value =
          transition_keyframe->GetValue();
      if (!value_filter(element, nullptr, interpolation_value)) {
        return false;
      }
      AddKeyframe(offset, timing_function, interpolation_value);
    }
  }
  return true;
}

wtf_size_t CompositorAnimationCurve::ComputeKeyframeIndex(double progress) {
  double result = 0;
  wtf_size_t limit = Size() - 2;
  double min_progress = GetBaseKeyframe(0).offset;
  double max_progress = GetBaseKeyframe(limit + 1).offset;
  if (progress >= max_progress) {
    result = limit;
  } else if (progress > min_progress && limit > 0) {
    // Most common case is that we are in the same keyframe interval as our
    // last call.
    result = last_index_;
    double offset = GetBaseKeyframe(result).offset;
    if (offset <= progress) {
      // Current keyframe interval or animating in the forwards direction.
      double next_offset = GetBaseKeyframe(result + 1).offset;
      while (next_offset <= progress) {
        next_offset = GetBaseKeyframe(++result + 1).offset;
        if (next_offset > progress) {
          break;
        }
      }
    } else {
      // Animating in the backwards direction.
      while (offset > progress) {
        offset = GetBaseKeyframe(--result).offset;
      }
    }
  }
  last_index_ = result;
  return result;
}

double CompositorAnimationCurve::ComputeKeyframeIntervalProgress(
    wtf_size_t index,
    double progress) {
  double start = GetBaseKeyframe(index).offset;
  double end = GetBaseKeyframe(index + 1).offset;
  double local_progress = (progress - start) / (end - start);

  // TODO(crbug.com/347958668): Fix limit direction to account for phase and
  // direction. Important for making the correct decision at the boundary when
  // using a step timing function. Currently blocked on lack of support for a
  // start delay.
  double transformed_progress =
      GetBaseKeyframe(index).timing_function
          ? GetBaseKeyframe(index).timing_function->GetValue(
                local_progress, TimingFunction::LimitDirection::RIGHT)
          : local_progress;

  return transformed_progress;
}

}  // end namespace blink
