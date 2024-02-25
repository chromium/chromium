// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/subresource_url_builder.h"

#include <optional>

#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using BundleSubresourceInfo = SubresourceUrlBuilder::BundleSubresourceInfo;

blink::DirectFromSellerSignalsSubresource CreateSubresource(
    const GURL& bundle_url) {
  blink::DirectFromSellerSignalsSubresource subresource;
  subresource.bundle_url = bundle_url;
  return subresource;
}

TEST(SubresourceUrlBuilderTest, NoSignals) {
  SubresourceUrlBuilder builder(std::nullopt);
  EXPECT_FALSE(builder.seller_signals());
  EXPECT_FALSE(builder.auction_signals());
  EXPECT_EQ(0u, builder.per_buyer_signals().size());
}

TEST(SubresourceUrlBuilderTest, Empty) {
  // All fields construct to empty / nullopt.
  blink::DirectFromSellerSignals direct_from_seller_signals;

  SubresourceUrlBuilder builder(direct_from_seller_signals);
  EXPECT_FALSE(builder.seller_signals());
  EXPECT_FALSE(builder.auction_signals());
  EXPECT_EQ(0u, builder.per_buyer_signals().size());
}

TEST(SubresourceUrlBuilderTest, AllFieldsPopulated) {
  const url::Origin buyer1_origin =
      url::Origin::Create(GURL("https://buyer1.test"));
  const url::Origin buyer2_origin =
      url::Origin::Create(GURL("https://buyer2.test"));

  const GURL bundle_url1 = GURL("https://seller.test/bundle1");
  const GURL bundle_url2 = GURL("https://seller.test/bundle2");

  blink::DirectFromSellerSignals direct_from_seller_signals;

  direct_from_seller_signals.prefix = GURL("https://seller.test/signals");

  blink::DirectFromSellerSignalsSubresource buyer1_signals =
      CreateSubresource(bundle_url1);
  direct_from_seller_signals.per_buyer_signals[buyer1_origin] = buyer1_signals;

  blink::DirectFromSellerSignalsSubresource buyer2_signals =
      CreateSubresource(bundle_url2);
  direct_from_seller_signals.per_buyer_signals[buyer2_origin] = buyer2_signals;

  blink::DirectFromSellerSignalsSubresource seller_signals =
      CreateSubresource(bundle_url1);
  direct_from_seller_signals.seller_signals = seller_signals;

  blink::DirectFromSellerSignalsSubresource auction_signals =
      CreateSubresource(bundle_url2);
  direct_from_seller_signals.auction_signals = auction_signals;

  SubresourceUrlBuilder builder(direct_from_seller_signals);

  ASSERT_TRUE(builder.seller_signals());
  EXPECT_EQ(GURL("https://seller.test/signals?sellerSignals"),
            builder.seller_signals()->subresource_url);
  EXPECT_EQ(bundle_url1,
            builder.seller_signals()->info_from_renderer.bundle_url);
  EXPECT_EQ(seller_signals.token,
            builder.seller_signals()->info_from_renderer.token);

  ASSERT_TRUE(builder.auction_signals());
  EXPECT_EQ(GURL("https://seller.test/signals?auctionSignals"),
            builder.auction_signals()->subresource_url);
  EXPECT_EQ(bundle_url2,
            builder.auction_signals()->info_from_renderer.bundle_url);
  EXPECT_EQ(auction_signals.token,
            builder.auction_signals()->info_from_renderer.token);

  ASSERT_EQ(2u, builder.per_buyer_signals().size());

  const BundleSubresourceInfo& buyer1_full_info =
      builder.per_buyer_signals().at(buyer1_origin);
  EXPECT_EQ(GURL("https://seller.test/"
                 "signals?perBuyerSignals=https%3A%2F%2Fbuyer1.test"),
            buyer1_full_info.subresource_url);
  EXPECT_EQ(bundle_url1, buyer1_full_info.info_from_renderer.bundle_url);
  EXPECT_EQ(buyer1_signals.token, buyer1_full_info.info_from_renderer.token);

  const BundleSubresourceInfo& buyer2_full_info =
      builder.per_buyer_signals().at(buyer2_origin);
  EXPECT_EQ(GURL("https://seller.test/"
                 "signals?perBuyerSignals=https%3A%2F%2Fbuyer2.test"),
            buyer2_full_info.subresource_url);
  EXPECT_EQ(bundle_url2, buyer2_full_info.info_from_renderer.bundle_url);
  EXPECT_EQ(buyer2_signals.token, buyer2_full_info.info_from_renderer.token);
}

}  // namespace

}  // namespace content
