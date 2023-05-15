// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_POSITION_CACHE_TEST_UTIL_H_
#define SERVICES_DEVICE_GEOLOCATION_POSITION_CACHE_TEST_UTIL_H_

#include "services/device/geolocation/wifi_data.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {
namespace testing {

WifiData CreateUniqueWifiData(int number_of_access_points);

inline WifiData CreateDefaultUniqueWifiData() {
  return CreateUniqueWifiData(10);
}

mojom::GeopositionPtr CreateGeoposition(int offset);

}  // namespace testing
}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_POSITION_CACHE_TEST_UTIL_H_
