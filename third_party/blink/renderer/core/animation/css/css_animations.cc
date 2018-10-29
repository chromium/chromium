/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/animation/css/css_animations.h"

#include <algorithm>
#include <bitset>
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/compositor_animations.h"
#include "third_party/blink/renderer/core/animation/css/css_animatable_value_factory.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_types_map.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/inert_effect.h"
#include "third_party/blink/renderer/core/animation/interpolation.h"
#include "third_party/blink/renderer/core/animation/interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/interpolation_type.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/transition_interpolation.h"
#include "third_party/blink/renderer/core/animation/worklet_animation_base.h"
#include "third_party/blink/renderer/core/css/css_keyframe_rule.h"
#include "third_party/blink/renderer/core/css/css_property_equality.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/css_to_style_map.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/animation_event.h"
#include "third_party/blink/renderer/core/events/transition_event.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

using PropertySet = HashSet<const CSSProperty*>;

namespace {

StringKeyframeEffectModel* CreateKeyframeEffectModel(
    StyleResolver* resolver,
    const Element* animating_element,
    Element& element,
    const ComputedStyle* style,
    const ComputedStyle* parent_style,
    const AtomicString& name,
    TimingFunction* default_timing_function,
    size_t animation_index) {
  // When the animating element is null, use its parent for scoping purposes.
  const Element* element_for_scoping =
      animating_element ? animating_element : &element;
  const StyleRuleKeyframes* keyframes_rule =
      resolver->FindKeyframesRule(element_for_scoping, name);
  DCHECK(keyframes_rule);

  StringKeyframeVector keyframes;
  const HeapVector<Member<StyleRuleKeyframe>>& style_keyframes =
      keyframes_rule->Keyframes();

  // Construct and populate the style for each keyframe
  PropertySet specified_properties_for_use_counter;
  for (wtf_size_t i = 0; i < style_keyframes.size(); ++i) {
    const StyleRuleKeyframe* style_keyframe = style_keyframes[i].Get();
    StringKeyframe* keyframe = StringKeyframe::Create();
    const Vector<double>& offsets = style_keyframe->Keys();
    DCHECK(!offsets.IsEmpty());
    keyframe->SetOffset(offsets[0]);
    keyframe->SetEasing(default_timing_function);
    const CSSPropertyValueSet& properties = style_keyframe->Properties();
    for (unsigned j = 0; j < properties.PropertyCount(); j++) {
      const CSSProperty& property = properties.PropertyAt(j).Property();
      specified_properties_for_use_counter.insert(&property);
      if (property.PropertyID() == CSSPropertyAnimationTimingFunction) {
        const CSSValue& value = properties.PropertyAt(j).Value();
        scoped_refptr<TimingFunction> timing_function;
        if (value.IsInheritedValue() && parent_style->Animations()) {
          timing_function = parent_style->Animations()->TimingFunctionList()[0];
        } else if (value.IsValueList()) {
          timing_function = CSSToStyleMap::MapAnimationTimingFunction(
              ToCSSValueList(value).Item(0));
        } else {
          DCHECK(value.IsCSSWideKeyword());
          timing_function = CSSTimingData::InitialTimingFunction();
        }
        keyframe->SetEasing(std::move(timing_function));
      } else if (!CSSAnimations::IsAnimationAffectingProperty(property)) {
        keyframe->SetCSSPropertyValue(property,
                                      properties.PropertyAt(j).Value());
      }
    }
    keyframes.push_back(keyframe);
    // The last keyframe specified at a given offset is used.
    for (wtf_size_t j = 1; j < offsets.size(); ++j) {
      keyframes.push_back(
          ToStringKeyframe(keyframe->CloneWithOffset(offsets[j])));
    }
  }

  for (const CSSProperty* property : specified_properties_for_use_counter) {
    DCHECK(isValidCSSPropertyID(property->PropertyID()));
    UseCounter::CountAnimatedCSS(element_for_scoping->GetDocument(),
                                 property->PropertyID());
  }

  // Merge duplicate keyframes.
  std::stable_sort(keyframes.begin(), keyframes.end(),
                   [](const Member<Keyframe>& a, const Member<Keyframe>& b) {
                     return a->CheckedOffset() < b->CheckedOffset();
                   });
  wtf_size_t target_index = 0;
  for (wtf_size_t i = 1; i < keyframes.size(); i++) {
    if (keyframes[i]->CheckedOffset() ==
        keyframes[target_index]->CheckedOffset()) {
      for (const auto& property : keyframes[i]->Properties()) {
        keyframes[target_index]->SetCSSPropertyValue(
            property.GetCSSProperty(),
            keyframes[i]->CssPropertyValue(property));
      }
    } else {
      target_index++;
      keyframes[target_index] = keyframes[i];
    }
  }
  if (!keyframes.IsEmpty())
    keyframes.Shrink(target_index + 1);

  // Add 0% and 100% keyframes if absent.
  StringKeyframe* start_keyframe = keyframes.IsEmpty() ? nullptr : keyframes[0];
  if (!start_keyframe || keyframes[0]->CheckedOffset() != 0) {
    start_keyframe = StringKeyframe::Create();
    start_keyframe->SetOffset(0);
    start_keyframe->SetEasing(default_timing_function);
    keyframes.push_front(start_keyframe);
  }
  StringKeyframe* end_keyframe = keyframes[keyframes.size() - 1];
  if (end_keyframe->CheckedOffset() != 1) {
    end_keyframe = StringKeyframe::Create();
    end_keyframe->SetOffset(1);
    end_keyframe->SetEasing(default_timing_function);
    keyframes.push_back(end_keyframe);
  }
  DCHECK_GE(keyframes.size(), 2U);
  DCHECK_EQ(keyframes.front()->CheckedOffset(), 0);
  DCHECK_EQ(keyframes.back()->CheckedOffset(), 1);

  StringKeyframeEffectModel* model = StringKeyframeEffectModel::Create(
      keyframes, EffectModel::kCompositeReplace, &keyframes[0]->Easing());
  if (animation_index > 0 && model->HasSyntheticKeyframes()) {
    UseCounter::Count(element_for_scoping->GetDocument(),
                      WebFeature::kCSSAnimationsStackedNeutralKeyframe);
  }
  return model;
}

// Sample the given |animation| at the given |inherited_time|. Returns nullptr
// if the |inherited_time| falls outside of the animation.
std::unique_ptr<TypedInterpolationValue> SampleAnimation(
    Animation* animation,
    double inherited_time) {
  KeyframeEffect* effect = ToKeyframeEffect(animation->effect());
  InertEffect* inert_animation_for_sampling = InertEffect::Create(
      effect->Model(), effect->SpecifiedTiming(), false, inherited_time);
  HeapVector<Member<Interpolation>> sample;
  inert_animation_for_sampling->Sample(sample);
  // Transition animation has only animated a single property or is not in
  // effect.
  DCHECK_LE(sample.size(), 1u);
  if (sample.IsEmpty())
    return nullptr;
  return ToTransitionInterpolation(*sample.at(0)).GetInterpolatedValue();
}

}  // namespace

