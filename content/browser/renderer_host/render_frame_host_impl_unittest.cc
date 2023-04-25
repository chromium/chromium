// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/cors_origin_pattern_setter.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/fake_local_frame.h"
#include "content/public/test/test_utils.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/mojom/cors.mojom.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_context.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_read_context.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/public/browser/authenticator_request_client_delegate.h"
#endif  // BUIDLFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
#include "content/browser/webauth/authenticator_environment.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom.h"
#endif

namespace content {

namespace {

void AddHostPermissions(const std::string& host, RenderFrameHost* rfh) {
  std::vector<network::mojom::CorsOriginPatternPtr> patterns;
  base::RunLoop run_loop;
  patterns.push_back(network::mojom::CorsOriginPattern::New(
      "https", host, 0, network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority));
  CorsOriginPatternSetter::Set(rfh->GetBrowserContext(),
                               rfh->GetLastCommittedOrigin(),
                               std::move(patterns), {}, run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace

class RenderFrameHostImplTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    contents()->GetPrimaryMainFrame()->InitializeRenderFrameIfNeeded();
  }
};

// TODO(https://crbug.com/1425337): This set-up is temporary. Eventually, all
// tests that reference extensions will be moved to chrome/browser/ and this
// class can be deleted.
class FirstPartyOverrideContentBrowserClient : public ContentBrowserClient {
 public:
  FirstPartyOverrideContentBrowserClient() = default;
  ~FirstPartyOverrideContentBrowserClient() override = default;

 private:
  bool ShouldUseFirstPartyStorageKey(const url::Origin& origin) override {
    return origin.scheme() == "chrome-extension";
  }
};

TEST_F(RenderFrameHostImplTest, ExpectedMainWorldOrigin) {
  GURL initial_url = GURL("https://initial.example.test/");
  GURL final_url = GURL("https://final.example.test/");

  auto get_expected_main_world_origin = [](RenderFrameHost* rfh) {
    NavigationRequest* in_flight_request =
        static_cast<RenderFrameHostImpl*>(rfh)
            ->FindLatestNavigationRequestThatIsStillCommitting();

    return in_flight_request ? in_flight_request->GetOriginToCommit()
                             : rfh->GetLastCommittedOrigin();
  };

  // Start the test with a simple navigation.
  {
    std::unique_ptr<NavigationSimulator> simulator =
        NavigationSimulator::CreateRendererInitiated(initial_url, main_rfh());
    simulator->Start();
    simulator->Commit();
  }
  RenderFrameHostImpl* initial_rfh = main_test_rfh();
  // This test is for a bug that only happens when there is no RFH swap on
  // same-site navigations, so we should disable same-site proactive
  // BrowsingInstance for |initial_rfh| before continiung.
  // Note: this will not disable RenderDocument.
  // TODO(crbug.com/936696): Skip this test when main-frame RenderDocument is
  // enabled.
  DisableProactiveBrowsingInstanceSwapFor(initial_rfh);
  // Verify expected main world origin in a steady state - after a commit it
  // should be the same as the last committed origin.
  EXPECT_EQ(url::Origin::Create(initial_url),
            get_expected_main_world_origin(main_rfh()));
  EXPECT_EQ(url::Origin::Create(initial_url),
            main_rfh()->GetLastCommittedOrigin());
  EXPECT_EQ(
      blink::StorageKey::CreateFirstParty(url::Origin::Create(initial_url)),
      main_test_rfh()->storage_key());

  // Verify expected main world origin when a pending navigation was started but
  // hasn't yet reached the ready-to-commit state.
  std::unique_ptr<NavigationSimulator> simulator2 =
      NavigationSimulator::CreateRendererInitiated(final_url, main_rfh());
  simulator2->Start();
  EXPECT_EQ(url::Origin::Create(initial_url),
            get_expected_main_world_origin(main_rfh()));

  // Verify expected main world origin when a pending navigation has reached the
  // ready-to-commit state.  Note that the last committed origin shouldn't
  // change yet at this point.
  simulator2->ReadyToCommit();
  simulator2->Wait();
  EXPECT_EQ(url::Origin::Create(final_url),
            get_expected_main_world_origin(main_rfh()));
  EXPECT_EQ(url::Origin::Create(initial_url),
            main_rfh()->GetLastCommittedOrigin());
  EXPECT_EQ(
      blink::StorageKey::CreateFirstParty(url::Origin::Create(initial_url)),
      main_test_rfh()->storage_key());

  // Verify expected main world origin once we are again in a steady state -
  // after a commit.
  simulator2->Commit();
  EXPECT_EQ(url::Origin::Create(final_url),
            get_expected_main_world_origin(main_rfh()));
  EXPECT_EQ(url::Origin::Create(final_url),
            main_rfh()->GetLastCommittedOrigin());
  EXPECT_EQ(blink::StorageKey::CreateFirstParty(url::Origin::Create(final_url)),
            main_test_rfh()->storage_key());

  // As a test correctness check, verify that there was no RFH swap (the bug
  // this test protects against would only happen if there is no swap).  In
  // fact, FindLatestNavigationRequestThatIsStillCommitting might possibly be
  // removed entirely once we swap on all document changes.
  EXPECT_EQ(initial_rfh, main_rfh());
}

// Ensures that IsolationInfo's SiteForCookies is empty and
// that it correctly generates a StorageKey with a kCrossSite
// AncestorChainBit when frames are nested in an A->B->A
// configuration.
TEST_F(RenderFrameHostImplTest, CrossSiteAncestorInFrameTree) {
  // Enable 3p partitioning to accurately test AncestorChainBit.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  // Load site A into the main frame.
  GURL parent_url = GURL("https://parent.example.test/");
  NavigationSimulator::CreateRendererInitiated(parent_url, main_rfh())
      ->Commit();

  // Create a child RenderFrameHost and navigate it to site B to establish A->B.
  auto* child_rfh_1 = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(main_test_rfh())
          ->AppendChild("child:a->b"));
  GURL child_url_1 = GURL("https://child.example.com");
  child_rfh_1 = static_cast<TestRenderFrameHost*>(
      NavigationSimulator::NavigateAndCommitFromDocument(child_url_1,
                                                         child_rfh_1));

  // Create a child RenderFrameHost in the existing child RenderFrameHost and
  // navigate it to site A to establish A->B->A.
  auto* child_rfh_2 = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(child_rfh_1)
          ->AppendChild("child:a->b->a"));
  child_rfh_2 = static_cast<TestRenderFrameHost*>(
      NavigationSimulator::NavigateAndCommitFromDocument(parent_url,
                                                         child_rfh_2));

  // Constructing expected values.
  url::Origin expected_final_origin = url::Origin::Create(parent_url);
  blink::StorageKey expected_final_storage_key = blink::StorageKey::Create(
      expected_final_origin, net::SchemefulSite(expected_final_origin),
      blink::mojom::AncestorChainBit::kCrossSite);
  // Set should contain the set of sites between the current and top frame.
  std::set<net::SchemefulSite> party_context = {
      net::SchemefulSite(child_url_1)};
  net::IsolationInfo expected_final_isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, expected_final_origin,
      expected_final_origin, net::SiteForCookies(), party_context);

  EXPECT_EQ(expected_final_origin, child_rfh_2->GetLastCommittedOrigin());
  EXPECT_EQ(expected_final_storage_key, child_rfh_2->storage_key());
  EXPECT_TRUE(expected_final_isolation_info.IsEqualForTesting(
      child_rfh_2->GetIsolationInfoForSubresources()));
  EXPECT_EQ(expected_final_isolation_info.network_isolation_key(),
            child_rfh_2->GetNetworkIsolationKey());
  EXPECT_TRUE(expected_final_isolation_info.site_for_cookies().IsEquivalent(
      child_rfh_2->ComputeSiteForCookies()));
  EXPECT_TRUE(expected_final_isolation_info.IsEqualForTesting(
      child_rfh_2->GetPendingIsolationInfoForSubresources()));
}

