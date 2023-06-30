// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/wifi_data.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>

#include "base/check.h"

namespace device {

WifiData::WifiData() = default;

WifiData::WifiData(const WifiData& other) = default;

WifiData& WifiData::operator=(const WifiData& other) = default;

WifiData::~WifiData() = default;

bool WifiData::DiffersSignificantly(const WifiData& other) const {
  // More than 4 or 50% of access points added or removed is significant.
  static const size_t kMinChangedAccessPoints = 4;
  const size_t min_ap_count =
      std::min(access_point_data.size(), other.access_point_data.size());
  const size_t max_ap_count =
      std::max(access_point_data.size(), other.access_point_data.size());
  const size_t difference_threadhold =
      std::min(kMinChangedAccessPoints, min_ap_count / 2);
  if (max_ap_count > min_ap_count + difference_threadhold)
    return true;
  // Compute size of intersection of old and new sets.
  size_t num_common = 0;
  for (AccessPointDataSet::const_iterator iter = access_point_data.begin();
       iter != access_point_data.end(); iter++) {
    if (other.access_point_data.find(*iter) != other.access_point_data.end()) {
      ++num_common;
    }
  }
  DCHECK(num_common <= min_ap_count);

  // Test how many have changed.
  return max_ap_count > num_common + difference_threadhold;
}

}  // namespace device
