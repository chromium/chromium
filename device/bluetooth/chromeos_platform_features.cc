// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/chromeos_platform_features.h"

namespace chromeos::bluetooth::features {

BASE_FEATURE(kBluetoothCoredump,
             "BluetoothCoredump",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBluetoothFlossCoredump,
             "BluetoothFlossCoredump",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBluetoothFlossTelephony,
             "BluetoothFlossTelephony",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBluetoothBtsnoopInternals,
             "BluetoothBtsnoopInternals",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace chromeos::bluetooth::features
