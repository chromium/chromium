// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/additional_bids_util.h"

#include <stdint.h>

#include <array>
#include <limits>
#include <optional>
#include <string>

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/interest_group/auction_metrics_recorder.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-forward.h"
#include "crypto/sha2.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::UnorderedElementsAre;

namespace content {
namespace {

// In case one wants to generate some keys for test use, the following may be
// useful:
// template <int N>
// std::string SerializeKey(uint8_t key[N]) {
//   std::string out;
//   for (int row = 0; row < (N / 8); ++row) {
//     for (int col = 0; col < 8; ++col) {
//       base::StrAppend(
//           &out, {base::StringPrintf("0x%02x",
//                                     static_cast<unsigned>(
//                                         key[row * 8 + col])),
//                  ", "});
//     }
//     base::StrAppend(&out, {"\n"});
//   }
//   return out;
// }
//
// TEST_F(AdditionalBidsUtilTest, GenerateKeyPair) {
//   uint8_t public_key[32];
//   uint8_t private_key[64];
//   ED25519_keypair(public_key, private_key);
//   std::cout << "public_key:\n";
//   std::cout << SerializeKey<32>(public_key) << "\n";
//   std::cout << base::Base64Encode(base::span(public_key));
//   std::cout << "\n\n";
//
//   std::cout << "private_key:\n";
//   std::cout << SerializeKey<64>(private_key) << "\n";
//   std::cout << base::Base64Encode(base::span(private_key));
//   std::cout << "\n\n";
// }

// Some test data for key/signature fields. These are just random sequences
// of bytes of the right length, not proper cryptographic ones.
constexpr blink::InterestGroup::AdditionalBidKey kKey1{
    0xF5, 0x30, 0x88, 0xE9, 0x9B, 0xC7, 0xB0, 0x2A, 0x8C, 0xBE, 0x11,
    0x8D, 0xD3, 0xEC, 0xEF, 0xEB, 0xB5, 0x71, 0xDF, 0xF9, 0x7D, 0x67,
    0xEF, 0xFF, 0x9A, 0xAD, 0xE1, 0x63, 0x86, 0xAD, 0x57, 0x5E};
constexpr char kKey1Base64[] = "9TCI6ZvHsCqMvhGN0+zv67Vx3/l9Z+//mq3hY4atV14=";

constexpr blink::InterestGroup::AdditionalBidKey kKey2{
    0x79, 0x34, 0x0E, 0x99, 0xF6, 0x02, 0x98, 0xB2, 0xF6, 0x82, 0xAA,
    0xDA, 0x3C, 0x95, 0xFA, 0x62, 0x3A, 0xF2, 0x53, 0xA8, 0x56, 0xEB,
    0x21, 0xC4, 0xC2, 0x67, 0x6C, 0x5D, 0xE3, 0x4B, 0xDA, 0xA0};
constexpr char kKey2Base64[] = "eTQOmfYCmLL2gqraPJX6YjryU6hW6yHEwmdsXeNL2qA=";

constexpr auto kSig1 = std::to_array<uint8_t>(
    {0x49, 0xD1, 0x27, 0x01, 0x29, 0x9E, 0xC8, 0x34, 0xE3, 0x12, 0x46,
     0xA0, 0xFA, 0x17, 0x33, 0x1E, 0xD2, 0x7B, 0xC0, 0x63, 0x7D, 0x7F,
     0x63, 0xF6, 0x12, 0x49, 0x39, 0x40, 0x80, 0x2F, 0x31, 0x93, 0x99,
     0xD7, 0x93, 0x16, 0x58, 0x4D, 0x3B, 0xEC, 0x0F, 0x46, 0x07, 0x29,
     0xE4, 0xE6, 0x13, 0x0D, 0xD7, 0xEA, 0x6D, 0x35, 0x60, 0xB8, 0x27,
     0x9E, 0x86, 0xC7, 0xE0, 0x10, 0x63, 0xEA, 0x44, 0xE6});
constexpr char kSig1Base64[] =
    "SdEnASmeyDTjEkag+hczHtJ7wGN9f2P2Ekk5QIAvMZOZ15MWWE077A9GBynk5hMN1+"
    "ptNWC4J56Gx+AQY+pE5g==";

constexpr auto kSig2 = std::to_array<uint8_t>(
    {0x91, 0x2C, 0xF4, 0x82, 0x8F, 0x62, 0x6B, 0x1F, 0x4A, 0x34, 0x1B,
     0x8C, 0x4C, 0xB8, 0xD6, 0xA1, 0x41, 0xD0, 0xBD, 0xCC, 0x67, 0xBA,
     0xCF, 0x08, 0xE4, 0x32, 0x09, 0x5D, 0x97, 0x06, 0x09, 0x41, 0xFA,
     0xEA, 0x12, 0x8E, 0x49, 0x05, 0x73, 0xE2, 0xA4, 0x57, 0x7B, 0xA5,
     0x3B, 0x00, 0xAE, 0x23, 0xAF, 0x61, 0xE9, 0x5F, 0xA4, 0x39, 0xBD,
     0x07, 0x9B, 0xB7, 0x49, 0x31, 0x52, 0xDD, 0x69, 0xDD});
constexpr char kSig2Base64[] =
    "kSz0go9iax9KNBuMTLjWoUHQvcxnus8I5DIJXZcGCUH66hKOSQVz4qRXe6U7AK4jr2HpX6Q5vQ"
    "ebt0kxUt1p3Q==";

// Version that requires forgiving decoder to accept.
constexpr char kSig2Base64Sloppy[] =
    " kSz0go9iax9KNBuMTLjWoUHQvcxnus8I5DIJXZcGCUH66hKOSQVz4qRXe6U7AK4jr2HpX6Q5v"
    "Qebt0kxUt1p3Q";

constexpr char kPretendBid[] = "Hi, I am a JSON bid.";

std::string ComputeBidNonce(const base::Uuid& auction_nonce,
                            const base::Uuid& seller_nonce) {
  return base::Base64Encode(crypto::SHA256HashString(base::StrCat(
      {auction_nonce.AsLowercaseString(), seller_nonce.AsLowercaseString()})));
}

class AdditionalBidsUtilTest : public testing::Test {
 protected:
  base::Value::Dict MakeMinimalValid() {
    base::Value::Dict ig_dict;
    ig_dict.Set("name", "trainfans");
    ig_dict.Set("biddingLogicURL", "https://rollingstock.test/logic.js");
    ig_dict.Set("owner", "https://rollingstock.test/");

    base::Value::Dict bid_dict;
    bid_dict.Set("bid", 10.0);
    bid_dict.Set("render", "https://en.wikipedia.test/wiki/Train");

    base::Value::Dict additional_bid_dict;
    additional_bid_dict.Set("auctionNonce", kAuctionNonce.AsLowercaseString());
    additional_bid_dict.Set("seller", "https://seller.test");
    additional_bid_dict.Set("topLevelSeller", "https://top-organizer.test");
    additional_bid_dict.Set("interestGroup", std::move(ig_dict));
    additional_bid_dict.Set("bid", std::move(bid_dict));
    return additional_bid_dict;
  }

