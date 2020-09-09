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

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
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
#include "third_party/blink/renderer/core/animation/timing_calculations.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/events/animation_playback_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_timeline.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

enum PseudoPriority { kNone, kMarker, kBefore, kOther, kAfter };

unsigned NextSequenceNumber() {
  static unsigned next = 0;
  return ++next;
}

double SecondsToMilliseconds(double seconds) {
  return seconds * 1000;
}

double MillisecondsToSeconds(double milliseconds) {
  return milliseconds / 1000;
}

double Max(base::Optional<double> a, double b) {
  if (a.has_value())
    return std::max(a.value(), b);
  return b;
}

double Min(base::Optional<double> a, double b) {
  if (a.has_value())
    return std::min(a.value(), b);
  return b;
}

PseudoPriority ConvertPseudoIdtoPriority(const PseudoId& pseudo) {
  if (pseudo == kPseudoIdNone)
    return PseudoPriority::kNone;
  if (pseudo == kPseudoIdMarker)
    return PseudoPriority::kMarker;
  if (pseudo == kPseudoIdBefore)
    return PseudoPriority::kBefore;
  if (pseudo == kPseudoIdAfter)
    return PseudoPriority::kAfter;
  return PseudoPriority::kOther;
}

Animation::AnimationClassPriority AnimationPriority(
    const Animation& animation) {
  // According to the spec:
  // https://drafts.csswg.org/web-animations/#animation-class,
  // CSS tranisiton has a lower composite order than the CSS animation, and CSS
  // animation has a lower composite order than other animations. Thus,CSS
  // transitions are to appear before CSS animations and CSS animations are to
  // appear before other animations
  // TODO: When animations are disassociated from their element they are sorted
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
    : ExecutionContextLifecycleObserver(execution_context),
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
      effect_suppressed_(false) {
  if (content_) {
    if (content_->GetAnimation()) {
      content_->GetAnimation()->cancel();
      content_->GetAnimation()->setEffect(nullptr);
    }
    content_->Attach(this);
  }
  document_ = timeline_ ? timeline_->GetDocument()
                        : To<LocalDOMWindow>(execution_context)->document();
  DCHECK(document_);

  if (timeline_)
    timeline_->AnimationAttached(this);
  else
    document_->Timeline().AnimationAttached(this);

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

double Animation::EffectEnd() const {
  return content_ ? content_->SpecifiedTiming().EndTimeInternal() : 0;
}

bool Animation::Limited(base::Optional<double> current_time) const {
  return (EffectivePlaybackRate() < 0 && current_time <= 0) ||
         (EffectivePlaybackRate() > 0 && current_time >= EffectEnd());
}

Document* Animation::GetDocument() const {
  return document_;
}

base::Optional<double> Animation::TimelineTime() const {
  return timeline_ ? timeline_->currentTime() : base::nullopt;
}

// https://drafts.csswg.org/web-animations/#setting-the-current-time-of-an-animation.
void Animation::setCurrentTime(base::Optional<double> new_current_time,
                               ExceptionState& exception_state) {
  if (!new_current_time) {
    // If the current time is resolved, then throw a TypeError.
    if (CurrentTimeInternal()) {
      exception_state.ThrowTypeError(
          "currentTime may not be changed from resolved to unresolved");
    }
    return;
  }

  SetCurrentTimeInternal(MillisecondsToSeconds(new_current_time.value()));

  // Synchronously resolve pending pause task.
  if (pending_pause_) {
    SetHoldTimeAndPhase(MillisecondsToSeconds(new_current_time.value()),
                        TimelinePhase::kActive);
    ApplyPendingPlaybackRate();
    start_time_ = base::nullopt;
    pending_pause_ = false;
    if (ready_promise_)
      ResolvePromiseMaybeAsync(ready_promise_.Get());
  }

  // Update the finished state.
  UpdateFinishedState(UpdateType::kDiscontinuous, NotificationType::kAsync);

  SetCompositorPending(/*effect_changed=*/false);

  // Notify of potential state change.
  NotifyProbe();
}

void Animation::setCurrentTime(base::Optional<double> new_current_time) {
  NonThrowableExceptionState exception_state;
  setCurrentTime(new_current_time, exception_state);
}

// https://drafts.csswg.org/web-animations/#setting-the-current-time-of-an-animation
// See steps for silently setting the current time. The preliminary step of
// handling an unresolved time are to be handled by the caller.
void Animation::SetCurrentTimeInternal(double new_current_time) {
  DCHECK(std::isfinite(new_current_time));

  base::Optional<double> previous_start_time = start_time_;
  base::Optional<double> previous_hold_time = hold_time_;
  base::Optional<TimelinePhase> previous_hold_phase = hold_phase_;

  // Update either the hold time or the start time.
  if (hold_time_ || !start_time_ || !timeline_ || !timeline_->IsActive() ||
      playback_rate_ == 0) {
    SetHoldTimeAndPhase(new_current_time, TimelinePhase::kActive);
  } else {
    start_time_ = CalculateStartTime(new_current_time);
  }
  reset_current_time_on_resume_ = false;

  // Preserve invariant that we can only set a start time or a hold time in the
  // absence of an active timeline.
  if (!timeline_ || !timeline_->IsActive())
    start_time_ = base::nullopt;

  // Reset the previous current time.
  previous_current_time_ = base::nullopt;

  if (previous_start_time != start_time_ || previous_hold_time != hold_time_ ||
      previous_hold_phase != hold_phase_)
    SetOutdated();
}

void Animation::SetHoldTimeAndPhase(
    base::Optional<double> new_hold_time /* in seconds */,
    TimelinePhase new_hold_phase) {
  // new_hold_time must be valid, unless new_hold_phase is inactive.
  DCHECK(new_hold_time ||
         (!new_hold_time && new_hold_phase == TimelinePhase::kInactive));
  hold_time_ = new_hold_time;
  hold_phase_ = new_hold_phase;
}

void Animation::ResetHoldTimeAndPhase() {
  hold_time_ = base::nullopt;
  hold_phase_ = base::nullopt;
}

base::Optional<double> Animation::startTime() const {
  return start_time_
             ? base::make_optional(SecondsToMilliseconds(start_time_.value()))
             : base::nullopt;
}

// https://drafts.csswg.org/web-animations/#the-current-time-of-an-animation
base::Optional<double> Animation::currentTime() const {
  // 1. If the animation’s hold time is resolved,
  //    The current time is the animation’s hold time.
  if (hold_time_.has_value())
    return SecondsToMilliseconds(hold_time_.value());

  // 2.  If any of the following are true:
  //    * the animation has no associated timeline, or
  //    * the associated timeline is inactive, or
  //    * the animation’s start time is unresolved.
  // The current time is an unresolved time value.
  if (!timeline_ || !timeline_->IsActive() || !start_time_)
    return base::nullopt;

  // 3. Otherwise,
  // current time = (timeline time - start time) × playback rate
  base::Optional<double> timeline_time = timeline_->CurrentTimeSeconds();

  // An active timeline should always have a value, and since inactive timeline
  // is handled in step 2 above, make sure that timeline_time has a value.
  DCHECK(timeline_time.has_value());

  double current_time =
      (timeline_time.value() - start_time_.value()) * playback_rate_;
  return SecondsToMilliseconds(current_time);
}

bool Animation::ValidateHoldTimeAndPhase() const {
  return (hold_phase_ && hold_time_) ||
         ((!hold_phase_ || hold_phase_ == TimelinePhase::kInactive) &&
          !hold_time_);
}

base::Optional<double> Animation::CurrentTimeInternal() const {
  DCHECK(ValidateHoldTimeAndPhase());
  return hold_time_ ? hold_time_ : CalculateCurrentTime();
}

TimelinePhase Animation::CurrentPhaseInternal() const {
  DCHECK(ValidateHoldTimeAndPhase());
  return hold_phase_ ? hold_phase_.value() : CalculateCurrentPhase();
}

base::Optional<double> Animation::UnlimitedCurrentTime() const {
  return CalculateAnimationPlayState() == kPaused || !start_time_
             ? CurrentTimeInternal()
             : CalculateCurrentTime();
}

String Animation::playState() const {
  return PlayStateString();
}

bool Animation::PreCommit(
    int compositor_group,
    const PaintArtifactCompositor* paint_artifact_compositor,
    bool start_on_compositor) {

  bool soft_change =
      compositor_state_ &&
      (Paused() || compositor_state_->playback_rate != EffectivePlaybackRate());
  bool hard_change =
      compositor_state_ && (compositor_state_->effect_changed ||
                            compositor_state_->start_time != start_time_ ||
                            !compositor_state_->start_time || !start_time_);

  // FIXME: softChange && !hardChange should generate a Pause/ThenStart,
  // not a Cancel, but we can't communicate these to the compositor yet.

  bool changed = soft_change || hard_change;
  bool should_cancel = (!Playing() && compositor_state_) || changed;
  bool should_start = Playing() && (!compositor_state_ || changed);

  if (start_on_compositor && should_cancel && should_start &&
      compositor_state_ && compositor_state_->pending_action == kStart) {
    // Restarting but still waiting for a start time.
    return false;
  }

  if (should_cancel) {
    CancelAnimationOnCompositor();
    compositor_state_ = nullptr;
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
        CreateCompositorAnimation();
        StartAnimationOnCompositor(paint_artifact_compositor);
        compositor_state_ = std::make_unique<CompositorState>(*this);
      } else {
        CancelIncompatibleAnimationsOnCompositor();
      }
      DCHECK_EQ(kRunning, CalculateAnimationPlayState());
      TRACE_EVENT_NESTABLE_ASYNC_INSTANT1(
          "blink.animations,devtools.timeline,benchmark,rail", "Animation",
          this, "data",
          inspector_animation_compositor_event::Data(failure_reasons,
                                                     unsupported_properties));
    }
  }

  return true;
}

