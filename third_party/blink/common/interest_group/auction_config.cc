// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/auction_config.h"

#include <cmath>
#include <string_view>
#include <tuple>

#include "base/strings/to_string.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"
#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom.h"

namespace blink {

DirectFromSellerSignalsSubresource::DirectFromSellerSignalsSubresource() =
    default;
DirectFromSellerSignalsSubresource::DirectFromSellerSignalsSubresource(
    const DirectFromSellerSignalsSubresource&) = default;
DirectFromSellerSignalsSubresource::DirectFromSellerSignalsSubresource(
    DirectFromSellerSignalsSubresource&&) = default;
DirectFromSellerSignalsSubresource::~DirectFromSellerSignalsSubresource() =
    default;

DirectFromSellerSignalsSubresource&
DirectFromSellerSignalsSubresource::operator=(
    const DirectFromSellerSignalsSubresource&) = default;
DirectFromSellerSignalsSubresource&
DirectFromSellerSignalsSubresource::operator=(
    DirectFromSellerSignalsSubresource&&) = default;
bool operator==(const DirectFromSellerSignalsSubresource&,
                const DirectFromSellerSignalsSubresource&) = default;

DirectFromSellerSignals::DirectFromSellerSignals() = default;
DirectFromSellerSignals::DirectFromSellerSignals(
    const DirectFromSellerSignals&) = default;
DirectFromSellerSignals::DirectFromSellerSignals(DirectFromSellerSignals&&) =
    default;
DirectFromSellerSignals::~DirectFromSellerSignals() = default;

DirectFromSellerSignals& DirectFromSellerSignals::operator=(
    const DirectFromSellerSignals&) = default;
DirectFromSellerSignals& DirectFromSellerSignals::operator=(
    DirectFromSellerSignals&&) = default;
bool operator==(const DirectFromSellerSignals&,
                const DirectFromSellerSignals&) = default;

bool operator==(const AuctionConfig::BuyerTimeouts&,
                const AuctionConfig::BuyerTimeouts&) = default;

bool operator==(const AuctionConfig::BuyerCurrencies&,
                const AuctionConfig::BuyerCurrencies&) = default;

AuctionConfig::NonSharedParams::NonSharedParams() = default;
AuctionConfig::NonSharedParams::NonSharedParams(const NonSharedParams&) =
    default;
AuctionConfig::NonSharedParams::NonSharedParams(NonSharedParams&&) = default;
AuctionConfig::NonSharedParams::~NonSharedParams() = default;

AuctionConfig::NonSharedParams& AuctionConfig::NonSharedParams::operator=(
    const NonSharedParams&) = default;
AuctionConfig::NonSharedParams& AuctionConfig::NonSharedParams::operator=(
    NonSharedParams&&) = default;
bool operator==(const AuctionConfig::NonSharedParams&,
                const AuctionConfig::NonSharedParams&) = default;

bool operator==(
    const AuctionConfig::NonSharedParams::AuctionReportBuyersConfig&,
    const AuctionConfig::NonSharedParams::AuctionReportBuyersConfig&) = default;

bool operator==(
    const AuctionConfig::NonSharedParams::AuctionReportBuyerDebugModeConfig&,
    const AuctionConfig::NonSharedParams::AuctionReportBuyerDebugModeConfig&) =
    default;

AuctionConfig::ServerResponseConfig::ServerResponseConfig() = default;
AuctionConfig::ServerResponseConfig::ServerResponseConfig(
    const ServerResponseConfig& other) = default;
AuctionConfig::ServerResponseConfig::ServerResponseConfig(
    ServerResponseConfig&&) = default;
AuctionConfig::ServerResponseConfig::~ServerResponseConfig() = default;

AuctionConfig::ServerResponseConfig&
AuctionConfig::ServerResponseConfig::operator=(
    const ServerResponseConfig& other) = default;

AuctionConfig::ServerResponseConfig&
AuctionConfig::ServerResponseConfig::operator=(ServerResponseConfig&&) =
    default;

bool operator==(const AuctionConfig::ServerResponseConfig&,
                const AuctionConfig::ServerResponseConfig&) = default;

AuctionConfig::AuctionConfig() = default;
AuctionConfig::AuctionConfig(const AuctionConfig&) = default;
AuctionConfig::AuctionConfig(AuctionConfig&&) = default;
AuctionConfig::~AuctionConfig() = default;

AuctionConfig& AuctionConfig::operator=(const AuctionConfig&) = default;
AuctionConfig& AuctionConfig::operator=(AuctionConfig&&) = default;

bool operator==(const AuctionConfig&, const AuctionConfig&) = default;

int AuctionConfig::NumPromises() const {
  int total = 0;
  if (non_shared_params.auction_signals.is_promise()) {
    ++total;
  }
  if (non_shared_params.seller_signals.is_promise()) {
    ++total;
  }
  if (non_shared_params.per_buyer_signals.is_promise()) {
    ++total;
  }
  if (non_shared_params.buyer_timeouts.is_promise()) {
    ++total;
  }
  if (non_shared_params.buyer_currencies.is_promise()) {
    ++total;
  }
  if (non_shared_params.buyer_cumulative_timeouts.is_promise()) {
    ++total;
  }
  if (non_shared_params.deprecated_render_url_replacements.is_promise()) {
    ++total;
  }
  if (direct_from_seller_signals.is_promise()) {
    ++total;
  }
  if (expects_direct_from_seller_signals_header_ad_slot) {
    ++total;
  }
  if (expects_additional_bids) {
    ++total;
  }
  for (const blink::AuctionConfig& sub_auction :
       non_shared_params.component_auctions) {
    total += sub_auction.NumPromises();
  }
  return total;
}

bool AuctionConfig::IsHttpsAndMatchesSellerOrigin(const GURL& url) const {
  return url.scheme() == url::kHttpsScheme &&
         url::Origin::Create(url) == seller;
}

bool AuctionConfig::IsValidTrustedScoringSignalsURL(const GURL& url) const {
  if (url.has_query() || url.has_ref() || url.has_username() ||
      url.has_password()) {
    return false;
  }

  if (base::FeatureList::IsEnabled(
          blink::features::kFledgePermitCrossOriginTrustedSignals)) {
    return url.scheme() == url::kHttpsScheme;
  } else {
    return IsHttpsAndMatchesSellerOrigin(url);
  }
}

bool AuctionConfig::IsDirectFromSellerSignalsValid(
    const std::optional<blink::DirectFromSellerSignals>&
        candidate_direct_from_seller_signals) const {
  if (!candidate_direct_from_seller_signals.has_value()) {
    return true;
  }

  const GURL& prefix = candidate_direct_from_seller_signals->prefix;
  // The prefix can't have a query because the browser process appends its own
  // query suffix.
  if (prefix.has_query()) {
    return false;
  }
  // NOTE: uuid-in-package isn't supported, since it doesn't support CORS.
  if (!IsHttpsAndMatchesSellerOrigin(prefix)) {
    return false;
  }

  base::flat_set<url::Origin> interest_group_buyers(
      non_shared_params.interest_group_buyers
          ? *non_shared_params.interest_group_buyers
          : std::vector<url::Origin>());
  for (const auto& [buyer_origin, bundle_url] :
       candidate_direct_from_seller_signals->per_buyer_signals) {
    // The renderer shouldn't provide bundles for origins that aren't buyers
    // in this auction -- there would be no worklet to receive them.
    if (interest_group_buyers.count(buyer_origin) < 1) {
      return false;
    }
    // All DirectFromSellerSignals must come from the seller.
    if (!IsHttpsAndMatchesSellerOrigin(bundle_url.bundle_url)) {
      return false;
    }
  }
  if (candidate_direct_from_seller_signals->seller_signals &&
      !IsHttpsAndMatchesSellerOrigin(
          candidate_direct_from_seller_signals->seller_signals->bundle_url)) {
    // All DirectFromSellerSignals must come from the seller.
    return false;
  }
  if (candidate_direct_from_seller_signals->auction_signals &&
      !IsHttpsAndMatchesSellerOrigin(
          candidate_direct_from_seller_signals->auction_signals->bundle_url)) {
    // All DirectFromSellerSignals must come from the seller.
    return false;
  }
  return true;
}

}  // namespace blink
