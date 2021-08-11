// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_BIDDING_INTEREST_GROUP_H_
#define CONTENT_BROWSER_INTEREST_GROUP_BIDDING_INTEREST_GROUP_H_

#include "content/common/content_export.h"

#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-forward.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"

namespace content {

// BiddingInterestGroup contains both the auction worklet's Bidding interest
// group as well as several fields that are used by the browser process during
// an auction but are not needed by or should not be sent to the worklet
// process.
struct CONTENT_EXPORT BiddingInterestGroup {
  BiddingInterestGroup();
  explicit BiddingInterestGroup(
      auction_worklet::mojom::BiddingInterestGroupPtr group);
  BiddingInterestGroup(BiddingInterestGroup&&);
  BiddingInterestGroup& operator=(BiddingInterestGroup&&) = default;
  ~BiddingInterestGroup();

  auction_worklet::mojom::BiddingInterestGroupPtr group;
  // Nothing here yet.
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_BIDDING_INTEREST_GROUP_H_
