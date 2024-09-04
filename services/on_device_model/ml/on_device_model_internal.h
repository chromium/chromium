// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_INTERNAL_H_
#define SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_INTERNAL_H_

#include "base/component_export.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/gpu_blocklist.h"

namespace ml {

// Deprecated
class COMPONENT_EXPORT(ON_DEVICE_MODEL_ML) OnDeviceModelInternalImpl final {
 public:
  explicit OnDeviceModelInternalImpl(const ChromeML* chrome_ml);
  ~OnDeviceModelInternalImpl();

  const ChromeML* chrome_ml() const { return chrome_ml_.get(); }

  const raw_ptr<const ChromeML> chrome_ml_;
};

// Deprecated
COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
const OnDeviceModelInternalImpl* GetOnDeviceModelInternalImpl();

// Deprecated
COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
const OnDeviceModelInternalImpl*
GetOnDeviceModelInternalImplWithoutGpuBlocklistForTesting();

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_INTERNAL_H_