// Test the IsolationInfo and related fields of a request during the various
// phases of a commit, when a RenderFrameHost is reused. Once RenderDocument
// ships, this test may no longer be needed.
TEST_F(RenderFrameHostImplTest, IsolationInfoDuringCommit) {
  GURL initial_url = GURL("https://initial.example.test/");
  url::Origin expected_initial_origin = url::Origin::Create(initial_url);
  const blink::StorageKey expected_initial_storage_key =
      blink::StorageKey::CreateFirstParty(expected_initial_origin);
  net::IsolationInfo expected_initial_isolation_info =
      net::IsolationInfo::Create(
          net::IsolationInfo::RequestType::kOther, expected_initial_origin,
          expected_initial_origin,
          net::SiteForCookies::FromOrigin(expected_initial_origin),
          std::set<net::SchemefulSite>());

  GURL final_url = GURL("https://final.example.test/");
  url::Origin expected_final_origin = url::Origin::Create(final_url);
  const blink::StorageKey expected_final_storage_key =
      blink::StorageKey::CreateFirstParty(expected_final_origin);
  net::IsolationInfo expected_final_isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, expected_final_origin,
      expected_final_origin,
      net::SiteForCookies::FromOrigin(expected_final_origin),
      std::set<net::SchemefulSite>());

  // Start the test with a simple navigation.
  {
    std::unique_ptr<NavigationSimulator> simulator =
        NavigationSimulator::CreateRendererInitiated(initial_url, main_rfh());
    simulator->Start();
    simulator->Commit();
  }

  // This test is targetted at the case an RFH is reused between navigations.
  RenderFrameHost* initial_rfh = main_rfh();
  DisableProactiveBrowsingInstanceSwapFor(main_rfh());

  // Check values for the initial commit.
  EXPECT_EQ(expected_initial_origin, main_rfh()->GetLastCommittedOrigin());
  EXPECT_EQ(expected_initial_storage_key, main_test_rfh()->storage_key());
  EXPECT_TRUE(expected_initial_isolation_info.IsEqualForTesting(
      main_rfh()->GetIsolationInfoForSubresources()));
  EXPECT_EQ(expected_initial_isolation_info.network_isolation_key(),
            main_rfh()->GetNetworkIsolationKey());
  EXPECT_TRUE(expected_initial_isolation_info.site_for_cookies().IsEquivalent(
      static_cast<RenderFrameHostImpl*>(main_rfh())->ComputeSiteForCookies()));
  EXPECT_TRUE(expected_initial_isolation_info.IsEqualForTesting(
      main_rfh()->GetPendingIsolationInfoForSubresources()));

  // Values should be the same when a pending navigation was started but
  // hasn't yet reached the ready-to-commit state.
  std::unique_ptr<NavigationSimulator> simulator2 =
      NavigationSimulator::CreateRendererInitiated(final_url, main_rfh());
  simulator2->Start();
  EXPECT_EQ(expected_initial_origin, main_rfh()->GetLastCommittedOrigin());
  EXPECT_EQ(expected_initial_storage_key, main_test_rfh()->storage_key());
  EXPECT_TRUE(expected_initial_isolation_info.IsEqualForTesting(
      main_rfh()->GetIsolationInfoForSubresources()));
  EXPECT_EQ(expected_initial_isolation_info.network_isolation_key(),
            main_rfh()->GetNetworkIsolationKey());
  EXPECT_TRUE(expected_initial_isolation_info.site_for_cookies().IsEquivalent(
      static_cast<RenderFrameHostImpl*>(main_rfh())->ComputeSiteForCookies()));
  EXPECT_TRUE(expected_initial_isolation_info.IsEqualForTesting(
      main_rfh()->GetPendingIsolationInfoForSubresources()));

  // Only the GetPendingIsolationInfoForSubresources() should change when a
  // pending navigation has reached the ready-to-commit state.
  simulator2->ReadyToCommit();
  simulator2->Wait();
  EXPECT_EQ(expected_initial_origin, main_rfh()->GetLastCommittedOrigin());
  EXPECT_EQ(expected_initial_storage_key, main_test_rfh()->storage_key());
  EXPECT_TRUE(expected_initial_isolation_info.IsEqualForTesting(
      main_rfh()->GetIsolationInfoForSubresources()));
  EXPECT_EQ(expected_initial_isolation_info.network_isolation_key(),
            main_rfh()->GetNetworkIsolationKey());
  EXPECT_TRUE(expected_initial_isolation_info.site_for_cookies().IsEquivalent(
      static_cast<RenderFrameHostImpl*>(main_rfh())->ComputeSiteForCookies()));
  EXPECT_TRUE(expected_final_isolation_info.IsEqualForTesting(
      main_rfh()->GetPendingIsolationInfoForSubresources()));

  // Verify expected main world origin once we are again in a steady state -
  // after a commit.
  simulator2->Commit();
  EXPECT_EQ(expected_final_origin, main_rfh()->GetLastCommittedOrigin());
  EXPECT_EQ(expected_final_storage_key, main_test_rfh()->storage_key());
  EXPECT_TRUE(expected_final_isolation_info.IsEqualForTesting(
      main_rfh()->GetIsolationInfoForSubresources()));
  EXPECT_EQ(expected_final_isolation_info.network_isolation_key(),
            main_rfh()->GetNetworkIsolationKey());
  EXPECT_TRUE(expected_final_isolation_info.site_for_cookies().IsEquivalent(
      static_cast<RenderFrameHostImpl*>(main_rfh())->ComputeSiteForCookies()));
  EXPECT_TRUE(expected_final_isolation_info.IsEqualForTesting(
      main_rfh()->GetPendingIsolationInfoForSubresources()));

  // As a test correctness check, verify that there was no RFH swap. When
  // there's always an RFH swap, this test will likely no longer be useful.
  EXPECT_EQ(initial_rfh, main_rfh());
}

TEST_F(RenderFrameHostImplTest, PolicyContainerLifecycle) {
  TestRenderFrameHost* main_rfh = contents()->GetPrimaryMainFrame();
  ASSERT_NE(main_rfh->policy_container_host(), nullptr);
  EXPECT_EQ(main_rfh->policy_container_host()->referrer_policy(),
            network::mojom::ReferrerPolicy::kDefault);

  static_cast<blink::mojom::PolicyContainerHost*>(
      main_rfh->policy_container_host())
      ->SetReferrerPolicy(network::mojom::ReferrerPolicy::kAlways);
  EXPECT_EQ(main_rfh->policy_container_host()->referrer_policy(),
            network::mojom::ReferrerPolicy::kAlways);

  // Create a child frame and check that it inherits the PolicyContainerHost
  // from the parent frame.
  auto* child_frame = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(main_test_rfh())
          ->AppendChild("child"));

  ASSERT_NE(child_frame->policy_container_host(), nullptr);
  EXPECT_EQ(child_frame->policy_container_host()->referrer_policy(),
            network::mojom::ReferrerPolicy::kAlways);

  // Create a new WebContents with opener and test that the new main frame
  // inherits the PolicyContainerHost from the opener.
  static_cast<blink::mojom::PolicyContainerHost*>(
      child_frame->policy_container_host())
      ->SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever);
  WebContents::CreateParams params(browser_context());
  std::unique_ptr<WebContentsImpl> new_contents(
      WebContentsImpl::CreateWithOpener(params, child_frame));
  RenderFrameHostImpl* new_frame =
      new_contents->GetPrimaryFrameTree().root()->current_frame_host();

  ASSERT_NE(new_frame->policy_container_host(), nullptr);
  EXPECT_EQ(new_frame->policy_container_host()->referrer_policy(),
            network::mojom::ReferrerPolicy::kNever);
}

TEST_F(RenderFrameHostImplTest, FaviconURLsSet) {
  TestRenderFrameHost* main_rfh = contents()->GetPrimaryMainFrame();
  const auto kFavicon =
      blink::mojom::FaviconURL(GURL("https://example.com/favicon.ico"),
                               blink::mojom::FaviconIconType::kFavicon, {});
  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateBrowserInitiated(GURL("https://example.com"),
                                                  contents());
  ui::PageTransition transition = ui::PAGE_TRANSITION_LINK;
  navigation->SetTransition(transition);
  navigation->Commit();
  EXPECT_EQ(0u, contents()->GetFaviconURLs().size());

  std::vector<blink::mojom::FaviconURLPtr> one_favicon_url;
  one_favicon_url.push_back(blink::mojom::FaviconURL::New(kFavicon));
  main_rfh->UpdateFaviconURL(std::move(one_favicon_url));
  EXPECT_EQ(1u, contents()->GetFaviconURLs().size());

  std::vector<blink::mojom::FaviconURLPtr> two_favicon_urls;
  two_favicon_urls.push_back(blink::mojom::FaviconURL::New(kFavicon));
  two_favicon_urls.push_back(blink::mojom::FaviconURL::New(kFavicon));
  main_rfh->UpdateFaviconURL(std::move(two_favicon_urls));
  EXPECT_EQ(2u, contents()->GetFaviconURLs().size());

  std::vector<blink::mojom::FaviconURLPtr> another_one_favicon_url;
  another_one_favicon_url.push_back(blink::mojom::FaviconURL::New(kFavicon));
  main_rfh->UpdateFaviconURL(std::move(another_one_favicon_url));
  EXPECT_EQ(1u, contents()->GetFaviconURLs().size());
}

