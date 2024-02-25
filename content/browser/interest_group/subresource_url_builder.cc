// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/subresource_url_builder.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/strings/escape.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "url/gurl.h"

namespace content {

namespace {

using BundleSubresourceInfo = SubresourceUrlBuilder::BundleSubresourceInfo;

std::optional<BundleSubresourceInfo> BuildSellerSignalsSubresourceURL(
    const std::optional<const blink::DirectFromSellerSignals>&
        direct_from_seller_signals) {
  if (!direct_from_seller_signals)
    return std::nullopt;
  if (!direct_from_seller_signals->seller_signals)
    return std::nullopt;
  BundleSubresourceInfo full_info(
      /*subresource_url=*/GURL(direct_from_seller_signals->prefix.spec() +
                               "?sellerSignals"),
      /*info_from_renderer=*/*direct_from_seller_signals->seller_signals);
  return full_info;
}

std::optional<BundleSubresourceInfo> BuildAuctionSignalsSubresourceURL(
    const std::optional<const blink::DirectFromSellerSignals>&
        direct_from_seller_signals) {
  if (!direct_from_seller_signals)
    return std::nullopt;
  if (!direct_from_seller_signals->auction_signals)
    return std::nullopt;
  BundleSubresourceInfo full_info(
      /*subresource_url=*/GURL(direct_from_seller_signals->prefix.spec() +
                               "?auctionSignals"),
      /*info_from_renderer=*/*direct_from_seller_signals->auction_signals);
  return full_info;
}

base::flat_map<url::Origin, BundleSubresourceInfo>
BuildPerBuyerSignalsSubresourceURLs(
    const std::optional<const blink::DirectFromSellerSignals>&
        direct_from_seller_signals) {
  if (!direct_from_seller_signals)
    return {};
  std::vector<std::pair<url::Origin, BundleSubresourceInfo>> result;
  for (const auto& [buyer_origin, subresource_renderer_info] :
       direct_from_seller_signals->per_buyer_signals) {
    result.emplace_back(
        buyer_origin,
        BundleSubresourceInfo(
            /*subresource_url=*/GURL(
                direct_from_seller_signals->prefix.spec() +
                "?perBuyerSignals=" +
                base::EscapeQueryParamValue(buyer_origin.Serialize(),
                                            /*use_plus=*/false)),
            /*info_from_renderer=*/subresource_renderer_info));
  }
  return base::flat_map<url::Origin, BundleSubresourceInfo>(result);
}

}  // namespace

BundleSubresourceInfo::BundleSubresourceInfo(
    const GURL& subresource_url,
    const blink::DirectFromSellerSignalsSubresource& info_from_renderer)
    : subresource_url(subresource_url),
      info_from_renderer(info_from_renderer) {}

BundleSubresourceInfo::~BundleSubresourceInfo() = default;

BundleSubresourceInfo::BundleSubresourceInfo(const BundleSubresourceInfo&) =
    default;

BundleSubresourceInfo& BundleSubresourceInfo::operator=(
    const BundleSubresourceInfo&) = default;

BundleSubresourceInfo::BundleSubresourceInfo(BundleSubresourceInfo&&) = default;

BundleSubresourceInfo& BundleSubresourceInfo::operator=(
    BundleSubresourceInfo&&) = default;

SubresourceUrlBuilder::SubresourceUrlBuilder(
    const std::optional<blink::DirectFromSellerSignals>&
        direct_from_seller_signals)
    : seller_signals_(
          BuildSellerSignalsSubresourceURL(direct_from_seller_signals)),
      auction_signals_(
          BuildAuctionSignalsSubresourceURL(direct_from_seller_signals)),
      per_buyer_signals_(
          BuildPerBuyerSignalsSubresourceURLs(direct_from_seller_signals)) {}

SubresourceUrlBuilder::~SubresourceUrlBuilder() = default;

bool operator==(const SubresourceUrlBuilder::BundleSubresourceInfo& a,
                const SubresourceUrlBuilder::BundleSubresourceInfo& b) {
  return std::tie(a.subresource_url, a.info_from_renderer) ==
         std::tie(b.subresource_url, b.info_from_renderer);
}

}  // namespace content
