// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_FEATURES_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace on_device_model::features {

// Whether the fake implementation is used in the OnDeviceModelService.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
BASE_DECLARE_FEATURE(kUseFakeChromeML);

}  // namespace on_device_model::features

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_FEATURES_H_
