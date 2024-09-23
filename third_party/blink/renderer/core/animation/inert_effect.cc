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

#include "third_party/blink/renderer/core/animation/inert_effect.h"

#include "third_party/blink/renderer/core/animation/interpolation.h"

namespace blink {

InertEffect::InertEffect(KeyframeEffectModelBase* model,
                         const Timing& timing,
                         const AnimationProxy& proxy)
    : AnimationEffect(timing),
      model_(model),
      paused_(proxy.Paused()),
      inherited_time_(proxy.InheritedTime()),
      timeline_duration_(proxy.TimelineDuration()),
      intrinsic_iteration_duration_(proxy.IntrinsicIterationDuration()),
      playback_rate_(proxy.PlaybackRate()),
      at_scroll_timeline_boundary_(proxy.AtScrollTimelineBoundary()) {}

void InertEffect::Sample(HeapVector<Member<Interpolation>>& result) const {
  UpdateInheritedTime(inherited_time_, /* is_idle */ false, playback_rate_,
                      kTimingUpdateOnDemand);
  if (!IsInEffect()) {
    result.clear();
    return;
  }

  std::optional<double> iteration = CurrentIteration();
  DCHECK(iteration);
  DCHECK_GE(iteration.value(), 0);

  TimingFunction::LimitDirection limit_direction =
      (GetPhase() == Timing::kPhaseBefore)
          ? TimingFunction::LimitDirection::LEFT
          : TimingFunction::LimitDirection::RIGHT;

  model_->Sample(ClampTo<int>(iteration.value(), 0), Progress().value(),
                 limit_direction, NormalizedTiming().iteration_duration,
                 result);
}

bool InertEffect::Affects(const PropertyHandle& property) const {
  return model_->Affects(property);
}

AnimationTimeDelta InertEffect::CalculateTimeToEffectChange(
    bool,
    std::optional<AnimationTimeDelta>,
    AnimationTimeDelta) const {
  return AnimationTimeDelta::Max();
}

std::optional<AnimationTimeDelta> InertEffect::TimelineDuration() const {
  return timeline_duration_;
}

AnimationTimeDelta InertEffect::IntrinsicIterationDuration() const {
  return intrinsic_iteration_duration_;
}

void InertEffect::Trace(Visitor* visitor) const {
  visitor->Trace(model_);
  AnimationEffect::Trace(visitor);
}

}  // namespace blink