void Animation::PostCommit() {
  compositor_pending_ = false;

  if (!compositor_state_ || compositor_state_->pending_action == kNone)
    return;

  DCHECK_EQ(kStart, compositor_state_->pending_action);
  if (compositor_state_->start_time) {
    DCHECK_EQ(start_time_.value(), compositor_state_->start_time.value());
    compositor_state_->pending_action = kNone;
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
    const PseudoId pseudo1 = owning_element1->GetPseudoId();
    const PseudoId pseudo2 = owning_element2->GetPseudoId();
    PseudoPriority priority1 = ConvertPseudoIdtoPriority(pseudo1);
    PseudoPriority priority2 = ConvertPseudoIdtoPriority(pseudo2);

    if (priority1 != priority2)
      return priority1 < priority2;

    // The following if statement is not reachable, but the implementation
    // matches the specification for composite ordering
    if (priority1 == kOther && pseudo1 != pseudo2) {
      return CodeUnitCompareLessThan(
          PseudoElement::PseudoElementNameForEvents(pseudo1),
          PseudoElement::PseudoElementNameForEvents(pseudo2));
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

void Animation::NotifyReady(double ready_time) {
  // Complete the pending updates prior to updating the compositor state in
  // order to ensure a correct start time for the compositor state without the
  // need to duplicate the calculations.
  if (pending_play_)
    CommitPendingPlay(ready_time);
  else if (pending_pause_)
    CommitPendingPause(ready_time);

  if (compositor_state_ && compositor_state_->pending_action == kStart) {
    DCHECK(!compositor_state_->start_time);
    compositor_state_->pending_action = kNone;
    compositor_state_->start_time = start_time_;
  }

  // Notify of change to play state.
  NotifyProbe();
}

// Microtask for playing an animation.
// Refer to Step 8.3 'pending play task' in
// https://drafts.csswg.org/web-animations/#playing-an-animation-section.
void Animation::CommitPendingPlay(double ready_time) {
  DCHECK(!Timing::IsNull(ready_time));
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
      ResetHoldTimeAndPhase();
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
    double current_time_to_match =
        (ready_time - start_time_.value()) * playback_rate_;
    ApplyPendingPlaybackRate();
    if (playback_rate_ == 0) {
      SetHoldTimeAndPhase(current_time_to_match, CalculateCurrentPhase());
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
// Refer to step 7 'pending pause task' in
// https://drafts.csswg.org/web-animations-1/#pausing-an-animation-section
void Animation::CommitPendingPause(double ready_time) {
  DCHECK(pending_pause_);
  pending_pause_ = false;

  // 1. Let ready time be the time value of the timeline associated with
  //    animation at the moment when the user agent completed processing
  //    necessary to suspend playback of animation’s associated effect.
  // 2. If animation’s start time is resolved and its hold time is not resolved,
  //    let animation’s hold time be the result of evaluating
  //    (ready time - start time) × playback rate.
  if (start_time_ && !hold_time_) {
    SetHoldTimeAndPhase((ready_time - start_time_.value()) * playback_rate_,
                        CalculateCurrentPhase());
  }

  // 3. Apply any pending playback rate on animation.
  // 4. Make animation’s start time unresolved.
  ApplyPendingPlaybackRate();
  start_time_ = base::nullopt;

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

void Animation::setTimeline(AnimationTimeline* timeline) {
  // https://drafts.csswg.org/web-animations-1/#setting-the-timeline

  // Steps refined to accommodate scroll timelines.
  // TODO(crbug.com/827626): Update the web-animation-1 spec.
  // https://github.com/w3c/csswg-drafts/pull/5423.

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
  base::Optional<double> old_current_time = CurrentTimeInternal();

  CancelAnimationOnCompositor();

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
  if (timeline)
    timeline->AnimationAttached(this);
  else
    document_->Timeline().AnimationAttached(this);
  SetOutdated();

  reset_current_time_on_resume_ = false;

  if (timeline) {
    if (!timeline->IsMonotonicallyIncreasing()) {
      ApplyPendingPlaybackRate();
      double boundary_time = (playback_rate_ > 0) ? 0 : EffectEnd();
      switch (old_play_state) {
        case kIdle:
          break;

        case kRunning:
        case kFinished:
          // A non-monotonic timeline has a fixed start time at the beginning or
          // end of the timeline.
          start_time_ = boundary_time;
          break;

        case kPaused:
          if (old_current_time) {
            reset_current_time_on_resume_ = true;
            start_time_ = base::nullopt;
            hold_time_ = old_current_time.value();
          } else if (PendingInternal()) {
            start_time_ = boundary_time;
          }
          break;

        default:
          NOTREACHED();
      }
    } else if (old_current_time && old_timeline &&
               !old_timeline->IsMonotonicallyIncreasing()) {
      SetCurrentTimeInternal(old_current_time.value());
    }
  }

  // 4. If the start time of animation is resolved, make the animation’s hold
  //    time unresolved. This step ensures that the finished play state of the
  //    animation is not “sticky” but is re-evaluated based on its updated
  //    current time.
  if (start_time_)
    ResetHoldTimeAndPhase();

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

  SetCompositorPending(false);

  // Inform devtools of a potential change to the play state.
  NotifyProbe();
}

base::Optional<double> Animation::CalculateStartTime(
    double current_time) const {
  base::Optional<double> start_time;
  if (timeline_) {
    base::Optional<double> timeline_time = timeline_->CurrentTimeSeconds();
    if (timeline_time)
      start_time = timeline_time.value() - current_time / playback_rate_;
    // TODO(crbug.com/916117): Handle NaN time for scroll-linked animations.
    DCHECK(start_time || timeline_->IsScrollTimeline());
  }
  return start_time;
}

base::Optional<double> Animation::CalculateCurrentTime() const {
  if (!start_time_ || !timeline_ || !timeline_->IsActive())
    return base::nullopt;
  base::Optional<double> timeline_time = timeline_->CurrentTimeSeconds();

  if (!timeline_time) {
    // timeline_time can be null only when the timeline is inactive
    DCHECK(!timeline_->IsActive());
    return base::nullopt;
  }

  return (timeline_time.value() - start_time_.value()) * playback_rate_;
}

TimelinePhase Animation::CalculateCurrentPhase() const {
  if (!start_time_ || !timeline_)
    return TimelinePhase::kInactive;
  return timeline_->Phase();
}

// https://drafts.csswg.org/web-animations/#setting-the-start-time-of-an-animation
void Animation::setStartTime(base::Optional<double> start_time_ms,
                             ExceptionState& exception_state) {
  bool had_start_time = start_time_.has_value();

  // 1. Let timeline time be the current time value of the timeline that
  //    animation is associated with. If there is no timeline associated with
  //    animation or the associated timeline is inactive, let the timeline time
  //    be unresolved.
  base::Optional<double> timeline_time = timeline_ && timeline_->IsActive()
                                             ? timeline_->CurrentTimeSeconds()
                                             : base::nullopt;

  // 2. If timeline time is unresolved and new start time is resolved, make
  //    animation’s hold time unresolved.
  // This preserves the invariant that when we don’t have an active timeline it
  // is only possible to set either the start time or the animation’s current
  // time.
  if (!timeline_time && start_time_ms) {
    ResetHoldTimeAndPhase();
  }

  // 3. Let previous current time be animation’s current time.
  base::Optional<double> previous_current_time = CurrentTimeInternal();
  TimelinePhase previous_current_phase = CurrentPhaseInternal();

  // 4. Apply any pending playback rate on animation.
  ApplyPendingPlaybackRate();

  // 5. Set animation’s start time to new start time.
  base::Optional<double> new_start_time;
  if (start_time_ms) {
    new_start_time = MillisecondsToSeconds(start_time_ms.value());
    // Snap to timeline time if within floating point tolerance to ensure
    // deterministic behavior in phase transitions.
    if (timeline_time && IsWithinAnimationTimeEpsilon(timeline_time.value(),
                                                      new_start_time.value())) {
      new_start_time = timeline_time.value();
    }
  }
  start_time_ = new_start_time;
  reset_current_time_on_resume_ = false;

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
      ResetHoldTimeAndPhase();
    }
  } else {
    SetHoldTimeAndPhase(previous_current_time, previous_current_phase);
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
  base::Optional<double> new_current_time = CurrentTimeInternal();
  // Even when the animation is not outdated,call SetOutdated to ensure
  // the animation is tracked by its timeline for future timing
  // updates.
  if (previous_current_time != new_current_time ||
      (!had_start_time && start_time_)) {
    SetOutdated();
  }
  SetCompositorPending(/*effect_changed=*/false);

  NotifyProbe();
}

void Animation::setStartTime(base::Optional<double> start_time_ms) {
  NonThrowableExceptionState exception_state;
  setStartTime(start_time_ms, exception_state);
}

// https://drafts.csswg.org/web-animations-1/#setting-the-associated-effect
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

  // 6. Let the associated effect of the animation be the new effect.
  if (old_effect)
    old_effect->Detach();
  content_ = new_effect;
  if (new_effect)
    new_effect->Attach(this);
  SetOutdated();

  // 7. Run the procedure to update an animation’s finished state for animation
  //    with the did seek flag set to false (continuous), and the synchronously
  //    notify flag set to false (async).
  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);

  SetCompositorPending(/*effect_change=*/true);

  // Notify of a potential state change.
  NotifyProbe();

  // The effect is no longer associated with CSS properties.
  if (new_effect) {
    new_effect->SetIgnoreCssTimingProperties();
    if (KeyframeEffect* keyframe_effect = DynamicTo<KeyframeEffect>(new_effect))
      keyframe_effect->SetIgnoreCSSKeyframes();
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
    base::Optional<double> current_time = CurrentTimeInternal();
    Timing::Phase phase;
    if (!current_time)
      phase = Timing::kPhaseNone;
    else if (current_time < 0)
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
      NOTREACHED();
      return "";
  }
}

// https://drafts.csswg.org/web-animations/#play-states
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

// https://drafts.csswg.org/web-animations-1/#reset-an-animations-pending-tasks.
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

// https://drafts.csswg.org/web-animations/#pausing-an-animation-section
void Animation::pause(ExceptionState& exception_state) {
  // 1. If animation has a pending pause task, abort these steps.
  // 2. If the play state of animation is paused, abort these steps.
  if (pending_pause_ || CalculateAnimationPlayState() == kPaused)
    return;

  // 3. Let seek time be a time value that is initially unresolved.
  base::Optional<double> seek_time;

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
  base::Optional<double> current_time = CurrentTimeInternal();
  if (!current_time) {
    if (playback_rate_ >= 0) {
      seek_time = 0;
    } else {
      if (EffectEnd() == std::numeric_limits<double>::infinity()) {
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
    if (has_finite_timeline) {
      start_time_ = seek_time;
    } else {
      SetHoldTimeAndPhase(seek_time, TimelinePhase::kActive);
    }
  }

  // 7. Let has pending ready promise be a boolean flag that is initially false.
  // 8. If animation has a pending play task, cancel that task and let has
  //    pending ready promise be true.
  // 9. If has pending ready promise is false, set animation’s current ready
  //    promise to a new promise in the relevant Realm of animation.
  if (pending_play_)
    pending_play_ = false;
  else if (ready_promise_)
    ready_promise_->Reset();

  // 10. Schedule a task to be executed at the first possible moment where both
  //    of the following conditions are true:
  //    10a. the user agent has performed any processing necessary to suspend
  //        the playback of animation’s associated effect, if any.
  //    10b. the animation is associated with a timeline that is not inactive.
  pending_pause_ = true;
  pending_play_ = false;

  SetOutdated();
  SetCompositorPending(false);

  // 11. Run the procedure to update an animation’s finished state for animation
  //    with the did seek flag set to false (continuous) , and thesynchronously
  //    notify flag set to false.
  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);

  NotifyProbe();
}

// ----------------------------------------------
// Play methods.
// ----------------------------------------------

// Refer to the unpause operation in the following spec:
// https://drafts.csswg.org/css-animations-1/#animation-play-state
void Animation::Unpause() {
  if (CalculateAnimationPlayState() != kPaused)
    return;
  PlayInternal(AutoRewind::kDisabled, ASSERT_NO_EXCEPTION);
}

// https://drafts.csswg.org/web-animations/#programming-interface.
void Animation::play(ExceptionState& exception_state) {
  // Begin or resume playback of the animation by running the procedure to
  // play an animation passing true as the value of the auto-rewind flag.
  PlayInternal(AutoRewind::kEnabled, exception_state);
}

// https://drafts.csswg.org/web-animations/#playing-an-animation-section.
void Animation::PlayInternal(AutoRewind auto_rewind,
                             ExceptionState& exception_state) {
  // 1. Let aborted pause be a boolean flag that is true if animation has a
  //    pending pause task, and false otherwise.
  // 2. Let has pending ready promise be a boolean flag that is initially false.
  // 3. Let seek time be a time value that is initially unresolved.
  // 4. Let has finite timeline be true if animation has an associated timeline
  //    that is not monotonically increasing.
  bool aborted_pause = pending_pause_;
  bool enable_seek =
      auto_rewind == AutoRewind::kEnabled || reset_current_time_on_resume_;
  bool has_pending_ready_promise = false;
  base::Optional<double> seek_time;
  bool has_finite_timeline =
      timeline_ && !timeline_->IsMonotonicallyIncreasing();

  // 5. Perform the steps corresponding to the first matching condition from the
  //    following, if any:
  //
  // 5a If animation’s effective playback rate > 0, the auto-rewind flag is true
  //    and either animation’s:
  //      current time is unresolved, or
  //      current time < zero, or
  //      current time ≥ target effect end,
  //    5a1. Set seek time to zero.
  //
  // 5b If animation’s effective playback rate < 0, the auto-rewind flag is true
  //    and either animation’s:
  //      current time is unresolved, or
  //      current time ≤ zero, or
  //      current time > target effect end,
  //    5b1. If associated effect end is positive infinity,
  //         throw an "InvalidStateError" DOMException and abort these steps.
  //    5b2. Otherwise,
  //         5b2a Set seek time to animation's associated effect end.
  //
  // 5c If animation’s effective playback rate = 0 and animation’s current time
  //    is unresolved,
  //    5c1. Set seek time to zero.
  double effective_playback_rate = EffectivePlaybackRate();
  base::Optional<double> current_time = CurrentTimeInternal();

  if (reset_current_time_on_resume_) {
    current_time = base::nullopt;
    reset_current_time_on_resume_ = false;
  }

  if (effective_playback_rate > 0 && enable_seek &&
      (!current_time || current_time < 0 || current_time >= EffectEnd())) {
    seek_time = 0;

  } else if (effective_playback_rate < 0 && enable_seek &&
             (!current_time || current_time <= 0 ||
              current_time > EffectEnd())) {
    if (EffectEnd() == std::numeric_limits<double>::infinity()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "Cannot play reversed Animation with infinite target effect end.");
      return;
    }
    seek_time = EffectEnd();
  } else if (effective_playback_rate == 0 && !current_time) {
    seek_time = 0;
  }

  // 6. If seek time is resolved,
  //    6a. If has finite timeline is true,
  //        6a1. Set animation's start time to seek time.
  //        6a2. Let animation's hold time be unresolved.
  //        6a3. Apply any pending playback rate on animation.
  //    6b. Otherwise,
  //        Set animation's hold time to seek time.
  if (seek_time) {
    if (has_finite_timeline) {
      start_time_ = seek_time;
      ResetHoldTimeAndPhase();
      ApplyPendingPlaybackRate();
    } else {
      SetHoldTimeAndPhase(seek_time, TimelinePhase::kActive);
    }
  }

  // 7. If animation's hold time is resolved, let its start time be unresolved.
  if (hold_time_)
    start_time_ = base::nullopt;

  // 8. If animation has a pending play task or a pending pause task,
  //   8.1 Cancel that task.
  //   8.2 Set has pending ready promise to true.
  if (pending_play_ || pending_pause_) {
    pending_play_ = pending_pause_ = false;
    has_pending_ready_promise = true;
  }

  // 9. If the following three conditions are all satisfied:
  //      animation’s hold time is unresolved, and
  //      seek time is unresolved, and
  //      aborted pause is false, and
  //      animation does not have a pending playback rate,
  //    abort this procedure.
  if (!hold_time_ && !seek_time && !aborted_pause && !pending_playback_rate_)
    return;

  // 10. If has pending ready promise is false, let animation’s current ready
  //    promise be a new promise in the relevant Realm of animation.
  if (ready_promise_ && !has_pending_ready_promise)
    ready_promise_->Reset();

  // 11. Schedule a task to run as soon as animation is ready.
  pending_play_ = true;
  finished_ = false;
  committed_finish_notification_ = false;
  SetOutdated();
  SetCompositorPending(/*effect_changed=*/false);

  // 12. Run the procedure to update an animation’s finished state for animation
  //    with the did seek flag set to false, and the synchronously notify flag
  //    set to false.
  // Boolean valued arguments replaced with enumerated values for clarity.
  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);

  // Notify change to pending play or finished state.
  NotifyProbe();
}

// https://drafts.csswg.org/web-animations/#reversing-an-animation-section
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
  base::Optional<double> original_pending_playback_rate =
      pending_playback_rate_;
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

// https://drafts.csswg.org/web-animations/#finishing-an-animation-section
void Animation::finish(ExceptionState& exception_state) {
  if (!EffectivePlaybackRate()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot finish Animation with a playbackRate of 0.");
    return;
  }
  if (EffectivePlaybackRate() > 0 &&
      EffectEnd() == std::numeric_limits<double>::infinity()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot finish Animation with an infinite target effect end.");
    return;
  }

  ApplyPendingPlaybackRate();

  double new_current_time = playback_rate_ < 0 ? 0 : EffectEnd();
  SetCurrentTimeInternal(new_current_time);

  if (!start_time_ && timeline_ && timeline_->IsActive())
    start_time_ = CalculateStartTime(new_current_time);

  if (pending_pause_ && start_time_) {
    ResetHoldTimeAndPhase();
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
  bool did_seek = update_type == UpdateType::kDiscontinuous;
  // 1. Calculate the unconstrained current time. The dependency on did_seek is
  // required to accommodate timelines that may change direction. Without this
  // distinction, a once-finished animation would remain finished even when its
  // timeline progresses in the opposite direction.
  base::Optional<double> unconstrained_current_time =
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
    base::Optional<double> hold_time;
    TimelinePhase hold_phase;
    if (playback_rate > 0 && unconstrained_current_time >= EffectEnd()) {
      hold_time = did_seek ? unconstrained_current_time
                           : Max(previous_current_time_, EffectEnd());
      hold_phase = did_seek ? TimelinePhase::kActive : CalculateCurrentPhase();

      SetHoldTimeAndPhase(hold_time, hold_phase);
    } else if (playback_rate < 0 && unconstrained_current_time <= 0) {
      hold_time = did_seek ? unconstrained_current_time
                           : Min(previous_current_time_, 0);
      hold_phase = did_seek ? TimelinePhase::kActive : CalculateCurrentPhase();

      // Hack for resolving precision issue at zero.
      if (hold_time.value() == -0)
        hold_time = 0;

      SetHoldTimeAndPhase(hold_time, hold_phase);
    } else if (playback_rate != 0) {
      // Update start time and reset hold time.
      if (did_seek && hold_time_)
        start_time_ = CalculateStartTime(hold_time_.value());
      ResetHoldTimeAndPhase();
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
    if (finished_)
      SetCompositorPending();
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
  // Run a task to handle the finished promise and event as a microtask. With
  // the exception of an explicit call to Animation::finish, it is important to
  // apply these updates asynchronously as it is possible to enter the finished
  // state temporarily.
  pending_finish_notification_ = true;
  if (!has_queued_microtask_) {
    Microtask::EnqueueMicrotask(
        WTF::Bind(&Animation::AsyncFinishMicrotask, WrapWeakPersistent(this)));
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
      NotifyReady(timeline_->CurrentTimeSeconds().value_or(0));
    CommitFinishNotification();
  }

  // This is a once callback and needs to be re-armed.
  has_queued_microtask_ = false;
}

// Refer to 'finished notification steps' in
// https://drafts.csswg.org/web-animations-1/#updating-the-finished-state
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

// https://drafts.csswg.org/web-animations/#setting-the-playback-rate-of-an-animation
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
      base::Optional<double> unconstrained_current_time =
          CalculateCurrentTime();
      base::Optional<double> timeline_time =
          timeline_ ? timeline_->CurrentTimeSeconds() : base::nullopt;
      if (playback_rate) {
        if (timeline_time) {
          start_time_ = (timeline_time && unconstrained_current_time)
                            ? base::make_optional<double>(
                                  (timeline_time.value() -
                                   unconstrained_current_time.value()) /
                                  playback_rate)
                            : base::nullopt;
        }
      } else {
        start_time_ = timeline_time;
      }
      ApplyPendingPlaybackRate();
      UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);
      SetCompositorPending(false);
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
      NOTREACHED();
  }
}

ScriptPromise Animation::finished(ScriptState* script_state) {
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

ScriptPromise Animation::ready(ScriptState* script_state) {
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
  return EventTargetWithInlineData::DispatchEventInternal(event);
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
    pending_playback_rate_ = base::nullopt;
  }
}

void Animation::setPlaybackRate(double playback_rate,
                                ExceptionState& exception_state) {
  base::Optional<double> start_time_before = start_time_;

  // 1. Clear any pending playback rate on animation.
  // 2. Let previous time be the value of the current time of animation before
  //    changing the playback rate.
  // 3. Set the playback rate to new playback rate.
  // 4. If previous time is resolved, set the current time of animation to
  //    previous time
  pending_playback_rate_ = base::nullopt;
  base::Optional<double> previous_current_time = currentTime();
  playback_rate_ = playback_rate;
  if (previous_current_time.has_value()) {
    setCurrentTime(previous_current_time, exception_state);
  }

  // Adds a UseCounter to check if setting playbackRate causes a compensatory
  // seek forcing a change in start_time_
  if (start_time_before && start_time_ != start_time_before &&
      CalculateAnimationPlayState() != kFinished) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kAnimationSetPlaybackRateCompensatorySeek);
  }

  SetCompositorPending(false);
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
  if (EffectivePlaybackRate() == 0)
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  if (!CurrentTimeInternal())
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  // Cannot composite an infinite duration animation with a negative playback
  // rate. TODO(crbug.com/1029167): Fix calculation of compositor timing to
  // enable compositing provided the iteration duration is finite. Having an
  // infinite number of iterations in the animation should not impede the
  // ability to composite the animation.
  if (std::isinf(EffectEnd()) && EffectivePlaybackRate() < 0)
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  // An Animation without a timeline effectively isn't playing, so there is no
  // reason to composite it. Additionally, mutating the timeline playback rate
  // is a debug feature available via devtools; we don't support this on the
  // compositor currently and there is no reason to do so.
  if (!timeline_ || (timeline_->IsDocumentTimeline() &&
                     To<DocumentTimeline>(*timeline_).PlaybackRate() != 1))
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  // If the scroll source is not composited, fall back to main thread.
  // TODO(crbug.com/476553): Once all ScrollNodes including uncomposited ones
  // are in the compositor, the animation should be composited.
  if (timeline_ && timeline_->IsScrollTimeline() &&
      !CompositorAnimations::CheckUsesCompositedScrolling(
          To<ScrollTimeline>(*timeline_).ResolvedScrollSource()))
    reasons |= CompositorAnimations::kTimelineSourceHasInvalidCompositingState;

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

