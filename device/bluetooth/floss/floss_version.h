// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_VERSION_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_VERSION_H_

#include "base/version.h"
#include "device/bluetooth/bluetooth_export.h"

namespace floss::version {

// Create a base::Version object.
base::Version DEVICE_BLUETOOTH_EXPORT IntoVersion(uint32_t version);

// Extract major version.
uint32_t GetMajorVersion(uint32_t version);

// Extract minor version.
uint32_t GetMinorVersion(uint32_t version);

// Minimal supported version of the Floss API exported by Floss daemon.
base::Version DEVICE_BLUETOOTH_EXPORT GetMinimalSupportedVersion();

// Maximal supported version of the Floss API exported by Floss daemon.
base::Version DEVICE_BLUETOOTH_EXPORT GetMaximalSupportedVersion();

}  // namespace floss::version
#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_VERSION_H_
