// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/bidding_and_auction_response.h"

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "content/browser/aggregation_service/aggregation_service_features.h"
#include "content/browser/interest_group/interest_group_features.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

const char kOwnerOrigin[] = "https://owner.example.com";
const char kUntrustedURL[] = "http://untrusted.example.com/foo";
const char kReportingURL[] = "https://reporting.example.com/report";
const char kAggregationCoordinator[] = "https://coordinator.example.com";
const char kAggregationCoordinator2[] = "https://coordinator2.example.com";
const char kDebugReportingURL[] = "https://fdo.com/report";

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

const base::flat_map<blink::InterestGroupKey, url::Origin>
GroupAggregationCoordinators() {
  return base::MakeFlatMap<blink::InterestGroupKey, url::Origin>(
      std::vector<std::pair<blink::InterestGroupKey, url::Origin>>{
          {
              blink::InterestGroupKey{url::Origin::Create(GURL(kOwnerOrigin)),
                                      "name"},
              url::Origin::Create(GURL(kAggregationCoordinator)),
          },
          {
              blink::InterestGroupKey{url::Origin::Create(GURL(kOwnerOrigin)),
                                      "name2"},
              url::Origin::Create(GURL(kAggregationCoordinator2)),
          }});
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

base::Value::List CreateBasicContributions() {
  std::vector<uint8_t> bucket_byte_string = base::Value::BlobStorage(
      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x01});
  return base::Value::List().Append(
      base::Value::Dict()
          .Set("bucket", base::Value(bucket_byte_string))
          .Set("value", 123));
}

base::Value::List CreateBasicEventContributions(
    const std::string& event = "reserved.win") {
  return base::Value::List().Append(
      base::Value::Dict()
          .Set("event", event)
          .Set("contributions", CreateBasicContributions()));
}

base::Value::Dict CreateResponseDictWithPAggResponse(
    base::Value::List contributions,
    const std::optional<std::string>& event,
    bool component_win) {
  base::Value::Dict event_contribution;
  if (event.has_value()) {
    event_contribution.Set("event", *event);
  }
  event_contribution.Set("contributions", std::move(contributions));

  base::Value::List event_contributions;
  event_contributions.Append(std::move(event_contribution));

  return CreateValidResponseDict().Set(
      "paggResponse",
      base::Value::List().Append(
          base::Value::Dict()
              .Set("reportingOrigin", kOwnerOrigin)
              .Set("igContributions",
                   base::Value::List().Append(
                       base::Value::Dict()
                           .Set("componentWin", component_win)
                           .Set("igIndex", 1)
                           .Set("eventContributions",
                                std::move(event_contributions))))));
}

base::Value::Dict CreateResponseDictWithDebugReports(
    std::optional<bool> maybe_component_win,
    std::optional<bool> maybe_is_seller_report,
    std::optional<bool> maybe_is_win_report) {
  base::Value::Dict report;
  report.Set("url", kDebugReportingURL);
  if (maybe_component_win.has_value()) {
    report.Set("componentWin", *maybe_component_win);
  }
  if (maybe_is_seller_report.has_value()) {
    report.Set("isSellerReport", *maybe_is_seller_report);
  }
  if (maybe_is_win_report.has_value()) {
    report.Set("isWinReport", *maybe_is_win_report);
  }

  return CreateValidResponseDict().Set(
      "debugReports",
      base::Value::List().Append(
          base::Value::Dict()
              .Set("adTechOrigin", kOwnerOrigin)
              .Set("reports", base::Value::List().Append(std::move(report)))));
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
         ", " + "top_level_seller_reporting: " +
         (response.top_level_seller_reporting.has_value()
              ? ToString(*response.top_level_seller_reporting)
              : "nullopt") +
         ", " + "component_seller_reporting: " +
         (response.component_seller_reporting.has_value()
              ? ToString(*response.component_seller_reporting)
              : "nullopt") +
         ")";
}

auction_worklet::mojom::EventTypePtr CreateReservedEventType(
    auction_worklet::mojom::ReservedEventType reserved_event_type) {
  return auction_worklet::mojom::EventType::NewReserved(reserved_event_type);
}

auction_worklet::mojom::EventTypePtr CreateNonReservedEventType(
    const std::string& event_type) {
  return auction_worklet::mojom::EventType::NewNonReserved(event_type);
}

auction_worklet::mojom::PrivateAggregationRequestPtr CreatePaggForEventRequest(
    absl::uint128 bucket,
    int value,
    std::optional<uint64_t> filtering_id,
    auction_worklet::mojom::EventTypePtr event) {
  return auction_worklet::mojom::PrivateAggregationRequest::New(
      auction_worklet::mojom::AggregatableReportContribution::
          NewForEventContribution(
              auction_worklet::mojom::AggregatableReportForEventContribution::
                  New(auction_worklet::mojom::ForEventSignalBucket::NewIdBucket(
                          bucket),
                      auction_worklet::mojom::ForEventSignalValue::NewIntValue(
                          value),
                      filtering_id, std::move(event))),
      // TODO(qingxinwu): consider allowing this to be set
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());
}

