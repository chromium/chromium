// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_animation_agent.h"

#include <memory>

#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_computed_effect_timing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_optional_effect_timing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_string_unrestricteddouble.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/animation/css/css_animation.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/css/css_transition.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/effect_model.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/scroll_snapshot_timeline.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/animation/view_timeline.h"
#include "third_party/blink/renderer/core/css/css_keyframe_rule.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_style_sheet.h"
#include "third_party/blink/renderer/core/inspector/protocol/animation.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

namespace {

protocol::DOM::ScrollOrientation ToScrollOrientation(
    ScrollSnapshotTimeline::ScrollAxis scroll_axis_enum,
    bool is_horizontal_writing_mode) {
  switch (scroll_axis_enum) {
    case ScrollSnapshotTimeline::ScrollAxis::kBlock:
      return is_horizontal_writing_mode
                 ? protocol::DOM::ScrollOrientationEnum::Vertical
                 : protocol::DOM::ScrollOrientationEnum::Horizontal;
    case ScrollSnapshotTimeline::ScrollAxis::kInline:
      return is_horizontal_writing_mode
                 ? protocol::DOM::ScrollOrientationEnum::Horizontal
                 : protocol::DOM::ScrollOrientationEnum::Vertical;
    case ScrollSnapshotTimeline::ScrollAxis::kX:
      return protocol::DOM::ScrollOrientationEnum::Horizontal;
    case ScrollSnapshotTimeline::ScrollAxis::kY:
      return protocol::DOM::ScrollOrientationEnum::Vertical;
  }
}

double NormalizedDuration(
    V8UnionCSSNumericValueOrStringOrUnrestrictedDouble* duration) {
  if (duration->IsUnrestrictedDouble()) {
    return duration->GetAsUnrestrictedDouble();
  }

  if (duration->IsCSSNumericValue()) {
    CSSUnitValue* percentage_unit_value = duration->GetAsCSSNumericValue()->to(
        CSSPrimitiveValue::UnitType::kPercentage);
    if (percentage_unit_value) {
      return percentage_unit_value->value();
    }
  }
  return 0;
}

double AsDoubleOrZero(Timing::V8Delay* value) {
  if (!value->IsDouble())
    return 0;

  return value->GetAsDouble();
}

}  // namespace

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

String InspectorAnimationAgent::AnimationDisplayName(
    const Animation& animation) {
  if (!animation.id().empty()) {
    return animation.id();
  } else if (auto* css_animation = DynamicTo<CSSAnimation>(animation)) {
    return css_animation->animationName();
  } else if (auto* css_transition = DynamicTo<CSSTransition>(animation)) {
    return css_transition->transitionProperty();
  } else {
    return "";
  }
}

void InspectorAnimationAgent::Restore() {
  if (enabled_.Get()) {
    instrumenting_agents_->AddInspectorAnimationAgent(this);
    setPlaybackRate(playback_rate_.Get());
  }
}

void InspectorAnimationAgent::InvalidateInternalState() {
  id_to_animation_snapshot_.clear();
  id_to_animation_.clear();
  cleared_animations_.clear();
  notify_animation_updated_tasks_.clear();
}

protocol::Response InspectorAnimationAgent::enable() {
  enabled_.Set(true);
  instrumenting_agents_->AddInspectorAnimationAgent(this);

  Document* document = inspected_frames_->Root()->GetDocument();
  DocumentAnimations& document_animations = document->GetDocumentAnimations();
  HeapVector<Member<Animation>> animations =
      document_animations.getAnimations(document->GetTreeScope());
  for (Animation* animation : animations) {
    const String& animation_id = String::Number(animation->SequenceNumber());
    blink::Animation::AnimationPlayState play_state =
        animation->CalculateAnimationPlayState();
    bool is_play_state_running_or_finished =
        play_state == blink::Animation::kRunning ||
        play_state == blink::Animation::kFinished;
    if (!is_play_state_running_or_finished ||
        cleared_animations_.Contains(animation_id) ||
        id_to_animation_.Contains(animation_id)) {
      continue;
    }

    AnimationSnapshot* snapshot;
    if (id_to_animation_snapshot_.Contains(animation_id)) {
      snapshot = id_to_animation_snapshot_.at(animation_id);
    } else {
      snapshot = MakeGarbageCollected<AnimationSnapshot>();
      id_to_animation_snapshot_.Set(animation_id, snapshot);
    }

    this->CompareAndUpdateInternalSnapshot(*animation, snapshot);
    id_to_animation_.Set(animation_id, animation);
    GetFrontend()->animationCreated(animation_id);
    GetFrontend()->animationStarted(BuildObjectForAnimation(*animation));
  }

