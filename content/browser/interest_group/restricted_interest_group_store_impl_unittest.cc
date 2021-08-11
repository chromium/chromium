// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/restricted_interest_group_store_impl.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/interest_group/interest_group_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_client.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/test/test_content_browser_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "third_party/blink/public/mojom/interest_group/restricted_interest_group_store.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr char kInterestGroupName[] = "interest-group-name";
constexpr char kOriginStringA[] = "https://a.test";
constexpr char kOriginStringB[] = "https://b.test";

class AllowInterestGroupContentBrowserClient : public TestContentBrowserClient {
 public:
  explicit AllowInterestGroupContentBrowserClient() = default;
  ~AllowInterestGroupContentBrowserClient() override = default;

  AllowInterestGroupContentBrowserClient(
      const AllowInterestGroupContentBrowserClient&) = delete;
  AllowInterestGroupContentBrowserClient& operator=(
      const AllowInterestGroupContentBrowserClient&) = delete;

  // ContentBrowserClient overrides:
  bool IsInterestGroupAPIAllowed(content::BrowserContext* browser_context,
                                 const url::Origin& top_frame_origin,
                                 const GURL& api_url) override {
    return top_frame_origin.host() == "a.test" ||
           top_frame_origin.host() == "b.test";
  }
};

constexpr char kFledgeHeaders[] =
    "HTTP/1.1 200 OK\n"
    "Content-type: Application/JSON\n"
    "X-Allow-FLEDGE: true\n";

// Allows registering network responses to update requests; *must* be destroyed
// before the task environment is shutdown (which happens in
// RenderViewHostTestHarness::TearDown()).
class UpdateResponder {
 public:
  void RegisterUpdateResponse(const std::string& url_path,
                              const std::string& response) {
    base::AutoLock auto_lock(json_update_lock_);
    json_update_map_.insert({url_path, response});
  }

  // Registers a URL to use a "deferred" response. For a deferred response, the
  // request handler returns true without a write, and writes are performed
  // later in DoDeferredWrite() using a "stolen" Mojo pipe to the
  // URLLoaderClient.
  //
  // Only one request / response can be handled with this method at a time.
  void RegisterDeferredResponse(const std::string& url_path) {
    base::AutoLock auto_lock(json_update_lock_);
    deferred_response_url_path_ = url_path;
  }

  // Perform the deferred response -- the test fails if the client isn't waiting
  // on `url_path` registered with RegisterDeferredResponse().
  void DoDeferredResponse(const std::string& response) {
    base::AutoLock auto_lock(json_update_lock_);
    ASSERT_TRUE(deferred_response_url_loader_client_.is_bound());
    URLLoaderInterceptor::WriteResponse(
        kFledgeHeaders, response, deferred_response_url_loader_client_.get());
    deferred_response_url_loader_client_.reset();
    deferred_response_url_path_ = "";
  }

 private:
  bool RequestHandlerForUpdates(URLLoaderInterceptor::RequestParams* params) {
    base::AutoLock auto_lock(json_update_lock_);
    EXPECT_TRUE(params->url_request.trusted_params->isolation_info
                    .network_isolation_key()
                    .IsTransient());
    if (params->url_request.url.path() == deferred_response_url_path_) {
      CHECK(!deferred_response_url_loader_client_);
      deferred_response_url_loader_client_ = std::move(params->client);
      return true;
    }
    const auto it = json_update_map_.find(params->url_request.url.path());
    if (it == json_update_map_.end())
      return false;
    URLLoaderInterceptor::WriteResponse(kFledgeHeaders, it->second,
                                        params->client.get());
    return true;
  }

  // Handles network requests for interest group updates.
  URLLoaderInterceptor update_interceptor_{
      base::BindRepeating(&UpdateResponder::RequestHandlerForUpdates,
                          base::Unretained(this))};

  base::Lock json_update_lock_;

  // For each HTTPS request, we see if any path in the map matches the request
  // path. If so, the server returns the mapped value string as the response.
  base::flat_map<std::string, std::string> json_update_map_
      GUARDED_BY(json_update_lock_);

  // Stores the last URL path that was registered with
  // RegisterDeferredResponse(). Empty initially and after DoDeferredResponse()
  // -- when empty, no deferred response can occur.
  std::string deferred_response_url_path_ GUARDED_BY(json_update_lock_);

  // Stores the Mojo URLLoaderClient remote "stolen" from
  // RequestHandlerForUpdates() for use with deferred responses -- unbound if no
  // remote has been "stolen" yet, or if the last deferred response completed.
  mojo::Remote<network::mojom::URLLoaderClient>
      deferred_response_url_loader_client_ GUARDED_BY(json_update_lock_);
};

}  // namespace

