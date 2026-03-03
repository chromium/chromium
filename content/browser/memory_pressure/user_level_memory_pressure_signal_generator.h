// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_PRESSURE_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_GENERATOR_H_
#define CONTENT_BROWSER_MEMORY_PRESSURE_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_GENERATOR_H_

#include <memory>
#include <optional>
#include <utility>

#include "base/byte_count.h"
#include "base/memory/memory_pressure_level.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/memory_pressure/memory_pressure_voter.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "components/memory_pressure/system_memory_pressure_evaluator.h"
#include "content/public/browser/user_level_memory_pressure_metrics.h"

namespace base {
class Process;
class TimeDelta;
}  // namespace base

namespace content {

// Generates extra memory pressure signals (on top of the OS generated ones)
// when the memory usage exceeds a threshold.
class UserLevelMemoryPressureSignalGenerator
    : public memory_pressure::SystemMemoryPressureEvaluator {
 public:
  // Creates an instance. Returns nullptr if
  // UserLevelMemoryPressureSignalGenerator if disabled on this device.
  static std::unique_ptr<UserLevelMemoryPressureSignalGenerator> MaybeCreate(
      std::unique_ptr<memory_pressure::MemoryPressureVoter> voter);

  ~UserLevelMemoryPressureSignalGenerator() override;

  // Returns the latest memory metrics if the metrics collection is enabled.
  static std::optional<content::UserLevelMemoryPressureMetrics>
  GetLatestMemoryMetrics();

 private:
  explicit UserLevelMemoryPressureSignalGenerator(
      std::unique_ptr<memory_pressure::MemoryPressureVoter> voter);

  void StartMetricsCollection();

  void CollectMemoryMetrics();

  void StartPeriodicTimer(base::TimeDelta interval);
  void OnTimerFired();

  void StartReportingTimer();
  void OnReportingTimerFired();

  static base::ByteCount
  GetTotalPrivateFootprintVisibleOrHigherPriorityRenderers();

  void HandleMemoryPressureLevel(base::MemoryPressureLevel level);

  static void ReportBeforeAfterMetrics(
      base::ByteCount total_pmf_visible_or_higher_priority_renderers,
      const char* suffix_name);

  static std::optional<base::ByteCount> GetPrivateFootprint(
      const base::Process& process);

  std::optional<content::UserLevelMemoryPressureMetrics>
  GetLatestMemoryMetricsImpl();

  base::ByteCount memory_threshold_;
  base::TimeDelta measure_interval_;
  base::TimeDelta minimum_interval_;
  base::OneShotTimer periodic_measuring_timer_;
  base::OneShotTimer delayed_report_timer_;

  base::MemoryPressureLevel current_level_ = base::MEMORY_PRESSURE_LEVEL_NONE;

  std::optional<UserLevelMemoryPressureMetrics> latest_metrics_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_PRESSURE_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_GENERATOR_H_