auction_worklet::mojom::PrivateAggregationRequestPtr CreatePaggHistogramRequest(
    absl::uint128 bucket,
    int value,
    std::optional<uint64_t> filtering_id) {
  return auction_worklet::mojom::PrivateAggregationRequest::New(
      auction_worklet::mojom::AggregatableReportContribution::
          NewHistogramContribution(
              blink::mojom::AggregatableReportHistogramContribution::New(
                  /*bucket=*/bucket,
                  /*value=*/value,
                  /*filtering_id=*/filtering_id)),
      // TODO(qingxinwu): consider allowing this to be set
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());
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

// Helper to avoid excess boilerplate.
template <typename... Ts>
auto ElementsAreRequests(Ts&... requests) {
  static_assert(
      std::conjunction<std::is_same<
          std::remove_const_t<Ts>,
          auction_worklet::mojom::PrivateAggregationRequestPtr>...>::value);
  // Need to use `std::ref` as `mojo::StructPtr`s are move-only.
  return testing::UnorderedElementsAre(testing::Eq(std::ref(requests))...);
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
      testing::Field("ad_metadata", &BiddingAndAuctionResponse::ad_metadata,
                     testing::Eq(other.get().ad_metadata)),
      testing::Field("buyer_reporting_id",
                     &BiddingAndAuctionResponse::buyer_reporting_id,
                     testing::Eq(other.get().buyer_reporting_id)),
      testing::Field("buyer_and_seller_reporting_id",
                     &BiddingAndAuctionResponse::buyer_and_seller_reporting_id,
                     testing::Eq(other.get().buyer_and_seller_reporting_id)),
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
        testing::Eq(std::nullopt)));
  }
  if (other.get().top_level_seller_reporting) {
    matchers.push_back(testing::Field(
        "top_level_seller_reporting",
        &BiddingAndAuctionResponse::top_level_seller_reporting,
        testing::Optional(EqualsReportingURLS(
            std::ref(*other.get().top_level_seller_reporting)))));
  } else {
    matchers.push_back(
        testing::Field("top_level_seller_reporting",
                       &BiddingAndAuctionResponse::top_level_seller_reporting,
                       testing::Eq(std::nullopt)));
  }
  if (other.get().component_seller_reporting) {
    matchers.push_back(testing::Field(
        "component_seller_reporting",
        &BiddingAndAuctionResponse::component_seller_reporting,
        testing::Optional(EqualsReportingURLS(
            std::ref(*other.get().component_seller_reporting)))));
  } else {
    matchers.push_back(
        testing::Field("component_seller_reporting",
                       &BiddingAndAuctionResponse::component_seller_reporting,
                       testing::Eq(std::nullopt)));
  }
  if (other.get().top_level_seller) {
    matchers.push_back(testing::Field(
        "top_level_seller", &BiddingAndAuctionResponse::top_level_seller,
        testing::Optional(testing::Eq(other.get().top_level_seller))));
  } else {
    matchers.push_back(testing::Field(
        "top_level_seller", &BiddingAndAuctionResponse::top_level_seller,
        testing::Eq(std::nullopt)));
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
      base::Value(CreateValidResponseDict().Set("topLevelSeller",
                                                "not a valid Origin")),
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.DebugString());
    std::optional<BiddingAndAuctionResponse> result =
        BiddingAndAuctionResponse::TryParse(test_case.Clone(), GroupNames(),
                                            GroupAggregationCoordinators());
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
          base::Value(base::Value::Dict().Set(
              "error", base::Value(base::Value::Dict().Set("message",
                                                           "error message")))),
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
          CreateExpectedValidResponse(),
      },
      {base::Value(CreateValidResponseDict().Set(
           "error", base::Value(base::Value::Dict().Set("message", 1)))),
       []() {
         BiddingAndAuctionResponse response;
         response.is_chaff = true;
         response.error = "Unknown server error";
         return response;
       }()},
      {
          base::Value(CreateValidResponseDict().Set(
              "error", base::Value(base::Value::Dict().Set("message",
                                                           "error message")))),
          []() {
            BiddingAndAuctionResponse response;
            response.is_chaff = true;
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
            response.top_level_seller_reporting.emplace();
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
            response.top_level_seller_reporting.emplace();
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
            response.top_level_seller_reporting.emplace();
            response.top_level_seller_reporting->reporting_url =
                GURL(kReportingURL);
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
            response.top_level_seller_reporting.emplace();
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
            response.top_level_seller_reporting.emplace();
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
            response.top_level_seller_reporting.emplace();
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
            response.top_level_seller_reporting.emplace();
            response.top_level_seller_reporting->beacon_urls.emplace(
                "click", GURL(kReportingURL));
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs",
              base::Value(base::Value::Dict().Set(
                  "componentSellerReportingURLs", "not a dict")))),
          CreateExpectedValidResponse(),  // ignore the error
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs", base::Value(base::Value::Dict().Set(
                                      "componentSellerReportingURLs",
                                      base::Value(base::Value::Dict().Set(
                                          "reportingURL", "not a URL")))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.component_seller_reporting.emplace();
            // ignore the error.
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs", base::Value(base::Value::Dict().Set(
                                      "componentSellerReportingURLs",
                                      base::Value(base::Value::Dict().Set(
                                          "reportingURL", kUntrustedURL)))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.component_seller_reporting.emplace();
            // ignore the error.
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs", base::Value(base::Value::Dict().Set(
                                      "componentSellerReportingURLs",
                                      base::Value(base::Value::Dict().Set(
                                          "reportingURL", kReportingURL)))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.component_seller_reporting.emplace();
            response.component_seller_reporting->reporting_url =
                GURL(kReportingURL);
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs",
              base::Value(base::Value::Dict().Set(
                  "componentSellerReportingURLs",
                  base::Value(base::Value::Dict().Set(
                      "interactionReportingURLs", "not a dict")))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.component_seller_reporting.emplace();
            // ignore the error.
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs",
              base::Value(base::Value::Dict().Set(
                  "componentSellerReportingURLs",
                  base::Value(base::Value::Dict().Set(
                      "interactionReportingURLs",
                      base::Value(base::Value::Dict().Set("click", 5)))))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.component_seller_reporting.emplace();
            // ignore the error.
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs", base::Value(base::Value::Dict().Set(
                                      "componentSellerReportingURLs",
                                      base::Value(base::Value::Dict().Set(
                                          "interactionReportingURLs",
                                          base::Value(base::Value::Dict().Set(
                                              "click", kUntrustedURL)))))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.component_seller_reporting.emplace();
            // ignore the error.
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "winReportingURLs", base::Value(base::Value::Dict().Set(
                                      "componentSellerReportingURLs",
                                      base::Value(base::Value::Dict().Set(
                                          "interactionReportingURLs",
                                          base::Value(base::Value::Dict().Set(
                                              "click", kReportingURL)))))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.component_seller_reporting.emplace();
            response.component_seller_reporting->beacon_urls.emplace(
                "click", GURL(kReportingURL));
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set("topLevelSeller",
                                                    "https://seller.test")),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.top_level_seller =
                url::Origin::Create(GURL("https://seller.test"));
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set("adMetadata", "data")),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.ad_metadata = "data";
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set("buyerReportingId", "foo")),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.buyer_reporting_id = "foo";
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set("buyerAndSellerReportingId",
                                                    "bar")),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.buyer_and_seller_reporting_id = "bar";
            return response;
          }(),
      },
      {
          base::Value(CreateValidResponseDict().Set("updateGroups", "invalid")),
          CreateExpectedValidResponse(),  // ignore error
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "updateGroups",
              base::Value(base::Value::Dict().Set(
                  "invalid", base::Value(base::Value::List()))))),
          CreateExpectedValidResponse(),  // ignore error
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "updateGroups",
              base::Value(base::Value::Dict().Set(
                  kOwnerOrigin, base::Value(base::Value::List()))))),
          CreateExpectedValidResponse(),
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "updateGroups",
              base::Value(base::Value::Dict().Set(
                  kOwnerOrigin,
                  base::Value(base::Value::List().Append(
                      base::Value(base::Value::Dict().Set("index", 0)))))))),
          CreateExpectedValidResponse(),  // ignore error
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "updateGroups",
              base::Value(base::Value::Dict().Set(
                  kOwnerOrigin,
                  base::Value(base::Value::List().Append(base::Value(
                      base::Value::Dict().Set("updateIfOlderThanMs", 0)))))))),
          CreateExpectedValidResponse(),  // ignore error
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "updateGroups",
              base::Value(base::Value::Dict().Set(
                  kOwnerOrigin,
                  base::Value(base::Value::List().Append(
                      base::Value(base::Value::Dict()
                                      .Set("index", "invalid")
                                      .Set("updateIfOlderThanMs", 0)))))))),
          CreateExpectedValidResponse(),  // ignore error
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "updateGroups",
              base::Value(base::Value::Dict().Set(
                  kOwnerOrigin,
                  base::Value(base::Value::List().Append(base::Value(
                      base::Value::Dict()
                          .Set("index", 0)
                          .Set("updateIfOlderThanMs", "invalid")))))))),
          CreateExpectedValidResponse(),  // ignore error
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "updateGroups",
              base::Value(base::Value::Dict().Set(
                  kOwnerOrigin,
                  base::Value(base::Value::List().Append(
                      base::Value(base::Value::Dict()
                                      .Set("index", -1)
                                      .Set("updateIfOlderThanMs", 0)))))))),
          CreateExpectedValidResponse(),  // ignore error
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "updateGroups",
              base::Value(base::Value::Dict().Set(
                  kOwnerOrigin,
                  base::Value(base::Value::List().Append(
                      base::Value(base::Value::Dict()
                                      .Set("index", 10)
                                      .Set("updateIfOlderThanMs", 0)))))))),
          CreateExpectedValidResponse(),  // ignore error
      },
      {
          base::Value(CreateValidResponseDict().Set(
              "updateGroups",
              base::Value(base::Value::Dict().Set(
                  kOwnerOrigin,
                  base::Value(base::Value::List().Append(
                      base::Value(base::Value::Dict()
                                      .Set("index", 0)
                                      .Set("updateIfOlderThanMs", 0)))))))),
          []() {
            BiddingAndAuctionResponse response = CreateExpectedValidResponse();
            response.triggered_updates[blink::InterestGroupKey(
                url::Origin::Create(GURL(kOwnerOrigin)), "name")] =
                base::Milliseconds(0);
            return response;
          }(),
      },
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.input.DebugString());
    std::optional<BiddingAndAuctionResponse> result =
        BiddingAndAuctionResponse::TryParse(test_case.input.Clone(),
                                            GroupNames(),
                                            GroupAggregationCoordinators());
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
    std::optional<base::span<const uint8_t>> result =
        ExtractCompressedBiddingAndAuctionResponse(test_case.input);
    ASSERT_TRUE(result);
    EXPECT_THAT(*result, testing::ElementsAreArray(test_case.expected_output));
  }
}