class RestrictedInterestGroupStoreImplTest : public RenderViewHostTestHarness {
 public:
  RestrictedInterestGroupStoreImplTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitAndEnableFeature(blink::features::kFledgeInterestGroups);
    old_content_browser_client_ =
        SetBrowserClientForTesting(&content_browser_client_);
  }

  ~RestrictedInterestGroupStoreImplTest() override {
    SetBrowserClientForTesting(old_content_browser_client_);
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    NavigateAndCommit(kUrlA);

    manager_ = (static_cast<StoragePartitionImpl*>(
                    browser_context()->GetDefaultStoragePartition()))
                   ->GetInterestGroupManager();
  }

  void TearDown() override {
    // `update_responder_` must be destructed while the task environment,
    // which gets destroyed by RenderViewHostTestHarness::TearDown(), is still
    // active.
    update_responder_.reset();
    RenderViewHostTestHarness::TearDown();
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

  int GetJoinCount(const url::Origin& owner, const std::string& name) {
    for (const auto& interest_group : GetInterestGroupsForOwner(owner)) {
      if (interest_group.group->group.name == name) {
        return interest_group.group->signals->join_count;
      }
    }
    return 0;
  }

  // Create a new RestrictedInterestGroupStoreImpl and use it to try and join
  // `interest_group`. Flushes the Mojo pipe to force the Mojo message to be
  // handled before returning.
  //
  // Creates a new RestrictedInterestGroupStoreImpl with each call so the RFH
  // can be navigated between different sites. And
  // RestrictedInterestGroupStoreImpl only handles one site (cross site navs use
  // different RestrictedInterestGroupStoreImpls, and generally use different
  // RFHs as well).
  void JoinInterestGroupAndFlush(const blink::InterestGroup& interest_group) {
    mojo::Remote<blink::mojom::RestrictedInterestGroupStore> interest_service;
    RestrictedInterestGroupStoreImpl::CreateMojoService(
        web_contents()->GetMainFrame(),
        interest_service.BindNewPipeAndPassReceiver());

    interest_service->JoinInterestGroup(interest_group);
    interest_service.FlushForTesting();
  }

  // Analogous to JoinInterestGroupAndFlush(), but leaves an interest group
  // instead of joining one.
  void LeaveInterestGroupAndFlush(const url::Origin& owner,
                                  const std::string& name) {
    mojo::Remote<blink::mojom::RestrictedInterestGroupStore> interest_service;
    RestrictedInterestGroupStoreImpl::CreateMojoService(
        web_contents()->GetMainFrame(),
        interest_service.BindNewPipeAndPassReceiver());

    interest_service->LeaveInterestGroup(owner, name);
    interest_service.FlushForTesting();
  }

  // Updates registered interest groups according to their registered update
  // URL. Doesn't flush since the update operation requires a sequence of
  // asynchronous operations.
  void UpdateInterestGroupNoFlush() {
    mojo::Remote<blink::mojom::RestrictedInterestGroupStore> interest_service;
    RestrictedInterestGroupStoreImpl::CreateMojoService(
        web_contents()->GetMainFrame(),
        interest_service.BindNewPipeAndPassReceiver());

    interest_service->UpdateAdInterestGroups();
  }

  // Helper to create a valid interest group with only an origin and name. All
  // URLs are nullopt.
  blink::InterestGroup CreateInterestGroup() {
    blink::InterestGroup interest_group;
    interest_group.expiry =
        base::Time::Now() + base::TimeDelta::FromSeconds(300);
    interest_group.name = kInterestGroupName;
    interest_group.owner = kOriginA;
    return interest_group;
  }

 protected:
  const GURL kUrlA = GURL(kOriginStringA);
  const url::Origin kOriginA = url::Origin::Create(kUrlA);
  const GURL kUrlB = GURL(kOriginStringB);
  const url::Origin kOriginB = url::Origin::Create(kUrlB);

  base::test::ScopedFeatureList feature_list_;

  AllowInterestGroupContentBrowserClient content_browser_client_;
  ContentBrowserClient* old_content_browser_client_ = nullptr;
  InterestGroupManager* manager_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  // Must be destroyed before RenderViewHostTestHarness::TearDown().
  std::unique_ptr<UpdateResponder> update_responder_{
      std::make_unique<UpdateResponder>()};
};

// Check basic success case.
TEST_F(RestrictedInterestGroupStoreImplTest, JoinInterestGroupBasic) {
  blink::InterestGroup interest_group = CreateInterestGroup();
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Several tests assume interest group API are also allowed on kOriginB, so
  // make sure that's enabled correctly.
  NavigateAndCommit(kUrlB);
  interest_group.owner = kOriginB;
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginB, kInterestGroupName));
}

// Non-HTTPS interest groups should be rejected.
TEST_F(RestrictedInterestGroupStoreImplTest, JoinInterestGroupOriginNotHttps) {
  // Note that the ContentBrowserClient allows URLs based on hosts, not origins,
  // so it should not block this URL. Instead, it should run into the HTTPS
  // check.
  const GURL kHttpUrlA = GURL("http://a.test/");
  const url::Origin kHttpOriginA = url::Origin::Create(kHttpUrlA);
  NavigateAndCommit(kHttpUrlA);
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.owner = kHttpOriginA;
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(0, GetJoinCount(kHttpOriginA, kInterestGroupName));
}

