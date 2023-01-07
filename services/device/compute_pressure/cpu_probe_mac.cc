// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/cpu_probe_mac.h"

#include "base/check_op.h"
#include "base/memory/ptr_util.h"

namespace device {

// static
std::unique_ptr<CpuProbeMac> CpuProbeMac::Create() {
  return base::WrapUnique(new CpuProbeMac());
}

CpuProbeMac::CpuProbeMac() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CpuProbeMac::~CpuProbeMac() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CpuProbeMac::Update() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  processor_info_scanner_.Update();
  const std::vector<CoreTimes>& per_core_times =
      processor_info_scanner_.core_times();

  double utilization_sum = 0.0;
  int utilization_cores = 0;
  for (size_t i = 0; i < per_core_times.size(); ++i) {
    DCHECK_GE(last_per_core_times_.size(), i);

    const CoreTimes& core_times = per_core_times[i];

    if (last_per_core_times_.size() == i) {
      InitializeCore(i, core_times);
      continue;
    }

    double core_utilization =
        core_times.TimeUtilization(last_per_core_times_[i]);
    if (core_utilization >= 0) {
      // Only overwrite `last_per_core_times_` if the cpu time counters are
      // monotonically increasing. Otherwise, discard the measurement.
      last_per_core_times_[i] = core_times;

      utilization_sum += core_utilization;
      ++utilization_cores;
    }
  }

  if (utilization_cores > 0) {
    last_sample_.cpu_utilization = utilization_sum / utilization_cores;
  } else {
    last_sample_ = kUnsupportedValue;
  }
}

PressureSample CpuProbeMac::LastSample() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return last_sample_;
}

void CpuProbeMac::InitializeCore(size_t core_index,
                                 const CoreTimes& initial_core_times) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(last_per_core_times_.size(), core_index);

  last_per_core_times_.push_back(initial_core_times);
}

}  // namespace device