CSSAnimations::CSSAnimations() = default;

bool CSSAnimations::IsAnimationForInspector(const Animation& animation) {
  for (const auto& running_animation : running_animations_) {
    if (running_animation->animation->SequenceNumber() ==
        animation.SequenceNumber())
      return true;
  }
  return false;
}

bool CSSAnimations::IsTransitionAnimationForInspector(
    const Animation& animation) const {
  for (const auto& it : transitions_) {
    if (it.value.animation->SequenceNumber() == animation.SequenceNumber())
      return true;
  }
  return false;
}

namespace {

const KeyframeEffectModelBase* GetKeyframeEffectModelBase(
    const AnimationEffect* effect) {
  if (!effect)
    return nullptr;
  const EffectModel* model = nullptr;
  if (effect->IsKeyframeEffect())
    model = ToKeyframeEffect(effect)->Model();
  else if (effect->IsInertEffect())
    model = ToInertEffect(effect)->Model();
  if (!model || !model->IsKeyframeEffectModel())
    return nullptr;
  return ToKeyframeEffectModelBase(model);
}

}  // namespace

void CSSAnimations::CalculateCompositorAnimationUpdate(
    CSSAnimationUpdate& update,
    const Element* animating_element,
    Element& element,
    const ComputedStyle& style,
    const ComputedStyle* parent_style,
    bool was_viewport_resized) {
  ElementAnimations* element_animations =
      animating_element ? animating_element->GetElementAnimations() : nullptr;

  // If the change in style is only due to the Blink-side animation update, we
  // do not need to update the compositor-side animations. The compositor is
  // already changing the same properties and as such this update would provide
  // no new information.
  if (!element_animations || element_animations->IsAnimationStyleChange())
    return;

  const ComputedStyle* old_style = animating_element->GetComputedStyle();
  if (!old_style || !old_style->ShouldCompositeForCurrentAnimations())
    return;

  bool transform_zoom_changed =
      old_style->HasCurrentTransformAnimation() &&
      old_style->EffectiveZoom() != style.EffectiveZoom();

  const auto& snapshot = [&](AnimationEffect* effect) {
    const KeyframeEffectModelBase* keyframe_effect =
        GetKeyframeEffectModelBase(effect);
    if (!keyframe_effect)
      return false;

    if ((transform_zoom_changed || was_viewport_resized) &&
        (keyframe_effect->Affects(PropertyHandle(GetCSSPropertyTransform())) ||
         keyframe_effect->Affects(PropertyHandle(GetCSSPropertyTranslate()))))
      keyframe_effect->InvalidateCompositorKeyframesSnapshot();

    if (keyframe_effect->SnapshotAllCompositorKeyframesIfNecessary(
            element, style, parent_style)) {
      return true;
    } else if (keyframe_effect->HasSyntheticKeyframes() &&
               keyframe_effect->SnapshotNeutralCompositorKeyframes(
                   element, *old_style, style, parent_style)) {
      return true;
    }
    return false;
  };

  for (auto& entry : element_animations->Animations()) {
    Animation& animation = *entry.key;
    if (snapshot(animation.effect()))
      update.UpdateCompositorKeyframes(&animation);
  }

  for (auto& entry : element_animations->GetWorkletAnimations()) {
    WorkletAnimationBase& animation = *entry;
    if (snapshot(animation.GetEffect()))
      animation.InvalidateCompositingState();
  }
}

