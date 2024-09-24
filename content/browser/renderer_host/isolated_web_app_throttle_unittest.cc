// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/isolated_web_app_throttle.h"

#include <optional>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/page_type.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_frame_host.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_declaration.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

namespace content {

namespace {

const char kAppUrl[] = "https://isolated.app";
const char kAppUrl2[] = "https://isolated.app/page";
const char kNonAppUrl[] = "https://example.com";
const char kNonAppUrl2[] = "https://example.com/page";
static constexpr WebExposedIsolationLevel kNotIsolated =
    WebExposedIsolationLevel::kNotIsolated;
static constexpr WebExposedIsolationLevel kIsolatedApplication =
    WebExposedIsolationLevel::kIsolatedApplication;

class IsolatedWebAppContentBrowserClient : public ContentBrowserClient {
 public:
  bool ShouldUrlUseApplicationIsolationLevel(BrowserContext* browser_context,
                                             const GURL& url) override {
    return url.host() == GURL(kAppUrl).host();
  }

  bool HandleExternalProtocol(
      const GURL& url,
      base::RepeatingCallback<WebContents*()> web_contents_getter,
      FrameTreeNodeId frame_tree_node_id,
      NavigationUIData* navigation_data,
      bool is_primary_main_frame,
      bool is_in_fenced_frame_tree,
      network::mojom::WebSandboxFlags sandbox_flags,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const std::optional<url::Origin>& initiating_origin,
      RenderFrameHost* initiator_document,
      const net::IsolationInfo& isolation_info,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory)
      override {
    external_protocol_call_count_++;
    last_page_transition_ = page_transition;
    return true;
  }

  unsigned int GetExternalProtocolCallCount() const {
    return external_protocol_call_count_;
  }

  void OpenURL(
      content::SiteInstance* site_instance,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::WebContents*)> callback) override {
    open_url_call_count_++;
    last_page_transition_ = params.transition;
  }

  unsigned int GetOpenUrlCallCount() const { return open_url_call_count_; }

  ui::PageTransition GetLastPageTransition() { return last_page_transition_; }

  void ResetExternalProtocolCallCount() {
    external_protocol_call_count_ = 0;
    last_page_transition_ = ui::PageTransition::PAGE_TRANSITION_QUALIFIER_MASK;
  }

  void ResetOpenUrlCallCount() {
    open_url_call_count_ = 0;
    last_page_transition_ = ui::PageTransition::PAGE_TRANSITION_QUALIFIER_MASK;
  }

  bool AreIsolatedWebAppsEnabled(BrowserContext*) override { return true; }

  std::optional<blink::ParsedPermissionsPolicy>
  GetPermissionsPolicyForIsolatedWebApp(
      WebContents* web_contents,
      const url::Origin& app_origin) override {
    return {{blink::ParsedPermissionsPolicyDeclaration(
        blink::mojom::PermissionsPolicyFeature::kCrossOriginIsolated,
        /*allowed_origins=*/{},
        /*self_if_matches=*/std::nullopt,
        /*matches_all_origins=*/true, /*matches_opaque_src=*/false)}};
  }

 private:
  unsigned int external_protocol_call_count_ = 0;
  unsigned int open_url_call_count_ = 0;
  ui::PageTransition last_page_transition_ =
      ui::PageTransition::PAGE_TRANSITION_QUALIFIER_MASK;
};

}  // namespace

class IsolatedWebAppThrottleTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    old_client_ = SetBrowserClientForTesting(&test_client_);

    // COOP/COEP headers must be sent to enable application isolation.
    coop_coep_headers_ =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    coop_coep_headers_->AddHeader("Cross-Origin-Embedder-Policy",
                                  "require-corp");
    coop_coep_headers_->AddHeader("Cross-Origin-Opener-Policy", "same-origin");

    // CORP/COEP headers must be sent to enable embedding.
    corp_coep_headers_ =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    corp_coep_headers_->AddHeader("Cross-Origin-Embedder-Policy",
                                  "require-corp");
    corp_coep_headers_->AddHeader("Cross-Origin-Resource-Policy",
                                  "cross-origin");
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);

    RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<NavigationSimulator> StartBrowserInitiatedNavigation(
      const char* url) {
    auto simulator =
        NavigationSimulator::CreateBrowserInitiated(GURL(url), web_contents());
    simulator->Start();
    return simulator;
  }

  void CommitBrowserInitiatedNavigation(
      const char* url,
      scoped_refptr<net::HttpResponseHeaders> response_headers = nullptr) {
    CommitNavigation(StartBrowserInitiatedNavigation(url), main_frame_id(), url,
                     response_headers);
  }

  std::unique_ptr<NavigationSimulator> StartRendererInitiatedNavigation(
      FrameTreeNodeId frame_tree_node_id,
      const char* url) {
    RenderFrameHost* rfh = FrameTreeNode::GloballyFindByID(frame_tree_node_id)
                               ->current_frame_host();
    auto simulator =
        NavigationSimulator::CreateRendererInitiated(GURL(url), rfh);
    simulator->Start();
    return simulator;
  }

  void CommitRendererInitiatedNavigation(
      FrameTreeNodeId frame_tree_node_id,
      const char* url,
      scoped_refptr<net::HttpResponseHeaders> response_headers = nullptr) {
    CommitNavigation(StartRendererInitiatedNavigation(frame_tree_node_id, url),
                     frame_tree_node_id, url, response_headers);
  }

  FrameTreeNodeId CreateIframe(FrameTreeNodeId parent_frame_tree_node_id,
                               const std::string& name) {
    TestRenderFrameHost* parent_rfh = static_cast<TestRenderFrameHost*>(
        FrameTreeNode::GloballyFindByID(parent_frame_tree_node_id)
            ->current_frame_host());
    RenderFrameHost* child_rfh = parent_rfh->AppendChild(name);
    return child_rfh->GetFrameTreeNodeId();
  }

  WebExposedIsolationLevel GetWebExposedIsolationLevel(
      FrameTreeNodeId frame_tree_node_id) {
    return FrameTreeNode::GloballyFindByID(frame_tree_node_id)
        ->current_frame_host()
        ->GetWebExposedIsolationLevel();
  }

  FrameTreeNodeId main_frame_id() { return main_rfh()->GetFrameTreeNodeId(); }

  scoped_refptr<net::HttpResponseHeaders> coop_coep_headers() {
    return coop_coep_headers_;
  }

  IsolatedWebAppContentBrowserClient& GetBrowserClient() {
    return test_client_;
  }

  scoped_refptr<net::HttpResponseHeaders> corp_coep_headers() {
    return corp_coep_headers_;
  }

  static PageType GetPageType(RenderFrameHost* render_frame_host) {
    return FrameTreeNode::From(render_frame_host)
        ->navigator()
        .controller()
        .GetLastCommittedEntry()
        ->GetPageType();
  }

 private:
  void CommitNavigation(
      std::unique_ptr<NavigationSimulator> simulator,
      FrameTreeNodeId frame_tree_node_id,
      const char* url,
      scoped_refptr<net::HttpResponseHeaders> response_headers) {
    // Verify that the Start call was successful.
    auto start_result = simulator->GetLastThrottleCheckResult();
    CHECK_EQ(NavigationThrottle::PROCEED, start_result.action());

    if (response_headers)
      simulator->SetResponseHeaders(response_headers);
    simulator->Commit();

    RenderFrameHost* rfh = FrameTreeNode::GloballyFindByID(frame_tree_node_id)
                               ->current_frame_host();
    auto commit_result = simulator->GetLastThrottleCheckResult();
    CHECK_EQ(NavigationThrottle::PROCEED, commit_result.action());
    CHECK_EQ(GURL(url), rfh->GetLastCommittedURL());
  }

  IsolatedWebAppContentBrowserClient test_client_;
  raw_ptr<ContentBrowserClient> old_client_;
  scoped_refptr<net::HttpResponseHeaders> coop_coep_headers_;
  scoped_refptr<net::HttpResponseHeaders> corp_coep_headers_;
};

TEST_F(IsolatedWebAppThrottleTest, AllowNavigationWithinNonApp) {
  CommitBrowserInitiatedNavigation(kNonAppUrl);
  EXPECT_EQ(kNotIsolated, GetWebExposedIsolationLevel(main_frame_id()));

  CommitRendererInitiatedNavigation(main_frame_id(), kNonAppUrl2);
  EXPECT_EQ(kNotIsolated, GetWebExposedIsolationLevel(main_frame_id()));
}

