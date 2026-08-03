// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/compositor_animation_curve.h"

#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/typed_interpolation_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"

namespace blink {

namespace {

bool IsStyleDependent(const CSSValue* css_value) {
  if (!css_value) {
    return false;
  }

  // Check for CSS Custom Properties (e.g. var(), env() references)
  if (css_value->IsUnparsedDeclaration() ||
      css_value->IsPendingSubstitutionValue()) {
    return true;
  }

  // Check computational independence for primitive/numeric values.
  // Returns true if the value has relative lengths (em, rem, ch, lh, cqw, etc.)
  // that depend on the element's font-size or parent container size.
  if (const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(css_value)) {
    if (!primitive_value->IsComputationallyIndependent()) {
      return true;
    }
  }

  // Check for keyword identifiers that depend on style/theme (e.g.,
  // currentcolor, system colors)
  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(css_value)) {
    CSSValueID value_id = identifier_value->GetValueID();
    if (value_id == CSSValueID::kCurrentcolor ||
        StyleColor::IsSystemColorIncludingDeprecated(value_id)) {
      return true;
    }
  }

  return false;
}

}  // end anonymous namespace

bool CompositorAnimationCurve::PopulateKeyframes(Animation* animation,
                                                 ValueFilter value_filter) {
  const KeyframeEffect* effect = To<KeyframeEffect>(animation->effect());
  const KeyframeEffectModelBase* model = effect->Model();
  // The CheckCanStart... call from Animation::PreCommit will force a main
  // thread fallback if the keyframes are not strictly using the 'replace'
  // composite-mode, but since keyframe generation is potentially expensive, it
  // seems prudent to run a quick check here to avoid throwaway work.
  if (model->AffectedByUnderlyingAnimations()) {
    return false;
  }

  Element* element = effect->EffectTarget();
  const PropertySpecificKeyframeVector* frames =
      model->GetPropertySpecificKeyframes(PropertyHandle(PropertyName()));
  for (const auto& frame : *frames) {
    double offset = frame->Offset();
    const TimingFunction& timing_function = frame->Easing();
    if (frame->IsCSSPropertySpecificKeyframe()) {
      const CSSValue* css_value =
          To<CSSPropertySpecificKeyframe>(frame.Get())->Value();
      if (!value_filter(element, css_value, nullptr)) {
        return false;
      }
      if (IsStyleDependent(css_value)) {
        style_dependent_keyframe_indices_.push_back(Size());
      }
      const CSSValue* computed_value = StyleResolver::ComputeValue(
          effect->EffectTarget(), property_name_, *css_value);
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

scoped_refptr<CompositorAnimationCurve>
CompositorAnimationCurve::UpdateKeyframeSnapshot(Animation* animation) {
  if (!HasStyleDependency()) {
    return base::WrapRefCounted(this);
  }

  KeyframeEffect* effect = To<KeyframeEffect>(animation->effect());
  const KeyframeEffectModelBase* model = effect->Model();
  const PropertySpecificKeyframeVector* frames =
      model->GetPropertySpecificKeyframes(PropertyHandle(PropertyName()));
  Element* element = effect->EffectTarget();
  scoped_refptr<CompositorAnimationCurve> maybe_copy = nullptr;
  for (wtf_size_t index : style_dependent_keyframe_indices_) {
    const CSSValue* css_value =
        To<CSSPropertySpecificKeyframe>(frames->at(index).Get())->Value();
    const CSSValue* computed_value =
        StyleResolver::ComputeValue(element, property_name_, *css_value);
    UpdateKeyframe(maybe_copy, index, computed_value);
  }

  if (maybe_copy) {
    return maybe_copy;
  }

  return base::WrapRefCounted(this);
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
