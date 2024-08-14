// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/interest_group.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(InterestGroupTest, DEPRECATED_KAnonKeyForAdNameReporting) {
  // Make sure that DEPRECATED_KAnonKeyForAdNameReporting properly prioritizes
  // and incorporates various kinds of reporting IDs.
  InterestGroup ig;
  ig.owner = url::Origin::Create(GURL("https://example.org"));
  ig.name = "ig_one";
  ig.bidding_url = GURL("https://example.org/bid.js");
  ig.ads = {{{/*render_url=*/GURL("https://ad1.com"),
              /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
              /*buyer_reporting_id=*/std::nullopt,
              /*buyer_and_seller_reporting_id=*/std::nullopt},
             {/*render_url=*/GURL("https://ad2.com"),
              /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
              /*buyer_reporting_id=*/"bid",
              /*buyer_and_seller_reporting_id=*/std::nullopt},
             {/*render_url=*/GURL("https://ad3.com"),
              /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
              /*buyer_reporting_id=*/std::nullopt,
              /*buyer_and_seller_reporting_id=*/"bsid"},
             {/*render_url=*/GURL("https://ad3.com"),
              /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
              /*buyer_reporting_id=*/"bid",
              /*buyer_and_seller_reporting_id=*/"bsid"},
             {/*render_url=*/GURL("https://ad1.com"),
              /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
              /*buyer_reporting_id=*/std::nullopt,
              /*buyer_and_seller_reporting_id=*/std::nullopt},
             {/*render_url=*/GURL("https://ad2.com"),
              /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
              /*buyer_reporting_id=*/"bid",
              /*buyer_and_seller_reporting_id=*/std::nullopt},
             {/*render_url=*/GURL("https://ad3.com"),
              /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
              /*buyer_reporting_id=*/std::nullopt,
              /*buyer_and_seller_reporting_id=*/"bsid"},
             {/*render_url=*/GURL("https://ad3.com"),
              /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
              /*buyer_reporting_id=*/"bid",
              /*buyer_and_seller_reporting_id=*/"bsid"}}};
  EXPECT_EQ(
      "NameReport\n"
      "https://example.org/\nhttps://example.org/bid.js\nhttps://ad1.com/\n"
      "ig_one",
      DEPRECATED_KAnonKeyForAdNameReporting(
          ig, ig.ads->at(0),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt));
  EXPECT_EQ(
      "BuyerReportId\n"
      "https://example.org/\nhttps://example.org/bid.js\nhttps://ad2.com/\n"
      "bid",
      DEPRECATED_KAnonKeyForAdNameReporting(
          ig, ig.ads->at(1),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt));
  EXPECT_EQ(
      "BuyerAndSellerReportId\n"
      "https://example.org/\nhttps://example.org/bid.js\nhttps://ad3.com/\n"
      "bsid",
      DEPRECATED_KAnonKeyForAdNameReporting(
          ig, ig.ads->at(2),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt));
  EXPECT_EQ(
      "BuyerAndSellerReportId\n"
      "https://example.org/\nhttps://example.org/bid.js\nhttps://ad3.com/\n"
      "bsid",
      DEPRECATED_KAnonKeyForAdNameReporting(
          ig, ig.ads->at(3),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt));
  EXPECT_EQ(
      std::string(
          "SelectedBuyerAndSellerReportId\n"
          "https://example.org/\nhttps://example.org/bid.js\nhttps://ad1.com/\n"
          "\01\00\00\00\05sbsid\n\00\00\00\00\00\n\00\00\00\00\00",
          118),
      DEPRECATED_KAnonKeyForAdNameReporting(
          ig, ig.ads->at(4),
          /*selected_buyer_and_seller_reporting_id=*/std::string("sbsid")));
  EXPECT_EQ(
      std::string(
          "SelectedBuyerAndSellerReportId\n"
          "https://example.org/\nhttps://example.org/bid.js\nhttps://ad2.com/\n"
          "\01\00\00\00\05sbsid\n\00\00\00\00\00\n\01\00\00\00\03bid",
          121),
      DEPRECATED_KAnonKeyForAdNameReporting(
          ig, ig.ads->at(5),
          /*selected_buyer_and_seller_reporting_id=*/std::string("sbsid")));
  EXPECT_EQ(
      std::string(
          "SelectedBuyerAndSellerReportId\n"
          "https://example.org/\nhttps://example.org/bid.js\nhttps://ad3.com/\n"
          "\01\00\00\00\05sbsid\n\01\00\00\00\04bsid\n\00\00\00\00\00",
          122),
      DEPRECATED_KAnonKeyForAdNameReporting(
          ig, ig.ads->at(6),
          /*selected_buyer_and_seller_reporting_id=*/std::string("sbsid")));
  EXPECT_EQ(
      std::string(
          "SelectedBuyerAndSellerReportId\n"
          "https://example.org/\nhttps://example.org/bid.js\nhttps://ad3.com/\n"
          "\01\00\00\00\05sbsid\n\01\00\00\00\04bsid\n\01\00\00\00\03bid",
          125),
      DEPRECATED_KAnonKeyForAdNameReporting(
          ig, ig.ads->at(7),
          /*selected_buyer_and_seller_reporting_id=*/std::string("sbsid")));
}

