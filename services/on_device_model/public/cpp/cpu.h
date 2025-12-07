// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_CPU_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_CPU_H_

#include "base/component_export.h"

namespace on_device_model {

// Whether the device is capable of running the on-device model on CPU.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
bool IsCpuCapable();

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_CPU_H_
