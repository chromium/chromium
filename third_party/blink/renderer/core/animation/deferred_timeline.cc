// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/deferred_timeline.h"

namespace blink {

DeferredTimeline::DeferredTimeline(Document* document)
    : ScrollSnapshotTimeline(document) {}

void DeferredTimeline::AttachTimeline(ScrollSnapshotTimeline* timeline) {
  attached_timelines_.push_back(timeline);
}

void DeferredTimeline::DetachTimeline(ScrollSnapshotTimeline* timeline) {
  wtf_size_t i = attached_timelines_.Find(timeline);
  if (i != kNotFound) {
    attached_timelines_.EraseAt(i);
  }
}

void DeferredTimeline::Trace(Visitor* visitor) const {
  visitor->Trace(attached_timelines_);
  ScrollSnapshotTimeline::Trace(visitor);
}

DeferredTimeline::TimelineState DeferredTimeline::ComputeTimelineState() const {
  if (const ScrollSnapshotTimeline* attached_timeline =
          SingleAttachedTimeline()) {
    return attached_timeline->ComputeTimelineState();
  }
  return TimelineState();
}

cc::AnimationTimeline* DeferredTimeline::EnsureCompositorTimeline() {
  // TODO(crbug.com/1425939): Return the compositor timeline of the attached
  // timeline.
  return nullptr;
}

}  // namespace blink
