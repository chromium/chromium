// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/virtual_cpu_probe_manager.h"

#include <optional>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/system_cpu/cpu_probe.h"
#include "components/system_cpu/cpu_sample.h"

namespace device {

namespace {

// CpuProbe implementation that always reports a user-provided value.
class VirtualCpuProbe final : public system_cpu::CpuProbe {
 public:
  VirtualCpuProbe();
  ~VirtualCpuProbe() final;

  // CpuProbe implementation.
  void Update(SampleCallback callback) final;
  base::WeakPtr<CpuProbe> GetWeakPtr() final;

  void SetSample(system_cpu::CpuSample sample);

 private:
  std::optional<system_cpu::CpuSample> current_sample_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<VirtualCpuProbe> weak_factory_{this};
};

VirtualCpuProbe::VirtualCpuProbe() = default;

VirtualCpuProbe::~VirtualCpuProbe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VirtualCpuProbe::Update(SampleCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Even though |callback| could be invoked directly here, we use PostTask()
  // to better mimic the platform-specific implementations in
  // //components/system_cpu.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), current_sample_));
}

base::WeakPtr<system_cpu::CpuProbe> VirtualCpuProbe::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void VirtualCpuProbe::SetSample(system_cpu::CpuSample sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  current_sample_ = sample;
}

}  // namespace

// static
std::unique_ptr<VirtualCpuProbeManager> VirtualCpuProbeManager::Create(
    base::TimeDelta sampling_interval,
    base::RepeatingCallback<void(mojom::PressureState)> sampling_callback) {
  return base::WrapUnique(new VirtualCpuProbeManager(
      sampling_interval, std::move(sampling_callback)));
}

VirtualCpuProbeManager::VirtualCpuProbeManager(
    base::TimeDelta sampling_interval,
    base::RepeatingCallback<void(mojom::PressureState)> sampling_callback)
    : CpuProbeManager(std::make_unique<VirtualCpuProbe>(),
                      sampling_interval,
                      std::move(sampling_callback)) {}

VirtualCpuProbeManager::~VirtualCpuProbeManager() = default;

void VirtualCpuProbeManager::SetPressureState(
    mojom::PressureState desired_state) {
  double cpu_utilization =
      state_thresholds().at(static_cast<size_t>(desired_state));
  // If the new `desired_state` is one state below the previously set
  // `desired_state`, its setting will be under the effect of `threshold_delta`
  // used to prevent state flip-flopping, therefore the hysteresis threshold
  // delta needs to be applied to validate the state change.
  cpu_utilization -= hysteresis_threshold_delta();
  static_cast<VirtualCpuProbe*>(cpu_probe())
      ->SetSample(system_cpu::CpuSample{cpu_utilization});
}

}  // namespace device
