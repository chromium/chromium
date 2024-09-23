// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_VIRTUAL_CPU_PROBE_MANAGER_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_VIRTUAL_CPU_PROBE_MANAGER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "services/device/compute_pressure/cpu_probe_manager.h"
#include "services/device/public/mojom/pressure_update.mojom-shared.h"

namespace device {

// CpuProbeManager subclass that with a custom CpuProbe implementation that
// receives its sample updates from user-provided input.
class VirtualCpuProbeManager final : public CpuProbeManager {
 public:
  static std::unique_ptr<VirtualCpuProbeManager> Create(
      base::TimeDelta sampling_interval,
      base::RepeatingCallback<void(mojom::PressureState)> sampling_callback);

  ~VirtualCpuProbeManager() final;

  // Creates a system_cpu::CpuSample that corresponds to |desired_state| and
  // provides it to this class' custom CpuProbe. The CpuSample instance will
  // eventually reach CpuProbeManager::OnCpuSampleAvailable() and cause
  // |desired_state| to be reported.
  void SetPressureState(mojom::PressureState desired_state);

 private:
  VirtualCpuProbeManager(
      base::TimeDelta sampling_interval,
      base::RepeatingCallback<void(mojom::PressureState)> sampling_callback);
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_VIRTUAL_CPU_PROBE_MANAGER_H_
