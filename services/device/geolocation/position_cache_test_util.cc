// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/position_cache_test_util.h"

#include <cmath>

#include "base/check_op.h"
#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"

namespace device {
namespace testing {

WifiData CreateUniqueWifiData(int number_of_access_points) {
  WifiData wifi_data;
  for (int i = 0; i < number_of_access_points; ++i) {
    AccessPointData single_access_point;
    single_access_point.channel = 2;
    single_access_point.mac_address = base::ASCIIToUTF16(base::GenerateGUID());
    single_access_point.radio_signal_strength = 4;
    single_access_point.signal_to_noise = 5;
    single_access_point.ssid = base::ASCIIToUTF16(base::GenerateGUID());
    wifi_data.access_point_data.insert(single_access_point);
  }
  return wifi_data;
}

mojom::Geoposition CreateGeoposition(int offset) {
  DCHECK_LT(std::abs(offset), 90) << "latitudes larger than 90 degrees are not "
                                     "possible on spherical planets";
  mojom::Geoposition position;
  position.latitude = offset;
  position.longitude = offset;
  return position;
}

}  // namespace testing
}  // namespace device
