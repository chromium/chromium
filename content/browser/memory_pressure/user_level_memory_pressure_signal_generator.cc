// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_pressure/user_level_memory_pressure_signal_generator.h"

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

#include "base/android/child_process_binding_types.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "content/browser/child_process_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/user_level_memory_pressure_signal_features.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/child_process_data.h"

namespace content {

namespace {
constexpr base::TimeDelta kFirstMeasurementInterval = base::Minutes(1);
constexpr base::TimeDelta kDefaultMeasurementInterval = base::Seconds(4);

// Time interval between measuring total private memory footprint.
base::TimeDelta MeasurementIntervalFor3GbDevices() {
  static const base::FeatureParam<base::TimeDelta> kMeasurementInterval{
      &features::kUserLevelMemoryPressureSignalOn3GbDevices,
      "measurement_interval", kDefaultMeasurementInterval};
  return kMeasurementInterval.Get();
}

base::TimeDelta MeasurementIntervalFor4GbDevices() {
  return kDefaultMeasurementInterval;
}

base::TimeDelta MeasurementIntervalFor6GbDevices() {
  return kDefaultMeasurementInterval;
}

// The memory threshold: 738 was selected at around the 99th percentile of
// the Memory.Total.PrivateMemoryFootprint reported by Android devices whose
// system memory were 3GB.
constexpr base::ByteCount kMemoryThresholdOf3GbDevices = base::MiB(738);

base::ByteCount MemoryThresholdParamFor3GbDevices() {
  static const base::FeatureParam<int> kMemoryThresholdParam{
      &features::kUserLevelMemoryPressureSignalOn3GbDevices,
      "memory_threshold_mb", kMemoryThresholdOf3GbDevices.InMiB()};
  return base::MiB(kMemoryThresholdParam.Get());
}

// The memory threshold: 458 was selected at around the 99th percentile of
// the Memory.Total.PrivateMemoryFootprint reported by Android devices whose
// system memory were 4GB.
constexpr base::ByteCount kMemoryThresholdOf4GbDevices = base::MiB(458);

base::ByteCount MemoryThresholdParamFor4GbDevices() {
  return kMemoryThresholdOf4GbDevices;
}

// The memory threshold: 494 was selected at around the 99th percentile of
// the Memory.Total.PrivateMemoryFootprint reported by Android devices whose
// system memory were 6GB.
constexpr base::ByteCount kMemoryThresholdOf6GbDevices = base::MiB(494);

base::ByteCount MemoryThresholdParamFor6GbDevices() {
  return kMemoryThresholdOf6GbDevices;
}

}  // namespace

// static
void UserLevelMemoryPressureSignalGenerator::Initialize() {
  // The metrics only feature will override the memory pressure signal features
  // on all devices to determine the most suitable memory heuristics. Memory
  // pressure signals will not be sent in the experiment group.
  if (base::FeatureList::IsEnabled(
          features::kUserLevelMemoryPressureSignalMetricsOnly)) {
    UserLevelMemoryPressureSignalGenerator::Get().StartMetricsCollection();
    return;
  }

  if (features::IsUserLevelMemoryPressureSignalEnabledOn3GbDevices()) {
    UserLevelMemoryPressureSignalGenerator::Get().Start(
        MemoryThresholdParamFor3GbDevices(), MeasurementIntervalFor3GbDevices(),
        features::MinUserMemoryPressureIntervalOn3GbDevices());
    return;
  }

  if (features::IsUserLevelMemoryPressureSignalEnabledOn4GbDevices()) {
    UserLevelMemoryPressureSignalGenerator::Get().Start(
        MemoryThresholdParamFor4GbDevices(), MeasurementIntervalFor4GbDevices(),
        features::MinUserMemoryPressureIntervalOn4GbDevices());
    return;
  }

  if (features::IsUserLevelMemoryPressureSignalEnabledOn6GbDevices()) {
    UserLevelMemoryPressureSignalGenerator::Get().Start(
        MemoryThresholdParamFor6GbDevices(), MeasurementIntervalFor6GbDevices(),
        features::MinUserMemoryPressureIntervalOn6GbDevices());
    return;
  }

  // No group defined for >6 GB devices.
}

// static
std::optional<UserLevelMemoryPressureMetrics>
UserLevelMemoryPressureSignalGenerator::GetLatestMemoryMetrics() {
  return Get().latest_metrics_;
}

// static
UserLevelMemoryPressureSignalGenerator&
UserLevelMemoryPressureSignalGenerator::Get() {
  static base::NoDestructor<UserLevelMemoryPressureSignalGenerator> instance;
  return *instance.get();
}

UserLevelMemoryPressureSignalGenerator::
    UserLevelMemoryPressureSignalGenerator() = default;
UserLevelMemoryPressureSignalGenerator::
    ~UserLevelMemoryPressureSignalGenerator() = default;

void UserLevelMemoryPressureSignalGenerator::Start(
    base::ByteCount memory_threshold,
    base::TimeDelta measure_interval,
    base::TimeDelta minimum_interval) {
  memory_threshold_ = memory_threshold;
  measure_interval_ = measure_interval;
  minimum_interval_ = minimum_interval;
  UserLevelMemoryPressureSignalGenerator::Get().StartPeriodicTimer(
      kFirstMeasurementInterval);
}

void UserLevelMemoryPressureSignalGenerator::StartMetricsCollection() {
  periodic_measuring_timer_.Start(
      FROM_HERE, kDefaultMeasurementInterval,
      base::BindRepeating(
          &UserLevelMemoryPressureSignalGenerator::CollectMemoryMetrics,
          base::Unretained(this)));
}

void UserLevelMemoryPressureSignalGenerator::CollectMemoryMetrics() {
  base::SystemMemoryInfo meminfo;
  base::GetSystemMemoryInfo(&meminfo);

  int total_process_count = 0;
  int visible_renderer_count = 0;

  for (BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    total_process_count++;
  }

  for (RenderProcessHost::iterator iter = RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    total_process_count++;
    RenderProcessHost* host = iter.GetCurrentValue();
    if (host && host->IsInitializedAndNotDead() &&
        host->GetProcess().IsValid() &&
        host->GetEffectiveChildBindingState() >=
            base::android::ChildBindingState::VISIBLE) {
      visible_renderer_count++;
    }
  }

  latest_metrics_ = UserLevelMemoryPressureMetrics{
      .total_private_footprint =
          GetTotalPrivateFootprintVisibleOrHigherPriorityRenderers(),
      .available_memory = meminfo.available,
      .total_process_count = total_process_count,
      .visible_renderer_count = visible_renderer_count,
  };
}

void UserLevelMemoryPressureSignalGenerator::OnTimerFired() {
  base::TimeDelta interval = measure_interval_;
  base::ByteCount total_pmf =
      GetTotalPrivateFootprintVisibleOrHigherPriorityRenderers();

  if (total_pmf > memory_threshold_) {
    NotifyMemoryPressure();
    interval = minimum_interval_;

    ReportBeforeAfterMetrics(total_pmf, "Before");
    StartReportingTimer();
  }

  StartPeriodicTimer(interval);
}

void UserLevelMemoryPressureSignalGenerator::StartPeriodicTimer(
    base::TimeDelta interval) {
  // Don't try to start the timer in tests that don't support it.
  if (!base::SequencedTaskRunner::HasCurrentDefault()) {
    return;
  }
  periodic_measuring_timer_.Start(
      FROM_HERE, interval,
      base::BindOnce(&UserLevelMemoryPressureSignalGenerator::OnTimerFired,
                     // Unretained is safe because |this| owns this timer.
                     base::Unretained(this)));
}

void UserLevelMemoryPressureSignalGenerator::StartReportingTimer() {
  // Don't try to start the timer in tests that don't support it.
  if (!base::SequencedTaskRunner::HasCurrentDefault()) {
    return;
  }
  delayed_report_timer_.Start(
      FROM_HERE, base::Seconds(10),
      base::BindOnce(
          &UserLevelMemoryPressureSignalGenerator::OnReportingTimerFired,
          base::Unretained(this)));
}

void UserLevelMemoryPressureSignalGenerator::OnReportingTimerFired() {
  base::ByteCount total_pmf =
      GetTotalPrivateFootprintVisibleOrHigherPriorityRenderers();
  ReportBeforeAfterMetrics(total_pmf, "After");
}

// static
base::ByteCount UserLevelMemoryPressureSignalGenerator::
    GetTotalPrivateFootprintVisibleOrHigherPriorityRenderers() {
  base::ByteCount total_pmf_visible_or_higher_priority_renderers_bytes =
      base::ByteCount(0u);

  auto add_process_private_footprint = [&](base::ByteCount& pmf,
                                           const base::Process& process) {
    if (process.IsValid()) {
      pmf += GetPrivateFootprint(process).value_or(base::ByteCount(0));
    }
  };

  // Measure private memory footprint of browser process
  add_process_private_footprint(
      total_pmf_visible_or_higher_priority_renderers_bytes,
      base::Process::Current());

  // Measure private memory footprints of GPU process and Utility processes.
  // Since GPU process uses the same user id as the browser process (android),
  // the browser process can measure the GPU's private memory footprint.
  // However, regarding the utility processes, their user ids are different.
  // So because of the hidepid=2 mount option, the browser process cannot
  // measure the private memory footprints of the utility processes.
  // TODO(crbug.com/40248151): measure the private memory footprints of
  // the utility processes correctly.
  for (BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    add_process_private_footprint(
        total_pmf_visible_or_higher_priority_renderers_bytes,
        iter.GetData().GetProcess());
  }

  // Measure private memory footprints of renderer processes with visible
  // or higher priority. Since the renderer processes with invisible or lower
  // priority will be cleaned up by Android OS, this pressure signal feature
  // doesn't need to take care of them.
  for (RenderProcessHost::iterator iter = RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    RenderProcessHost* host = iter.GetCurrentValue();
    if (!host || !host->IsInitializedAndNotDead())
      continue;

    const base::Process& process = host->GetProcess();
    if (!process.IsValid())
      continue;

    // Ignore renderer processes with invisible or lower priority.
    if (host->GetEffectiveChildBindingState() <
        base::android::ChildBindingState::VISIBLE) {
      continue;
    }

    // Because of the "hidepid=2" mount option for /proc on Android,
    // the browser process cannot open /proc/{render process pid}/maps and
    // status, i.e. no such file or directory. So each renderer process
    // provides its private memory footprint for the browser process and
    // the browser process gets the (cached) value via RenderProcessHostImpl.
    total_pmf_visible_or_higher_priority_renderers_bytes += base::ByteCount(
        static_cast<RenderProcessHostImpl*>(host)->GetPrivateMemoryFootprint());
  }

  return total_pmf_visible_or_higher_priority_renderers_bytes;
}

// static
void UserLevelMemoryPressureSignalGenerator::NotifyMemoryPressure() {
  // Notifies GPU process and Utility processes.
  for (BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    if (!iter.GetData().GetProcess().IsValid())
      continue;

    ChildProcessHostImpl* host =
        static_cast<ChildProcessHostImpl*>(iter.GetHost());
    host->NotifyMemoryPressureToChildProcess(
        base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  }

  // Notifies renderer processes.
  for (RenderProcessHost::iterator iter = RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    RenderProcessHost* host = iter.GetCurrentValue();
    if (!host || !host->IsInitializedAndNotDead())
      continue;
    if (!host->GetProcess().IsValid())
      continue;

    static_cast<RenderProcessHostImpl*>(host)->NotifyMemoryPressureToRenderer(
        base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  }

  // Notifies browser process.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL);
}

// static
void UserLevelMemoryPressureSignalGenerator::ReportBeforeAfterMetrics(
    base::ByteCount total_pmf_visible_or_higher_priority_renderers,
    const char* suffix_name) {
  std::string metric_name_total_pmf_visible_or_higher_priority_renderers =
      base::StringPrintf(
          "Memory.Experimental.UserLevelMemoryPressureSignal."
          "TotalPrivateMemoryFootprintVisibleOrHigherPriorityRenderers%s",
          suffix_name);
  base::UmaHistogramMemoryLargeMB(
      metric_name_total_pmf_visible_or_higher_priority_renderers,
      total_pmf_visible_or_higher_priority_renderers);
}

namespace {

// TODO(crbug.com/40248151): if this feature is approved, refactor the duplicate
// code under //third_party/blink/renderer/controller. If not approved,
// remove the code as soon as possible.
std::optional<base::ByteCount> CalculateProcessMemoryFootprint(
    base::File& statm_file,
    base::File& status_file) {
  // Get total resident and shared sizes from statm file.
  static size_t page_size = getpagesize();
  uint64_t resident_pages = 0;
  uint64_t shared_pages = 0;
  uint64_t vm_size_pages = 0;
  uint64_t swap_footprint = 0;
  constexpr uint32_t kMaxLineSize = 4096;
  char line[kMaxLineSize];

  int n = UNSAFE_TODO(statm_file.ReadAtCurrentPos(line, sizeof(line) - 1));
  if (n <= 0)
    return std::nullopt;
  UNSAFE_TODO(line[n]) = '\0';

  int num_scanned =
      UNSAFE_TODO(sscanf(line, "%" SCNu64 " %" SCNu64 " %" SCNu64,
                         &vm_size_pages, &resident_pages, &shared_pages));
  if (num_scanned != 3)
    return std::nullopt;

  // Get swap size from status file. The format is: VmSwap :  10 kB.
  n = UNSAFE_TODO(status_file.ReadAtCurrentPos(line, sizeof(line) - 1));
  if (n <= 0)
    return std::nullopt;
  UNSAFE_TODO(line[n]) = '\0';

  char* swap_line = UNSAFE_TODO(strstr(line, "VmSwap"));
  if (!swap_line)
    return std::nullopt;
  num_scanned =
      UNSAFE_TODO(sscanf(swap_line, "VmSwap: %" SCNu64 " kB", &swap_footprint));
  if (num_scanned != 1)
    return std::nullopt;

  swap_footprint *= 1024;
  return base::ByteCount((resident_pages - shared_pages) * page_size +
                         swap_footprint);
}

}  // namespace

// static
std::optional<base::ByteCount>
UserLevelMemoryPressureSignalGenerator::GetPrivateFootprint(
    const base::Process& process) {
  // ScopedAllowBlocking is required to use base::File, but /proc/{pid}/status
  // and /proc/{pid}/statm are not regular files. For example, regarding linux,
  // proc_pid_statm() defined in fs/proc/array.c is invoked when reading
  // /proc/{pid}/statm. proc_pid_statm() gets task information and directly
  // writes the information into the given seq_file. This is different from
  // regular file operations.
  base::ScopedAllowBlocking allow_blocking;

  base::FilePath proc_pid_dir =
      base::FilePath("/proc").Append(base::NumberToString(process.Pid()));
  base::File status_file(
      proc_pid_dir.Append("status"),
      base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
  base::File statm_file(
      proc_pid_dir.Append("statm"),
      base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
  if (!status_file.IsValid() || !statm_file.IsValid())
    return std::nullopt;

  return CalculateProcessMemoryFootprint(statm_file, status_file);
}

}  // namespace content
