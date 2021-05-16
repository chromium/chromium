// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THROTTLING_THROTTLED_TIME_DOMAIN_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THROTTLING_THROTTLED_TIME_DOMAIN_H_

#include "base/macros.h"
#include "base/task/sequence_manager/time_domain.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {
namespace scheduler {

// A time domain for throttled tasks. Behaves like an RealTimeDomain except it
// relies on the owner (TaskQueueThrottler) to schedule wake-ups.
class PLATFORM_EXPORT ThrottledTimeDomain
    : public base::sequence_manager::TimeDomain {
 public:
  ThrottledTimeDomain();
  ~ThrottledTimeDomain() override;

  void SetNextTaskRunTime(base::TimeTicks run_time);

  // TimeDomain implementation:
  base::sequence_manager::LazyNow CreateLazyNow() const override;
  base::TimeTicks Now() const override;
  absl::optional<base::TimeDelta> DelayTillNextTask(
      base::sequence_manager::LazyNow* lazy_now) override;
  bool MaybeFastForwardToNextTask(bool quit_when_idle_requested) override;

 protected:
  const char* GetName() const override;
  void SetNextDelayedDoWork(base::sequence_manager::LazyNow* lazy_now,
                            base::TimeTicks run_time) override;

 private:
  // Next task run time provided by task queue throttler. Note that it does not
  // get reset, so it is valid only when in the future.
  absl::optional<base::TimeTicks> next_task_run_time_;

  DISALLOW_COPY_AND_ASSIGN(ThrottledTimeDomain);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THROTTLING_THROTTLED_TIME_DOMAIN_H_
