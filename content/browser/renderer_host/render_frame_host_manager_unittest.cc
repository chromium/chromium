// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_host_manager.h"

#include <stdint.h>

#include <set>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/site_info.h"
#include "content/browser/site_instance_group.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/features.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/javascript_dialog_type.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/fake_local_frame.h"
#include "content/public/test/fake_remote_frame.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_utils.h"
#include "content/test/mock_widget_input_handler.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/render_document_feature.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_render_widget_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/public/browser/android/compositor.h"
#endif

namespace content {
namespace {

// VerifyPageFocusMessage from the mojo input handler.
void VerifyPageFocusMessage(TestRenderWidgetHost* twh, bool expected_focus) {
  MockWidgetInputHandler::MessageVector events =
      twh->GetMockWidgetInputHandler()->GetAndResetDispatchedMessages();
  EXPECT_EQ(1u, events.size());
  MockWidgetInputHandler::DispatchedFocusMessage* focus_message =
      events.at(0)->ToFocus();
  EXPECT_TRUE(focus_message);
  EXPECT_EQ(expected_focus, focus_message->focused());
}

class RenderFrameHostManagerTestWebUIControllerFactory
    : public WebUIControllerFactory {
 public:
  RenderFrameHostManagerTestWebUIControllerFactory() {}

  RenderFrameHostManagerTestWebUIControllerFactory(
      const RenderFrameHostManagerTestWebUIControllerFactory&) = delete;
  RenderFrameHostManagerTestWebUIControllerFactory& operator=(
      const RenderFrameHostManagerTestWebUIControllerFactory&) = delete;

  ~RenderFrameHostManagerTestWebUIControllerFactory() override {}

  // WebUIFactory implementation.
  std::unique_ptr<WebUIController> CreateWebUIControllerForURL(
      WebUI* web_ui,
      const GURL& url) override {
    // If WebUI creation is enabled for the test and this is a WebUI URL,
    // returns a new instance.
    if (HasWebUIScheme(url))
      return std::make_unique<WebUIController>(web_ui);
    return nullptr;
  }

  WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                             const GURL& url) override {
    // If WebUI creation is enabled for the test and this is a WebUI URL,
    // returns a mock WebUI type.
    if (HasWebUIScheme(url)) {
      return reinterpret_cast<WebUI::TypeID>(base::FastHash(url.host()));
    }
    return WebUI::kNoWebUI;
  }

  bool UseWebUIForURL(BrowserContext* browser_context,
                      const GURL& url) override {
    return HasWebUIScheme(url);
  }
};

class BeforeUnloadFiredWebContentsDelegate : public WebContentsDelegate {
 public:
  BeforeUnloadFiredWebContentsDelegate() {}

  BeforeUnloadFiredWebContentsDelegate(
      const BeforeUnloadFiredWebContentsDelegate&) = delete;
  BeforeUnloadFiredWebContentsDelegate& operator=(
      const BeforeUnloadFiredWebContentsDelegate&) = delete;

  ~BeforeUnloadFiredWebContentsDelegate() override {}

  void BeforeUnloadFired(WebContents* web_contents,
                         bool proceed,
                         bool* proceed_to_fire_unload) override {
    *proceed_to_fire_unload = proceed;
  }
};

class CloseWebContentsDelegate : public WebContentsDelegate {
 public:
  CloseWebContentsDelegate() : close_called_(false) {}

  CloseWebContentsDelegate(const CloseWebContentsDelegate&) = delete;
  CloseWebContentsDelegate& operator=(const CloseWebContentsDelegate&) = delete;

  ~CloseWebContentsDelegate() override {}

  void CloseContents(WebContents* web_contents) override {
    close_called_ = true;
  }

  bool is_closed() { return close_called_; }

 private:
  bool close_called_;
};

// This observer keeps track of the last deleted RenderViewHost to avoid
// accessing it and causing use-after-free condition.
class RenderViewHostDeletedObserver : public WebContentsObserver {
 public:
  explicit RenderViewHostDeletedObserver(RenderViewHost* rvh)
      : WebContentsObserver(WebContents::FromRenderViewHost(rvh)),
        process_id_(rvh->GetProcess()->GetID()),
        routing_id_(rvh->GetRoutingID()),
        deleted_(false) {}

  RenderViewHostDeletedObserver(const RenderViewHostDeletedObserver&) = delete;
  RenderViewHostDeletedObserver& operator=(
      const RenderViewHostDeletedObserver&) = delete;

  void RenderViewDeleted(RenderViewHost* render_view_host) override {
    if (render_view_host->GetProcess()->GetID() == process_id_ &&
        render_view_host->GetRoutingID() == routing_id_) {
      deleted_ = true;
    }
  }

  bool deleted() { return deleted_; }

 private:
  int process_id_;
  int routing_id_;
  bool deleted_;
};

// This observer keeps track of the last created RenderFrameHost to allow tests
// to ensure that no RenderFrameHost objects are created when not expected.
class RenderFrameHostCreatedObserver : public WebContentsObserver {
 public:
  explicit RenderFrameHostCreatedObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents), created_(false) {}

  RenderFrameHostCreatedObserver(const RenderFrameHostCreatedObserver&) =
      delete;
  RenderFrameHostCreatedObserver& operator=(
      const RenderFrameHostCreatedObserver&) = delete;

  void RenderFrameCreated(RenderFrameHost* render_frame_host) override {
    created_ = true;
  }

  bool created() { return created_; }

 private:
  bool created_;
};

// This observer is used to check whether IPC messages are being filtered for
// swapped out RenderFrameHost objects. It observes the plugin crash and favicon
// update events, which the FilterMessagesWhileSwappedOut test simulates being
// sent. The test is successful if the event is not observed.
// See http://crbug.com/351815
class PluginFaviconMessageObserver : public WebContentsObserver {
 public:
  explicit PluginFaviconMessageObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        plugin_crashed_(false),
        favicon_received_(false) {}

  PluginFaviconMessageObserver(const PluginFaviconMessageObserver&) = delete;
  PluginFaviconMessageObserver& operator=(const PluginFaviconMessageObserver&) =
      delete;

  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid) override {
    plugin_crashed_ = true;
  }

  void DidUpdateFaviconURL(
      RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override {
    favicon_received_ = true;
  }

  bool plugin_crashed() { return plugin_crashed_; }
  bool favicon_received() { return favicon_received_; }

 private:
  bool plugin_crashed_;
  bool favicon_received_;
};

// A shorter version for RenderFrameHostManager::DidNavigateFrame(rfh, ...).
// This provides all the arguments that aren't tested in this file.
void DidNavigateFrame(RenderFrameHostManager* rfh_manager,
                      RenderFrameHostImpl* rfh) {
  rfh_manager->DidNavigateFrame(rfh, true /* was_caused_by_user_gesture */,
                                false /* is_same_document_navigation */,
                                false /* clear_proxies_on_commit */,
                                blink::FramePolicy(),
                                true /* allow_paint_holding */);
}

class TestDevToolsClientHost : public DevToolsAgentHostClient {
 public:
  TestDevToolsClientHost() = default;

  TestDevToolsClientHost(const TestDevToolsClientHost&) = delete;
  TestDevToolsClientHost& operator=(const TestDevToolsClientHost&) = delete;

  void Close() { agent_host_->DetachClient(this); }

  void AgentHostClosed(DevToolsAgentHost* agent_host) override {}

  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {}

  void InspectAgentHost(DevToolsAgentHost* agent_host) {
    agent_host_ = agent_host;
    agent_host_->AttachClient(this);
  }

  DevToolsAgentHost* agent_host() { return agent_host_.get(); }

 private:
  scoped_refptr<DevToolsAgentHost> agent_host_;
};

}  // namespace

// Test that the "level" feature param has the expected effect.
class RenderDocumentFeatureTest : public testing::Test {
 protected:
  void SetLevel(const RenderDocumentLevel level) {
    InitAndEnableRenderDocumentFeature(&feature_list_,
                                       GetRenderDocumentLevelName(level));
  }

  void DisableRenderDocument() {
    feature_list_.InitAndDisableFeature(features::kRenderDocument);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(RenderDocumentFeatureTest, FeatureDisabled) {
  DisableRenderDocument();
  // Non-local-root subframe.
  EXPECT_FALSE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/false, /*is_local_root=*/false));

  // Local root subframe.
  EXPECT_FALSE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/false, /*is_local_root=*/true));

  // Main frame.
  EXPECT_FALSE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/true, /*is_local_root=*/true));

  // Crashed main frame.
  EXPECT_TRUE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/true, /*is_local_root=*/true,
      /*has_committed_any_navigation=*/true, /*must_be_replaced=*/true));
}

TEST_F(RenderDocumentFeatureTest, LevelCrashed) {
  SetLevel(RenderDocumentLevel::kCrashedFrame);
  // Non-local-root subframe.
  EXPECT_FALSE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/false, /*is_local_root=*/false));

  // Local root subframe.
  EXPECT_FALSE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/false, /*is_local_root=*/true));

  // Main frame.
  EXPECT_FALSE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/true, /*is_local_root=*/true));

  // Crashed main frame.
  EXPECT_TRUE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/true, /*is_local_root=*/true,
      /*has_committed_any_navigation=*/true, /*must_be_replaced=*/true));
}

TEST_F(RenderDocumentFeatureTest, LevelNonLocalRootSubframe) {
  SetLevel(RenderDocumentLevel::kNonLocalRootSubframe);
  // Non-local-root subframe.
  EXPECT_TRUE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/false, /*is_local_root=*/false));

  // Initial non-local-root subframe.
  EXPECT_FALSE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/false, /*is_local_root=*/false,
      /*has_committed_any_navigation=*/false));

  // Crashed non-local-root subframe.
  EXPECT_TRUE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/false, /*is_local_root=*/false,
      /*has_committed_any_navigation=*/false, /*must_be_replaced=*/true));
  EXPECT_TRUE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/false, /*is_local_root=*/false,
      /*has_committed_any_navigation=*/true, /*must_be_replaced=*/true));

  // Local root subframe.
  EXPECT_FALSE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/false, /*is_local_root=*/true));

  // Main frame.
  EXPECT_FALSE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/true, /*is_local_root=*/true));
}

TEST_F(RenderDocumentFeatureTest, LevelSubframe) {
  SetLevel(RenderDocumentLevel::kSubframe);

  // Non-local-root subframe.
  EXPECT_TRUE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/false, /*is_local_root=*/false));

  // Local root subframe.
  EXPECT_TRUE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/false, /*is_local_root=*/true));

  // Initial local root subframe.
  EXPECT_FALSE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/false, /*is_local_root=*/true,
      /*has_committed_any_navigation=*/false));

  // Crashed local root subframe.
  EXPECT_TRUE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/false, /*is_local_root=*/true,
      /*has_committed_any_navigation=*/false, /*must_be_replaced=*/true));
  EXPECT_TRUE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/false, /*is_local_root=*/true,
      /*has_committed_any_navigation=*/true, /*must_be_replaced=*/true));

  // Main frame.
  EXPECT_FALSE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/true, /*is_local_root=*/true));
}

TEST_F(RenderDocumentFeatureTest, LevelAllFrames) {
  SetLevel(RenderDocumentLevel::kAllFrames);

  // Non-local-root subframe.
  EXPECT_TRUE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/false, /*is_local_root=*/false));

  // Local root subframe.
  EXPECT_TRUE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/false, /*is_local_root=*/true));

  // Main frame.
  EXPECT_TRUE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/true, /*is_local_root=*/true));

  // Initial main frame.
  EXPECT_FALSE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/true, /*is_local_root=*/true,
      /*has_committed_any_navigation=*/false));

  // Crashed main frame.
  EXPECT_TRUE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/true, /*is_local_root=*/true,
      /*has_committed_any_navigation=*/false, /*must_be_replaced=*/true));
  EXPECT_TRUE(ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      /*is_main_frame=*/true, /*is_local_root=*/true,
      /*has_committed_any_navigation=*/true, /*must_be_replaced=*/true));
}

class RenderFrameHostManagerTest
    : public RenderViewHostImplTestHarness,
      public ::testing::WithParamInterface<std::string> {
 public:
  RenderFrameHostManagerTest() {
    InitAndEnableRenderDocumentFeature(&feature_list_, GetParam());
  }

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    if (IsIsolatedOriginRequiredToGuaranteeDedicatedProcess()) {
      // Isolate |isolated_cross_site_url()|so it cannot share a process
      // with another site.
      ChildProcessSecurityPolicyImpl::GetInstance()->AddFutureIsolatedOrigins(
          {url::Origin::Create(isolated_cross_site_url())},
          ChildProcessSecurityPolicy::IsolatedOriginSource::TEST,
          browser_context());

      // Reset the WebContents so the isolated origin will be honored by
      // all BrowsingInstances used in the test.
      SetContents(CreateTestWebContents());
    }
  }

  GURL isolated_cross_site_url() const {
    return GURL("http://isolated-cross-site.com");
  }

  // Creates an inactive test RenderViewHost.
  void CreateInactiveRenderViewHost() {
    const GURL kChromeURL(GetWebUIURL("foo"));
    const GURL kDestUrl("http://www.google.com/");

    // Navigate our first tab to a chrome url and then to the destination.
    NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kChromeURL);
    TestRenderFrameHost* ntp_rfh = contents()->GetPrimaryMainFrame();

    // Navigate to a cross-site URL.
    auto navigation =
        NavigationSimulator::CreateBrowserInitiated(kDestUrl, contents());
    navigation->ReadyToCommit();
    EXPECT_TRUE(contents()->CrossProcessNavigationPending());

    // Manually increase the number of active frames in the
    // SiteInstanceGroup that ntp_rfh belongs to, to prevent the
    // SiteInstanceGroup from being destroyed when ntp_rfh goes away.
    ntp_rfh->GetSiteInstance()->group()->IncrementActiveFrameCount();

    TestRenderFrameHost* dest_rfh =
        contents()->GetSpeculativePrimaryMainFrame();
    CHECK(dest_rfh);
    EXPECT_NE(ntp_rfh, dest_rfh);

    // Commit. This replaces ntp_rfh with a proxy and leaves its RVH inactive.
    navigation->Commit();
  }

  // Returns the RenderFrameHost that should be used in the navigation to
  // |entry|.
  RenderFrameHostImpl* NavigateToEntry(RenderFrameHostManager* manager,
                                       NavigationEntryImpl* entry) {
    // Tests currently only navigate using main frame FrameNavigationEntries.
    FrameNavigationEntry* frame_entry = entry->root_node()->frame_entry.get();
    FrameTreeNode* frame_tree_node =
        manager->current_frame_host()->frame_tree_node();
    NavigationControllerImpl& controller = manager->current_frame_host()
                                               ->frame_tree_node()
                                               ->navigator()
                                               .controller();
    blink::mojom::NavigationType navigate_type =
        entry->IsRestored() ? blink::mojom::NavigationType::RESTORE
                            : blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
    scoped_refptr<network::ResourceRequestBody> request_body;
    std::string post_content_type;
    bool is_form_submission = false;
    if (frame_entry->method() == "POST") {
      request_body = frame_entry->GetPostData(&post_content_type);
      // Might have a LF at end.
      post_content_type = std::string(
          base::TrimWhitespaceASCII(post_content_type, base::TRIM_ALL));
      is_form_submission = !!request_body;
    }

    auto& referrer = frame_entry->referrer();
    blink::mojom::CommonNavigationParamsPtr common_params =
        entry->ConstructCommonNavigationParams(
            *frame_entry, request_body, frame_entry->url(),
            blink::mojom::Referrer::New(referrer.url, referrer.policy),
            navigate_type, base::TimeTicks::Now(), base::TimeTicks::Now());
    blink::mojom::CommitNavigationParamsPtr commit_params =
        entry->ConstructCommitNavigationParams(
            *frame_entry, common_params->url, common_params->method,
            entry->GetSubframeUniqueNames(frame_tree_node),
            controller.GetPendingEntryIndex() == -1 /* intended_as_new_entry */,
            controller.GetIndexOfEntry(entry),
            controller.GetLastCommittedEntryIndex(), controller.GetEntryCount(),
            frame_tree_node->current_replication_state().frame_policy,
            frame_tree_node->AncestorOrSelfHasCSPEE(),
            blink::mojom::SystemEntropy::kNormal,
            /*soft_navigation_heuristics_task_id=*/std::nullopt);
    commit_params->post_content_type = post_content_type;

    std::unique_ptr<NavigationRequest> navigation_request =
        NavigationRequest::Create(
            frame_tree_node, std::move(common_params), std::move(commit_params),
            !entry->is_renderer_initiated(), false /* was_opener_suppressed */,
            std::nullopt /* initiator_frame_token */,
            ChildProcessHost::kInvalidUniqueID /* initiator_process_id */,
            entry->extra_headers(), frame_entry, entry, is_form_submission,
            nullptr /* navigation_ui_data */, std::nullopt /* impression */,
            blink::mojom::NavigationInitiatorActivationAndAdStatus::
                kDidNotStartWithTransientActivation,
            false /* is_pdf */);

    // Simulates request creation that triggers the 1st internal call to
    // GetFrameHostForNavigation.
    frame_tree_node->TakeNavigationRequest(std::move(navigation_request));

    // And also simulates the 2nd and final call to GetFrameHostForNavigation
    // that determines the final frame that will commit the navigation.
    BrowsingContextGroupSwap ignored_bcg_swap_info =
        BrowsingContextGroupSwap::CreateDefault();
    TestRenderFrameHost* frame_host = static_cast<TestRenderFrameHost*>(
        manager
            ->GetFrameHostForNavigation(frame_tree_node->navigation_request(),
                                        &ignored_bcg_swap_info)
            .value());
    CHECK(frame_host);

    frame_host->SetPolicyContainerHost(
        base::MakeRefCounted<PolicyContainerHost>());
    return frame_host;
  }

  // Returns the speculative RenderFrameHost.
  RenderFrameHostImpl* GetPendingFrameHost(RenderFrameHostManager* manager) {
    return manager->speculative_render_frame_host_.get();
  }

  // Exposes RenderFrameHostManager::CollectOpenerFrameTrees for testing.
  void CollectOpenerFrameTrees(
      FrameTreeNode* node,
      SiteInstanceGroup* site_instance_group,
      std::vector<FrameTree*>* opener_frame_trees,
      std::unordered_set<FrameTreeNode*>* nodes_with_back_links,
      std::unordered_set<FrameTreeNode*>*
          cross_browsing_context_group_openers) {
    node->render_manager()->CollectOpenerFrameTrees(
        site_instance_group, opener_frame_trees, nodes_with_back_links,
        cross_browsing_context_group_openers);
  }

 private:
  RenderFrameHostManagerTestWebUIControllerFactory factory_;
  ScopedWebUIControllerFactoryRegistration factory_registration_{&factory_};
  base::test::ScopedFeatureList feature_list_;
};

// Tests that when you navigate from a chrome:// url to another page, and
// then do that same thing in another tab, that the two resulting pages have
// different SiteInstances, BrowsingInstances, and RenderProcessHosts. This is
// a regression test for bug 9364.
TEST_P(RenderFrameHostManagerTest, ChromeSchemeProcesses) {
  const GURL kChromeUrl(GetWebUIURL("foo"));
  const GURL kDestUrl("http://www.google.com/");

  // Navigate our first tab to the chrome url and then to the destination,
  // ensuring we grant bindings to the chrome URL.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kChromeUrl);
  EXPECT_TRUE(
      main_rfh()->GetEnabledBindings().Has(BindingsPolicyValue::kWebUi));
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kDestUrl);

  EXPECT_FALSE(contents()->GetSpeculativePrimaryMainFrame());

  // Make a second tab.
  std::unique_ptr<TestWebContents> contents2(
      TestWebContents::Create(browser_context(), nullptr));

  // Load the two URLs in the second tab. Note that the first navigation creates
  // a RFH that's not pending (since there is no cross-site transition), so
  // we use the committed one.
  auto navigation1 =
      NavigationSimulator::CreateBrowserInitiated(kChromeUrl, contents2.get());
  navigation1->Start();
  EXPECT_FALSE(contents2->CrossProcessNavigationPending());
  navigation1->Commit();

  // The second one is the opposite, creating a cross-site transition.
  auto navigation2 =
      NavigationSimulator::CreateBrowserInitiated(kDestUrl, contents2.get());
  navigation2->Start();
  EXPECT_TRUE(contents2->CrossProcessNavigationPending());
  TestRenderFrameHost* dest_rfh2 = contents2->GetSpeculativePrimaryMainFrame();
  ASSERT_TRUE(dest_rfh2);

  navigation2->Commit();

  // The two RFH's should be different in every way.
  EXPECT_NE(contents()->GetPrimaryMainFrame()->GetProcess(),
            dest_rfh2->GetProcess());
  EXPECT_NE(contents()->GetPrimaryMainFrame()->GetSiteInstance(),
            dest_rfh2->GetSiteInstance());
  EXPECT_FALSE(dest_rfh2->GetSiteInstance()->IsRelatedSiteInstance(
      contents()->GetPrimaryMainFrame()->GetSiteInstance()));

  // Navigate both to a chrome://... URL, and verify that they have a separate
  // RenderProcessHost and a separate SiteInstance.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kChromeUrl);
  EXPECT_FALSE(contents()->GetSpeculativePrimaryMainFrame());

  NavigationSimulator::NavigateAndCommitFromBrowser(contents2.get(),
                                                    kChromeUrl);

  EXPECT_NE(contents()->GetPrimaryMainFrame()->GetSiteInstance(),
            contents2->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_NE(contents()->GetPrimaryMainFrame()->GetSiteInstance()->GetProcess(),
            contents2->GetPrimaryMainFrame()->GetSiteInstance()->GetProcess());
}

