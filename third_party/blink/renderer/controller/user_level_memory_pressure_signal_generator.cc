// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/user_level_memory_pressure_signal_generator.h"

#include <algorithm>
#include <limits>

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_user_level_memory_pressure_signal_generator.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

namespace blink {

namespace {

// Each renderer does not generate memory pressure signals until the interval
// has passed after page loading is finished. This parameter must be larger
// than or equal to the time from navigation start to the time the
// DOMContentLoaded event is finished. 5min is much larger than
// the 99p of PageLoad.DocumentTiming.NavigationToDOMContentLoadedEventFired
// (14sec) and we expect the DOMContentLoaded events will finish in 5min.
// Negative inert interval disables delayed memory pressure signals
// This is intended to keep the old behavior.
constexpr base::TimeDelta kDefaultInertInterval = base::Minutes(5);

constexpr base::TimeDelta kDefaultMinimumInterval = base::Minutes(10);

UserLevelMemoryPressureSignalGenerator* g_instance = nullptr;

}  // namespace

// static
UserLevelMemoryPressureSignalGenerator*
UserLevelMemoryPressureSignalGenerator::Instance() {
  DCHECK(g_instance);
  return g_instance;
}

// static
void UserLevelMemoryPressureSignalGenerator::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DEFINE_STATIC_LOCAL(UserLevelMemoryPressureSignalGenerator, generator,
                      (std::move(task_runner)));
  (void)generator;
}

