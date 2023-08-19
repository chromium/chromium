// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/bidding_and_auction_response.h"

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

const char kOwnerOrigin[] = "https://owner.example.com";
const char kUntrustedURL[] = "http://untrusted.example.com/foo";
const char kReportingURL[] = "https://reporting.example.com/report";

const base::flat_map<url::Origin, std::vector<std::string>> GroupNames() {
  return base::MakeFlatMap<url::Origin, std::vector<std::string>>(
      std::vector<std::pair<url::Origin, std::vector<std::string>>>{
          {
              url::Origin::Create(GURL(kOwnerOrigin)),
              std::vector<std::string>{"name", "name2", "name3"},
          },
          {
              url::Origin::Create(GURL("https://otherowner.example.com")),
              std::vector<std::string>{"foo"},
          },
      });
}

BiddingAndAuctionResponse CreateExpectedValidResponse() {
  BiddingAndAuctionResponse response;
  response.is_chaff = false;
  response.ad_render_url = GURL("https://example.com/ad");
  response.ad_components = {GURL("https://example.com/component")};
  response.interest_group_name = "name";
  response.interest_group_owner = url::Origin::Create(GURL(kOwnerOrigin));
  response.bidding_groups = {
      {url::Origin::Create(GURL(kOwnerOrigin)), "name"},
      {url::Origin::Create(GURL(kOwnerOrigin)), "name2"}};
  return response;
}

base::Value::Dict CreateValidResponseDict() {
  return base::Value::Dict()
      .Set("isChaff", false)
      .Set("adRenderURL", "https://example.com/ad")
      .Set("components", base::Value(base::Value::List().Append(
                             "https://example.com/component")))
      .Set("interestGroupName", "name")
      .Set("interestGroupOwner", kOwnerOrigin)
      .Set("biddingGroups",
           base::Value(base::Value::Dict().Set(
               kOwnerOrigin,
               base::Value(base::Value::List().Append(0).Append(1)))));
}

std::string ToString(
    const BiddingAndAuctionResponse::ReportingURLs& reporting) {
  return std::string("ReportingURLs(") +
         "reporting_url: " + testing::PrintToString(reporting.reporting_url) +
         ", " +
         "beacon_urls: " + testing::PrintToString(reporting.beacon_urls) + ")";
}

std::string ToString(const BiddingAndAuctionResponse& response) {
  return std::string("BiddingAndAuctionResponse(") +
         "is_chaff: " + (response.is_chaff ? "true" : "false") + ", " +
         "ad_render_url: " + response.ad_render_url.spec() + ", " +
         "ad_components: " + testing::PrintToString(response.ad_components) +
         ", " + "interest_group_name: " + response.interest_group_name + ", " +
         "interest_group_owner: " + response.interest_group_owner.Serialize() +
         ", " +
         "bidding_groups: " + testing::PrintToString(response.bidding_groups) +
         ", " + "score:" + testing::PrintToString(response.score) + ", " +
         "bid:" + testing::PrintToString(response.bid) + ", " +
         "error:" + testing::PrintToString(response.error) + ", " +
         "buyer_reporting: " +
         (response.buyer_reporting.has_value()
              ? ToString(*response.buyer_reporting)
              : "nullopt") +
         ", " + "seller_reporting: " +
         (response.seller_reporting.has_value()
              ? ToString(*response.seller_reporting)
              : "nullopt") +
         ")";
}

MATCHER_P(EqualsReportingURLS, other, "EqualsReportingURLS") {
  std::vector<std::pair<std::string, GURL>> beacon_urls(
      other.get().beacon_urls.begin(), other.get().beacon_urls.end());
  return testing::ExplainMatchResult(
      testing::AllOf(
          testing::Field(
              "reporting_url",
              &BiddingAndAuctionResponse::ReportingURLs::reporting_url,
              testing::Eq(other.get().reporting_url)),
          testing::Field("beacon_urls",
                         &BiddingAndAuctionResponse::ReportingURLs::beacon_urls,
                         testing::ElementsAreArray(beacon_urls))),
      std::move(arg), result_listener);
}

