// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/worklet_animation.h"

#include <optional>

#include "cc/animation/animation_timeline.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_animationeffect_animationeffectsequence.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_documenttimeline_scrolltimeline.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline_util.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/animation/worklet_animation_controller.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/modules/animationworklet/css_animation_worklet.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

bool ConvertAnimationEffects(
    const V8UnionAnimationEffectOrAnimationEffectSequence* effects,
    HeapVector<Member<KeyframeEffect>>& keyframe_effects,
    String& error_string) {
  DCHECK(effects);
  DCHECK(keyframe_effects.empty());

  // Currently we only support KeyframeEffect.
  switch (effects->GetContentType()) {
    case V8UnionAnimationEffectOrAnimationEffectSequence::ContentType::
        kAnimationEffect: {
      AnimationEffect* effect = effects->GetAsAnimationEffect();
      KeyframeEffect* keyframe_effect = DynamicTo<KeyframeEffect>(effect);
      if (!keyframe_effect) {
        error_string = "Effect must be a KeyframeEffect object";
        return false;
      }
      keyframe_effects.push_back(keyframe_effect);
      break;
    }
    case V8UnionAnimationEffectOrAnimationEffectSequence::ContentType::
        kAnimationEffectSequence: {
      const HeapVector<Member<AnimationEffect>>& effect_sequence =
          effects->GetAsAnimationEffectSequence();
      keyframe_effects.ReserveInitialCapacity(effect_sequence.size());
      for (const auto& effect : effect_sequence) {
        KeyframeEffect* keyframe_effect =
            DynamicTo<KeyframeEffect>(effect.Get());
        if (!keyframe_effect) {
          error_string = "Effects must all be KeyframeEffect objects";
          return false;
        }
        keyframe_effects.push_back(keyframe_effect);
      }
      break;
    }
  }

  if (keyframe_effects.empty()) {
    error_string = "Effects array must be non-empty";
    return false;
  }

  if (keyframe_effects.size() > 1 &&
      !RuntimeEnabledFeatures::GroupEffectEnabled()) {
    error_string = "Multiple effects are not currently supported";
    return false;
  }

  return true;
}