TEST_F(RenderFrameHostImplTest, FaviconURLsResetWithNavigation) {
  TestRenderFrameHost* main_rfh = contents()->GetPrimaryMainFrame();
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      GURL("https://example.com/favicon.ico"),
      blink::mojom::FaviconIconType::kFavicon, std::vector<gfx::Size>()));

  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateBrowserInitiated(GURL("https://example.com"),
                                                  contents());
  ui::PageTransition transition = ui::PAGE_TRANSITION_LINK;
  navigation->SetTransition(transition);
  navigation->Commit();

  EXPECT_EQ(0u, contents()->GetFaviconURLs().size());
  main_rfh->UpdateFaviconURL(std::move(favicon_urls));
  EXPECT_EQ(1u, contents()->GetFaviconURLs().size());

  navigation = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/navigation.html"), contents());
  navigation->SetTransition(transition);
  navigation->Commit();
  EXPECT_EQ(0u, contents()->GetFaviconURLs().size());
}

TEST_F(RenderFrameHostImplTest, ChildOfCredentiallessIsCredentialless) {
  EXPECT_FALSE(main_test_rfh()->IsCredentialless());

  auto* child_frame = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(main_test_rfh())
          ->AppendChild("child"));
  EXPECT_FALSE(child_frame->IsCredentialless());
  EXPECT_FALSE(child_frame->storage_key().nonce().has_value());

  auto attributes = blink::mojom::IframeAttributes::New();
  attributes->parsed_csp_attribute = std::move(
      child_frame->frame_tree_node()->attributes_->parsed_csp_attribute);
  attributes->id = child_frame->frame_tree_node()->html_id();
  attributes->name = child_frame->frame_tree_node()->html_name();
  attributes->src = child_frame->frame_tree_node()->html_src();
  attributes->credentialless = true;
  child_frame->frame_tree_node()->SetAttributes(std::move(attributes));

  EXPECT_FALSE(child_frame->IsCredentialless());
  EXPECT_FALSE(child_frame->storage_key().nonce().has_value());

  // A navigation in the credentialless iframe commits a credentialless RFH.
  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateRendererInitiated(
          GURL("https://example.com/navigation.html"), child_frame);
  navigation->Commit();
  child_frame =
      static_cast<TestRenderFrameHost*>(navigation->GetFinalRenderFrameHost());
  EXPECT_TRUE(child_frame->IsCredentialless());
  EXPECT_TRUE(child_frame->storage_key().nonce().has_value());

  // A credentialless document sets a nonce on its network isolation key.
  EXPECT_TRUE(child_frame->GetNetworkIsolationKey().GetNonce().has_value());
  EXPECT_EQ(main_test_rfh()->credentialless_iframes_nonce(),
            child_frame->GetNetworkIsolationKey().GetNonce().value());

  // A child of a credentialless RFH is credentialless.
  auto* grandchild_frame = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(child_frame)
          ->AppendChild("grandchild"));
  EXPECT_TRUE(grandchild_frame->IsCredentialless());
  EXPECT_TRUE(grandchild_frame->storage_key().nonce().has_value());

  // The two credentialless RFH's storage keys should have the same nonce.
  EXPECT_EQ(child_frame->storage_key().nonce().value(),
            grandchild_frame->storage_key().nonce().value());

  // Also the credentialless initial empty document sets a nonce on its network
  // isolation key.
  EXPECT_TRUE(
      grandchild_frame->GetNetworkIsolationKey().GetNonce().has_value());
  EXPECT_EQ(main_test_rfh()->credentialless_iframes_nonce(),
            grandchild_frame->GetNetworkIsolationKey().GetNonce().value());
}

// FakeLocalFrame implementation that records calls to BeforeUnload().
class FakeLocalFrameWithBeforeUnload : public content::FakeLocalFrame {
 public:
  explicit FakeLocalFrameWithBeforeUnload(TestRenderFrameHost* test_host) {
    Init(test_host->GetRemoteAssociatedInterfaces());
  }

  bool was_before_unload_called() const { return was_before_unload_called_; }

  void RunBeforeUnloadCallback() {
    ASSERT_TRUE(before_unload_callback_);
    std::move(before_unload_callback_)
        .Run(true, base::TimeTicks::Now(), base::TimeTicks::Now());
  }

  // FakeLocalFrame:
  void BeforeUnload(bool is_reload, BeforeUnloadCallback callback) override {
    was_before_unload_called_ = true;
    before_unload_callback_ = std::move(callback);
  }

 private:
  bool was_before_unload_called_ = false;
  BeforeUnloadCallback before_unload_callback_;
};

// Verifies BeforeUnload() is not sent to renderer if there is no before
// unload handler present.
TEST_F(RenderFrameHostImplTest, BeforeUnloadNotSentToRenderer) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {}, {features::kAvoidUnnecessaryBeforeUnloadCheckSync});
  FakeLocalFrameWithBeforeUnload local_frame(contents()->GetPrimaryMainFrame());
  auto simulator = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL("https://example.com/simple.html"), contents());
  simulator->set_block_invoking_before_unload_completed_callback(true);
  simulator->Start();
  EXPECT_TRUE(contents()
                  ->GetPrimaryMainFrame()
                  ->is_waiting_for_beforeunload_completion());
  EXPECT_FALSE(local_frame.was_before_unload_called());
  // This is necessary to trigger FakeLocalFrameWithBeforeUnload to be bound.
  contents()->GetPrimaryMainFrame()->FlushLocalFrameMessages();
  // This runs a MessageLoop, which also results in the PostTask() scheduled
  // completing.
  local_frame.FlushMessages();
  EXPECT_FALSE(local_frame.was_before_unload_called());
  // Because of the nested message loops run by the previous calls, the task
  // that RenderFrameHostImpl will have also completed.
  EXPECT_FALSE(contents()
                   ->GetPrimaryMainFrame()
                   ->is_waiting_for_beforeunload_completion());
}

class LoadingStateChangedDelegate : public WebContentsDelegate {
 public:
  void LoadingStateChanged(WebContents* source,
                           bool should_show_loading_ui) final {
    should_show_loading_ui_ = should_show_loading_ui;
  }

  bool should_show_loading_ui() { return should_show_loading_ui_; }

 private:
  bool should_show_loading_ui_ = false;
};

TEST_F(RenderFrameHostImplTest, NavigationApiInterceptShowLoadingUi) {
  // Initial commit.
  const GURL url1("http://foo");
  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());

  std::unique_ptr<LoadingStateChangedDelegate> delegate =
      std::make_unique<LoadingStateChangedDelegate>();
  contents()->SetDelegate(delegate.get());
  ASSERT_FALSE(delegate->should_show_loading_ui());
  ASSERT_FALSE(contents()->IsLoading());
  ASSERT_FALSE(contents()->ShouldShowLoadingUI());

  // Emulate navigateEvent.intercept().
  const GURL url2("http://foo#a");
  auto params = mojom::DidCommitProvisionalLoadParams::New();
  params->did_create_new_entry = false;
  params->url = url2;
  params->origin = url::Origin::Create(url2);
  params->referrer = blink::mojom::Referrer::New();
  params->transition = ui::PAGE_TRANSITION_LINK;
  params->should_update_history = true;
  params->method = "GET";
  params->page_state = blink::PageState::CreateFromURL(url2);
  params->post_id = -1;
  main_test_rfh()->SendDidCommitSameDocumentNavigation(
      std::move(params),
      blink::mojom::SameDocumentNavigationType::kNavigationApiIntercept,
      /*should_replace_current_entry=*/false);
  EXPECT_FALSE(delegate->should_show_loading_ui());
  EXPECT_TRUE(contents()->IsLoading());
  EXPECT_FALSE(contents()->ShouldShowLoadingUI());

  // After a delay, the NavigationApi sends a message to start the loading UI.
  // This delay is to prevent jitters due to short same-document navigations.
  main_test_rfh()->SendStartLoadingForAsyncNavigationApiCommit();

  // Once the delay has elapsed, navigateEvent.intercept() should leave
  // WebContents in the loading state and showing loading UI, unlike other
  // same-document navigations.
  EXPECT_TRUE(delegate->should_show_loading_ui());
  EXPECT_TRUE(contents()->IsLoading());
  EXPECT_TRUE(contents()->ShouldShowLoadingUI());
}

