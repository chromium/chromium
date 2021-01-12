// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_IDLE_TIME_ESTIMATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_IDLE_TIME_ESTIMATOR_H_

#include "base/macros.h"
#include "base/task/task_observer.h"
#include "base/time/tick_clock.h"
#include "cc/base/rolling_time_delta_history.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

namespace blink {
namespace scheduler {

// Estimates how much idle time there is available.  Ignores nested tasks.
class PLATFORM_EXPORT IdleTimeEstimator : public base::TaskObserver {
 public:
  IdleTimeEstimator(
      const scoped_refptr<MainThreadTaskQueue>& compositor_task_queue,
      const base::TickClock* time_source,
      int sample_count,
      double estimation_percentile);

  ~IdleTimeEstimator() override;

  // Expected Idle time is defined as: |compositor_frame_interval| minus
  // expected compositor task duration.
  base::TimeDelta GetExpectedIdleDuration(
      base::TimeDelta compositor_frame_interval) const;

  void DidCommitFrameToCompositor();

  void Clear();

  // TaskObserver implementation:
  void WillProcessTask(const base::PendingTask& pending_task,
                       bool was_blocked_or_low_priority) override;
  void DidProcessTask(const base::PendingTask& pending_task) override;

 private:
  scoped_refptr<MainThreadTaskQueue> compositor_task_queue_;
  cc::RollingTimeDeltaHistory per_frame_compositor_task_runtime_;
  const base::TickClock* time_source_;  // NOT OWNED
  double estimation_percentile_;

  base::TimeTicks task_start_time_;
  base::TimeTicks prev_commit_time_;
  base::TimeDelta cumulative_compositor_runtime_;
  int nesting_level_;
  bool did_commit_;

  DISALLOW_COPY_AND_ASSIGN(IdleTimeEstimator);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_IDLE_TIME_ESTIMATOR_H_
