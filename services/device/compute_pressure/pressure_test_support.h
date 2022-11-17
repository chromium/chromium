// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_PRESSURE_TEST_SUPPORT_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_PRESSURE_TEST_SUPPORT_H_

#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "services/device/compute_pressure/cpu_probe.h"
#include "services/device/compute_pressure/pressure_sample.h"

namespace device {

// Test double for CpuProbe that always returns a predetermined value.
class FakeCpuProbe : public CpuProbe {
 public:
  // Value returned by LastSample() if SetLastSample() is not called.
  static constexpr PressureSample kInitialSample{0.42};

  FakeCpuProbe();
  ~FakeCpuProbe() override;

  // CpuProbe implementation.
  void Update() override;
  PressureSample LastSample() override;

  // Can be called from any thread.
  void SetLastSample(PressureSample sample);

 private:
  // Bound to the sequence for Update() and LastSample().
  SEQUENCE_CHECKER(sequence_checker_);

  base::Lock lock_;
  PressureSample last_sample_ GUARDED_BY_CONTEXT(lock_);
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_PRESSURE_TEST_SUPPORT_H_
