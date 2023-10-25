// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "services/on_device_model/on_device_model_service.h"

#if defined(ENABLE_ML_INTERNAL)
#include "services/on_device_model/chrome_ml_instance.h"
#endif

namespace on_device_model {

// static
bool OnDeviceModelService::PreSandboxInit() {
#if defined(ENABLE_ML_INTERNAL)
  // Ensure the library is loaded before the sandbox is initialized.
  if (!GetChromeMLInstance()) {
    LOG(ERROR) << "Unable to load ChromeML.";
    return false;
  }
#endif
  return true;
}

}  // namespace on_device_model