MATCHER_P(EqualsBiddingAndAuctionResponse,
          other,
          "EqualsBiddingAndAuctionResponse(" + ToString(other.get()) + ")") {
  std::vector<testing::Matcher<BiddingAndAuctionResponse>> matchers = {
      testing::Field("is_chaff", &BiddingAndAuctionResponse::is_chaff,
                     testing::Eq(other.get().is_chaff)),
      testing::Field("ad_render_url", &BiddingAndAuctionResponse::ad_render_url,
                     testing::Eq(other.get().ad_render_url)),
      testing::Field("ad_components", &BiddingAndAuctionResponse::ad_components,
                     testing::ElementsAreArray(other.get().ad_components)),
      testing::Field("interest_group_name",
                     &BiddingAndAuctionResponse::interest_group_name,
                     testing::Eq(other.get().interest_group_name)),
      testing::Field("bidding_groups",
                     &BiddingAndAuctionResponse::bidding_groups,
                     testing::ElementsAreArray(other.get().bidding_groups)),
      testing::Field("score", &BiddingAndAuctionResponse::score,
                     testing::Eq(other.get().score)),
      testing::Field("bid", &BiddingAndAuctionResponse::bid,
                     testing::Eq(other.get().bid)),
      testing::Field("error", &BiddingAndAuctionResponse::error,
                     testing::Eq(other.get().error)),
  };
  if (other.get().interest_group_owner.opaque()) {
    // Treat opaque as equal
    matchers.push_back(testing::Field(
        "interest_group_owner",
        &BiddingAndAuctionResponse::interest_group_owner,
        testing::Property("opaque", &url::Origin::opaque, testing::Eq(true))));

  } else {
    matchers.push_back(
        testing::Field("interest_group_owner",
                       &BiddingAndAuctionResponse::interest_group_owner,
                       testing::Eq(other.get().interest_group_owner)));
  }
  if (other.get().buyer_reporting) {
    matchers.push_back(testing::Field(
        "buyer_reporting", &BiddingAndAuctionResponse::buyer_reporting,
        testing::Optional(
            EqualsReportingURLS(std::ref(*other.get().buyer_reporting)))));
  } else {
    matchers.push_back(testing::Field(
        "buyer_reporting", &BiddingAndAuctionResponse::buyer_reporting,
        testing::Eq(absl::nullopt)));
  }
  if (other.get().seller_reporting) {
    matchers.push_back(testing::Field(
        "seller_reporting", &BiddingAndAuctionResponse::seller_reporting,
        testing::Optional(
            EqualsReportingURLS(std::ref(*other.get().seller_reporting)))));
  } else {
    matchers.push_back(testing::Field(
        "seller_reporting", &BiddingAndAuctionResponse::seller_reporting,
        testing::Eq(absl::nullopt)));
  }
  return testing::ExplainMatchResult(testing::AllOfArray(matchers),
                                     std::move(arg), result_listener);
}

TEST(BiddingAndAuctionResponseTest, ParseFails) {
  static const base::Value kTestCases[] = {
      base::Value(1),                                          // Not a dict
      base::Value(base::Value::Dict()),                        // empty
      base::Value(base::Value::Dict().Set("isChaff", 1)),      // wrong type
      base::Value(base::Value::Dict().Set("isChaff", false)),  // missing fields
      base::Value(
          CreateValidResponseDict().Set("adRenderURL", "not a valid URL")),
      base::Value(CreateValidResponseDict().Set("components", "not a list")),
      base::Value(CreateValidResponseDict().Set(
          "components",
          base::Value(base::Value::List().Append("not a valid URL")))),
      base::Value(CreateValidResponseDict().Set("interestGroupOwner",
                                                "not a valid origin")),
      base::Value(CreateValidResponseDict().Set("biddingGroups", "not a dict")),
      base::Value(CreateValidResponseDict().Set(
          "biddingGroups",
          base::Value(base::Value::Dict().Set(
              "not an owner", base::Value(base::Value::List().Append(0)))))),
      base::Value(CreateValidResponseDict().Set(
          "biddingGroups", base::Value(base::Value::Dict().Set(
                               kOwnerOrigin,
                               base::Value(base::Value::List().Append(
                                   1000)))))),  // out of bounds
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.DebugString());
    absl::optional<BiddingAndAuctionResponse> result =
        BiddingAndAuctionResponse::TryParse(test_case.Clone(), GroupNames());
    EXPECT_FALSE(result);
  }
}

