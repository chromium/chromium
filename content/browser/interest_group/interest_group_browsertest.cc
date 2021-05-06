// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/interest_group/interest_group_manager.h"
#include "content/browser/interest_group/interest_group_service_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"
#include "net/base/isolation_info.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "third_party/blink/public/mojom/interest_group/restricted_interest_group_store.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

class AllowlistedOriginContentBrowserClient : public TestContentBrowserClient {
 public:
  explicit AllowlistedOriginContentBrowserClient() = default;

  void SetAllowList(base::flat_set<url::Origin>&& allow_list) {
    allow_list_ = allow_list;
  }

  // ContentBrowserClient overrides:
  bool IsInterestGroupAPIAllowed(content::BrowserContext* browser_context,
                                 const url::Origin& top_frame_origin,
                                 const GURL& api_url) override {
    return allow_list_.contains(top_frame_origin) &&
           allow_list_.contains(url::Origin::Create(api_url));
  }

 private:
  base::flat_set<url::Origin> allow_list_;

  DISALLOW_COPY_AND_ASSIGN(AllowlistedOriginContentBrowserClient);
};

class InterestGroupBrowserTest : public ContentBrowserTest {
 public:
  InterestGroupBrowserTest() {
    feature_list_.InitWithFeatures({blink::features::kFledgeInterestGroups,
                                    blink::features::kFledgeInterestGroupAPI},
                                   {});
  }

  ~InterestGroupBrowserTest() override {
    if (old_content_browser_client_)
      SetBrowserClientForTesting(old_content_browser_client_);
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
    https_server_->RegisterRequestMonitor(base::BindRepeating(
        &InterestGroupBrowserTest::OnHttpsTestServerRequestMonitor,
        base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());
    storage_ = static_cast<StoragePartitionImpl*>(
                   BrowserContext::GetDefaultStoragePartition(
                       shell()->web_contents()->GetBrowserContext()))
                   ->GetInterestGroupStorage();
    content_browser_client_.SetAllowList(
        {url::Origin::Create(https_server_->GetURL("a.test", "/")),
         url::Origin::Create(https_server_->GetURL("b.test", "/")),
         url::Origin::Create(https_server_->GetURL("c.test", "/"))});
    old_content_browser_client_ =
        SetBrowserClientForTesting(&content_browser_client_);
  }

  bool JoinInterestGroupInJS(url::Origin owner,
                             std::string name) WARN_UNUSED_RESULT {
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

  // The `trusted_bidding_signals_keys` and `ads` fields of `group` will be
  // ignored in favor of the passed in values.
  bool JoinInterestGroupInJS(const blink::mojom::InterestGroupPtr& group,
                             const std::string& ads = std::string(),
                             const std::string& trusted_bidding_signals_keys =
                                 std::string()) WARN_UNUSED_RESULT {
    // TODO(qingxin): Use base::Value to replace ostringstream.
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
    if (group->user_bidding_signals) {
      buf << ", userBiddingSignals: " << group->user_bidding_signals.value();
    }
    if (!trusted_bidding_signals_keys.empty()) {
      buf << ", trustedBiddingSignalsKeys: " << trusted_bidding_signals_keys;
    }
    if (!ads.empty()) {
      buf << ", ads: " << ads;
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

  bool JoinInterestGroupAndWait(url::Origin owner,
                                std::string name) WARN_UNUSED_RESULT {
    int initial_count = GetJoinCount(owner, name);
    if (!JoinInterestGroupInJS(owner, name)) {
      return false;
    }
    while (GetJoinCount(owner, name) != initial_count + 1) {
    }

    return true;
  }

  bool JoinInterestGroupAndWait(
      const blink::mojom::InterestGroupPtr& group,
      const std::string& ads = std::string(),
      const std::string& trusted_bidding_signals_keys = std::string())
      WARN_UNUSED_RESULT {
    int initial_count = GetJoinCount(group->owner, group->name);
    if (!JoinInterestGroupInJS(group, ads, trusted_bidding_signals_keys)) {
      return false;
    }
    while (GetJoinCount(group->owner, group->name) != initial_count + 1) {
    }

    return true;
  }

  bool LeaveInterestGroupAndWait(url::Origin owner, std::string name) {
    if (!LeaveInterestGroupInJS(owner, name)) {
      return false;
    }
    while (GetJoinCount(owner, name) != 0) {
    }
    return true;
  }

  content::EvalJsResult RunAuctionAndWait(
      const std::string& auction_config_json) WARN_UNUSED_RESULT {
    return EvalJs(shell(), base::StringPrintf(
                               R"(
(async function() {
  try {
    return await navigator.runAdAuction(%s);
  } catch (e) {
    return e.toString();
  }
})())",
                               auction_config_json.c_str()));
  }

  void WaitForURL(const GURL& url) {
    {
      base::AutoLock auto_lock(requests_lock_);
      if (received_https_test_server_requests_.count(url) > 0u)
        return;
      wait_for_url_ = url;
      request_run_loop_ = std::make_unique<base::RunLoop>();
    }
    request_run_loop_->Run();
    request_run_loop_.reset();
  }

  void OnHttpsTestServerRequestMonitor(
      const net::test_server::HttpRequest& request) {
    base::AutoLock auto_lock(requests_lock_);
    received_https_test_server_requests_.insert(request.GetURL());
    if (wait_for_url_ == request.GetURL()) {
      wait_for_url_ = GURL();
      request_run_loop_->Quit();
    }
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;
  AllowlistedOriginContentBrowserClient content_browser_client_;
  ContentBrowserClient* old_content_browser_client_;
  InterestGroupManager* storage_;
  base::Lock requests_lock_;
  std::set<GURL> received_https_test_server_requests_
      GUARDED_BY(requests_lock_);
  std::unique_ptr<base::RunLoop> request_run_loop_;
  GURL wait_for_url_ GUARDED_BY(requests_lock_);
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

  // This join should silently fail since d.test is not allowlisted for the API
  GURL test_url_d = https_server_->GetURL("d.test", "/echo");
  url::Origin test_origin_d = url::Origin::Create(test_url_d);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_d));
  EXPECT_TRUE(JoinInterestGroupInJS(test_origin_d, "toys"));

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
  // Test that we can't leave an interest group from a site not allowedlisted
  // for the API.
  // Inject an interest group into the DB for that for that site so we can try
  // to remove it.
  storage_->JoinInterestGroup(blink::mojom::InterestGroup::New(
      /* expiry */ base::Time::Now() + base::TimeDelta::FromSeconds(300),
      /* owner= */ test_origin_d,
      /* name = */ "candy",
      /* bidding_url = */ base::nullopt,
      /* update_url  = */ base::nullopt,
      /* trusted_bidding_signals_url = */ base::nullopt,
      /* trusted_bidding_signals_keys = */ base::nullopt,
      /* user_bidding_signals = */ base::nullopt,
      /* ads = */ base::nullopt));

  ASSERT_TRUE(NavigateToURL(shell(), test_url_d));
  // This leave should do nothing because origin_d is not allowed by privacy
  // sandbox.
  EXPECT_TRUE(LeaveInterestGroupInJS(test_origin_d, "candy"));

  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));
  // This leave should do nothing because there is not interest group of that
  // name.
  EXPECT_TRUE(LeaveInterestGroupInJS(test_origin_b, "cars"));