  return protocol::Response::Success();
}

protocol::Response InspectorAnimationAgent::disable() {
  setPlaybackRate(1.0);
  enabled_.Clear();
  instrumenting_agents_->RemoveInspectorAnimationAgent(this);
  InvalidateInternalState();
  return protocol::Response::Success();
}

void InspectorAnimationAgent::DidCommitLoadForLocalFrame(LocalFrame* frame) {
  if (frame == inspected_frames_->Root()) {
    InvalidateInternalState();
  }
  setPlaybackRate(playback_rate_.Get());
}

static std::unique_ptr<protocol::Animation::ViewOrScrollTimeline>
BuildObjectForViewOrScrollTimeline(AnimationTimeline* timeline) {
  ScrollSnapshotTimeline* scroll_snapshot_timeline =
      DynamicTo<ScrollSnapshotTimeline>(timeline);
  if (scroll_snapshot_timeline) {
    Node* resolved_source = scroll_snapshot_timeline->ResolvedSource();
    if (!resolved_source) {
      return nullptr;
    }

    LayoutBox* scroll_container = scroll_snapshot_timeline->ScrollContainer();
    if (!scroll_container) {
      return nullptr;
    }

    std::unique_ptr<protocol::Animation::ViewOrScrollTimeline> timeline_object =
        protocol::Animation::ViewOrScrollTimeline::create()
            .setSourceNodeId(IdentifiersFactory::IntIdForNode(resolved_source))
            .setAxis(ToScrollOrientation(
                scroll_snapshot_timeline->GetAxis(),
                scroll_container->IsHorizontalWritingMode()))
            .build();
    std::optional<ScrollSnapshotTimeline::ScrollOffsets> scroll_offsets =
        scroll_snapshot_timeline->GetResolvedScrollOffsets();
    if (scroll_offsets.has_value()) {
      timeline_object->setStartOffset(scroll_offsets->start);
      timeline_object->setEndOffset(scroll_offsets->end);
    }

    ViewTimeline* view_timeline =
        DynamicTo<ViewTimeline>(scroll_snapshot_timeline);
    if (view_timeline && view_timeline->subject()) {
      timeline_object->setSubjectNodeId(
          IdentifiersFactory::IntIdForNode(view_timeline->subject()));
    }

    return timeline_object;
  }

  return nullptr;
}

static std::unique_ptr<protocol::Animation::AnimationEffect>
BuildObjectForAnimationEffect(KeyframeEffect* effect) {
  ComputedEffectTiming* computed_timing = effect->getComputedTiming();
  double delay = AsDoubleOrZero(computed_timing->delay());
  double end_delay = AsDoubleOrZero(computed_timing->endDelay());
  String easing = effect->SpecifiedTiming().timing_function->ToString();

  std::unique_ptr<protocol::Animation::AnimationEffect> animation_object =
      protocol::Animation::AnimationEffect::create()
          .setDelay(delay)
          .setEndDelay(end_delay)
          .setIterationStart(computed_timing->iterationStart())
          .setIterations(computed_timing->iterations())
          .setDuration(NormalizedDuration(computed_timing->duration()))
          .setDirection(computed_timing->direction().AsString())
          .setFill(computed_timing->fill().AsString())
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
  double current_time = Timing::NullValue();
  std::optional<AnimationTimeDelta> animation_current_time =
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

  std::unique_ptr<protocol::Animation::ViewOrScrollTimeline>
      view_or_scroll_timeline =
          BuildObjectForViewOrScrollTimeline(animation.TimelineInternal());
  if (view_or_scroll_timeline) {
    animation_object->setViewOrScrollTimeline(
        std::move(view_or_scroll_timeline));
  }
  return animation_object;
}

protocol::Response InspectorAnimationAgent::getPlaybackRate(
    double* playback_rate) {
  *playback_rate = ReferenceTimeline().PlaybackRate();
  return protocol::Response::Success();
}

