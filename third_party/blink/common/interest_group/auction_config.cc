// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/auction_config.h"

#include <tuple>

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

bool operator==(const DirectFromSellerSignalsSubresource& a,
                const DirectFromSellerSignalsSubresource& b) {
  return std::tie(a.bundle_url, a.token) == std::tie(b.bundle_url, b.token);
}

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

AuctionConfig::MaybePromiseJson::MaybePromiseJson() = default;
AuctionConfig::MaybePromiseJson::MaybePromiseJson(const MaybePromiseJson&) =
    default;
AuctionConfig::MaybePromiseJson::MaybePromiseJson(MaybePromiseJson&&) = default;
AuctionConfig::MaybePromiseJson::~MaybePromiseJson() = default;

AuctionConfig::MaybePromiseJson& AuctionConfig::MaybePromiseJson::operator=(
    const MaybePromiseJson&) = default;
AuctionConfig::MaybePromiseJson& AuctionConfig::MaybePromiseJson::operator=(
    MaybePromiseJson&&) = default;

AuctionConfig::NonSharedParams::NonSharedParams() = default;
AuctionConfig::NonSharedParams::NonSharedParams(const NonSharedParams&) =
    default;
AuctionConfig::NonSharedParams::NonSharedParams(NonSharedParams&&) = default;
AuctionConfig::NonSharedParams::~NonSharedParams() = default;

AuctionConfig::NonSharedParams& AuctionConfig::NonSharedParams::operator=(
    const NonSharedParams&) = default;
AuctionConfig::NonSharedParams& AuctionConfig::NonSharedParams::operator=(
    NonSharedParams&&) = default;

int AuctionConfig::NonSharedParams::NumPromises() const {
  int total = 0;
  if (auction_signals.is_promise()) {
    ++total;
  }
  if (seller_signals.is_promise()) {
    ++total;
  }
  for (const blink::AuctionConfig& sub_auction : component_auctions) {
    total += sub_auction.non_shared_params.NumPromises();
  }
  return total;
}

AuctionConfig::AuctionConfig() = default;
AuctionConfig::AuctionConfig(const AuctionConfig&) = default;
AuctionConfig::AuctionConfig(AuctionConfig&&) = default;
AuctionConfig::~AuctionConfig() = default;

AuctionConfig& AuctionConfig::operator=(const AuctionConfig&) = default;
AuctionConfig& AuctionConfig::operator=(AuctionConfig&&) = default;

bool BLINK_COMMON_EXPORT operator==(const AuctionConfig::MaybePromiseJson& a,
                                    const AuctionConfig::MaybePromiseJson& b) {
  return a.tag() == b.tag() && a.json_payload() == b.json_payload();
}

}  // namespace blink