  // This leave should silently fail because it is cross-origin
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));
  EXPECT_TRUE(LeaveInterestGroupInJS(test_origin_b, "trucks"));

  // This leave should succeed.
  EXPECT_TRUE(LeaveInterestGroupAndWait(test_origin_a, "cars"));

  // We expect that test_origin_b and the (injected) test_origin_d's interest
  // groups remain.
  expected_groups = {{test_origin_b, "trucks"}, {test_origin_d, "candy"}};
  received_groups = GetAllInterestGroups();
  EXPECT_THAT(received_groups,
              testing::UnorderedElementsAreArray(expected_groups));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionBasic) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_EQ(
      nullptr,
      RunAuctionAndWait(JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2
  })",
          test_url.GetOrigin().spec(),
          https_server_->GetURL("b.test", "/interest_group/decision_logic.js")
              .spec())));
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

  EXPECT_EQ(nullptr, RunAuctionAndWait(R"({
    seller: 'https://test.com',
    decisionLogicUrl: 'https://test.com/decision_logic',
    interestGroupBuyers: ['https://www.buyer1.com', 'https://www.buyer2.com'],
    auctionSignals: {more: 'json', stuff: {}},
    sellerSignals: {yet: 'more', info: 1},
    perBuyerSignals: {
      'https://www.buyer1.com': {even: 'more', x: 4.5},
      'https://www.buyer2.com': {the: 'end'}
    }
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionStarInterestGroupBuyers) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(nullptr, RunAuctionAndWait(R"({
    seller: 'https://test.com',
    decisionLogicUrl: 'https://test.com/decision_logic',
    interestGroupBuyers: '*',
  })"));
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
      RunAuctionAndWait(R"({
      seller: 'https://invalid^&',
      decisionLogicUrl: 'https://test.com/decision_logic'
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionHttpSeller) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': seller "
      "'http://test.com' for AuctionAdConfig must be a valid https origin.",
      RunAuctionAndWait(R"({
      seller: 'http://test.com',
      decisionLogicUrl: 'https://test.com/decision_logic'
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidDecisionLogicUrl) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "decisionLogicUrl 'https://invalid^&' for AuctionAdConfig with seller "
      "'https://test.com' cannot be resolved to a valid URL.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://invalid^&'
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionDecisionLogicUrlDifferentFromSeller) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  std::string ads =
      "[{renderUrl : 'https://example.com/render',"
      "metadata : {ad:'metadata', here : [ 1, 2 ]}}]";
  EXPECT_TRUE(JoinInterestGroupAndWait(
      blink::mojom::InterestGroup::New(
          /* expiry */ base::Time(),
          /* owner= */ url::Origin::Create(test_url.GetOrigin()),
          /* name = */ "cars",
          /* bidding_url = */
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /* update_url  = */ base::nullopt,
          /* trusted_bidding_signals_url = */ base::nullopt,
          /* trusted_bidding_signals_keys = */ base::nullopt,
          /* user_bidding_signals = */ "{some: 'json', data: {here: [1, 2]}}",
          /* ads = */ base::nullopt),
      ads));

  EXPECT_EQ(
      nullptr,
      RunAuctionAndWait(JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
  })",
          test_url.GetOrigin().spec(),
          https_server_->GetURL("b.test", "/interest_group/decision_logic.js")
              .spec())));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidInterestGroupBuyers) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "interestGroupBuyers buyer 'https://invalid^&' for AuctionAdConfig "
      "with seller 'https://test.com' must be a valid https origin.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      interestGroupBuyers: ['https://invalid^&'],
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidInterestGroupBuyersStr) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "interestGroupBuyers 'not star' for AuctionAdConfig with seller "
      "'https://test.com' must be \"*\" (wildcard) or a list of buyer "
      "https origin strings.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      interestGroupBuyers: 'not star',
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionNoInterestGroupBuyersField) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(nullptr, RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionNoInterestGroupBuyers) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(nullptr, RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      interestGroupBuyers: [],
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidAuctionSignals) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "auctionSignals for AuctionAdConfig with seller 'https://test.com' must "
      "be a JSON-serializable object.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      auctionSignals: alert
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidSellerSignals) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "sellerSignals for AuctionAdConfig with seller 'https://test.com' must "
      "be a JSON-serializable object.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      sellerSignals: function() {}
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidPerBuyerSignalsOrigin) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "perBuyerSignals buyer 'https://invalid^&' for AuctionAdConfig with "
      "seller 'https://test.com' must be a valid https origin.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      perBuyerSignals: {'https://invalid^&': {a:1}}
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionPerBuyerSignalsOriginNotInBuyers) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  std::string ads =
      "[{renderUrl : 'https://example.com/render',"
      "metadata : {ad:'metadata', here : [ 1, 2 ]}}]";
  EXPECT_TRUE(JoinInterestGroupAndWait(
      blink::mojom::InterestGroup::New(
          /* expiry */ base::Time(),
          /* owner= */ url::Origin::Create(test_url.GetOrigin()),
          /* name = */ "cars",
          /* bidding_url = */
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /* update_url  = */ base::nullopt,
          /* trusted_bidding_signals_url = */ base::nullopt,
          /* trusted_bidding_signals_keys = */ base::nullopt,
          /* user_bidding_signals = */ "{some: 'json', data: {here: [1, 2]}}",
          /* ads = */ base::nullopt),
      ads));

  EXPECT_EQ(
      nullptr,
      RunAuctionAndWait(JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    perBuyerSignals: {$1: {a:1}, 'https://not_in_buyers.com': {a:1}}
  })",
          test_url.GetOrigin().spec(),
          https_server_->GetURL("a.test", "/interest_group/decision_logic.js")
              .spec())));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidPerBuyerSignals) {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "perBuyerSignals for AuctionAdConfig with seller 'https://test.com' "
      "must be a JSON-serializable object.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      perBuyerSignals: {'https://test.com': function() {}}
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionBuyersNoInterestGroup) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_EQ(
      nullptr,
      RunAuctionAndWait(JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
  })",
          test_url.GetOrigin().spec(),
          https_server_->GetURL("a.test", "/interest_group/decision_logic.js")
              .spec())));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionPrivacySandboxDisabled) {
  // Successful join at a.test
  GURL test_url_a = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));
  std::string ads =
      "[{renderUrl : 'https://example.com/render',"
      "metadata : {ad:'metadata', here : [ 1, 2 ]}}]";
  EXPECT_TRUE(JoinInterestGroupAndWait(
      blink::mojom::InterestGroup::New(
          /* expiry */ base::Time(),
          /* owner= */ url::Origin::Create(test_url_a),
          /* name = */ "cars",
          /* bidding_url = */
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /* update_url  = */ base::nullopt,
          /* trusted_bidding_signals_url = */
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /* trusted_bidding_signals_keys = */ base::nullopt,
          /* user_bidding_signals = */ "{some: 'json', data: {here: [1, 2]}}",
          /* ads = */ base::nullopt),
      ads, "['key1']"));

  GURL test_url_d = https_server_->GetURL("d.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url_d));

  // Auction should not be run since d.test has the API disabled.
  EXPECT_EQ(
      nullptr,
      RunAuctionAndWait(JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    perBuyerSignals: {$3: {even: 'more', x: 4.5}}
  })",
          test_url_d.GetOrigin(),
          https_server_->GetURL("d.test", "/interest_group/decision_logic.js"),
          test_url_a.GetOrigin())));

  // No requests should have been made for the interest group or auction URLs.
  base::AutoLock auto_lock(requests_lock_);
  EXPECT_FALSE(base::Contains(
      received_https_test_server_requests_,
      https_server_->GetURL("/interest_group/bidding_logic.js")));
  EXPECT_FALSE(base::Contains(
      received_https_test_server_requests_,
      https_server_->GetURL("/interest_group/trusted_bidding_signals.json")));
  EXPECT_FALSE(base::Contains(
      received_https_test_server_requests_,
      https_server_->GetURL("/interest_group/decision_logic.js")));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionDisabledInterestGroup) {
  // Inject an interest group into the DB for that for a disabled site so we can
  // try to remove it.
  GURL disabled_domain = https_server_->GetURL("d.test", "/");
  blink::mojom::InterestGroupPtr disabled_group =
      blink::mojom::InterestGroup::New();
  disabled_group->expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(300);
  disabled_group->owner = url::Origin::Create(disabled_domain);
  disabled_group->name = "candy";
  disabled_group->bidding_url = https_server_->GetURL(
      disabled_domain.host(),
      "/interest_group/bidding_logic_stop_bidding_after_win.js");
  disabled_group->ads = std::vector<blink::mojom::InterestGroupAdPtr>();
  disabled_group->ads->emplace_back(blink::mojom::InterestGroupAd::New(
      GURL("https://stop_bidding_after_win.com/render"), base::nullopt));
  storage_->JoinInterestGroup(std::move(disabled_group));
  ASSERT_EQ(1, GetJoinCount(url::Origin::Create(disabled_domain), "candy"));

  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  std::string ads =
      "[{renderUrl : 'https://example.com/render',"
      "metadata : {ad:'metadata', here : [ 1, 2 ]}}]";
  EXPECT_TRUE(JoinInterestGroupAndWait(
      blink::mojom::InterestGroup::New(
          /* expiry */ base::Time(),
          /* owner= */ url::Origin::Create(test_url.GetOrigin()),
          /* name = */ "cars",
          /* bidding_url = */
          https_server_->GetURL(test_url.host(),
                                "/interest_group/bidding_logic.js"),
          /* update_url  = */ base::nullopt,
          /* trusted_bidding_signals_url = */
          https_server_->GetURL(test_url.host(),
                                "/interest_group/trusted_bidding_signals.json"),
          /* trusted_bidding_signals_keys = */ base::nullopt,
          /* user_bidding_signals = */ "{some: 'json', data: {here: [1, 2]}}",
          /* ads = */ base::nullopt),
      ads, "['key1']"));

  EXPECT_EQ("https://example.com/render",
            RunAuctionAndWait(JsReplace(
                R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1, $3],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    perBuyerSignals: {$1: {even: 'more', x: 4.5}}
  })",
                test_url.GetOrigin(),
                https_server_->GetURL(test_url.host(),
                                      "/interest_group/decision_logic.js"),
                disabled_domain.GetOrigin())));
  // No requests should have been made for the disabled interest group's URLs.
  base::AutoLock auto_lock(requests_lock_);
  EXPECT_FALSE(base::Contains(
      received_https_test_server_requests_,
      https_server_->GetURL(
          "/interest_group/bidding_logic_stop_bidding_after_win.js")));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionWithWinner) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  std::string ads =
      "[{renderUrl : 'https://example.com/render',"
      "metadata : {ad:'metadata', here : [ 1, 2 ]}}]";
  EXPECT_TRUE(JoinInterestGroupAndWait(
      blink::mojom::InterestGroup::New(
          /* expiry */ base::Time(),
          /* owner= */ url::Origin::Create(test_url.GetOrigin()),
          /* name = */ "cars",
          /* bidding_url = */
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /* update_url  = */ base::nullopt,
          /* trusted_bidding_signals_url = */
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /* trusted_bidding_signals_keys = */ base::nullopt,
          /* user_bidding_signals = */ "{some: 'json', data: {here: [1, 2]}}",
          /* ads = */ base::nullopt),
      ads, "['key1']"));

  EXPECT_EQ(
      "https://example.com/render",
      RunAuctionAndWait(JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    perBuyerSignals: {$1: {even: 'more', x: 4.5}}
  })",
          test_url.GetOrigin().spec(),
          https_server_->GetURL("a.test", "/interest_group/decision_logic.js")
              .spec())));
  // Reporting urls should be fetched after an auction succeeded.
  WaitForURL(https_server_->GetURL("/echoall?report_seller"));
  WaitForURL(https_server_->GetURL("/echoall?report_bidder"));

  // Check ResourceRequest structs of requests issued by the worklet process.
  const struct ExpectedRequest {
    GURL url;
    const char* accept_header;
    bool expect_trusted_params;
  } kExpectedRequests[] = {
      {https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
       "application/javascript", true /* expect_trusted_params */},
      {https_server_->GetURL(
           "a.test",
           "/interest_group/"
           "trusted_bidding_signals.json?hostname=a.test&keys=key1"),
       "application/json", true /* expect_trusted_params */},
      {https_server_->GetURL("a.test", "/interest_group/decision_logic.js"),
       "application/javascript", false /* expect_trusted_params */},
  };
  for (const auto& expected_request : kExpectedRequests) {
    SCOPED_TRACE(expected_request.url);

    base::Optional<network::ResourceRequest> request =
        url_loader_monitor.GetRequestInfo(expected_request.url);
    ASSERT_TRUE(request);
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
              request->credentials_mode);
    EXPECT_EQ(network::mojom::RedirectMode::kError, request->redirect_mode);
    EXPECT_EQ(url::Origin::Create(test_url), request->request_initiator);

    EXPECT_EQ(1u, request->headers.GetHeaderVector().size());
    std::string accept_value;
    ASSERT_TRUE(request->headers.GetHeader(net::HttpRequestHeaders::kAccept,
                                           &accept_value));
    EXPECT_EQ(expected_request.accept_header, accept_value);

    EXPECT_EQ(expected_request.expect_trusted_params,
              request->trusted_params.has_value());
    if (!request->trusted_params) {
      // Requests for render-provided URLs use an empty trusted params value and
      // enable CORS (and should use the RenderFrameHosts's URLLoaderFactory,
      // which is validated in the next test).
      EXPECT_EQ(network::mojom::RequestMode::kCors, request->mode);
    } else {
      // Requests for interest-group provided URLs are cross-origin, and set
      // trusted params to use the right cache shard, since they use a trusted
      // URLLoaderFactory.
      EXPECT_EQ(network::mojom::RequestMode::kNoCors, request->mode);
      const net::IsolationInfo& isolation_info =
          request->trusted_params->isolation_info;
      EXPECT_EQ(net::IsolationInfo::RequestType::kOther,
                isolation_info.request_type());
      url::Origin expected_origin = url::Origin::Create(expected_request.url);
      EXPECT_EQ(expected_origin, isolation_info.top_frame_origin());
      EXPECT_EQ(expected_origin, isolation_info.frame_origin());
      EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
    }
  }

  // Check ResourceRequest structs of report requests.
  const GURL kExpectedReportUrls[] = {
      https_server_->GetURL("a.test", "/echoall?report_seller"),
      https_server_->GetURL("a.test", "/echoall?report_bidder"),
  };
  for (const auto& expected_report_url : kExpectedReportUrls) {
    SCOPED_TRACE(expected_report_url);

    base::Optional<network::ResourceRequest> request =
        url_loader_monitor.GetRequestInfo(expected_report_url);
    ASSERT_TRUE(request);
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
              request->credentials_mode);
    EXPECT_EQ(network::mojom::RedirectMode::kError, request->redirect_mode);
    EXPECT_EQ(url::Origin::Create(test_url), request->request_initiator);

    EXPECT_TRUE(request->headers.IsEmpty());

    ASSERT_TRUE(request->trusted_params);
    const net::IsolationInfo& isolation_info =
        request->trusted_params->isolation_info;
    EXPECT_EQ(net::IsolationInfo::RequestType::kOther,
              isolation_info.request_type());
    EXPECT_TRUE(isolation_info.network_isolation_key().IsTransient());
    EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  }

  // The two reporting requests should use different NIKs to prevent the
  // requests from being correlated.
  EXPECT_NE(url_loader_monitor.GetRequestInfo(kExpectedReportUrls[0])
                ->trusted_params->isolation_info.network_isolation_key(),
            url_loader_monitor.GetRequestInfo(kExpectedReportUrls[1])
                ->trusted_params->isolation_info.network_isolation_key());
}