// Ensure that the browser ignores most IPC messages that arrive from a
// RenderViewHost that has been swapped out.  We do not want to take
// action on requests from a non-active renderer.  The main exception is
// for synchronous messages, which cannot be ignored without leaving the
// renderer in a stuck state.  See http://crbug.com/93427.
TEST_P(RenderFrameHostManagerTest, FilterMessagesWhileSwappedOut) {
  const GURL kChromeURL(GetWebUIURL("foo"));
  const GURL kDestUrl("http://www.google.com/");
  std::vector<blink::mojom::FaviconURLPtr> icons;

  // Navigate our first tab to a chrome url and then to the destination.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kChromeURL);
  TestRenderFrameHost* ntp_rfh = contents()->GetPrimaryMainFrame();

  // Send an update favicon message and make sure it works.
  {
    PluginFaviconMessageObserver observer(contents());
    ntp_rfh->UpdateFaviconURL(std::move(icons));
    EXPECT_TRUE(observer.favicon_received());
  }
  // Create one more frame in the same SiteInstanceGroup where ntp_rfh
  // exists so that it doesn't get deleted on navigation to another
  // site.
  ntp_rfh->GetSiteInstance()->group()->IncrementActiveFrameCount();

  // Navigate to a cross-site URL (don't unload to keep |ntp_rfh| alive).
  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(kDestUrl, contents());
  navigation->set_drop_unload_ack(true);
  navigation->Commit();
  TestRenderFrameHost* dest_rfh = contents()->GetPrimaryMainFrame();
  ASSERT_TRUE(dest_rfh);
  EXPECT_NE(ntp_rfh, dest_rfh);

  // The new RVH should be able to update its favicon.
  {
    PluginFaviconMessageObserver observer(contents());
    dest_rfh->UpdateFaviconURL(std::move(icons));
    EXPECT_TRUE(observer.favicon_received());
  }

  // The old renderer, being slow, now updates the favicon. It should be
  // filtered out and not take effect.
  {
    PluginFaviconMessageObserver observer(contents());
    ntp_rfh->UpdateFaviconURL(std::move(icons));
    EXPECT_FALSE(observer.favicon_received());
  }
}

// Test that the UpdateFaviconURL function is ignored if the
// renderer is in the pending deletion state. The favicon code assumes
// that it only gets UpdateFaviconURL function for the most
// recently committed navigation for each WebContentsImpl.
TEST_P(RenderFrameHostManagerTest, UpdateFaviconURLWhilePendingUnload) {
  const GURL kChromeURL(GetWebUIURL("foo"));
  const GURL kDestUrl("http://www.google.com/");
  std::vector<blink::mojom::FaviconURLPtr> icons;

  // Navigate our first tab to a chrome url and then to the destination.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kChromeURL);
  TestRenderFrameHost* ntp_rfh = contents()->GetPrimaryMainFrame();

  // Send an update favicon message and make sure it works.
  {
    PluginFaviconMessageObserver observer(contents());
    ntp_rfh->UpdateFaviconURL(std::move(icons));
    EXPECT_TRUE(observer.favicon_received());
  }

  // Create one more frame in the same SiteInstanceGroup where |ntp_rfh| exists
  // so that it doesn't get deleted on navigation to another site.
  ntp_rfh->GetSiteInstance()->group()->IncrementActiveFrameCount();

  // Navigate to a cross-site URL and commit the new page.
  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(kDestUrl, contents());
  navigation->set_drop_unload_ack(true);
  navigation->Commit();
  TestRenderFrameHost* dest_rfh = contents()->GetPrimaryMainFrame();
  EXPECT_TRUE(ntp_rfh->IsPendingDeletion());
  EXPECT_TRUE(dest_rfh->IsActive());

  // The new RFH should be able to update its favicons.
  {
    PluginFaviconMessageObserver observer(contents());
    dest_rfh->UpdateFaviconURL(std::move(icons));
    EXPECT_TRUE(observer.favicon_received());
  }

  // The old renderer, being slow, now updates its favicons. The message should
  // be ignored.
  {
    PluginFaviconMessageObserver observer(contents());
    ntp_rfh->UpdateFaviconURL(std::move(icons));
    EXPECT_FALSE(observer.favicon_received());
  }
}

// Test if RenderViewHost::GetRenderWidgetHosts() only returns active
// widgets.
TEST_P(RenderFrameHostManagerTest, GetRenderWidgetHostsReturnsActiveViews) {
  CreateInactiveRenderViewHost();
  std::unique_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHost::GetRenderWidgetHosts());

  // We know that there is the only one active widget. Another view is
  // now inactive, so the inactive view is not included in the list.
  RenderWidgetHost* widget = widgets->GetNextHost();
  EXPECT_FALSE(widgets->GetNextHost());
  RenderViewHost* rvh = RenderViewHost::From(widget);
  EXPECT_TRUE(static_cast<RenderViewHostImpl*>(rvh)->is_active());
}

// Test if RenderViewHost::GetRenderWidgetHosts() returns a subset of
// RenderViewHostImpl::GetAllRenderWidgetHosts().
// RenderViewHost::GetRenderWidgetHosts() returns only active widgets, but
// RenderViewHostImpl::GetAllRenderWidgetHosts() returns everything
// including inactive ones.
TEST_P(RenderFrameHostManagerTest,
       GetRenderWidgetHostsWithinGetAllRenderWidgetHosts) {
  CreateInactiveRenderViewHost();
  std::unique_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHost::GetRenderWidgetHosts());

  while (RenderWidgetHost* w = widgets->GetNextHost()) {
    bool found = false;
    std::unique_ptr<RenderWidgetHostIterator> all_widgets(
        RenderWidgetHostImpl::GetAllRenderWidgetHosts());
    while (RenderWidgetHost* widget = all_widgets->GetNextHost()) {
      if (w == widget) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }
}

// Test if SiteInstanceImpl::active_frame_count() is correctly updated
// as frames in a SiteInstance get replaced.
TEST_P(RenderFrameHostManagerTest, ActiveFrameCountWhileSwappingInAndOut) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();

  SiteInstanceImpl* instance1 = rfh1->GetSiteInstance();
  EXPECT_EQ(instance1->group()->active_frame_count(), 1U);

  // Create 2 new tabs and simulate them being the opener chain for the main
  // tab.  They should be in the same SiteInstance.
  std::unique_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), instance1));
  contents()->SetOpener(opener1.get());

  std::unique_ptr<TestWebContents> opener2(
      TestWebContents::Create(browser_context(), instance1));
  opener1->SetOpener(opener2.get());

  EXPECT_EQ(instance1->group()->active_frame_count(), 3U);

  // Navigate to a cross-site URL (different SiteInstance but same
  // BrowsingInstance).
  contents()->NavigateAndCommit(kUrl2);
  TestRenderFrameHost* rfh2 = main_test_rfh();
  SiteInstanceImpl* instance2 = rfh2->GetSiteInstance();

  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(instance1->IsDefaultSiteInstance());
    EXPECT_EQ(instance1->group()->active_frame_count(), 3U);
    EXPECT_EQ(instance1, instance2);
  } else {
    // rvh2 is on chromium.org which is different from google.com on
    // which other tabs are.
    EXPECT_EQ(instance2->group()->active_frame_count(), 1U);

    // There are two active views on google.com now.
    EXPECT_EQ(instance1->group()->active_frame_count(), 2U);
  }

  // Navigate to the original origin (google.com).
  contents()->NavigateAndCommit(kUrl1);

  EXPECT_EQ(instance1->group()->active_frame_count(), 3U);
}

// This deletes a WebContents when the given RVH is deleted. This is
// only for testing whether deleting an RVH does not cause any UaF in
// other parts of the system. For now, this class is only used for the
// next test cases to detect the bug mentioned at
// http://crbug.com/259859.
class RenderViewHostDestroyer : public WebContentsObserver {
 public:
  RenderViewHostDestroyer(RenderViewHost* render_view_host,
                          std::unique_ptr<WebContents> web_contents)
      : WebContentsObserver(WebContents::FromRenderViewHost(render_view_host)),
        render_view_host_(render_view_host),
        web_contents_(std::move(web_contents)) {}

  RenderViewHostDestroyer(const RenderViewHostDestroyer&) = delete;
  RenderViewHostDestroyer& operator=(const RenderViewHostDestroyer&) = delete;

  void RenderViewDeleted(RenderViewHost* render_view_host) override {
    if (render_view_host == render_view_host_)
      web_contents_.reset();
  }

 private:
  raw_ptr<RenderViewHost, DanglingUntriaged> render_view_host_;
  std::unique_ptr<WebContents> web_contents_;
};

// Test if ShutdownRenderViewHostsInSiteInstance() does not touch any
// RenderWidget that has been freed while deleting a RenderViewHost in
// a previous iteration. This is a regression test for
// http://crbug.com/259859.
TEST_P(RenderFrameHostManagerTest,
       DetectUseAfterFreeInShutdownRenderViewHostsInSiteInstance) {
  const GURL kChromeURL(GetWebUIURL("newtab"));
  const GURL kUrl1("http://www.google.com");
  const GURL kUrl2("http://www.chromium.org");

  // Navigate our first tab to a chrome url and then to the destination.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kChromeURL);
  TestRenderFrameHost* ntp_rfh = contents()->GetPrimaryMainFrame();

  // Create one more tab and navigate to kUrl1.  web_contents is not
  // wrapped as scoped_ptr since it intentionally deleted by destroyer
  // below as part of this test.
  std::unique_ptr<TestWebContents> web_contents =
      TestWebContents::Create(browser_context(), ntp_rfh->GetSiteInstance());
  web_contents->NavigateAndCommit(kUrl1);
  RenderViewHostDestroyer destroyer(ntp_rfh->GetRenderViewHost(),
                                    std::move(web_contents));

  // This causes the first tab to navigate to kUrl2, which destroys
  // the ntp_rfh in ShutdownRenderViewHostsInSiteInstance(). When
  // ntp_rfh is destroyed, it also destroys the RVHs in web_contents
  // too. This can test whether
  // SiteInstanceImpl::ShutdownRenderViewHostsInSiteInstance() can
  // touch any object freed in this way or not while iterating through
  // all widgets.
  contents()->NavigateAndCommit(kUrl2);
}

// Stub out local frame mojo binding. Intercepts calls to EnableViewSourceMode
// and marks the message as received. This class attaches to the first
// RenderFrameHostImpl created.
class EnableViewSourceLocalFrame : public content::FakeLocalFrame,
                                   public WebContentsObserver {
 public:
  explicit EnableViewSourceLocalFrame(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    if (!initialized_) {
      initialized_ = true;
      Init(navigation_handle->GetRenderFrameHost()
               ->GetRemoteAssociatedInterfaces());
    }
  }

  void EnableViewSourceMode() final { enabled_view_source_ = true; }

  bool IsViewSourceModeEnabled() const { return enabled_view_source_; }

  void ResetState() { enabled_view_source_ = false; }

 private:
  bool enabled_view_source_ = false;
  bool initialized_ = false;
};

// When there is an error with the specified page, renderer exits view-source
// mode. See WebFrameImpl::DidFail(). We check by this test that
// EnableViewSourceMode message is sent on every navigation regardless
// `blink::WebView` is being newly created or reused.
TEST_P(RenderFrameHostManagerTest, AlwaysSendEnableViewSourceMode) {
  const GURL kChromeUrl(GetWebUIURL("foo"));
  const GURL kUrl("http://foo/");
  const GURL kViewSourceUrl("view-source:http://foo/");

  // We have to navigate to some page at first since without this, the first
  // navigation will reuse the SiteInstance created by Init(), and the second
  // one will create a new SiteInstance. Because current_instance and
  // new_instance will be different, a new RenderViewHost will be created for
  // the second navigation. We have to avoid this in order to exercise the
  // target code path.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kChromeUrl);

  EnableViewSourceLocalFrame local_frame(contents());

  // Navigate. Note that "view source" URLs are implemented by putting the RFH
  // into a view-source mode and then navigating to the inner URL, so that's why
  // the bare URL is what's committed and returned by the last committed entry's
  // GetURL() call.
  auto navigation = NavigationSimulatorImpl::CreateBrowserInitiated(
      kViewSourceUrl, contents());
  navigation->Start();
  NavigationRequest* request =
      main_test_rfh()->frame_tree_node()->navigation_request();
  CHECK(request);
  ASSERT_TRUE(contents()->GetSpeculativePrimaryMainFrame())
      << "Expected new pending RenderFrameHost to be created.";
  RenderFrameHost* last_rfh = contents()->GetSpeculativePrimaryMainFrame();

  navigation->Commit();
  EXPECT_EQ(1, controller().GetLastCommittedEntryIndex());
  NavigationEntry* last_committed = controller().GetLastCommittedEntry();
  ASSERT_NE(nullptr, last_committed);
  EXPECT_EQ(kUrl, last_committed->GetURL());
  EXPECT_EQ(kViewSourceUrl, last_committed->GetVirtualURL());
  EXPECT_FALSE(controller().GetPendingEntry());

  // The RFH should have been put in view-source mode.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(local_frame.IsViewSourceModeEnabled());
  local_frame.ResetState();

  // Navigate, again.
  navigation = NavigationSimulatorImpl::CreateBrowserInitiated(kViewSourceUrl,
                                                               contents());

  navigation->Start();
  request = main_test_rfh()->frame_tree_node()->navigation_request();
  CHECK(request);

  // The same RenderFrameHost should be reused, unless RenderDocument for all
  // frames is enabled, which will create a new speculative RenderFrameHost.
  // In that case, view-source mode enabling will be done on the new RenderFrame
  // instead. To capture that, create a new EnableViewSourceLocalFrame for the
  // new RenderFrame.
  EnableViewSourceLocalFrame local_frame2(contents());
  navigation->ReadyToCommit();
  EXPECT_EQ(ShouldCreateNewHostForAllFrames(),
            !!contents()->GetSpeculativePrimaryMainFrame());
  EXPECT_EQ(last_rfh, contents()->GetPrimaryMainFrame());

  navigation->Commit();
  EXPECT_EQ(1, controller().GetLastCommittedEntryIndex());
  EXPECT_FALSE(controller().GetPendingEntry());

  // New message should be sent out to make sure to enter view-source mode.
  base::RunLoop().RunUntilIdle();
  if (ShouldCreateNewHostForAllFrames()) {
    EXPECT_TRUE(local_frame2.IsViewSourceModeEnabled());
  } else {
    EXPECT_TRUE(local_frame.IsViewSourceModeEnabled());
  }
}

// Tests the Init function by checking the initial RenderViewHost.
TEST_P(RenderFrameHostManagerTest, Init) {
  // Using TestBrowserContext.
  scoped_refptr<SiteInstanceImpl> instance =
      SiteInstanceImpl::Create(browser_context());
  EXPECT_FALSE(instance->HasSite());

  std::unique_ptr<TestWebContents> web_contents(
      TestWebContents::Create(browser_context(), instance));

  RenderFrameHostManager* manager =
      web_contents->GetPrimaryFrameTree().root()->render_manager();
  RenderFrameHostImpl* rfh = manager->current_frame_host();
  RenderViewHostImpl* rvh = rfh->render_view_host();
  ASSERT_TRUE(rfh);
  ASSERT_TRUE(rvh);
  EXPECT_EQ(instance, rfh->GetSiteInstance());
  EXPECT_EQ(web_contents.get(), rvh->GetDelegate());
  EXPECT_EQ(web_contents.get(), rfh->delegate());
  EXPECT_TRUE(manager->GetRenderWidgetHostView());
}

// Tests the Navigate function. We navigate three sites consecutively and check
// how the pending/committed RenderViewHost are modified.
TEST_P(RenderFrameHostManagerTest, Navigate) {
  std::unique_ptr<TestWebContents> web_contents(TestWebContents::Create(
      browser_context(), SiteInstance::Create(browser_context())));

  RenderFrameHostManager* manager =
      web_contents->GetPrimaryFrameTree().root()->render_manager();
  RenderFrameHostImpl* host = nullptr;
  RenderViewHost* initial_render_view_host = nullptr;

  // 1) The first navigation. --------------------------
  const GURL kUrl1("http://www.google.com/");
  NavigationEntryImpl entry1(
      nullptr /* instance */, kUrl1, Referrer(),
      /* initiator_origin= */ std::nullopt,
      /* initiator_base_url= */ std::nullopt, std::u16string() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);
  host = NavigateToEntry(manager, &entry1);

  // The RenderFrameHost created in Init will be reused.
  EXPECT_TRUE(host == manager->current_frame_host());
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // Commit.
  DidNavigateFrame(manager, host);
  // Commit to SiteInstance should be delayed until RenderFrame commit.
  EXPECT_TRUE(host == manager->current_frame_host());
  ASSERT_TRUE(host);
  EXPECT_FALSE(host->GetSiteInstance()->HasSite());
  host->GetSiteInstance()->SetSite(UrlInfo::CreateForTesting(kUrl1));

  manager->GetRenderWidgetHostView()->SetBackgroundColor(SK_ColorRED);

  // 2) Navigate to next site. -------------------------
  const GURL kUrl2("http://www.google.com/foo");
  const url::Origin kInitiatorOrigin =
      url::Origin::Create(GURL("https://initiator.example.com"));
  NavigationEntryImpl entry2(
      nullptr /* instance */, kUrl2,
      Referrer(kUrl1, network::mojom::ReferrerPolicy::kDefault),
      kInitiatorOrigin, /* initiator_base_url= */ std::nullopt,
      std::u16string() /* title */, ui::PAGE_TRANSITION_LINK,
      true /* is_renderer_init */, nullptr /* blob_url_loader_factory */,
      false /* is_initial_entry */);
  host = NavigateToEntry(manager, &entry2);
  initial_render_view_host = host->GetRenderViewHost();
  EXPECT_TRUE(initial_render_view_host);

  // The RenderFrameHost created in Init will be reused.
  EXPECT_TRUE(host == manager->current_frame_host());
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // Commit.
  DidNavigateFrame(manager, host);
  EXPECT_TRUE(host == manager->current_frame_host());
  ASSERT_TRUE(host);
  EXPECT_TRUE(host->GetSiteInstance()->HasSite());

  ASSERT_TRUE(manager->GetRenderWidgetHostView()->GetBackgroundColor());
  EXPECT_EQ(SK_ColorRED,
            *manager->GetRenderWidgetHostView()->GetBackgroundColor());

  // 3) Cross-site navigate to next site. --------------
  const GURL kUrl3("http://webkit.org/");
  NavigationEntryImpl entry3(
      nullptr /* instance */, kUrl3,
      Referrer(kUrl2, network::mojom::ReferrerPolicy::kDefault),
      /* initiator_origin= */ std::nullopt,
      /* initiator_base_url= */ std::nullopt, std::u16string() /* title */,
      ui::PAGE_TRANSITION_LINK, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);
  host = NavigateToEntry(manager, &entry3);

  // A new RenderFrameHost should be created.
  EXPECT_TRUE(GetPendingFrameHost(manager));
  ASSERT_EQ(host, GetPendingFrameHost(manager));

  // Commit.
  DidNavigateFrame(manager, GetPendingFrameHost(manager));
  EXPECT_TRUE(host == manager->current_frame_host());
  ASSERT_TRUE(host);
  EXPECT_TRUE(host->GetSiteInstance()->HasSite());
  EXPECT_NE(initial_render_view_host, host->GetRenderViewHost());

  // Check the pending RenderFrameHost has been committed.
  EXPECT_FALSE(GetPendingFrameHost(manager));

  ASSERT_TRUE(manager->GetRenderWidgetHostView()->GetBackgroundColor());
  EXPECT_EQ(SK_ColorRED,
            *manager->GetRenderWidgetHostView()->GetBackgroundColor());
}

// Tests WebUI creation.
TEST_P(RenderFrameHostManagerTest, WebUI) {
  scoped_refptr<SiteInstance> instance =
      SiteInstance::Create(browser_context());

  std::unique_ptr<TestWebContents> web_contents(
      TestWebContents::Create(browser_context(), instance));
  RenderFrameHostManager* manager =
      web_contents->GetPrimaryFrameTree().root()->render_manager();
  RenderFrameHostImpl* initial_rfh = manager->current_frame_host();

  EXPECT_FALSE(initial_rfh->render_view_host()->IsRenderViewLive());
  EXPECT_FALSE(manager->current_frame_host()->web_ui());
  EXPECT_TRUE(initial_rfh);

  const GURL kUrl(GetWebUIURL("foo"));
  NavigationEntryImpl entry(
      nullptr /* instance */, kUrl, Referrer(),
      /* initiator_origin= */ std::nullopt,
      /* initiator_base_url= */ std::nullopt, std::u16string() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);
  RenderFrameHostImpl* host = NavigateToEntry(manager, &entry);

  // The initial non-live RenderFrameHost should be reused for the new WebUI
  // navigation.  We test a case where it is live in WebUIInNewTab.
  EXPECT_TRUE(host);
  EXPECT_EQ(initial_rfh, host);
  EXPECT_EQ(host, manager->current_frame_host());
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // It's important that the SiteInstance get set on the Web UI page as soon
  // as the navigation starts, rather than lazily after it commits, so we don't
  // try to re-use the SiteInstance/process for non Web UI things that may
  // get loaded in between.
  EXPECT_TRUE(host->GetSiteInstance()->HasSite());
  EXPECT_EQ(kUrl, host->GetSiteInstance()->GetSiteURL());

  // There will be a WebUI because GetFrameHostForNavigation was already called
  // twice.
  EXPECT_TRUE(host->web_ui());
  EXPECT_TRUE(manager->current_frame_host()->web_ui());

  // Commit.
  DidNavigateFrame(manager, host);
  EXPECT_TRUE(host->GetEnabledBindings().Has(BindingsPolicyValue::kWebUi));
}

