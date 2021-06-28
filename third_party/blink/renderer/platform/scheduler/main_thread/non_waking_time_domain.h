// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_NON_WAKING_TIME_DOMAIN_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_NON_WAKING_TIME_DOMAIN_H_

#include "base/task/sequence_manager/time_domain.h"
#include "base/time/tick_clock.h"

namespace blink {
namespace scheduler {

// A time domain which never generates wake-ups on its own. Useful for tasks
// which should run only when the system is non-idle.
class NonWakingTimeDomain : public base::sequence_manager::TimeDomain {
 public:
  explicit NonWakingTimeDomain(const base::TickClock* tick_clock);
  ~NonWakingTimeDomain() override;

  // TimeDomain:
  base::sequence_manager::LazyNow CreateLazyNow() const override;
  base::TimeTicks Now() const override;
  absl::optional<base::TimeDelta> DelayTillNextTask(
      base::sequence_manager::LazyNow* lazy_now) override;
  bool MaybeFastForwardToNextTask(bool quit_when_idle_requested) override;
  const char* GetName() const override;
  void SetNextDelayedDoWork(base::sequence_manager::LazyNow* lazy_now,
                            base::TimeTicks run_time) override;

 private:
  const base::TickClock* tick_clock_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_NON_WAKING_TIME_DOMAIN_H_