TEST_F(IsolatedWebAppThrottleTest, BlockNavigationIntoIsolatedWebApp) {
  CommitBrowserInitiatedNavigation(kNonAppUrl);
  EXPECT_EQ(kNotIsolated, GetWebExposedIsolationLevel(main_frame_id()));

  auto simulator = StartRendererInitiatedNavigation(main_frame_id(), kAppUrl);

  auto start_result = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::BLOCK_REQUEST, start_result.action());
}

TEST_F(IsolatedWebAppThrottleTest, AllowNavigationIfNoPreviousPage) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kIsolatedApplication, GetWebExposedIsolationLevel(main_frame_id()));
}

TEST_F(IsolatedWebAppThrottleTest, AllowNavigationWithinIsolatedWebApp) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kIsolatedApplication, GetWebExposedIsolationLevel(main_frame_id()));

  CommitRendererInitiatedNavigation(main_frame_id(), kAppUrl2,
                                    coop_coep_headers());
  EXPECT_EQ(kIsolatedApplication, GetWebExposedIsolationLevel(main_frame_id()));
}

TEST_F(IsolatedWebAppThrottleTest, CancelCrossOriginNavigation) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kIsolatedApplication, GetWebExposedIsolationLevel(main_frame_id()));

  auto simulator =
      StartRendererInitiatedNavigation(main_frame_id(), kNonAppUrl);

  auto start_result = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::CANCEL, start_result.action());
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(1u, GetBrowserClient().GetOpenUrlCallCount());
#else
  EXPECT_EQ(1u, GetBrowserClient().GetExternalProtocolCallCount());
#endif
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      GetBrowserClient().GetLastPageTransition(),
      ui::PageTransition::PAGE_TRANSITION_LINK));

  simulator = StartRendererInitiatedNavigation(main_frame_id(), kNonAppUrl2);
  start_result = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::CANCEL, start_result.action());
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(2u, GetBrowserClient().GetOpenUrlCallCount());
#else
  EXPECT_EQ(2u, GetBrowserClient().GetExternalProtocolCallCount());
#endif
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      GetBrowserClient().GetLastPageTransition(),
      ui::PageTransition::PAGE_TRANSITION_LINK));
}

TEST_F(IsolatedWebAppThrottleTest, BlockRedirectOutOfIsolatedWebApp) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kIsolatedApplication, GetWebExposedIsolationLevel(main_frame_id()));

  auto simulator = StartRendererInitiatedNavigation(main_frame_id(), kAppUrl2);

  auto start_result = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::PROCEED, start_result.action());

  // Redirect to a non-app page.
  simulator->Redirect(GURL(kNonAppUrl));

  auto redirect_result = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::CANCEL, redirect_result.action());
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(1u, GetBrowserClient().GetOpenUrlCallCount());
#else
  EXPECT_EQ(1u, GetBrowserClient().GetExternalProtocolCallCount());
#endif
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      GetBrowserClient().GetLastPageTransition(),
      ui::PageTransition::PAGE_TRANSITION_SERVER_REDIRECT));
}

TEST_F(IsolatedWebAppThrottleTest, AllowIframeNavigationOutOfApp) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kIsolatedApplication, GetWebExposedIsolationLevel(main_frame_id()));
  FrameTreeNodeId iframe_id = CreateIframe(main_frame_id(), "test_frame");

  // Navigate the iframe to an app page.
  CommitRendererInitiatedNavigation(iframe_id, kAppUrl, corp_coep_headers());

  // Navigate the iframe to a non-app page.
  CommitRendererInitiatedNavigation(iframe_id, kNonAppUrl, corp_coep_headers());
}

TEST_F(IsolatedWebAppThrottleTest,
       BlockIframeRendererInitiatedNavigationIntoIsolatedWebApp) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kIsolatedApplication, GetWebExposedIsolationLevel(main_frame_id()));
  FrameTreeNodeId iframe_id = CreateIframe(main_frame_id(), "test_frame");

  // Navigate the iframe to a non-app page.
  CommitRendererInitiatedNavigation(iframe_id, kNonAppUrl, corp_coep_headers());

  // Perform a renderer-initiated navigation the iframe to an app page.
  auto simulator = StartRendererInitiatedNavigation(iframe_id, kAppUrl);

  auto start_result = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::BLOCK_REQUEST, start_result.action());
}

