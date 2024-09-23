// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/interest_group.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {
template <int N>
std::string MakeNullSafeString(const char (&as_chars)[N]) {
  return std::string(as_chars, N - 1);
}
}  // namespace

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
      MakeNullSafeString(
          "SelectedBuyerAndSellerReportId\n"
          "https://example.org/\nhttps://example.org/bid.js\nhttps://ad1.com/\n"
          "\x01\x00\x00\x00\x05"
          "sbsid\n"
          "\x00\x00\x00\x00\x00\n"
          "\x00\x00\x00\x00\x00"),
      DEPRECATED_KAnonKeyForAdNameReporting(
          ig, ig.ads->at(0),
          /*selected_buyer_and_seller_reporting_id=*/std::string("sbsid")));
  EXPECT_EQ(
      MakeNullSafeString(
          "SelectedBuyerAndSellerReportId\n"
          "https://example.org/\nhttps://example.org/bid.js\nhttps://ad2.com/\n"
          "\x01\x00\x00\x00\x05"
          "sbsid\n"
          "\x00\x00\x00\x00\x00\n"
          "\x01\x00\x00\x00\x03"
          "bid"),
      DEPRECATED_KAnonKeyForAdNameReporting(
          ig, ig.ads->at(1),
          /*selected_buyer_and_seller_reporting_id=*/std::string("sbsid")));
  EXPECT_EQ(
      MakeNullSafeString(
          "SelectedBuyerAndSellerReportId\n"
          "https://example.org/\nhttps://example.org/bid.js\nhttps://ad3.com/\n"
          "\x01\x00\x00\x00\x05"
          "sbsid\n"
          "\x01\x00\x00\x00\x04"
          "bsid\n"
          "\x00\x00\x00\x00\x00"),
      DEPRECATED_KAnonKeyForAdNameReporting(
          ig, ig.ads->at(2),
          /*selected_buyer_and_seller_reporting_id=*/std::string("sbsid")));
  EXPECT_EQ(
      MakeNullSafeString(
          "SelectedBuyerAndSellerReportId\n"
          "https://example.org/\nhttps://example.org/bid.js\nhttps://ad3.com/\n"
          "\x01\x00\x00\x00\x05"
          "sbsid\n"
          "\x01\x00\x00\x00\x04"
          "bsid\n"
          "\x01\x00\x00\x00\x03"
          "bid"),
      DEPRECATED_KAnonKeyForAdNameReporting(
          ig, ig.ads->at(3),
          /*selected_buyer_and_seller_reporting_id=*/std::string("sbsid")));
}

TEST(InterestGroupTest, KAnonKeyForReportingIdsWithSpecialCharacters) {
  InterestGroup ig;
  ig.owner = url::Origin::Create(GURL("https://example.org"));
  ig.name = MakeNullSafeString("i\x00\x01\ng_one");
  ig.bidding_url = GURL("https://example.org/bid.js");
  ig.ads = {{{/*render_url=*/GURL("https://ad1.com"),
              /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
              /*buyer_reporting_id=*/std::nullopt,
              /*buyer_and_seller_reporting_id=*/std::nullopt},
             {/*render_url=*/GURL("https://ad2.com"),
              /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
              /*buyer_reporting_id=*/MakeNullSafeString("b\x00\x01\nid"),
              /*buyer_and_seller_reporting_id=*/std::nullopt},
             {/*render_url=*/GURL("https://ad3.com"),
              /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
              /*buyer_reporting_id=*/MakeNullSafeString("b\x00\x01\nid"),
              /*buyer_and_seller_reporting_id=*/
              MakeNullSafeString("b\x00\x01\nsid")}}};
  EXPECT_EQ(
      MakeNullSafeString(
          "NameReport\n"
          "https://example.org/\nhttps://example.org/bid.js\nhttps://ad1.com/\n"
          "i\x00\x01\ng_one"),
      DEPRECATED_KAnonKeyForAdNameReporting(
          ig, ig.ads->at(0),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt));
  EXPECT_EQ(
      MakeNullSafeString(
          "BuyerReportId\n"
          "https://example.org/\nhttps://example.org/bid.js\nhttps://ad2.com/\n"
          "b\x00\x01\nid"),
      DEPRECATED_KAnonKeyForAdNameReporting(
          ig, ig.ads->at(1),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt));
  EXPECT_EQ(
      MakeNullSafeString(
          "BuyerAndSellerReportId\n"
          "https://example.org/\nhttps://example.org/bid.js\nhttps://ad3.com/\n"
          "b\x00\x01\nsid"),
      DEPRECATED_KAnonKeyForAdNameReporting(
          ig, ig.ads->at(2),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt));
  EXPECT_EQ(
      MakeNullSafeString(
          "SelectedBuyerAndSellerReportId\n"
          "https://example.org/\nhttps://example.org/bid.js\nhttps://ad1.com/\n"
          "\x01\x00\x00\x00\x08"
          "s\x00\x01\nbsid\n"
          "\x00\x00\x00\x00\x00\n"
          "\x00\x00\x00\x00\x00"),
      DEPRECATED_KAnonKeyForAdNameReporting(
          ig, ig.ads->at(0),
          /*selected_buyer_and_seller_reporting_id=*/
          MakeNullSafeString("s\x00\x01\nbsid")));
  EXPECT_EQ(
      MakeNullSafeString(
          "SelectedBuyerAndSellerReportId\n"
          "https://example.org/\nhttps://example.org/bid.js\nhttps://ad2.com/\n"
          "\x01\x00\x00\x00\x08"
          "s\x00\x01\nbsid\n"
          "\x00\x00\x00\x00\x00\n"
          "\x01\x00\x00\x00\x06"
          "b\x00\x01\nid"),
      DEPRECATED_KAnonKeyForAdNameReporting(
          ig, ig.ads->at(1),
          /*selected_buyer_and_seller_reporting_id=*/
          MakeNullSafeString("s\x00\x01\nbsid")));
  EXPECT_EQ(
      MakeNullSafeString(
          "SelectedBuyerAndSellerReportId\n"
          "https://example.org/\nhttps://example.org/bid.js\nhttps://ad3.com/\n"
          "\x01\x00\x00\x00\x08"
          "s\x00\x01\nbsid\n"
          "\x01\x00\x00\x00\x07"
          "b\x00\x01\nsid\n"
          "\x01\x00\x00\x00\x06"
          "b\x00\x01\nid"),
      DEPRECATED_KAnonKeyForAdNameReporting(
          ig, ig.ads->at(2),
          /*selected_buyer_and_seller_reporting_id=*/
          MakeNullSafeString("s\x00\x01\nbsid")));
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