bool IsActive(const Animation::AnimationPlayState& state) {
  switch (state) {
    case Animation::kIdle:
    case Animation::kPending:
      return false;
    case Animation::kRunning:
    case Animation::kPaused:
      return true;
    default:
      // kUnset and kFinished are not used in WorkletAnimation.
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

bool ValidateTimeline(const V8UnionDocumentTimelineOrScrollTimeline* timeline,
                      String& error_string) {
  if (!timeline)
    return true;
  if (timeline->IsScrollTimeline()) {
    // crbug.com/1238130 Add support for progress based timelines to worklet
    // animations
    error_string = "ScrollTimeline is not yet supported for worklet animations";
    return false;
  }
  return true;
}

AnimationTimeline* ConvertAnimationTimeline(
    const Document& document,
    const V8UnionDocumentTimelineOrScrollTimeline* timeline) {
  if (!timeline)
    return &document.Timeline();
  switch (timeline->GetContentType()) {
    case V8UnionDocumentTimelineOrScrollTimeline::ContentType::
        kDocumentTimeline:
      return timeline->GetAsDocumentTimeline();
    case V8UnionDocumentTimelineOrScrollTimeline::ContentType::kScrollTimeline:
      return timeline->GetAsScrollTimeline();
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void StartEffectOnCompositor(CompositorAnimation* animation,
                             KeyframeEffect* effect) {
  DCHECK(effect);
  DCHECK(effect->EffectTarget());
  Element& target = *effect->EffectTarget();
  effect->Model()->SnapshotAllCompositorKeyframesIfNecessary(
      target, target.ComputedStyleRef(), target.ParentComputedStyle());

  int group = 0;
  std::optional<double> start_time = std::nullopt;

  // Normally the playback rate of a blink animation gets translated into
  // equivalent playback rate of cc::KeyframeModels.
  // This has worked for regular animations since their current time was not
  // exposed in cc. However, for worklet animations this does not work because
  // the current time is exposed and it is an animation level concept as
  // opposed to a keyframe model level concept.
  // So it makes sense here that we use "1" as playback rate for KeyframeModels
  // and separately plumb the playback rate to cc worklet animation.
  // TODO(majidvp): Remove playbackRate from KeyframeModel in favor of having
  // it on animation. https://crbug.com/925373.
  double playback_rate = 1;

  effect->StartAnimationOnCompositor(group, start_time, base::TimeDelta(),
                                     playback_rate, animation);
}

unsigned NextSequenceNumber() {
  // TODO(majidvp): This should actually come from the same source as other
  // animation so that they have the correct ordering.
  static unsigned next = 0;
  return ++next;
}

double ToMilliseconds(std::optional<base::TimeDelta> time) {
  return time ? time->InMillisecondsF()
              : std::numeric_limits<double>::quiet_NaN();
}

// Calculates start time backwards from the current time and
// timeline.currentTime.
std::optional<base::TimeDelta> CalculateStartTime(base::TimeDelta current_time,
                                                  double playback_rate,
                                                  AnimationTimeline& timeline) {
  // Handle some special cases, note |playback_rate| can never be 0 before
  // SetPlaybackRateInternal has a DCHECK for that.
  DCHECK_NE(playback_rate, 0);
  if (current_time.is_max())
    return base::Milliseconds(0);
  if (current_time.is_min())
    return base::TimeDelta::Max();
  std::optional<double> timeline_current_time_ms =
      timeline.CurrentTimeMilliseconds();
  return base::Milliseconds(timeline_current_time_ms.value()) -
         (current_time / playback_rate);
}

}  // namespace

WorkletAnimation* WorkletAnimation::Create(
    ScriptState* script_state,
    const String& animator_name,
    const V8UnionAnimationEffectOrAnimationEffectSequence* effects,
    ExceptionState& exception_state) {
  return Create(script_state, animator_name, effects, nullptr, ScriptValue(),
                exception_state);
}

WorkletAnimation* WorkletAnimation::Create(
    ScriptState* script_state,
    const String& animator_name,
    const V8UnionAnimationEffectOrAnimationEffectSequence* effects,
    const V8UnionDocumentTimelineOrScrollTimeline* timeline,
    ExceptionState& exception_state) {
  return Create(script_state, animator_name, effects, timeline, ScriptValue(),
                exception_state);
}

WorkletAnimation* WorkletAnimation::Create(
    ScriptState* script_state,
    const String& animator_name,
    const V8UnionAnimationEffectOrAnimationEffectSequence* effects,
    const V8UnionDocumentTimelineOrScrollTimeline* timeline,
    const ScriptValue& options,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  HeapVector<Member<KeyframeEffect>> keyframe_effects;
  String error_string;
  if (!ConvertAnimationEffects(effects, keyframe_effects, error_string)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      error_string);
    return nullptr;
  }

  if (!ValidateTimeline(timeline, error_string)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      error_string);
    return nullptr;
  }

  Document& document = *LocalDOMWindow::From(script_state)->document();
  if (!document.GetWorkletAnimationController().IsAnimatorRegistered(
          animator_name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The animator '" + animator_name + "' has not yet been registered.");
    return nullptr;
  }

  AnimationWorklet* worklet =
      CSSAnimationWorklet::animationWorklet(script_state);

  WorkletAnimationId id = worklet->NextWorkletAnimationId();

  AnimationTimeline* animation_timeline =
      ConvertAnimationTimeline(document, timeline);

  scoped_refptr<SerializedScriptValue> animation_options;
  if (!options.IsEmpty()) {
    animation_options = SerializedScriptValue::Serialize(
        script_state->GetIsolate(), options.V8Value(),
        SerializedScriptValue::SerializeOptions(
            SerializedScriptValue::kNotForStorage),
        exception_state);
    if (exception_state.HadException())
      return nullptr;
  }

  WorkletAnimation* animation = MakeGarbageCollected<WorkletAnimation>(
      id, animator_name, document, keyframe_effects, animation_timeline,
      std::move(animation_options));

  return animation;
}

WorkletAnimation::WorkletAnimation(
    WorkletAnimationId id,
    const String& animator_name,
    Document& document,
    const HeapVector<Member<KeyframeEffect>>& effects,
    AnimationTimeline* timeline,
    scoped_refptr<SerializedScriptValue> options)
    : sequence_number_(NextSequenceNumber()),
      id_(id),
      animator_name_(animator_name),
      play_state_(Animation::kIdle),
      last_play_state_(play_state_),
      playback_rate_(1),
      was_timeline_active_(false),
      document_(document),
      effects_(effects),
      timeline_(timeline),
      options_(std::make_unique<WorkletAnimationOptions>(options)),
      effect_needs_restart_(false) {
  DCHECK(IsMainThread());

  auto timings = base::MakeRefCounted<base::RefCountedData<Vector<Timing>>>();
  timings->data.ReserveInitialCapacity(effects_.size());

  auto normalized_timings = base::MakeRefCounted<
      base::RefCountedData<Vector<Timing::NormalizedTiming>>>();
  normalized_timings->data.ReserveInitialCapacity(effects_.size());

  DCHECK_GE(effects_.size(), 1u);
  for (auto& effect : effects_) {
    AnimationEffect* target_effect = effect;
    target_effect->Attach(this);
    local_times_.push_back(std::nullopt);
    timings->data.push_back(target_effect->SpecifiedTiming());
    normalized_timings->data.push_back(target_effect->NormalizedTiming());
  }
  effect_timings_ = std::make_unique<WorkletAnimationEffectTimings>(
      timings, normalized_timings);
}

String WorkletAnimation::playState() {
  DCHECK(IsMainThread());
  return Animation::PlayStateString(play_state_);
}

void WorkletAnimation::play(ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  if (play_state_ == Animation::kPending || play_state_ == Animation::kRunning)
    return;

  if (play_state_ == Animation::kPaused) {
    // If we have ever started before then just unpause otherwise we need to
    // start the animation.
    if (has_started_) {
      SetPlayState(Animation::kPending);
      SetCurrentTime(CurrentTime());
      InvalidateCompositingState();
      return;
    }
  } else {
    DCHECK(!IsCurrentTimeInitialized());
  }

  String failure_message;
  if (!CheckCanStart(&failure_message)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      failure_message);
    return;
  }

  document_->GetWorkletAnimationController().AttachAnimation(*this);
  // While animation is pending, it hold time at Zero, see:
  // https://drafts.csswg.org/web-animations-1/#playing-an-animation-section
  SetPlayState(Animation::kPending);
  SetCurrentTime(InitialCurrentTime());
  has_started_ = true;

  for (auto& effect : effects_) {
    Element* target = effect->EffectTarget();
    if (!target)
      continue;

    // TODO(crbug.com/896249): Currently we have to keep a set of worklet
    // animations in ElementAnimations so that the compositor knows that there
    // are active worklet animations running. Ideally, this should be done via
    // the regular Animation path, i.e., unify the logic between the two
    // Animations.
    target->EnsureElementAnimations().GetWorkletAnimations().insert(this);
    target->SetNeedsAnimationStyleRecalc();
  }
}

std::optional<double> WorkletAnimation::currentTime() {
  std::optional<base::TimeDelta> current_time = CurrentTime();
  if (!current_time)
    return std::nullopt;
  return ToMilliseconds(current_time.value());
}

std::optional<double> WorkletAnimation::startTime() {
  // The timeline may have become newly active or inactive, which then can cause
  // the start time to change.
  UpdateCurrentTimeIfNeeded();
  if (!start_time_)
    return std::nullopt;
  return ToMilliseconds(start_time_.value());
}

void WorkletAnimation::pause(ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  if (play_state_ == Animation::kPaused)
    return;

  // If animation is pending it means we have not sent an update to
  // compositor. Since we are pausing, immediately start the animation
  // which updates start time and marks animation as main thread.
  // This ensures we have a valid current time to hold.
  if (play_state_ == Animation::kPending)
    StartOnMain();

  // If animation is playing then we should hold the current time
  // otherwise hold zero.
  SetPlayState(Animation::kPaused);
  std::optional<base::TimeDelta> new_current_time =
      IsCurrentTimeInitialized() ? CurrentTime() : InitialCurrentTime();
  DCHECK(new_current_time);
  SetCurrentTime(new_current_time);
}

void WorkletAnimation::cancel() {
  DCHECK(IsMainThread());
  if (play_state_ == Animation::kIdle)
    return;
  document_->GetWorkletAnimationController().DetachAnimation(*this);
  if (compositor_animation_) {
    GetEffect()->CancelAnimationOnCompositor(compositor_animation_.get());
    DestroyCompositorAnimation();
  }
  has_started_ = false;
  local_times_.Fill(std::nullopt);
  running_on_main_thread_ = false;
  // TODO(crbug.com/883312): Because this animation has been detached and will
  // not receive updates anymore, we have to update its value upon cancel.
  // Similar to regular animations, we should not detach them immediately and
  // update the value in the next frame.
  if (IsActive(play_state_)) {
    for (auto& effect : effects_) {
      effect->UpdateInheritedTime(std::nullopt,
                                  /* is_idle */ false, playback_rate_,
                                  kTimingUpdateOnDemand);
    }
  }
  SetPlayState(Animation::kIdle);
  SetCurrentTime(std::nullopt);

  for (auto& effect : effects_) {
    Element* target = effect->EffectTarget();
    if (!target)
      continue;
    // TODO(crbug.com/896249): Currently we have to keep a set of worklet
    // animations in ElementAnimations so that the compositor knows that there
    // are active worklet animations running. Ideally, this should be done via
    // the regular Animation path, i.e., unify the logic between the two
    // Animations.
    target->EnsureElementAnimations().GetWorkletAnimations().erase(this);
    target->SetNeedsAnimationStyleRecalc();
  }
}

bool WorkletAnimation::Playing() const {
  return play_state_ == Animation::kRunning;
}

void WorkletAnimation::UpdateIfNecessary() {
  // TODO(crbug.com/833846): This is updating more often than necessary. This
  // gets fixed once WorkletAnimation becomes a subclass of Animation.
  Update(kTimingUpdateOnDemand);
}

double WorkletAnimation::playbackRate(ScriptState* script_state) const {
  return playback_rate_;
}

void WorkletAnimation::setPlaybackRate(ScriptState* script_state,
                                       double playback_rate) {
  if (playback_rate == playback_rate_)
    return;

  // TODO(https://crbug.com/821910): Implement 0 playback rate after pause()
  // support is in.
  if (!playback_rate) {
    if (document_->GetFrame() && ExecutionContext::From(script_state)) {
      document_->GetFrame()->Console().AddMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::ConsoleMessageSource::kJavaScript,
              mojom::ConsoleMessageLevel::kWarning,
              "WorkletAnimation currently does not support "
              "playback rate of Zero."));
    }
    return;
  }

  SetPlaybackRateInternal(playback_rate);
}

