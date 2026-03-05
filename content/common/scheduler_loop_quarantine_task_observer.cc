// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/scheduler_loop_quarantine_task_observer.h"

#include <limits>

#include "base/check_op.h"

namespace content {
SchedulerLoopQuarantineTaskObserver::SchedulerLoopQuarantineTaskObserver() {
  // DETACH to allow tests to construct this off the main thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

// Don't check sequence_checker_ due to the same reason we detached in the
// constructor.
SchedulerLoopQuarantineTaskObserver::~SchedulerLoopQuarantineTaskObserver() =
    default;

void SchedulerLoopQuarantineTaskObserver::WillProcessTask(
    const base::PendingTask&,
    bool) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LT(active_tasks_, std::numeric_limits<uint32_t>::max());
  ++active_tasks_;
  if (active_tasks_ == 1u) {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    // We want the entire task (and all nested tasks) to be under the protection
    // of the quarantine and zapping.
    exclusion_.reset();
#endif
    scan_policy_updater_.DisallowScanlessPurge();
  }
}

void SchedulerLoopQuarantineTaskObserver::DidProcessTask(
    const base::PendingTask&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(active_tasks_, 0u);
  --active_tasks_;
  if (active_tasks_ == 0u) {
    scan_policy_updater_.AllowScanlessPurge();
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    // In-between tasks, we want to disable the quarantine and zapping entirely.
    // This avoids impacting things like native OS events or message pump
    // overhead.
    exclusion_.emplace();
#endif
  }
}
}  // namespace content
