// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/scheduler_loop_quarantine_task_observer.h"

namespace content {
void SchedulerLoopQuarantineTaskObserver::WillProcessTask(
    const base::PendingTask&,
    bool) {
  scan_policy_updater_.DisallowScanlessPurge();
}

void SchedulerLoopQuarantineTaskObserver::DidProcessTask(
    const base::PendingTask&) {
  scan_policy_updater_.AllowScanlessPurge();
}
}  // namespace content
