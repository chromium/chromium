// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_CHROMEOS_PLATFORM_FEATURES_H_
#define DEVICE_BLUETOOTH_CHROMEOS_PLATFORM_FEATURES_H_

#include "base/feature_list.h"
#include "device/bluetooth/bluetooth_export.h"

namespace chromeos::bluetooth::features {

// Enables/disables the bluetooth devcoredump feature
extern const DEVICE_BLUETOOTH_EXPORT base::Feature kBluetoothCoredump;

}  // namespace chromeos::bluetooth::features

#endif  // DEVICE_BLUETOOTH_CHROMEOS_PLATFORM_FEATURES_H_
