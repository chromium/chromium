// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_host_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "components/input/timeout_monitor.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/features.h"
#include "content/public/browser/cors_origin_pattern_setter.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/fake_local_frame.h"
#include "content/public/test/test_utils.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/network_isolation_partition.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/cookies/cookie_constants.h"
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

// TODO(crbug.com/40260854): This set-up is temporary. Eventually, all
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

// Extends `FirstPartyOverrideContentBrowserClient` with a configurable
// MIME handler effective top frame for StorageKey truncation tests.
class MimeHandlerOverrideContentBrowserClient
    : public FirstPartyOverrideContentBrowserClient {
 public:
  void SetEffectiveTopFrame(RenderFrameHost* rfh) { effective_top_ = rfh; }

  RenderFrameHost* GetEffectiveTopFrameForPartitioning(
      RenderFrameHost* render_frame_host) override {
    return effective_top_;
  }

 private:
  raw_ptr<RenderFrameHost> effective_top_ = nullptr;
};

// A test class that forces kOriginKeyedProcessesByDefault off for tests that
// require that same-site cross-origin navigations don't trigger a RFH swap.
class RenderFrameHostImplTest_NoOriginKeyedProcessesByDefault
    : public RenderFrameHostImplTest {
 public:
  RenderFrameHostImplTest_NoOriginKeyedProcessesByDefault() {
    feature_list_.InitAndDisableFeature(
        features::kOriginKeyedProcessesByDefault);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Note: Since this test is predicate on not having a RFH swap for a
// cross-origin, same-site navigation, it only makes sense to run it with
// kOriginKeyedProcessesByDefault disabled.
TEST_F(RenderFrameHostImplTest_NoOriginKeyedProcessesByDefault,
       ExpectedMainWorldOrigin) {
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
  // BrowsingInstance for |initial_rfh| before continuing.
  DisableProactiveBrowsingInstanceSwapFor(initial_rfh);
  if (ShouldCreateNewHostForAllFrames()) {
    GTEST_SKIP();
  }
  // Verify expected main world origin in a steady state - after a commit it
  // should be the same as the last committed origin.
  EXPECT_EQ(url::Origin::Create(initial_url),
            get_expected_main_world_origin(main_rfh()));
  EXPECT_EQ(url::Origin::Create(initial_url),
            main_rfh()->GetLastCommittedOrigin());
  EXPECT_EQ(
      blink::StorageKey::CreateFirstParty(url::Origin::Create(initial_url)),
      main_test_rfh()->GetStorageKey());

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
      main_test_rfh()->GetStorageKey());

  // Verify expected main world origin once we are again in a steady state -
  // after a commit.
  simulator2->Commit();
  EXPECT_EQ(url::Origin::Create(final_url),
            get_expected_main_world_origin(main_rfh()));
  EXPECT_EQ(url::Origin::Create(final_url),
            main_rfh()->GetLastCommittedOrigin());
  EXPECT_EQ(blink::StorageKey::CreateFirstParty(url::Origin::Create(final_url)),
            main_test_rfh()->GetStorageKey());

  // As a test correctness check, verify that there was no RFH swap (the bug
  // this test protects against would only happen if there is no swap).  In
  // fact, FindLatestNavigationRequestThatIsStillCommitting might possibly be
  // removed entirely once we swap on all document changes.
  EXPECT_EQ(initial_rfh, main_rfh());
}

// Test that navigating to an invalid URL (which creates an empty GURL) causes
// about:blank to commit.
TEST_F(RenderFrameHostImplTest, InvalidURL) {
  // Start from a valid commit.
  NavigateAndCommit(GURL("https://test.example.com"));

  // Attempt to navigate to a non-empty invalid URL, which GURL treats as an
  // empty invalid URL. Blink treats navigations to an empty URL as navigations
  // to about:blank.
  GURL invalid_url("invalidurl");
  EXPECT_TRUE(invalid_url.is_empty());
  EXPECT_FALSE(invalid_url.is_valid());
  NavigateAndCommit(invalid_url);
  EXPECT_EQ(GURL(url::kAboutBlankURL), main_rfh()->GetLastCommittedURL());
}

TEST_F(RenderFrameHostImplTest, ExitFullscreenDestruction) {
  class DestructionDelegate : public WebContentsDelegate {
   public:
    explicit DestructionDelegate(base::OnceClosure destruction_closure)
        : destruction_closure_(std::move(destruction_closure)) {}
    void ExitFullscreenModeForTab(WebContents* web_contents) override {
      std::move(destruction_closure_).Run();
    }

   private:
    base::OnceClosure destruction_closure_;
  };

  DestructionDelegate delegate(base::BindOnce(
      &RenderFrameHostImplTest::DeleteContents, base::Unretained(this)));
  contents()->SetDelegate(&delegate);

  // This should not crash.
  main_test_rfh()->ExitFullscreen();
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
  net::IsolationInfo expected_final_isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, expected_final_origin,
      expected_final_origin, net::SiteForCookies(), /*nonce=*/std::nullopt,
      net::NetworkIsolationPartition::kGeneral,
      net::IsolationInfo::FrameAncestorRelation::kCrossSite);

  EXPECT_EQ(expected_final_origin, child_rfh_2->GetLastCommittedOrigin());
  EXPECT_EQ(expected_final_storage_key, child_rfh_2->GetStorageKey());
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
// Note: Since this test is predicate on not having a RFH swap for a
// cross-origin, same-site navigation, it only makes sense to run it with
// kOriginKeyedProcessesByDefault disabled.
TEST_F(RenderFrameHostImplTest_NoOriginKeyedProcessesByDefault,
       IsolationInfoDuringCommit) {
  GURL initial_url = GURL("https://initial.example.test/");
  url::Origin expected_initial_origin = url::Origin::Create(initial_url);
  const blink::StorageKey expected_initial_storage_key =
      blink::StorageKey::CreateFirstParty(expected_initial_origin);
  net::IsolationInfo expected_initial_isolation_info =
      net::IsolationInfo::Create(
          net::IsolationInfo::RequestType::kOther, expected_initial_origin,
          expected_initial_origin,
          net::SiteForCookies::FromOrigin(expected_initial_origin),
          /*nonce=*/std::nullopt, net::NetworkIsolationPartition::kGeneral,
          net::IsolationInfo::FrameAncestorRelation::kSameOrigin);

  GURL final_url = GURL("https://final.example.test/");
  url::Origin expected_final_origin = url::Origin::Create(final_url);
  const blink::StorageKey expected_final_storage_key =
      blink::StorageKey::CreateFirstParty(expected_final_origin);
  net::IsolationInfo expected_final_isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, expected_final_origin,
      expected_final_origin,
      net::SiteForCookies::FromOrigin(expected_final_origin),
      /*nonce=*/std::nullopt, net::NetworkIsolationPartition::kGeneral,
      net::IsolationInfo::FrameAncestorRelation::kSameOrigin);
  // Start the test with a simple navigation.
  {
    std::unique_ptr<NavigationSimulator> simulator =
        NavigationSimulator::CreateRendererInitiated(initial_url, main_rfh());
    simulator->Start();
    simulator->Commit();
  }

  // This test is targeted at the case an RFH is reused between navigations.
  RenderFrameHost* initial_rfh = main_rfh();
  DisableProactiveBrowsingInstanceSwapFor(main_rfh());
  if (ShouldCreateNewHostForAllFrames()) {
    GTEST_SKIP();
  }

  // Check values for the initial commit.
  EXPECT_EQ(expected_initial_origin, main_rfh()->GetLastCommittedOrigin());
  EXPECT_EQ(expected_initial_storage_key, main_test_rfh()->GetStorageKey());
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
  EXPECT_EQ(expected_initial_storage_key, main_test_rfh()->GetStorageKey());
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
  EXPECT_EQ(expected_initial_storage_key, main_test_rfh()->GetStorageKey());
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
  EXPECT_EQ(expected_final_storage_key, main_test_rfh()->GetStorageKey());
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
  const auto kFavicon = blink::mojom::FaviconURL(
      GURL("https://example.com/favicon.ico"),
      blink::mojom::FaviconIconType::kFavicon, {}, /*is_default_icon=*/false);
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
      blink::mojom::FaviconIconType::kFavicon, std::vector<gfx::Size>(),
      /*is_default_icon=*/false));

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
  EXPECT_FALSE(child_frame->GetStorageKey().nonce().has_value());

  auto attributes = blink::mojom::IframeAttributes::New();
  attributes->parsed_csp_attribute = std::move(
      child_frame->frame_tree_node()->attributes_->parsed_csp_attribute);
  attributes->id = child_frame->frame_tree_node()->html_id();
  attributes->name = child_frame->frame_tree_node()->html_name();
  attributes->src = child_frame->frame_tree_node()->html_src();
  attributes->credentialless = true;
  child_frame->frame_tree_node()->SetAttributes(std::move(attributes));

  EXPECT_FALSE(child_frame->IsCredentialless());
  EXPECT_FALSE(child_frame->GetStorageKey().nonce().has_value());

  // A navigation in the credentialless iframe commits a credentialless RFH.
  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateRendererInitiated(
          GURL("https://example.com/navigation.html"), child_frame);
  navigation->Commit();
  child_frame =
      static_cast<TestRenderFrameHost*>(navigation->GetFinalRenderFrameHost());
  EXPECT_TRUE(child_frame->IsCredentialless());
  EXPECT_TRUE(child_frame->GetStorageKey().nonce().has_value());

  // A credentialless document sets a nonce on its network isolation key.
  EXPECT_TRUE(child_frame->GetNetworkIsolationKey().GetNonce().has_value());
  EXPECT_EQ(main_test_rfh()->GetPage().credentialless_iframes_nonce(),
            child_frame->GetNetworkIsolationKey().GetNonce().value());

  // A child of a credentialless RFH is credentialless.
  auto* grandchild_frame = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(child_frame)
          ->AppendChild("grandchild"));
  EXPECT_TRUE(grandchild_frame->IsCredentialless());
  EXPECT_TRUE(grandchild_frame->GetStorageKey().nonce().has_value());

  // The two credentialless RFH's storage keys should have the same nonce.
  EXPECT_EQ(child_frame->GetStorageKey().nonce().value(),
            grandchild_frame->GetStorageKey().nonce().value());

  // Also the credentialless initial empty document sets a nonce on its network
  // isolation key.
  EXPECT_TRUE(
      grandchild_frame->GetNetworkIsolationKey().GetNonce().has_value());
  EXPECT_EQ(main_test_rfh()->GetPage().credentialless_iframes_nonce(),
            grandchild_frame->GetNetworkIsolationKey().GetNonce().value());
}

