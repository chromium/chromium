// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/interest_group/ad_auction_service_impl.h"
#include "content/browser/interest_group/interest_group_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"
#include "net/base/isolation_info.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::testing::Eq;
using ::testing::Optional;

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

// Allows registering responses to network requests.
class NetworkResponder {
 public:
  explicit NetworkResponder(net::EmbeddedTestServer& server) {
    server.RegisterRequestHandler(base::BindRepeating(
        &NetworkResponder::RequestHandler, base::Unretained(this)));
  }

  void RegisterNetworkResponse(
      const std::string& url_path,
      const std::string& body,
      const std::string& mime_type = "application/json") {
    base::AutoLock auto_lock(response_map_lock_);
    Response response;
    response.body = body;
    response.mime_type = mime_type;
    response_map_[url_path] = response;
  }

 private:
  struct Response {
    std::string body;
    std::string mime_type;
  };

  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    base::AutoLock auto_lock(response_map_lock_);
    const auto it = response_map_.find(request.GetURL().path());
    if (it == response_map_.end())
      return nullptr;
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->AddCustomHeader("X-Allow-FLEDGE", "true");
    response->set_code(net::HTTP_OK);
    response->set_content(it->second.body);
    response->set_content_type(it->second.mime_type);
    return std::move(response);
  }

  // EmbeddedTestServer RequestHandlers can't be added after the server has
  // started, but tests may want to specify network responses after the server
  // starts in the fixture. A handler is therefore registered that uses
  // `response_map_` to serve network responses.
  base::Lock response_map_lock_;

  // For each HTTPS request, we see if any path in the map matches the request
  // path. If so, the server returns the mapped value string as the response.
  base::flat_map<std::string, Response> response_map_
      GUARDED_BY(response_map_lock_);
};

class InterestGroupBrowserTest : public ContentBrowserTest {
 public:
  InterestGroupBrowserTest() {
    feature_list_.InitWithFeatures(
        /*`enabled_features`=*/
        {blink::features::kInterestGroupStorage,
         blink::features::kAdInterestGroupAPI, blink::features::kParakeet,
         blink::features::kFledge},
        /*disabled_features=*/
        {blink::features::kFencedFrames});
  }

