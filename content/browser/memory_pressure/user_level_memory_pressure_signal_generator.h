// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_PRESSURE_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_GENERATOR_H_
#define CONTENT_BROWSER_MEMORY_PRESSURE_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_GENERATOR_H_

#include <optional>
#include <utility>

#include "base/byte_count.h"
#include "base/no_destructor.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/public/browser/user_level_memory_pressure_metrics.h"

namespace base {
class Process;
class TimeDelta;
}  // namespace base

namespace content {

// Generates extra memory pressure signals (on top of the OS generated ones)
// when the memory usage exceeds a threshold.
class UserLevelMemoryPressureSignalGenerator {
 public:
  static void Initialize();

  // Returns the latest memory metrics if the metrics collection is enabled.
  static std::optional<content::UserLevelMemoryPressureMetrics>
  GetLatestMemoryMetrics();

 private:
  friend class base::NoDestructor<UserLevelMemoryPressureSignalGenerator>;

  // Singleton
  static UserLevelMemoryPressureSignalGenerator& Get();

  UserLevelMemoryPressureSignalGenerator();
  ~UserLevelMemoryPressureSignalGenerator();

  void Start(base::ByteCount memory_threshold,
             base::TimeDelta measure_interval,
             base::TimeDelta minimum_interval);
  void OnTimerFired();
  void OnReportingTimerFired();

  void StartPeriodicTimer(base::TimeDelta interval);
  void StartReportingTimer();

  void StartMetricsCollection();
  void CollectMemoryMetrics();

  static base::ByteCount
  GetTotalPrivateFootprintVisibleOrHigherPriorityRenderers();

  static void NotifyMemoryPressure();

  static void ReportBeforeAfterMetrics(
      base::ByteCount total_pmf_visible_or_higher_priority_renderers,
      const char* suffix_name);

  static std::optional<base::ByteCount> GetPrivateFootprint(
      const base::Process& process);

  base::ByteCount memory_threshold_;
  base::TimeDelta measure_interval_;
  base::TimeDelta minimum_interval_;
  base::OneShotTimer periodic_measuring_timer_;
  base::OneShotTimer delayed_report_timer_;

  std::optional<UserLevelMemoryPressureMetrics> latest_metrics_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_PRESSURE_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_GENERATOR_H_
