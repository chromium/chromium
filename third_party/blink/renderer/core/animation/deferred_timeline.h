// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_DEFERRED_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_DEFERRED_TIMELINE_H_

#include "third_party/blink/renderer/core/animation/scroll_snapshot_timeline.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class Document;

class CORE_EXPORT DeferredTimeline : public ScrollSnapshotTimeline {
 public:
  explicit DeferredTimeline(Document*);

  // TODO(crbug.com/1425939): Support attaching other timelines to this
  // timeline.

 protected:
  TimelineState ComputeTimelineState() const override;
  cc::AnimationTimeline* EnsureCompositorTimeline() override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_DEFERRED_TIMELINE_H_
