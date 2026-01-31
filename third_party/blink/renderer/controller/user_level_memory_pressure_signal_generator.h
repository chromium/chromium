// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_GENERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_GENERATOR_H_

#include <optional>

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/controller/controller_export.h"
#include "third_party/blink/renderer/controller/memory_usage_monitor.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/rail_mode_observer.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class MainThreadScheduler;

// Generates extra memory pressure signals (on top of the OS generated ones)
// when the memory usage goes over a threshold.
class CONTROLLER_EXPORT UserLevelMemoryPressureSignalGenerator
    : public RAILModeObserver {
  USING_FAST_MALLOC(UserLevelMemoryPressureSignalGenerator);

 public:
  static UserLevelMemoryPressureSignalGenerator* Instance();

  // Returns the shared instance.
  static void Initialize(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  explicit UserLevelMemoryPressureSignalGenerator(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  UserLevelMemoryPressureSignalGenerator(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::TimeDelta inert_interval,
      base::TimeDelta minimum_interval,
      MainThreadScheduler* main_thread_scheduler);

  ~UserLevelMemoryPressureSignalGenerator() override;

  void RequestMemoryPressureSignal(base::MemoryPressureLevel level);

  // RAILModeObserver:
  void OnRAILModeChanged(RAILMode rail_mode) override;

 private:
  void Generate(base::MemoryPressureLevel level, base::TimeTicks now);

  void OnTimerFired(TimerBase*);

  // Calculates the next valid timestamp for signal generation, accounting for
  // inert and minimum intervals.
  base::TimeTicks CalculateNextValidGenerationTimestamp() const;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::TimeDelta inert_interval_;
  base::TimeDelta minimum_interval_;
  raw_ptr<MainThreadScheduler> main_thread_scheduler_;

  // Indicates if the RAILMode is currently kLoad.
  bool is_loading_ = false;

  // The timestamp at which the RAILMode last became kLoad. If nullopt, the
  // RAILMode never became kLoad.
  std::optional<base::TimeTicks> last_loaded_;

  // The timestamp of the pending request. If nullopt, there are no pending
  // request.
  std::optional<base::TimeTicks> last_requested_;

  // The timestamp of the last generated MEMORY_PRESSURE_LEVEL_CRITICAL signal.
  // If nullopt, no signal was generated yet since the last time the memory
  // pressure level was MEMORY_PRESSURE_LEVEL_NONE.
  std::optional<base::TimeTicks> last_critical_generated_;

  base::MemoryPressureLevel current_level_ = base::MEMORY_PRESSURE_LEVEL_NONE;

  // Timer that tracks when the next signal can be generated.
  TaskRunnerTimer<UserLevelMemoryPressureSignalGenerator> timer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CONTROLLER_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_GENERATOR_H_