// Tests that we can open a WebUI link in a new tab from a WebUI page and still
// grant the correct bindings.  http://crbug.com/189101.
TEST_P(RenderFrameHostManagerTest, WebUIInNewTab) {
  scoped_refptr<SiteInstance> blank_instance =
      SiteInstance::Create(browser_context());
  blank_instance->GetProcess()->Init();

  // Create a blank tab.
  std::unique_ptr<TestWebContents> web_contents1(
      TestWebContents::Create(browser_context(), blank_instance));
  RenderFrameHostManager* manager1 =
      web_contents1->GetPrimaryFrameTree().root()->render_manager();
  // Test the case that new RVH is considered live.
  RenderViewHostImpl* rvh1 = manager1->current_frame_host()->render_view_host();
  rvh1->CreateRenderView(std::nullopt, MSG_ROUTING_NONE, false);
  EXPECT_TRUE(rvh1->IsRenderViewLive());
  EXPECT_TRUE(manager1->current_frame_host()->IsRenderFrameLive());

  // Navigate to a WebUI page.
  const GURL kUrl1(GetWebUIURL("foo"));
  NavigationEntryImpl entry1(
      nullptr /* instance */, kUrl1, Referrer(),
      /* initiator_origin= */ std::nullopt,
      /* initiator_base_url= */ std::nullopt, std::u16string() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);
  RenderFrameHostImpl* host1 = NavigateToEntry(manager1, &entry1);

  // Because we've called CreateRenderView(), the initial RenderFrameHost is
  // live prior to the navigation start, but it can still be reused for the
  // WebUI RenderFrameHost, since it hasn't committed any navigations and it has
  // an unassigned SiteInstance and unlocked process.  There should be no
  // speculative RenderFrameHost.
  EXPECT_FALSE(GetPendingFrameHost(manager1));
  EXPECT_EQ(host1, manager1->current_frame_host());

  // At this point, the initial RFH should have set the WebUI bindings.  This
  // should happen as part of selecting that RFH for the WebUI navigation in
  // GetFrameHostForNavigation().
  EXPECT_TRUE(host1->GetEnabledBindings().Has(BindingsPolicyValue::kWebUi));

  // Commit and ensure we still have bindings.
  DidNavigateFrame(manager1, host1);
  SiteInstance* webui_instance = host1->GetSiteInstance();
  EXPECT_EQ(host1, manager1->current_frame_host());
  EXPECT_TRUE(host1->GetEnabledBindings().Has(BindingsPolicyValue::kWebUi));

  // Now simulate clicking a link that opens in a new tab.
  std::unique_ptr<TestWebContents> web_contents2(
      TestWebContents::Create(browser_context(), webui_instance));
  RenderFrameHostManager* manager2 =
      web_contents2->GetPrimaryFrameTree().root()->render_manager();
  // Make sure the new RVH is considered live.  This is usually done in
  // RenderWidgetHost::Init when opening a new tab from a link.
  RenderViewHostImpl* rvh2 = manager2->current_frame_host()->render_view_host();
  rvh2->CreateRenderView(std::nullopt, MSG_ROUTING_NONE, false);
  EXPECT_TRUE(rvh2->IsRenderViewLive());

  const GURL kUrl2(GetWebUIURL("foo/bar"));
  const url::Origin kInitiatorOrigin =
      url::Origin::Create(GURL("https://initiator.example.com"));
  NavigationEntryImpl entry2(
      nullptr /* instance */, kUrl2, Referrer(), kInitiatorOrigin,
      /* initiator_base_url= */ std::nullopt, std::u16string() /* title */,
      ui::PAGE_TRANSITION_LINK, true /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);
  RenderFrameHostImpl* host2 = NavigateToEntry(manager2, &entry2);

  // No cross-process transition happens because we are already in the right
  // SiteInstance.  We should grant bindings immediately.
  EXPECT_EQ(host2, manager2->current_frame_host());
  EXPECT_TRUE(host2->web_ui());
  EXPECT_TRUE(host2->GetEnabledBindings().Has(BindingsPolicyValue::kWebUi));

  DidNavigateFrame(manager2, host2);
}

// Tests that a WebUI is correctly reused between chrome:// pages.
TEST_P(RenderFrameHostManagerTest, WebUIWasReused) {
  // Navigate to a WebUI page.
  const GURL kUrl1(GetWebUIURL("foo"));
  contents()->NavigateAndCommit(kUrl1);
  WebUIImpl* web_ui = main_test_rfh()->web_ui();
  EXPECT_TRUE(web_ui);

  // Navigate to another WebUI page which should be same-site the same WebUI
  // object is reused if the RenderFrameHost is reused.
  const GURL kUrl2(GetWebUIURL("foo/bar"));
  contents()->NavigateAndCommit(kUrl2);
  if (ShouldCreateNewHostForAllFrames()) {
    EXPECT_NE(web_ui, main_test_rfh()->web_ui());
  } else {
    EXPECT_EQ(web_ui, main_test_rfh()->web_ui());
  }
}

// Tests that a WebUI is correctly cleaned up when navigating from a chrome://
// page to a non-chrome:// page.
TEST_P(RenderFrameHostManagerTest, WebUIWasCleared) {
  // Navigate to a WebUI page.
  const GURL kUrl1(GetWebUIURL("foo"));
  contents()->NavigateAndCommit(kUrl1);
  EXPECT_TRUE(main_test_rfh()->web_ui());

  // Navigate to a non-WebUI page.
  const GURL kUrl2("http://www.google.com");
  contents()->NavigateAndCommit(kUrl2);
  EXPECT_FALSE(main_test_rfh()->web_ui());
}

// Ensure that we can go back and forward even if a unload ACK isn't received.
// See http://crbug.com/93427.
TEST_P(RenderFrameHostManagerTest, NavigateAfterMissingUnloadACK) {
  // When a page enters the BackForwardCache, the RenderFrameHost is not
  // deleted.  Similarly, no
  // mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame message is sent.
  contents()->GetController().GetBackForwardCache().DisableForTesting(
      BackForwardCache::TEST_REQUIRES_NO_CACHING);
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2 = isolated_cross_site_url();

  // Navigate to two pages.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();

  // Keep active_frame_count nonzero so that no unloaded frames in this
  // SiteInstanceGroup get forcefully deleted.
  rfh1->GetSiteInstance()->group()->IncrementActiveFrameCount();

  contents()->NavigateAndCommit(kUrl2);
  TestRenderFrameHost* rfh2 = main_test_rfh();
  rfh2->GetSiteInstance()->group()->IncrementActiveFrameCount();

  // Now go back, but suppose the
  // mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame isn't received.  This
  // shouldn't happen, but we have seen it when going back quickly across many
  // entries (http://crbug.com/93427).
  auto back_navigation1 = NavigationSimulatorImpl::CreateHistoryNavigation(
      -1, contents(), false /* is_renderer_initiated */);
  back_navigation1->ReadyToCommit();
  EXPECT_FALSE(rfh2->is_waiting_for_beforeunload_completion());

  // The back navigation commits.
  back_navigation1->set_drop_unload_ack(true);
  back_navigation1->Commit();
  EXPECT_TRUE(rfh2->IsWaitingForUnloadACK());
  EXPECT_TRUE(rfh2->IsPendingDeletion());

  // We should be able to navigate forward.
  NavigationSimulator::GoForward(contents());
  EXPECT_TRUE(main_test_rfh()->IsActive());
}

// Test that we create `blink::RemoteFrame` objects for the opener chain when
// navigating an opened tab cross-process.  This allows us to support certain
// cross-process JavaScript calls (http://crbug.com/99202).
TEST_P(RenderFrameHostManagerTest, CreateProxiesForOpeners) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2 = isolated_cross_site_url();
  const GURL kChromeUrl(GetWebUIURL("foo"));

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  RenderFrameHostManager* manager =
      contents()->GetPrimaryFrameTree().root()->render_manager();
  TestRenderFrameHost* rfh1 = main_test_rfh();
  scoped_refptr<SiteInstanceImpl> site_instance1 = rfh1->GetSiteInstance();
  RenderFrameDeletedObserver rfh1_deleted_observer(rfh1);
  TestRenderViewHost* rvh1 = test_rvh();
  EXPECT_EQ(AreDefaultSiteInstancesEnabled(),
            site_instance1->IsDefaultSiteInstance());

  // Create 2 new tabs and simulate them being the opener chain for the main
  // tab.  They should be in the same SiteInstance.
  std::unique_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), site_instance1.get()));
  RenderFrameHostManager* opener1_manager =
      opener1->GetPrimaryFrameTree().root()->render_manager();
  contents()->SetOpener(opener1.get());

  std::unique_ptr<TestWebContents> opener2(
      TestWebContents::Create(browser_context(), site_instance1.get()));
  RenderFrameHostManager* opener2_manager =
      opener2->GetPrimaryFrameTree().root()->render_manager();
  opener1->SetOpener(opener2.get());

  // Navigate to a cross-site URL (different SiteInstance but same
  // BrowsingInstance).
  contents()->NavigateAndCommit(kUrl2);
  TestRenderFrameHost* rfh2 = main_test_rfh();
  EXPECT_NE(site_instance1, rfh2->GetSiteInstance());
  EXPECT_TRUE(site_instance1->IsRelatedSiteInstance(rfh2->GetSiteInstance()));

  // Ensure rvh1 is kept with the proxy of the current tab.
  EXPECT_TRUE(rfh1_deleted_observer.deleted());
  EXPECT_EQ(rvh1, manager->current_frame_host()
                      ->browsing_context_state()
                      ->GetRenderFrameProxyHost(site_instance1->group())
                      ->GetRenderViewHost());

  // Ensure a proxy and inactive RVH are created in the first opener tab.
  RenderFrameProxyHost* rfph1 =
      opener1_manager->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(rfh2->GetSiteInstance()->group());
  TestRenderViewHost* opener1_rvh =
      static_cast<TestRenderViewHost*>(rfph1->GetRenderViewHost());
  EXPECT_FALSE(opener1_rvh->is_active());

  // Ensure a proxy and inactive RVH are created in the second opener tab.
  RenderFrameProxyHost* rfph2 =
      opener2_manager->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(rfh2->GetSiteInstance()->group());
  TestRenderViewHost* opener2_rvh =
      static_cast<TestRenderViewHost*>(rfph2->GetRenderViewHost());
  EXPECT_FALSE(opener2_rvh->is_active());

  // Navigate to a cross-BrowsingInstance URL.
  contents()->NavigateAndCommit(kChromeUrl);
  TestRenderFrameHost* rfh3 = main_test_rfh();
  EXPECT_NE(site_instance1, rfh3->GetSiteInstance());
  EXPECT_FALSE(site_instance1->IsRelatedSiteInstance(rfh3->GetSiteInstance()));

  // No scripting is allowed across BrowsingInstances, so we should not create
  // proxies for the opener chain in this case.
  EXPECT_FALSE(opener1_manager->current_frame_host()
                   ->browsing_context_state()
                   ->GetRenderFrameProxyHost(rfh3->GetSiteInstance()->group()));
  EXPECT_FALSE(opener2_manager->current_frame_host()
                   ->browsing_context_state()
                   ->GetRenderFrameProxyHost(rfh3->GetSiteInstance()->group()));
}

// Test that a page can disown the opener of the WebContents.
TEST_P(RenderFrameHostManagerTest, DisownOpener) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2 = isolated_cross_site_url();

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();
  scoped_refptr<SiteInstanceImpl> site_instance1 = rfh1->GetSiteInstance();
  EXPECT_EQ(AreDefaultSiteInstancesEnabled(),
            site_instance1->IsDefaultSiteInstance());

  // Create a new tab and simulate having it be the opener for the main tab.
  std::unique_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), rfh1->GetSiteInstance()));
  contents()->SetOpener(opener1.get());
  EXPECT_TRUE(contents()->HasOpener());

  // Navigate to a cross-site URL (different SiteInstance but same
  // BrowsingInstance).
  contents()->NavigateAndCommit(kUrl2);
  TestRenderFrameHost* rfh2 = main_test_rfh();
  EXPECT_NE(site_instance1, rfh2->GetSiteInstance());

  // Disown the opener from rfh2.
  rfh2->SimulateDidChangeOpener(std::nullopt);

  // Ensure the opener is cleared.
  EXPECT_FALSE(contents()->HasOpener());
}

// Test that a page can disown a same-site opener of the WebContents.
TEST_P(RenderFrameHostManagerTest, DisownSameSiteOpener) {
  const GURL kUrl1("http://www.google.com/");

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();

  // Create a new tab and simulate having it be the opener for the main tab.
  std::unique_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), rfh1->GetSiteInstance()));
  contents()->SetOpener(opener1.get());
  EXPECT_TRUE(contents()->HasOpener());

  // Disown the opener from rfh1.
  rfh1->SimulateDidChangeOpener(std::nullopt);

  // Ensure the opener is cleared even if it is in the same process.
  EXPECT_FALSE(contents()->HasOpener());
}

// Test that a page can disown the opener just as a cross-process navigation is
// in progress.
TEST_P(RenderFrameHostManagerTest, DisownOpenerDuringNavigation) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2 = isolated_cross_site_url();

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  scoped_refptr<SiteInstanceImpl> site_instance1 =
      main_test_rfh()->GetSiteInstance();
  EXPECT_EQ(AreDefaultSiteInstancesEnabled(),
            site_instance1->IsDefaultSiteInstance());

  // Create a new tab and simulate having it be the opener for the main tab.
  std::unique_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), site_instance1.get()));
  contents()->SetOpener(opener1.get());
  EXPECT_TRUE(contents()->HasOpener());

  // Navigate to a cross-site URL (different SiteInstance but same
  // BrowsingInstance).
  contents()->NavigateAndCommit(kUrl2);
  TestRenderFrameHost* rfh2 = main_test_rfh();
  EXPECT_NE(site_instance1, rfh2->GetSiteInstance());

  // Start a back navigation.
  contents()->GetController().GoBack();
  contents()->GetPrimaryMainFrame()->PrepareForCommit();

  // Disown the opener from rfh2.
  rfh2->SimulateDidChangeOpener(std::nullopt);

  // Ensure the opener is cleared.
  EXPECT_FALSE(contents()->HasOpener());

  // The back navigation commits.
  NavigationEntry* entry1 = contents()->GetController().GetPendingEntry();
  contents()->GetSpeculativePrimaryMainFrame()->SendNavigateWithTransition(
      entry1->GetUniqueID(), false, entry1->GetURL(),
      entry1->GetTransitionType());

  // Ensure the opener is still cleared.
  EXPECT_FALSE(contents()->HasOpener());
}

// Test that a page can disown the opener just after a cross-process navigation
// commits.
TEST_P(RenderFrameHostManagerTest, DisownOpenerAfterNavigation) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2 = isolated_cross_site_url();

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  scoped_refptr<SiteInstanceImpl> site_instance1 =
      main_test_rfh()->GetSiteInstance();
  EXPECT_EQ(AreDefaultSiteInstancesEnabled(),
            site_instance1->IsDefaultSiteInstance());

  // Create a new tab and simulate having it be the opener for the main tab.
  std::unique_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), site_instance1.get()));
  contents()->SetOpener(opener1.get());
  EXPECT_TRUE(contents()->HasOpener());

  // Navigate to a cross-site URL (different SiteInstance but same
  // BrowsingInstance).
  contents()->NavigateAndCommit(kUrl2);
  TestRenderFrameHost* rfh2 = main_test_rfh();
  EXPECT_NE(site_instance1, rfh2->GetSiteInstance());

  // Commit a back navigation before the DidChangeOpener message arrives.
  contents()->GetController().GoBack();
  contents()->GetPrimaryMainFrame()->PrepareForCommit();
  NavigationEntry* entry1 = contents()->GetController().GetPendingEntry();
  contents()->GetSpeculativePrimaryMainFrame()->SendNavigateWithTransition(
      entry1->GetUniqueID(), false, entry1->GetURL(),
      entry1->GetTransitionType());

  // Disown the opener from rfh2.
  rfh2->SimulateDidChangeOpener(std::nullopt);
  EXPECT_FALSE(contents()->HasOpener());
}

// Test that we clean up RenderFrameProxyHosts when a process hosting the
// associated frames crashes. http://crbug.com/258993
TEST_P(RenderFrameHostManagerTest, CleanUpProxiesOnProcessCrash) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2 = isolated_cross_site_url();

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = contents()->GetPrimaryMainFrame();

  // Create a new tab as an opener for the main tab.
  std::unique_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), rfh1->GetSiteInstance()));
  RenderFrameHostManager* opener1_manager =
      opener1->GetPrimaryFrameTree().root()->render_manager();
  contents()->SetOpener(opener1.get());

  // Make sure the new opener RVH is considered live.
  RenderViewHostImpl* opener_rvh =
      opener1_manager->current_frame_host()->render_view_host();
  opener_rvh->CreateRenderView(std::nullopt, MSG_ROUTING_NONE, false);
  EXPECT_TRUE(opener_rvh->IsRenderViewLive());
  EXPECT_TRUE(opener1_manager->current_frame_host()->IsRenderFrameLive());

  // Use a cross-process navigation in the opener to make the old RVH inactive.
  EXPECT_FALSE(opener1_manager->current_frame_host()
                   ->browsing_context_state()
                   ->GetRenderFrameProxyHost(rfh1->GetSiteInstance()->group()));
  opener1->NavigateAndCommit(kUrl2);
  RenderFrameProxyHost* rfph1 =
      opener1_manager->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(rfh1->GetSiteInstance()->group());
  RenderViewHostImpl* rvh1 = rfph1->GetRenderViewHost();
  EXPECT_TRUE(rvh1);
  EXPECT_FALSE(rvh1->is_active());

  // Fake a process crash.
  rfh1->GetProcess()->SimulateCrash();

  // Ensure that the RenderFrameProxyHost stays around and the RenderFrameProxy
  // is deleted.
  RenderFrameProxyHost* render_frame_proxy_host =
      opener1_manager->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(rfh1->GetSiteInstance()->group());
  EXPECT_EQ(rfph1, render_frame_proxy_host);
  EXPECT_FALSE(render_frame_proxy_host->is_render_frame_proxy_live());

  // Expect the RVH to exist but not be live.
  EXPECT_TRUE(rfph1->GetRenderViewHost());
  EXPECT_FALSE(rfph1->GetRenderViewHost()->IsRenderViewLive());

  // Reload the initial tab. This should recreate the opener's RVH in the
  // original SiteInstanceGroup.
  contents()->GetController().Reload(ReloadType::NORMAL, true);
  contents()->GetPrimaryMainFrame()->PrepareForCommit();
  TestRenderFrameHost* rfh2 = contents()->GetPrimaryMainFrame();
  EXPECT_TRUE(opener1_manager->current_frame_host()
                  ->browsing_context_state()
                  ->GetRenderFrameProxyHost(rfh2->GetSiteInstance()->group())
                  ->GetRenderViewHost()
                  ->IsRenderViewLive());
  EXPECT_EQ(opener1_manager->GetFrameTokenForSiteInstanceGroup(
                rfh2->GetSiteInstance()->group()),
            rfh2->GetRenderViewHost()->opener_frame_token());
}