// Test one origin trying to add an interest group for another.
TEST_F(RestrictedInterestGroupStoreImplTest,
       JoinInterestGroupWrongOwnerOrigin) {
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.owner = kOriginB;
  JoinInterestGroupAndFlush(interest_group);
  // Interest group should not be added for either origin.
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0, GetJoinCount(kOriginB, kInterestGroupName));
}

// Test joining an interest group with a disallowed cross-origin URL. Doesn't
// exhaustively test all cases, as the validation function has its own unit
// tests. This is just to make sure those are hooked up.
//
// TODO(mmenke): Once ReportBadMessage is called in these cases, make sure Mojo
// pipe is closed as well.
TEST_F(RestrictedInterestGroupStoreImplTest, JoinInterestGroupCrossSiteUrls) {
  const GURL kBadUrl = GURL("https://user:pass@a.test/");

  // Test `bidding_url`.
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kBadUrl;
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));

  // Test `update_url`.
  interest_group = CreateInterestGroup();
  interest_group.update_url = kBadUrl;
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));

  // Test `trusted_bidding_signals_url`.
  interest_group = CreateInterestGroup();
  interest_group.trusted_bidding_signals_url = kBadUrl;
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
}

// Check that cross-origin leave interest group operations don't work.
TEST_F(RestrictedInterestGroupStoreImplTest,
       LeaveInterestGroupWrongOwnerOrigin) {
  // https://a.test/ joins an interest group.
  JoinInterestGroupAndFlush(CreateInterestGroup());
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // https://b.test/ cannot leave https://a.test/'s interest group.
  NavigateAndCommit(kUrlB);
  LeaveInterestGroupAndFlush(kOriginA, kInterestGroupName);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // https://a.test/ can leave its own the interest group.
  NavigateAndCommit(GURL("https://a.test/"));
  LeaveInterestGroupAndFlush(kOriginA, kInterestGroupName);
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
}

// These tests validate the `dailyUpdateUrl` and
// navigator.updateAdInterestGroups() functionality.

// The server JSON updates all fields that can be updated.
TEST_F(RestrictedInterestGroupStoreImplTest, UpdateAllUpdatableFields) {
  constexpr char kDailyUpdateUrlPath[] =
      "/interest_group/daily_update_partial.json";
  update_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath,
      base::StringPrintf(R"({
"biddingLogicUrl": "%s/interest_group/new_bidding_logic.js",
"trustedBiddingSignalsUrl":
  "%s/interest_group/new_trusted_bidding_signals_url.json",
"trustedBiddingSignalsKeys": ["new_key"],
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                         kOriginStringA, kOriginStringA, kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUrlA.Resolve(kDailyUpdateUrlPath);
  interest_group.bidding_url =
      kUrlA.Resolve("/interest_group/bidding_logic.js");
  interest_group.trusted_bidding_signals_url =
      kUrlA.Resolve("/interest_group/trusted_bidding_signals.json");
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->push_back(std::move(ad));
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  std::vector<BiddingInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].group->group;
  EXPECT_EQ(group.name, kInterestGroupName);
  ASSERT_TRUE(group.bidding_url.has_value());
  EXPECT_EQ(group.bidding_url->spec(),
            base::StringPrintf("%s/interest_group/new_bidding_logic.js",
                               kOriginStringA));
  ASSERT_TRUE(group.trusted_bidding_signals_url.has_value());
  EXPECT_EQ(group.trusted_bidding_signals_url->spec(),
            base::StringPrintf(
                "%s/interest_group/new_trusted_bidding_signals_url.json",
                kOriginStringA));
  ASSERT_TRUE(group.trusted_bidding_signals_keys.has_value());
  EXPECT_EQ(group.trusted_bidding_signals_keys->size(), 1u);
  EXPECT_EQ(group.trusted_bidding_signals_keys.value()[0], "new_key");
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(group.ads.value()[0].metadata, "{\"new_a\":\"b\"}");
}

