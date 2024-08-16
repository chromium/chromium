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

#include "third_party/blink/renderer/core/animation/animation.h"

#include <limits>
#include <memory>

#include "base/debug/stack_trace.h"
#include "base/metrics/histogram_macros.h"
#include "cc/animation/animation_timeline.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range_offset.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/animation_utils.h"
#include "third_party/blink/renderer/core/animation/compositor_animations.h"
#include "third_party/blink/renderer/core/animation/css/css_animation.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/css/css_transition.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/pending_animations.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline_util.h"
#include "third_party/blink/renderer/core/animation/timeline_range.h"
#include "third_party/blink/renderer/core/animation/timing_calculations.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_values.h"
#include "third_party/blink/renderer/core/css/native_paint_image_generator.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_attribute_mutation_scope.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/events/animation_playback_event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

// Accessing the compositor animation state should not be done during style,
// layout or paint to avoid blocking on a previous pending commit.
#if DCHECK_IS_ON()
#define VERIFY_PAINT_CLEAN_LOG_ONCE()                                         \
  if (VLOG_IS_ON(1)) {                                                        \
    if (document_->Lifecycle().GetState() < DocumentLifecycle::kPaintClean) { \
      static bool first_call = true;                                          \
      bool was_first_call = first_call;                                       \
      first_call = false;                                                     \
      if (was_first_call) {                                                   \
        VLOG(1) << __PRETTY_FUNCTION__                                        \
                << " called during style, layout or paint";                   \
        if (VLOG_IS_ON(2)) {                                                  \
          base::debug::StackTrace().Print();                                  \
        }                                                                     \
      }                                                                       \
    }                                                                         \
  }
#else
#define VERIFY_PAINT_CLEAN_LOG_ONCE()
#endif

// Ensure the time is bounded such that it can be resolved to microsecond
// accuracy. Beyond this limit, we can effectively stall an animation when
// ticking (i.e. b + delta == b for high enough floating point value of b).
// Furthermore, we can encounter numeric overflows when converting to a
// time format that is backed by a 64-bit integer.
bool SupportedTimeValue(double time_in_ms) {
  return std::abs(time_in_ms) < std::pow(std::numeric_limits<double>::radix,
                                         std::numeric_limits<double>::digits) /
                                    1000;
}

enum class PseudoPriority {
  kNone,
  kScrollPrevButton,
  kScrollMarkerGroupBefore,
  kMarker,
  kScrollMarker,
  kBefore,
  kOther,
  kAfter,
  kScrollMarkerGroupAfter,
  kScrollNextButton,
};

unsigned NextSequenceNumber() {
  static unsigned next = 0;
  return ++next;
}

PseudoPriority ConvertPseudoIdtoPriority(const PseudoId& pseudo) {
  if (pseudo == kPseudoIdNone)
    return PseudoPriority::kNone;
  if (pseudo == kPseudoIdScrollPrevButton) {
    return PseudoPriority::kScrollPrevButton;
  }
  if (pseudo == kPseudoIdScrollMarkerGroupBefore) {
    return PseudoPriority::kScrollMarkerGroupBefore;
  }
  if (pseudo == kPseudoIdMarker)
    return PseudoPriority::kMarker;
  if (pseudo == kPseudoIdScrollMarker) {
    return PseudoPriority::kScrollMarker;
  }
  if (pseudo == kPseudoIdBefore)
    return PseudoPriority::kBefore;
  if (pseudo == kPseudoIdAfter)
    return PseudoPriority::kAfter;
  if (pseudo == kPseudoIdScrollMarkerGroupAfter) {
    return PseudoPriority::kScrollMarkerGroupAfter;
  }
  if (pseudo == kPseudoIdScrollNextButton) {
    return PseudoPriority::kScrollNextButton;
  }
  return PseudoPriority::kOther;
}

Animation::AnimationClassPriority AnimationPriority(
    const Animation& animation) {
  // https://www.w3.org/TR/web-animations-1/#animation-class

  // CSS transitions have a lower composite order than CSS animations, and CSS
  // animations have a lower composite order than other animations. Thus,CSS
  // transitions are to appear before CSS animations and CSS animations are to
  // appear before other animations.
  // When animations are disassociated from their element they are sorted
  // by their sequence number, i.e. kDefaultPriority. See
  // https://drafts.csswg.org/css-animations-2/#animation-composite-order and
  // https://drafts.csswg.org/css-transitions-2/#animation-composite-order
  Animation::AnimationClassPriority priority;
  if (animation.IsCSSTransition() && animation.IsOwned())
    priority = Animation::AnimationClassPriority::kCssTransitionPriority;
  else if (animation.IsCSSAnimation() && animation.IsOwned())
    priority = Animation::AnimationClassPriority::kCssAnimationPriority;
  else
    priority = Animation::AnimationClassPriority::kDefaultPriority;
  return priority;
}

void RecordCompositorAnimationFailureReasons(
    CompositorAnimations::FailureReasons failure_reasons) {
  // UMA_HISTOGRAM_ENUMERATION requires that the enum_max must be strictly
  // greater than the sample value. kFailureReasonCount doesn't include the
  // kNoFailure value but the histograms do so adding the +1 is necessary.
  // TODO(dcheng): Fix https://crbug.com/705169 so this isn't needed.
  constexpr uint32_t kFailureReasonEnumMax =
      CompositorAnimations::kFailureReasonCount + 1;

  if (failure_reasons == CompositorAnimations::kNoFailure) {
    UMA_HISTOGRAM_ENUMERATION(
        "Blink.Animation.CompositedAnimationFailureReason",
        CompositorAnimations::kNoFailure, kFailureReasonEnumMax);
    return;
  }

  for (uint32_t i = 0; i < CompositorAnimations::kFailureReasonCount; i++) {
    unsigned val = 1 << i;
    if (failure_reasons & val) {
      UMA_HISTOGRAM_ENUMERATION(
          "Blink.Animation.CompositedAnimationFailureReason", i + 1,
          kFailureReasonEnumMax);
    }
  }
}

Element* OriginatingElement(Element* owning_element) {
  if (owning_element->IsPseudoElement()) {
    return owning_element->parentElement();
  }
  return owning_element;
}

AtomicString GetCSSTransitionCSSPropertyName(const Animation* animation) {
  CSSPropertyID property_id =
      To<CSSTransition>(animation)->TransitionCSSPropertyName().Id();
  if (property_id == CSSPropertyID::kVariable ||
      property_id == CSSPropertyID::kInvalid)
    return AtomicString();
  return To<CSSTransition>(animation)
      ->TransitionCSSPropertyName()
      .ToAtomicString();
}

bool GreaterThanOrEqualWithinTimeTolerance(const AnimationTimeDelta& a,
                                           const AnimationTimeDelta& b) {
  double a_ms = a.InMillisecondsF();
  double b_ms = b.InMillisecondsF();
  if (std::abs(a_ms - b_ms) < Animation::kTimeToleranceMs)
    return true;

  return a_ms > b_ms;
}

// Consider boundaries aligned if they round to the same integer pixel value.
const double kScrollBoundaryTolerance = 0.5;

}  // namespace

Animation* Animation::Create(AnimationEffect* effect,
                             AnimationTimeline* timeline,
                             ExceptionState& exception_state) {
  DCHECK(timeline);
  if (!IsA<DocumentTimeline>(timeline) && !timeline->IsScrollTimeline()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Invalid timeline. Animation requires a "
                                      "DocumentTimeline or ScrollTimeline");
    return nullptr;
  }
  DCHECK(IsA<DocumentTimeline>(timeline) || timeline->IsScrollTimeline());

  if (effect && timeline->IsScrollTimeline()) {
    if (effect->timing_.iteration_duration) {
      if (effect->timing_.iteration_duration->is_inf()) {
        exception_state.ThrowTypeError(
            "Effect duration cannot be Infinity when used with Scroll "
            "Timelines");
        return nullptr;
      }
    } else {
      // TODO(crbug.com/1216527)
      // Eventually we hope to be able to be more flexible with
      // iteration_duration "auto" and its interaction with start_delay and
      // end_delay. For now we will throw an exception if either delay is set
      // to a non-zero time-based value.
      // Once the spec (https://github.com/w3c/csswg-drafts/pull/6337) has been
      // ratified, we will be able to better handle mixed scenarios like "auto"
      // and time based delays.

      // If either delay or end_delay are non-zero, we can't yet handle "auto"
      if (effect->timing_.start_delay.IsNonzeroTimeBasedDelay() ||
          effect->timing_.end_delay.IsNonzeroTimeBasedDelay()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kNotSupportedError,
            "Effect duration \"auto\" with time-based delays is not yet "
            "implemented when used with Scroll Timelines");
        return nullptr;
      }
    }

    if (effect->timing_.iteration_count ==
        std::numeric_limits<double>::infinity()) {
      // iteration count of infinity makes no sense for scroll timelines
      exception_state.ThrowTypeError(
          "Effect iterations cannot be Infinity when used with Scroll "
          "Timelines");
      return nullptr;
    }
  }

  auto* context = timeline->GetDocument()->GetExecutionContext();
  return MakeGarbageCollected<Animation>(context, timeline, effect);
}

Animation* Animation::Create(ExecutionContext* execution_context,
                             AnimationEffect* effect,
                             ExceptionState& exception_state) {
  Document* document = To<LocalDOMWindow>(execution_context)->document();
  return Create(effect, &document->Timeline(), exception_state);
}

Animation* Animation::Create(ExecutionContext* execution_context,
                             AnimationEffect* effect,
                             AnimationTimeline* timeline,
                             ExceptionState& exception_state) {
  if (!timeline) {
    Animation* animation =
        MakeGarbageCollected<Animation>(execution_context, nullptr, effect);
    return animation;
  }

  return Create(effect, timeline, exception_state);
}

Animation::Animation(ExecutionContext* execution_context,
                     AnimationTimeline* timeline,
                     AnimationEffect* content)
    : ActiveScriptWrappable<Animation>({}),
      ExecutionContextLifecycleObserver(nullptr),
      reported_play_state_(kIdle),
      playback_rate_(1),
      start_time_(),
      hold_time_(),
      sequence_number_(NextSequenceNumber()),
      content_(content),
      timeline_(timeline),
      replace_state_(kActive),
      is_paused_for_testing_(false),
      is_composited_animation_disabled_for_testing_(false),
      pending_pause_(false),
      pending_play_(false),
      pending_finish_notification_(false),
      has_queued_microtask_(false),
      outdated_(false),
      finished_(true),
      committed_finish_notification_(false),
      compositor_state_(nullptr),
      compositor_pending_(false),
      compositor_group_(0),
      effect_suppressed_(false),
      compositor_property_animations_have_no_effect_(false),
      animation_has_no_effect_(false) {
  if (execution_context && !execution_context->IsContextDestroyed())
    SetExecutionContext(execution_context);

  if (content_) {
    if (content_->GetAnimation()) {
      content_->GetAnimation()->setEffect(nullptr);
    }
    content_->Attach(this);
  }

  AnimationTimeline* attached_timeline = timeline_;
  if (!attached_timeline) {
    attached_timeline =
        &To<LocalDOMWindow>(execution_context)->document()->Timeline();
  }
  document_ = attached_timeline->GetDocument();
  DCHECK(document_);
  attached_timeline->AnimationAttached(this);
  timeline_duration_ = attached_timeline->GetDuration();
  probe::DidCreateAnimation(document_, sequence_number_);
}

Animation::~Animation() {
  // Verify that compositor_animation_ has been disposed of.
  DCHECK(!compositor_animation_);
}

void Animation::Dispose() {
  if (timeline_)
    timeline_->AnimationDetached(this);
  DestroyCompositorAnimation();
  // If the DocumentTimeline and its Animation objects are
  // finalized by the same GC, we have to eagerly clear out
  // this Animation object's compositor animation registration.
  DCHECK(!compositor_animation_);
}

AnimationTimeDelta Animation::EffectEnd() const {
  return content_ ? content_->NormalizedTiming().end_time
                  : AnimationTimeDelta();
}

bool Animation::Limited(std::optional<AnimationTimeDelta> current_time) const {
  if (!current_time)
    return false;

  return (EffectivePlaybackRate() < 0 &&
          current_time <= AnimationTimeDelta()) ||
         (EffectivePlaybackRate() > 0 &&
          GreaterThanOrEqualWithinTimeTolerance(current_time.value(),
                                                EffectEnd()));
}

Document* Animation::GetDocument() const {
  return document_.Get();
}

std::optional<AnimationTimeDelta> Animation::TimelineTime() const {
  return timeline_ ? timeline_->CurrentTime() : std::nullopt;
}

bool Animation::ConvertCSSNumberishToTime(
    const V8CSSNumberish* numberish,
    std::optional<AnimationTimeDelta>& time,
    String variable_name,
    ExceptionState& exception_state) {
  // This function is used to handle the CSSNumberish input for setting
  // currentTime and startTime. Spec issue can be found here for this process:
  // https://github.com/w3c/csswg-drafts/issues/6458

  // Handle converting null
  if (!numberish) {
    time = std::nullopt;
    return true;
  }

  if (timeline_ && timeline_->IsProgressBased()) {
    // Progress based timeline
    if (numberish->IsCSSNumericValue()) {
      CSSUnitValue* numberish_as_percentage =
          numberish->GetAsCSSNumericValue()->to(
              CSSPrimitiveValue::UnitType::kPercentage);
      if (!numberish_as_percentage) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kNotSupportedError,
            "Invalid " + variable_name +
                ". CSSNumericValue must be a percentage for "
                "progress based animations.");
        return false;
      }
      timeline_duration_ = timeline_->GetDuration();
      time =
          (numberish_as_percentage->value() / 100) * timeline_duration_.value();
      return true;
    } else {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotSupportedError,
          "Invalid " + variable_name + ". Setting " + variable_name +
              " using absolute time "
              "values is not supported for progress based animations.");
      return false;
    }
  }

  // Document timeline
  if (numberish->IsCSSNumericValue()) {
    CSSUnitValue* numberish_as_number = numberish->GetAsCSSNumericValue()->to(
        CSSPrimitiveValue::UnitType::kNumber);
    if (numberish_as_number) {
      time =
          ANIMATION_TIME_DELTA_FROM_MILLISECONDS(numberish_as_number->value());
      return true;
    }

    CSSUnitValue* numberish_as_milliseconds =
        numberish->GetAsCSSNumericValue()->to(
            CSSPrimitiveValue::UnitType::kMilliseconds);
    if (numberish_as_milliseconds) {
      time = ANIMATION_TIME_DELTA_FROM_MILLISECONDS(
          numberish_as_milliseconds->value());
      return true;
    }

    CSSUnitValue* numberish_as_seconds = numberish->GetAsCSSNumericValue()->to(
        CSSPrimitiveValue::UnitType::kSeconds);
    if (numberish_as_seconds) {
      time = ANIMATION_TIME_DELTA_FROM_SECONDS(numberish_as_seconds->value());
      return true;
    }

    // TODO (crbug.com/1232181): Look into allowing document timelines to set
    // currentTime and startTime using CSSNumericValues that are percentages.
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Invalid " + variable_name +
            ". CSSNumericValue must be either a number or a time value for "
            "time based animations.");
    return false;
  }

  time = ANIMATION_TIME_DELTA_FROM_MILLISECONDS(numberish->GetAsDouble());
  return true;
}

