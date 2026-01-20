// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/deferred_timeline.h"

#include "third_party/blink/renderer/core/animation/scroll_timeline.h"

namespace blink {

DeferredTimeline::DeferredTimeline(Document* document)
    : ScrollSnapshotTimeline(document) {}

AnimationTimeline* DeferredTimeline::ExposedTimeline() {
  return SingleAttachedTimeline();
}

void DeferredTimeline::AttachTimeline(ScrollTimeline* timeline) {
  ScrollTimeline* original_timeline = SingleAttachedTimeline();

  attached_timelines_.push_back(timeline);

  if (original_timeline != SingleAttachedTimeline()) {
    OnAttachedTimelineChange();
  }
}

void DeferredTimeline::DetachTimeline(ScrollTimeline* timeline) {
  ScrollTimeline* original_timeline = SingleAttachedTimeline();

  wtf_size_t i = attached_timelines_.Find(timeline);
  if (i != kNotFound) {
    attached_timelines_.EraseAt(i);
  }

  if (original_timeline != SingleAttachedTimeline()) {
    OnAttachedTimelineChange();
  }
}

void DeferredTimeline::Trace(Visitor* visitor) const {
  visitor->Trace(attached_timelines_);
  ScrollSnapshotTimeline::Trace(visitor);
}

DeferredTimeline::ScrollAxis DeferredTimeline::GetAxis() const {
  if (const ScrollTimeline* attached_timeline = SingleAttachedTimeline()) {
    return attached_timeline->GetAxis();
  }
  return ScrollAxis::kBlock;
}

DeferredTimeline::TimelineState DeferredTimeline::ComputeTimelineState() const {
  if (const ScrollTimeline* attached_timeline = SingleAttachedTimeline()) {
    return attached_timeline->ComputeTimelineState();
  }
  return TimelineState();
}

void DeferredTimeline::OnAttachedTimelineChange() {
  compositor_timeline_ = nullptr;
  MarkAnimationsCompositorPending(/* source_changed */ true);
}

}  // namespace blink
