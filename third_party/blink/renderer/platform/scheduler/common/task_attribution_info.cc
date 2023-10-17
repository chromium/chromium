// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink::scheduler {

void TaskAttributionInfo::Dispose() {
  auto* tracker = ThreadScheduler::Current()->GetTaskAttributionTracker();
  if (!tracker) {
    return;
  }
  if (auto* observer = tracker->GetObserverForTaskDisposal(task_id_)) {
    observer->OnTaskDisposal(*this);
  }
}

}  // namespace blink::scheduler