void WorkletAnimation::SetPlaybackRateInternal(double playback_rate) {
  DCHECK(std::isfinite(playback_rate));
  DCHECK_NE(playback_rate, playback_rate_);
  DCHECK(playback_rate);

  std::optional<base::TimeDelta> previous_current_time = CurrentTime();
  playback_rate_ = playback_rate;
  // Update startTime in order to maintain previous currentTime and, as a
  // result, prevent the animation from jumping.
  if (previous_current_time)
    SetCurrentTime(previous_current_time);

  if (Playing())
    document_->GetWorkletAnimationController().InvalidateAnimation(*this);
}

void WorkletAnimation::EffectInvalidated() {
  InvalidateCompositingState();
}

void WorkletAnimation::Update(TimingUpdateReason reason) {
  if (play_state_ != Animation::kRunning && play_state_ != Animation::kPaused)
    return;

  DCHECK_EQ(effects_.size(), local_times_.size());
  for (wtf_size_t i = 0; i < effects_.size(); ++i) {
    effects_[i]->UpdateInheritedTime(
        local_times_[i]
            ? std::make_optional(AnimationTimeDelta(local_times_[i].value()))
            : std::nullopt,
        /* is_idle */ false, playback_rate_, reason);
  }
}

bool WorkletAnimation::CheckCanStart(String* failure_message) {
  DCHECK(IsMainThread());

  for (auto& effect : effects_) {
    if (effect->Model()->HasFrames())
      continue;
    *failure_message = "Animation effect has no keyframes";
    return false;
  }

  return true;
}

