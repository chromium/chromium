// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/throttling/network_conditions.h"

namespace network {

NetworkConditions::NetworkConditions() : NetworkConditions(false) {}

NetworkConditions::NetworkConditions(bool offline)
    : NetworkConditions(offline, 0, 0, 0) {}

NetworkConditions::NetworkConditions(bool offline,
                                     double latency,
                                     double download_throughput,
                                     double upload_throughput)
    : offline_(offline),
      latency_(latency),
      download_throughput_(download_throughput),
      upload_throughput_(upload_throughput) {}

NetworkConditions::~NetworkConditions() {}

bool NetworkConditions::IsThrottling() const {
  return !offline_ && ((latency_ != 0) || (download_throughput_ != 0.0) ||
                       (upload_throughput_ != 0));
}

}  // namespace network
