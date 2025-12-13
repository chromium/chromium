// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/base/features.h"

#include "build/build_config.h"

namespace device {

#if BUILDFLAG(IS_WIN)
// Controls whether a more reliable GATT session handling
// implementation is used on Windows 10 1709 (RS3) and beyond.
//
// Disabled due to crbug/1120338.
BASE_FEATURE(kNewBLEGattSessionHandling, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

namespace features {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
// Controls whether Web Bluetooth should support confirm-only and confirm-PIN
// pairing mode on Win/Linux
BASE_FEATURE(kWebBluetoothConfirmPairingSupport,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
// Controls whether to use uncached mode when triggering GATT discovery for
// creating a GATT connection.
BASE_FEATURE(kUncachedGattDiscoveryForGattConnection,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
// Controls whether to override LocationRequest parameters in
// LocationProviderGmsCore
BASE_FEATURE(kGmsCoreLocationRequestParamOverride,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
// Controls whether to enable Web Serial API for wired devices on Android.
BASE_FEATURE(kWebSerialWiredDevicesAndroid, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace features
}  // namespace device