// https://www.w3.org/TR/web-animations-1/#setting-the-current-time-of-an-animation
void Animation::setCurrentTime(const V8CSSNumberish* current_time,
                               ExceptionState& exception_state) {
  if (!current_time) {
    // If the current time is resolved, then throw a TypeError.
    if (CurrentTimeInternal()) {
      exception_state.ThrowTypeError(
          "currentTime may not be changed from resolved to unresolved");
    }
    return;
  }

  auto_align_start_time_ = false;

  std::optional<AnimationTimeDelta> new_current_time;
  // Failure to convert results in a thrown exception and returning false.
  if (!ConvertCSSNumberishToTime(current_time, new_current_time, "currentTime",
                                 exception_state))
    return;

  DCHECK(new_current_time);
  SetCurrentTimeInternal(new_current_time.value());

  // Synchronously resolve pending pause task.
  if (pending_pause_) {
    hold_time_ = new_current_time;
    ApplyPendingPlaybackRate();
    start_time_ = std::nullopt;
    pending_pause_ = false;
    if (ready_promise_)
      ResolvePromiseMaybeAsync(ready_promise_.Get());
  }

  // Update the finished state.
  UpdateFinishedState(UpdateType::kDiscontinuous, NotificationType::kAsync);

  SetCompositorPending(CompositorPendingReason::kPendingUpdate);

  // Notify of potential state change.
  NotifyProbe();
}

// https://www.w3.org/TR/web-animations-1/#setting-the-current-time-of-an-animation
// See steps for silently setting the current time. The preliminary step of
// handling an unresolved time are to be handled by the caller.
void Animation::SetCurrentTimeInternal(AnimationTimeDelta new_current_time) {
  std::optional<AnimationTimeDelta> previous_start_time = start_time_;
  std::optional<AnimationTimeDelta> previous_hold_time = hold_time_;

  // Update either the hold time or the start time.
  if (hold_time_ || !start_time_ || !timeline_ || !timeline_->IsActive() ||
      playback_rate_ == 0) {
    hold_time_ = new_current_time;
  } else {
    start_time_ = CalculateStartTime(new_current_time);
  }

  // Preserve invariant that we can only set a start time or a hold time in the
  // absence of an active timeline.
  if (!timeline_ || !timeline_->IsActive())
    start_time_ = std::nullopt;

  // Reset the previous current time.
  previous_current_time_ = std::nullopt;

  if (previous_start_time != start_time_ || previous_hold_time != hold_time_)
    SetOutdated();
}

V8CSSNumberish* Animation::startTime() const {
  if (start_time_) {
    return ConvertTimeToCSSNumberish(start_time_.value());
  }
  return nullptr;
}

V8CSSNumberish* Animation::ConvertTimeToCSSNumberish(
    std::optional<AnimationTimeDelta> time) const {
  if (time) {
    if (timeline_ && timeline_->IsScrollSnapshotTimeline()) {
      return To<ScrollSnapshotTimeline>(*timeline_)
          .ConvertTimeToProgress(time.value());
    }
    return MakeGarbageCollected<V8CSSNumberish>(time.value().InMillisecondsF());
  }
  return nullptr;
}

std::optional<double> Animation::TimeAsAnimationProgress(
    AnimationTimeDelta time) const {
  return !EffectEnd().is_zero() ? std::make_optional(time / EffectEnd())
                                : std::nullopt;
}

// https://www.w3.org/TR/web-animations-1/#the-current-time-of-an-animation
V8CSSNumberish* Animation::currentTime() const {
  // 1. If the animation’s hold time is resolved,
  //    The current time is the animation’s hold time.
  if (hold_time_.has_value()) {
    return ConvertTimeToCSSNumberish(hold_time_.value());
  }

  // 2.  If any of the following are true:
  //    * the animation has no associated timeline, or
  //    * the associated timeline is inactive, or
  //    * the animation’s start time is unresolved.
  // The current time is an unresolved time value.
  if (!timeline_ || !timeline_->IsActive() || !start_time_)
    return nullptr;

  // 3. Otherwise,
  // current time = (timeline time - start time) × playback rate
  std::optional<AnimationTimeDelta> timeline_time = timeline_->CurrentTime();

  // An active timeline should always have a value, and since inactive timeline
  // is handled in step 2 above, make sure that timeline_time has a value.
  DCHECK(timeline_time.has_value());

  AnimationTimeDelta calculated_current_time =
      (timeline_time.value() - start_time_.value()) * playback_rate_;

  return ConvertTimeToCSSNumberish(calculated_current_time);
}

std::optional<AnimationTimeDelta> Animation::CurrentTimeInternal() const {
  return hold_time_ ? hold_time_ : CalculateCurrentTime();
}

std::optional<AnimationTimeDelta> Animation::UnlimitedCurrentTime() const {
  return CalculateAnimationPlayState() == kPaused || !start_time_
             ? CurrentTimeInternal()
             : CalculateCurrentTime();
}

std::optional<double> Animation::progress() const {
  std::optional<AnimationTimeDelta> current_time = CurrentTimeInternal();
  if (!effect() || !current_time) {
    return std::nullopt;
  }

  const AnimationTimeDelta effect_end = EffectEnd();
  if (effect_end.is_zero()) {
    if (current_time < AnimationTimeDelta()) {
      return 0;
    }
    return 1;
  }

  if (effect_end.is_inf()) {
    return 0;
  }

  return std::clamp<double>(*current_time / effect_end, 0, 1);
}

String Animation::playState() const {
  return PlayStateString();
}

bool Animation::PreCommit(
    int compositor_group,
    const PaintArtifactCompositor* paint_artifact_compositor,
    bool start_on_compositor) {
  if (CompositorPendingCancel()) {
    CancelAnimationOnCompositor();
  }

  if (!start_time_ && !hold_time_) {
    // Waiting on a deferred start time.
    return false;
  }

  bool soft_change =
      compositor_state_ &&
      (Paused() || compositor_state_->playback_rate != EffectivePlaybackRate());
  bool hard_change =
      compositor_state_ && (compositor_state_->effect_changed ||
                            !compositor_state_->start_time || !start_time_ ||
                            !TimingCalculations::IsWithinAnimationTimeEpsilon(
                                compositor_state_->start_time.value(),
                                start_time_.value().InSecondsF()));

  bool compositor_property_animations_had_no_effect =
      compositor_property_animations_have_no_effect_;
  compositor_property_animations_have_no_effect_ = false;
  animation_has_no_effect_ = false;

  // FIXME: softChange && !hardChange should generate a Pause/ThenStart,
  // not a Cancel, but we can't communicate these to the compositor yet.

  bool changed = soft_change || hard_change;
  bool should_cancel = (!Playing() && compositor_state_) || changed;
  bool should_start = Playing() && (!compositor_state_ || changed);

  // If the property nodes were removed for this animation we must
  // cancel it. It may be running even though blink has not been
  // notified yet.
  if (!compositor_property_animations_had_no_effect && start_on_compositor &&
      should_cancel && should_start && compositor_state_ &&
      compositor_state_->pending_action == CompositorAction::kStart &&
      !compositor_state_->effect_changed) {
    // Restarting but still waiting for a start time.
    return false;
  }

  std::optional<int> replaced_cc_animation_id;
  if (should_cancel) {
    // TODO(https://crbug.com/41496930): This code currently avoids preserving
    // the id and compositor group of the cc animation on playback rate and
    // state changes (i.e. "soft changes") due to the linked bug. That's
    // because these soft changes use a time offset that assumes the start_time
    // is reset. A more complete fix should account for the fact that the start
    // time may be preserved when computing the offset.
    if (should_start && GetCompositorAnimation() && !soft_change) {
      // If the animation is being canceled and restarted, pass the replaced
      // cc::Animation's id along so the compositor can recreate the
      // cc::Animation with the same id, ensuring continuity in the animation.
      replaced_cc_animation_id = GetCompositorAnimation()->CcAnimationId();
      // Preserve the compositor group for a restarted Animation so that
      // animation events are routed correctly.
      compositor_group = compositor_group_;
    }
    CancelAnimationOnCompositor();
  }

  DCHECK(!compositor_state_ || compositor_state_->start_time);

  if (should_start) {
    compositor_group_ = compositor_group;
    if (start_on_compositor) {
      PropertyHandleSet unsupported_properties;
      CompositorAnimations::FailureReasons failure_reasons =
          CheckCanStartAnimationOnCompositor(paint_artifact_compositor,
                                             &unsupported_properties);
      RecordCompositorAnimationFailureReasons(failure_reasons);

      if (failure_reasons == CompositorAnimations::kNoFailure) {
        // We could still have a stale compositor keyframe model ID if
        // a previous cancel failed due to not having a layout object at the
        // time of the cancel operation. The start and stop of an animation
        // for a marquee element does not depend on having a layout object.
        if (HasActiveAnimationsOnCompositor())
          CancelAnimationOnCompositor();
        CreateCompositorAnimation(replaced_cc_animation_id);
        StartAnimationOnCompositor(paint_artifact_compositor);
        compositor_state_ = std::make_unique<CompositorState>(*this);
      } else {
        CancelIncompatibleAnimationsOnCompositor();
      }

      compositor_property_animations_have_no_effect_ =
          failure_reasons & CompositorAnimations::kAnimationHasNoVisibleChange;
      animation_has_no_effect_ =
          failure_reasons == CompositorAnimations::kAnimationHasNoVisibleChange;

      DCHECK_EQ(kRunning, CalculateAnimationPlayState());
      TRACE_EVENT_NESTABLE_ASYNC_INSTANT1(
          "blink.animations,devtools.timeline,benchmark,rail", "Animation",
          this, "data", [&](perfetto::TracedValue context) {
            inspector_animation_compositor_event::Data(
                std::move(context), failure_reasons, unsupported_properties);
          });
    }
  }

  return true;
}

void Animation::PostCommit() {
  compositor_pending_ = false;

  if (!compositor_state_ ||
      compositor_state_->pending_action == CompositorAction::kNone) {
    return;
  }

  DCHECK_EQ(CompositorAction::kStart, compositor_state_->pending_action);
  if (compositor_state_->start_time) {
    DCHECK(TimingCalculations::IsWithinAnimationTimeEpsilon(
        start_time_.value().InSecondsF(),
        compositor_state_->start_time.value()));
    compositor_state_->pending_action = CompositorAction::kNone;
  }
}

bool Animation::HasLowerCompositeOrdering(
    const Animation* animation1,
    const Animation* animation2,
    CompareAnimationsOrdering compare_animation_type) {
  AnimationClassPriority anim_priority1 = AnimationPriority(*animation1);
  AnimationClassPriority anim_priority2 = AnimationPriority(*animation2);
  if (anim_priority1 != anim_priority2)
    return anim_priority1 < anim_priority2;

  // If the the animation class is CssAnimation or CssTransition, then first
  // compare the owning element of animation1 and animation2, sort two of them
  // by tree order of their conrresponding owning element
  // The specs:
  // https://drafts.csswg.org/css-animations-2/#animation-composite-order
  // https://drafts.csswg.org/css-transitions-2/#animation-composite-order
  if (anim_priority1 != kDefaultPriority) {
    Element* owning_element1 = animation1->OwningElement();
    Element* owning_element2 = animation2->OwningElement();

    // Both animations are either CSS transitions or CSS animations with owning
    // elements.
    DCHECK(owning_element1 && owning_element2);
    Element* originating_element1 = OriginatingElement(owning_element1);
    Element* originating_element2 = OriginatingElement(owning_element2);

    // The tree position comparison would take a longer time, thus affect the
    // performance. We only do it when it comes to getAnimation.
    if (originating_element1 != originating_element2) {
      if (compare_animation_type == CompareAnimationsOrdering::kTreeOrder) {
        // Since pseudo elements are compared by their originating element,
        // they sort before their children.
        return originating_element1->compareDocumentPosition(
                   originating_element2) &
               Node::kDocumentPositionFollowing;
      } else {
        return originating_element1 < originating_element2;
      }
    }

    // A pseudo-element has a higher composite ordering than its originating
    // element, hence kPseudoIdNone is sorted earliest.
    // Two pseudo-elements sharing the same originating element are sorted
    // as follows:
    // ::marker
    // ::before
    // other pseudo-elements (ordered by selector)
    // ::after
    // TODO(bokan): ::view-transition ordering should probably also be explicit:
    // https://github.com/w3c/csswg-drafts/issues/9588.
    const PseudoId pseudo1 = owning_element1->GetPseudoId();
    const PseudoId pseudo2 = owning_element2->GetPseudoId();
    PseudoPriority priority1 = ConvertPseudoIdtoPriority(pseudo1);
    PseudoPriority priority2 = ConvertPseudoIdtoPriority(pseudo2);

    if (priority1 != priority2)
      return priority1 < priority2;

    if (priority1 == PseudoPriority::kOther && pseudo1 != pseudo2) {
      // TODO(bokan): This can happen with child pseudos in the
      // ::view-transition subtree but we may want to sort them based on their
      // actual composite order.
      // https://github.com/w3c/csswg-drafts/issues/9588.
      return CodeUnitCompareLessThan(
          PseudoElement::PseudoElementNameForEvents(owning_element1),
          PseudoElement::PseudoElementNameForEvents(owning_element2));
    }
    if (anim_priority1 == kCssAnimationPriority) {
      // When comparing two CSSAnimations with the same owning element, we sort
      // A and B based on their position in the computed value of the
      // animation-name property of the (common) owning element.
      return To<CSSAnimation>(animation1)->AnimationIndex() <
             To<CSSAnimation>(animation2)->AnimationIndex();
    } else {
      // First compare the transition generation of two transitions, then
      // compare them by the property name.
      if (To<CSSTransition>(animation1)->TransitionGeneration() !=
          To<CSSTransition>(animation2)->TransitionGeneration()) {
        return To<CSSTransition>(animation1)->TransitionGeneration() <
               To<CSSTransition>(animation2)->TransitionGeneration();
      }
      AtomicString css_property_name1 =
          GetCSSTransitionCSSPropertyName(animation1);
      AtomicString css_property_name2 =
          GetCSSTransitionCSSPropertyName(animation2);
      if (css_property_name1 && css_property_name2)
        return css_property_name1.Utf8() < css_property_name2.Utf8();
    }
    return animation1->SequenceNumber() < animation2->SequenceNumber();
  }
  // If the anmiations are not-CSS WebAnimation just compare them via generation
  // time/ sequence number.
  return animation1->SequenceNumber() < animation2->SequenceNumber();
}

void Animation::NotifyReady(AnimationTimeDelta ready_time) {
  // Complete the pending updates prior to updating the compositor state in
  // order to ensure a correct start time for the compositor state without the
  // need to duplicate the calculations.
  if (pending_play_)
    CommitPendingPlay(ready_time);
  else if (pending_pause_)
    CommitPendingPause(ready_time);

  if (compositor_state_ &&
      compositor_state_->pending_action == CompositorAction::kStart) {
    DCHECK(!compositor_state_->start_time);
    compositor_state_->pending_action = CompositorAction::kNone;
    compositor_state_->start_time =
        start_time_ ? std::make_optional(start_time_.value().InSecondsF())
                    : std::nullopt;
  }

  // Notify of change to play state.
  NotifyProbe();
}