  ~InterestGroupBrowserTest() override {
    if (old_content_browser_client_)
      SetBrowserClientForTesting(old_content_browser_client_);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->AddDefaultHandlers(GetTestDataFilePath());
    https_server_->RegisterRequestMonitor(base::BindRepeating(
        &InterestGroupBrowserTest::OnHttpsTestServerRequestMonitor,
        base::Unretained(this)));
    network_responder_ = std::make_unique<NetworkResponder>(*https_server_);
    ASSERT_TRUE(https_server_->Start());
    manager_ =
        static_cast<StoragePartitionImpl*>(shell()
                                               ->web_contents()
                                               ->GetBrowserContext()
                                               ->GetDefaultStoragePartition())
            ->GetInterestGroupManager();
    content_browser_client_.SetAllowList(
        {url::Origin::Create(https_server_->GetURL("a.test", "/")),
         url::Origin::Create(https_server_->GetURL("b.test", "/")),
         url::Origin::Create(https_server_->GetURL("c.test", "/")),
         // HTTP origins like those below aren't supported for FLEDGE -- some
         // tests verify that HTTP origins are rejected, even if somehow they
         // are allowed by the allowlist.
         url::Origin::Create(embedded_test_server()->GetURL("a.test", "/")),
         url::Origin::Create(embedded_test_server()->GetURL("b.test", "/")),
         url::Origin::Create(embedded_test_server()->GetURL("c.test", "/"))});
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
  bool JoinInterestGroupInJS(const blink::InterestGroup& group,
                             const std::string& ads = std::string(),
                             const std::string& trusted_bidding_signals_keys =
                                 std::string()) WARN_UNUSED_RESULT {
    // TODO(qingxin): Use base::Value to replace ostringstream.
    std::ostringstream buf;
    buf << "{"
        << "name: '" << group.name << "', "
        << "owner: '" << group.owner << "'";
    if (group.bidding_url) {
      buf << ", biddingLogicUrl: '" << *group.bidding_url << "'";
    }
    if (group.update_url) {
      buf << ", dailyUpdateUrl: '" << *group.update_url << "'";
    }
    if (group.trusted_bidding_signals_url) {
      buf << ", trustedBiddingSignalsUrl: '"
          << *group.trusted_bidding_signals_url << "'";
    }
    if (group.user_bidding_signals) {
      buf << ", userBiddingSignals: " << group.user_bidding_signals.value();
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
    manager_->GetAllInterestGroupOwners(base::BindLambdaForTesting(
        [&run_loop, &interest_group_owners](std::vector<url::Origin> owners) {
          interest_group_owners = std::move(owners);
          run_loop.Quit();
        }));
    run_loop.Run();
    return interest_group_owners;
  }

  std::vector<BiddingInterestGroup> GetInterestGroupsForOwner(
      const url::Origin& owner) {
    std::vector<BiddingInterestGroup> interest_groups;
    base::RunLoop run_loop;
    manager_->GetInterestGroupsForOwner(
        owner, base::BindLambdaForTesting(
                   [&run_loop, &interest_groups](
                       std::vector<BiddingInterestGroup> groups) {
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
        interest_groups.emplace_back(interest_group.group->group.owner,
                                     interest_group.group->group.name);
      }
    }
    return interest_groups;
  }

  int GetJoinCount(const url::Origin& owner, const std::string& name) {
    for (const auto& interest_group : GetInterestGroupsForOwner(owner)) {
      if (interest_group.group->group.name == name) {
        return interest_group.group->signals->join_count;
      }
    }
    return 0;
  }

  bool JoinInterestGroupAndWaitInJs(const url::Origin& owner,
                                    const std::string& name)
      WARN_UNUSED_RESULT {
    int initial_count = GetJoinCount(owner, name);
    if (!JoinInterestGroupInJS(owner, name)) {
      return false;
    }
    while (GetJoinCount(owner, name) != initial_count + 1) {
    }

    return true;
  }

  bool JoinInterestGroupAndWaitInJs(
      const blink::InterestGroup& group,
      const std::string& ads = std::string(),
      const std::string& trusted_bidding_signals_keys = std::string())
      WARN_UNUSED_RESULT {
    int initial_count = GetJoinCount(group.owner, group.name);
    if (!JoinInterestGroupInJS(group, ads, trusted_bidding_signals_keys)) {
      return false;
    }
    while (GetJoinCount(group.owner, group.name) != initial_count + 1) {
    }

    return true;
  }

  bool LeaveInterestGroupAndWait(const url::Origin& owner,
                                 const std::string& name) {
    if (!LeaveInterestGroupInJS(owner, name)) {
      return false;
    }
    while (GetJoinCount(owner, name) != 0) {
    }
    return true;
  }

  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  content::EvalJsResult RunAuctionAndWait(
      const std::string& auction_config_json,
      const absl::optional<ToRenderFrameHost> execution_target = absl::nullopt)
      WARN_UNUSED_RESULT {
    return EvalJs(execution_target ? *execution_target : shell(),
                  base::StringPrintf(
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

  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  content::EvalJsResult CreateAdRequestAndWait(
      const absl::optional<ToRenderFrameHost> execution_target = absl::nullopt)
      WARN_UNUSED_RESULT {
    return EvalJs(execution_target ? *execution_target : shell(),
                  R"(
(async function() {
  try {
    return await navigator.createAdRequest();
  } catch (e) {
    return e.toString();
  }
})())");
  }

  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  content::EvalJsResult FinalizeAdAndWait(
      const absl::optional<ToRenderFrameHost> execution_target = absl::nullopt)
      WARN_UNUSED_RESULT {
    return EvalJs(execution_target ? *execution_target : shell(),
                  R"(
(async function() {
  try {
    return await navigator.finalizeAd();
  } catch (e) {
    return e.toString();
  }
})())");
  }

  // Waits until the `condition` callback over the interest groups returns true.
  void WaitForInterestGroupsSatisfying(
      const url::Origin& owner,
      base::RepeatingCallback<bool(const std::vector<BiddingInterestGroup>&)>
          condition) {
    while (true) {
      if (condition.Run(GetInterestGroupsForOwner(owner)))
        break;
    }
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

  void ClearReceivedRequests() {
    base::AutoLock auto_lock(requests_lock_);
    received_https_test_server_requests_.clear();
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;
  AllowlistedOriginContentBrowserClient content_browser_client_;
  ContentBrowserClient* old_content_browser_client_;
  InterestGroupManager* manager_;
  base::Lock requests_lock_;
  std::set<GURL> received_https_test_server_requests_
      GUARDED_BY(requests_lock_);
  std::unique_ptr<base::RunLoop> request_run_loop_;
  GURL wait_for_url_ GUARDED_BY(requests_lock_);
  std::unique_ptr<NetworkResponder> network_responder_;
};

// At the moment, InterestGroups use URN urls when fenced frames are enabled,
// and normal URLs when not. This means they require ads be loaded in fenced
// frames when Chrome is running with the option enabled.
class InterestGroupFencedFrameBrowserTest
    : public InterestGroupBrowserTest,
      public ::testing::WithParamInterface<
          blink::features::FencedFramesImplementationType> {
 public:
  InterestGroupFencedFrameBrowserTest() {
    // Tests are run with both the ShadowDOM and MPArch ("Multi-Page
    // Architecture") fenced frames implementations.
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames,
        {{"implementation_type", GetFencedFrameFeatureParam()}});
  }

  const char* GetFencedFrameFeatureParam() const {
    switch (GetParam()) {
      case blink::features::FencedFramesImplementationType::kShadowDOM:
        return "shadow_dom";
      case blink::features::FencedFramesImplementationType::kMPArch:
        return "mparch";
    }
  }

  ~InterestGroupFencedFrameBrowserTest() override = default;

  // Runs the specified auction using RunAuctionAndWait(), expecting a success
  // resulting in a URN URL. Then navigates a pre-existing fenced frame to that
  // URL, expecting `expected_ad_url` to be loaded in the fenced frame.
  //
  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  //
  // The target must already contain a single fenced frame.
  void RunAuctionAndNavigateFencedFrame(
      const GURL& expected_ad_url,
      const std::string& auction_config_json,
      absl::optional<ToRenderFrameHost> execution_target = absl::nullopt) {
    if (!execution_target)
      execution_target = shell();
    content::EvalJsResult urn_url_string =
        RunAuctionAndWait(auction_config_json, execution_target);
    ASSERT_TRUE(urn_url_string.value.is_string())
        << "Expected string, but got " << urn_url_string.value;

    GURL urn_url(urn_url_string.ExtractString());
    ASSERT_TRUE(urn_url.is_valid())
        << "URL is not valid: " << urn_url_string.ExtractString();

    EXPECT_EQ(url::kUrnScheme, urn_url.scheme_piece());

    NavigateFencedFrameAndWait(urn_url, expected_ad_url, *execution_target);
  }

  // Navigates the only fenced frame in `execution_target` to `url` and waits
  // for the navigation to complete, expecting the frame to navigate to
  // `expected_url` and make a network request for it.
  void NavigateFencedFrameAndWait(const GURL& url,
                                  const GURL& expected_url,
                                  const ToRenderFrameHost& execution_target) {
    // Use to wait for navigation completion in the ShadowDOM case only.
    // Harmlessly created but not used in the MPArch case.
    TestFrameNavigationObserver observer(
        GetFencedFrameRenderFrameHost(execution_target));

    EXPECT_TRUE(
        ExecJs(execution_target,
               JsReplace("document.querySelector('fencedframe').src = $1;",
                         url.spec())));

    // Wait for the URL to be requested, to make sure the fenced frame has
    // started loading. Used in both ShadowDOM and MPArch cases, but it's only
    // needed in the MPArch case. On regression, this is likely to hang.
    GURL::Replacements replacements;
    replacements.SetHostStr("127.0.0.1");
    WaitForURL(expected_url.ReplaceComponents(replacements));

    switch (GetParam()) {
      case blink::features::FencedFramesImplementationType::kShadowDOM: {
        observer.Wait();
        break;
      }
      case blink::features::FencedFramesImplementationType::kMPArch: {
        // Wait for the load to complete.
        FencedFrame* fenced_frame = GetFencedFrame(execution_target);
        fenced_frame->WaitForDidStopLoadingForTesting();
      }
    }

    EXPECT_EQ(
        expected_url,
        GetFencedFrameRenderFrameHost(execution_target)->GetLastCommittedURL());
  }

  // Returns the RenderFrameHost for a fenced frame in `execution_target`, which
  // is assumed to contain only one fenced frame and no iframes.
  RenderFrameHost* GetFencedFrameRenderFrameHost(
      const ToRenderFrameHost& execution_target) {
    switch (GetParam()) {
      case blink::features::FencedFramesImplementationType::kShadowDOM: {
        // Make sure there's only one child frame.
        CHECK(!ChildFrameAt(execution_target, 1));

        return ChildFrameAt(execution_target, 0);
      }
      case blink::features::FencedFramesImplementationType::kMPArch: {
        return GetFencedFrame(execution_target)->GetInnerRoot();
      }
    }
  }

  // Returns FencedFrame in `execution_target` frame. Requires that
  // `execution_target` have one and only one FencedFrame. MPArch only, as the
  // ShadowDOM implementation doesn't use the FencedFrame class.
  FencedFrame* GetFencedFrame(const ToRenderFrameHost& execution_target) {
    CHECK_EQ(GetParam(),
             blink::features::FencedFramesImplementationType::kMPArch);

    std::vector<FencedFrame*> fenced_frames =
        static_cast<RenderFrameHostImpl*>(execution_target.render_frame_host())
            ->GetFencedFrames();
    CHECK_EQ(1u, fenced_frames.size());
    return fenced_frames[0];
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    InterestGroupFencedFrameBrowserTest,
    ::testing::Values(
        blink::features::FencedFramesImplementationType::kShadowDOM,
        blink::features::FencedFramesImplementationType::kMPArch));

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, JoinLeaveInterestGroup) {
  GURL test_url_a = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin_a = url::Origin::Create(test_url_a);
  ASSERT_TRUE(test_url_a.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));

  // This join should succeed and be added to the database.
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(test_origin_a, "cars"));

  // This join should fail and throw an exception since a.test is not the same
  // origin as foo.a.test.
  EXPECT_FALSE(JoinInterestGroupInJS(
      url::Origin::Create(GURL("https://foo.a.test")), "cars"));

  // This join should fail and throw an exception since a.test is not the same
  // origin as the bidding_url, bid.a.test.
  EXPECT_FALSE(JoinInterestGroupInJS(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin_a,
      /*name=*/"bicycles",
      /*bidding_url=*/GURL("https://bid.a.test"),
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/absl::nullopt,
      /*user_bidding_signals=*/absl::nullopt,
      /*ads=*/absl::nullopt,
      /*ad_components=*/absl::nullopt)));

  // This join should fail and throw an exception since a.test is not the same
  // origin as the update_url, update.a.test.
  EXPECT_FALSE(JoinInterestGroupInJS(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin_a,
      /*name=*/"tricycles",
      /*bidding_url=*/absl::nullopt,
      /*update_url=*/GURL("https://update.a.test"),
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/absl::nullopt,
      /*user_bidding_signals=*/absl::nullopt,
      /*ads=*/absl::nullopt,
      /*ad_components=*/absl::nullopt)));

  // This join should fail and throw an exception since a.test is not the same
  // origin as the trusted_bidding_signals_url, signals.a.test.
  EXPECT_FALSE(JoinInterestGroupInJS(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin_a,
      /*name=*/"four-wheelers",
      /*bidding_url=*/absl::nullopt,
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/GURL("https://signals.a.test"),
      /*trusted_bidding_signals_keys=*/absl::nullopt,
      /*user_bidding_signals=*/absl::nullopt,
      /*ads=*/absl::nullopt,
      /*ad_components=*/absl::nullopt)));

  // This join should silently fail since d.test is not allowlisted for the API,
  // and allowlist checks only happen in the browser process, so don't throw an
  // exception.
  GURL test_url_d = https_server_->GetURL("d.test", "/echo");
  url::Origin test_origin_d = url::Origin::Create(test_url_d);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_d));
  EXPECT_TRUE(JoinInterestGroupInJS(test_origin_d, "toys"));