void Animation::StartAnimationOnCompositor(
    const PaintArtifactCompositor* paint_artifact_compositor) {
  DCHECK_EQ(
      CheckCanStartAnimationOnCompositor(paint_artifact_compositor, nullptr),
      CompositorAnimations::kNoFailure);

  bool reversed = EffectivePlaybackRate() < 0;

  base::Optional<double> start_time = base::nullopt;
  double time_offset = 0;
  // Start the animation on the compositor with either a start time or time
  // offset. The start time is used for synchronous updates where the
  // compositor start time must be in precise alignment with the specified time
  // (e.g. after calling setStartTime). Asynchronous updates such as updating
  // the playback rate preserve current time even if the start time is set.
  // Asynchronous updates have an associated pending play or pending pause
  // task associated with them.
  if (start_time_ && !PendingInternal()) {
    start_time = timeline_->ZeroTimeInSeconds() + start_time_.value();
    if (reversed) {
      start_time =
          start_time.value() - (EffectEnd() / fabs(EffectivePlaybackRate()));
    }
  } else {
    base::Optional<double> current_time = CurrentTimeInternal();
    DCHECK(current_time);
    time_offset =
        reversed ? EffectEnd() - current_time.value() : current_time.value();
    time_offset = time_offset / fabs(EffectivePlaybackRate());
  }

  DCHECK(!start_time || !Timing::IsNull(start_time.value()));
  DCHECK_NE(compositor_group_, 0);
  DCHECK(To<KeyframeEffect>(content_.Get()));
  DCHECK(std::isfinite(time_offset));
  To<KeyframeEffect>(content_.Get())
      ->StartAnimationOnCompositor(compositor_group_, start_time,
                                   base::TimeDelta::FromSecondsD(time_offset),
                                   EffectivePlaybackRate());
}

