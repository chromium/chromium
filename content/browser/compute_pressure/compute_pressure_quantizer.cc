// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/sequence_checker.h"
#include "content/browser/compute_pressure/compute_pressure_quantizer.h"

namespace content {

ComputePressureQuantizer::ComputePressureQuantizer() = default;

ComputePressureQuantizer::~ComputePressureQuantizer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
bool ComputePressureQuantizer::IsValid(
    const blink::mojom::ComputePressureQuantization& quantization) {
  if (quantization.cpu_utilization_thresholds.size() >
      blink::mojom::kMaxComputePressureCpuUtilizationThresholds) {
    return false;
  }
  if (!ValueQuantizer::IsValid(quantization.cpu_utilization_thresholds))
    return false;

  if (quantization.cpu_speed_thresholds.size() >
      blink::mojom::kMaxComputePressureCpuSpeedThresholds) {
    return false;
  }
  if (!ValueQuantizer::IsValid(quantization.cpu_speed_thresholds))
    return false;

  return true;
}

bool ComputePressureQuantizer::IsSame(
    const blink::mojom::ComputePressureQuantization& quantization) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ComputePressureQuantizer::IsValid(quantization));

  if (!cpu_utilization_quantizer_.IsSame(
          quantization.cpu_utilization_thresholds)) {
    return false;
  }
  if (!cpu_speed_quantizer_.IsSame(quantization.cpu_speed_thresholds))
    return false;

  return true;
}

blink::mojom::ComputePressureState ComputePressureQuantizer::Quantize(
    ComputePressureSample sample) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  blink::mojom::ComputePressureState state;
  state.cpu_utilization =
      cpu_utilization_quantizer_.Quantize(sample.cpu_utilization);
  state.cpu_speed = cpu_speed_quantizer_.Quantize(sample.cpu_speed);

  return state;
}

void ComputePressureQuantizer::Assign(
    blink::mojom::ComputePressureQuantizationPtr quantization) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ComputePressureQuantizer::IsValid(*quantization));

  cpu_utilization_quantizer_.Assign(
      std::move(quantization->cpu_utilization_thresholds));
  cpu_speed_quantizer_.Assign(std::move(quantization->cpu_speed_thresholds));
}

ComputePressureQuantizer::ValueQuantizer::ValueQuantizer() = default;

ComputePressureQuantizer::ValueQuantizer::~ValueQuantizer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
bool ComputePressureQuantizer::ValueQuantizer::IsValid(
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

bool ComputePressureQuantizer::ValueQuantizer::IsSame(
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

double ComputePressureQuantizer::ValueQuantizer::Quantize(double value) const {
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

void ComputePressureQuantizer::ValueQuantizer::Assign(
    std::vector<double> thresholds) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ValueQuantizer::IsValid(thresholds));

  thresholds_ = std::move(thresholds);
}

}  // namespace content