// Only set the ads field -- the other fields shouldn't be changed.
TEST_F(RestrictedInterestGroupStoreImplTest, UpdatePartialPerformsMerge) {
  constexpr char kDailyUpdateUrlPath[] =
      "/interest_group/daily_update_partial.json";
  update_responder_->RegisterUpdateResponse(kDailyUpdateUrlPath,
                                            base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                                               kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUrlA.Resolve(kDailyUpdateUrlPath);
  interest_group.bidding_url =
      kUrlA.Resolve("/interest_group/bidding_logic.js");
  interest_group.trusted_bidding_signals_url =
      kUrlA.Resolve("/interest_group/trusted_bidding_signals.json");
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->push_back(std::move(ad));
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  std::vector<BiddingInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].group->group;
  EXPECT_EQ(group.name, kInterestGroupName);
  ASSERT_TRUE(group.bidding_url.has_value());
  EXPECT_EQ(
      group.bidding_url->spec(),
      base::StringPrintf("%s/interest_group/bidding_logic.js", kOriginStringA));
  ASSERT_TRUE(group.update_url.has_value());
  EXPECT_EQ(group.update_url->spec(),
            base::StringPrintf("%s/interest_group/daily_update_partial.json",
                               kOriginStringA));
  ASSERT_TRUE(group.trusted_bidding_signals_url.has_value());
  EXPECT_EQ(group.trusted_bidding_signals_url->spec(),
            base::StringPrintf("%s/interest_group/trusted_bidding_signals.json",
                               kOriginStringA));
  ASSERT_TRUE(group.trusted_bidding_signals_keys.has_value());
  EXPECT_EQ(group.trusted_bidding_signals_keys->size(), 1u);
  EXPECT_EQ(group.trusted_bidding_signals_keys.value()[0], "key1");
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(group.ads.value()[0].metadata, "{\"new_a\":\"b\"}");
}

// The update shouldn't change the expiration time of the interest group.
TEST_F(RestrictedInterestGroupStoreImplTest, UpdateDoesntChangeExpiration) {
  constexpr char kDailyUpdateUrlPath[] =
      "/interest_group/daily_update_partial.json";
  update_responder_->RegisterUpdateResponse(kDailyUpdateUrlPath,
                                            base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                                               kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUrlA.Resolve(kDailyUpdateUrlPath);
  interest_group.bidding_url =
      kUrlA.Resolve("/interest_group/bidding_logic.js");
  interest_group.trusted_bidding_signals_url =
      kUrlA.Resolve("/interest_group/trusted_bidding_signals.json");
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->push_back(std::move(ad));
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Lookup expiry from the database before updating.
  const auto groups_before_update = GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups_before_update.size(), 1u);
  const base::Time kExpirationTime =
      groups_before_update[0].group->group.expiry;

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The expiration time shouldn't change.
  std::vector<BiddingInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].group->group;
  EXPECT_EQ(group.name, kInterestGroupName);
  EXPECT_EQ(group.expiry, kExpirationTime);
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(group.ads.value()[0].metadata, "{\"new_a\":\"b\"}");
}

// Only set the ads field -- the other fields shouldn't be changed.
TEST_F(RestrictedInterestGroupStoreImplTest,
       UpdateSucceedsIfOptionalNameOwnerMatch) {
  constexpr char kDailyUpdateUrlPath[] =
      "/interest_group/daily_update_partial.json";
  update_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath,
      base::StringPrintf(R"({
"name": "%s",
"owner": "%s",
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                         kInterestGroupName, kOriginStringA, kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUrlA.Resolve(kDailyUpdateUrlPath);
  interest_group.bidding_url =
      kUrlA.Resolve("/interest_group/bidding_logic.js");
  interest_group.trusted_bidding_signals_url =
      kUrlA.Resolve("/interest_group/trusted_bidding_signals.json");
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->push_back(std::move(ad));
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  std::vector<BiddingInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].group->group;
  EXPECT_EQ(group.name, kInterestGroupName);
  ASSERT_TRUE(group.bidding_url.has_value());
  EXPECT_EQ(
      group.bidding_url->spec(),
      base::StringPrintf("%s/interest_group/bidding_logic.js", kOriginStringA));
  ASSERT_TRUE(group.update_url.has_value());
  EXPECT_EQ(group.update_url->spec(),
            base::StringPrintf("%s/interest_group/daily_update_partial.json",
                               kOriginStringA));
  ASSERT_TRUE(group.trusted_bidding_signals_url.has_value());
  EXPECT_EQ(group.trusted_bidding_signals_url->spec(),
            base::StringPrintf("%s/interest_group/trusted_bidding_signals.json",
                               kOriginStringA));
  ASSERT_TRUE(group.trusted_bidding_signals_keys.has_value());
  EXPECT_EQ(group.trusted_bidding_signals_keys->size(), 1u);
  EXPECT_EQ(group.trusted_bidding_signals_keys.value()[0], "key1");
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(group.ads.value()[0].metadata, "{\"new_a\":\"b\"}");
}

// Try to set the name -- for security, name and owner shouldn't be
// allowed to change. If they don't match the interest group (update URLs are
// registered per interest group), fail the update and don't update anything.
TEST_F(RestrictedInterestGroupStoreImplTest,
       NoUpdateIfOptionalNameDoesntMatch) {
  constexpr char kDailyUpdateUrlPath[] =
      "/interest_group/daily_update_partial.json";
  update_responder_->RegisterUpdateResponse(kDailyUpdateUrlPath,
                                            base::StringPrintf(R"({
"name": "boats",
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata":{"new_a": "b"}
        }]
})",
                                                               kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUrlA.Resolve(kDailyUpdateUrlPath);
  interest_group.bidding_url =
      kUrlA.Resolve("/interest_group/bidding_logic.js");
  interest_group.trusted_bidding_signals_url =
      kUrlA.Resolve("/interest_group/trusted_bidding_signals.json");
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->push_back(std::move(ad));
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads didn't change.
  std::vector<BiddingInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].group->group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");
}