TEST_F(IsolatedWebAppThrottleTest,
       AllowIframeBrowserInitiatedNavigationIntoIsolatedWebApp) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kIsolatedApplication, GetWebExposedIsolationLevel(main_frame_id()));
  FrameTreeNodeId iframe_id = CreateIframe(main_frame_id(), "test_frame");

  // Navigate the iframe to an app page.
  CommitRendererInitiatedNavigation(iframe_id, kAppUrl, corp_coep_headers());

  // Navigate the iframe to a non-app page.
  CommitRendererInitiatedNavigation(iframe_id, kNonAppUrl, corp_coep_headers());

  // Go back. We can't use NavigationSimulator::GoBack because we need to set
  // coop headers.
  auto simulator = NavigationSimulator::CreateHistoryNavigation(
      -1, web_contents(), false /* is_renderer_initiated */);
  simulator->Start();

  // Create a new simulator to finish the navigation because the previous one
  // will reset its RenderFrameHost pointer after onunload has run, and will
  // incorrectly choose the main frame as the one being navigated.
  simulator = NavigationSimulatorImpl::CreateFromPendingInFrame(
      FrameTreeNode::GloballyFindByID(iframe_id));
  simulator->SetResponseHeaders(corp_coep_headers());
  simulator->Commit();

  auto commit_result = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::PROCEED, commit_result.action());
}

TEST_F(IsolatedWebAppThrottleTest,
       BlockIframeRedirectOutThenIntoIsolatedWebApp) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kIsolatedApplication, GetWebExposedIsolationLevel(main_frame_id()));
  FrameTreeNodeId iframe_id = CreateIframe(main_frame_id(), "test_frame");

  auto simulator = StartRendererInitiatedNavigation(iframe_id, kAppUrl);

  auto start_result = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::PROCEED, start_result.action());

  // Redirect to a non-app page.
  simulator->SetRedirectHeaders(corp_coep_headers());
  simulator->Redirect(GURL(kNonAppUrl));

  auto redirect_result1 = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::PROCEED, redirect_result1.action());
  EXPECT_EQ(GURL(kNonAppUrl), simulator->GetNavigationHandle()->GetURL());

  // Redirect back to an app page.
  simulator->SetRedirectHeaders(corp_coep_headers());
  simulator->Redirect(GURL(kAppUrl2));

  auto redirect_result2 = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::BLOCK_REQUEST, redirect_result2.action());
}

TEST_F(IsolatedWebAppThrottleTest, BlockIsolatedIframeInNonIsolatedIframe) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kIsolatedApplication, GetWebExposedIsolationLevel(main_frame_id()));

  // Create a non-app iframe.
  FrameTreeNodeId child_iframe_id =
      CreateIframe(main_frame_id(), "test_frame1");
  CommitRendererInitiatedNavigation(child_iframe_id, kNonAppUrl,
                                    corp_coep_headers());

  // Try to create an app iframe within the non-app iframe.
  FrameTreeNodeId grandchild_iframe_id =
      CreateIframe(child_iframe_id, "test_frame2");
  auto simulator =
      StartRendererInitiatedNavigation(grandchild_iframe_id, kAppUrl);

  auto start_result = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::BLOCK_REQUEST, start_result.action());
}

TEST_F(IsolatedWebAppThrottleTest, AllowHistoryNavigationFromErrorPage) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kIsolatedApplication, GetWebExposedIsolationLevel(main_frame_id()));

  auto* error_rfh = NavigationSimulator::NavigateAndFailFromDocument(
      GURL(kAppUrl2), net::ERR_TIMED_OUT,
      web_contents()->GetPrimaryMainFrame());
  EXPECT_NE(nullptr, error_rfh);
  EXPECT_EQ(GURL(kAppUrl2), error_rfh->GetLastCommittedURL());
  EXPECT_EQ(PAGE_TYPE_ERROR, GetPageType(error_rfh));

  // Go back. We can't use NavigationSimulator::GoBack because we need to set
  // coop headers.
  auto simulator = NavigationSimulator::CreateHistoryNavigation(
      -1, web_contents(), false /* is_renderer_initiated */);
  simulator->Start();
  simulator->SetResponseHeaders(coop_coep_headers());
  simulator->Commit();

  auto* app_rfh = simulator->GetFinalRenderFrameHost();
  EXPECT_NE(nullptr, app_rfh);
  EXPECT_EQ(GURL(kAppUrl), app_rfh->GetLastCommittedURL());
  EXPECT_EQ(PAGE_TYPE_NORMAL, GetPageType(app_rfh));
}

}  // namespace content
