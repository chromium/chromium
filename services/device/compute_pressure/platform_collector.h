// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_PLATFORM_COLLECTOR_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_PLATFORM_COLLECTOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"
#include "services/device/compute_pressure/pressure_sample.h"
#include "services/device/public/mojom/pressure_update.mojom-shared.h"

namespace device {

class CpuProbe;

// Drives the process that measures the compute pressure state.
//
// Responsible for invoking the platform-specific measurement code in a CpuProbe
// implementation at regular intervals, and for straddling between sequences to
// meet the CpuProbe requirements.
//
// Instances are not thread-safe. They must be used on the same sequence.
//
// The instance is owned by a PressureManagerImpl.
class PlatformCollector {
 public:
  // The caller must ensure that `cpu_probe` outlives this instance. Production
  // code should pass CpuProbe::Create().
  //
  // `sampling_interval` is exposed to avoid idling in tests. Production code
  // should pass `kDefaultSamplingInterval`.
  //
  // `sampling_callback` is called regularly every `sampling_interval` while
  // the collector is started.
  PlatformCollector(
      std::unique_ptr<CpuProbe> cpu_probe,
      base::TimeDelta sampling_interval,
      base::RepeatingCallback<void(mojom::PressureState)> sampling_callback);
  ~PlatformCollector();

  bool has_probe() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return probe_ != nullptr;
  }

  // Idempotent. Must only be called if has_probe() returns true.
  //
  // After this method is called, the sampling callback passsed to the
  // constructor will be called regularly.
  void EnsureStarted();

  // Idempotent.
  void Stop();

  // Used by tests that pass in a FakeCpuProbe that they need to direct.
  CpuProbe* cpu_probe_for_testing() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return probe_.get();
  }

 private:
  // Called periodically while the collector is running.
  void UpdateProbe();
  // Called after the CpuProbe is updated.
  void DidUpdateProbe(PressureSample);

  // Calculate PressureState based on PressureSample.
  mojom::PressureState CalculateState(PressureSample) const;

  SEQUENCE_CHECKER(sequence_checker_);

  // A sequence that can execute methods on the CpuProbe instance.
  const scoped_refptr<base::SequencedTaskRunner> probe_task_runner_;

  // Methods on the underlying probe must be executed on `probe_task_runner_`.
  //
  // Constant between the collector's construction and destruction.
  std::unique_ptr<CpuProbe> probe_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Drives repeated sampling.
  base::RepeatingTimer timer_ GUARDED_BY_CONTEXT(sequence_checker_);
  const base::TimeDelta sampling_interval_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Called with each sample reading.
  base::RepeatingCallback<void(mojom::PressureState)> sampling_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // True if the CpuProbe state will be reported after the next update.
  //
  // The PressureSample reported by many CpuProbe implementations relies
  // on the differences observed between two Update() calls. For this reason,
  // the PressureSample reported after a first Update() call is not
  // reported via `sampling_callback_`.
  bool got_probe_baseline_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  base::WeakPtrFactory<PlatformCollector> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_PLATFORM_COLLECTOR_H_