// FakeLocalFrame implementation that records calls to BeforeUnload().
class FakeLocalFrameWithBeforeUnload : public content::FakeLocalFrame {
 public:
  explicit FakeLocalFrameWithBeforeUnload(TestRenderFrameHost* test_host) {
    Init(test_host->GetRemoteAssociatedInterfaces());
  }

  bool was_before_unload_sent_to_renderer() const {
    return was_before_unload_sent_to_renderer_;
  }

  // FakeLocalFrame:
  void BeforeUnload(bool is_reload,
                    bool force_to_proceed,
                    BeforeUnloadCallback callback) override {
    was_before_unload_sent_to_renderer_ = true;
  }

 private:
  bool was_before_unload_sent_to_renderer_ = false;
};

// Verifies BeforeUnload() is not sent to renderer if there is no before
// unload handler present.
TEST_F(RenderFrameHostImplTest, BeforeUnloadNotSentToRenderer) {
  TestRenderFrameHost* rfh = contents()->GetPrimaryMainFrame();
  base::RunLoop run_loop;
  bool before_unload_completed = false;
  rfh->set_on_process_before_unload_completed_for_testing(
      base::BindLambdaForTesting([&]() {
        before_unload_completed = true;
        EXPECT_TRUE(rfh->is_waiting_for_beforeunload_completion());
        run_loop.Quit();
      }));
  FakeLocalFrameWithBeforeUnload local_frame(rfh);
  auto simulator = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL("https://example.com/simple.html"), contents());
  simulator->set_block_invoking_before_unload_completed_callback(true);
  simulator->Start();
  run_loop.Run();
  EXPECT_TRUE(before_unload_completed);
  EXPECT_FALSE(local_frame.was_before_unload_sent_to_renderer());
  // This is necessary to trigger FakeLocalFrameWithBeforeUnload to be bound.
  rfh->FlushLocalFrameMessages();
  // This runs a MessageLoop, which also results in the PostTask() scheduled
  // completing.
  local_frame.FlushMessages();
  EXPECT_FALSE(local_frame.was_before_unload_sent_to_renderer());
  // Because of the nested message loops run by the previous calls, the task
  // that RenderFrameHostImpl will have also completed.
  EXPECT_FALSE(rfh->is_waiting_for_beforeunload_completion());
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
  params->document_sequence_number = 1;
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
  params->document_sequence_number = 1;
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

