// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ad_auction/event_record_request_helper.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "net/http/structured_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/ad_auction/event_record.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/ad_auction.mojom.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

void AdAuctionEventRecordRequestHelper::HandleResponse(
    const net::URLRequest& request) {
  if (!base::FeatureList::IsEnabled(features::kAdAuctionEventRegistration) ||
      !url_loader_network_observer_) {
    return;
  }
  AdAuctionEventRecord::Type expected_type;
  switch (attribution_reporting_eligibility_) {
    case mojom::AttributionReportingEligibility::kEventSource:
    case mojom::AttributionReportingEligibility::kEventSourceOrTrigger:
      expected_type = AdAuctionEventRecord::Type::kView;
      break;
    case mojom::AttributionReportingEligibility::kNavigationSource:
      expected_type = AdAuctionEventRecord::Type::kClick;
      break;
    case mojom::AttributionReportingEligibility::kUnset:
    case mojom::AttributionReportingEligibility::kEmpty:
    case mojom::AttributionReportingEligibility::kTrigger:
      // Nothing to do for these requests, they're not eligible for click or
      // view events.
      return;
  }

  std::optional<std::string> ad_auction_record_event_header =
      AdAuctionEventRecord::GetAdAuctionRecordEventHeader(
          request.response_headers());
  if (!ad_auction_record_event_header) {
    return;
  }

  std::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(*ad_auction_record_event_header);
  if (!dict) {
    return;
  }

  std::optional<AdAuctionEventRecord> maybe_parsed =
      AdAuctionEventRecord::MaybeCreateFromStructuredDict(
          *dict, /*expected_type=*/expected_type,
          /*providing_origin=*/url::Origin::Create(request.url()));
  if (!maybe_parsed) {
    return;
  }

  url_loader_network_observer_->OnAdAuctionEventRecordHeaderReceived(
      std::move(*maybe_parsed));
}

}  // namespace network
