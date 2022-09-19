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

#include "third_party/blink/renderer/core/animation/pending_animations.h"

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

void PendingAnimations::Add(Animation* animation) {
  DCHECK(animation);
  DCHECK_EQ(pending_.Find(animation), kNotFound);
  pending_.push_back(animation);

  Document* document = animation->GetDocument();
  if (document->View())
    document->View()->ScheduleAnimation();

  bool visible = document->GetPage() && document->GetPage()->IsPageVisible();
  if (!visible && !timer_.IsActive()) {
    // Verify the timer is not activated in cycles.
    CHECK(!inside_timer_fired_);
    timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
  }
}

bool PendingAnimations::Update(
    const PaintArtifactCompositor* paint_artifact_compositor,
    bool start_on_compositor) {
  HeapVector<Member<Animation>> waiting_for_start_time;
  bool started_synchronized_on_compositor = false;

  HeapVector<Member<Animation>> animations;
  HeapVector<Member<Animation>> deferred;
  animations.swap(pending_);
  int compositor_group = NextCompositorGroup();

  for (auto& animation : animations) {
    bool had_compositor_animation =
        animation->HasActiveAnimationsOnCompositor();
    // Animations with a start time do not participate in compositor start-time
    // grouping.
    if (animation->PreCommit(
            animation->StartTimeInternal() ? 1 : compositor_group,
            paint_artifact_compositor, start_on_compositor)) {
      if (animation->HasActiveAnimationsOnCompositor() &&
          !had_compositor_animation && !animation->StartTimeInternal()) {
        started_synchronized_on_compositor = true;
      }

      if (!animation->timeline() || !animation->timeline()->IsActive())
        continue;

      if (animation->Playing() && !animation->StartTimeInternal()) {
        waiting_for_start_time.push_back(animation.Get());
      } else if (animation->PendingInternal()) {
        DCHECK(animation->timeline()->IsActive() &&
               animation->timeline()->CurrentTime());
        // A pending animation that is not waiting on a start time does not need
        // to be synchronized with animations that are starting up. Nonetheless,
        // it needs to notify the animation to resolve the ready promise and
        // commit the pending state.
        animation->NotifyReady(animation->timeline()->CurrentTime().value());
      }
    } else {
      deferred.push_back(animation);
    }
  }

  // If any synchronized animations were started on the compositor, all
  // remaining synchronized animations need to wait for the synchronized
  // start time. Otherwise they may start immediately.
  if (started_synchronized_on_compositor) {
    FlushWaitingNonCompositedAnimations();
    waiting_for_compositor_animation_start_.AppendVector(
        waiting_for_start_time);
  } else {
    for (auto& animation : waiting_for_start_time) {
      DCHECK(!animation->StartTimeInternal());
      DCHECK(animation->timeline()->IsActive() &&
             animation->timeline()->CurrentTime());
      animation->NotifyReady(animation->timeline()->CurrentTime().value());
    }
  }

  // FIXME: The postCommit should happen *after* the commit, not before.
  for (auto& animation : animations)
    animation->PostCommit();

  DCHECK(pending_.empty());
  DCHECK(start_on_compositor || deferred.empty());
  for (auto& animation : deferred)
    animation->SetCompositorPending();
  DCHECK_EQ(pending_.size(), deferred.size());

  if (started_synchronized_on_compositor)
    return true;

  if (waiting_for_compositor_animation_start_.empty())
    return false;

  // Check if we're still waiting for any compositor animations to start.
  for (auto& animation : waiting_for_compositor_animation_start_) {
    if (animation->HasActiveAnimationsOnCompositor())
      return true;
  }

  // If not, go ahead and start any animations that were waiting.
  NotifyCompositorAnimationStarted(
      base::TimeTicks::Now().since_origin().InSecondsF());

  DCHECK_EQ(pending_.size(), deferred.size());
  return false;
}

void PendingAnimations::NotifyCompositorAnimationStarted(
    double monotonic_animation_start_time,
    int compositor_group) {
  TRACE_EVENT0("blink", "PendingAnimations::notifyCompositorAnimationStarted");

  HeapVector<Member<Animation>> animations;
  animations.swap(waiting_for_compositor_animation_start_);

  for (auto animation : animations) {
    if (animation->StartTimeInternal() || !animation->PendingInternal() ||
        !animation->timeline() || !animation->timeline()->IsActive()) {
      // Already started or no longer relevant.
      continue;
    }
    if (compositor_group && animation->CompositorGroup() != compositor_group) {
      // Still waiting.
      waiting_for_compositor_animation_start_.push_back(animation);
      continue;
    }
    if (!animation->timeline()->IsMonotonicallyIncreasing()) {
      animation->NotifyReady(
          animation->timeline()->CurrentTime().value_or(AnimationTimeDelta()));
    } else {
      animation->NotifyReady(
          ANIMATION_TIME_DELTA_FROM_SECONDS(monotonic_animation_start_time) -
          animation->timeline()->ZeroTime());
    }
  }
}

int PendingAnimations::NextCompositorGroup() {
  do {
    // Wrap around, skipping 0, 1.
    // * 0 is reserved for automatic assignment
    // * 1 is used for animations with a specified start time
    ++compositor_group_;
  } while (compositor_group_ == 0 || compositor_group_ == 1);

  return compositor_group_;
}

void PendingAnimations::FlushWaitingNonCompositedAnimations() {
  if (waiting_for_compositor_animation_start_.empty())
    return;

  // Start any main thread animations that were scheduled to wait on
  // compositor synchronization from a previous frame. Otherwise, a
  // continuous influx of new composited animations could delay the start
  // of non-composited animations indefinitely (crbug.com/666710).
  HeapVector<Member<Animation>> animations;
  animations.swap(waiting_for_compositor_animation_start_);
  for (auto& animation : animations) {
    if (animation->HasActiveAnimationsOnCompositor()) {
      waiting_for_compositor_animation_start_.push_back(animation);
    } else if (animation->timeline() && animation->timeline()->IsActive() &&
               animation->timeline()->CurrentTime().has_value()) {
      animation->NotifyReady(animation->timeline()->CurrentTime().value());
    }
  }
}

void PendingAnimations::Trace(Visitor* visitor) const {
  visitor->Trace(pending_);
  visitor->Trace(waiting_for_compositor_animation_start_);
  visitor->Trace(timer_);
}

void PendingAnimations::TimerFired(TimerBase*) {
  base::AutoReset<bool> mark_inside(&inside_timer_fired_, true);
  Update(nullptr, false);
}

}  // namespace blink