  base::Value::Dict MakeValidWithMultipleNegativeIGs() {
    base::Value::Dict additional_bid_dict = MakeMinimalValid();
    base::Value::Dict negative_igs_dict;
    negative_igs_dict.Set("joiningOrigin", "https://depot.test");
    base::Value::List negative_ig_names_list;
    negative_ig_names_list.Append("negative_group");
    negative_ig_names_list.Append("another negative group");
    negative_igs_dict.Set("interestGroupNames",
                          std::move(negative_ig_names_list));
    additional_bid_dict.Set("negativeInterestGroups",
                            std::move(negative_igs_dict));
    return additional_bid_dict;
  }

  static base::Value::Dict MakeValidSignedBid() {
    base::Value::Dict signed_dict;
    signed_dict.Set("bid", kPretendBid);

    base::Value::List sigs_list;
    base::Value::Dict sig1;
    sig1.Set("key", kKey1Base64);
    sig1.Set("signature", kSig1Base64);
    sigs_list.Append(std::move(sig1));

    base::Value::Dict sig2;
    sig2.Set("key", kKey2Base64);
    sig2.Set("signature", kSig2Base64);
    sigs_list.Append(std::move(sig2));

    signed_dict.Set("signatures", std::move(sigs_list));
    return signed_dict;
  }

  // Fills in the key only, we don't actually need the signature part any more.
  SignedAdditionalBidSignature SignatureWithKey(
      const blink::InterestGroup::AdditionalBidKey& key) {
    SignedAdditionalBidSignature result;
    result.key = key;
    return result;
  }

  const base::Uuid kAuctionNonce{base::Uuid::GenerateRandomV4()};
  const base::Uuid kSellerNonce{base::Uuid::GenerateRandomV4()};
  const std::string kBidNonce{ComputeBidNonce(kAuctionNonce, kSellerNonce)};
  const base::flat_set<url::Origin> kInterestGroupBuyers{
      url::Origin::Create(GURL("https://buyer.test")),
      url::Origin::Create(GURL("https://rollingstock.test")),
      url::Origin::Create(GURL("https://trainstuff.test"))};
  const url::Origin kSeller = url::Origin::Create(GURL("https://seller.test"));
  const url::Origin kTopSeller =
      url::Origin::Create(GURL("https://top-organizer.test"));
};

TEST_F(AdditionalBidsUtilTest, FailNotDict) {
  base::Value input(5);

  auto result = DecodeAdditionalBid(/*auction=*/nullptr, input, kAuctionNonce,
                                    /*seller_nonce=*/std::nullopt,
                                    kInterestGroupBuyers, kSeller,
                                    /*top_level_seller=*/std::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' is not a "
      "dictionary.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailNoNonce) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(blink::features::kFledgeSellerNonce);

  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Remove("auctionNonce");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(/*auction=*/nullptr, input, kAuctionNonce,
                                    /*seller_nonce=*/std::nullopt,
                                    kInterestGroupBuyers, kSeller,
                                    /*top_level_seller=*/std::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or incorrect nonce.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailInvalidNonce) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(blink::features::kFledgeSellerNonce);

  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Set("auctionNonce", "not-a-nonce");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(/*auction=*/nullptr, input, kAuctionNonce,
                                    /*seller_nonce=*/std::nullopt,
                                    kInterestGroupBuyers, kSeller,
                                    /*top_level_seller=*/std::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or incorrect nonce.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailNoNonceWithSellerNonce) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Remove("auctionNonce");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(/*auction=*/nullptr, input, kAuctionNonce,
                                    /*seller_nonce=*/std::nullopt,
                                    kInterestGroupBuyers, kSeller,
                                    /*top_level_seller=*/std::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to no auctionNonce or bidNonce in bid -- exactly one is required.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailInvalidNonceWithSellerNonce) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Set("auctionNonce", "not-a-nonce");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(/*auction=*/nullptr, input, kAuctionNonce,
                                    /*seller_nonce=*/std::nullopt,
                                    kInterestGroupBuyers, kSeller,
                                    /*top_level_seller=*/std::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      base::StringPrintf(
          "Additional bid on auction with seller 'https://seller.test' "
          "rejected due to auctionNonce from bid (not-a-nonce) not matching "
          "the header auctionNonce (%s).",
          kAuctionNonce.AsLowercaseString()),
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailBothAuctionNonceAndBidNonce) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Set("auctionNonce", kAuctionNonce.AsLowercaseString());
  additional_bid_dict.Set("bidNonce", kBidNonce);
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(/*auction=*/nullptr, input, kAuctionNonce,
                                    /*seller_nonce=*/std::nullopt,
                                    kInterestGroupBuyers, kSeller,
                                    /*top_level_seller=*/std::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to both auctionNonce and bidNonce in bid -- exactly one is "
      "required.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailBidNoSellerNonceButNoAuctionNonce) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Remove("auctionNonce");
  additional_bid_dict.Set("bidNonce", kBidNonce);
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(/*auction=*/nullptr, input, kAuctionNonce,
                                    /*seller_nonce=*/std::nullopt,
                                    kInterestGroupBuyers, kSeller,
                                    /*top_level_seller=*/std::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing auctionNonce on a bid returned without a seller nonce.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailBidSellerNonceButNoBidNonce) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Set("auctionNonce", kAuctionNonce.AsLowercaseString());
  base::Value input(std::move(additional_bid_dict));

  auto result =
      DecodeAdditionalBid(/*auction=*/nullptr, input, kAuctionNonce,
                          /*seller_nonce=*/kSellerNonce.AsLowercaseString(),
                          kInterestGroupBuyers, kSeller,
                          /*top_level_seller=*/std::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing bidNonce on a bid returned with a seller nonce.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailInvalidBidNonce) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Remove("auctionNonce");
  // Set bidNonce to base64(sha256("incorrect")).
  constexpr char kIncorrectNonce[] =
      "ID01Nr1irTOscLfqPU9eELbVLr0Mt1goQaBTrrtxhqM=";
  additional_bid_dict.Set("bidNonce", kIncorrectNonce);
  base::Value input(std::move(additional_bid_dict));

  auto result =
      DecodeAdditionalBid(/*auction=*/nullptr, input, kAuctionNonce,
                          /*seller_nonce=*/kSellerNonce.AsLowercaseString(),
                          kInterestGroupBuyers, kSeller,
                          /*top_level_seller=*/std::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(base::StringPrintf(
                "Additional bid on auction with seller "
                "'https://seller.test' rejected due to bidNonce from bid (%s) "
                "not matching its expectation (%s) as calculated from the "
                "header auctionNonce (%s) and sellerNonce (%s).",
                kIncorrectNonce, kBidNonce, kAuctionNonce.AsLowercaseString(),
                kSellerNonce.AsLowercaseString()),
            result.error());
}

TEST_F(AdditionalBidsUtilTest, FailMissingSeller) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Remove("seller");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(/*auction=*/nullptr, input, kAuctionNonce,
                                    /*seller_nonce=*/std::nullopt,
                                    kInterestGroupBuyers, kSeller,
                                    /*top_level_seller=*/std::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or incorrect seller.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailInvalidSeller) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Set("seller", "http://notseller.test");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(/*auction=*/nullptr, input, kAuctionNonce,
                                    /*seller_nonce=*/std::nullopt,
                                    kInterestGroupBuyers, kSeller,
                                    /*top_level_seller=*/std::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or incorrect seller.",
      result.error());
}

// Specifying topLevelSeller in a bid in a non-component auction is a problem.
TEST_F(AdditionalBidsUtilTest, FailInvalidTopLevelSeller) {
  base::Value input(MakeMinimalValid());
  auto result = DecodeAdditionalBid(/*auction=*/nullptr, input, kAuctionNonce,
                                    /*seller_nonce=*/std::nullopt,
                                    kInterestGroupBuyers, kSeller,
                                    /*top_level_seller=*/std::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to specifying topLevelSeller in a non-component auction.",
      result.error());
}

// Not specifying topLevelSeller in component auction bid is also a problem.
TEST_F(AdditionalBidsUtilTest, FailInvalidTopLevelSeller2) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Remove("topLevelSeller");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or incorrect topLevelSeller.",
      result.error());
}

// An incorrect topLevelSeller in a bid in a component auction is also a
// problem.
TEST_F(AdditionalBidsUtilTest, FailInvalidTopLevelSeller3) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Set("topLevelSeller", "https://wrong-organizer.test");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or incorrect topLevelSeller.",
      result.error());
}

