// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_AD_AUCTION_EVENT_RECORD_REQUEST_HELPER_H_
#define SERVICES_NETWORK_AD_AUCTION_EVENT_RECORD_REQUEST_HELPER_H_

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/types/optional_ref.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy.h"
#include "services/network/public/mojom/ad_auction.mojom-forward.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom-forward.h"

namespace network {

// When constructed for requests with eligible values of
// `attribution_reporting_eligibility`, parses Ad-Auction-Record-Event response
// headers headers and sends the result, if successful, to
// `OnAdAuctionEventRecordHeaderReceived()` on `url_loader_network_observer_`.
class AdAuctionEventRecordRequestHelper {
 public:
  // `url_loader_network_observer` may be null -- if non-null, it must outlive
  // this instance.
  AdAuctionEventRecordRequestHelper(
      mojom::AttributionReportingEligibility attribution_reporting_eligibility,
      mojom::URLLoaderNetworkServiceObserver* url_loader_network_observer)
      : attribution_reporting_eligibility_(attribution_reporting_eligibility),
        url_loader_network_observer_(url_loader_network_observer) {}

  // If `attribution_reporting_eligibility_` is one of the eligible values,
  // parses the Ad-Auction-Record-Event header in `request`, sending the result
  // to `OnAdAuctionEventRecordHeaderReceived()` on
  // `url_loader_network_observer_`.
  void HandleResponse(
      const net::URLRequest& request,
      base::optional_ref<const network::PermissionsPolicy> permissions_policy);

 private:
  mojom::AttributionReportingEligibility attribution_reporting_eligibility_ =
      mojom::AttributionReportingEligibility::kUnset;

  const raw_ptr<mojom::URLLoaderNetworkServiceObserver>
      url_loader_network_observer_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_AD_AUCTION_EVENT_RECORD_REQUEST_HELPER_H_