// TODO(crbug.com/960944): Rename to SetPendingCommit. This method handles both
// composited and non-composited animations. The use of 'compositor' in the name
// is confusing.
void Animation::SetCompositorPending(bool effect_changed) {
  // FIXME: KeyframeEffect could notify this directly?
  if (!HasActiveAnimationsOnCompositor()) {
    DestroyCompositorAnimation();
    compositor_state_.reset();
  }
  if (effect_changed && compositor_state_) {
    compositor_state_->effect_changed = true;
  }
  if (compositor_pending_ || is_paused_for_testing_) {
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
      compositor_state_->playback_rate != EffectivePlaybackRate() ||
      compositor_state_->start_time != start_time_ ||
      !compositor_state_->start_time || !start_time_) {
    compositor_pending_ = true;
    document_->GetPendingAnimations().Add(this);
  }
}

void Animation::CancelAnimationOnCompositor() {
  if (HasActiveAnimationsOnCompositor()) {
    To<KeyframeEffect>(content_.Get())
        ->CancelAnimationOnCompositor(GetCompositorAnimation());
  }

  DestroyCompositorAnimation();
}

void Animation::RestartAnimationOnCompositor() {
  if (!HasActiveAnimationsOnCompositor())
    return;
  if (To<KeyframeEffect>(content_.Get())
          ->CancelAnimationOnCompositor(GetCompositorAnimation()))
    SetCompositorPending(true);
}

