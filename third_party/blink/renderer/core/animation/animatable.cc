// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animatable.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_get_animations_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_keyframe_animation_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range_offset.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_keyframeanimationoptions_unrestricteddouble.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_keyframeeffectoptions_unrestricteddouble.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_timelinerangeoffset.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/effect_input.h"
#include "third_party/blink/renderer/core/animation/effect_model.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/animation/timing_input.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/css_to_style_map.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {
namespace {

V8UnionKeyframeEffectOptionsOrUnrestrictedDouble* CoerceEffectOptions(
    const V8UnionKeyframeAnimationOptionsOrUnrestrictedDouble* options) {
  switch (options->GetContentType()) {
    case V8UnionKeyframeAnimationOptionsOrUnrestrictedDouble::ContentType::
        kKeyframeAnimationOptions:
      return MakeGarbageCollected<
          V8UnionKeyframeEffectOptionsOrUnrestrictedDouble>(
          options->GetAsKeyframeAnimationOptions());
    case V8UnionKeyframeAnimationOptionsOrUnrestrictedDouble::ContentType::
        kUnrestrictedDouble:
      return MakeGarbageCollected<
          V8UnionKeyframeEffectOptionsOrUnrestrictedDouble>(
          options->GetAsUnrestrictedDouble());
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

}  // namespace

// https://w3.org/TR/web-animations-1/#dom-animatable-animate
Animation* Animatable::animate(
    ScriptState* script_state,
    const ScriptValue& keyframes,
    const V8UnionKeyframeAnimationOptionsOrUnrestrictedDouble* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid())
    return nullptr;
  Element* element = GetAnimationTarget();
  if (!element->GetExecutionContext())
    return nullptr;
  KeyframeEffect* effect =
      KeyframeEffect::Create(script_state, element, keyframes,
                             CoerceEffectOptions(options), exception_state);
  if (exception_state.HadException())
    return nullptr;

  // Creation of the keyframe effect parses JavaScript, which could result
  // in destruction of the execution context. Recheck that it is still valid.
  if (!element->GetExecutionContext())
    return nullptr;

  if (!options->IsKeyframeAnimationOptions())
    return element->GetDocument().Timeline().Play(effect, exception_state);

  Animation* animation;
  const KeyframeAnimationOptions* options_dict =
      options->GetAsKeyframeAnimationOptions();
  if (!options_dict->hasTimeline()) {
    animation = element->GetDocument().Timeline().Play(effect, exception_state);
  } else if (AnimationTimeline* timeline = options_dict->timeline()) {
    animation = timeline->Play(effect, exception_state);
  } else {
    animation = Animation::Create(element->GetExecutionContext(), effect,
                                  nullptr, exception_state);
  }

  if (!animation)
    return nullptr;

  animation->setId(options_dict->id());

  // ViewTimeline options.
  if (options_dict->hasRangeStart() &&
      RuntimeEnabledFeatures::ScrollTimelineEnabled()) {
    animation->SetRangeStartInternal(TimelineOffset::Create(
        element, options_dict->rangeStart(), 0, exception_state));
  }
  if (options_dict->hasRangeEnd() &&
      RuntimeEnabledFeatures::ScrollTimelineEnabled()) {
    animation->SetRangeEndInternal(TimelineOffset::Create(
        element, options_dict->rangeEnd(), 100, exception_state));
  }
  return animation;
}

// https://w3.org/TR/web-animations-1/#dom-animatable-animate
Animation* Animatable::animate(ScriptState* script_state,
                               const ScriptValue& keyframes,
                               ExceptionState& exception_state) {
  if (!script_state->ContextIsValid())
    return nullptr;
  Element* element = GetAnimationTarget();
  if (!element->GetExecutionContext())
    return nullptr;
  KeyframeEffect* effect =
      KeyframeEffect::Create(script_state, element, keyframes, exception_state);
  if (exception_state.HadException())
    return nullptr;

  // Creation of the keyframe effect parses JavaScript, which could result
  // in destruction of the execution context. Recheck that it is still valid.
  if (!element->GetExecutionContext())
    return nullptr;

  return element->GetDocument().Timeline().Play(effect, exception_state);
}

// https://w3.org/TR/web-animations-1/#dom-animatable-getanimations
HeapVector<Member<Animation>> Animatable::getAnimations(
    GetAnimationsOptions* options) {
  bool use_subtree = options && options->subtree();
  return GetAnimationsInternal(
      GetAnimationsOptionsResolved{.use_subtree = use_subtree});
}

HeapVector<Member<Animation>> Animatable::GetAnimationsInternal(
    GetAnimationsOptionsResolved options) {
  Element* element = GetAnimationTarget();
  if (options.use_subtree) {
    element->GetDocument().UpdateStyleAndLayoutTreeForSubtree(
        element, DocumentUpdateReason::kWebAnimation);
  } else {
    element->GetDocument().UpdateStyleAndLayoutTreeForElement(
        element, DocumentUpdateReason::kWebAnimation);
  }

  HeapVector<Member<Animation>> animations;
  if (!options.use_subtree && !element->HasAnimations())
    return animations;

  for (const auto& animation :
       element->GetDocument().GetDocumentAnimations().getAnimations(
           element->GetTreeScope())) {
    DCHECK(animation->effect());
    // TODO(gtsteel) make this use the idl properties
    Element* target = To<KeyframeEffect>(animation->effect())->EffectTarget();
    if (element == target ||
        (options.use_subtree && element->contains(target))) {
      // DocumentAnimations::getAnimations should only give us animations that
      // are either current or in effect.
      DCHECK(animation->effect()->IsCurrent() ||
             animation->effect()->IsInEffect());
      animations.push_back(animation);
    }
  }
  return animations;
}

}  // namespace blink