// Test guest navigation behavior when navigating across sites.  Since guests
// support site isolation, we should swap guest SiteInstances as usual.
TEST_P(RenderFrameHostManagerTest, GuestNavigations) {
  // Create a custom StoragePartitionConfig for the guest SiteInstance. The
  // resulting SiteInstance should become associated with this
  // StoragePartitionConfig rather than a default one.
  const StoragePartitionConfig kGuestPartitionConfig =
      StoragePartitionConfig::Create(browser_context(), "someapp",
                                     "somepartition", /*in_memory=*/false);
  scoped_refptr<SiteInstance> initial_instance =
      SiteInstance::CreateForGuest(browser_context(), kGuestPartitionConfig);
  std::unique_ptr<TestWebContents> web_contents(
      TestWebContents::Create(browser_context(), initial_instance));

  EXPECT_TRUE(initial_instance->IsGuest());
  EXPECT_EQ(kGuestPartitionConfig,
            initial_instance->GetStoragePartitionConfig());

  RenderFrameHostManager* manager =
      web_contents->GetPrimaryFrameTree().root()->render_manager();
  RenderFrameHostImpl* initial_host = manager->current_frame_host();

  // 1) First navigation. ------------------------
  // Start the first navigation, but do not commit.
  const GURL kUrl1("http://www.google.com/");
  NavigationEntryImpl entry1(
      nullptr /* instance */, kUrl1, Referrer(),
      /* initiator_origin= */ std::nullopt,
      /* initiator_base_url= */ std::nullopt, std::u16string() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);
  RenderFrameHostImpl* host = NavigateToEntry(manager, &entry1);

  // The SiteInstance of the navigating RenderFrameHost should still be a guest
  // SiteInstance in the same StoragePartition.
  scoped_refptr<SiteInstanceImpl> first_instance = host->GetSiteInstance();
  EXPECT_EQ(first_instance->GetStoragePartitionConfig(), kGuestPartitionConfig);
  EXPECT_TRUE(first_instance->IsGuest());

  // We have to swap SiteInstances and RenderFrameHosts, since the initial
  // SiteInstance (`instance`) has an empty site and process lock, whereas the
  // navigation needs a SiteInstance with the site URL that corresponds to
  // `kUrl1`.  Note that there will be no speculative RenderFrameHost in that
  // case, since the new RenderFrameHost will be committed right away due to
  // the early commit optimization. This behavior may change if the early
  // commit optimization is removed in https://crbug.com/1072817.
  EXPECT_NE(first_instance, initial_instance);
  EXPECT_NE(host, initial_host);
  // This test may run without strict site isolation, e.g. on Android.  In
  // that case, the navigation will end up in a default SiteInstance.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_EQ("http://google.com/",
              first_instance->GetSiteInfo().site_url().spec());
  } else {
    EXPECT_TRUE(first_instance->IsDefaultSiteInstance());
  }
  EXPECT_FALSE(manager->speculative_frame_host());
  EXPECT_EQ(host, manager->current_frame_host());

  // Commit.
  DidNavigateFrame(manager, host);
  EXPECT_EQ(host, manager->current_frame_host());
  ASSERT_TRUE(host);
  EXPECT_TRUE(host->GetSiteInstance()->HasSite());

  // 2) Second navigation. ------------------------
  // Navigate to a different site. If strict site isolation is enabled, this
  // will swap processes. Otherwise, the guest will stay in the same process.
  const GURL kUrl2("http://www.chromium.org");
  const url::Origin kInitiatorOrigin =
      url::Origin::Create(GURL("https://initiator.example.com"));
  NavigationEntryImpl entry2(
      nullptr /* instance */, kUrl2,
      Referrer(kUrl1, network::mojom::ReferrerPolicy::kDefault),
      kInitiatorOrigin, /* initiator_base_url= */ std::nullopt,
      std::u16string() /* title */, ui::PAGE_TRANSITION_LINK,
      true /* is_renderer_init */, nullptr /* blob_url_loader_factory */,
      false /* is_initial_entry */);
  host = NavigateToEntry(manager, &entry2);

  // The first RenderFrameHost will be reused only when there's no site
  // isolation between the two sites.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(host, manager->current_frame_host());
    EXPECT_TRUE(manager->speculative_frame_host());
  } else {
    EXPECT_EQ(host, manager->current_frame_host());
    EXPECT_FALSE(manager->speculative_frame_host());
  }

  // Commit.
  DidNavigateFrame(manager, host);
  EXPECT_EQ(host, manager->current_frame_host());
  ASSERT_TRUE(host);
  EXPECT_TRUE(host->GetSiteInstance()->IsGuest());

  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(host->GetSiteInstance(), first_instance);
    EXPECT_EQ("http://chromium.org/",
              host->GetSiteInstance()->GetSiteInfo().site_url().spec());
  } else {
    EXPECT_EQ(host->GetSiteInstance(), first_instance);
  }
}

namespace {

class WidgetDestructionObserver : public RenderWidgetHostObserver {
 public:
  explicit WidgetDestructionObserver(base::OnceClosure closure)
      : closure_(std::move(closure)) {}

  WidgetDestructionObserver(const WidgetDestructionObserver&) = delete;
  WidgetDestructionObserver& operator=(const WidgetDestructionObserver&) =
      delete;

  void RenderWidgetHostDestroyed(RenderWidgetHost* widget_host) override {
    std::move(closure_).Run();
  }

 private:
  base::OnceClosure closure_;
};

}  // namespace

// Test that we cancel a pending RVH if we close the tab while it's pending.
// http://crbug.com/294697.
TEST_P(RenderFrameHostManagerTest, NavigateWithEarlyClose) {
  scoped_refptr<SiteInstance> instance =
      SiteInstance::Create(browser_context());

  BeforeUnloadFiredWebContentsDelegate delegate;
  std::unique_ptr<TestWebContents> web_contents(
      TestWebContents::Create(browser_context(), instance));
  web_contents->SetDelegate(&delegate);

  RenderFrameHostManager* manager =
      web_contents->GetPrimaryFrameTree().root()->render_manager();

  // 1) The first navigation. --------------------------
  const GURL kUrl1("http://www.google.com/");
  NavigationEntryImpl entry1(
      nullptr /* instance */, kUrl1, Referrer(),
      /* initiator_origin= */ std::nullopt,
      /* initiator_base_url= */ std::nullopt, std::u16string() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);
  RenderFrameHostImpl* host = NavigateToEntry(manager, &entry1);

  // The RenderFrameHost created in Init will be reused.
  EXPECT_EQ(host, manager->current_frame_host());
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // Commit.
  DidNavigateFrame(manager, host);

  // Commit to SiteInstance should be delayed until RenderFrame commits.
  EXPECT_EQ(host, manager->current_frame_host());
  EXPECT_FALSE(host->GetSiteInstance()->HasSite());
  host->GetSiteInstance()->SetSite(UrlInfo::CreateForTesting(kUrl1));

  // 2) Cross-site navigate to next site. -------------------------
  const GURL kUrl2("http://www.example.com");
  NavigationEntryImpl entry2(
      nullptr /* instance */, kUrl2, Referrer(),
      /* initiator_origin= */ std::nullopt,
      /* initiator_base_url= */ std::nullopt, std::u16string() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);
  RenderFrameHostImpl* host2 = NavigateToEntry(manager, &entry2);

  // A new RenderFrameHost should be created.
  ASSERT_EQ(host2, GetPendingFrameHost(manager));
  EXPECT_NE(host2, host);

  EXPECT_EQ(host, manager->current_frame_host());
  EXPECT_EQ(host2, GetPendingFrameHost(manager));

  // 3) Close the tab. -------------------------
  base::RunLoop run_loop;
  WidgetDestructionObserver observer(run_loop.QuitClosure());
  host2->render_view_host()->GetWidget()->AddObserver(&observer);

  manager->BeforeUnloadCompleted(/*proceed=*/true);

  run_loop.Run();
  EXPECT_FALSE(GetPendingFrameHost(manager));
  EXPECT_EQ(host, manager->current_frame_host());
}

TEST_P(RenderFrameHostManagerTest, CloseWithPendingWhileUnresponsive) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2 = isolated_cross_site_url();

  CloseWebContentsDelegate close_delegate;
  contents()->SetDelegate(&close_delegate);

  // Navigate to the first page.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = contents()->GetPrimaryMainFrame();

  // Start to close the tab, but assume it's unresponsive.
  rfh1->ClosePage(RenderFrameHostImpl::ClosePageSource::kBrowser);
  EXPECT_EQ(rfh1->page_close_state_,
            RenderFrameHostImpl::PageCloseState::kRunningUnloadHandlers);

  // Start a navigation to a new site.
  controller().LoadURL(kUrl2, Referrer(), ui::PAGE_TRANSITION_LINK,
                       std::string());
  rfh1->PrepareForCommit();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());

  // Simulate the unresponsiveness timer.  The tab should close.
  rfh1->ClosePageTimeout(RenderFrameHostImpl::ClosePageSource::kBrowser);
  EXPECT_TRUE(close_delegate.is_closed());
}

TEST_P(RenderFrameHostManagerTest,
       CloseWithPendingWhileUnresponsiveWithDevTools) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2 = isolated_cross_site_url();

  CloseWebContentsDelegate close_delegate;
  contents()->SetDelegate(&close_delegate);

  // Attach a DevTools session.
  auto agent = DevToolsAgentHost::GetOrCreateFor(contents());
  TestDevToolsClientHost client_host;
  client_host.InspectAgentHost(agent.get());
  // Test that the connection is established.
  EXPECT_TRUE(agent->IsAttached());

  // Navigate to the first page.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = contents()->GetPrimaryMainFrame();

  // Start to close the tab, but assume it's unresponsive.
  rfh1->ClosePage(RenderFrameHostImpl::ClosePageSource::kBrowser);
  EXPECT_EQ(rfh1->page_close_state_,
            RenderFrameHostImpl::PageCloseState::kRunningUnloadHandlers);

  // Start a navigation to a new site.
  controller().LoadURL(kUrl2, Referrer(), ui::PAGE_TRANSITION_LINK,
                       std::string());
  rfh1->PrepareForCommit();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());

  // Simulate the unresponsiveness timer.  The tab should close.
  rfh1->ClosePageTimeout(RenderFrameHostImpl::ClosePageSource::kBrowser);
  EXPECT_TRUE(close_delegate.is_closed());

  // Cleanup the DevTools session.
  client_host.Close();
}

// Tests that the RenderFrameHost is properly deleted when the
// mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame is received.
// (mojo::FrameNavigationControl::Unload and the corresponding
// mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame always occur after
// commit.) Also tests that an early
// mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame is properly ignored.
TEST_P(RenderFrameHostManagerTest, DeleteFrameAfterUnloadACK) {
  // When a page enters the BackForwardCache, the RenderFrameHost is not
  // deleted.  Similarly, no
  // mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame message is sent.
  contents()->GetController().GetBackForwardCache().DisableForTesting(
      BackForwardCache::TEST_REQUIRES_NO_CACHING);
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to the first page.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = contents()->GetPrimaryMainFrame();
  RenderFrameDeletedObserver rfh_deleted_observer(rfh1);
  EXPECT_TRUE(rfh1->IsActive());

  // Navigate to new site, simulating onbeforeunload approval.
  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(kUrl2, contents());
  navigation->ReadyToCommit();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_TRUE(rfh1->IsActive());
  TestRenderFrameHost* rfh2 = contents()->GetSpeculativePrimaryMainFrame();

  // Simulate the unload ack, unexpectedly early (before commit).  It should
  // have no effect.
  rfh1->SimulateUnloadACK();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_TRUE(rfh1->IsActive());

  // The new page commits.
  navigation->set_drop_unload_ack(true);
  navigation->Commit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(rfh2, contents()->GetPrimaryMainFrame());
  EXPECT_TRUE(contents()->GetSpeculativePrimaryMainFrame() == nullptr);
  EXPECT_TRUE(rfh2->IsActive());
  EXPECT_TRUE(rfh1->IsPendingDeletion());

  // Simulate the unload ack.
  rfh1->SimulateUnloadACK();

  // rfh1 should have been deleted.
  EXPECT_TRUE(rfh_deleted_observer.deleted());
  rfh1 = nullptr;
}

// Tests that the RenderFrameHost is properly unloaded when the
// mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame is received.
// (mojo::FrameNavigationControl::Unload and the corresponding
// mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame always occur after
// commit.)
TEST_P(RenderFrameHostManagerTest, UnloadFrameAfterUnloadACK) {
  // When a page enters the BackForwardCache, the RenderFrameHost is not
  // deleted.  Similarly, no
  // mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame message is sent.
  contents()->GetController().GetBackForwardCache().DisableForTesting(
      BackForwardCache::TEST_REQUIRES_NO_CACHING);
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to the first page.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = contents()->GetPrimaryMainFrame();
  RenderFrameDeletedObserver rfh_deleted_observer(rfh1);
  EXPECT_TRUE(rfh1->IsActive());

  // Increment the number of active frames in SiteInstanceGroup so that rfh1 is
  // not deleted on unload.
  rfh1->GetSiteInstance()->group()->IncrementActiveFrameCount();

  // Navigate to new site, simulating onbeforeunload approval.
  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(kUrl2, contents());
  navigation->ReadyToCommit();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_TRUE(rfh1->IsActive());
  TestRenderFrameHost* rfh2 = contents()->GetSpeculativePrimaryMainFrame();

  // The new page commits.
  navigation->set_drop_unload_ack(true);
  navigation->Commit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(rfh2, contents()->GetPrimaryMainFrame());
  EXPECT_TRUE(contents()->GetSpeculativePrimaryMainFrame() == nullptr);
  EXPECT_TRUE(rfh1->IsPendingDeletion());
  EXPECT_TRUE(rfh2->IsActive());

  // Simulate the unload ack.
  rfh1->OnUnloaded();

  // rfh1 should be deleted.
  EXPECT_TRUE(rfh_deleted_observer.deleted());
}

// Test that a RenderFrameHost is properly deleted if a navigation in the new
// renderer commits before sending the mojo::FrameNavigationControl::Unload
// message to the old renderer. This simulates a cross-site navigation to a
// synchronously committing URL (e.g., a data URL) and ensures it works
// properly.
TEST_P(RenderFrameHostManagerTest, CommitNewNavigationBeforeSendingUnload) {
  // When a page enters the BackForwardCache, the RenderFrameHost is not
  // deleted.  Similarly, no
  // mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame message is sent.
  contents()->GetController().GetBackForwardCache().DisableForTesting(
      BackForwardCache::TEST_REQUIRES_NO_CACHING);
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to the first page.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = contents()->GetPrimaryMainFrame();
  RenderFrameDeletedObserver rfh_deleted_observer(rfh1);
  EXPECT_TRUE(rfh1->IsActive());

  // Increment the number of active frames in rfh1's SiteInstanceGroup so that
  // the SiteInstanceGroup is not deleted on unload.
  scoped_refptr<SiteInstanceGroup> site_instance_group =
      rfh1->GetSiteInstance()->group();
  site_instance_group->IncrementActiveFrameCount();

  // Navigate to new site, simulating onbeforeunload approval.
  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(kUrl2, contents());
  navigation->ReadyToCommit();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_TRUE(rfh1->IsActive());
  TestRenderFrameHost* rfh2 = contents()->GetSpeculativePrimaryMainFrame();

  // The new page commits.
  navigation->set_drop_unload_ack(true);
  navigation->Commit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(rfh2, contents()->GetPrimaryMainFrame());
  EXPECT_TRUE(contents()->GetSpeculativePrimaryMainFrame() == nullptr);
  EXPECT_TRUE(rfh1->IsPendingDeletion());
  EXPECT_TRUE(rfh2->IsActive());

  // Simulate the unload ack.
  rfh1->OnUnloaded();

  // rfh1 should be deleted.
  EXPECT_TRUE(rfh_deleted_observer.deleted());
  EXPECT_TRUE(contents()
                  ->GetPrimaryFrameTree()
                  .root()
                  ->current_frame_host()
                  ->browsing_context_state()
                  ->GetRenderFrameProxyHost(site_instance_group.get()));
}

// Test that a RenderFrameHost is properly deleted when a cross-site navigation
// is cancelled.
TEST_P(RenderFrameHostManagerTest, CancelPendingProperlyDeletesOrSwaps) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2 = isolated_cross_site_url();
  RenderFrameHostImpl* pending_rfh = nullptr;

  // Navigate to the first page.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();
  EXPECT_TRUE(rfh1->IsActive());

  rfh1->SuddenTerminationDisablerChanged(
      true, blink::mojom::SuddenTerminationDisablerType::kBeforeUnloadHandler);

  // Navigate to a new site, starting a cross-site navigation.
  controller().LoadURL(kUrl2, Referrer(), ui::PAGE_TRANSITION_LINK,
                       std::string());
  {
    pending_rfh = contents()->GetSpeculativePrimaryMainFrame();
    RenderFrameDeletedObserver rfh_deleted_observer(pending_rfh);

    // Cancel the navigation by simulating a declined beforeunload dialog.
    contents()->GetPrimaryMainFrame()->SimulateBeforeUnloadCompleted(false);
    EXPECT_FALSE(contents()->CrossProcessNavigationPending());

    // Since the pending RFH is the only one for the new SiteInstance, it should
    // be deleted.
    EXPECT_TRUE(rfh_deleted_observer.deleted());
  }

  // Start another cross-site navigation.
  controller().LoadURL(kUrl2, Referrer(), ui::PAGE_TRANSITION_LINK,
                       std::string());
  {
    pending_rfh = contents()->GetSpeculativePrimaryMainFrame();
    RenderFrameDeletedObserver rfh_deleted_observer(pending_rfh);

    // Increment the number of active frames in the new SiteInstanceGroup, which
    // will cause the pending RFH to be deleted and a RenderFrameProxyHost to be
    // created.
    scoped_refptr<SiteInstanceGroup> site_instance_group =
        pending_rfh->GetSiteInstance()->group();
    site_instance_group->IncrementActiveFrameCount();

    contents()->GetPrimaryMainFrame()->SimulateBeforeUnloadCompleted(false);
    EXPECT_FALSE(contents()->CrossProcessNavigationPending());

    EXPECT_TRUE(rfh_deleted_observer.deleted());
    EXPECT_TRUE(contents()
                    ->GetPrimaryFrameTree()
                    .root()
                    ->current_frame_host()
                    ->browsing_context_state()
                    ->GetRenderFrameProxyHost(site_instance_group.get()));
  }
}

class RenderFrameHostManagerTestWithSiteIsolation
    : public RenderFrameHostManagerTest {
 public:
  RenderFrameHostManagerTestWithSiteIsolation() {
    IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  }
};

// Test that a pending RenderFrameHost in a non-root frame tree node is properly
// deleted when the node is detached. Motivated by http://crbug.com/441357 and
// http://crbug.com/444955.
TEST_P(RenderFrameHostManagerTestWithSiteIsolation, DetachPendingChild) {
  const GURL kUrlA("http://www.google.com/");
  const GURL kUrlB("http://webkit.org/");

  constexpr auto kOwnerType = blink::FrameOwnerElementType::kIframe;
  // Create a page with two child frames.
  contents()->NavigateAndCommit(kUrlA);
  contents()->GetPrimaryMainFrame()->OnCreateChildFrame(
      contents()->GetPrimaryMainFrame()->GetProcess()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, "frame_name", "uniqueName1",
      false, blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(), kOwnerType, ukm::kInvalidSourceId);
  contents()->GetPrimaryMainFrame()->OnCreateChildFrame(
      contents()->GetPrimaryMainFrame()->GetProcess()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, "frame_name", "uniqueName2",
      false, blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(), kOwnerType, ukm::kInvalidSourceId);
  RenderFrameHostManager* root_manager =
      contents()->GetPrimaryFrameTree().root()->render_manager();
  RenderFrameHostManager* iframe1 =
      contents()->GetPrimaryFrameTree().root()->child_at(0)->render_manager();
  RenderFrameHostManager* iframe2 =
      contents()->GetPrimaryFrameTree().root()->child_at(1)->render_manager();

  // 1) The first navigation.
  NavigationEntryImpl entryA(
      nullptr /* instance */, kUrlA, Referrer(),
      /* initiator_origin= */ std::nullopt,
      /* initiator_base_url= */ std::nullopt, std::u16string() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);
  RenderFrameHostImpl* host1 = NavigateToEntry(iframe1, &entryA);

  // The RenderFrameHost created in Init will be reused.
  EXPECT_TRUE(host1 == iframe1->current_frame_host());
  EXPECT_FALSE(GetPendingFrameHost(iframe1));

  // Commit.
  DidNavigateFrame(iframe1, host1);
  // Commit to SiteInstance should be delayed until RenderFrame commit.
  EXPECT_TRUE(host1 == iframe1->current_frame_host());
  ASSERT_TRUE(host1);
  EXPECT_TRUE(host1->GetSiteInstance()->HasSite());

  // 2) Cross-site navigate both frames to next site.
  NavigationEntryImpl entryB(
      nullptr /* instance */, kUrlB,
      Referrer(kUrlA, network::mojom::ReferrerPolicy::kDefault),
      /* initiator_origin= */ std::nullopt,
      /* initiator_base_url= */ std::nullopt, std::u16string() /* title */,
      ui::PAGE_TRANSITION_LINK, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);
  host1 = NavigateToEntry(iframe1, &entryB);
  RenderFrameHostImpl* host2 = NavigateToEntry(iframe2, &entryB);

  // A new, pending RenderFrameHost should be created in each FrameTreeNode.
  EXPECT_TRUE(GetPendingFrameHost(iframe1));
  EXPECT_TRUE(GetPendingFrameHost(iframe2));
  EXPECT_EQ(host1, GetPendingFrameHost(iframe1));
  EXPECT_EQ(host2, GetPendingFrameHost(iframe2));
  EXPECT_EQ(GetPendingFrameHost(iframe1)->lifecycle_state(),
            RenderFrameHostImpl::LifecycleStateImpl::kSpeculative);
  EXPECT_EQ(GetPendingFrameHost(iframe2)->lifecycle_state(),
            RenderFrameHostImpl::LifecycleStateImpl::kSpeculative);
  EXPECT_NE(GetPendingFrameHost(iframe1), GetPendingFrameHost(iframe2));
  EXPECT_EQ(GetPendingFrameHost(iframe1)->GetSiteInstance(),
            GetPendingFrameHost(iframe2)->GetSiteInstance());
  EXPECT_NE(iframe1->current_frame_host(), GetPendingFrameHost(iframe1));
  EXPECT_NE(iframe2->current_frame_host(), GetPendingFrameHost(iframe2));
  EXPECT_FALSE(contents()->CrossProcessNavigationPending())
      << "There should be no top-level pending navigation.";

  RenderFrameDeletedObserver delete_watcher1(GetPendingFrameHost(iframe1));
  RenderFrameDeletedObserver delete_watcher2(GetPendingFrameHost(iframe2));
  EXPECT_FALSE(delete_watcher1.deleted());
  EXPECT_FALSE(delete_watcher2.deleted());

  // Keep the SiteInstance alive for testing.
  scoped_refptr<SiteInstanceImpl> site_instance =
      GetPendingFrameHost(iframe1)->GetSiteInstance();
  EXPECT_TRUE(site_instance->HasSite());
  EXPECT_NE(site_instance, contents()->GetSiteInstance());
  EXPECT_EQ(2U, site_instance->group()->active_frame_count());

  // Proxies should exist.
  EXPECT_NE(nullptr, root_manager->current_frame_host()
                         ->browsing_context_state()
                         ->GetRenderFrameProxyHost(site_instance->group()));
  EXPECT_NE(nullptr, iframe1->current_frame_host()
                         ->browsing_context_state()
                         ->GetRenderFrameProxyHost(site_instance->group()));
  EXPECT_NE(nullptr, iframe2->current_frame_host()
                         ->browsing_context_state()
                         ->GetRenderFrameProxyHost(site_instance->group()));

  // Detach the first child FrameTreeNode. This should kill the pending host but
  // not yet destroy proxies in |site_instance| since the other child remains.
  iframe1->current_frame_host()->Detach();
  iframe1 = nullptr;  // Was just destroyed.

  EXPECT_TRUE(delete_watcher1.deleted());
  EXPECT_FALSE(delete_watcher2.deleted());
  EXPECT_EQ(1U, site_instance->group()->active_frame_count());

  // Proxies should still exist.
  EXPECT_NE(nullptr, root_manager->current_frame_host()
                         ->browsing_context_state()
                         ->GetRenderFrameProxyHost(site_instance->group()));
  EXPECT_NE(nullptr, iframe2->current_frame_host()
                         ->browsing_context_state()
                         ->GetRenderFrameProxyHost(site_instance->group()));

  // Detach the second child FrameTreeNode. This should trigger cleanup of
  // RenderFrameProxyHosts in |site_instance|.
  iframe2->current_frame_host()->Detach();
  iframe2 = nullptr;  // Was just destroyed.

  EXPECT_TRUE(delete_watcher1.deleted());
  EXPECT_TRUE(delete_watcher2.deleted());

  // |site_instance| should no longer have a group, which means there are no
  // active frames left, or any proxies for its group.
  EXPECT_FALSE(site_instance->group());
  EXPECT_TRUE(site_instance->HasOneRef())
      << "This SiteInstance should be destroyable now.";
}