// Missing IG dictionary.
TEST_F(AdditionalBidsUtilTest, FailNoIGDictionary) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Remove("interestGroup");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing interest group name.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailMissingInterestGroupName) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.RemoveByDottedPath("interestGroup.name");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing interest group name.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailMissingInterestGroupBiddingScript) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.RemoveByDottedPath("interestGroup.biddingLogicURL");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing interest group bidding URL.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailMissingInterestGroupOwner) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.RemoveByDottedPath("interestGroup.owner");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing interest group owner.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailNonHttpsInterestGroupOwner) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("interestGroup.owner",
                                      "http://rollingstock.test/");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to non-https interest group owner URL.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailDomainMismatchBetweenOwnerAndBiddingScript) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("interestGroup.owner",
                                      "https://trainstuff.test/");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to invalid origin of biddingLogicURL.",
      result.error());
}

// The additional bid owner is missing from interestGroupBuyers.
TEST_F(AdditionalBidsUtilTest, AdditionalBidOwnerNotInInterestGroupBuyers) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  base::Value input(std::move(additional_bid_dict));

  const base::flat_set<url::Origin> wrong_interest_group_buyers{
      url::Origin::Create(GURL("https://wrongbuyer.test"))};

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      wrong_interest_group_buyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "because the additional bid's owner, 'https://rollingstock.test', "
      "is not in interestGroupBuyers.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailMissingBid) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Remove("bid");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing bid info.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailMissingBidCreative) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.RemoveByDottedPath("bid.render");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or invalid creative URL.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailMissingBidValue) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.RemoveByDottedPath("bid.bid");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or invalid bid value.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailInvalidBidValue) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.bid", 0.0);
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or invalid bid value.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, MinimalValid) {
  base::Value input(MakeMinimalValid());

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_TRUE(result->bid_state);
  ASSERT_TRUE(result->bid);
  const InterestGroupAuction::BidState* bid_state = result->bid_state.get();
  const InterestGroupAuction::Bid* bid = result->bid.get();

  EXPECT_TRUE(bid_state->made_bid);
  EXPECT_EQ("trainfans", bid_state->bidder->interest_group.name);
  ASSERT_TRUE(bid_state->additional_bid_buyer.has_value());
  EXPECT_EQ(bid_state->bidder->interest_group.owner,
            bid_state->additional_bid_buyer);
  EXPECT_EQ("https://rollingstock.test",
            bid_state->bidder->interest_group.owner.Serialize());
  ASSERT_TRUE(bid_state->bidder->interest_group.bidding_url.has_value());
  EXPECT_EQ("https://rollingstock.test/logic.js",
            bid_state->bidder->interest_group.bidding_url->spec());

  ASSERT_TRUE(bid_state->bidder->interest_group.ads.has_value());
  ASSERT_EQ(1u, bid_state->bidder->interest_group.ads->size());
  EXPECT_EQ("https://en.wikipedia.test/wiki/Train",
            bid_state->bidder->interest_group.ads.value()[0].render_url());

  EXPECT_EQ(auction_worklet::mojom::BidRole::kBothKAnonModes, bid->bid_role);
  EXPECT_EQ("null", bid->ad_metadata);
  EXPECT_EQ(10.0, bid->bid);
  EXPECT_EQ(std::nullopt, bid->bid_currency);
  EXPECT_EQ(std::nullopt, bid->ad_cost);
  EXPECT_EQ(blink::AdDescriptor(GURL("https://en.wikipedia.test/wiki/Train")),
            bid->ad_descriptor);
  EXPECT_EQ(0u, bid->selected_ad_components.size());
  EXPECT_EQ(std::nullopt, bid->modeling_signals);
  EXPECT_EQ(std::nullopt, bid->aggregate_win_signals);
  EXPECT_EQ(&bid_state->bidder->interest_group, bid->interest_group);
  EXPECT_EQ(&bid_state->bidder->interest_group.ads.value()[0], bid->bid_ad);
  EXPECT_EQ(bid_state, bid->bid_state);
}