protocol::Response InspectorAnimationAgent::setPlaybackRate(
    double playback_rate) {
  for (LocalFrame* frame : *inspected_frames_)
    frame->GetDocument()->Timeline().SetPlaybackRate(playback_rate);
  playback_rate_.Set(playback_rate);
  return protocol::Response::Success();
}

protocol::Response InspectorAnimationAgent::getCurrentTime(
    const String& id,
    double* current_time) {
  blink::Animation* animation = nullptr;
  protocol::Response response = AssertAnimation(id, animation);
  if (!response.IsSuccess())
    return response;

  *current_time = Timing::NullValue();
  if (animation->Paused() || !animation->TimelineInternal()->IsActive()) {
    std::optional<AnimationTimeDelta> animation_current_time =
        animation->CurrentTimeInternal();
    if (animation_current_time) {
      *current_time = animation_current_time.value().InMillisecondsF();
    }
  } else {
    // Use startTime where possible since currentTime is limited.
    std::optional<AnimationTimeDelta> animation_start_time =
        animation->StartTimeInternal();
    if (animation_start_time) {
      std::optional<AnimationTimeDelta> timeline_time =
          animation->TimelineInternal()->CurrentTime();
      // TODO(crbug.com/916117): Handle NaN values for scroll linked animations.
      if (timeline_time) {
        *current_time = timeline_time.value().InMillisecondsF() -
                        animation_start_time.value().InMillisecondsF();
      }
    }
  }
  return protocol::Response::Success();
}

protocol::Response InspectorAnimationAgent::setPaused(
    std::unique_ptr<protocol::Array<String>> animation_ids,
    bool paused) {
  for (const String& animation_id : *animation_ids) {
    blink::Animation* animation = nullptr;
    protocol::Response response = AssertAnimation(animation_id, animation);
    if (!response.IsSuccess())
      return response;
    if (paused && !animation->Paused()) {
      // Ensure we restore a current time if the animation is limited.
      std::optional<AnimationTimeDelta> current_time;
      if (!animation->TimelineInternal()->IsActive()) {
        current_time = animation->CurrentTimeInternal();
      } else {
        std::optional<AnimationTimeDelta> start_time =
            animation->StartTimeInternal();
        if (start_time) {
          std::optional<AnimationTimeDelta> timeline_time =
              animation->TimelineInternal()->CurrentTime();
          // TODO(crbug.com/916117): Handle NaN values.
          if (timeline_time) {
            current_time = timeline_time.value() - start_time.value();
          }
        }
      }

      animation->pause();
      if (current_time) {
        animation->SetCurrentTimeInternal(current_time.value());
      }
    } else if (!paused && animation->Paused()) {
      animation->Unpause();
    }
  }
  return protocol::Response::Success();
}

protocol::Response InspectorAnimationAgent::seekAnimations(
    std::unique_ptr<protocol::Array<String>> animation_ids,
    double current_time) {
  for (const String& animation_id : *animation_ids) {
    blink::Animation* animation = nullptr;
    protocol::Response response = AssertAnimation(animation_id, animation);
    if (!response.IsSuccess())
      return response;
    if (!animation->Paused()) {
      animation->play();
    }
    animation->SetCurrentTimeInternal(
        ANIMATION_TIME_DELTA_FROM_MILLISECONDS(current_time));
  }
  return protocol::Response::Success();
}

protocol::Response InspectorAnimationAgent::releaseAnimations(
    std::unique_ptr<protocol::Array<String>> animation_ids) {
  for (const String& animation_id : *animation_ids) {
    auto it = id_to_animation_.find(animation_id);
    if (it != id_to_animation_.end())
      it->value->SetEffectSuppressed(false);

    id_to_animation_.erase(animation_id);
    cleared_animations_.insert(animation_id);
    id_to_animation_snapshot_.erase(animation_id);
    notify_animation_updated_tasks_.erase(animation_id);
  }
  return protocol::Response::Success();
}