// Use different origins for publisher, bidder, and seller, and make sure
// everything works as expected.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, CrossOrigin) {
  const char kPublisher[] = "a.test";
  const char kBidder[] = "b.test";
  const char kSeller[] = "c.test";

  // Navigate to bidder site, and add an interest group.
  GURL bidder_url = https_server_->GetURL(kBidder, "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), bidder_url));
  std::string ads =
      "[{renderUrl : 'https://example.com/render',"
      "metadata : {ad:'metadata', here : [ 1, 2 ]}}]";
  EXPECT_TRUE(JoinInterestGroupAndWait(
      blink::mojom::InterestGroup::New(
          /* expiry */ base::Time(),
          /* owner= */ url::Origin::Create(bidder_url.GetOrigin()),
          /* name = */ "cars",
          /* bidding_url = */
          https_server_->GetURL(kBidder, "/interest_group/bidding_logic.js"),
          /* update_url  = */ base::nullopt,
          /* trusted_bidding_signals_url = */
          https_server_->GetURL(kBidder,
                                "/interest_group/trusted_bidding_signals.json"),
          /* trusted_bidding_signals_keys = */ base::nullopt,
          /* user_bidding_signals = */ "{some: 'json', data: {here: [1, 2]}}",
          /* ads = */ base::nullopt),
      ads, "['key1']"));

  // Navigate to publisher.
  ASSERT_TRUE(
      NavigateToURL(shell(), https_server_->GetURL(kPublisher, "/echo")));

  // Run auction with a seller script missing an "Access-Control-Allow-Origin"
  // header. The request for the seller script should fail, and so should the
  // auction.
  GURL seller_logic_url =
      https_server_->GetURL(kSeller, "/interest_group/decision_logic.js");
  EXPECT_EQ(nullptr,
            RunAuctionAndWait(JsReplace(
                R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$3],
  auctionSignals: {x: 1},
  sellerSignals: {yet: 'more', info: 1},
  perBuyerSignals: {$3: {even: 'more', x: 4.5}}
}
)",
                url::Origin::Create(seller_logic_url), seller_logic_url.spec(),
                url::Origin::Create(bidder_url))));

  // Run auction with a seller script with an "Access-Control-Allow-Origin"
  // header. The auction should succeed.
  seller_logic_url = https_server_->GetURL(
      kSeller, "/interest_group/decision_logic_cross_origin.js");
  ASSERT_EQ("https://example.com/render",
            RunAuctionAndWait(JsReplace(
                R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$3],
  auctionSignals: {x: 1},
  sellerSignals: {yet: 'more', info: 1},
  perBuyerSignals: {$3: {even: 'more', x: 4.5}}
}
)",
                url::Origin::Create(seller_logic_url), seller_logic_url.spec(),
                url::Origin::Create(bidder_url))));
  // Reporting urls should be fetched after an auction succeeded.
  WaitForURL(https_server_->GetURL("/echoall?report_seller"));
  WaitForURL(https_server_->GetURL("/echoall?report_bidder"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWithWinnerManyInterestGroups) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_TRUE(JoinInterestGroupAndWait(
      blink::mojom::InterestGroup::New(
          /* expiry */ base::Time(),
          /* owner= */ url::Origin::Create(test_url.GetOrigin()),
          /* name = */ "cars",
          /* bidding_url = */
          https_server_->GetURL(
              "a.test",
              "/interest_group/bidding_logic_stop_bidding_after_win.js"),
          /* update_url  = */ base::nullopt,
          /* trusted_bidding_signals_url = */ base::nullopt,
          /* trusted_bidding_signals_keys = */ base::nullopt,
          /* user_bidding_signals = */ "{some: 'json', data: {here: [1, 2]}}",
          /* ads = */ base::nullopt),
      "[{renderUrl : 'https://stop_bidding_after_win.com/render'}]"));
  EXPECT_TRUE(JoinInterestGroupAndWait(
      blink::mojom::InterestGroup::New(
          /* expiry */ base::Time(),
          /* owner= */ url::Origin::Create(test_url.GetOrigin()),
          /* name = */ "bikes",
          /* bidding_url = */
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /* update_url  = */ base::nullopt,
          /* trusted_bidding_signals_url = */
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /* trusted_bidding_signals_keys = */ base::nullopt,
          /* user_bidding_signals = */ "{some: 'json', data: {here: [1, 2]}}",
          /* ads = */ base::nullopt),
      "[{renderUrl : 'https://example.com/render'}]", "['key1']"));
  EXPECT_TRUE(JoinInterestGroupAndWait(
      blink::mojom::InterestGroup::New(
          /* expiry */ base::Time(),
          /* owner= */ url::Origin::Create(test_url.GetOrigin()),
          /* name = */ "shoes",
          /* bidding_url = */
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /* update_url  = */ base::nullopt,
          /* trusted_bidding_signals_url = */ base::nullopt,
          /* trusted_bidding_signals_keys = */ base::nullopt,
          /* user_bidding_signals = */ "{some: 'json', data: {here: [1, 2]}}",
          /* ads = */ base::nullopt),
      "[{renderUrl : 'https://example.com/render2'}]"));

  EXPECT_EQ(
      "https://stop_bidding_after_win.com/render",
      RunAuctionAndWait(JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1, $3],
  })",
          test_url.GetOrigin().spec(),
          https_server_->GetURL("a.test", "/interest_group/decision_logic.js")
              .spec())));
  // Seller and winning bidder should get reports, and other bidders shouldn't
  // get reports.
  WaitForURL(https_server_->GetURL("/echoall?report_seller"));
  WaitForURL(
      https_server_->GetURL("/echoall?report_bidder_stop_bidding_after_win"));
  base::AutoLock auto_lock(requests_lock_);
  EXPECT_FALSE(base::Contains(received_https_test_server_requests_,
                              https_server_->GetURL("/echoall?report_bidder")));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionMultipleAuctions) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  std::string ads =
      "[{renderUrl : 'https://stop_bidding_after_win.com/render',"
      "metadata : {ad:'metadata', here : [ 1, 2 ]}}]";
  // This group will win if it has never won an auction.
  EXPECT_TRUE(JoinInterestGroupAndWait(
      blink::mojom::InterestGroup::New(
          /* expiry */ base::Time(),
          /* owner= */ url::Origin::Create(test_url.GetOrigin()),
          /* name = */ "cars",
          /* bidding_url = */
          https_server_->GetURL(
              "a.test",
              "/interest_group/bidding_logic_stop_bidding_after_win.js"),
          /* update_url  = */ base::nullopt,
          /* trusted_bidding_signals_url = */ base::nullopt,
          /* trusted_bidding_signals_keys = */ base::nullopt,
          /* user_bidding_signals = */ "{some: 'json', data: {here: [1, 2]}}",
          /* ads = */ base::nullopt),
      ads));

  GURL test_url2 = https_server_->GetURL("b.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url2));
  // This group will win if the other interest group has won an auction.
  EXPECT_TRUE(JoinInterestGroupAndWait(
      blink::mojom::InterestGroup::New(
          /* expiry */ base::Time(),
          /* owner= */ url::Origin::Create(test_url2.GetOrigin()),
          /* name = */ "shoes",
          /* bidding_url = */
          https_server_->GetURL("b.test", "/interest_group/bidding_logic.js"),
          /* update_url  = */ base::nullopt,
          /* trusted_bidding_signals_url = */ base::nullopt,
          /* trusted_bidding_signals_keys = */ base::nullopt,
          /* user_bidding_signals = */ "{some: 'json', data: {here: [1, 2]}}",
          /* ads = */ base::nullopt),
      "[{renderUrl : 'https://example.com/render'}]"));

  // Both owners have one interest group in storage, and both interest groups
  // have no `prev_wins`.
  const url::Origin origin = url::Origin::Create(test_url);
  const url::Origin origin2 = url::Origin::Create(test_url2);
  std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>
      bidding_interest_groups = GetInterestGroupsForOwner(origin);
  EXPECT_EQ(bidding_interest_groups.size(), 1u);
  EXPECT_EQ(bidding_interest_groups.front()->signals->prev_wins.size(), 0u);
  EXPECT_EQ(bidding_interest_groups.front()->signals->bid_count, 0);
  std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>
      bidding_interest_groups2 = GetInterestGroupsForOwner(origin2);
  EXPECT_EQ(bidding_interest_groups2.size(), 1u);
  EXPECT_EQ(bidding_interest_groups2.front()->signals->prev_wins.size(), 0u);
  EXPECT_EQ(bidding_interest_groups2.front()->signals->bid_count, 0);

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1, $3],
  })",
      test_url2.GetOrigin().spec(),
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js")
          .spec(),
      test_url.GetOrigin().spec());
  // Run an ad auction. Interest group cars of owner `test_url` wins.
  EXPECT_EQ("https://stop_bidding_after_win.com/render",
            RunAuctionAndWait(auction_config));
  // `prev_wins` of `test_url`'s interest group cars is updated in storage.
  bidding_interest_groups = GetInterestGroupsForOwner(origin);
  bidding_interest_groups2 = GetInterestGroupsForOwner(origin2);
  EXPECT_EQ(bidding_interest_groups.front()->signals->prev_wins.size(), 1u);
  EXPECT_EQ(bidding_interest_groups2.front()->signals->prev_wins.size(), 0u);
  EXPECT_EQ(
      bidding_interest_groups.front()->signals->prev_wins.front()->ad_json,
      R"({"render_url":"https://stop_bidding_after_win.com/render","metadata":{"ad":"metadata","here":[1,2]}})");
  EXPECT_EQ(bidding_interest_groups.front()->signals->bid_count, 1);
  EXPECT_EQ(bidding_interest_groups2.front()->signals->bid_count, 1);

  // Run auction again. Interest group shoes of owner `test_url2` wins.
  EXPECT_EQ("https://example.com/render", RunAuctionAndWait(auction_config));
  // `test_url2`'s interest group shoes has one `prev_wins` in storage.
  bidding_interest_groups = GetInterestGroupsForOwner(origin);
  bidding_interest_groups2 = GetInterestGroupsForOwner(origin2);
  EXPECT_EQ(bidding_interest_groups.front()->signals->prev_wins.size(), 1u);
  EXPECT_EQ(bidding_interest_groups2.front()->signals->prev_wins.size(), 1u);
  EXPECT_EQ(
      bidding_interest_groups2.front()->signals->prev_wins.front()->ad_json,
      R"({"render_url":"https://example.com/render"})");
  EXPECT_EQ(bidding_interest_groups.front()->signals->bid_count, 2);
  EXPECT_EQ(bidding_interest_groups2.front()->signals->bid_count, 2);

  // Run auction third time, and only interest group "shoes" bids this time.
  EXPECT_EQ(
      "https://example.com/render",
      RunAuctionAndWait(JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
  })",
          test_url2.GetOrigin().spec(),
          https_server_->GetURL("b.test", "/interest_group/decision_logic.js")
              .spec())));
  // `test_url2`'s interest group shoes has two `prev_wins` in storage.
  bidding_interest_groups = GetInterestGroupsForOwner(origin);
  bidding_interest_groups2 = GetInterestGroupsForOwner(origin2);
  EXPECT_EQ(bidding_interest_groups.front()->signals->prev_wins.size(), 1u);
  EXPECT_EQ(bidding_interest_groups2.front()->signals->prev_wins.size(), 2u);
  EXPECT_EQ(
      bidding_interest_groups2.front()->signals->prev_wins.back()->ad_json,
      R"({"render_url":"https://example.com/render"})");
  EXPECT_EQ(bidding_interest_groups.front()->signals->bid_count, 2);
  EXPECT_EQ(bidding_interest_groups2.front()->signals->bid_count, 3);
}

// The winning ad's render url is invalid (invalid url or has http scheme).
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionWithInvalidAdUrl) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  std::string ads =
      "[{renderUrl : 'https://shoes.com/render',"
      "metadata : {ad:'metadata', here : [ 1, 2 ]}}]";
  EXPECT_TRUE(JoinInterestGroupAndWait(
      blink::mojom::InterestGroup::New(
          /* expiry */ base::Time(),
          /* owner= */ url::Origin::Create(test_url.GetOrigin()),
          /* name = */ "cars",
          /* bidding_url = */
          https_server_->GetURL(
              "a.test", "/interest_group/bidding_logic_invalid_ad_url.js"),
          /* update_url  = */ base::nullopt,
          /* trusted_bidding_signals_url = */ base::nullopt,
          /* trusted_bidding_signals_keys = */ base::nullopt,
          /* user_bidding_signals = */ "{some: 'json', data: {here: [1, 2]}}",
          /* ads = */ base::nullopt),
      ads));

  EXPECT_EQ(
      nullptr,
      RunAuctionAndWait(JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
  })",
          test_url.GetOrigin().spec(),
          https_server_->GetURL("a.test", "/interest_group/decision_logic.js")
              .spec())));
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
