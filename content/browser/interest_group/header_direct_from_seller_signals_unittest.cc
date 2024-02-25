// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/header_direct_from_seller_signals.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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

constexpr char kOriginStringA[] = "a.test";
constexpr char kOriginStringB[] = "b.test";

class HeaderDirectFromSellerSignalsTest : public ::testing::Test {
 protected:
  scoped_refptr<HeaderDirectFromSellerSignals::Result> ParseAndFind(
      const url::Origin& origin,
      const std::string& ad_slot) {
    // Synchronously look up the signals for a given `origin`, `ad_slot` tuple.
    scoped_refptr<HeaderDirectFromSellerSignals::Result> my_result;
    base::RunLoop run_loop;
    header_direct_from_seller_signals_.ParseAndFind(
        origin, ad_slot,
        base::BindLambdaForTesting(
            [&my_result, &run_loop](
                scoped_refptr<HeaderDirectFromSellerSignals::Result> result) {
              my_result = std::move(result);
              run_loop.Quit();
            }));
    run_loop.Run();
    return my_result;
  }

  // Synchronously add a series of witnesses for an origin `origin`, returning a
  // vector of all error strings returned.
  std::vector<std::string> AddWitnessesForOrigin(
      const url::Origin& origin,
      const std::vector<std::string>& responses) {
    std::vector<std::string> all_errors;
    for (const std::string& response : responses) {
      base::RunLoop run_loop;
      header_direct_from_seller_signals_.AddWitnessForOrigin(
          data_decoder_, origin, response,
          base::BindLambdaForTesting(
              [&all_errors, &run_loop](std::vector<std::string> errors) {
                all_errors.reserve(all_errors.size() + errors.size());
                std::move(std::begin(errors), std::end(errors),
                          std::back_inserter(all_errors));
                run_loop.Quit();
              }));
      run_loop.Run();
    }
    return all_errors;
  }

  const url::Origin kOriginA = url::Origin::Create(GURL(kOriginStringA));
  const url::Origin kOriginB = url::Origin::Create(GURL(kOriginStringB));

  HeaderDirectFromSellerSignals header_direct_from_seller_signals_;
  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder data_decoder_provider_;
  data_decoder::DataDecoder data_decoder_;
};

TEST_F(HeaderDirectFromSellerSignalsTest, DefaultConstruct) {
  auto signals = base::MakeRefCounted<HeaderDirectFromSellerSignals::Result>();

  EXPECT_EQ(signals->seller_signals(), std::nullopt);
  EXPECT_EQ(signals->auction_signals(), std::nullopt);
  EXPECT_THAT(signals->per_buyer_signals(), IsEmpty());
}

TEST_F(HeaderDirectFromSellerSignalsTest, Valid) {
  const std::vector<std::string> kResponses = {R"([{
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
  std::vector<std::string> errors = AddWitnessesForOrigin(kOriginA, kResponses);
  EXPECT_THAT(errors, IsEmpty());

  scoped_refptr<HeaderDirectFromSellerSignals::Result> parsed1 =
      ParseAndFind(kOriginA, "slot1");
  ASSERT_NE(parsed1, nullptr);
  EXPECT_EQ(parsed1->seller_signals(), "[\"signals\",\"for\",\"seller\"]");
  EXPECT_EQ(parsed1->auction_signals(), "42");
  EXPECT_THAT(
      parsed1->per_buyer_signals(),
      UnorderedElementsAre(
          Pair(url::Origin::Create(GURL("https://buyer1.com")), "false"),
          Pair(url::Origin::Create(GURL("https://buyer2.com")),
               R"({"an":"object"})")));

  scoped_refptr<HeaderDirectFromSellerSignals::Result> parsed2 =
      ParseAndFind(kOriginA, "slot2");
  ASSERT_NE(parsed2, nullptr);
  EXPECT_EQ(parsed2->seller_signals(), "[\"signals2\",\"for\",\"seller\"]");
  EXPECT_EQ(parsed2->auction_signals(), std::nullopt);
  EXPECT_THAT(parsed2->per_buyer_signals(), IsEmpty());

  scoped_refptr<HeaderDirectFromSellerSignals::Result> parsed3 =
      ParseAndFind(kOriginA, "slot3");
  ASSERT_NE(parsed3, nullptr);
  EXPECT_EQ(parsed3->seller_signals(), std::nullopt);
  EXPECT_EQ(parsed3->auction_signals(), "null");
  EXPECT_THAT(parsed3->per_buyer_signals(), IsEmpty());
}

TEST_F(HeaderDirectFromSellerSignalsTest, TwoOrigins) {
  // Intentionally use the same adSlot -- signals from separate origins should
  // be held separately.
  constexpr char kResponseA[] = R"([{
    "adSlot": "slot1",
    "sellerSignals": "abc"
  }])";
  constexpr char kResponseB[] = R"([{
    "adSlot": "slot1",
    "sellerSignals": "def"
  }])";

  AddWitnessesForOrigin(kOriginA, {kResponseA});
  AddWitnessesForOrigin(kOriginB, {kResponseB});

  scoped_refptr<HeaderDirectFromSellerSignals::Result> result_a =
      ParseAndFind(kOriginA, "slot1");
  scoped_refptr<HeaderDirectFromSellerSignals::Result> result_b =
      ParseAndFind(kOriginB, "slot1");

  ASSERT_NE(result_a, nullptr);
  EXPECT_EQ(result_a->seller_signals(), "\"abc\"");

  ASSERT_NE(result_b, nullptr);
  EXPECT_EQ(result_b->seller_signals(), "\"def\"");
}

