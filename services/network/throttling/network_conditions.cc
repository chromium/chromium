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
    : NetworkConditions(offline, 0, 0, 0, 0.0, 0, false) {}

NetworkConditions::NetworkConditions(bool offline,
                                     double latency,
                                     double download_throughput,
                                     double upload_throughput)
    : NetworkConditions(offline,
                        latency,
                        download_throughput,
                        upload_throughput,
                        0.0,
                        0,
                        false) {}

NetworkConditions::NetworkConditions(bool offline,
                                     double latency,
                                     double download_throughput,
                                     double upload_throughput,
                                     double packet_loss,
                                     int packet_queue_length,
                                     bool packet_reordering)
    : offline_(offline),
      latency_(std::max(latency, 0.0)),
      download_throughput_(std::max(download_throughput, 0.0)),
      upload_throughput_(std::max(upload_throughput, 0.0)),
      packet_loss_(std::max(packet_loss, 0.0)),
      packet_queue_length_(packet_queue_length),
      packet_reordering_(packet_reordering) {}

NetworkConditions::~NetworkConditions() {}

bool NetworkConditions::IsThrottling() const {
  return !offline_ && ((latency_ != 0) || (download_throughput_ != 0.0) ||
                       (upload_throughput_ != 0) || (packet_loss_ != 0.0) ||
                       (packet_queue_length_ != 0) || packet_reordering_);
}

}  // namespace network
