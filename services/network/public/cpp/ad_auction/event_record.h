// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_AD_AUCTION_EVENT_RECORD_H_
#define SERVICES_NETWORK_PUBLIC_CPP_AD_AUCTION_EVENT_RECORD_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "net/http/structured_headers.h"
#include "services/network/public/mojom/ad_auction.mojom-shared.h"
#include "url/origin.h"

namespace net {

class HttpResponseHeaders;

}

namespace network {

// Represents the result of parsing the Ad-Auction-Record-Event header, which
// allows providing origins to record ad view or click events. Counts of those
// events are provided to eligible origins during interest group auctions.
struct COMPONENT_EXPORT(NETWORK_CPP_AD_AUCTION) AdAuctionEventRecord {
  using Type = network::mojom::AdAuctionEventRecord_Type;

  AdAuctionEventRecord();
  ~AdAuctionEventRecord();
  AdAuctionEventRecord(AdAuctionEventRecord&&);
  AdAuctionEventRecord& operator=(AdAuctionEventRecord&&);

  // Finds the Ad-Auction-Record-Event header value, if it exists. If
  // `headers` is null, returns std::nullopt.
  static std::optional<std::string> GetAdAuctionRecordEventHeader(
      const net::HttpResponseHeaders* headers);

  // Parses and validates `dict`, producing an AdAuctionEventRecord if
  // successful, and std::nullopt otherwise.
  static std::optional<AdAuctionEventRecord> MaybeCreateFromStructuredDict(
      const net::structured_headers::Dictionary& dict,
      Type expected_type,
      const url::Origin& providing_origin);

  // Validates that the origins are all HTTPS. Note that this does *not* check
  // if origins are allowed to use the API.
  bool IsValid() const;

  // Whether this event is a view or a click.
  Type type = Type::kUninitialized;

  // The origin that served the header that was parsed into this record.
  url::Origin providing_origin;

  // The set of origins that may include this event among their view and click
  // counts.
  std::vector<url::Origin> eligible_origins;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_AD_AUCTION_EVENT_RECORD_H_
