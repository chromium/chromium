// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/auction_config.h"

#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/test/values_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/common/interest_group/auction_config_test_util.h"

namespace blink {
namespace {

TEST(AuctionConfigTest, SerializeComponents) {
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
   "value": [  ]
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

  EXPECT_THAT(config.SerializeForDevtools(), base::test::IsJson(kExpected));
}

TEST(AuctionConfigTest, FullConfig) {
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
   "deprecatedRenderURLReplacements" : {
      "pending": false,
      "value": [ ]
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
   "trustedScoringSignalsURL": "https://seller.test/bar"
}
)";

  EXPECT_THAT(config.SerializeForDevtools(), base::test::IsJson(kExpected));
}

TEST(AuctionConfigTest, PendingPromise) {
  AuctionConfig config = CreateBasicAuctionConfig();
  config.non_shared_params.seller_signals =
      AuctionConfig::MaybePromiseJson::FromPromise();
  base::Value::Dict serialized = config.SerializeForDevtools();
  const base::Value::Dict* signal_dict = serialized.FindDict("sellerSignals");
  ASSERT_TRUE(signal_dict);

  const char kExpected[] = R"({
   "pending": true
}
)";

  EXPECT_THAT(*signal_dict, base::test::IsJson(kExpected));
}

TEST(AuctionConfigTest, ServerResponse) {
  AuctionConfig config = CreateBasicAuctionConfig();
  config.server_response.emplace();
  config.server_response->request_id =
      base::Uuid::ParseLowercase("626e6419-1872-48ac-877d-c4c096f28284");
  base::Value::Dict serialized = config.SerializeForDevtools();
  const base::Value::Dict* server_dict = serialized.FindDict("serverResponse");
  ASSERT_TRUE(server_dict);

  const char kExpected[] = R"({
   "requestId": "626e6419-1872-48ac-877d-c4c096f28284"
}
)";

  EXPECT_THAT(*server_dict, base::test::IsJson(kExpected));
}

}  // namespace
}  // namespace blink
