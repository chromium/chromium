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
// Enables or disables the use of newblue Bluetooth daemon on Chrome OS.
const base::Feature kNewblueDaemon{"Newblue",
                                   base::FEATURE_DISABLED_BY_DEFAULT};
// Shows all Bluetooth devices in UI (System Tray/Settings Page).
// Needed for working on the early integration with NewBlue.
// TODO(crbug.com/862492): Remove this feature once NewBlue gets stable.
const base::Feature kUnfilteredBluetoothDevices{
    "UnfilteredBluetoothDevices", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
// Controls whether the CTAP2 implementation should use a built-in platform
// authenticator, where available.
const base::Feature kWebAuthTouchId{"WebAuthenticationTouchId",
                                    base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_MACOSX)

const base::Feature kNewCtap2Device{"WebAuthenticationCtap2",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace device
