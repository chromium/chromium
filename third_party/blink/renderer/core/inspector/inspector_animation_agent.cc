// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_animation_agent.h"

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/animation/computed_effect_timing.h"
#include "third_party/blink/renderer/core/animation/css/css_animation.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/css/css_transition.h"
#include "third_party/blink/renderer/core/animation/effect_model.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/optional_effect_timing.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/css/css_keyframe_rule.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_style_sheet.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"

namespace blink {

namespace {

String AnimationDisplayName(const Animation& animation) {
  if (!animation.id().IsEmpty())
    return animation.id();
  else if (animation.IsCSSAnimation())
    return ToCSSAnimation(animation).animationName();
  else if (animation.IsCSSTransition())
    return ToCSSTransition(animation).transitionProperty();
  else
    return animation.id();
}

}  // namespace

using protocol::Response;

InspectorAnimationAgent::InspectorAnimationAgent(
    InspectedFrames* inspected_frames,
    InspectorCSSAgent* css_agent,
    v8_inspector::V8InspectorSession* v8_session)
    : inspected_frames_(inspected_frames),
      css_agent_(css_agent),
      v8_session_(v8_session),
      is_cloning_(false),
      enabled_(&agent_state_, /*default_value=*/false),
      playback_rate_(&agent_state_, /*default_value=*/1.0) {}

void InspectorAnimationAgent::Restore() {
  if (enabled_.Get()) {
    instrumenting_agents_->AddInspectorAnimationAgent(this);
    setPlaybackRate(playback_rate_.Get());
  }
}

Response InspectorAnimationAgent::enable() {
  enabled_.Set(true);
  instrumenting_agents_->AddInspectorAnimationAgent(this);
  return Response::OK();
}

Response InspectorAnimationAgent::disable() {
  setPlaybackRate(1.0);
  for (const auto& clone : id_to_animation_clone_.Values())
    clone->cancel();
  enabled_.Clear();
  instrumenting_agents_->RemoveInspectorAnimationAgent(this);
  id_to_animation_.clear();
  id_to_animation_clone_.clear();
  cleared_animations_.clear();
  return Response::OK();
}

void InspectorAnimationAgent::DidCommitLoadForLocalFrame(LocalFrame* frame) {
  if (frame == inspected_frames_->Root()) {
    id_to_animation_.clear();
    id_to_animation_clone_.clear();
    cleared_animations_.clear();
  }
  setPlaybackRate(playback_rate_.Get());
}

static std::unique_ptr<protocol::Animation::AnimationEffect>
BuildObjectForAnimationEffect(KeyframeEffect* effect) {
  ComputedEffectTiming* computed_timing = effect->getComputedTiming();
  double delay = computed_timing->delay();
  double duration = computed_timing->duration().GetAsUnrestrictedDouble();
  String easing = effect->SpecifiedTiming().timing_function->ToString();

  std::unique_ptr<protocol::Animation::AnimationEffect> animation_object =
      protocol::Animation::AnimationEffect::create()
          .setDelay(delay)
          .setEndDelay(computed_timing->endDelay())
          .setIterationStart(computed_timing->iterationStart())
          .setIterations(computed_timing->iterations())
          .setDuration(duration)
          .setDirection(computed_timing->direction())
          .setFill(computed_timing->fill())
          .setEasing(easing)
          .build();
  if (effect->target()) {
    animation_object->setBackendNodeId(
        IdentifiersFactory::IntIdForNode(effect->target()));
  }
  return animation_object;
}

static std::unique_ptr<protocol::Animation::KeyframeStyle>
BuildObjectForStringKeyframe(const StringKeyframe* keyframe,
                             double computed_offset) {
  String offset = String::NumberToStringECMAScript(computed_offset * 100) + "%";

  std::unique_ptr<protocol::Animation::KeyframeStyle> keyframe_object =
      protocol::Animation::KeyframeStyle::create()
          .setOffset(offset)
          .setEasing(keyframe->Easing().ToString())
          .build();
  return keyframe_object;
}

static std::unique_ptr<protocol::Animation::KeyframesRule>
BuildObjectForAnimationKeyframes(const KeyframeEffect* effect) {
  if (!effect || !effect->Model() || !effect->Model()->IsKeyframeEffectModel())
    return nullptr;
  const KeyframeEffectModelBase* model = effect->Model();
  Vector<double> computed_offsets =
      KeyframeEffectModelBase::GetComputedOffsets(model->GetFrames());
  auto keyframes =
      std::make_unique<protocol::Array<protocol::Animation::KeyframeStyle>>();

  for (wtf_size_t i = 0; i < model->GetFrames().size(); i++) {
    const Keyframe* keyframe = model->GetFrames().at(i);
    // Ignore CSS Transitions
    if (!keyframe->IsStringKeyframe())
      continue;
    const StringKeyframe* string_keyframe = ToStringKeyframe(keyframe);
    keyframes->emplace_back(
        BuildObjectForStringKeyframe(string_keyframe, computed_offsets.at(i)));
  }
  return protocol::Animation::KeyframesRule::create()
      .setKeyframes(std::move(keyframes))
      .build();
}

std::unique_ptr<protocol::Animation::Animation>
InspectorAnimationAgent::BuildObjectForAnimation(blink::Animation& animation) {
  String animation_type = AnimationType::WebAnimation;
  std::unique_ptr<protocol::Animation::AnimationEffect> animation_effect_object;

  if (animation.effect()) {
    animation_effect_object =
        BuildObjectForAnimationEffect(ToKeyframeEffect(animation.effect()));

    if (animation.IsCSSTransition()) {
      animation_type = AnimationType::CSSTransition;
    } else {
      animation_effect_object->setKeyframesRule(
          BuildObjectForAnimationKeyframes(
              ToKeyframeEffect(animation.effect())));

      if (animation.IsCSSAnimation())
        animation_type = AnimationType::CSSAnimation;
    }
  }

  String id = String::Number(animation.SequenceNumber());
  id_to_animation_.Set(id, &animation);

  std::unique_ptr<protocol::Animation::Animation> animation_object =
      protocol::Animation::Animation::create()
          .setId(id)
          .setName(AnimationDisplayName(animation))
          .setPausedState(animation.Paused())
          .setPlayState(animation.playState())
          .setPlaybackRate(animation.playbackRate())
          .setStartTime(NormalizedStartTime(animation))
          .setCurrentTime(animation.currentTime())
          .setType(animation_type)
          .build();
  if (animation_type != AnimationType::WebAnimation)
    animation_object->setCssId(CreateCSSId(animation));
  if (animation_effect_object)
    animation_object->setSource(std::move(animation_effect_object));
  return animation_object;
}

Response InspectorAnimationAgent::getPlaybackRate(double* playback_rate) {
  *playback_rate = ReferenceTimeline().PlaybackRate();
  return Response::OK();
}

Response InspectorAnimationAgent::setPlaybackRate(double playback_rate) {
  for (LocalFrame* frame : *inspected_frames_)
    frame->GetDocument()->Timeline().SetPlaybackRate(playback_rate);
  playback_rate_.Set(playback_rate);
  return Response::OK();
}

Response InspectorAnimationAgent::getCurrentTime(const String& id,
                                                 double* current_time) {
  blink::Animation* animation = nullptr;
  Response response = AssertAnimation(id, animation);
  if (!response.isSuccess())
    return response;
  if (id_to_animation_clone_.at(id))
    animation = id_to_animation_clone_.at(id);

  if (animation->Paused() || !animation->timeline()->IsActive()) {
    *current_time = animation->currentTime();
  } else {
    // Use startTime where possible since currentTime is limited.
    base::Optional<double> timeline_time = animation->timeline()->CurrentTime();
    // TODO(crbug.com/916117): Handle NaN values for scroll linked animations.
    *current_time = timeline_time
                        ? timeline_time.value() -
                              animation->startTime().value_or(NullValue())
                        : NullValue();
  }
  return Response::OK();
}

Response InspectorAnimationAgent::setPaused(
    std::unique_ptr<protocol::Array<String>> animation_ids,
    bool paused) {
  for (const String& animation_id : *animation_ids) {
    blink::Animation* animation = nullptr;
    Response response = AssertAnimation(animation_id, animation);
    if (!response.isSuccess())
      return response;
    blink::Animation* clone = AnimationClone(animation);
    if (!clone)
      return Response::Error("Failed to clone detached animation");
    if (paused && !clone->Paused()) {
      // Ensure we restore a current time if the animation is limited.
      double current_time = 0;
      if (!clone->timeline()->IsActive()) {
        current_time = clone->currentTime();
      } else {
        base::Optional<double> timeline_time = clone->timeline()->CurrentTime();
        // TODO(crbug.com/916117): Handle NaN values.
        current_time = timeline_time
                           ? timeline_time.value() -
                                 clone->startTime().value_or(NullValue())
                           : NullValue();
      }
      clone->pause();
      clone->setCurrentTime(current_time, false);
    } else if (!paused && clone->Paused()) {
      clone->Unpause();
    }
  }
  return Response::OK();
}

blink::Animation* InspectorAnimationAgent::AnimationClone(
    blink::Animation* animation) {
  const String id = String::Number(animation->SequenceNumber());
  if (!id_to_animation_clone_.at(id)) {
    KeyframeEffect* old_effect = ToKeyframeEffect(animation->effect());
    DCHECK(old_effect->Model()->IsKeyframeEffectModel());
    KeyframeEffectModelBase* old_model = old_effect->Model();
    KeyframeEffectModelBase* new_model = nullptr;
    // Clone EffectModel.
    // TODO(samli): Determine if this is an animations bug.
    if (old_model->IsStringKeyframeEffectModel()) {
      StringKeyframeEffectModel* old_string_keyframe_model =
          ToStringKeyframeEffectModel(old_model);
      KeyframeVector old_keyframes = old_string_keyframe_model->GetFrames();
      StringKeyframeVector new_keyframes;
      for (auto& old_keyframe : old_keyframes)
        new_keyframes.push_back(ToStringKeyframe(old_keyframe));
      new_model =
          MakeGarbageCollected<StringKeyframeEffectModel>(new_keyframes);
    } else if (old_model->IsTransitionKeyframeEffectModel()) {
      TransitionKeyframeEffectModel* old_transition_keyframe_model =
          ToTransitionKeyframeEffectModel(old_model);
      KeyframeVector old_keyframes = old_transition_keyframe_model->GetFrames();
      TransitionKeyframeVector new_keyframes;
      for (auto& old_keyframe : old_keyframes)
        new_keyframes.push_back(ToTransitionKeyframe(old_keyframe));
      new_model =
          MakeGarbageCollected<TransitionKeyframeEffectModel>(new_keyframes);
    }

    auto* new_effect = MakeGarbageCollected<KeyframeEffect>(
        old_effect->target(), new_model, old_effect->SpecifiedTiming());
    is_cloning_ = true;
    blink::Animation* clone =
        blink::Animation::Create(new_effect, animation->timeline());
    is_cloning_ = false;
    id_to_animation_clone_.Set(id, clone);
    id_to_animation_.Set(String::Number(clone->SequenceNumber()), clone);
    clone->play();
    clone->setStartTime(animation->startTime().value_or(NullValue()), false);

    animation->SetEffectSuppressed(true);
  }
  return id_to_animation_clone_.at(id);
}

Response InspectorAnimationAgent::seekAnimations(
    std::unique_ptr<protocol::Array<String>> animation_ids,
    double current_time) {
  for (const String& animation_id : *animation_ids) {
    blink::Animation* animation = nullptr;
    Response response = AssertAnimation(animation_id, animation);
    if (!response.isSuccess())
      return response;
    blink::Animation* clone = AnimationClone(animation);
    if (!clone)
      return Response::Error("Failed to clone a detached animation.");
    if (!clone->Paused())
      clone->play();
    clone->setCurrentTime(current_time, false);
  }
  return Response::OK();
}

Response InspectorAnimationAgent::releaseAnimations(
    std::unique_ptr<protocol::Array<String>> animation_ids) {
  for (const String& animation_id : *animation_ids) {
    blink::Animation* animation = id_to_animation_.at(animation_id);
    if (animation)
      animation->SetEffectSuppressed(false);
    blink::Animation* clone = id_to_animation_clone_.at(animation_id);
    if (clone)
      clone->cancel();
    id_to_animation_clone_.erase(animation_id);
    id_to_animation_.erase(animation_id);
    cleared_animations_.insert(animation_id);
  }
  return Response::OK();
}

Response InspectorAnimationAgent::setTiming(const String& animation_id,
                                            double duration,
                                            double delay) {
  blink::Animation* animation = nullptr;
  Response response = AssertAnimation(animation_id, animation);
  if (!response.isSuccess())
    return response;

  animation = AnimationClone(animation);
  NonThrowableExceptionState exception_state;

  OptionalEffectTiming* timing = OptionalEffectTiming::Create();
  UnrestrictedDoubleOrString unrestricted_duration;
  unrestricted_duration.SetUnrestrictedDouble(duration);
  timing->setDuration(unrestricted_duration);
  timing->setDelay(delay);
  animation->effect()->updateTiming(timing, exception_state);
  return Response::OK();
}

Response InspectorAnimationAgent::resolveAnimation(
    const String& animation_id,
    std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>*
        result) {
  blink::Animation* animation = nullptr;
  Response response = AssertAnimation(animation_id, animation);
  if (!response.isSuccess())
    return response;
  if (id_to_animation_clone_.at(animation_id))
    animation = id_to_animation_clone_.at(animation_id);
  const Element* element = ToKeyframeEffect(animation->effect())->target();
  Document* document = element->ownerDocument();
  LocalFrame* frame = document ? document->GetFrame() : nullptr;
  ScriptState* script_state =
      frame ? ToScriptStateForMainWorld(frame) : nullptr;
  if (!script_state)
    return Response::Error("Element not associated with a document.");

  ScriptState::Scope scope(script_state);
  static const char kAnimationObjectGroup[] = "animation";
  v8_session_->releaseObjectGroup(
      ToV8InspectorStringView(kAnimationObjectGroup));
  *result = v8_session_->wrapObject(
      script_state->GetContext(),
      ToV8(animation, script_state->GetContext()->Global(),
           script_state->GetIsolate()),
      ToV8InspectorStringView(kAnimationObjectGroup),
      false /* generatePreview */);
  if (!*result)
    return Response::Error("Element not associated with a document.");
  return Response::OK();
}

String InspectorAnimationAgent::CreateCSSId(blink::Animation& animation) {
  static const CSSProperty* g_animation_properties[] = {
      &GetCSSPropertyAnimationDelay(),
      &GetCSSPropertyAnimationDirection(),
      &GetCSSPropertyAnimationDuration(),
      &GetCSSPropertyAnimationFillMode(),
      &GetCSSPropertyAnimationIterationCount(),
      &GetCSSPropertyAnimationName(),
      &GetCSSPropertyAnimationTimingFunction(),
  };
  static const CSSProperty* g_transition_properties[] = {
      &GetCSSPropertyTransitionDelay(), &GetCSSPropertyTransitionDuration(),
      &GetCSSPropertyTransitionProperty(),
      &GetCSSPropertyTransitionTimingFunction(),
  };

  KeyframeEffect* effect = ToKeyframeEffect(animation.effect());
  Vector<const CSSProperty*> css_properties;
  if (animation.IsCSSAnimation()) {
    for (const CSSProperty* property : g_animation_properties)
      css_properties.push_back(property);
  } else if (animation.IsCSSTransition()) {
    for (const CSSProperty* property : g_transition_properties)
      css_properties.push_back(property);
    css_properties.push_back(
        &ToCSSTransition(animation).TransitionCSSProperty());
  } else {
    NOTREACHED();
  }

  Element* element = effect->target();
  HeapVector<Member<CSSStyleDeclaration>> styles =
      css_agent_->MatchingStyles(element);
  Digestor digestor(kHashAlgorithmSha1);
  digestor.UpdateUtf8(animation.IsCSSTransition()
                          ? AnimationType::CSSTransition
                          : AnimationType::CSSAnimation);
  digestor.UpdateUtf8(animation.id());
  for (const CSSProperty* property : css_properties) {
    CSSStyleDeclaration* style =
        css_agent_->FindEffectiveDeclaration(*property, styles);
    // Ignore inline styles.
    if (!style || !style->ParentStyleSheet() || !style->parentRule() ||
        style->parentRule()->type() != CSSRule::kStyleRule)
      continue;
    digestor.UpdateUtf8(property->GetPropertyNameString());
    digestor.UpdateUtf8(css_agent_->StyleSheetId(style->ParentStyleSheet()));
    digestor.UpdateUtf8(To<CSSStyleRule>(style->parentRule())->selectorText());
  }
  DigestValue digest_result;
  digestor.Finish(digest_result);
  DCHECK(!digestor.has_failed());
  return Base64Encode(base::make_span(digest_result).first<10>());
}

void InspectorAnimationAgent::DidCreateAnimation(unsigned sequence_number) {
  if (is_cloning_)
    return;
  GetFrontend()->animationCreated(String::Number(sequence_number));
}

void InspectorAnimationAgent::AnimationPlayStateChanged(
    blink::Animation* animation,
    blink::Animation::AnimationPlayState old_play_state,
    blink::Animation::AnimationPlayState new_play_state) {
  const String& animation_id = String::Number(animation->SequenceNumber());

  // We no longer care about animations that have been released.
  if (cleared_animations_.Contains(animation_id))
    return;

  // Record newly starting animations only once, as |buildObjectForAnimation|
  // constructs and caches our internal representation of the given |animation|.
  if ((new_play_state == blink::Animation::kRunning ||
       new_play_state == blink::Animation::kFinished) &&
      !id_to_animation_.Contains(animation_id))
    GetFrontend()->animationStarted(BuildObjectForAnimation(*animation));
  else if (new_play_state == blink::Animation::kIdle ||
           new_play_state == blink::Animation::kPaused)
    GetFrontend()->animationCanceled(animation_id);
}

void InspectorAnimationAgent::DidClearDocumentOfWindowObject(
    LocalFrame* frame) {
  if (!enabled_.Get())
    return;
  DCHECK(frame->GetDocument());
  frame->GetDocument()->Timeline().SetPlaybackRate(
      ReferenceTimeline().PlaybackRate());
}

Response InspectorAnimationAgent::AssertAnimation(const String& id,
                                                  blink::Animation*& result) {
  result = id_to_animation_.at(id);
  if (!result)
    return Response::Error("Could not find animation with given id");
  return Response::OK();
}

DocumentTimeline& InspectorAnimationAgent::ReferenceTimeline() {
  return inspected_frames_->Root()->GetDocument()->Timeline();
}

double InspectorAnimationAgent::NormalizedStartTime(
    blink::Animation& animation) {
  double time_ms = animation.startTime().value_or(NullValue());
  if (animation.timeline()->IsDocumentTimeline()) {
    if (ReferenceTimeline().PlaybackRate() == 0) {
      time_ms += ReferenceTimeline().currentTime() -
                 ToDocumentTimeline(animation.timeline())->currentTime();
    } else {
      time_ms += (ToDocumentTimeline(animation.timeline())->ZeroTime() -
                  ReferenceTimeline().ZeroTime())
                     .InMillisecondsF() *
                 ReferenceTimeline().PlaybackRate();
    }
  }
  // Round to the closest microsecond.
  return std::round(time_ms * 1000) / 1000;
}

void InspectorAnimationAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(inspected_frames_);
  visitor->Trace(css_agent_);
  visitor->Trace(id_to_animation_);
  visitor->Trace(id_to_animation_clone_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