#if BUILDFLAG(IS_ANDROID)
// TODO(lukasza): https://crbug.com/1067432: Calling Compositor::Initialize()
// DCHECKs flakily and without such call the test below consistently fails on
// Android (DCHECKing about parent_view->GetFrameSinkId().is_valid() in
// RenderWidgetHostViewChildFrame::SetFrameConnectorDelegate).
#define MAYBE_TwoTabsCrashOneReloadsOneLeaves \
  DISABLED_TwoTabsCrashOneReloadsOneLeaves
#else
#define MAYBE_TwoTabsCrashOneReloadsOneLeaves TwoTabsCrashOneReloadsOneLeaves
#endif
// Two tabs in the same process crash. The first tab is reloaded, and the second
// tab navigates away without reloading. The second tab's navigation shouldn't
// mess with the first tab's content. Motivated by http://crbug.com/473714.
TEST_P(RenderFrameHostManagerTestWithSiteIsolation,
       MAYBE_TwoTabsCrashOneReloadsOneLeaves) {
#if BUILDFLAG(IS_ANDROID)
  // TODO(lukasza): https://crbug.com/1067432: This call might DCHECK flakily
  // about !CompositorImpl::IsInitialized()..
  Compositor::Initialize();
#endif

  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://webkit.org/");
  const GURL kUrl3("http://whatwg.org/");

  // |contents1| and |contents2| navigate to the same page and then crash.
  TestWebContents* contents1 = contents();
  std::unique_ptr<TestWebContents> contents2(
      TestWebContents::Create(browser_context(), contents1->GetSiteInstance()));
  contents1->NavigateAndCommit(kUrl1);
  contents2->NavigateAndCommit(kUrl1);
  MockRenderProcessHost* rph = contents1->GetPrimaryMainFrame()->GetProcess();
  EXPECT_EQ(rph, contents2->GetPrimaryMainFrame()->GetProcess());
  EXPECT_TRUE(contents1->GetPrimaryMainFrame()->GetView());
  EXPECT_TRUE(contents2->GetPrimaryMainFrame()->GetView());

  rph->SimulateCrash();
  EXPECT_FALSE(contents1->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_FALSE(contents2->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_EQ(contents1->GetSiteInstance(), contents2->GetSiteInstance());
  EXPECT_FALSE(contents1->GetPrimaryMainFrame()->GetView());
  EXPECT_FALSE(contents2->GetPrimaryMainFrame()->GetView());

  // Reload |contents1|.
  contents1->NavigateAndCommit(kUrl1);
  EXPECT_TRUE(contents1->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_FALSE(contents2->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_EQ(contents1->GetSiteInstance(), contents2->GetSiteInstance());
  EXPECT_TRUE(contents1->GetPrimaryMainFrame()->GetView());
  EXPECT_FALSE(contents2->GetPrimaryMainFrame()->GetView());

  // |contents1| creates an out of process iframe.
  contents1->GetPrimaryMainFrame()->OnCreateChildFrame(
      contents1->GetPrimaryMainFrame()->GetProcess()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, "frame_name", "uniqueName1",
      false, blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(),
      blink::FrameOwnerElementType::kIframe, ukm::kInvalidSourceId);
  RenderFrameHostManager* iframe =
      contents()->GetPrimaryFrameTree().root()->child_at(0)->render_manager();
  NavigationEntryImpl entry(
      nullptr /* instance */, kUrl2,
      Referrer(kUrl1, network::mojom::ReferrerPolicy::kDefault),
      /* initiator_origin= */ std::nullopt,
      /* initiator_base_url= */ std::nullopt, std::u16string() /* title */,
      ui::PAGE_TRANSITION_LINK, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);
  RenderFrameHostImpl* cross_site = NavigateToEntry(iframe, &entry);
  DidNavigateFrame(iframe, cross_site);

  // A proxy to the iframe should now exist in the SiteInstanceGroup of the main
  // frames.
  EXPECT_NE(cross_site->GetSiteInstance(), contents1->GetSiteInstance());
  EXPECT_NE(nullptr, iframe->current_frame_host()
                         ->browsing_context_state()
                         ->GetRenderFrameProxyHost(
                             contents1->GetSiteInstance()->group()));
  EXPECT_NE(nullptr, iframe->current_frame_host()
                         ->browsing_context_state()
                         ->GetRenderFrameProxyHost(
                             contents2->GetSiteInstance()->group()));

  // Navigate |contents2| away from the sad tab (and thus away from the
  // SiteInstance of |contents1|). This should not destroy the proxies needed by
  // |contents1| -- that was http://crbug.com/473714.
  EXPECT_FALSE(contents2->GetPrimaryMainFrame()->IsRenderFrameLive());
  contents2->NavigateAndCommit(kUrl3);
  EXPECT_TRUE(contents2->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_NE(nullptr, iframe->current_frame_host()
                         ->browsing_context_state()
                         ->GetRenderFrameProxyHost(
                             contents1->GetSiteInstance()->group()));
  EXPECT_EQ(nullptr, iframe->current_frame_host()
                         ->browsing_context_state()
                         ->GetRenderFrameProxyHost(
                             contents2->GetSiteInstance()->group()));
}

// Tests two WebContents from the same origin, where one is first navigated to
// a different origin. This different origin experiences a Renderer crash.
// However we then navigate that WebContents back to the old origin, which still
// has an active Renderer.
//
// This test confirms that for this return navigation that we identified that
// there is no FallbackSurface for the RenderWidgetHostView to display during
// the navigation. (https://crbug.com/1258363)
TEST_P(RenderFrameHostManagerTestWithSiteIsolation,
       TwoTabsOneNavigatesAndCrashesThenNavigatesBack) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://webkit.org/");

  // `contents1` and `contents2` navigate to the same page.
  TestWebContents* contents1 = contents();
  std::unique_ptr<TestWebContents> contents2(
      TestWebContents::Create(browser_context(), contents1->GetSiteInstance()));
  contents1->NavigateAndCommit(kUrl1);
  contents2->NavigateAndCommit(kUrl1);
  MockRenderProcessHost* rph = contents1->GetPrimaryMainFrame()->GetProcess();
  EXPECT_EQ(rph, contents2->GetPrimaryMainFrame()->GetProcess());
  EXPECT_TRUE(contents1->GetPrimaryMainFrame()->GetView());
  EXPECT_TRUE(contents2->GetPrimaryMainFrame()->GetView());
  EXPECT_TRUE(contents1->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_TRUE(contents2->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_EQ(contents1->GetSiteInstance(), contents2->GetSiteInstance());
  TestRenderWidgetHostView* initial_view =
      static_cast<TestRenderWidgetHostView*>(
          contents2->GetPrimaryMainFrame()->GetView());

  // Navigate `content2` to a different page. This navigation should have a
  // valid FallbackSurface for the RenderWidgetHostView to display.
  contents2->NavigateAndCommit(kUrl2);
  TestRenderWidgetHostView* post_nav_view =
      static_cast<TestRenderWidgetHostView*>(
          contents2->GetPrimaryMainFrame()->GetView());
  // Since this is a different origin we should also be using a different
  // RenderWidgetHostView.
  EXPECT_NE(initial_view, post_nav_view);
  EXPECT_FALSE(
      post_nav_view->clear_fallback_surface_for_commit_pending_called());
  // Since this is a cross-origin navigation, paint holding would not be
  // enabled without user activation.
  EXPECT_FALSE(post_nav_view->take_fallback_content_from_called());

  EXPECT_TRUE(contents1->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_TRUE(contents2->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_NE(contents1->GetSiteInstance(), contents2->GetSiteInstance());
  EXPECT_TRUE(contents1->GetPrimaryMainFrame()->GetView());
  EXPECT_TRUE(contents2->GetPrimaryMainFrame()->GetView());

  // Crash the Renderer of the tab that navigated.
  MockRenderProcessHost* rph2 = contents2->GetPrimaryMainFrame()->GetProcess();
  EXPECT_NE(rph, rph2);
  rph2->SimulateCrash();
  EXPECT_TRUE(contents1->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_FALSE(contents2->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_NE(contents1->GetSiteInstance(), contents2->GetSiteInstance());
  EXPECT_TRUE(contents1->GetPrimaryMainFrame()->GetView());
  EXPECT_FALSE(contents2->GetPrimaryMainFrame()->GetView());

  // Navigate `contents2` back to previous host, which still has an active
  // Renderer. This should notify the RenderWidgetHostView that there is no
  // new FallbackSurface to take, and that it should update it's currently
  // cached one.
  contents2->NavigateAndCommit(kUrl1);
  TestRenderWidgetHostView* return_nav_view =
      static_cast<TestRenderWidgetHostView*>(
          contents2->GetPrimaryMainFrame()->GetView());
  // We should be reusing the original RenderWidgetHostView that `contents2`
  // used before the navigation to `kUrl2`
  EXPECT_EQ(initial_view, return_nav_view);
  EXPECT_TRUE(
      return_nav_view->clear_fallback_surface_for_commit_pending_called());
  EXPECT_FALSE(return_nav_view->take_fallback_content_from_called());
  return_nav_view->ClearFallbackSurfaceCalled();

  EXPECT_TRUE(contents1->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_TRUE(contents2->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_EQ(contents1->GetSiteInstance(), contents2->GetSiteInstance());
  EXPECT_TRUE(contents1->GetPrimaryMainFrame()->GetView());
  EXPECT_TRUE(contents2->GetPrimaryMainFrame()->GetView());
  // We should also be back to sharing the same RenderProcessHost.
  MockRenderProcessHost* rph3 = contents2->GetPrimaryMainFrame()->GetProcess();
  EXPECT_NE(rph2, rph3);
  EXPECT_EQ(rph, rph3);
}

// Ensure that we don't grant WebUI bindings to a pending RenderViewHost when
// creating proxies for a non-WebUI subframe navigation.  This was possible due
// to the InitRenderView call from CreateRenderFrameProxy.
// See https://crbug.com/536145.
TEST_P(RenderFrameHostManagerTestWithSiteIsolation,
       DontGrantPendingWebUIToSubframe) {
  // Make sure the initial process is live so that the pending WebUI navigation
  // does not commit immediately.  Give the page a subframe as well.
  const GURL kUrl1("http://foo.com");
  RenderFrameHostImpl* main_rfh = contents()->GetPrimaryMainFrame();
  NavigateAndCommit(kUrl1);
  EXPECT_TRUE(main_rfh->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(main_rfh->IsRenderFrameLive());
  main_rfh->OnCreateChildFrame(
      main_rfh->GetProcess()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName1",
      false, blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(),
      blink::FrameOwnerElementType::kIframe, ukm::kInvalidSourceId);
  RenderFrameHostManager* subframe_rfhm =
      contents()->GetPrimaryFrameTree().root()->child_at(0)->render_manager();

  // Start a pending WebUI navigation in the main frame and verify that the
  // pending RVH has bindings.
  const GURL kWebUIUrl(GetWebUIURL("foo"));
  NavigationEntryImpl webui_entry(
      nullptr /* instance */, kWebUIUrl, Referrer(),
      /* initiator_origin= */ std::nullopt,
      /* initiator_base_url= */ std::nullopt, std::u16string() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);
  RenderFrameHostManager* main_rfhm =
      contents()->GetPrimaryFrameTree().root()->render_manager();
  RenderFrameHostImpl* webui_rfh = NavigateToEntry(main_rfhm, &webui_entry);
  EXPECT_EQ(webui_rfh, GetPendingFrameHost(main_rfhm));
  EXPECT_TRUE(webui_rfh->GetEnabledBindings().Has(BindingsPolicyValue::kWebUi));

  // Before it commits, do a cross-process navigation in a subframe.  This
  // should not grant WebUI bindings to the subframe's RVH.
  const GURL kSubframeUrl("http://bar.com");
  NavigationEntryImpl subframe_entry(
      nullptr /* instance */, kSubframeUrl, Referrer(),
      /* initiator_origin= */ std::nullopt,
      /* initiator_base_url= */ std::nullopt, std::u16string() /* title */,
      ui::PAGE_TRANSITION_LINK, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);
  RenderFrameHostImpl* bar_rfh =
      NavigateToEntry(subframe_rfhm, &subframe_entry);
  EXPECT_FALSE(bar_rfh->GetEnabledBindings().Has(BindingsPolicyValue::kWebUi));
}

// This class intercepts RenderFrameProxyHost creations, and overrides their
// respective blink::mojom::RemoteFrame instances, so that it can watch the
// updates of opener frames.
class UpdateOpenerProxyObserver : public RenderFrameProxyHost::TestObserver {
 public:
  UpdateOpenerProxyObserver() {
    RenderFrameProxyHost::SetObserverForTesting(this);
  }
  ~UpdateOpenerProxyObserver() override {
    RenderFrameProxyHost::SetObserverForTesting(nullptr);
  }
  std::optional<blink::FrameToken> OpenerFrameToken(
      RenderFrameProxyHost* proxy) {
    return remote_frames_[proxy]->opener_frame_token();
  }

 private:
  class Remote : public content::FakeRemoteFrame {
   public:
    explicit Remote(RenderFrameProxyHost* proxy) {
      Init(proxy->BindRemoteFrameReceiverForTesting());
    }
    void UpdateOpener(
        const std::optional<blink::FrameToken>& frame_token) override {
      frame_token_ = frame_token;
    }
    std::optional<blink::FrameToken> opener_frame_token() {
      return frame_token_;
    }

   private:
    std::optional<blink::FrameToken> frame_token_;
  };

  void OnRemoteFrameBound(RenderFrameProxyHost* proxy_host) override {
    remote_frames_[proxy_host] = std::make_unique<Remote>(proxy_host);
  }

  std::map<RenderFrameProxyHost*, std::unique_ptr<Remote>> remote_frames_;
};

// Test that opener proxies are created properly with a cycle on the opener
// chain.
TEST_P(RenderFrameHostManagerTest, CreateOpenerProxiesWithCycleOnOpenerChain) {
  UpdateOpenerProxyObserver proxy_observers;

  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2 = isolated_cross_site_url();

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();
  scoped_refptr<SiteInstanceImpl> site_instance1 = rfh1->GetSiteInstance();
  EXPECT_EQ(AreDefaultSiteInstancesEnabled(),
            site_instance1->IsDefaultSiteInstance());

  // Create 2 new tabs and construct the opener chain as follows:
  //
  //     tab2 <--- tab1 <---- contents()
  //        |       ^
  //        +-------+
  //
  std::unique_ptr<TestWebContents> tab1(
      TestWebContents::Create(browser_context(), site_instance1.get()));
  RenderFrameHostManager* tab1_manager =
      tab1->GetPrimaryFrameTree().root()->render_manager();
  std::unique_ptr<TestWebContents> tab2(
      TestWebContents::Create(browser_context(), site_instance1.get()));
  RenderFrameHostManager* tab2_manager =
      tab2->GetPrimaryFrameTree().root()->render_manager();

  contents()->SetOpener(tab1.get());
  tab1->SetOpener(tab2.get());
  tab2->SetOpener(tab1.get());

  // Navigate main window to a cross-site URL.  This will call
  // CreateOpenerProxies() to create proxies for the two opener tabs in the new
  // SiteInstanceGroup.
  contents()->NavigateAndCommit(kUrl2);
  TestRenderFrameHost* rfh2 = main_test_rfh();
  EXPECT_NE(site_instance1, rfh2->GetSiteInstance());
  EXPECT_NE(site_instance1->group(), rfh2->GetSiteInstance()->group());

  // Check that each tab now has a proxy in the new SiteInstanceGroup.
  RenderFrameProxyHost* tab1_proxy =
      tab1_manager->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(rfh2->GetSiteInstance()->group());
  EXPECT_TRUE(tab1_proxy);
  RenderFrameProxyHost* tab2_proxy =
      tab2_manager->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(rfh2->GetSiteInstance()->group());
  EXPECT_TRUE(tab2_proxy);

  // Verify that the proxies' openers point to each other.
  auto tab1_opener_frame_token =
      tab1_manager->GetOpenerFrameToken(rfh2->GetSiteInstance()->group());
  auto tab2_opener_frame_token =
      tab2_manager->GetOpenerFrameToken(rfh2->GetSiteInstance()->group());
  EXPECT_EQ(*tab1_opener_frame_token, tab2_proxy->GetFrameToken());
  EXPECT_EQ(*tab2_opener_frame_token, tab1_proxy->GetFrameToken());

  // Setting tab2_proxy's opener required an extra IPC message to be set, since
  // the opener's frame token wasn't available when tab2_proxy was created.
  // Verify that this IPC was sent and that it passed correct frame token.
  base::RunLoop().RunUntilIdle();
  DCHECK(proxy_observers.OpenerFrameToken(tab2_proxy) ==
         tab2_manager->GetOpenerFrameToken(rfh2->GetSiteInstance()->group()));
}

// Test that opener proxies are created properly when the opener points
// to itself.
TEST_P(RenderFrameHostManagerTest, CreateOpenerProxiesWhenOpenerPointsToSelf) {
  UpdateOpenerProxyObserver proxy_observers;

  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2 = isolated_cross_site_url();

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();
  scoped_refptr<SiteInstanceImpl> site_instance1 = rfh1->GetSiteInstance();
  EXPECT_EQ(AreDefaultSiteInstancesEnabled(),
            site_instance1->IsDefaultSiteInstance());

  // Create an opener tab, and simulate that its opener points to itself.
  std::unique_ptr<TestWebContents> opener(
      TestWebContents::Create(browser_context(), site_instance1.get()));
  RenderFrameHostManager* opener_manager =
      opener->GetPrimaryFrameTree().root()->render_manager();
  contents()->SetOpener(opener.get());
  opener->SetOpener(opener.get());

  // Navigate main window to a cross-site URL.  This will call
  // CreateOpenerProxies() to create proxies for the opener tab in the new
  // SiteInstanceGroup.
  contents()->NavigateAndCommit(kUrl2);
  TestRenderFrameHost* rfh2 = main_test_rfh();
  EXPECT_NE(site_instance1, rfh2->GetSiteInstance());
  EXPECT_NE(site_instance1->group(), rfh2->GetSiteInstance()->group());

  // Check that the opener now has a proxy in the new SiteInstanceGroup.
  RenderFrameProxyHost* opener_proxy =
      opener_manager->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(rfh2->GetSiteInstance()->group());
  EXPECT_TRUE(opener_proxy);

  // Verify that the proxy's opener points to itself.
  auto opener_frame_token =
      opener_manager->GetOpenerFrameToken(rfh2->GetSiteInstance()->group());
  EXPECT_EQ(*opener_frame_token, opener_proxy->GetFrameToken());

  // Setting the opener in opener_proxy required an extra IPC message, since
  // the opener's frame_token wasn't available when opener_proxy was created.
  // Verify that this IPC was sent and that it passed correct frame token.
  base::RunLoop().RunUntilIdle();
  DCHECK(proxy_observers.OpenerFrameToken(opener_proxy) ==
         opener_manager->GetOpenerFrameToken(rfh2->GetSiteInstance()->group()));
}

// Build the following frame opener graph and see that it can be properly
// traversed when creating opener proxies:
//
//     +-> root4 <--+   root3 <---- root2    +--- root1
//     |     /      |     ^         /  \     |    /  \     .
//     |    42      +-----|------- 22  23 <--+   12  13
//     |     +------------+            |             | ^
//     +-------------------------------+             +-+
//
// The test starts traversing openers from root1 and expects to discover all
// four FrameTrees.  Nodes 13 (with cycle to itself) and 42 (with back link to
// root3) should be put on the list of nodes that will need their frame openers
// set separately in a second pass, since their opener routing IDs won't be
// available during the first pass of CreateOpenerProxies.
TEST_P(RenderFrameHostManagerTest, TraverseComplexOpenerChain) {
  contents()->NavigateAndCommit(GURL("http://tab1.com"));
  FrameTree* tree1 = &contents()->GetPrimaryFrameTree();
  FrameTreeNode* root1 = tree1->root();
  int process_id = root1->current_frame_host()->GetProcess()->GetID();
  constexpr auto kOwnerType = blink::FrameOwnerElementType::kIframe;
  const bool is_dummy_frame_for_inner_tree = false;
  tree1->AddFrame(
      root1->current_frame_host(), process_id, 12,
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName0",
      false, blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(), false, kOwnerType,
      is_dummy_frame_for_inner_tree);
  tree1->AddFrame(
      root1->current_frame_host(), process_id, 13,
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName1",
      false, blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(), false, kOwnerType,
      is_dummy_frame_for_inner_tree);

  std::unique_ptr<TestWebContents> tab2(
      TestWebContents::Create(browser_context(), nullptr));
  tab2->NavigateAndCommit(GURL("http://tab2.com"));
  FrameTree* tree2 = &tab2->GetPrimaryFrameTree();
  FrameTreeNode* root2 = tree2->root();
  process_id = root2->current_frame_host()->GetProcess()->GetID();
  tree2->AddFrame(
      root2->current_frame_host(), process_id, 22,
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName2",
      false, blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(), false, kOwnerType,
      is_dummy_frame_for_inner_tree);
  tree2->AddFrame(
      root2->current_frame_host(), process_id, 23,
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName3",
      false, blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(), false, kOwnerType,
      is_dummy_frame_for_inner_tree);

  std::unique_ptr<TestWebContents> tab3(
      TestWebContents::Create(browser_context(), nullptr));
  FrameTree* tree3 = &tab3->GetPrimaryFrameTree();
  FrameTreeNode* root3 = tree3->root();

  std::unique_ptr<TestWebContents> tab4(
      TestWebContents::Create(browser_context(), nullptr));
  tab4->NavigateAndCommit(GURL("http://tab4.com"));
  FrameTree* tree4 = &tab4->GetPrimaryFrameTree();
  FrameTreeNode* root4 = tree4->root();
  process_id = root4->current_frame_host()->GetProcess()->GetID();
  tree4->AddFrame(
      root4->current_frame_host(), process_id, 42,
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName4",
      false, blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(), false, kOwnerType,
      is_dummy_frame_for_inner_tree);

  root1->child_at(1)->SetOpener(root1->child_at(1));
  root1->SetOpener(root2->child_at(1));
  root2->SetOpener(root3);
  root2->child_at(0)->SetOpener(root4);
  root2->child_at(1)->SetOpener(root4);
  root4->child_at(0)->SetOpener(root3);

  std::vector<FrameTree*> opener_frame_trees;
  std::unordered_set<FrameTreeNode*> nodes_with_back_links;
  std::unordered_set<FrameTreeNode*> cross_browsing_context_group_openers;

  CollectOpenerFrameTrees(root1, /*site_instance_group=*/nullptr,
                          &opener_frame_trees, &nodes_with_back_links,
                          &cross_browsing_context_group_openers);

  EXPECT_EQ(4U, opener_frame_trees.size());
  EXPECT_EQ(tree1, opener_frame_trees[0]);
  EXPECT_EQ(tree2, opener_frame_trees[1]);
  EXPECT_EQ(tree3, opener_frame_trees[2]);
  EXPECT_EQ(tree4, opener_frame_trees[3]);

  EXPECT_EQ(2U, nodes_with_back_links.size());
  EXPECT_TRUE(nodes_with_back_links.find(root1->child_at(1)) !=
              nodes_with_back_links.end());
  EXPECT_TRUE(nodes_with_back_links.find(root4->child_at(0)) !=
              nodes_with_back_links.end());
}

// This class intercepts RenderFrameProxyHost creations, and overrides their
// respective blink::mojom::RemoteFrame instances, so that it can watch the
// start and stop loading states.
class PageFocusProxyObserver : public RenderFrameProxyHost::TestObserver {
 public:
  PageFocusProxyObserver() {
    RenderFrameProxyHost::SetObserverForTesting(this);
  }
  ~PageFocusProxyObserver() override {
    RenderFrameProxyHost::SetObserverForTesting(nullptr);
  }
  bool IsPageFocused(RenderFrameProxyHost* proxy) {
    return remote_frames_[proxy]->set_page_focus();
  }

 private:
  class Remote : public content::FakeRemoteFrame {
   public:
    explicit Remote(RenderFrameProxyHost* proxy) {
      Init(proxy->BindRemoteFrameReceiverForTesting());
    }
    void SetPageFocus(bool is_focused) override {
      set_page_focus_ = is_focused;
    }
    bool set_page_focus() { return set_page_focus_; }

   private:
    bool set_page_focus_ = false;
  };

  void OnRemoteFrameBound(RenderFrameProxyHost* proxy_host) override {
    remote_frames_[proxy_host] = std::make_unique<Remote>(proxy_host);
  }

  std::map<RenderFrameProxyHost*, std::unique_ptr<Remote>> remote_frames_;
};

// Check that when a window is focused/blurred, the message that sets
// page-level focus updates is sent to each process involved in rendering the
// current page.
//
// TODO(alexmos): Move this test to FrameTree unit tests once NavigateToEntry
// is moved to a common place.  See https://crbug.com/547275.
TEST_P(RenderFrameHostManagerTest, PageFocusPropagatesToSubframeProcesses) {
  // This test only makes sense when cross-site subframes use separate
  // processes.
  if (!AreAllSitesIsolatedForTesting())
    return;

  // Start monitoring RenderFrameProxyHost.
  PageFocusProxyObserver proxy_observer;

  const GURL kUrlA("http://a.com/");
  const GURL kUrlB("http://b.com/");
  const GURL kUrlC("http://c.com/");

  constexpr auto kOwnerType = blink::FrameOwnerElementType::kIframe;
  // Set up a page at a.com with three subframes: two for b.com and one for
  // c.com.
  contents()->NavigateAndCommit(kUrlA);
  main_test_rfh()->OnCreateChildFrame(
      main_test_rfh()->GetProcess()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, "frame1", "uniqueName1", false,
      blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(), kOwnerType, ukm::kInvalidSourceId);
  main_test_rfh()->OnCreateChildFrame(
      main_test_rfh()->GetProcess()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, "frame2", "uniqueName2", false,
      blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(), kOwnerType, ukm::kInvalidSourceId);
  main_test_rfh()->OnCreateChildFrame(
      main_test_rfh()->GetProcess()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, "frame3", "uniqueName3", false,
      blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(), kOwnerType, ukm::kInvalidSourceId);

  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  RenderFrameHostManager* child1 = root->child_at(0)->render_manager();
  RenderFrameHostManager* child2 = root->child_at(1)->render_manager();
  RenderFrameHostManager* child3 = root->child_at(2)->render_manager();

  // Navigate first two subframes to B.
  NavigationEntryImpl entryB(
      nullptr /* instance */, kUrlB,
      Referrer(kUrlA, network::mojom::ReferrerPolicy::kDefault),
      /* initiator_origin= */ std::nullopt,
      /* initiator_base_url= */ std::nullopt, std::u16string() /* title */,
      ui::PAGE_TRANSITION_LINK, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);
  TestRenderFrameHost* host1 =
      static_cast<TestRenderFrameHost*>(NavigateToEntry(child1, &entryB));

  // The main frame should have proxies for B.
  RenderFrameProxyHost* proxyB =
      root->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(host1->GetSiteInstance()->group());
  EXPECT_TRUE(proxyB);

  TestRenderFrameHost* host2 =
      static_cast<TestRenderFrameHost*>(NavigateToEntry(child2, &entryB));

  DidNavigateFrame(child1, host1);
  DidNavigateFrame(child2, host2);

  // Navigate the third subframe to C.
  NavigationEntryImpl entryC(
      nullptr /* instance */, kUrlC,
      Referrer(kUrlA, network::mojom::ReferrerPolicy::kDefault),
      /* initiator_origin= */ std::nullopt,
      /* initiator_base_url= */ std::nullopt, std::u16string() /* title */,
      ui::PAGE_TRANSITION_LINK, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);
  TestRenderFrameHost* host3 =
      static_cast<TestRenderFrameHost*>(NavigateToEntry(child3, &entryC));

  // The main frame should have proxies for C.
  RenderFrameProxyHost* proxyC =
      root->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(host3->GetSiteInstance()->group());
  EXPECT_TRUE(proxyC);

  DidNavigateFrame(child3, host3);

  // Make sure the first two subframes and the third subframe are placed in
  // distinct processes.
  EXPECT_NE(host1->GetProcess(), main_test_rfh()->GetProcess());
  EXPECT_EQ(host1->GetProcess(), host2->GetProcess());
  EXPECT_NE(host3->GetProcess(), main_test_rfh()->GetProcess());
  EXPECT_NE(host3->GetProcess(), host1->GetProcess());

  base::RunLoop().RunUntilIdle();

  // Focus the main page, and verify that the focus message was sent to all
  // processes.  The message to A should be sent through the main frame's
  // RenderViewHost, and the message to B and C should be send through proxies
  // that the main frame has for B and C.
  main_test_rfh()->GetProcess()->sink().ClearMessages();
  host1->GetProcess()->sink().ClearMessages();
  host3->GetProcess()->sink().ClearMessages();
  main_test_rfh()->GetRenderWidgetHost()->Focus();
  base::RunLoop().RunUntilIdle();
  VerifyPageFocusMessage(main_test_rfh()->GetRenderWidgetHost(), true);
  EXPECT_TRUE(proxy_observer.IsPageFocused(proxyB));
  EXPECT_TRUE(proxy_observer.IsPageFocused(proxyC));

  // Similarly, simulate focus loss on main page, and verify that the focus
  // message was sent to all processes.
  main_test_rfh()->GetProcess()->sink().ClearMessages();
  host1->GetProcess()->sink().ClearMessages();
  host3->GetProcess()->sink().ClearMessages();
  main_test_rfh()->GetRenderWidgetHost()->Blur();
  base::RunLoop().RunUntilIdle();
  VerifyPageFocusMessage(main_test_rfh()->GetRenderWidgetHost(), false);
  EXPECT_FALSE(proxy_observer.IsPageFocused(proxyB));
  EXPECT_FALSE(proxy_observer.IsPageFocused(proxyC));
}

// Check that page-level focus state is preserved across subframe navigations.
//
// TODO(alexmos): Move this test to FrameTree unit tests once NavigateToEntry
// is moved to a common place.  See https://crbug.com/547275.
TEST_P(RenderFrameHostManagerTest,
       PageFocusIsPreservedAcrossSubframeNavigations) {
  // This test only makes sense when cross-site subframes use separate
  // processes.
  if (!AreAllSitesIsolatedForTesting())
    return;

  // Start monitoring RenderFrameProxyHost.
  PageFocusProxyObserver proxy_observer;

  const GURL kUrlA("http://a.com/");
  const GURL kUrlB("http://b.com/");
  const GURL kUrlC("http://c.com/");

  constexpr auto kOwnerType = blink::FrameOwnerElementType::kIframe;
  // Set up a page at a.com with a b.com subframe.
  contents()->NavigateAndCommit(kUrlA);
  main_test_rfh()->OnCreateChildFrame(
      main_test_rfh()->GetProcess()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, "frame1", "uniqueName1", false,
      blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(), kOwnerType, ukm::kInvalidSourceId);

  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  RenderFrameHostManager* child = root->child_at(0)->render_manager();

  // Navigate subframe to B.
  NavigationEntryImpl entryB(
      nullptr /* instance */, kUrlB,
      Referrer(kUrlA, network::mojom::ReferrerPolicy::kDefault),
      /* initiator_origin= */ std::nullopt,
      /* initiator_base_url= */ std::nullopt, std::u16string() /* title */,
      ui::PAGE_TRANSITION_LINK, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);
  TestRenderFrameHost* hostB =
      static_cast<TestRenderFrameHost*>(NavigateToEntry(child, &entryB));
  DidNavigateFrame(child, hostB);

  // Ensure that the main page is focused.
  main_test_rfh()->GetView()->Focus();
  EXPECT_TRUE(main_test_rfh()->GetView()->HasFocus());
  main_test_rfh()->GetRenderWidgetHost()->SetPageFocus(true);
  EXPECT_TRUE(main_test_rfh()->GetRenderWidgetHost()->is_focused());

  // Navigate the subframe to C.
  NavigationEntryImpl entryC(
      nullptr /* instance */, kUrlC,
      Referrer(kUrlA, network::mojom::ReferrerPolicy::kDefault),
      /* initiator_origin= */ std::nullopt,
      /* initiator_base_url= */ std::nullopt, std::u16string() /* title */,
      ui::PAGE_TRANSITION_LINK, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);
  TestRenderFrameHost* hostC =
      static_cast<TestRenderFrameHost*>(NavigateToEntry(child, &entryC));

  // The main frame should now have a proxy for C.
  RenderFrameProxyHost* proxyC =
      root->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(hostC->GetSiteInstance()->group());
  EXPECT_TRUE(proxyC);

  DidNavigateFrame(child, hostC);

  base::RunLoop().RunUntilIdle();

  // Since the B->C navigation happened while the current page was focused,
  // page focus should propagate to the new subframe process.  Check that
  // process C received the proper focus message.
  EXPECT_TRUE(proxy_observer.IsPageFocused(proxyC));
}

// Checks that a restore navigation to a WebUI works.
TEST_P(RenderFrameHostManagerTest, RestoreNavigationToWebUI) {
  const GURL kInitUrl(GetWebUIURL("foo"));
  scoped_refptr<SiteInstanceImpl> initial_instance =
      SiteInstanceImpl::Create(browser_context());
  initial_instance->SetSite(UrlInfo::CreateForTesting(kInitUrl));
  std::unique_ptr<TestWebContents> web_contents(
      TestWebContents::Create(browser_context(), initial_instance));
  RenderFrameHostManager* manager =
      web_contents->GetPrimaryFrameTree().root()->render_manager();
  NavigationControllerImpl& controller = web_contents->GetController();

  // Setup a restored entry.
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  std::unique_ptr<NavigationEntry> new_entry =
      NavigationController::CreateNavigationEntry(
          kInitUrl, Referrer(), /* initiator_origin= */ std::nullopt,
          /* initiator_base_url= */ std::nullopt, ui::PAGE_TRANSITION_TYPED,
          false, std::string(), browser_context(),
          nullptr /* blob_url_loader_factory */);
  entries.push_back(std::move(new_entry));
  controller.Restore(0, RestoreType::kRestored, &entries);
  ASSERT_EQ(0u, entries.size());
  ASSERT_EQ(1, controller.GetEntryCount());

  RenderFrameHostImpl* initial_host = manager->current_frame_host();
  ASSERT_TRUE(initial_host);
  EXPECT_FALSE(initial_host->IsRenderFrameLive());
  EXPECT_FALSE(initial_host->web_ui());

  // Navigation request to an entry from a previous browsing session.
  NavigationEntryImpl entry(
      nullptr /* instance */, kInitUrl, Referrer(),
      /* initiator_origin= */ std::nullopt,
      /* initiator_base_url= */ std::nullopt, std::u16string() /* title */,
      ui::PAGE_TRANSITION_RELOAD, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);
  entry.set_restore_type(RestoreType::kRestored);
  NavigateToEntry(manager, &entry);

  // As the initial renderer was not live, the new RenderFrameHost should be
  // made immediately active at request time.
  EXPECT_FALSE(GetPendingFrameHost(manager));
  TestRenderFrameHost* current_host =
      static_cast<TestRenderFrameHost*>(manager->current_frame_host());
  ASSERT_TRUE(current_host);
  EXPECT_EQ(current_host, initial_host);
  EXPECT_TRUE(current_host->IsRenderFrameLive());
  EXPECT_TRUE(current_host->web_ui());

  // The RenderFrameHost committed.
  DidNavigateFrame(manager, current_host);
  EXPECT_EQ(current_host, manager->current_frame_host());
  EXPECT_TRUE(current_host->web_ui());
}

// Simulates two simultaneous navigations involving one WebUI where the current
// RenderFrameHost commits.
TEST_P(RenderFrameHostManagerTest, SimultaneousNavigationWithOneWebUI1) {
  if (ShouldCreateNewHostForAllFrames()) {
    // This test involves starting a navigation while another navigation is
    // committing, which might lead to deletion of a pending commit RFH, which
    // will crash when RenderDocument is enabled. Skip the test if so.
    // TODO(crbug.com/40186427): Update this test to work under
    // navigation queueing, which will prevent the deletion of the pending
    // commit RFH but still fails because this test waits for the new navigation
    // to get to the ReadyToCommit stage before finishing the commit of the
    // pending commit RFH.
    return;
  }
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(),
                                                    GetWebUIURL("foo/"));

  RenderFrameHostManager* manager =
      contents()->GetPrimaryFrameTree().root()->render_manager();
  RenderFrameHostImpl* host1 = manager->current_frame_host();
  EXPECT_TRUE(host1->IsRenderFrameLive());
  WebUIImpl* web_ui = host1->web_ui();
  EXPECT_TRUE(web_ui);

  // Starts a reload of the WebUI page.
  contents()->GetController().Reload(ReloadType::NORMAL, true);
  auto reload =
      NavigationSimulator::CreateFromPending(contents()->GetController());
  reload->ReadyToCommit();

  // It should be a same-site navigation reusing the same WebUI.
  EXPECT_EQ(web_ui, host1->web_ui());
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // Navigation request to a non-WebUI page.
  const GURL kUrl("http://google.com");
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl, contents());
  navigation->ReadyToCommit();
  RenderFrameHostImpl* host2 = GetPendingFrameHost(manager);
  ASSERT_TRUE(host2);

  // The previous navigation should still be ongoing along with the new,
  // cross-site one.
  EXPECT_FALSE(host2->web_ui());
  EXPECT_EQ(web_ui, host1->web_ui());

  EXPECT_NE(host2, host1);
  EXPECT_NE(web_ui, host2->web_ui());

  // The current RenderFrameHost commits; its WebUI should still be in place.
  reload->Commit();
  EXPECT_EQ(host1, manager->current_frame_host());
  EXPECT_EQ(web_ui, host1->web_ui());

  // Because the Navigation that committed was browser-initiated, it will not
  // have the user gesture bit set to true. This has the side-effect of not
  // deleting the speculative RenderFrameHost at commit time.
  // TODO(clamy): The speculative RenderFrameHost should be deleted at commit
  // time if a browser-initiated navigation commits.
  EXPECT_TRUE(GetPendingFrameHost(manager));
}

// Simulates two simultaneous navigations involving one WebUI where the new,
// cross-site RenderFrameHost commits.
TEST_P(RenderFrameHostManagerTest, SimultaneousNavigationWithOneWebUI2) {
  if (ShouldCreateNewHostForAllFrames()) {
    // This test involves starting a navigation while another navigation is
    // committing, which might lead to deletion of a pending commit RFH, which
    // will crash when RenderDocument is enabled. Skip the test if so.
    // TODO(crbug.com/40186427): Update this test to work under
    // navigation queueing, which will prevent the deletion of the pending
    // commit RFH but still fails because this test waits for the new navigation
    // to get to the ReadyToCommit stage before finishing the commit of the
    // pending commit RFH.
    return;
  }
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(),
                                                    GetWebUIURL("foo/"));

  RenderFrameHostManager* manager =
      contents()->GetPrimaryFrameTree().root()->render_manager();
  RenderFrameHostImpl* host1 = manager->current_frame_host();
  EXPECT_TRUE(host1->IsRenderFrameLive());
  WebUIImpl* web_ui = host1->web_ui();
  EXPECT_TRUE(web_ui);

  // Starts a reload of the WebUI page.
  contents()->GetController().Reload(ReloadType::NORMAL, true);
  auto reload =
      NavigationSimulator::CreateFromPending(contents()->GetController());
  reload->ReadyToCommit();

  // It should be a same-site navigation reusing the same WebUI.
  EXPECT_FALSE(GetPendingFrameHost(manager));
  EXPECT_EQ(web_ui, host1->web_ui());

  // Navigation request to a non-WebUI page.
  const GURL kUrl("http://google.com");
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl, contents());
  navigation->ReadyToCommit();
  RenderFrameHostImpl* host2 = GetPendingFrameHost(manager);
  ASSERT_TRUE(host2);

  // The previous navigation should still be ongoing along with the new,
  // cross-site one.
  EXPECT_FALSE(host2->web_ui());
  EXPECT_EQ(web_ui, host1->web_ui());

  EXPECT_NE(host2, host1);
  EXPECT_NE(web_ui, host2->web_ui());

  // The new RenderFrameHost commits; there should be no active WebUI.
  navigation->Commit();
  EXPECT_EQ(host2, manager->current_frame_host());
  EXPECT_FALSE(host2->web_ui());
  EXPECT_FALSE(GetPendingFrameHost(manager));
}

// Shared code until before commit for the SimultaneousNavigationWithTwoWebUIs*
// tests, accepting a lambda to execute the commit step.
// Simulates two simultaneous navigations involving two WebUIs where the current
// RenderFrameHost commits.
TEST_P(RenderFrameHostManagerTest, SimultaneousNavigationWithTwoWebUIs1) {
  if (ShouldCreateNewHostForAllFrames()) {
    // This test involves starting a navigation while another navigation is
    // committing, which might lead to deletion of a pending commit RFH, which
    // will crash when RenderDocument is enabled. Skip the test if so.
    // TODO(crbug.com/40186427): Update this test to work under
    // navigation queueing, which will prevent the deletion of the pending
    // commit RFH but still fails because this test waits for the new navigation
    // to get to the ReadyToCommit stage before finishing the commit of the
    // pending commit RFH.
    return;
  }
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(),
                                                    GetWebUIURL("foo"));

  RenderFrameHostManager* manager =
      contents()->GetPrimaryFrameTree().root()->render_manager();
  RenderFrameHostImpl* host1 = manager->current_frame_host();
  EXPECT_TRUE(host1->IsRenderFrameLive());
  WebUIImpl* web_ui1 = host1->web_ui();
  EXPECT_TRUE(web_ui1);

  // Starts a reload of the WebUI page.
  contents()->GetController().Reload(ReloadType::NORMAL, true);
  auto reload =
      NavigationSimulator::CreateFromPending(contents()->GetController());
  reload->ReadyToCommit();

  // It should be a same-site navigation reusing the same WebUI.
  EXPECT_EQ(web_ui1, host1->web_ui());
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // Navigate to another WebUI page.
  const GURL kUrl(GetWebUIURL("bar"));
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl, contents());
  navigation->ReadyToCommit();
  RenderFrameHostImpl* host2 = GetPendingFrameHost(manager);
  ASSERT_TRUE(host2);

  // The previous navigation should still be ongoing along with the new,
  // cross-site one.
  EXPECT_EQ(web_ui1, host1->web_ui());
  EXPECT_TRUE(manager->speculative_frame_host());
  WebUIImpl* web_ui2 = manager->speculative_frame_host()->web_ui();
  EXPECT_TRUE(web_ui2);
  EXPECT_NE(web_ui2, web_ui1);

  EXPECT_NE(host2, host1);
  EXPECT_EQ(web_ui2, host2->web_ui());

  // The current RenderFrameHost commits; its WebUI should still be active.
  reload->Commit();
  EXPECT_EQ(host1, manager->current_frame_host());
  EXPECT_EQ(web_ui1, host1->web_ui());

  // Because the Navigation that committed was browser-initiated, it will not
  // have the user gesture bit set to true. This has the side-effect of not
  // deleting the speculative RenderFrameHost at commit time.
  // TODO(clamy): The speculative RenderFrameHost should be deleted at commit
  // time if a browser-initiated navigation commits.
  EXPECT_TRUE(manager->speculative_frame_host()->web_ui());
  EXPECT_TRUE(GetPendingFrameHost(manager));
}

// Simulates two simultaneous navigations involving two WebUIs where the new,
// cross-site RenderFrameHost commits.
TEST_P(RenderFrameHostManagerTest, SimultaneousNavigationWithTwoWebUIs2) {
  if (ShouldCreateNewHostForAllFrames()) {
    // This test involves starting a navigation while another navigation is
    // committing, which might lead to deletion of a pending commit RFH, which
    // will crash when RenderDocument is enabled. Skip the test if so.
    // TODO(crbug.com/40186427): Update this test to work under
    // navigation queueing, which will prevent the deletion of the pending
    // commit RFH but still fails because this test waits for the new navigation
    // to get to the ReadyToCommit stage before finishing the commit of the
    // pending commit RFH.
    return;
  }
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(),
                                                    GetWebUIURL("foo/"));

  RenderFrameHostManager* manager =
      contents()->GetPrimaryFrameTree().root()->render_manager();
  RenderFrameHostImpl* host1 = manager->current_frame_host();
  EXPECT_TRUE(host1->IsRenderFrameLive());
  WebUIImpl* web_ui1 = host1->web_ui();
  EXPECT_TRUE(web_ui1);

  // Starts a reload of the WebUI page.
  contents()->GetController().Reload(ReloadType::NORMAL, true);
  auto reload =
      NavigationSimulator::CreateFromPending(contents()->GetController());
  reload->ReadyToCommit();

  // It should be a same-site navigation reusing the same WebUI.
  EXPECT_EQ(web_ui1, host1->web_ui());
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // Navigate to another WebUI page.
  const GURL kUrl(GetWebUIURL("bar/"));
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl, contents());
  navigation->ReadyToCommit();
  RenderFrameHostImpl* host2 = GetPendingFrameHost(manager);
  ASSERT_TRUE(host2);

  // The previous navigation should still be ongoing along with the new,
  // cross-site one.
  EXPECT_EQ(web_ui1, host1->web_ui());
  WebUIImpl* web_ui2 = manager->speculative_frame_host()->web_ui();
  EXPECT_TRUE(web_ui2);
  EXPECT_NE(web_ui2, web_ui1);

  EXPECT_NE(host2, host1);
  EXPECT_EQ(web_ui2, host2->web_ui());

  navigation->Commit();
  EXPECT_EQ(host2, manager->current_frame_host());
  EXPECT_EQ(web_ui2, host2->web_ui());
  EXPECT_FALSE(manager->speculative_frame_host());
  EXPECT_FALSE(GetPendingFrameHost(manager));
}

TEST_P(RenderFrameHostManagerTest, CanCommitOrigin) {
  if (ShouldCreateNewHostForAllFrames() &&
      !ShouldQueueNavigationsWhenPendingCommitRFHExists()) {
    // This test involves starting multiple navigations consecutively, which
    // might lead to deletion of a pending commit RFH, which will crash when
    // RenderDocument is enabled. Skip the test if so, unless navigation
    // queueing is enabled.
    return;
  }
  const GURL kUrl("http://a.com/");
  const GURL kUrlBar("http://a.com/bar");

  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kUrl);

  struct TestCase {
    const char* const url;
    const char* const origin;
    bool mismatch;
  } cases[] = {
      // Positive case where the two match.
      {"http://a.com/foo.html", "http://a.com", false},

      // Host mismatches.
      {"http://a.com/", "http://b.com", true},
      {"http://b.com/", "http://a.com", true},

      // Scheme mismatches.
      {"file://", "http://a.com", true},
      {"https://a.com/", "http://a.com", true},

      // about:blank URLs inherit the origin of the context that navigated them.
      {"about:blank", "http://a.com", false},

      // Unique origin.
      {"http://a.com", "null", false},
  };

  for (const auto& test_case : cases) {
    auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
        GURL(test_case.url), main_test_rfh());
    url::Origin origin = url::Origin::Create(GURL(test_case.origin));

    // Manually control what origin will be committed on the "renderer-side" in
    // cases where we want to force the "renderer-side" to mismatch the correct
    // origin.
    if (test_case.mismatch)
      navigation->set_origin(origin);

    if (origin.opaque()) {
      auto response_headers =
          base::MakeRefCounted<net::HttpResponseHeaders>(std::string());
      response_headers->SetHeader("Content-Security-Policy", "sandbox");
      navigation->SetResponseHeaders(response_headers);
    }
    navigation->ReadyToCommit();

    auto* process = static_cast<MockRenderProcessHost*>(
        navigation->GetNavigationHandle()->GetRenderFrameHost()->GetProcess());
    int expected_bad_msg_count = process->bad_msg_count();
    if (test_case.mismatch)
      expected_bad_msg_count++;

    navigation->Commit();

    EXPECT_EQ(expected_bad_msg_count, process->bad_msg_count())
        << " url:" << test_case.url << " origin:" << test_case.origin
        << " mismatch:" << test_case.mismatch;
  }
}

// Tests that the correct intermediary and final navigation states are reached
// when navigating from a renderer that is not live to a WebUI URL.
TEST_P(RenderFrameHostManagerTest, NavigateFromDeadRendererToWebUI) {
  RenderFrameHostManager* manager =
      contents()->GetPrimaryFrameTree().root()->render_manager();

  RenderFrameHostImpl* initial_host = manager->current_frame_host();
  ASSERT_TRUE(initial_host);
  EXPECT_FALSE(initial_host->IsRenderFrameLive());

  // Navigation request.
  const GURL kUrl(GetWebUIURL("foo"));
  NavigationEntryImpl entry(
      nullptr /* instance */, kUrl, Referrer(),
      /* initiator_origin= */ std::nullopt,
      /* initiator_base_url= */ std::nullopt, std::u16string() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);
  FrameNavigationEntry* frame_entry = entry.root_node()->frame_entry.get();
  FrameTreeNode* frame_tree_node =
      manager->current_frame_host()->frame_tree_node();
  auto& referrer = frame_entry->referrer();
  blink::mojom::CommonNavigationParamsPtr common_params =
      entry.ConstructCommonNavigationParams(
          *frame_entry, nullptr, frame_entry->url(),
          blink::mojom::Referrer::New(referrer.url, referrer.policy),
          blink::mojom::NavigationType::DIFFERENT_DOCUMENT,
          base::TimeTicks::Now(), base::TimeTicks::Now());
  blink::mojom::CommitNavigationParamsPtr commit_params =
      entry.ConstructCommitNavigationParams(
          *frame_entry, common_params->url, common_params->method,
          entry.GetSubframeUniqueNames(frame_tree_node),
          controller().GetPendingEntryIndex() == -1 /* intended_as_new_entry */,
          static_cast<NavigationControllerImpl&>(controller())
              .GetIndexOfEntry(&entry),
          controller().GetLastCommittedEntryIndex(),
          controller().GetEntryCount(),
          frame_tree_node->current_replication_state().frame_policy,
          frame_tree_node->AncestorOrSelfHasCSPEE(),
          blink::mojom::SystemEntropy::kNormal,
          /*soft_navigation_heuristics_task_id=*/std::nullopt);

  std::unique_ptr<NavigationRequest> navigation_request =
      NavigationRequest::CreateBrowserInitiated(
          frame_tree_node, std::move(common_params), std::move(commit_params),
          false /* was_opener_suppressed */, entry.extra_headers(), frame_entry,
          &entry, false /* is_form_submission */,
          nullptr /* navigation_ui_data */, std::nullopt /* impression */,
          false /* is_pdf */
      );
  frame_tree_node->TakeNavigationRequest(std::move(navigation_request));

  // The initial non-live RenderFrameHost should be reused for the WebUI
  // navigation, and it should have gotten WebUI bindings by this time.
  RenderFrameHostImpl* host = manager->current_frame_host();
  ASSERT_TRUE(host);
  EXPECT_EQ(host, initial_host);
  EXPECT_TRUE(host->IsRenderFrameLive());
  WebUIImpl* web_ui = host->web_ui();
  EXPECT_TRUE(web_ui);
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // Prepare to commit, update the navigating RenderFrameHost.
  BrowsingContextGroupSwap ignored_bcg_swap_info =
      BrowsingContextGroupSwap::CreateDefault();
  EXPECT_EQ(host, manager
                      ->GetFrameHostForNavigation(
                          frame_tree_node->navigation_request(),
                          &ignored_bcg_swap_info)
                      .value());

  // No pending RenderFrameHost as the current one should be reused.
  EXPECT_FALSE(GetPendingFrameHost(manager));
  EXPECT_EQ(web_ui, host->web_ui());

  // The RenderFrameHost committed.
  DidNavigateFrame(manager, host);
  EXPECT_EQ(host, manager->current_frame_host());
  EXPECT_FALSE(GetPendingFrameHost(manager));
  EXPECT_EQ(web_ui, host->web_ui());
}

// Tests that the correct intermediary and final navigation states are reached
// when navigating same-site between two WebUIs of the same type.
TEST_P(RenderFrameHostManagerTest, NavigateSameSiteBetweenWebUIs) {
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(),
                                                    GetWebUIURL("foo"));

  RenderFrameHostManager* manager =
      contents()->GetPrimaryFrameTree().root()->render_manager();
  RenderFrameHostImpl* host = manager->current_frame_host();
  EXPECT_TRUE(host->IsRenderFrameLive());
  WebUIImpl* web_ui = host->web_ui();
  EXPECT_TRUE(web_ui);

  // Navigation request. No change in the returned WebUI type.
  const GURL kUrl(GetWebUIURL("foo/bar"));
  auto web_ui_navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl, contents());
  web_ui_navigation->Start();

  // The current WebUI should still be in place.
  EXPECT_EQ(web_ui, host->web_ui());
  EXPECT_EQ(ShouldCreateNewHostForAllFrames(), !!GetPendingFrameHost(manager));

  // Prepare to commit, update the navigating RenderFrameHost.
  web_ui_navigation->ReadyToCommit();

  EXPECT_EQ(web_ui, host->web_ui());
  EXPECT_EQ(ShouldCreateNewHostForAllFrames(), !!GetPendingFrameHost(manager));

  // The RenderFrameHost committed and used the same WebUI object if the
  // RenderFrameHost is reused.
  web_ui_navigation->Commit();
  if (ShouldCreateNewHostForAllFrames()) {
    EXPECT_NE(web_ui, manager->current_frame_host()->web_ui());
  } else {
    EXPECT_EQ(web_ui, manager->current_frame_host()->web_ui());
  }
}

// Tests that the correct intermediary and final navigation states are reached
// when navigating cross-site between two different WebUI types.
TEST_P(RenderFrameHostManagerTest, NavigateCrossSiteBetweenWebUIs) {
  // Cross-site navigations will always cause the change of the WebUI instance
  // but for consistency sake different types will be set for each navigation.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(),
                                                    GetWebUIURL("foo"));

  RenderFrameHostManager* manager =
      contents()->GetPrimaryFrameTree().root()->render_manager();
  RenderFrameHostImpl* host = manager->current_frame_host();
  EXPECT_TRUE(host->IsRenderFrameLive());
  EXPECT_TRUE(host->web_ui());

  // Navigate to different WebUI. This will cause the next navigation to
  // "chrome://bar" to require a different WebUI than the current one,
  // forcing it to be treated as cross-site.
  const GURL kUrl(GetWebUIURL("bar"));
  auto web_ui_navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl, contents());
  web_ui_navigation->Start();

  // The current WebUI should still be in place and there should be a new
  // active WebUI instance in the speculative RenderFrameHost.
  EXPECT_TRUE(manager->current_frame_host()->web_ui());
  RenderFrameHostImpl* speculative_host = GetPendingFrameHost(manager);
  EXPECT_TRUE(speculative_host);
  WebUIImpl* next_web_ui = speculative_host->web_ui();
  EXPECT_TRUE(next_web_ui);
  EXPECT_NE(next_web_ui, manager->current_frame_host()->web_ui());

  // The RenderFrameHost committed.
  web_ui_navigation->Commit();
  EXPECT_EQ(speculative_host, manager->current_frame_host());
  EXPECT_EQ(next_web_ui, manager->current_frame_host()->web_ui());
  EXPECT_FALSE(GetPendingFrameHost(manager));
}

