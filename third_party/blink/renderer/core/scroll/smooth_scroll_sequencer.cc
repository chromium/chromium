// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/scroll/programmatic_scroll_animator.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"

namespace blink {

void SequencedScroll::Trace(Visitor* visitor) const {
  visitor->Trace(scrollable_area);
}

void SmoothScrollSequencer::QueueAnimation(
    ScrollableArea* scrollable,
    ScrollOffset offset,
    mojom::blink::ScrollBehavior behavior) {
  if (scrollable->ClampScrollOffset(offset) != scrollable->GetScrollOffset()) {
    queue_.push_back(
        MakeGarbageCollected<SequencedScroll>(scrollable, offset, behavior));
  }
}

SmoothScrollSequencer::SmoothScrollSequencer(LocalFrame& owner_frame)
    : owner_frame_(&owner_frame),
      scroll_type_(mojom::blink::ScrollType::kProgrammatic) {
  CHECK(owner_frame_->IsLocalRoot());
}

void SmoothScrollSequencer::RunQueuedAnimations() {
  if (queue_.empty()) {
    CHECK_EQ(owner_frame_->GetSmoothScrollSequencer(), this);
    owner_frame_->FinishedScrollSequence();
    return;
  }
  SequencedScroll* sequenced_scroll = queue_.back();
  queue_.pop_back();
  current_scrollable_ = sequenced_scroll->scrollable_area;
  current_scrollable_->SetScrollOffset(sequenced_scroll->scroll_offset,
                                       mojom::blink::ScrollType::kSequenced,
                                       sequenced_scroll->scroll_behavior);
}

void SmoothScrollSequencer::AbortAnimations() {
  if (current_scrollable_) {
    current_scrollable_->CancelProgrammaticScrollAnimation();
    current_scrollable_ = nullptr;
  }
  queue_.clear();

  // The sequence may be aborted after being replaced by a new sequence.
  if (owner_frame_->GetSmoothScrollSequencer() == this) {
    owner_frame_->FinishedScrollSequence();
  }
}

bool SmoothScrollSequencer::FilterNewScrollOrAbortCurrent(
    mojom::blink::ScrollType incoming_type) {
  // Allow the incoming scroll to co-exist if its scroll type is
  // kSequenced, kClamping, or kAnchoring
  if (incoming_type == mojom::blink::ScrollType::kSequenced ||
      incoming_type == mojom::blink::ScrollType::kClamping ||
      incoming_type == mojom::blink::ScrollType::kAnchoring)
    return false;

  // If the current sequenced scroll is UserScroll, but the incoming scroll is
  // not, filter the incoming scroll. See crbug.com/913009 for more details.
  if (scroll_type_ == mojom::blink::ScrollType::kUser &&
      incoming_type != mojom::blink::ScrollType::kUser)
    return true;

  // Otherwise, abort the current sequenced scroll.
  AbortAnimations();
  return false;
}

wtf_size_t SmoothScrollSequencer::GetCount() const {
  return queue_.size();
}

bool SmoothScrollSequencer::IsEmpty() const {
  return queue_.empty();
}

void SmoothScrollSequencer::DidDisposeScrollableArea(
    const ScrollableArea& area) {
  for (Member<SequencedScroll>& sequenced_scroll : queue_) {
    if (sequenced_scroll->scrollable_area.Get() == &area) {
      AbortAnimations();
      break;
    }
  }
}

void SmoothScrollSequencer::Trace(Visitor* visitor) const {
  visitor->Trace(queue_);
  visitor->Trace(current_scrollable_);
  visitor->Trace(owner_frame_);
}

}  // namespace blink
