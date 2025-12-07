// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/public/cpp/bluetooth_features.h"

namespace features {

// When enabled, calling navigator.bluetooth.getAvailability() does not prevent
// the frame from entering the back forward cache.
BASE_FEATURE(kWebBluetoothAllowGetAvailabilityWithBfcache,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features
