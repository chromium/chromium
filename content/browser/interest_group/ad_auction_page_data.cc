// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction_page_data.h"

#include "base/no_destructor.h"

namespace content {

AdAuctionPageData::AdAuctionPageData(Page& page)
    : PageUserData<AdAuctionPageData>(page) {}

AdAuctionPageData::~AdAuctionPageData() = default;

PAGE_USER_DATA_KEY_IMPL(AdAuctionPageData);

void AdAuctionPageData::AddAuctionResultWitnessForOrigin(
    const url::Origin& origin,
    const std::string& response) {
  origin_auction_result_map_[origin].insert(response);
}

bool AdAuctionPageData::WitnessedAuctionResultForOrigin(
    const url::Origin& origin,
    const std::string& response) const {
  auto it = origin_auction_result_map_.find(origin);
  if (it == origin_auction_result_map_.end()) {
    return false;
  }

  return it->second.contains(response);
}

void AdAuctionPageData::AddAuctionSignalsWitnessForOrigin(
    const url::Origin& origin,
    const std::string& response) {
  origin_auction_signals_map_[origin].insert(response);
}

const std::set<std::string>& AdAuctionPageData::GetAuctionSignalsForOrigin(
    const url::Origin& origin) const {
  auto it = origin_auction_signals_map_.find(origin);
  if (it == origin_auction_signals_map_.end()) {
    static base::NoDestructor<std::set<std::string>> kEmptySet;
    return *kEmptySet;
  }

  return it->second;
}

void AdAuctionPageData::AddAuctionAdditionalBidsWitnessForOrigin(
    const url::Origin& origin,
    const std::map<std::string, std::vector<std::string>>&
        nonce_additional_bids_map) {
  CHECK(!nonce_additional_bids_map.empty());

  std::map<std::string, std::vector<std::string>>&
      existing_nonce_additional_bids_map =
          origin_nonce_additional_bids_map_[origin];

  for (const auto& [nonce, additional_bids] : nonce_additional_bids_map) {
    CHECK(!additional_bids.empty());

    std::vector<std::string>& existing_additional_bids =
        existing_nonce_additional_bids_map[nonce];

    existing_additional_bids.insert(existing_additional_bids.end(),
                                    additional_bids.begin(),
                                    additional_bids.end());
  }
}

std::vector<std::string>
AdAuctionPageData::TakeAuctionAdditionalBidsForOriginAndNonce(
    const url::Origin& origin,
    const std::string& nonce) {
  auto origin_map_it = origin_nonce_additional_bids_map_.find(origin);
  if (origin_map_it == origin_nonce_additional_bids_map_.end()) {
    return {};
  }

  std::map<std::string, std::vector<std::string>>& nonce_additional_bids_map =
      origin_map_it->second;

  auto nonce_map_it = nonce_additional_bids_map.find(nonce);
  if (nonce_map_it == nonce_additional_bids_map.end()) {
    return {};
  }

  auto extracted_nonce_and_additional_bids =
      nonce_additional_bids_map.extract(nonce_map_it);
  return std::move(extracted_nonce_and_additional_bids.mapped());
}

void AdAuctionPageData::RegisterAdAuctionRequestContext(
    const base::Uuid& id,
    AdAuctionRequestContext context) {
  context_map_.insert(std::make_pair(id, std::move(context)));
}

AdAuctionRequestContext* AdAuctionPageData::GetContextForAdAuctionRequest(
    const base::Uuid& id) {
  auto it = context_map_.find(id);
  if (it == context_map_.end()) {
    return nullptr;
  }
  return &it->second;
}

AdAuctionRequestContext::AdAuctionRequestContext(
    url::Origin seller,
    base::flat_map<url::Origin, std::vector<std::string>> group_names,
    quiche::ObliviousHttpRequest::Context context)
    : seller(std::move(seller)),
      group_names(std::move(group_names)),
      context(std::move(context)) {}
AdAuctionRequestContext::AdAuctionRequestContext(
    AdAuctionRequestContext&& other) = default;
AdAuctionRequestContext::~AdAuctionRequestContext() = default;

}  // namespace content
