// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BASE_FEATURES_H_
#define DEVICE_BASE_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "device/base/device_base_export.h"

namespace device {

#if BUILDFLAG(IS_WIN)
DEVICE_BASE_EXPORT BASE_DECLARE_FEATURE(kNewBLEGattSessionHandling);
#endif  // BUILDFLAG(IS_WIN)

// New features should be added to the device::features namespace.

namespace features {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
DEVICE_BASE_EXPORT BASE_DECLARE_FEATURE(kWebBluetoothConfirmPairingSupport);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

}  // namespace features
}  // namespace device

#endif  // DEVICE_BASE_FEATURES_H_
