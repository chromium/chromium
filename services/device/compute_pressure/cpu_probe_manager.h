// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_MANAGER_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_MANAGER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/system_cpu/cpu_sample.h"
#include "services/device/public/mojom/pressure_update.mojom-shared.h"

namespace system_cpu {
class CpuProbe;
}

namespace device {

// Interface for retrieving the compute pressure state for CPU from the
// underlying OS at regular intervals.
//
// This class uses system_cpu::CpuProbe to get CPU samples. Instances maintain a
// CpuProbe and a timer, so that at each time interval CpuProbe::RequestSample
// is called to get the CPU pressure state since the last sample.
//
// Instances are not thread-safe and should be used on the same sequence.
//
// The instance is owned by a PressureManagerImpl.
class CpuProbeManager {
 public:
  // Return this value when the implementation fails to get a result.
  static constexpr system_cpu::CpuSample kUnsupportedValue = {.cpu_utilization =
                                                                  0.0};

  // Instantiates CpuProbeManager with a CpuProbe for the current platform.
  // Returns nullptr if no suitable implementation exists.
  static std::unique_ptr<CpuProbeManager> Create(
      base::TimeDelta sampling_interval,
      base::RepeatingCallback<void(mojom::PressureState)> sampling_callback);

  // Instantiates CpuProbeManager with a supplied CpuProbe.
  static std::unique_ptr<CpuProbeManager> CreateForTesting(
      std::unique_ptr<system_cpu::CpuProbe> system_cpu_probe,
      base::TimeDelta sampling_interval,
      base::RepeatingCallback<void(mojom::PressureState)> sampling_callback);

  CpuProbeManager(const CpuProbeManager&) = delete;
  CpuProbeManager& operator=(const CpuProbeManager&) = delete;

  virtual ~CpuProbeManager();

  // Idempotent.
  // Start the timer to retrieve the compute pressure state from the
  // underlying OS at specific time interval.
  void EnsureStarted();

  // Idempotent.
  // Stop the timer.
  void Stop();

  base::TimeDelta GetRandomizationTimeForTesting() const {
    return randomization_time_;
  }

  void SetCpuProbeForTesting(std::unique_ptr<system_cpu::CpuProbe>);

 protected:
  CpuProbeManager(std::unique_ptr<system_cpu::CpuProbe> system_cpu_probe,
                  base::TimeDelta,
                  base::RepeatingCallback<void(mojom::PressureState)>);

  system_cpu::CpuProbe* cpu_probe();

  // Returns the current thresholds being used for each mojom::PressureState,
  // taking state randomization into account.
  const std::array<double,
                   static_cast<size_t>(mojom::PressureState::kMaxValue) + 1>&
  state_thresholds() const;

  // Returns the hysteresis threshold delta value used
  // to prevent state flip-flopping.
  double hysteresis_threshold_delta() const;

 private:
  friend class CpuProbeManagerTest;
  FRIEND_TEST_ALL_PREFIXES(CpuProbeManagerDeathTest,
                           CalculateStateValueTooLarge);
  FRIEND_TEST_ALL_PREFIXES(CpuProbeManagerTest, CreateCpuProbeExists);

  // Implements the "break calibration" mitigation by toggling the
  // |state_randomization_requested_| flag every |randomization_time_|
  // interval.
  void ToggleStateRandomization();

  // Called periodically while the CpuProbe is running.
  void OnCpuSampleAvailable(std::optional<system_cpu::CpuSample>);

  // Calculate PressureState based on optional CpuSample.
  mojom::PressureState CalculateState(const system_cpu::CpuSample&);

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<system_cpu::CpuProbe> system_cpu_probe_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Variable storing |randomization_timer_| time.
  base::TimeDelta randomization_time_;

  // Last state stored as index instead of value.
  size_t last_state_index_ =
      static_cast<size_t>(mojom::PressureState::kNominal);

  // Drive repeated sampling.
  base::RepeatingTimer timer_ GUARDED_BY_CONTEXT(sequence_checker_);
  const base::TimeDelta sampling_interval_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Drive randomization interval by invoking `ToggleStateRandomization()`.
  base::OneShotTimer randomization_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Flag to indicate that state randomization has been requested.
  bool state_randomization_requested_ = false;

  // Called with each sample reading.
  base::RepeatingCallback<void(mojom::PressureState)> sampling_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<CpuProbeManager> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_MANAGER_H_
