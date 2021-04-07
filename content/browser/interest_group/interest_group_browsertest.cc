// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class InterestGroupBrowserTest : public ContentBrowserTest {
 public:
  InterestGroupBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kFledgeInterestGroups);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, JoinInterestGroupBasic) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ("done", EvalJs(shell(),
                           R"(
(function() {
  navigator.joinAdInterestGroup(
      {name: 'cars', owner: 'https://test.com'}, /*joinDurationSec=*/1);
  return 'done';
})())"));

  // We just verify that the operation didn't crash and that the JS completes
  // successfully.
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, LeaveInterestGroupBasic) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ("done", EvalJs(shell(), R"(
(function() {
  navigator.leaveAdInterestGroup({name:"cars", owner:"https://test.com"});
  return 'done';
})())"));

  // We just verify that the operation didn't crash and that the JS completes
  // successfully.
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionBasic) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(nullptr, EvalJs(shell(), R"(
(async function() {
  return await navigator.runAdAuction({
    seller: 'https://test.com',
    decisionLogicUrl: 'https://test.com/decision_logic'
  });
})())"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, JoinInterestGroupFull) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ("done", EvalJs(shell(), R"(
(function() {
  navigator.joinAdInterestGroup(
      {
        name: 'cars',
        owner: 'https://test.com',
        biddingLogicUrl: 'https://test.com/bidding_url',
        dailyUpdateUrl: 'https://test.com/update_url',
        trustedBiddingSignalsUrl:
            'https://test.com/trusted_bidding_signals_url',
        trustedBiddingSignalsKeys: ['key1', 'key2'],
        userBiddingSignals: {some: 'json', data: {here: [1, 2, 3]}},
        ads: [{
          renderUrl: 'https://test.com/ad_url',
          metadata: {ad: 'metadata', here: [1, 2, 3]}
        }]
      },
      /*joinDurationSec=*/1);
  return 'done';
})())"));

  // We just verify that the operation didn't crash and that the JS completes
  // successfully.
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionFull) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(nullptr, EvalJs(shell(), R"(
(async function() {
  return await navigator.runAdAuction({
    seller: 'https://test.com',
    decisionLogicUrl: 'https://test.com/decision_logic',
    interestGroupBuyers: ['https://www.buyer1.com', 'https://www.buyer2.com'],
    auctionSignals: {more: 'json', stuff: {}},
    sellerSignals: {yet: 'more', info: 1},
    perBuyerSignals: {
      'https://www.buyer1.com': {even: 'more', x: 4.5},
      'https://www.buyer2.com': {the: 'end'}
    }
  });
})())"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionStarInterestGroupBuyers) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(nullptr, EvalJs(shell(), R"(
(async function() {
  return await navigator.runAdAuction({
    seller: 'https://test.com',
    decisionLogicUrl: 'https://test.com/decision_logic',
    interestGroupBuyers: '*',
  });
})())"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidOwner) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
      "owner 'https://invalid^&' for AuctionAdInterestGroup with name 'cars' "
      "must be a valid https origin.",
      EvalJs(shell(), R"(
(function() {
  try {
    navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidBiddingLogicUrl) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
      "biddingLogicUrl 'https://invalid^&' for AuctionAdInterestGroup with "
      "owner 'https://test.com' and name 'cars' cannot be resolved to a valid "
      "URL.",
      EvalJs(shell(), R"(
(function() {
  try {
    navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: 'https://test.com',
          biddingLogicUrl: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidDailyUpdateUrl) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
      "dailyUpdateUrl 'https://invalid^&' for AuctionAdInterestGroup with "
      "owner 'https://test.com' and name 'cars' cannot be resolved to a valid "
      "URL.",
      EvalJs(shell(), R"(
(function() {
  try {
    navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: 'https://test.com',
          dailyUpdateUrl: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidTrustedBiddingSignalsUrl) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
      "trustedBiddingSignalsUrl 'https://invalid^&' for "
      "AuctionAdInterestGroup with owner 'https://test.com' and name 'cars' "
      "cannot be resolved to a valid URL.",
      EvalJs(shell(), R"(
(function() {
  try {
    navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: 'https://test.com',
          trustedBiddingSignalsUrl: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidUserBiddingSignals) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
      "userBiddingSignals for AuctionAdInterestGroup with owner "
      "'https://test.com' and name 'cars' must be a JSON-serializable object.",
      EvalJs(shell(), R"(
(function() {
  try {
    navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: 'https://test.com',
          userBiddingSignals: function() {},
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidAdUrl) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
      "ad renderUrl 'https://invalid^&' for AuctionAdInterestGroup with owner "
      "'https://test.com' and name 'cars' cannot be resolved to a valid URL.",
      EvalJs(shell(), R"(
(function() {
  try {
    navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: 'https://test.com',
          ads: [{renderUrl:"https://invalid^&"}],
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidAdMetadata) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': ad "
      "metadata for AuctionAdInterestGroup with owner 'https://test.com' and "
      "name 'cars' must be a JSON-serializable object.",
      EvalJs(shell(), R"(
(function() {
  let x = {};
  let y = {};
  x.a = y;
  y.a = x;
  try {
    navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: 'https://test.com',
          ads: [{renderUrl:"https://test.com", metadata:x}],
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       LeaveInterestGroupInvalidOwner) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'leaveAdInterestGroup' on 'Navigator': "
      "owner 'https://invalid^&' for AuctionAdInterestGroup with name 'cars' "
      "must be a valid https origin.",
      EvalJs(shell(), R"(
(function() {
  try {
    navigator.leaveAdInterestGroup(
        {
          name: 'cars',
          owner: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionInvalidSeller) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': seller "
      "'https://invalid^&' for AuctionAdConfig must be a valid https origin.",
      EvalJs(shell(), R"(
(async function() {
  try {
    await navigator.runAdAuction({
      seller: 'https://invalid^&',
      decisionLogicUrl: 'https://test.com/decision_logic'
    });
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidDecisionLogicUrl) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "decisionLogicUrl 'https://invalid^&' for AuctionAdConfig with seller "
      "'https://test.com' cannot be resolved to a valid URL.",
      EvalJs(shell(), R"(
(async function() {
  try {
    await navigator.runAdAuction({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://invalid^&'
    });
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidInterestGroupBuyers) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "interestGroupBuyers buyer 'https://invalid^&' for AuctionAdConfig "
      "with seller 'https://test.com' must be a valid https origin.",
      EvalJs(shell(), R"(
(async function() {
  try {
    await navigator.runAdAuction({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      interestGroupBuyers: ['https://invalid^&'],
    });
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidInterestGroupBuyersStr) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "interestGroupBuyers 'not star' for AuctionAdConfig with seller "
      "'https://test.com' must be \"*\" (wildcard) or a list of buyer "
      "https origin strings.",
      EvalJs(shell(), R"(
(async function() {
  try {
    await navigator.runAdAuction({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      interestGroupBuyers: 'not star',
    });
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidAuctionSignals) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "auctionSignals for AuctionAdConfig with seller 'https://test.com' must "
      "be a JSON-serializable object.",
      EvalJs(shell(), R"(
(async function() {
  try {
    await navigator.runAdAuction({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      auctionSignals: alert
    });
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidSellerSignals) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "sellerSignals for AuctionAdConfig with seller 'https://test.com' must "
      "be a JSON-serializable object.",
      EvalJs(shell(), R"(
(async function() {
  try {
    await navigator.runAdAuction({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      sellerSignals: function() {}
    });
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidPerBuyerSignalsOrigin) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "perBuyerSignals buyer 'https://invalid^&' for AuctionAdConfig with "
      "seller 'https://test.com' must be a valid https origin.",
      EvalJs(shell(), R"(
(async function() {
  try {
    await navigator.runAdAuction({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      perBuyerSignals: {'https://invalid^&': {a:1}}
    });
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidPerBuyerSignals) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "perBuyerSignals for AuctionAdConfig with seller 'https://test.com' "
      "must be a JSON-serializable object.",
      EvalJs(shell(), R"(
(async function() {
  try {
    await navigator.runAdAuction({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      perBuyerSignals: {'https://test.com': function() {}}
    });
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
}

}  // namespace

}  // namespace content