// Try to set the owner -- for security, name and owner shouldn't be
// allowed to change. If they don't match the interest group (update URLs are
// registered per interest group), fail the update and don't update anything.
TEST_F(RestrictedInterestGroupStoreImplTest,
       NoUpdateIfOptionalOwnerDoesntMatch) {
  constexpr char kDailyUpdateUrlPath[] =
      "/interest_group/daily_update_partial.json";
  update_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"owner": "%s",
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                              kOriginStringB, kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUrlA.Resolve(kDailyUpdateUrlPath);
  interest_group.bidding_url =
      kUrlA.Resolve("/interest_group/bidding_logic.js");
  interest_group.trusted_bidding_signals_url =
      kUrlA.Resolve("/interest_group/trusted_bidding_signals.json");
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->push_back(std::move(ad));
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads didn't change.
  std::vector<BiddingInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].group->group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");
}

// Join 2 interest groups, each with the same owner, but with different update
// URLs. Both interest groups should be updated correctly.
TEST_F(RestrictedInterestGroupStoreImplTest, UpdateMultipleInterestGroups) {
  constexpr char kGroupName1[] = "group1";
  constexpr char kGroupName2[] = "group2";
  constexpr char kDailyUpdateUrlPath1[] =
      "/interest_group/daily_update_partial1.json";
  constexpr char kDailyUpdateUrlPath2[] =
      "/interest_group/daily_update_partial2.json";
  update_responder_->RegisterUpdateResponse(kDailyUpdateUrlPath1,
                                            base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url1",
         "metadata": {"new_a": "b1"}
        }]
})",
                                                               kOriginStringA));
  update_responder_->RegisterUpdateResponse(kDailyUpdateUrlPath2,
                                            base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url2",
         "metadata": {"new_a": "b2"}
        }]
})",
                                                               kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.name = kGroupName1;
  interest_group.update_url = kUrlA.Resolve(kDailyUpdateUrlPath1);
  interest_group.bidding_url =
      kUrlA.Resolve("/interest_group/bidding_logic.js");
  interest_group.trusted_bidding_signals_url =
      kUrlA.Resolve("/interest_group/trusted_bidding_signals.json");
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->push_back(std::move(ad));
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(1, GetJoinCount(kOriginA, kGroupName1));

  // Now, join the second interest group, also belonging to `kOriginA`.
  blink::InterestGroup interest_group_2 = CreateInterestGroup();
  interest_group_2.name = kGroupName2;
  interest_group_2.update_url = kUrlA.Resolve(kDailyUpdateUrlPath2);
  interest_group_2.bidding_url =
      kUrlA.Resolve("/interest_group/bidding_logic.js");
  interest_group_2.trusted_bidding_signals_url =
      kUrlA.Resolve("/interest_group/trusted_bidding_signals.json");
  interest_group_2.trusted_bidding_signals_keys.emplace();
  interest_group_2.trusted_bidding_signals_keys->push_back("key1");
  interest_group_2.ads.emplace();
  ad = blink::InterestGroup::Ad();
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group_2.ads->push_back(std::move(ad));
  JoinInterestGroupAndFlush(std::move(interest_group_2));
  EXPECT_EQ(1, GetJoinCount(kOriginA, kGroupName2));

  // Now, run the update. Both interest groups should update.
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Both interest groups should update.
  std::vector<BiddingInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 2u);
  const auto& first_group = groups[0].group->group.name == kGroupName1
                                ? groups[0].group->group
                                : groups[1].group->group;
  const auto& second_group = groups[0].group->group.name == kGroupName2
                                 ? groups[0].group->group
                                 : groups[1].group->group;

  EXPECT_EQ(first_group.name, kGroupName1);
  ASSERT_TRUE(first_group.ads.has_value());
  ASSERT_EQ(first_group.ads->size(), 1u);
  EXPECT_EQ(first_group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url1", kOriginStringA));
  EXPECT_EQ(first_group.ads.value()[0].metadata, "{\"new_a\":\"b1\"}");

  EXPECT_EQ(second_group.name, kGroupName2);
  ASSERT_TRUE(second_group.ads.has_value());
  ASSERT_EQ(second_group.ads->size(), 1u);
  EXPECT_EQ(second_group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url2", kOriginStringA));
  EXPECT_EQ(second_group.ads.value()[0].metadata, "{\"new_a\":\"b2\"}");
}

// Join 2 interest groups, each with a different owner. When updating interest
// groups, only the 1 interest group owned by the origin of the frame that
// called navigator.updateAdInterestGroups() gets updated.
TEST_F(RestrictedInterestGroupStoreImplTest, UpdateOnlyOwnOrigin) {
  // Both interest groups can share the same update logic and path (they just
  // use different origins).
  constexpr char kDailyUpdateUrlPath[] =
      "/interest_group/daily_update_partial.json";
  update_responder_->RegisterUpdateResponse(kDailyUpdateUrlPath,
                                            base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                                               kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUrlA.Resolve(kDailyUpdateUrlPath);
  interest_group.bidding_url =
      kUrlA.Resolve("/interest_group/bidding_logic.js");
  interest_group.trusted_bidding_signals_url =
      kUrlA.Resolve("/interest_group/trusted_bidding_signals.json");
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->push_back(std::move(ad));
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Now, join the second interest group, belonging to `kOriginB`.
  NavigateAndCommit(kUrlB);
  blink::InterestGroup interest_group_b = CreateInterestGroup();
  interest_group_b.owner = kOriginB;
  interest_group_b.update_url = kUrlB.Resolve(kDailyUpdateUrlPath);
  interest_group_b.bidding_url =
      kUrlB.Resolve("/interest_group/bidding_logic.js");
  interest_group_b.trusted_bidding_signals_url =
      kUrlB.Resolve("/interest_group/trusted_bidding_signals.json");
  interest_group_b.trusted_bidding_signals_keys.emplace();
  interest_group_b.trusted_bidding_signals_keys->push_back("key1");
  interest_group_b.ads.emplace();
  ad = blink::InterestGroup::Ad();
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group_b.ads->push_back(std::move(ad));
  JoinInterestGroupAndFlush(std::move(interest_group_b));
  EXPECT_EQ(1, GetJoinCount(kOriginB, kInterestGroupName));

  // Now, run the update. Only the `kOriginB` group should get updated.
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The `kOriginB` interest group should update...
  std::vector<BiddingInterestGroup> origin_b_groups =
      GetInterestGroupsForOwner(kOriginB);
  ASSERT_EQ(origin_b_groups.size(), 1u);
  const auto& origin_b_group = origin_b_groups[0].group->group;
  EXPECT_EQ(origin_b_group.name, kInterestGroupName);
  ASSERT_TRUE(origin_b_group.ads.has_value());
  ASSERT_EQ(origin_b_group.ads->size(), 1u);
  EXPECT_EQ(origin_b_group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(origin_b_group.ads.value()[0].metadata, "{\"new_a\":\"b\"}");

  // ...but the `kOriginA` interest group shouldn't change.
  std::vector<BiddingInterestGroup> origin_a_groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(origin_a_groups.size(), 1u);
  const auto& origin_a_group = origin_a_groups[0].group->group;
  ASSERT_TRUE(origin_a_group.ads.has_value());
  ASSERT_EQ(origin_a_group.ads->size(), 1u);
  EXPECT_EQ(origin_a_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(origin_a_group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");
}

// The `ads` field is valid, but the ad `renderUrl` field is an invalid
// URL. The entire update should get cancelled, since updates are atomic.
TEST_F(RestrictedInterestGroupStoreImplTest,
       UpdateInvalidFieldCancelsAllUpdates) {
  constexpr char kDailyUpdateUrlPath[] =
      "/interest_group/daily_update_partial.json";
  update_responder_->RegisterUpdateResponse(kDailyUpdateUrlPath,
                                            base::StringPrintf(R"({
"biddingLogicUrl": "%s/interest_group/new_bidding_logic.js",
"ads": [{"renderUrl": "https://invalid^&",
         "metadata": {"new_a": "b"}
        }]
})",
                                                               kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUrlA.Resolve(kDailyUpdateUrlPath);
  interest_group.bidding_url =
      kUrlA.Resolve("/interest_group/bidding_logic.js");
  interest_group.trusted_bidding_signals_url =
      kUrlA.Resolve("/interest_group/trusted_bidding_signals.json");
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->push_back(std::move(ad));
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads didn't change.
  std::vector<BiddingInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].group->group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");
}

// The server response can't be parsed as valid JSON. The update is cancelled.
TEST_F(RestrictedInterestGroupStoreImplTest, UpdateInvalidJSONIgnored) {
  constexpr char kDailyUpdateUrlPath[] =
      "/interest_group/daily_update_partial.json";
  update_responder_->RegisterUpdateResponse(kDailyUpdateUrlPath,
                                            "This isn't JSON.");

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUrlA.Resolve(kDailyUpdateUrlPath);
  interest_group.bidding_url =
      kUrlA.Resolve("/interest_group/bidding_logic.js");
  interest_group.trusted_bidding_signals_url =
      kUrlA.Resolve("/interest_group/trusted_bidding_signals.json");
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->push_back(std::move(ad));
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads didn't change.
  std::vector<BiddingInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].group->group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");
}

// UpdateJSONParserCrash fails on Android because Android doesn't use a separate
// process to parse JSON -- instead, it validates JSON in-process in Java, then,
// if validation succeeded, uses the C++ JSON parser, also in-proc. On other
// platforms, the C++ parser runs out-of-proc for safety.
#if !defined(OS_ANDROID)

// The server response is valid, but we simulate the JSON parser (which may
// run in a separate process) crashing, so the update doesn't happen.
TEST_F(RestrictedInterestGroupStoreImplTest, UpdateJSONParserCrash) {
  constexpr char kDailyUpdateUrlPath[] =
      "/interest_group/daily_update_partial.json";
  update_responder_->RegisterUpdateResponse(kDailyUpdateUrlPath,
                                            base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                                               kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUrlA.Resolve(kDailyUpdateUrlPath);
  interest_group.bidding_url =
      kUrlA.Resolve("/interest_group/bidding_logic.js");
  interest_group.trusted_bidding_signals_url =
      kUrlA.Resolve("/interest_group/trusted_bidding_signals.json");
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->push_back(std::move(ad));
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Simulate the JSON service crashing instead of returning a result.
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  in_process_data_decoder.service().SimulateJsonParserCrashForTesting(
      /*drop=*/true);

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads didn't change.
  std::vector<BiddingInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].group->group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");
}

#endif  // !defined(OS_ANDROID)

// The network request fails (not implemented), so the update is cancelled.
TEST_F(RestrictedInterestGroupStoreImplTest, UpdateNetworkFailure) {
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUrlA.Resolve("no_handler.json");
  interest_group.bidding_url =
      kUrlA.Resolve("/interest_group/bidding_logic.js");
  interest_group.trusted_bidding_signals_url =
      kUrlA.Resolve("/interest_group/trusted_bidding_signals.json");
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->push_back(std::move(ad));
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads didn't change.
  std::vector<BiddingInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].group->group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");
}

// Start an update, and delay the server response so that the interest group
// expires before the interest group updates. Don't advance time enough for DB
// maintenance tasks to run -- that is the interest group will only exist on
// disk in an expired state, and not appear in queries.
TEST_F(RestrictedInterestGroupStoreImplTest,
       UpdateDuringInterestGroupExpirationNoDbMaintenence) {
  constexpr char kDailyUpdateUrlPath[] =
      "/interest_group/daily_update_partial.json";
  const std::string kServerResponse = base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}}]
})",
                                                         kOriginStringA);
  update_responder_->RegisterDeferredResponse(kDailyUpdateUrlPath);

  // Make the interest group expire before the DB maintenance task should be
  // run, with a gap second where expiration has happened, but DB maintenance
  // has not. Time order:
  // (*NOW*, group expiration, db maintenance).
  const base::TimeDelta kExpiryDelta =
      InterestGroupStorage::kIdlePeriod - base::TimeDelta::FromSeconds(2);
  ASSERT_GT(kExpiryDelta, base::TimeDelta::FromSeconds(0));
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now() + kExpiryDelta;
  interest_group.update_url = kUrlA.Resolve(kDailyUpdateUrlPath);
  interest_group.bidding_url =
      kUrlA.Resolve("/interest_group/bidding_logic.js");
  interest_group.trusted_bidding_signals_url =
      kUrlA.Resolve("/interest_group/trusted_bidding_signals.json");
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->push_back(std::move(ad));
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Start an interest group update and then advance time to ensure the interest
  // group expires before a response is returned.
  UpdateInterestGroupNoFlush();
  task_environment()->FastForwardBy(kExpiryDelta +
                                    base::TimeDelta::FromSeconds(1));
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0u, GetInterestGroupsForOwner(kOriginA).size());

  // Due to FastForwardBy(), we're at this time order:
  // (group expiration, *NOW*, db maintenance).
  // So, DB maintenance should not have been run.
  base::RunLoop run_loop;
  manager_->GetLastMaintenanceTimeForTesting(
      base::BindLambdaForTesting([&run_loop](base::Time time) {
        EXPECT_EQ(time, base::Time::Min());
        run_loop.Quit();
      }));
  run_loop.Run();

  // Now return the server response. The interest group shouldn't change as it's
  // expired.
  update_responder_->DoDeferredResponse(kServerResponse);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0u, GetInterestGroupsForOwner(kOriginA).size());

  // Updating again when the interest group has been deleted shouldn't somehow
  // bring it back.
  update_responder_->RegisterUpdateResponse(kDailyUpdateUrlPath,
                                            kServerResponse);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0u, GetInterestGroupsForOwner(kOriginA).size());

  // DB maintenance never occurs since we never FastForward() past db
  // maintenance. We still are at time order:
  // (group expiration, *NOW*, db maintenance).
}

