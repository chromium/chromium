// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/pressure_test_support.h"

#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/threading/scoped_blocking_call.h"
#include "services/device/compute_pressure/pressure_sample.h"

namespace device {

constexpr PressureSample FakeCpuProbe::kInitialSample;

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

PressureSample FakeCpuProbe::LastSample() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock auto_lock(lock_);
  return last_sample_;
}

void FakeCpuProbe::SetLastSample(PressureSample sample) {
  base::AutoLock auto_lock(lock_);
  last_sample_ = sample;
}

}  // namespace device
