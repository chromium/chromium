// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/gpu_blocklist.h"

namespace ml {

bool GpuBlocklist::IsGpuBlocked(const ChromeMLAPI& api) const {
  if (skip_for_testing) {
    return false;
  }

  // Do not blocklist any iOS GPUs for now.
  return false;
}

}  // namespace ml