// Start an update, and delay the server response so that the interest group
// expires before the interest group updates. Advance time enough for DB
// maintenance tasks to run -- that is the interest group will be deleted from
// the database.
TEST_F(RestrictedInterestGroupStoreImplTest,
       UpdateDuringInterestGroupExpirationWithDbMaintenence) {
  constexpr char kDailyUpdateUrlPath[] =
      "/interest_group/daily_update_partial.json";
  const std::string kServerResponse = base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}}]
})",
                                                         kOriginStringA);
  update_responder_->RegisterDeferredResponse(kDailyUpdateUrlPath);

  // Make the interest group expire just before the DB maintenance task should
  // be run. Time order:
  // (*NOW*, group expiration, db maintenance).
  const base::Time now = base::Time::Now();
  const base::TimeDelta kExpiryDelta =
      InterestGroupStorage::kIdlePeriod - base::TimeDelta::FromSeconds(1);
  ASSERT_GT(kExpiryDelta, base::TimeDelta::FromSeconds(0));
  const base::Time next_maintenance_time =
      now + InterestGroupStorage::kIdlePeriod;
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = now + kExpiryDelta;
  interest_group.update_url = kUrlA.Resolve(kDailyUpdateUrlPath);
  interest_group.bidding_url =
      kUrlA.Resolve("/interest_group/bidding_logic.js");
  interest_group.trusted_bidding_signals_url =
      kUrlA.Resolve("/interest_group/trusted_bidding_signals.json");
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->push_back(std::move(ad));
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Start an interest group update and then advance time to ensure the interest
  // group expires and then DB maintenance is performed, both before a response
  // is returned.
  UpdateInterestGroupNoFlush();
  task_environment()->FastForwardBy(InterestGroupStorage::kIdlePeriod +
                                    base::TimeDelta::FromSeconds(1));
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0u, GetInterestGroupsForOwner(kOriginA).size());

  // Due to FastForwardBy(), we're at this time order:
  // (group expiration, db maintenance, *NOW*).
  // So, DB maintenance should have been run.
  base::RunLoop run_loop;
  manager_->GetLastMaintenanceTimeForTesting(base::BindLambdaForTesting(
      [&run_loop, next_maintenance_time](base::Time time) {
        EXPECT_EQ(time, next_maintenance_time);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Now return the server response. The interest group shouldn't change as it's
  // expired.
  update_responder_->DoDeferredResponse(kServerResponse);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0u, GetInterestGroupsForOwner(kOriginA).size());

  // Updating again when the interest group has been deleted shouldn't somehow
  // bring it back.
  update_responder_->RegisterUpdateResponse(kDailyUpdateUrlPath,
                                            kServerResponse);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0u, GetInterestGroupsForOwner(kOriginA).size());
}

