// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_CHROME_ML_INSTANCE_H_
#define SERVICES_ON_DEVICE_MODEL_CHROME_ML_INSTANCE_H_

#include "third_party/ml/public/chrome_ml.h"

namespace on_device_model {

// Retrieves a lazily-initialized global instance of the ChromeML module; or
// null if the requisite library could not be loaded.
ml::ChromeML* GetChromeMLInstance();

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_CHROME_ML_INSTANCE_H_