TEST_F(AdditionalBidsUtilTest, MinimalValidWithSellerNonce) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Remove("auctionNonce");
  additional_bid_dict.Set("bidNonce", kBidNonce);
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce,
      /*seller_nonce=*/kSellerNonce.AsLowercaseString(), kInterestGroupBuyers,
      kSeller, base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_TRUE(result->bid_state);
  ASSERT_TRUE(result->bid);
  const InterestGroupAuction::BidState* bid_state = result->bid_state.get();
  const InterestGroupAuction::Bid* bid = result->bid.get();

  EXPECT_TRUE(bid_state->made_bid);
  EXPECT_EQ("trainfans", bid_state->bidder->interest_group.name);
  ASSERT_TRUE(bid_state->additional_bid_buyer.has_value());
  EXPECT_EQ(bid_state->bidder->interest_group.owner,
            bid_state->additional_bid_buyer);
  EXPECT_EQ("https://rollingstock.test",
            bid_state->bidder->interest_group.owner.Serialize());
  ASSERT_TRUE(bid_state->bidder->interest_group.bidding_url.has_value());
  EXPECT_EQ("https://rollingstock.test/logic.js",
            bid_state->bidder->interest_group.bidding_url->spec());

  ASSERT_TRUE(bid_state->bidder->interest_group.ads.has_value());
  ASSERT_EQ(1u, bid_state->bidder->interest_group.ads->size());
  EXPECT_EQ("https://en.wikipedia.test/wiki/Train",
            bid_state->bidder->interest_group.ads.value()[0].render_url());

  EXPECT_EQ(auction_worklet::mojom::BidRole::kBothKAnonModes, bid->bid_role);
  EXPECT_EQ("null", bid->ad_metadata);
  EXPECT_EQ(10.0, bid->bid);
  EXPECT_EQ(std::nullopt, bid->bid_currency);
  EXPECT_EQ(std::nullopt, bid->ad_cost);
  EXPECT_EQ(blink::AdDescriptor(GURL("https://en.wikipedia.test/wiki/Train")),
            bid->ad_descriptor);
  EXPECT_EQ(0u, bid->selected_ad_components.size());
  EXPECT_EQ(std::nullopt, bid->modeling_signals);
  EXPECT_EQ(std::nullopt, bid->aggregate_win_signals);
  EXPECT_EQ(&bid_state->bidder->interest_group, bid->interest_group);
  EXPECT_EQ(&bid_state->bidder->interest_group.ads.value()[0], bid->bid_ad);
  EXPECT_EQ(bid_state, bid->bid_state);
}

TEST_F(AdditionalBidsUtilTest, InvalidBidCurrencyType) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.bidCurrency", 5);
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to invalid bidCurrency.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, InvalidBidCurrencySyntax) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.bidCurrency", "Dollars");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to invalid bidCurrency.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, ValidBidCurrency) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.bidCurrency", "USD");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  EXPECT_TRUE(result.has_value());
  ASSERT_TRUE(result->bid);
  ASSERT_TRUE(result->bid->bid_currency);
  EXPECT_EQ("USD", result->bid->bid_currency->currency_code());
}

TEST_F(AdditionalBidsUtilTest, InvalidAdCost) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.adCost", "big");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to invalid adCost.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, ValidAdCost) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.adCost", 15.5);
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_TRUE(result->bid);
  ASSERT_TRUE(result->bid->ad_cost);
  EXPECT_EQ(15.5, *result->bid->ad_cost);
}

// We have a tradition of ignoring modeling signals if they're out of range,
// so this follows.
TEST_F(AdditionalBidsUtilTest, InvalidModelingSignals) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.modelingSignals", 4096);
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_TRUE(result->bid);
  EXPECT_FALSE(result->bid->modeling_signals);
}

TEST_F(AdditionalBidsUtilTest, InvalidModelingSignals2) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.modelingSignals", -0.001);
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_TRUE(result->bid);
  EXPECT_FALSE(result->bid->modeling_signals);
}

// Bad-type modeling signals still an error, however.
TEST_F(AdditionalBidsUtilTest, BadTypeModelingSignals) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.modelingSignals", "string");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to non-numeric modelingSignals.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, ValidAggregateWinSignals) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  base::Value::Dict aggregate_win_signals_dict;
  aggregate_win_signals_dict.Set("test_string", "hello");
  aggregate_win_signals_dict.Set("test_number", 1.0);
  base::Value::List test_array;
  test_array.Append(1);
  test_array.Append(2);
  test_array.Append(3);
  aggregate_win_signals_dict.Set("test_array", std::move(test_array));
  additional_bid_dict.SetByDottedPath("bid.aggregateWinSignals",
                                      std::move(aggregate_win_signals_dict));

  base::Value input(std::move(additional_bid_dict));
  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_TRUE(result->bid);
  ASSERT_TRUE(result->bid->aggregate_win_signals);
  EXPECT_EQ(
      *result->bid->aggregate_win_signals,
      R"({"test_array":[1,2,3],"test_number":1.0,"test_string":"hello"})");
}

TEST_F(AdditionalBidsUtilTest, InvalidAggregateWinSignals) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  base::Value::Dict aggregate_win_signals_dict;

  // Create a deeply nested list that exceeds the maximum depth
  // for JSON serialization.
  const size_t kMaxDepth = 200;
  base::Value::List deep_list;
  for (size_t i = 0; i < kMaxDepth + 1; ++i) {
    base::Value::List new_top_list;
    new_top_list.Append(std::move(deep_list));
    deep_list = std::move(new_top_list);
  }
  aggregate_win_signals_dict.Set("deeply_nested", std::move(deep_list));

  additional_bid_dict.SetByDottedPath("bid.aggregateWinSignals",
                                      std::move(aggregate_win_signals_dict));

  base::Value input(std::move(additional_bid_dict));
  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));

  // Expect the decoding to fail due to exceeding max depth
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            base::StrCat({"Additional bid on auction with seller '",
                          kSeller.Serialize(),
                          "' rejected due to invalid aggregateWinSignals."}));
}

TEST_F(AdditionalBidsUtilTest, ValidModelingSignals) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.modelingSignals", 0);
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_TRUE(result->bid);
  ASSERT_TRUE(result->bid->modeling_signals);
  EXPECT_EQ(*result->bid->modeling_signals, 0);
}

TEST_F(AdditionalBidsUtilTest, ValidModelingSignals2) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.modelingSignals", 2.5);
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_TRUE(result->bid);
  ASSERT_TRUE(result->bid->modeling_signals);
  EXPECT_EQ(*result->bid->modeling_signals, 2);
}

TEST_F(AdditionalBidsUtilTest, ValidModelingSignals3) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.modelingSignals", 4095.5);
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_TRUE(result->bid);
  ASSERT_TRUE(result->bid->modeling_signals);
  EXPECT_EQ(*result->bid->modeling_signals, 4095);
}

