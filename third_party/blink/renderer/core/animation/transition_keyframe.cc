// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/transition_keyframe.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/animation/animation_input_helpers.h"
#include "third_party/blink/renderer/core/animation/animation_utils.h"
#include "third_party/blink/renderer/core/animation/compositor_animations.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_types_map.h"
#include "third_party/blink/renderer/core/animation/interpolation_type.h"
#include "third_party/blink/renderer/core/animation/pairwise_interpolation_value.h"
#include "third_party/blink/renderer/core/animation/transition_interpolation.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

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
    V8ObjectBuilder& object_builder,
    Element* element) const {
  Keyframe::AddKeyframePropertiesToV8Object(object_builder, element);

  // TODO(crbug.com/933761): Fix resolution of the style in the case where the
  // target element has been removed.
  if (!element)
    return;

  Document& document = element->GetDocument();
  StyleResolverState state(document, *element);
  state.SetStyle(document.GetStyleResolver().InitialStyle());
  CSSInterpolationTypesMap map(document.GetPropertyRegistry(), document);
  CSSInterpolationEnvironment environment(map, state);
  value_->GetType().Apply(value_->GetInterpolableValue(),
                          value_->GetNonInterpolableValue(), environment);

  const ComputedStyle* style = state.TakeStyle();
  String property_value =
      AnimationUtils::KeyframeValueFromComputedStyle(
          property_, *style, document, element->GetLayoutObject())
          ->CssText();

  String property_name =
      AnimationInputHelpers::PropertyHandleToKeyframeAttribute(property_);
  object_builder.AddString(property_name, property_value);
}

void TransitionKeyframe::Trace(Visitor* visitor) const {
  visitor->Trace(value_);
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
  const auto& other = To<TransitionPropertySpecificKeyframe>(other_super_class);
  DCHECK(value_->GetType() == other.value_->GetType());
  return MakeGarbageCollected<TransitionInterpolation>(
      property, value_->GetType(), value_->Value().Clone(),
      other.value_->Value().Clone(), compositor_value_,
      other.compositor_value_);
}

void TransitionKeyframe::PropertySpecificKeyframe::Trace(
    Visitor* visitor) const {
  visitor->Trace(value_);
  visitor->Trace(compositor_value_);
  Keyframe::PropertySpecificKeyframe::Trace(visitor);
}

}  // namespace blink
