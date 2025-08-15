// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_PERFORMANCE_CLASS_H_
#define SERVICES_ON_DEVICE_MODEL_ML_PERFORMANCE_CLASS_H_

#include "base/component_export.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace ml {

// Returns the low threshold of VRAM in Mib. All devices with VRAM under this
// value are considered `kVeryLow` as their `PerformanceClass`.
COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
uint64_t GetLowRamThresholdMb();

// Returns the high threshold of VRAM in Mib. Only devices with VRAM higher than
// this value may be considered `kHigh` or better as their `PerformanceClass`.
COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
uint64_t GetHighRamThresholdMb();

// Returns the device info and performance info as a pair.
COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
std::pair<on_device_model::mojom::DevicePerformanceInfoPtr,
          on_device_model::mojom::DeviceInfoPtr>
GetDeviceAndPerformanceInfo(const ChromeML& chrome_ml);

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_PERFORMANCE_CLASS_H_
