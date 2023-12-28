// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/utils.h"

#include "base/compiler_specific.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "components/optimization_guide/core/optimization_guide_features.h"

namespace ml {
namespace {

constexpr uint64_t kBytesPerMb = 1024 * 1024;

// The threshold for GPU RAM below which the device is considered VeryLow.
const base::FeatureParam<int> kLowRAMThreshold{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_low_ram_threshold_mb", 3600};
// RAM threshold necessary to be considered High or better.
const base::FeatureParam<int> kHighRAMThreshold{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_high_ram_threshold_mb", 7600};

// Output threshold to be considered Low or better.
const base::FeatureParam<int> kLowOutputThreshold{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_low_output_threshold", 5};

// Input speed thresholds or each device class.
const base::FeatureParam<int> kLowThreshold{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_low_threshold", 50};
const base::FeatureParam<int> kMediumThreshold{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_medium_threshold", 100};
const base::FeatureParam<int> kHighThreshold{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_high_threshold", 250};
const base::FeatureParam<int> kVeryHighThreshold{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_very_high_threshold", 750};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class VeryLowPerformanceReason {
  kLowRAM = 0,
  kSlowOutput = 1,
  kSlowInput = 2,
  kMaxValue = kSlowInput,
};

void LogVeryLowReason(VeryLowPerformanceReason reason) {
  base::UmaHistogramEnumeration("OnDeviceModel.BenchmarkVeryLowReason", reason);
}

}  // namespace

DISABLE_CFI_DLSYM
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
  uint64_t device_heap_mb = info.device_heap_size / kBytesPerMb;
  base::UmaHistogramMemoryLargeMB(
      base::StrCat({"OnDeviceModel.DeviceHeapSize.",
                    is_integrated_gpu ? "Integrated" : "Discrete"}),
      device_heap_mb);
  if (info.max_buffer_size) {
    base::UmaHistogramMemoryLargeMB(
        base::StrCat({"OnDeviceModel.MaxBufferSize.",
                      is_integrated_gpu ? "Integrated" : "Discrete"}),
        info.max_buffer_size);
  }

  base::UmaHistogramCounts10000(
      "OnDeviceModel.BenchmarkEstimatedTokensPerSecond.Input", input_speed);
  base::UmaHistogramCounts1000(
      "OnDeviceModel.BenchmarkEstimatedTokensPerSecond.Output", output_speed);

  // Devices with low RAM are considered very low perf.
  if (device_heap_mb < static_cast<uint64_t>(kLowRAMThreshold.Get())) {
    LogVeryLowReason(VeryLowPerformanceReason::kLowRAM);
    return on_device_model::mojom::PerformanceClass::kVeryLow;
  }

  // Devices that output less than 6 tk/s are considered very low perf.
  if (output_speed < kLowOutputThreshold.Get()) {
    LogVeryLowReason(VeryLowPerformanceReason::kSlowOutput);
    return on_device_model::mojom::PerformanceClass::kVeryLow;
  }
  // VeryLow:  [0, 50)
  // Low:      [50, 100)
  // Medium:   [100, 250)
  // High:     [250, 750)
  // VeryHigh: [750, inf)
  if (input_speed < kLowThreshold.Get()) {
    LogVeryLowReason(VeryLowPerformanceReason::kSlowInput);
    return on_device_model::mojom::PerformanceClass::kVeryLow;
  } else if (input_speed < kMediumThreshold.Get()) {
    return on_device_model::mojom::PerformanceClass::kLow;
  } else if (input_speed < kHighThreshold.Get() ||
             device_heap_mb < static_cast<uint64_t>(kHighRAMThreshold.Get())) {
    return on_device_model::mojom::PerformanceClass::kMedium;
  } else if (input_speed < kVeryHighThreshold.Get()) {
    return on_device_model::mojom::PerformanceClass::kHigh;
  } else {
    return on_device_model::mojom::PerformanceClass::kVeryHigh;
  }
}

}  // namespace ml
