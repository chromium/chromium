// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_CHROMEOS_PLATFORM_FEATURES_H_
#define DEVICE_BLUETOOTH_CHROMEOS_PLATFORM_FEATURES_H_

#include "base/feature_list.h"
#include "device/bluetooth/bluetooth_export.h"

namespace chromeos::bluetooth::features {

// Enables/disables the bluetooth devcoredump feature
DEVICE_BLUETOOTH_EXPORT BASE_DECLARE_FEATURE(kBluetoothCoredump);

// Enables/disables the bluetooth devcoredump feature for floss
DEVICE_BLUETOOTH_EXPORT BASE_DECLARE_FEATURE(kBluetoothFlossCoredump);

// Enables/disables the bluetooth telephony feature for floss
DEVICE_BLUETOOTH_EXPORT BASE_DECLARE_FEATURE(kBluetoothFlossTelephony);

// Enables/disables the option to collect btsnoop log from
// chrome://bluetooth-internals
DEVICE_BLUETOOTH_EXPORT BASE_DECLARE_FEATURE(kBluetoothBtsnoopInternals);

}  // namespace chromeos::bluetooth::features

#endif  // DEVICE_BLUETOOTH_CHROMEOS_PLATFORM_FEATURES_H_