// This class intercepts RenderFrameProxyHost creations, and overrides their
// respective blink::mojom::RemoteFrame instances.
class InsecureRequestPolicyProxyObserver
    : public RenderFrameProxyHost::TestObserver {
 public:
  InsecureRequestPolicyProxyObserver() {
    RenderFrameProxyHost::SetObserverForTesting(this);
  }
  ~InsecureRequestPolicyProxyObserver() override {
    RenderFrameProxyHost::SetObserverForTesting(nullptr);
  }
  blink::mojom::InsecureRequestPolicy GetRequestPolicy(
      RenderFrameProxyHost* proxy_host) {
    return remote_frames_[proxy_host]->enforce_insecure_request_policy();
  }

 private:
  // Stub out remote frame mojo binding. Intercepts calls to
  // EnforceInsecureRequestPolicy and marks the message as received.
  class RemoteFrame : public content::FakeRemoteFrame {
   public:
    explicit RemoteFrame(RenderFrameProxyHost* render_frame_proxy_host) {
      Init(render_frame_proxy_host->BindRemoteFrameReceiverForTesting());
    }

    void EnforceInsecureRequestPolicy(
        blink::mojom::InsecureRequestPolicy policy) override {
      enforce_insecure_request_policy_ = policy;
    }
    blink::mojom::InsecureRequestPolicy enforce_insecure_request_policy() {
      return enforce_insecure_request_policy_;
    }

   private:
    blink::mojom::InsecureRequestPolicy enforce_insecure_request_policy_;
  };

  void OnRemoteFrameBound(RenderFrameProxyHost* proxy_host) override {
    remote_frames_[proxy_host] = std::make_unique<RemoteFrame>(proxy_host);
  }

  std::map<RenderFrameProxyHost*, std::unique_ptr<RemoteFrame>> remote_frames_;
};

