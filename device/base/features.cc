// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/base/features.h"

#include "build/build_config.h"

namespace device {

#if BUILDFLAG(IS_MAC)
const base::Feature kNewUsbBackend{"NewUsbBackend",
                                   base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
const base::Feature kNewBLEWinImplementation{"NewBLEWinImplementation",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether a more reliable GATT session handling
// implementation is used on Windows 10 1709 (RS3) and beyond.
//
// Disabled due to crbug/1120338.
const base::Feature kNewBLEGattSessionHandling{
    "NewBLEGattSessionHandling", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_WIN)

namespace features {

// Enables access to articulated hand tracking sensor input.
const base::Feature kWebXrHandInput{"WebXRHandInput",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables access to raycasting against estimated XR scene geometry.
const base::Feature kWebXrHitTest{"WebXRHitTest",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// Enables access to experimental WebXR features.
const base::Feature kWebXrIncubations{"WebXRIncubations",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
}  // namespace features
}  // namespace device
