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
#include "services/device/public/mojom/pressure_update.mojom.h"

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
      base::RepeatingCallback<void(mojom::PressureDataPtr)> sampling_callback);

  // Instantiates CpuProbeManager with a supplied CpuProbe.
  static std::unique_ptr<CpuProbeManager> CreateForTesting(
      base::TimeDelta sampling_interval,
      base::RepeatingCallback<void(mojom::PressureDataPtr)> sampling_callback,
      std::unique_ptr<system_cpu::CpuProbe> system_cpu_probe);

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

  void SetCpuProbeForTesting(std::unique_ptr<system_cpu::CpuProbe>);

 protected:
  SEQUENCE_CHECKER(sequence_checker_);

  CpuProbeManager(base::TimeDelta,
                  base::RepeatingCallback<void(mojom::PressureDataPtr)>,
                  std::unique_ptr<system_cpu::CpuProbe> system_cpu_probe);

  system_cpu::CpuProbe* cpu_probe();

  // Drive repeated sampling.
  base::RepeatingTimer timer_ GUARDED_BY_CONTEXT(sequence_checker_);
  const base::TimeDelta sampling_interval_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Called with each sample reading.
  base::RepeatingCallback<void(mojom::PressureDataPtr)> sampling_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

 private:
  friend class CpuProbeManagerTest;
  FRIEND_TEST_ALL_PREFIXES(CpuProbeManagerDeathTest,
                           CalculateStateValueTooLarge);
  FRIEND_TEST_ALL_PREFIXES(CpuProbeManagerTest, CreateCpuProbeExists);

  // Called periodically while the CpuProbe is running.
  virtual void OnCpuSampleAvailable(std::optional<system_cpu::CpuSample>);

  std::unique_ptr<system_cpu::CpuProbe> system_cpu_probe_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<CpuProbeManager> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_MANAGER_H_
