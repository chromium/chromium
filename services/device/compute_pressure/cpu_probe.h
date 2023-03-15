// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"
#include "services/device/compute_pressure/pressure_sample.h"
#include "services/device/public/mojom/pressure_update.mojom-shared.h"

namespace device {

// Interface for retrieving the compute pressure state for CPU from the
// underlying OS at regular intervals.
//
// Operating systems differ in how they summarize the info needed to derive the
// compute pressure state. For example, the Linux kernel exposes CPU utilization
// as a summary over the device's entire uptime, while the Windows WMI exposes
// CPU utilization over the last second.
//
// This interface abstracts over the differences with a unified model where the
// implementation is responsible for integrating over the time between two
// Update() calls.
//
// This interface has rather strict requirements. This is because operating
// systems differ in requirements for accessing compute pressure information,
// and this interface expresses the union of all requirements.
//
// Instances are not thread-safe and should be used on the same sequence.
//
// The instance is owned by a PressureManagerImpl.
class CpuProbe {
 public:
  // Return this value when the implementation fails to get a result.
  static constexpr PressureSample kUnsupportedValue = {.cpu_utilization = 0.0};

  // Instantiates the CpuProbe subclass most suitable for the current platform.
  //
  // Returns nullptr if no suitable implementation exists.
  static std::unique_ptr<CpuProbe> Create(
      base::TimeDelta,
      base::RepeatingCallback<void(mojom::PressureState)>);

  CpuProbe(const CpuProbe&) = delete;
  CpuProbe& operator=(const CpuProbe&) = delete;

  virtual ~CpuProbe();

  // Idempotent.
  // Start the timer to retrieve the compute pressure state from the
  // underlying OS at specific time interval.
  void EnsureStarted();

  // Idempotent.
  // Stop the timer.
  void Stop();

 protected:
  // The constructor is intentionally only exposed to subclasses. Production
  // code must use the Create() factory method.
  CpuProbe(base::TimeDelta,
           base::RepeatingCallback<void(mojom::PressureState)>);

  // Called periodically while the CpuProbe is running.
  // This function can be overridden in tests to deal with `sample`.
  virtual void OnPressureSampleAvailable(PressureSample sample);

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  friend class PressureManagerImpl;

  // Implemented by subclasses to retrieve the compute pressure state for
  // different operating systems.
  virtual void Update() = 0;

  // Calculate PressureState based on PressureSample.
  mojom::PressureState CalculateState(const PressureSample&);

  // Last state stored as index instead of value.
  size_t last_state_index_ =
      static_cast<size_t>(mojom::PressureState::kNominal);

  // Drive repeated sampling.
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
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_H_
