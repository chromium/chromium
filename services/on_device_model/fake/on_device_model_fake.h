// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_FAKE_ON_DEVICE_MODEL_FAKE_H_
#define SERVICES_ON_DEVICE_MODEL_FAKE_ON_DEVICE_MODEL_FAKE_H_

#include "base/component_export.h"
#include "services/on_device_model/ml/on_device_model_internal.h"

namespace fake_ml {

COMPONENT_EXPORT(ON_DEVICE_MODEL_FAKE)
const ml::ChromeML* GetFakeChromeML();

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_FAKE_ON_DEVICE_MODEL_FAKE_H_
