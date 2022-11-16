// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/base/features.h"

#include "build/build_config.h"

namespace device {

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kNewUsbBackend,
             "NewUsbBackend",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kNewBLEWinImplementation,
             "NewBLEWinImplementation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether a more reliable GATT session handling
// implementation is used on Windows 10 1709 (RS3) and beyond.
//
// Disabled due to crbug/1120338.
BASE_FEATURE(kNewBLEGattSessionHandling,
             "NewBLEGattSessionHandling",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_VR)
// Controls whether the orientation sensor based device is enabled.
BASE_FEATURE(kWebXrOrientationSensorDevice,
             "WebXROrientationSensorDevice",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             // TODO(https://crbug.com/820308, https://crbug.com/773829): Enable
             // once platform specific bugs have been fixed.
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
#endif  // BUILDFLAG(ENABLE_VR)
namespace features {
#if BUILDFLAG(ENABLE_OPENXR)
// Controls WebXR support for the OpenXR Runtime.
BASE_FEATURE(kOpenXR, "OpenXR", base::FEATURE_ENABLED_BY_DEFAULT);

// Some WebXR features may have been enabled for ARCore, but are not yet ready
// to be plumbed up from the OpenXR backend. This feature provides a mechanism
// to gate such support in a generic way. Note that this feature should not be
// used for features we intend to ship simultaneously on both OpenXR and ArCore.
// For those features, a feature-specific flag should be created if needed.
BASE_FEATURE(kOpenXrExtendedFeatureSupport,
             "OpenXrExtendedFeatureSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether shared images are used for OpenXR Runtime
BASE_FEATURE(kOpenXRSharedImages,
             "OpenXRSharedImages",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // ENABLE_OPENXR

// Enables access to articulated hand tracking sensor input.
BASE_FEATURE(kWebXrHandInput,
             "WebXRHandInput",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables access to raycasting against estimated XR scene geometry.
BASE_FEATURE(kWebXrHitTest, "WebXRHitTest", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables access to experimental WebXR features.
BASE_FEATURE(kWebXrIncubations,
             "WebXRIncubations",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables access to WebXR composition layers.
BASE_FEATURE(kWebXrLayers, "WebXRLayers", base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
// Controls whether Web Bluetooth should support confirm-only and confirm-PIN
// pairing mode on Win/Linux
BASE_FEATURE(kWebBluetoothConfirmPairingSupport,
             "WebBluetoothConfirmPairingSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
}  // namespace features
}  // namespace device
