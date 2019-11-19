// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/base/features.h"

#include "build/build_config.h"

namespace device {

#if defined(OS_WIN)
const base::Feature kNewUsbBackend{"NewUsbBackend",
                                   base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kNewBLEWinImplementation{"NewBLEWinImplementation",
                                             base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_WIN)

#if defined(OS_CHROMEOS)
// Enables or disables the use of Bluetooth dispatcher daemon on Chrome OS.
const base::Feature kNewblueDaemon{"Newblue", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_VR)
// Controls whether the orientation sensor based device is enabled.
const base::Feature kWebXrOrientationSensorDevice {
  "WebXROrientationSensorDevice",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      // TODO(https://crbug.com/820308, https://crbug.com/773829): Enable once
      // platform specific bugs have been fixed.
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};
#endif  // BUILDFLAG(ENABLE_VR)

}  // namespace device
