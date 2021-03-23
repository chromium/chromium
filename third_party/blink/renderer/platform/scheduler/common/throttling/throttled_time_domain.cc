// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/throttling/throttled_time_domain.h"

#include "base/task/sequence_manager/sequence_manager.h"

extern "C" void V8RecordReplayAssert(const char* format, ...);

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

base::Optional<base::TimeDelta> ThrottledTimeDomain::DelayTillNextTask(
    base::sequence_manager::LazyNow* lazy_now) {
  V8RecordReplayAssert("ThrottledTimeDomain::DelayTillNextTask Start");

  base::TimeTicks now = lazy_now->Now();
  if (next_task_run_time_ && next_task_run_time_ > now) {
    V8RecordReplayAssert("ThrottledTimeDomain::DelayTillNextTask #1");
    return next_task_run_time_.value() - now;
  }

  base::Optional<base::TimeTicks> next_run_time = NextScheduledRunTime();
  if (!next_run_time) {
    V8RecordReplayAssert("ThrottledTimeDomain::DelayTillNextTask #2");
    return base::nullopt;
  }

  if (now >= next_run_time) {
    V8RecordReplayAssert("ThrottledTimeDomain::DelayTillNextTask #3");
    return base::TimeDelta();  // Makes DoWork post an immediate continuation.
  }

  // We assume the owner (i.e. TaskQueueThrottler) will manage wake-ups on our
  // behalf.
  V8RecordReplayAssert("ThrottledTimeDomain::DelayTillNextTask Done");
  return base::nullopt;
}

bool ThrottledTimeDomain::MaybeFastForwardToNextTask(
    bool quit_when_idle_requested) {
  return false;
}

}  // namespace scheduler
}  // namespace blink