UserLevelMemoryPressureSignalGenerator::UserLevelMemoryPressureSignalGenerator(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : UserLevelMemoryPressureSignalGenerator(
          std::move(task_runner),
          kDefaultInertInterval,
          kDefaultMinimumInterval,
          ThreadScheduler::Current()->ToMainThreadScheduler()) {}

UserLevelMemoryPressureSignalGenerator::UserLevelMemoryPressureSignalGenerator(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::TimeDelta inert_interval,
    base::TimeDelta minimum_interval,
    MainThreadScheduler* main_thread_scheduler)
    : task_runner_(std::move(task_runner)),
      inert_interval_(inert_interval),
      minimum_interval_(minimum_interval),
      main_thread_scheduler_(main_thread_scheduler),
      timer_(task_runner_,
             this,
             &UserLevelMemoryPressureSignalGenerator::OnTimerFired) {
  CHECK(task_runner_);
  CHECK(!inert_interval_.is_negative());
  CHECK(minimum_interval_.is_positive());
  CHECK(main_thread_scheduler_);
  main_thread_scheduler->AddRAILModeObserver(this);
  DCHECK(!g_instance);
  g_instance = this;
}

UserLevelMemoryPressureSignalGenerator::
    ~UserLevelMemoryPressureSignalGenerator() {
  main_thread_scheduler_->RemoveRAILModeObserver(this);
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void UserLevelMemoryPressureSignalGenerator::RequestMemoryPressureSignal(
    base::MemoryPressureLevel level) {
  base::TimeTicks now = base::TimeTicks::Now();

  if (level == base::MEMORY_PRESSURE_LEVEL_NONE) {
    // Returning to no pressure. Cancel the pending request, if any.
    timer_.Stop();
    last_requested_ = std::nullopt;

    // Forget about the last time a critical signal was generated, so we don't
    // have to wait for `minimum_interval_` to propagate the memory pressure
    // level if it returns to critical.
    last_critical_generated_ = std::nullopt;

    // Don't send repeat NONE notifications.
    if (current_level_ != base::MEMORY_PRESSURE_LEVEL_NONE) {
      Generate(base::MEMORY_PRESSURE_LEVEL_NONE, now);
    }

    return;
  }

  CHECK_EQ(level, base::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // Check if there is already a pending request, while ensuring the timestamp
  // of the most recent request is saved.
  const bool has_pending_request = last_requested_.has_value();
  last_requested_ = now;
  if (has_pending_request) {
    return;
  }

  if (is_loading_) {
    // Still loading. Can't know when to generate the signal until loading
    // finishes.
    return;
  }

  // Calculate the next valid timestamp for signal generation, accounting for
  // inert and minimum intervals.
  base::TimeTicks next_valid_timestamp =
      CalculateNextValidGenerationTimestamp();

  // If that timestamp has already passed, generate immediately. Else start the
  // timer.
  if (next_valid_timestamp <= now) {
    Generate(base::MEMORY_PRESSURE_LEVEL_CRITICAL, now);
  } else {
    timer_.StartOneShot(next_valid_timestamp - now, FROM_HERE);
  }
}

void UserLevelMemoryPressureSignalGenerator::OnRAILModeChanged(
    RAILMode rail_mode) {
  bool was_loading = is_loading_;
  is_loading_ = rail_mode == RAILMode::kLoad;

  // State did not change.
  if (is_loading_ == was_loading) {
    return;
  }

  if (is_loading_) {
    // Just started loading. The timer must be stopped so the signal is not
    // generated. However, if there is a pending request, it is *not* cancelled.
    // A signal could still be generated if loading finishes quickly enough.
    timer_.Stop();
    return;
  }

  // Loading just ended.
  CHECK(!timer_.IsActive());
  base::TimeTicks now = base::TimeTicks::Now();
  last_loaded_ = now;

  // If there is no pending request, nothing left to do.
  if (!last_requested_) {
    return;
  }

  // We want to honor the pending request, but only if the signal would be
  // generated in a timely matter. If not, the request is cancelled.
  base::TimeTicks next_valid_timestamp =
      CalculateNextValidGenerationTimestamp();
  base::TimeTicks request_expiry = last_requested_.value() + minimum_interval_;
  if (next_valid_timestamp > request_expiry) {
    // Cancel the request.
    last_requested_ = std::nullopt;
    return;
  }

  timer_.StartOneShot(next_valid_timestamp - now, FROM_HERE);
}

base::TimeTicks
UserLevelMemoryPressureSignalGenerator::CalculateNextValidGenerationTimestamp()
    const {
  base::TimeTicks inert_interval_expiry =
      last_loaded_.value_or(base::TimeTicks::Min()) + inert_interval_;
  base::TimeTicks minimum_interval_expiry =
      last_critical_generated_.value_or(base::TimeTicks::Min()) +
      minimum_interval_;
  return std::max(inert_interval_expiry, minimum_interval_expiry);
}

void UserLevelMemoryPressureSignalGenerator::Generate(
    base::MemoryPressureLevel level,
    base::TimeTicks now) {
  if (level == base::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    last_critical_generated_ = now;
  }
  last_requested_ = std::nullopt;
  current_level_ = level;
  base::MemoryPressureListenerRegistry::NotifyMemoryPressureFromAnyThread(
      level);
}

void UserLevelMemoryPressureSignalGenerator::OnTimerFired(TimerBase*) {
  CHECK(last_requested_);
  CHECK(!is_loading_);
  base::TimeTicks now = base::TimeTicks::Now();
  // The inert interval is definitely passed.
  CHECK(!last_loaded_.has_value() ||
        now - last_loaded_.value() >= inert_interval_);
  // The minimum interval is also passed since the last generated CRITICAL
  // signal.
  CHECK(!last_critical_generated_.has_value() ||
        now - last_critical_generated_.value() >= minimum_interval_);

  // There shouldn't be any expired requests, but sometimes the task runs later
  // than scheduled.
  if (now - last_requested_.value() > minimum_interval_) {
    last_requested_ = std::nullopt;
    return;
  }

  Generate(base::MEMORY_PRESSURE_LEVEL_CRITICAL, now);
}

void RequestUserLevelMemoryPressureSignal(base::MemoryPressureLevel level) {
  // TODO(crbug.com/1473814): AndroidWebView creates renderer processes
  // without appending extra commandline switches,
  // c.f. ChromeContentBrowserClient::AppendExtraCommandLineSwitches(),
  // So renderer processes do not initialize user-level memory pressure
  // siginal generators but the browser code expects they have already been
  // initialized. So when requesting memory pressure signals, g_instance is
  // nullptr and will crash.
  if (UserLevelMemoryPressureSignalGenerator* generator =
          UserLevelMemoryPressureSignalGenerator::Instance()) {
    generator->RequestMemoryPressureSignal(level);
  }
}

}  // namespace blink
