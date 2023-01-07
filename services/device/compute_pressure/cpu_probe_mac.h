// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_MAC_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_MAC_H_

#include <vector>

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "services/device/compute_pressure/core_times.h"
#include "services/device/compute_pressure/cpu_probe.h"
#include "services/device/compute_pressure/host_processor_info_scanner.h"
#include "services/device/compute_pressure/pressure_sample.h"

namespace device {

class CpuProbeMac : public CpuProbe {
 public:
  // Factory method for production instances.
  static std::unique_ptr<CpuProbeMac> Create();

  ~CpuProbeMac() override;

  CpuProbeMac(const CpuProbeMac&) = delete;
  CpuProbeMac& operator=(const CpuProbeMac&) = delete;

  // CpuProbe implementation.
  void Update() override;
  PressureSample LastSample() override;

 private:
  CpuProbeMac();

  // Called when a core is seen the first time.
  void InitializeCore(size_t core_index, const CoreTimes& initial_core_times);

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to derive CPU utilization.
  HostProcessorInfoScanner processor_info_scanner_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Most recent per-core times.
  std::vector<CoreTimes> last_per_core_times_
      GUARDED_BY_CONTEXT(sequence_checker_);

  PressureSample last_sample_ GUARDED_BY_CONTEXT(sequence_checker_) =
      kUnsupportedValue;
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_MAC_H_