TEST_F(RenderFrameHostImplTest, NavigationApiInterceptBrowserInitiated) {
  // Initial commit.
  const GURL url1("http://foo");
  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());

  std::unique_ptr<LoadingStateChangedDelegate> delegate =
      std::make_unique<LoadingStateChangedDelegate>();
  contents()->SetDelegate(delegate.get());
  ASSERT_FALSE(delegate->should_show_loading_ui());
  ASSERT_FALSE(contents()->IsLoading());
  ASSERT_FALSE(contents()->ShouldShowLoadingUI());

  // Emulate navigateEvent.intercept().
  const GURL url2("http://foo#a");
  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateBrowserInitiated(url2, contents());
  navigation->Start();
  ASSERT_TRUE(contents()->IsLoading());
  ASSERT_FALSE(contents()->ShouldShowLoadingUI());

  auto params = mojom::DidCommitProvisionalLoadParams::New();
  params->did_create_new_entry = false;
  params->url = url2;
  params->origin = url::Origin::Create(url2);
  params->referrer = blink::mojom::Referrer::New();
  params->transition = ui::PAGE_TRANSITION_LINK;
  params->should_update_history = true;
  params->method = "GET";
  params->page_state = blink::PageState::CreateFromURL(url2);
  params->post_id = -1;
  main_test_rfh()->SendDidCommitSameDocumentNavigation(
      std::move(params),
      blink::mojom::SameDocumentNavigationType::kNavigationApiIntercept, true);
  EXPECT_FALSE(delegate->should_show_loading_ui());
  EXPECT_TRUE(contents()->IsLoading());
  EXPECT_FALSE(contents()->ShouldShowLoadingUI());

  // After a delay, the NavigationApi sends a message to start the loading UI.
  // This delay is to prevent jitters due to short same-document navigations.
  main_test_rfh()->SendStartLoadingForAsyncNavigationApiCommit();

  // Once the delay has elapsed, navigateEvent.intercept() should leave
  // WebContents in the loading state and showing loading UI, unlike other
  // same-document navigations.
  EXPECT_TRUE(delegate->should_show_loading_ui());
  EXPECT_TRUE(contents()->IsLoading());
  EXPECT_TRUE(contents()->ShouldShowLoadingUI());
}

// TODO(crbug.com/1425337): This test should be migrated to //chrome.
TEST_F(RenderFrameHostImplTest, CalculateStorageKey) {
  // Register extension scheme for testing.
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);

  GURL initial_url_ext = GURL("chrome-extension://initial.example.test/");
  NavigationSimulator::CreateRendererInitiated(initial_url_ext, main_rfh())
      ->Commit();

  // Create a child frame and navigate to `child_url`.
  auto* child_frame = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(main_test_rfh())
          ->AppendChild("child"));

  GURL child_url = GURL("https://childframe.com");
  child_frame = static_cast<TestRenderFrameHost*>(
      NavigationSimulator::NavigateAndCommitFromDocument(child_url,
                                                         child_frame));

  // Create a grandchild frame and navigate to `grandchild_url`.
  auto* grandchild_frame = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(child_frame)
          ->AppendChild("grandchild"));

  GURL grandchild_url = GURL("https://grandchildframe.com/");
  grandchild_frame = static_cast<TestRenderFrameHost*>(
      NavigationSimulator::NavigateAndCommitFromDocument(grandchild_url,
                                                         grandchild_frame));

  // With no host permissions the grandchild document should have a cross-site
  // storage key with the `initial_url_ext` as it's top level origin.
  blink::StorageKey expected_grandchild_no_permissions_storage_key =
      blink::StorageKey::Create(
          grandchild_frame->GetLastCommittedOrigin(),
          net::SchemefulSite(url::Origin::Create(initial_url_ext)),
          blink::mojom::AncestorChainBit::kCrossSite);

  EXPECT_EQ(expected_grandchild_no_permissions_storage_key,
            grandchild_frame->CalculateStorageKey(
                grandchild_frame->GetLastCommittedOrigin(), nullptr));

  // Give extension host permissions to `grandchild_frame`. Since
  // `grandchild_frame` is not the root non-extension frame
  // `CalculateStorageKey` should still create a storage key that has the
  // extension as the `top_level_site`.
  AddHostPermissions("grandchildframe.com", main_rfh());

  EXPECT_EQ(expected_grandchild_no_permissions_storage_key,
            grandchild_frame->CalculateStorageKey(
                grandchild_frame->GetLastCommittedOrigin(), nullptr));

  // Now give extension host permissions to `child_frame`. Since the root
  // extension rfh has host permissions to`child_frame` calling
  // `CalculateStorageKey` should create a storage key with the `child_origin`
  // as the `top_level_site`.
  AddHostPermissions("childframe.com", main_rfh());

  // Child host should now have a storage key that is same site and uses the
  // `child_origin` as the `top_level_site`.
  blink::StorageKey expected_child_with_permissions_storage_key =
      blink::StorageKey::Create(
          child_frame->GetLastCommittedOrigin(),
          net::SchemefulSite(child_frame->GetLastCommittedOrigin()),
          blink::mojom::AncestorChainBit::kSameSite);
  EXPECT_EQ(expected_child_with_permissions_storage_key,
            child_frame->CalculateStorageKey(
                child_frame->GetLastCommittedOrigin(), nullptr));

  blink::StorageKey expected_grandchild_with_permissions_storage_key =
      blink::StorageKey::Create(
          grandchild_frame->GetLastCommittedOrigin(),
          net::SchemefulSite(child_frame->GetLastCommittedOrigin()),
          blink::mojom::AncestorChainBit::kCrossSite);
  EXPECT_EQ(expected_grandchild_with_permissions_storage_key,
            grandchild_frame->CalculateStorageKey(
                grandchild_frame->GetLastCommittedOrigin(), nullptr));
}

// TODO(https://crbug.com/1425337): Eventually, this test will be moved to
// chrome/browser/ so that we no longer need to override the
// ContentBrowserClient, and we can test using real extension URLs.
TEST_F(RenderFrameHostImplTest, CalculateStorageKeyFirstPartyOverride) {
  // Enable third-party storage partitioning.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  // Temporarily enable FirstPartyOverrideContentBrowserClient. This allows us
  // to mock ShouldUseFirstPartyStorageKey() to check the origin scheme (as done
  // in the ChromeContentBrowserClient) rather than indiscriminately return
  // false (as written in the ContentBrowserClient implementation).
  FirstPartyOverrideContentBrowserClient modified_client;
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&modified_client);

  // Register extension scheme for testing.
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);

  // Navigate and commit to a non-extension URL.
  GURL initial_url("https://initial.example.test");
  NavigationSimulator::CreateRendererInitiated(initial_url, main_rfh())
      ->Commit();

  // Create a child extension frame and navigate to it.
  auto* child_frame = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(main_test_rfh())
          ->AppendChild("child"));

  // TODO(https://crbug.com/1425337): once this test is moved to chrome/browser/
  // replace with a legitimate chrome-extension URL. But for the purposes of
  // this test, it is sufficient to check that it has a chrome-extension scheme.
  GURL child_url = GURL("chrome-extension://childframeid");
  child_frame = static_cast<TestRenderFrameHost*>(
      NavigationSimulator::NavigateAndCommitFromDocument(child_url,
                                                         child_frame));

  // Subframes that contain extension URLs should have first-party StorageKeys.
  EXPECT_EQ(child_frame->GetLastCommittedOrigin().GetURL(), child_url);
  blink::StorageKey expected_storage_key = blink::StorageKey::CreateFirstParty(
      child_frame->GetLastCommittedOrigin());

  EXPECT_EQ(expected_storage_key,
            child_frame->CalculateStorageKey(
                child_frame->GetLastCommittedOrigin(), /*nonce=*/nullptr));

  SetBrowserClientForTesting(regular_client);
}

TEST_F(RenderFrameHostImplTest,
       CalculateStorageKeyWhenPassedOriginIsNotCurrentFrame) {
  // Register extension scheme for testing.
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);

  GURL initial_url_ext = GURL("chrome-extension://initial.example.test/");
  NavigationSimulator::CreateRendererInitiated(initial_url_ext, main_rfh())
      ->Commit();

  // Create a child frame and navigate to `child_url`.
  auto* child_frame = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(main_test_rfh())
          ->AppendChild("child"));

  GURL child_url = GURL("https://childframe.com");
  child_frame = static_cast<TestRenderFrameHost*>(
      NavigationSimulator::NavigateAndCommitFromDocument(child_url,
                                                         child_frame));

  // Give extension host permissions to `child_url`.
  AddHostPermissions("childframe.com", main_rfh());

  // The top level document has host permssions to the child_url so the top
  // level document should be excluded from storage key calculations and a first
  // party, same-site storage key is expected.
  blink::StorageKey expected_child_with_permissions_storage_key =
      blink::StorageKey::Create(
          child_frame->GetLastCommittedOrigin(),
          net::SchemefulSite(child_frame->GetLastCommittedOrigin()),
          blink::mojom::AncestorChainBit::kSameSite);
  EXPECT_EQ(expected_child_with_permissions_storage_key,
            child_frame->CalculateStorageKey(
                child_frame->GetLastCommittedOrigin(), nullptr));

  // CalculateStorageKey is called with an origin that the top level document
  // does not have host permissions to. A cross-site storage key is expected and
  // the top level document's site should be used in the storage key
  // calculation.
  GURL no_host_permissions_url = GURL("https://noHostPermissions.com/");
  blink::StorageKey expected_storage_key_no_permissions =
      blink::StorageKey::Create(
          url::Origin::Create(no_host_permissions_url),
          net::SchemefulSite(url::Origin::Create(initial_url_ext)),
          blink::mojom::AncestorChainBit::kCrossSite);
  EXPECT_EQ(expected_storage_key_no_permissions,
            child_frame->CalculateStorageKey(
                url::Origin::Create(no_host_permissions_url), nullptr));
}

