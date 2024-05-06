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

#include "third_party/blink/renderer/core/animation/document_timeline.h"

#include "cc/animation/animation_id_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_timeline_options.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

// Returns the current animation time for a given |document|. This is
// the animation clock time capped to be at least this document's
// CalculateZeroTime() such that the animation time is never negative when
// converted.
base::TimeTicks CurrentAnimationTime(Document* document) {
  base::TimeTicks animation_time = document->GetAnimationClock().CurrentTime();
  base::TimeTicks document_zero_time = document->Timeline().CalculateZeroTime();

  // The AnimationClock time may be null or less than the local document's
  // zero time if we have not generated any frames for this document yet. If
  // so, assume animation_time is the document zero time.
  if (animation_time < document_zero_time)
    return document_zero_time;

  return animation_time;
}

}  // namespace

// This value represents 1 frame at 30Hz plus a little bit of wiggle room.
// TODO: Plumb a nominal framerate through and derive this value from that.
const double DocumentTimeline::kMinimumDelay = 0.04;

DocumentTimeline* DocumentTimeline::Create(
    ExecutionContext* execution_context,
    const DocumentTimelineOptions* options) {
  Document* document = To<LocalDOMWindow>(execution_context)->document();
  return MakeGarbageCollected<DocumentTimeline>(
      document, base::Milliseconds(options->originTime()), nullptr);
}

DocumentTimeline::DocumentTimeline(Document* document,
                                   base::TimeDelta origin_time,
                                   PlatformTiming* timing)
    : AnimationTimeline(document),
      origin_time_(origin_time),
      zero_time_(base::TimeTicks() + origin_time_),
      playback_rate_(1),
      zero_time_initialized_(false) {
  if (!timing)
    timing_ = MakeGarbageCollected<DocumentTimelineTiming>(this);
  else
    timing_ = timing;
  if (Platform::Current()->IsThreadedAnimationEnabled())
    EnsureCompositorTimeline();

  DCHECK(document);
}

bool DocumentTimeline::IsActive() const {
  return document_->GetPage();
}

// Document-linked animations are initialized with start time of the document
// timeline current time.
std::optional<base::TimeDelta>
DocumentTimeline::InitialStartTimeForAnimations() {
  std::optional<double> current_time_ms = CurrentTimeMilliseconds();
  if (current_time_ms.has_value()) {
    return base::Milliseconds(current_time_ms.value());
  }
  return std::nullopt;
}

void DocumentTimeline::ScheduleNextService() {
  DCHECK_EQ(outdated_animation_count_, 0U);

  std::optional<AnimationTimeDelta> time_to_next_effect;
  for (const auto& animation : animations_needing_update_) {
    std::optional<AnimationTimeDelta> time_to_effect_change =
        animation->TimeToEffectChange();
    if (!time_to_effect_change)
      continue;

    time_to_next_effect = time_to_next_effect
                              ? std::min(time_to_next_effect.value(),
                                         time_to_effect_change.value())
                              : time_to_effect_change.value();
  }

  if (!time_to_next_effect)
    return;
  double next_effect_delay = time_to_next_effect.value().InSecondsF();
  if (next_effect_delay < kMinimumDelay) {
    ScheduleServiceOnNextFrame();
  } else {
    timing_->WakeAfter(base::Seconds(next_effect_delay - kMinimumDelay));
  }
}

void DocumentTimeline::DocumentTimelineTiming::WakeAfter(
    base::TimeDelta duration) {
  if (timer_.IsActive() && timer_.NextFireInterval() < duration)
    return;
  timer_.StartOneShot(duration, FROM_HERE);
}

void DocumentTimeline::DocumentTimelineTiming::Trace(Visitor* visitor) const {
  visitor->Trace(timeline_);
  visitor->Trace(timer_);
  DocumentTimeline::PlatformTiming::Trace(visitor);
}

base::TimeTicks DocumentTimeline::CalculateZeroTime() {
  if (!zero_time_initialized_ && document_->Loader()) {
    zero_time_ = document_->Loader()->GetTiming().ReferenceMonotonicTime() +
                 origin_time_;
    zero_time_initialized_ = true;
  }
  return zero_time_;
}

void DocumentTimeline::ResetForTesting() {
  zero_time_ = base::TimeTicks() + origin_time_;
  zero_time_initialized_ = true;
  playback_rate_ = 1;
  last_current_phase_and_time_.reset();
}

void DocumentTimeline::SetTimingForTesting(PlatformTiming* timing) {
  timing_ = timing;
}

AnimationTimeline::PhaseAndTime DocumentTimeline::CurrentPhaseAndTime() {
  if (!IsActive()) {
    return {TimelinePhase::kInactive, /*current_time*/ std::nullopt};
  }

  std::optional<base::TimeDelta> result =
      playback_rate_ == 0
          ? CalculateZeroTime().since_origin()
          : (CurrentAnimationTime(GetDocument()) - CalculateZeroTime()) *
                playback_rate_;
  return {TimelinePhase::kActive, result};
}

void DocumentTimeline::PauseAnimationsForTesting(
    AnimationTimeDelta pause_time) {
  for (const auto& animation : animations_needing_update_)
    animation->PauseForTesting(pause_time);
  ServiceAnimations(kTimingUpdateOnDemand);
}

void DocumentTimeline::SetPlaybackRate(double playback_rate) {
  if (!IsActive())
    return;
  base::TimeDelta current_time = CurrentPhaseAndTime().time.value();
  playback_rate_ = playback_rate;
  zero_time_ = playback_rate == 0 ? base::TimeTicks() + current_time
                                  : CurrentAnimationTime(GetDocument()) -
                                        current_time / playback_rate;
  zero_time_initialized_ = true;

  // Corresponding compositor animation may need to be restarted to pick up
  // the new playback rate. Marking the effect changed forces this.
  MarkAnimationsCompositorPending(true);
}

double DocumentTimeline::PlaybackRate() const {
  return playback_rate_;
}

void DocumentTimeline::InvalidateKeyframeEffects(
    const TreeScope& tree_scope,
    const StyleChangeReasonForTracing& reason) {
  for (const auto& animation : animations_)
    animation->InvalidateKeyframeEffect(tree_scope, reason);
}

cc::AnimationTimeline* DocumentTimeline::EnsureCompositorTimeline() {
  if (compositor_timeline_)
    return compositor_timeline_.get();

  compositor_timeline_ =
      cc::AnimationTimeline::Create(cc::AnimationIdProvider::NextTimelineId());
  return compositor_timeline_.get();
}

void DocumentTimeline::Trace(Visitor* visitor) const {
  visitor->Trace(timing_);
  AnimationTimeline::Trace(visitor);
}

}  // namespace blink