TEST_F(HeaderDirectFromSellerSignalsTest, InvalidAndOrNotFound) {
  struct {
    const std::vector<std::string> responses;
    const std::vector<testing::Matcher<std::string>> errors;
  } kCases[] = {
      {{R"(This is not JSON)"},
       {MatchesRegex(
           // NOTE: the JSON error varies by platform (Android uses a Java JSON
           // parser), so use a regex to ignore the actual error message.
           "directFromSellerSignalsHeaderAdSlot: encountered invalid JSON: "
           "'.+' for Ad-Auction-Signals=This is not JSON")}},
      {{R"({"Not": "a list"})"},
       {Eq("directFromSellerSignalsHeaderAdSlot: encountered response where "
           "top-level JSON value isn't an array: Ad-Auction-Signals={\"Not\": "
           "\"a list\"}")}},
      {{R"(["Not a dict"])"},
       {Eq("directFromSellerSignalsHeaderAdSlot: encountered non-dict list "
           "item: Ad-AuctionSignals=[\"Not a dict\"]")}},
      {{R"([{"no":"adSlot"}])"},
       {Eq("directFromSellerSignalsHeaderAdSlot: encountered dict without "
           "\"adSlot\" key: Ad-Auction-Signals=[{\"no\":\"adSlot\"}]")}},
      {{R"([{"no":"adSlot"}, "not a dict"])"},
       {Eq("directFromSellerSignalsHeaderAdSlot: encountered dict without "
           "\"adSlot\" key: Ad-Auction-Signals=[{\"no\":\"adSlot\"}, \"not a "
           "dict\"]"),
        Eq("directFromSellerSignalsHeaderAdSlot: encountered non-dict list "
           "item: Ad-AuctionSignals=[{\"no\":\"adSlot\"}, \"not a dict\"]")}},
      {{R"([{"no":"adSlot"}])", R"(["not a dict"])"},
       {Eq("directFromSellerSignalsHeaderAdSlot: encountered dict without "
           "\"adSlot\" key: Ad-Auction-Signals=[{\"no\":\"adSlot\"}]"),
        Eq("directFromSellerSignalsHeaderAdSlot: encountered non-dict list "
           "item: Ad-AuctionSignals=[\"not a dict\"]")}},
      {{R"([{"adSlot":"slot2", "sellerSignals":3}])"}, {}},
      {{R"([{"adSlot":"slot2", "sellerSignals":3}])",
        R"([{"adSlot":"slot3", "sellerSignals":4}])"},
       {}},
      {{R"([{"adSlot":"slot2", "sellerSignals":3},
            {"adSlot":"slot3", "sellerSignals":4}])"},
       {}},
      {{}, {}}};

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(testing::PrintToString(test_case.responses));
    std::vector<std::string> errors =
        AddWitnessesForOrigin(kOriginA, test_case.responses);
    EXPECT_THAT(errors, UnorderedElementsAreArray(test_case.errors));
    scoped_refptr<HeaderDirectFromSellerSignals::Result> parsed =
        ParseAndFind(kOriginA, "slot1");
    EXPECT_EQ(parsed, nullptr);
  }
}

