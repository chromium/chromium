// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/animation/compositor_animation_timeline.h"

#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/scroll_timeline.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_client.h"

namespace blink {

CompositorAnimationTimeline::CompositorAnimationTimeline()
    : animation_timeline_(cc::AnimationTimeline::Create(
          cc::AnimationIdProvider::NextTimelineId())) {}

CompositorAnimationTimeline::CompositorAnimationTimeline(
    scoped_refptr<cc::AnimationTimeline> timeline)
    : animation_timeline_(timeline) {}

CompositorAnimationTimeline::~CompositorAnimationTimeline() {
  // Detach timeline from host, otherwise it stays there (leaks) until
  // compositor shutdown.
  if (animation_timeline_->animation_host())
    animation_timeline_->animation_host()->RemoveAnimationTimeline(
        animation_timeline_);
}

cc::AnimationTimeline* CompositorAnimationTimeline::GetAnimationTimeline()
    const {
  return animation_timeline_.get();
}

void CompositorAnimationTimeline::UpdateCompositorTimeline(
    base::Optional<CompositorElementId> pending_id,
    const std::vector<double> scroll_offsets) {
  ToScrollTimeline(animation_timeline_.get())
      ->UpdateScrollerIdAndScrollOffsets(pending_id, scroll_offsets);
}

void CompositorAnimationTimeline::AnimationAttached(
    const blink::CompositorAnimationClient& client) {
  if (client.GetCompositorAnimation()) {
    animation_timeline_->AttachAnimation(
        client.GetCompositorAnimation()->CcAnimation());
  }
}

void CompositorAnimationTimeline::AnimationDestroyed(
    const blink::CompositorAnimationClient& client) {
  if (client.GetCompositorAnimation()) {
    animation_timeline_->DetachAnimation(
        client.GetCompositorAnimation()->CcAnimation());
  }
}

}  // namespace blink
