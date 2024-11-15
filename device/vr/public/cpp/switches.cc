// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/public/cpp/switches.h"

namespace device::switches {
const char kWebXrHandAnonymizationStrategy[] =
    "webxr-hand-anonymization-strategy";
const char kWebXrHandAnonymizationStrategyNone[] = "none";
const char kWebXrHandAnonymizationStrategyRuntime[] = "runtime";
const char kWebXrHandAnonymizationStrategyFallback[] = "fallback";
}  // namespace device::switches
