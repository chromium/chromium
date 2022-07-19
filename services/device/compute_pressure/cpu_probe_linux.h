// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_LINUX_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_LINUX_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "services/device/compute_pressure/compute_pressure_sample.h"
#include "services/device/compute_pressure/cpu_probe.h"
#include "services/device/compute_pressure/procfs_stat_cpu_parser.h"
#include "services/device/compute_pressure/sysfs_cpufreq_core_parser.h"

namespace device {

class CpuProbeLinux : public CpuProbe {
 public:
  // Factory method for production instances.
  static std::unique_ptr<CpuProbeLinux> Create();

  // Factory method with dependency injection support for testing.
  //
  // The caller is responsible for keeping `sysfs_root_path` alive while the
  // newly created instance is alive.
  static std::unique_ptr<CpuProbeLinux> CreateForTesting(
      base::FilePath procfs_stat_path,
      const base::FilePath::CharType* sysfs_root_path);

  ~CpuProbeLinux() override;

  CpuProbeLinux(const CpuProbeLinux&) = delete;
  CpuProbeLinux& operator=(const CpuProbeLinux&) = delete;

  // CpuProbe implementation.
  void Update() override;
  ComputePressureSample LastSample() override;

 private:
  CpuProbeLinux(base::FilePath procfs_stat_path,
                const base::FilePath::CharType* sysfs_root_path);

  // Initial value for `cpuid_base_frequency_`. This must be different from all
  // valid return values of ParseBaseFrequencyFromCpuid().
  static constexpr int64_t kUninitializedCpuidBaseFrequency = -2;

  // Called when a core is seen the first time in /proc/stat.
  //
  // For most systems, the cores listed in /proc/stat are static. However, it is
  // theoretically possible for cores to go online and offline.
  void InitializeCore(int core_index,
                      const ProcfsStatCpuParser::CoreTimes& initial_core_times);

  // One-time initialization.
  void Initialize();

  // Computes the normalized speed of a single core.
  double CoreSpeed(SysfsCpufreqCoreParser& cpufreq_parser);

  SEQUENCE_CHECKER(sequence_checker_);

  // /proc/stat parser. Used to derive CPU utilization.
  ProcfsStatCpuParser stat_parser_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Most recent per-core times from /proc/stat.
  std::vector<ProcfsStatCpuParser::CoreTimes> last_core_times_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Per-core CPUfreq info parsers. Used to derive CPU speed.
  //
  // This vector's size is kept in sync with `last_core_times_`.
  std::vector<std::unique_ptr<SysfsCpufreqCoreParser>> cpufreq_parsers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Used to instantiate CPUfreq parsers on-demand.
  const base::FilePath::CharType* const sysfs_root_path_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Base frequency parsed from the CPUID vendor string.
  //
  // This is used as a fallback, in case the CPUfreq driver does not report the
  // base frequency.
  int64_t cpuid_base_frequency_ GUARDED_BY_CONTEXT(sequence_checker_) =
      kUninitializedCpuidBaseFrequency;

  ComputePressureSample last_sample_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_LINUX_H_
