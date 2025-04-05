// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/attribution_request_helper.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/attribution/request_headers_internal.h"
#include "services/network/public/cpp/attribution_utils.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"

namespace network {

namespace {

using ::network::mojom::AttributionReportingEligibility;

}  // namespace


// https://wicg.github.io/attribution-reporting-api/#mark-a-request-for-attribution-reporting-eligibility
net::HttpRequestHeaders ComputeAttributionReportingHeaders(
    const ResourceRequest& request) {
  net::HttpRequestHeaders headers;
  if (request.attribution_reporting_eligibility ==
      AttributionReportingEligibility::kUnset) {
    return headers;
  }

  const bool is_attribution_reporting_support_set =
      request.attribution_reporting_support !=
      network::mojom::AttributionSupport::kUnset;

  AttributionReportingEligibility effective_eligibility =
      is_attribution_reporting_support_set &&
              !HasAttributionSupport(request.attribution_reporting_support)
          ? AttributionReportingEligibility::kEmpty
          : request.attribution_reporting_eligibility;

  uint64_t grease_bits = base::RandUint64();

  std::string reporting_eligible_header =
      SerializeAttributionReportingEligibleHeader(
          effective_eligibility,
          AttributionReportingHeaderGreaseOptions::FromBits(grease_bits &
                                                            0xff));
  grease_bits >>= 8;

  headers.SetHeader("Attribution-Reporting-Eligible",
                    std::move(reporting_eligible_header));

  if (base::FeatureList::IsEnabled(features::kAdAuctionEventRegistration)) {
    std::string ad_auction_registration_eligible =
        SerializeAdAuctionRegistrationEligibleHeader(
            request.attribution_reporting_eligibility,
            AttributionReportingHeaderGreaseOptions::FromBits(grease_bits &
                                                              0xff));
    grease_bits >>= 8;
    headers.SetHeader("Sec-Ad-Auction-Event-Recording-Eligible",
                      std::move(ad_auction_registration_eligible));
  }

  base::UmaHistogramEnumeration("Conversions.RequestSupportHeader",
                                request.attribution_reporting_support);

  if (is_attribution_reporting_support_set) {
    headers.SetHeader("Attribution-Reporting-Support",
                      GetAttributionSupportHeader(
                          request.attribution_reporting_support,
                          AttributionReportingHeaderGreaseOptions::FromBits(
                              grease_bits & 0xff)));
    grease_bits >>= 8;
  }

  return headers;
}

}  // namespace network