TEST(BiddingAndAuctionResponseTest, PrivateAggregationDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      blink::features::kPrivateAggregationApi);

  base::Value::Dict response = CreateResponseDictWithPAggResponse(
      CreateBasicContributions(), "reserved.win",
      /*component_win=*/true);

  std::optional<BiddingAndAuctionResponse> result =
      BiddingAndAuctionResponse::TryParse(base::Value(response.Clone()),
                                          GroupNames(),
                                          GroupAggregationCoordinators());
  ASSERT_TRUE(result);
  BiddingAndAuctionResponse output = CreateExpectedValidResponse();
  EXPECT_THAT(*result, EqualsBiddingAndAuctionResponse(std::ref(output)));
  EXPECT_TRUE(result->component_win_pagg_requests.empty());
  EXPECT_TRUE(result->server_filtered_pagg_requests_reserved.empty());
  EXPECT_TRUE(result->server_filtered_pagg_requests_non_reserved.empty());
}

TEST(BiddingAndAuctionResponseTest, BAndAPrivateAggregationDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{blink::features::kPrivateAggregationApi,
        {{"enabled_in_fledge", "true"}}},
       {blink::features::kPrivateAggregationApiFilteringIds, {}},
       {kPrivacySandboxAggregationServiceFilteringIds, {}}},
      /*disabled_features=*/{features::kEnableBandAPrivateAggregation});

  base::Value::Dict response = CreateResponseDictWithPAggResponse(
      CreateBasicContributions(), "reserved.win",
      /*component_win=*/true);

  std::optional<BiddingAndAuctionResponse> result =
      BiddingAndAuctionResponse::TryParse(base::Value(response.Clone()),
                                          GroupNames(),
                                          GroupAggregationCoordinators());
  ASSERT_TRUE(result);
  BiddingAndAuctionResponse output = CreateExpectedValidResponse();
  EXPECT_THAT(*result, EqualsBiddingAndAuctionResponse(std::ref(output)));
  EXPECT_TRUE(result->component_win_pagg_requests.empty());
  EXPECT_TRUE(result->server_filtered_pagg_requests_reserved.empty());
  EXPECT_TRUE(result->server_filtered_pagg_requests_non_reserved.empty());
}