// Microtask for playing an animation.
// Refer to Step 8.3 'pending play task' in the following spec:
// https://www.w3.org/TR/web-animations-1/#playing-an-animation-section
void Animation::CommitPendingPlay(AnimationTimeDelta ready_time) {
  DCHECK(start_time_ || hold_time_);
  DCHECK(pending_play_);
  pending_play_ = false;

  // Update hold and start time.
  if (hold_time_) {
    // A: If animation’s hold time is resolved,
    // A.1. Apply any pending playback rate on animation.
    // A.2. Let new start time be the result of evaluating:
    //        ready time - hold time / playback rate for animation.
    //      If the playback rate is zero, let new start time be simply ready
    //      time.
    // A.3. Set the start time of animation to new start time.
    // A.4. If animation’s playback rate is not 0, make animation’s hold time
    //      unresolved.
    ApplyPendingPlaybackRate();
    if (playback_rate_ == 0) {
      start_time_ = ready_time;
    } else {
      start_time_ = ready_time - hold_time_.value() / playback_rate_;
      hold_time_ = std::nullopt;
    }
  } else if (start_time_ && pending_playback_rate_) {
    // B: If animation’s start time is resolved and animation has a pending
    //    playback rate,
    // B.1. Let current time to match be the result of evaluating:
    //        (ready time - start time) × playback rate for animation.
    // B.2 Apply any pending playback rate on animation.
    // B.3 If animation’s playback rate is zero, let animation’s hold time be
    //     current time to match.
    // B.4 Let new start time be the result of evaluating:
    //       ready time - current time to match / playback rate for animation.
    //     If the playback rate is zero, let new start time be simply ready
    //     time.
    // B.5 Set the start time of animation to new start time.
    AnimationTimeDelta current_time_to_match =
        (ready_time - start_time_.value()) * playback_rate_;
    ApplyPendingPlaybackRate();
    if (playback_rate_ == 0) {
      hold_time_ = current_time_to_match;
      start_time_ = ready_time;
    } else {
      start_time_ = ready_time - current_time_to_match / playback_rate_;
    }
  }

  // 8.4 Resolve animation’s current ready promise with animation.
  if (ready_promise_ &&
      ready_promise_->GetState() == AnimationPromise::kPending)
    ResolvePromiseMaybeAsync(ready_promise_.Get());

  // 8.5 Run the procedure to update an animation’s finished state for
  //     animation with the did seek flag set to false, and the synchronously
  //     notify flag set to false.
  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);
}

// Microtask for pausing an animation.
// Refer to step 7 'pending pause task' in the following spec:
// https://www.w3.org/TR/web-animations-1/#pausing-an-animation-section
void Animation::CommitPendingPause(AnimationTimeDelta ready_time) {
  DCHECK(pending_pause_);
  pending_pause_ = false;

  // 1. Let ready time be the time value of the timeline associated with
  //    animation at the moment when the user agent completed processing
  //    necessary to suspend playback of animation’s associated effect.
  // 2. If animation’s start time is resolved and its hold time is not resolved,
  //    let animation’s hold time be the result of evaluating
  //    (ready time - start time) × playback rate.
  if (start_time_ && !hold_time_) {
    hold_time_ = (ready_time - start_time_.value()) * playback_rate_;
  }

  // 3. Apply any pending playback rate on animation.
  // 4. Make animation’s start time unresolved.
  ApplyPendingPlaybackRate();
  start_time_ = std::nullopt;

  // 5. Resolve animation’s current ready promise with animation.
  if (ready_promise_ &&
      ready_promise_->GetState() == AnimationPromise::kPending)
    ResolvePromiseMaybeAsync(ready_promise_.Get());

  // 6. Run the procedure to update an animation’s finished state for animation
  //    with the did seek flag set to false (continuous), and the synchronously
  //    notify flag set to false.
  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);
}

bool Animation::Affects(const Element& element,
                        const CSSProperty& property) const {
  const auto* effect = DynamicTo<KeyframeEffect>(content_.Get());
  if (!effect)
    return false;

  return (effect->EffectTarget() == &element) &&
         effect->Affects(PropertyHandle(property));
}

AnimationTimeline* Animation::timeline() {
  if (AnimationTimeline* timeline = TimelineInternal()) {
    return timeline->ExposedTimeline();
  }
  return nullptr;
}

void Animation::setTimeline(AnimationTimeline* timeline) {
  // https://www.w3.org/TR/web-animations-1/#setting-the-timeline

  // Unfortunately cannot mark the setter only as being conditionally enabled
  // via a feature flag. Conditionally making the feature a no-op is nearly
  // equivalent.
  if (!RuntimeEnabledFeatures::ScrollTimelineEnabled())
    return;

  // 1. Let the old timeline be the current timeline of the animation, if any.
  AnimationTimeline* old_timeline = timeline_;

  // 2. If the new timeline is the same object as the old timeline, abort this
  //    procedure.
  if (old_timeline == timeline)
    return;

  UpdateIfNecessary();
  AnimationPlayState old_play_state = CalculateAnimationPlayState();
  std::optional<AnimationTimeDelta> old_current_time = CurrentTimeInternal();

  // In some cases, we need to preserve the progress of the animation between
  // the old timeline and the new one. We do this by storing the progress using
  // the old current time and the effect end based on the old timeline. Pending
  // spec issue: https://github.com/w3c/csswg-drafts/issues/6452
  double progress = 0;
  if (old_current_time && !EffectEnd().is_zero()) {
    progress = old_current_time.value() / EffectEnd();
  }

  // 3. Let the timeline of the animation be the new timeline.

  // The Blink implementation requires additional steps to link the animation
  // to the new timeline. Animations with a null timeline hang off of the
  // document timeline in order to be properly included in the results for
  // getAnimations calls.
  if (old_timeline)
    old_timeline->AnimationDetached(this);
  else
    document_->Timeline().AnimationDetached(this);
  timeline_ = timeline;
  timeline_duration_ = timeline ? timeline->GetDuration() : std::nullopt;
  if (timeline)
    timeline->AnimationAttached(this);
  else
    document_->Timeline().AnimationAttached(this);
  SetOutdated();

  // Update content timing to be based on new timeline type. This ensures that
  // EffectEnd() is returning a value appropriate to the new timeline.
  if (content_) {
    content_->InvalidateNormalizedTiming();
  }

  if (timeline && !timeline->IsMonotonicallyIncreasing()) {
    switch (old_play_state) {
      case kIdle:
        break;

      case kRunning:
      case kFinished:
        if (old_current_time) {
          start_time_ = std::nullopt;
          hold_time_ = progress * EffectEnd();
        }
        PlayInternal(AutoRewind::kEnabled, ASSERT_NO_EXCEPTION);
        return;

      case kPaused:
        if (old_current_time) {
          start_time_ = std::nullopt;
          hold_time_ = progress * EffectEnd();
        }
        break;

      default:
        NOTREACHED_IN_MIGRATION();
    }
  } else if (old_current_time && old_timeline &&
             !old_timeline->IsMonotonicallyIncreasing()) {
    SetCurrentTimeInternal(progress * EffectEnd());
  }

  // 4. If the start time of animation is resolved, make the animation’s hold
  //    time unresolved. This step ensures that the finished play state of the
  //    animation is not “sticky” but is re-evaluated based on its updated
  //    current time.
  if (start_time_)
    hold_time_ = std::nullopt;

  // 5. Run the procedure to update an animation’s finished state for animation
  //    with the did seek flag set to false, and the synchronously notify flag
  //    set to false.
  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);

  if (content_ && !timeline_) {
    // Update the timing model to capture the phase change and cancel an active
    // CSS animation or transition.
    content_->Invalidate();
    Update(kTimingUpdateOnDemand);
  }

  SetCompositorPending(CompositorPendingReason::kPendingRestart);

  // Inform devtools of a potential change to the play state.
  NotifyProbe();
}

std::optional<AnimationTimeDelta> Animation::CalculateStartTime(
    AnimationTimeDelta current_time) const {
  std::optional<AnimationTimeDelta> start_time;
  if (timeline_) {
    std::optional<AnimationTimeDelta> timeline_time = timeline_->CurrentTime();
    if (timeline_time)
      start_time = timeline_time.value() - current_time / playback_rate_;
    // TODO(crbug.com/916117): Handle NaN time for scroll-linked animations.
    DCHECK(start_time || timeline_->IsProgressBased());
  }
  return start_time;
}

std::optional<AnimationTimeDelta> Animation::CalculateCurrentTime() const {
  if (!start_time_ || !timeline_ || !timeline_->IsActive())
    return std::nullopt;

  std::optional<AnimationTimeDelta> timeline_time = timeline_->CurrentTime();
  // timeline_ must be active here, make sure it is returning a current_time.
  DCHECK(timeline_time);

  return (timeline_time.value() - start_time_.value()) * playback_rate_;
}

// https://www.w3.org/TR/web-animations-1/#setting-the-start-time-of-an-animation
void Animation::setStartTime(const V8CSSNumberish* start_time,
                             ExceptionState& exception_state) {
  std::optional<AnimationTimeDelta> new_start_time;
  // Failure to convert results in a thrown exception and returning false.
  if (!ConvertCSSNumberishToTime(start_time, new_start_time, "startTime",
                                 exception_state))
    return;

  auto_align_start_time_ = false;

  const bool had_start_time = start_time_.has_value();

  // 1. Let timeline time be the current time value of the timeline that
  //    animation is associated with. If there is no timeline associated with
  //    animation or the associated timeline is inactive, let the timeline time
  //    be unresolved.
  std::optional<AnimationTimeDelta> timeline_time =
      timeline_ && timeline_->IsActive() ? timeline_->CurrentTime()
                                         : std::nullopt;

  // 2. If timeline time is unresolved and new start time is resolved, make
  //    animation’s hold time unresolved.
  // This preserves the invariant that when we don’t have an active timeline it
  // is only possible to set either the start time or the animation’s current
  // time.
  if (!timeline_time && new_start_time) {
    hold_time_ = std::nullopt;
  }

  // 3. Let previous current time be animation’s current time.
  std::optional<AnimationTimeDelta> previous_current_time =
      CurrentTimeInternal();

  // 4. Apply any pending playback rate on animation.
  ApplyPendingPlaybackRate();

  // 5. Set animation’s start time to new start time.
  if (new_start_time) {
    // Snap to timeline time if within floating point tolerance to ensure
    // deterministic behavior in phase transitions.
    if (timeline_time && TimingCalculations::IsWithinAnimationTimeEpsilon(
                             timeline_time.value().InSecondsF(),
                             new_start_time.value().InSecondsF())) {
      new_start_time = timeline_time.value();
    }
  }
  start_time_ = new_start_time;

  // 6. Update animation’s hold time based on the first matching condition from
  //    the following,
  // 6a If new start time is resolved,
  //      If animation’s playback rate is not zero, make animation’s hold time
  //      unresolved.
  // 6b Otherwise (new start time is unresolved),
  //      Set animation’s hold time to previous current time even if previous
  //      current time is unresolved.
  if (start_time_) {
    if (playback_rate_ != 0) {
      hold_time_ = std::nullopt;
    }
  } else {
    hold_time_ = previous_current_time;
  }

  // 7. If animation has a pending play task or a pending pause task, cancel
  //    that task and resolve animation’s current ready promise with animation.
  if (PendingInternal()) {
    pending_pause_ = false;
    pending_play_ = false;
    if (ready_promise_ &&
        ready_promise_->GetState() == AnimationPromise::kPending)
      ResolvePromiseMaybeAsync(ready_promise_.Get());
  }

  // 8. Run the procedure to update an animation’s finished state for animation
  //    with the did seek flag set to true (discontinuous), and the
  //    synchronously notify flag set to false (async).
  UpdateFinishedState(UpdateType::kDiscontinuous, NotificationType::kAsync);

  // Update user agent.
  std::optional<AnimationTimeDelta> new_current_time = CurrentTimeInternal();
  // Even when the animation is not outdated,call SetOutdated to ensure
  // the animation is tracked by its timeline for future timing
  // updates.
  if (previous_current_time != new_current_time ||
      (!had_start_time && start_time_)) {
    SetOutdated();
  }
  SetCompositorPending(CompositorPendingReason::kPendingUpdate);

  NotifyProbe();
}

// https://www.w3.org/TR/web-animations-1/#setting-the-associated-effect
void Animation::setEffect(AnimationEffect* new_effect) {
  // 1. Let old effect be the current associated effect of animation, if any.
  AnimationEffect* old_effect = content_;

  // 2. If new effect is the same object as old effect, abort this procedure.
  if (new_effect == old_effect)
    return;

  // 3. If animation has a pending pause task, reschedule that task to run as
  //    soon as animation is ready.
  // 4. If animation has a pending play task, reschedule that task to run as
  //    soon as animation is ready to play new effect.
  // No special action required for a reschedule. The pending_pause_ and
  // pending_play_ flags remain unchanged.

  // 5. If new effect is not null and if new effect is the associated effect of
  //    another previous animation, run the procedure to set the associated
  //    effect of an animation (this procedure) on previous animation passing
  //    null as new effect.
  if (new_effect && new_effect->GetAnimation())
    new_effect->GetAnimation()->setEffect(nullptr);

  // Clear timeline offsets for old effect.
  ResolveTimelineOffsets(TimelineRange());

  // 6. Let the associated effect of the animation be the new effect.
  if (old_effect)
    old_effect->Detach();
  content_ = new_effect;
  if (new_effect)
    new_effect->Attach(this);

  // Resolve timeline offsets for new effect.
  ResolveTimelineOffsets(timeline_ ? timeline_->GetTimelineRange()
                                   : TimelineRange());

  SetOutdated();

  // 7. Run the procedure to update an animation’s finished state for animation
  //    with the did seek flag set to false (continuous), and the synchronously
  //    notify flag set to false (async).
  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);

  SetCompositorPending(CompositorPendingReason::kPendingEffectChange);

  // Notify of a potential state change.
  NotifyProbe();

  // The effect is no longer associated with CSS properties.
  if (new_effect) {
    new_effect->SetIgnoreCssTimingProperties();
    if (KeyframeEffect* keyframe_effect =
            DynamicTo<KeyframeEffect>(new_effect)) {
      keyframe_effect->SetIgnoreCSSKeyframes();
    }
  }

  // The remaining steps are for handling CSS animation and transition events.
  // Both use an event delegate to dispatch events, which must be reattached to
  // the new effect.

  // When the animation no longer has an associated effect, calls to
  // Animation::Update will no longer update the animation timing and,
  // consequently, do not trigger animation or transition events.
  // Each transitionrun or transitionstart requires a corresponding
  // transitionend or transitioncancel.
  // https://drafts.csswg.org/css-transitions-2/#event-dispatch
  // Similarly, each animationstart requires a corresponding animationend or
  // animationcancel.
  // https://drafts.csswg.org/css-animations-2/#event-dispatch
  AnimationEffect::EventDelegate* old_event_delegate =
      old_effect ? old_effect->GetEventDelegate() : nullptr;
  if (!new_effect && old_effect && old_event_delegate) {
    // If the animation|transition has no target effect, the timing phase is set
    // according to the first matching condition from below:
    //   If the current time is unresolved,
    //     The timing phase is ‘idle’.
    //   If current time < 0,
    //     The timing phase is ‘before’.
    //   Otherwise,
    //     The timing phase is ‘after’.
    std::optional<AnimationTimeDelta> current_time = CurrentTimeInternal();
    Timing::Phase phase;
    if (!current_time)
      phase = Timing::kPhaseNone;
    else if (current_time < AnimationTimeDelta())
      phase = Timing::kPhaseBefore;
    else
      phase = Timing::kPhaseAfter;
    old_event_delegate->OnEventCondition(*old_effect, phase);
    return;
  }

  if (!new_effect || !old_effect)
    return;

  // Use the original target for event targeting.
  Element* target = To<KeyframeEffect>(old_effect)->target();
  if (!target)
    return;

  // Attach an event delegate to the new effect.
  AnimationEffect::EventDelegate* new_event_delegate =
      CreateEventDelegate(target, old_event_delegate);
  new_effect->SetEventDelegate(new_event_delegate);

  // Force an update to the timing model to ensure correct ordering of
  // animation or transition events.
  Update(kTimingUpdateOnDemand);
}

