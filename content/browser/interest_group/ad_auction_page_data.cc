// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction_page_data.h"

namespace content {

AdAuctionPageData::AdAuctionPageData(Page& page)
    : PageUserData<AdAuctionPageData>(page) {}

AdAuctionPageData::~AdAuctionPageData() = default;

PAGE_USER_DATA_KEY_IMPL(AdAuctionPageData);

void AdAuctionPageData::AddAuctionResponseWitnessForOrigin(
    const url::Origin& origin,
    const std::string& response) {
  origin_auction_responses_map_[origin].insert(response);
}

bool AdAuctionPageData::WitnessedAuctionResponseForOrigin(
    const url::Origin& origin,
    const std::string& response) const {
  auto it = origin_auction_responses_map_.find(origin);
  if (it == origin_auction_responses_map_.end()) {
    return false;
  }

  return it->second.contains(response);
}

}  // namespace content
