// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/throttling/throttled_time_domain.h"

#include "base/task/sequence_manager/sequence_manager.h"

namespace blink {
namespace scheduler {

ThrottledTimeDomain::ThrottledTimeDomain() {}

ThrottledTimeDomain::~ThrottledTimeDomain() = default;

base::sequence_manager::LazyNow ThrottledTimeDomain::CreateLazyNow() const {
  return base::sequence_manager::LazyNow(sequence_manager()->GetTickClock());
}

base::TimeTicks ThrottledTimeDomain::Now() const {
  return sequence_manager()->NowTicks();
}

const char* ThrottledTimeDomain::GetName() const {
  return "ThrottledTimeDomain";
}

void ThrottledTimeDomain::SetNextDelayedDoWork(
    base::sequence_manager::LazyNow* lazy_now,
    base::TimeTicks run_time) {
  // We assume the owner (i.e. TaskQueueThrottler) will manage wake-ups on our
  // behalf.
}

void ThrottledTimeDomain::SetNextTaskRunTime(base::TimeTicks run_time) {
  next_task_run_time_ = run_time;
}

absl::optional<base::TimeDelta> ThrottledTimeDomain::DelayTillNextTask(
    base::sequence_manager::LazyNow* lazy_now) {
  base::TimeTicks now = lazy_now->Now();
  if (next_task_run_time_ && next_task_run_time_ > now)
    return next_task_run_time_.value() - now;

  absl::optional<base::TimeTicks> next_run_time = NextScheduledRunTime();
  if (!next_run_time)
    return absl::nullopt;

  if (now >= next_run_time)
    return base::TimeDelta();  // Makes DoWork post an immediate continuation.

  // We assume the owner (i.e. TaskQueueThrottler) will manage wake-ups on our
  // behalf.
  return absl::nullopt;
}

bool ThrottledTimeDomain::MaybeFastForwardToNextTask(
    bool quit_when_idle_requested) {
  return false;
}

}  // namespace scheduler
}  // namespace blink
