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
BASE_FEATURE(kNewBLEGattSessionHandling,
             "NewBLEGattSessionHandling",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

namespace features {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
// Controls whether Web Bluetooth should support confirm-only and confirm-PIN
// pairing mode on Win/Linux
BASE_FEATURE(kWebBluetoothConfirmPairingSupport,
             "WebBluetoothConfirmPairingSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

}  // namespace features
}  // namespace device