void Animation::CancelIncompatibleAnimationsOnCompositor() {
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
// https://drafts.csswg.org/web-animations/#update-animations-and-send-events
bool Animation::Update(TimingUpdateReason reason) {
  // Due to the hierarchical nature of the timing model, updating the current
  // time of an animation also involves:
  //   * Running the update an animation’s finished state procedure.
  //   * Queueing animation events.
  ClearOutdated();
  bool idle = CalculateAnimationPlayState() == kIdle;
  if (!idle)
    UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);

  if (content_) {
    base::Optional<double> inherited_time;
    TimelinePhase inherited_phase = TimelinePhase::kInactive;

    if (!idle) {
      inherited_time = CurrentTimeInternal();
      // Special case for end-exclusivity when playing backwards.
      if (inherited_time == 0 && EffectivePlaybackRate() < 0)
        inherited_time = -1;

      inherited_phase = CurrentPhaseInternal();
    }

    content_->UpdateInheritedTime(inherited_time, inherited_phase, reason);

    // After updating the animation time if the animation is no longer current
    // blink will no longer composite the element (see
    // CompositingReasonFinder::RequiresCompositingFor*Animation). We cancel any
    // running compositor animation so that we don't try to animate the
    // non-existent element on the compositor.
    if (!content_->IsCurrent())
      CancelAnimationOnCompositor();
  }

  if (reason == kTimingUpdateForAnimationFrame) {
    if (idle || CalculateAnimationPlayState() == kFinished) {
      // TODO(crbug.com/1029348): Per spec, we should have a microtask
      // checkpoint right after the update cycle. Once this is fixed we should
      // no longer need to force a synchronous resolution here.
      AsyncFinishMicrotask();
      finished_ = true;
    }
  }

  DCHECK(!outdated_);
  NotifyProbe();

  return !finished_ || TimeToEffectChange() ||
         // Always return true for not idle animations attached to not
         // monotonically increasing timelines even if the animation is
         // finished. This is required to accommodate cases where timeline ticks
         // back in time.
         (!idle && !timeline_->IsMonotonicallyIncreasing());
}