String Animation::PlayStateString() const {
  return PlayStateString(CalculateAnimationPlayState());
}

const char* Animation::PlayStateString(AnimationPlayState play_state) {
  switch (play_state) {
    case kIdle:
      return "idle";
    case kPending:
      return "pending";
    case kRunning:
      return "running";
    case kPaused:
      return "paused";
    case kFinished:
      return "finished";
    default:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

// https://www.w3.org/TR/web-animations-1/#play-states
Animation::AnimationPlayState Animation::CalculateAnimationPlayState() const {
  // 1. All of the following conditions are true:
  //    * The current time of animation is unresolved, and
  //    * the start time of animation is unresolved, and
  //    * animation does not have either a pending play task or a pending pause
  //      task,
  //    then idle.
  if (!CurrentTimeInternal() && !start_time_ && !PendingInternal())
    return kIdle;

  // 2. Either of the following conditions are true:
  //    * animation has a pending pause task, or
  //    * both the start time of animation is unresolved and it does not have a
  //      pending play task,
  //    then paused.
  if (pending_pause_ || (!start_time_ && !pending_play_))
    return kPaused;

  // 3.  For animation, current time is resolved and either of the following
  //     conditions are true:
  //     * animation’s effective playback rate > 0 and current time ≥ target
  //       effect end; or
  //     * animation’s effective playback rate < 0 and current time ≤ 0,
  //    then finished.
  if (Limited())
    return kFinished;

  // 4.  Otherwise
  return kRunning;
}

bool Animation::PendingInternal() const {
  return pending_pause_ || pending_play_;
}

bool Animation::pending() const {
  return PendingInternal();
}

// https://www.w3.org/TR/web-animations-1/#reset-an-animations-pending-tasks.
void Animation::ResetPendingTasks() {
  // 1. If animation does not have a pending play task or a pending pause task,
  //    abort this procedure.
  if (!PendingInternal())
    return;

  // 2. If animation has a pending play task, cancel that task.
  // 3. If animation has a pending pause task, cancel that task.
  pending_play_ = false;
  pending_pause_ = false;

  // 4. Apply any pending playback rate on animation.
  ApplyPendingPlaybackRate();

  // 5. Reject animation’s current ready promise with a DOMException named
  //    "AbortError".
  // 6. Let animation’s current ready promise be the result of creating a new
  //    resolved Promise object with value animation in the relevant Realm of
  //    animation.
  if (ready_promise_)
    RejectAndResetPromiseMaybeAsync(ready_promise_.Get());
}

// ----------------------------------------------
// Pause methods.
// ----------------------------------------------

// https://www.w3.org/TR/web-animations-1/#pausing-an-animation-section
void Animation::pause(ExceptionState& exception_state) {
  // 1. If animation has a pending pause task, abort these steps.
  // 2. If the play state of animation is paused, abort these steps.
  if (pending_pause_ || CalculateAnimationPlayState() == kPaused)
    return;

  // 3. Let seek time be a time value that is initially unresolved.
  std::optional<AnimationTimeDelta> seek_time;

  // 4. Let has finite timeline be true if animation has an associated timeline
  //    that is not monotonically increasing.
  bool has_finite_timeline =
      timeline_ && !timeline_->IsMonotonicallyIncreasing();

  // 5.  If the animation’s current time is unresolved, perform the steps
  //     according to the first matching condition from below:
  // 5a. If animation’s playback rate is ≥ 0,
  //       Set seek time to zero.
  // 5b. Otherwise,
  //         If associated effect end for animation is positive infinity,
  //             throw an "InvalidStateError" DOMException and abort these
  //             steps.
  //         Otherwise,
  //             Set seek time to animation's associated effect end.
  if (!CurrentTimeInternal() && !has_finite_timeline) {
    if (playback_rate_ >= 0) {
      seek_time = AnimationTimeDelta();
    } else {
      if (EffectEnd().is_inf()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kInvalidStateError,
            "Cannot play reversed Animation with infinite target effect end.");
        return;
      }
      seek_time = EffectEnd();
    }
  }

  // 6. If seek time is resolved,
  //        If has finite timeline is true,
  //            Set animation's start time to seek time.
  //        Otherwise,
  //            Set animation's hold time to seek time.
  if (seek_time) {
    hold_time_ = seek_time;
  }

  // TODO(kevers): Add step to the spec for handling scroll-driven animations.
  if (!hold_time_ && !start_time_) {
    DCHECK(has_finite_timeline);
    auto_align_start_time_ = true;
  }

  // 7. Let has pending ready promise be a boolean flag that is initially false.
  // 8. If animation has a pending play task, cancel that task and let has
  //    pending ready promise be true.
  // 9. If has pending ready promise is false, set animation’s current ready
  //    promise to a new promise in the relevant Realm of animation.
  if (pending_play_) {
    pending_play_ = false;
  } else if (ready_promise_) {
    ready_promise_->Reset();
  }

  // 10. Schedule a task to be executed at the first possible moment where both
  //    of the following conditions are true:
  //    10a. the user agent has performed any processing necessary to suspend
  //        the playback of animation’s associated effect, if any.
  //    10b. the animation is associated with a timeline that is not inactive.
  pending_pause_ = true;

  SetOutdated();
  SetCompositorPending(CompositorPendingReason::kPendingUpdate);

  // 11. Run the procedure to update an animation’s finished state for animation
  //    with the did seek flag set to false (continuous), and synchronously
  //    notify flag set to false.
  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);

  NotifyProbe();
}

// ----------------------------------------------
// Play methods.
// ----------------------------------------------

// Refer to the unpause operation in the following spec:
// https://www.w3.org/TR/css-animations-1/#animation-play-state
void Animation::Unpause() {
  if (CalculateAnimationPlayState() != kPaused)
    return;

  // TODO(kevers): Add step in the spec for making auto-rewind dependent on the
  // type of timeline.
  bool has_finite_timeline =
      timeline_ && !timeline_->IsMonotonicallyIncreasing();
  AutoRewind rewind_mode =
      has_finite_timeline ? AutoRewind::kEnabled : AutoRewind::kDisabled;
  PlayInternal(rewind_mode, ASSERT_NO_EXCEPTION);
}

// https://www.w3.org/TR/web-animations-1/#playing-an-animation-section
void Animation::play(ExceptionState& exception_state) {
  // Begin or resume playback of the animation by running the procedure to
  // play an animation passing true as the value of the auto-rewind flag.
  PlayInternal(AutoRewind::kEnabled, exception_state);
}

// https://www.w3.org/TR/web-animations-2/#playing-an-animation-section
void Animation::PlayInternal(AutoRewind auto_rewind,
                             ExceptionState& exception_state) {
  // 1. Let aborted pause be a boolean flag that is true if animation has a
  //    pending pause task, and false otherwise.
  // 2. Let has pending ready promise be a boolean flag that is initially false.
  // 3. Let seek time be a time value that is initially unresolved.
  //
  //    TODO(kevers): We should not use a seek time for scroll-driven
  //    animations.
  //
  //    NOTE: Seeking is enabled for time based animations when a discontinuity
  //    in the animation's progress is permitted, such as when starting from
  //    the idle state, or rewinding an animation that outside of the range
  //    [0, effect end]. Operations like unpausing an animation or updating its
  //    playback rate must preserve current time for time-based animations.
  //    Conversely, seeking is never permitted for scroll-driven animations
  //    because the start time is layout dependent and may not be resolvable at
  //    this stage.
  //
  // 4. Let has finite timeline be true if animation has an associated timeline
  //    that is not monotonically increasing.
  //
  //    TODO(kevers): Move this before step 3 in the spec since we shouldn't
  //    calculate a seek time for a scroll-driven animation.
  //
  // 5. Let previous current time be the animation’s current time
  // 6. If reset current time on resume is set:
  //      * Set previous current time to unresolved.
  //      * Set the reset current time on resume flag to false.
  //
  //    TODO(kevers): Remove the reset current time on resume flag. Unpausing
  //    a scroll-linked animation should update its start time based on the
  //    animation range regardless of whether the timeline was changed.

  bool aborted_pause = pending_pause_;
  bool has_pending_ready_promise = false;
  std::optional<AnimationTimeDelta> seek_time;
  bool has_finite_timeline =
      timeline_ && !timeline_->IsMonotonicallyIncreasing();
  bool enable_seek =
      auto_rewind == AutoRewind::kEnabled && !has_finite_timeline;

  // 7. Perform the steps corresponding to the first matching condition from the
  //    following, if any:
  //     * If animation’s effective playback rate > 0, the auto-rewind flag is
  //       true and either animation’s:
  //         * previous current time is unresolved, or
  //         * previous current time < zero, or
  //         * previous current time ≥ associated effect end,
  //       Set seek time to zero.
  //     * If animation’s effective playback rate < 0, the auto-rewind flag is
  //       true and either animation’s:
  //         * previous current time is unresolved, or
  //         * previous current time ≤ zero, or
  //         * previous current time > associated effect end,
  //       If associated effect end is positive infinity,
  //         throw an "InvalidStateError" DOMException and abort these steps.
  //       Otherwise,
  //         Set seek time to animation’s associated effect end.
  //     * If animation’s effective playback rate = 0 and animation’s current
  //       time is unresolved,
  //         Set seek time to zero.
  //
  // (TLDR version) If seek is enabled:
  //   Jump to the beginning or end of the animation depending on the playback
  //   rate if the current time is not resolved or out of bounds. Attempting
  //   to jump to the end of an infinite duration animation is not permitted.
  double effective_playback_rate = EffectivePlaybackRate();
  std::optional<AnimationTimeDelta> current_time = CurrentTimeInternal();
  std::optional<AnimationTimeDelta> effect_end = EffectEnd();
  if (effective_playback_rate > 0 && enable_seek &&
      (!current_time || current_time < AnimationTimeDelta() ||
       current_time >= effect_end)) {
    hold_time_ = AnimationTimeDelta();
  } else if (effective_playback_rate < 0 && enable_seek &&
             (!current_time || current_time <= AnimationTimeDelta() ||
              current_time > EffectEnd())) {
    if (EffectEnd().is_inf()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "Cannot play reversed Animation with infinite target effect end.");
      return;
    }
    hold_time_ = EffectEnd();
  } else if (effective_playback_rate == 0 && !current_time) {
    hold_time_ = AnimationTimeDelta();
  }

  // 8. If seek time is resolved,
  //      * If has finite timeline is true,
  //          * Set animation’s start time to seek time.
  //          * Let animation’s hold time be unresolved.
  //          * Apply any pending playback rate on animation.
  //      * Otherwise,
  //          * Set animation’s hold time to seek time.
  //
  // TODO(kevers): Replace seek time with hold time, and remove this block
  // entirely from the spec. We should not use a seek time with a scroll-driven
  // animation.

  // TODO(Kevers): Add steps the the spec for setting flags for scroll-driven
  // animations.

  // Note: An explicit call to play a scroll-driven animation resets any
  // stickiness in the start time of the animation, re-enabling auto-alignment
  // of the start time to the beginning or end of the animation range depending
  // on the playback rate. A flag is set to indicate that a new start time is
  // required. A play pending animation will be locked in that state until a new
  // start time is set in OnValidateSnapshot even if the animation already has a
  // start time.
  if (has_finite_timeline && auto_rewind == AutoRewind::kEnabled) {
    auto_align_start_time_ = true;
    hold_time_ = CurrentTimeInternal();
  }

  // 9. If animation’s hold time is resolved, let its start time be unresolved.

  // Note: The combination of a start time and a hold time is only permitted
  // when in the finished state. If the hold time is set, we clear the start
  // time. The finished state will be re-evaluated on the next update.
  if (hold_time_) {
    start_time_ = std::nullopt;
  }

  // 10. If animation has a pending play task or a pending pause task,
  if (pending_play_ || pending_pause_) {
    pending_play_ = false;
    pending_pause_ = false;
    has_pending_ready_promise = true;
  }

  // 11. If the following four conditions are all satisfied:
  //       * animation’s hold time is unresolved, and
  //       * seek time is unresolved, and
  //       * aborted pause is false, and
  //       * animation does not have a pending playback rate,
  //     abort this procedure.
  //
  // TODO(kevers): add an extra condition to prevent aborting if playing a
  // scroll-driven animation, which defers calculation of the start time.
  //
  // Note: If the animation is already running and there will be no change to
  // the start time or playback rate, then we can abort early as there is no
  // need for a ready promise. The remaining steps are for setting up and
  // resolving the ready promise.
  if (!hold_time_ && !seek_time && !has_finite_timeline && !aborted_pause &&
      !pending_playback_rate_) {
    return;
  }

  // 12. If has pending ready promise is false, let animation’s current ready
  //     promise be a new promise in the relevant Realm of animation.
  if (ready_promise_ && !has_pending_ready_promise) {
    ready_promise_->Reset();
  }

  // 13. Schedule a task to run as soon as animation is ready.
  pending_play_ = true;

  // Blink specific implementation details.
  finished_ = false;
  committed_finish_notification_ = false;
  SetOutdated();
  SetCompositorPending(CompositorPendingReason::kPendingUpdate);

  // Update an animation’s finished state. As the finished state may be
  // transient, we defer resolving the finished promise until the next
  // microtask checkpoint. Even if seeking, the update type is "continuous"
  // to avoid altering the hold time if set.
  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);

  // Notify change to pending play or finished state.
  NotifyProbe();
}

// https://www.w3.org/TR/web-animations-1/#reversing-an-animation-section
void Animation::reverse(ExceptionState& exception_state) {
  // 1. If there is no timeline associated with animation, or the associated
  //    timeline is inactive throw an "InvalidStateError" DOMException and abort
  //    these steps.
  if (!timeline_ || !timeline_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot reverse an animation with no active timeline");
    return;
  }

  // 2. Let original pending playback rate be animation’s pending playback rate.
  // 3. Let animation’s pending playback rate be the additive inverse of its
  //    effective playback rate (i.e. -effective playback rate).
  std::optional<double> original_pending_playback_rate = pending_playback_rate_;
  pending_playback_rate_ = -EffectivePlaybackRate();

  // Resolve precision issue at zero.
  if (pending_playback_rate_.value() == -0)
    pending_playback_rate_ = 0;

  // 4. Run the steps to play an animation for animation with the auto-rewind
  //    flag set to true.
  //    If the steps to play an animation throw an exception, set animation’s
  //    pending playback rate to original pending playback rate and propagate
  //    the exception.
  PlayInternal(AutoRewind::kEnabled, exception_state);
  if (exception_state.HadException())
    pending_playback_rate_ = original_pending_playback_rate;
}