TEST(BiddingAndAuctionResponseTest, BAndASampleDebugReportsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kEnableBandASampleDebugReports);

  base::Value::Dict response = CreateResponseDictWithDebugReports(
      /*maybe_component_win=*/false, /*maybe_is_seller_report=*/std::nullopt,
      /*maybe_is_win_report=*/false);

  std::optional<BiddingAndAuctionResponse> result =
      BiddingAndAuctionResponse::TryParse(base::Value(response.Clone()),
                                          GroupNames(),
                                          GroupAggregationCoordinators());
  ASSERT_TRUE(result);
  BiddingAndAuctionResponse output = CreateExpectedValidResponse();
  EXPECT_THAT(*result, EqualsBiddingAndAuctionResponse(std::ref(output)));
  EXPECT_TRUE(result->component_win_debugging_only_reports.empty());
  EXPECT_TRUE(result->server_filtered_debugging_only_reports.empty());
}

class BiddingAndAuctionPAggResponseTest : public testing::Test {
 public:
  BiddingAndAuctionPAggResponseTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{blink::features::kPrivateAggregationApi,
          {{"enabled_in_fledge", "true"}}},
         {blink::features::kPrivateAggregationApiFilteringIds, {}},
         {kPrivacySandboxAggregationServiceFilteringIds, {}},
         {features::kEnableBandAPrivateAggregation, {}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(BiddingAndAuctionPAggResponseTest, ParsePAggResponse) {
  base::Value::List ig_contributions;
  ig_contributions.Append(
      base::Value::Dict()
          .Set("componentWin", false)
          .Set("igIndex", 0)
          .Set("eventContributions",
               CreateBasicEventContributions("reserved.loss")));
  ig_contributions.Append(
      base::Value::Dict()
          .Set("componentWin", true)
          .Set("igIndex", 1)
          .Set("eventContributions", CreateBasicEventContributions("click")));
  ig_contributions.Append(
      base::Value::Dict()
          .Set("componentWin", true)
          .Set("coordinator", "https://seller.coordinator.com")
          .Set("eventContributions",
               CreateBasicEventContributions("reserved.win")));

  base::Value::Dict response = CreateValidResponseDict().Set(
      "paggResponse",
      base::Value::List().Append(
          base::Value::Dict()
              .Set("reportingOrigin", kOwnerOrigin)
              .Set("igContributions", std::move(ig_contributions))));

  std::optional<BiddingAndAuctionResponse> result =
      BiddingAndAuctionResponse::TryParse(base::Value(response.Clone()),
                                          GroupNames(),
                                          GroupAggregationCoordinators());
  ASSERT_TRUE(result);
  BiddingAndAuctionResponse output = CreateExpectedValidResponse();
  EXPECT_THAT(*result, EqualsBiddingAndAuctionResponse(std::ref(output)));

  EXPECT_EQ(2u, result->component_win_pagg_requests.size());
  PrivateAggregationPhaseKey phase_key1 = {
      url::Origin::Create(GURL(kOwnerOrigin)),
      PrivateAggregationPhase::kNonTopLevelSeller,
      url::Origin::Create(GURL(kAggregationCoordinator2))};
  auction_worklet::mojom::PrivateAggregationRequestPtr request1 =
      CreatePaggForEventRequest(1, 123, std::nullopt,
                                CreateNonReservedEventType("click"));
  EXPECT_THAT(result->component_win_pagg_requests[std::move(phase_key1)],
              ElementsAreRequests(request1));

  PrivateAggregationPhaseKey phase_key2 = {
      url::Origin::Create(GURL(kOwnerOrigin)),
      PrivateAggregationPhase::kNonTopLevelSeller,
      url::Origin::Create(GURL("https://seller.coordinator.com"))};
  auction_worklet::mojom::PrivateAggregationRequestPtr request2 =
      CreatePaggForEventRequest(
          1, 123, std::nullopt,
          CreateReservedEventType(
              auction_worklet::mojom::ReservedEventType::kReservedWin));
  EXPECT_THAT(result->component_win_pagg_requests[std::move(phase_key2)],
              ElementsAreRequests(request2));

  EXPECT_EQ(1u, result->server_filtered_pagg_requests_reserved.size());
  PrivateAggregationKey key = {
      url::Origin::Create(GURL(kOwnerOrigin)),
      url::Origin::Create(GURL(kAggregationCoordinator))};
  auction_worklet::mojom::PrivateAggregationRequestPtr histogram_request =
      CreatePaggHistogramRequest(1, 123, std::nullopt);
  EXPECT_THAT(result->server_filtered_pagg_requests_reserved[std::move(key)],
              ElementsAreRequests(histogram_request));

  EXPECT_TRUE(result->server_filtered_pagg_requests_non_reserved.empty());
}

TEST_F(BiddingAndAuctionPAggResponseTest, ParsePAggResponseIgnoreErrors) {
  BiddingAndAuctionResponse output = CreateExpectedValidResponse();
  static const struct {
    std::string description;
    base::Value response;
  } kTestCases[] = {
      {
          "paggResponse is not a list",
          base::Value(
              CreateValidResponseDict().Set("paggResponse", "not a list")),
      },
      {"missing required reporting origin",
       base::Value(CreateValidResponseDict().Set(
           "paggResponse",
           base::Value::List().Append(base::Value::Dict().Set(
               "igContributions",
               base::Value::List().Append(base::Value::Dict().Set(
                   "eventContributions", CreateBasicEventContributions()))))))},
      {
          "negative igIndex",
          base::Value(CreateValidResponseDict().Set(
              "paggResponse",
              base::Value::List().Append(
                  base::Value::Dict()
                      .Set("reportingOrigin", kOwnerOrigin)
                      .Set("igContributions",
                           base::Value::List().Append(
                               base::Value::Dict()
                                   .Set("igIndex", -1)
                                   .Set("eventContributions",
                                        CreateBasicEventContributions())))))),
      },
      {
          "too big igIndex",
          base::Value(CreateValidResponseDict().Set(
              "paggResponse",
              base::Value::List().Append(
                  base::Value::Dict()
                      .Set("reportingOrigin", kOwnerOrigin)
                      .Set("igContributions",
                           base::Value::List().Append(
                               base::Value::Dict()
                                   .Set("igIndex", 100000)
                                   .Set("eventContributions",
                                        CreateBasicEventContributions())))))),
      },
      {
          "HTTP coordinator",
          base::Value(CreateValidResponseDict().Set(
              "paggResponse",
              base::Value::List().Append(
                  base::Value::Dict()
                      .Set("reportingOrigin", kOwnerOrigin)
                      .Set("igContributions",
                           base::Value::List().Append(
                               base::Value::Dict()
                                   .Set("coordinator", "http://a.com")
                                   .Set("eventContributions",
                                        CreateBasicEventContributions())))))),
      },
      {
          "unknown reserved event",
          base::Value(CreateResponseDictWithPAggResponse(
              CreateBasicContributions(), "reserved.unknown",
              /*component_win=*/true)),
      },
      {
          "missing required event field",
          base::Value(CreateResponseDictWithPAggResponse(
              CreateBasicContributions(), /*event=*/std::nullopt,
              /*component_win=*/true)),
      },
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    std::optional<BiddingAndAuctionResponse> result =
        BiddingAndAuctionResponse::TryParse(test_case.response.Clone(),
                                            GroupNames(),
                                            GroupAggregationCoordinators());
    ASSERT_TRUE(result);
    EXPECT_THAT(*result, EqualsBiddingAndAuctionResponse(std::ref(output)));

    EXPECT_TRUE(result->component_win_pagg_requests.empty());
    EXPECT_TRUE(result->server_filtered_pagg_requests_reserved.empty());
    EXPECT_TRUE(result->server_filtered_pagg_requests_non_reserved.empty());
  }
}

TEST_F(BiddingAndAuctionPAggResponseTest, ParsePAggResponseContribution) {
  const std::vector<uint8_t> bucket_byte_string = base::Value::BlobStorage(
      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x02});
  BiddingAndAuctionResponse output = CreateExpectedValidResponse();
  static const struct {
    std::string description;
    std::optional<base::Value::BlobStorage> bucket;
    std::optional<int> value;
    std::optional<int> filtering_id;
    auction_worklet::mojom::PrivateAggregationRequestPtr pagg_request;
  } kTestCases[] = {
      {
          "bucket is big-endian",
          bucket_byte_string,
          123,
          123,
          CreatePaggForEventRequest(
              absl::MakeUint128(1, 2), 123, 123,
              CreateReservedEventType(
                  auction_worklet::mojom::ReservedEventType::kReservedWin)),
      },
      {
          "bucket is bigger than 128 bits",
          base::Value::BlobStorage({0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
                                    0x0F, 0x10, 0x11}),
          123,
          123,
          /*pagg_request=*/nullptr,
      },
      {
          "missing required bucket",
          std::nullopt,
          123,
          123,
          /*pagg_request=*/nullptr,
      },
      {
          "missing required value",
          base::Value::BlobStorage({0x01}),
          std::nullopt,
          123,
          /*pagg_request=*/nullptr,
      },
      {
          "missing optional filtering id",
          bucket_byte_string,
          123,
          std::nullopt,
          CreatePaggForEventRequest(
              absl::MakeUint128(1, 2), 123, std::nullopt,
              CreateReservedEventType(
                  auction_worklet::mojom::ReservedEventType::kReservedWin)),
      },
      {
          "Invalid filtering_id",
          base::Value::BlobStorage({0x01}),
          123,
          1000,
          /*pagg_request=*/nullptr,
      },
  };

  PrivateAggregationPhaseKey key = {
      url::Origin::Create(GURL(kOwnerOrigin)),
      PrivateAggregationPhase::kNonTopLevelSeller,
      url::Origin::Create(GURL(kAggregationCoordinator2))};
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    base::Value::Dict contribution;
    if (test_case.bucket.has_value()) {
      contribution.Set("bucket", base::Value(std::move(*test_case.bucket)));
    }
    if (test_case.value.has_value()) {
      contribution.Set("value", *test_case.value);
    }
    if (test_case.filtering_id.has_value()) {
      contribution.Set("filteringId", *test_case.filtering_id);
    }
    base::Value::List contributions;
    contributions.Append(std::move(contribution));
    base::Value::Dict response = CreateResponseDictWithPAggResponse(
        std::move(contributions), "reserved.win", /*component_win=*/true);

    std::optional<BiddingAndAuctionResponse> result =
        BiddingAndAuctionResponse::TryParse(base::Value(response.Clone()),
                                            GroupNames(),
                                            GroupAggregationCoordinators());
    ASSERT_TRUE(result);
    EXPECT_THAT(*result, EqualsBiddingAndAuctionResponse(std::ref(output)));

    if (test_case.pagg_request) {
      EXPECT_EQ(1u, result->component_win_pagg_requests.size());
      EXPECT_THAT(result->component_win_pagg_requests[key],
                  ElementsAreRequests(test_case.pagg_request));
    }
    EXPECT_TRUE(result->server_filtered_pagg_requests_reserved.empty());
    EXPECT_TRUE(result->server_filtered_pagg_requests_non_reserved.empty());
  }
}

