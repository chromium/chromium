// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_BATTERY_METRICS_H_
#define CONTENT_BROWSER_ANDROID_BATTERY_METRICS_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/power_monitor/energy_monitor_android.h"
#include "base/power_monitor/power_observer.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "content/common/content_export.h"
#include "content/common/process_visibility_tracker.h"

namespace content {

// Records metrics around battery usage on Android. The metrics are only tracked
// while the device is not charging and the app is visible. This class is not
// thread-safe.
class AndroidBatteryMetrics
    : public base::PowerStateObserver,
      public base::PowerThermalObserver,
      public ProcessVisibilityTracker::ProcessVisibilityObserver,
      public performance_scenarios::PerformanceScenarioObserver {
 public:
  // CaptureAndReportMetrics() reports some metrics sliced by loading/input
  // scenarios. These are not necessarily the last values reported by
  // OnLoadingScenarioChanged() / OnInputScenarioChanged().
  // This class tracks observed scenarios over the metrics reporting window, and
  // computes the combined scenario to be reported.
  class CONTENT_EXPORT PerformanceScenarioTracker {
   public:
    void UpdateLoadingScenario(
        performance_scenarios::LoadingScenario new_scenario);
    void UpdateInputScenario(performance_scenarios::InputScenario new_scenario);
    std::string GetMetricSuffix() const;
    void UseLatestScenarios();

   private:
    std::optional<performance_scenarios::LoadingScenario>
        latest_loading_scenario_;
    std::optional<performance_scenarios::LoadingScenario>
        loading_scenario_to_report_;
    std::optional<performance_scenarios::InputScenario> latest_input_scenario_;
    std::optional<performance_scenarios::InputScenario>
        input_scenario_to_report_;
  };

  class CONTENT_EXPORT EnergyConsumedTracker {
   public:
    // Classification of power monitor consumers into subsystems for power
    // attribution. The exact list of subsystems and their meaning depends on
    // the device
    // https://developer.android.com/reference/android/os/PowerMonitor#POWER_MONITOR_TYPE_CONSUMER,
    // so this works on a best-effort basis.
    enum class Subsystem {
      kCpu = 0,
      kGpu = 1,
      kDisplay = 2,
      kOther = 3,
    };

    struct Delta {
      Subsystem subsystem;
      int64_t energy_consumed_mwh;
    };

    EnergyConsumedTracker();
    ~EnergyConsumedTracker();

    void UpdatePowerMonitorReadings(
        const std::vector<base::android::PowerMonitorReading>& readings);
    // Returns per subsystem deltas in milliwatt-hours.
    std::vector<Delta> GetDeltas(
        const std::vector<base::android::PowerMonitorReading>& readings) const;

   private:
    base::flat_map<Subsystem, int64_t> last_total_energy_uws_;
  };

  static void CreateInstance();

  AndroidBatteryMetrics(const AndroidBatteryMetrics&) = delete;
  AndroidBatteryMetrics& operator=(const AndroidBatteryMetrics&) = delete;

 private:
  friend class base::NoDestructor<AndroidBatteryMetrics>;
  AndroidBatteryMetrics();
  ~AndroidBatteryMetrics() override;

  void InitializeOnSequence();

  // PerformanceScenarioObserverList is initialized after this class, so the
  // observer is added lazily by this method.
  // `is_observing_peformance_scenarios_` enforces it only happens once.
  void TryObservePerformanceScenarios();

  // ProcessVisibilityTracker::ProcessVisibilityObserver implementation:
  void OnVisibilityChanged(bool visible) override;

  // PerformanceScenarioObserver implementation:
  void OnLoadingScenarioChanged(
      performance_scenarios::ScenarioScope scope,
      performance_scenarios::LoadingScenario old_scenario,
      performance_scenarios::LoadingScenario new_scenario) override;
  void OnInputScenarioChanged(
      performance_scenarios::ScenarioScope scope,
      performance_scenarios::InputScenario old_scenario,
      performance_scenarios::InputScenario new_scenario) override;

  // base::PowerStateObserver implementation:
  void OnBatteryPowerStatusChange(base::PowerStateObserver::BatteryPowerStatus
                                      battery_power_status) override;

  // base::PowerThermalObserver implementation:
  void OnThermalStateChange(DeviceThermalState new_state) override;
  void OnSpeedLimitChange(int speed_limit) override;

  void UpdateMetricsEnabled();
  void CaptureAndReportMetrics(bool disabling);

  // Whether or not we've seen at least two consecutive capacity drops while
  // the embedding app was visible. Battery drain reported prior to this could
  // be caused by a different app.
  bool IsMeasuringDrainExclusively() const;

  // Battery drain is captured and reported periodically in this interval while
  // the device is on battery power and the app is visible.
  static constexpr base::TimeDelta kMetricsInterval = base::Seconds(30);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  bool app_visible_ = false;
  PowerStateObserver::BatteryPowerStatus battery_power_status_ =
      PowerStateObserver::BatteryPowerStatus::kUnknown;
  int last_remaining_capacity_uah_ = 0;
  EnergyConsumedTracker energy_consumed_tracker_;
  base::RepeatingTimer metrics_timer_;
  int skipped_timers_ = 0;

  // Number of consecutive charge drops seen while the app has been visible.
  int observed_capacity_drops_ = 0;

  bool is_observing_performance_scenarios_ = false;
  PerformanceScenarioTracker performance_scenario_tracker_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_BATTERY_METRICS_H_
