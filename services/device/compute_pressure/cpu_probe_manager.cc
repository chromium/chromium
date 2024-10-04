// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/cpu_probe_manager.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
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
    kStateBaseThresholds = {0.6,   // kNominal
                            0.75,  // kFair
                            0.9,   // kSerious
                            1.0};  // kCritical

// Thresholds to use during randomization.
constexpr std::array<double,
                     static_cast<size_t>(mojom::PressureState::kMaxValue) + 1>
    kStateRandomizedThresholds = {0.5,   // kNominal
                                  0.8,   // kFair
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
  if (!system_cpu_probe) {
    return nullptr;
  }
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
    // Already started.
    return;
  }

  // The CpuSample reported by many CpuProbe implementations relies on the
  // differences observed between two Update() calls. For this reason, the
  // CpuSample reported from StartSampling() is not reported via
  // `sampling_callback_`.
  system_cpu_probe_->StartSampling(base::DoNothing());

  timer_.Start(FROM_HERE, sampling_interval_,
               base::BindRepeating(
                   &CpuProbe::RequestSample, system_cpu_probe_->GetWeakPtr(),
                   base::BindRepeating(&CpuProbeManager::OnCpuSampleAvailable,
                                       weak_factory_.GetWeakPtr())));

  if (base::FeatureList::IsEnabled(
          features::kComputePressureBreakCalibrationMitigation)) {
    randomization_time_ = base::Seconds(base::RandInt(
        kMinRandomizationTimeInSeconds, kMaxRandomizationTimeInSeconds));

    randomization_timer_.Start(
        FROM_HERE, randomization_time_,
        base::BindRepeating(&CpuProbeManager::ToggleStateRandomization,
                            weak_factory_.GetWeakPtr()));
  }
}

void CpuProbeManager::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  timer_.AbandonAndStop();
  randomization_timer_.AbandonAndStop();
  state_randomization_requested_ = false;
  // Drop the replies to any RequestSample calls that were posted before the
  // timer stopped.
  weak_factory_.InvalidateWeakPtrs();
}

system_cpu::CpuProbe* CpuProbeManager::cpu_probe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return system_cpu_probe_.get();
}

const std::array<double,
                 static_cast<size_t>(mojom::PressureState::kMaxValue) + 1>&
CpuProbeManager::state_thresholds() const {
  return state_randomization_requested_ ? kStateRandomizedThresholds
                                        : kStateBaseThresholds;
}

double CpuProbeManager::hysteresis_threshold_delta() const {
  return kThresholdDelta;
}

void CpuProbeManager::ToggleStateRandomization() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  state_randomization_requested_ = !state_randomization_requested_;
  randomization_time_ = base::Seconds(base::RandInt(
      kMinRandomizationTimeInSeconds, kMaxRandomizationTimeInSeconds));
  randomization_timer_.Start(
      FROM_HERE, randomization_time_,
      base::BindRepeating(&CpuProbeManager::ToggleStateRandomization,
                          weak_factory_.GetWeakPtr()));
}

void CpuProbeManager::OnCpuSampleAvailable(std::optional<CpuSample> sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the timer was stopped, OnCpuSampleAvailable should have been cancelled
  // by InvalidateWeakPtrs().
  CHECK(timer_.IsRunning());
  if (sample.has_value()) {
    sampling_callback_.Run(CalculateState(sample.value()));
  }
}

mojom::PressureState CpuProbeManager::CalculateState(const CpuSample& sample) {
  // TODO(crbug.com/40231044): A more advanced algorithm that calculates
  // PressureState using CpuSample needs to be determined.
  // At this moment the algorithm is the simplest possible
  // with thresholds defining the state.
  const auto& kStateThresholds = state_thresholds();

  auto it = base::ranges::lower_bound(kStateThresholds, sample.cpu_utilization);
  if (it == kStateThresholds.end()) {
    NOTREACHED() << "unexpected value: " << sample.cpu_utilization;
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
  system_cpu_probe_ = std::move(cpu_probe);
}

}  // namespace device