// Test that the correct StorageKey is calculated when a RFH takes its document
// properties from a navigation.
// TODO(https://crbug.com/888079): Once we are able to compute the origin to
// commit in the browser, `navigation_request->commit_params().storage_key`
// will contain the correct origin and it won't be necessary to override it
// with `param.origin` anymore. Meaning this test may be removed because we
// already check that the NavigationRequest calculates the correct key.
TEST_F(RenderFrameHostImplTest,
       CalculateStorageKeyTakeNewDocumentPropertiesFromNavigation) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Because the StorageKey's (and Storage Partitioning's) usage of
  // RuntimeFeatureState is only meant to disable partitioning (i.e.:
  // first-party only), we need the make sure the net::features is always
  // enabled.
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  // This lamdba performs the navigation and disables Storage Partitioning for
  // the navigation if `disable_sp` is true. It returns the new
  // TestRenderFrameHost* to the navigated frame.
  auto NavigateFrame = [](NavigationSimulator* navigation,
                          bool disable_sp = false) -> TestRenderFrameHost* {
    navigation->Start();

    if (disable_sp) {
      NavigationRequest* request =
          NavigationRequest::From(navigation->GetNavigationHandle());
      // Disable Storage Partitioning by enabling the deprecation trial.
      request->GetMutableRuntimeFeatureStateContext()
          .SetDisableThirdPartyStoragePartitioningEnabled(true);
    }

    navigation->Commit();
    return static_cast<TestRenderFrameHost*>(
        navigation->GetFinalRenderFrameHost());
  };

  // Throughout the test we'll be creating a frame tree with a main frame, a
  // child frame, and a grandchild frame.
  GURL main_url("https://main.com");
  GURL b_url("https://b.com");
  GURL c_url("https://c.com");

  url::Origin main_origin = url::Origin::Create(main_url);
  url::Origin b_origin = url::Origin::Create(b_url);
  url::Origin c_origin = url::Origin::Create(c_url);

  // Begin by testing with Storage Partitioning enabled.

  auto main_navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(main_url, contents());

  // By definition the main frame's StorageKey will always be first party
  blink::StorageKey main_frame_key =
      blink::StorageKey::CreateFirstParty(main_origin);

  NavigateFrame(main_navigation.get());

  EXPECT_EQ(main_frame_key, main_test_rfh()->storage_key());

  TestRenderFrameHost* child_frame = static_cast<TestRenderFrameHost*>(
      RenderFrameHostTester::For(main_rfh())->AppendChild("child"));

  auto child_navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(b_url, child_frame);

  // The child and grandchild should both be third-party keys.
  blink::StorageKey child_frame_key =
      blink::StorageKey::Create(b_origin, net::SchemefulSite(main_origin),
                                blink::mojom::AncestorChainBit::kCrossSite);

  child_frame = NavigateFrame(child_navigation.get());

  EXPECT_EQ(child_frame_key, child_frame->storage_key());

  TestRenderFrameHost* grandchild_frame =
      child_frame->AppendChild("grandchild");

  auto grandchild_navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(c_url, grandchild_frame);

  blink::StorageKey grandchild_frame_key =
      blink::StorageKey::Create(c_origin, net::SchemefulSite(main_origin),
                                blink::mojom::AncestorChainBit::kCrossSite);
  grandchild_frame = NavigateFrame(grandchild_navigation.get());

  EXPECT_EQ(grandchild_frame_key, grandchild_frame->storage_key());

  // Only the RuntimeFeatureStateContext in the main frame's matters. So
  // disabling Storage Partitioning in the child_frame shouldn't affect the
  // child's or the grandchild's StorageKey.
  child_navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(b_url, child_frame);

  child_frame = NavigateFrame(child_navigation.get(),
                              /*disable_sp=*/true);
  EXPECT_EQ(child_frame_key, child_frame->storage_key());

  grandchild_frame = child_frame->AppendChild("grandchild");

  grandchild_navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(c_url, grandchild_frame);

  grandchild_frame = NavigateFrame(grandchild_navigation.get());

  EXPECT_EQ(grandchild_frame_key, grandchild_frame->storage_key());

  // Disabling Storage Partitioning on the main frame should cause the child's
  // and grandchild's StorageKey to be first-party.
  main_navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(main_url, contents());

  NavigateFrame(main_navigation.get(),
                /*disable_sp=*/true);

  child_frame = static_cast<TestRenderFrameHost*>(
      RenderFrameHostTester::For(main_rfh())->AppendChild("child"));

  child_navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(b_url, child_frame);

  // The child and grandchild should both be first-party keys.
  blink::StorageKey child_frame_key_1p =
      blink::StorageKey::CreateFirstParty(b_origin);

  child_frame = NavigateFrame(child_navigation.get());

  EXPECT_EQ(child_frame_key_1p, child_frame->storage_key());

  grandchild_frame = child_frame->AppendChild("grandchild");

  blink::StorageKey grandchild_frame_key_1p =
      blink::StorageKey::CreateFirstParty(c_origin);

  grandchild_navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(c_url, grandchild_frame);

  grandchild_frame = NavigateFrame(grandchild_navigation.get());

  EXPECT_EQ(grandchild_frame_key_1p, grandchild_frame->storage_key());
}

// Tests that the StorageKey calculated for a frame under an extension main
// frame has storage partitioning enabled/disabled as expected via the
// RuntimeFeatureStateReadContext when the extension has host permissions.
// TODO(crbug.com/1425337): This test should be migrated to //chrome.
TEST_F(RenderFrameHostImplTest,
       CalculateStorageKeyStoragePartitioningCorrectFrameWithExtension) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Because the StorageKey's (and Storage Partitioning's) usage of
  // RuntimeFeatureState is only meant to disable partitioning (i.e.:
  // first-party only), we need the make sure the net::features is always
  // enabled.
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  // Register extension scheme for testing.
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);

  GURL initial_url_ext = GURL("chrome-extension://initial.example.test/");
  NavigationSimulator::CreateRendererInitiated(initial_url_ext, main_rfh())
      ->Commit();

  // Create a child frame, disable Storage Partitioning, and navigate to
  // `child_url`.
  auto* child_frame = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(main_test_rfh())
          ->AppendChild("child"));

  GURL child_url = GURL("https://childframe.com");
  auto child_navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(child_url, child_frame);

  // This lamdba performs the navigation and disables Storage Partitioning for
  // the navigation if `disable_sp` is true. It returns the new
  // TestRenderFrameHost* to the navigated frame.
  auto NavigateFrame = [](NavigationSimulator* navigation,
                          bool disable_sp = false) -> TestRenderFrameHost* {
    navigation->Start();

    if (disable_sp) {
      NavigationRequest* request =
          NavigationRequest::From(navigation->GetNavigationHandle());
      request->GetMutableRuntimeFeatureStateContext()
          .SetDisableThirdPartyStoragePartitioningEnabled(true);
    }

    navigation->Commit();
    return static_cast<TestRenderFrameHost*>(
        navigation->GetFinalRenderFrameHost());
  };

  child_frame = NavigateFrame(child_navigation.get(), /*disable_sp=*/true);

  // Create a grandchild frame and navigate to `grandchild_url`.
  auto* grandchild_frame = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(child_frame)
          ->AppendChild("grandchild"));

  GURL grandchild_url = GURL("https://grandchildframe.com/");
  grandchild_frame = static_cast<TestRenderFrameHost*>(
      NavigationSimulator::NavigateAndCommitFromDocument(grandchild_url,
                                                         grandchild_frame));

  // At this point the extension doesn't have host permissions for the
  // child_frame, so the child_frame's RuntimeFeatureStateReadContext's state
  // isn't used and therefore any StorageKeys created for the grandchild_frame
  // will be third-party.
  url::Origin grandchild_origin = url::Origin::Create(grandchild_url);
  blink::StorageKey grandchild_frame_key = blink::StorageKey::Create(
      grandchild_origin, net::SchemefulSite(initial_url_ext),
      blink::mojom::AncestorChainBit::kCrossSite);

  EXPECT_EQ(grandchild_frame_key,
            grandchild_frame->CalculateStorageKey(
                grandchild_frame->GetLastCommittedOrigin(), nullptr));

  // Now give extension host permissions to `child_frame`. Since the root
  // extension rfh has host permissions to`child_frame` calling
  // `CalculateStorageKey` should use the RuntimeFeatureStateReadContext in
  // child_frame thereby creating a first-party StorageKey in grandchild_frame
  // (since we disabled storage partitioning).
  AddHostPermissions("childframe.com", main_rfh());

  blink::StorageKey grandchild_frame_key_1P =
      blink::StorageKey::CreateFirstParty(grandchild_origin);

  EXPECT_EQ(grandchild_frame_key_1P,
            grandchild_frame->CalculateStorageKey(
                grandchild_frame->GetLastCommittedOrigin(), nullptr));
}