TEST_F(AdditionalBidsUtilTest, InvalidAdComponents) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.adComponents", "oops");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to invalid adComponents.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, InvalidAdComponentsEntry) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  base::Value::List ad_components_list;
  ad_components_list.Append(10);
  additional_bid_dict.SetByDottedPath("bid.adComponents",
                                      std::move(ad_components_list));
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to invalid entry in adComponents.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, TooManyAdComponents) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  base::Value::List ad_components_list;
  const size_t kMaxAdAuctionAdComponents = blink::MaxAdAuctionAdComponents();
  for (size_t i = 0; i < kMaxAdAuctionAdComponents + 1; ++i) {
    ad_components_list.Append("https://en.wikipedia.test/wiki/Locomotive");
  }
  additional_bid_dict.SetByDottedPath("bid.adComponents",
                                      std::move(ad_components_list));
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to too many ad component URLs.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, ValidAdComponents) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  base::Value::List ad_components_list;
  ad_components_list.Append("https://en.wikipedia.test/wiki/Locomotive");
  ad_components_list.Append("https://en.wikipedia.test/wiki/High-speed_rail");
  additional_bid_dict.SetByDottedPath("bid.adComponents",
                                      std::move(ad_components_list));
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_TRUE(result->bid);
  ASSERT_TRUE(result->bid_state);

  // Components should be both in the ad and the synthesized IG.
  ASSERT_EQ(2u, result->bid->selected_ad_components.size());
  EXPECT_EQ(
      blink::AdDescriptor(GURL("https://en.wikipedia.test/wiki/Locomotive")),
      result->bid->selected_ad_components[0].ad_descriptor);
  EXPECT_EQ(blink::AdDescriptor(
                GURL("https://en.wikipedia.test/wiki/High-speed_rail")),
            result->bid->selected_ad_components[1].ad_descriptor);

  ASSERT_TRUE(
      result->bid_state->bidder->interest_group.ad_components.has_value());
  ASSERT_EQ(2u,
            result->bid_state->bidder->interest_group.ad_components->size());
  EXPECT_EQ("https://en.wikipedia.test/wiki/Locomotive",
            result->bid_state->bidder->interest_group.ad_components.value()[0]
                .render_url());
  EXPECT_EQ("https://en.wikipedia.test/wiki/High-speed_rail",
            result->bid_state->bidder->interest_group.ad_components.value()[1]
                .render_url());
  EXPECT_EQ(&result->bid_state->bidder->interest_group.ad_components.value()[0],
            result->bid->selected_ad_components[0].ad);
  EXPECT_EQ(&result->bid_state->bidder->interest_group.ad_components.value()[1],
            result->bid->selected_ad_components[1].ad);
}

TEST_F(AdditionalBidsUtilTest, ValidAdComponentsEmpty) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  base::Value::List ad_components_list;
  additional_bid_dict.SetByDottedPath("bid.adComponents",
                                      std::move(ad_components_list));
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_TRUE(result->bid);
  ASSERT_TRUE(result->bid_state);

  EXPECT_EQ(0u, result->bid->selected_ad_components.size());
  ASSERT_TRUE(
      result->bid_state->bidder->interest_group.ad_components.has_value());
  EXPECT_EQ(0u,
            result->bid_state->bidder->interest_group.ad_components->size());
}

TEST_F(AdditionalBidsUtilTest, ValidAdMetadata) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  base::Value::Dict metadata_dict;
  metadata_dict.Set("a", "hello");
  metadata_dict.Set("b", 1.0);
  additional_bid_dict.SetByDottedPath("bid.ad", std::move(metadata_dict));

  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, /*seller_nonce=*/std::nullopt,
      kInterestGroupBuyers, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_TRUE(result->bid);
  EXPECT_EQ(R"({"a":"hello","b":1.0})", result->bid->ad_metadata);
}

TEST_F(AdditionalBidsUtilTest, ValidSingleNegativeIG) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Set("negativeInterestGroup", "not_if_here");

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, base::Value(std::move(additional_bid_dict)),
      kAuctionNonce, /*seller_nonce=*/std::nullopt, kInterestGroupBuyers,
      kSeller, base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->negative_target_joining_origin.has_value());
  ASSERT_EQ(1u, result->negative_target_interest_group_names.size());
  EXPECT_EQ("not_if_here", result->negative_target_interest_group_names[0]);
}

TEST_F(AdditionalBidsUtilTest, InvalidSingleNegativeIG) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Set("negativeInterestGroup", false);

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, base::Value(std::move(additional_bid_dict)),
      kAuctionNonce, /*seller_nonce=*/std::nullopt, kInterestGroupBuyers,
      kSeller, base::optional_ref<const url::Origin>(kTopSeller));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to non-string 'negativeInterestGroup'.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, InvalidBothKindsOfNegativeIG) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Set("negativeInterestGroup", "not_if_here");
  additional_bid_dict.Set("negativeInterestGroups", "boo");

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, base::Value(std::move(additional_bid_dict)),
      kAuctionNonce, /*seller_nonce=*/std::nullopt, kInterestGroupBuyers,
      kSeller, base::optional_ref<const url::Origin>(kTopSeller));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to specifying both 'negativeInterestGroup' and "
      "'negativeInterestGroups'.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, ValidMultipleNegativeIG) {
  base::Value::Dict additional_bid_dict = MakeValidWithMultipleNegativeIGs();

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, base::Value(std::move(additional_bid_dict)),
      kAuctionNonce, /*seller_nonce=*/std::nullopt, kInterestGroupBuyers,
      kSeller, base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_TRUE(result->negative_target_joining_origin.has_value());
  EXPECT_EQ("https://depot.test",
            result->negative_target_joining_origin->Serialize());
  ASSERT_EQ(2u, result->negative_target_interest_group_names.size());
  EXPECT_EQ("negative_group", result->negative_target_interest_group_names[0]);
  EXPECT_EQ("another negative group",
            result->negative_target_interest_group_names[1]);
}

// Non-string joining origin.
TEST_F(AdditionalBidsUtilTest, InvalidMultipleNegativeIG) {
  base::Value::Dict additional_bid_dict = MakeValidWithMultipleNegativeIGs();
  additional_bid_dict.SetByDottedPath("negativeInterestGroups.joiningOrigin",
                                      10);
  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, base::Value(std::move(additional_bid_dict)),
      kAuctionNonce, /*seller_nonce=*/std::nullopt, kInterestGroupBuyers,
      kSeller, base::optional_ref<const url::Origin>(kTopSeller));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to invalid or missing 'joiningOrigin'.",
      result.error());
}

// Non-HTTPS joining origin.
TEST_F(AdditionalBidsUtilTest, InvalidMultipleNegativeIG2) {
  base::Value::Dict additional_bid_dict = MakeValidWithMultipleNegativeIGs();
  additional_bid_dict.SetByDottedPath("negativeInterestGroups.joiningOrigin",
                                      "http://example.org");
  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, base::Value(std::move(additional_bid_dict)),
      kAuctionNonce, /*seller_nonce=*/std::nullopt, kInterestGroupBuyers,
      kSeller, base::optional_ref<const url::Origin>(kTopSeller));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to invalid or missing 'joiningOrigin'.",
      result.error());
}

// Missing joining origin.
TEST_F(AdditionalBidsUtilTest, InvalidMultipleNegativeIG3) {
  base::Value::Dict additional_bid_dict = MakeValidWithMultipleNegativeIGs();
  additional_bid_dict.RemoveByDottedPath(
      "negativeInterestGroups.joiningOrigin");
  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, base::Value(std::move(additional_bid_dict)),
      kAuctionNonce, /*seller_nonce=*/std::nullopt, kInterestGroupBuyers,
      kSeller, base::optional_ref<const url::Origin>(kTopSeller));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to invalid or missing 'joiningOrigin'.",
      result.error());
}

// Missing interestGroupNames.
TEST_F(AdditionalBidsUtilTest, InvalidMultipleNegativeIG4) {
  base::Value::Dict additional_bid_dict = MakeValidWithMultipleNegativeIGs();
  additional_bid_dict.RemoveByDottedPath(
      "negativeInterestGroups.interestGroupNames");
  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, base::Value(std::move(additional_bid_dict)),
      kAuctionNonce, /*seller_nonce=*/std::nullopt, kInterestGroupBuyers,
      kSeller, base::optional_ref<const url::Origin>(kTopSeller));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or invalid 'interestGroupNames' within "
      "'negativeInterestGroups'.",
      result.error());
}

