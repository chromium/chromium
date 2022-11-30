// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/main_thread_event_queue_task_list.h"

#include <utility>

#include "base/containers/adapters.h"

namespace blink {

MainThreadEventQueueTaskList::MainThreadEventQueueTaskList() {}

MainThreadEventQueueTaskList::~MainThreadEventQueueTaskList() {}

MainThreadEventQueueTaskList::EnqueueResult
MainThreadEventQueueTaskList::Enqueue(
    std::unique_ptr<MainThreadEventQueueTask> event) {
  for (const auto& last_event : base::Reversed(queue_)) {
    switch (last_event->FilterNewEvent(event.get())) {
      case MainThreadEventQueueTask::FilterResult::CoalescedEvent:
        return EnqueueResult::kCoalesced;
      case MainThreadEventQueueTask::FilterResult::StopIterating:
        break;
      case MainThreadEventQueueTask::FilterResult::KeepIterating:
        continue;
    }
    break;
  }
  queue_.emplace_back(std::move(event));
  return EnqueueResult::kEnqueued;
}

std::unique_ptr<MainThreadEventQueueTask> MainThreadEventQueueTaskList::Pop() {
  std::unique_ptr<MainThreadEventQueueTask> result;
  if (!queue_.empty()) {
    result = std::move(queue_.front());
    queue_.pop_front();
  }
  return result;
}

std::unique_ptr<MainThreadEventQueueTask> MainThreadEventQueueTaskList::remove(
    size_t pos) {
  std::unique_ptr<MainThreadEventQueueTask> result;
  if (!queue_.empty()) {
    result = std::move(queue_.at(pos));
    queue_.erase(queue_.begin() + pos);
  }
  return result;
}

}  // namespace blink