  // Another successful join.
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(test_origin_b, "trucks"));

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
  manager_->JoinInterestGroup(
      blink::InterestGroup(
          /*expiry=*/base::Time::Now() + base::Seconds(300),
          /*owner=*/test_origin_d,
          /*name=*/"candy",
          /*bidding_url=*/absl::nullopt,
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/absl::nullopt,
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
      test_origin_d.GetURL());

  ASSERT_TRUE(NavigateToURL(shell(), test_url_d));
  // This leave should do nothing because origin_d is not allowed by privacy
  // sandbox.
  EXPECT_TRUE(LeaveInterestGroupInJS(test_origin_d, "candy"));

  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));
  // This leave should do nothing because there is not interest group of that
  // name.
  EXPECT_TRUE(LeaveInterestGroupInJS(test_origin_b, "cars"));

  // This leave should silently fail because it is cross-origin.
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
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

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
                       JoinInterestGroupOwnerDoesntMatchFrame) {
  const GURL page_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), page_url));

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "owner 'https://test.com' for AuctionAdInterestGroup with name "
          "'cars' match frame origin '%s'.",
          url::Origin::Create(page_url).Serialize().c_str()),
      EvalJs(shell(), R"(
(function() {
  try {
    navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: 'https://test.com',
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
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "biddingLogicUrl 'https://invalid^&' for AuctionAdInterestGroup with "
          "owner '%s' and name 'cars' cannot be resolved to a valid URL.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(function() {
  try {
    navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          biddingLogicUrl: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidDailyUpdateUrl) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "dailyUpdateUrl 'https://invalid^&' for AuctionAdInterestGroup with "
          "owner '%s' and name 'cars' cannot be resolved to a valid URL.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(function() {
  try {
    navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          dailyUpdateUrl: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidTrustedBiddingSignalsUrl) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(base::StringPrintf(
                "TypeError: Failed to execute 'joinAdInterestGroup' on "
                "'Navigator': trustedBiddingSignalsUrl 'https://invalid^&' for "
                "AuctionAdInterestGroup with owner '%s' and name 'cars' cannot "
                "be resolved to a valid URL.",
                origin_string.c_str()),
            EvalJs(shell(), JsReplace(R"(
(function() {
  try {
    navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          trustedBiddingSignalsUrl: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                      origin_string.c_str())));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidUserBiddingSignals) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "userBiddingSignals for AuctionAdInterestGroup with owner '%s' and "
          "name 'cars' must be a JSON-serializable object.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(function() {
  try {
    navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          userBiddingSignals: function() {},
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidAdUrl) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "ad renderUrl 'https://invalid^&' for AuctionAdInterestGroup with "
          "owner '%s' and name 'cars' cannot be resolved to a valid URL.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(function() {
  try {
    navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          ads: [{renderUrl:"https://invalid^&"}],
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidAdMetadata) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on "
          "'Navigator': ad metadata for AuctionAdInterestGroup with "
          "owner '%s' and name 'cars' must be a JSON-serializable object.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(function() {
  let x = {};
  let y = {};
  x.a = y;
  y.a = x;
  try {
    navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          ads: [{renderUrl:"https://test.com", metadata:x}],
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       LeaveInterestGroupInvalidOwner) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

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
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': seller "
      "'https://invalid^&' for AuctionAdConfig must be a valid https origin.",
      RunAuctionAndWait(R"({
      seller: 'https://invalid^&',
      decisionLogicUrl: 'https://test.com/decision_logic'
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionHttpSeller) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

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
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

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
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/url::Origin::Create(test_url.GetOrigin()),
          /*name=*/"cars",
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
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
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

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
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

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
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

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
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

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
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

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
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/url::Origin::Create(test_url.GetOrigin()),
          /*name=*/"cars",
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
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
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

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
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/url::Origin::Create(test_url_a),
          /*name=*/"cars",
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
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
  blink::InterestGroup disabled_group;
  disabled_group.expiry = base::Time::Now() + base::Seconds(300);
  disabled_group.owner = url::Origin::Create(disabled_domain);
  disabled_group.name = "candy";
  disabled_group.bidding_url = https_server_->GetURL(
      disabled_domain.host(),
      "/interest_group/bidding_logic_stop_bidding_after_win.js");
  disabled_group.ads.emplace();
  disabled_group.ads->emplace_back(blink::InterestGroup::Ad(
      GURL("https://stop_bidding_after_win.com/render"), absl::nullopt));
  manager_->JoinInterestGroup(std::move(disabled_group), disabled_domain);
  ASSERT_EQ(1, GetJoinCount(url::Origin::Create(disabled_domain), "candy"));

  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  std::string ads =
      "[{renderUrl : 'https://example.com/render',"
      "metadata : {ad:'metadata', here : [ 1, 2 ]}}]";
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/url::Origin::Create(test_url.GetOrigin()),
          /*name=*/"cars",
          /*bidding_url=*/
          https_server_->GetURL(test_url.host(),
                                "/interest_group/bidding_logic.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL(test_url.host(),
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
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
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/url::Origin::Create(test_url.GetOrigin()),
          /*name=*/"cars",
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
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
       "application/javascript", /*expect_trusted_params=*/true},
      {https_server_->GetURL(
           "a.test",
           "/interest_group/"
           "trusted_bidding_signals.json?hostname=a.test&keys=key1"),
       "application/json", /*expect_trusted_params=*/true},
      {https_server_->GetURL("a.test", "/interest_group/decision_logic.js"),
       "application/javascript", /*expect_trusted_params=*/false},
  };
  for (const auto& expected_request : kExpectedRequests) {
    SCOPED_TRACE(expected_request.url);

    absl::optional<network::ResourceRequest> request =
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

    absl::optional<network::ResourceRequest> request =
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

// Runs auction just like the above test, but runs with fenced frames enabled
// and expects to receive a URN URL to be used. After the auction, loads the URL
// in a fenced frame, and expects the correct URL is loaded.
IN_PROC_BROWSER_TEST_P(InterestGroupFencedFrameBrowserTest,
                       RunAdAuctionWithWinner) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/fenced_frames/basic.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GURL ad_url = https_server_->GetURL("c.test", "/echo");
  std::string ads = base::StringPrintf(
      "[{renderUrl : '%s',"
      "metadata : {ad:'metadata', here : [ 1, 2 ]}}]",
      ad_url.spec().c_str());
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/url::Origin::Create(test_url.GetOrigin()),
          /*name=*/"cars",
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
      ads, "['key1']"));

  ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
      ad_url,
      JsReplace(
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
       "application/javascript", /*expect_trusted_params=*/true},
      {https_server_->GetURL(
           "a.test",
           "/interest_group/"
           "trusted_bidding_signals.json?hostname=a.test&keys=key1"),
       "application/json", /*expect_trusted_params=*/true},
      {https_server_->GetURL("a.test", "/interest_group/decision_logic.js"),
       "application/javascript", /*expect_trusted_params=*/false},
  };
  for (const auto& expected_request : kExpectedRequests) {
    SCOPED_TRACE(expected_request.url);

    absl::optional<network::ResourceRequest> request =
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

    absl::optional<network::ResourceRequest> request =
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
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/url::Origin::Create(bidder_url.GetOrigin()),
          /*name=*/"cars",
          /*bidding_url=*/
          https_server_->GetURL(kBidder, "/interest_group/bidding_logic.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL(kBidder,
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
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

// Make sure correct topFrameHostname is passed in. Check auctions from top
// frames, and iframes of various depth.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, TopFrameHostname) {
  // Buyer, seller, and iframe all use the same host.
  const char kOtherHost[] = "b.test";
  // Top frame host is unique.
  const char kTopFrameHost[] = "a.test";

  // Navigate to bidder site, and add an interest group.
  GURL other_url = https_server_->GetURL(kOtherHost, "/echo");
  url::Origin other_origin = url::Origin::Create(other_url);
  ASSERT_TRUE(NavigateToURL(shell(), other_url));
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/other_origin,
          /*name=*/"cars",
          /*bidding_url=*/
          https_server_->GetURL(
              kOtherHost,
              "/interest_group/bidding_logic_expect_top_frame_a_test.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/absl::nullopt,
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
      "[{renderUrl : 'https://example.com/render'}]"));

  const struct {
    int depth;
    std::string top_frame_path;
    const char* seller_path;
  } kTestCases[] = {
      {0, "/echo",
       "/interest_group/decision_logic_expect_top_frame_a_test_cross_site.js"},
      {1,
       base::StringPrintf("/cross_site_iframe_factory.html?a.test(%s)",
                          kOtherHost),
       "/interest_group/decision_logic_expect_top_frame_a_test.js"},
      {2,
       base::StringPrintf("/cross_site_iframe_factory.html?a.test(%s(%s))",
                          kOtherHost, kOtherHost),
       "/interest_group/decision_logic_expect_top_frame_a_test.js"},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.depth);

    // Navigate to publisher, with the cross-site iframe..
    ASSERT_TRUE(NavigateToURL(
        shell(),
        https_server_->GetURL(kTopFrameHost, test_case.top_frame_path)));

    RenderFrameHost* frame = shell()->web_contents()->GetMainFrame();
    EXPECT_EQ(https_server_->GetOrigin(kTopFrameHost),
              frame->GetLastCommittedOrigin());
    for (int i = 0; i < test_case.depth; ++i) {
      frame = ChildFrameAt(frame, 0);
      ASSERT_TRUE(frame);
      EXPECT_EQ(other_origin, frame->GetLastCommittedOrigin());
    }

    // Run auction with a seller script with an "Access-Control-Allow-Origin"
    // header. The auction should succeed.
    GURL seller_logic_url =
        https_server_->GetURL(kOtherHost, test_case.seller_path);
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
                                    url::Origin::Create(seller_logic_url),
                                    seller_logic_url.spec(), other_origin),
                                frame));

    // Reporting urls should be fetched after an auction succeeded.
    WaitForURL(https_server_->GetURL("/echoall?report_seller"));
    WaitForURL(https_server_->GetURL("/echoall?report_bidder"));
    ClearReceivedRequests();
  }
}