TEST_F(BiddingAndAuctionPAggResponseTest, ParsePAggResponseComponentWinEvents) {
  BiddingAndAuctionResponse output = CreateExpectedValidResponse();

  PrivateAggregationPhaseKey key = {
      url::Origin::Create(GURL(kOwnerOrigin)),
      PrivateAggregationPhase::kNonTopLevelSeller,
      url::Origin::Create(GURL(kAggregationCoordinator2))};
  static const struct {
    std::string event;
    auction_worklet::mojom::PrivateAggregationRequestPtr pagg_request;
  } kTestCases[] = {
      {
          "reserved.win",
          CreatePaggForEventRequest(
              1, 123, std::nullopt,
              CreateReservedEventType(
                  auction_worklet::mojom::ReservedEventType::kReservedWin)),
      },
      {
          "reserved.always",
          CreatePaggForEventRequest(
              1, 123, std::nullopt,
              CreateReservedEventType(
                  auction_worklet::mojom::ReservedEventType::kReservedAlways)),
      },
      {
          "reserved.loss",
          CreatePaggForEventRequest(
              1, 123, std::nullopt,
              CreateReservedEventType(
                  auction_worklet::mojom::ReservedEventType::kReservedLoss)),
      },
      {
          "click",
          CreatePaggForEventRequest(
              1, 123, std::nullopt,
              auction_worklet::mojom::EventType::NewNonReserved("click")),
      },
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.event);
    base::Value::Dict response = CreateResponseDictWithPAggResponse(
        CreateBasicContributions(), test_case.event,
        /*component_win=*/true);
    std::optional<BiddingAndAuctionResponse> result =
        BiddingAndAuctionResponse::TryParse(base::Value(response.Clone()),
                                            GroupNames(),
                                            GroupAggregationCoordinators());
    ASSERT_TRUE(result);
    EXPECT_THAT(*result, EqualsBiddingAndAuctionResponse(std::ref(output)));

    EXPECT_THAT(result->component_win_pagg_requests[key],
                ElementsAreRequests(test_case.pagg_request));
    EXPECT_TRUE(result->server_filtered_pagg_requests_reserved.empty());
    EXPECT_TRUE(result->server_filtered_pagg_requests_non_reserved.empty());
  }
}

