// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_AD_AUCTION_EVENT_RECORD_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_AD_AUCTION_EVENT_RECORD_MOJOM_TRAITS_H_

#include <vector>

#include "base/component_export.h"
#include "services/network/public/cpp/ad_auction/event_record.h"
#include "services/network/public/mojom/ad_auction.mojom.h"
#include "url/origin.h"

namespace mojo {

template <>
class COMPONENT_EXPORT(NETWORK_CPP_AD_AUCTION)
    StructTraits<::network::mojom::AdAuctionEventRecordDataView,
                 ::network::AdAuctionEventRecord> {
 public:
  static ::network::AdAuctionEventRecord::Type type(
      const ::network::AdAuctionEventRecord& record) {
    return record.type;
  }

  static const url::Origin& providing_origin(
      const ::network::AdAuctionEventRecord& record) {
    return record.providing_origin;
  }

  static const std::vector<url::Origin>& eligible_origins(
      const ::network::AdAuctionEventRecord& record) {
    return record.eligible_origins;
  }

  static bool Read(::network::mojom::AdAuctionEventRecordDataView data,
                   ::network::AdAuctionEventRecord* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_AD_AUCTION_EVENT_RECORD_MOJOM_TRAITS_H_