// interestGroupNames not a list.
TEST_F(AdditionalBidsUtilTest, InvalidMultipleNegativeIG5) {
  base::Value::Dict additional_bid_dict = MakeValidWithMultipleNegativeIGs();
  additional_bid_dict.SetByDottedPath(
      "negativeInterestGroups.interestGroupNames", "hi");
  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, base::Value(std::move(additional_bid_dict)),
      kAuctionNonce, /*seller_nonce=*/std::nullopt, kInterestGroupBuyers,
      kSeller, base::optional_ref<const url::Origin>(kTopSeller));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or invalid 'interestGroupNames' within "
      "'negativeInterestGroups'.",
      result.error());
}

// Non-string entry in interestGroupNames
TEST_F(AdditionalBidsUtilTest, InvalidMultipleNegativeIG6) {
  base::Value::Dict additional_bid_dict = MakeValidWithMultipleNegativeIGs();
  additional_bid_dict
      .FindListByDottedPath("negativeInterestGroups.interestGroupNames")
      ->Append(50);
  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, base::Value(std::move(additional_bid_dict)),
      kAuctionNonce, /*seller_nonce=*/std::nullopt, kInterestGroupBuyers,
      kSeller, base::optional_ref<const url::Origin>(kTopSeller));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to non-string 'interestGroupNames' entry.",
      result.error());
}

// Wrong type for negativeInterestGroups
TEST_F(AdditionalBidsUtilTest, InvalidMultipleNegativeIG7) {
  base::Value::Dict additional_bid_dict = MakeValidWithMultipleNegativeIGs();
  additional_bid_dict.Set("negativeInterestGroups", "boo");
  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, base::Value(std::move(additional_bid_dict)),
      kAuctionNonce, /*seller_nonce=*/std::nullopt, kInterestGroupBuyers,
      kSeller, base::optional_ref<const url::Origin>(kTopSeller));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to non-dictionary 'negativeInterestGroups'.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, DecodeBasicSignedBid) {
  for (bool require_forgiving_base64 : {false, true}) {
    SCOPED_TRACE(require_forgiving_base64);
    base::Value::Dict signed_bid_dict = MakeValidSignedBid();
    if (require_forgiving_base64) {
      *((*signed_bid_dict.FindList("signatures"))[1].GetDict().FindString(
          "signature")) = kSig2Base64Sloppy;
    }
    auto result =
        DecodeSignedAdditionalBid(base::Value(std::move(signed_bid_dict)));
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(kPretendBid, result->additional_bid_json);
    ASSERT_EQ(2u, result->signatures.size());
    EXPECT_EQ(kKey1, result->signatures[0].key);
    EXPECT_EQ(kSig1, result->signatures[0].signature);
    EXPECT_EQ(kKey2, result->signatures[1].key);
    EXPECT_EQ(kSig2, result->signatures[1].signature);
  }
}

TEST_F(AdditionalBidsUtilTest, SignedNotDict) {
  auto result = DecodeSignedAdditionalBid(base::Value(10));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ("Signed additional bid not a dictionary.", result.error());
}

TEST_F(AdditionalBidsUtilTest, DecodeSignedMissingBid) {
  base::Value::Dict signed_bid_dict = MakeValidSignedBid();
  signed_bid_dict.Remove("bid");
  auto result =
      DecodeSignedAdditionalBid(base::Value(std::move(signed_bid_dict)));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ("Signed additional bid missing string 'bid' field.",
            result.error());
}

TEST_F(AdditionalBidsUtilTest, DecodeSignedMissingSignatures) {
  base::Value::Dict signed_bid_dict = MakeValidSignedBid();
  signed_bid_dict.Remove("signatures");
  auto result =
      DecodeSignedAdditionalBid(base::Value(std::move(signed_bid_dict)));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ("Signed additional bid missing list 'signatures' field.",
            result.error());
}

TEST_F(AdditionalBidsUtilTest, DecodeSignedInvalidSignatures) {
  base::Value::Dict signed_bid_dict = MakeValidSignedBid();
  signed_bid_dict.FindList("signatures")->Append(40);
  auto result =
      DecodeSignedAdditionalBid(base::Value(std::move(signed_bid_dict)));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ("Signed additional bid 'signatures' list entry not a dictionary.",
            result.error());
}

TEST_F(AdditionalBidsUtilTest, DecodeSignedMissingSignatureKey) {
  base::Value::Dict signed_bid_dict = MakeValidSignedBid();
  (*signed_bid_dict.FindList("signatures"))[0].GetDict().Remove("key");
  auto result =
      DecodeSignedAdditionalBid(base::Value(std::move(signed_bid_dict)));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Signed additional bid 'signatures' list entry missing 'key' string.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, DecodeSignedInvalidSignatureKey) {
  base::Value::Dict signed_bid_dict = MakeValidSignedBid();
  (*signed_bid_dict.FindList("signatures"))[0].GetDict().Set("key", "$$$");
  auto result =
      DecodeSignedAdditionalBid(base::Value(std::move(signed_bid_dict)));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ("Field 'key' is not valid base64.", result.error());
}

TEST_F(AdditionalBidsUtilTest, DecodeSignedInvalidSignatureKeyLength) {
  const char kLength31[] = "r7J39NbxqA5AvGD57ENOYdOvxzHPwA6KoehNIFCjDw==";

  base::Value::Dict signed_bid_dict = MakeValidSignedBid();
  (*signed_bid_dict.FindList("signatures"))[0].GetDict().Set("key", kLength31);
  auto result =
      DecodeSignedAdditionalBid(base::Value(std::move(signed_bid_dict)));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ("Field 'key' has unexpected length.", result.error());
}

TEST_F(AdditionalBidsUtilTest, DecodeSignedMissingSignatureSig) {
  base::Value::Dict signed_bid_dict = MakeValidSignedBid();
  (*signed_bid_dict.FindList("signatures"))[0].GetDict().Remove("signature");
  auto result =
      DecodeSignedAdditionalBid(base::Value(std::move(signed_bid_dict)));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Signed additional bid 'signatures' list entry missing 'signature' "
      "string.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, DecodeSignedInvalidSignatureSig) {
  base::Value::Dict signed_bid_dict = MakeValidSignedBid();
  (*signed_bid_dict.FindList("signatures"))[0].GetDict().Set("signature",
                                                             "$$$");
  auto result =
      DecodeSignedAdditionalBid(base::Value(std::move(signed_bid_dict)));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ("Field 'signature' is not valid base64.", result.error());
}

