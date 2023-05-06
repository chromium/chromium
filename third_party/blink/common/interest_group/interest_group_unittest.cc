// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/interest_group.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(InterestGroupTest, KAnonKeyForAdNameReporting) {
  // Make sure that KAnonKeyForAdNameReporting properly prioritizes and
  // incorporates various kinds of reporting IDs.
  InterestGroup ig;
  ig.owner = url::Origin::Create(GURL("https://example.org"));
  ig.name = "ig_one";
  ig.bidding_url = GURL("https://example.org/bid.js");
  ig.ads = {{{/*render_url=*/GURL("https://ad1.com"),
              /*metadata=*/absl::nullopt, /*size_group=*/absl::nullopt,
              /*buyer_reporting_id=*/absl::nullopt,
              /*buyer_and_seller_reporting_id=*/absl::nullopt},
             {/*render_url=*/GURL("https://ad2.com"),
              /*metadata=*/absl::nullopt, /*size_group=*/absl::nullopt,
              /*buyer_reporting_id=*/"bid",
              /*buyer_and_seller_reporting_id=*/absl::nullopt},
             {/*render_url=*/GURL("https://ad3.com"),
              /*metadata=*/absl::nullopt, /*size_group=*/absl::nullopt,
              /*buyer_reporting_id=*/"bid",
              /*buyer_and_seller_reporting_id=*/"bsid"}}};
  EXPECT_EQ(
      "NameReport\nhttps://example.org/\nhttps://example.org/bid.js\n"
      "https://ad1.com/\nig_one",
      KAnonKeyForAdNameReporting(ig, ig.ads->at(0)));
  EXPECT_EQ(
      "BuyerReportId\nhttps://example.org/\nhttps://example.org/bid.js\n"
      "https://ad2.com/\nbid",
      KAnonKeyForAdNameReporting(ig, ig.ads->at(1)));
  EXPECT_EQ(
      "BuyerAndSellerReportId\nhttps://example.org/\n"
      "https://example.org/bid.js\nhttps://ad3.com/\nbsid",
      KAnonKeyForAdNameReporting(ig, ig.ads->at(2)));
}

}  // namespace blink