void CSSAnimations::CalculateAnimationUpdate(CSSAnimationUpdate& update,
                                             const Element* animating_element,
                                             Element& element,
                                             const ComputedStyle& style,
                                             const ComputedStyle* parent_style,
                                             StyleResolver* resolver) {
  const ElementAnimations* element_animations =
      animating_element ? animating_element->GetElementAnimations() : nullptr;

  bool is_animation_style_change =
      element_animations && element_animations->IsAnimationStyleChange();

#if !DCHECK_IS_ON()
  // If we're in an animation style change, no animations can have started, been
  // cancelled or changed play state. When DCHECK is enabled, we verify this
  // optimization.
  if (is_animation_style_change) {
    CalculateAnimationActiveInterpolations(update, animating_element);
    return;
  }
#endif

  const CSSAnimationData* animation_data = style.Animations();
  const CSSAnimations* css_animations =
      element_animations ? &element_animations->CssAnimations() : nullptr;
  const Element* element_for_scoping =
      animating_element ? animating_element : &element;

  Vector<bool> cancel_running_animation_flags(
      css_animations ? css_animations->running_animations_.size() : 0);
  for (bool& flag : cancel_running_animation_flags)
    flag = true;

  if (animation_data && style.Display() != EDisplay::kNone) {
    const Vector<AtomicString>& name_list = animation_data->NameList();
    for (wtf_size_t i = 0; i < name_list.size(); ++i) {
      AtomicString name = name_list[i];
      if (name == CSSAnimationData::InitialName())
        continue;

      // Find n where this is the nth occurence of this animation name.
      wtf_size_t name_index = 0;
      for (wtf_size_t j = 0; j < i; j++) {
        if (name_list[j] == name)
          name_index++;
      }

      const bool is_paused =
          CSSTimingData::GetRepeated(animation_data->PlayStateList(), i) ==
          EAnimPlayState::kPaused;

      Timing timing = animation_data->ConvertToTiming(i);
      Timing specified_timing = timing;
      scoped_refptr<TimingFunction> keyframe_timing_function =
          timing.timing_function;
      timing.timing_function = Timing::Defaults().timing_function;

      StyleRuleKeyframes* keyframes_rule =
          resolver->FindKeyframesRule(element_for_scoping, name);
      if (!keyframes_rule)
        continue;  // Cancel the animation if there's no style rule for it.

      const RunningAnimation* existing_animation = nullptr;
      wtf_size_t existing_animation_index = 0;

      if (css_animations) {
        for (wtf_size_t i = 0; i < css_animations->running_animations_.size();
             i++) {
          const RunningAnimation& running_animation =
              *css_animations->running_animations_[i];
          if (running_animation.name == name &&
              running_animation.name_index == name_index) {
            existing_animation = &running_animation;
            existing_animation_index = i;
            break;
          }
        }
      }

      if (existing_animation) {
        cancel_running_animation_flags[existing_animation_index] = false;

        Animation* animation = existing_animation->animation.Get();

        const bool was_paused =
            CSSTimingData::GetRepeated(existing_animation->play_state_list,
                                       i) == EAnimPlayState::kPaused;

        if (keyframes_rule != existing_animation->style_rule ||
            keyframes_rule->Version() !=
                existing_animation->style_rule_version ||
            existing_animation->specified_timing != specified_timing ||
            is_paused != was_paused) {
          DCHECK(!is_animation_style_change);
          update.UpdateAnimation(
              existing_animation_index, animation,
              *InertEffect::Create(
                  CreateKeyframeEffectModel(resolver, animating_element,
                                            element, &style, parent_style, name,
                                            keyframe_timing_function.get(), i),
                  timing, is_paused, animation->UnlimitedCurrentTimeInternal()),
              specified_timing, keyframes_rule,
              animation_data->PlayStateList());
          if (is_paused != was_paused)
            update.ToggleAnimationIndexPaused(existing_animation_index);
        }
      } else {
        DCHECK(!is_animation_style_change);
        update.StartAnimation(
            name, name_index,
            *InertEffect::Create(
                CreateKeyframeEffectModel(resolver, animating_element, element,
                                          &style, parent_style, name,
                                          keyframe_timing_function.get(), i),
                timing, is_paused, 0),
            specified_timing, keyframes_rule, animation_data->PlayStateList());
      }
    }
  }

  for (wtf_size_t i = 0; i < cancel_running_animation_flags.size(); i++) {
    if (cancel_running_animation_flags[i]) {
      DCHECK(css_animations && !is_animation_style_change);
      update.CancelAnimation(
          i, *css_animations->running_animations_[i]->animation);
    }
  }

  CalculateAnimationActiveInterpolations(update, animating_element);
}

void CSSAnimations::SnapshotCompositorKeyframes(
    Element& element,
    CSSAnimationUpdate& update,
    const ComputedStyle& style,
    const ComputedStyle* parent_style) {
  const auto& snapshot = [&element, &style,
                          parent_style](const AnimationEffect* effect) {
    const KeyframeEffectModelBase* keyframe_effect =
        GetKeyframeEffectModelBase(effect);
    if (keyframe_effect) {
      keyframe_effect->SnapshotAllCompositorKeyframesIfNecessary(element, style,
                                                                 parent_style);
    }
  };

  ElementAnimations* element_animations = element.GetElementAnimations();
  if (element_animations) {
    for (auto& entry : element_animations->Animations())
      snapshot(entry.key->effect());
  }

  for (const auto& new_animation : update.NewAnimations())
    snapshot(new_animation.effect.Get());

  for (const auto& updated_animation : update.AnimationsWithUpdates())
    snapshot(updated_animation.effect.Get());

  for (const auto& new_transition : update.NewTransitions())
    snapshot(new_transition.value.effect.Get());
}

