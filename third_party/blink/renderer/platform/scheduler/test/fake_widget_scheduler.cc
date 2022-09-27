// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/test/fake_widget_scheduler.h"

namespace blink {
namespace scheduler {

FakeWidgetScheduler::~FakeWidgetScheduler() = default;

void FakeWidgetScheduler::Shutdown() {
  // Delete the pending tasks because it may cause a leak.
  // TODO(altimin): This will not prevent all leaks if someone holds a reference
  // to the |input_task_runner_| and continues to post tasks after this class is
  // destroyed.
  input_task_runner_->TakePendingTasksForTesting();
}

}  // namespace scheduler
}  // namespace blink