protocol::Response InspectorAnimationAgent::setTiming(
    const String& animation_id,
    double duration,
    double delay) {
  blink::Animation* animation = nullptr;
  protocol::Response response = AssertAnimation(animation_id, animation);
  if (!response.IsSuccess())
    return response;

  NonThrowableExceptionState exception_state;

  OptionalEffectTiming* timing = OptionalEffectTiming::Create();
  timing->setDuration(
      MakeGarbageCollected<V8UnionCSSNumericValueOrStringOrUnrestrictedDouble>(
          duration));
  timing->setDelay(MakeGarbageCollected<Timing::V8Delay>(delay));
  animation->effect()->updateTiming(timing, exception_state);
  return protocol::Response::Success();
}

protocol::Response InspectorAnimationAgent::resolveAnimation(
    const String& animation_id,
    std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>*
        result) {
  blink::Animation* animation = nullptr;
  protocol::Response response = AssertAnimation(animation_id, animation);
  if (!response.IsSuccess())
    return response;

  const Element* element =
      To<KeyframeEffect>(animation->effect())->EffectTarget();
  Document* document = element->ownerDocument();
  LocalFrame* frame = document ? document->GetFrame() : nullptr;
  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  if (!script_state) {
    return protocol::Response::ServerError(
        "Element not associated with a document.");
  }

  ScriptState::Scope scope(script_state);
  static const char kAnimationObjectGroup[] = "animation";
  v8_session_->releaseObjectGroup(
      ToV8InspectorStringView(kAnimationObjectGroup));
  *result = v8_session_->wrapObject(
      script_state->GetContext(),
      ToV8Traits<Animation>::ToV8(script_state, animation),
      ToV8InspectorStringView(kAnimationObjectGroup),
      false /* generatePreview */);
  if (!*result) {
    return protocol::Response::ServerError(
        "Element not associated with a document.");
  }
  return protocol::Response::Success();
}

