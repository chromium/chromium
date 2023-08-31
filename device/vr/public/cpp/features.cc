// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/public/cpp/features.h"

#include "base/feature_list.h"
#include "device/vr/buildflags/buildflags.h"

namespace device::features {
// Enables access to articulated hand tracking sensor input.
BASE_FEATURE(kWebXrHandInput,
             "WebXRHandInput",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables access to experimental WebXR features.
BASE_FEATURE(kWebXrIncubations,
             "WebXRIncubations",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag for the WebXRInternals debugging page.
BASE_FEATURE(kWebXrInternals,
             "WebXrInternals",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables access to WebXR composition layers.
BASE_FEATURE(kWebXrLayers, "WebXRLayers", base::FEATURE_DISABLED_BY_DEFAULT);

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

#if BUILDFLAG(IS_ANDROID)
// Controls whether or not SharedBuffer support is enabled. This is enabled by
// default; but some platforms (e.g. below O) cannot support the feature; while
// on other GPUs there may be quirks that prevent using the shared buffers.
BASE_FEATURE(kWebXrSharedBuffers,
             "WebXrSharedBuffers",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(ENABLE_CARDBOARD)
// Controls WebXR support for the Cardboard SDK Runtime. Note that enabling
// this will also disable the GVR runtime.
BASE_FEATURE(kEnableCardboard, "Cardboard", base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // ENABLE_CARDBOARD

#if BUILDFLAG(ENABLE_OPENXR)
// Controls WebXR support for the OpenXR Runtime.
BASE_FEATURE(kOpenXR,
             "OpenXR",
             BUILDFLAG(IS_WIN) ? base::FEATURE_ENABLED_BY_DEFAULT
                               : base::FEATURE_DISABLED_BY_DEFAULT);

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
}  // namespace device::features
