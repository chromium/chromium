// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_GPU_BLOCKLIST_H_
#define SERVICES_ON_DEVICE_MODEL_ML_GPU_BLOCKLIST_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "services/on_device_model/ml/chrome_ml_api.h"

namespace ml {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GpuBlockedReason {
  kGpuConfigError = 0,
  kBlocklisted = 1,
  kBlocklistedForCpuAdapter = 2,
  kNotBlocked = 3,
  kMaxValue = kNotBlocked,
};

struct DeviceInfo {
  GpuBlockedReason gpu_blocked_reason = GpuBlockedReason::kGpuConfigError;
  int32_t vendor_id = 0;
  int32_t device_id = 0;
  std::string driver_version;
  bool supports_fp16 = false;
};

COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
BASE_DECLARE_FEATURE(kOnDeviceModelAllowGpuForTesting);

// Returns GPU device info. If `log_histogram` is true a histogram
// will be logged with the result of the check.
COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
DeviceInfo QueryDeviceInfo(const ChromeMLAPI& api, bool log_histogram);

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_GPU_BLOCKLIST_H_
