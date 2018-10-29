// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/network_quality_observation.h"
#include "base/macros.h"

namespace net {

namespace nqe {

namespace internal {

Observation::Observation(int32_t value,
                         base::TimeTicks timestamp,
                         int32_t signal_strength,
                         NetworkQualityObservationSource source)
    : Observation(value, timestamp, signal_strength, source, base::nullopt) {}

Observation::Observation(int32_t value,
                         base::TimeTicks timestamp,
                         int32_t signal_strength,
                         NetworkQualityObservationSource source,
                         const base::Optional<IPHash>& host)
    : value_(value),
      timestamp_(timestamp),
      signal_strength_(signal_strength),
      source_(source),
      host_(host) {
  DCHECK(!timestamp_.is_null());
  DCHECK(signal_strength_ == INT32_MIN ||
         (signal_strength_ >= 0 && signal_strength_ <= 4));
}

Observation::Observation(const Observation& other) = default;

Observation& Observation::operator=(const Observation& other) = default;

Observation::~Observation() = default;

std::vector<ObservationCategory> Observation::GetObservationCategories() const {
  std::vector<ObservationCategory> observation_categories;
  switch (source_) {
    case NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP:
    case NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP_CACHED_ESTIMATE:
    case NETWORK_QUALITY_OBSERVATION_SOURCE_DEFAULT_HTTP_FROM_PLATFORM:
    case DEPRECATED_NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP_EXTERNAL_ESTIMATE:
      observation_categories.push_back(
          ObservationCategory::OBSERVATION_CATEGORY_HTTP);
      return observation_categories;
    case NETWORK_QUALITY_OBSERVATION_SOURCE_TRANSPORT_CACHED_ESTIMATE:
    case NETWORK_QUALITY_OBSERVATION_SOURCE_DEFAULT_TRANSPORT_FROM_PLATFORM:
    case NETWORK_QUALITY_OBSERVATION_SOURCE_TCP:
      observation_categories.push_back(
          ObservationCategory::OBSERVATION_CATEGORY_TRANSPORT);
      return observation_categories;
    case NETWORK_QUALITY_OBSERVATION_SOURCE_QUIC:
    case NETWORK_QUALITY_OBSERVATION_SOURCE_H2_PINGS:
      observation_categories.push_back(
          ObservationCategory::OBSERVATION_CATEGORY_TRANSPORT);
      observation_categories.push_back(
          ObservationCategory::OBSERVATION_CATEGORY_END_TO_END);
      return observation_categories;
    case NETWORK_QUALITY_OBSERVATION_SOURCE_MAX:
      NOTREACHED();
      return observation_categories;
  }
  NOTREACHED();
  return observation_categories;
}

}  // namespace internal

}  // namespace nqe

}  // namespace net