TEST_F(HeaderDirectFromSellerSignalsTest, ContinueOnInvalid) {
  const std::vector<std::string> kResponses = {R"(This is not JSON)",
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
    }, {
      "adSlot": "slot1",
      "sellerSignals": "dupe slot1, should error and be skipped"
    }
  ])"};

  std::vector<std::string> errors = AddWitnessesForOrigin(kOriginA, kResponses);
  EXPECT_THAT(
      errors,
      UnorderedElementsAre(
          MatchesRegex(
              // NOTE: the JSON error varies by platform (Android uses a Java
              // JSON parser), so use a regex to ignore the actual error
              // message.
              "directFromSellerSignalsHeaderAdSlot: encountered invalid JSON: "
              "'.+' for Ad-Auction-Signals=This is not JSON"),
          Eq("directFromSellerSignalsHeaderAdSlot: encountered non-dict list "
             "item: Ad-AuctionSignals=[\n    \"Not a dict\", {\n      "
             "\"adSlot\": \"slot2\",\n      \"sellerSignals\": \"other "
             "signals\"\n    }, {\n      \"adSlot\": \"slot1\",\n      "
             "\"sellerSignals\": \"signals\",\n      \"perBuyerSignals\": {\n  "
             "      \"badorigin\": 1,\n        \"https://valid.com\": 2\n      "
             "}\n    }, {\n      \"adSlot\": \"slot1\",\n      "
             "\"sellerSignals\": \"dupe slot1, should error and be skipped\"\n "
             "   }\n  ]"),
          Eq("directFromSellerSignalsHeaderAdSlot: encountered non-https "
             "perBuyerSignals origin 'badorigin': Ad-Auction-Signals=[\n    "
             "\"Not a dict\", {\n      \"adSlot\": \"slot2\",\n      "
             "\"sellerSignals\": \"other signals\"\n    }, {\n      "
             "\"adSlot\": \"slot1\",\n      \"sellerSignals\": \"signals\",\n  "
             "    \"perBuyerSignals\": {\n        \"badorigin\": 1,\n        "
             "\"https://valid.com\": 2\n      }\n    }, {\n      \"adSlot\": "
             "\"slot1\",\n      \"sellerSignals\": \"dupe slot1, should error "
             "and be skipped\"\n    }\n  ]"),
          Eq("directFromSellerSignalsHeaderAdSlot: encountered dict with "
             "duplicate adSlot key \"slot1\": Ad-Auction-Signals=[\n    \"Not "
             "a dict\", {\n      \"adSlot\": \"slot2\",\n      "
             "\"sellerSignals\": \"other signals\"\n    }, {\n      "
             "\"adSlot\": \"slot1\",\n      \"sellerSignals\": \"signals\",\n  "
             "    \"perBuyerSignals\": {\n        \"badorigin\": 1,\n        "
             "\"https://valid.com\": 2\n      }\n    }, {\n      \"adSlot\": "
             "\"slot1\",\n      \"sellerSignals\": \"dupe slot1, should error "
             "and be skipped\"\n    }\n  ]")));
  scoped_refptr<HeaderDirectFromSellerSignals::Result> parsed =
      ParseAndFind(kOriginA, "slot1");
  ASSERT_NE(parsed, nullptr);
  EXPECT_EQ(parsed->seller_signals(), R"("signals")");
  EXPECT_EQ(parsed->auction_signals(), std::nullopt);
  EXPECT_THAT(parsed->per_buyer_signals(),
              UnorderedElementsAre(
                  Pair(url::Origin::Create(GURL("https://valid.com")), "2")));
}

TEST_F(HeaderDirectFromSellerSignalsTest, OverwriteOldAdSlotSignals) {
  const std::vector<std::string> kResponses = {R"([{
    "adSlot": "slot1",
    "sellerSignals": "version1"
  }])",
                                               R"([{
    "adSlot": "slot1",
    "sellerSignals": "version2"
  }])"};

  std::vector<std::string> errors = AddWitnessesForOrigin(kOriginA, kResponses);
  EXPECT_THAT(errors, IsEmpty());

  scoped_refptr<HeaderDirectFromSellerSignals::Result> parsed =
      ParseAndFind(kOriginA, "slot1");
  ASSERT_NE(parsed, nullptr);
  EXPECT_EQ(parsed->seller_signals(), "\"version2\"");
}

