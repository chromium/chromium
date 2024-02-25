// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_PRESSURE_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_GENERATOR_H_
#define CONTENT_BROWSER_MEMORY_PRESSURE_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_GENERATOR_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include <optional>
#include <utility>

#include "base/no_destructor.h"
#include "base/timer/timer.h"

namespace base {
class Process;
class TimeDelta;
}  // namespace base

namespace memory_pressure {

// Generates extra memory pressure signals (on top of the OS generated ones)
// when the memory usage exceeds a threshold.
class UserLevelMemoryPressureSignalGenerator {
 public:
  static void Initialize();

 private:
  friend class base::NoDestructor<UserLevelMemoryPressureSignalGenerator>;

  // Singleton
  static UserLevelMemoryPressureSignalGenerator& Get();

  UserLevelMemoryPressureSignalGenerator();
  ~UserLevelMemoryPressureSignalGenerator();

  void Start(uint64_t memory_threshold,
             base::TimeDelta measure_interval,
             base::TimeDelta minimum_interval);
  void OnTimerFired();
  void OnReportingTimerFired();

  void StartPeriodicTimer(base::TimeDelta interval);
  void StartReportingTimer();

  static std::pair<uint64_t, uint64_t>
  GetTotalPrivateFootprintVisibleOrHigherPriorityRenderers();

  static void NotifyMemoryPressure();

  static void ReportBeforeAfterMetrics(
      uint64_t total_pmf_visible_or_higher_priority_renderers,
      uint64_t total_pmf,
      const char* suffix_name);

  static std::optional<uint64_t> GetPrivateFootprint(
      const base::Process& process);

  uint64_t memory_threshold_;
  base::TimeDelta measure_interval_;
  base::TimeDelta minimum_interval_;
  base::OneShotTimer periodic_measuring_timer_;
  base::OneShotTimer delayed_report_timer_;
};

}  // namespace memory_pressure

#endif  // BUILDFLAG(IS_ANDROID)

#endif  // CONTENT_BROWSER_MEMORY_PRESSURE_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_GENERATOR_H_