// The update doesn't happen because the update URL isn't specified at
// Join() time.
TEST_F(RestrictedInterestGroupStoreImplTest,
       DoesntChangeGroupsWithNoUpdateUrl) {
  constexpr char kDailyUpdateUrlPath[] =
      "/interest_group/daily_update_partial.json";
  update_responder_->RegisterUpdateResponse(kDailyUpdateUrlPath,
                                            base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                                               kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url =
      kUrlA.Resolve("/interest_group/bidding_logic.js");
  interest_group.trusted_bidding_signals_url =
      kUrlA.Resolve("/interest_group/trusted_bidding_signals.json");
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->push_back(std::move(ad));
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads didn't change.
  std::vector<BiddingInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].group->group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");
}

// Register a bid and a win, then perform a successful update. The bid and win
// stats shouldn't change.
TEST_F(RestrictedInterestGroupStoreImplTest, UpdateDoesntChangeBrowserSignals) {
  constexpr char kDailyUpdateUrlPath[] =
      "/interest_group/daily_update_partial.json";
  update_responder_->RegisterUpdateResponse(kDailyUpdateUrlPath,
                                            base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                                               kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUrlA.Resolve(kDailyUpdateUrlPath);
  interest_group.bidding_url =
      kUrlA.Resolve("/interest_group/bidding_logic.js");
  interest_group.trusted_bidding_signals_url =
      kUrlA.Resolve("/interest_group/trusted_bidding_signals.json");
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->push_back(std::move(ad));
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Register 2 bids and a win.
  manager_->RecordInterestGroupBid(kOriginA, kInterestGroupName);
  manager_->RecordInterestGroupBid(kOriginA, kInterestGroupName);
  manager_->RecordInterestGroupWin(kOriginA, kInterestGroupName, "{}");

  std::vector<BiddingInterestGroup> prev_groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(prev_groups.size(), 1u);
  const auto& prev_signals = prev_groups[0].group->signals;
  EXPECT_EQ(prev_signals->join_count, 1);
  EXPECT_EQ(prev_signals->bid_count, 2);
  EXPECT_EQ(prev_signals->prev_wins.size(), 1u);

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The group updates, but the signals don't.
  std::vector<BiddingInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].group->group;
  const auto& signals = groups[0].group->signals;

  EXPECT_EQ(signals->join_count, 1);
  EXPECT_EQ(signals->bid_count, 2);
  EXPECT_EQ(signals->prev_wins.size(), 1u);

  EXPECT_EQ(group.name, kInterestGroupName);
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(group.ads.value()[0].metadata, "{\"new_a\":\"b\"}");
}

}  // namespace content