void CSSAnimations::MaybeApplyPendingUpdate(Element* element) {
  previous_active_interpolations_for_custom_animations_.clear();
  previous_active_interpolations_for_standard_animations_.clear();
  if (pending_update_.IsEmpty())
    return;

  previous_active_interpolations_for_custom_animations_.swap(
      pending_update_.ActiveInterpolationsForCustomAnimations());
  previous_active_interpolations_for_standard_animations_.swap(
      pending_update_.ActiveInterpolationsForStandardAnimations());

  // FIXME: cancelling, pausing, unpausing animations all query
  // compositingState, which is not necessarily up to date here
  // since we call this from recalc style.
  // https://code.google.com/p/chromium/issues/detail?id=339847
  DisableCompositingQueryAsserts disabler;

  for (wtf_size_t paused_index :
       pending_update_.AnimationIndicesWithPauseToggled()) {
    Animation& animation = *running_animations_[paused_index]->animation;
    if (animation.Paused())
      animation.Unpause();
    else
      animation.pause();
    if (animation.Outdated())
      animation.Update(kTimingUpdateOnDemand);
  }

  for (const auto& animation : pending_update_.UpdatedCompositorKeyframes())
    animation->SetCompositorPending(true);

  for (const auto& entry : pending_update_.AnimationsWithUpdates()) {
    if (entry.animation->effect()) {
      KeyframeEffect* effect = ToKeyframeEffect(entry.animation->effect());
      effect->SetModel(entry.effect->Model());
      effect->UpdateSpecifiedTiming(entry.effect->SpecifiedTiming());
    }

    running_animations_[entry.index]->Update(entry);
  }

  const Vector<wtf_size_t>& cancelled_indices =
      pending_update_.CancelledAnimationIndices();
  for (wtf_size_t i = cancelled_indices.size(); i-- > 0;) {
    DCHECK(i == cancelled_indices.size() - 1 ||
           cancelled_indices[i] < cancelled_indices[i + 1]);
    Animation& animation =
        *running_animations_[cancelled_indices[i]]->animation;
    animation.cancel();
    animation.Update(kTimingUpdateOnDemand);
    running_animations_.EraseAt(cancelled_indices[i]);
  }

  for (const auto& entry : pending_update_.NewAnimations()) {
    const InertEffect* inert_animation = entry.effect.Get();
    AnimationEventDelegate* event_delegate =
        new AnimationEventDelegate(element, entry.name);
    KeyframeEffect* effect = KeyframeEffect::Create(
        element, inert_animation->Model(), inert_animation->SpecifiedTiming(),
        KeyframeEffect::kDefaultPriority, event_delegate);
    Animation* animation = element->GetDocument().Timeline().Play(effect);
    animation->setId(entry.name);
    if (inert_animation->Paused())
      animation->pause();
    animation->Update(kTimingUpdateOnDemand);

    running_animations_.push_back(new RunningAnimation(animation, entry));
  }

  // Transitions that are run on the compositor only update main-thread state
  // lazily. However, we need the new state to know what the from state shoud
  // be when transitions are retargeted. Instead of triggering complete style
  // recalculation, we find these cases by searching for new transitions that
  // have matching cancelled animation property IDs on the compositor.
  HashSet<PropertyHandle> retargeted_compositor_transitions;
  for (const PropertyHandle& property :
       pending_update_.CancelledTransitions()) {
    DCHECK(transitions_.Contains(property));

    Animation* animation = transitions_.Take(property).animation;
    KeyframeEffect* effect = ToKeyframeEffect(animation->effect());
    if (effect->HasActiveAnimationsOnCompositor(property) &&
        pending_update_.NewTransitions().find(property) !=
            pending_update_.NewTransitions().end() &&
        !animation->Limited()) {
      retargeted_compositor_transitions.insert(property);
    }
    animation->cancel();
    // after cancelation, transitions must be downgraded or they'll fail
    // to be considered when retriggering themselves. This can happen if
    // the transition is captured through getAnimations then played.
    if (animation->effect() && animation->effect()->IsKeyframeEffect())
      ToKeyframeEffect(animation->effect())->DowngradeToNormal();
    animation->Update(kTimingUpdateOnDemand);
  }

  for (const PropertyHandle& property : pending_update_.FinishedTransitions()) {
    // This transition can also be cancelled and finished at the same time
    if (transitions_.Contains(property)) {
      Animation* animation = transitions_.Take(property).animation;
      // Transition must be downgraded
      if (animation->effect() && animation->effect()->IsKeyframeEffect())
        ToKeyframeEffect(animation->effect())->DowngradeToNormal();
    }
  }

  for (const auto& entry : pending_update_.NewTransitions()) {
    const CSSAnimationUpdate::NewTransition& new_transition = entry.value;

    RunningTransition running_transition;
    running_transition.from = new_transition.from;
    running_transition.to = new_transition.to;
    running_transition.reversing_adjusted_start_value =
        new_transition.reversing_adjusted_start_value;
    running_transition.reversing_shortening_factor =
        new_transition.reversing_shortening_factor;

    const PropertyHandle& property = new_transition.property;
    const InertEffect* inert_animation = new_transition.effect.Get();
    TransitionEventDelegate* event_delegate =
        new TransitionEventDelegate(element, property);

    KeyframeEffectModelBase* model = inert_animation->Model();

    KeyframeEffect* transition = KeyframeEffect::Create(
        element, model, inert_animation->SpecifiedTiming(),
        KeyframeEffect::kTransitionPriority, event_delegate);
    Animation* animation = element->GetDocument().Timeline().Play(transition);
    if (property.IsCSSCustomProperty()) {
      animation->setId(property.CustomPropertyName());
    } else {
      animation->setId(property.GetCSSProperty().GetPropertyName());
    }
    // Set the current time as the start time for retargeted transitions
    if (retargeted_compositor_transitions.Contains(property)) {
      animation->setStartTime(element->GetDocument().Timeline().currentTime(),
                              false);
    }
    animation->Update(kTimingUpdateOnDemand);
    running_transition.animation = animation;
    transitions_.Set(property, running_transition);
    DCHECK(isValidCSSPropertyID(property.GetCSSProperty().PropertyID()));
    UseCounter::CountAnimatedCSS(element->GetDocument(),
                                 property.GetCSSProperty().PropertyID());
  }
  ClearPendingUpdate();
}

