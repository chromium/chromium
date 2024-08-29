// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/devtools_serialization.h"

#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/test/values_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/common/interest_group/auction_config_test_util.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/origin.h"

namespace blink {
namespace {

TEST(SerializeAuctionConfigTest, SerializeComponents) {
  // Component auction serialization just includes the origins.
  AuctionConfig config = CreateBasicAuctionConfig();
  config.non_shared_params.component_auctions.push_back(
      CreateBasicAuctionConfig(GURL("https://example.org/foo.js")));
  config.non_shared_params.component_auctions.push_back(
      CreateBasicAuctionConfig(GURL("https://example.com/bar.js")));

  const char kExpected[] = R"({
   "auctionSignals": {
      "pending": false,
      "value": null
   },
   "componentAuctions": [ "https://example.org", "https://example.com" ],
   "decisionLogicURL": "https://seller.test/foo",
   "deprecatedRenderURLReplacements": {
      "pending": false,
      "value": [ ]
   },
   "expectsAdditionalBids": false,
   "expectsDirectFromSellerSignalsHeaderAdSlot": false,
   "maxTrustedScoringSignalsURLLength": 0,
   "perBuyerCumulativeTimeouts": {
      "pending": false,
      "value": {
      }
   },
   "perBuyerCurrencies": {
      "pending": false,
      "value": {
      }
   },
   "perBuyerExperimentGroupIds": {
   },
   "perBuyerGroupLimits": {
      "*": 65535
   },
   "perBuyerMultiBidLimit": {
       "*": 1
   },
   "perBuyerPrioritySignals": {
   },
   "perBuyerSignals": {
      "pending": false,
      "value": null
   },
   "perBuyerTimeouts": {
      "pending": false,
      "value": {
      }
   },
   "requiredSellerCapabilities": [  ],
   "seller": "https://seller.test",
   "sellerSignals": {
      "pending": false,
      "value": null
   }
}
)";

  EXPECT_THAT(SerializeAuctionConfigForDevtools(config),
              base::test::IsJson(kExpected));
}

TEST(SerializeAuctionConfigTest, FullConfig) {
  AuctionConfig config = CreateFullAuctionConfig();
  // Fix the nonce for easier testing.
  config.non_shared_params.auction_nonce =
      base::Uuid::ParseLowercase("626e6419-1872-48ac-877d-c4c096f28284");

  const char kExpected[] = R"({
   "aggregationCoordinatorOrigin": "https://example.com",
   "allSlotsRequestedSizes": [ {
      "height": "70sh",
      "width": "100px"
   }, {
      "height": "50.5px",
      "width": "55.5sw"
   } ],
   "auctionNonce": "626e6419-1872-48ac-877d-c4c096f28284",
   "auctionReportBuyerKeys": [ "18446744073709551617", "18446744073709551618" ],
   "auctionReportBuyers": {
      "interestGroupCount": {
         "bucket": "0",
         "scale": 1.0
      },
      "totalSignalsFetchLatency": {
         "bucket": "1",
         "scale": 2.0
      }
   },
   "auctionSignals": {
      "pending": false,
      "value": "[4]"
   },
   "auctionReportBuyerDebugModeConfig": {
       "debugKey": "9223372036854775808",
       "enabled": true
   },
   "decisionLogicURL": "https://seller.test/foo",
   "expectsAdditionalBids": true,
   "expectsDirectFromSellerSignalsHeaderAdSlot": false,
   "maxTrustedScoringSignalsURLLength": 2560,
   "trustedScoringSignalsCoordinator": "https://example.test",
   "deprecatedRenderURLReplacements" : {
      "pending": false,
      "value": [ {
         "match": "${SELLER}",
         "replacement": "ExampleSSP"
      } ]
   },
   "interestGroupBuyers": [ "https://buyer.test" ],
   "perBuyerCumulativeTimeouts": {
      "pending": false,
      "value": {
         "*": 234000.0,
         "https://buyer.test": 432000.0
      }
   },
   "perBuyerCurrencies": {
      "pending": false,
      "value": {
         "*": "USD",
         "https://buyer.test": "CAD"
      }
   },
   "perBuyerExperimentGroupIds": {
      "*": 2,
      "https://buyer.test": 3
   },
   "perBuyerGroupLimits": {
      "*": 11,
      "https://buyer.test": 10
   },
   "perBuyerMultiBidLimit": {
       "*": 5,
       "https://buyer.test": 10
   },
   "perBuyerPrioritySignals": {
      "*": {
         "for": 5.0,
         "goats": -1.5,
         "sale": 0.0
      },
      "https://buyer.test": {
         "for": 0.0,
         "hats": 1.5,
         "sale": -2.0
      }
   },
   "perBuyerSignals": {
      "pending": false,
      "value": {
         "https://buyer.test": "[7]"
      }
   },
   "perBuyerTimeouts": {
      "pending": false,
      "value": {
         "*": 9000.0,
         "https://buyer.test": 8000.0
      }
   },
   "requestedSize": {
      "height": "70sh",
      "width": "100px"
   },
   "requiredSellerCapabilities": [ "latency-stats" ],
   "seller": "https://seller.test",
   "sellerCurrency": "EUR",
   "sellerExperimentGroupId": 1,
   "sellerSignals": {
      "pending": false,
      "value": "[5]"
   },
   "sellerTimeout": 6000.0,
   "reportingTimeout": 7000.0,
   "trustedScoringSignalsURL": "https://seller.test/bar",
   "sellerRealTimeReportingType": "default-local-reporting",
   "perBuyerRealTimeReportingTypes": {
      "https://buyer.test": "default-local-reporting"
   }
}
)";

  EXPECT_THAT(SerializeAuctionConfigForDevtools(config),
              base::test::IsJson(kExpected));
}

