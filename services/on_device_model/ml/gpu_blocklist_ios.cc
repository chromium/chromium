// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/gpu_blocklist.h"

namespace ml {

COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
bool IsGpuBlocked(const ChromeMLAPI& api) {
  return false;
}

}  // namespace ml
