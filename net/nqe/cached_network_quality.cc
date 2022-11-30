// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/cached_network_quality.h"

namespace net::nqe::internal {

CachedNetworkQuality::CachedNetworkQuality()
    : effective_connection_type_(EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {}

CachedNetworkQuality::CachedNetworkQuality(
    EffectiveConnectionType effective_connection_type)
    : CachedNetworkQuality(base::TimeTicks::Now(),
                           NetworkQuality(),
                           effective_connection_type) {}

CachedNetworkQuality::CachedNetworkQuality(
    base::TimeTicks last_update_time,
    const NetworkQuality& network_quality,
    EffectiveConnectionType effective_connection_type)
    : last_update_time_(last_update_time),
      network_quality_(network_quality),
      effective_connection_type_(effective_connection_type) {}

CachedNetworkQuality::CachedNetworkQuality(const CachedNetworkQuality& other) =
    default;

CachedNetworkQuality::~CachedNetworkQuality() = default;

CachedNetworkQuality& CachedNetworkQuality::operator=(
    const CachedNetworkQuality& other) = default;

bool CachedNetworkQuality::OlderThan(
    const CachedNetworkQuality& cached_network_quality) const {
  return last_update_time_ < cached_network_quality.last_update_time_;
}

}  // namespace net::nqe::internal
