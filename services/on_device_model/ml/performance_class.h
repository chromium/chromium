// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_PERFORMANCE_CLASS_H_
#define SERVICES_ON_DEVICE_MODEL_ML_PERFORMANCE_CLASS_H_

#include "base/component_export.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace ml {

// Returns the estimated performance class of this device based on a small
// benchmark.
COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
on_device_model::mojom::PerformanceClass GetEstimatedPerformanceClass(
    const ChromeML& chrome_ml);

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_PERFORMANCE_CLASS_H_