// TODO(crbug.com/40260854): This test should be migrated to //chrome.
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
  // storage key with the `initial_url_ext` as its top level origin.
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
  // extension rfh has host permissions to `child_frame` calling
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

// TODO(crbug.com/41483148): Flaky on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_CalculateStorageKeyFirstPartyOverride \
  DISABLED_CalculateStorageKeyFirstPartyOverride
#else
#define MAYBE_CalculateStorageKeyFirstPartyOverride \
  CalculateStorageKeyFirstPartyOverride
#endif

// TODO(crbug.com/40260854): Eventually, this test will be moved to
// chrome/browser/ so that we no longer need to override the
// ContentBrowserClient, and we can test using real extension URLs.
TEST_F(RenderFrameHostImplTest, MAYBE_CalculateStorageKeyFirstPartyOverride) {
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

  // TODO(crbug.com/40260854): once this test is moved to chrome/browser/
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

  // The top level document has host permissions to the child_url so the top
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
class TestWebAuthnContentBrowserClientImpl : public ContentBrowserClient {
 public:
  MOCK_METHOD(bool,
              IsSecurityLevelAcceptableForWebAuthn,
              (RenderFrameHost*, const url::Origin& origin),
              ());
};

class RenderFrameHostImplWebAuthnTest : public RenderFrameHostImplTest {
 public:
  void SetUp() override {
    RenderFrameHostImplTest::SetUp();
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
  std::unique_ptr<TestWebAuthnContentBrowserClientImpl> browser_client_ =
      std::make_unique<TestWebAuthnContentBrowserClientImpl>();
};

TEST_F(RenderFrameHostImplWebAuthnTest,
       PerformGetAssertionWebAuthSecurityChecks_TLSError) {
  GURL url("https://doofenshmirtz.evil");
  const auto origin = url::Origin::Create(url);
  EXPECT_CALL(*browser_client_,
              IsSecurityLevelAcceptableForWebAuthn(main_test_rfh(), origin))
      .WillOnce(testing::Return(false));
  std::optional<blink::mojom::AuthenticatorStatus> status;
  main_test_rfh()->PerformGetAssertionWebAuthSecurityChecks(
      "doofenshmirtz.evil", url::Origin::Create(url),
      /*is_payment_credential_get_assertion=*/false,
      /*remote_desktop_client_override_origin=*/std::nullopt,
      /*app_id=*/std::nullopt,
      base::BindLambdaForTesting(
          [&status](blink::mojom::AuthenticatorStatus s, bool is_cross_origin) {
            status = s;
          }));
  EXPECT_EQ(status.value(),
            blink::mojom::AuthenticatorStatus::CERTIFICATE_ERROR);
}

TEST_F(RenderFrameHostImplWebAuthnTest,
       PerformMakeCredentialWebAuthSecurityChecks_TLSError) {
  GURL url("https://doofenshmirtz.evil");
  const auto origin = url::Origin::Create(url);
  EXPECT_CALL(*browser_client_,
              IsSecurityLevelAcceptableForWebAuthn(main_test_rfh(), origin))
      .WillOnce(testing::Return(false));
  std::optional<blink::mojom::AuthenticatorStatus> status;
  main_test_rfh()->PerformMakeCredentialWebAuthSecurityChecks(
      "doofenshmirtz.evil", url::Origin::Create(url),
      /*is_payment_credential_creation=*/false,
      /*remote_desktop_client_override_origin=*/std::nullopt,
      /*app_id=*/std::nullopt,
      base::BindLambdaForTesting(
          [&status](blink::mojom::AuthenticatorStatus s, bool is_cross_origin) {
            status = s;
          }));
  EXPECT_EQ(status.value(),
            blink::mojom::AuthenticatorStatus::CERTIFICATE_ERROR);
}

TEST_F(RenderFrameHostImplWebAuthnTest,
       PerformGetAssertionWebAuthSecurityChecks_Success) {
  GURL url("https://owca.org");
  const auto origin = url::Origin::Create(url);
  EXPECT_CALL(*browser_client_,
              IsSecurityLevelAcceptableForWebAuthn(main_test_rfh(), origin))
      .WillOnce(testing::Return(true));
  std::optional<blink::mojom::AuthenticatorStatus> status;
  main_test_rfh()->PerformGetAssertionWebAuthSecurityChecks(
      "owca.org", url::Origin::Create(url),
      /*is_payment_credential_get_assertion=*/false,
      /*remote_desktop_client_override_origin=*/std::nullopt,
      /*app_id=*/std::nullopt,
      base::BindLambdaForTesting(
          [&status](blink::mojom::AuthenticatorStatus s, bool is_cross_origin) {
            status = s;
          }));
  EXPECT_EQ(status.value(), blink::mojom::AuthenticatorStatus::SUCCESS);
}

TEST_F(RenderFrameHostImplWebAuthnTest,
       PerformGetAssertionWebAuthSecurityChecks_AppId_Success) {
  GURL url("https://owca.org");
  const auto origin = url::Origin::Create(url);
  EXPECT_CALL(*browser_client_,
              IsSecurityLevelAcceptableForWebAuthn(main_test_rfh(), origin))
      .WillOnce(testing::Return(true));
  std::optional<blink::mojom::AuthenticatorStatus> status;
  main_test_rfh()->PerformGetAssertionWebAuthSecurityChecks(
      "owca.org", url::Origin::Create(url),
      /*is_payment_credential_get_assertion=*/false,
      /*remote_desktop_client_override_origin=*/std::nullopt,
      /*app_id=*/"https://owca.org/appid.json",
      base::BindLambdaForTesting(
          [&status](blink::mojom::AuthenticatorStatus s, bool is_cross_origin) {
            status = s;
          }));
  EXPECT_EQ(status.value(), blink::mojom::AuthenticatorStatus::SUCCESS);
}

TEST_F(RenderFrameHostImplWebAuthnTest,
       PerformGetAssertionWebAuthSecurityChecks_AppId_Invalid) {
  GURL url("https://owca.org");
  const auto origin = url::Origin::Create(url);
  // AppId validation happens before IsSecurityLevelAcceptableForWebAuthn check.
  std::optional<blink::mojom::AuthenticatorStatus> status;
  main_test_rfh()->PerformGetAssertionWebAuthSecurityChecks(
      "owca.org", url::Origin::Create(url),
      /*is_payment_credential_get_assertion=*/false,
      /*remote_desktop_client_override_origin=*/std::nullopt,
      /*app_id=*/"https://evil.com/appid.json",
      base::BindLambdaForTesting(
          [&status](blink::mojom::AuthenticatorStatus s, bool is_cross_origin) {
            status = s;
          }));
  EXPECT_EQ(status.value(), blink::mojom::AuthenticatorStatus::INVALID_DOMAIN);
}

TEST_F(
    RenderFrameHostImplWebAuthnTest,
    PerformGetAssertionWebAuthSecurityChecks_RemoteDesktopOrigin_AppIdMismatch) {
  GURL url("https://owca.org");
  const auto origin = url::Origin::Create(url);
  // AppId validation happens before IsSecurityLevelAcceptableForWebAuthn check.
  std::optional<blink::mojom::AuthenticatorStatus> status;
  main_test_rfh()->PerformGetAssertionWebAuthSecurityChecks(
      "owca.org", origin,
      /*is_payment_credential_get_assertion=*/false,
      /*remote_desktop_client_override_origin=*/
      url::Origin::Create(GURL("https://evil.com")),
      /*app_id=*/"https://owca.org/appid.json",
      base::BindLambdaForTesting(
          [&status](blink::mojom::AuthenticatorStatus s, bool is_cross_origin) {
            status = s;
          }));
  EXPECT_EQ(status.value(), blink::mojom::AuthenticatorStatus::
                                REMOTE_DESKTOP_CLIENT_OVERRIDE_NOT_AUTHORIZED);
}

TEST_F(RenderFrameHostImplWebAuthnTest,
       PerformGetAssertionWebAuthSecurityChecks_InvalidRemoteDesktopOrigin) {
  GURL url("https://owca.org");
  const auto origin = url::Origin::Create(url);
  EXPECT_CALL(*browser_client_,
              IsSecurityLevelAcceptableForWebAuthn(main_test_rfh(), origin))
      .WillOnce(testing::Return(true));
  // Security checks happen before IsSecurityLevelAcceptableForWebAuthn check.
  std::optional<blink::mojom::AuthenticatorStatus> status;
  main_test_rfh()->PerformGetAssertionWebAuthSecurityChecks(
      "owca.org", origin,
      /*is_payment_credential_get_assertion=*/false,
      /*remote_desktop_client_override_origin=*/
      url::Origin::Create(GURL("https://evil.com")),
      /*app_id=*/std::nullopt,
      base::BindLambdaForTesting(
          [&status](blink::mojom::AuthenticatorStatus s, bool is_cross_origin) {
            status = s;
          }));
  EXPECT_EQ(status.value(), blink::mojom::AuthenticatorStatus::
                                REMOTE_DESKTOP_CLIENT_OVERRIDE_NOT_AUTHORIZED);
}

TEST_F(RenderFrameHostImplWebAuthnTest,
       PerformMakeCredentialWebAuthSecurityChecks_AppId_Success) {
  GURL url("https://owca.org");
  const auto origin = url::Origin::Create(url);
  EXPECT_CALL(*browser_client_,
              IsSecurityLevelAcceptableForWebAuthn(main_test_rfh(), origin))
      .WillOnce(testing::Return(true));
  std::optional<blink::mojom::AuthenticatorStatus> status;
  main_test_rfh()->PerformMakeCredentialWebAuthSecurityChecks(
      "owca.org", url::Origin::Create(url),
      /*is_payment_credential_creation=*/false,
      /*remote_desktop_client_override_origin=*/std::nullopt,
      /*app_id=*/"https://owca.org/appid.json",
      base::BindLambdaForTesting(
          [&status](blink::mojom::AuthenticatorStatus s, bool is_cross_origin) {
            status = s;
          }));
  EXPECT_EQ(status.value(), blink::mojom::AuthenticatorStatus::SUCCESS);
}

TEST_F(RenderFrameHostImplWebAuthnTest,
       PerformMakeCredentialWebAuthSecurityChecks_AppId_Invalid) {
  GURL url("https://owca.org");
  const auto origin = url::Origin::Create(url);
  std::optional<blink::mojom::AuthenticatorStatus> status;
  main_test_rfh()->PerformMakeCredentialWebAuthSecurityChecks(
      "owca.org", url::Origin::Create(url),
      /*is_payment_credential_creation=*/false,
      /*remote_desktop_client_override_origin=*/std::nullopt,
      /*app_id=*/"https://evil.com/appid.json",
      base::BindLambdaForTesting(
          [&status](blink::mojom::AuthenticatorStatus s, bool is_cross_origin) {
            status = s;
          }));
  EXPECT_EQ(status.value(), blink::mojom::AuthenticatorStatus::INVALID_DOMAIN);
}

TEST_F(RenderFrameHostImplWebAuthnTest,
       PerformMakeCredentialWebAuthSecurityChecks_Success) {
  GURL url("https://owca.org");
  const auto origin = url::Origin::Create(url);
  EXPECT_CALL(*browser_client_,
              IsSecurityLevelAcceptableForWebAuthn(main_test_rfh(), origin))
      .WillOnce(testing::Return(true));
  std::optional<blink::mojom::AuthenticatorStatus> status;
  main_test_rfh()->PerformMakeCredentialWebAuthSecurityChecks(
      "owca.org", url::Origin::Create(url),
      /*is_payment_credential_creation=*/false,
      /*remote_desktop_client_override_origin=*/std::nullopt,
      /*app_id=*/std::nullopt,
      base::BindLambdaForTesting(
          [&status](blink::mojom::AuthenticatorStatus s, bool is_cross_origin) {
            status = s;
          }));
  EXPECT_EQ(status.value(), blink::mojom::AuthenticatorStatus::SUCCESS);
}

#endif  // BUILDFLAG(IS_ANDROID)

class SkipBeforeUnloadDialogAndNavigateContentBrowserClient
    : public ContentBrowserClient {
 public:
  bool ShouldSkipBeforeUnloadDialog(content::RenderFrameHost* rfh) override {
    called_ = true;
    return true;
  }

  bool called() const { return called_; }

 private:
  bool called_ = false;
};

TEST_F(RenderFrameHostImplTest, RunBeforeUnloadConfirm_SkipDialogAndNavigate) {
  SkipBeforeUnloadDialogAndNavigateContentBrowserClient skip_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&skip_client);

  TestRenderFrameHost* rfh = contents()->GetPrimaryMainFrame();

  bool callback_ran = false;
  rfh->RunBeforeUnloadConfirm(
      /*is_reload=*/false, base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        callback_ran = true;
      }));

  EXPECT_TRUE(callback_ran);
  EXPECT_TRUE(skip_client.called());

  SetBrowserClientForTesting(old_browser_client);
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

