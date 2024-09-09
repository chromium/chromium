// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_BATTERY_METRICS_H_
#define CONTENT_BROWSER_ANDROID_BATTERY_METRICS_H_

#include "base/no_destructor.h"
#include "base/power_monitor/power_observer.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "content/common/process_visibility_tracker.h"

namespace content {

// Records metrics around battery usage on Android. The metrics are only tracked
// while the device is not charging and the app is visible. This class is not
// thread-safe.
class AndroidBatteryMetrics
    : public base::PowerStateObserver,
      public base::PowerThermalObserver,
      public ProcessVisibilityTracker::ProcessVisibilityObserver {
 public:
  static void CreateInstance();

  AndroidBatteryMetrics(const AndroidBatteryMetrics&) = delete;
  AndroidBatteryMetrics& operator=(const AndroidBatteryMetrics&) = delete;

 private:
  friend class base::NoDestructor<AndroidBatteryMetrics>;
  AndroidBatteryMetrics();
  ~AndroidBatteryMetrics() override;

  void InitializeOnSequence();

  // ProcessVisibilityTracker::ProcessVisibilityObserver implementation:
  void OnVisibilityChanged(bool visible) override;

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
  base::RepeatingTimer metrics_timer_;
  int skipped_timers_ = 0;

  // Number of consecutive charge drops seen while the app has been visible.
  int observed_capacity_drops_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_BATTERY_METRICS_H_
