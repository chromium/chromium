// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_INTERNAL_H_
#define SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_INTERNAL_H_

#include "base/component_export.h"
#include "services/on_device_model/public/cpp/on_device_model.h"

namespace ml {

COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
const on_device_model::OnDeviceModelShim* GetOnDeviceModelInternalImpl();

COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
const on_device_model::OnDeviceModelShim*
GetOnDeviceModelInternalImplWithoutGpuBlocklistForTesting();

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_INTERNAL_H_