// ----------------------------------------------
// Finish methods.
// ----------------------------------------------

// https://www.w3.org/TR/web-animations-1/#finishing-an-animation-section
void Animation::finish(ExceptionState& exception_state) {
  if (!EffectivePlaybackRate()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot finish Animation with a playbackRate of 0.");
    return;
  }
  if (EffectivePlaybackRate() > 0 && EffectEnd().is_inf()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot finish Animation with an infinite target effect end.");
    return;
  }

  auto_align_start_time_ = false;

  ApplyPendingPlaybackRate();

  AnimationTimeDelta new_current_time =
      playback_rate_ < 0 ? AnimationTimeDelta() : EffectEnd();
  SetCurrentTimeInternal(new_current_time);

  if (!start_time_ && timeline_ && timeline_->IsActive())
    start_time_ = CalculateStartTime(new_current_time);

  if (pending_pause_ && start_time_) {
    hold_time_ = std::nullopt;
    pending_pause_ = false;
    if (ready_promise_)
      ResolvePromiseMaybeAsync(ready_promise_.Get());
  }
  if (pending_play_ && start_time_) {
    pending_play_ = false;
    if (ready_promise_)
      ResolvePromiseMaybeAsync(ready_promise_.Get());
  }

  SetOutdated();
  UpdateFinishedState(UpdateType::kDiscontinuous, NotificationType::kSync);

  // Notify of change to finished state.
  NotifyProbe();
}

void Animation::UpdateFinishedState(UpdateType update_type,
                                    NotificationType notification_type) {
  // TODO(kevers): Add a new step to the spec.
  // Clear finished state and abort the procedure if play-pending and waiting
  // for a new start time.
  if (timeline_ && timeline_->IsScrollTimeline() && pending_play_ &&
      auto_align_start_time_) {
    finished_ = false;
    pending_finish_notification_ = false;
    committed_finish_notification_ = false;
    return;
  }

  bool did_seek = update_type == UpdateType::kDiscontinuous;
  // 1. Calculate the unconstrained current time. The dependency on did_seek is
  // required to accommodate timelines that may change direction. Without this
  // distinction, a once-finished animation would remain finished even when its
  // timeline progresses in the opposite direction.
  std::optional<AnimationTimeDelta> unconstrained_current_time =
      did_seek ? CurrentTimeInternal() : CalculateCurrentTime();

  // 2. Conditionally update the hold time.
  if (unconstrained_current_time && start_time_ && !pending_play_ &&
      !pending_pause_) {
    // Can seek outside the bounds of the active effect. Set the hold time to
    // the unconstrained value of the current time in the event that this update
    // is the result of explicitly setting the current time and the new time
    // is out of bounds. An update due to a time tick should not snap the hold
    // value back to the boundary if previously set outside the normal effect
    // boundary. The value of previous current time is used to retain this
    // value.
    double playback_rate = EffectivePlaybackRate();
    std::optional<AnimationTimeDelta> hold_time;

    if (playback_rate > 0 &&
        GreaterThanOrEqualWithinTimeTolerance(
            unconstrained_current_time.value(), EffectEnd())) {
      if (did_seek) {
        hold_time = unconstrained_current_time;
      } else {
        if (previous_current_time_ > EffectEnd()) {
          hold_time = previous_current_time_;
        } else {
          hold_time = EffectEnd();
        }
      }
      hold_time_ = hold_time;
    } else if (playback_rate < 0 &&
               unconstrained_current_time.value() <= AnimationTimeDelta()) {
      if (did_seek) {
        hold_time = unconstrained_current_time;
      } else {
        if (previous_current_time_ <= AnimationTimeDelta()) {
          hold_time = previous_current_time_;
        } else {
          hold_time = AnimationTimeDelta();
        }
      }

      // Hack for resolving precision issue at zero.
      if (hold_time.has_value() &&
          TimingCalculations::IsWithinAnimationTimeEpsilon(
              hold_time.value().InSecondsF(), -0)) {
        hold_time = AnimationTimeDelta();
      }

      hold_time_ = hold_time;
    } else if (playback_rate != 0) {
      // Update start time and reset hold time.
      if (did_seek && hold_time_)
        start_time_ = CalculateStartTime(hold_time_.value());
      hold_time_ = std::nullopt;
    }
  }

  // 3. Set the previous current time.
  previous_current_time_ = CurrentTimeInternal();

  // 4. Set the current finished state.
  AnimationPlayState play_state = CalculateAnimationPlayState();
  if (play_state == kFinished) {
    if (!committed_finish_notification_) {
      // 5. Setup finished notification.
      if (notification_type == NotificationType::kSync)
        CommitFinishNotification();
      else
        ScheduleAsyncFinish();
    }
  } else {
    // Previously finished animation may restart so they should be added to
    // pending animations to make sure that a compositor animation is re-created
    // during future PreCommit.
    if (finished_) {
      SetCompositorPending(CompositorPendingReason::kPendingUpdate);
    }
    // 6. If not finished but the current finished promise is already resolved,
    //    create a new promise.
    finished_ = pending_finish_notification_ = committed_finish_notification_ =
        false;
    if (finished_promise_ &&
        finished_promise_->GetState() == AnimationPromise::kResolved) {
      finished_promise_->Reset();
    }
  }
}

void Animation::ScheduleAsyncFinish() {
  auto* execution_context = GetExecutionContext();
  if (!execution_context)
    return;
  // Run a task to handle the finished promise and event as a microtask. With
  // the exception of an explicit call to Animation::finish, it is important to
  // apply these updates asynchronously as it is possible to enter the finished
  // state temporarily.
  pending_finish_notification_ = true;
  if (!has_queued_microtask_) {
    execution_context->GetAgent()->event_loop()->EnqueueMicrotask(WTF::BindOnce(
        &Animation::AsyncFinishMicrotask, WrapWeakPersistent(this)));
    has_queued_microtask_ = true;
  }
}

void Animation::AsyncFinishMicrotask() {
  // Resolve the finished promise and queue the finished event only if the
  // animation is still in a pending finished state. It is possible that the
  // transition was only temporary.
  if (pending_finish_notification_) {
    // A pending play or pause must resolve before the finish promise.
    if (PendingInternal() && timeline_)
      NotifyReady(timeline_->CurrentTime().value_or(AnimationTimeDelta()));
    CommitFinishNotification();
  }

  // This is a once callback and needs to be re-armed.
  has_queued_microtask_ = false;
}

// Refer to 'finished notification steps' in
// https://www.w3.org/TR/web-animations-1/#updating-the-finished-state
void Animation::CommitFinishNotification() {
  if (committed_finish_notification_)
    return;

  pending_finish_notification_ = false;

  // 1. If animation’s play state is not equal to finished, abort these steps.
  if (CalculateAnimationPlayState() != kFinished)
    return;

  // 2. Resolve animation’s current finished promise object with animation.
  if (finished_promise_ &&
      finished_promise_->GetState() == AnimationPromise::kPending) {
    ResolvePromiseMaybeAsync(finished_promise_.Get());
  }

  // 3. Create an AnimationPlaybackEvent, finishEvent.
  QueueFinishedEvent();

  committed_finish_notification_ = true;
}

// https://www.w3.org/TR/web-animations-1/#setting-the-playback-rate-of-an-animation
void Animation::updatePlaybackRate(double playback_rate,
                                   ExceptionState& exception_state) {
  // 1. Let previous play state be animation’s play state.
  // 2. Let animation’s pending playback rate be new playback rate.
  AnimationPlayState play_state = CalculateAnimationPlayState();
  pending_playback_rate_ = playback_rate;

  // 3. Perform the steps corresponding to the first matching condition from
  //    below:
  //
  // 3a If animation has a pending play task or a pending pause task,
  //    Abort these steps.
  if (PendingInternal())
    return;

  switch (play_state) {
    // 3b If previous play state is idle or paused,
    //    Apply any pending playback rate on animation.
    case kIdle:
    case kPaused:
      ApplyPendingPlaybackRate();
      break;

    // 3c If previous play state is finished,
    //    3c.1 Let the unconstrained current time be the result of calculating
    //         the current time of animation substituting an unresolved time
    //          value for the hold time.
    //    3c.2 Let animation’s start time be the result of evaluating the
    //         following expression:
    //    timeline time - (unconstrained current time / pending playback rate)
    // Where timeline time is the current time value of the timeline associated
    // with animation.
    //    3c.3 If pending playback rate is zero, let animation’s start time be
    //         timeline time.
    //    3c.4 Apply any pending playback rate on animation.
    //    3c.5 Run the procedure to update an animation’s finished state for
    //         animation with the did seek flag set to false, and the
    //         synchronously notify flag set to false.
    case kFinished: {
      std::optional<AnimationTimeDelta> unconstrained_current_time =
          CalculateCurrentTime();
      std::optional<AnimationTimeDelta> timeline_time =
          timeline_ ? timeline_->CurrentTime() : std::nullopt;
      if (playback_rate) {
        if (timeline_time) {
          start_time_ = (timeline_time && unconstrained_current_time)
                            ? std::make_optional<AnimationTimeDelta>(
                                  (timeline_time.value() -
                                   unconstrained_current_time.value()) /
                                  playback_rate)
                            : std::nullopt;
        }
      } else {
        start_time_ = timeline_time;
      }
      ApplyPendingPlaybackRate();
      UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);
      SetCompositorPending(CompositorPendingReason::kPendingUpdate);
      SetOutdated();
      NotifyProbe();
      break;
    }

    // 3d Otherwise,
    // Run the procedure to play an animation for animation with the
    // auto-rewind flag set to false.
    case kRunning:
      PlayInternal(AutoRewind::kDisabled, exception_state);
      break;

    case kUnset:
    case kPending:
      NOTREACHED_IN_MIGRATION();
  }
}

ScriptPromise<Animation> Animation::finished(ScriptState* script_state) {
  if (!finished_promise_) {
    finished_promise_ = MakeGarbageCollected<AnimationPromise>(
        ExecutionContext::From(script_state));
    // Do not report unhandled rejections of the finished promise.
    finished_promise_->MarkAsHandled();

    // Defer resolving the finished promise if the finish notification task is
    // pending. The finished state could change before the next microtask
    // checkpoint.
    if (CalculateAnimationPlayState() == kFinished &&
        !pending_finish_notification_)
      finished_promise_->Resolve(this);
  }
  return finished_promise_->Promise(script_state->World());
}

ScriptPromise<Animation> Animation::ready(ScriptState* script_state) {
  // Check for a pending state change prior to checking the ready promise, since
  // the pending check may force a style flush, which in turn could trigger a
  // reset of the ready promise when resolving a change to the
  // animationPlayState style.
  bool is_pending = pending();
  if (!ready_promise_) {
    ready_promise_ = MakeGarbageCollected<AnimationPromise>(
        ExecutionContext::From(script_state));
    // Do not report unhandled rejections of the ready promise.
    ready_promise_->MarkAsHandled();
    if (!is_pending)
      ready_promise_->Resolve(this);
  }
  return ready_promise_->Promise(script_state->World());
}

const AtomicString& Animation::InterfaceName() const {
  return event_target_names::kAnimation;
}

ExecutionContext* Animation::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

bool Animation::HasPendingActivity() const {
  bool has_pending_promise =
      finished_promise_ &&
      finished_promise_->GetState() == AnimationPromise::kPending;

  return pending_finished_event_ || pending_cancelled_event_ ||
         pending_remove_event_ || has_pending_promise ||
         (!finished_ && HasEventListeners(event_type_names::kFinish));
}

void Animation::ContextDestroyed() {
  finished_ = true;
  pending_finished_event_ = nullptr;
  pending_cancelled_event_ = nullptr;
  pending_remove_event_ = nullptr;
}

DispatchEventResult Animation::DispatchEventInternal(Event& event) {
  if (pending_finished_event_ == &event)
    pending_finished_event_ = nullptr;
  if (pending_cancelled_event_ == &event)
    pending_cancelled_event_ = nullptr;
  if (pending_remove_event_ == &event)
    pending_remove_event_ = nullptr;
  return EventTarget::DispatchEventInternal(event);
}

double Animation::playbackRate() const {
  return playback_rate_;
}

double Animation::EffectivePlaybackRate() const {
  return pending_playback_rate_.value_or(playback_rate_);
}

void Animation::ApplyPendingPlaybackRate() {
  if (pending_playback_rate_) {
    playback_rate_ = pending_playback_rate_.value();
    pending_playback_rate_ = std::nullopt;
    InvalidateNormalizedTiming();
  }
}

void Animation::setPlaybackRate(double playback_rate,
                                ExceptionState& exception_state) {
  std::optional<AnimationTimeDelta> start_time_before = start_time_;

  // 1. Clear any pending playback rate on animation.
  // 2. Let previous time be the value of the current time of animation before
  //    changing the playback rate.
  // 3. Set the playback rate to new playback rate.
  // 4. If the timeline is monotonically increasing and the previous time is
  //    resolved, set the current time of animation to previous time.
  // 5. If the timeline is not monotonically increasing, the start time is
  //    resolved and either:
  //      * the previous playback rate < 0 and the new playback rate >= 0, or
  //      * the previous playback rate >= 0 and the new playback rate < 0,
  //    Set animation's start time to the result of evaluating:
  //        associated effect end - start time
  bool preserve_current_time =
      timeline_ && timeline_->IsMonotonicallyIncreasing();
  bool reversal = (EffectivePlaybackRate() < 0) != (playback_rate < 0);
  pending_playback_rate_ = std::nullopt;
  V8CSSNumberish* previous_current_time = currentTime();
  playback_rate_ = playback_rate;
  if (previous_current_time && preserve_current_time) {
    setCurrentTime(previous_current_time, exception_state);
  }

  if (timeline_ && !timeline_->IsMonotonicallyIncreasing() && reversal &&
      start_time_) {
    if (auto_align_start_time_) {
      UpdateAutoAlignedStartTime();
    } else {
      start_time_ = EffectEnd() - start_time_.value();
    }
  }

  // Adds a UseCounter to check if setting playbackRate causes a compensatory
  // seek forcing a change in start_time_
  // We use an epsilon (1 microsecond) to handle precision issue.
  double epsilon = 1e-6;
  if (preserve_current_time && start_time_before && start_time_ &&
      fabs(start_time_.value().InMillisecondsF() -
           start_time_before.value().InMillisecondsF()) > epsilon &&
      CalculateAnimationPlayState() != kFinished) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kAnimationSetPlaybackRateCompensatorySeek);
  }
  InvalidateNormalizedTiming();
  SetCompositorPending(CompositorPendingReason::kPendingUpdate);
  SetOutdated();
  NotifyProbe();
}

void Animation::ClearOutdated() {
  if (!outdated_)
    return;
  outdated_ = false;
  if (timeline_)
    timeline_->ClearOutdatedAnimation(this);
}

void Animation::SetOutdated() {
  if (outdated_)
    return;
  outdated_ = true;
  if (timeline_)
    timeline_->SetOutdatedAnimation(this);
}

