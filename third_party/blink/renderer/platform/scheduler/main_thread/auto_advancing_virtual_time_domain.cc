// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/auto_advancing_virtual_time_domain.h"

#include <atomic>

#include "base/atomicops.h"
#include "base/time/time_override.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/scheduler/common/scheduler_helper.h"

namespace blink {
namespace scheduler {

AutoAdvancingVirtualTimeDomain::AutoAdvancingVirtualTimeDomain(
    base::Time initial_time,
    base::TimeTicks initial_time_ticks,
    SchedulerHelper* helper,
    BaseTimeOverridePolicy policy)
    : task_starvation_count_(0),
      max_task_starvation_count_(0),
      can_advance_virtual_time_(true),
      helper_(helper),
      now_ticks_(initial_time_ticks),
      initial_time_ticks_(initial_time_ticks),
      initial_time_(initial_time),
      previous_time_(initial_time) {
  DCHECK_EQ(AutoAdvancingVirtualTimeDomain::g_time_domain_, nullptr);
  AutoAdvancingVirtualTimeDomain::g_time_domain_ = this;

  // GetVirtualTime / GetVirtualTimeTicks access g_time_domain_.
  std::atomic_thread_fence(std::memory_order_seq_cst);

  if (policy == BaseTimeOverridePolicy::OVERRIDE) {
    time_overrides_ = std::make_unique<base::subtle::ScopedTimeClockOverrides>(
        &AutoAdvancingVirtualTimeDomain::GetVirtualTime,
        &AutoAdvancingVirtualTimeDomain::GetVirtualTimeTicks, nullptr);
  }

  helper_->AddTaskObserver(this);
}

AutoAdvancingVirtualTimeDomain::~AutoAdvancingVirtualTimeDomain() {
  helper_->RemoveTaskObserver(this);

  time_overrides_.reset();

  // GetVirtualTime / GetVirtualTimeTicks (the functions we may have
  // temporariliy installed in the constructor) access g_time_domain_.
  std::atomic_thread_fence(std::memory_order_seq_cst);

  DCHECK_EQ(AutoAdvancingVirtualTimeDomain::g_time_domain_, this);
  AutoAdvancingVirtualTimeDomain::g_time_domain_ = nullptr;
}

base::sequence_manager::LazyNow AutoAdvancingVirtualTimeDomain::CreateLazyNow()
    const {
  base::AutoLock lock(now_ticks_lock_);
  return base::sequence_manager::LazyNow(now_ticks_);
}

base::TimeTicks AutoAdvancingVirtualTimeDomain::Now() const {
  base::AutoLock lock(now_ticks_lock_);
  return now_ticks_;
}

base::Optional<base::TimeDelta>
AutoAdvancingVirtualTimeDomain::DelayTillNextTask(
    base::sequence_manager::LazyNow* lazy_now) {
  base::Optional<base::TimeTicks> run_time = NextScheduledRunTime();
  if (!run_time)
    return base::nullopt;

  // We may have advanced virtual time past the next task when a
  // WebScopedVirtualTimePauser unpauses.
  if (run_time <= Now())
    return base::TimeDelta();

  // Rely on MaybeFastForwardToNextTask to be called to advance
  // virtual time.
  return base::nullopt;
}

bool AutoAdvancingVirtualTimeDomain::MaybeFastForwardToNextTask(
    bool quit_when_idle_requested) {
  if (!can_advance_virtual_time_)
    return false;

  base::Optional<base::TimeTicks> run_time = NextScheduledRunTime();
  if (!run_time)
    return false;

  if (MaybeAdvanceVirtualTime(*run_time)) {
    task_starvation_count_ = 0;
    return true;
  }

  return false;
}

void AutoAdvancingVirtualTimeDomain::SetNextDelayedDoWork(
    base::sequence_manager::LazyNow* lazy_now,
    base::TimeTicks run_time) {
  // Ignore cancelation since no delayed work is actually being posted.
  if (run_time == base::TimeTicks::Max())
    return;

  // Avoid posting pointless DoWorks, i.e. if the time domain has more then one
  // scheduled wake up then we don't need to do anything.
  if (can_advance_virtual_time_ && NumberOfScheduledWakeUps() == 1u)
    RequestDoWork();
}

void AutoAdvancingVirtualTimeDomain::SetCanAdvanceVirtualTime(
    bool can_advance_virtual_time) {
  can_advance_virtual_time_ = can_advance_virtual_time;
  if (can_advance_virtual_time_)
    RequestDoWork();
}

void AutoAdvancingVirtualTimeDomain::SetMaxVirtualTimeTaskStarvationCount(
    int max_task_starvation_count) {
  max_task_starvation_count_ = max_task_starvation_count;
  if (max_task_starvation_count_ == 0)
    task_starvation_count_ = 0;
}

void AutoAdvancingVirtualTimeDomain::SetVirtualTimeFence(
    base::TimeTicks virtual_time_fence) {
  virtual_time_fence_ = virtual_time_fence;
  if (!requested_next_virtual_time_.is_null())
    MaybeAdvanceVirtualTime(requested_next_virtual_time_);
}

bool AutoAdvancingVirtualTimeDomain::MaybeAdvanceVirtualTime(
    base::TimeTicks new_virtual_time) {
  // If set, don't advance past the end of |virtual_time_fence_|.
  if (!virtual_time_fence_.is_null() &&
      new_virtual_time > virtual_time_fence_) {
    requested_next_virtual_time_ = new_virtual_time;
    new_virtual_time = virtual_time_fence_;
  } else {
    requested_next_virtual_time_ = base::TimeTicks();
  }

  if (new_virtual_time <= Now())
    return false;

  {
    base::AutoLock lock(now_ticks_lock_);
    now_ticks_ = new_virtual_time;
  }

  return true;
}

const char* AutoAdvancingVirtualTimeDomain::GetName() const {
  return "AutoAdvancingVirtualTimeDomain";
}

void AutoAdvancingVirtualTimeDomain::WillProcessTask(
    const base::PendingTask& pending_task) {}

void AutoAdvancingVirtualTimeDomain::DidProcessTask(
    const base::PendingTask& pending_task) {
  if (max_task_starvation_count_ == 0 ||
      ++task_starvation_count_ < max_task_starvation_count_) {
    return;
  }

  // Delayed tasks are being excessively starved, so allow virtual time to
  // advance.
  base::Optional<base::TimeTicks> run_time = NextScheduledRunTime();
  if (run_time && MaybeAdvanceVirtualTime(*run_time))
    task_starvation_count_ = 0;
}

base::Time AutoAdvancingVirtualTimeDomain::Date() const {
  base::TimeDelta offset = Now() - initial_time_ticks_;
  return initial_time_ + offset;
}

AutoAdvancingVirtualTimeDomain* AutoAdvancingVirtualTimeDomain::g_time_domain_ =
    nullptr;

// static
base::TimeTicks AutoAdvancingVirtualTimeDomain::GetVirtualTimeTicks() {
  DCHECK(AutoAdvancingVirtualTimeDomain::g_time_domain_);
  return AutoAdvancingVirtualTimeDomain::g_time_domain_->Now();
}

// static
base::Time AutoAdvancingVirtualTimeDomain::GetVirtualTime() {
  DCHECK(AutoAdvancingVirtualTimeDomain::g_time_domain_);
  return AutoAdvancingVirtualTimeDomain::g_time_domain_->Date();
}

}  // namespace scheduler
}  // namespace blink