// Test that CalculateStorageKey creates a first-party or third-party key
// depending on state of Storage Partitioning the main frame's
// RuntimeFeatureStateReadContext for a new unnavigated frame.
TEST_F(RenderFrameHostImplTest, CalculateStorageKeyOfUnnavigatedFrame) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Because Storage partitioning's usage of RuntimeFeatureState is only meant
  // to disable (i.e.: 1p only) partitioning, we need the make sure the feature
  // is on first.
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  // This test will create a main frame that has Storage Partitioning disabled
  // via its RuntimeFeatureStateReadContext. It will have a navigated child
  // frame's whose RFSRC will be the default (i.e.: Storage Partitioning
  // enabled) and that child will then spawn an unnavigated grandchild whose
  // StorageKey should still depend upon the main frame's RFSRC.

  GURL url = GURL("https://a.com");
  GURL child_url = GURL("https://b.com");

  // Start by giving the main frame a SP disabled
  // RuntimeFeatureStateReadContext.
  auto navigation =
      NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  navigation->Start();

  NavigationRequest* request =
      NavigationRequest::From(navigation->GetNavigationHandle());

  // Disable Storage Partitioning by enabling the deprecation trial.
  request->GetMutableRuntimeFeatureStateContext()
      .SetDisableThirdPartyStoragePartitioningEnabled(true);

  navigation->Commit();

  EXPECT_TRUE(RuntimeFeatureStateDocumentData::GetForCurrentDocument(main_rfh())
                  ->runtime_feature_state_read_context()
                  .IsDisableThirdPartyStoragePartitioningEnabled());

  // Create a child frame and navigate to `child_url`.
  auto* child_frame = main_test_rfh()->AppendChild("child");
  auto child_navigation =
      NavigationSimulator::CreateRendererInitiated(child_url, child_frame);
  child_navigation->Commit();
  child_frame = static_cast<TestRenderFrameHost*>(
      child_navigation->GetFinalRenderFrameHost());

  // Create a grand child and check it's StorageKey.
  auto* grandchild_frame = child_frame->AppendChild("grandchild");

  // Since Storage Partitioning is disabled, the key should be first party.
  blink::StorageKey grandchild_frame_key_1p =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(child_url));
  EXPECT_EQ(grandchild_frame_key_1p, grandchild_frame->storage_key());

  // Now perform the same test, except the main frame also gets a default
  // RuntimeFeatureStateReadContext. (I.e.: Storage Partitioning enabled).
  NavigationSimulator::NavigateAndCommitFromDocument(url, main_rfh());

  child_frame = main_test_rfh()->AppendChild("child");
  child_navigation =
      NavigationSimulator::CreateRendererInitiated(child_url, child_frame);
  child_navigation->Commit();
  child_frame = static_cast<TestRenderFrameHost*>(
      child_navigation->GetFinalRenderFrameHost());

  grandchild_frame = child_frame->AppendChild("grandchild");

  blink::StorageKey grandchild_frame_key =
      blink::StorageKey::Create(url::Origin::Create(child_url),
                                net::SchemefulSite(url::Origin::Create(url)),
                                blink::mojom::AncestorChainBit::kCrossSite);
  EXPECT_EQ(grandchild_frame_key, grandchild_frame->storage_key());
}

TEST_F(RenderFrameHostImplTest,
       NewFrameInheritsRuntimeFeatureStateReadContext) {
  GURL url = GURL("https://a.com");
  GURL child_url = GURL("https://b.com");

  // Start by giving the main frame a non-default
  // RuntimeFeatureStateReadContext.

  auto navigation =
      NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  navigation->Start();

  NavigationRequest* request =
      NavigationRequest::From(navigation->GetNavigationHandle());

  request->GetMutableRuntimeFeatureStateContext().SetTestFeatureEnabled(true);

  navigation->Commit();

  EXPECT_TRUE(RuntimeFeatureStateDocumentData::GetForCurrentDocument(main_rfh())
                  ->runtime_feature_state_read_context()
                  .IsTestFeatureEnabled());

  // Now add a child and check its RFSRC.
  auto* child_frame = main_test_rfh()->AppendChild("child");
  EXPECT_TRUE(
      RuntimeFeatureStateDocumentData::GetForCurrentDocument(child_frame)
          ->runtime_feature_state_read_context()
          .IsTestFeatureEnabled());

  // Navigating the child away should change the RFSRC.
  auto child_navigation =
      NavigationSimulator::CreateRendererInitiated(child_url, child_frame);
  child_navigation->Commit();
  child_frame = static_cast<TestRenderFrameHost*>(
      child_navigation->GetFinalRenderFrameHost());
  EXPECT_FALSE(
      RuntimeFeatureStateDocumentData::GetForCurrentDocument(child_frame)
          ->runtime_feature_state_read_context()
          .IsTestFeatureEnabled());
}

#if BUILDFLAG(IS_ANDROID)
class TestWebAuthenticationDelegate : public WebAuthenticationDelegate {
 public:
  MOCK_METHOD(bool,
              IsSecurityLevelAcceptableForWebAuthn,
              (RenderFrameHost*, const url::Origin& origin),
              ());
};

class TestWebAuthnContentBrowserClientImpl : public ContentBrowserClient {
 public:
  explicit TestWebAuthnContentBrowserClientImpl(
      TestWebAuthenticationDelegate* delegate)
      : delegate_(delegate) {}

  WebAuthenticationDelegate* GetWebAuthenticationDelegate() override {
    return delegate_;
  }

 private:
  raw_ptr<TestWebAuthenticationDelegate> delegate_;
};

class RenderFrameHostImplWebAuthnTest : public RenderFrameHostImplTest {
 public:
  void SetUp() override {
    RenderFrameHostImplTest::SetUp();
    browser_client_ = std::make_unique<TestWebAuthnContentBrowserClientImpl>(
        webauthn_delegate_.get());
    old_browser_client_ = SetBrowserClientForTesting(browser_client_.get());
    contents()->GetController().LoadURLWithParams(
        NavigationController::LoadURLParams(
            GURL("https://example.com/navigation.html")));
  }

  void TearDown() override {
    RenderFrameHostImplTest::TearDown();
    SetBrowserClientForTesting(old_browser_client_);
  }

 protected:
  raw_ptr<ContentBrowserClient> old_browser_client_;
  std::unique_ptr<TestWebAuthnContentBrowserClientImpl> browser_client_;
  std::unique_ptr<TestWebAuthenticationDelegate> webauthn_delegate_ =
      std::make_unique<TestWebAuthenticationDelegate>();
};

TEST_F(RenderFrameHostImplWebAuthnTest,
       PerformGetAssertionWebAuthSecurityChecks_TLSError) {
  GURL url("https://doofenshmirtz.evil");
  const auto origin = url::Origin::Create(url);
  EXPECT_CALL(*webauthn_delegate_,
              IsSecurityLevelAcceptableForWebAuthn(main_test_rfh(), origin))
      .WillOnce(testing::Return(false));
  std::pair<blink::mojom::AuthenticatorStatus, bool> result =
      main_test_rfh()->PerformGetAssertionWebAuthSecurityChecks(
          "doofenshmirtz.evil", url::Origin::Create(url),
          /*is_payment_credential_get_assertion=*/false,
          /*remote_desktop_client_override=*/nullptr);
  EXPECT_EQ(std::get<blink::mojom::AuthenticatorStatus>(result),
            blink::mojom::AuthenticatorStatus::CERTIFICATE_ERROR);
}

