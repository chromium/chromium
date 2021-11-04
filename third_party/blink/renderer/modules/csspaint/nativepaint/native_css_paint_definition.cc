// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/native_css_paint_definition.h"

#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

NativeCssPaintDefinition::NativeCssPaintDefinition(
    LocalFrame* local_root,
    PaintWorkletInput::PaintWorkletInputType type)
    : NativePaintDefinition(local_root, type) {}

bool NativeCssPaintDefinition::CanGetValueFromKeyframe(
    const PropertySpecificKeyframe* frame,
    const KeyframeEffectModelBase* model) {
  if (model->IsStringKeyframeEffectModel()) {
    DCHECK(frame->IsCSSPropertySpecificKeyframe());
    const CSSValue* value = To<CSSPropertySpecificKeyframe>(frame)->Value();
    if (!value)
      return false;
  } else {
    DCHECK(frame->IsTransitionPropertySpecificKeyframe());
    const TransitionKeyframe::PropertySpecificKeyframe* keyframe =
        To<TransitionKeyframe::PropertySpecificKeyframe>(frame);
    InterpolableValue* value =
        keyframe->GetValue()->Value().interpolable_value.get();
    if (!value)
      return false;
  }
  return true;
}

Animation* NativeCssPaintDefinition::GetAnimationForProperty(
    const Element* element,
    const CSSProperty& property) {
  if (!element->GetElementAnimations())
    return nullptr;
  Animation* compositable_animation = nullptr;
  // We'd composite only if it is the only animation of its type on
  // this element.
  unsigned count = 0;
  for (const auto& animation : element->GetElementAnimations()->Animations()) {
    if (animation.key->CalculateAnimationPlayState() == Animation::kIdle ||
        !animation.key->Affects(*element, property))
      continue;
    count++;
    compositable_animation = animation.key;
  }
  if (!compositable_animation || count > 1)
    return nullptr;

  // If we are here, this element must have one animation of the CSSProperty
  // type only. Fall back to the main thread if it is not composite:replace.
  const AnimationEffect* effect = compositable_animation->effect();
  DCHECK(effect->IsKeyframeEffect());
  const KeyframeEffectModelBase* model =
      static_cast<const KeyframeEffect*>(effect)->Model();
  if (model->AffectedByUnderlyingAnimations())
    return nullptr;
  const PropertySpecificKeyframeVector* frames =
      model->GetPropertySpecificKeyframes(PropertyHandle(property));
  DCHECK_GE(frames->size(), 2u);
  for (const auto& frame : *frames) {
    if (!CanGetValueFromKeyframe(frame, model)) {
      return nullptr;
    }
  }
  return compositable_animation;
}

}  // namespace blink
