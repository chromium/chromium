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

#include "third_party/blink/renderer/core/animation/document_animations.h"

#include "cc/animation/animation_host.h"
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/pending_animations.h"
#include "third_party/blink/renderer/core/animation/worklet_animation_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"

namespace blink {

namespace {

void UpdateAnimationTiming(Document& document, TimingUpdateReason reason) {
  document.Timeline().ServiceAnimations(reason);
  document.GetWorkletAnimationController().UpdateAnimationTimings(reason);
}

}  // namespace

void DocumentAnimations::UpdateAnimationTimingForAnimationFrame(
    Document& document) {
  UpdateAnimationTiming(document, kTimingUpdateForAnimationFrame);
}

bool DocumentAnimations::NeedsAnimationTimingUpdate(const Document& document) {
  return document.Timeline().HasOutdatedAnimation() ||
         document.Timeline().NeedsAnimationTimingUpdate();
}

void DocumentAnimations::UpdateAnimationTimingIfNeeded(Document& document) {
  if (NeedsAnimationTimingUpdate(document))
    UpdateAnimationTiming(document, kTimingUpdateOnDemand);
}

void DocumentAnimations::UpdateAnimations(
    Document& document,
    DocumentLifecycle::LifecycleState required_lifecycle_state,
    const PaintArtifactCompositor* paint_artifact_compositor) {
  DCHECK(document.Lifecycle().GetState() >= required_lifecycle_state);

  if (document.GetPendingAnimations().Update(paint_artifact_compositor)) {
    DCHECK(document.View());
    document.View()->ScheduleAnimation();
  }
  if (document.View()) {
    if (cc::AnimationHost* host =
            document.View()->GetCompositorAnimationHost()) {
      wtf_size_t total_animations_count = 0;
      if (document.Timeline().HasAnimations()) {
        total_animations_count = document.Timeline().PendingAnimationsCount();
      }
      // In the CompositorTimingHistory::DidDraw where we know that there is
      // visual update, we will use document.CurrentFrameHadRAF as a signal to
      // record UMA or not.
      host->SetAnimationCounts(total_animations_count,
                               document.CurrentFrameHadRAF(),
                               document.NextFrameHasPendingRAF());
    }
  }

  document.GetWorkletAnimationController().UpdateAnimationStates();

  document.Timeline().ScheduleNextService();
}

}  // namespace blink