void Animation::QueueFinishedEvent() {
  const AtomicString& event_type = event_type_names::kFinish;
  if (GetExecutionContext() && HasEventListeners(event_type)) {
    base::Optional<double> event_current_time = CurrentTimeInternal();
    if (event_current_time)
      event_current_time = SecondsToMilliseconds(event_current_time.value());
    // TODO(crbug.com/916117): Handle NaN values for scroll-linked animations.
    pending_finished_event_ = MakeGarbageCollected<AnimationPlaybackEvent>(
        event_type, event_current_time, TimelineTime());
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
  SetCompositorPending(true);
}

bool Animation::IsEventDispatchAllowed() const {
  return Paused() || start_time_;
}

base::Optional<AnimationTimeDelta> Animation::TimeToEffectChange() {
  DCHECK(!outdated_);
  if (!start_time_ || hold_time_ || !playback_rate_)
    return base::nullopt;

  if (!content_) {
    base::Optional<double> current_time = CurrentTimeInternal();
    DCHECK(current_time);
    return AnimationTimeDelta::FromSecondsD(-current_time.value() /
                                            playback_rate_);
  }

  if (!HasActiveAnimationsOnCompositor() &&
      (content_->GetPhase() == Timing::kPhaseActive))
    return AnimationTimeDelta();

  return (playback_rate_ > 0)
             ? (content_->TimeToForwardsEffectChange() / playback_rate_)
             : (content_->TimeToReverseEffectChange() / -playback_rate_);
}

void Animation::cancel() {
  double current_time_before_cancel = CurrentTimeInternal().value_or(0);
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
      base::Optional<double> event_current_time = base::nullopt;
      // TODO(crbug.com/916117): Handle NaN values for scroll-linked
      // animations.
      pending_cancelled_event_ = MakeGarbageCollected<AnimationPlaybackEvent>(
          event_type, event_current_time, TimelineTime());
      pending_cancelled_event_->SetTarget(this);
      pending_cancelled_event_->SetCurrentTarget(this);
      document_->EnqueueAnimationFrameEvent(pending_cancelled_event_);
    }
  } else {
    // Quietly reset without rejecting promises.
    pending_playback_rate_ = base::nullopt;
    pending_pause_ = pending_play_ = false;
  }

  ResetHoldTimeAndPhase();
  start_time_ = base::nullopt;

  // Apply changes synchronously.
  SetCompositorPending(/*effect_changed=*/false);
  SetOutdated();

  // Force dispatch of canceled event.
  if (content_)
    content_->SetCancelTime(current_time_before_cancel);
  Update(kTimingUpdateOnDemand);

  // Notify of change to canceled state.
  NotifyProbe();
}

