// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/cpu_probe_manager.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "components/system_cpu/cpu_probe.h"
#include "services/device/public/cpp/device_features.h"

namespace device {

namespace {

using system_cpu::CpuProbe;
using system_cpu::CpuSample;

// Delta for the state decision hysteresis.
constexpr double kThresholdDelta = 0.03;

// |randomization_timer_| boundaries in second.
constexpr uint64_t kMinRandomizationTimeInSeconds = 120;
constexpr uint64_t kMaxRandomizationTimeInSeconds = 240;

// Thresholds to use with no randomization.
constexpr std::array<double,
                     static_cast<size_t>(mojom::PressureState::kMaxValue) + 1>
    kStateBaseThresholds = {0.3,   // kNominal
                            0.6,   // kFair
                            0.9,   // kSerious
                            1.0};  // kCritical

// Thresholds to use during randomization.
constexpr std::array<double,
                     static_cast<size_t>(mojom::PressureState::kMaxValue) + 1>
    kStateRandomizedThresholds = {0.2,   // kNominal
                                  0.7,   // kFair
                                  0.85,  // kSerious
                                  1.0};  // kCritical

}  // namespace

// static
std::unique_ptr<CpuProbeManager> CpuProbeManager::Create(
    base::TimeDelta sampling_interval,
    base::RepeatingCallback<void(mojom::PressureState)> sampling_callback) {
  std::unique_ptr<CpuProbe> system_cpu_probe = CpuProbe::Create();
  if (!system_cpu_probe) {
    return nullptr;
  }
  return base::WrapUnique(new CpuProbeManager(
      std::move(system_cpu_probe), sampling_interval, sampling_callback));
}

// static
std::unique_ptr<CpuProbeManager> CpuProbeManager::CreateForTesting(
    std::unique_ptr<CpuProbe> system_cpu_probe,
    base::TimeDelta sampling_interval,
    base::RepeatingCallback<void(mojom::PressureState)> sampling_callback) {
  return base::WrapUnique(new CpuProbeManager(
      std::move(system_cpu_probe), sampling_interval, sampling_callback));
}

CpuProbeManager::CpuProbeManager(
    std::unique_ptr<CpuProbe> system_cpu_probe,
    base::TimeDelta sampling_interval,
    base::RepeatingCallback<void(mojom::PressureState)> sampling_callback)
    : system_cpu_probe_(std::move(system_cpu_probe)),
      sampling_interval_(sampling_interval),
      sampling_callback_(std::move(sampling_callback)) {
  CHECK(system_cpu_probe_);
  CHECK(sampling_callback_);
}

CpuProbeManager::~CpuProbeManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CpuProbeManager::EnsureStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (timer_.IsRunning()) {
    return;
  }

  CHECK(!got_probe_baseline_) << "got_probe_baseline_ incorrectly reset";

  system_cpu_probe_->StartSampling(base::BindOnce(
      &CpuProbeManager::OnSamplingStarted, base::Unretained(this)));

  // base::Unretained usage is safe here because the callback is only run
  // while `system_cpu_probe_` is alive, and `system_cpu_probe_` is owned by
  // this instance.
  timer_.Start(FROM_HERE, sampling_interval_,
               base::BindRepeating(
                   &CpuProbe::RequestSample, system_cpu_probe_->GetWeakPtr(),
                   base::BindRepeating(&CpuProbeManager::OnCpuSampleAvailable,
                                       base::Unretained(this))));

  if (base::FeatureList::IsEnabled(
          features::kComputePressureBreakCalibrationMitigation)) {
    randomization_time_ = base::Seconds(base::RandInt(
        kMinRandomizationTimeInSeconds, kMaxRandomizationTimeInSeconds));

    randomization_timer_.Start(
        FROM_HERE, randomization_time_,
        base::BindRepeating(&CpuProbeManager::ToggleStateRandomization,
                            base::Unretained(this)));
  }
}

void CpuProbeManager::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  timer_.AbandonAndStop();
  randomization_timer_.AbandonAndStop();
  state_randomization_requested_ = false;
  got_probe_baseline_ = false;
}

void CpuProbeManager::ToggleStateRandomization() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  state_randomization_requested_ = !state_randomization_requested_;
  randomization_time_ = base::Seconds(base::RandInt(
      kMinRandomizationTimeInSeconds, kMaxRandomizationTimeInSeconds));
  randomization_timer_.Start(
      FROM_HERE, randomization_time_,
      base::BindRepeating(&CpuProbeManager::ToggleStateRandomization,
                          base::Unretained(this)));
}

void CpuProbeManager::OnSamplingStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Don't set got_probe_baseline_ when Stop() was already called.
  if (!timer_.IsRunning()) {
    return;
  }

  got_probe_baseline_ = true;
}

void CpuProbeManager::OnCpuSampleAvailable(std::optional<CpuSample> sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Stop sending data when Stop() was already called.
  if (!timer_.IsRunning()) {
    return;
  }

  CHECK(got_probe_baseline_) << "got_probe_baseline_ incorrectly reset";

  sampling_callback_.Run(CalculateState(sample));
}

mojom::PressureState CpuProbeManager::CalculateState(
    std::optional<CpuSample> maybe_sample) {
  const CpuSample sample = maybe_sample.value_or(kUnsupportedValue);

  // TODO(crbug.com/40231044): A more advanced algorithm that calculates
  // PressureState using CpuSample needs to be determined.
  // At this moment the algorithm is the simplest possible
  // with thresholds defining the state.
  const auto& kStateThresholds = state_randomization_requested_
                                     ? kStateRandomizedThresholds
                                     : kStateBaseThresholds;

  auto it = base::ranges::lower_bound(kStateThresholds, sample.cpu_utilization);
  if (it == kStateThresholds.end()) {
    NOTREACHED_NORETURN() << "unexpected value: " << sample.cpu_utilization;
  }

  size_t state_index = std::distance(kStateThresholds.begin(), it);

  // Hysteresis to avoid flip-flop between state.
  // Threshold needs to drop by level and
  // cpu_utilization needs a drop of kThresholdDelta below the state
  // threshold to be validated as a lower pressure state.
  if (last_state_index_ - state_index != 1 ||
      kStateThresholds[state_index] - sample.cpu_utilization >=
          kThresholdDelta) {
    last_state_index_ = state_index;
  }

  return static_cast<mojom::PressureState>(last_state_index_);
}

void CpuProbeManager::SetCpuProbeForTesting(
    std::unique_ptr<system_cpu::CpuProbe> cpu_probe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!timer_.IsRunning());
  CHECK(!got_probe_baseline_);
  system_cpu_probe_ = std::move(cpu_probe);
}

system_cpu::CpuProbe* CpuProbeManager::GetCpuProbeForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return system_cpu_probe_.get();
}

}  // namespace device
