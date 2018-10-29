// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/network_quality_observation_source.h"

#include "base/logging.h"

namespace net {

namespace {

static constexpr const char* kObservationSourceMapping[] = {
    "Http",
    "Tcp",
    "Quic",
    "HttpCachedEstimate",
    "HttpPlatform",
    "HttpExternalEstimate",
    "TransportCachedEstimate",
    "TransportPlatform",
    "H2Pings"};

static_assert(static_cast<size_t>(NETWORK_QUALITY_OBSERVATION_SOURCE_MAX) ==
                  arraysize(kObservationSourceMapping),
              "unhandled network quality observation source");

}  // namespace

namespace nqe {

namespace internal {

// Returns the string equivalent of |source|.
const char* GetNameForObservationSource(
    NetworkQualityObservationSource source) {
  if (source >= NETWORK_QUALITY_OBSERVATION_SOURCE_MAX) {
    NOTREACHED();
    return "";
  }
  return kObservationSourceMapping[static_cast<size_t>(source)];
}

}  // namespace internal

}  // namespace nqe

}  // namespace net