// Make sure correct topFrameHostname is passed in. Check auctions from top
// frames, and iframes of various depth. Also test running auctions in
// cross-site iframes, and loading them into those iframes' fenced frames.
IN_PROC_BROWSER_TEST_P(InterestGroupFencedFrameBrowserTest, TopFrameHostname) {
  // Buyer, seller, and iframe all use the same host.
  const char kOtherHost[] = "b.test";
  // Top frame host is unique.
  const char kTopFrameHost[] = "a.test";

  // Navigate to bidder site, and add an interest group.
  GURL other_url = https_server_->GetURL(kOtherHost, "/echo");
  url::Origin other_origin = url::Origin::Create(other_url);
  ASSERT_TRUE(NavigateToURL(shell(), other_url));

  GURL ad_url = https_server_->GetURL("c.test", "/echo");
  std::string ads = base::StringPrintf(
      "[{renderUrl : '%s',"
      "metadata : {ad:'metadata', here : [ 1, 2 ]}}]",
      ad_url.spec().c_str());
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/other_origin,
          /*name=*/"cars",
          /*bidding_url=*/
          https_server_->GetURL(
              kOtherHost,
              "/interest_group/bidding_logic_expect_top_frame_a_test.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/absl::nullopt,
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
      ads));

  const struct {
    int depth;
    std::string top_frame_path;
    const char* seller_path;
  } kTestCases[] = {
      {0, "/fenced_frames/basic.html",
       "/interest_group/decision_logic_expect_top_frame_a_test_cross_site.js"},
      {1,
       base::StringPrintf(
           "/cross_site_iframe_factory.html?a.test(%s)",
           https_server_->GetURL(kOtherHost, "/fenced_frames/basic.html")
               .spec()
               .c_str()),
       "/interest_group/decision_logic_expect_top_frame_a_test.js"},
      {2,
       base::StringPrintf(
           "/cross_site_iframe_factory.html?a.test(%s(%s))", kOtherHost,
           https_server_->GetURL(kOtherHost, "/fenced_frames/basic.html")
               .spec()
               .c_str()),
       "/interest_group/decision_logic_expect_top_frame_a_test.js"},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.depth);

    // Navigate to publisher, with the cross-site iframe..
    ASSERT_TRUE(NavigateToURL(
        shell(),
        https_server_->GetURL(kTopFrameHost, test_case.top_frame_path)));

    RenderFrameHost* frame = shell()->web_contents()->GetMainFrame();
    EXPECT_EQ(https_server_->GetOrigin(kTopFrameHost),
              frame->GetLastCommittedOrigin());
    for (int i = 0; i < test_case.depth; ++i) {
      frame = ChildFrameAt(frame, 0);
      ASSERT_TRUE(frame);
      EXPECT_EQ(other_origin, frame->GetLastCommittedOrigin());
    }

    // Run auction with a seller script with an "Access-Control-Allow-Origin"
    // header, if needed. The auction should succeed.
    GURL seller_logic_url =
        https_server_->GetURL(kOtherHost, test_case.seller_path);
    ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
        ad_url,
        JsReplace(
            R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$1],
  auctionSignals: {x: 1},
  sellerSignals: {yet: 'more', info: 1},
  perBuyerSignals: {$1: {even: 'more', x: 4.5}}
}
            )",
            other_origin.Serialize(), seller_logic_url.spec()),
        frame));

    // Reporting urls should be fetched after an auction succeeded.
    WaitForURL(https_server_->GetURL("/echoall?report_seller"));
    WaitForURL(https_server_->GetURL("/echoall?report_bidder"));
    ClearReceivedRequests();
  }
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWithWinnerManyInterestGroups) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/url::Origin::Create(test_url.GetOrigin()),
          /*name=*/"cars",
          /*bidding_url=*/
          https_server_->GetURL(
              "a.test",
              "/interest_group/bidding_logic_stop_bidding_after_win.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
      "[{renderUrl : 'https://stop_bidding_after_win.com/render'}]"));
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/url::Origin::Create(test_url.GetOrigin()),
          /*name=*/"bikes",
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
      "[{renderUrl : 'https://example.com/render'}]", "['key1']"));
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/url::Origin::Create(test_url.GetOrigin()),
          /*name=*/"shoes",
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
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
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/url::Origin::Create(test_url.GetOrigin()),
          /*name=*/"cars",
          /*bidding_url=*/
          https_server_->GetURL(
              "a.test",
              "/interest_group/bidding_logic_stop_bidding_after_win.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
      ads));

  GURL test_url2 = https_server_->GetURL("b.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url2));
  // This group will win if the other interest group has won an auction.
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/url::Origin::Create(test_url2.GetOrigin()),
          /*name=*/"shoes",
          /*bidding_url=*/
          https_server_->GetURL("b.test", "/interest_group/bidding_logic.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
      "[{renderUrl : 'https://example.com/render'}]"));

  // Both owners have one interest group in storage, and both interest groups
  // have no `prev_wins`.
  const url::Origin origin = url::Origin::Create(test_url);
  const url::Origin origin2 = url::Origin::Create(test_url2);
  std::vector<BiddingInterestGroup> bidding_interest_groups =
      GetInterestGroupsForOwner(origin);
  EXPECT_EQ(bidding_interest_groups.size(), 1u);
  EXPECT_EQ(bidding_interest_groups.front().group->signals->prev_wins.size(),
            0u);
  EXPECT_EQ(bidding_interest_groups.front().group->signals->bid_count, 0);
  std::vector<BiddingInterestGroup> bidding_interest_groups2 =
      GetInterestGroupsForOwner(origin2);
  EXPECT_EQ(bidding_interest_groups2.size(), 1u);
  EXPECT_EQ(bidding_interest_groups2.front().group->signals->prev_wins.size(),
            0u);
  EXPECT_EQ(bidding_interest_groups2.front().group->signals->bid_count, 0);

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
  EXPECT_EQ(bidding_interest_groups.front().group->signals->prev_wins.size(),
            1u);
  EXPECT_EQ(bidding_interest_groups2.front().group->signals->prev_wins.size(),
            0u);
  EXPECT_EQ(
      bidding_interest_groups.front()
          .group->signals->prev_wins.front()
          ->ad_json,
      R"({"render_url":"https://stop_bidding_after_win.com/render","metadata":{"ad":"metadata","here":[1,2]}})");
  EXPECT_EQ(bidding_interest_groups.front().group->signals->bid_count, 1);
  EXPECT_EQ(bidding_interest_groups2.front().group->signals->bid_count, 1);

  // Run auction again. Interest group shoes of owner `test_url2` wins.
  EXPECT_EQ("https://example.com/render", RunAuctionAndWait(auction_config));
  // `test_url2`'s interest group shoes has one `prev_wins` in storage.
  bidding_interest_groups = GetInterestGroupsForOwner(origin);
  bidding_interest_groups2 = GetInterestGroupsForOwner(origin2);
  EXPECT_EQ(bidding_interest_groups.front().group->signals->prev_wins.size(),
            1u);
  EXPECT_EQ(bidding_interest_groups2.front().group->signals->prev_wins.size(),
            1u);
  EXPECT_EQ(bidding_interest_groups2.front()
                .group->signals->prev_wins.front()
                ->ad_json,
            R"({"render_url":"https://example.com/render"})");
  // First interest group didn't bid this time.
  EXPECT_EQ(bidding_interest_groups.front().group->signals->bid_count, 1);
  EXPECT_EQ(bidding_interest_groups2.front().group->signals->bid_count, 2);

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
  EXPECT_EQ(bidding_interest_groups.front().group->signals->prev_wins.size(),
            1u);
  EXPECT_EQ(bidding_interest_groups2.front().group->signals->prev_wins.size(),
            2u);
  EXPECT_EQ(bidding_interest_groups2.front()
                .group->signals->prev_wins.back()
                ->ad_json,
            R"({"render_url":"https://example.com/render"})");
  // First interest group didn't bid this time.
  EXPECT_EQ(bidding_interest_groups.front().group->signals->bid_count, 1);
  EXPECT_EQ(bidding_interest_groups2.front().group->signals->bid_count, 3);
}

// Adding an interest group and then immediately running the ad acution, without
// waiting in between, should always work because although adding the interest
// group is async (and intentionally without completion notification), it should
// complete before the auction runs.
//
// On regression, this test will likely only fail with very low frequency.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       AddInterestGroupRunAuctionWithWinnerWithoutWaiting) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  std::string ads =
      "[{renderUrl : 'https://example.com/render',"
      "metadata : {ad:'metadata', here : [ 1, 2 ]}}]";

  // Use JoinInterestGroupInJS() instead of JoinInterestGroupAndWaitInJs().

  EXPECT_TRUE(JoinInterestGroupInJS(
      blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/url::Origin::Create(test_url.GetOrigin()),
          /*name=*/"cars",
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
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

  // Leave the interest group, then re-run the auction. We shouldn't get a
  // result.
  LeaveInterestGroupInJS(/*owner=*/url::Origin::Create(test_url.GetOrigin()),
                         /*name=*/"cars");
  EXPECT_EQ(
      nullptr,
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
}

// The winning ad's render url is invalid (invalid url or has http scheme).
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionWithInvalidAdUrl) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  std::string ads =
      "[{renderUrl : 'https://shoes.com/render',"
      "metadata : {ad:'metadata', here : [ 1, 2 ]}}]";
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/url::Origin::Create(test_url.GetOrigin()),
          /*name=*/"cars",
          /*bidding_url=*/
          https_server_->GetURL(
              "a.test", "/interest_group/bidding_logic_invalid_ad_url.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
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

// These end-to-end tests validate that information from navigator-exposed APIs
// is correctly passed to worklets.

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       BuyerWorkletThrowsFailsAuction) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  ASSERT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time() + base::Seconds(300),
          /*owner=*/url::Origin::Create(test_url.GetOrigin()),
          /*name=*/"cars",
          /*bidding_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/bidding_logic_throws.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2, 3]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
      /*ads=*/
      "[{renderUrl: 'https://example.com/render', metadata: {ad: 'metadata', "
      "here: [1, 2, 3]}}]",
      /*trusted_bidding_signals_keys=*/"['key1']"));

  EXPECT_EQ(
      nullptr,
      EvalJs(shell(),
             JsReplace(
                 R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
  });
})())",
                 test_url.GetOrigin().spec(),
                 https_server_
                     ->GetURL("a.test", "/interest_group/decision_logic.js")
                     .spec())));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, ValidateGenerateBid) {
  // Start by adding a placeholder bidder in domain b.test, used for
  // perBuyerSignals validation.
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  ASSERT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time() + base::Seconds(300),
          /*owner=*/url::Origin::Create(test_url_b.GetOrigin()),
          /*name=*/"boats",
          /*bidding_url=*/
          https_server_->GetURL("b.test", "/interest_group/bidding_logic.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("b.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2, 3]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
      /*ads=*/
      "[{renderUrl: 'https://example2.com/render', metadata: {ad: 'metadata', "
      "here: [1, 2, 3]}}]",
      /*trusted_bidding_signals_keys=*/"['key1']"));

  // This is the primary interest group that wins the auction because
  // bidding_argument_validator.js bids 2, whereas bidding_logic.js bids 1, and
  // decision_logic.js just returns the bid as the rank -- highest rank wins.
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  ASSERT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time() + base::Seconds(300),
          /*owner=*/url::Origin::Create(test_url.GetOrigin()),
          /*name=*/"cars",
          /*bidding_url=*/
          https_server_->GetURL(
              "a.test", "/interest_group/bidding_argument_validator.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2, 3]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
      /*ads=*/
      "[{renderUrl: 'https://example.com/render', metadata: {ad: 'metadata', "
      "here: [1, 2, 3]}}]",
      /*trusted_bidding_signals_keys=*/"['key1']"));

  EXPECT_EQ(
      "https://example.com/render",
      EvalJs(shell(),
             JsReplace(
                 R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $3,
    interestGroupBuyers: [$1, $2],
    auctionSignals: {so: 'I', hear: ['you', 'like', 'json']},
    sellerSignals: {signals: 'from', the: ['seller']},
    perBuyerSignals: {$1: {signalsForBuyer: 1}, $2: {signalsForBuyer: 2}}
  });
})())",
                 test_url.GetOrigin().spec(), test_url_b.GetOrigin().spec(),
                 https_server_
                     ->GetURL("a.test", "/interest_group/decision_logic.js")
                     .spec())));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       SellerWorkletThrowsFailsAuction) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  ASSERT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time() + base::Seconds(300),
          /*owner=*/url::Origin::Create(test_url.GetOrigin()),
          /*name=*/"cars",
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2, 3]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
      /*ads=*/
      "[{renderUrl: 'https://example.com/render', metadata: {ad: 'metadata', "
      "here: [1, 2, 3]}}]",
      /*trusted_bidding_signals_keys=*/"['key1']"));

  EXPECT_EQ(nullptr,
            EvalJs(shell(),
                   JsReplace(
                       R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
  });
})())",
                       test_url.GetOrigin().spec(),
                       https_server_
                           ->GetURL("a.test",
                                    "/interest_group/decision_logic_throws.js")
                           .spec())));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, ValidateScoreAd) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  ASSERT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time() + base::Seconds(300),
          /*owner=*/url::Origin::Create(test_url.GetOrigin()),
          /*name=*/"cars",
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2, 3]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
      /*ads=*/
      "[{renderUrl: 'https://example.com/render', metadata: {ad: 'metadata', "
      "here: [1, 2, 3]}}]",
      /*trusted_bidding_signals_keys=*/"['key1']"));

  EXPECT_EQ(
      "https://example.com/render",
      EvalJs(shell(),
             JsReplace(
                 R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {so: 'I', hear: ['you', 'like', 'json']},
    sellerSignals: {signals: 'from', the: ['seller']},
    perBuyerSignals: {$1: {signalsForBuyer: 1}}
  });
})())",
                 test_url.GetOrigin().spec(),
                 https_server_
                     ->GetURL("a.test",
                              "/interest_group/decision_argument_validator.js")
                     .spec())));
}

