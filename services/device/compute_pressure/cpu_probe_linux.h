// Copyright 2021 The Chromium Authors
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
#include "services/device/compute_pressure/core_times.h"
#include "services/device/compute_pressure/cpu_probe.h"
#include "services/device/compute_pressure/pressure_sample.h"
#include "services/device/compute_pressure/procfs_stat_cpu_parser.h"

namespace device {

class CpuProbeLinux : public CpuProbe {
 public:
  // Factory method for production instances.
  static std::unique_ptr<CpuProbeLinux> Create();

  // Factory method with dependency injection support for testing.
  static std::unique_ptr<CpuProbeLinux> CreateForTesting(
      base::FilePath procfs_stat_path);

  ~CpuProbeLinux() override;

  CpuProbeLinux(const CpuProbeLinux&) = delete;
  CpuProbeLinux& operator=(const CpuProbeLinux&) = delete;

  // CpuProbe implementation.
  void Update() override;
  PressureSample LastSample() override;

 private:
  explicit CpuProbeLinux(base::FilePath procfs_stat_path);

  // Called when a core is seen the first time in /proc/stat.
  //
  // For most systems, the cores listed in /proc/stat are static. However, it is
  // theoretically possible for cores to go online and offline.
  void InitializeCore(size_t core_index, const CoreTimes& initial_core_times);

  SEQUENCE_CHECKER(sequence_checker_);

  // /proc/stat parser. Used to derive CPU utilization.
  ProcfsStatCpuParser stat_parser_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Most recent per-core times from /proc/stat.
  std::vector<CoreTimes> last_per_core_times_
      GUARDED_BY_CONTEXT(sequence_checker_);

  PressureSample last_sample_ GUARDED_BY_CONTEXT(sequence_checker_) =
      kUnsupportedValue;
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_LINUX_H_
