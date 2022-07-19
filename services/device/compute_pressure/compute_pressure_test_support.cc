// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/compute_pressure_test_support.h"

#include <ostream>

#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/threading/scoped_blocking_call.h"
#include "services/device/compute_pressure/compute_pressure_sample.h"
#include "services/device/compute_pressure/cpu_core_speed_info.h"

namespace device {

bool operator==(const ComputePressureSample& lhs,
                const ComputePressureSample& rhs) noexcept {
  return std::make_pair(lhs.cpu_utilization, lhs.cpu_speed) ==
         std::make_pair(rhs.cpu_utilization, rhs.cpu_speed);
}

std::ostream& operator<<(std::ostream& os,
                         const ComputePressureSample& sample) {
  os << "[utilization: " << sample.cpu_utilization
     << " speed: " << sample.cpu_speed << "]";
  return os;
}

std::ostream& operator<<(std::ostream& os, const CpuCoreSpeedInfo& info) {
  os << "[min: " << info.min_frequency << " max: " << info.max_frequency
     << " base: " << info.base_frequency
     << " current: " << info.current_frequency << "]";
  return os;
}

constexpr ComputePressureSample FakeCpuProbe::kInitialSample;

FakeCpuProbe::FakeCpuProbe() : last_sample_(kInitialSample) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FakeCpuProbe::~FakeCpuProbe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FakeCpuProbe::Update() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // In DCHECKed builds, the ScopedBlockingCall ensures that Update() is only
  // called on sequences where I/O is allowed.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
}

ComputePressureSample FakeCpuProbe::LastSample() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock auto_lock(lock_);
  return last_sample_;
}

void FakeCpuProbe::SetLastSample(ComputePressureSample sample) {
  base::AutoLock auto_lock(lock_);
  last_sample_ = sample;
}

}  // namespace device
