// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_FEATURES_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_FEATURES_H_

#include "base/feature_list.h"
#include "build/chromeos_buildflags.h"
#include "device/bluetooth/bluetooth_export.h"

namespace floss {
namespace features {

// The feature is defined in Ash and propagated to Lacros over crosapi.
#if BUILDFLAG(IS_CHROMEOS_ASH)
DEVICE_BLUETOOTH_EXPORT BASE_DECLARE_FEATURE(kFlossEnabled);
DEVICE_BLUETOOTH_EXPORT BASE_DECLARE_FEATURE(kFlossIsAvailable);
DEVICE_BLUETOOTH_EXPORT BASE_DECLARE_FEATURE(kFlossIsAvailabilityCheckNeeded);
DEVICE_BLUETOOTH_EXPORT BASE_DECLARE_FEATURE(kLLPrivacyIsAvailable);
#endif

// A helper method that has the appropriate behavior on both Ash and Lacros.
bool DEVICE_BLUETOOTH_EXPORT IsFlossEnabled();

// Helper method to get if the current device is available to enable Floss.
bool DEVICE_BLUETOOTH_EXPORT IsFlossAvailable();

// Helper method to get if it needs to check Floss availability.
bool DEVICE_BLUETOOTH_EXPORT IsFlossAvailabilityCheckNeeded();

// Helper method to get if the current device is available to enable LL privacy.
bool DEVICE_BLUETOOTH_EXPORT IsLLPrivacyAvailable();
}  // namespace features
}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_FEATURES_H_