TEST(SerializeAuctionConfigTest, PendingPromise) {
  AuctionConfig config = CreateBasicAuctionConfig();
  config.non_shared_params.seller_signals =
      AuctionConfig::MaybePromiseJson::FromPromise();
  base::Value::Dict serialized = SerializeAuctionConfigForDevtools(config);
  const base::Value::Dict* signal_dict = serialized.FindDict("sellerSignals");
  ASSERT_TRUE(signal_dict);

  const char kExpected[] = R"({
   "pending": true
}
)";

  EXPECT_THAT(*signal_dict, base::test::IsJson(kExpected));
}

TEST(SerializeAuctionConfigTest, ServerResponse) {
  AuctionConfig config = CreateBasicAuctionConfig();
  config.server_response.emplace();
  config.server_response->request_id =
      base::Uuid::ParseLowercase("626e6419-1872-48ac-877d-c4c096f28284");
  base::Value::Dict serialized = SerializeAuctionConfigForDevtools(config);
  const base::Value::Dict* server_dict = serialized.FindDict("serverResponse");
  ASSERT_TRUE(server_dict);

  const char kExpected[] = R"({
   "requestId": "626e6419-1872-48ac-877d-c4c096f28284"
}
)";

  EXPECT_THAT(*server_dict, base::test::IsJson(kExpected));
}

