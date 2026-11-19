// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_UNBOUNDED_EVENT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_UNBOUNDED_EVENT_DATA_H_

#include <optional>

#include "third_party/blink/renderer/core/dom/node_rare_data_field.h"
#include "third_party/blink/renderer/core/frame/post_layout_snapshot_client.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

class HTMLElement;

// Snapshots the screen/escape constraining bounds for an active fixed-pos
// unbounded element after layout so that ancestor-frame scrolls or iframe
// movements trigger a re-layout to re-evaluate `position-try-fallbacks`.
class UnboundedConstrainingBoundsSnapshot final
    : public GarbageCollected<UnboundedConstrainingBoundsSnapshot>,
      public PostLayoutSnapshotClient {
 public:
  explicit UnboundedConstrainingBoundsSnapshot(HTMLElement& element);

  void Deactivate() { active_ = false; }

  // PostLayoutSnapshotClient:
  bool UpdateSnapshot() override;
  bool ShouldScheduleNextService() override;

  void Trace(Visitor* visitor) const override;

 private:
  bool IsActive() const;
  std::optional<gfx::Rect> ComputeConstrainingBounds() const;

  Member<HTMLElement> element_;
  std::optional<gfx::Rect> constraining_bounds_;
  bool active_ = true;
};

class UnboundedEventData final : public GarbageCollected<UnboundedEventData>,
                                 public NodeRareDataField {
 public:
  UnboundedEventData() = default;
  UnboundedEventData(const UnboundedEventData&) = delete;
  UnboundedEventData& operator=(const UnboundedEventData&) = delete;

  bool hasPendingEventTask() const { return pending_event_task_.IsActive(); }
  void cancelPendingEventTask() { pending_event_task_.Cancel(); }
  void setPendingEventTask(TaskHandle&& task) {
    DCHECK(!pending_event_task_.IsActive());
    pending_event_task_ = std::move(task);
  }

  bool pendingEventStartedClosed() const {
    return pending_event_started_closed_;
  }
  void setPendingEventStartedClosed(bool closed) {
    pending_event_started_closed_ = closed;
  }

  bool isTransitioning() const { return is_transitioning_; }
  void setIsTransitioning(bool transitioning) {
    is_transitioning_ = transitioning;
  }

  UnboundedConstrainingBoundsSnapshot* ConstrainingBoundsSnapshot() const {
    return constraining_bounds_snapshot_.Get();
  }
  void EnsureConstrainingBoundsSnapshot(HTMLElement& element) {
    if (constraining_bounds_snapshot_) {
      constraining_bounds_snapshot_->Deactivate();
    }
    constraining_bounds_snapshot_ =
        MakeGarbageCollected<UnboundedConstrainingBoundsSnapshot>(element);
  }
  void ClearConstrainingBoundsSnapshot() {
    if (constraining_bounds_snapshot_) {
      constraining_bounds_snapshot_->Deactivate();
      constraining_bounds_snapshot_ = nullptr;
    }
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(constraining_bounds_snapshot_);
    NodeRareDataField::Trace(visitor);
  }

 private:
  TaskHandle pending_event_task_;
  Member<UnboundedConstrainingBoundsSnapshot> constraining_bounds_snapshot_;
  bool pending_event_started_closed_ = false;
  bool is_transitioning_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_UNBOUNDED_EVENT_DATA_H_
