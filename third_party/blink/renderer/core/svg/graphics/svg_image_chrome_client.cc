/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/svg/graphics/svg_image_chrome_client.h"

#include <algorithm>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/platform/graphics/image_observer.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {

static constexpr base::TimeDelta kAnimationFrameDelay = base::Hertz(60);

bool IsolatedSVGChromeClient::IsIsolatedSVGChromeClient() const {
  return true;
}

SVGImageChromeClient::SVGImageChromeClient() : timeline_state_(kRunning) {}

void SVGImageChromeClient::InitAnimationTimer(
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner) {
  animation_timer_ = MakeGarbageCollected<
      DisallowNewWrapper<HeapTaskRunnerTimer<SVGImageChromeClient>>>(
      std::move(compositor_task_runner), this,
      &SVGImageChromeClient::AnimationTimerFired);
}

void SVGImageChromeClient::ChromeDestroyed() {
  image_ = nullptr;
}

void SVGImageChromeClient::InvalidateContainer() {
  // If image_->document_host_ is null, we're being destructed, so don't fire
  // |Changed()| in that case.
  if (image_ && image_->GetImageObserver() && image_->document_host_) {
    image_->GetImageObserver()->Changed(image_);
  }
}

void SVGImageChromeClient::SuspendAnimation() {
  if (image_->MaybeAnimated()) {
    timeline_state_ = kSuspendedWithAnimationPending;
  } else {
    // Preserve SuspendedWithAnimationPending if set.
    timeline_state_ = std::max(timeline_state_, kSuspended);
  }
}

void SVGImageChromeClient::ResumeAnimation() {
  bool have_pending_animation =
      timeline_state_ == kSuspendedWithAnimationPending;
  timeline_state_ = kRunning;

  // If an animation frame was pending/requested while animations were
  // suspended, schedule a new animation frame.
  if (!have_pending_animation)
    return;
  ScheduleAnimation(nullptr);
}

void SVGImageChromeClient::RestoreAnimationIfNeeded() {
  // If the timeline is not suspended we needn't attempt to restore.
  if (!IsSuspended())
    return;
  image_->RestoreAnimation();
}

void SVGImageChromeClient::ScheduleAnimation(const LocalFrameView*,
                                             base::TimeDelta fire_time) {
  DCHECK(animation_timer_);
  // If `image_` is null, nothing to do.
  if (!image_) {
    return;
  }
  // Because a single SVGImage can be shared by multiple pages, we can't key
  // our svg image layout on the page's real animation frame. Therefore, we
  // run this fake animation timer to trigger layout in SVGImages. The name,
  // "animationTimer", is to match the new requestAnimationFrame-based layout
  // approach.
  if (animation_timer_->Value().IsActive())
    return;
  // Schedule the 'animation' ASAP if the image does not contain any
  // animations, but prefer a fixed, jittery, frame-delay if there're any
  // animations. Checking for pending/active animations could be more
  // stringent.
  if (image_->MaybeAnimated()) {
    if (IsSuspended())
      return;
    if (fire_time.is_zero())
      fire_time = kAnimationFrameDelay;
  }
  animation_timer_->Value().StartOneShot(fire_time, FROM_HERE);
}

void SVGImageChromeClient::SetTimerForTesting(
    DisallowNewWrapper<HeapTaskRunnerTimer<SVGImageChromeClient>>* timer) {
  animation_timer_ = timer;
}

void SVGImageChromeClient::AnimationTimerFired(TimerBase*) {
  if (!image_)
    return;

  // The SVGImageChromeClient object's lifetime is dependent on
  // the ImageObserver (an ImageResourceContent) of its image. Should it
  // be dead and about to be lazily swept out, then GetImageObserver()
  // becomes null and we do not proceed.
  //
  // TODO(Oilpan): move (SVG)Image to the Oilpan heap, and avoid
  // this explicit lifetime check.
  if (!image_->GetImageObserver())
    return;

  image_->ServiceAnimations(base::TimeTicks::Now());
}

void SVGImageChromeClient::Trace(Visitor* visitor) const {
  visitor->Trace(animation_timer_);
  EmptyChromeClient::Trace(visitor);
}

}  // namespace blink
