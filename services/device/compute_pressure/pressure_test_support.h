// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_PRESSURE_TEST_SUPPORT_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_PRESSURE_TEST_SUPPORT_H_

#include <stdint.h>

#include <type_traits>

#include "base/functional/callback_forward.h"
#include "base/synchronization/lock.h"
#include "base/test/repeating_test_future.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "services/device/compute_pressure/cpu_probe.h"
#include "services/device/compute_pressure/pressure_sample.h"
#include "services/device/public/mojom/pressure_update.mojom-shared.h"

namespace device {

// Test double for platform specific CpuProbe that overrides
// OnPressureSampleAvailable() to get PressureSample.
template <typename T,
          typename = std::enable_if_t<std::is_base_of_v<CpuProbe, T>>>
class FakePlatformCpuProbe : public T {
 public:
  template <typename... Args>
  explicit FakePlatformCpuProbe(Args&&... args)
      : T(std::forward<Args>(args)...) {}
  ~FakePlatformCpuProbe() override = default;

  void OnPressureSampleAvailable(PressureSample sample) override {
    T::OnPressureSampleAvailable(sample);
    sample_.AddValue(std::move(sample));
  }

  PressureSample WaitForSample() { return sample_.Take(); }

 private:
  base::test::RepeatingTestFuture<PressureSample> sample_;
};

// Test double for CpuProbe that always returns a predetermined value.
class FakeCpuProbe : public CpuProbe {
 public:
  // Value returned by LastSample() if SetLastSample() is not called.
  static constexpr PressureSample kInitialSample{0.42};

  FakeCpuProbe(base::TimeDelta,
               base::RepeatingCallback<void(mojom::PressureState)>);
  ~FakeCpuProbe() override;

  // CpuProbe implementation.
  void Update() override;

  void OnUpdate();

  // Can be called from any thread.
  void SetLastSample(PressureSample sample);

 private:
  base::Lock lock_;
  PressureSample last_sample_ GUARDED_BY_CONTEXT(lock_);
};

// Test double for CpuProbe that produces a different value after every
// Update().
class StreamingCpuProbe : public CpuProbe {
 public:
  StreamingCpuProbe(base::TimeDelta,
                    base::RepeatingCallback<void(mojom::PressureState)>,
                    std::vector<PressureSample>,
                    base::OnceClosure);

  ~StreamingCpuProbe() override;

  // CpuProbe implementation.
  void Update() override;

  void OnUpdate();

 private:
  std::vector<PressureSample> samples_ GUARDED_BY_CONTEXT(sequence_checker_);
  size_t sample_index_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // This closure is called on a LastSample call after expected number of
  // samples has been taken by PressureSampler.
  base::OnceClosure callback_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_PRESSURE_TEST_SUPPORT_H_