void Animation::ForceServiceOnNextFrame() {
  if (timeline_)
    timeline_->ScheduleServiceOnNextFrame();
}

CompositorAnimations::FailureReasons
Animation::CheckCanStartAnimationOnCompositor(
    const PaintArtifactCompositor* paint_artifact_compositor,
    PropertyHandleSet* unsupported_properties) const {
  CompositorAnimations::FailureReasons reasons =
      CheckCanStartAnimationOnCompositorInternal();

  if (auto* keyframe_effect = DynamicTo<KeyframeEffect>(content_.Get())) {
    reasons |= keyframe_effect->CheckCanStartAnimationOnCompositor(
        paint_artifact_compositor, playback_rate_, unsupported_properties);
  }
  return reasons;
}

CompositorAnimations::FailureReasons
Animation::CheckCanStartAnimationOnCompositorInternal() const {
  CompositorAnimations::FailureReasons reasons =
      CompositorAnimations::kNoFailure;

  if (is_composited_animation_disabled_for_testing_)
    reasons |= CompositorAnimations::kAcceleratedAnimationsDisabled;

  if (EffectSuppressed())
    reasons |= CompositorAnimations::kEffectSuppressedByDevtools;

  // An Animation with zero playback rate will produce no visual output, so
  // there is no reason to composite it.
  if (TimingCalculations::IsWithinAnimationTimeEpsilon(
          0, EffectivePlaybackRate())) {
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;
  }

  // Animation times with large magnitudes cannot be accurately reflected by
  // TimeTicks. These animations will stall, be finished next frame, or
  // stuck in the before phase. In any case, there will be no visible changes
  // after the initial frame.
  std::optional<AnimationTimeDelta> current_time = CurrentTimeInternal();
  if (current_time.has_value() &&
      !SupportedTimeValue(current_time.value().InMillisecondsF()))
    reasons |= CompositorAnimations::kEffectHasUnsupportedTimingParameters;

  if (!CurrentTimeInternal())
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  // Cannot composite an infinite duration animation with a negative playback
  // rate. TODO(crbug.com/1029167): Fix calculation of compositor timing to
  // enable compositing provided the iteration duration is finite. Having an
  // infinite number of iterations in the animation should not impede the
  // ability to composite the animation.
  if (EffectEnd().is_inf() && EffectivePlaybackRate() < 0)
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  // An Animation without a timeline effectively isn't playing, so there is no
  // reason to composite it. Additionally, mutating the timeline playback rate
  // is a debug feature available via devtools; we don't support this on the
  // compositor currently and there is no reason to do so.
  if (!timeline_ || (timeline_->IsDocumentTimeline() &&
                     To<DocumentTimeline>(*timeline_).PlaybackRate() != 1))
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  // If the scroll source is not composited, or we have not enabled scroll
  // driven animations on the compositor, fall back to main thread.
  // TODO(crbug.com/476553): Once all ScrollNodes including uncomposited ones
  // are in the compositor, the animation should be composited.
  if (timeline_ && timeline_->IsScrollSnapshotTimeline() &&
      !CompositorAnimations::CanStartScrollTimelineOnCompositor(
          To<ScrollSnapshotTimeline>(*timeline_).ResolvedSource())) {
    reasons |= CompositorAnimations::kTimelineSourceHasInvalidCompositingState;
  }

  // An Animation without an effect cannot produce a visual, so there is no
  // reason to composite it.
  if (!IsA<KeyframeEffect>(content_.Get()))
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  // An Animation that is not playing will not produce a visual, so there is no
  // reason to composite it.
  if (!Playing())
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  return reasons;
}

base::TimeDelta Animation::ComputeCompositorTimeOffset() const {
  if (start_time_ && !PendingInternal())
    return base::TimeDelta();

  double playback_rate = EffectivePlaybackRate();
  if (!playback_rate)
    return base::TimeDelta::Max();

  // Don't set a compositor time offset for progress-based timelines. When we
  // tick the animation, we pass "absolute" times to cc::KeyframeEffect::Pause.
  if (timeline_ && timeline_->IsProgressBased()) {
    return base::TimeDelta();
  }

  bool reversed = playback_rate < 0;

  std::optional<AnimationTimeDelta> current_time = CurrentTimeInternal();
  if (!current_time)
    return base::TimeDelta();

  double time_offset_s =
      reversed ? EffectEnd().InSecondsF() - current_time.value().InSecondsF()
               : current_time.value().InSecondsF();
  return base::Seconds(time_offset_s / fabs(playback_rate));
}

void Animation::MarkPendingIfCompositorPropertyAnimationChanges(
    const PaintArtifactCompositor* paint_artifact_compositor) {
  // |compositor_property_animations_have_no_effect_| will already be calculated
  // in |Animation::PreCommit| if the animation is pending.
  if (compositor_pending_)
    return;

  bool had_no_effect = compositor_property_animations_have_no_effect_;
  compositor_property_animations_have_no_effect_ = false;

  auto* keyframe_effect = DynamicTo<KeyframeEffect>(content_.Get());
  if (!keyframe_effect || !keyframe_effect->IsCurrent()) {
    // If the animation is not running, we can skip checking for having no
    // effect. We can also skip the call to |SetCompositorPending| to avoid
    // marking finished animations as pending.
    return;
  }

  Element* target = keyframe_effect->EffectTarget();
  if (target && keyframe_effect->Model() && keyframe_effect->IsCurrent()) {
    compositor_property_animations_have_no_effect_ =
        CompositorAnimations::CompositorPropertyAnimationsHaveNoEffect(
            *target, *keyframe_effect->Model(), paint_artifact_compositor);
  }
  if (compositor_property_animations_have_no_effect_ != had_no_effect)
    SetCompositorPending(CompositorPendingReason::kPendingEffectChange);
}

void Animation::StartAnimationOnCompositor(
    const PaintArtifactCompositor* paint_artifact_compositor) {
  DCHECK_EQ(
      CheckCanStartAnimationOnCompositor(paint_artifact_compositor, nullptr),
      CompositorAnimations::kNoFailure);

  // If PlaybackRate is 0, then we will run into divide by 0 issues.
  DCHECK(!TimingCalculations::IsWithinAnimationTimeEpsilon(
      0, EffectivePlaybackRate()));

  bool reversed = EffectivePlaybackRate() < 0;

  std::optional<AnimationTimeDelta> start_time;
  base::TimeDelta time_offset = base::TimeDelta();

  // Start the animation on the compositor with either a start time or time
  // offset. The start time is used for synchronous updates where the
  // compositor start time must be in precise alignment with the specified time
  // (e.g. after calling setStartTime). Scroll-driven animations always use this
  // mode even if it causes a discontinuity in the current time calculation.

  // Asynchronous updates such as updating the playback rate preserve current
  // time for a time-based animation even if the start time is set.
  // Asynchronous updates have an associated pending play or pending pause
  // task associated with them.
  if (start_time_ &&
      (timeline()->IsScrollSnapshotTimeline() || !PendingInternal())) {
    start_time = timeline_->ZeroTime() + start_time_.value();
    if (reversed) {
      start_time =
          start_time.value() - (EffectEnd() / fabs(EffectivePlaybackRate()));
    }
  } else {
    // Update preserves current time, which may not align with the value
    // computed from start time.
    time_offset = ComputeCompositorTimeOffset();
  }

  DCHECK_NE(compositor_group_, 0);
  DCHECK(To<KeyframeEffect>(content_.Get()));
  std::optional<double> start_time_s;
  if (start_time) {
    start_time_s = start_time.value().InSecondsF();
  }

  const Timing::NormalizedTiming& timing = effect()->NormalizedTiming();
  bool boundary_aligned = EffectivePlaybackRate() >= 0
                              ? timing.is_end_boundary_aligned
                              : timing.is_start_boundary_aligned;

  To<KeyframeEffect>(content_.Get())
      ->StartAnimationOnCompositor(
          compositor_group_, start_time_s, time_offset, EffectivePlaybackRate(),
          /*compositor_animation=*/nullptr,
          timeline()->IsMonotonicallyIncreasing(), boundary_aligned);
}

// TODO(crbug.com/960944): Rename to SetPendingCommit. This method handles both
// composited and non-composited animations. The use of 'compositor' in the name
// is confusing.
void Animation::SetCompositorPending(CompositorPendingReason reason) {
  // Determine if we need to reset the cached state for a property that is
  // composited via a native paint worklet. If reset, it forces Paint to
  // re-evaluate whether to paint with a native paint worklet.
  UpdateCompositedPaintStatus();

  if (RuntimeEnabledFeatures::
          CompositedAnimationsCancelledAsynchronouslyEnabled()) {
    if (compositor_state_ &&
        (reason == CompositorPendingReason::kPendingCancel ||
         reason == CompositorPendingReason::kPendingRestart)) {
      compositor_state_->pending_action = CompositorAction::kCancel;
    }
  } else {
    if (reason == CompositorPendingReason::kPendingCancel) {
      CancelAnimationOnCompositor();
      return;
    }
    if (reason == CompositorPendingReason::kPendingRestart) {
      CancelAnimationOnCompositor();
    }
    if (!HasActiveAnimationsOnCompositor()) {
      DestroyCompositorAnimation();
      compositor_state_.reset();
    }
  }

  if (compositor_state_) {
    if (reason == CompositorPendingReason::kPendingEffectChange) {
      compositor_state_->effect_changed = true;
    }
  } else {
    if (reason == CompositorPendingReason::kPendingCancel) {
      return;
    }
  }

  if (compositor_pending_) {
    return;
  }

  if (is_paused_for_testing_) {
    // Since the pause for testing API does not add the animation to the
    // list of pending animations, we must deal with any cancellations
    // immediately.
    // TODO(kevers): Fully deprecated the pause for testing API.
    if (CompositorPendingCancel()) {
      CancelAnimationOnCompositor();
    }
    return;
  }

  // In general, we need to update the compositor-side if anything has changed
  // on the blink version of the animation. There is also an edge case; if
  // neither the compositor nor blink side have a start time we still have to
  // sync them. This can happen if the blink side animation was started, the
  // compositor side hadn't started on its side yet, and then the blink side
  // start time was cleared (e.g. by setting current time).
  if (PendingInternal() || !compositor_state_ ||
      compositor_state_->effect_changed ||
      compositor_state_->pending_action == CompositorAction::kCancel ||
      compositor_state_->playback_rate != EffectivePlaybackRate() ||
      compositor_state_->start_time.has_value() != start_time_.has_value() ||
      (compositor_state_->start_time && start_time_ &&
       !TimingCalculations::IsWithinAnimationTimeEpsilon(
           compositor_state_->start_time.value(),
           start_time_.value().InSecondsF())) ||
      !compositor_state_->start_time || !start_time_) {
    compositor_pending_ = true;
    document_->GetPendingAnimations().Add(this);
  }
}

const Animation::RangeBoundary* Animation::rangeStart() {
  return ToRangeBoundary(range_start_);
}

const Animation::RangeBoundary* Animation::rangeEnd() {
  return ToRangeBoundary(range_end_);
}

void Animation::setRangeStart(const Animation::RangeBoundary* range_start,
                              ExceptionState& exception_state) {
  SetRangeStartInternal(
      GetEffectiveTimelineOffset(range_start, 0, exception_state));
}

void Animation::setRangeEnd(const Animation::RangeBoundary* range_end,
                            ExceptionState& exception_state) {
  SetRangeEndInternal(
      GetEffectiveTimelineOffset(range_end, 1, exception_state));
}

std::optional<TimelineOffset> Animation::GetEffectiveTimelineOffset(
    const Animation::RangeBoundary* boundary,
    double default_percent,
    ExceptionState& exception_state) {
  KeyframeEffect* keyframe_effect = DynamicTo<KeyframeEffect>(effect());
  Element* element = keyframe_effect ? keyframe_effect->target() : nullptr;

  return TimelineOffset::Create(element, boundary, default_percent,
                                exception_state);
}

/* static */
Animation::RangeBoundary* Animation::ToRangeBoundary(
    std::optional<TimelineOffset> timeline_offset) {
  if (!timeline_offset) {
    return MakeGarbageCollected<RangeBoundary>("normal");
  }

  TimelineRangeOffset* timeline_range_offset =
      MakeGarbageCollected<TimelineRangeOffset>();
  timeline_range_offset->setRangeName(timeline_offset->name);
  CSSPrimitiveValue* value =
      CSSPrimitiveValue::CreateFromLength(timeline_offset->offset, 1);
  CSSNumericValue* offset = CSSNumericValue::FromCSSValue(*value);
  timeline_range_offset->setOffset(offset);
  return MakeGarbageCollected<RangeBoundary>(timeline_range_offset);
}

void Animation::UpdateAutoAlignedStartTime() {
  DCHECK(auto_align_start_time_ || !start_time_);

  double relative_offset = 0;
  std::optional<TimelineOffset> boundary;
  if (EffectivePlaybackRate() >= 0) {
    boundary = GetRangeStartInternal();
  } else {
    boundary = GetRangeEndInternal();
    relative_offset = 1;
  }

  if (boundary) {
    relative_offset =
        timeline_->GetTimelineRange().ToFractionalOffset(boundary.value());
  }

  AnimationTimeDelta duration = timeline_->GetDuration().value();
  start_time_ = duration * relative_offset;
  SetCompositorPending(CompositorPendingReason::kPendingEffectChange);
}

bool Animation::OnValidateSnapshot(bool snapshot_changed) {
  bool needs_update = snapshot_changed;

  // Track a change in duration and update hold time if required.
  std::optional<AnimationTimeDelta> duration = timeline_->GetDuration();
  if (duration != timeline_duration_) {
    if (hold_time_) {
      DCHECK(timeline_duration_);
      double progress =
          hold_time_->InMillisecondsF() / timeline_duration_->InMillisecondsF();
      hold_time_ = progress * duration.value();
    }
    if (start_time_ && !auto_align_start_time_) {
      DCHECK(timeline_duration_);
      std::optional<AnimationTimeDelta> current_time = UnlimitedCurrentTime();
      if (current_time) {
        double progress = current_time->InMillisecondsF() /
                          timeline_duration_->InMillisecondsF();
        start_time_ = CalculateStartTime(progress * duration.value());
      }
    }
    timeline_duration_ = duration;
  }

  // Update style-dependent range offsets.
  bool range_changed = false;
  if (auto* keyframe_effect = DynamicTo<KeyframeEffect>(effect())) {
    if (keyframe_effect->target()) {
      if (style_dependent_range_start_) {
        DCHECK(range_start_);
        range_changed |= range_start_->UpdateOffset(
            keyframe_effect->target(), style_dependent_range_start_);
      }
      if (style_dependent_range_end_) {
        DCHECK(range_end_);
        range_changed |= range_end_->UpdateOffset(keyframe_effect->target(),
                                                  style_dependent_range_end_);
      }
    }
  }

  bool needs_new_start_time = false;
  switch (CalculateAnimationPlayState()) {
    case kIdle:
      break;

    case kPaused:
      needs_new_start_time = !start_time_ && !hold_time_;
      DCHECK(!needs_new_start_time || pending_pause_);
      break;

    case kRunning:
    case kFinished:
      if (!auto_align_start_time_ && hold_time_ && pending_play_ &&
          timeline_->CurrentTime()) {
        // The auto-alignment flag was reset via an API call. Set the start time
        // to preserve current time.
        ApplyPendingPlaybackRate();
        start_time_ = (playback_rate_ != 0)
                          ? CalculateStartTime(hold_time_.value()).value()
                          : timeline()->CurrentTime().value();
        hold_time_ = std::nullopt;
        needs_update = true;
      }
      needs_new_start_time =
          auto_align_start_time_ &&
          (!start_time_ || snapshot_changed || range_changed);
      break;

    default:
      NOTREACHED_IN_MIGRATION();
  }

  if (snapshot_changed || needs_new_start_time || range_changed) {
    InvalidateNormalizedTiming();
  }

  if (needs_new_start_time) {
    // Previous current time is used in update finished state to maintain
    // the current time if seeking out of bounds. A range update can place
    // current time temporarily out of bounds, but this should not be
    // confused with an explicit seek operation like setting the current or
    // start time.
    previous_current_time_ = std::nullopt;

    std::optional<AnimationTimeDelta> previous_start_time = start_time_;
    UpdateAutoAlignedStartTime();
    ApplyPendingPlaybackRate();
    if (start_time_ != previous_start_time) {
      needs_update = true;
      if (start_time_ && hold_time_) {
        hold_time_ = std::nullopt;
      }
    }
  }

  if (needs_update) {
    InvalidateEffectTargetStyle();
    SetOutdated();
    if (content_) {
      content_->Invalidate();
    }
    SetCompositorPending(CompositorPendingReason::kPendingEffectChange);
  }

  return !needs_update;
}

