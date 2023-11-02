// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_LOCATION_SYSTEM_PERMISSION_STATUS_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_LOCATION_SYSTEM_PERMISSION_STATUS_H_

namespace device {

// System permission state.
enum class LocationSystemPermissionStatus {
  kNotDetermined = 0,
  kDenied = 1,
  kAllowed = 2,
  kMaxValue = kAllowed
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_LOCATION_SYSTEM_PERMISSION_STATUS_H_