// Make sure that qutting with a live auction doesn't crash.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, QuitWithRunningAuction) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GURL hanging_url = https_server_->GetURL("a.test", "/hung");

  std::string ads =
      "[{renderUrl : 'https://example.com/render',"
      "metadata : {ad:'metadata', here : [ 1, 2 ]}}]";
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/url::Origin::Create(hanging_url.GetOrigin()),
          /*name=*/"cars",
          /*bidding_url=*/hanging_url,
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
      ads, "['key1']"));

  ExecuteScriptAsync(
      shell(), JsReplace(R"(
navigator.runAdAuction({
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$1]
});
)",
                         hanging_url.GetOrigin().spec(), hanging_url.spec()));

  WaitForURL(https_server_->GetURL("/hung"));
}

// These tests validate the `dailyUpdateUrl` and
// navigator.updateAdInterestGroups() functionality.

// The server JSON updates all fields that can be updated.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, UpdateAllUpdatableFields) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  // The server JSON updates all fields that can be updated.
  constexpr char kDailyUpdateUrlPath[] =
      "/interest_group/daily_update_partial.json";
  network_responder_->RegisterNetworkResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"biddingLogicUrl": "%s/interest_group/new_bidding_logic.js",
"trustedBiddingSignalsUrl":
  "%s/interest_group/new_trusted_bidding_signals_url.json",