// Same as OverwriteOldAdSlotSignals, but the signals aren't parsed before
// ParseAndRun() is invoked.
TEST_F(HeaderDirectFromSellerSignalsTest,
       AsyncOrder_OverwriteOldAdSlotSignals) {
  constexpr char kResponse1[] = R"([{
    "adSlot": "slot1",
    "sellerSignals": "version1"
  }])";
  constexpr char kResponse2[] = R"([{
    "adSlot": "slot1",
    "sellerSignals": "version2"
  }])";

  base::RunLoop add_witness1_run_loop;
  header_direct_from_seller_signals_.AddWitnessForOrigin(
      data_decoder_, kOriginA, kResponse1,
      base::BindLambdaForTesting(
          [&add_witness1_run_loop](std::vector<std::string> errors) {
            EXPECT_THAT(errors, IsEmpty());
            add_witness1_run_loop.Quit();
          }));

  header_direct_from_seller_signals_.AddWitnessForOrigin(
      data_decoder_, kOriginA, kResponse2,
      base::BindLambdaForTesting([](std::vector<std::string> errors) {
        // Only the first AddWitnessForOrigin() should be called -- it's given
        // all errors, if any.
        ADD_FAILURE() << "Shouldn't be called";
      }));

  base::RunLoop parse_and_find_run_loop;
  header_direct_from_seller_signals_.ParseAndFind(
      kOriginA, "slot1",
      base::BindLambdaForTesting(
          [&parse_and_find_run_loop](
              scoped_refptr<HeaderDirectFromSellerSignals::Result> result) {
            ASSERT_NE(result, nullptr);
            EXPECT_EQ(result->seller_signals(), "\"version2\"");
            parse_and_find_run_loop.Quit();
          }));

  add_witness1_run_loop.Run();
  parse_and_find_run_loop.Run();
}

// A response is received, and ParseAndFind() is called before the response is
// processed.
TEST_F(HeaderDirectFromSellerSignalsTest,
       AsyncOrder_ParseAndFindBeforeResponseProcessed) {
  constexpr char kResponse[] = R"([{
    "adSlot": "slot1",
    "sellerSignals": "abc"
  }])";

  base::RunLoop add_witness_run_loop;
  header_direct_from_seller_signals_.AddWitnessForOrigin(
      data_decoder_, kOriginA, kResponse,
      base::BindLambdaForTesting(
          [&add_witness_run_loop](std::vector<std::string> errors) {
            EXPECT_THAT(errors, IsEmpty());
            add_witness_run_loop.Quit();
          }));

  base::RunLoop parse_and_find_run_loop;
  header_direct_from_seller_signals_.ParseAndFind(
      kOriginA, "slot1",
      base::BindLambdaForTesting(
          [&parse_and_find_run_loop](
              scoped_refptr<HeaderDirectFromSellerSignals::Result> result) {
            EXPECT_NE(result, nullptr);
            parse_and_find_run_loop.Quit();
          }));

  add_witness_run_loop.Run();
  parse_and_find_run_loop.Run();
}

// Two responses are received, and ParseAndFind() is called before the responses
// are processed.
TEST_F(HeaderDirectFromSellerSignalsTest,
       AsyncOrder_ParseAndFindBefore2ResponsesProcessed) {
  constexpr char kResponse1[] = R"([{
    "adSlot": "slot1"
  }])";
  constexpr char kResponse2[] = R"([{
    "adSlot": "slot2"
  }])";

  base::RunLoop add_witness1_run_loop;
  header_direct_from_seller_signals_.AddWitnessForOrigin(
      data_decoder_, kOriginA, kResponse1,
      base::BindLambdaForTesting(
          [&add_witness1_run_loop](std::vector<std::string> errors) {
            EXPECT_THAT(errors, IsEmpty());
            add_witness1_run_loop.Quit();
          }));

  header_direct_from_seller_signals_.AddWitnessForOrigin(
      data_decoder_, kOriginA, kResponse2,
      base::BindLambdaForTesting([](std::vector<std::string> errors) {
        // Only the first AddWitnessForOrigin() should be called -- it's given
        // all errors, if any.
        ADD_FAILURE() << "Shouldn't be called";
      }));

  base::RunLoop parse_and_find_run_loop;
  header_direct_from_seller_signals_.ParseAndFind(
      kOriginA, "slot2",
      base::BindLambdaForTesting(
          [&parse_and_find_run_loop](
              scoped_refptr<HeaderDirectFromSellerSignals::Result> result) {
            EXPECT_NE(result, nullptr);
            parse_and_find_run_loop.Quit();
          }));

  add_witness1_run_loop.Run();
  parse_and_find_run_loop.Run();
}

