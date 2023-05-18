// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/deferred_timeline.h"

namespace blink {

DeferredTimeline::DeferredTimeline(Document* document)
    : ScrollSnapshotTimeline(document) {}

DeferredTimeline::TimelineState DeferredTimeline::ComputeTimelineState() const {
  // TODO(crbug.com/1425939): Grab state from the attached timeline.
  // For now this timeline is always inactive.
  return TimelineState();
}

cc::AnimationTimeline* DeferredTimeline::EnsureCompositorTimeline() {
  // TODO(crbug.com/1425939): Return the compositor timeline of the attached
  // timeline.
  return nullptr;
}

}  // namespace blink