void CSSAnimations::CalculateTransitionUpdateForProperty(
    TransitionUpdateState& state,
    const PropertyHandle& property,
    size_t transition_index) {
  state.listed_properties.insert(property);

  // FIXME: We should transition if an !important property changes even when an
  // animation is running, but this is a bit hard to do with the current
  // applyMatchedProperties system.
  if (property.IsCSSCustomProperty()) {
    if (state.update.ActiveInterpolationsForCustomAnimations().Contains(
            property) ||
        (state.animating_element->GetElementAnimations() &&
         state.animating_element->GetElementAnimations()
             ->CssAnimations()
             .previous_active_interpolations_for_custom_animations_.Contains(
                 property))) {
      return;
    }
  } else if (state.update.ActiveInterpolationsForStandardAnimations().Contains(
                 property) ||
             (state.animating_element->GetElementAnimations() &&
              state.animating_element->GetElementAnimations()
                  ->CssAnimations()
                  .previous_active_interpolations_for_standard_animations_
                  .Contains(property))) {
    return;
  }

  const RunningTransition* interrupted_transition = nullptr;
  const RunningTransition* retargeted_compositor_transition = nullptr;
  if (state.active_transitions) {
    TransitionMap::const_iterator active_transition_iter =
        state.active_transitions->find(property);
    if (active_transition_iter != state.active_transitions->end()) {
      const RunningTransition* running_transition =
          &active_transition_iter->value;
      if (CSSPropertyEquality::PropertiesEqual(property, state.style,
                                               *running_transition->to)) {
        return;
      }
      state.update.CancelTransition(property);
      KeyframeEffect* effect =
          ToKeyframeEffect(running_transition->animation->effect());
      if (effect->HasActiveAnimationsOnCompositor())
        retargeted_compositor_transition = running_transition;
      DCHECK(!state.animating_element->GetElementAnimations() ||
             !state.animating_element->GetElementAnimations()
                  ->IsAnimationStyleChange());

      if (CSSPropertyEquality::PropertiesEqual(
              property, state.style,
              *running_transition->reversing_adjusted_start_value)) {
        interrupted_transition = running_transition;
      }
    }
  }

  const PropertyRegistry* registry =
      state.animating_element->GetDocument().GetPropertyRegistry();
  if (property.IsCSSCustomProperty()) {
    if (!registry || !registry->Registration(property.CustomPropertyName())) {
      return;
    }
  }

  if (CSSPropertyEquality::PropertiesEqual(property, state.old_style,
                                           state.style)) {
    return;
  }

  CSSInterpolationTypesMap map(registry,
                               state.animating_element->GetDocument());
  CSSInterpolationEnvironment old_environment(map, state.old_style);
  CSSInterpolationEnvironment new_environment(map, state.style);
  const InterpolationType* transition_type = nullptr;
  InterpolationValue start = nullptr;
  InterpolationValue end = nullptr;
  if (retargeted_compositor_transition) {
    base::Optional<double> old_start_time =
        retargeted_compositor_transition->animation->StartTimeInternal();
    // TODO(flackr): This should be able to just use
    // animation->currentTime() / 1000 rather than trying to calculate current
    // time.
    double inherited_time = old_start_time.has_value()
                                ? state.animating_element->GetDocument()
                                          .Timeline()
                                          .CurrentTimeInternal() -
                                      old_start_time.value()
                                : 0;
    std::unique_ptr<TypedInterpolationValue> retargeted_start = SampleAnimation(
        retargeted_compositor_transition->animation, inherited_time);
    if (retargeted_start) {
      const InterpolationType& interpolation_type = retargeted_start->GetType();
      start = retargeted_start->Value().Clone();
      end = interpolation_type.MaybeConvertUnderlyingValue(new_environment);
      if (end &&
          interpolation_type.MaybeMergeSingles(start.Clone(), end.Clone()))
        transition_type = &interpolation_type;
    } else {
      // If the previous transition was not in effect it is not used for
      // retargeting.
      retargeted_compositor_transition = nullptr;
    }
  }
  if (!retargeted_compositor_transition) {
    for (const auto& interpolation_type : map.Get(property)) {
      start = interpolation_type->MaybeConvertUnderlyingValue(old_environment);
      if (!start) {
        continue;
      }
      end = interpolation_type->MaybeConvertUnderlyingValue(new_environment);
      if (!end) {
        continue;
      }
      // Merge will only succeed if the two values are considered interpolable.
      if (interpolation_type->MaybeMergeSingles(start.Clone(), end.Clone())) {
        transition_type = interpolation_type.get();
        break;
      }
    }
  }

  // No smooth interpolation exists between these values so don't start a
  // transition.
  if (!transition_type) {
    return;
  }

  // If we have multiple transitions on the same property, we will use the
  // last one since we iterate over them in order.

  Timing timing = state.transition_data.ConvertToTiming(transition_index);
  // CSS Transitions always have a valid duration (i.e. the value 'auto' is not
  // supported), so iteration_duration will always be set.
  if (timing.start_delay + timing.iteration_duration->InSecondsF() <= 0) {
    // We may have started a transition in a prior CSSTransitionData update,
    // this CSSTransitionData update needs to override them.
    // TODO(alancutter): Just iterate over the CSSTransitionDatas in reverse and
    // skip any properties that have already been visited so we don't need to
    // "undo" work like this.
    state.update.UnstartTransition(property);
    return;
  }

  const ComputedStyle* reversing_adjusted_start_value = &state.old_style;
  double reversing_shortening_factor = 1;
  if (interrupted_transition) {
    const base::Optional<double> interrupted_progress =
        interrupted_transition->animation->effect()->Progress();
    if (interrupted_progress) {
      reversing_adjusted_start_value = interrupted_transition->to.get();
      reversing_shortening_factor =
          clampTo((interrupted_progress.value() *
                   interrupted_transition->reversing_shortening_factor) +
                      (1 - interrupted_transition->reversing_shortening_factor),
                  0.0, 1.0);
      timing.iteration_duration.value() *= reversing_shortening_factor;
      if (timing.start_delay < 0) {
        timing.start_delay *= reversing_shortening_factor;
      }
    }
  }

  TransitionKeyframeVector keyframes;
  double start_keyframe_offset = 0;

  if (timing.start_delay > 0) {
    timing.iteration_duration.value() +=
        AnimationTimeDelta::FromSecondsD(timing.start_delay);
    start_keyframe_offset =
        timing.start_delay / timing.iteration_duration->InSecondsF();
    timing.start_delay = 0;
  }

  TransitionKeyframe* delay_keyframe = TransitionKeyframe::Create(property);
  delay_keyframe->SetValue(TypedInterpolationValue::Create(
      *transition_type, start.interpolable_value->Clone(),
      start.non_interpolable_value));
  delay_keyframe->SetOffset(0);
  keyframes.push_back(delay_keyframe);

  TransitionKeyframe* start_keyframe = TransitionKeyframe::Create(property);
  start_keyframe->SetValue(TypedInterpolationValue::Create(
      *transition_type, start.interpolable_value->Clone(),
      start.non_interpolable_value));
  start_keyframe->SetOffset(start_keyframe_offset);
  start_keyframe->SetEasing(std::move(timing.timing_function));
  timing.timing_function = LinearTimingFunction::Shared();
  keyframes.push_back(start_keyframe);

  TransitionKeyframe* end_keyframe = TransitionKeyframe::Create(property);
  end_keyframe->SetValue(TypedInterpolationValue::Create(
      *transition_type, end.interpolable_value->Clone(),
      end.non_interpolable_value));
  end_keyframe->SetOffset(1);
  keyframes.push_back(end_keyframe);

  if (property.GetCSSProperty().IsCompositableProperty()) {
    AnimatableValue* from =
        CSSAnimatableValueFactory::Create(property, state.old_style);
    AnimatableValue* to =
        CSSAnimatableValueFactory::Create(property, state.style);
    delay_keyframe->SetCompositorValue(from);
    start_keyframe->SetCompositorValue(from);
    end_keyframe->SetCompositorValue(to);
  }

  TransitionKeyframeEffectModel* model =
      TransitionKeyframeEffectModel::Create(keyframes);
  if (!state.cloned_style) {
    state.cloned_style = ComputedStyle::Clone(state.style);
  }
  state.update.StartTransition(property, &state.old_style, state.cloned_style,
                               reversing_adjusted_start_value,
                               reversing_shortening_factor,
                               *InertEffect::Create(model, timing, false, 0));
  DCHECK(!state.animating_element->GetElementAnimations() ||
         !state.animating_element->GetElementAnimations()
              ->IsAnimationStyleChange());
}