// Similar to ParsePAggResponseComponentWinEvents(), but for server filtered
// private aggregation requests (i.e., componentWin field is false).
TEST_F(BiddingAndAuctionPAggResponseTest,
       ParsePAggResponseServerFilteredEvents) {
  BiddingAndAuctionResponse output = CreateExpectedValidResponse();
  PrivateAggregationKey key = {
      url::Origin::Create(GURL(kOwnerOrigin)),
      url::Origin::Create(GURL(kAggregationCoordinator2))};
  static const struct {
    std::string event;
    auction_worklet::mojom::PrivateAggregationRequestPtr pagg_request;
  } kTestCases[] = {
      {
          "reserved.win",
          CreatePaggHistogramRequest(1, 123, std::nullopt),
      },
      {
          "reserved.always",
          CreatePaggHistogramRequest(1, 123, std::nullopt),
      },
      {
          "reserved.loss",
          CreatePaggHistogramRequest(1, 123, std::nullopt),
      },
      {
          "click",
          CreatePaggHistogramRequest(1, 123, std::nullopt),
      },
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.event);
    base::Value::Dict response = CreateResponseDictWithPAggResponse(
        CreateBasicContributions(), test_case.event,
        /*component_win=*/false);
    std::optional<BiddingAndAuctionResponse> result =
        BiddingAndAuctionResponse::TryParse(base::Value(response.Clone()),
                                            GroupNames(),
                                            GroupAggregationCoordinators());
    ASSERT_TRUE(result);
    EXPECT_THAT(*result, EqualsBiddingAndAuctionResponse(std::ref(output)));
    EXPECT_TRUE(result->component_win_pagg_requests.empty());

    if (base::StartsWith(test_case.event, "reserved.")) {
      EXPECT_EQ(1u, result->server_filtered_pagg_requests_reserved.size());
      EXPECT_THAT(result->server_filtered_pagg_requests_reserved[key],
                  ElementsAreRequests(test_case.pagg_request));
      EXPECT_TRUE(result->server_filtered_pagg_requests_non_reserved.empty());
    } else {
      EXPECT_TRUE(result->server_filtered_pagg_requests_reserved.empty());
      EXPECT_EQ(1u, result->server_filtered_pagg_requests_non_reserved.size());
      EXPECT_THAT(
          result->server_filtered_pagg_requests_non_reserved[test_case.event],
          ElementsAreRequests(test_case.pagg_request));
    }
  }
}

