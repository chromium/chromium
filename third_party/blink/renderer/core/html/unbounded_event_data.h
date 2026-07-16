// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_UNBOUNDED_EVENT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_UNBOUNDED_EVENT_DATA_H_

#include "third_party/blink/renderer/core/dom/node_rare_data_field.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"

namespace blink {

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

  void Trace(Visitor* visitor) const override {
    NodeRareDataField::Trace(visitor);
  }

 private:
  TaskHandle pending_event_task_;
  bool pending_event_started_closed_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_UNBOUNDED_EVENT_DATA_H_
