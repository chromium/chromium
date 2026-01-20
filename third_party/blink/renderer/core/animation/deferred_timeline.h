// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_DEFERRED_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_DEFERRED_TIMELINE_H_

#include "third_party/blink/renderer/core/animation/scroll_snapshot_timeline.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

class Document;
class ScrollTimeline;

// A DeferredTimeline is a ScrollSnapshotTimeline that updates its state
// (snapshot) from some ScrollTimeline, possibly attached at a later time.
//
// Instances of DeferredTimeline are created by the 'timeline-scope' property
// on some element, and their "attachments" (i.e. the timelines they fetch
// their state from) are later attached by the animation update of a descendant
// element (see CSSAnimations::CalculateTimelineAttachmentUpdate()).
//
// https://drafts.csswg.org/scroll-animations-1/#timeline-scope
class CORE_EXPORT DeferredTimeline : public ScrollSnapshotTimeline {
 public:
  explicit DeferredTimeline(Document*);

  AnimationTimeline* ExposedTimeline() override;

  void AttachTimeline(ScrollTimeline*);
  void DetachTimeline(ScrollTimeline*);

  const HeapVector<Member<ScrollTimeline>>& AttachedTimelinesForTest() const {
    return attached_timelines_;
  }

  void Trace(Visitor*) const override;

 protected:
  ScrollAxis GetAxis() const override;
  TimelineState ComputeTimelineState() const override;

 private:
  ScrollTimeline* EffectiveScrollTimeline();

  const ScrollTimeline* EffectiveScrollTimeline() const {
    return const_cast<DeferredTimeline*>(this)->EffectiveScrollTimeline();
  }

  void OnAttachedTimelineChange();

  // Note that while multiple timelines can be attached, this DeferredTimeline
  // is always inactive when there isn't exactly one attached timeline.
  //
  // With the CSSTimelineScopeGlobal runtime flag enabled, we instead
  // use the last attachment timeline in flat tree order.
  HeapVector<Member<ScrollTimeline>> attached_timelines_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_DEFERRED_TIMELINE_H_