class RenderFrameHostImplLazyBrowserInterfaceBrokerTest
    : public RenderFrameHostImplTest,
      public testing::WithParamInterface<bool> {
 public:
  RenderFrameHostImplLazyBrowserInterfaceBrokerTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(features::kLazyBrowserInterfaceBroker);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kLazyBrowserInterfaceBroker);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that kLazyBrowserInterfaceBroker controls whether the
// RenderFrameHostImpl is constructed with a BrokerHolder. When the flag is
// enabled the test also confirms that lazy initialization works correctly.
TEST_P(RenderFrameHostImplLazyBrowserInterfaceBrokerTest,
       LazyBrowserInterfaceBroker) {
  scoped_refptr<SiteInstance> instance =
      SiteInstance::Create(GetBrowserContext());
  std::unique_ptr<TestWebContents> web_contents =
      TestWebContents::Create(GetBrowserContext(), std::move(instance));
  RenderFrameHostImpl* rfh = web_contents->GetPrimaryMainFrame();
  if (GetParam()) {
    EXPECT_FALSE(rfh->has_broker_holder_for_testing());
    mojo::Remote<blink::mojom::BrowserInterfaceBroker> broker_remote;
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
        broker_receiver = broker_remote.BindNewPipeAndPassReceiver();
    rfh->BindBrowserInterfaceBrokerReceiver(std::move(broker_receiver));
    EXPECT_TRUE(rfh->has_broker_holder_for_testing());
  } else {
    EXPECT_TRUE(rfh->has_broker_holder_for_testing());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         RenderFrameHostImplLazyBrowserInterfaceBrokerTest,
                         testing::Bool());

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
      main_test_rfh()->GetStorageKey());

  if (ThirdPartyStoragePartitioningEnabled()) {
    // child frame storage key should contain child_origin + top_level_origin if
    // third party partitioning is on.
    EXPECT_EQ(blink::StorageKey::Create(
                  url::Origin::Create(child_url),
                  net::SchemefulSite(url::Origin::Create(initial_url)),
                  blink::mojom::AncestorChainBit::kCrossSite),
              child_frame->GetStorageKey());
  } else {
    // child frame storage key should only be partitioned by child origin if
    // third party partitioning is off.
    EXPECT_EQ(
        blink::StorageKey::CreateFirstParty(url::Origin::Create(child_url)),
        child_frame->GetStorageKey());
  }
}

