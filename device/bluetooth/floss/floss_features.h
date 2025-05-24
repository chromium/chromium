// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_FEATURES_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_export.h"

namespace floss {
namespace features {

#if BUILDFLAG(IS_CHROMEOS)
DEVICE_BLUETOOTH_EXPORT BASE_DECLARE_FEATURE(kFlossEnabled);
DEVICE_BLUETOOTH_EXPORT BASE_DECLARE_FEATURE(kLLPrivacyIsAvailable);
#endif

bool DEVICE_BLUETOOTH_EXPORT IsFlossEnabled();

// Helper method to get if the current device is available to enable LL privacy.
bool DEVICE_BLUETOOTH_EXPORT IsLLPrivacyAvailable();
}  // namespace features
}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_FEATURES_H_