TEST(SerializeInterestGroupTest, Basic) {
  InterestGroup ig;
  ig.expiry = base::Time::FromSecondsSinceUnixEpoch(100.5);
  ig.owner = url::Origin::Create(GURL("https://example.org"));
  ig.name = "ig_one";
  ig.priority = 5.5;
  ig.enable_bidding_signals_prioritization = true;
  ig.priority_vector = {{"i", 1}, {"j", 2}, {"k", 4}};
  ig.priority_signals_overrides = {{"a", 0.5}, {"b", 2}};
  ig.all_sellers_capabilities = {
      blink::SellerCapabilities::kInterestGroupCounts,
      blink::SellerCapabilities::kLatencyStats};
  ig.seller_capabilities = {
      {url::Origin::Create(GURL("https://example.org")),
       {blink::SellerCapabilities::kInterestGroupCounts}}};
  ig.execution_mode = InterestGroup::ExecutionMode::kGroupedByOriginMode;
  ig.bidding_url = GURL("https://example.org/bid.js");
  ig.bidding_wasm_helper_url = GURL("https://example.org/bid.wasm");
  ig.update_url = GURL("https://example.org/ig_update.json");
  ig.trusted_bidding_signals_url = GURL("https://example.org/trust.json");
  ig.trusted_bidding_signals_keys = {"l", "m"};
  ig.trusted_bidding_signals_slot_size_mode =
      InterestGroup::TrustedBiddingSignalsSlotSizeMode::kAllSlotsRequestedSizes;
  ig.max_trusted_bidding_signals_url_length = 100;
  ig.trusted_bidding_signals_coordinator =
      url::Origin::Create(GURL("https://example.test"));
  ig.user_bidding_signals = "hello";
  ig.ads = {
      {blink::InterestGroup::Ad(
           GURL("https://example.com/train"), "metadata", "sizegroup", "bid",
           "bsid", std::vector<std::string>{"selectable_id1", "selectable_id2"},
           "ad_render_id",
           {{url::Origin::Create(GURL("https://reporting.example.org"))}}),
       blink::InterestGroup::Ad(GURL("https://example.com/plane"), "meta2")}};
  ig.ad_components = {{
      {GURL("https://example.com/locomotive"), "meta3"},
      {GURL("https://example.com/turbojet"), "meta4"},
  }};
  ig.ad_sizes = {{"small", AdSize(100, AdSize::LengthUnit::kPixels, 5,
                                  AdSize::LengthUnit::kScreenHeight)}};
  ig.size_groups = {{"g1", {"small", "medium"}}, {"g2", {"large"}}};
  ig.auction_server_request_flags = {AuctionServerRequestFlagsEnum::kOmitAds};
  ig.additional_bid_key.emplace();
  ig.additional_bid_key->fill(0);
  ig.aggregation_coordinator_origin =
      url::Origin::Create(GURL("https://aggegator.example.org"));

  const char kExpected[] = R"({
    "expirationTime": 100.5,
    "ownerOrigin": "https://example.org",
    "name": "ig_one",
    "priority": 5.5,
    "enableBiddingSignalsPrioritization": true,
    "priorityVector": {"i": 1.0, "j": 2.0, "k": 4.0},
    "prioritySignalsOverrides": {"a": 0.5, "b": 2.0},
    "sellerCapabilities": {
        "*": [ "interest-group-counts", "latency-stats" ],
        "https://example.org": [ "interest-group-counts" ]
    },
    "executionMode": "group-by-origin",
    "auctionServerRequestFlags": [],
    "biddingLogicURL": "https://example.org/bid.js",
    "biddingWasmHelperURL": "https://example.org/bid.wasm",
    "updateURL": "https://example.org/ig_update.json",
    "trustedBiddingSignalsURL": "https://example.org/trust.json",
    "trustedBiddingSignalsKeys": [ "l", "m" ],
    "trustedBiddingSignalsSlotSizeMode": "all-slots-requested-sizes",
    "maxTrustedBiddingSignalsURLLength": 100,
    "trustedBiddingSignalsCoordinator": "https://example.test",
    "userBiddingSignals": "hello",
    "ads": [ {
      "adRenderId": "ad_render_id",
      "allowedReportingOrigins": [ "https://reporting.example.org" ],
      "buyerAndSellerReportingId": "bsid",
      "selectableBuyerAndSellerReportingIds": [ "selectable_id1", "selectable_id2" ],
      "buyerReportingId": "bid",
      "metadata": "metadata",
      "renderURL": "https://example.com/train"
    }, {
      "metadata": "meta2",
      "renderURL": "https://example.com/plane"
    } ],
    "adComponents": [ {
      "metadata": "meta3",
      "renderURL": "https://example.com/locomotive"
    }, {
      "metadata": "meta4",
      "renderURL": "https://example.com/turbojet"
    } ],
    "adSizes": {
        "small": {
          "height": "5sh",
          "width": "100px"
        }
    },
    "sizeGroups": {
      "g1": ["small", "medium"],
      "g2": ["large"]
    },
    "auctionServerRequestFlags": [ "omit-ads" ],
    "additionalBidKey": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
    "aggregationCoordinatorOrigin": "https://aggegator.example.org",
  })";
  EXPECT_THAT(SerializeInterestGroupForDevtools(ig),
              base::test::IsJson(kExpected));
}

}  // namespace
}  // namespace blink