TEST_F(AdditionalBidsUtilTest, DecodeSignedInvalidSignatureSigLength) {
  const char kLength65[] =
      "rq9Nm5seElZB7vH9u8o6Cjt4v72LkPKGVKVl6k4uOlmV8Y7n023fmOk47R2bPRNYx/"
      "EzpBSXdJainpItZwK5DTI=";

  base::Value::Dict signed_bid_dict = MakeValidSignedBid();
  (*signed_bid_dict.FindList("signatures"))[0].GetDict().Set("signature",
                                                             kLength65);
  auto result =
      DecodeSignedAdditionalBid(base::Value(std::move(signed_bid_dict)));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ("Field 'signature' has unexpected length.", result.error());
}

TEST_F(AdditionalBidsUtilTest, VerifySignature) {
  const int kKeys = 4;

  struct KeyPairs {
    blink::InterestGroup::AdditionalBidKey public_key;
    std::array<uint8_t, 64> private_key;
  };
  std::array<KeyPairs, kKeys> key_pairs;

  SignedAdditionalBid data;
  data.additional_bid_json = "Greetings. I am JSON!";
  for (int i = 0; i < kKeys; ++i) {
    ED25519_keypair(key_pairs[i].public_key.data(),
                    key_pairs[i].private_key.data());

    data.signatures.emplace_back(SignatureWithKey(key_pairs[i].public_key));

    bool ok = ED25519_sign(
        data.signatures[i].signature.data(),
        reinterpret_cast<const uint8_t*>(data.additional_bid_json.data()),
        data.additional_bid_json.size(), key_pairs[i].private_key.data());
    CHECK(ok);
  }

  EXPECT_THAT(data.VerifySignatures(), UnorderedElementsAre(0, 1, 2, 3));

  // Flip a bit in the [1] signature.
  data.signatures[1].signature[3] ^= 0x02;
  EXPECT_THAT(data.VerifySignatures(), UnorderedElementsAre(0, 2, 3));

  // Flip a couple of bits in the [2] key.
  data.signatures[2].key[7] ^= 0x41;
  EXPECT_THAT(data.VerifySignatures(), UnorderedElementsAre(0, 3));

  // Change the payload.
  data.additional_bid_json += "Boo. Bad unverified data appended!";
  EXPECT_THAT(data.VerifySignatures(), UnorderedElementsAre());
}

class AdditionalBidsUtilNegativeTargetingTest : public AdditionalBidsUtilTest {
 public:
  const url::Origin kBuyer = url::Origin::Create(GURL("https://buyer.test"));
  const url::Origin kOtherBuyer =
      url::Origin::Create(GURL("https://other.test"));

  const url::Origin kJoin = url::Origin::Create(GURL("https://engines.test"));
  const url::Origin kOtherJoin =
      url::Origin::Create(GURL("https://wagons.test"));

  static constexpr size_t kNumNegativeInterestGroups = 4;

  AdditionalBidsUtilNegativeTargetingTest()
      : source_id_(ukm::AssignNewSourceId()), recorder_(source_id_) {
    negative_targeter_.AddInterestGroupInfo(kBuyer, "a", kJoin, kKey1);
    negative_targeter_.AddInterestGroupInfo(kBuyer, "b", kJoin, kKey2);
    negative_targeter_.AddInterestGroupInfo(kOtherBuyer, "c", kJoin, kKey1);
    negative_targeter_.AddInterestGroupInfo(kBuyer, "z", kOtherJoin, kKey1);
  }

  void VerifyMetricValue(std::string metric_name, int64_t expected_value) {
    std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> entries =
        ukm_recorder_.GetEntries(
            ukm::builders::AdsInterestGroup_AuctionLatency_V2::kEntryName,
            {metric_name});
    ASSERT_THAT(entries, testing::SizeIs(1));
    ASSERT_EQ(entries.at(0).source_id, source_id_);
    ASSERT_TRUE(entries.at(0).metrics.contains(metric_name))
        << "Missing expected metric, " << metric_name;
    EXPECT_EQ(entries.at(0).metrics[metric_name], expected_value)
        << "Unexpected value for " << metric_name << " metric";
  }

