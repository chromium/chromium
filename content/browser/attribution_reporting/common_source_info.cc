// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/common_source_info.h"

#include <utility>

#include "base/check_op.h"
#include "base/cxx17_backports.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace content {

base::Time CommonSourceInfo::GetExpiryTime(
    absl::optional<base::TimeDelta> declared_expiry,
    base::Time source_time,
    AttributionSourceType source_type) {
  constexpr base::TimeDelta kMinImpressionExpiry = base::Days(1);

  // Default to the maximum expiry time.
  base::TimeDelta expiry =
      declared_expiry.value_or(kDefaultAttributionSourceExpiry);

  // Expiry time for event sources must be a whole number of days.
  if (source_type == AttributionSourceType::kEvent)
    expiry = expiry.RoundToMultiple(base::Days(1));

  // If the impression specified its own expiry, clamp it to the minimum and
  // maximum.
  return source_time + base::clamp(expiry, kMinImpressionExpiry,
                                   kDefaultAttributionSourceExpiry);
}

CommonSourceInfo::CommonSourceInfo(uint64_t source_event_id,
                                   url::Origin source_origin,
                                   url::Origin destination_origin,
                                   url::Origin reporting_origin,
                                   base::Time source_time,
                                   base::Time expiry_time,
                                   AttributionSourceType source_type,
                                   int64_t priority,
                                   AttributionFilterData filter_data,
                                   absl::optional<uint64_t> debug_key,
                                   AttributionAggregationKeys aggregation_keys)
    : source_event_id_(source_event_id),
      source_origin_(std::move(source_origin)),
      destination_origin_(std::move(destination_origin)),
      reporting_origin_(std::move(reporting_origin)),
      source_time_(source_time),
      expiry_time_(expiry_time),
      source_type_(source_type),
      priority_(priority),
      filter_data_(std::move(filter_data)),
      debug_key_(debug_key),
      aggregation_keys_(std::move(aggregation_keys)) {
  // 30 days is the max allowed expiry for an impression.
  DCHECK_GE(base::Days(30), expiry_time - source_time);
  // The impression must expire strictly after it occurred.
  DCHECK_GT(expiry_time, source_time);
  DCHECK(network::IsOriginPotentiallyTrustworthy(source_origin_));
  DCHECK(network::IsOriginPotentiallyTrustworthy(reporting_origin_));
  DCHECK(network::IsOriginPotentiallyTrustworthy(destination_origin_));
}

CommonSourceInfo::~CommonSourceInfo() = default;

CommonSourceInfo::CommonSourceInfo(const CommonSourceInfo&) = default;

CommonSourceInfo::CommonSourceInfo(CommonSourceInfo&&) = default;

CommonSourceInfo& CommonSourceInfo::operator=(const CommonSourceInfo&) =
    default;

CommonSourceInfo& CommonSourceInfo::operator=(CommonSourceInfo&&) = default;

net::SchemefulSite CommonSourceInfo::DestinationSite() const {
  return net::SchemefulSite(destination_origin_);
}

net::SchemefulSite CommonSourceInfo::SourceSite() const {
  return net::SchemefulSite(source_origin_);
}

}  // namespace content
