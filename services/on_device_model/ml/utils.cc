// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/utils.h"

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "services/on_device_model/public/cpp/features.h"

namespace ml {
namespace {

// The threshold for integrated GPU system RAM below which the device is
// considered VeryLow. Intel integrated GPUs on Windows can use half of system
// RAM as VRAM.
const base::FeatureParam<int> kLowIntegratedRAMThreshold{
    &on_device_model::features::kOnDeviceModelService,
    "on_device_low_integrated_ram_threshold_mb", 4000};
// RAM threshold necessary to be considered High or better.
const base::FeatureParam<int> kHighIntegratedRAMThreshold{
    &on_device_model::features::kOnDeviceModelService,
    "on_device_high_integrated_ram_threshold_mb", 8000};

// Output threshold to be considered Low or better.
const base::FeatureParam<int> kLowOutputThreshold{
    &on_device_model::features::kOnDeviceModelService,
    "on_device_low_output_threshold", 6};

// Input speed thresholds or each device class.
const base::FeatureParam<int> kLowThreshold{
    &on_device_model::features::kOnDeviceModelService,
    "on_device_low_threshold", 50};
const base::FeatureParam<int> kMediumThreshold{
    &on_device_model::features::kOnDeviceModelService,
    "on_device_medium_threshold", 100};
const base::FeatureParam<int> kHighThreshold{
    &on_device_model::features::kOnDeviceModelService,
    "on_device_high_threshold", 250};
const base::FeatureParam<int> kVeryHighThreshold{
    &on_device_model::features::kOnDeviceModelService,
    "on_device_very_high_threshold", 750};

}  // namespace

on_device_model::mojom::PerformanceClass GetEstimatedPerformanceClass(
    const ChromeML& chrome_ml) {
  ChromeMLPerformanceInfo info;
  bool success = chrome_ml.api().GetEstimatedPerformance(&info);
  base::UmaHistogramBoolean("OnDeviceModel.BenchmarkSuccess", success);
  if (!success) {
    return on_device_model::mojom::PerformanceClass::kError;
  }
  const float input_speed = info.input_speed;
  const float output_speed = info.output_speed;
  const bool is_integrated_gpu = info.is_integrated_gpu;

  int system_ram = base::SysInfo::AmountOfPhysicalMemoryMB();
  base::UmaHistogramMemoryLargeMB(
      base::StrCat({"OnDeviceModel.SystemRAM.",
                    is_integrated_gpu ? "Integrated" : "Discrete"}),
      system_ram);

  base::UmaHistogramCounts10000(
      "OnDeviceModel.BenchmarkEstimatedTokensPerSecond.Input", input_speed);
  base::UmaHistogramCounts1000(
      "OnDeviceModel.BenchmarkEstimatedTokensPerSecond.Output", output_speed);

  // Devices with low RAM are considered very low perf.
  if (is_integrated_gpu && system_ram < kLowIntegratedRAMThreshold.Get()) {
    return on_device_model::mojom::PerformanceClass::kVeryLow;
  }

  // Devices that output less than 6 tk/s are considered very low perf.
  if (output_speed < kLowOutputThreshold.Get()) {
    return on_device_model::mojom::PerformanceClass::kVeryLow;
  }
  // VeryLow:  [0, 50)
  // Low:      [50, 100)
  // Medium:   [100, 250)
  // High:     [250, 750)
  // VeryHigh: [750, inf)
  if (input_speed < kLowThreshold.Get()) {
    return on_device_model::mojom::PerformanceClass::kVeryLow;
  } else if (input_speed < kMediumThreshold.Get()) {
    return on_device_model::mojom::PerformanceClass::kLow;
  } else if (input_speed < kHighThreshold.Get() ||
             (is_integrated_gpu &&
              system_ram < kHighIntegratedRAMThreshold.Get())) {
    return on_device_model::mojom::PerformanceClass::kMedium;
  } else if (input_speed < kVeryHighThreshold.Get()) {
    return on_device_model::mojom::PerformanceClass::kHigh;
  } else {
    return on_device_model::mojom::PerformanceClass::kVeryHigh;
  }
}

}  // namespace ml
