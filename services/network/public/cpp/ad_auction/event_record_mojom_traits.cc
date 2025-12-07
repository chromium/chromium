// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ad_auction/event_record_mojom_traits.h"

#include "services/network/public/cpp/ad_auction/event_record.h"
#include "services/network/public/mojom/ad_auction.mojom.h"

namespace mojo {

// static
bool StructTraits<::network::mojom::AdAuctionEventRecordDataView,
                  ::network::AdAuctionEventRecord>::
    Read(network::mojom::AdAuctionEventRecordDataView data,
         network::AdAuctionEventRecord* out) {
  if (!data.ReadType(&out->type) ||
      !data.ReadProvidingOrigin(&out->providing_origin) ||
      !data.ReadEligibleOrigins(&out->eligible_origins)) {
    return false;
  }
  return out->IsValid();
}

}  // namespace mojo