"trustedBiddingSignalsKeys": ["new_key"],
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                              test_origin.Serialize().c_str(),
                                              test_origin.Serialize().c_str(),
                                              test_origin.Serialize().c_str()));

  ASSERT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time() + base::Seconds(300),
          /*owner=*/url::Origin::Create(test_url.GetOrigin()),
          /*name=*/"cars",
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*update_url=*/
          https_server_->GetURL("a.test", kDailyUpdateUrlPath),
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2, 3]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
      /*ads=*/
      "[{renderUrl: 'https://example.com/render', metadata: {ad: 'metadata', "
      "here: [1, 2, 3]}}]",
      /*trusted_bidding_signals_keys=*/"['key1']"));

  EXPECT_EQ("done", EvalJs(shell(), R"(
(function() {
  navigator.updateAdInterestGroups();
  return 'done';
})())"));

  WaitForInterestGroupsSatisfying(
      test_origin,
      base::BindLambdaForTesting(
          [](const std::vector<BiddingInterestGroup>& groups) {
            if (groups.size() != 1)
              return false;
            const auto& group = groups[0].group->group;
            return group.name == "cars" && group.bidding_url.has_value() &&
                   group.bidding_url->path() ==
                       "/interest_group/new_bidding_logic.js" &&
                   group.trusted_bidding_signals_url.has_value() &&
                   group.trusted_bidding_signals_url->path() ==
                       "/interest_group/new_trusted_bidding_signals_url.json" &&
                   group.trusted_bidding_signals_keys.has_value() &&
                   group.trusted_bidding_signals_keys->size() == 1 &&
                   group.trusted_bidding_signals_keys.value()[0] == "new_key" &&
                   group.ads.has_value() && group.ads->size() == 1 &&
                   group.ads.value()[0].render_url.path() ==
                       "/new_ad_render_url" &&
                   group.ads.value()[0].metadata == "{\"new_a\":\"b\"}";
          }));
}

