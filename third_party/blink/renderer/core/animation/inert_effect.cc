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
                         bool paused,
                         base::Optional<AnimationTimeDelta> inherited_time,
                         base::Optional<TimelinePhase> inherited_phase)
    : AnimationEffect(timing),
      model_(model),
      paused_(paused),
      inherited_time_(inherited_time),
      inherited_phase_(inherited_phase) {}

void InertEffect::Sample(HeapVector<Member<Interpolation>>& result) const {
  UpdateInheritedTime(inherited_time_, inherited_phase_, kTimingUpdateOnDemand);
  if (!IsInEffect()) {
    result.clear();
    return;
  }

  base::Optional<double> iteration = CurrentIteration();
  DCHECK(iteration);
  DCHECK_GE(iteration.value(), 0);
  model_->Sample(clampTo<int>(iteration.value(), 0), Progress().value(),
                 SpecifiedTiming().IterationDuration(), result);
}

AnimationTimeDelta InertEffect::CalculateTimeToEffectChange(
    bool,
    base::Optional<double>,
    AnimationTimeDelta) const {
  return AnimationTimeDelta::Max();
}

void InertEffect::Trace(Visitor* visitor) const {
  visitor->Trace(model_);
  AnimationEffect::Trace(visitor);
}

}  // namespace blink