class BiddingAndAuctionSampleDebugReportsTest : public testing::Test {
 public:
  BiddingAndAuctionSampleDebugReportsTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kEnableBandASampleDebugReports);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(BiddingAndAuctionSampleDebugReportsTest, ForDebuggingOnlyReports) {
  BiddingAndAuctionResponse output = CreateExpectedValidResponse();
  base::Value::List reports;
  reports.Append(base::Value::Dict()
                     .Set("isWinReport", true)
                     .Set("componentWin", true)
                     .Set("url", "https://component-win.win-debug-report.com"));
  reports.Append(
      base::Value::Dict()
          .Set("isWinReport", false)
          .Set("componentWin", true)
          .Set("url", "https://component-win.loss-debug-report.com"));
  reports.Append(base::Value::Dict().Set("url", kDebugReportingURL));

  base::Value::Dict response = CreateValidResponseDict().Set(
      "debugReports",
      base::Value::List().Append(base::Value::Dict()
                                     .Set("adTechOrigin", kOwnerOrigin)
                                     .Set("reports", std::move(reports))));
  std::optional<BiddingAndAuctionResponse> result =
      BiddingAndAuctionResponse::TryParse(base::Value(response.Clone()),
                                          GroupNames(),
                                          /*group_pagg_coordinators=*/{});
  ASSERT_TRUE(result);
  EXPECT_THAT(*result, EqualsBiddingAndAuctionResponse(std::ref(output)));

  EXPECT_EQ(2u, result->component_win_debugging_only_reports.size());
  EXPECT_THAT(result->component_win_debugging_only_reports
                  [BiddingAndAuctionResponse::DebugReportKey(false, true)],
              GURL("https://component-win.win-debug-report.com"));
  EXPECT_THAT(result->component_win_debugging_only_reports
                  [BiddingAndAuctionResponse::DebugReportKey(false, false)],
              GURL("https://component-win.loss-debug-report.com"));

  EXPECT_EQ(1u, result->server_filtered_debugging_only_reports.size());
  EXPECT_THAT(
      result->server_filtered_debugging_only_reports[url::Origin::Create(
          GURL(kOwnerOrigin))],
      testing::UnorderedElementsAre(kDebugReportingURL));
}

