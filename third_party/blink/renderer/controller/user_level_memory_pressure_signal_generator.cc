// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/user_level_memory_pressure_signal_generator.h"

#include <limits>

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/public/common/features.h"
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
          base::DefaultTickClock::GetInstance(),
          ThreadScheduler::Current()->ToMainThreadScheduler()) {}

UserLevelMemoryPressureSignalGenerator::UserLevelMemoryPressureSignalGenerator(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::TimeDelta inert_interval,
    base::TimeDelta minimum_interval,
    const base::TickClock* clock,
    MainThreadScheduler* main_thread_scheduler)
    : task_runner_(std::move(task_runner)),
      inert_interval_(inert_interval),
      minimum_interval_(minimum_interval),
      clock_(clock),
      main_thread_scheduler_(main_thread_scheduler) {
  CHECK(!inert_interval_.is_negative());
  CHECK(minimum_interval_.is_positive());
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

void UserLevelMemoryPressureSignalGenerator::OnRAILModeChanged(
    RAILMode rail_mode) {
  bool was_loading = is_loading_;
  is_loading_ = rail_mode == RAILMode::kLoad;

  if (!is_loading_) {
    if (!was_loading) {
      return;
    }

    // Loading is finished because rail_mode changes another mode from kLoad.
    last_loaded_ = clock_->NowTicks();
    if (has_pending_request_) {
      task_runner_->PostDelayedTask(
          FROM_HERE,
          BindOnce(&UserLevelMemoryPressureSignalGenerator::OnTimerFired,
                   UnretainedWrapper(this)),
          inert_interval_);
    }
  }
}

void UserLevelMemoryPressureSignalGenerator::RequestMemoryPressureSignal() {
  base::TimeTicks now = clock_->NowTicks();

  last_requested_ = now;

  // Wait |inert_interval_| after loading is finished.

  // If still loading, make |has_pending_request_| true and do not dispatch
  // any pressure signals now.
  if (is_loading_) {
    has_pending_request_ = true;
    return;
  }

  // Since loading is finished, we will see if |inert_interval_| has passed.
  base::TimeDelta elapsed = !last_loaded_.has_value()
                                ? inert_interval_
                                : (now - last_loaded_.value());

  // If |inert_interval_| has not passed yet, do not dispatch any memory
  // pressure signals now.
  if (elapsed < inert_interval_) {
    // If |has_pending_request_| = true, we will dispatch memory pressure
    // signal when |inert_interval_ - elapsed| passes.

    // Since we may have already started the timer, i.e.
    // - start at OnRAILModeChanged(),
    // - RequestMemoryPressureSignal() was invoked but still waiting
    // |inert_interval_|. in the case, |has_pending_request_| is true.
    if (!has_pending_request_) {
      task_runner_->PostDelayedTask(
          FROM_HERE,
          BindOnce(&UserLevelMemoryPressureSignalGenerator::OnTimerFired,
                   UnretainedWrapper(this)),
          inert_interval_ - elapsed);
    }
    has_pending_request_ = true;
    return;
  }

  // - if inert_interval_ < 0, dispatch memory pressure signal now.
  // - if loading is finished and >= |inert_interval_| passes after loading,
  //   dispatch memory pressure signal now.
  Generate(now);
}

void UserLevelMemoryPressureSignalGenerator::Generate(base::TimeTicks now) {
  // If |minimum_interval_| has not passed yet since the last generated time,
  // does not generate any signals to avoid too many signals.
  if (!last_generated_.has_value() ||
      (now - last_generated_.value()) >= minimum_interval_) {
    base::MemoryPressureListenerRegistry::NotifyMemoryPressureFromAnyThread(
        base::MEMORY_PRESSURE_LEVEL_CRITICAL);
    last_generated_ = now;
  }
  has_pending_request_ = false;
}

void UserLevelMemoryPressureSignalGenerator::OnTimerFired() {
  base::TimeTicks now = clock_->NowTicks();

  DCHECK(has_pending_request_);

  // If still loading, skip generating memory pressure signals. After loading
  // is finished, start |signal_dispatch_timer_|.
  if (is_loading_) {
    // |has_pending_request_| must be kept true to know that memory pressure
    // signal was requested when loading is finished.
    return;
  }

  // If the inert interval has not passed yet, skip generating memory pressure
  // signals. A new delayed task is posted and it will be executed at the end
  // of inert interval.
  if ((now - last_loaded_.value()) < inert_interval_) {
    return;
  }

  // UserLevelMemoryPressureSignalGenerator will start monitoring if
  // |minimum_interval_| passes after requesting memory pressure signals.
  // So if we cannot dispatch pressure signals for kMinimumInterval (because
  // of loading), we will wait for another request. If TotalPMF is still
  // large, UserLevelMemoryPressureSignalGenerator will request pressure
  // signals soon.
  if ((now - last_requested_) > minimum_interval_) {
    has_pending_request_ = false;
    return;
  }

  Generate(now);
}

void RequestUserLevelMemoryPressureSignal() {
  // TODO(crbug.com/1473814): AndroidWebView creates renderer processes
  // without appending extra commandline switches,
  // c.f. ChromeContentBrowserClient::AppendExtraCommandLineSwitches(),
  // So renderer processes do not initialize user-level memory pressure
  // siginal generators but the browser code expects they have already been
  // initialized. So when requesting memory pressure signals, g_instance is
  // nullptr and g_instance->clock_ will crash.
  if (UserLevelMemoryPressureSignalGenerator* generator =
          UserLevelMemoryPressureSignalGenerator::Instance()) {
    generator->RequestMemoryPressureSignal();
  }
}

}  // namespace blink