void WorkletAnimation::SetCurrentTime(
    std::optional<base::TimeDelta> current_time) {
  DCHECK(timeline_);
  DCHECK(current_time || play_state_ == Animation::kIdle ||
         play_state_ == Animation::kUnset);
  // The procedure either:
  // 1) updates the hold time (for paused animations, non-existent or inactive
  //    timeline)
  // 2) updates the start time (for playing animations)
  bool should_hold =
      play_state_ == Animation::kPaused || !current_time || !IsTimelineActive();
  if (should_hold) {
    start_time_ = std::nullopt;
    hold_time_ = current_time;
  } else {
    start_time_ =
        CalculateStartTime(current_time.value(), playback_rate_, *timeline_);
    hold_time_ = std::nullopt;
  }
  last_current_time_ = current_time;
  was_timeline_active_ = IsTimelineActive();
}

void WorkletAnimation::UpdateCompositingState() {
  DCHECK(play_state_ != Animation::kIdle && play_state_ != Animation::kUnset);

  if (play_state_ == Animation::kPending) {
#if DCHECK_IS_ON()
    String warning_message;
    DCHECK(CheckCanStart(&warning_message));
    DCHECK(warning_message.empty());
#endif  // DCHECK_IS_ON()
    if (StartOnCompositor())
      return;
    StartOnMain();
  } else if (play_state_ == Animation::kRunning) {
    // TODO(majidvp): If keyframes have changed then it may be possible to now
    // run the animation on compositor. The current logic does not allow this
    // switch from main to compositor to happen. https://crbug.com/972691.
    if (!running_on_main_thread_) {
      if (!UpdateOnCompositor()) {
        // When an animation that is running on compositor loses the target, it
        // falls back to main thread. We need to initialize the last play state
        // before this transition to avoid re-adding the same animation to the
        // worklet.
        last_play_state_ = play_state_;

        StartOnMain();
      }
    }
  }
  DCHECK(running_on_main_thread_ != !!compositor_animation_)
      << "Active worklet animation should either run on main or compositor";
}

