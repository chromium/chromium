// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_COMPUTE_PRESSURE_CPU_PRESSURE_CONVERTER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_COMPUTE_PRESSURE_CPU_PRESSURE_CONVERTER_H_

#include <array>

#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "services/device/public/mojom/pressure_update.mojom-shared.h"

namespace device {

class CpuPressureConverter final {
 public:
  CpuPressureConverter() = default;

  CpuPressureConverter(const CpuPressureConverter&) = delete;
  CpuPressureConverter& operator=(const CpuPressureConverter&) = delete;

  ~CpuPressureConverter() = default;

  // Returns the current thresholds being used for each mojom::PressureState,
  // taking state randomization into account.
  const std::array<double,
                   static_cast<size_t>(mojom::PressureState::kMaxValue) + 1>&
  state_thresholds() const;

  // Returns the hysteresis threshold delta value used
  // to prevent state flip-flopping.
  double hysteresis_threshold_delta() const;

  void EnableStateRandomizationMitigation();

  void DisableStateRandomizationMigitation();

  base::TimeDelta GetRandomizationTimeForTesting() const {
    return randomization_time_;
  }

  // Calculate PressureState based on cpu_utilization.
  // The range is between 0.0 and 1.0.
  mojom::PressureState CalculateState(const double cpu_utilization);

 private:
  // Implements the "break calibration" mitigation by toggling the
  // |state_randomization_requested_| flag every |randomization_time_|
  // interval.
  void ToggleStateRandomization();

  SEQUENCE_CHECKER(sequence_checker_);

  // Variable storing |randomization_timer_| time.
  base::TimeDelta randomization_time_;

  // Drive randomization interval by invoking `ToggleStateRandomization()`.
  base::OneShotTimer randomization_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Flag to indicate that state randomization has been requested.
  bool state_randomization_requested_ = false;

  // Last state stored as index instead of value.
  size_t last_state_index_ =
      static_cast<size_t>(mojom::PressureState::kNominal);
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_COMPUTE_PRESSURE_CPU_PRESSURE_CONVERTER_H_