TEST_F(BiddingAndAuctionSampleDebugReportsTest,
       ForDebuggingOnlyReportsIgnoreErrors) {
  BiddingAndAuctionResponse output = CreateExpectedValidResponse();
  static const base::Value kTestCases[] = {
      {
          base::Value(
              CreateValidResponseDict().Set("debugReports", "not a list")),
      },
      {base::Value(CreateValidResponseDict().Set(
          "debugReports", base::Value::List().Append("not a dict")))},
      // Miss required ad tech origin.
      {base::Value(CreateValidResponseDict().Set(
          "debugReports",
          base::Value::List().Append(base::Value::Dict().Set(
              "reports", base::Value::List().Append(base::Value::Dict().Set(
                             "url", "https://fdo.com"))))))},
      // Http ad tech origin.
      {base::Value(CreateValidResponseDict().Set(
          "debugReports",
          base::Value::List().Append(
              base::Value::Dict()
                  .Set("adTechOrigin", "http://adtech.com")
                  .Set("reports",
                       base::Value::List().Append(base::Value::Dict().Set(
                           "url", "https://fdo.com"))))))},
      // Http url.
      {base::Value(CreateValidResponseDict().Set(
          "debugReports",
          base::Value::List().Append(
              base::Value::Dict()
                  .Set("adTechOrigin", "https://adtech.com")
                  .Set("reports",
                       base::Value::List().Append(base::Value::Dict().Set(
                           "url", "http://fdo.com"))))))},
      // Invalid url.
      {base::Value(CreateValidResponseDict().Set(
          "debugReports",
          base::Value::List().Append(
              base::Value::Dict()
                  .Set("adTechOrigin", "https://adtech.com")
                  .Set("reports",
                       base::Value::List().Append(
                           base::Value::Dict().Set("url", "not a url"))))))},
  };
  for (const auto& response : kTestCases) {
    SCOPED_TRACE(response.DebugString());
    std::optional<BiddingAndAuctionResponse> result =
        BiddingAndAuctionResponse::TryParse(response.Clone(), GroupNames(),
                                            /*group_pagg_coordinators=*/{});
    ASSERT_TRUE(result);
    EXPECT_THAT(*result, EqualsBiddingAndAuctionResponse(std::ref(output)));

    EXPECT_TRUE(result->component_win_debugging_only_reports.empty());
    EXPECT_TRUE(result->server_filtered_debugging_only_reports.empty());
  }
}

TEST_F(BiddingAndAuctionSampleDebugReportsTest,
       ForDebuggingOnlyReportsComponentWinner) {
  BiddingAndAuctionResponse output = CreateExpectedValidResponse();
  static const struct {
    std::optional<bool> is_seller_report;
    std::optional<bool> is_win_report;
  } kTestCases[] = {
      {true, true},         {true, false},         {true, std::nullopt},
      {false, true},        {false, false},        {false, std::nullopt},
      {std::nullopt, true}, {std::nullopt, false}, {std::nullopt, std::nullopt},
  };

  for (const auto& test_case : kTestCases) {
    base::Value::Dict response = CreateResponseDictWithDebugReports(
        /*maybe_component_win=*/true, test_case.is_seller_report,
        test_case.is_win_report);
    SCOPED_TRACE(response.DebugString());
    std::optional<BiddingAndAuctionResponse> result =
        BiddingAndAuctionResponse::TryParse(base::Value(response.Clone()),
                                            GroupNames(),
                                            /*group_pagg_coordinators=*/{});
    ASSERT_TRUE(result);
    EXPECT_THAT(*result, EqualsBiddingAndAuctionResponse(std::ref(output)));
    EXPECT_EQ(1u, result->component_win_debugging_only_reports.size());
    bool is_seller_report =
        test_case.is_seller_report.has_value() && *test_case.is_seller_report;
    bool is_win_report =
        test_case.is_win_report.has_value() && *test_case.is_win_report;
    EXPECT_THAT(result->component_win_debugging_only_reports
                    [BiddingAndAuctionResponse::DebugReportKey(is_seller_report,
                                                               is_win_report)],
                kDebugReportingURL);
    EXPECT_TRUE(result->server_filtered_debugging_only_reports.empty());
  }
}

TEST_F(BiddingAndAuctionSampleDebugReportsTest,
       ForDebuggingOnlyReportsServerFiltered) {
  BiddingAndAuctionResponse output = CreateExpectedValidResponse();
  static const std::optional<bool> kTestCases[] = {
      true,
      false,
      std::nullopt,
  };
  for (const auto& test_case : kTestCases) {
    base::Value::Dict response = CreateResponseDictWithDebugReports(
        /*maybe_component_win=*/false, /*maybe_is_seller_report=*/std::nullopt,
        /*maybe_is_win_report=*/test_case);
    SCOPED_TRACE(response.DebugString());
    std::optional<BiddingAndAuctionResponse> result =
        BiddingAndAuctionResponse::TryParse(base::Value(response.Clone()),
                                            GroupNames(),
                                            /*group_pagg_coordinators=*/{});
    ASSERT_TRUE(result);
    EXPECT_THAT(*result, EqualsBiddingAndAuctionResponse(std::ref(output)));
    EXPECT_TRUE(result->component_win_debugging_only_reports.empty());
    EXPECT_EQ(1u, result->server_filtered_debugging_only_reports.size());
    EXPECT_THAT(
        result->server_filtered_debugging_only_reports[url::Origin::Create(
            GURL(kOwnerOrigin))],
        testing::UnorderedElementsAre(kDebugReportingURL));
  }
}

}  // namespace
}  // namespace content
