// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SMOOTH_SCROLL_SEQUENCER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SMOOTH_SCROLL_SEQUENCER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class LocalFrame;
class ScrollableArea;

struct SequencedScroll final : public GarbageCollected<SequencedScroll> {
  SequencedScroll();

  SequencedScroll(ScrollableArea* area,
                  ScrollOffset offset,
                  mojom::blink::ScrollBehavior behavior)
      : scrollable_area(area),
        scroll_offset(offset),
        scroll_behavior(behavior) {}

  SequencedScroll(const SequencedScroll& other)
      : scrollable_area(other.scrollable_area),
        scroll_offset(other.scroll_offset),
        scroll_behavior(other.scroll_behavior) {}

  Member<ScrollableArea> scrollable_area;
  ScrollOffset scroll_offset;
  mojom::blink::ScrollBehavior scroll_behavior;

  void Trace(Visitor*) const;
};

// A sequencer that queues the nested scrollers from inside to outside,
// so that they can be animated from outside to inside when smooth scroll
// is called.
class CORE_EXPORT SmoothScrollSequencer final
    : public GarbageCollected<SmoothScrollSequencer> {
 public:
  explicit SmoothScrollSequencer(LocalFrame& owner_frame);
  void SetScrollType(mojom::blink::ScrollType type) { scroll_type_ = type; }

  // Add a scroll offset animation to the back of a queue.
  void QueueAnimation(ScrollableArea*,
                      ScrollOffset,
                      mojom::blink::ScrollBehavior);

  // Run the animation at the back of the queue.
  void RunQueuedAnimations();

  // Abort the currently running animation and all the animations in the queue.
  void AbortAnimations();

  // Given the incoming scroll's scroll type, returns whether to filter the
  // incoming scroll. It may also abort the current sequenced scroll.
  bool FilterNewScrollOrAbortCurrent(mojom::blink::ScrollType incoming_type);

  // Returns the size of the scroll sequence queue.
  // TODO(bokan): Temporary, to track cross-origin scroll-into-view prevalence.
  // https://crbug.com/1339003.
  wtf_size_t GetCount() const;

  // Returns true if there are no scrolls queued.
  bool IsEmpty() const;

  void DidDisposeScrollableArea(const ScrollableArea&);

  void Trace(Visitor*) const;

 private:
  HeapVector<Member<SequencedScroll>> queue_;
  Member<ScrollableArea> current_scrollable_;
  Member<LocalFrame> owner_frame_;
  mojom::blink::ScrollType scroll_type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SMOOTH_SCROLL_SEQUENCER_H_
