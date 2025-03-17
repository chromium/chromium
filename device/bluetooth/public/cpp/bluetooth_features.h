// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the
// services/device module.

#ifndef DEVICE_BLUETOOTH_PUBLIC_CPP_BLUETOOTH_FEATURES_H_
#define DEVICE_BLUETOOTH_PUBLIC_CPP_BLUETOOTH_FEATURES_H_

#include "base/feature_list.h"
#include "device/bluetooth/public/cpp/bluetooth_features_export.h"

namespace features {

BLUETOOTH_FEATURES_EXPORT BASE_DECLARE_FEATURE(
    kWebBluetoothAllowGetAvailabilityWithBfcache);

}  // namespace features

#endif  // DEVICE_BLUETOOTH_PUBLIC_CPP_BLUETOOTH_FEATURES_H_