void Animation::SetRangeStartInternal(
    const std::optional<TimelineOffset>& range_start) {
  auto_align_start_time_ = true;
  if (range_start_ != range_start) {
    range_start_ = range_start;
    if (range_start_ && range_start_->style_dependent_offset) {
      style_dependent_range_start_ = TimelineOffset::ParseOffset(
          GetDocument(), range_start_->style_dependent_offset.value());
    } else {
      style_dependent_range_start_ = nullptr;
    }
    OnRangeUpdate();
  }
}

void Animation::SetRangeEndInternal(
    const std::optional<TimelineOffset>& range_end) {
  auto_align_start_time_ = true;
  if (range_end_ != range_end) {
    range_end_ = range_end;
    if (range_end_ && range_end_->style_dependent_offset) {
      style_dependent_range_end_ = TimelineOffset::ParseOffset(
          GetDocument(), range_end_->style_dependent_offset.value());
    } else {
      style_dependent_range_end_ = nullptr;
    }
    OnRangeUpdate();
  }
}

void Animation::SetRange(const std::optional<TimelineOffset>& range_start,
                         const std::optional<TimelineOffset>& range_end) {
  SetRangeStartInternal(range_start);
  SetRangeEndInternal(range_end);
}

void Animation::OnRangeUpdate() {
  // Change in animation range has no effect unless using a scroll-timeline.
  if (!IsA<ScrollSnapshotTimeline>(timeline_.Get())) {
    return;
  }

  // Force recalculation of the intrinsic iteration duration.
  InvalidateNormalizedTiming();
  if (PendingInternal()) {
    return;
  }

  AnimationPlayState play_state = CalculateAnimationPlayState();
  if (play_state == kRunning || play_state == kFinished) {
    PlayInternal(AutoRewind::kEnabled, ASSERT_NO_EXCEPTION);
  }
}

void Animation::UpdateBoundaryAlignment(
    Timing::NormalizedTiming& timing) const {
  timing.is_start_boundary_aligned = false;
  timing.is_end_boundary_aligned = false;
  if (!auto_align_start_time_) {
    // If the start time is not auto adjusted to align with the bounds of the
    // animation range, then it is not possible in all cases to test whether
    // setting the scroll position with either end of the scroll range will
    // align with the before-active or active-after boundaries. Safest to
    // assume that we are not-aligned and the boundary is exclusive.
    // TODO(kevers): Investigate if/when a use-case pops up that is important to
    // address.
    return;
  }

  if (auto* scroll_timeline = DynamicTo<ScrollTimeline>(TimelineInternal())) {
    std::optional<double> max_scroll =
        scroll_timeline->GetMaximumScrollPosition();
    if (!max_scroll) {
      return;
    }
    std::optional<ScrollOffsets> scroll_offsets =
        scroll_timeline->GetResolvedScrollOffsets();
    if (!scroll_offsets) {
      return;
    }
    TimelineRange timeline_range = scroll_timeline->GetTimelineRange();
    double start = range_start_
                       ? timeline_range.ToFractionalOffset(range_start_.value())
                       : 0;
    double end =
        range_end_ ? timeline_range.ToFractionalOffset(range_end_.value()) : 1;

    AnimationTimeDelta timeline_duration =
        scroll_timeline->GetDuration().value();
    if (timeline_duration > AnimationTimeDelta()) {
      start += timing.start_delay / timeline_duration;
      end -= timing.end_delay / timeline_duration;
    }

    double start_offset =
        start * scroll_offsets->end + (1 - start) * scroll_offsets->start;

    double end_offset =
        end * scroll_offsets->end + (1 - end) * scroll_offsets->start;

    double rate = EffectivePlaybackRate();
    timing.is_start_boundary_aligned =
        rate < 0 && start_offset <= kScrollBoundaryTolerance;
    timing.is_end_boundary_aligned =
        rate > 0 &&
        rate * end_offset >= max_scroll.value() - kScrollBoundaryTolerance;
  }
}

namespace {

double ResolveAnimationRange(const std::optional<TimelineOffset>& offset,
                             const TimelineRange& timeline_range,
                             double default_value) {
  if (offset.has_value()) {
    return timeline_range.ToFractionalOffset(offset.value());
  }
  if (timeline_range.IsEmpty()) {
    return 0;
  }
  return default_value;
}

}  // namespace

bool Animation::ResolveTimelineOffsets(const TimelineRange& timeline_range) {
  if (auto* keyframe_effect = DynamicTo<KeyframeEffect>(effect())) {
    double range_start = ResolveAnimationRange(
        GetRangeStartInternal(), timeline_range, /* default_value */ 0);
    double range_end = ResolveAnimationRange(
        GetRangeEndInternal(), timeline_range, /* default_value */ 1);
    return keyframe_effect->Model()->ResolveTimelineOffsets(
        timeline_range, range_start, range_end);
  }
  return false;
}

void Animation::CancelAnimationOnCompositor() {
  VERIFY_PAINT_CLEAN_LOG_ONCE()
  if (HasActiveAnimationsOnCompositor()) {
    To<KeyframeEffect>(content_.Get())
        ->CancelAnimationOnCompositor(GetCompositorAnimation());
  }

  // Note: We do not update the composited paint status here since already
  // updated via setCompositorPending. If the animation is to be restarted on
  // compositor, paint has already been given the opportunity to make the
  // compositing decision.
  DestroyCompositorAnimation();
  compositor_state_.reset();
}

void Animation::RestartAnimationOnCompositor() {
  if (!HasActiveAnimationsOnCompositor()) {
    return;
  }
  SetCompositorPending(CompositorPendingReason::kPendingRestart);
}

void Animation::CancelIncompatibleAnimationsOnCompositor() {
  VERIFY_PAINT_CLEAN_LOG_ONCE()
  if (auto* keyframe_effect = DynamicTo<KeyframeEffect>(content_.Get()))
    keyframe_effect->CancelIncompatibleAnimationsOnCompositor();
}

bool Animation::HasActiveAnimationsOnCompositor() {
  auto* keyframe_effect = DynamicTo<KeyframeEffect>(content_.Get());
  if (!keyframe_effect)
    return false;

  return keyframe_effect->HasActiveAnimationsOnCompositor();
}

// Update current time of the animation. Refer to step 1 in:
// https://www.w3.org/TR/web-animations-1/#update-animations-and-send-events
bool Animation::Update(TimingUpdateReason reason) {
  // Due to the hierarchical nature of the timing model, updating the current
  // time of an animation also involves:
  //   * Running the update an animation’s finished state procedure.
  //   * Queueing animation events.

  if (!Outdated() && reason == kTimingUpdateForAnimationFrame &&
      IsInDisplayLockedSubtree())
    return true;

  ClearOutdated();
  bool idle = CalculateAnimationPlayState() == kIdle;
  if (!idle && reason == kTimingUpdateForAnimationFrame)
    UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);

  if (content_) {
    std::optional<AnimationTimeDelta> inherited_time;

    if (!idle) {
      inherited_time = CurrentTimeInternal();
    }

    content_->UpdateInheritedTime(inherited_time, idle, playback_rate_, reason);

    // After updating the animation time if the animation is no longer current
    // blink will no longer composite the element (see
    // CompositingReasonFinder::RequiresCompositingFor*Animation).
    if (!content_->IsCurrent()) {
      SetCompositorPending(CompositorPendingReason::kPendingCancel);
    }
  }

  if (reason == kTimingUpdateForAnimationFrame) {
    if (idle || CalculateAnimationPlayState() == kFinished) {
      finished_ = true;
    }
    NotifyProbe();
  }

  DCHECK(!outdated_);

  return !finished_ || TimeToEffectChange() ||
         // Always return true for not idle animations attached to not
         // monotonically increasing timelines even if the animation is
         // finished. This is required to accommodate cases where timeline ticks
         // back in time.
         (!idle && timeline_ && !timeline_->IsMonotonicallyIncreasing());
}

void Animation::QueueFinishedEvent() {
  const AtomicString& event_type = event_type_names::kFinish;
  if (GetExecutionContext() && HasEventListeners(event_type)) {
    pending_finished_event_ = MakeGarbageCollected<AnimationPlaybackEvent>(
        event_type, currentTime(), ConvertTimeToCSSNumberish(TimelineTime()));
    pending_finished_event_->SetTarget(this);
    pending_finished_event_->SetCurrentTarget(this);
    document_->EnqueueAnimationFrameEvent(pending_finished_event_);
  }
}

void Animation::UpdateIfNecessary() {
  if (Outdated())
    Update(kTimingUpdateOnDemand);
  DCHECK(!Outdated());
}

void Animation::EffectInvalidated() {
  SetOutdated();
  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);
  // FIXME: Needs to consider groups when added.
  SetCompositorPending(CompositorPendingReason::kPendingEffectChange);
}

bool Animation::IsEventDispatchAllowed() const {
  return Paused() || start_time_;
}

std::optional<AnimationTimeDelta> Animation::TimeToEffectChange() {
  DCHECK(!outdated_);
  if (!start_time_ || hold_time_ || !playback_rate_) {
    return std::nullopt;
  }

  if (!content_) {
    std::optional<AnimationTimeDelta> current_time = CurrentTimeInternal();
    if (!current_time) {
      return std::nullopt;
    }
    return -current_time.value() / playback_rate_;
  }

  // If this animation has no effect, we can skip ticking it on main.
  if (!HasActiveAnimationsOnCompositor() && !animation_has_no_effect_ &&
      (content_->GetPhase() == Timing::kPhaseActive)) {
    return AnimationTimeDelta();
  }

  return (playback_rate_ > 0)
             ? (content_->TimeToForwardsEffectChange() / playback_rate_)
             : (content_->TimeToReverseEffectChange() / -playback_rate_);
}

// https://www.w3.org/TR/web-animations-1/#canceling-an-animation-section
void Animation::cancel() {
  AnimationTimeDelta current_time_before_cancel =
      CurrentTimeInternal().value_or(AnimationTimeDelta());
  AnimationPlayState initial_play_state = CalculateAnimationPlayState();
  if (initial_play_state != kIdle) {
    ResetPendingTasks();

    if (finished_promise_) {
      if (finished_promise_->GetState() == AnimationPromise::kPending)
        RejectAndResetPromiseMaybeAsync(finished_promise_.Get());
      else
        finished_promise_->Reset();
    }

    const AtomicString& event_type = event_type_names::kCancel;
    if (GetExecutionContext() && HasEventListeners(event_type)) {
      pending_cancelled_event_ = MakeGarbageCollected<AnimationPlaybackEvent>(
          event_type, nullptr, ConvertTimeToCSSNumberish(TimelineTime()));
      pending_cancelled_event_->SetTarget(this);
      pending_cancelled_event_->SetCurrentTarget(this);
      document_->EnqueueAnimationFrameEvent(pending_cancelled_event_);
    }
  } else {
    // Quietly reset without rejecting promises.
    pending_playback_rate_ = std::nullopt;
    pending_pause_ = pending_play_ = false;
  }

  hold_time_ = std::nullopt;
  start_time_ = std::nullopt;

  SetCompositorPending(CompositorPendingReason::kPendingCancel);
  SetOutdated();

  // Force dispatch of canceled event.
  if (content_)
    content_->SetCancelTime(current_time_before_cancel);
  Update(kTimingUpdateOnDemand);

  // Notify of change to canceled state.
  NotifyProbe();
}

void Animation::CreateCompositorAnimation(
    std::optional<int> replaced_cc_animation_id) {
  VERIFY_PAINT_CLEAN_LOG_ONCE()
  if (Platform::Current()->IsThreadedAnimationEnabled() &&
      !compositor_animation_) {
    compositor_animation_ =
        CompositorAnimationHolder::Create(this, replaced_cc_animation_id);
    AttachCompositorTimeline();
  }

  AttachCompositedLayers();
}

void Animation::DestroyCompositorAnimation() {
  VERIFY_PAINT_CLEAN_LOG_ONCE()
  DetachCompositedLayers();

  if (compositor_animation_) {
    DetachCompositorTimeline();
    compositor_animation_->Detach();
    compositor_animation_ = nullptr;
  }
}

void Animation::AttachCompositorTimeline() {
  VERIFY_PAINT_CLEAN_LOG_ONCE()
  DCHECK(compositor_animation_);

  // Register ourselves on the compositor timeline. This will cause our cc-side
  // animation animation to be registered.
  cc::AnimationTimeline* compositor_timeline =
      timeline_ ? timeline_->EnsureCompositorTimeline() : nullptr;
  if (!compositor_timeline)
    return;

  if (CompositorAnimation* compositor_animation = GetCompositorAnimation()) {
    compositor_timeline->AttachAnimation(compositor_animation->CcAnimation());
  }

  // Note that while we attach here but we don't detach because the
  // |compositor_timeline| is detached in its destructor.
  document_->AttachCompositorTimeline(compositor_timeline);
}

void Animation::DetachCompositorTimeline() {
  VERIFY_PAINT_CLEAN_LOG_ONCE()
  DCHECK(compositor_animation_);
  cc::AnimationTimeline* compositor_timeline =
      timeline_ ? timeline_->CompositorTimeline() : nullptr;
  if (!compositor_timeline)
    return;

  if (CompositorAnimation* compositor_animation = GetCompositorAnimation()) {
    compositor_timeline->DetachAnimation(compositor_animation->CcAnimation());
  }
}

void Animation::AttachCompositedLayers() {
  VERIFY_PAINT_CLEAN_LOG_ONCE()
  if (!compositor_animation_) {
    return;
  }

  DCHECK(content_);
  DCHECK(IsA<KeyframeEffect>(*content_));

  To<KeyframeEffect>(content_.Get())->AttachCompositedLayers();
}

