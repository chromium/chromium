// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/process_time_override_coordinator.h"

namespace blink::scheduler {

ProcessTimeOverrideCoordinator::ScopedOverride::ScopedOverride(
    base::RepeatingClosure schedule_work_callback)
    : schedule_work_callback_(std::move(schedule_work_callback)) {}

ProcessTimeOverrideCoordinator::ScopedOverride::~ScopedOverride() {
  ProcessTimeOverrideCoordinator::Instance().UnregisterOverride(this);
}

base::TimeTicks
ProcessTimeOverrideCoordinator::ScopedOverride::TryAdvancingTime(
    base::TimeTicks requested_ticks) {
  return ProcessTimeOverrideCoordinator::Instance().TryAdvancingTime(
      this, requested_ticks);
}

base::TimeTicks ProcessTimeOverrideCoordinator::ScopedOverride::NowTicks()
    const {
  return ProcessTimeOverrideCoordinator::CurrentTicks();
}

// static
ProcessTimeOverrideCoordinator& ProcessTimeOverrideCoordinator::Instance() {
  static base::NoDestructor<ProcessTimeOverrideCoordinator> s_instance;
  return *s_instance;
}

ProcessTimeOverrideCoordinator::ProcessTimeOverrideCoordinator() = default;

std::unique_ptr<ProcessTimeOverrideCoordinator::ScopedOverride>
ProcessTimeOverrideCoordinator::CreateOverride(
    base::Time requested_time,
    base::TimeTicks requested_ticks,
    base::RepeatingClosure schedule_work_callback) {
  auto handle =
      base::WrapUnique(new ScopedOverride(std::move(schedule_work_callback)));

  Instance().RegisterOverride(handle.get(), requested_time, requested_ticks);
  return handle;
}

void ProcessTimeOverrideCoordinator::RegisterOverride(
    ScopedOverride* handle,
    base::Time requested_time,
    base::TimeTicks requested_ticks) {
  base::AutoLock auto_lock(lock_);
  if (requested_ticks_by_client_.empty()) {
    EnableOverride(requested_time, requested_ticks);
  }
  bool inserted =
      requested_ticks_by_client_
          .insert({handle, current_ticks_.load(std::memory_order_relaxed)})
          .second;
  DCHECK(inserted);
}

void ProcessTimeOverrideCoordinator::UnregisterOverride(
    ScopedOverride* handle) {
  base::AutoLock auto_lock(lock_);
  size_t erased = requested_ticks_by_client_.erase(handle);
  DCHECK(erased);
  if (requested_ticks_by_client_.empty()) {
    DisableOverride();
  }
}

void ProcessTimeOverrideCoordinator::EnableOverride(
    base::Time initial_time,
    base::TimeTicks initial_ticks) {
  DCHECK(!clock_override_);
  initial_time_ = initial_time;
  initial_ticks_ = initial_ticks;
  current_ticks_.store(initial_ticks, std::memory_order_release);

  clock_override_ = std::make_unique<base::subtle::ScopedTimeClockOverrides>(
      &ProcessTimeOverrideCoordinator::CurrentTime,
      &ProcessTimeOverrideCoordinator::CurrentTicks, nullptr);
}

void ProcessTimeOverrideCoordinator::DisableOverride() {
  DCHECK(clock_override_);
  clock_override_.reset();
  // This is only to keep tests happy, as we may re-enable overrides again
  // and expect time to increase monotonically.
  current_ticks_.store(base::TimeTicks(), std::memory_order_release);
}

base::TimeTicks ProcessTimeOverrideCoordinator::TryAdvancingTime(
    ScopedOverride* handle,
    base::TimeTicks requested_ticks) {
  base::AutoLock auto_lock(lock_);

  const auto previous_ticks = current_ticks_.load(std::memory_order_relaxed);
  // We can't count on clients to always request ticks in the future,
  // as they use the time of next delayed task to request it and may
  // thus change their mind when getting a shorter term task posted
  // after having originally requested a longer term advance.
  if (requested_ticks <= previous_ticks) {
    return previous_ticks;
  }

  auto client_it = requested_ticks_by_client_.find(handle);
  CHECK(client_it != requested_ticks_by_client_.end());
  if (client_it->second == requested_ticks) {
    // A client may re-request the time it has asked for previously in case
    // it got awaken before that time is reached.
    return previous_ticks;
  }

  client_it->second = requested_ticks;

  base::TimeTicks new_ticks = requested_ticks;
  for (const auto& entry : requested_ticks_by_client_) {
    if (entry.second < new_ticks) {
      new_ticks = entry.second;
    }
  }

  if (new_ticks > previous_ticks) {
    current_ticks_.store(new_ticks, std::memory_order_release);

    for (const auto& entry : requested_ticks_by_client_) {
      if (entry.first != handle) {
        entry.first->ScheduleWork();
      }
    }
  }

  return new_ticks;
}

// static
base::Time ProcessTimeOverrideCoordinator::CurrentTime() {
  auto& self = ProcessTimeOverrideCoordinator::Instance();
  auto ticks = self.current_ticks_.load(std::memory_order_acquire);
  if (ticks.is_null()) {
    return base::subtle::TimeNowIgnoringOverride();
  }
  return self.initial_time_ + (ticks - self.initial_ticks_);
}

// static
base::TimeTicks ProcessTimeOverrideCoordinator::CurrentTicks() {
  auto ticks = ProcessTimeOverrideCoordinator::Instance().current_ticks_.load(
      std::memory_order_relaxed);
  return ticks.is_null() ? base::subtle::TimeTicksNowIgnoringOverride() : ticks;
}

}  // namespace blink::scheduler