void CSSAnimations::CalculateTransitionUpdateForCustomProperty(
    TransitionUpdateState& state,
    const CSSTransitionData::TransitionProperty& transition_property,
    size_t transition_index) {
  if (transition_property.property_type !=
      CSSTransitionData::kTransitionUnknownProperty) {
    return;
  }
  if (!CSSVariableParser::IsValidVariableName(
          transition_property.property_string)) {
    return;
  }
  CalculateTransitionUpdateForProperty(
      state, PropertyHandle(transition_property.property_string),
      transition_index);
}

void CSSAnimations::CalculateTransitionUpdateForStandardProperty(
    TransitionUpdateState& state,
    const CSSTransitionData::TransitionProperty& transition_property,
    size_t transition_index,
    const ComputedStyle& style) {
  if (transition_property.property_type !=
      CSSTransitionData::kTransitionKnownProperty) {
    return;
  }

  CSSPropertyID resolved_id =
      resolveCSSPropertyID(transition_property.unresolved_property);
  bool animate_all = resolved_id == CSSPropertyAll;
  const StylePropertyShorthand& property_list =
      animate_all ? PropertiesForTransitionAll()
                  : shorthandForProperty(resolved_id);
  // If not a shorthand we only execute one iteration of this loop, and
  // refer to the property directly.
  for (unsigned i = 0; !i || i < property_list.length(); ++i) {
    CSSPropertyID longhand_id =
        property_list.length() ? property_list.properties()[i]->PropertyID()
                               : resolved_id;
    DCHECK_GE(longhand_id, firstCSSProperty);
    const CSSProperty& property =
        CSSProperty::Get(longhand_id)
            .ResolveDirectionAwareProperty(style.Direction(),
                                           style.GetWritingMode());
    PropertyHandle property_handle = PropertyHandle(property);

    if (!animate_all && !property.IsInterpolable()) {
      continue;
    }

    CalculateTransitionUpdateForProperty(state, property_handle,
                                         transition_index);
  }
}

void CSSAnimations::CalculateTransitionUpdate(CSSAnimationUpdate& update,
                                              PropertyPass property_pass,
                                              const Element* animating_element,
                                              const ComputedStyle& style) {
  if (!animating_element)
    return;

  if (animating_element->GetDocument().FinishingOrIsPrinting())
    return;

  ElementAnimations* element_animations =
      animating_element->GetElementAnimations();
  const TransitionMap* active_transitions =
      element_animations ? &element_animations->CssAnimations().transitions_
                         : nullptr;
  const CSSTransitionData* transition_data = style.Transitions();

  const bool animation_style_recalc =
      element_animations && element_animations->IsAnimationStyleChange();

  HashSet<PropertyHandle> listed_properties;
  bool any_transition_had_transition_all = false;
  const ComputedStyle* old_style = animating_element->GetComputedStyle();
  if (!animation_style_recalc && style.Display() != EDisplay::kNone &&
      old_style && transition_data) {
    TransitionUpdateState state = {
        update,  animating_element,  *old_style,        style,
        nullptr, active_transitions, listed_properties, *transition_data};

    for (wtf_size_t transition_index = 0;
         transition_index < transition_data->PropertyList().size();
         ++transition_index) {
      const CSSTransitionData::TransitionProperty& transition_property =
          transition_data->PropertyList()[transition_index];
      if (transition_property.unresolved_property == CSSPropertyAll) {
        any_transition_had_transition_all = true;
      }
      if (property_pass == PropertyPass::kCustom) {
        CalculateTransitionUpdateForCustomProperty(state, transition_property,
                                                   transition_index);
      } else {
        DCHECK_EQ(property_pass, PropertyPass::kStandard);
        CalculateTransitionUpdateForStandardProperty(state, transition_property,
                                                     transition_index, style);
      }
    }
  }

  if (active_transitions) {
    for (const auto& entry : *active_transitions) {
      const PropertyHandle& property = entry.key;
      if (property.IsCSSCustomProperty() !=
          (property_pass == PropertyPass::kCustom)) {
        continue;
      }
      if (!any_transition_had_transition_all && !animation_style_recalc &&
          !listed_properties.Contains(property)) {
        update.CancelTransition(property);
      } else if (entry.value.animation->FinishedInternal()) {
        update.FinishTransition(property);
      }
    }
  }

  CalculateTransitionActiveInterpolations(update, property_pass,
                                          animating_element);
}

void CSSAnimations::Cancel() {
  for (const auto& running_animation : running_animations_) {
    running_animation->animation->cancel();
    running_animation->animation->Update(kTimingUpdateOnDemand);
  }

  for (const auto& entry : transitions_) {
    entry.value.animation->cancel();
    entry.value.animation->Update(kTimingUpdateOnDemand);
  }

  running_animations_.clear();
  transitions_.clear();
  ClearPendingUpdate();
}

namespace {

bool IsCustomPropertyHandle(const PropertyHandle& property) {
  return property.IsCSSCustomProperty();
}

// TODO(alancutter): CSS properties and presentation attributes may have
// identical effects. By grouping them in the same set we introduce a bug where
// arbitrary hash iteration will determine the order the apply in and thus which
// one "wins". We should be more deliberate about the order of application in
// the case of effect collisions.
// Example: Both 'color' and 'svg-color' set the color on ComputedStyle but are
// considered distinct properties in the ActiveInterpolationsMap.
bool IsStandardPropertyHandle(const PropertyHandle& property) {
  return (property.IsCSSProperty() && !property.IsCSSCustomProperty()) ||
         property.IsPresentationAttribute();
}

void AdoptActiveAnimationInterpolations(
    EffectStack* effect_stack,
    CSSAnimationUpdate& update,
    const HeapVector<Member<const InertEffect>>* new_animations,
    const HeapHashSet<Member<const Animation>>* suppressed_animations) {
  ActiveInterpolationsMap custom_interpolations(
      EffectStack::ActiveInterpolations(
          effect_stack, new_animations, suppressed_animations,
          KeyframeEffect::kDefaultPriority, IsCustomPropertyHandle));
  update.AdoptActiveInterpolationsForCustomAnimations(custom_interpolations);

  ActiveInterpolationsMap standard_interpolations(
      EffectStack::ActiveInterpolations(
          effect_stack, new_animations, suppressed_animations,
          KeyframeEffect::kDefaultPriority, IsStandardPropertyHandle));
  update.AdoptActiveInterpolationsForStandardAnimations(
      standard_interpolations);
}

}  // namespace