namespace {

class MockWebContentsDelegate : public WebContentsDelegate {
 public:
  MOCK_METHOD(void, CloseContents, (WebContents*));
  MOCK_METHOD(void,
              OnTextCopiedToClipboard,
              (RenderFrameHost*, std::u16string));
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

// A mock WebContentsObserver for listening to text copy events.
class TextCopiedEventObserver : public WebContentsObserver {
 public:
  explicit TextCopiedEventObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  MOCK_METHOD(void,
              OnTextCopiedToClipboard,
              (RenderFrameHost*, const std::u16string&),
              (override));
};

// Test that the WebContentObserver is notified when text is copied to the
// clipboard for a RenderFrameHost.
TEST_F(RenderFrameHostImplTest, OnTextCopiedToClipboard) {
  testing::StrictMock<TextCopiedEventObserver> observer(contents());
  std::u16string copied_text = u"copied_text";

  RenderFrameHostImpl* rfh = main_test_rfh();
  EXPECT_CALL(observer, OnTextCopiedToClipboard(rfh, copied_text));

  rfh->OnTextCopiedToClipboard(copied_text);
}

// Test if `LoadedWithCacheControlNoStoreHeader()` behaves
// as expected.
TEST_F(RenderFrameHostImplTest, LoadedWithCacheControlNoStoreHeader) {
  TestRenderFrameHost* rfh = main_test_rfh();
  // In the default state, `LoadedWithCacheControlNoStoreHeader()`
  // will return false.
  ASSERT_FALSE(rfh->LoadedWithCacheControlNoStoreHeader());
  // Register the `kMainResourceHasCacheControlNoStore` feature and
  // `LoadedWithCacheControlNoStoreHeader()` will return true.
  rfh->OnBackForwardCacheDisablingStickyFeatureUsed(
      blink::scheduler::WebSchedulerTrackedFeature::
          kMainResourceHasCacheControlNoStore);
  ASSERT_TRUE(rfh->LoadedWithCacheControlNoStoreHeader());
  // Simulate a same RFH navigation and the
  // `LoadedWithCacheControlNoStoreHeader()` should return false because the
  // registered feature is reset.
  NavigationSimulator::NavigateAndCommitFromDocument(GURL("http://foo"), rfh);
  ASSERT_EQ(main_test_rfh(), rfh);
  ASSERT_FALSE(main_test_rfh()->LoadedWithCacheControlNoStoreHeader());
}

class MediaStreamCaptureObserver : public WebContentsObserver {
 public:
  explicit MediaStreamCaptureObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  MOCK_METHOD(void,
              OnFrameIsCapturingMediaStreamChanged,
              (RenderFrameHost*, bool),
              (override));
};

TEST_F(RenderFrameHostImplTest, CapturedMediaStreamAddedRemoved) {
  testing::StrictMock<MediaStreamCaptureObserver> observer(contents());

  TestRenderFrameHost* main_rfh = contents()->GetPrimaryMainFrame();

  // Calling OnMediaStreamAdded for the first time will cause a notification.
  EXPECT_CALL(observer, OnFrameIsCapturingMediaStreamChanged(main_rfh, true));
  main_rfh->OnMediaStreamAdded(
      RenderFrameHostImpl::MediaStreamType::kCapturingMediaStream);

  // Calling it again will not result in a notification (verified by the
  // StrictMock).
  main_rfh->OnMediaStreamAdded(
      RenderFrameHostImpl::MediaStreamType::kCapturingMediaStream);

  // Calling OnMediaStreamRemoved to cancel out one of the OnMediaStreamAdded
  // calls. Overall, the frame is still capturing at least one media stream so
  // there is no notifications.
  main_rfh->OnMediaStreamRemoved(
      RenderFrameHostImpl::MediaStreamType::kCapturingMediaStream);

  // Cancelling the first OnMediaStreamAdded call. This changes the state of the
  // frame and thus cause a notification.
  EXPECT_CALL(observer, OnFrameIsCapturingMediaStreamChanged(main_rfh, false));
  main_rfh->OnMediaStreamRemoved(
      RenderFrameHostImpl::MediaStreamType::kCapturingMediaStream);
}

// Ensure that an invalid WindowOpenDisposition in CreateNewWindow causes a bad
// message.
TEST_F(RenderFrameHostImplTest, CreateNewWindowInvalidDisposition) {
  mojom::CreateNewWindowParamsPtr params = mojom::CreateNewWindowParams::New();
  params->disposition = WindowOpenDisposition::UNKNOWN;

  // The bad message is reported to the process.
  EXPECT_EQ(0, process()->bad_msg_count());

  static_cast<RenderFrameHostImpl*>(main_rfh())
      ->CreateNewWindow(std::move(params), base::DoNothing());

  EXPECT_EQ(1, process()->bad_msg_count());
}

class RenderFrameHostImplCookieChangeListenerTest
    : public RenderFrameHostImplTest,
      public testing::WithParamInterface<bool> {
 public:
  RenderFrameHostImplCookieChangeListenerTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          features::kBackForwardCacheCCNSIgnoreUnchangedCookies);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kBackForwardCacheCCNSIgnoreUnchangedCookies);
    }
  }

 protected:
  bool IsBackForwardCacheCCNSIgnoreUnchangedCookiesEnabled() {
    return GetParam();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         RenderFrameHostImplCookieChangeListenerTest,
                         testing::Bool());

