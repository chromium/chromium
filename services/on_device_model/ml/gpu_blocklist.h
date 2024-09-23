// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_GPU_BLOCKLIST_H_
#define SERVICES_ON_DEVICE_MODEL_ML_GPU_BLOCKLIST_H_

#include "base/component_export.h"
#include "services/on_device_model/ml/chrome_ml_api.h"

namespace ml {

// Checks if the GPU is on the blocklist.
COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
bool IsGpuBlocked(const ChromeMLAPI& api);

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_GPU_BLOCKLIST_H_