void Animation::CreateCompositorAnimation() {
  if (Platform::Current()->IsThreadedAnimationEnabled() &&
      !compositor_animation_) {
    compositor_animation_ = CompositorAnimationHolder::Create(this);
    AttachCompositorTimeline();
  }

  AttachCompositedLayers();
}

void Animation::DestroyCompositorAnimation() {
  DetachCompositedLayers();

  if (compositor_animation_) {
    DetachCompositorTimeline();
    compositor_animation_->Detach();
    compositor_animation_ = nullptr;
  }
}

void Animation::AttachCompositorTimeline() {
  DCHECK(compositor_animation_);

  // Register ourselves on the compositor timeline. This will cause our cc-side
  // animation animation to be registered.
  CompositorAnimationTimeline* compositor_timeline =
      timeline_ ? timeline_->EnsureCompositorTimeline() : nullptr;
  if (!compositor_timeline)
    return;

  compositor_timeline->AnimationAttached(*this);
  // Note that while we attach here but we don't detach because the
  // |compositor_timeline| is detached in its destructor.
  if (compositor_timeline->GetAnimationTimeline()->IsScrollTimeline())
    document_->AttachCompositorTimeline(compositor_timeline);
}

void Animation::DetachCompositorTimeline() {
  DCHECK(compositor_animation_);

  CompositorAnimationTimeline* compositor_timeline =
      timeline_ ? timeline_->CompositorTimeline() : nullptr;
  if (!compositor_timeline)
    return;

  compositor_timeline->AnimationDestroyed(*this);
}

void Animation::AttachCompositedLayers() {
  if (!compositor_animation_)
    return;

  DCHECK(content_);
  DCHECK(IsA<KeyframeEffect>(*content_));

  To<KeyframeEffect>(content_.Get())->AttachCompositedLayers();
}

void Animation::DetachCompositedLayers() {
  if (compositor_animation_ &&
      compositor_animation_->GetAnimation()->IsElementAttached())
    compositor_animation_->GetAnimation()->DetachElement();
}

void Animation::NotifyAnimationStarted(double monotonic_time, int group) {
  document_->GetPendingAnimations().NotifyCompositorAnimationStarted(
      monotonic_time, group);
}

