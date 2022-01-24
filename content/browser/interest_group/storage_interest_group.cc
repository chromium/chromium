// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/storage_interest_group.h"

#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"

namespace content {

StorageInterestGroup::StorageInterestGroup() = default;
StorageInterestGroup::StorageInterestGroup(
    auction_worklet::mojom::BiddingInterestGroupPtr group) {
  this->bidding_group = std::move(group);
}
StorageInterestGroup::StorageInterestGroup(StorageInterestGroup&&) = default;
StorageInterestGroup::~StorageInterestGroup() = default;

std::ostream& operator<<(std::ostream& out,
                         const StorageInterestGroup::KAnonymityData& kanon) {
  return out << "KAnonymityData[key=`" << kanon.key << "`, k=" << kanon.k
             << ", last_updated=`" << kanon.last_updated << "`]";
}

}  // namespace content
