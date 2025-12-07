// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_VIRTUAL_CPU_PROBE_MANAGER_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_VIRTUAL_CPU_PROBE_MANAGER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "services/device/compute_pressure/cpu_probe_manager.h"
#include "services/device/public/cpp/compute_pressure/cpu_pressure_converter.h"
#include "services/device/public/mojom/pressure_update.mojom-shared.h"

namespace device {

// CpuProbeManager subclass that with a custom CpuProbe implementation that
// receives its sample updates from user-provided input.
class VirtualCpuProbeManager final : public CpuProbeManager {
 public:
  static std::unique_ptr<VirtualCpuProbeManager> Create(
      base::TimeDelta sampling_interval,
      base::RepeatingCallback<void(mojom::PressureDataPtr)> sampling_callback);

  ~VirtualCpuProbeManager() final;

  void OnCpuSampleAvailable(std::optional<system_cpu::CpuSample>) override;

  // Creates a system_cpu::CpuSample that corresponds to |desired_state| and
  // provides it to this class' custom CpuProbe. The CpuSample instance will
  // eventually reach CpuProbeManager::OnCpuSampleAvailable() and cause
  // |desired_state| to be reported.
  void SetPressureState(mojom::PressureState desired_state);

  // Set the own_contribution_estimate to desired_estimate.
  // The CpuSample instance will eventually reach
  // CpuProbeManager::OnCpuSampleAvailable() and cause |desired_estimate| to be
  // reported.
  void SetOwnContributionEstimate(double desired_estimate);

 private:
  VirtualCpuProbeManager(
      base::TimeDelta sampling_interval,
      base::RepeatingCallback<void(mojom::PressureDataPtr)> sampling_callback);

  // Handles break calibration mitigation and conversion from PressureSample
  // to PressureState.
  device::CpuPressureConverter converter_;

  // Virtual own_contribution_estimate.
  double own_contribution_estimate_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_VIRTUAL_CPU_PROBE_MANAGER_H_