void Animation::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);
  if (event_type == event_type_names::kFinish)
    UseCounter::Count(GetExecutionContext(), WebFeature::kAnimationFinishEvent);
}

void Animation::PauseForTesting(double pause_time) {
  // Do not restart a canceled animation.
  if (CalculateAnimationPlayState() == kIdle)
    return;

  // Pause a running animation, or update the hold time of a previously paused
  // animation.
  SetCurrentTimeInternal(pause_time);
  if (HasActiveAnimationsOnCompositor()) {
    base::Optional<double> current_time = CurrentTimeInternal();
    DCHECK(current_time);
    To<KeyframeEffect>(content_.Get())
        ->PauseAnimationForTestingOnCompositor(
            base::TimeDelta::FromSecondsD(current_time.value()));
  }

  // Do not wait for animation ready to lock in the hold time. Otherwise,
  // the pause won't take effect until the next frame and the hold time will
  // potentially drift.
  is_paused_for_testing_ = true;
  pending_pause_ = false;
  pending_play_ = false;
  SetHoldTimeAndPhase(pause_time, TimelinePhase::kActive);
  start_time_ = base::nullopt;
}

void Animation::SetEffectSuppressed(bool suppressed) {
  effect_suppressed_ = suppressed;
  if (suppressed)
    CancelAnimationOnCompositor();
}

void Animation::DisableCompositedAnimationForTesting() {
  is_composited_animation_disabled_for_testing_ = true;
  CancelAnimationOnCompositor();
}

void Animation::InvalidateKeyframeEffect(const TreeScope& tree_scope) {
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
    target->SetNeedsStyleRecalc(kLocalStyleChange,
                                StyleChangeReasonForTracing::Create(
                                    style_change_reason::kStyleSheetChange));
  }
}

void Animation::ResolvePromiseMaybeAsync(AnimationPromise* promise) {
  if (ScriptForbiddenScope::IsScriptForbidden()) {
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kDOMManipulation)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&AnimationPromise::Resolve<Animation*>,
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
        ->PostTask(FROM_HERE,
                   WTF::Bind(&Animation::RejectAndResetPromise,
                             WrapPersistent(this), WrapPersistent(promise)));
  } else {
    RejectAndResetPromise(promise);
  }
}

void Animation::NotifyProbe() {
  AnimationPlayState old_play_state = reported_play_state_;
  AnimationPlayState new_play_state =
      PendingInternal() ? kPending : CalculateAnimationPlayState();

  if (old_play_state != new_play_state) {
    if (!PendingInternal()) {
      probe::AnimationPlayStateChanged(document_, this, old_play_state,
                                       new_play_state);
    }
    reported_play_state_ = new_play_state;

    bool was_active = old_play_state == kPending || old_play_state == kRunning;
    bool is_active = new_play_state == kPending || new_play_state == kRunning;

    if (!was_active && is_active) {
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
          "blink.animations,devtools.timeline,benchmark,rail", "Animation",
          this, "data", inspector_animation_event::Data(*this));
    } else if (was_active && !is_active) {
      TRACE_EVENT_NESTABLE_ASYNC_END1(
          "blink.animations,devtools.timeline,benchmark,rail", "Animation",
          this, "endData", inspector_animation_state_event::Data(*this));
    } else {
      TRACE_EVENT_NESTABLE_ASYNC_INSTANT1(
          "blink.animations,devtools.timeline,benchmark,rail", "Animation",
          this, "data", inspector_animation_state_event::Data(*this));
    }
  }
}

// -------------------------------------
// Replacement of animations
// -------------------------------------

// https://drafts.csswg.org/web-animations-1/#removing-replaced-animations
bool Animation::IsReplaceable() {
  // An animation is replaceable if all of the following conditions are true:

  // 1. The existence of the animation is not prescribed by markup. That is, it
  //    is not a CSS animation with an owning element, nor a CSS transition with
  //    an owning element.
  if (IsCSSAnimation() || IsCSSTransition()) {
    // TODO(crbug.com/981905): Add OwningElement method to Animation and
    // override in CssAnimations and CssTransitions. Only bail here if the
    // animation has an owning element.
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
  Element* target = To<KeyframeEffect>(content_.Get())->target();
  if (!target)
    return false;

  return true;
}

// https://drafts.csswg.org/web-animations-1/#removing-replaced-animations
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
    base::Optional<double> event_current_time = CurrentTimeInternal();
    if (event_current_time)
      event_current_time = SecondsToMilliseconds(event_current_time.value());
    pending_remove_event_ = MakeGarbageCollected<AnimationPlaybackEvent>(
        event_type, event_current_time, TimelineTime());
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
      NOTREACHED();
      return "";
  }
}

// https://drafts.csswg.org/web-animations-1/#dom-animation-commitstyles
void Animation::commitStyles(ExceptionState& exception_state) {
  Element* target = content_ && content_->IsKeyframeEffect()
                        ? To<KeyframeEffect>(effect())->target()
                        : nullptr;

  // 1. If target is not an element capable of having a style attribute
  //    (for example, it is a pseudo-element or is an element in a document
  //    format for which style attributes are not defined) throw a
  //    "NoModificationAllowedError" DOMException and abort these steps.
  if (!target || !target->IsStyledElement() ||
      !To<KeyframeEffect>(effect())->pseudoElement().IsEmpty()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNoModificationAllowedError,
        "Animation not associated with a styled target element");
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

  AnimationUtils::ForEachInterpolatedPropertyValue(
      target, animation_properties, interpolations_map,
      WTF::BindRepeating(
          [](CSSStyleDeclaration* inline_style, Element* target,
             PropertyHandle property, const CSSValue* value) {
            inline_style->setProperty(
                target->GetExecutionContext(),
                property.GetCSSPropertyName().ToAtomicString(),
                value->CssText(), "", ASSERT_NO_EXCEPTION);
          },
          WrapWeakPersistent(inline_style), WrapWeakPersistent(target)));
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
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

Animation::CompositorAnimationHolder*
Animation::CompositorAnimationHolder::Create(Animation* animation) {
  return MakeGarbageCollected<CompositorAnimationHolder>(animation);
}

Animation::CompositorAnimationHolder::CompositorAnimationHolder(
    Animation* animation)
    : animation_(animation) {
  compositor_animation_ = CompositorAnimation::Create();
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