// A response is received, ParseAndFind() is called, then another response is
// received, and then processing completes.
TEST_F(HeaderDirectFromSellerSignalsTest,
       AsyncOrder_ParseAndFindBetween2Responses) {
  constexpr char kResponse1[] = R"([{
    "adSlot": "slot1"
  }])";
  constexpr char kResponse2[] = R"([{
    "adSlot": "slot2"
  }])";

  base::RunLoop add_witness1_run_loop;
  header_direct_from_seller_signals_.AddWitnessForOrigin(
      data_decoder_, kOriginA, kResponse1,
      base::BindLambdaForTesting(
          [&add_witness1_run_loop](std::vector<std::string> errors) {
            EXPECT_THAT(errors, IsEmpty());
            add_witness1_run_loop.Quit();
          }));

  base::RunLoop parse_and_find_run_loop;
  header_direct_from_seller_signals_.ParseAndFind(
      kOriginA, "slot2",
      base::BindLambdaForTesting(
          [&parse_and_find_run_loop](
              scoped_refptr<HeaderDirectFromSellerSignals::Result> result) {
            EXPECT_NE(result, nullptr);
            parse_and_find_run_loop.Quit();
          }));

  header_direct_from_seller_signals_.AddWitnessForOrigin(
      data_decoder_, kOriginA, kResponse2,
      base::BindLambdaForTesting([](std::vector<std::string> errors) {
        // Only the first AddWitnessForOrigin() should be called -- it's given
        // all errors, if any.
        ADD_FAILURE() << "Shouldn't be called";
      }));

  add_witness1_run_loop.Run();
  parse_and_find_run_loop.Run();
}

// Two responses with errors are received, and ParseAndFind() is called before
// the responses are processed.
TEST_F(HeaderDirectFromSellerSignalsTest,
       AsyncOrder_ParseAndFindBefore2ResponsesProcessedWithErrors) {
  constexpr char kResponse1[] = R"({"Not": "a list"})";
  constexpr char kResponse2[] = R"({"AlsoNot": "a list"})";

  base::RunLoop add_witness1_run_loop;
  header_direct_from_seller_signals_.AddWitnessForOrigin(
      data_decoder_, kOriginA, kResponse1,
      base::BindLambdaForTesting(
          [&add_witness1_run_loop](std::vector<std::string> errors) {
            EXPECT_THAT(
                errors,
                UnorderedElementsAre(
                    Eq("directFromSellerSignalsHeaderAdSlot: encountered "
                       "response where top-level JSON value isn't an "
                       "array: Ad-Auction-Signals={\"Not\": \"a list\"}"),
                    Eq("directFromSellerSignalsHeaderAdSlot: encountered "
                       "response where top-level JSON value isn't an array: "
                       "Ad-Auction-Signals={\"AlsoNot\": \"a list\"}")));
            add_witness1_run_loop.Quit();
          }));

  header_direct_from_seller_signals_.AddWitnessForOrigin(
      data_decoder_, kOriginA, kResponse2,
      base::BindLambdaForTesting([](std::vector<std::string> errors) {
        // Only the first AddWitnessForOrigin() should be called -- it's given
        // all errors, if any.
        ADD_FAILURE() << "Shouldn't be called";
      }));

  base::RunLoop parse_and_find_run_loop;
  header_direct_from_seller_signals_.ParseAndFind(
      kOriginA, "slot2",
      base::BindLambdaForTesting(
          [&parse_and_find_run_loop](
              scoped_refptr<HeaderDirectFromSellerSignals::Result> result) {
            EXPECT_EQ(result, nullptr);
            parse_and_find_run_loop.Quit();
          }));

  add_witness1_run_loop.Run();
  parse_and_find_run_loop.Run();
}

TEST_F(HeaderDirectFromSellerSignalsTest,
       AsyncOrder_DeleteDecoderDuringParsing) {
  constexpr char kResponse[] = R"([{
    "adSlot": "slot1",
    "sellerSignals": "abc"
  }])";
  std::optional<data_decoder::DataDecoder> data_decoder;
  data_decoder.emplace();

  header_direct_from_seller_signals_.AddWitnessForOrigin(
      *data_decoder, kOriginA, kResponse,
      base::BindLambdaForTesting([](std::vector<std::string> errors) {
        EXPECT_THAT(errors, IsEmpty());
        ADD_FAILURE() << "Shouldn't be called";
      }));

  header_direct_from_seller_signals_.ParseAndFind(
      kOriginA, "slot1",
      base::BindLambdaForTesting(
          [](scoped_refptr<HeaderDirectFromSellerSignals::Result> result) {
            ADD_FAILURE() << "Shouldn't be called";
          }));
  data_decoder.reset();
  task_environment_.RunUntilIdle();
}

}  // namespace

}  // namespace content