TEST_F(RenderFrameHostImplWebAuthnTest,
       PerformMakeCredentialWebAuthSecurityChecks_TLSError) {
  GURL url("https://doofenshmirtz.evil");
  const auto origin = url::Origin::Create(url);
  EXPECT_CALL(*webauthn_delegate_,
              IsSecurityLevelAcceptableForWebAuthn(main_test_rfh(), origin))
      .WillOnce(testing::Return(false));
  blink::mojom::AuthenticatorStatus result =
      main_test_rfh()->PerformMakeCredentialWebAuthSecurityChecks(
          "doofenshmirtz.evil", url::Origin::Create(url),
          /*is_payment_credential_creation=*/false,
          /*remote_desktop_client_override=*/nullptr);
  EXPECT_EQ(result, blink::mojom::AuthenticatorStatus::CERTIFICATE_ERROR);
}

TEST_F(RenderFrameHostImplWebAuthnTest,
       PerformGetAssertionWebAuthSecurityChecks_Success) {
  GURL url("https://owca.org");
  const auto origin = url::Origin::Create(url);
  EXPECT_CALL(*webauthn_delegate_,
              IsSecurityLevelAcceptableForWebAuthn(main_test_rfh(), origin))
      .WillOnce(testing::Return(true));
  std::pair<blink::mojom::AuthenticatorStatus, bool> result =
      main_test_rfh()->PerformGetAssertionWebAuthSecurityChecks(
          "owca.org", url::Origin::Create(url),
          /*is_payment_credential_get_assertion=*/false,
          /*remote_desktop_client_override=*/nullptr);
  EXPECT_EQ(std::get<blink::mojom::AuthenticatorStatus>(result),
            blink::mojom::AuthenticatorStatus::SUCCESS);
}

TEST_F(RenderFrameHostImplWebAuthnTest,
       PerformMakeCredentialWebAuthSecurityChecks_Success) {
  GURL url("https://owca.org");
  const auto origin = url::Origin::Create(url);
  EXPECT_CALL(*webauthn_delegate_,
              IsSecurityLevelAcceptableForWebAuthn(main_test_rfh(), origin))
      .WillOnce(testing::Return(true));
  blink::mojom::AuthenticatorStatus result =
      main_test_rfh()->PerformMakeCredentialWebAuthSecurityChecks(
          "owca.org", url::Origin::Create(url),
          /*is_payment_credential_creation=*/false,
          /*remote_desktop_client_override=*/nullptr);
  EXPECT_EQ(result, blink::mojom::AuthenticatorStatus::SUCCESS);
}

#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(RenderFrameHostImplTest, NoBeforeUnloadCheckForBrowserInitiated) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAvoidUnnecessaryBeforeUnloadCheckSync);
  contents()->GetController().LoadURLWithParams(
      NavigationController::LoadURLParams(
          GURL("https://example.com/navigation.html")));
  EXPECT_FALSE(contents()
                   ->GetPrimaryMainFrame()
                   ->is_waiting_for_beforeunload_completion());
}

TEST_F(RenderFrameHostImplTest,
       NoBeforeUnloadCheckForBrowserInitiatedSyncTakesPrecedence) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kAvoidUnnecessaryBeforeUnloadCheckSync}, {});
  contents()->GetController().LoadURLWithParams(
      NavigationController::LoadURLParams(
          GURL("https://example.com/navigation.html")));
  EXPECT_FALSE(contents()
                   ->GetPrimaryMainFrame()
                   ->is_waiting_for_beforeunload_completion());
}

// ContentBrowserClient::SupportsAvoidUnnecessaryBeforeUnloadCheckSync() is
// android specific.
#if BUILDFLAG(IS_ANDROID)
class TestContentBrowserClientImpl : public ContentBrowserClient {
  bool SupportsAvoidUnnecessaryBeforeUnloadCheckSync() override {
    return false;
  }
};

TEST_F(RenderFrameHostImplTest,
       SupportsAvoidUnnecessaryBeforeUnloadCheckSyncReturnsFalse) {
  TestContentBrowserClientImpl browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&browser_client);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAvoidUnnecessaryBeforeUnloadCheckSync);
  contents()->GetController().LoadURLWithParams(
      NavigationController::LoadURLParams(
          GURL("https://example.com/navigation.html")));
  // Should be waiting on beforeunload as
  // SupportsAvoidUnnecessaryBeforeUnloadCheckSync() takes
  // precedence.
  EXPECT_TRUE(contents()
                  ->GetPrimaryMainFrame()
                  ->is_waiting_for_beforeunload_completion());
  SetBrowserClientForTesting(old_browser_client);
}
#endif

TEST_F(RenderFrameHostImplTest, BeforeUnloadCheckForBrowserInitiated) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAvoidUnnecessaryBeforeUnloadCheckSync);
  contents()->GetController().LoadURLWithParams(
      NavigationController::LoadURLParams(
          GURL("https://example.com/navigation.html")));
  EXPECT_TRUE(contents()
                  ->GetPrimaryMainFrame()
                  ->is_waiting_for_beforeunload_completion());
}

// Tests GetCookieSettingOverrides on a committed frame, and implicitly tests
// SubresourceLoaderFactoriesConfig::ForLastCommittedNavigation.
// TODO(crbug.com/1386190): Also add browser tests before the user bypass
// BREF is actually turned on anywhere.
TEST_F(RenderFrameHostImplTest, GetCookieSettingOverrides) {
  GURL url = GURL("https://example.test/");
  GURL child_url = GURL("https://example.test/child");

  // Start a navigation that has the ThirdPartyCookiesUserBypass feature
  // enabled (the only feature pertaining to CookieSettingOverrides currently).
  std::unique_ptr<NavigationSimulator> simulator =
      NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  simulator->Start();

  // Set user bypass BREF and prepare to commit.
  {
    blink::RuntimeFeatureStateContext& context =
        NavigationRequest::From(simulator->GetNavigationHandle())
            ->GetMutableRuntimeFeatureStateContext();
    context.SetThirdPartyCookiesUserBypassEnabled(true);
  }
  simulator->ReadyToCommit();

  // Verify that calling GetCookieSettingOverrides before the main frame
  // is at least committing will return an empty set. This corresponds to
  // scenarios like an empty initial document.
  {
    auto* rfh = static_cast<TestRenderFrameHost*>(
        simulator->GetNavigationHandle()->GetRenderFrameHost());
    EXPECT_TRUE(rfh->GetCookieSettingOverrides().Empty());

    // Check that a child frame gives the same result.
    EXPECT_TRUE(
        rfh->AppendChild("child1")->GetCookieSettingOverrides().Empty());
  }

  // Commit the navigation and check that GetCookieSettingOverrides now
  // returns the correct override.
  simulator->Commit();
  {
    auto* rfh =
        static_cast<TestRenderFrameHost*>(simulator->GetFinalRenderFrameHost());
    EXPECT_TRUE(rfh->GetCookieSettingOverrides().Has(
        net::CookieSettingOverride::kForceThirdPartyByUser));

    // Check that the child frame gives the same result.
    EXPECT_EQ(rfh->GetCookieSettingOverrides(),
              rfh->AppendChild("child2")->GetCookieSettingOverrides());
  }

  // Start a different navigation on a new child frame and verify that the
  // CookieSettingOverrides come from committed main frame.
  {
    auto* main_rfh =
        static_cast<TestRenderFrameHost*>(simulator->GetFinalRenderFrameHost());
    TestRenderFrameHost* child_rfh = main_rfh->AppendChild("child3");
    std::unique_ptr<NavigationSimulator> child_simulator =
        NavigationSimulator::CreateRendererInitiated(child_url, child_rfh);
    child_simulator->Start();
    blink::RuntimeFeatureStateContext& context =
        NavigationRequest::From(child_simulator->GetNavigationHandle())
            ->GetMutableRuntimeFeatureStateContext();
    context.SetThirdPartyCookiesUserBypassEnabled(false);
    child_simulator->ReadyToCommit();
    EXPECT_TRUE(child_rfh->GetCookieSettingOverrides().Has(
        net::CookieSettingOverride::kForceThirdPartyByUser));
    child_simulator->Commit();
    EXPECT_TRUE(child_rfh->GetCookieSettingOverrides().Has(
        net::CookieSettingOverride::kForceThirdPartyByUser));
  }
}