// Tests that frame proxies receive updates when a frame's enforcement
// of insecure request policy changes.
TEST_P(RenderFrameHostManagerTestWithSiteIsolation,
       ProxiesReceiveInsecureRequestPolicy) {
  const GURL kUrl1("http://www.google.test");
  const GURL kUrl2("http://www.google2.test");
  const GURL kUrl3("http://www.google2.test/foo");
  InsecureRequestPolicyProxyObserver observer;

  contents()->NavigateAndCommit(kUrl1);

  // Create a child frame and navigate it cross-site.
  main_test_rfh()->OnCreateChildFrame(
      main_test_rfh()->GetProcess()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, "frame1", "uniqueName1", false,
      blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(),
      blink::FrameOwnerElementType::kIframe, ukm::kInvalidSourceId);

  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  RenderFrameHostManager* child = root->child_at(0)->render_manager();

  // Navigate subframe to kUrl2.
  NavigationSimulator::NavigateAndCommitFromDocument(
      kUrl2, child->current_frame_host());

  // Verify that parent and child are in different processes.
  TestRenderFrameHost* child_host =
      static_cast<TestRenderFrameHost*>(child->current_frame_host());
  EXPECT_NE(child_host->GetProcess(), main_test_rfh()->GetProcess());

  // Change the parent's enforcement of strict mixed content checking,
  // and check that the correct IPC is sent to the child frame's
  // process.
  EXPECT_EQ(blink::mojom::InsecureRequestPolicy::kLeaveInsecureRequestsAlone,
            root->current_replication_state().insecure_request_policy);
  main_test_rfh()->DidEnforceInsecureRequestPolicy(
      blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent);
  RenderFrameProxyHost* proxy_to_child =
      root->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(child_host->GetSiteInstance()->group());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent,
            observer.GetRequestPolicy(proxy_to_child));
  EXPECT_EQ(blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent,
            root->current_replication_state().insecure_request_policy);

  // Do the same for the child's enforcement. In general, the parent
  // needs to know the status of the child's flag in case a grandchild
  // is created: if A.com embeds B.com, and B.com enforces strict mixed
  // content checking, and B.com adds an iframe to A.com, then the
  // A.com process needs to know B.com's flag so that the grandchild
  // A.com frame can inherit it.
  EXPECT_EQ(
      blink::mojom::InsecureRequestPolicy::kLeaveInsecureRequestsAlone,
      root->child_at(0)->current_replication_state().insecure_request_policy);
  child_host->DidEnforceInsecureRequestPolicy(
      blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent);
  RenderFrameProxyHost* proxy_to_parent =
      child->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(
              main_test_rfh()->GetSiteInstance()->group());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent,
            observer.GetRequestPolicy(proxy_to_parent));
  EXPECT_EQ(
      blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent,
      root->child_at(0)->current_replication_state().insecure_request_policy);

  // Check that the flag for the parent's proxy to the child is reset
  // when the child navigates.
  main_test_rfh()->GetProcess()->sink().ClearMessages();
  NavigationSimulator::NavigateAndCommitFromDocument(kUrl3, child_host);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::mojom::InsecureRequestPolicy::kLeaveInsecureRequestsAlone,
            observer.GetRequestPolicy(proxy_to_parent));
  EXPECT_EQ(
      blink::mojom::InsecureRequestPolicy::kLeaveInsecureRequestsAlone,
      root->child_at(0)->current_replication_state().insecure_request_policy);
}

// This class intercepts RenderFrameProxyHost creations, and overrides their
// respective blink::mojom::RemoteFrame instances, so that it can watch the
// start and stop loading states.
class StartStopLoadingProxyObserver
    : public RenderFrameProxyHost::TestObserver {
 public:
  StartStopLoadingProxyObserver() {
    RenderFrameProxyHost::SetObserverForTesting(this);
  }
  ~StartStopLoadingProxyObserver() override {
    RenderFrameProxyHost::SetObserverForTesting(nullptr);
  }
  bool IsLoading(RenderFrameProxyHost* proxy) {
    return remote_frames_[proxy]->is_loading();
  }

 private:
  class Remote : public content::FakeRemoteFrame {
   public:
    explicit Remote(RenderFrameProxyHost* proxy) {
      Init(proxy->BindRemoteFrameReceiverForTesting());
    }
    void DidStartLoading() override { is_loading_ = true; }
    void DidStopLoading() override { is_loading_ = false; }
    bool is_loading() { return is_loading_; }

   private:
    bool is_loading_ = false;
  };

  void OnRemoteFrameBound(RenderFrameProxyHost* proxy_host) override {
    remote_frames_[proxy_host] = std::make_unique<Remote>(proxy_host);
  }

  std::map<RenderFrameProxyHost*, std::unique_ptr<Remote>> remote_frames_;
};