void WorkletAnimation::InvalidateCompositingState() {
  effect_needs_restart_ = true;
  document_->GetWorkletAnimationController().InvalidateAnimation(*this);
}

void WorkletAnimation::StartOnMain() {
  running_on_main_thread_ = true;
  std::optional<base::TimeDelta> current_time =
      IsCurrentTimeInitialized() ? CurrentTime() : InitialCurrentTime();
  DCHECK(current_time);
  SetPlayState(Animation::kRunning);
  SetCurrentTime(current_time);
}

bool WorkletAnimation::CanStartOnCompositor() {
  if (effects_.size() > 1) {
    // Compositor doesn't support multiple effects but they can be run via main.
    return false;
  }

  if (!GetEffect()->EffectTarget())
    return false;

  Element& target = *GetEffect()->EffectTarget();

  // TODO(crbug.com/836393): This should not be possible but it is currently
  // happening and needs to be investigated/fixed.
  if (!target.GetComputedStyle())
    return false;
  // CheckCanStartAnimationOnCompositor requires that the property-specific
  // keyframe groups have been created. To ensure this we manually snapshot the
  // frames in the target effect.
  // TODO(smcgruer): This shouldn't be necessary - Animation doesn't do this.
  GetEffect()->Model()->SnapshotAllCompositorKeyframesIfNecessary(
      target, target.ComputedStyleRef(), target.ParentComputedStyle());

  CompositorAnimations::FailureReasons failure_reasons =
      GetEffect()->CheckCanStartAnimationOnCompositor(nullptr, playback_rate_);

  if (failure_reasons != CompositorAnimations::kNoFailure)
    return false;

  // If the scroll source is not composited, fall back to main thread.
  if (timeline_->IsScrollTimeline() &&
      !CompositorAnimations::CanStartScrollTimelineOnCompositor(
          To<ScrollTimeline>(*timeline_).ResolvedSource())) {
    return false;
  }

  // TODO(crbug.com/1281413): This function has returned false since the launch
  // of CompositeAfterPaint, but that may not be intended. Should this return
  // true?
  return false;
}

bool WorkletAnimation::StartOnCompositor() {
  DCHECK(IsMainThread());
  // There is no need to proceed if an animation has already started on main
  // thread.
  // TODO(majidvp): If keyframes have changed then it may be possible to now
  // run the animation on compositor. The current logic does not allow this
  // switch from main to compositor to happen. https://crbug.com/972691.
  if (running_on_main_thread_)
    return false;

  if (!CanStartOnCompositor())
    return false;

  if (!compositor_animation_) {
    // TODO(smcgruer): If the scroll source later gets a LayoutBox (e.g. was
    // display:none and now isn't) or the writing mode changes, we need to
    // update the compositor to have the correct orientation and start/end
    // offset information.
    compositor_animation_ = CompositorAnimation::CreateWorkletAnimation(
        id_, animator_name_, playback_rate_, std::move(options_),
        std::move(effect_timings_));
    compositor_animation_->SetAnimationDelegate(this);
  }

  // Register ourselves on the compositor timeline. This will cause our cc-side
  // animation animation to be registered.
  cc::AnimationTimeline* compositor_timeline =
      timeline_ ? timeline_->EnsureCompositorTimeline() : nullptr;
  if (compositor_timeline) {
    if (GetCompositorAnimation()) {
      compositor_timeline->AttachAnimation(
          GetCompositorAnimation()->CcAnimation());
    }
    // Note that while we attach here but we don't detach because the
    // |compositor_timeline| is detached in its destructor.
    if (compositor_timeline->IsScrollTimeline())
      document_->AttachCompositorTimeline(compositor_timeline);
  }

  CompositorAnimations::AttachCompositedLayers(*GetEffect()->EffectTarget(),
                                               compositor_animation_.get());

  // TODO(smcgruer): We need to start all of the effects, not just the first.
  StartEffectOnCompositor(compositor_animation_.get(), GetEffect());
  SetPlayState(Animation::kRunning);
  SetCurrentTime(InitialCurrentTime());
  return true;
}

