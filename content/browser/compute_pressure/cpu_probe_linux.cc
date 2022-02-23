// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/cpu_probe_linux.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/cpu.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/format_macros.h"
#include "base/notreached.h"
#include "base/numerics/safe_math.h"
#include "base/sequence_checker.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/types/pass_key.h"
#include "content/browser/compute_pressure/cpu_core_speed_info.h"
#include "content/browser/compute_pressure/cpuid_base_frequency_parser.h"
#include "content/browser/compute_pressure/procfs_stat_cpu_parser.h"
#include "content/browser/compute_pressure/sysfs_cpufreq_core_parser.h"
#include "third_party/re2/src/re2/re2.h"

namespace content {

// static
std::unique_ptr<CpuProbeLinux> CpuProbeLinux::Create() {
  return std::make_unique<CpuProbeLinux>(
      base::FilePath(ProcfsStatCpuParser::kProcfsStatPath),
      SysfsCpufreqCoreParser::kSysfsCpuPath, base::PassKey<CpuProbeLinux>());
}

// static
std::unique_ptr<CpuProbeLinux> CpuProbeLinux::CreateForTesting(
    base::FilePath procfs_stat_path,
    const base::FilePath::CharType* sysfs_root_path) {
  return std::make_unique<CpuProbeLinux>(std::move(procfs_stat_path),
                                         sysfs_root_path,
                                         base::PassKey<CpuProbeLinux>());
}

CpuProbeLinux::CpuProbeLinux(base::FilePath procfs_stat_path,
                             const base::FilePath::CharType* sysfs_root_path,
                             base::PassKey<CpuProbeLinux>)
    : stat_parser_(std::move(procfs_stat_path)),
      sysfs_root_path_(sysfs_root_path),
      last_sample_(CpuProbe::kUnsupportedValue) {
  DCHECK(sysfs_root_path_);

  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CpuProbeLinux::~CpuProbeLinux() = default;

void CpuProbeLinux::Update() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  stat_parser_.Update();
  const std::vector<ProcfsStatCpuParser::CoreTimes>& core_times =
      stat_parser_.core_times();

  double utilization_sum = 0.0, speed_sum = 0.0;
  int utilization_cores = 0, speed_cores = 0;
  for (size_t i = 0; i < core_times.size(); ++i) {
    DCHECK_GE(last_core_times_.size(), i);
    DCHECK_EQ(last_core_times_.size(), cpufreq_parsers_.size());

    const ProcfsStatCpuParser::CoreTimes& current_core_times = core_times[i];

    if (last_core_times_.size() == i) {
      InitializeCore(static_cast<int>(i), current_core_times);
      continue;
    }

    double core_utilization =
        current_core_times.TimeUtilization(last_core_times_[i]);
    if (core_utilization >= 0) {
      // Only overwrite `last_core_times_` if the /proc/stat counters are
      // monotonically increasing. Otherwise, discard the measurement.
      last_core_times_[i] = current_core_times;

      utilization_sum += core_utilization;
      ++utilization_cores;

      double core_speed = CoreSpeed(*cpufreq_parsers_[i]);
      if (core_speed >= 0) {
        speed_sum += core_speed;
        ++speed_cores;
      }
    }
  }

  if (utilization_cores > 0 && speed_cores > 0) {
    last_sample_.cpu_utilization = utilization_sum / utilization_cores;
    last_sample_.cpu_speed = speed_sum / speed_cores;
  } else {
    last_sample_ = CpuProbe::kUnsupportedValue;
  }
}

ComputePressureSample CpuProbeLinux::LastSample() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return last_sample_;
}

constexpr int64_t CpuProbeLinux::kUninitializedCpuidBaseFrequency;

void CpuProbeLinux::InitializeCore(
    int core_index,
    const ProcfsStatCpuParser::CoreTimes& initial_core_times) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(core_index, 0);
  DCHECK_EQ(last_core_times_.size(), static_cast<size_t>(core_index));
  DCHECK_EQ(cpufreq_parsers_.size(), static_cast<size_t>(core_index));

  if (core_index == 0)
    Initialize();

  last_core_times_.push_back(initial_core_times);
  cpufreq_parsers_.emplace_back(std::make_unique<SysfsCpufreqCoreParser>(
      SysfsCpufreqCoreParser::CorePath(core_index, sysfs_root_path_)));
}

void CpuProbeLinux::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(cpuid_base_frequency_, kUninitializedCpuidBaseFrequency)
      << __func__ << " already called";

  base::CPU cpu;
  cpuid_base_frequency_ = ParseBaseFrequencyFromCpuid(cpu.cpu_brand());
  DCHECK_GE(cpuid_base_frequency_, -1);
}

double CpuProbeLinux::CoreSpeed(SysfsCpufreqCoreParser& cpufreq_parser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CpuCoreSpeedInfo core_speed_info;
  core_speed_info.max_frequency = cpufreq_parser.ReadMaxFrequency();
  core_speed_info.min_frequency = cpufreq_parser.ReadMinFrequency();
  core_speed_info.current_frequency = cpufreq_parser.ReadCurrentFrequency();
  core_speed_info.base_frequency = cpufreq_parser.ReadBaseFrequency();
  if (core_speed_info.base_frequency == -1)
    core_speed_info.base_frequency = cpuid_base_frequency_;

  if (!core_speed_info.IsValid())
    return -1;

  return core_speed_info.NormalizedSpeed();
}

}  // namespace content
