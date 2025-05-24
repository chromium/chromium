// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/compute_pressure/cpu_pressure_converter.h"

#include "base/rand_util.h"

namespace device {

namespace {

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

const std::array<double,
                 static_cast<size_t>(mojom::PressureState::kMaxValue) + 1>&
CpuPressureConverter::state_thresholds() const {
  return state_randomization_requested_ ? kStateRandomizedThresholds
                                        : kStateBaseThresholds;
}

double CpuPressureConverter::hysteresis_threshold_delta() const {
  return kThresholdDelta;
}

void CpuPressureConverter::EnableStateRandomizationMitigation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  randomization_time_ = base::Seconds(base::RandInt(
      kMinRandomizationTimeInSeconds, kMaxRandomizationTimeInSeconds));
  randomization_timer_.Start(
      FROM_HERE, randomization_time_,
      base::BindRepeating(&CpuPressureConverter::ToggleStateRandomization,
                          base::Unretained(this)));
}

void CpuPressureConverter::DisableStateRandomizationMigitation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  randomization_timer_.Stop();
  state_randomization_requested_ = false;
}

void CpuPressureConverter::ToggleStateRandomization() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  state_randomization_requested_ = !state_randomization_requested_;
  randomization_time_ = base::Seconds(base::RandInt(
      kMinRandomizationTimeInSeconds, kMaxRandomizationTimeInSeconds));
  randomization_timer_.Start(
      FROM_HERE, randomization_time_,
      base::BindRepeating(&CpuPressureConverter::ToggleStateRandomization,
                          base::Unretained(this)));
}

mojom::PressureState CpuPressureConverter::CalculateState(
    const double cpu_utilization) {
  // TODO(crbug.com/40231044): A more advanced algorithm that calculates
  // PressureState using PressureSample needs to be determined.
  // At this moment the algorithm is the simplest possible
  // with thresholds defining the state.
  const auto& kStateThresholds = state_randomization_requested_
                                     ? kStateRandomizedThresholds
                                     : kStateBaseThresholds;

  auto it = std::ranges::lower_bound(kStateThresholds, cpu_utilization);
  if (it == kStateThresholds.end()) {
    NOTREACHED() << "unexpected value: " << cpu_utilization;
  }

  size_t state_index = std::distance(kStateThresholds.begin(), it);
  // Hysteresis to avoid flip-flop between state.
  // Threshold needs to drop by level and
  // cpu_utilization needs a drop of kThresholdDelta below the state
  // threshold to be validated as a lower pressure state.
  if (last_state_index_ - state_index != 1 ||
      kStateThresholds[state_index] - cpu_utilization >= kThresholdDelta) {
    last_state_index_ = state_index;
  }

  return static_cast<mojom::PressureState>(last_state_index_);
}

}  // namespace device