TEST_F(RenderFrameHostImplTest, GetIsThirdPartyCookiesUserBypassEnabled) {
  GURL url = GURL("https://example.test/");
  GURL child_url = GURL("https://example.test/child");

  std::unique_ptr<NavigationSimulator> simulator =
      NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  simulator->Start();

  // Set user bypass BREF and prepare to commit.
  {
    blink::RuntimeFeatureStateContext& context =
        NavigationRequest::From(simulator->GetNavigationHandle())
            ->GetMutableRuntimeFeatureStateContext();
    context.SetThirdPartyCookiesUserBypassEnabled(true);
  }
  simulator->ReadyToCommit();

  // Verify that GetIsThirdPartyCookiesUserBypassEnabled returns false
  // (because the BREF is not yet committed).
  {
    auto* rfh = static_cast<TestRenderFrameHost*>(
        simulator->GetNavigationHandle()->GetRenderFrameHost());
    EXPECT_FALSE(rfh->GetIsThirdPartyCookiesUserBypassEnabled());

    // Check that a child frame gives the same result.
    EXPECT_FALSE(
        rfh->AppendChild("child1")->GetIsThirdPartyCookiesUserBypassEnabled());
  }

  // Commit the navigation and check that
  // GetIsThirdPartyCookiesUserBypassEnabled now returns true.
  simulator->Commit();
  {
    auto* rfh =
        static_cast<TestRenderFrameHost*>(simulator->GetFinalRenderFrameHost());
    EXPECT_TRUE(rfh->GetIsThirdPartyCookiesUserBypassEnabled());

    // Check that a child frame gives the same result.
    EXPECT_TRUE(
        rfh->AppendChild("child1")->GetIsThirdPartyCookiesUserBypassEnabled());
  }

  // Start a different navigation on a new child frame and verify that the
  // value comes from the committed main frame.
  {
    auto* main_rfh =
        static_cast<TestRenderFrameHost*>(simulator->GetFinalRenderFrameHost());
    TestRenderFrameHost* child_rfh = main_rfh->AppendChild("child3");
    std::unique_ptr<NavigationSimulator> child_simulator =
        NavigationSimulator::CreateRendererInitiated(child_url, child_rfh);
    child_simulator->Start();
    blink::RuntimeFeatureStateContext& context =
        NavigationRequest::From(child_simulator->GetNavigationHandle())
            ->GetMutableRuntimeFeatureStateContext();
    context.SetThirdPartyCookiesUserBypassEnabled(false);
    child_simulator->ReadyToCommit();
    EXPECT_TRUE(child_rfh->GetIsThirdPartyCookiesUserBypassEnabled());
    child_simulator->Commit();
    EXPECT_TRUE(child_rfh->GetIsThirdPartyCookiesUserBypassEnabled());
  }
}

class RenderFrameHostImplThirdPartyStorageTest
    : public RenderViewHostImplTestHarness,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    contents()->GetPrimaryMainFrame()->InitializeRenderFrameIfNeeded();
    if (ThirdPartyStoragePartitioningEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(
          net::features::kThirdPartyStoragePartitioning);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          net::features::kThirdPartyStoragePartitioning);
    }
  }
  bool ThirdPartyStoragePartitioningEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    RenderFrameHostImplThirdPartyStorageTest,
    /*third_party_storage_partitioning_enabled*/ testing::Bool());

TEST_P(RenderFrameHostImplThirdPartyStorageTest,
       ChildFramePartitionedByThirdPartyStorageKey) {
  GURL initial_url = GURL("https://initial.example.test/");

  NavigationSimulator::CreateRendererInitiated(initial_url, main_rfh())
      ->Commit();

  // Create a child frame and check that it has the correct storage key.
  auto* child_frame = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(main_test_rfh())
          ->AppendChild("child"));

  GURL child_url = GURL("https://exampleChildSite.com");
  child_frame = static_cast<TestRenderFrameHost*>(
      NavigationSimulator::NavigateAndCommitFromDocument(child_url,
                                                         child_frame));

  // Top level storage key should not change if third party partitioning is on
  // or off
  EXPECT_EQ(
      blink::StorageKey::CreateFirstParty(url::Origin::Create(initial_url)),
      main_test_rfh()->storage_key());

  if (ThirdPartyStoragePartitioningEnabled()) {
    // child frame storage key should contain child_origin + top_level_origin if
    // third party partitioning is on.
    EXPECT_EQ(blink::StorageKey::Create(
                  url::Origin::Create(child_url),
                  net::SchemefulSite(url::Origin::Create(initial_url)),
                  blink::mojom::AncestorChainBit::kCrossSite),
              child_frame->storage_key());
  } else {
    // child frame storage key should only be partitioned by child origin if
    // third party partitioning is off.
    EXPECT_EQ(
        blink::StorageKey::CreateFirstParty(url::Origin::Create(child_url)),
        child_frame->storage_key());
  }
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(RenderFrameHostImplTest, GetVirtualAuthenticatorManagerWhenInactiveRFH) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableWebAuthDeprecatedMojoTestingApi);

  // Enable a back forward cache.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      GetBasicBackForwardCacheFeatureForTesting(),
      GetDefaultDisabledBackForwardCacheFeaturesForTesting());

  // Create a page with an iframe:
  contents()->NavigateAndCommit(GURL("https://initial.example.test/"));

  RenderFrameHostImpl* parent_rfh = main_test_rfh();
  RenderFrameHostImpl* child_rfh = static_cast<RenderFrameHostImpl*>(
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://childframe.com"),
          RenderFrameHostTester::For(parent_rfh)->AppendChild("child")));
  EXPECT_TRUE(child_rfh->IsActive());

  // The active child document should enable VirtualAuthenticator.
  {
    mojo::Remote<blink::test::mojom::VirtualAuthenticatorManager> remote;
    child_rfh->GetVirtualAuthenticatorManager(
        remote.BindNewPipeAndPassReceiver());
    EXPECT_TRUE(AuthenticatorEnvironment::GetInstance()
                    ->IsVirtualAuthenticatorEnabledFor(
                        contents()->GetPrimaryFrameTree().root()->child_at(0)));
  }

  // Navigate to another page, causing the two RenderFrameHost to become
  // inactive.
  RenderFrameDeletedObserver parent_rfh_deleted(parent_rfh);
  auto navigation = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL("https://final.example.test/"), contents());
  navigation->set_drop_unload_ack(true);
  navigation->Commit();
  ASSERT_FALSE(parent_rfh_deleted.deleted());
  EXPECT_FALSE(parent_rfh->IsActive());

  // The inactive document should not enable VirtualAuthenticator.
  {
    mojo::Remote<blink::test::mojom::VirtualAuthenticatorManager> remote;
    child_rfh->GetVirtualAuthenticatorManager(
        remote.BindNewPipeAndPassReceiver());
    EXPECT_FALSE(AuthenticatorEnvironment::GetInstance()
                     ->IsVirtualAuthenticatorEnabledFor(
                         contents()->GetPrimaryFrameTree().root()));
  }
}
#endif

namespace {

class MockWebContentsDelegate : public WebContentsDelegate {
 public:
  MOCK_METHOD(void, CloseContents, (WebContents*));
};

}  // namespace

// Ensure that a close request from the renderer process is ignored if a
// navigation causes a different RenderFrameHost to commit first. See
// https://crbug.com/1406023.
TEST_F(RenderFrameHostImplTest,
       RendererInitiatedCloseIsCancelledIfPageIsntPrimary) {
  MockWebContentsDelegate delegate;
  contents()->SetDelegate(&delegate);

  RenderFrameHostImpl* rfh = main_test_rfh();
  EXPECT_CALL(delegate, CloseContents(contents())).Times(0);

  // Have the renderer request to close the page.
  rfh->ClosePage(RenderFrameHostImpl::ClosePageSource::kRenderer);

  // The close timeout should be running.
  EXPECT_TRUE(rfh->close_timeout_ && rfh->close_timeout_->IsRunning());

  // Simulate the rfh going into the back-forward cache before the close timeout
  // fires.
  rfh->lifecycle_state_ =
      RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache;

  // Simulate the close timer firing.
  rfh->ClosePageTimeout(RenderFrameHostImpl::ClosePageSource::kRenderer);

  // The page should not close since it's no longer the primary page.
  testing::Mock::VerifyAndClearExpectations(&delegate);
}

// Ensure that a close request from the browser process cannot be ignored even
// if a navigation causes a different RenderFrameHost to commit first. See
// https://crbug.com/1406023.
TEST_F(RenderFrameHostImplTest,
       BrowserInitiatedCloseIsNotCancelledIfPageIsntPrimary) {
  MockWebContentsDelegate delegate;
  contents()->SetDelegate(&delegate);

  RenderFrameHostImpl* rfh = main_test_rfh();
  EXPECT_CALL(delegate, CloseContents(contents()));

  // Have the browser request to close the page.
  rfh->ClosePage(RenderFrameHostImpl::ClosePageSource::kBrowser);

  // The close timeout should be running.
  EXPECT_TRUE(rfh->close_timeout_ && rfh->close_timeout_->IsRunning());

  // Simulate the rfh going into the back-forward cache before the close timeout
  // fires.
  rfh->lifecycle_state_ =
      RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache;

  // Simulate the close timer firing.
  rfh->ClosePageTimeout(RenderFrameHostImpl::ClosePageSource::kBrowser);

  // The page should close regardless of it not being primary since the browser
  // requested it.
  testing::Mock::VerifyAndClearExpectations(&delegate);
}

}  // namespace content
