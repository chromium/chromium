// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <vector>

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/interest_group/interest_group_manager.h"
#include "content/browser/interest_group/interest_group_service_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "third_party/blink/public/mojom/interest_group/restricted_interest_group_store.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

class InterestGroupBrowserTest : public ContentBrowserTest {
 public:
  InterestGroupBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kFledgeInterestGroups);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    ASSERT_TRUE(embedded_test_server()->Start());
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(https_server_->Start());
    storage_ = static_cast<StoragePartitionImpl*>(
                   BrowserContext::GetDefaultStoragePartition(
                       shell()->web_contents()->GetBrowserContext()))
                   ->GetInterestGroupStorage();
  }

  bool JoinInterestGroupInJS(url::Origin owner, std::string name) {
    return "done" ==
           EvalJs(shell(),
                  base::StringPrintf(R"(
    (function() {
      navigator.joinAdInterestGroup(
        {name: '%s', owner: '%s'}, /*joinDurationSec=*/ 300);
      return 'done';
    })())",
                                     name.c_str(), owner.Serialize().c_str()));
  }

  bool JoinInterestGroupInJS(const blink::mojom::InterestGroupPtr& group) {
    std::ostringstream buf;
    buf << "{"
        << "name: '" << group->name << "', "
        << "owner: '" << group->owner << "'";
    if (group->bidding_url) {
      buf << ", biddingLogicUrl: '" << *group->bidding_url << "'";
    }
    if (group->update_url) {
      buf << ", dailyUpdateUrl: '" << *group->update_url << "'";
    }
    if (group->trusted_bidding_signals_url) {
      buf << ", trustedBiddingSignalsUrl: '"
          << *group->trusted_bidding_signals_url << "'";
    }
    buf << "}";

    return "done" == EvalJs(shell(), base::StringPrintf(R"(
    (function() {
      navigator.joinAdInterestGroup(
        %s, /*join_duration_sec=*/ 300);
      return 'done';
    })())",
                                                        buf.str().c_str()));
  }

  bool LeaveInterestGroupInJS(url::Origin owner, std::string name) {
    return "done" ==
           EvalJs(shell(),
                  base::StringPrintf(R"(
    (function() {
      navigator.leaveAdInterestGroup({name: '%s', owner: '%s'});
      return 'done';
    })())",
                                     name.c_str(), owner.Serialize().c_str()));
  }

  std::vector<url::Origin> GetAllInterestGroupsOwners() {
    std::vector<url::Origin> interest_group_owners;
    base::RunLoop run_loop;
    storage_->GetAllInterestGroupOwners(base::BindLambdaForTesting(
        [&run_loop, &interest_group_owners](std::vector<url::Origin> owners) {
          interest_group_owners = std::move(owners);
          run_loop.Quit();
        }));
    run_loop.Run();
    return interest_group_owners;
  }

  std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>
  GetInterestGroupsForOwner(const url::Origin& owner) {
    std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>
        interest_groups;
    base::RunLoop run_loop;
    storage_->GetInterestGroupsForOwner(
        owner,
        base::BindLambdaForTesting(
            [&run_loop, &interest_groups](
                std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>
                    groups) {
              interest_groups = std::move(groups);
              run_loop.Quit();
            }));
    run_loop.Run();
    return interest_groups;
  }

  std::vector<std::pair<url::Origin, std::string>> GetAllInterestGroups() {
    std::vector<std::pair<url::Origin, std::string>> interest_groups;
    for (const auto& owner : GetAllInterestGroupsOwners()) {
      for (const auto& interest_group : GetInterestGroupsForOwner(owner)) {
        interest_groups.emplace_back(interest_group->group->owner,
                                     interest_group->group->name);
      }
    }
    return interest_groups;
  }

  int GetJoinCount(url::Origin owner, std::string name) {
    for (const auto& interest_group : GetInterestGroupsForOwner(owner)) {
      if (interest_group->group->name == name) {
        return interest_group->signals->join_count;
      }
    }
    return 0;
  }

  bool JoinInterestGroupAndWait(url::Origin owner, std::string name) {
    int initial_count = GetJoinCount(owner, name);
    if (!JoinInterestGroupInJS(owner, name)) {
      return false;
    }
    while (GetJoinCount(owner, name) != initial_count + 1)
      ;

    return true;
  }

  bool LeaveInterestGroupAndWait(url::Origin owner, std::string name) {
    if (!LeaveInterestGroupInJS(owner, name)) {
      return false;
    }
    while (GetJoinCount(owner, name) != 0)
      ;
    return true;
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;
  InterestGroupManager* storage_;
};

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, JoinLeaveInterestGroup) {
  GURL test_url_a = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin_a = url::Origin::Create(test_url_a);
  ASSERT_TRUE(test_url_a.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));

  // This join should succeed and be added to the database.
  EXPECT_TRUE(JoinInterestGroupAndWait(test_origin_a, "cars"));

  // This join should silently fail since a.test is not the same origin as
  // foo.a.test.
  EXPECT_TRUE(JoinInterestGroupInJS(
      url::Origin::Create(GURL("https://foo.a.test")), "cars"));

  // This join should silently fail since a.test is not the same origin as
  // the bidding_url, bid.a.test
  EXPECT_TRUE(JoinInterestGroupInJS(blink::mojom::InterestGroup::New(
      /* expiry */ base::Time(),
      /* owner= */ test_origin_a,
      /* name = */ "bicycles",
      /* bidding_url = */ GURL("https://bid.a.test"),
      /* update_url  = */ base::nullopt,
      /* trusted_bidding_signals_url = */ base::nullopt,
      /* trusted_bidding_signals_keys = */ base::nullopt,
      /* user_bidding_signals = */ base::nullopt,
      /* ads = */ base::nullopt)));

  // This join should silently fail since a.test is not the same origin as
  // the update_url, update.a.test
  EXPECT_TRUE(JoinInterestGroupInJS(blink::mojom::InterestGroup::New(
      /* expiry */ base::Time(),
      /* owner= */ test_origin_a,
      /* name = */ "tricycles",
      /* bidding_url = */ base::nullopt,
      /* update_url  = */ GURL("https://update.a.test"),
      /* trusted_bidding_signals_url = */ base::nullopt,
      /* trusted_bidding_signals_keys = */ base::nullopt,
      /* user_bidding_signals = */ base::nullopt,
      /* ads = */ base::nullopt)));

  // This join should silently fail since a.test is not the same origin as
  // the trusted_bidding_signals_url, signals.a.test
  EXPECT_TRUE(JoinInterestGroupInJS(blink::mojom::InterestGroup::New(
      /* expiry */ base::Time(),
      /* owner= */ test_origin_a,
      /* name = */ "four-wheelers",
      /* bidding_url = */ base::nullopt,
      /* update_url  = */ base::nullopt,
      /* trusted_bidding_signals_url = */ GURL("https://signals.a.test"),
      /* trusted_bidding_signals_keys = */ base::nullopt,
      /* user_bidding_signals = */ base::nullopt,
      /* ads = */ base::nullopt)));

  // Another successful join.
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  url::Origin test_origin_b = url::Origin::Create(test_url_b);

  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));
  EXPECT_TRUE(JoinInterestGroupAndWait(test_origin_b, "trucks"));

  // Check that only the a.test and b.test interest groups were added to
  // the database.
  std::vector<std::pair<url::Origin, std::string>> expected_groups = {
      {test_origin_a, "cars"}, {test_origin_b, "trucks"}};
  std::vector<std::pair<url::Origin, std::string>> received_groups;

  received_groups = GetAllInterestGroups();
  EXPECT_THAT(received_groups,
              testing::UnorderedElementsAreArray(expected_groups));

  // Now test leaving
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));

  // This leave should silently fail because it is cross-origin
  EXPECT_TRUE(LeaveInterestGroupInJS(test_origin_b, "trucks"));

  // This leave should succeed.
  EXPECT_TRUE(LeaveInterestGroupAndWait(test_origin_a, "cars"));

  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));
  // This leave should do nothing because there is not interest group of that
  // name.
  EXPECT_TRUE(LeaveInterestGroupAndWait(test_origin_b, "cars"));

  // We expect that only test_origin_b's interest group remains.
  expected_groups = {{test_origin_b, "trucks"}};

  received_groups = GetAllInterestGroups();
  EXPECT_THAT(received_groups,
              testing::UnorderedElementsAreArray(expected_groups));
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

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupBasicNoBlink) {
  GURL test_url_a = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin_a = url::Origin::Create(test_url_a);
  ASSERT_TRUE(test_url_a.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));

  mojo::Remote<blink::mojom::RestrictedInterestGroupStore> interest_service;
  InterestGroupServiceImpl::CreateMojoService(
      shell()->web_contents()->GetMainFrame(),
      interest_service.BindNewPipeAndPassReceiver());

  auto interest_group = blink::mojom::InterestGroup::New();
  constexpr char kGroupName[] = "cars";
  interest_group->expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(300);
  interest_group->name = kGroupName;
  interest_group->owner = test_origin_a;
  interest_service->JoinInterestGroup(std::move(interest_group));
  interest_service.FlushForTesting();
  EXPECT_EQ(1, GetJoinCount(test_origin_a, kGroupName));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupOriginNotHttps) {
  // JS API is https-only, but a compromised renderer could send a http origin.
  GURL test_url_http = embedded_test_server()->GetURL("a.test", "/echo");
  url::Origin test_origin_http = url::Origin::Create(test_url_http);
  ASSERT_TRUE(test_url_http.SchemeIs(url::kHttpScheme));
  ASSERT_TRUE(NavigateToURL(shell(), test_url_http));

  mojo::Remote<blink::mojom::RestrictedInterestGroupStore> interest_service;
  InterestGroupServiceImpl::CreateMojoService(
      shell()->web_contents()->GetMainFrame(),
      interest_service.BindNewPipeAndPassReceiver());

  // Silently fails to join -- the frame origin isn't https.
  auto interest_group_http = blink::mojom::InterestGroup::New();
  constexpr char kGroupName[] = "cars";
  interest_group_http->expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(300);
  interest_group_http->name = kGroupName;
  interest_group_http->owner = test_origin_http;
  interest_service->JoinInterestGroup(std::move(interest_group_http));
  interest_service.FlushForTesting();
  EXPECT_EQ(0, GetJoinCount(test_origin_http, kGroupName));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupWrongOwnerOrigin) {
  GURL test_url_a = https_server_->GetURL("a.test", "/echo");
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  url::Origin test_origin_a = url::Origin::Create(test_url_a);
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(test_url_a.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));

  mojo::Remote<blink::mojom::RestrictedInterestGroupStore> interest_service;
  InterestGroupServiceImpl::CreateMojoService(
      shell()->web_contents()->GetMainFrame(),
      interest_service.BindNewPipeAndPassReceiver());

  // Silently fails to join -- the owner origin doesn't match the frame origin.
  auto interest_group = blink::mojom::InterestGroup::New();
  constexpr char kGroupName[] = "cars";
  interest_group->expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(300);
  interest_group->name = kGroupName;
  interest_group->owner = test_origin_b;
  interest_service->JoinInterestGroup(std::move(interest_group));
  interest_service.FlushForTesting();
  EXPECT_EQ(0, GetJoinCount(test_origin_b, kGroupName));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupWrongBiddingUrlOrigin) {
  GURL test_url_a = https_server_->GetURL("a.test", "/echo");
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  url::Origin test_origin_a = url::Origin::Create(test_url_a);
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(test_url_a.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));

  mojo::Remote<blink::mojom::RestrictedInterestGroupStore> interest_service;
  InterestGroupServiceImpl::CreateMojoService(
      shell()->web_contents()->GetMainFrame(),
      interest_service.BindNewPipeAndPassReceiver());

  // Silently fails to join -- the bidding URL origin doesn't match the frame
  // origin.
  auto interest_group = blink::mojom::InterestGroup::New();
  constexpr char kGroupName[] = "cars";
  interest_group->expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(300);
  interest_group->name = kGroupName;
  interest_group->owner = test_origin_a;
  interest_group->bidding_url = test_url_b;
  interest_service->JoinInterestGroup(std::move(interest_group));
  interest_service.FlushForTesting();
  EXPECT_EQ(0, GetJoinCount(test_origin_a, kGroupName));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupWrongUpdateUrlOrigin) {
  GURL test_url_a = https_server_->GetURL("a.test", "/echo");
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  url::Origin test_origin_a = url::Origin::Create(test_url_a);
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(test_url_a.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));

  mojo::Remote<blink::mojom::RestrictedInterestGroupStore> interest_service;
  InterestGroupServiceImpl::CreateMojoService(
      shell()->web_contents()->GetMainFrame(),
      interest_service.BindNewPipeAndPassReceiver());

  // Silently fails to join -- the update URL origin doesn't match the frame
  // origin.
  auto interest_group = blink::mojom::InterestGroup::New();
  constexpr char kGroupName[] = "cars";
  interest_group->expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(300);
  interest_group->name = kGroupName;
  interest_group->owner = test_origin_a;
  interest_group->update_url = test_url_b;
  interest_service->JoinInterestGroup(std::move(interest_group));
  interest_service.FlushForTesting();
  EXPECT_EQ(0, GetJoinCount(test_origin_a, kGroupName));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupWrongTrustedBiddingSignalsOrigin) {
  GURL test_url_a = https_server_->GetURL("a.test", "/echo");
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  url::Origin test_origin_a = url::Origin::Create(test_url_a);
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(test_url_a.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));

  mojo::Remote<blink::mojom::RestrictedInterestGroupStore> interest_service;
  InterestGroupServiceImpl::CreateMojoService(
      shell()->web_contents()->GetMainFrame(),
      interest_service.BindNewPipeAndPassReceiver());

  // Silently fails to join -- the trusted bidding signals URL origin doesn't
  // match the frame origin.
  auto interest_group = blink::mojom::InterestGroup::New();
  constexpr char kGroupName[] = "cars";
  interest_group->expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(300);
  interest_group->name = kGroupName;
  interest_group->owner = test_origin_a;
  interest_group->trusted_bidding_signals_url = test_url_b;
  interest_service->JoinInterestGroup(std::move(interest_group));
  interest_service.FlushForTesting();
  EXPECT_EQ(0, GetJoinCount(test_origin_a, kGroupName));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupAdRenderingUrlNotHttps) {
  GURL test_url_https = https_server_->GetURL("a.test", "/echo");
  GURL test_url_http = embedded_test_server()->GetURL("b.test", "/echo");
  url::Origin test_origin_https = url::Origin::Create(test_url_https);
  ASSERT_TRUE(test_url_https.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(test_url_http.SchemeIs(url::kHttpScheme));
  ASSERT_TRUE(NavigateToURL(shell(), test_url_https));

  mojo::Remote<blink::mojom::RestrictedInterestGroupStore> interest_service;
  InterestGroupServiceImpl::CreateMojoService(
      shell()->web_contents()->GetMainFrame(),
      interest_service.BindNewPipeAndPassReceiver());

  // Silently fails to join -- the ad renderingUrl doesn't use https.
  auto interest_group = blink::mojom::InterestGroup::New();
  constexpr char kGroupName[] = "cars";
  interest_group->expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(300);
  interest_group->name = kGroupName;
  interest_group->owner = test_origin_https;
  auto ad = blink::mojom::InterestGroupAd::New();
  ad->render_url = test_url_http;
  interest_group->ads.emplace();
  interest_group->ads->push_back(std::move(ad));
  interest_service->JoinInterestGroup(std::move(interest_group));
  interest_service.FlushForTesting();
  EXPECT_EQ(0, GetJoinCount(test_origin_https, kGroupName));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       LeaveInterestGroupWrongOwnerOrigin) {
  GURL test_url_a = https_server_->GetURL("a.test", "/echo");
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  url::Origin test_origin_a = url::Origin::Create(test_url_a);
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(test_url_a.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));

  mojo::Remote<blink::mojom::RestrictedInterestGroupStore> interest_service1;
  InterestGroupServiceImpl::CreateMojoService(
      shell()->web_contents()->GetMainFrame(),
      interest_service1.BindNewPipeAndPassReceiver());

  // Join succeeds.
  auto interest_group = blink::mojom::InterestGroup::New();
  constexpr char kGroupName[] = "cars";
  interest_group->expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(300);
  interest_group->name = kGroupName;
  interest_group->owner = test_origin_a;
  interest_service1->JoinInterestGroup(std::move(interest_group));
  interest_service1.FlushForTesting();
  EXPECT_EQ(1, GetJoinCount(test_origin_a, kGroupName));

  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));
  // The frame changes upon navigation -- connect to the new service.
  mojo::Remote<blink::mojom::RestrictedInterestGroupStore> interest_service2;
  InterestGroupServiceImpl::CreateMojoService(
      shell()->web_contents()->GetMainFrame(),
      interest_service2.BindNewPipeAndPassReceiver());

  // Silently fails to leave -- the owner origin doesn't match the frame origin.
  interest_service2->LeaveInterestGroup(test_origin_a, kGroupName);
  interest_service2.FlushForTesting();
  EXPECT_EQ(1, GetJoinCount(test_origin_a, kGroupName));
}

}  // namespace

}  // namespace content