void Animation::DetachCompositedLayers() {
  VERIFY_PAINT_CLEAN_LOG_ONCE()
  if (compositor_animation_ &&
      compositor_animation_->GetAnimation()->IsElementAttached())
    compositor_animation_->GetAnimation()->DetachElement();
}

void Animation::NotifyAnimationStarted(base::TimeDelta monotonic_time,
                                       int group) {
  document_->GetPendingAnimations().NotifyCompositorAnimationStarted(
      monotonic_time.InSecondsF(), group);
}

void Animation::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTarget::AddedEventListener(event_type, registered_listener);
  if (event_type == event_type_names::kFinish)
    UseCounter::Count(GetExecutionContext(), WebFeature::kAnimationFinishEvent);
}

void Animation::PauseForTesting(AnimationTimeDelta pause_time) {
  // Normally, cancel is deferred until Precommit, but cannot here since
  // updated below and must not be stale.
  if (CompositorPendingCancel()) {
    CancelAnimationOnCompositor();
  }

  // Do not restart a canceled animation.
  if (CalculateAnimationPlayState() == kIdle) {
    return;
  }

  // Pause a running animation, or update the hold time of a previously paused
  // animation.
  SetCurrentTimeInternal(pause_time);
  if (HasActiveAnimationsOnCompositor()) {
    std::optional<AnimationTimeDelta> current_time = CurrentTimeInternal();
    DCHECK(current_time);
    To<KeyframeEffect>(content_.Get())
        ->PauseAnimationForTestingOnCompositor(
            base::Seconds(current_time.value().InSecondsF()));
  }

  // Do not wait for animation ready to lock in the hold time. Otherwise,
  // the pause won't take effect until the next frame and the hold time will
  // potentially drift.
  is_paused_for_testing_ = true;
  pending_pause_ = false;
  pending_play_ = false;
  hold_time_ = pause_time;
  start_time_ = std::nullopt;
  UpdateCompositedPaintStatus();
}

void Animation::SetEffectSuppressed(bool suppressed) {
  effect_suppressed_ = suppressed;
  if (suppressed) {
    SetCompositorPending(CompositorPendingReason::kPendingCancel);
  }
}

void Animation::DisableCompositedAnimationForTesting() {
  is_composited_animation_disabled_for_testing_ = true;
  CancelAnimationOnCompositor();
}

void Animation::InvalidateKeyframeEffect(
    const TreeScope& tree_scope,
    const StyleChangeReasonForTracing& reason) {
  auto* keyframe_effect = DynamicTo<KeyframeEffect>(content_.Get());
  if (!keyframe_effect)
    return;

  Element* target = keyframe_effect->EffectTarget();

  // TODO(alancutter): Remove dependency of this function on CSSAnimations.
  // This function makes the incorrect assumption that the animation uses
  // @keyframes for its effect model when it may instead be using JS provided
  // keyframes.
  if (target &&
      CSSAnimations::IsAffectedByKeyframesFromScope(*target, tree_scope)) {
    target->SetNeedsStyleRecalc(kLocalStyleChange, reason);
  }
}

void Animation::InvalidateEffectTargetStyle() {
  auto* keyframe_effect = DynamicTo<KeyframeEffect>(content_.Get());
  if (!keyframe_effect)
    return;
  Element* target = keyframe_effect->EffectTarget();
  if (target) {
    // TODO(andruud): Should we add a new style_change_reason?
    target->SetNeedsStyleRecalc(kLocalStyleChange,
                                StyleChangeReasonForTracing::Create(
                                    style_change_reason::kScrollTimeline));
  }
}

void Animation::InvalidateNormalizedTiming() {
  if (effect())
    effect()->InvalidateNormalizedTiming();
}

void Animation::ResolvePromiseMaybeAsync(AnimationPromise* promise) {
  if (ScriptForbiddenScope::IsScriptForbidden()) {
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kDOMManipulation)
        ->PostTask(
            FROM_HERE,
            WTF::BindOnce(&AnimationPromise::Resolve<Animation*>,
                          WrapPersistent(promise), WrapPersistent(this)));
  } else {
    promise->Resolve(this);
  }
}

void Animation::RejectAndResetPromise(AnimationPromise* promise) {
  promise->Reject(
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError));
  promise->Reset();
}

void Animation::RejectAndResetPromiseMaybeAsync(AnimationPromise* promise) {
  if (ScriptForbiddenScope::IsScriptForbidden()) {
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kDOMManipulation)
        ->PostTask(FROM_HERE, WTF::BindOnce(&Animation::RejectAndResetPromise,
                                            WrapPersistent(this),
                                            WrapPersistent(promise)));
  } else {
    RejectAndResetPromise(promise);
  }
}

void Animation::NotifyProbe() {
  AnimationPlayState old_play_state = reported_play_state_;
  AnimationPlayState new_play_state =
      PendingInternal() ? kPending : CalculateAnimationPlayState();
  probe::AnimationUpdated(document_, this);

  if (old_play_state != new_play_state) {
    reported_play_state_ = new_play_state;

    bool was_active = old_play_state == kPending || old_play_state == kRunning;
    bool is_active = new_play_state == kPending || new_play_state == kRunning;

    if (!was_active && is_active) {
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
          "blink.animations,devtools.timeline,benchmark,rail", "Animation",
          this, "data", [&](perfetto::TracedValue context) {
            inspector_animation_event::Data(std::move(context), *this);
          });
    } else if (was_active && !is_active) {
      TRACE_EVENT_NESTABLE_ASYNC_END1(
          "blink.animations,devtools.timeline,benchmark,rail", "Animation",
          this, "endData", [&](perfetto::TracedValue context) {
            inspector_animation_state_event::Data(std::move(context), *this);
          });
    } else {
      TRACE_EVENT_NESTABLE_ASYNC_INSTANT1(
          "blink.animations,devtools.timeline,benchmark,rail", "Animation",
          this, "data", [&](perfetto::TracedValue context) {
            inspector_animation_state_event::Data(std::move(context), *this);
          });
    }
  }
}

// -------------------------------------
// Replacement of animations
// -------------------------------------

// https://www.w3.org/TR/web-animations-1/#removing-replaced-animations
bool Animation::IsReplaceable() {
  // An animation is replaceable if all of the following conditions are true:

  // 1. The existence of the animation is not prescribed by markup. That is, it
  //    is not a CSS animation with an owning element, nor a CSS transition with
  //    an owning element.
  if ((IsCSSAnimation() || IsCSSTransition()) && OwningElement()) {
    // A CSS animation or transition that is bound to markup is not replaceable.
    return false;
  }

  // 2. The animation's play state is finished.
  if (CalculateAnimationPlayState() != kFinished)
    return false;

  // 3. The animation's replace state is not removed.
  if (replace_state_ == kRemoved)
    return false;

  // 4. The animation is associated with a monotonically increasing timeline.
  if (!timeline_ || !timeline_->IsMonotonicallyIncreasing())
    return false;

  // 5. The animation has an associated effect.
  if (!content_ || !content_->IsKeyframeEffect())
    return false;

  // 6. The animation's associated effect is in effect.
  if (!content_->IsInEffect())
    return false;

  // 7. The animation's associated effect has an effect target.
  Element* target = To<KeyframeEffect>(content_.Get())->EffectTarget();
  if (!target)
    return false;

  return true;
}

// https://www.w3.org/TR/web-animations-1/#removing-replaced-animations
void Animation::RemoveReplacedAnimation() {
  DCHECK(IsReplaceable());

  // To remove a replaced animation, perform the following steps:
  // 1. Set animation’s replace state to removed.
  // 2. Create an AnimationPlaybackEvent, removeEvent.
  // 3. Set removeEvent’s type attribute to remove.
  // 4. Set removeEvent’s currentTime attribute to the current time of
  //    animation.
  // 5. Set removeEvent’s timelineTime attribute to the current time of the
  //    timeline with which animation is associated.
  //
  // If animation has a document for timing, then append removeEvent to its
  // document for timing's pending animation event queue along with its target,
  // animation. For the scheduled event time, use the result of applying the
  // procedure to convert timeline time to origin-relative time to the current
  // time of the timeline with which animation is associated.
  replace_state_ = kRemoved;
  const AtomicString& event_type = event_type_names::kRemove;
  if (GetExecutionContext() && HasEventListeners(event_type)) {
    pending_remove_event_ = MakeGarbageCollected<AnimationPlaybackEvent>(
        event_type, currentTime(), ConvertTimeToCSSNumberish(TimelineTime()));
    pending_remove_event_->SetTarget(this);
    pending_remove_event_->SetCurrentTarget(this);
    document_->EnqueueAnimationFrameEvent(pending_remove_event_);
  }

  // Force timing update to clear the effect.
  if (content_)
    content_->Invalidate();
  Update(kTimingUpdateOnDemand);
}

void Animation::persist() {
  if (replace_state_ == kPersisted)
    return;

  replace_state_ = kPersisted;

  // Force timing update to reapply the effect.
  if (content_)
    content_->Invalidate();
  Update(kTimingUpdateOnDemand);
}

String Animation::replaceState() {
  switch (replace_state_) {
    case kActive:
      return "active";

    case kRemoved:
      return "removed";

    case kPersisted:
      return "persisted";

    default:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

// https://www.w3.org/TR/web-animations-1/#dom-animation-commitstyles
void Animation::commitStyles(ExceptionState& exception_state) {
  Element* target = content_ && content_->IsKeyframeEffect()
                        ? To<KeyframeEffect>(effect())->target()
                        : nullptr;

  // 1. If target is not an element capable of having a style attribute
  //    (for example, it is a pseudo-element or is an element in a document
  //    format for which style attributes are not defined) throw a
  //    "NoModificationAllowedError" DOMException and abort these steps.
  if (!target || !target->IsStyledElement() ||
      !To<KeyframeEffect>(effect())->pseudoElement().empty()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNoModificationAllowedError,
        "Animation not associated with a styled element");
    return;
  }
  // 2. If, after applying any pending style changes, target is not being
  //    rendered, throw an "InvalidStateError" DOMException and abort these
  //    steps.
  target->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);
  if (!target->GetLayoutObject()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Target element is not rendered.");
    return;
  }

  // 3. Let inline style be the result of getting the CSS declaration block
  //    corresponding to target’s style attribute. If target does not have a
  //    style attribute, let inline style be a new empty CSS declaration block
  //    with the readonly flag unset and owner node set to target.
  CSSStyleDeclaration* inline_style = target->style();

  // 4. Let targeted properties be the set of physical longhand properties
  //    that are a target property for at least one animation effect
  //    associated with animation whose effect target is target.
  PropertyHandleSet animation_properties =
      To<KeyframeEffect>(effect())->Model()->Properties();

  // 5. For each property, property, in targeted properties:
  //   5.1 Let partialEffectStack be a copy of the effect stack for property
  //       on target.
  //   5.2 If animation’s replace state is removed, add all animation effects
  //       associated with animation whose effect target is target and which
  //       include property as a target property to partialEffectStack.
  //   5.3 Remove from partialEffectStack any animation effects whose
  //       associated animation has a higher composite order than animation.
  //   5.4 Let effect value be the result of calculating the result of
  //       partialEffectStack for property using target’s computed style
  //       (see § 5.4.3 Calculating the result of an effect stack).
  //   5.5 Set a CSS declaration property for effect value in inline style.
  // 6. Update style attribute for inline style.
  ActiveInterpolationsMap interpolations_map =
      To<KeyframeEffect>(effect())->InterpolationsForCommitStyles();

  // `inline_style` must be an inline style declaration, which is a subclass of
  // `AbstractPropertySetCSSStyleDeclaration`.
  CHECK(inline_style->IsAbstractPropertySet());
  StyleAttributeMutationScope style_attr_mutation_scope(
      To<AbstractPropertySetCSSStyleDeclaration>(inline_style));

  AnimationUtils::ForEachInterpolatedPropertyValue(
      target, animation_properties, interpolations_map,
      [inline_style, target](PropertyHandle property, const CSSValue* value) {
        inline_style->setProperty(
            target->GetExecutionContext(),
            property.GetCSSPropertyName().ToAtomicString(), value->CssText(),
            "", ASSERT_NO_EXCEPTION);
      });
}

bool Animation::IsInDisplayLockedSubtree() {
  Element* owning_element = OwningElement();
  if (!owning_element || !GetDocument())
    return false;

  base::TimeTicks display_lock_update_timestamp =
      GetDocument()->GetDisplayLockDocumentState().GetLockUpdateTimestamp();

  if (last_display_lock_update_time_ < display_lock_update_timestamp) {
    const Element* element =
        DisplayLockUtilities::LockedAncestorPreventingPaint(*owning_element);
    is_in_display_locked_subtree_ = !!element;
    last_display_lock_update_time_ = display_lock_update_timestamp;
  }

  return is_in_display_locked_subtree_;
}

void Animation::UpdateCompositedPaintStatus() {
  if (!NativePaintImageGenerator::NativePaintWorkletAnimationsEnabled()) {
    return;
  }

  KeyframeEffect* keyframe_effect = DynamicTo<KeyframeEffect>(content_.Get());
  if (!keyframe_effect) {
    return;
  }

  Element* target = keyframe_effect->EffectTarget();
  if (!target) {
    return;
  }

  ElementAnimations* element_animations = target->GetElementAnimations();
  DCHECK(element_animations);

  if (RuntimeEnabledFeatures::CompositeBGColorAnimationEnabled()) {
    element_animations->RecalcCompositedStatus(target,
                                               GetCSSPropertyBackgroundColor());
  }
  if (RuntimeEnabledFeatures::CompositeClipPathAnimationEnabled()) {
    element_animations->RecalcCompositedStatus(target,
                                               GetCSSPropertyClipPath());
  }
}

void Animation::Trace(Visitor* visitor) const {
  visitor->Trace(content_);
  visitor->Trace(document_);
  visitor->Trace(timeline_);
  visitor->Trace(pending_finished_event_);
  visitor->Trace(pending_cancelled_event_);
  visitor->Trace(pending_remove_event_);
  visitor->Trace(finished_promise_);
  visitor->Trace(ready_promise_);
  visitor->Trace(compositor_animation_);
  visitor->Trace(style_dependent_range_start_);
  visitor->Trace(style_dependent_range_end_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

Animation::CompositorAnimationHolder*
Animation::CompositorAnimationHolder::Create(
    Animation* animation,
    std::optional<int> replaced_cc_animation_id) {
  return MakeGarbageCollected<CompositorAnimationHolder>(
      animation, replaced_cc_animation_id);
}

Animation::CompositorAnimationHolder::CompositorAnimationHolder(
    Animation* animation,
    std::optional<int> replaced_cc_animation_id)
    : animation_(animation) {
  compositor_animation_ = CompositorAnimation::Create(replaced_cc_animation_id);
  compositor_animation_->SetAnimationDelegate(animation_);
}

void Animation::CompositorAnimationHolder::Dispose() {
  if (!animation_)
    return;
  animation_->Dispose();
  DCHECK(!animation_);
  DCHECK(!compositor_animation_);
}

void Animation::CompositorAnimationHolder::Detach() {
  DCHECK(compositor_animation_);
  compositor_animation_->SetAnimationDelegate(nullptr);
  animation_ = nullptr;
  compositor_animation_.reset();
}
}  // namespace blink