TEST(BiddingAndAuctionResponseTest, ParseSucceeds) {
  static const struct {
    base::Value input;
    BiddingAndAuctionResponse output;
  } kTestCases[] = {
      {
          base::Value(base::Value::Dict().Set("isChaff", true)),
          []() {
            BiddingAndAuctionResponse response;
            response.is_chaff = true;
            return response;
          }(),
      },
      {
          base::Value(base::Value::Dict()
                          .Set("isChaff", true)
                          .Set("error", base::Value(base::Value::Dict().Set(
                                            "message", "error message")))),
          []() {
            BiddingAndAuctionResponse response;
            response.is_chaff = true;
            response.error = "error message";
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict()),
          CreateExpectedValidResponse(),
      },
      {
          base::Value(CreateValidResponseDict().Set("error", "not a dict")),
          CreateExpectedValidResponse(),  // ignore the error
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "error", base::Value(base::Value::Dict().Set("message", 1)))),
          CreateExpectedValidResponse(),  // ignore the error
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "error", base::Value(base::Value::Dict().Set("message",
                                                           "error message")))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.error = "error message";
            return response;
          }(),
      },
      {
          base::Value(
              CreateValidResponseDict().Set("winReportingURLs", "not a dict")),
          CreateExpectedValidResponse(),  // ignore the error
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs", base::Value(base::Value::Dict().Set(
                                      "buyerReportingURLs", "not a dict")))),
          CreateExpectedValidResponse(),  // ignore the error
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs",
              base::Value(base::Value::Dict().Set(
                  "buyerReportingURLs", base::Value(base::Value::Dict().Set(
                                            "reportingURL", "not a URL")))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.buyer_reporting.emplace();
            // ignore the error.
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs",
              base::Value(base::Value::Dict().Set(
                  "buyerReportingURLs", base::Value(base::Value::Dict().Set(
                                            "reportingURL", kUntrustedURL)))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.buyer_reporting.emplace();
            // ignore the error.
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs",
              base::Value(base::Value::Dict().Set(
                  "buyerReportingURLs", base::Value(base::Value::Dict().Set(
                                            "reportingURL", kReportingURL)))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.buyer_reporting.emplace();
            response.buyer_reporting->reporting_url = GURL(kReportingURL);
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs",
              base::Value(base::Value::Dict().Set(
                  "buyerReportingURLs",
                  base::Value(base::Value::Dict().Set(
                      "interactionReportingURLs", "not a dict")))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.buyer_reporting.emplace();
            // ignore the error.
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs",
              base::Value(base::Value::Dict().Set(
                  "buyerReportingURLs",
                  base::Value(base::Value::Dict().Set(
                      "interactionReportingURLs",
                      base::Value(base::Value::Dict().Set("click", 5)))))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.buyer_reporting.emplace();
            // ignore the error.
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs",
              base::Value(base::Value::Dict().Set(
                  "buyerReportingURLs", base::Value(base::Value::Dict().Set(
                                            "interactionReportingURLs",
                                            base::Value(base::Value::Dict().Set(
                                                "click", kUntrustedURL)))))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.buyer_reporting.emplace();
            // ignore the error.
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs",
              base::Value(base::Value::Dict().Set(
                  "buyerReportingURLs", base::Value(base::Value::Dict().Set(
                                            "interactionReportingURLs",
                                            base::Value(base::Value::Dict().Set(
                                                "click", kReportingURL)))))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.buyer_reporting.emplace();
            response.buyer_reporting->beacon_urls.emplace("click",
                                                          GURL(kReportingURL));
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs",
              base::Value(base::Value::Dict().Set("topLevelSellerReportingURLs",
                                                  "not a dict")))),
          CreateExpectedValidResponse(),  // ignore the error
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs", base::Value(base::Value::Dict().Set(
                                      "topLevelSellerReportingURLs",
                                      base::Value(base::Value::Dict().Set(
                                          "reportingURL", "not a URL")))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.seller_reporting.emplace();
            // ignore the error.
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs", base::Value(base::Value::Dict().Set(
                                      "topLevelSellerReportingURLs",
                                      base::Value(base::Value::Dict().Set(
                                          "reportingURL", kUntrustedURL)))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.seller_reporting.emplace();
            // ignore the error.
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs", base::Value(base::Value::Dict().Set(
                                      "topLevelSellerReportingURLs",
                                      base::Value(base::Value::Dict().Set(
                                          "reportingURL", kReportingURL)))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.seller_reporting.emplace();
            response.seller_reporting->reporting_url = GURL(kReportingURL);
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs",
              base::Value(base::Value::Dict().Set(
                  "topLevelSellerReportingURLs",
                  base::Value(base::Value::Dict().Set(
                      "interactionReportingURLs", "not a dict")))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.seller_reporting.emplace();
            // ignore the error.
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs",
              base::Value(base::Value::Dict().Set(
                  "topLevelSellerReportingURLs",
                  base::Value(base::Value::Dict().Set(
                      "interactionReportingURLs",
                      base::Value(base::Value::Dict().Set("click", 5)))))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.seller_reporting.emplace();
            // ignore the error.
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs", base::Value(base::Value::Dict().Set(
                                      "topLevelSellerReportingURLs",
                                      base::Value(base::Value::Dict().Set(
                                          "interactionReportingURLs",
                                          base::Value(base::Value::Dict().Set(
                                              "click", kUntrustedURL)))))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.seller_reporting.emplace();
            // ignore the error.
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs", base::Value(base::Value::Dict().Set(
                                      "topLevelSellerReportingURLs",
                                      base::Value(base::Value::Dict().Set(
                                          "interactionReportingURLs",
                                          base::Value(base::Value::Dict().Set(
                                              "click", kReportingURL)))))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.seller_reporting.emplace();
            response.seller_reporting->beacon_urls.emplace("click",
                                                           GURL(kReportingURL));
            return response;
          }(),
      },
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.input.DebugString());
    absl::optional<BiddingAndAuctionResponse> result =
        BiddingAndAuctionResponse::TryParse(test_case.input.Clone(),
                                            GroupNames());
    ASSERT_TRUE(result);
    EXPECT_THAT(*result,
                EqualsBiddingAndAuctionResponse(std::ref(test_case.output)));
  }
}

TEST(BiddingAndAuctionResponseTest, RemovingFramingSucceeds) {
  struct {
    std::vector<uint8_t> input;
    std::vector<uint8_t> expected_output;
  } kTestCases[] = {
      // Small one to test basic functionality
      {
          {0x02, 0x00, 0x00, 0x00, 0x01, 0xFE, 0x02},
          {0xFE},
      },
      // Bigger one to check that we have the size right.
      {
          []() {
            std::vector<uint8_t> unframed_input(1000, ' ');
            std::vector<uint8_t> framing = {0x02, 0x00, 0x00, 0x02, 0xFF};
            std::copy(framing.begin(), framing.end(),
                      std::inserter(unframed_input, unframed_input.begin()));
            return unframed_input;
          }(),
          std::vector<uint8_t>(0x2FF, ' '),
      },
  };

  for (const auto& test_case : kTestCases) {
    absl::optional<base::span<const uint8_t>> result =
        ExtractCompressedBiddingAndAuctionResponse(test_case.input);
    ASSERT_TRUE(result);
    EXPECT_THAT(*result, testing::ElementsAreArray(test_case.expected_output));
  }
}

}  // namespace
}  // namespace content