bool WorkletAnimation::UpdateOnCompositor() {
  if (effect_needs_restart_) {
    // We want to update the keyframe effect on compositor animation without
    // destroying the compositor animation instance. This is achieved by
    // canceling, and starting the blink keyframe effect on compositor.
    effect_needs_restart_ = false;
    GetEffect()->CancelAnimationOnCompositor(compositor_animation_.get());
    if (!CanStartOnCompositor()) {
      // Destroy the compositor animation if the animation is no longer
      // compositable.
      //
      // TODO(821910): At the moment destroying the compositor animation
      // instance also deletes the animator instance which is problematic for
      // stateful animators. A more seamless hand-off is needed here and for
      // pause.
      DestroyCompositorAnimation();
      return false;
    }

    StartEffectOnCompositor(compositor_animation_.get(), GetEffect());
  }

  if (timeline_->IsScrollTimeline())
    timeline_->UpdateCompositorTimeline();

  compositor_animation_->UpdatePlaybackRate(playback_rate_);
  return true;
}

void WorkletAnimation::DestroyCompositorAnimation() {
  if (compositor_animation_ && compositor_animation_->IsElementAttached())
    compositor_animation_->DetachElement();

  cc::AnimationTimeline* compositor_timeline =
      timeline_ ? timeline_->CompositorTimeline() : nullptr;
  if (compositor_timeline && GetCompositorAnimation()) {
    compositor_timeline->DetachAnimation(
        GetCompositorAnimation()->CcAnimation());
  }

  if (compositor_animation_) {
    compositor_animation_->SetAnimationDelegate(nullptr);
    compositor_animation_ = nullptr;
  }
}

KeyframeEffect* WorkletAnimation::GetEffect() const {
  DCHECK(effects_.at(0));
  return effects_.at(0).Get();
}

bool WorkletAnimation::IsActiveAnimation() const {
  return IsActive(play_state_);
}

bool WorkletAnimation::IsTimelineActive() const {
  return timeline_ && timeline_->IsActive();
}

bool WorkletAnimation::IsCurrentTimeInitialized() const {
  return start_time_ || hold_time_;
}

// Returns initial current time of an animation. This method is called when
// calculating initial start time.
// Document-linked animations are initialized with the current time of zero
// and start time of the document timeline current time.
// Scroll-linked animations are initialized with the start time of
// zero (i.e., scroll origin) and the current time corresponding to the current
// scroll position adjusted by the playback rate.
//
// More information at AnimationTimeline::InitialStartTimeForAnimations
//
// TODO(https://crbug.com/986925): The playback rate should be taken into
// consideration when calculating the initial current time.
// https://drafts.csswg.org/web-animations/#playing-an-animation-section
std::optional<base::TimeDelta> WorkletAnimation::InitialCurrentTime() const {
  if (play_state_ == Animation::kIdle || play_state_ == Animation::kUnset ||
      !IsTimelineActive())
    return std::nullopt;

  std::optional<base::TimeDelta> starting_time =
      timeline_->InitialStartTimeForAnimations();
  std::optional<double> current_time = timeline_->CurrentTimeMilliseconds();

  if (!starting_time || !current_time) {
    return std::nullopt;
  }

  return (base::Milliseconds(current_time.value()) - starting_time.value()) *
         playback_rate_;
}