TEST(InterestGroupTest, HashedKAnonKeyForAdNameReportingReturnsDistinctHashes) {
  InterestGroup ig;
  ig.owner = url::Origin::Create(GURL("https://example.org"));
  ig.name = "ig_one";
  ig.bidding_url = GURL("https://example.org/bid.js");

  // Without field length, both would have a key of "bsid\nbid\n".
  ig.ads = {{{/*render_url=*/GURL("https://ad3.com"),
              /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
              /*buyer_reporting_id=*/"bid\n",
              /*buyer_and_seller_reporting_id=*/"bsid"},
             {/*render_url=*/GURL("https://ad3.com"),
              /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
              /*buyer_reporting_id=*/"",
              /*buyer_and_seller_reporting_id=*/"bsid\nbid"}}};
  EXPECT_NE(
      HashedKAnonKeyForAdNameReporting(ig, ig.ads->at(0), std::string("sbsid")),
      HashedKAnonKeyForAdNameReporting(ig, ig.ads->at(1),
                                       std::string("sbsid")));
}

// Test ParseTrustedBiddingSignalsSlotSizeMode() and
// TrustedBiddingSignalsSlotSizeModeToString().
TEST(InterestGroupTest, TrustedBiddingSignalsSlotSizeMode) {
  EXPECT_EQ(InterestGroup::TrustedBiddingSignalsSlotSizeMode::kNone,
            InterestGroup::ParseTrustedBiddingSignalsSlotSizeMode("none"));
  EXPECT_EQ("none",
            InterestGroup::TrustedBiddingSignalsSlotSizeModeToString(
                InterestGroup::TrustedBiddingSignalsSlotSizeMode::kNone));

  EXPECT_EQ(InterestGroup::TrustedBiddingSignalsSlotSizeMode::kSlotSize,
            InterestGroup::ParseTrustedBiddingSignalsSlotSizeMode("slot-size"));
  EXPECT_EQ("slot-size",
            InterestGroup::TrustedBiddingSignalsSlotSizeModeToString(
                InterestGroup::TrustedBiddingSignalsSlotSizeMode::kSlotSize));

  EXPECT_EQ(
      InterestGroup::TrustedBiddingSignalsSlotSizeMode::kAllSlotsRequestedSizes,
      InterestGroup::ParseTrustedBiddingSignalsSlotSizeMode(
          "all-slots-requested-sizes"));
  EXPECT_EQ("all-slots-requested-sizes",
            InterestGroup::TrustedBiddingSignalsSlotSizeModeToString(
                InterestGroup::TrustedBiddingSignalsSlotSizeMode::
                    kAllSlotsRequestedSizes));

  EXPECT_EQ(InterestGroup::TrustedBiddingSignalsSlotSizeMode::kNone,
            InterestGroup::ParseTrustedBiddingSignalsSlotSizeMode(
                "not-a-valid-mode"));
}

}  // namespace blink
