// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/transition_keyframe.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/animation/compositor_animations.h"
#include "third_party/blink/renderer/core/animation/interpolation_type.h"
#include "third_party/blink/renderer/core/animation/pairwise_interpolation_value.h"
#include "third_party/blink/renderer/core/animation/transition_interpolation.h"

namespace blink {

void TransitionKeyframe::SetCompositorValue(
    CompositorKeyframeValue* compositor_value) {
  DCHECK_EQ(property_.GetCSSProperty().IsCompositableProperty(),
            static_cast<bool>(compositor_value));
  compositor_value_ = compositor_value;
}

PropertyHandleSet TransitionKeyframe::Properties() const {
  PropertyHandleSet result;
  result.insert(property_);
  return result;
}

void TransitionKeyframe::AddKeyframePropertiesToV8Object(
    V8ObjectBuilder& object_builder) const {
  Keyframe::AddKeyframePropertiesToV8Object(object_builder);
  // TODO(crbug.com/777971): Add in the property/value for TransitionKeyframe.
}

void TransitionKeyframe::Trace(Visitor* visitor) {
  visitor->Trace(compositor_value_);
  Keyframe::Trace(visitor);
}

Keyframe::PropertySpecificKeyframe*
TransitionKeyframe::CreatePropertySpecificKeyframe(
    const PropertyHandle& property,
    EffectModel::CompositeOperation effect_composite,
    double offset) const {
  DCHECK(property == property_);
  DCHECK(offset == offset_);
  EffectModel::CompositeOperation composite =
      composite_.value_or(effect_composite);
  return MakeGarbageCollected<PropertySpecificKeyframe>(
      CheckedOffset(), &Easing(), composite, value_->Clone(),
      compositor_value_);
}

Interpolation*
TransitionKeyframe::PropertySpecificKeyframe::CreateInterpolation(
    const PropertyHandle& property,
    const Keyframe::PropertySpecificKeyframe& other_super_class) const {
  const PropertySpecificKeyframe& other =
      ToTransitionPropertySpecificKeyframe(other_super_class);
  DCHECK(value_->GetType() == other.value_->GetType());
  return TransitionInterpolation::Create(
      property, value_->GetType(), value_->Value().Clone(),
      other.value_->Value().Clone(), compositor_value_,
      other.compositor_value_);
}

void TransitionKeyframe::PropertySpecificKeyframe::Trace(Visitor* visitor) {
  visitor->Trace(compositor_value_);
  Keyframe::PropertySpecificKeyframe::Trace(visitor);
}

}  // namespace blink
