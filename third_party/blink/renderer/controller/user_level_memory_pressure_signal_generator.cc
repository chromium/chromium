// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/user_level_memory_pressure_signal_generator.h"

#include <limits>
#include "base/memory/memory_pressure_listener.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

namespace blink {

namespace {

constexpr double kDefaultMemoryThresholdMB =
    std::numeric_limits<double>::infinity();

double MemoryThresholdParamOf512MbDevices() {
  static const base::FeatureParam<double> k512MBDeviceMemoryThresholdParam{
      &blink::features::kUserLevelMemoryPressureSignal,
      "param_512mb_device_memory_threshold_mb", kDefaultMemoryThresholdMB};
  return k512MBDeviceMemoryThresholdParam.Get();
}

double MemoryThresholdParamOf1GbDevices() {
  static const base::FeatureParam<double> k1GBDeviceMemoryThresholdParam{
      &blink::features::kUserLevelMemoryPressureSignal,
      "param_1gb_device_memory_threshold_mb", kDefaultMemoryThresholdMB};
  return k1GBDeviceMemoryThresholdParam.Get();
}

double MemoryThresholdParamOf2GbDevices() {
  static const base::FeatureParam<double> k2GBDeviceMemoryThresholdParam{
      &blink::features::kUserLevelMemoryPressureSignal,
      "param_2gb_device_memory_threshold_mb", kDefaultMemoryThresholdMB};
  return k2GBDeviceMemoryThresholdParam.Get();
}

double MemoryThresholdParamOf3GbDevices() {
  static const base::FeatureParam<double> k3GBDeviceMemoryThresholdParam{
      &blink::features::kUserLevelMemoryPressureSignal,
      "param_3gb_device_memory_threshold_mb", kDefaultMemoryThresholdMB};
  return k3GBDeviceMemoryThresholdParam.Get();
}

double MemoryThresholdParamOf4GbDevices() {
  static const base::FeatureParam<double> k4GBDeviceMemoryThresholdParam{
      &blink::features::kUserLevelMemoryPressureSignal,
      "param_4gb_device_memory_threshold_mb", kDefaultMemoryThresholdMB};
  return k4GBDeviceMemoryThresholdParam.Get();
}

constexpr double kDefaultMinimumIntervalSeconds = 10 * 60;

// Minimum time interval between generated memory pressure signals.
base::TimeDelta MinimumIntervalSeconds() {
  static const base::FeatureParam<double> kMinimumIntervalSeconds{
      &blink::features::kUserLevelMemoryPressureSignal, "minimum_interval_s",
      kDefaultMinimumIntervalSeconds};
  return base::Seconds(kMinimumIntervalSeconds.Get());
}

double MemoryThresholdParam() {
  int physical_memory_mb = base::SysInfo::AmountOfPhysicalMemoryMB();
  if (physical_memory_mb > 3.1 * 1024)
    return MemoryThresholdParamOf4GbDevices();
  if (physical_memory_mb > 2.1 * 1024)
    return MemoryThresholdParamOf3GbDevices();
  if (physical_memory_mb > 1.1 * 1024)
    return MemoryThresholdParamOf2GbDevices();
  return (physical_memory_mb > 600) ? MemoryThresholdParamOf1GbDevices()
                                    : MemoryThresholdParamOf512MbDevices();
}

// static
bool IsUserLevelMemoryPressureSignalGeneratorEnabled() {
  if (!base::FeatureList::IsEnabled(
          blink::features::kUserLevelMemoryPressureSignal))
    return false;

  // Can be disabled for certain device classes by setting the field param to an
  // empty string.
  return !std::isinf(MemoryThresholdParam());
}

}  // namespace

// static
void UserLevelMemoryPressureSignalGenerator::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!IsUserLevelMemoryPressureSignalGeneratorEnabled())
    return;
  DEFINE_STATIC_LOCAL(UserLevelMemoryPressureSignalGenerator, generator,
                      (std::move(task_runner)));
  (void)generator;
}

UserLevelMemoryPressureSignalGenerator::UserLevelMemoryPressureSignalGenerator(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : UserLevelMemoryPressureSignalGenerator(
          std::move(task_runner),
          base::DefaultTickClock::GetInstance()) {}

UserLevelMemoryPressureSignalGenerator::UserLevelMemoryPressureSignalGenerator(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::TickClock* clock)
    : memory_threshold_mb_(MemoryThresholdParam()),
      minimum_interval_(MinimumIntervalSeconds()),
      delayed_report_timer_(
          std::move(task_runner),
          this,
          &UserLevelMemoryPressureSignalGenerator::OnTimerFired),
      clock_(clock) {
  DCHECK(base::FeatureList::IsEnabled(
      blink::features::kUserLevelMemoryPressureSignal));
  DCHECK(!std::isinf(memory_threshold_mb_));

  MemoryUsageMonitor::Instance().AddObserver(this);
  ThreadScheduler::Current()->ToMainThreadScheduler()->AddRAILModeObserver(
      this);
}

UserLevelMemoryPressureSignalGenerator::
    ~UserLevelMemoryPressureSignalGenerator() {
  MemoryUsageMonitor::Instance().RemoveObserver(this);
  ThreadScheduler::Current()->ToMainThreadScheduler()->RemoveRAILModeObserver(
      this);
}

void UserLevelMemoryPressureSignalGenerator::OnRAILModeChanged(
    RAILMode rail_mode) {
  is_loading_ = rail_mode == RAILMode::kLoad;
}

void UserLevelMemoryPressureSignalGenerator::OnMemoryPing(MemoryUsage usage) {
  // Disabled during loading as we don't want to purge caches that has just been
  // created.
  if (is_loading_)
    return;
  if (usage.private_footprint_bytes / 1024 / 1024 < memory_threshold_mb_)
    return;
  base::TimeDelta elapsed = clock_->NowTicks() - last_generated_;
  if (elapsed >= MinimumIntervalSeconds())
    Generate(usage);
}

void UserLevelMemoryPressureSignalGenerator::Generate(MemoryUsage usage) {
  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.UserLevelMemoryPressureSignal."
      "RendererPrivateMemoryFootprintBefore",
      base::saturated_cast<base::Histogram::Sample>(
          usage.private_footprint_bytes / 1024 / 1024));

  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  last_generated_ = clock_->NowTicks();

  delayed_report_timer_.StartOneShot(base::Seconds(10), FROM_HERE);
}

void UserLevelMemoryPressureSignalGenerator::OnTimerFired(TimerBase*) {
  MemoryUsage usage = MemoryUsageMonitor::Instance().GetCurrentMemoryUsage();
  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.UserLevelMemoryPressureSignal."
      "RendererPrivateMemoryFootprintAfter",
      base::saturated_cast<base::Histogram::Sample>(
          usage.private_footprint_bytes / 1024 / 1024));
}

}  // namespace blink
