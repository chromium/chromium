// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_host_utils.h"

#include <utility>

#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_source.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "url/origin.h"

namespace content {

namespace attribution_host_utils {

void VerifyAndStoreImpression(AttributionSourceType source_type,
                              const url::Origin& impression_origin,
                              const blink::Impression& impression,
                              AttributionManager& attribution_manager,
                              base::Time impression_time) {
  // Convert |impression| into a StorableImpression that can be forwarded to
  // storage. If a reporting origin was not provided, default to the impression
  // origin for reporting.
  const url::Origin& reporting_origin = !impression.reporting_origin
                                            ? impression_origin
                                            : *impression.reporting_origin;

  // Conversion measurement is only allowed in secure contexts.
  if (!network::IsOriginPotentiallyTrustworthy(impression_origin) ||
      !network::IsOriginPotentiallyTrustworthy(reporting_origin) ||
      !network::IsOriginPotentiallyTrustworthy(
          impression.conversion_destination)) {
    return;
  }

  StorableSource storable_impression(
      // Impression data doesn't need to be sanitized.
      CommonSourceInfo(
          impression.impression_data, impression_origin,
          impression.conversion_destination, reporting_origin, impression_time,
          CommonSourceInfo::GetExpiryTime(impression.expiry, impression_time,
                                          source_type),
          source_type, impression.priority, AttributionFilterData(),
          /*debug_key=*/absl::nullopt, AttributionAggregatableSource()));

  attribution_manager.HandleSource(std::move(storable_impression));
}

}  // namespace attribution_host_utils

}  // namespace content
