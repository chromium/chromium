// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_animation_agent.h"

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_computed_effect_timing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_optional_effect_timing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_string_unrestricteddouble.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_double_timelineoffset.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/animation/css/css_animation.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/css/css_transition.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/effect_model.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
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
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"

namespace blink {

namespace {

double AsDoubleOrZero(V8UnionDoubleOrTimelineOffset* value) {
  if (!value->IsDouble())
    return 0;

  return value->GetAsDouble();
}

String AnimationDisplayName(const Animation& animation) {
  if (!animation.id().empty())
    return animation.id();
  else if (auto* css_animation = DynamicTo<CSSAnimation>(animation))
    return css_animation->animationName();
  else if (auto* css_transition = DynamicTo<CSSTransition>(animation))
    return css_transition->transitionProperty();
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
      playback_rate_(&agent_state_, /*default_value=*/1.0) {
  DCHECK(css_agent);
}

void InspectorAnimationAgent::Restore() {
  if (enabled_.Get()) {
    instrumenting_agents_->AddInspectorAnimationAgent(this);
    setPlaybackRate(playback_rate_.Get());
  }
}

Response InspectorAnimationAgent::enable() {
  enabled_.Set(true);
  instrumenting_agents_->AddInspectorAnimationAgent(this);
  return Response::Success();
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
  return Response::Success();
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
  double delay = AsDoubleOrZero(computed_timing->delay());
  double end_delay = AsDoubleOrZero(computed_timing->endDelay());
  double duration = computed_timing->duration()->GetAsUnrestrictedDouble();
  String easing = effect->SpecifiedTiming().timing_function->ToString();

  std::unique_ptr<protocol::Animation::AnimationEffect> animation_object =
      protocol::Animation::AnimationEffect::create()
          .setDelay(delay)
          .setEndDelay(end_delay)
          .setIterationStart(computed_timing->iterationStart())
          .setIterations(computed_timing->iterations())
          .setDuration(duration)
          .setDirection(computed_timing->direction())
          .setFill(computed_timing->fill())
          .setEasing(easing)
          .build();
  if (effect->EffectTarget()) {
    animation_object->setBackendNodeId(
        IdentifiersFactory::IntIdForNode(effect->EffectTarget()));
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
    const auto* string_keyframe = To<StringKeyframe>(keyframe);
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
        BuildObjectForAnimationEffect(To<KeyframeEffect>(animation.effect()));

    if (IsA<CSSTransition>(animation)) {
      animation_type = AnimationType::CSSTransition;
    } else {
      animation_effect_object->setKeyframesRule(
          BuildObjectForAnimationKeyframes(
              To<KeyframeEffect>(animation.effect())));

      if (IsA<CSSAnimation>(animation))
        animation_type = AnimationType::CSSAnimation;
    }
  }

  String id = String::Number(animation.SequenceNumber());
  id_to_animation_.Set(id, &animation);

  double current_time = Timing::NullValue();
  absl::optional<AnimationTimeDelta> animation_current_time =
      animation.CurrentTimeInternal();
  if (animation_current_time) {
    current_time = animation_current_time.value().InMillisecondsF();
  }

  std::unique_ptr<protocol::Animation::Animation> animation_object =
      protocol::Animation::Animation::create()
          .setId(id)
          .setName(AnimationDisplayName(animation))
          .setPausedState(animation.Paused())
          .setPlayState(animation.PlayStateString())
          .setPlaybackRate(animation.playbackRate())
          .setStartTime(NormalizedStartTime(animation))
          .setCurrentTime(current_time)
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
  return Response::Success();
}

Response InspectorAnimationAgent::setPlaybackRate(double playback_rate) {
  for (LocalFrame* frame : *inspected_frames_)
    frame->GetDocument()->Timeline().SetPlaybackRate(playback_rate);
  playback_rate_.Set(playback_rate);
  return Response::Success();
}

Response InspectorAnimationAgent::getCurrentTime(const String& id,
                                                 double* current_time) {
  blink::Animation* animation = nullptr;
  Response response = AssertAnimation(id, animation);
  if (!response.IsSuccess())
    return response;

  auto it = id_to_animation_clone_.find(id);
  if (it != id_to_animation_clone_.end())
    animation = it->value;

  *current_time = Timing::NullValue();
  if (animation->Paused() || !animation->timeline()->IsActive()) {
    absl::optional<AnimationTimeDelta> animation_current_time =
        animation->CurrentTimeInternal();
    if (animation_current_time) {
      *current_time = animation_current_time.value().InMillisecondsF();
    }
  } else {
    // Use startTime where possible since currentTime is limited.
    absl::optional<AnimationTimeDelta> animation_start_time =
        animation->StartTimeInternal();
    if (animation_start_time) {
      absl::optional<AnimationTimeDelta> timeline_time =
          animation->timeline()->CurrentTime();
      // TODO(crbug.com/916117): Handle NaN values for scroll linked animations.
      if (timeline_time) {
        *current_time = timeline_time.value().InMillisecondsF() -
                        animation_start_time.value().InMillisecondsF();
      }
    }
  }
  return Response::Success();
}

Response InspectorAnimationAgent::setPaused(
    std::unique_ptr<protocol::Array<String>> animation_ids,
    bool paused) {
  for (const String& animation_id : *animation_ids) {
    blink::Animation* animation = nullptr;
    Response response = AssertAnimation(animation_id, animation);
    if (!response.IsSuccess())
      return response;
    blink::Animation* clone = AnimationClone(animation);
    if (!clone)
      return Response::ServerError("Failed to clone detached animation");
    if (paused && !clone->Paused()) {
      // Ensure we restore a current time if the animation is limited.
      absl::optional<AnimationTimeDelta> current_time;
      if (!clone->timeline()->IsActive()) {
        current_time = clone->CurrentTimeInternal();
      } else {
        absl::optional<AnimationTimeDelta> start_time =
            clone->StartTimeInternal();
        if (start_time) {
          absl::optional<AnimationTimeDelta> timeline_time =
              clone->timeline()->CurrentTime();
          // TODO(crbug.com/916117): Handle NaN values.
          if (timeline_time) {
            current_time = timeline_time.value() - start_time.value();
          }
        }
      }
      clone->pause();
      clone->SetCurrentTimeInternal(current_time.value());
    } else if (!paused && clone->Paused()) {
      clone->Unpause();
    }
  }
  return Response::Success();
}

blink::Animation* InspectorAnimationAgent::AnimationClone(
    blink::Animation* animation) {
  const String id = String::Number(animation->SequenceNumber());
  auto it = id_to_animation_clone_.find(id);
  if (it != id_to_animation_clone_.end())
    return it->value;

  auto* old_effect = To<KeyframeEffect>(animation->effect());
  DCHECK(old_effect->Model()->IsKeyframeEffectModel());
  KeyframeEffectModelBase* old_model = old_effect->Model();
  KeyframeEffectModelBase* new_model = nullptr;
  // Clone EffectModel.
  // TODO(samli): Determine if this is an animations bug.
  if (old_model->IsStringKeyframeEffectModel()) {
    auto* old_string_keyframe_model = To<StringKeyframeEffectModel>(old_model);
    KeyframeVector old_keyframes = old_string_keyframe_model->GetFrames();
    StringKeyframeVector new_keyframes;
    for (auto& old_keyframe : old_keyframes)
      new_keyframes.push_back(To<StringKeyframe>(*old_keyframe));
    new_model = MakeGarbageCollected<StringKeyframeEffectModel>(new_keyframes);
  } else if (old_model->IsTransitionKeyframeEffectModel()) {
    auto* old_transition_keyframe_model =
        To<TransitionKeyframeEffectModel>(old_model);
    KeyframeVector old_keyframes = old_transition_keyframe_model->GetFrames();
    TransitionKeyframeVector new_keyframes;
    for (auto& old_keyframe : old_keyframes)
      new_keyframes.push_back(To<TransitionKeyframe>(*old_keyframe));
    new_model =
        MakeGarbageCollected<TransitionKeyframeEffectModel>(new_keyframes);
  }

  auto* new_effect = MakeGarbageCollected<KeyframeEffect>(
      old_effect->EffectTarget(), new_model, old_effect->SpecifiedTiming());
  is_cloning_ = true;
  blink::Animation* clone =
      blink::Animation::Create(new_effect, animation->timeline());
  is_cloning_ = false;
  id_to_animation_clone_.Set(id, clone);
  id_to_animation_.Set(String::Number(clone->SequenceNumber()), clone);
  clone->play();
  clone->setStartTime(animation->startTime(), ASSERT_NO_EXCEPTION);

  animation->SetEffectSuppressed(true);
  return clone;
}

Response InspectorAnimationAgent::seekAnimations(
    std::unique_ptr<protocol::Array<String>> animation_ids,
    double current_time) {
  for (const String& animation_id : *animation_ids) {
    blink::Animation* animation = nullptr;
    Response response = AssertAnimation(animation_id, animation);
    if (!response.IsSuccess())
      return response;
    blink::Animation* clone = AnimationClone(animation);
    if (!clone)
      return Response::ServerError("Failed to clone a detached animation.");
    if (!clone->Paused())
      clone->play();
    clone->SetCurrentTimeInternal(
        ANIMATION_TIME_DELTA_FROM_MILLISECONDS(current_time));
  }
  return Response::Success();
}

Response InspectorAnimationAgent::releaseAnimations(
    std::unique_ptr<protocol::Array<String>> animation_ids) {
  for (const String& animation_id : *animation_ids) {
    auto it = id_to_animation_.find(animation_id);
    if (it != id_to_animation_.end())
      it->value->SetEffectSuppressed(false);

    it = id_to_animation_clone_.find(animation_id);
    if (it != id_to_animation_clone_.end())
      it->value->cancel();

    id_to_animation_clone_.erase(animation_id);
    id_to_animation_.erase(animation_id);
    cleared_animations_.insert(animation_id);
  }
  return Response::Success();
}

Response InspectorAnimationAgent::setTiming(const String& animation_id,
                                            double duration,
                                            double delay) {
  blink::Animation* animation = nullptr;
  Response response = AssertAnimation(animation_id, animation);
  if (!response.IsSuccess())
    return response;

  animation = AnimationClone(animation);
  NonThrowableExceptionState exception_state;

  OptionalEffectTiming* timing = OptionalEffectTiming::Create();
  timing->setDuration(
      MakeGarbageCollected<V8UnionCSSNumericValueOrStringOrUnrestrictedDouble>(
          duration));
  timing->setDelay(MakeGarbageCollected<V8UnionDoubleOrTimelineOffset>(delay));
  animation->effect()->updateTiming(timing, exception_state);
  return Response::Success();
}

Response InspectorAnimationAgent::resolveAnimation(
    const String& animation_id,
    std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>*
        result) {
  blink::Animation* animation = nullptr;
  Response response = AssertAnimation(animation_id, animation);
  if (!response.IsSuccess())
    return response;

  auto it = id_to_animation_clone_.find(animation_id);
  if (it != id_to_animation_clone_.end())
    animation = it->value;

  const Element* element =
      To<KeyframeEffect>(animation->effect())->EffectTarget();
  Document* document = element->ownerDocument();
  LocalFrame* frame = document ? document->GetFrame() : nullptr;
  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  if (!script_state)
    return Response::ServerError("Element not associated with a document.");

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
    return Response::ServerError("Element not associated with a document.");
  return Response::Success();
}

String InspectorAnimationAgent::CreateCSSId(blink::Animation& animation) {
  static CSSPropertyID g_animation_properties[] = {
      CSSPropertyID::kAnimationDelayStart,
      CSSPropertyID::kAnimationDelayEnd,
      CSSPropertyID::kAnimationDirection,
      CSSPropertyID::kAnimationDuration,
      CSSPropertyID::kAnimationFillMode,
      CSSPropertyID::kAnimationIterationCount,
      CSSPropertyID::kAnimationName,
      CSSPropertyID::kAnimationTimingFunction,
  };
  static CSSPropertyID g_transition_properties[] = {
      CSSPropertyID::kTransitionDelay,
      CSSPropertyID::kTransitionDuration,
      CSSPropertyID::kTransitionProperty,
      CSSPropertyID::kTransitionTimingFunction,
  };

  auto* effect = To<KeyframeEffect>(animation.effect());
  Vector<CSSPropertyName> css_property_names;
  if (IsA<CSSAnimation>(animation)) {
    for (CSSPropertyID property : g_animation_properties)
      css_property_names.push_back(CSSPropertyName(property));
  } else if (auto* css_transition = DynamicTo<CSSTransition>(animation)) {
    for (CSSPropertyID property : g_transition_properties)
      css_property_names.push_back(CSSPropertyName(property));
    css_property_names.push_back(css_transition->TransitionCSSPropertyName());
  } else {
    NOTREACHED();
  }

  Element* element = effect->EffectTarget();
  HeapVector<Member<CSSStyleDeclaration>> styles =
      css_agent_->MatchingStyles(element);
  Digestor digestor(kHashAlgorithmSha1);
  digestor.UpdateUtf8(IsA<CSSTransition>(animation)
                          ? AnimationType::CSSTransition
                          : AnimationType::CSSAnimation);
  digestor.UpdateUtf8(animation.id());
  for (const CSSPropertyName& name : css_property_names) {
    CSSStyleDeclaration* style =
        css_agent_->FindEffectiveDeclaration(name, styles);
    // Ignore inline styles.
    if (!style || !style->ParentStyleSheet() || !style->parentRule() ||
        style->parentRule()->GetType() != CSSRule::kStyleRule)
      continue;
    digestor.UpdateUtf8(name.ToAtomicString());
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
  auto it = id_to_animation_.find(id);
  if (it == id_to_animation_.end()) {
    result = nullptr;
    return Response::ServerError("Could not find animation with given id");
  }
  result = it->value;
  return Response::Success();
}

DocumentTimeline& InspectorAnimationAgent::ReferenceTimeline() {
  return inspected_frames_->Root()->GetDocument()->Timeline();
}

double InspectorAnimationAgent::NormalizedStartTime(
    blink::Animation& animation) {
  double time_ms = Timing::NullValue();
  absl::optional<AnimationTimeDelta> start_time = animation.StartTimeInternal();
  if (start_time) {
    time_ms = start_time.value().InMillisecondsF();
  }

  auto* document_timeline = DynamicTo<DocumentTimeline>(animation.timeline());
  if (document_timeline) {
    if (ReferenceTimeline().PlaybackRate() == 0) {
      time_ms += ReferenceTimeline().CurrentTimeMilliseconds().value_or(
                     Timing::NullValue()) -
                 document_timeline->CurrentTimeMilliseconds().value_or(
                     Timing::NullValue());
    } else {
      time_ms +=
          (document_timeline->ZeroTime() - ReferenceTimeline().ZeroTime())
              .InMillisecondsF() *
          ReferenceTimeline().PlaybackRate();
    }
  }
  // Round to the closest microsecond.
  return std::round(time_ms * 1000) / 1000;
}

void InspectorAnimationAgent::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
  visitor->Trace(css_agent_);
  visitor->Trace(id_to_animation_);
  visitor->Trace(id_to_animation_clone_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