// Tests the behavior of the `RenderFrameHostImpl::CookieChangeListener`.
TEST_P(RenderFrameHostImplCookieChangeListenerTest, CookieChangeListener) {
  StoragePartition* partition = main_rfh()->GetStoragePartition();
  GURL url("https://example.com");
  std::unique_ptr<RenderFrameHostImpl::CookieChangeListener> listener =
      std::make_unique<RenderFrameHostImpl::CookieChangeListener>(partition,
                                                                  url);
  std::unique_ptr<net::CanonicalCookie> cookie_ignored =
      net::CanonicalCookie::CreateForTesting(url, "a=1", base::Time::Now(),
                                             net::CookieSourceType::kHTTP);
  ASSERT_TRUE(cookie_ignored);
  std::unique_ptr<net::CanonicalCookie> cookie_overwrite =
      net::CanonicalCookie::CreateForTesting(url, "a=2", base::Time::Now(),
                                             net::CookieSourceType::kHTTP);
  ASSERT_TRUE(cookie_overwrite);
  std::unique_ptr<net::CanonicalCookie> cookie_overwrite_no_change =
      net::CanonicalCookie::CreateForTesting(url, "a=2", base::Time::Now(),
                                             net::CookieSourceType::kHTTP);
  ASSERT_TRUE(cookie_overwrite_no_change);

  // Initially the count is 0.
  listener->AddNavigationCookieToIgnoreForTesting(*cookie_ignored);
  EXPECT_EQ(0, listener->cookie_change_info().cookie_modification_count);

  // The counter doesn't change if the cookie change is ignored.
  {
    listener->OnCookieChangeForTesting(
        net::CookieChangeInfo(*cookie_ignored, net::CookieAccessResult(),
                              net::CookieChangeCause::INSERTED));
    EXPECT_EQ(0, listener->cookie_change_info().cookie_modification_count);
  }

  // The counter increments if the ignored cookie gets changed for another time.
  {
    listener->OnCookieChangeForTesting(
        net::CookieChangeInfo(*cookie_ignored, net::CookieAccessResult(),
                              net::CookieChangeCause::INSERTED));
    EXPECT_EQ(1, listener->cookie_change_info().cookie_modification_count);
  }

  // The counter increments if there is a new cookie value set.
  {
    listener->OnCookieChangeForTesting(
        net::CookieChangeInfo(*cookie_overwrite, net::CookieAccessResult(),
                              net::CookieChangeCause::INSERTED));
    EXPECT_EQ(2, listener->cookie_change_info().cookie_modification_count);
  }

  // The counter increments if there is a cookie modification without value
  // change and the `kBackForwardCacheCCNSIgnoreUnchangedCookies` is enabled.
  {
    net::CookieChangeInfo change_info(
        *cookie_overwrite_no_change, net::CookieAccessResult(),
        net::CookieChangeCause::INSERTED_NO_CHANGE_OVERWRITE);
    listener->OnCookieChangeForTesting(change_info);
    EXPECT_EQ(IsBackForwardCacheCCNSIgnoreUnchangedCookiesEnabled() ? 2 : 3,
              listener->cookie_change_info().cookie_modification_count);
  }

  {
    net::CookieChangeInfo change_info(
        *cookie_overwrite_no_change, net::CookieAccessResult(),
        net::CookieChangeCause::INSERTED_NO_VALUE_CHANGE_OVERWRITE);
    listener->OnCookieChangeForTesting(change_info);
    EXPECT_EQ(IsBackForwardCacheCCNSIgnoreUnchangedCookiesEnabled() ? 2 : 4,
              listener->cookie_change_info().cookie_modification_count);
  }
}