String InspectorAnimationAgent::CreateCSSId(blink::Animation& animation) {
  static CSSPropertyID g_animation_properties[] = {
      CSSPropertyID::kAnimationDelay,
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
    NOTREACHED_IN_MIGRATION();
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

void InspectorAnimationAgent::NotifyAnimationUpdated(
    const String& animation_id) {
  if (!notify_animation_updated_tasks_.Contains(animation_id)) {
    return;
  }

  notify_animation_updated_tasks_.erase(animation_id);
  blink::Animation* animation = id_to_animation_.at(animation_id);
  if (!animation) {
    return;
  }

  blink::Animation::AnimationPlayState play_state =
      animation->CalculateAnimationPlayState();
  if (play_state != blink::Animation::kRunning &&
      play_state != blink::Animation::kFinished) {
    return;
  }

  GetFrontend()->animationUpdated(BuildObjectForAnimation(*animation));
}

bool InspectorAnimationAgent::CompareAndUpdateKeyframesSnapshot(
    KeyframeEffect* keyframe_effect,
    HeapVector<Member<AnimationKeyframeSnapshot>>*
        animation_snapshot_keyframes) {
  bool should_notify_frontend = false;
  const KeyframeEffectModelBase* model = keyframe_effect->Model();
  Vector<double> computed_offsets =
      KeyframeEffectModelBase::GetComputedOffsets(model->GetFrames());
  if (model->GetFrames().size() != animation_snapshot_keyframes->size()) {
    // Notify frontend if there were previous keyframe snapshots and the
    // size has changed. Otherwise we don't notify frontend as it means
    // this is the first initialization of the `animation_snapshot_keyframes`
    // vector.
    if (animation_snapshot_keyframes->size() != 0) {
      should_notify_frontend = true;
    }

    for (wtf_size_t i = 0; i < model->GetFrames().size(); i++) {
      const Keyframe* keyframe = model->GetFrames().at(i);
      if (!keyframe->IsStringKeyframe()) {
        continue;
      }

      const auto* string_keyframe = To<StringKeyframe>(keyframe);
      AnimationKeyframeSnapshot* keyframe_snapshot =
          MakeGarbageCollected<AnimationKeyframeSnapshot>();
      keyframe_snapshot->computed_offset = computed_offsets.at(i);
      keyframe_snapshot->easing = string_keyframe->Easing().ToString();
      animation_snapshot_keyframes->emplace_back(keyframe_snapshot);
    }

    return should_notify_frontend;
  }

  for (wtf_size_t i = 0; i < animation_snapshot_keyframes->size(); i++) {
    AnimationKeyframeSnapshot* keyframe_snapshot =
        animation_snapshot_keyframes->at(i);
    const Keyframe* keyframe = model->GetFrames().at(i);
    if (!keyframe->IsStringKeyframe()) {
      continue;
    }

    const auto* string_keyframe = To<StringKeyframe>(keyframe);
    if (keyframe_snapshot->computed_offset != computed_offsets.at(i)) {
      keyframe_snapshot->computed_offset = computed_offsets.at(i);
      should_notify_frontend = true;
    }

    if (keyframe_snapshot->easing != string_keyframe->Easing().ToString()) {
      keyframe_snapshot->easing = string_keyframe->Easing().ToString();
      should_notify_frontend = true;
    }
  }

  return should_notify_frontend;
}

bool InspectorAnimationAgent::CompareAndUpdateInternalSnapshot(
    blink::Animation& animation,
    AnimationSnapshot* snapshot) {
  blink::Animation::AnimationPlayState new_play_state =
      animation.PendingInternal() ? blink::Animation::AnimationPlayState::kPending
                          : animation.CalculateAnimationPlayState();
  bool should_notify_frontend = false;
  double start_time = NormalizedStartTime(animation);
  if (snapshot->start_time != start_time) {
    snapshot->start_time = start_time;
    should_notify_frontend = true;
  }

  if (snapshot->play_state != new_play_state) {
    snapshot->play_state = new_play_state;
    should_notify_frontend = true;
  }

  if (animation.effect()) {
    ComputedEffectTiming* computed_timing =
        animation.effect()->getComputedTiming();
    if (computed_timing) {
      double duration = NormalizedDuration(computed_timing->duration());
      double delay = AsDoubleOrZero(computed_timing->delay());
      double end_delay = AsDoubleOrZero(computed_timing->endDelay());
      double iterations = computed_timing->iterations();
      String easing = computed_timing->easing();
      if (snapshot->duration != duration) {
        snapshot->duration = duration;
        should_notify_frontend = true;
      }

      if (snapshot->delay != delay) {
        snapshot->delay = delay;
        should_notify_frontend = true;
      }

      if (snapshot->end_delay != end_delay) {
        snapshot->end_delay = end_delay;
        should_notify_frontend = true;
      }

      if (snapshot->iterations != iterations) {
        snapshot->iterations = iterations;
        should_notify_frontend = true;
      }

      if (snapshot->timing_function != easing) {
        snapshot->timing_function = easing;
        should_notify_frontend = true;
      }
    }

    if (KeyframeEffect* keyframe_effect =
            DynamicTo<KeyframeEffect>(animation.effect())) {
      if (CompareAndUpdateKeyframesSnapshot(keyframe_effect,
                                            &snapshot->keyframes)) {
        should_notify_frontend = true;
      }
    }
  }

  ScrollSnapshotTimeline* scroll_snapshot_timeline =
      DynamicTo<ScrollSnapshotTimeline>(animation.TimelineInternal());
  if (scroll_snapshot_timeline) {
    std::optional<ScrollSnapshotTimeline::ScrollOffsets> scroll_offsets =
        scroll_snapshot_timeline->GetResolvedScrollOffsets();
    if (scroll_offsets.has_value()) {
      if (scroll_offsets->start != snapshot->start_offset) {
        snapshot->start_offset = scroll_offsets->start;
        should_notify_frontend = true;
      }

      if (scroll_offsets->end != snapshot->end_offset) {
        snapshot->end_offset = scroll_offsets->end;
        should_notify_frontend = true;
      }
    }
  }

  return should_notify_frontend;
}

void InspectorAnimationAgent::AnimationUpdated(blink::Animation* animation) {
  const String& animation_id = String::Number(animation->SequenceNumber());
  // We no longer care about animations that have been released.
  if (cleared_animations_.Contains(animation_id)) {
    return;
  }

  // Initialize the animation snapshot to keep track of animation state changes
  // on `AnimationUpdated` probe calls.
  // * If a snapshot is found, it means there were previous calls to
  // AnimationUpdated so, we retrieve the snapshot for comparison.
  // * If a snapshot is not found, it means this is the animation's first call
  // to AnimationUpdated so, we create a snapshot and store the play state for
  // future comparisons.
  AnimationSnapshot* snapshot;
  blink::Animation::AnimationPlayState new_play_state =
      animation->PendingInternal() ? blink::Animation::AnimationPlayState::kPending
                           : animation->CalculateAnimationPlayState();
  blink::Animation::AnimationPlayState old_play_state =
      blink::Animation::AnimationPlayState::kIdle;
  if (id_to_animation_snapshot_.Contains(animation_id)) {
    snapshot = id_to_animation_snapshot_.at(animation_id);
    old_play_state = snapshot->play_state;
    snapshot->play_state = new_play_state;
  } else {
    snapshot = MakeGarbageCollected<AnimationSnapshot>();
    snapshot->play_state = new_play_state;
    id_to_animation_snapshot_.Set(animation_id, snapshot);
  }

  // Do not record pending animations in `id_to_animation_` and do not notify
  // frontend.
  if (new_play_state == blink::Animation::AnimationPlayState::kPending) {
    return;
  }

  // Record newly starting animations only once.
  if (old_play_state != new_play_state) {
    switch (new_play_state) {
      case blink::Animation::kRunning:
      case blink::Animation::kFinished: {
        if (id_to_animation_.Contains(animation_id)) {
          break;
        }

        this->CompareAndUpdateInternalSnapshot(*animation, snapshot);
        id_to_animation_.Set(animation_id, animation);
        GetFrontend()->animationStarted(BuildObjectForAnimation(*animation));
        break;
      }
      case blink::Animation::kIdle:
      case blink::Animation::kPaused:
        GetFrontend()->animationCanceled(animation_id);
        break;
      case blink::Animation::kUnset:
        // No-op for now.
        break;
      case blink::Animation::kPending:
        NOTREACHED_IN_MIGRATION();
    }
  }

  // We only send animationUpdated events for running or finished animations.
  if (new_play_state != blink::Animation::kRunning &&
      new_play_state != blink::Animation::kFinished) {
    return;
  }

  bool should_notify_frontend =
      this->CompareAndUpdateInternalSnapshot(*animation, snapshot);
  if (should_notify_frontend &&
      !notify_animation_updated_tasks_.Contains(animation_id)) {
    notify_animation_updated_tasks_.insert(animation_id);
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        inspected_frames_->Root()->GetTaskRunner(TaskType::kInternalInspector);
    task_runner->PostDelayedTask(
        FROM_HERE,
        WTF::BindOnce(&InspectorAnimationAgent::NotifyAnimationUpdated,
                      WrapPersistent(weak_factory_.GetWeakCell()),
                      animation_id),
        base::Milliseconds(50));
  }
}

void InspectorAnimationAgent::DidClearDocumentOfWindowObject(
    LocalFrame* frame) {
  if (!enabled_.Get())
    return;
  DCHECK(frame->GetDocument());
  frame->GetDocument()->Timeline().SetPlaybackRate(
      ReferenceTimeline().PlaybackRate());
}

protocol::Response InspectorAnimationAgent::AssertAnimation(
    const String& id,
    blink::Animation*& result) {
  auto it = id_to_animation_.find(id);
  if (it == id_to_animation_.end()) {
    result = nullptr;
    return protocol::Response::ServerError(
        "Could not find animation with given id");
  }
  result = it->value;
  return protocol::Response::Success();
}

DocumentTimeline& InspectorAnimationAgent::ReferenceTimeline() {
  return inspected_frames_->Root()->GetDocument()->Timeline();
}

double InspectorAnimationAgent::NormalizedStartTime(
    blink::Animation& animation) {
  V8CSSNumberish* start_time = animation.startTime();
  if (!start_time) {
    return 0;
  }

  if (start_time->IsDouble()) {
    double time_ms = start_time->GetAsDouble();
    auto* document_timeline =
        DynamicTo<DocumentTimeline>(animation.TimelineInternal());
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

  if (start_time->IsCSSNumericValue()) {
    CSSUnitValue* percent_unit_value = start_time->GetAsCSSNumericValue()->to(
        CSSPrimitiveValue::UnitType::kPercentage);
    return percent_unit_value->value();
  }

  return 0;
}

void InspectorAnimationAgent::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
  visitor->Trace(css_agent_);
  visitor->Trace(id_to_animation_snapshot_);
  visitor->Trace(id_to_animation_);
  visitor->Trace(weak_factory_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