void WorkletAnimation::UpdateCurrentTimeIfNeeded() {
  bool is_timeline_active = IsTimelineActive();
  if (is_timeline_active != was_timeline_active_) {
    if (is_timeline_active) {
      if (!IsCurrentTimeInitialized()) {
        // The animation has started with inactive timeline. Initialize the
        // current time now.
        SetCurrentTime(InitialCurrentTime());
      } else {
        // Apply hold_time on current_time.
        SetCurrentTime(hold_time_);
      }
    } else {
      // Apply current_time on hold_time.
      SetCurrentTime(last_current_time_);
    }
    was_timeline_active_ = is_timeline_active;
  }
}

std::optional<base::TimeDelta> WorkletAnimation::CurrentTime() {
  if (play_state_ == Animation::kIdle || play_state_ == Animation::kUnset)
    return std::nullopt;

  // Current time calculated for scroll-linked animations depends on style
  // of the associated scroller. However it does not force style recalc when it
  // changes. This may create a situation when style has changed, style recalc
  // didn't run and the current time is calculated on the "dirty" style.
  UpdateCurrentTimeIfNeeded();
  last_current_time_ = CurrentTimeInternal();
  return last_current_time_;
}

std::optional<base::TimeDelta> WorkletAnimation::CurrentTimeInternal() const {
  if (play_state_ == Animation::kIdle || play_state_ == Animation::kUnset)
    return std::nullopt;

  if (hold_time_)
    return hold_time_.value();

  // We return early here when the animation has started with inactive
  // timeline and the timeline has never been activated.
  if (!IsTimelineActive())
    return std::nullopt;

  // Currently ScrollTimeline may return unresolved current time when:
  // - Current scroll offset is less than startScrollOffset and fill mode is
  //   none or forward.
  // OR
  // - Current scroll offset is greater than or equal to endScrollOffset and
  //   fill mode is none or backwards.
  std::optional<double> timeline_time_ms = timeline_->CurrentTimeMilliseconds();
  if (!timeline_time_ms)
    return std::nullopt;

  base::TimeDelta timeline_time = base::Milliseconds(timeline_time_ms.value());
  DCHECK(start_time_);
  return (timeline_time - start_time_.value()) * playback_rate_;
}

void WorkletAnimation::UpdateInputState(
    AnimationWorkletDispatcherInput* input_state) {
  std::optional<base::TimeDelta> current_time = CurrentTime();
  if (!running_on_main_thread_) {
    return;
  }
  bool was_active = IsActive(last_play_state_);
  bool is_active = IsActive(play_state_);

  // We don't animate if there is no valid current time.
  if (!current_time)
    return;

  bool did_time_change = current_time != last_input_update_current_time_;
  last_input_update_current_time_ = current_time;

  double current_time_ms = current_time.value().InMillisecondsF();

  if (!was_active && is_active) {
    input_state->Add({id_, animator_name_.Utf8(), current_time_ms,
                      CloneOptions(), CloneEffectTimings()});
  } else if (was_active && is_active) {
    // Skip if the input time is not changed.
    if (did_time_change)
      // TODO(jortaylo): EffectTimings need to be sent to the worklet during
      // updates, otherwise the timing info will become outdated.
      // https://crbug.com/915344.
      input_state->Update({id_, current_time_ms});
  } else if (was_active && !is_active) {
    input_state->Remove(id_);
  }
  last_play_state_ = play_state_;
}

void WorkletAnimation::SetOutputState(
    const AnimationWorkletOutput::AnimationState& state) {
  DCHECK(state.worklet_animation_id == id_);
  // The local times for composited effects, i.e. not running on main, are
  // updated via posting animation events from the compositor thread to the main
  // thread (see WorkletAnimation::NotifyLocalTimeUpdated).
  DCHECK(local_times_.size() == state.local_times.size() &&
         running_on_main_thread_);
  for (wtf_size_t i = 0; i < state.local_times.size(); ++i)
    local_times_[i] = state.local_times[i];
}

void WorkletAnimation::NotifyLocalTimeUpdated(
    std::optional<base::TimeDelta> local_time) {
  DCHECK(!running_on_main_thread_);
  local_times_[0] = local_time;
}

void WorkletAnimation::Dispose() {
  DCHECK(IsMainThread());
  DestroyCompositorAnimation();
}

void WorkletAnimation::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(effects_);
  visitor->Trace(timeline_);
  WorkletAnimationBase::Trace(visitor);
}

}  // namespace blink
