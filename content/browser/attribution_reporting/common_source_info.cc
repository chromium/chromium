// Copyright 2022 The Chromium Authors. All rights reserved.
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
    base::Time impression_time,
    AttributionSourceType source_type) {
  constexpr base::TimeDelta kMinImpressionExpiry = base::Days(1);
  constexpr base::TimeDelta kDefaultImpressionExpiry = base::Days(30);

  // Default to the maximum expiry time.
  base::TimeDelta expiry = declared_expiry.value_or(kDefaultImpressionExpiry);

  // Expiry time for event sources must be a whole number of days.
  if (source_type == AttributionSourceType::kEvent)
    expiry = expiry.RoundToMultiple(base::Days(1));

  // If the impression specified its own expiry, clamp it to the minimum and
  // maximum.
  return impression_time +
         base::clamp(expiry, kMinImpressionExpiry, kDefaultImpressionExpiry);
}

CommonSourceInfo::CommonSourceInfo(uint64_t source_event_id,
                                   url::Origin impression_origin,
                                   url::Origin conversion_origin,
                                   url::Origin reporting_origin,
                                   base::Time impression_time,
                                   base::Time expiry_time,
                                   AttributionSourceType source_type,
                                   int64_t priority,
                                   AttributionFilterData filter_data,
                                   absl::optional<uint64_t> debug_key,
                                   AttributionAggregationKeys aggregation_keys)
    : source_event_id_(source_event_id),
      impression_origin_(std::move(impression_origin)),
      conversion_origin_(std::move(conversion_origin)),
      reporting_origin_(std::move(reporting_origin)),
      impression_time_(impression_time),
      expiry_time_(expiry_time),
      source_type_(source_type),
      priority_(priority),
      filter_data_(std::move(filter_data)),
      debug_key_(debug_key),
      aggregation_keys_(std::move(aggregation_keys)) {
  // 30 days is the max allowed expiry for an impression.
  DCHECK_GE(base::Days(30), expiry_time - impression_time);
  // The impression must expire strictly after it occurred.
  DCHECK_GT(expiry_time, impression_time);
  DCHECK(network::IsOriginPotentiallyTrustworthy(impression_origin_));
  DCHECK(network::IsOriginPotentiallyTrustworthy(reporting_origin_));
  DCHECK(network::IsOriginPotentiallyTrustworthy(conversion_origin_));
}

CommonSourceInfo::~CommonSourceInfo() = default;

CommonSourceInfo::CommonSourceInfo(const CommonSourceInfo&) = default;

CommonSourceInfo::CommonSourceInfo(CommonSourceInfo&&) = default;

CommonSourceInfo& CommonSourceInfo::operator=(const CommonSourceInfo&) =
    default;

CommonSourceInfo& CommonSourceInfo::operator=(CommonSourceInfo&&) = default;

net::SchemefulSite CommonSourceInfo::ConversionDestination() const {
  return net::SchemefulSite(conversion_origin_);
}

net::SchemefulSite CommonSourceInfo::ImpressionSite() const {
  return net::SchemefulSite(impression_origin_);
}

}  // namespace content