// Shared fixture for tests that exercise
// `ContentBrowserClient::GetEffectiveTopFrameForPartitioning()` and the
// IsolationInfo / StorageKey overrides keyed off it. Centralizes the
// scheme registration, ContentBrowserClient swap, third-party storage
// partitioning flag, and the canonical embedder/extension/grandchild
// frame tree.
class RenderFrameHostImplMimeHandlerStoragePartitioningTest
    : public RenderFrameHostImplTest {
 public:
  RenderFrameHostImplMimeHandlerStoragePartitioningTest() {
    feature_list_.InitAndEnableFeature(
        net::features::kThirdPartyStoragePartitioning);
    url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);
    previous_client_ = SetBrowserClientForTesting(&modified_client_);
  }

  ~RenderFrameHostImplMimeHandlerStoragePartitioningTest() override {
    SetBrowserClientForTesting(previous_client_);
  }

  void TearDown() override {
    // Drop the raw_ptr to the extension frame before
    // `RenderFrameHostImplTest::TearDown` destroys the WebContents and its
    // RFHs, otherwise the dangling-pointer detector trips.
    modified_client_.SetEffectiveTopFrame(nullptr);
    RenderFrameHostImplTest::TearDown();
  }

 protected:
  // Commits the main frame at the embedder URL and returns it.
  TestRenderFrameHost* CommitEmbedder(const GURL& embedder_url) {
    NavigationSimulator::CreateRendererInitiated(embedder_url, main_rfh())
        ->Commit();
    return main_test_rfh();
  }

  // Appends a child of `parent` and (optionally) commits a navigation to
  // `url`. When `url` is empty, returns the still-uncommitted child.
  TestRenderFrameHost* AppendAndMaybeCommit(TestRenderFrameHost* parent,
                                            std::string_view name,
                                            const GURL& url) {
    auto* child = static_cast<TestRenderFrameHost*>(
        content::RenderFrameHostTester::For(parent)->AppendChild(
            std::string(name)));
    if (url.is_empty()) {
      return child;
    }
    return static_cast<TestRenderFrameHost*>(
        NavigationSimulator::NavigateAndCommitFromDocument(url, child));
  }

  // main(embedder) → extension(extension_url) → grandchild(grandchild_url).
  struct Subtree {
    raw_ptr<TestRenderFrameHost> main_frame;
    raw_ptr<TestRenderFrameHost> extension_frame;
    raw_ptr<TestRenderFrameHost> grandchild_frame;
  };
  Subtree BuildEmbedderExtensionGrandchild(const GURL& embedder_url,
                                           const GURL& extension_url,
                                           const GURL& grandchild_url) {
    Subtree tree;
    tree.main_frame = CommitEmbedder(embedder_url);
    tree.extension_frame =
        AppendAndMaybeCommit(tree.main_frame, "extension", extension_url);
    tree.grandchild_frame = AppendAndMaybeCommit(tree.extension_frame,
                                                 "grandchild", grandchild_url);
    return tree;
  }

  MimeHandlerOverrideContentBrowserClient modified_client_;

 private:
  base::test::ScopedFeatureList feature_list_;
  url::ScopedSchemeRegistryForTests scoped_registry_;
  raw_ptr<ContentBrowserClient> previous_client_ = nullptr;
};

namespace {
constexpr char kEmbedderUrl[] = "https://embedder.com/doc.pdf";
constexpr char kExtensionUrl[] = "chrome-extension://abc123/viewer.html";
constexpr char kGrandchildUrl[] = "https://child-content.com/page.html";
}  // namespace

