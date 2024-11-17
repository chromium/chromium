// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_FAKE_FAKE_CHROME_ML_API_H_
#define SERVICES_ON_DEVICE_MODEL_FAKE_FAKE_CHROME_ML_API_H_

#include "base/component_export.h"
#include "services/on_device_model/ml/chrome_ml_api.h"

namespace fake_ml {

const ChromeMLAPI* GetFakeMlApi();

COMPONENT_EXPORT(ON_DEVICE_MODEL_FAKE)
int GetActiveNonCloneSessions();

}  // namespace fake_ml

#endif  // SERVICES_ON_DEVICE_MODEL_FAKE_FAKE_CHROME_ML_API_H_
