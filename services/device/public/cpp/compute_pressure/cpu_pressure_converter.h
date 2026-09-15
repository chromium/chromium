// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_COMPUTE_PRESSURE_CPU_PRESSURE_CONVERTER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_COMPUTE_PRESSURE_CPU_PRESSURE_CONVERTER_H_

#include <array>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "services/device/public/mojom/pressure_update.mojom-shared.h"

namespace device {

class SharedRandomizationState;

class CpuPressureConverter final {
 public:
  CpuPressureConverter();

  CpuPressureConverter(const CpuPressureConverter&) = delete;
  CpuPressureConverter& operator=(const CpuPressureConverter&) = delete;

  ~CpuPressureConverter();

  // Returns the current thresholds being used for each mojom::PressureState,
  // taking state randomization into account.
  const std::array<double,
                   static_cast<size_t>(mojom::PressureState::kMaxValue) + 1>&
  state_thresholds() const;

  // Returns the hysteresis threshold delta value used
  // to prevent state flip-flopping.
  double hysteresis_threshold_delta() const;

  void EnableStateRandomizationMitigation();

  void DisableStateRandomizationMitigation();

  base::TimeDelta GetRandomizationTimeForTesting() const;

  // Calculate PressureState based on cpu_utilization.
  // The range is between 0.0 and 1.0.
  mojom::PressureState CalculateState(const double cpu_utilization);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<SharedRandomizationState> shared_randomization_state_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Last state stored as index instead of value.
  size_t last_state_index_ GUARDED_BY_CONTEXT(sequence_checker_) =
      static_cast<size_t>(mojom::PressureState::kNominal);
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_COMPUTE_PRESSURE_CPU_PRESSURE_CONVERTER_H_
