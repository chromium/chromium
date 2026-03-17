// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SCHEDULER_LOOP_QUARANTINE_TASK_OBSERVER_H_
#define CONTENT_COMMON_SCHEDULER_LOOP_QUARANTINE_TASK_OBSERVER_H_

#include <optional>

#include "base/memory/safety_checks.h"
#include "base/sequence_checker.h"
#include "base/task/task_observer.h"
#include "content/common/content_export.h"
#include "partition_alloc/buildflags.h"

// `//chrome` and `//android_webview` expect `use_partition_alloc=true`,
// but some other `//content` embedders like Edge do support both.
// Hence this #ifdef.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/allocator/partition_alloc_support.h"
#endif

namespace content {
// Scheduler-Loop Quarantine is a PartitionAlloc feature to protect pointers
// on stack memory. This is a task observer to tell when stack memory is
// nearly emptied based on task scheduling.
//
// For more details on the quarantine, see
// `partition_alloc/scheduler_loop_quarantine.h`.
class CONTENT_EXPORT SchedulerLoopQuarantineTaskObserver final
    : public base::TaskObserver {
 public:
  SchedulerLoopQuarantineTaskObserver();
  SchedulerLoopQuarantineTaskObserver(
      const SchedulerLoopQuarantineTaskObserver&) = delete;
  SchedulerLoopQuarantineTaskObserver& operator=(
      const SchedulerLoopQuarantineTaskObserver&) = delete;
  ~SchedulerLoopQuarantineTaskObserver() override;

 private:
  // A task is about to start. To protect the task from Use-after-Free,
  // this forces Scheduler-Loop Quarantine to perform stack-scanning
  // when it needs to purge quarantined allocations.
  // Re-entrancy is taken care of inside `scan_policy_updater_`.
  void WillProcessTask(const base::PendingTask&, bool) final;

  // At this point, the task is finished and we can say all local variables
  // for it were destroyed. It implies there is no risk of dangling local
  // pointer, hence allowing scan-less purge (faster but less secure).
  void DidProcessTask(const base::PendingTask&) final;

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  std::optional<base::allocator::ScopedSchedulerLoopQuarantineExclusion>
      exclusion_;
#endif
  // To handle nested tasks we increment a counter and emplace only when
  // transitioning from 0 or 1.
  uint32_t active_tasks_ = 0u;
  base::SchedulerLoopQuarantineScanPolicyUpdater scan_policy_updater_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_COMMON_SCHEDULER_LOOP_QUARANTINE_TASK_OBSERVER_H_