// Override active: the descendant's StorageKey uses the extension as
// top_level_site, not the embedder. The extension frame itself stays
// first-party (handled by `ShouldUseFirstPartyStorageKey`), and the
// main frame is unaffected.
TEST_F(RenderFrameHostImplMimeHandlerStoragePartitioningTest,
       StorageKeyMimeHandlerTruncation) {
  Subtree tree = BuildEmbedderExtensionGrandchild(
      GURL(kEmbedderUrl), GURL(kExtensionUrl), GURL(kGrandchildUrl));
  modified_client_.SetEffectiveTopFrame(tree.extension_frame);

  const net::SchemefulSite embedder_site(
      url::Origin::Create(GURL(kEmbedderUrl)));
  const net::SchemefulSite extension_site(
      url::Origin::Create(GURL(kExtensionUrl)));

  blink::StorageKey main_key = tree.main_frame->CalculateStorageKey(
      tree.main_frame->GetLastCommittedOrigin(), /*nonce=*/nullptr);
  EXPECT_EQ(embedder_site, main_key.top_level_site());
  EXPECT_EQ(blink::mojom::AncestorChainBit::kSameSite,
            main_key.ancestor_chain_bit());

  blink::StorageKey extension_key = tree.extension_frame->CalculateStorageKey(
      tree.extension_frame->GetLastCommittedOrigin(), /*nonce=*/nullptr);
  EXPECT_EQ(extension_site, extension_key.top_level_site());
  EXPECT_EQ(blink::mojom::AncestorChainBit::kSameSite,
            extension_key.ancestor_chain_bit());

  blink::StorageKey grandchild_key = tree.grandchild_frame->CalculateStorageKey(
      tree.grandchild_frame->GetLastCommittedOrigin(), /*nonce=*/nullptr);
  EXPECT_EQ(extension_site, grandchild_key.top_level_site());
  EXPECT_EQ(blink::mojom::AncestorChainBit::kCrossSite,
            grandchild_key.ancestor_chain_bit());
}

// No override: descendant's StorageKey falls through to the embedder.
TEST_F(RenderFrameHostImplMimeHandlerStoragePartitioningTest,
       StorageKeyNoMimeHandlerTruncation) {
  Subtree tree = BuildEmbedderExtensionGrandchild(
      GURL(kEmbedderUrl), GURL(kExtensionUrl), GURL(kGrandchildUrl));

  blink::StorageKey storage_key = tree.grandchild_frame->CalculateStorageKey(
      tree.grandchild_frame->GetLastCommittedOrigin(), /*nonce=*/nullptr);

  EXPECT_EQ(net::SchemefulSite(url::Origin::Create(GURL(kEmbedderUrl))),
            storage_key.top_level_site());
  EXPECT_EQ(blink::mojom::AncestorChainBit::kCrossSite,
            storage_key.ancestor_chain_bit());
}

// Override active: descendant's IsolationInfo uses the extension as
// top_frame_origin.
TEST_F(RenderFrameHostImplMimeHandlerStoragePartitioningTest,
       IsolationInfoMimeHandlerChildFrameOverride) {
  Subtree tree = BuildEmbedderExtensionGrandchild(GURL(kEmbedderUrl),
                                                  GURL(kExtensionUrl), GURL());
  // Set the override before the grandchild navigates so its commit-time
  // IsolationInfo sees the extension as effective top.
  modified_client_.SetEffectiveTopFrame(tree.extension_frame);
  tree.grandchild_frame = AppendAndMaybeCommit(
      tree.extension_frame, "grandchild", GURL(kGrandchildUrl));

  EXPECT_EQ(url::Origin::Create(GURL(kExtensionUrl)),
            tree.grandchild_frame->GetIsolationInfoForSubresources()
                .top_frame_origin());
}

// No override: descendant's IsolationInfo falls through to the embedder.
TEST_F(RenderFrameHostImplMimeHandlerStoragePartitioningTest,
       IsolationInfoNoMimeHandlerChildFrameOverride) {
  Subtree tree = BuildEmbedderExtensionGrandchild(
      GURL(kEmbedderUrl), GURL(kExtensionUrl), GURL(kGrandchildUrl));

  EXPECT_EQ(url::Origin::Create(GURL(kEmbedderUrl)),
            tree.grandchild_frame->GetIsolationInfoForSubresources()
                .top_frame_origin());
}

// During the extension frame's own navigation, `IsExtensionHost` matches
// by frame tree node id (set before the navigation commits), so the
// override fires with `effective_top == this` while
// `GetLastCommittedOrigin()` still returns the initial about:blank
// inherited from the embedder. The override must use the pending
// `frame_origin` instead.
TEST_F(RenderFrameHostImplMimeHandlerStoragePartitioningTest,
       IsolationInfoMimeHandlerSelfFramePendingCommit) {
  TestRenderFrameHost* embedder = CommitEmbedder(GURL(kEmbedderUrl));
  TestRenderFrameHost* extension_frame =
      AppendAndMaybeCommit(embedder, "extension", GURL());
  modified_client_.SetEffectiveTopFrame(extension_frame);

  url::Origin extension_origin = url::Origin::Create(GURL(kExtensionUrl));
  net::IsolationInfo info =
      extension_frame->ComputeIsolationInfoForSubresourcesForPendingCommit(
          extension_origin, /*is_credentialless=*/false,
          /*fenced_frame_nonce_for_navigation=*/std::nullopt);

  EXPECT_EQ(extension_origin, info.top_frame_origin());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(RenderFrameHostImplTest,
       UpdateUserGestureCarryoverInfoWithoutActivation) {
  TestRenderFrameHost* rfh = contents()->GetPrimaryMainFrame();
  MockRenderProcessHost* process =
      static_cast<MockRenderProcessHost*>(rfh->GetProcess());

  // Initially, there should be no bad messages.
  EXPECT_EQ(0, process->bad_msg_count());
  EXPECT_FALSE(rfh->HasTransientUserActivation());

  // Call UpdateUserGestureCarryoverInfo.
  static_cast<mojom::FrameHost*>(rfh)->UpdateUserGestureCarryoverInfo();

  // This should trigger a bad message because there is no transient activation.
  EXPECT_EQ(1, process->bad_msg_count());
}

TEST_F(RenderFrameHostImplTest, UpdateUserGestureCarryoverInfoWithActivation) {
  TestRenderFrameHost* rfh = contents()->GetPrimaryMainFrame();
  MockRenderProcessHost* process =
      static_cast<MockRenderProcessHost*>(rfh->GetProcess());

  // Set transient user activation.
  rfh->SimulateUserActivation();
  EXPECT_TRUE(rfh->HasTransientUserActivation());

  // Call UpdateUserGestureCarryoverInfo.
  static_cast<mojom::FrameHost*>(rfh)->UpdateUserGestureCarryoverInfo();

  // This should not trigger a bad message.
  EXPECT_EQ(0, process->bad_msg_count());
}
#endif

}  // namespace content
