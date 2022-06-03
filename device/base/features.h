// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BASE_FEATURES_H_
#define DEVICE_BASE_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "device/base/device_base_export.h"

namespace device {

#if BUILDFLAG(IS_MAC)
DEVICE_BASE_EXPORT extern const base::Feature kNewUsbBackend;
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
DEVICE_BASE_EXPORT extern const base::Feature kNewBLEWinImplementation;
DEVICE_BASE_EXPORT extern const base::Feature kNewBLEGattSessionHandling;
#endif  // BUILDFLAG(IS_WIN)

// New features should be added to the device::features namespace.

namespace features {

DEVICE_BASE_EXPORT extern const base::Feature kWebXrHandInput;
DEVICE_BASE_EXPORT extern const base::Feature kWebXrHitTest;
DEVICE_BASE_EXPORT extern const base::Feature kWebXrIncubations;

}  // namespace features
}  // namespace device

#endif  // DEVICE_BASE_FEATURES_H_
