// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/header_direct_from_seller_signals.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::MatchesRegex;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

class HeaderDirectFromSellerSignalsTest : public ::testing::Test {
 protected:
  std::unique_ptr<HeaderDirectFromSellerSignals> ParseAndFind(
      const std::set<std::string>& responses,
      std::string ad_slot) {
    std::unique_ptr<HeaderDirectFromSellerSignals> result;
    base::RunLoop run_loop;
    HeaderDirectFromSellerSignals::ParseAndFind(
        responses, std::move(ad_slot),
        base::BindLambdaForTesting(
            [this, &result, &run_loop](
                std::unique_ptr<HeaderDirectFromSellerSignals> parsed,
                std::vector<std::string> errors) {
              result = std::move(parsed);
              errors_ = std::move(errors);
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder data_decoder_;

  std::vector<std::string> errors_;
};

TEST_F(HeaderDirectFromSellerSignalsTest, DefaultConstruct) {
  HeaderDirectFromSellerSignals signals;

  EXPECT_EQ(signals.seller_signals(), absl::nullopt);
  EXPECT_EQ(signals.auction_signals(), absl::nullopt);
  EXPECT_THAT(signals.per_buyer_signals(), IsEmpty());
}

TEST_F(HeaderDirectFromSellerSignalsTest, Valid) {
  const std::set<std::string> kResponses = {R"([{
        "adSlot": "slot1",
        "sellerSignals": ["signals", "for", "seller"],
        "auctionSignals": 42,
        "perBuyerSignals": {
          "https://buyer1.com": false,
          "https://buyer2.com": {
            "an": "object"
          }
        }
      }, {
        "adSlot": "slot2",
        "sellerSignals": ["signals2", "for", "seller"]
      }])",
                                            R"([{
        "adSlot": "slot3",
        "auctionSignals": null
      }])"};

  std::unique_ptr<HeaderDirectFromSellerSignals> parsed1 =
      ParseAndFind(kResponses, "slot1");
  EXPECT_EQ(parsed1->seller_signals(), "[\"signals\",\"for\",\"seller\"]");
  EXPECT_EQ(parsed1->auction_signals(), "42");
  EXPECT_THAT(
      parsed1->per_buyer_signals(),
      UnorderedElementsAre(
          Pair(url::Origin::Create(GURL("https://buyer1.com")), "false"),
          Pair(url::Origin::Create(GURL("https://buyer2.com")),
               R"({"an":"object"})")));
  EXPECT_THAT(errors_, IsEmpty());

  std::unique_ptr<HeaderDirectFromSellerSignals> parsed2 =
      ParseAndFind(kResponses, "slot2");
  EXPECT_EQ(parsed2->seller_signals(), "[\"signals2\",\"for\",\"seller\"]");
  EXPECT_EQ(parsed2->auction_signals(), absl::nullopt);
  EXPECT_THAT(parsed2->per_buyer_signals(), IsEmpty());
  EXPECT_THAT(errors_, IsEmpty());

  std::unique_ptr<HeaderDirectFromSellerSignals> parsed3 =
      ParseAndFind(kResponses, "slot3");
  EXPECT_EQ(parsed3->seller_signals(), absl::nullopt);
  EXPECT_EQ(parsed3->auction_signals(), "null");
  EXPECT_THAT(parsed3->per_buyer_signals(), IsEmpty());
  EXPECT_THAT(errors_, IsEmpty());
}