// Updates can proceed even if the page that started the update isn't running
// anymore.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       UpdateAndNavigateAwayStillCompletes) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  // Start an update, then navigate to a different page. The update completes
  // even though the page that started the update is gone.
  constexpr char kDailyUpdateUrlPath[] =
      "/interest_group/daily_update_partial.json";
  network_responder_->RegisterNetworkResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                              test_origin.Serialize().c_str()));

  ASSERT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time() + base::Seconds(300),
          /*owner=*/url::Origin::Create(test_url.GetOrigin()),
          /*name=*/"cars",
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*update_url=*/
          https_server_->GetURL("a.test", kDailyUpdateUrlPath),
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2, 3]}}",
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
      /*ads=*/
      "[{renderUrl: 'https://example.com/render', metadata: {ad: 'metadata', "
      "here: [1, 2, 3]}}]",
      /*trusted_bidding_signals_keys=*/"['key1']"));

  EXPECT_EQ("done", EvalJs(shell(), R"(
(function() {
  navigator.updateAdInterestGroups();
  return 'done';
})())"));

  // Navigate away -- the update should still continue.
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  WaitForInterestGroupsSatisfying(
      test_origin,
      base::BindLambdaForTesting(
          [](const std::vector<BiddingInterestGroup>& groups) {
            if (groups.size() != 1)
              return false;
            const auto& group = groups[0].group->group;
            return group.name == "cars" && group.bidding_url.has_value() &&
                   group.bidding_url->path() ==
                       "/interest_group/bidding_logic.js" &&
                   group.update_url.has_value() &&
                   group.update_url->path() ==
                       "/interest_group/daily_update_partial.json" &&
                   group.trusted_bidding_signals_url.has_value() &&
                   group.trusted_bidding_signals_url->path() ==
                       "/interest_group/trusted_bidding_signals.json" &&
                   group.trusted_bidding_signals_keys.has_value() &&
                   group.trusted_bidding_signals_keys->size() == 1 &&
                   group.trusted_bidding_signals_keys.value()[0] == "key1" &&
                   group.ads.has_value() && group.ads->size() == 1 &&
                   group.ads.value()[0].render_url.path() ==
                       "/new_ad_render_url" &&
                   group.ads.value()[0].metadata == "{\"new_a\":\"b\"}";
          }));
}

