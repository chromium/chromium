// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/pressure_quantizer.h"

#include <vector>

namespace content {

PressureQuantizer::PressureQuantizer() = default;

PressureQuantizer::~PressureQuantizer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
bool PressureQuantizer::IsValid(
    const blink::mojom::PressureQuantization& quantization) {
  if (quantization.cpu_utilization_thresholds.size() >
      blink::mojom::kMaxPressureCpuUtilizationThresholds) {
    return false;
  }
  if (!ValueQuantizer::IsValid(quantization.cpu_utilization_thresholds))
    return false;

  return true;
}

bool PressureQuantizer::IsSame(
    const blink::mojom::PressureQuantization& quantization) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(PressureQuantizer::IsValid(quantization));

  if (!cpu_utilization_quantizer_.IsSame(
          quantization.cpu_utilization_thresholds)) {
    return false;
  }

  return true;
}

device::mojom::PressureState PressureQuantizer::Quantize(
    device::mojom::PressureStatePtr sample) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  device::mojom::PressureState state;
  state.cpu_utilization =
      cpu_utilization_quantizer_.Quantize(sample->cpu_utilization);

  return state;
}

void PressureQuantizer::Assign(
    blink::mojom::PressureQuantizationPtr quantization) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(PressureQuantizer::IsValid(*quantization));

  cpu_utilization_quantizer_.Assign(
      std::move(quantization->cpu_utilization_thresholds));
}

PressureQuantizer::ValueQuantizer::ValueQuantizer() = default;

PressureQuantizer::ValueQuantizer::~ValueQuantizer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
bool PressureQuantizer::ValueQuantizer::IsValid(
    const std::vector<double>& thresholds) {
  double last_threshold = 0.0;

  for (double threshold : thresholds) {
    if (threshold <= 0.0 || threshold >= 1.0)
      return false;

    if (threshold <= last_threshold)
      return false;

    last_threshold = threshold;
  }
  return true;
}

bool PressureQuantizer::ValueQuantizer::IsSame(
    const std::vector<double>& thresholds) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ValueQuantizer::IsValid(thresholds));

  if (thresholds_.size() != thresholds.size())
    return false;

  for (size_t i = 0; i < thresholds_.size(); ++i) {
    if (std::abs(thresholds_[i] - thresholds[i]) >= 0.00001)
      return false;
  }
  return true;
}

double PressureQuantizer::ValueQuantizer::Quantize(double value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  double lower_bound = 0.0;
  double upper_bound = 1.0;

  for (double threshold : thresholds_) {
    DCHECK_GT(threshold, lower_bound) << "thresholds_ is not sorted";
    if (value < threshold) {
      upper_bound = threshold;
      break;
    }
    lower_bound = threshold;
  }

  return (lower_bound + upper_bound) / 2;
}

void PressureQuantizer::ValueQuantizer::Assign(std::vector<double> thresholds) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ValueQuantizer::IsValid(thresholds));

  thresholds_ = std::move(thresholds);
}

}  // namespace content