TEST_F(HeaderDirectFromSellerSignalsTest, Invalid) {
  struct {
    const std::set<std::string> responses;
    const std::vector<testing::Matcher<std::string>> errors;
  } kCases[] = {
      {{R"(This is not JSON)"},
       {MatchesRegex(
            // NOTE: the JSON error varies by platform (Android uses a Java JSON
            // parser), so use a regex to ignore the actual error message.
            "When looking for directFromSellerSignalsHeaderAdSlot slot1, "
            "encountered invalid JSON: '.+' for Ad-Auction-Signals=This is not "
            "JSON"),
        Eq("When looking for directFromSellerSignalsHeaderAdSlot slot1, failed "
           "to find a matching response.")}},
      {{R"({"Not": "a list"})"},
       {Eq("When looking for directFromSellerSignalsHeaderAdSlot slot1, "
           "encountered response where top-level JSON value isn't an array: "
           "Ad-Auction-Signals={\"Not\": \"a list\"}"),
        Eq("When looking for directFromSellerSignalsHeaderAdSlot slot1, failed "
           "to find a matching response.")}},
      {{R"(["Not a dict"])"},
       {Eq("When looking for directFromSellerSignalsHeaderAdSlot slot1, "
           "encountered non-dict list item: Ad-AuctionSignals=[\"Not a "
           "dict\"]"),
        Eq("When looking for directFromSellerSignalsHeaderAdSlot slot1, failed "
           "to find a matching response.")}},
      {{R"([{"no":"adSlot"}])"},
       {Eq("When looking for directFromSellerSignalsHeaderAdSlot slot1, "
           "encountered dict without \"adSlot\" key: "
           "Ad-Auction-Signals=[{\"no\":\"adSlot\"}]"),
        Eq("When looking for directFromSellerSignalsHeaderAdSlot slot1, failed "
           "to find a matching response.")}},
      {{R"([{"adSlot":"slot2", "sellerSignals":3}])"},
       {Eq("When looking for directFromSellerSignalsHeaderAdSlot slot1, failed "
           "to find a matching response.")}},
      {{},
       {Eq("When looking for directFromSellerSignalsHeaderAdSlot slot1, failed "
           "to find a matching response.")}}};

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(testing::PrintToString(test_case.responses));
    std::unique_ptr<HeaderDirectFromSellerSignals> parsed =
        ParseAndFind(test_case.responses, "slot1");
    EXPECT_EQ(parsed->seller_signals(), absl::nullopt);
    EXPECT_EQ(parsed->auction_signals(), absl::nullopt);
    EXPECT_THAT(parsed->per_buyer_signals(), IsEmpty());
    EXPECT_THAT(errors_, UnorderedElementsAreArray(test_case.errors));
  }
}

TEST_F(HeaderDirectFromSellerSignalsTest, ContinueOnInvalid) {
  const std::set<std::string> kResponses = {R"(This is not JSON)",
                                            R"([
    "Not a dict", {
      "adSlot": "slot2",
      "sellerSignals": "other signals"
    }, {
      "adSlot": "slot1",
      "sellerSignals": "signals",
      "perBuyerSignals": {
        "badorigin": 1,
        "https://valid.com": 2
      }
    }
  ])"};

  std::unique_ptr<HeaderDirectFromSellerSignals> parsed =
      ParseAndFind(kResponses, "slot1");
  EXPECT_EQ(parsed->seller_signals(), R"("signals")");
  EXPECT_EQ(parsed->auction_signals(), absl::nullopt);
  EXPECT_THAT(parsed->per_buyer_signals(),
              UnorderedElementsAre(
                  Pair(url::Origin::Create(GURL("https://valid.com")), "2")));
  EXPECT_THAT(
      errors_,
      UnorderedElementsAre(
          MatchesRegex(
              // NOTE: the JSON error varies by platform (Android uses a Java
              // JSON parser), so use a regex to ignore the actual error
              // message.
              "When looking for directFromSellerSignalsHeaderAdSlot slot1, "
              "encountered invalid JSON: '.+' for Ad-Auction-Signals=This is "
              "not JSON"),
          Eq("When looking for directFromSellerSignalsHeaderAdSlot slot1, "
             "encountered non-dict list item: Ad-AuctionSignals=[\n    \"Not a "
             "dict\", {\n      \"adSlot\": \"slot2\",\n      "
             "\"sellerSignals\": \"other signals\"\n    }, {\n      "
             "\"adSlot\": \"slot1\",\n      \"sellerSignals\": \"signals\",\n  "
             "    \"perBuyerSignals\": {\n        \"badorigin\": 1,\n        "
             "\"https://valid.com\": 2\n      }\n    }\n  ]"),
          Eq("When looking for directFromSellerSignalsHeaderAdSlot slot1, "
             "encountered non-https perBuyerSignals origin 'badorigin': "
             "Ad-Auction-Signals=[\n    \"Not a dict\", {\n      \"adSlot\": "
             "\"slot2\",\n      \"sellerSignals\": \"other signals\"\n    }, "
             "{\n      \"adSlot\": \"slot1\",\n      \"sellerSignals\": "
             "\"signals\",\n      \"perBuyerSignals\": {\n        "
             "\"badorigin\": 1,\n        \"https://valid.com\": 2\n      }\n   "
             " }\n  ]")));
}

}  // namespace

}  // namespace content