// Tests that new frame proxies receive an IPC to update their loading state,
// if they are created for a frame that's currently loading.  See
// https://crbug.com/916137.
TEST_P(RenderFrameHostManagerTestWithSiteIsolation,
       NewProxyReceivesLoadingState) {
  StartStopLoadingProxyObserver proxy_observer;

  const GURL kUrl1("http://www.chromium.org");
  const GURL kUrl2("http://www.google.com");
  const GURL kUrl3("http://foo.com");

  // Navigate main frame to |kUrl1| and commit, but don't simulate
  // DidStopLoading.  The main frame should still be considered loading at this
  // point.
  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(kUrl1, contents());
  navigation->SetKeepLoading(true);
  navigation->Commit();
  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  EXPECT_TRUE(root->IsLoading());

  // Create a child frame.
  TestRenderFrameHost* child_host = main_test_rfh()->AppendChild("subframe");

  // Navigate the child cross-site.  Main frame should still be loading after
  // this point.
  child_host = static_cast<TestRenderFrameHost*>(
      NavigationSimulator::NavigateAndCommitFromDocument(kUrl2, child_host));
  EXPECT_TRUE(root->IsLoading());

  // Verify that parent and child are in different processes, and that there's
  // a proxy for the main frame in the child frame's process.
  ASSERT_NE(child_host->GetProcess(), main_test_rfh()->GetProcess());
  RenderFrameProxyHost* proxy_to_child =
      root->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(child_host->GetSiteInstance()->group());
  ASSERT_TRUE(proxy_to_child);
  ASSERT_EQ(proxy_to_child->GetProcess(), child_host->GetProcess());
  base::RunLoop().RunUntilIdle();

  // Since the main frame was loading at the time the main frame proxy was
  // created in child frame's process, verify that we sent a separate IPC to
  // update the proxy's loading state.  Note that we'll create two proxies for
  // the main frame and subframe, and we're interested in the message for
  // the main frame proxy.
  EXPECT_TRUE(proxy_observer.IsLoading(proxy_to_child));

  // Simulate load stop in the main frame.
  navigation->StopLoading();

  // Navigate the child to a third site.
  child_host = static_cast<TestRenderFrameHost*>(
      NavigationSimulator::NavigateAndCommitFromDocument(kUrl3, child_host));
  proxy_to_child =
      root->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(child_host->GetSiteInstance()->group());
  ASSERT_TRUE(proxy_to_child);
  ASSERT_EQ(proxy_to_child->GetProcess(), child_host->GetProcess());
  base::RunLoop().RunUntilIdle();

  // Since this time the main frame wasn't loading at the time |proxy_to_child|
  // was created in the process for |kUrl3|, verify that we didn't send any
  // extra IPCs to update that proxy's loading state.
  EXPECT_FALSE(proxy_observer.IsLoading(proxy_to_child));
}

// Tests that a BeginNavigation IPC from a no longer active RFH in pending
// deletion state is ignored.
TEST_P(RenderFrameHostManagerTest,
       BeginNavigationIgnoredWhenInPendingDeletion) {
  // When a page enters the BackForwardCache, the RenderFrameHost is not
  // deleted and is in BackForwardCache instead of being in pending deletion.
  // Disabling to consider this scenario.
  contents()->GetController().GetBackForwardCache().DisableForTesting(
      BackForwardCache::TEST_REQUIRES_NO_CACHING);

  const GURL kUrl1("http://www.google.com");
  const GURL kUrl2("http://www.chromium.org");
  const GURL kUrl3("http://foo.com");

  contents()->NavigateAndCommit(kUrl1);

  TestRenderFrameHost* initial_rfh = main_test_rfh();
  RenderViewHostDeletedObserver delete_observer(
      initial_rfh->GetRenderViewHost());

  // Navigate cross-site but don't simulate the swap out ACK. The initial RFH
  // should be pending delete.
  auto navigation_to_kUrl2 =
      NavigationSimulatorImpl::CreateBrowserInitiated(kUrl2, contents());
  navigation_to_kUrl2->set_drop_unload_ack(true);
  navigation_to_kUrl2->Commit();
  EXPECT_NE(initial_rfh, main_test_rfh());
  ASSERT_FALSE(delete_observer.deleted());
  EXPECT_NE(initial_rfh->lifecycle_state(),
            RenderFrameHostImpl::LifecycleStateImpl::kActive);
  EXPECT_TRUE(initial_rfh->IsPendingDeletion());

  // The initial RFH receives a BeginNavigation IPC. The navigation should not
  // start.
  auto navigation_to_kUrl3 =
      NavigationSimulator::CreateRendererInitiated(kUrl3, initial_rfh);
  navigation_to_kUrl3->Start();
  EXPECT_FALSE(main_test_rfh()->frame_tree_node()->navigation_request());
}

// Run tests with BackForwardCache.
class RenderFrameHostManagerTestWithBackForwardCache
    : public RenderFrameHostManagerTest,
      public WebContentsDelegate {
 public:
  RenderFrameHostManagerTestWithBackForwardCache() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            /*ignore_outstanding_network_request=*/false),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

  bool IsBackForwardCacheSupported(WebContents& web_contents) override {
    return true;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that a BeginNavigation IPC from a no longer active RFH in
// BackForwardCache is ignored. This test is a copy of
// "RenderFrameHostManagerTest.BeginNavigationIgnoredWhenInPendingDeletion" with
// BackForwardCache consideration.
TEST_P(RenderFrameHostManagerTestWithBackForwardCache,
       BeginNavigationIgnoredWhenInBackForwardCache) {
  const GURL kUrl1("http://www.google.com");
  const GURL kUrl2("http://www.chromium.org");
  const GURL kUrl3("http://foo.com");

  contents()->SetDelegate(this);
  contents()->NavigateAndCommit(kUrl1);

  TestRenderFrameHost* initial_rfh = main_test_rfh();
  RenderViewHostDeletedObserver delete_observer(
      initial_rfh->GetRenderViewHost());

  // Navigate cross-site but don't simulate the swap out ACK. The initial RFH
  // should be in BackForwardCache.
  auto navigation_to_kUrl2 =
      NavigationSimulatorImpl::CreateBrowserInitiated(kUrl2, contents());
  navigation_to_kUrl2->set_drop_unload_ack(true);
  navigation_to_kUrl2->Commit();
  EXPECT_NE(initial_rfh, main_test_rfh());
  ASSERT_FALSE(delete_observer.deleted());
  EXPECT_NE(initial_rfh->lifecycle_state(),
            RenderFrameHostImpl::LifecycleStateImpl::kActive);
  EXPECT_TRUE(initial_rfh->IsInBackForwardCache());

  // The initial RFH receives a BeginNavigation IPC. The navigation should not
  // start as initial RFH is not active.
  auto navigation_to_kUrl3 =
      NavigationSimulator::CreateRendererInitiated(kUrl3, initial_rfh);
  navigation_to_kUrl3->Start();
  EXPECT_FALSE(main_test_rfh()->frame_tree_node()->navigation_request());
}

// Check that after a navigation, the final SiteInstance has the correct
// original URL that was used to determine its site URL.
TEST_P(RenderFrameHostManagerTest,
       SiteInstanceOriginalURLIsPreservedAfterNavigation) {
  const GURL kFooUrl("https://foo.com");
  const GURL kOriginalUrl("https://original.com");
  const GURL kTranslatedUrl("https://translated.com");
  EffectiveURLContentBrowserClient modified_client(
      kOriginalUrl, kTranslatedUrl, /* requires_dedicated_process */ true);
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&modified_client);

  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kFooUrl);
  scoped_refptr<SiteInstanceImpl> initial_instance =
      main_test_rfh()->GetSiteInstance();
  SiteInfo foo_site_info = SiteInfo::CreateForTesting(
      initial_instance->GetIsolationContext(), kFooUrl);
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(initial_instance->IsDefaultSiteInstance());
  } else {
    EXPECT_FALSE(initial_instance->IsDefaultSiteInstance());
    EXPECT_EQ(kFooUrl, initial_instance->original_url());
    EXPECT_EQ(foo_site_info, initial_instance->GetSiteInfo());
  }

  // Simulate a browser-initiated navigation to an app URL, which should swap
  // processes and create a new SiteInstance in a new BrowsingInstance.
  // This new SiteInstance should have correct |original_url()| and a SiteInfo
  // that's based on it.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kOriginalUrl);
  EXPECT_NE(initial_instance.get(), main_test_rfh()->GetSiteInstance());
  EXPECT_FALSE(initial_instance->IsRelatedSiteInstance(
      main_test_rfh()->GetSiteInstance()));
  EXPECT_EQ(kOriginalUrl, main_test_rfh()->GetSiteInstance()->original_url());

  SiteInfo expected_site_info = SiteInfo::CreateForTesting(
      main_test_rfh()->GetSiteInstance()->GetIsolationContext(), kOriginalUrl);
  EXPECT_EQ(expected_site_info,
            main_test_rfh()->GetSiteInstance()->GetSiteInfo());
  EXPECT_NE(foo_site_info, main_test_rfh()->GetSiteInstance()->GetSiteInfo());

  SetBrowserClientForTesting(regular_client);
}

class AdTaggingSimulator : public WebContentsObserver {
 public:
  explicit AdTaggingSimulator(const std::set<GURL>& ad_urls,
                              WebContents* web_contents)
      : WebContentsObserver(web_contents), ad_urls_(ad_urls) {}

  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    auto it = ad_urls_.find(navigation_handle->GetURL());
    navigation_handle->GetRenderFrameHost()->UpdateIsAdFrame(it !=
                                                             ad_urls_.end());
  }

  void SimulateOnFrameIsAd(RenderFrameHost* rfh) { rfh->UpdateIsAdFrame(true); }

 private:
  std::set<GURL> ad_urls_;
};

class AdStatusInterceptingRemoteFrame : public content::FakeRemoteFrame {
 public:
  void SetReplicatedIsAdFrame(bool is_ad_frame) override {
    is_ad_frame_ = is_ad_frame;
  }

  // These methods reset state back to default when they are called.
  bool LastAdFrame() {
    bool is_ad_frame = is_ad_frame_;
    is_ad_frame_ = false;
    return is_ad_frame;
  }

 private:
  bool is_ad_frame_ = false;
};

class RenderFrameHostManagerAdTaggingSignalTest
    : public RenderFrameHostManagerTest,
      public RenderFrameProxyHost::TestObserver {
 public:
  RenderFrameHostManagerAdTaggingSignalTest() {
    RenderFrameProxyHost::SetObserverForTesting(this);
  }

  ~RenderFrameHostManagerAdTaggingSignalTest() override {
    RenderFrameProxyHost::SetObserverForTesting(nullptr);
  }

  void OnRemoteFrameBound(RenderFrameProxyHost* proxy_host) override {
    auto fake_remote_frame =
        std::make_unique<AdStatusInterceptingRemoteFrame>();
    fake_remote_frame->Init(proxy_host->BindRemoteFrameReceiverForTesting());

    // TODO(yaoxia): when a proxy host is deleted, remove the corresponding map
    // entry.
    proxy_map_[proxy_host] = std::move(fake_remote_frame);

    if (proxy_host->frame_tree_node()
            ->current_replication_state()
            .is_ad_frame) {
      ad_frames_on_proxy_created_.insert(proxy_host);
    }
  }

  void ExpectAdSubframeSignalForFrameProxy(RenderFrameProxyHost* proxy_host,
                                           bool expect_is_ad_frame) {
    base::RunLoop().RunUntilIdle();

    auto it = proxy_map_.find(proxy_host);
    EXPECT_TRUE(it != proxy_map_.end());

    AdStatusInterceptingRemoteFrame* remote_frame = it->second.get();
    EXPECT_EQ(expect_is_ad_frame, remote_frame->LastAdFrame());
  }

  void ExpectAdStatusOnFrameProxyCreated(RenderFrameProxyHost* proxy_host) {
    EXPECT_TRUE(ad_frames_on_proxy_created_.find(proxy_host) !=
                ad_frames_on_proxy_created_.end());
  }

  void AppendChildToFrame(const std::string& frame_name,
                          const GURL& url,
                          RenderFrameHost* rfh) {
    RenderFrameHost* subframe_host =
        RenderFrameHostTester::For(rfh)->AppendChild(frame_name);
    if (url.is_valid())
      NavigationSimulator::NavigateAndCommitFromDocument(url, subframe_host);
  }

  RenderFrameProxyHost* GetProxyHost(FrameTreeNode* proxy_node,
                                     FrameTreeNode* proxy_to_node) {
    return proxy_node->current_frame_host()
        ->browsing_context_state()
        ->GetRenderFrameProxyHost(
            proxy_to_node->current_frame_host()->GetSiteInstance()->group());
  }

 private:
  // The set of proxies that when created, the replication state of that frame
  // indicates it's an ad.
  std::set<raw_ptr<RenderFrameProxyHost, SetExperimental>>
      ad_frames_on_proxy_created_;

  std::map<RenderFrameProxyHost*,
           std::unique_ptr<AdStatusInterceptingRemoteFrame>>
      proxy_map_;
};

// Test that when the proxy host is created for the local child frame to be
// swapped out (which occurs before UnfreezableFrameMsg_SwapOut IPC was sent),
// the frame replication state should already have the ad status set.
TEST_P(RenderFrameHostManagerAdTaggingSignalTest,
       AdStatusForLocalChildFrameToBeSwappedOut) {
  if (!AreAllSitesIsolatedForTesting())
    return;

  const GURL kUrlA("http://a.com/");
  const GURL kUrlB("http://b.com/");

  std::set<GURL> ad_urls = {kUrlB};

  AdTaggingSimulator ad_tagging_simulator(ad_urls, contents());

  contents()->NavigateAndCommit(kUrlA);

  AppendChildToFrame("name", kUrlB, web_contents()->GetPrimaryMainFrame());

  FrameTreeNode* subframe_node =
      contents()->GetPrimaryFrameTree().root()->child_at(0);

  ExpectAdStatusOnFrameProxyCreated(
      subframe_node->render_manager()->GetProxyToParent());

  EXPECT_TRUE(subframe_node->current_replication_state().is_ad_frame);
}

// A page with top frame A that has subframes B and A1. A1 is an ad iframe that
// does not commit. We expect that the proxy of A1 in B's process will receive
// an ad tagging signal.
TEST_P(RenderFrameHostManagerAdTaggingSignalTest,
       AdTagSignalForFrameProxyOfFrameThatDoesNotCommit) {
  if (!AreAllSitesIsolatedForTesting())
    return;

  const GURL kUrlA("http://a.com/");
  const GURL kUrlB("http://b.com/");

  AdTaggingSimulator ad_tagging_simulator({}, contents());

  contents()->NavigateAndCommit(kUrlA);
  EXPECT_FALSE(contents()
                   ->GetPrimaryFrameTree()
                   .root()
                   ->current_replication_state()
                   .is_ad_frame);

  AppendChildToFrame("subframe_b", kUrlB,
                     web_contents()->GetPrimaryMainFrame());
  AppendChildToFrame("subframe_a1", GURL(),
                     web_contents()->GetPrimaryMainFrame());

  FrameTreeNode* top_frame_node_a = contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* subframe_node_b = top_frame_node_a->child_at(0);
  FrameTreeNode* subframe_node_a1 = top_frame_node_a->child_at(1);

  ad_tagging_simulator.SimulateOnFrameIsAd(
      subframe_node_a1->current_frame_host());

  RenderFrameProxyHost* proxy_a1_to_b =
      GetProxyHost(subframe_node_a1, subframe_node_b);

  EXPECT_TRUE(subframe_node_a1->current_replication_state().is_ad_frame);
  ExpectAdSubframeSignalForFrameProxy(proxy_a1_to_b, true);
}

// A page with top frame A that has subframes B and C. C is then navigated to an
// ad frame D. We expect that both the proxy of D in A's process and the proxy
// of D in B's process will receive an ad tagging signal.
TEST_P(RenderFrameHostManagerAdTaggingSignalTest,
       AdTagSignalForFrameProxyOfNewFrame) {
  if (!AreAllSitesIsolatedForTesting())
    return;

  const GURL kUrlA("http://a.com/");
  const GURL kUrlB("http://b.com/");
  const GURL kUrlC("http://c.com/");
  const GURL kUrlD("http://d.com/");

  std::set<GURL> ad_urls = {kUrlD};

  AdTaggingSimulator ad_tagging_simulator(ad_urls, contents());

  contents()->NavigateAndCommit(kUrlA);
  EXPECT_FALSE(contents()
                   ->GetPrimaryFrameTree()
                   .root()
                   ->current_replication_state()
                   .is_ad_frame);

  AppendChildToFrame("subframe_b", kUrlB,
                     web_contents()->GetPrimaryMainFrame());
  AppendChildToFrame("subframe_c", kUrlC,
                     web_contents()->GetPrimaryMainFrame());

  FrameTreeNode* top_frame_node_a = contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* subframe_node_b = top_frame_node_a->child_at(0);
  FrameTreeNode* subframe_node_c = top_frame_node_a->child_at(1);

  EXPECT_FALSE(subframe_node_b->current_replication_state().is_ad_frame);
  EXPECT_FALSE(subframe_node_c->current_replication_state().is_ad_frame);

  RenderFrameProxyHost* proxy_c_to_a =
      GetProxyHost(subframe_node_c, top_frame_node_a);
  RenderFrameProxyHost* proxy_c_to_b =
      GetProxyHost(subframe_node_c, subframe_node_b);
  RenderFrameProxyHost* proxy_b_to_a =
      GetProxyHost(subframe_node_b, top_frame_node_a);
  RenderFrameProxyHost* proxy_b_to_c =
      GetProxyHost(subframe_node_b, subframe_node_c);
  RenderFrameProxyHost* proxy_a_to_b =
      GetProxyHost(top_frame_node_a, subframe_node_b);
  RenderFrameProxyHost* proxy_a_to_c =
      GetProxyHost(top_frame_node_a, subframe_node_c);

  NavigationSimulator::NavigateAndCommitFromDocument(
      kUrlD, subframe_node_c->current_frame_host());

  EXPECT_TRUE(subframe_node_c->current_replication_state().is_ad_frame);

  ExpectAdSubframeSignalForFrameProxy(proxy_c_to_a, true);
  ExpectAdSubframeSignalForFrameProxy(proxy_c_to_b, true);
  ExpectAdSubframeSignalForFrameProxy(proxy_b_to_a, false);
  ExpectAdSubframeSignalForFrameProxy(proxy_b_to_c, false);
  ExpectAdSubframeSignalForFrameProxy(proxy_a_to_b, false);
  ExpectAdSubframeSignalForFrameProxy(proxy_a_to_c, false);
}

// A page with top frame A that has an ad subframe B. Frame C is then created in
// A. We expect that when the proxy host of B in C's process is created, the
// frame's replication status already has the ad bit set, which will be
// propagated to the renderer side later.
TEST_P(RenderFrameHostManagerAdTaggingSignalTest,
       AdStatusForFrameProxyOfExistingFrameToNewFrame) {
  if (!AreAllSitesIsolatedForTesting())
    return;

  const GURL kUrlA("http://a.com/");
  const GURL kUrlB("http://b.com/");
  const GURL kUrlC("http://c.com/");

  std::set<GURL> ad_urls = {kUrlB};

  AdTaggingSimulator ad_tagging_simulator(ad_urls, contents());

  contents()->NavigateAndCommit(kUrlA);

  AppendChildToFrame("subframe_b", kUrlB,
                     web_contents()->GetPrimaryMainFrame());
  AppendChildToFrame("subframe_c", kUrlC,
                     web_contents()->GetPrimaryMainFrame());

  FrameTreeNode* subframe_node_b =
      contents()->GetPrimaryFrameTree().root()->child_at(0);
  FrameTreeNode* subframe_node_c =
      contents()->GetPrimaryFrameTree().root()->child_at(1);
  RenderFrameProxyHost* proxy_b_to_c =
      GetProxyHost(subframe_node_b, subframe_node_c);
  ExpectAdStatusOnFrameProxyCreated(proxy_b_to_c);
}

// Test a A(B(C)) setup where B and C are ads. The creation of C will trigger
// an ad tagging signal for the proxy of C in the process of A.
TEST_P(RenderFrameHostManagerAdTaggingSignalTest, RemoteGrandchildAdTagSignal) {
  if (!AreAllSitesIsolatedForTesting())
    return;

  const GURL kUrlA("http://a.com/");
  const GURL kUrlB("http://b.com/");
  const GURL kUrlC("http://c.com/");

  std::set<GURL> ad_urls = {kUrlB, kUrlC};

  AdTaggingSimulator ad_tagging_simulator(ad_urls, contents());

  contents()->NavigateAndCommit(kUrlA);

  RenderFrameHost* subframe_host =
      RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
          ->AppendChild("subframe_name");

  auto navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(kUrlB, subframe_host);
  navigation_simulator->Start();
  navigation_simulator->Commit();

  RenderFrameHost* grandchild_host =
      RenderFrameHostTester::For(
          navigation_simulator->GetFinalRenderFrameHost())
          ->AppendChild("subframe_name");

  FrameTreeNode* top_frame_node = contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* subframe_node = top_frame_node->child_at(0);
  FrameTreeNode* grandchild_node = subframe_node->child_at(0);
  RenderFrameProxyHost* proxy_to_main_frame =
      GetProxyHost(grandchild_node, top_frame_node);

  NavigationSimulator::NavigateAndCommitFromDocument(kUrlC, grandchild_host);

  EXPECT_TRUE(subframe_node->current_replication_state().is_ad_frame);
  EXPECT_TRUE(grandchild_node->current_replication_state().is_ad_frame);
  ExpectAdSubframeSignalForFrameProxy(proxy_to_main_frame, true);
}

INSTANTIATE_TEST_SUITE_P(All,
                         RenderFrameHostManagerTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         RenderFrameHostManagerTestWithSiteIsolation,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         RenderFrameHostManagerAdTaggingSignalTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         RenderFrameHostManagerTestWithBackForwardCache,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
}  // namespace content
