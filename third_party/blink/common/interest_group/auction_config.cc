// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/auction_config.h"

namespace blink {

AuctionConfig::NonSharedParams::NonSharedParams() = default;
AuctionConfig::NonSharedParams::NonSharedParams(const NonSharedParams&) =
    default;
AuctionConfig::NonSharedParams::NonSharedParams(NonSharedParams&&) = default;
AuctionConfig::NonSharedParams::~NonSharedParams() = default;

AuctionConfig::NonSharedParams& AuctionConfig::NonSharedParams::operator=(
    const NonSharedParams&) = default;
AuctionConfig::NonSharedParams& AuctionConfig::NonSharedParams::operator=(
    NonSharedParams&&) = default;

bool AuctionConfig::NonSharedParams::operator==(
    const NonSharedParams& other) const {
  return std::tie(interest_group_buyers, auction_signals, seller_signals,
                  seller_timeout, per_buyer_signals, per_buyer_timeouts,
                  all_buyers_timeout, per_buyer_group_limits,
                  all_buyers_group_limit, component_auctions) !=
         std::tie(other.interest_group_buyers, other.auction_signals,
                  other.seller_signals, other.seller_timeout,
                  other.per_buyer_signals, other.per_buyer_timeouts,
                  other.all_buyers_timeout, other.per_buyer_group_limits,
                  other.all_buyers_group_limit, other.component_auctions);
}

AuctionConfig::AuctionConfig() = default;
AuctionConfig::AuctionConfig(const AuctionConfig&) = default;
AuctionConfig::AuctionConfig(AuctionConfig&&) = default;
AuctionConfig::~AuctionConfig() = default;

AuctionConfig& AuctionConfig::operator=(const AuctionConfig&) = default;
AuctionConfig& AuctionConfig::operator=(AuctionConfig&&) = default;

bool AuctionConfig::operator==(const AuctionConfig& other) const {
  return std::tie(seller, decision_logic_url, trusted_scoring_signals_url,
                  non_shared_params, seller_experiment_group_id,
                  all_buyer_experiment_group_id,
                  per_buyer_experiment_group_ids) !=
         std::tie(other.seller, other.decision_logic_url,
                  other.trusted_scoring_signals_url, other.non_shared_params,
                  other.seller_experiment_group_id,
                  other.all_buyer_experiment_group_id,
                  other.per_buyer_experiment_group_ids);
}

}  // namespace blink
