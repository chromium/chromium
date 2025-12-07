// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/performance_class.h"

#include "base/compiler_specific.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "services/on_device_model/ml/gpu_blocklist.h"

namespace ml {
namespace {

constexpr uint64_t kBytesPerMb = 1024 * 1024;

// The threshold for GPU RAM below which the device is considered VeryLow.
const base::FeatureParam<int> kLowRAMThreshold{
    &optimization_guide::features::kOnDeviceModelPerformanceParams,
    "on_device_low_ram_threshold_mb", 3000};
// RAM threshold necessary to be considered High or better.
const base::FeatureParam<int> kHighRAMThreshold{
    &optimization_guide::features::kOnDeviceModelPerformanceParams,
    "on_device_high_ram_threshold_mb", 5500};

// Output threshold to be considered Low or better.
const base::FeatureParam<int> kLowOutputThreshold{
    &optimization_guide::features::kOnDeviceModelPerformanceParams,
    "on_device_low_output_threshold", 5};

// Input speed min thresholds or each device class.
const base::FeatureParam<int> kLowThreshold{
    &optimization_guide::features::kOnDeviceModelPerformanceParams,
    "on_device_low_threshold", 50};
const base::FeatureParam<int> kMediumThreshold{
    &optimization_guide::features::kOnDeviceModelPerformanceParams,
    "on_device_medium_threshold", 75};
const base::FeatureParam<int> kHighThreshold{
    &optimization_guide::features::kOnDeviceModelPerformanceParams,
    "on_device_high_threshold", 150};
const base::FeatureParam<int> kVeryHighThreshold{
    &optimization_guide::features::kOnDeviceModelPerformanceParams,
    "on_device_very_high_threshold", 500};

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

COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
uint64_t GetLowRamThresholdMb() {
  return static_cast<uint64_t>(kLowRAMThreshold.Get());
}

COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
uint64_t GetHighRamThresholdMb() {
  return static_cast<uint64_t>(kHighRAMThreshold.Get());
}

DISABLE_CFI_DLSYM
COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
std::pair<on_device_model::mojom::DevicePerformanceInfoPtr,
          on_device_model::mojom::DeviceInfoPtr>
GetDeviceAndPerformanceInfo(const ChromeML& chrome_ml) {
  auto perf_info = on_device_model::mojom::DevicePerformanceInfo::New();
  auto device_info = on_device_model::mojom::DeviceInfo::New();

  ml::DeviceInfo query_device_info =
      ml::QueryDeviceInfo(chrome_ml.api(), /*log_histogram=*/true);
  if (query_device_info.gpu_blocked_reason != GpuBlockedReason::kNotBlocked) {
    perf_info->performance_class =
        on_device_model::mojom::PerformanceClass::kGpuBlocked;
    perf_info->vram_mb = 0ul;
    return std::make_pair(std::move(perf_info), std::move(device_info));
  }

  device_info->vendor_id = query_device_info.vendor_id;
  device_info->device_id = query_device_info.device_id;
  device_info->driver_version = query_device_info.driver_version;
  device_info->supports_fp16 = query_device_info.supports_fp16;

  ChromeMLPerformanceInfo info;
  bool success = chrome_ml.api().GetEstimatedPerformance(&info);
  base::UmaHistogramBoolean("OnDeviceModel.BenchmarkSuccess", success);
  if (!success) {
    perf_info->performance_class =
        on_device_model::mojom::PerformanceClass::kError;
    perf_info->vram_mb = 0ul;
    return std::make_pair(std::move(perf_info), std::move(device_info));
  }
  const float input_speed = info.input_speed;
  const float output_speed = info.output_speed;
  const bool is_integrated_gpu = info.is_integrated_gpu;

  int system_ram = base::SysInfo::AmountOfPhysicalMemory().InMiB();
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

  // Integrated GPUs can use at least 1/2 of system RAM as VRAM. Mac doesn't
  // allow directly querying VRAM, and instead returns the "recommended" maximum
  // VRAM to use, which may change depending on system load. This ensures that
  // for integrated GPUs we have a more reasonable value in that case.
  if (is_integrated_gpu) {
    device_heap_mb =
        std::max(static_cast<uint64_t>(system_ram / 2), device_heap_mb);
  }

  perf_info->vram_mb = device_heap_mb;

  // Devices with low RAM are considered very low perf.
  if (device_heap_mb < GetLowRamThresholdMb()) {
    LogVeryLowReason(VeryLowPerformanceReason::kLowRAM);
    perf_info->performance_class =
        on_device_model::mojom::PerformanceClass::kVeryLow;
    return std::make_pair(std::move(perf_info), std::move(device_info));
  }

  // Devices that output less than 6 tk/s are considered very low perf.
  if (output_speed < kLowOutputThreshold.Get()) {
    LogVeryLowReason(VeryLowPerformanceReason::kSlowOutput);
    perf_info->performance_class =
        on_device_model::mojom::PerformanceClass::kVeryLow;
    return std::make_pair(std::move(perf_info), std::move(device_info));
  }
  // VeryLow:  [0, 50)
  // Low:      [50, 100)
  // Medium:   [100, 250)
  // High:     [250, 750)
  // VeryHigh: [750, inf)
  if (input_speed < kLowThreshold.Get()) {
    LogVeryLowReason(VeryLowPerformanceReason::kSlowInput);
    perf_info->performance_class =
        on_device_model::mojom::PerformanceClass::kVeryLow;
  } else if (input_speed < kMediumThreshold.Get()) {
    perf_info->performance_class =
        on_device_model::mojom::PerformanceClass::kLow;
  } else if (input_speed < kHighThreshold.Get() ||
             device_heap_mb < GetHighRamThresholdMb()) {
    perf_info->performance_class =
        on_device_model::mojom::PerformanceClass::kMedium;
  } else if (input_speed < kVeryHighThreshold.Get()) {
    perf_info->performance_class =
        on_device_model::mojom::PerformanceClass::kHigh;
  } else {
    perf_info->performance_class =
        on_device_model::mojom::PerformanceClass::kVeryHigh;
  }
  return std::make_pair(std::move(perf_info), std::move(device_info));
}

}  // namespace ml
