// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_pressure/user_level_memory_pressure_signal_generator.h"

#if BUILDFLAG(IS_ANDROID)
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>
#include <utility>
#include "base/android/child_process_binding_types.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/child_process_data.h"

namespace memory_pressure {

#if !defined(ARCH_CPU_64_BITS)

namespace features {

BASE_FEATURE(kUserLevelMemoryPressureSignalOn4GbDevices,
             "UserLevelMemoryPressureSignalOn4GbDevices",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kUserLevelMemoryPressureSignalOn6GbDevices,
             "UserLevelMemoryPressureSignalOn6GbDevices",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace features

namespace {

constexpr base::TimeDelta kDefaultMeasurementInterval = base::Seconds(1);
constexpr base::TimeDelta kDefaultMinimumInterval = base::Minutes(10);

// Time interval between measuring total private memory footprint.
base::TimeDelta MeasurementIntervalFor4GbDevices() {
  static const base::FeatureParam<base::TimeDelta> kMeasurementInterval{
      &features::kUserLevelMemoryPressureSignalOn4GbDevices,
      "measurement_interval", kDefaultMeasurementInterval};
  return kMeasurementInterval.Get();
}

base::TimeDelta MeasurementIntervalFor6GbDevices() {
  static const base::FeatureParam<base::TimeDelta> kMeasurementInterval{
      &features::kUserLevelMemoryPressureSignalOn6GbDevices,
      "measurement_interval", kDefaultMeasurementInterval};
  return kMeasurementInterval.Get();
}

// Minimum time interval between generated memory pressure signals.
base::TimeDelta MinimumIntervalFor4GbDevices() {
  static const base::FeatureParam<base::TimeDelta> kMinimumInterval{
      &features::kUserLevelMemoryPressureSignalOn4GbDevices, "minimum_interval",
      kDefaultMinimumInterval};
  return kMinimumInterval.Get();
}

base::TimeDelta MinimumIntervalFor6GbDevices() {
  static const base::FeatureParam<base::TimeDelta> kMinimumInterval{
      &features::kUserLevelMemoryPressureSignalOn6GbDevices, "minimum_interval",
      kDefaultMinimumInterval};
  return kMinimumInterval.Get();
}

constexpr uint64_t k1MB = 1024ull * 1024;
constexpr size_t kDefaultMemoryThresholdMB = 485;

uint64_t MemoryThresholdParamFor4GbDevices() {
  static const base::FeatureParam<int> kMemoryThresholdParam{
      &features::kUserLevelMemoryPressureSignalOn4GbDevices,
      "memory_threshold_mb", kDefaultMemoryThresholdMB};
  return base::as_unsigned(kMemoryThresholdParam.Get()) * k1MB;
}

uint64_t MemoryThresholdParamFor6GbDevices() {
  static const base::FeatureParam<int> kMemoryThresholdParam{
      &features::kUserLevelMemoryPressureSignalOn6GbDevices,
      "memory_threshold_mb", kDefaultMemoryThresholdMB};
  return base::as_unsigned(kMemoryThresholdParam.Get()) * k1MB;
}

}  // namespace
#endif  // !defined(ARCH_CPU_64_BITS)

// static
void UserLevelMemoryPressureSignalGenerator::Initialize() {
#if !defined(ARCH_CPU_64_BITS)
  uint64_t physical_memory = base::SysInfo::AmountOfPhysicalMemory();
  constexpr uint64_t k1GB = 1024ull * k1MB;

  // Because of Android carveouts, AmountOfPhysicalMemory() returns smaller
  // than the actual memory size, So we will use a small lowerbound than 4GB
  // to discriminate real 4GB devices from lower memory ones.
  if (physical_memory < 3 * k1GB + 200 * k1MB) {
    // No experiment defined for low memory Android devices.
    return;
  }

  if (physical_memory <= 4 * k1GB) {
    if (base::FeatureList::IsEnabled(
            features::kUserLevelMemoryPressureSignalOn4GbDevices))
      UserLevelMemoryPressureSignalGenerator::Get().Start(
          MemoryThresholdParamFor4GbDevices(),
          MeasurementIntervalFor4GbDevices(), MinimumIntervalFor4GbDevices());
    return;
  }

  if (physical_memory <= 6 * k1GB) {
    if (base::FeatureList::IsEnabled(
            features::kUserLevelMemoryPressureSignalOn6GbDevices))
      UserLevelMemoryPressureSignalGenerator::Get().Start(
          MemoryThresholdParamFor6GbDevices(),
          MeasurementIntervalFor6GbDevices(), MinimumIntervalFor6GbDevices());
    return;
  }

  // No group defined for >6 GB devices.
#endif  // !defined(ARCH_CPU_64_BITS)
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
    uint64_t memory_threshold,
    base::TimeDelta measure_interval,
    base::TimeDelta minimum_interval) {
  memory_threshold_ = memory_threshold;
  measure_interval_ = measure_interval;
  minimum_interval_ = minimum_interval;
  UserLevelMemoryPressureSignalGenerator::Get().StartPeriodicTimer(
      measure_interval);
}
void UserLevelMemoryPressureSignalGenerator::OnTimerFired() {
  base::TimeDelta interval = measure_interval_;
  uint64_t total_private_footprint_bytes =
      GetTotalPrivateFootprintVisibleOrHigherPriorityRenderers();

  if (total_private_footprint_bytes > memory_threshold_) {
    NotifyMemoryPressure();
    interval = minimum_interval_;
  }

  StartPeriodicTimer(interval);
}

void UserLevelMemoryPressureSignalGenerator::StartPeriodicTimer(
    base::TimeDelta interval) {
  // Don't try to start the timer in tests that don't support it.
  if (!base::SequencedTaskRunnerHandle::IsSet())
    return;
  periodic_measuring_timer_.Start(
      FROM_HERE, interval,
      base::BindOnce(&UserLevelMemoryPressureSignalGenerator::OnTimerFired,
                     // Unretained is safe because |this| owns this timer.
                     base::Unretained(this)));
}

// static
uint64_t UserLevelMemoryPressureSignalGenerator::
    GetTotalPrivateFootprintVisibleOrHigherPriorityRenderers() {
  uint64_t total_private_footprint_bytes = 0u;

  auto add_process_private_footprint = [&](const base::Process& process) {
    if (process.IsValid()) {
      total_private_footprint_bytes += GetPrivateFootprint(process).value_or(0);
    }
  };

  // Measure private memory footprint of browser process
  add_process_private_footprint(base::Process::Current());

  // Measure private memory footprints of GPU process and Utility processes.
  for (content::BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    add_process_private_footprint(iter.GetData().GetProcess());
  }

  // Measure private memory footprints of renderer processes with visible
  // or higher priority. Since the renderer processes with invisible or lower
  // priority will be cleaned up by Android OS, this pressure signal feature
  // doesn't need to take care of them.
  for (content::RenderProcessHost::iterator iter =
           content::RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    content::RenderProcessHost* host = iter.GetCurrentValue();
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

    total_private_footprint_bytes += GetPrivateFootprint(process).value_or(0);
  }
  return total_private_footprint_bytes;
}

// static
void UserLevelMemoryPressureSignalGenerator::NotifyMemoryPressure() {
  // Notifies GPU process and Utility processes.
  for (content::BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    if (!iter.GetData().GetProcess().IsValid())
      continue;

    content::ChildProcessHostImpl* host =
        static_cast<content::ChildProcessHostImpl*>(iter.GetHost());
    host->NotifyMemoryPressureToChildProcess(
        base::MemoryPressureListener::MemoryPressureLevel::
            MEMORY_PRESSURE_LEVEL_CRITICAL);
  }

  // Notifies renderer processes.
  for (content::RenderProcessHost::iterator iter =
           content::RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    content::RenderProcessHost* host = iter.GetCurrentValue();
    if (!host || !host->IsInitializedAndNotDead())
      continue;
    if (!host->GetProcess().IsValid())
      continue;

    static_cast<content::RenderProcessHostImpl*>(host)
        ->NotifyMemoryPressureToRenderer(
            base::MemoryPressureListener::MemoryPressureLevel::
                MEMORY_PRESSURE_LEVEL_CRITICAL);
  }

  // Notifies browser process.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_CRITICAL);
}

namespace {

// TODO(crbug.com/1393282): if this feature is approved, refactor the duplicate
// code under //third_party/blink/renderer/controller. If not approved,
// remove the code as soon as possible.
absl::optional<uint64_t> CalculateProcessMemoryFootprint(
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

  int n = statm_file.ReadAtCurrentPos(line, sizeof(line) - 1);
  if (n <= 0)
    return absl::optional<size_t>();
  line[n] = '\0';

  int num_scanned = sscanf(line, "%" SCNu64 " %" SCNu64 " %" SCNu64,
                           &vm_size_pages, &resident_pages, &shared_pages);
  if (num_scanned != 3)
    return absl::optional<size_t>();

  // Get swap size from status file. The format is: VmSwap :  10 kB.
  n = status_file.ReadAtCurrentPos(line, sizeof(line) - 1);
  if (n <= 0)
    return absl::optional<size_t>();
  line[n] = '\0';

  char* swap_line = strstr(line, "VmSwap");
  if (!swap_line)
    return absl::optional<size_t>();
  num_scanned = sscanf(swap_line, "VmSwap: %" SCNu64 " kB", &swap_footprint);
  if (num_scanned != 1)
    return absl::optional<size_t>();

  swap_footprint *= 1024;
  return (resident_pages - shared_pages) * page_size + swap_footprint;
}

}  // namespace

// static
absl::optional<uint64_t>
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
    return absl::optional<size_t>();

  return CalculateProcessMemoryFootprint(statm_file, status_file);
}

}  // namespace memory_pressure

#endif  // BUILDFLAG(IS_ANDROID)
