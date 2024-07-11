// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ON_DEVICE_MODEL_FAKE_H_
#define SERVICES_ON_DEVICE_MODEL_ON_DEVICE_MODEL_FAKE_H_

#include "base/component_export.h"
#include "services/on_device_model/public/cpp/on_device_model.h"

namespace on_device_model {

COMPONENT_EXPORT(ON_DEVICE_MODEL_FAKE)
const OnDeviceModelShim* GetOnDeviceModelFakeImpl();

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_ON_DEVICE_MODEL_FAKE_H_
