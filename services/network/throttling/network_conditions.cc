// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/throttling/network_conditions.h"

#include <algorithm>

namespace network {

NetworkConditions::NetworkConditions() : NetworkConditions(false) {}
NetworkConditions::NetworkConditions(const NetworkConditions&) = default;
NetworkConditions& NetworkConditions::operator=(const NetworkConditions&) =
    default;

NetworkConditions::NetworkConditions(bool offline)
    : NetworkConditions(offline, 0, 0, 0) {}

NetworkConditions::NetworkConditions(bool offline,
                                     double latency,
                                     double download_throughput,
                                     double upload_throughput)
    : offline_(offline),
      latency_(std::max(latency, 0.0)),
      download_throughput_(std::max(download_throughput, 0.0)),
      upload_throughput_(std::max(upload_throughput, 0.0)) {}

NetworkConditions::~NetworkConditions() {}

bool NetworkConditions::IsThrottling() const {
  return !offline_ && ((latency_ != 0) || (download_throughput_ != 0.0) ||
                       (upload_throughput_ != 0));
}

}  // namespace network
