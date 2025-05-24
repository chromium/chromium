// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_CPP_SWITCHES_H_
#define DEVICE_VR_PUBLIC_CPP_SWITCHES_H_

#include "base/component_export.h"

namespace device::switches {
COMPONENT_EXPORT(VR_FEATURES)
extern const char kWebXrHandAnonymizationStrategy[];
COMPONENT_EXPORT(VR_FEATURES)
extern const char kWebXrHandAnonymizationStrategyNone[];
COMPONENT_EXPORT(VR_FEATURES)
extern const char kWebXrHandAnonymizationStrategyRuntime[];
COMPONENT_EXPORT(VR_FEATURES)
extern const char kWebXrHandAnonymizationStrategyFallback[];
}  // namespace device::switches

#endif  // DEVICE_VR_PUBLIC_CPP_SWITCHES_H_