void CSSAnimations::CalculateAnimationActiveInterpolations(
    CSSAnimationUpdate& update,
    const Element* animating_element) {
  ElementAnimations* element_animations =
      animating_element ? animating_element->GetElementAnimations() : nullptr;
  EffectStack* effect_stack =
      element_animations ? &element_animations->GetEffectStack() : nullptr;

  if (update.NewAnimations().IsEmpty() &&
      update.SuppressedAnimations().IsEmpty()) {
    AdoptActiveAnimationInterpolations(effect_stack, update, nullptr, nullptr);
    return;
  }

  HeapVector<Member<const InertEffect>> new_effects;
  for (const auto& new_animation : update.NewAnimations())
    new_effects.push_back(new_animation.effect);

  // Animations with updates use a temporary InertEffect for the current frame.
  for (const auto& updated_animation : update.AnimationsWithUpdates())
    new_effects.push_back(updated_animation.effect);

  AdoptActiveAnimationInterpolations(effect_stack, update, &new_effects,
                                     &update.SuppressedAnimations());
}

namespace {

EffectStack::PropertyHandleFilter PropertyFilter(
    CSSAnimations::PropertyPass property_pass) {
  if (property_pass == CSSAnimations::PropertyPass::kCustom) {
    return IsCustomPropertyHandle;
  }
  DCHECK_EQ(property_pass, CSSAnimations::PropertyPass::kStandard);
  return IsStandardPropertyHandle;
}

}  // namespace

void CSSAnimations::CalculateTransitionActiveInterpolations(
    CSSAnimationUpdate& update,
    PropertyPass property_pass,
    const Element* animating_element) {
  ElementAnimations* element_animations =
      animating_element ? animating_element->GetElementAnimations() : nullptr;
  EffectStack* effect_stack =
      element_animations ? &element_animations->GetEffectStack() : nullptr;

  ActiveInterpolationsMap active_interpolations_for_transitions;
  if (update.NewTransitions().IsEmpty() &&
      update.CancelledTransitions().IsEmpty()) {
    active_interpolations_for_transitions = EffectStack::ActiveInterpolations(
        effect_stack, nullptr, nullptr, KeyframeEffect::kTransitionPriority,
        PropertyFilter(property_pass));
  } else {
    HeapVector<Member<const InertEffect>> new_transitions;
    for (const auto& entry : update.NewTransitions())
      new_transitions.push_back(entry.value.effect.Get());

    HeapHashSet<Member<const Animation>> cancelled_animations;
    if (!update.CancelledTransitions().IsEmpty()) {
      DCHECK(element_animations);
      const TransitionMap& transition_map =
          element_animations->CssAnimations().transitions_;
      for (const PropertyHandle& property : update.CancelledTransitions()) {
        DCHECK(transition_map.Contains(property));
        cancelled_animations.insert(
            transition_map.at(property).animation.Get());
      }
    }

    active_interpolations_for_transitions = EffectStack::ActiveInterpolations(
        effect_stack, &new_transitions, &cancelled_animations,
        KeyframeEffect::kTransitionPriority, PropertyFilter(property_pass));
  }

  const ActiveInterpolationsMap& animations =
      property_pass == PropertyPass::kCustom
          ? update.ActiveInterpolationsForCustomAnimations()
          : update.ActiveInterpolationsForStandardAnimations();
  // Properties being animated by animations don't get values from transitions
  // applied.
  if (!animations.IsEmpty() &&
      !active_interpolations_for_transitions.IsEmpty()) {
    for (const auto& entry : animations)
      active_interpolations_for_transitions.erase(entry.key);
  }

  if (property_pass == PropertyPass::kCustom) {
    update.AdoptActiveInterpolationsForCustomTransitions(
        active_interpolations_for_transitions);
  } else {
    DCHECK_EQ(property_pass, PropertyPass::kStandard);
    update.AdoptActiveInterpolationsForStandardTransitions(
        active_interpolations_for_transitions);
  }
}

EventTarget* CSSAnimations::AnimationEventDelegate::GetEventTarget() const {
  return EventPath::EventTargetRespectingTargetRules(*animation_target_);
}

void CSSAnimations::AnimationEventDelegate::MaybeDispatch(
    Document::ListenerType listener_type,
    const AtomicString& event_name,
    double elapsed_time) {
  if (animation_target_->GetDocument().HasListenerType(listener_type)) {
    String pseudo_element_name = PseudoElement::PseudoElementNameForEvents(
        animation_target_->GetPseudoId());
    AnimationEvent* event = AnimationEvent::Create(
        event_name, name_, elapsed_time, pseudo_element_name);
    event->SetTarget(GetEventTarget());
    GetDocument().EnqueueAnimationFrameEvent(event);
  }
}

bool CSSAnimations::AnimationEventDelegate::RequiresIterationEvents(
    const AnimationEffect& animation_node) {
  return GetDocument().HasListenerType(Document::kAnimationIterationListener);
}