  void VerifyMetrics(int64_t invalid_signatures,
                     int64_t joining_origin_mismatches) {
    recorder_.OnAuctionEnd(AuctionResult::kSuccess);
    VerifyMetricValue(
        ukm::builders::AdsInterestGroup_AuctionLatency_V2::
            kNumNegativeInterestGroupsIgnoredDueToInvalidSignatureName,
        invalid_signatures);
    VerifyMetricValue(
        ukm::builders::AdsInterestGroup_AuctionLatency_V2::
            kNumNegativeInterestGroupsIgnoredDueToJoiningOriginMismatchName,
        joining_origin_mismatches);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  ukm::SourceId source_id_;
  AuctionMetricsRecorder recorder_;
  AdAuctionNegativeTargeter negative_targeter_;
};

TEST_F(AdditionalBidsUtilNegativeTargetingTest, GetNumNegativeInterestGroups) {
  EXPECT_EQ(negative_targeter_.GetNumNegativeInterestGroups(),
            kNumNegativeInterestGroups);
}

TEST_F(AdditionalBidsUtilNegativeTargetingTest, SuccessfullyNegativeTargets) {
  // Negative targets a, key1 matches that.
  std::vector<std::string> errors_out;
  EXPECT_TRUE(negative_targeter_.ShouldDropDueToNegativeTargeting(
      kBuyer,
      /*negative_target_joining_origin=*/std::nullopt,
      /*negative_target_interest_group_names=*/{"a"},
      /*signatures=*/
      {SignatureWithKey(kKey1), SignatureWithKey(kKey2)},
      /*valid_signatures=*/{0, 1}, kSeller, recorder_, errors_out));
  EXPECT_THAT(errors_out, UnorderedElementsAre());
  VerifyMetrics(
      /*invalid_signatures=*/0,
      /*joining_origin_mismatches=*/0);
}

TEST_F(AdditionalBidsUtilNegativeTargetingTest, WrongBuyer) {
  // Negative targets c, which isn't under kBuyer.
  std::vector<std::string> errors_out;
  EXPECT_FALSE(negative_targeter_.ShouldDropDueToNegativeTargeting(
      kBuyer,
      /*negative_target_joining_origin=*/std::nullopt,
      /*negative_target_interest_group_names=*/{"c"},
      /*signatures=*/
      {SignatureWithKey(kKey1)},
      /*valid_signatures=*/{0}, kSeller, recorder_, errors_out));
  EXPECT_THAT(errors_out, UnorderedElementsAre());
  VerifyMetrics(
      /*invalid_signatures=*/0,
      /*joining_origin_mismatches=*/0);
}

TEST_F(AdditionalBidsUtilNegativeTargetingTest,
       SuccessfulDespiteUnusedBadSignature) {
  // Negative targets a, key1 matches that, but one of the keys is wrong.
  std::vector<std::string> errors_out;
  EXPECT_TRUE(negative_targeter_.ShouldDropDueToNegativeTargeting(
      kBuyer,
      /*negative_target_joining_origin=*/std::nullopt,
      /*negative_target_interest_group_names=*/{"a"},
      /*signatures=*/
      {SignatureWithKey(kKey1), SignatureWithKey(kKey2)},
      /*valid_signatures=*/{0}, kSeller, recorder_, errors_out));
  EXPECT_THAT(
      errors_out,
      UnorderedElementsAre("Warning: Some signatures on an additional bid "
                           "from 'https://buyer.test' on auction with seller "
                           "'https://seller.test' failed to verify."));
  VerifyMetrics(
      /*invalid_signatures=*/0,
      /*joining_origin_mismatches=*/0);
}

TEST_F(AdditionalBidsUtilNegativeTargetingTest, NoMatchingKey) {
  // Negative targets b, key does not match that.
  std::vector<std::string> errors_out;
  EXPECT_FALSE(negative_targeter_.ShouldDropDueToNegativeTargeting(
      kBuyer,
      /*negative_target_joining_origin=*/std::nullopt,
      /*negative_target_interest_group_names=*/{"b"},
      /*signatures=*/
      {SignatureWithKey(kKey1)},
      /*valid_signatures=*/{0}, kSeller, recorder_, errors_out));
  EXPECT_THAT(errors_out,
              UnorderedElementsAre(
                  "Warning: Ignoring negative targeting group 'b' on an "
                  "additional bid from 'https://buyer.test' on auction with "
                  "seller 'https://seller.test' since its key does not "
                  "correspond to a valid signature."));
  VerifyMetrics(
      /*invalid_signatures=*/1,
      /*joining_origin_mismatches=*/0);
}

TEST_F(AdditionalBidsUtilNegativeTargetingTest, SuccessfulDespiteMissingKey) {
  // Negative targets a with invalid key, non-existent c and d,
  // then b with valid key.
  std::vector<std::string> errors_out;
  EXPECT_TRUE(negative_targeter_.ShouldDropDueToNegativeTargeting(
      kBuyer,
      /*negative_target_joining_origin=*/std::nullopt,
      /*negative_target_interest_group_names=*/{"a", "c", "d", "b"},
      /*signatures=*/
      {SignatureWithKey(kKey2)},
      /*valid_signatures=*/{0}, kSeller, recorder_, errors_out));
  EXPECT_THAT(errors_out,
              UnorderedElementsAre(
                  "Warning: Ignoring negative targeting group 'a' on an "
                  "additional bid from 'https://buyer.test' on auction with "
                  "seller 'https://seller.test' since its key does not "
                  "correspond to a valid signature."));
  VerifyMetrics(
      /*invalid_signatures=*/1,
      /*joining_origin_mismatches=*/0);
}

TEST_F(AdditionalBidsUtilNegativeTargetingTest,
       SuccessfulSoDoesNotEvenSeeMissingKey) {
  // Negative targets a with valid key, non-existent c and d,
  // then b with invalid key. We don't get far enough to warn about b.
  std::vector<std::string> errors_out;
  EXPECT_TRUE(negative_targeter_.ShouldDropDueToNegativeTargeting(
      kBuyer,
      /*negative_target_joining_origin=*/std::nullopt,
      /*negative_target_interest_group_names=*/{"a", "c", "d", "b"},
      /*signatures=*/
      {SignatureWithKey(kKey1)},
      /*valid_signatures=*/{0}, kSeller, recorder_, errors_out));
  EXPECT_THAT(errors_out, UnorderedElementsAre());
  VerifyMetrics(
      /*invalid_signatures=*/0,
      /*joining_origin_mismatches=*/0);
}

TEST_F(AdditionalBidsUtilNegativeTargetingTest, JoiningOriginMismatch) {
  // Negative targets a, b with valid keys, but none of the joins match.
  std::vector<std::string> errors_out;
  EXPECT_FALSE(negative_targeter_.ShouldDropDueToNegativeTargeting(
      kBuyer,
      /*negative_target_joining_origin=*/kOtherJoin,
      /*negative_target_interest_group_names=*/{"a", "b"},
      /*signatures=*/
      {SignatureWithKey(kKey1), SignatureWithKey(kKey2)},
      /*valid_signatures=*/{0, 1}, kSeller, recorder_, errors_out));
  EXPECT_THAT(errors_out,
              UnorderedElementsAre(
                  "Warning: Ignoring negative targeting group 'a' on an "
                  "additional bid from 'https://buyer.test' on auction with "
                  "seller 'https://seller.test' since it does not have the "
                  "expected joining origin.",
                  "Warning: Ignoring negative targeting group 'b' on an "
                  "additional bid from 'https://buyer.test' on auction with "
                  "seller 'https://seller.test' since it does not have the "
                  "expected joining origin."));
  VerifyMetrics(
      /*invalid_signatures=*/0,
      /*joining_origin_mismatches=*/2);
}

TEST_F(AdditionalBidsUtilNegativeTargetingTest,
       SuccessfulWithMultipleNegativeInterestGroups) {
  // Negative targets a, b with valid keys; all of the joins match.
  std::vector<std::string> errors_out;
  EXPECT_TRUE(negative_targeter_.ShouldDropDueToNegativeTargeting(
      kBuyer,
      /*negative_target_joining_origin=*/kJoin,
      /*negative_target_interest_group_names=*/{"a", "b"},
      /*signatures=*/
      {SignatureWithKey(kKey1), SignatureWithKey(kKey2)},
      /*valid_signatures=*/{0, 1}, kSeller, recorder_, errors_out));
  EXPECT_THAT(errors_out, UnorderedElementsAre());
  VerifyMetrics(
      /*invalid_signatures=*/0,
      /*joining_origin_mismatches=*/0);
}

TEST_F(AdditionalBidsUtilNegativeTargetingTest,
       SuccessfulDespiteTwoJoiningOriginMismatches) {
  // Negative targets a, b,z with valid keys; only the join on z matches.
  std::vector<std::string> errors_out;
  EXPECT_TRUE(negative_targeter_.ShouldDropDueToNegativeTargeting(
      kBuyer,
      /*negative_target_joining_origin=*/kOtherJoin,
      /*negative_target_interest_group_names=*/{"a", "b", "z"},
      /*signatures=*/
      {SignatureWithKey(kKey1), SignatureWithKey(kKey2)},
      /*valid_signatures=*/{0, 1}, kSeller, recorder_, errors_out));
  EXPECT_THAT(errors_out,
              UnorderedElementsAre(
                  "Warning: Ignoring negative targeting group 'a' on an "
                  "additional bid from 'https://buyer.test' on auction with "
                  "seller 'https://seller.test' since it does not have the "
                  "expected joining origin.",
                  "Warning: Ignoring negative targeting group 'b' on an "
                  "additional bid from 'https://buyer.test' on auction with "
                  "seller 'https://seller.test' since it does not have the "
                  "expected joining origin."));
  VerifyMetrics(
      /*invalid_signatures=*/0,
      /*joining_origin_mismatches=*/2);
}

}  // namespace
}  // namespace content
