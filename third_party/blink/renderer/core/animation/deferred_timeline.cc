// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/deferred_timeline.h"

#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

DeferredTimeline::DeferredTimeline(Document* document)
    : ScrollSnapshotTimeline(document) {}

AnimationTimeline* DeferredTimeline::ExposedTimeline() {
  return EffectiveScrollTimeline();
}

void DeferredTimeline::AttachTimeline(ScrollTimeline* timeline) {
  ScrollTimeline* original_timeline = EffectiveScrollTimeline();

  wtf_size_t insertion_point = attached_timelines_.size();

  if (RuntimeEnabledFeatures::CSSTimelineScopeGlobalEnabled()) {
    Element* reference_element = timeline->GetReferenceElement();
    // Only named ScrollTimelines and ViewTimelines produced by CSS
    // should be attached, and such timelines always have a reference element.
    CHECK(reference_element);

    while (insertion_point > 0) {
      // We can insert at `insertion_point` only if `reference_element`
      // comes after the preceding element in flat tree order.
      Element* preceding =
          attached_timelines_[insertion_point - 1]->GetReferenceElement();
      if (LayoutTreeBuilderTraversal::ComparePreorderTreePosition(
              *preceding, *reference_element) > 0) {
        --insertion_point;
        continue;
      }
      break;
    }
  }

  attached_timelines_.insert(insertion_point, timeline);

  if (original_timeline != EffectiveScrollTimeline()) {
    OnAttachedTimelineChange();
  }
}

void DeferredTimeline::DetachTimeline(ScrollTimeline* timeline) {
  ScrollTimeline* original_timeline = EffectiveScrollTimeline();

  wtf_size_t i = attached_timelines_.Find(timeline);
  if (i != kNotFound) {
    attached_timelines_.EraseAt(i);
  }

  if (original_timeline != EffectiveScrollTimeline()) {
    OnAttachedTimelineChange();
  }
}

void DeferredTimeline::Trace(Visitor* visitor) const {
  visitor->Trace(attached_timelines_);
  ScrollSnapshotTimeline::Trace(visitor);
}

DeferredTimeline::ScrollAxis DeferredTimeline::GetAxis() const {
  if (const ScrollTimeline* attached_timeline = EffectiveScrollTimeline()) {
    return attached_timeline->GetAxis();
  }
  return ScrollAxis::kBlock;
}

DeferredTimeline::TimelineState DeferredTimeline::ComputeTimelineState() const {
  if (const ScrollTimeline* attached_timeline = EffectiveScrollTimeline()) {
    return attached_timeline->ComputeTimelineState();
  }
  return TimelineState();
}

ScrollTimeline* DeferredTimeline::EffectiveScrollTimeline() {
  if (!RuntimeEnabledFeatures::CSSTimelineScopeGlobalEnabled() &&
      attached_timelines_.size() != 1u) {
    return nullptr;
  }
  // The item at back() is the last attached timeline in flat tree order.
  return attached_timelines_.empty() ? nullptr : attached_timelines_.back();
}

void DeferredTimeline::OnAttachedTimelineChange() {
  compositor_timeline_ = nullptr;
  MarkAnimationsCompositorPending(/* source_changed */ true);
}

}  // namespace blink
