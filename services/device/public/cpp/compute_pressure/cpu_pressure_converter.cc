// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/compute_pressure/cpu_pressure_converter.h"

#include <algorithm>
#include <array>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/timer/timer.h"

namespace device {

namespace {

// Delta for the state decision hysteresis.
constexpr double kThresholdDelta = 0.03;

// |randomization_timer_| boundaries in seconds.
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

class SharedRandomizationState;

namespace {
SharedRandomizationState* g_shared_state = nullptr;
}  // namespace

// Manages the shared "Break Calibration" privacy mitigation timer and threshold
// states across all active CpuPressureConverter instances. All
// CpuPressureConverter instances enabling the mitigation are sequence-affine
// to the sequence where the shared state is bound.
//
// The "Break Calibration" privacy mitigation alternates between base thresholds
// and randomized thresholds at randomized intervals (120-240 seconds) to
// prevent calibration attacks by malicious contexts. Starting and stopping the
// mitigation is requested explicitly via EnableStateRandomizationMitigation()
// and DisableStateRandomizationMitigation(). While active, the shared state
// alternates between base and randomized threshold sets using explicit state
// transitions rather than a blind toggle.
class SharedRandomizationState
    : public base::RefCounted<SharedRandomizationState> {
 public:
  static scoped_refptr<SharedRandomizationState> CreateOrGet() {
    if (!g_shared_state) {
      g_shared_state = new SharedRandomizationState();
    } else {
      DCHECK_CALLED_ON_VALID_SEQUENCE(g_shared_state->sequence_checker_);
    }
    return base::WrapRefCounted(g_shared_state);
  }

  SharedRandomizationState(const SharedRandomizationState&) = delete;
  SharedRandomizationState& operator=(const SharedRandomizationState&) = delete;

  bool is_randomized_thresholds_active() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_randomized_thresholds_active_;
  }

  base::TimeDelta randomization_time() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return randomization_time_;
  }

 private:
  friend class base::RefCounted<SharedRandomizationState>;

  SharedRandomizationState() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Start with base thresholds and schedule activation of randomized
    // thresholds.
    ScheduleNextTimer(/*activate_randomized=*/true);
  }

  ~SharedRandomizationState() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    randomization_timer_.Stop();
    DCHECK_EQ(g_shared_state, this);
    g_shared_state = nullptr;
  }

  void ActivateRandomizedThresholds() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    is_randomized_thresholds_active_ = true;
    ScheduleNextTimer(/*activate_randomized=*/false);
  }

  void ActivateBaseThresholds() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    is_randomized_thresholds_active_ = false;
    ScheduleNextTimer(/*activate_randomized=*/true);
  }

  void ScheduleNextTimer(bool activate_randomized) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    randomization_time_ = base::Seconds(base::RandIntInclusive(
        kMinRandomizationTimeInSeconds, kMaxRandomizationTimeInSeconds));
    randomization_timer_.Start(
        FROM_HERE, randomization_time_,
        base::BindOnce(
            activate_randomized
                ? &SharedRandomizationState::ActivateRandomizedThresholds
                : &SharedRandomizationState::ActivateBaseThresholds,
            base::Unretained(this)));
  }

  SEQUENCE_CHECKER(sequence_checker_);
  base::TimeDelta randomization_time_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::OneShotTimer randomization_timer_ GUARDED_BY_CONTEXT(sequence_checker_);
  bool is_randomized_thresholds_active_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;
};

CpuPressureConverter::CpuPressureConverter() = default;

CpuPressureConverter::~CpuPressureConverter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DisableStateRandomizationMitigation();
}

const std::array<double,
                 static_cast<size_t>(mojom::PressureState::kMaxValue) + 1>&
CpuPressureConverter::state_thresholds() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return (shared_randomization_state_ &&
          shared_randomization_state_->is_randomized_thresholds_active())
             ? kStateRandomizedThresholds
             : kStateBaseThresholds;
}

double CpuPressureConverter::hysteresis_threshold_delta() const {
  return kThresholdDelta;
}

void CpuPressureConverter::EnableStateRandomizationMitigation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (shared_randomization_state_) {
    return;
  }
  shared_randomization_state_ = SharedRandomizationState::CreateOrGet();
}

void CpuPressureConverter::DisableStateRandomizationMitigation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  shared_randomization_state_.reset();
}

base::TimeDelta CpuPressureConverter::GetRandomizationTimeForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return shared_randomization_state_
             ? shared_randomization_state_->randomization_time()
             : base::TimeDelta();
}

mojom::PressureState CpuPressureConverter::CalculateState(
    const double cpu_utilization) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40231044): A more advanced algorithm that calculates
  // PressureState using PressureSample needs to be determined.
  // At this moment the algorithm is the simplest possible
  // with thresholds defining the state.
  const auto& kStateThresholds =
      (shared_randomization_state_ &&
       shared_randomization_state_->is_randomized_thresholds_active())
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
