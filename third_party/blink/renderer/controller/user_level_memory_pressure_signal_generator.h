// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_GENERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_GENERATOR_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/controller/controller_export.h"
#include "third_party/blink/renderer/controller/memory_usage_monitor.h"
#include "third_party/blink/renderer/platform/scheduler/public/rail_mode_observer.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace base {
class TickClock;
}

namespace blink {

namespace user_level_memory_pressure_signal_generator_test {
class MockUserLevelMemoryPressureSignalGenerator;
}  // namespace user_level_memory_pressure_signal_generator_test

// Generates extra memory pressure signals (on top of the OS generated ones)
// when the memory usage goes over a threshold.
class CONTROLLER_EXPORT UserLevelMemoryPressureSignalGenerator
    : public RAILModeObserver,
      public MemoryUsageMonitor::Observer {
  USING_FAST_MALLOC(UserLevelMemoryPressureSignalGenerator);

 public:
  // Returns the shared instance.
  static void Initialize(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

 private:
  friend class user_level_memory_pressure_signal_generator_test::
      MockUserLevelMemoryPressureSignalGenerator;

  explicit UserLevelMemoryPressureSignalGenerator(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  UserLevelMemoryPressureSignalGenerator(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::TickClock* clock);
  ~UserLevelMemoryPressureSignalGenerator() override;

  // This is only virtual to override in tests.
  virtual void Generate(MemoryUsage);

  // Called by delayed_report_timer_ to report metrics.
  void OnTimerFired(TimerBase*);

  // RAILModeObserver:
  void OnRAILModeChanged(RAILMode rail_mode) override;

  // MemoryUsageMonitor::Observer:
  void OnMemoryPing(MemoryUsage) override;

  bool is_loading_ = false;
  base::TimeTicks last_generated_;
  double memory_threshold_mb_;
  base::TimeDelta minimum_interval_;
  TaskRunnerTimer<UserLevelMemoryPressureSignalGenerator> delayed_report_timer_;
  const base::TickClock* clock_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CONTROLLER_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_GENERATOR_H_
