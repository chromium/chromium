// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_GPU_BLOCKLIST_H_
#define SERVICES_ON_DEVICE_MODEL_ML_GPU_BLOCKLIST_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "services/on_device_model/ml/chrome_ml_api.h"

namespace ml {

COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
BASE_DECLARE_FEATURE(kOnDeviceModelAllowGpuForTesting);

// Checks if the GPU is on the blocklist. If `log_histogram` is true a histogram
// will be logged with the result of the check.
COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
bool IsGpuBlocked(const ChromeMLAPI& api, bool log_histogram);

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_GPU_BLOCKLIST_H_