// This test exercises the interest group and ad auction services directly,
// rather than via Blink, to ensure that those services running in the browser
// implement important security checks (Blink may also perform its own
// checking, but the render process is untrusted).
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionBasicBypassBlink) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  mojo::Remote<blink::mojom::AdAuctionService> auction_service;
  AdAuctionServiceImpl::CreateMojoService(
      shell()->web_contents()->GetMainFrame(),
      auction_service.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;

  auction_service->RunAdAuction(
      blink::mojom::AuctionAdConfig::New(),
      base::BindLambdaForTesting([&run_loop](const absl::optional<GURL>& url) {
        EXPECT_THAT(url, Eq(absl::nullopt));
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Fixture for Blink-bypassing auction tests that share the same interest group
// -- useful for checking auction service security validations.
class InterestGroupBrowserTestRunAdAuctionBypassBlink
    : public InterestGroupBrowserTest {
 protected:
  const GURL kAdUrl{"https://example.com/render"};

  void SetUpOnMainThread() override {
    InterestGroupBrowserTest::SetUpOnMainThread();

    GURL test_url_a = https_server_->GetURL("a.test", "/echo");
    test_origin_a_ = url::Origin::Create(test_url_a);
    ASSERT_TRUE(test_url_a.SchemeIs(url::kHttpsScheme));
    ASSERT_TRUE(NavigateToURL(shell(), test_url_a));

    mojo::Remote<blink::mojom::AdAuctionService> interest_service;
    AdAuctionServiceImpl::CreateMojoService(
        shell()->web_contents()->GetMainFrame(),
        interest_service.BindNewPipeAndPassReceiver());

    // Set up kAdUrl as the only interest group ad in the auction.
    blink::InterestGroup interest_group;
    interest_group.expiry = base::Time::Now() + base::Seconds(300);
    constexpr char kGroupName[] = "cars";
    interest_group.name = kGroupName;
    interest_group.owner = test_origin_a_;
    interest_group.bidding_url =
        https_server_->GetURL("a.test", "/interest_group/bidding_logic.js");
    interest_group.trusted_bidding_signals_url = https_server_->GetURL(
        "a.test", "/interest_group/trusted_bidding_signals.json");
    interest_group.trusted_bidding_signals_keys.emplace();
    interest_group.trusted_bidding_signals_keys->push_back("key1");
    interest_group.user_bidding_signals =
        "{\"some\": \"json\", \"data\": {\"here\": [1, 2, 3]}}";
    interest_group.ads.emplace();
    interest_group.ads->push_back(blink::InterestGroup::Ad(
        /* render_url = */ kAdUrl,
        /* metadata = */ "{\"ad\": \"metadata\", \"here\": [1, 2, 3]}"));
    interest_service->JoinInterestGroup(std::move(interest_group));
    interest_service.FlushForTesting();
    EXPECT_EQ(1, GetJoinCount(test_origin_a_, kGroupName));
  }

  absl::optional<GURL> RunAuctionBypassBlink(
      blink::mojom::AuctionAdConfigPtr config) {
    absl::optional<GURL> maybe_url;
    base::RunLoop run_loop;
    mojo::Remote<blink::mojom::AdAuctionService> auction_service;
    AdAuctionServiceImpl::CreateMojoService(
        shell()->web_contents()->GetMainFrame(),
        auction_service.BindNewPipeAndPassReceiver());

    auction_service->RunAdAuction(
        std::move(config),
        base::BindLambdaForTesting(
            [&run_loop, &maybe_url](const absl::optional<GURL>& url) {
              maybe_url = url;
              run_loop.Quit();
            }));
    run_loop.Run();
    return maybe_url;
  }

  url::Origin test_origin_a_;
};

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTestRunAdAuctionBypassBlink,
                       BasicSuccess) {
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  auto config = blink::mojom::AuctionAdConfig::New();
  config->seller = test_origin_b;
  config->decision_logic_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");
  config->interest_group_buyers = blink::mojom::InterestGroupBuyers::New();
  config->interest_group_buyers->set_buyers({test_origin_a_});

  EXPECT_THAT(RunAuctionBypassBlink(std::move(config)), Optional(Eq(kAdUrl)));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTestRunAdAuctionBypassBlink,
                       SellerNotHttps) {
  GURL test_url_b = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  auto config = blink::mojom::AuctionAdConfig::New();
  config->seller = test_origin_b;
  config->decision_logic_url = embedded_test_server()->GetURL(
      "b.test", "/interest_group/decision_logic.js");
  ASSERT_TRUE(config->decision_logic_url.SchemeIs(url::kHttpScheme));
  config->interest_group_buyers = blink::mojom::InterestGroupBuyers::New();
  config->interest_group_buyers->set_buyers({test_origin_a_});

  EXPECT_THAT(RunAuctionBypassBlink(std::move(config)), Eq(absl::nullopt));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTestRunAdAuctionBypassBlink,
                       SellerDoesntMatchFrameOrigin) {
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  url::Origin test_origin_b = url::Origin::Create(test_url_b);

  // Frame is `test_origin_a`, which doesn't match seller `test_origin_b`, so
  // the auction fails.
  auto config = blink::mojom::AuctionAdConfig::New();
  config->seller = test_origin_b;
  config->decision_logic_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");
  config->interest_group_buyers = blink::mojom::InterestGroupBuyers::New();
  config->interest_group_buyers->set_buyers({test_origin_a_});

  EXPECT_THAT(RunAuctionBypassBlink(std::move(config)), Eq(absl::nullopt));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTestRunAdAuctionBypassBlink,
                       WrongDecisionUrlOrigin) {
  // The `decision_logic_url` origin doesn't match `seller`s, which is invalid.
  auto config = blink::mojom::AuctionAdConfig::New();
  config->seller = test_origin_a_;
  config->decision_logic_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");
  config->interest_group_buyers = blink::mojom::InterestGroupBuyers::New();
  config->interest_group_buyers->set_buyers({test_origin_a_});

  EXPECT_THAT(RunAuctionBypassBlink(std::move(config)), Eq(absl::nullopt));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTestRunAdAuctionBypassBlink,
                       InterestGroupBuyerOriginNotHttps) {
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  // Same hostname as `test_url_a_`, different scheme. This buyer is not valid
  // because it is not https, so the auction fails.
  GURL test_url_a_http = embedded_test_server()->GetURL("a.test", "/echo");
  ASSERT_TRUE(test_url_a_http.SchemeIs(url::kHttpScheme));
  url::Origin test_origin_a_http = url::Origin::Create(test_url_a_http);

  auto config = blink::mojom::AuctionAdConfig::New();
  config->seller = test_origin_b;
  config->decision_logic_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");
  config->interest_group_buyers = blink::mojom::InterestGroupBuyers::New();
  config->interest_group_buyers->set_buyers({test_origin_a_http});

  EXPECT_THAT(RunAuctionBypassBlink(std::move(config)), Eq(absl::nullopt));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTestRunAdAuctionBypassBlink,
                       InterestGroupBuyerOriginNotHttpsMultipleBuyers) {
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  // Same hostname as `test_url_a_`, different scheme. This buyer is not valid
  // because it is not https, so the auction fails, even though the other buyer
  // is valid.
  GURL test_url_a_http = embedded_test_server()->GetURL("a.test", "/echo");
  ASSERT_TRUE(test_url_a_http.SchemeIs(url::kHttpScheme));
  url::Origin test_origin_a_http = url::Origin::Create(test_url_a_http);

  auto config = blink::mojom::AuctionAdConfig::New();
  config->seller = test_origin_b;
  config->decision_logic_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");
  config->interest_group_buyers = blink::mojom::InterestGroupBuyers::New();
  config->interest_group_buyers->set_buyers(
      {test_origin_a_, test_origin_a_http});

  EXPECT_THAT(RunAuctionBypassBlink(std::move(config)), Eq(absl::nullopt));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTestRunAdAuctionBypassBlink,
                       BuyerWithNoRegisteredInterestGroupsIgnored) {
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  // New valid origin, not associated with any registered interest group. Its
  // presence in the auctions `interest_group_buyers` shouldn't affect the
  // auction outcome.
  GURL test_url_c = https_server_->GetURL("c.test", "/echo");
  ASSERT_TRUE(test_url_c.SchemeIs(url::kHttpsScheme));
  url::Origin test_origin_c = url::Origin::Create(test_url_c);

  auto config = blink::mojom::AuctionAdConfig::New();
  config->seller = test_origin_b;
  config->decision_logic_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");
  config->interest_group_buyers = blink::mojom::InterestGroupBuyers::New();
  config->interest_group_buyers->set_buyers({test_origin_a_, test_origin_c});

  EXPECT_THAT(RunAuctionBypassBlink(std::move(config)), Optional(Eq(kAdUrl)));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTestRunAdAuctionBypassBlink,
                       InterestGroupWildcardStarNotSupported) {
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  auto config = blink::mojom::AuctionAdConfig::New();
  config->seller = test_origin_b;
  config->decision_logic_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");
  config->interest_group_buyers = blink::mojom::InterestGroupBuyers::New();
  config->interest_group_buyers->set_all_buyers(blink::mojom::AllBuyers::New());

  // All buyers isn't supported.
  EXPECT_THAT(RunAuctionBypassBlink(std::move(config)), Eq(absl::nullopt));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTestRunAdAuctionBypassBlink,
                       PerBuyerSignalsValid) {
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  // Per-buyer signals are valid because `test_origin_a_` is in the set of
  // buyers, so the auction succeeds.
  auto config = blink::mojom::AuctionAdConfig::New();
  config->seller = test_origin_b;
  config->decision_logic_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");
  config->interest_group_buyers = blink::mojom::InterestGroupBuyers::New();
  config->interest_group_buyers->set_buyers({test_origin_a_});
  config->per_buyer_signals.emplace();
  config->per_buyer_signals.value()[test_origin_a_] =
      "{\"even\": \"more\", \"x\": 4.5}";

  EXPECT_THAT(RunAuctionBypassBlink(std::move(config)), Optional(Eq(kAdUrl)));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTestRunAdAuctionBypassBlink,
                       PerBuyerSignalsNotSubsetOfBuyers) {
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  // Per-buyer signals are invalid because `test_origin_a_` is not in the set of
  // buyers, so the auction fails.
  auto config = blink::mojom::AuctionAdConfig::New();
  config->seller = test_origin_b;
  config->decision_logic_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");
  config->interest_group_buyers = blink::mojom::InterestGroupBuyers::New();
  config->interest_group_buyers->set_buyers({test_origin_a_});
  config->per_buyer_signals.emplace();
  // `test_origin_b` isn't in `interest_group_buyers`.
  config->per_buyer_signals.value()[test_origin_b] =
      "{\"even\": \"more\", \"x\": 4.5}";

  EXPECT_THAT(RunAuctionBypassBlink(std::move(config)), Eq(absl::nullopt));
}

// Validate that createAdRequest is available and be successfully called as part
// of PARAKEET.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, CreateAdRequestWorks) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ("NotSupportedError: createAdRequest API not yet implemented",
            CreateAdRequestAndWait());
}

// Validate that finalizeAd is available and be successfully called as part of
// PARAKEET.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, FinalizeAdWorks) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ("NotSupportedError: finalizeAd API not yet implemented",
            FinalizeAdAndWait());
}

}  // namespace

}  // namespace content