void CSSAnimations::AnimationEventDelegate::OnEventCondition(
    const AnimationEffect& animation_node) {
  const AnimationEffect::Phase current_phase = animation_node.GetPhase();
  const double current_iteration = animation_node.CurrentIteration();

  if (previous_phase_ != current_phase &&
      (current_phase == AnimationEffect::kPhaseActive ||
       current_phase == AnimationEffect::kPhaseAfter) &&
      (previous_phase_ == AnimationEffect::kPhaseNone ||
       previous_phase_ == AnimationEffect::kPhaseBefore)) {
    const double start_delay = animation_node.SpecifiedTiming().start_delay;
    const double elapsed_time = start_delay < 0 ? -start_delay : 0;
    MaybeDispatch(Document::kAnimationStartListener,
                  EventTypeNames::animationstart, elapsed_time);
  }

  if (current_phase == AnimationEffect::kPhaseActive &&
      previous_phase_ == current_phase &&
      previous_iteration_ != current_iteration) {
    // We fire only a single event for all iterations that terminate
    // between a single pair of samples. See http://crbug.com/275263. For
    // compatibility with the existing implementation, this event uses
    // the elapsedTime for the first iteration in question.
    const AnimationTimeDelta elapsed_time =
        animation_node.SpecifiedTiming().iteration_duration.value() *
        (previous_iteration_ + 1);
    MaybeDispatch(Document::kAnimationIterationListener,
                  EventTypeNames::animationiteration,
                  elapsed_time.InSecondsF());
  }

  if (current_phase == AnimationEffect::kPhaseAfter &&
      previous_phase_ != AnimationEffect::kPhaseAfter) {
    MaybeDispatch(Document::kAnimationEndListener, EventTypeNames::animationend,
                  animation_node.RepeatedDuration());
  }

  previous_phase_ = current_phase;
  previous_iteration_ = current_iteration;
}

void CSSAnimations::AnimationEventDelegate::Trace(blink::Visitor* visitor) {
  visitor->Trace(animation_target_);
  AnimationEffect::EventDelegate::Trace(visitor);
}

EventTarget* CSSAnimations::TransitionEventDelegate::GetEventTarget() const {
  return EventPath::EventTargetRespectingTargetRules(*transition_target_);
}

void CSSAnimations::TransitionEventDelegate::OnEventCondition(
    const AnimationEffect& animation_node) {
  const AnimationEffect::Phase current_phase = animation_node.GetPhase();
  if (current_phase == AnimationEffect::kPhaseAfter &&
      current_phase != previous_phase_ &&
      GetDocument().HasListenerType(Document::kTransitionEndListener)) {
    String property_name =
        property_.IsCSSCustomProperty()
            ? property_.CustomPropertyName()
            : property_.GetCSSProperty().GetPropertyNameString();
    const Timing& timing = animation_node.SpecifiedTiming();
    double elapsed_time = timing.iteration_duration->InSecondsF();
    const AtomicString& event_type = EventTypeNames::transitionend;
    String pseudo_element =
        PseudoElement::PseudoElementNameForEvents(GetPseudoId());
    TransitionEvent* event = TransitionEvent::Create(
        event_type, property_name, elapsed_time, pseudo_element);
    event->SetTarget(GetEventTarget());
    GetDocument().EnqueueAnimationFrameEvent(event);
  }

  previous_phase_ = current_phase;
}

void CSSAnimations::TransitionEventDelegate::Trace(blink::Visitor* visitor) {
  visitor->Trace(transition_target_);
  AnimationEffect::EventDelegate::Trace(visitor);
}

const StylePropertyShorthand& CSSAnimations::PropertiesForTransitionAll() {
  DEFINE_STATIC_LOCAL(Vector<const CSSProperty*>, properties, ());
  DEFINE_STATIC_LOCAL(StylePropertyShorthand, property_shorthand, ());
  if (properties.IsEmpty()) {
    for (int i = firstCSSProperty; i <= lastCSSProperty; ++i) {
      CSSPropertyID id = convertToCSSPropertyID(i);
      // Avoid creating overlapping transitions with perspective-origin and
      // transition-origin.
      if (id == CSSPropertyWebkitPerspectiveOriginX ||
          id == CSSPropertyWebkitPerspectiveOriginY ||
          id == CSSPropertyWebkitTransformOriginX ||
          id == CSSPropertyWebkitTransformOriginY ||
          id == CSSPropertyWebkitTransformOriginZ)
        continue;
      const CSSProperty& property = CSSProperty::Get(id);
      if (property.IsInterpolable())
        properties.push_back(&property);
    }
    property_shorthand = StylePropertyShorthand(
        CSSPropertyInvalid, properties.begin(), properties.size());
  }
  return property_shorthand;
}

// Properties that affect animations are not allowed to be affected by
// animations. https://drafts.csswg.org/web-animations/#not-animatable-section
bool CSSAnimations::IsAnimationAffectingProperty(const CSSProperty& property) {
  switch (property.PropertyID()) {
    case CSSPropertyAnimation:
    case CSSPropertyAnimationDelay:
    case CSSPropertyAnimationDirection:
    case CSSPropertyAnimationDuration:
    case CSSPropertyAnimationFillMode:
    case CSSPropertyAnimationIterationCount:
    case CSSPropertyAnimationName:
    case CSSPropertyAnimationPlayState:
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyContain:
    case CSSPropertyDirection:
    case CSSPropertyDisplay:
    case CSSPropertyTextOrientation:
    case CSSPropertyTransition:
    case CSSPropertyTransitionDelay:
    case CSSPropertyTransitionDuration:
    case CSSPropertyTransitionProperty:
    case CSSPropertyTransitionTimingFunction:
    case CSSPropertyUnicodeBidi:
    case CSSPropertyWillChange:
    case CSSPropertyWritingMode:
      return true;
    default:
      return false;
  }
}

bool CSSAnimations::IsAffectedByKeyframesFromScope(
    const Element& element,
    const TreeScope& tree_scope) {
  // Animated elements are affected by @keyframes rules from the same scope
  // and from their shadow sub-trees if they are shadow hosts.
  if (element.GetTreeScope() == tree_scope)
    return true;
  if (!IsShadowHost(element))
    return false;
  if (tree_scope.RootNode() == tree_scope.GetDocument())
    return false;
  return ToShadowRoot(tree_scope.RootNode()).host() == element;
}

bool CSSAnimations::IsAnimatingCustomProperties(
    const ElementAnimations* element_animations) {
  return element_animations &&
         element_animations->GetEffectStack().AffectsProperties(
             IsCustomPropertyHandle);
}

void CSSAnimations::Trace(blink::Visitor* visitor) {
  visitor->Trace(transitions_);
  visitor->Trace(pending_update_);
  visitor->Trace(running_animations_);
  visitor->Trace(previous_active_interpolations_for_standard_animations_);
  visitor->Trace(previous_active_interpolations_for_custom_animations_);
}

}  // namespace blink
