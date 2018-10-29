// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_manager.h"

#include <stdint.h>

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_owner_properties.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/javascript_dialog_type.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/browser_side_navigation_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "content/test/mock_widget_input_handler.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_render_widget_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/load_flags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "ui/base/page_transition_types.h"

namespace content {
namespace {

// Helper to check that the provided RenderProcessHost received exactly one
// page focus message with the provided focus and routing ID values.
void VerifyPageFocusMessage(MockRenderProcessHost* rph,
                            bool expected_focus,
                            int expected_routing_id) {
  const IPC::Message* message =
      rph->sink().GetUniqueMessageMatching(InputMsg_SetFocus::ID);
  EXPECT_TRUE(message);
  EXPECT_EQ(expected_routing_id, message->routing_id());
  InputMsg_SetFocus::Param params;
  EXPECT_TRUE(InputMsg_SetFocus::Read(message, &params));
  EXPECT_EQ(expected_focus, std::get<0>(params));
}

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

// Helper function for strict mixed content checking tests.
void CheckInsecureRequestPolicyIPC(
    TestRenderFrameHost* rfh,
    blink::WebInsecureRequestPolicy expected_param,
    int expected_routing_id) {
  const IPC::Message* message =
      rfh->GetProcess()->sink().GetUniqueMessageMatching(
          FrameMsg_EnforceInsecureRequestPolicy::ID);
  ASSERT_TRUE(message);
  EXPECT_EQ(expected_routing_id, message->routing_id());
  FrameMsg_EnforceInsecureRequestPolicy::Param params;
  EXPECT_TRUE(FrameMsg_EnforceInsecureRequestPolicy::Read(message, &params));
  EXPECT_EQ(expected_param, std::get<0>(params));
}

class RenderFrameHostManagerTestWebUIControllerFactory
    : public WebUIControllerFactory {
 public:
  RenderFrameHostManagerTestWebUIControllerFactory()
      : should_create_webui_(false), type_(1) {
    CHECK_NE(reinterpret_cast<WebUI::TypeID>(type_), WebUI::kNoWebUI);
  }
  ~RenderFrameHostManagerTestWebUIControllerFactory() override {}

  void set_should_create_webui(bool should_create_webui) {
    should_create_webui_ = should_create_webui;
  }

  // This method simulates the expectation that different WebUI instance types
  // would be created. The |type| value will be returned by GetWebUIType casted
  // to WebUI::TypeID.
  // As WebUI::TypeID is a typedef to void pointer, factory implementations
  // return values that they know to be unique to their respective cases. So
  // values set here should be safe if kept very low (just above zero).
  void set_webui_type(uintptr_t type) {
    CHECK_NE(reinterpret_cast<WebUI::TypeID>(type), WebUI::kNoWebUI);
    type_ = type;
  }

  // WebUIFactory implementation.
  std::unique_ptr<WebUIController> CreateWebUIControllerForURL(
      WebUI* web_ui,
      const GURL& url) const override {
    // If WebUI creation is enabled for the test and this is a WebUI URL,
    // returns a new instance.
    if (should_create_webui_ && HasWebUIScheme(url))
      return std::make_unique<WebUIController>(web_ui);
    return nullptr;
  }

  WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                             const GURL& url) const override {
    // If WebUI creation is enabled for the test and this is a WebUI URL,
    // returns a mock WebUI type.
    if (should_create_webui_ && HasWebUIScheme(url)) {
      return reinterpret_cast<WebUI::TypeID>(type_);
    }
    return WebUI::kNoWebUI;
  }

  bool UseWebUIForURL(BrowserContext* browser_context,
                      const GURL& url) const override {
    return HasWebUIScheme(url);
  }

  bool UseWebUIBindingsForURL(BrowserContext* browser_context,
                              const GURL& url) const override {
    return HasWebUIScheme(url);
  }

 private:
  bool should_create_webui_;
  uintptr_t type_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostManagerTestWebUIControllerFactory);
};

class BeforeUnloadFiredWebContentsDelegate : public WebContentsDelegate {
 public:
  BeforeUnloadFiredWebContentsDelegate() {}
  ~BeforeUnloadFiredWebContentsDelegate() override {}

  void BeforeUnloadFired(WebContents* web_contents,
                         bool proceed,
                         bool* proceed_to_fire_unload) override {
    *proceed_to_fire_unload = proceed;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BeforeUnloadFiredWebContentsDelegate);
};

class CloseWebContentsDelegate : public WebContentsDelegate {
 public:
  CloseWebContentsDelegate() : close_called_(false) {}
  ~CloseWebContentsDelegate() override {}

  void CloseContents(WebContents* web_contents) override {
    close_called_ = true;
  }

  bool is_closed() { return close_called_; }

 private:
  bool close_called_;

  DISALLOW_COPY_AND_ASSIGN(CloseWebContentsDelegate);
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

  void RenderViewDeleted(RenderViewHost* render_view_host) override {
    if (render_view_host->GetProcess()->GetID() == process_id_ &&
        render_view_host->GetRoutingID() == routing_id_) {
      deleted_ = true;
    }
  }

  bool deleted() {
    return deleted_;
  }

 private:
  int process_id_;
  int routing_id_;
  bool deleted_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostDeletedObserver);
};

// This observer keeps track of the last created RenderFrameHost to allow tests
// to ensure that no RenderFrameHost objects are created when not expected.
class RenderFrameHostCreatedObserver : public WebContentsObserver {
 public:
  explicit RenderFrameHostCreatedObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents), created_(false) {}

  void RenderFrameCreated(RenderFrameHost* render_frame_host) override {
    created_ = true;
  }

  bool created() {
    return created_;
  }

 private:
  bool created_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostCreatedObserver);
};

// This WebContents observer keep track of its RVH change.
class RenderViewHostChangedObserver : public WebContentsObserver {
 public:
  explicit RenderViewHostChangedObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents), host_changed_(false) {}

  // WebContentsObserver.
  void RenderViewHostChanged(RenderViewHost* old_host,
                             RenderViewHost* new_host) override {
    host_changed_ = true;
  }

  bool DidHostChange() {
    bool host_changed = host_changed_;
    Reset();
    return host_changed;
  }

  void Reset() { host_changed_ = false; }

 private:
  bool host_changed_;
  DISALLOW_COPY_AND_ASSIGN(RenderViewHostChangedObserver);
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

  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid) override {
    plugin_crashed_ = true;
  }

  void DidUpdateFaviconURL(const std::vector<FaviconURL>& candidates) override {
    favicon_received_ = true;
  }

  bool plugin_crashed() {
    return plugin_crashed_;
  }

  bool favicon_received() {
    return favicon_received_;
  }

 private:
  bool plugin_crashed_;
  bool favicon_received_;

  DISALLOW_COPY_AND_ASSIGN(PluginFaviconMessageObserver);
};

}  // namespace

class RenderFrameHostManagerTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    WebUIControllerFactory::RegisterFactory(&factory_);
  }

  void TearDown() override {
    RenderViewHostImplTestHarness::TearDown();
    WebUIControllerFactory::UnregisterFactoryForTesting(&factory_);
  }

  void set_should_create_webui(bool should_create_webui) {
    factory_.set_should_create_webui(should_create_webui);
  }

  void set_webui_type(int type) { factory_.set_webui_type(type); }

  void NavigateActiveAndCommit(const GURL& url, bool dont_swap_out = false) {
    // Note: we navigate the active RenderFrameHost because previous navigations
    // won't have committed yet, so NavigateAndCommit does the wrong thing
    // for us.
    controller().LoadURL(
        url, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
    int entry_id = controller().GetPendingEntry()->GetUniqueID();

    // Simulate the BeforeUnload_ACK that is received from the current renderer
    // for a cross-site navigation.
    // PlzNavigate: it is necessary to call PrepareForCommit before getting the
    // main and the pending frame because when we are trying to navigate to a
    // WebUI from a new tab, a RenderFrameHost is created to display it that is
    // committed immediately (since it is a new tab). Therefore the main frame
    // is replaced without a pending frame being created, and we don't get the
    // right values for the RFH to navigate: we try to use the old one that has
    // been deleted in the meantime.
    contents()->GetMainFrame()->PrepareForCommit();

    TestRenderFrameHost* old_rfh = contents()->GetMainFrame();
    TestRenderFrameHost* active_rfh = contents()->GetPendingMainFrame()
                                          ? contents()->GetPendingMainFrame()
                                          : old_rfh;
    EXPECT_TRUE(old_rfh->is_active());

    // Use an observer to avoid accessing a deleted renderer later on when the
    // state is being checked.
    RenderFrameDeletedObserver rfh_observer(old_rfh);
    RenderViewHostDeletedObserver rvh_observer(old_rfh->GetRenderViewHost());
    active_rfh->SendNavigate(entry_id, true, url);

    // Make sure that we start to run the unload handler at the time of commit.
    if (old_rfh != active_rfh && !rfh_observer.deleted()) {
      EXPECT_FALSE(old_rfh->is_active());
    }

    // Simulate the swap out ACK coming from the pending renderer.  This should
    // either shut down the old RFH or leave it in a swapped out state.
    if (old_rfh != active_rfh) {
      if (dont_swap_out)
        return;
      old_rfh->OnSwappedOut();
      EXPECT_TRUE(rfh_observer.deleted());
    }
    EXPECT_EQ(active_rfh, contents()->GetMainFrame());
    EXPECT_EQ(nullptr, contents()->GetPendingMainFrame());
  }

  bool ShouldSwapProcesses(RenderFrameHostManager* manager,
                           const NavigationEntryImpl* current_entry,
                           const NavigationEntryImpl* new_entry) const {
    CHECK(new_entry);
    BrowserContext* browser_context =
        manager->delegate_->GetControllerForRenderManager().GetBrowserContext();
    const GURL& current_effective_url = current_entry ?
        SiteInstanceImpl::GetEffectiveURL(browser_context,
                                          current_entry->GetURL()) :
        manager->render_frame_host_->GetSiteInstance()->GetSiteURL();
    bool current_is_view_source_mode = current_entry ?
        current_entry->IsViewSourceMode() : new_entry->IsViewSourceMode();
    return manager->ShouldSwapBrowsingInstancesForNavigation(
        current_effective_url, current_is_view_source_mode,
        new_entry->site_instance(),
        SiteInstanceImpl::GetEffectiveURL(browser_context, new_entry->GetURL()),
        new_entry->IsViewSourceMode(), false);
  }

  // Creates a test RenderViewHost that's swapped out.
  void CreateSwappedOutRenderViewHost() {
    const GURL kChromeURL("chrome://foo");
    const GURL kDestUrl("http://www.google.com/");

    // Navigate our first tab to a chrome url and then to the destination.
    NavigateActiveAndCommit(kChromeURL);
    TestRenderFrameHost* ntp_rfh = contents()->GetMainFrame();

    // Navigate to a cross-site URL.
    contents()->GetController().LoadURL(
        kDestUrl, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
    int entry_id = contents()->GetController().GetPendingEntry()->GetUniqueID();
    contents()->GetMainFrame()->PrepareForCommit();
    EXPECT_TRUE(contents()->CrossProcessNavigationPending());

    // Manually increase the number of active frames in the
    // SiteInstance that ntp_rfh belongs to, to prevent it from being
    // destroyed when it gets swapped out.
    ntp_rfh->GetSiteInstance()->IncrementActiveFrameCount();

    TestRenderFrameHost* dest_rfh = contents()->GetPendingMainFrame();
    CHECK(dest_rfh);
    EXPECT_NE(ntp_rfh, dest_rfh);

    // BeforeUnload finishes.
    ntp_rfh->SendBeforeUnloadACK(true);

    dest_rfh->SendNavigate(entry_id, true, kDestUrl);
    ntp_rfh->OnSwappedOut();
  }

  // Returns the RenderFrameHost that should be used in the navigation to
  // |entry|.
  RenderFrameHostImpl* NavigateToEntry(RenderFrameHostManager* manager,
                                       const NavigationEntryImpl& entry) {
    // Tests currently only navigate using main frame FrameNavigationEntries.
    FrameNavigationEntry* frame_entry = entry.root_node()->frame_entry.get();
    FrameTreeNode* frame_tree_node =
        manager->current_frame_host()->frame_tree_node();
    NavigationControllerImpl* controller =
        static_cast<NavigationControllerImpl*>(manager->current_frame_host()
                                                   ->frame_tree_node()
                                                   ->navigator()
                                                   ->GetController());
    FrameMsg_Navigate_Type::Value navigate_type =
        entry.restore_type() == RestoreType::NONE
            ? FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT
            : FrameMsg_Navigate_Type::RESTORE;
    scoped_refptr<network::ResourceRequestBody> request_body;
    std::string post_content_type;
    if (frame_entry->method() == "POST") {
      request_body = frame_entry->GetPostData(&post_content_type);
      // Might have a LF at end.
      post_content_type =
          base::TrimWhitespaceASCII(post_content_type, base::TRIM_ALL)
              .as_string();
    }

    CommonNavigationParams common_params =
        entry.ConstructCommonNavigationParams(
            *frame_entry, request_body, frame_entry->url(),
            frame_entry->referrer(), navigate_type, PREVIEWS_UNSPECIFIED,
            base::TimeTicks::Now(), base::TimeTicks::Now());
    RequestNavigationParams request_params =
        entry.ConstructRequestNavigationParams(
            *frame_entry, common_params.url, common_params.method, false,
            entry.GetSubframeUniqueNames(frame_tree_node),
            controller->GetPendingEntryIndex() ==
                -1 /* intended_as_new_entry */,
            controller->GetIndexOfEntry(&entry),
            controller->GetLastCommittedEntryIndex(),
            controller->GetEntryCount());
    request_params.post_content_type = post_content_type;

    std::unique_ptr<NavigationRequest> navigation_request =
        NavigationRequest::CreateBrowserInitiated(
            frame_tree_node, common_params, request_params,
            !entry.is_renderer_initiated(), entry.extra_headers(), *frame_entry,
            entry, request_body, nullptr /* navigation_ui_data */);

    // Simulates request creation that triggers the 1st internal call to
    // GetFrameHostForNavigation.
    manager->DidCreateNavigationRequest(navigation_request.get());

    // And also simulates the 2nd and final call to GetFrameHostForNavigation
    // that determines the final frame that will commit the navigation.
    TestRenderFrameHost* frame_host = static_cast<TestRenderFrameHost*>(
        manager->GetFrameHostForNavigation(*navigation_request));
    CHECK(frame_host);
    return frame_host;
  }

  // Returns the speculative RenderFrameHost.
  RenderFrameHostImpl* GetPendingFrameHost(
      RenderFrameHostManager* manager) {
    return manager->speculative_render_frame_host_.get();
  }

  // Exposes RenderFrameHostManager::CollectOpenerFrameTrees for testing.
  void CollectOpenerFrameTrees(
      FrameTreeNode* node,
      std::vector<FrameTree*>* opener_frame_trees,
      base::hash_set<FrameTreeNode*>* nodes_with_back_links) {
    node->render_manager()->CollectOpenerFrameTrees(opener_frame_trees,
                                                    nodes_with_back_links);
  }

  void BaseSimultaneousNavigationWithOneWebUI(
      const std::function<void(RenderFrameHostImpl*,
                               RenderFrameHostImpl*,
                               WebUIImpl*,
                               RenderFrameHostManager*)>& commit_lambda);

  void BaseSimultaneousNavigationWithTwoWebUIs(
      const std::function<void(RenderFrameHostImpl*,
                               RenderFrameHostImpl*,
                               WebUIImpl*,
                               WebUIImpl*,
                               RenderFrameHostManager*)>& commit_lambda);

 private:
  RenderFrameHostManagerTestWebUIControllerFactory factory_;
};

// Tests that when you navigate from a chrome:// url to another page, and
// then do that same thing in another tab, that the two resulting pages have
// different SiteInstances, BrowsingInstances, and RenderProcessHosts. This is
// a regression test for bug 9364.
TEST_F(RenderFrameHostManagerTest, NewTabPageProcesses) {
  set_should_create_webui(true);
  const GURL kChromeUrl("chrome://foo");
  const GURL kDestUrl("http://www.google.com/");

  // Navigate our first tab to the chrome url and then to the destination,
  // ensuring we grant bindings to the chrome URL.
  NavigateActiveAndCommit(kChromeUrl);
  EXPECT_TRUE(main_rfh()->GetEnabledBindings() &
              BINDINGS_POLICY_WEB_UI);
  NavigateActiveAndCommit(kDestUrl);

  EXPECT_FALSE(contents()->GetPendingMainFrame());

  // Make a second tab.
  std::unique_ptr<TestWebContents> contents2(
      TestWebContents::Create(browser_context(), nullptr));

  // Load the two URLs in the second tab. Note that the first navigation creates
  // a RFH that's not pending (since there is no cross-site transition), so
  // we use the committed one.
  contents2->GetController().LoadURL(
      kChromeUrl, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  int entry_id = contents2->GetController().GetPendingEntry()->GetUniqueID();
  contents2->GetMainFrame()->PrepareForCommit();
  TestRenderFrameHost* ntp_rfh2 = contents2->GetMainFrame();
  EXPECT_FALSE(contents2->CrossProcessNavigationPending());
  ntp_rfh2->SendNavigate(entry_id, true, kChromeUrl);

  // The second one is the opposite, creating a cross-site transition and
  // requiring a beforeunload ack.
  contents2->GetController().LoadURL(
      kDestUrl, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  entry_id = contents2->GetController().GetPendingEntry()->GetUniqueID();
  contents2->GetMainFrame()->PrepareForCommit();
  EXPECT_TRUE(contents2->CrossProcessNavigationPending());
  TestRenderFrameHost* dest_rfh2 = contents2->GetPendingMainFrame();
  ASSERT_TRUE(dest_rfh2);

  dest_rfh2->SendNavigate(entry_id, true, kDestUrl);

  // The two RFH's should be different in every way.
  EXPECT_NE(contents()->GetMainFrame()->GetProcess(), dest_rfh2->GetProcess());
  EXPECT_NE(contents()->GetMainFrame()->GetSiteInstance(),
            dest_rfh2->GetSiteInstance());
  EXPECT_FALSE(dest_rfh2->GetSiteInstance()->IsRelatedSiteInstance(
                   contents()->GetMainFrame()->GetSiteInstance()));

  // Navigate both to the new tab page, and verify that they share a
  // RenderProcessHost (not a SiteInstance).
  NavigateActiveAndCommit(kChromeUrl);
  EXPECT_FALSE(contents()->GetPendingMainFrame());

  contents2->GetController().LoadURL(
      kChromeUrl, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  entry_id = contents2->GetController().GetPendingEntry()->GetUniqueID();
  contents2->GetMainFrame()->PrepareForCommit();
  contents2->GetPendingMainFrame()->SendNavigate(entry_id, true, kChromeUrl);

  EXPECT_NE(contents()->GetMainFrame()->GetSiteInstance(),
            contents2->GetMainFrame()->GetSiteInstance());
  EXPECT_EQ(contents()->GetMainFrame()->GetSiteInstance()->GetProcess(),
            contents2->GetMainFrame()->GetSiteInstance()->GetProcess());
}

// Ensure that the browser ignores most IPC messages that arrive from a
// RenderViewHost that has been swapped out.  We do not want to take
// action on requests from a non-active renderer.  The main exception is
// for synchronous messages, which cannot be ignored without leaving the
// renderer in a stuck state.  See http://crbug.com/93427.
TEST_F(RenderFrameHostManagerTest, FilterMessagesWhileSwappedOut) {
  const GURL kChromeURL("chrome://foo");
  const GURL kDestUrl("http://www.google.com/");
  std::vector<FaviconURL> icons;

  // Navigate our first tab to a chrome url and then to the destination.
  NavigateActiveAndCommit(kChromeURL);
  TestRenderFrameHost* ntp_rfh = contents()->GetMainFrame();

  // Send an update favicon message and make sure it works.
  {
    PluginFaviconMessageObserver observer(contents());
    EXPECT_TRUE(ntp_rfh->OnMessageReceived(
        FrameHostMsg_UpdateFaviconURL(ntp_rfh->GetRoutingID(), icons)));
    EXPECT_TRUE(observer.favicon_received());
  }
  // Create one more frame in the same SiteInstance where ntp_rfh
  // exists so that it doesn't get deleted on navigation to another
  // site.
  ntp_rfh->GetSiteInstance()->IncrementActiveFrameCount();

  // Navigate to a cross-site URL (don't swap out to keep |ntp_rfh| alive).
  NavigateActiveAndCommit(kDestUrl, true /* dont_swap_out */);
  TestRenderFrameHost* dest_rfh = contents()->GetMainFrame();
  ASSERT_TRUE(dest_rfh);
  EXPECT_NE(ntp_rfh, dest_rfh);

  // The new RVH should be able to update its favicon.
  {
    PluginFaviconMessageObserver observer(contents());
    EXPECT_TRUE(dest_rfh->OnMessageReceived(
        FrameHostMsg_UpdateFaviconURL(dest_rfh->GetRoutingID(), icons)));
    EXPECT_TRUE(observer.favicon_received());
  }

  // The old renderer, being slow, now updates the favicon. It should be
  // filtered out and not take effect.
  {
    PluginFaviconMessageObserver observer(contents());
    EXPECT_TRUE(ntp_rfh->OnMessageReceived(
        FrameHostMsg_UpdateFaviconURL(ntp_rfh->GetRoutingID(), icons)));
    EXPECT_FALSE(observer.favicon_received());
  }
}

// Test that the FrameHostMsg_UpdateFaviconURL IPC message is ignored if the
// renderer is in the STATE_PENDING_SWAP_OUT_STATE. The favicon code assumes
// that it only gets FrameHostMsg_UpdateFaviconURL messages for the most
// recently committed navigation for each WebContentsImpl.
TEST_F(RenderFrameHostManagerTest, UpdateFaviconURLWhilePendingSwapOut) {
  const GURL kChromeURL("chrome://foo");
  const GURL kDestUrl("http://www.google.com/");
  std::vector<FaviconURL> icons;

  // Navigate our first tab to a chrome url and then to the destination.
  NavigateActiveAndCommit(kChromeURL);
  TestRenderFrameHost* rfh1 = contents()->GetMainFrame();

  // Send an update favicon message and make sure it works.
  {
    PluginFaviconMessageObserver observer(contents());
    EXPECT_TRUE(rfh1->OnMessageReceived(
        FrameHostMsg_UpdateFaviconURL(rfh1->GetRoutingID(), icons)));
    EXPECT_TRUE(observer.favicon_received());
  }

  // Create one more frame in the same SiteInstance where |rfh1| exists so that
  // it doesn't get deleted on navigation to another site.
  rfh1->GetSiteInstance()->IncrementActiveFrameCount();

  // Navigate to a cross-site URL and commit the new page.
  controller().LoadURL(
      kDestUrl, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  int entry_id = controller().GetPendingEntry()->GetUniqueID();
  contents()->GetMainFrame()->PrepareForCommit();
  TestRenderFrameHost* rfh2 = contents()->GetPendingMainFrame();
  contents()->TestDidNavigate(rfh2, entry_id, true, kDestUrl,
                              ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(rfh1->is_active());
  EXPECT_TRUE(rfh2->is_active());

  // The new RVH should be able to update its favicons.
  {
    PluginFaviconMessageObserver observer(contents());
    EXPECT_TRUE(rfh2->OnMessageReceived(
        FrameHostMsg_UpdateFaviconURL(rfh2->GetRoutingID(), icons)));
    EXPECT_TRUE(observer.favicon_received());
  }

  // The old renderer, being slow, now updates its favicons. The message should
  // be ignored.
  {
    PluginFaviconMessageObserver observer(contents());
    EXPECT_TRUE(rfh1->OnMessageReceived(
        FrameHostMsg_UpdateFaviconURL(rfh1->GetRoutingID(), icons)));
    EXPECT_FALSE(observer.favicon_received());
  }
}

// Test if RenderViewHost::GetRenderWidgetHosts() only returns active
// widgets.
TEST_F(RenderFrameHostManagerTest, GetRenderWidgetHostsReturnsActiveViews) {
  CreateSwappedOutRenderViewHost();
  std::unique_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHost::GetRenderWidgetHosts());

  // We know that there is the only one active widget. Another view is
  // now swapped out, so the swapped out view is not included in the
  // list.
  RenderWidgetHost* widget = widgets->GetNextHost();
  EXPECT_FALSE(widgets->GetNextHost());
  RenderViewHost* rvh = RenderViewHost::From(widget);
  EXPECT_TRUE(static_cast<RenderViewHostImpl*>(rvh)->is_active());
}

// Test if RenderViewHost::GetRenderWidgetHosts() returns a subset of
// RenderViewHostImpl::GetAllRenderWidgetHosts().
// RenderViewHost::GetRenderWidgetHosts() returns only active widgets, but
// RenderViewHostImpl::GetAllRenderWidgetHosts() returns everything
// including swapped out ones.
TEST_F(RenderFrameHostManagerTest,
       GetRenderWidgetHostsWithinGetAllRenderWidgetHosts) {
  CreateSwappedOutRenderViewHost();
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
// as frames in a SiteInstance get swapped out and in.
TEST_F(RenderFrameHostManagerTest, ActiveFrameCountWhileSwappingInAndOut) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();

  SiteInstanceImpl* instance1 = rfh1->GetSiteInstance();
  EXPECT_EQ(instance1->active_frame_count(), 1U);

  // Create 2 new tabs and simulate them being the opener chain for the main
  // tab.  They should be in the same SiteInstance.
  std::unique_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), instance1));
  contents()->SetOpener(opener1.get());

  std::unique_ptr<TestWebContents> opener2(
      TestWebContents::Create(browser_context(), instance1));
  opener1->SetOpener(opener2.get());

  EXPECT_EQ(instance1->active_frame_count(), 3U);

  // Navigate to a cross-site URL (different SiteInstance but same
  // BrowsingInstance).
  contents()->NavigateAndCommit(kUrl2);
  TestRenderFrameHost* rfh2 = main_test_rfh();
  SiteInstanceImpl* instance2 = rfh2->GetSiteInstance();

  // rvh2 is on chromium.org which is different from google.com on
  // which other tabs are.
  EXPECT_EQ(instance2->active_frame_count(), 1U);

  // There are two active views on google.com now.
  EXPECT_EQ(instance1->active_frame_count(), 2U);

  // Navigate to the original origin (google.com).
  contents()->NavigateAndCommit(kUrl1);

  EXPECT_EQ(instance1->active_frame_count(), 3U);
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

  void RenderViewDeleted(RenderViewHost* render_view_host) override {
    if (render_view_host == render_view_host_)
      web_contents_.reset();
  }

 private:
  RenderViewHost* render_view_host_;
  std::unique_ptr<WebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostDestroyer);
};

// Test if ShutdownRenderViewHostsInSiteInstance() does not touch any
// RenderWidget that has been freed while deleting a RenderViewHost in
// a previous iteration. This is a regression test for
// http://crbug.com/259859.
TEST_F(RenderFrameHostManagerTest,
       DetectUseAfterFreeInShutdownRenderViewHostsInSiteInstance) {
  const GURL kChromeURL("chrome://newtab");
  const GURL kUrl1("http://www.google.com");
  const GURL kUrl2("http://www.chromium.org");

  // Navigate our first tab to a chrome url and then to the destination.
  NavigateActiveAndCommit(kChromeURL);
  TestRenderFrameHost* ntp_rfh = contents()->GetMainFrame();

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

// When there is an error with the specified page, renderer exits view-source
// mode. See WebFrameImpl::DidFail(). We check by this test that
// EnableViewSourceMode message is sent on every navigation regardless
// RenderView is being newly created or reused.
TEST_F(RenderFrameHostManagerTest, AlwaysSendEnableViewSourceMode) {
  const GURL kChromeUrl("chrome://foo/");
  const GURL kUrl("http://foo/");
  const GURL kViewSourceUrl("view-source:http://foo/");

  // We have to navigate to some page at first since without this, the first
  // navigation will reuse the SiteInstance created by Init(), and the second
  // one will create a new SiteInstance. Because current_instance and
  // new_instance will be different, a new RenderViewHost will be created for
  // the second navigation. We have to avoid this in order to exercise the
  // target code path.
  NavigateActiveAndCommit(kChromeUrl);

  // Navigate. Note that "view source" URLs are implemented by putting the RFH
  // into a view-source mode and then navigating to the inner URL, so that's why
  // the bare URL is what's committed and returned by the last committed entry's
  // GetURL() call.
  controller().LoadURL(
      kViewSourceUrl, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller().GetPendingEntry()->GetUniqueID();

  NavigationRequest* request =
      main_test_rfh()->frame_tree_node()->navigation_request();
  CHECK(request);

  // Simulate response from RenderFrame for DispatchBeforeUnload.
  contents()->GetMainFrame()->PrepareForCommit();
  ASSERT_TRUE(contents()->GetPendingMainFrame())
      << "Expected new pending RenderFrameHost to be created.";
  RenderFrameHost* last_rfh = contents()->GetPendingMainFrame();
  contents()->GetPendingMainFrame()->SimulateCommitProcessed(
      request->navigation_handle()->GetNavigationId(),
      true /* was_successful */);
  contents()->GetPendingMainFrame()->SendNavigate(entry_id, true, kUrl);

  EXPECT_EQ(1, controller().GetLastCommittedEntryIndex());
  NavigationEntry* last_committed = controller().GetLastCommittedEntry();
  ASSERT_NE(nullptr, last_committed);
  EXPECT_EQ(kUrl, last_committed->GetURL());
  EXPECT_EQ(kViewSourceUrl, last_committed->GetVirtualURL());
  EXPECT_FALSE(controller().GetPendingEntry());
  // Because we're using TestWebContents and TestRenderViewHost in this
  // unittest, no one calls WebContentsImpl::RenderViewCreated(). So, we see no
  // EnableViewSourceMode message, here.

  // Clear queued messages before load.
  process()->sink().ClearMessages();

  // Navigate, again.
  controller().LoadURL(
      kViewSourceUrl, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  request = main_test_rfh()->frame_tree_node()->navigation_request();
  CHECK(request);
  entry_id = controller().GetPendingEntry()->GetUniqueID();
  contents()->GetMainFrame()->PrepareForCommit();

  // The same RenderViewHost should be reused.
  EXPECT_FALSE(contents()->GetPendingMainFrame());
  EXPECT_EQ(last_rfh, contents()->GetMainFrame());

  // The renderer sends a commit.
  contents()->GetMainFrame()->SimulateCommitProcessed(
      request->navigation_handle()->GetNavigationId(),
      true /* was_successful */);
  contents()->GetMainFrame()->SendNavigateWithTransition(
      entry_id, false, kUrl, ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(1, controller().GetLastCommittedEntryIndex());
  EXPECT_FALSE(controller().GetPendingEntry());

  // New message should be sent out to make sure to enter view-source mode.
  EXPECT_TRUE(process()->sink().GetUniqueMessageMatching(
      FrameMsg_EnableViewSourceMode::ID));
}

// Tests the Init function by checking the initial RenderViewHost.
TEST_F(RenderFrameHostManagerTest, Init) {
  // Using TestBrowserContext.
  scoped_refptr<SiteInstanceImpl> instance =
      SiteInstanceImpl::Create(browser_context());
  EXPECT_FALSE(instance->HasSite());

  std::unique_ptr<TestWebContents> web_contents(
      TestWebContents::Create(browser_context(), instance));

  RenderFrameHostManager* manager = web_contents->GetRenderManagerForTesting();
  RenderViewHostImpl* rvh = manager->current_host();
  RenderFrameHostImpl* rfh = manager->current_frame_host();
  ASSERT_TRUE(rvh);
  ASSERT_TRUE(rfh);
  EXPECT_EQ(rvh, rfh->render_view_host());
  EXPECT_EQ(instance, rvh->GetSiteInstance());
  EXPECT_EQ(web_contents.get(), rvh->GetDelegate());
  EXPECT_EQ(web_contents.get(), rfh->delegate());
  EXPECT_TRUE(manager->GetRenderWidgetHostView());
}

// Tests the Navigate function. We navigate three sites consecutively and check
// how the pending/committed RenderViewHost are modified.
TEST_F(RenderFrameHostManagerTest, Navigate) {
  std::unique_ptr<TestWebContents> web_contents(TestWebContents::Create(
      browser_context(), SiteInstance::Create(browser_context())));
  RenderViewHostChangedObserver change_observer(web_contents.get());

  RenderFrameHostManager* manager = web_contents->GetRenderManagerForTesting();
  RenderFrameHostImpl* host = nullptr;

  // 1) The first navigation. --------------------------
  const GURL kUrl1("http://www.google.com/");
  NavigationEntryImpl entry1(
      nullptr /* instance */, kUrl1, Referrer(), base::string16() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */);
  host = NavigateToEntry(manager, entry1);

  // The RenderFrameHost created in Init will be reused.
  EXPECT_TRUE(host == manager->current_frame_host());
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // Commit.
  manager->DidNavigateFrame(host, true /* was_caused_by_user_gesture */,
                            false /* is_same_document_navigation */);
  // Commit to SiteInstance should be delayed until RenderFrame commit.
  EXPECT_TRUE(host == manager->current_frame_host());
  ASSERT_TRUE(host);
  EXPECT_FALSE(host->GetSiteInstance()->HasSite());
  host->GetSiteInstance()->SetSite(kUrl1);

  manager->GetRenderWidgetHostView()->SetBackgroundColor(SK_ColorRED);

  // 2) Navigate to next site. -------------------------
  const GURL kUrl2("http://www.google.com/foo");
  NavigationEntryImpl entry2(
      nullptr /* instance */, kUrl2,
      Referrer(kUrl1, network::mojom::ReferrerPolicy::kDefault),
      base::string16() /* title */, ui::PAGE_TRANSITION_LINK,
      true /* is_renderer_init */, nullptr /* blob_url_loader_factory */);
  host = NavigateToEntry(manager, entry2);

  // The RenderFrameHost created in Init will be reused.
  EXPECT_TRUE(host == manager->current_frame_host());
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // Commit.
  manager->DidNavigateFrame(host, true /* was_caused_by_user_gesture */,
                            false /* is_same_document_navigation */);
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
      base::string16() /* title */, ui::PAGE_TRANSITION_LINK,
      false /* is_renderer_init */, nullptr /* blob_url_loader_factory */);
  host = NavigateToEntry(manager, entry3);

  // A new RenderFrameHost should be created.
  EXPECT_TRUE(GetPendingFrameHost(manager));
  ASSERT_EQ(host, GetPendingFrameHost(manager));

  change_observer.Reset();

  // Commit.
  manager->DidNavigateFrame(GetPendingFrameHost(manager),
                            true /* was_caused_by_user_gesture */,
                            false /* is_same_document_navigation */);
  EXPECT_TRUE(host == manager->current_frame_host());
  ASSERT_TRUE(host);
  EXPECT_TRUE(host->GetSiteInstance()->HasSite());
  // Check the pending RenderFrameHost has been committed.
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // We should observe RVH changed event.
  EXPECT_TRUE(change_observer.DidHostChange());

  ASSERT_TRUE(manager->GetRenderWidgetHostView()->GetBackgroundColor());
  EXPECT_EQ(SK_ColorRED,
            *manager->GetRenderWidgetHostView()->GetBackgroundColor());
}

// Tests WebUI creation.
TEST_F(RenderFrameHostManagerTest, WebUI) {
  set_should_create_webui(true);
  scoped_refptr<SiteInstance> instance =
      SiteInstance::Create(browser_context());

  std::unique_ptr<TestWebContents> web_contents(
      TestWebContents::Create(browser_context(), instance));
  RenderFrameHostManager* manager = web_contents->GetRenderManagerForTesting();
  RenderFrameHostImpl* initial_rfh = manager->current_frame_host();

  EXPECT_FALSE(manager->current_host()->IsRenderViewLive());
  EXPECT_FALSE(manager->current_frame_host()->web_ui());
  EXPECT_TRUE(initial_rfh);

  const GURL kUrl("chrome://foo");
  NavigationEntryImpl entry(
      nullptr /* instance */, kUrl, Referrer(), base::string16() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */);
  RenderFrameHostImpl* host = NavigateToEntry(manager, entry);

  // We commit the pending RenderFrameHost immediately because the previous
  // RenderFrameHost was not live.  We test a case where it is live in
  // WebUIInNewTab.
  EXPECT_TRUE(host);
  EXPECT_NE(initial_rfh, host);
  EXPECT_EQ(host, manager->current_frame_host());
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // It's important that the SiteInstance get set on the Web UI page as soon
  // as the navigation starts, rather than lazily after it commits, so we don't
  // try to re-use the SiteInstance/process for non Web UI things that may
  // get loaded in between.
  EXPECT_TRUE(host->GetSiteInstance()->HasSite());
  EXPECT_EQ(kUrl, host->GetSiteInstance()->GetSiteURL());

  // There will be a navigating WebUI because GetFrameHostForNavigation was
  // already called twice and the committed  WebUI should be set to be reused.
  EXPECT_TRUE(manager->GetNavigatingWebUI());
  EXPECT_EQ(host->web_ui(), manager->GetNavigatingWebUI());
  EXPECT_EQ(host->web_ui(), host->pending_web_ui());
  EXPECT_TRUE(manager->current_frame_host()->web_ui());

  // Commit.
  manager->DidNavigateFrame(host, true /* was_caused_by_user_gesture */,
                            false /* is_same_document_navigation */);
  EXPECT_TRUE(host->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);
}

// Tests that we can open a WebUI link in a new tab from a WebUI page and still
// grant the correct bindings.  http://crbug.com/189101.
TEST_F(RenderFrameHostManagerTest, WebUIInNewTab) {
  set_should_create_webui(true);
  scoped_refptr<SiteInstance> blank_instance =
      SiteInstance::Create(browser_context());
  blank_instance->GetProcess()->Init();

  // Create a blank tab.
  std::unique_ptr<TestWebContents> web_contents1(
      TestWebContents::Create(browser_context(), blank_instance));
  RenderFrameHostManager* manager1 =
      web_contents1->GetRenderManagerForTesting();
  // Test the case that new RVH is considered live.
  manager1->current_host()->CreateRenderView(-1, MSG_ROUTING_NONE,
                                             base::UnguessableToken::Create(),
                                             FrameReplicationState(), false);
  EXPECT_TRUE(manager1->current_host()->IsRenderViewLive());
  EXPECT_TRUE(manager1->current_frame_host()->IsRenderFrameLive());

  // Navigate to a WebUI page.
  const GURL kUrl1("chrome://foo");
  NavigationEntryImpl entry1(
      nullptr /* instance */, kUrl1, Referrer(), base::string16() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */);
  RenderFrameHostImpl* host1 = NavigateToEntry(manager1, entry1);

  // We should have a pending navigation to the WebUI RenderViewHost.
  // It should already have bindings.
  EXPECT_EQ(host1, GetPendingFrameHost(manager1));
  EXPECT_NE(host1, manager1->current_frame_host());
  EXPECT_TRUE(host1->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);

  // Commit and ensure we still have bindings.
  manager1->DidNavigateFrame(host1, true /* was_caused_by_user_gesture */,
                             false /* is_same_document_navigation */);
  SiteInstance* webui_instance = host1->GetSiteInstance();
  EXPECT_EQ(host1, manager1->current_frame_host());
  EXPECT_TRUE(host1->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);

  // Now simulate clicking a link that opens in a new tab.
  std::unique_ptr<TestWebContents> web_contents2(
      TestWebContents::Create(browser_context(), webui_instance));
  RenderFrameHostManager* manager2 =
      web_contents2->GetRenderManagerForTesting();
  // Make sure the new RVH is considered live.  This is usually done in
  // RenderWidgetHost::Init when opening a new tab from a link.
  manager2->current_host()->CreateRenderView(-1, MSG_ROUTING_NONE,
                                             base::UnguessableToken::Create(),
                                             FrameReplicationState(), false);
  EXPECT_TRUE(manager2->current_host()->IsRenderViewLive());

  const GURL kUrl2("chrome://foo/bar");
  NavigationEntryImpl entry2(
      nullptr /* instance */, kUrl2, Referrer(), base::string16() /* title */,
      ui::PAGE_TRANSITION_LINK, true /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */);
  RenderFrameHostImpl* host2 = NavigateToEntry(manager2, entry2);

  // No cross-process transition happens because we are already in the right
  // SiteInstance.  We should grant bindings immediately.
  EXPECT_EQ(host2, manager2->current_frame_host());
  EXPECT_TRUE(manager2->GetNavigatingWebUI());
  EXPECT_FALSE(host2->web_ui());
  EXPECT_TRUE(host2->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);

  manager2->DidNavigateFrame(host2, true /* was_caused_by_user_gesture */,
                             false /* is_same_document_navigation */);
}

// Tests that a WebUI is correctly reused between chrome:// pages.
TEST_F(RenderFrameHostManagerTest, WebUIWasReused) {
  set_should_create_webui(true);

  // Navigate to a WebUI page.
  const GURL kUrl1("chrome://foo");
  contents()->NavigateAndCommit(kUrl1);
  WebUIImpl* web_ui = main_test_rfh()->web_ui();
  EXPECT_TRUE(web_ui);

  // Navigate to another WebUI page which should be same-site and keep the
  // current WebUI.
  const GURL kUrl2("chrome://foo/bar");
  contents()->NavigateAndCommit(kUrl2);
  EXPECT_EQ(web_ui, main_test_rfh()->web_ui());
}

// Tests that a WebUI is correctly cleaned up when navigating from a chrome://
// page to a non-chrome:// page.
TEST_F(RenderFrameHostManagerTest, WebUIWasCleared) {
  set_should_create_webui(true);

  // Navigate to a WebUI page.
  const GURL kUrl1("chrome://foo");
  contents()->NavigateAndCommit(kUrl1);
  EXPECT_TRUE(main_test_rfh()->web_ui());

  // Navigate to a non-WebUI page.
  const GURL kUrl2("http://www.google.com");
  contents()->NavigateAndCommit(kUrl2);
  EXPECT_FALSE(main_test_rfh()->web_ui());
}

// Ensure that we can go back and forward even if a SwapOut ACK isn't received.
// See http://crbug.com/93427.
TEST_F(RenderFrameHostManagerTest, NavigateAfterMissingSwapOutACK) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to two pages.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();

  // Keep active_frame_count nonzero so that no swapped out frames in
  // this SiteInstance get forcefully deleted.
  rfh1->GetSiteInstance()->IncrementActiveFrameCount();

  contents()->NavigateAndCommit(kUrl2);
  TestRenderFrameHost* rfh2 = main_test_rfh();
  rfh2->GetSiteInstance()->IncrementActiveFrameCount();

  // Now go back, but suppose the SwapOut_ACK isn't received.  This shouldn't
  // happen, but we have seen it when going back quickly across many entries
  // (http://crbug.com/93427).
  contents()->GetController().GoBack();
  EXPECT_TRUE(rfh2->is_waiting_for_beforeunload_ack());
  contents()->GetMainFrame()->PrepareForCommit();
  EXPECT_FALSE(rfh2->is_waiting_for_beforeunload_ack());

  // The back navigation commits.
  const NavigationEntry* entry1 = contents()->GetController().GetPendingEntry();
  contents()->GetPendingMainFrame()->SendNavigateWithTransition(
      entry1->GetUniqueID(), false, entry1->GetURL(),
      entry1->GetTransitionType());
  EXPECT_TRUE(rfh2->IsWaitingForUnloadACK());
  EXPECT_FALSE(rfh2->is_active());

  // We should be able to navigate forward.
  contents()->GetController().GoForward();
  contents()->GetMainFrame()->PrepareForCommit();
  const NavigationEntry* entry2 = contents()->GetController().GetPendingEntry();
  contents()->GetPendingMainFrame()->SendNavigateWithTransition(
      entry2->GetUniqueID(), false, entry2->GetURL(),
      entry2->GetTransitionType());
  EXPECT_TRUE(main_test_rfh()->is_active());
}

// Test that we create swapped out RFHs for the opener chain when navigating an
// opened tab cross-process.  This allows us to support certain cross-process
// JavaScript calls (http://crbug.com/99202).
TEST_F(RenderFrameHostManagerTest, CreateSwappedOutOpenerRFHs) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");
  const GURL kChromeUrl("chrome://foo");

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  RenderFrameHostManager* manager = contents()->GetRenderManagerForTesting();
  TestRenderFrameHost* rfh1 = main_test_rfh();
  scoped_refptr<SiteInstanceImpl> site_instance1 = rfh1->GetSiteInstance();
  RenderFrameDeletedObserver rfh1_deleted_observer(rfh1);
  TestRenderViewHost* rvh1 = test_rvh();

  // Create 2 new tabs and simulate them being the opener chain for the main
  // tab.  They should be in the same SiteInstance.
  std::unique_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), site_instance1.get()));
  RenderFrameHostManager* opener1_manager =
      opener1->GetRenderManagerForTesting();
  contents()->SetOpener(opener1.get());

  std::unique_ptr<TestWebContents> opener2(
      TestWebContents::Create(browser_context(), site_instance1.get()));
  RenderFrameHostManager* opener2_manager =
      opener2->GetRenderManagerForTesting();
  opener1->SetOpener(opener2.get());

  // Navigate to a cross-site URL (different SiteInstance but same
  // BrowsingInstance).
  contents()->NavigateAndCommit(kUrl2);
  TestRenderFrameHost* rfh2 = main_test_rfh();
  TestRenderViewHost* rvh2 = test_rvh();
  EXPECT_NE(site_instance1, rfh2->GetSiteInstance());
  EXPECT_TRUE(site_instance1->IsRelatedSiteInstance(rfh2->GetSiteInstance()));

  // Ensure rvh1 is placed on swapped out list of the current tab.
  EXPECT_TRUE(rfh1_deleted_observer.deleted());
  EXPECT_TRUE(manager->GetRenderFrameProxyHost(site_instance1.get()));
  EXPECT_EQ(rvh1,
            manager->GetSwappedOutRenderViewHost(rvh1->GetSiteInstance()));

  // Ensure a proxy and swapped out RVH are created in the first opener tab.
  EXPECT_TRUE(
      opener1_manager->GetRenderFrameProxyHost(rfh2->GetSiteInstance()));
  TestRenderViewHost* opener1_rvh = static_cast<TestRenderViewHost*>(
      opener1_manager->GetSwappedOutRenderViewHost(rvh2->GetSiteInstance()));
  EXPECT_FALSE(opener1_rvh->is_active());

  // Ensure a proxy and swapped out RVH are created in the second opener tab.
  EXPECT_TRUE(
      opener2_manager->GetRenderFrameProxyHost(rfh2->GetSiteInstance()));
  TestRenderViewHost* opener2_rvh = static_cast<TestRenderViewHost*>(
      opener2_manager->GetSwappedOutRenderViewHost(rvh2->GetSiteInstance()));
  EXPECT_FALSE(opener2_rvh->is_active());

  // Navigate to a cross-BrowsingInstance URL.
  contents()->NavigateAndCommit(kChromeUrl);
  TestRenderFrameHost* rfh3 = main_test_rfh();
  EXPECT_NE(site_instance1, rfh3->GetSiteInstance());
  EXPECT_FALSE(site_instance1->IsRelatedSiteInstance(rfh3->GetSiteInstance()));

  // No scripting is allowed across BrowsingInstances, so we should not create
  // swapped out RVHs for the opener chain in this case.
  EXPECT_FALSE(opener1_manager->GetRenderFrameProxyHost(
                   rfh3->GetSiteInstance()));
  EXPECT_FALSE(opener1_manager->GetSwappedOutRenderViewHost(
                   rfh3->GetSiteInstance()));
  EXPECT_FALSE(opener2_manager->GetRenderFrameProxyHost(
                   rfh3->GetSiteInstance()));
  EXPECT_FALSE(opener2_manager->GetSwappedOutRenderViewHost(
                   rfh3->GetSiteInstance()));
}

// Test that a page can disown the opener of the WebContents.
TEST_F(RenderFrameHostManagerTest, DisownOpener) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();
  scoped_refptr<SiteInstanceImpl> site_instance1 = rfh1->GetSiteInstance();

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
  rfh2->DidChangeOpener(MSG_ROUTING_NONE);

  // Ensure the opener is cleared.
  EXPECT_FALSE(contents()->HasOpener());
}

// Test that a page can disown a same-site opener of the WebContents.
TEST_F(RenderFrameHostManagerTest, DisownSameSiteOpener) {
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
  rfh1->DidChangeOpener(MSG_ROUTING_NONE);

  // Ensure the opener is cleared even if it is in the same process.
  EXPECT_FALSE(contents()->HasOpener());
}

// Test that a page can disown the opener just as a cross-process navigation is
// in progress.
TEST_F(RenderFrameHostManagerTest, DisownOpenerDuringNavigation) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  scoped_refptr<SiteInstanceImpl> site_instance1 =
      main_test_rfh()->GetSiteInstance();

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
  contents()->GetMainFrame()->PrepareForCommit();

  // Disown the opener from rfh2.
  rfh2->DidChangeOpener(MSG_ROUTING_NONE);

  // Ensure the opener is cleared.
  EXPECT_FALSE(contents()->HasOpener());

  // The back navigation commits.
  const NavigationEntry* entry1 = contents()->GetController().GetPendingEntry();
  contents()->GetPendingMainFrame()->SendNavigateWithTransition(
      entry1->GetUniqueID(), false, entry1->GetURL(),
      entry1->GetTransitionType());

  // Ensure the opener is still cleared.
  EXPECT_FALSE(contents()->HasOpener());
}

// Test that a page can disown the opener just after a cross-process navigation
// commits.
TEST_F(RenderFrameHostManagerTest, DisownOpenerAfterNavigation) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  scoped_refptr<SiteInstanceImpl> site_instance1 =
      main_test_rfh()->GetSiteInstance();

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
  contents()->GetMainFrame()->PrepareForCommit();
  const NavigationEntry* entry1 = contents()->GetController().GetPendingEntry();
  contents()->GetPendingMainFrame()->SendNavigateWithTransition(
      entry1->GetUniqueID(), false, entry1->GetURL(),
      entry1->GetTransitionType());

  // Disown the opener from rfh2.
  rfh2->DidChangeOpener(MSG_ROUTING_NONE);
  EXPECT_FALSE(contents()->HasOpener());
}

// Test that we clean up swapped out RenderViewHosts when a process hosting
// those associated RenderViews crashes. http://crbug.com/258993
TEST_F(RenderFrameHostManagerTest, CleanUpSwappedOutRVHOnProcessCrash) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = contents()->GetMainFrame();

  // Create a new tab as an opener for the main tab.
  std::unique_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), rfh1->GetSiteInstance()));
  RenderFrameHostManager* opener1_manager =
      opener1->GetRenderManagerForTesting();
  contents()->SetOpener(opener1.get());

  // Make sure the new opener RVH is considered live.
  opener1_manager->current_host()->CreateRenderView(
      -1, MSG_ROUTING_NONE, base::UnguessableToken::Create(),
      FrameReplicationState(), false);
  EXPECT_TRUE(opener1_manager->current_host()->IsRenderViewLive());
  EXPECT_TRUE(opener1_manager->current_frame_host()->IsRenderFrameLive());

  // Use a cross-process navigation in the opener to swap out the old RVH.
  EXPECT_FALSE(
      opener1_manager->GetSwappedOutRenderViewHost(rfh1->GetSiteInstance()));
  opener1->NavigateAndCommit(kUrl2);
  RenderViewHostImpl* swapped_out_rvh =
      opener1_manager->GetSwappedOutRenderViewHost(rfh1->GetSiteInstance());
  EXPECT_TRUE(swapped_out_rvh);
  EXPECT_TRUE(swapped_out_rvh->is_swapped_out_);
  EXPECT_FALSE(swapped_out_rvh->is_active());

  // Fake a process crash.
  rfh1->GetProcess()->SimulateCrash();

  // Ensure that the RenderFrameProxyHost stays around and the RenderFrameProxy
  // is deleted.
  RenderFrameProxyHost* render_frame_proxy_host =
      opener1_manager->GetRenderFrameProxyHost(rfh1->GetSiteInstance());
  EXPECT_TRUE(render_frame_proxy_host);
  EXPECT_FALSE(render_frame_proxy_host->is_render_frame_proxy_live());

  // Expect the swapped out RVH to exist but not be live.
  EXPECT_TRUE(
      opener1_manager->GetSwappedOutRenderViewHost(rfh1->GetSiteInstance()));
  EXPECT_FALSE(
      opener1_manager->GetSwappedOutRenderViewHost(rfh1->GetSiteInstance())
          ->IsRenderViewLive());

  // Reload the initial tab. This should recreate the opener's swapped out RVH
  // in the original SiteInstance.
  contents()->GetController().Reload(ReloadType::NORMAL, true);
  contents()->GetMainFrame()->PrepareForCommit();
  EXPECT_TRUE(
      opener1_manager->GetSwappedOutRenderViewHost(rfh1->GetSiteInstance())
          ->IsRenderViewLive());
  EXPECT_EQ(
      opener1_manager->GetRoutingIdForSiteInstance(rfh1->GetSiteInstance()),
      contents()->GetMainFrame()->GetRenderViewHost()->opener_frame_route_id());
}

// Test that RenderViewHosts created for WebUI navigations are properly
// granted WebUI bindings even if an unprivileged swapped out RenderViewHost
// is in the same process (http://crbug.com/79918).
TEST_F(RenderFrameHostManagerTest, EnableWebUIWithSwappedOutOpener) {
  set_should_create_webui(true);
  const GURL kSettingsUrl("chrome://chrome/settings");
  const GURL kPluginUrl("chrome://plugins");

  // Navigate to an initial WebUI URL.
  contents()->NavigateAndCommit(kSettingsUrl);

  // Ensure the RVH has WebUI bindings.
  TestRenderViewHost* rvh1 = test_rvh();
  EXPECT_TRUE(rvh1->GetMainFrame()->GetEnabledBindings() &
              BINDINGS_POLICY_WEB_UI);

  // Create a new tab and simulate it being the opener for the main
  // tab.  It should be in the same SiteInstance.
  std::unique_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), rvh1->GetSiteInstance()));
  RenderFrameHostManager* opener1_manager =
      opener1->GetRenderManagerForTesting();
  contents()->SetOpener(opener1.get());

  // Navigate to a different WebUI URL (different SiteInstance, same
  // BrowsingInstance).
  contents()->NavigateAndCommit(kPluginUrl);
  TestRenderViewHost* rvh2 = test_rvh();
  EXPECT_NE(rvh1->GetSiteInstance(), rvh2->GetSiteInstance());
  EXPECT_TRUE(rvh1->GetSiteInstance()->IsRelatedSiteInstance(
                  rvh2->GetSiteInstance()));

  // Ensure a proxy and swapped out RVH are created in the first opener tab.
  EXPECT_TRUE(
      opener1_manager->GetRenderFrameProxyHost(rvh2->GetSiteInstance()));
  TestRenderViewHost* opener1_rvh = static_cast<TestRenderViewHost*>(
      opener1_manager->GetSwappedOutRenderViewHost(rvh2->GetSiteInstance()));
  EXPECT_FALSE(opener1_rvh->is_active());

  // Ensure the new RVH has WebUI bindings.
  EXPECT_TRUE(rvh2->GetMainFrame()->GetEnabledBindings() &
              BINDINGS_POLICY_WEB_UI);
}

// Test that we reuse the same guest SiteInstance if we navigate across sites.
TEST_F(RenderFrameHostManagerTest, NoSwapOnGuestNavigations) {
  GURL guest_url(std::string(kGuestScheme).append("://abc123"));
  scoped_refptr<SiteInstance> instance =
      SiteInstance::CreateForURL(browser_context(), guest_url);
  std::unique_ptr<TestWebContents> web_contents(
      TestWebContents::Create(browser_context(), instance));

  RenderFrameHostManager* manager = web_contents->GetRenderManagerForTesting();

  RenderFrameHostImpl* host = nullptr;

  // 1) The first navigation. --------------------------
  const GURL kUrl1("http://www.google.com/");
  NavigationEntryImpl entry1(
      nullptr /* instance */, kUrl1, Referrer(), base::string16() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */);
  host = NavigateToEntry(manager, entry1);

  // The RenderFrameHost created in Init will be reused.
  EXPECT_TRUE(host == manager->current_frame_host());
  EXPECT_FALSE(manager->speculative_frame_host());
  EXPECT_EQ(manager->current_frame_host()->GetSiteInstance(), instance);

  // Commit.
  manager->DidNavigateFrame(host, true /* was_caused_by_user_gesture */,
                            false /* is_same_document_navigation */);
  // Commit to SiteInstance should be delayed until RenderFrame commit.
  EXPECT_EQ(host, manager->current_frame_host());
  ASSERT_TRUE(host);
  EXPECT_TRUE(host->GetSiteInstance()->HasSite());

  // 2) Navigate to a different domain. -------------------------
  // Guests stay in the same process on navigation.
  const GURL kUrl2("http://www.chromium.org");
  NavigationEntryImpl entry2(
      nullptr /* instance */, kUrl2,
      Referrer(kUrl1, network::mojom::ReferrerPolicy::kDefault),
      base::string16() /* title */, ui::PAGE_TRANSITION_LINK,
      true /* is_renderer_init */, nullptr /* blob_url_loader_factory */);
  host = NavigateToEntry(manager, entry2);

  // The RenderFrameHost created in Init will be reused.
  EXPECT_EQ(host, manager->current_frame_host());
  EXPECT_FALSE(manager->speculative_frame_host());

  // Commit.
  manager->DidNavigateFrame(host, true /* was_caused_by_user_gesture */,
                            false /* is_same_document_navigation */);
  EXPECT_EQ(host, manager->current_frame_host());
  ASSERT_TRUE(host);
  EXPECT_EQ(host->GetSiteInstance(), instance);
}

namespace {

class WidgetDestructionObserver : public RenderWidgetHostObserver {
 public:
  explicit WidgetDestructionObserver(base::OnceClosure closure)
      : closure_(std::move(closure)) {}

  void RenderWidgetHostDestroyed(RenderWidgetHost* widget_host) override {
    std::move(closure_).Run();
  }

 private:
  base::OnceClosure closure_;

  DISALLOW_COPY_AND_ASSIGN(WidgetDestructionObserver);
};

}  // namespace

// Test that we cancel a pending RVH if we close the tab while it's pending.
// http://crbug.com/294697.
TEST_F(RenderFrameHostManagerTest, NavigateWithEarlyClose) {
  scoped_refptr<SiteInstance> instance =
      SiteInstance::Create(browser_context());

  BeforeUnloadFiredWebContentsDelegate delegate;
  std::unique_ptr<TestWebContents> web_contents(
      TestWebContents::Create(browser_context(), instance));
  RenderViewHostChangedObserver change_observer(web_contents.get());
  web_contents->SetDelegate(&delegate);

  RenderFrameHostManager* manager = web_contents->GetRenderManagerForTesting();

  // 1) The first navigation. --------------------------
  const GURL kUrl1("http://www.google.com/");
  NavigationEntryImpl entry1(
      nullptr /* instance */, kUrl1, Referrer(), base::string16() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */);
  RenderFrameHostImpl* host = NavigateToEntry(manager, entry1);

  // The RenderFrameHost created in Init will be reused.
  EXPECT_EQ(host, manager->current_frame_host());
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // We should observe RVH changed event.
  EXPECT_TRUE(change_observer.DidHostChange());

  // Commit.
  manager->DidNavigateFrame(host, true /* was_caused_by_user_gesture */,
                            false /* is_same_document_navigation */);

  // Commit to SiteInstance should be delayed until RenderFrame commits.
  EXPECT_EQ(host, manager->current_frame_host());
  EXPECT_FALSE(host->GetSiteInstance()->HasSite());
  host->GetSiteInstance()->SetSite(kUrl1);

  // 2) Cross-site navigate to next site. -------------------------
  const GURL kUrl2("http://www.example.com");
  NavigationEntryImpl entry2(
      nullptr /* instance */, kUrl2, Referrer(), base::string16() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */);
  RenderFrameHostImpl* host2 = NavigateToEntry(manager, entry2);

  // A new RenderFrameHost should be created.
  ASSERT_EQ(host2, GetPendingFrameHost(manager));
  EXPECT_NE(host2, host);

  EXPECT_EQ(host, manager->current_frame_host());
  EXPECT_EQ(host2, GetPendingFrameHost(manager));

  // 3) Close the tab. -------------------------
  base::RunLoop run_loop;
  WidgetDestructionObserver observer(run_loop.QuitClosure());
  host2->render_view_host()->GetWidget()->AddObserver(&observer);

  manager->OnBeforeUnloadACK(true, base::TimeTicks());

  run_loop.Run();
  EXPECT_FALSE(GetPendingFrameHost(manager));
  EXPECT_EQ(host, manager->current_frame_host());
}

TEST_F(RenderFrameHostManagerTest, CloseWithPendingWhileUnresponsive) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  CloseWebContentsDelegate close_delegate;
  contents()->SetDelegate(&close_delegate);

  // Navigate to the first page.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = contents()->GetMainFrame();

  // Start to close the tab, but assume it's unresponsive.
  rfh1->render_view_host()->ClosePage();
  EXPECT_TRUE(rfh1->render_view_host()->is_waiting_for_close_ack());

  // Start a navigation to a new site.
  controller().LoadURL(
      kUrl2, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  rfh1->PrepareForCommit();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());

  // Simulate the unresponsiveness timer.  The tab should close.
  rfh1->render_view_host()->ClosePageTimeout();
  EXPECT_TRUE(close_delegate.is_closed());
}

// Tests that the RenderFrameHost is properly deleted when the SwapOutACK is
// received.  (SwapOut and the corresponding ACK always occur after commit.)
// Also tests that an early SwapOutACK is properly ignored.
TEST_F(RenderFrameHostManagerTest, DeleteFrameAfterSwapOutACK) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to the first page.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = contents()->GetMainFrame();
  RenderFrameDeletedObserver rfh_deleted_observer(rfh1);
  EXPECT_TRUE(rfh1->is_active());

  // Navigate to new site, simulating onbeforeunload approval.
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl2, contents());
  navigation->ReadyToCommit();
  int entry_id = controller().GetPendingEntry()->GetUniqueID();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_TRUE(rfh1->is_active());
  TestRenderFrameHost* rfh2 = contents()->GetPendingMainFrame();

  // Simulate the swap out ack, unexpectedly early (before commit).  It should
  // have no effect.
  rfh1->OnSwappedOut();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_TRUE(rfh1->is_active());

  // The new page commits.
  contents()->TestDidNavigate(rfh2, entry_id, true, kUrl2,
                              ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(rfh2, contents()->GetMainFrame());
  EXPECT_TRUE(contents()->GetPendingMainFrame() == nullptr);
  EXPECT_TRUE(rfh2->is_active());
  EXPECT_FALSE(rfh1->is_active());

  // Simulate the swap out ack.
  rfh1->OnSwappedOut();

  // rfh1 should have been deleted.
  EXPECT_TRUE(rfh_deleted_observer.deleted());
  rfh1 = nullptr;
}

// Tests that the RenderFrameHost is properly swapped out when the SwapOut ACK
// is received.  (SwapOut and the corresponding ACK always occur after commit.)
TEST_F(RenderFrameHostManagerTest, SwapOutFrameAfterSwapOutACK) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to the first page.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = contents()->GetMainFrame();
  RenderFrameDeletedObserver rfh_deleted_observer(rfh1);
  EXPECT_TRUE(rfh1->is_active());

  // Increment the number of active frames in SiteInstanceImpl so that rfh1 is
  // not deleted on swap out.
  rfh1->GetSiteInstance()->IncrementActiveFrameCount();

  // Navigate to new site, simulating onbeforeunload approval.
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl2, contents());
  navigation->ReadyToCommit();
  int entry_id = controller().GetPendingEntry()->GetUniqueID();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_TRUE(rfh1->is_active());
  TestRenderFrameHost* rfh2 = contents()->GetPendingMainFrame();

  // The new page commits.
  contents()->TestDidNavigate(rfh2, entry_id, true, kUrl2,
                              ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(rfh2, contents()->GetMainFrame());
  EXPECT_TRUE(contents()->GetPendingMainFrame() == nullptr);
  EXPECT_FALSE(rfh1->is_active());
  EXPECT_TRUE(rfh2->is_active());

  // Simulate the swap out ack.
  rfh1->OnSwappedOut();

  // rfh1 should be deleted.
  EXPECT_TRUE(rfh_deleted_observer.deleted());
}

// Test that the RenderViewHost is properly swapped out if a navigation in the
// new renderer commits before sending the SwapOut message to the old renderer.
// This simulates a cross-site navigation to a synchronously committing URL
// (e.g., a data URL) and ensures it works properly.
TEST_F(RenderFrameHostManagerTest,
       CommitNewNavigationBeforeSendingSwapOut) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to the first page.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = contents()->GetMainFrame();
  RenderFrameDeletedObserver rfh_deleted_observer(rfh1);
  EXPECT_TRUE(rfh1->is_active());

  // Increment the number of active frames in SiteInstanceImpl so that rfh1 is
  // not deleted on swap out.
  scoped_refptr<SiteInstanceImpl> site_instance = rfh1->GetSiteInstance();
  site_instance->IncrementActiveFrameCount();

  // Navigate to new site, simulating onbeforeunload approval.
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl2, contents());
  navigation->ReadyToCommit();
  int entry_id = controller().GetPendingEntry()->GetUniqueID();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_TRUE(rfh1->is_active());
  TestRenderFrameHost* rfh2 = contents()->GetPendingMainFrame();

  // The new page commits.
  contents()->TestDidNavigate(rfh2, entry_id, true, kUrl2,
                              ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(rfh2, contents()->GetMainFrame());
  EXPECT_TRUE(contents()->GetPendingMainFrame() == nullptr);
  EXPECT_FALSE(rfh1->is_active());
  EXPECT_TRUE(rfh2->is_active());

  // Simulate the swap out ack.
  rfh1->OnSwappedOut();

  // rfh1 should be deleted.
  EXPECT_TRUE(rfh_deleted_observer.deleted());
  EXPECT_TRUE(contents()->GetFrameTree()->root()->render_manager()
              ->GetRenderFrameProxyHost(site_instance.get()));
}

// Test that a RenderFrameHost is properly deleted when a cross-site navigation
// is cancelled.
TEST_F(RenderFrameHostManagerTest,
       CancelPendingProperlyDeletesOrSwaps) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");
  RenderFrameHostImpl* pending_rfh = nullptr;
  base::TimeTicks now = base::TimeTicks::Now();

  // Navigate to the first page.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();
  EXPECT_TRUE(rfh1->is_active());

  // Navigate to a new site, starting a cross-site navigation.
  controller().LoadURL(
      kUrl2, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  {
    pending_rfh = contents()->GetPendingMainFrame();
    RenderFrameDeletedObserver rfh_deleted_observer(pending_rfh);

    // Cancel the navigation by simulating a declined beforeunload dialog.
    contents()->GetMainFrame()->OnMessageReceived(
        FrameHostMsg_BeforeUnload_ACK(0, false, now, now));
    EXPECT_FALSE(contents()->CrossProcessNavigationPending());

    // Since the pending RFH is the only one for the new SiteInstance, it should
    // be deleted.
    EXPECT_TRUE(rfh_deleted_observer.deleted());
  }

  // Start another cross-site navigation.
  controller().LoadURL(
      kUrl2, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  {
    pending_rfh = contents()->GetPendingMainFrame();
    RenderFrameDeletedObserver rfh_deleted_observer(pending_rfh);

    // Increment the number of active frames in the new SiteInstance, which will
    // cause the pending RFH to be deleted and a RenderFrameProxyHost to be
    // created.
    scoped_refptr<SiteInstanceImpl> site_instance =
        pending_rfh->GetSiteInstance();
    site_instance->IncrementActiveFrameCount();

    contents()->GetMainFrame()->OnMessageReceived(
        FrameHostMsg_BeforeUnload_ACK(0, false, now, now));
    EXPECT_FALSE(contents()->CrossProcessNavigationPending());

    EXPECT_TRUE(rfh_deleted_observer.deleted());
    EXPECT_TRUE(contents()->GetFrameTree()->root()->render_manager()
                ->GetRenderFrameProxyHost(site_instance.get()));
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
TEST_F(RenderFrameHostManagerTestWithSiteIsolation, DetachPendingChild) {
  const GURL kUrlA("http://www.google.com/");
  const GURL kUrlB("http://webkit.org/");

  constexpr auto kOwnerType = blink::FrameOwnerElementType::kIframe;
  // Create a page with two child frames.
  contents()->NavigateAndCommit(kUrlA);
  contents()->GetMainFrame()->OnCreateChildFrame(
      contents()->GetMainFrame()->GetProcess()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
      blink::WebTreeScopeType::kDocument, "frame_name", "uniqueName1", false,
      base::UnguessableToken::Create(), blink::FramePolicy(),
      FrameOwnerProperties(), kOwnerType);
  contents()->GetMainFrame()->OnCreateChildFrame(
      contents()->GetMainFrame()->GetProcess()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
      blink::WebTreeScopeType::kDocument, "frame_name", "uniqueName2", false,
      base::UnguessableToken::Create(), blink::FramePolicy(),
      FrameOwnerProperties(), kOwnerType);
  RenderFrameHostManager* root_manager =
      contents()->GetFrameTree()->root()->render_manager();
  RenderFrameHostManager* iframe1 =
      contents()->GetFrameTree()->root()->child_at(0)->render_manager();
  RenderFrameHostManager* iframe2 =
      contents()->GetFrameTree()->root()->child_at(1)->render_manager();

  // 1) The first navigation.
  NavigationEntryImpl entryA(
      nullptr /* instance */, kUrlA, Referrer(), base::string16() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */);
  RenderFrameHostImpl* host1 = NavigateToEntry(iframe1, entryA);

  // The RenderFrameHost created in Init will be reused.
  EXPECT_TRUE(host1 == iframe1->current_frame_host());
  EXPECT_FALSE(GetPendingFrameHost(iframe1));

  // Commit.
  iframe1->DidNavigateFrame(host1, true /* was_caused_by_user_gesture */,
                            false /* is_same_document_navigation */);
  // Commit to SiteInstance should be delayed until RenderFrame commit.
  EXPECT_TRUE(host1 == iframe1->current_frame_host());
  ASSERT_TRUE(host1);
  EXPECT_TRUE(host1->GetSiteInstance()->HasSite());

  // 2) Cross-site navigate both frames to next site.
  NavigationEntryImpl entryB(
      nullptr /* instance */, kUrlB,
      Referrer(kUrlA, network::mojom::ReferrerPolicy::kDefault),
      base::string16() /* title */, ui::PAGE_TRANSITION_LINK,
      false /* is_renderer_init */, nullptr /* blob_url_loader_factory */);
  host1 = NavigateToEntry(iframe1, entryB);
  RenderFrameHostImpl* host2 = NavigateToEntry(iframe2, entryB);

  // A new, pending RenderFrameHost should be created in each FrameTreeNode.
  EXPECT_TRUE(GetPendingFrameHost(iframe1));
  EXPECT_TRUE(GetPendingFrameHost(iframe2));
  EXPECT_EQ(host1, GetPendingFrameHost(iframe1));
  EXPECT_EQ(host2, GetPendingFrameHost(iframe2));
  EXPECT_TRUE(GetPendingFrameHost(iframe1)->is_active());
  EXPECT_TRUE(GetPendingFrameHost(iframe2)->is_active());
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
  EXPECT_EQ(2U, site_instance->active_frame_count());

  // Proxies should exist.
  EXPECT_NE(nullptr,
            root_manager->GetRenderFrameProxyHost(site_instance.get()));
  EXPECT_NE(nullptr,
            iframe1->GetRenderFrameProxyHost(site_instance.get()));
  EXPECT_NE(nullptr,
            iframe2->GetRenderFrameProxyHost(site_instance.get()));

  // Detach the first child FrameTreeNode. This should kill the pending host but
  // not yet destroy proxies in |site_instance| since the other child remains.
  iframe1->current_frame_host()->OnMessageReceived(
      FrameHostMsg_Detach(iframe1->current_frame_host()->GetRoutingID()));
  iframe1 = nullptr;  // Was just destroyed.

  EXPECT_TRUE(delete_watcher1.deleted());
  EXPECT_FALSE(delete_watcher2.deleted());
  EXPECT_EQ(1U, site_instance->active_frame_count());

  // Proxies should still exist.
  EXPECT_NE(nullptr,
            root_manager->GetRenderFrameProxyHost(site_instance.get()));
  EXPECT_NE(nullptr,
            iframe2->GetRenderFrameProxyHost(site_instance.get()));

  // Detach the second child FrameTreeNode. This should trigger cleanup of
  // RenderFrameProxyHosts in |site_instance|.
  iframe2->current_frame_host()->OnMessageReceived(
      FrameHostMsg_Detach(iframe2->current_frame_host()->GetRoutingID()));
  iframe2 = nullptr;  // Was just destroyed.

  EXPECT_TRUE(delete_watcher1.deleted());
  EXPECT_TRUE(delete_watcher2.deleted());

  EXPECT_EQ(0U, site_instance->active_frame_count());
  EXPECT_EQ(nullptr,
            root_manager->GetRenderFrameProxyHost(site_instance.get()))
      << "Proxies should have been cleaned up";
  EXPECT_TRUE(site_instance->HasOneRef())
      << "This SiteInstance should be destroyable now.";
}

// Two tabs in the same process crash. The first tab is reloaded, and the second
// tab navigates away without reloading. The second tab's navigation shouldn't
// mess with the first tab's content. Motivated by http://crbug.com/473714.
TEST_F(RenderFrameHostManagerTestWithSiteIsolation,
       TwoTabsCrashOneReloadsOneLeaves) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://webkit.org/");
  const GURL kUrl3("http://whatwg.org/");

  // |contents1| and |contents2| navigate to the same page and then crash.
  TestWebContents* contents1 = contents();
  std::unique_ptr<TestWebContents> contents2(
      TestWebContents::Create(browser_context(), contents1->GetSiteInstance()));
  contents1->NavigateAndCommit(kUrl1);
  contents2->NavigateAndCommit(kUrl1);
  MockRenderProcessHost* rph = contents1->GetMainFrame()->GetProcess();
  EXPECT_EQ(rph, contents2->GetMainFrame()->GetProcess());
  rph->SimulateCrash();
  EXPECT_FALSE(contents1->GetMainFrame()->IsRenderFrameLive());
  EXPECT_FALSE(contents2->GetMainFrame()->IsRenderFrameLive());
  EXPECT_EQ(contents1->GetSiteInstance(), contents2->GetSiteInstance());

  // Reload |contents1|.
  contents1->NavigateAndCommit(kUrl1);
  EXPECT_TRUE(contents1->GetMainFrame()->IsRenderFrameLive());
  EXPECT_FALSE(contents2->GetMainFrame()->IsRenderFrameLive());
  EXPECT_EQ(contents1->GetSiteInstance(), contents2->GetSiteInstance());

  // |contents1| creates an out of process iframe.
  contents1->GetMainFrame()->OnCreateChildFrame(
      contents1->GetMainFrame()->GetProcess()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
      blink::WebTreeScopeType::kDocument, "frame_name", "uniqueName1", false,
      base::UnguessableToken::Create(), blink::FramePolicy(),
      FrameOwnerProperties(), blink::FrameOwnerElementType::kIframe);
  RenderFrameHostManager* iframe =
      contents()->GetFrameTree()->root()->child_at(0)->render_manager();
  NavigationEntryImpl entry(
      nullptr /* instance */, kUrl2,
      Referrer(kUrl1, network::mojom::ReferrerPolicy::kDefault),
      base::string16() /* title */, ui::PAGE_TRANSITION_LINK,
      false /* is_renderer_init */, nullptr /* blob_url_loader_factory */);
  RenderFrameHostImpl* cross_site = NavigateToEntry(iframe, entry);
  iframe->DidNavigateFrame(cross_site, true /* was_caused_by_user_gesture */,
                           false /* is_same_document_navigation */);

  // A proxy to the iframe should now exist in the SiteInstance of the main
  // frames.
  EXPECT_NE(cross_site->GetSiteInstance(), contents1->GetSiteInstance());
  EXPECT_NE(nullptr,
            iframe->GetRenderFrameProxyHost(contents1->GetSiteInstance()));
  EXPECT_NE(nullptr,
            iframe->GetRenderFrameProxyHost(contents2->GetSiteInstance()));

  // Navigate |contents2| away from the sad tab (and thus away from the
  // SiteInstance of |contents1|). This should not destroy the proxies needed by
  // |contents1| -- that was http://crbug.com/473714.
  EXPECT_FALSE(contents2->GetMainFrame()->IsRenderFrameLive());
  contents2->NavigateAndCommit(kUrl3);
  EXPECT_TRUE(contents2->GetMainFrame()->IsRenderFrameLive());
  EXPECT_NE(nullptr,
            iframe->GetRenderFrameProxyHost(contents1->GetSiteInstance()));
  EXPECT_EQ(nullptr,
            iframe->GetRenderFrameProxyHost(contents2->GetSiteInstance()));
}

// Ensure that we don't grant WebUI bindings to a pending RenderViewHost when
// creating proxies for a non-WebUI subframe navigation.  This was possible due
// to the InitRenderView call from CreateRenderFrameProxy.
// See https://crbug.com/536145.
TEST_F(RenderFrameHostManagerTestWithSiteIsolation,
       DontGrantPendingWebUIToSubframe) {
  set_should_create_webui(true);

  // Make sure the initial process is live so that the pending WebUI navigation
  // does not commit immediately.  Give the page a subframe as well.
  const GURL kUrl1("http://foo.com");
  RenderFrameHostImpl* main_rfh = contents()->GetMainFrame();
  NavigateAndCommit(kUrl1);
  EXPECT_TRUE(main_rfh->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(main_rfh->IsRenderFrameLive());
  main_rfh->OnCreateChildFrame(
      main_rfh->GetProcess()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
      blink::WebTreeScopeType::kDocument, std::string(), "uniqueName1", false,
      base::UnguessableToken::Create(), blink::FramePolicy(),
      FrameOwnerProperties(), blink::FrameOwnerElementType::kIframe);
  RenderFrameHostManager* subframe_rfhm =
      contents()->GetFrameTree()->root()->child_at(0)->render_manager();

  // Start a pending WebUI navigation in the main frame and verify that the
  // pending RVH has bindings.
  const GURL kWebUIUrl("chrome://foo");
  NavigationEntryImpl webui_entry(
      nullptr /* instance */, kWebUIUrl, Referrer(),
      base::string16() /* title */, ui::PAGE_TRANSITION_TYPED,
      false /* is_renderer_init */, nullptr /* blob_url_loader_factory */);
  RenderFrameHostManager* main_rfhm = contents()->GetRenderManagerForTesting();
  RenderFrameHostImpl* webui_rfh = NavigateToEntry(main_rfhm, webui_entry);
  EXPECT_EQ(webui_rfh, GetPendingFrameHost(main_rfhm));
  EXPECT_TRUE(webui_rfh->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);

  // Before it commits, do a cross-process navigation in a subframe.  This
  // should not grant WebUI bindings to the subframe's RVH.
  const GURL kSubframeUrl("http://bar.com");
  NavigationEntryImpl subframe_entry(
      nullptr /* instance */, kSubframeUrl, Referrer(),
      base::string16() /* title */, ui::PAGE_TRANSITION_LINK,
      false /* is_renderer_init */, nullptr /* blob_url_loader_factory */);
  RenderFrameHostImpl* bar_rfh = NavigateToEntry(subframe_rfhm, subframe_entry);
  EXPECT_FALSE(bar_rfh->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);
}

// Test that opener proxies are created properly with a cycle on the opener
// chain.
TEST_F(RenderFrameHostManagerTest, CreateOpenerProxiesWithCycleOnOpenerChain) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();
  scoped_refptr<SiteInstanceImpl> site_instance1 = rfh1->GetSiteInstance();

  // Create 2 new tabs and construct the opener chain as follows:
  //
  //     tab2 <--- tab1 <---- contents()
  //        |       ^
  //        +-------+
  //
  std::unique_ptr<TestWebContents> tab1(
      TestWebContents::Create(browser_context(), site_instance1.get()));
  RenderFrameHostManager* tab1_manager = tab1->GetRenderManagerForTesting();
  std::unique_ptr<TestWebContents> tab2(
      TestWebContents::Create(browser_context(), site_instance1.get()));
  RenderFrameHostManager* tab2_manager = tab2->GetRenderManagerForTesting();

  contents()->SetOpener(tab1.get());
  tab1->SetOpener(tab2.get());
  tab2->SetOpener(tab1.get());

  // Navigate main window to a cross-site URL.  This will call
  // CreateOpenerProxies() to create proxies for the two opener tabs in the new
  // SiteInstance.
  contents()->NavigateAndCommit(kUrl2);
  TestRenderFrameHost* rfh2 = main_test_rfh();
  EXPECT_NE(site_instance1, rfh2->GetSiteInstance());

  // Check that each tab now has a proxy in the new SiteInstance.
  RenderFrameProxyHost* tab1_proxy =
      tab1_manager->GetRenderFrameProxyHost(rfh2->GetSiteInstance());
  EXPECT_TRUE(tab1_proxy);
  RenderFrameProxyHost* tab2_proxy =
      tab2_manager->GetRenderFrameProxyHost(rfh2->GetSiteInstance());
  EXPECT_TRUE(tab2_proxy);

  // Verify that the proxies' openers point to each other.
  int tab1_opener_routing_id =
      tab1_manager->GetOpenerRoutingID(rfh2->GetSiteInstance());
  int tab2_opener_routing_id =
      tab2_manager->GetOpenerRoutingID(rfh2->GetSiteInstance());
  EXPECT_EQ(tab2_proxy->GetRoutingID(), tab1_opener_routing_id);
  EXPECT_EQ(tab1_proxy->GetRoutingID(), tab2_opener_routing_id);

  // Setting tab2_proxy's opener required an extra IPC message to be set, since
  // the opener's routing ID wasn't available when tab2_proxy was created.
  // Verify that this IPC was sent and that it passed correct routing ID.
  const IPC::Message* message =
      rfh2->GetProcess()->sink().GetUniqueMessageMatching(
          FrameMsg_UpdateOpener::ID);
  EXPECT_TRUE(message);
  FrameMsg_UpdateOpener::Param params;
  EXPECT_TRUE(FrameMsg_UpdateOpener::Read(message, &params));
  EXPECT_EQ(tab2_opener_routing_id, std::get<0>(params));
}

// Test that opener proxies are created properly when the opener points
// to itself.
TEST_F(RenderFrameHostManagerTest, CreateOpenerProxiesWhenOpenerPointsToSelf) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();
  scoped_refptr<SiteInstanceImpl> site_instance1 = rfh1->GetSiteInstance();

  // Create an opener tab, and simulate that its opener points to itself.
  std::unique_ptr<TestWebContents> opener(
      TestWebContents::Create(browser_context(), site_instance1.get()));
  RenderFrameHostManager* opener_manager = opener->GetRenderManagerForTesting();
  contents()->SetOpener(opener.get());
  opener->SetOpener(opener.get());

  // Navigate main window to a cross-site URL.  This will call
  // CreateOpenerProxies() to create proxies for the opener tab in the new
  // SiteInstance.
  contents()->NavigateAndCommit(kUrl2);
  TestRenderFrameHost* rfh2 = main_test_rfh();
  EXPECT_NE(site_instance1, rfh2->GetSiteInstance());

  // Check that the opener now has a proxy in the new SiteInstance.
  RenderFrameProxyHost* opener_proxy =
      opener_manager->GetRenderFrameProxyHost(rfh2->GetSiteInstance());
  EXPECT_TRUE(opener_proxy);

  // Verify that the proxy's opener points to itself.
  int opener_routing_id =
      opener_manager->GetOpenerRoutingID(rfh2->GetSiteInstance());
  EXPECT_EQ(opener_proxy->GetRoutingID(), opener_routing_id);

  // Setting the opener in opener_proxy required an extra IPC message, since
  // the opener's routing ID wasn't available when opener_proxy was created.
  // Verify that this IPC was sent and that it passed correct routing ID.
  const IPC::Message* message =
      rfh2->GetProcess()->sink().GetUniqueMessageMatching(
          FrameMsg_UpdateOpener::ID);
  EXPECT_TRUE(message);
  FrameMsg_UpdateOpener::Param params;
  EXPECT_TRUE(FrameMsg_UpdateOpener::Read(message, &params));
  EXPECT_EQ(opener_routing_id, std::get<0>(params));
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
TEST_F(RenderFrameHostManagerTest, TraverseComplexOpenerChain) {
  contents()->NavigateAndCommit(GURL("http://tab1.com"));
  FrameTree* tree1 = contents()->GetFrameTree();
  FrameTreeNode* root1 = tree1->root();
  int process_id = root1->current_frame_host()->GetProcess()->GetID();
  constexpr auto kOwnerType = blink::FrameOwnerElementType::kIframe;
  tree1->AddFrame(root1, process_id, 12,
                  TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
                  blink::WebTreeScopeType::kDocument, std::string(),
                  "uniqueName0", false, base::UnguessableToken::Create(),
                  blink::FramePolicy(), FrameOwnerProperties(), false,
                  kOwnerType);
  tree1->AddFrame(root1, process_id, 13,
                  TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
                  blink::WebTreeScopeType::kDocument, std::string(),
                  "uniqueName1", false, base::UnguessableToken::Create(),
                  blink::FramePolicy(), FrameOwnerProperties(), false,
                  kOwnerType);

  std::unique_ptr<TestWebContents> tab2(
      TestWebContents::Create(browser_context(), nullptr));
  tab2->NavigateAndCommit(GURL("http://tab2.com"));
  FrameTree* tree2 = tab2->GetFrameTree();
  FrameTreeNode* root2 = tree2->root();
  process_id = root2->current_frame_host()->GetProcess()->GetID();
  tree2->AddFrame(root2, process_id, 22,
                  TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
                  blink::WebTreeScopeType::kDocument, std::string(),
                  "uniqueName2", false, base::UnguessableToken::Create(),
                  blink::FramePolicy(), FrameOwnerProperties(), false,
                  kOwnerType);
  tree2->AddFrame(root2, process_id, 23,
                  TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
                  blink::WebTreeScopeType::kDocument, std::string(),
                  "uniqueName3", false, base::UnguessableToken::Create(),
                  blink::FramePolicy(), FrameOwnerProperties(), false,
                  kOwnerType);

  std::unique_ptr<TestWebContents> tab3(
      TestWebContents::Create(browser_context(), nullptr));
  FrameTree* tree3 = tab3->GetFrameTree();
  FrameTreeNode* root3 = tree3->root();

  std::unique_ptr<TestWebContents> tab4(
      TestWebContents::Create(browser_context(), nullptr));
  tab4->NavigateAndCommit(GURL("http://tab4.com"));
  FrameTree* tree4 = tab4->GetFrameTree();
  FrameTreeNode* root4 = tree4->root();
  process_id = root4->current_frame_host()->GetProcess()->GetID();
  tree4->AddFrame(root4, process_id, 42,
                  TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
                  blink::WebTreeScopeType::kDocument, std::string(),
                  "uniqueName4", false, base::UnguessableToken::Create(),
                  blink::FramePolicy(), FrameOwnerProperties(), false,
                  kOwnerType);

  root1->child_at(1)->SetOpener(root1->child_at(1));
  root1->SetOpener(root2->child_at(1));
  root2->SetOpener(root3);
  root2->child_at(0)->SetOpener(root4);
  root2->child_at(1)->SetOpener(root4);
  root4->child_at(0)->SetOpener(root3);

  std::vector<FrameTree*> opener_frame_trees;
  base::hash_set<FrameTreeNode*> nodes_with_back_links;

  CollectOpenerFrameTrees(root1, &opener_frame_trees, &nodes_with_back_links);

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

// Check that when a window is focused/blurred, the message that sets
// page-level focus updates is sent to each process involved in rendering the
// current page.
//
// TODO(alexmos): Move this test to FrameTree unit tests once NavigateToEntry
// is moved to a common place.  See https://crbug.com/547275.
TEST_F(RenderFrameHostManagerTest, PageFocusPropagatesToSubframeProcesses) {
  // This test only makes sense when cross-site subframes use separate
  // processes.
  if (!AreAllSitesIsolatedForTesting())
    return;

  const GURL kUrlA("http://a.com/");
  const GURL kUrlB("http://b.com/");
  const GURL kUrlC("http://c.com/");

  constexpr auto kOwnerType = blink::FrameOwnerElementType::kIframe;
  // Set up a page at a.com with three subframes: two for b.com and one for
  // c.com.
  contents()->NavigateAndCommit(kUrlA);
  main_test_rfh()->OnCreateChildFrame(
      main_test_rfh()->GetProcess()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
      blink::WebTreeScopeType::kDocument, "frame1", "uniqueName1", false,
      base::UnguessableToken::Create(), blink::FramePolicy(),
      FrameOwnerProperties(), kOwnerType);
  main_test_rfh()->OnCreateChildFrame(
      main_test_rfh()->GetProcess()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
      blink::WebTreeScopeType::kDocument, "frame2", "uniqueName2", false,
      base::UnguessableToken::Create(), blink::FramePolicy(),
      FrameOwnerProperties(), kOwnerType);
  main_test_rfh()->OnCreateChildFrame(
      main_test_rfh()->GetProcess()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
      blink::WebTreeScopeType::kDocument, "frame3", "uniqueName3", false,
      base::UnguessableToken::Create(), blink::FramePolicy(),
      FrameOwnerProperties(), kOwnerType);

  FrameTreeNode* root = contents()->GetFrameTree()->root();
  RenderFrameHostManager* child1 = root->child_at(0)->render_manager();
  RenderFrameHostManager* child2 = root->child_at(1)->render_manager();
  RenderFrameHostManager* child3 = root->child_at(2)->render_manager();

  // Navigate first two subframes to B.
  NavigationEntryImpl entryB(
      nullptr /* instance */, kUrlB,
      Referrer(kUrlA, network::mojom::ReferrerPolicy::kDefault),
      base::string16() /* title */, ui::PAGE_TRANSITION_LINK,
      false /* is_renderer_init */, nullptr /* blob_url_loader_factory */);
  TestRenderFrameHost* host1 =
      static_cast<TestRenderFrameHost*>(NavigateToEntry(child1, entryB));
  TestRenderFrameHost* host2 =
      static_cast<TestRenderFrameHost*>(NavigateToEntry(child2, entryB));
  child1->DidNavigateFrame(host1, true /* was_caused_by_user_gesture */,
                           false /* is_same_document_navigation */);
  child2->DidNavigateFrame(host2, true /* was_caused_by_user_gesture */,
                           false /* is_same_document_navigation */);

  // Navigate the third subframe to C.
  NavigationEntryImpl entryC(
      nullptr /* instance */, kUrlC,
      Referrer(kUrlA, network::mojom::ReferrerPolicy::kDefault),
      base::string16() /* title */, ui::PAGE_TRANSITION_LINK,
      false /* is_renderer_init */, nullptr /* blob_url_loader_factory */);
  TestRenderFrameHost* host3 =
      static_cast<TestRenderFrameHost*>(NavigateToEntry(child3, entryC));
  child3->DidNavigateFrame(host3, true /* was_caused_by_user_gesture */,
                           false /* is_same_document_navigation */);

  // Make sure the first two subframes and the third subframe are placed in
  // distinct processes.
  EXPECT_NE(host1->GetProcess(), main_test_rfh()->GetProcess());
  EXPECT_EQ(host1->GetProcess(), host2->GetProcess());
  EXPECT_NE(host3->GetProcess(), main_test_rfh()->GetProcess());
  EXPECT_NE(host3->GetProcess(), host1->GetProcess());

  // The main frame should have proxies for B and C.
  RenderFrameProxyHost* proxyB =
      root->render_manager()->GetRenderFrameProxyHost(host1->GetSiteInstance());
  EXPECT_TRUE(proxyB);
  RenderFrameProxyHost* proxyC =
      root->render_manager()->GetRenderFrameProxyHost(host3->GetSiteInstance());
  EXPECT_TRUE(proxyC);
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
  VerifyPageFocusMessage(host1->GetProcess(), true, proxyB->GetRoutingID());
  VerifyPageFocusMessage(host3->GetProcess(), true, proxyC->GetRoutingID());

  // Similarly, simulate focus loss on main page, and verify that the focus
  // message was sent to all processes.
  main_test_rfh()->GetProcess()->sink().ClearMessages();
  host1->GetProcess()->sink().ClearMessages();
  host3->GetProcess()->sink().ClearMessages();
  main_test_rfh()->GetRenderWidgetHost()->Blur();
  base::RunLoop().RunUntilIdle();
  VerifyPageFocusMessage(main_test_rfh()->GetRenderWidgetHost(), false);
  VerifyPageFocusMessage(host1->GetProcess(), false, proxyB->GetRoutingID());
  VerifyPageFocusMessage(host3->GetProcess(), false, proxyC->GetRoutingID());
}

// Check that page-level focus state is preserved across subframe navigations.
//
// TODO(alexmos): Move this test to FrameTree unit tests once NavigateToEntry
// is moved to a common place.  See https://crbug.com/547275.
TEST_F(RenderFrameHostManagerTest,
       PageFocusIsPreservedAcrossSubframeNavigations) {
  // This test only makes sense when cross-site subframes use separate
  // processes.
  if (!AreAllSitesIsolatedForTesting())
    return;

  const GURL kUrlA("http://a.com/");
  const GURL kUrlB("http://b.com/");
  const GURL kUrlC("http://c.com/");

  constexpr auto kOwnerType = blink::FrameOwnerElementType::kIframe;
  // Set up a page at a.com with a b.com subframe.
  contents()->NavigateAndCommit(kUrlA);
  main_test_rfh()->OnCreateChildFrame(
      main_test_rfh()->GetProcess()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
      blink::WebTreeScopeType::kDocument, "frame1", "uniqueName1", false,
      base::UnguessableToken::Create(), blink::FramePolicy(),
      FrameOwnerProperties(), kOwnerType);

  FrameTreeNode* root = contents()->GetFrameTree()->root();
  RenderFrameHostManager* child = root->child_at(0)->render_manager();

  // Navigate subframe to B.
  NavigationEntryImpl entryB(
      nullptr /* instance */, kUrlB,
      Referrer(kUrlA, network::mojom::ReferrerPolicy::kDefault),
      base::string16() /* title */, ui::PAGE_TRANSITION_LINK,
      false /* is_renderer_init */, nullptr /* blob_url_loader_factory */);
  TestRenderFrameHost* hostB =
      static_cast<TestRenderFrameHost*>(NavigateToEntry(child, entryB));
  child->DidNavigateFrame(hostB, true /* was_caused_by_user_gesture */,
                          false /* is_same_document_navigation */);

  // Ensure that the main page is focused.
  main_test_rfh()->GetView()->Focus();
  EXPECT_TRUE(main_test_rfh()->GetView()->HasFocus());

  // Navigate the subframe to C.
  NavigationEntryImpl entryC(
      nullptr /* instance */, kUrlC,
      Referrer(kUrlA, network::mojom::ReferrerPolicy::kDefault),
      base::string16() /* title */, ui::PAGE_TRANSITION_LINK,
      false /* is_renderer_init */, nullptr /* blob_url_loader_factory */);
  TestRenderFrameHost* hostC =
      static_cast<TestRenderFrameHost*>(NavigateToEntry(child, entryC));
  child->DidNavigateFrame(hostC, true /* was_caused_by_user_gesture */,
                          false /* is_same_document_navigation */);

  // The main frame should now have a proxy for C.
  RenderFrameProxyHost* proxy =
      root->render_manager()->GetRenderFrameProxyHost(hostC->GetSiteInstance());
  EXPECT_TRUE(proxy);

  // Since the B->C navigation happened while the current page was focused,
  // page focus should propagate to the new subframe process.  Check that
  // process C received the proper focus message.
  VerifyPageFocusMessage(hostC->GetProcess(), true, proxy->GetRoutingID());
}

// Checks that a restore navigation to a WebUI works.
TEST_F(RenderFrameHostManagerTest, RestoreNavigationToWebUI) {
  set_should_create_webui(true);

  const GURL kInitUrl("chrome://foo/");
  scoped_refptr<SiteInstanceImpl> initial_instance =
      SiteInstanceImpl::Create(browser_context());
  initial_instance->SetSite(kInitUrl);
  std::unique_ptr<TestWebContents> web_contents(
      TestWebContents::Create(browser_context(), initial_instance));
  RenderFrameHostManager* manager = web_contents->GetRenderManagerForTesting();
  NavigationControllerImpl& controller = web_contents->GetController();

  // Setup a restored entry.
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  std::unique_ptr<NavigationEntry> new_entry =
      NavigationController::CreateNavigationEntry(
          kInitUrl, Referrer(), ui::PAGE_TRANSITION_TYPED, false, std::string(),
          browser_context(), nullptr /* blob_url_loader_factory */);
  entries.push_back(std::move(new_entry));
  controller.Restore(0, RestoreType::LAST_SESSION_EXITED_CLEANLY, &entries);
  ASSERT_EQ(0u, entries.size());
  ASSERT_EQ(1, controller.GetEntryCount());

  RenderFrameHostImpl* initial_host = manager->current_frame_host();
  ASSERT_TRUE(initial_host);
  EXPECT_FALSE(initial_host->IsRenderFrameLive());
  EXPECT_FALSE(initial_host->web_ui());

  // Navigation request to an entry from a previous browsing session.
  NavigationEntryImpl entry(
      nullptr /* instance */, kInitUrl, Referrer(),
      base::string16() /* title */, ui::PAGE_TRANSITION_RELOAD,
      false /* is_renderer_init */, nullptr /* blob_url_loader_factory */);
  entry.set_restore_type(RestoreType::LAST_SESSION_EXITED_CLEANLY);
  NavigateToEntry(manager, entry);

  // As the initial renderer was not live, the new RenderFrameHost should be
  // made immediately active at request time.
  EXPECT_FALSE(GetPendingFrameHost(manager));
  TestRenderFrameHost* current_host =
      static_cast<TestRenderFrameHost*>(manager->current_frame_host());
  ASSERT_TRUE(current_host);
  EXPECT_EQ(current_host, initial_host);
  EXPECT_TRUE(current_host->IsRenderFrameLive());
  WebUIImpl* web_ui = manager->GetNavigatingWebUI();
  EXPECT_TRUE(web_ui);
  EXPECT_EQ(web_ui, current_host->pending_web_ui());
  EXPECT_FALSE(current_host->web_ui());

  // The RenderFrameHost committed.
  manager->DidNavigateFrame(current_host, true /* was_caused_by_user_gesture */,
                            false /* is_same_document_navigation */);
  EXPECT_EQ(current_host, manager->current_frame_host());
  EXPECT_EQ(web_ui, current_host->web_ui());
  EXPECT_FALSE(current_host->pending_web_ui());
}

// Shared code until before commit for the SimultaneousNavigationWithOneWebUI*
// tests, accepting a lambda to execute the commit step.
void RenderFrameHostManagerTest::BaseSimultaneousNavigationWithOneWebUI(
    const std::function<void(RenderFrameHostImpl*,
                             RenderFrameHostImpl*,
                             WebUIImpl*,
                             RenderFrameHostManager*)>& commit_lambda) {
  set_should_create_webui(true);
  NavigateActiveAndCommit(GURL("chrome://foo/"));

  RenderFrameHostManager* manager = contents()->GetRenderManagerForTesting();
  RenderFrameHostImpl* host1 = manager->current_frame_host();
  EXPECT_TRUE(host1->IsRenderFrameLive());
  WebUIImpl* web_ui = host1->web_ui();
  EXPECT_TRUE(web_ui);

  // Starts a reload of the WebUI page.
  contents()->GetController().Reload(ReloadType::NORMAL, true);
  main_test_rfh()->PrepareForCommit();

  // It should be a same-site navigation reusing the same WebUI.
  EXPECT_EQ(web_ui, manager->GetNavigatingWebUI());
  EXPECT_EQ(web_ui, host1->web_ui());
  EXPECT_EQ(web_ui, host1->pending_web_ui());
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // Navigation request to a non-WebUI page.
  const GURL kUrl("http://google.com");
  NavigationEntryImpl entry(
      nullptr /* instance */, kUrl, Referrer(), base::string16() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */);
  RenderFrameHostImpl* host2 = NavigateToEntry(manager, entry);
  ASSERT_TRUE(host2);

  // The previous navigation should still be ongoing along with the new,
  // cross-site one.
  // Note: Simultaneous navigations are weird: there are two ongoing
  // navigations, a same-site using a WebUI and a cross-site not using one. So
  // it's unclear what GetNavigatingWebUI should return in this case. As it
  // currently favors the cross-site navigation it returns null.
  EXPECT_FALSE(manager->GetNavigatingWebUI());
  EXPECT_EQ(web_ui, host1->web_ui());
  EXPECT_EQ(web_ui, host1->pending_web_ui());

  EXPECT_NE(host2, host1);
  EXPECT_EQ(host2, GetPendingFrameHost(manager));
  EXPECT_FALSE(host2->web_ui());
  EXPECT_FALSE(host2->pending_web_ui());
  EXPECT_NE(web_ui, host2->web_ui());

  commit_lambda(host1, host2, web_ui, manager);
}

// Simulates two simultaneous navigations involving one WebUI where the current
// RenderFrameHost commits.
TEST_F(RenderFrameHostManagerTest, SimultaneousNavigationWithOneWebUI1) {
  auto commit_current_frame_host = [this](
      RenderFrameHostImpl* host1, RenderFrameHostImpl* host2, WebUIImpl* web_ui,
      RenderFrameHostManager* manager) {
    // The current RenderFrameHost commits; its WebUI should still be in place.
    manager->DidNavigateFrame(host1, true /* was_caused_by_user_gesture */,
                              false /* is_same_document_navigation */);
    EXPECT_EQ(host1, manager->current_frame_host());
    EXPECT_EQ(web_ui, host1->web_ui());
    EXPECT_FALSE(host1->pending_web_ui());
    EXPECT_FALSE(manager->GetNavigatingWebUI());
    EXPECT_FALSE(GetPendingFrameHost(manager));
  };

  BaseSimultaneousNavigationWithOneWebUI(commit_current_frame_host);
}

// Simulates two simultaneous navigations involving one WebUI where the new,
// cross-site RenderFrameHost commits.
TEST_F(RenderFrameHostManagerTest, SimultaneousNavigationWithOneWebUI2) {
  auto commit_new_frame_host = [this](
      RenderFrameHostImpl* host1, RenderFrameHostImpl* host2, WebUIImpl* web_ui,
      RenderFrameHostManager* manager) {
    // The new RenderFrameHost commits; there should be no active WebUI.
    manager->DidNavigateFrame(host2, true /* was_caused_by_user_gesture */,
                              false /* is_same_document_navigation */);
    EXPECT_EQ(host2, manager->current_frame_host());
    EXPECT_FALSE(host2->web_ui());
    EXPECT_FALSE(host2->pending_web_ui());
    EXPECT_FALSE(manager->GetNavigatingWebUI());
    EXPECT_FALSE(GetPendingFrameHost(manager));
  };

  BaseSimultaneousNavigationWithOneWebUI(commit_new_frame_host);
}

// Shared code until before commit for the SimultaneousNavigationWithTwoWebUIs*
// tests, accepting a lambda to execute the commit step.
void RenderFrameHostManagerTest::BaseSimultaneousNavigationWithTwoWebUIs(
    const std::function<void(RenderFrameHostImpl*,
                             RenderFrameHostImpl*,
                             WebUIImpl*,
                             WebUIImpl*,
                             RenderFrameHostManager*)>& commit_lambda) {
  set_should_create_webui(true);
  set_webui_type(1);
  NavigateActiveAndCommit(GURL("chrome://foo/"));

  RenderFrameHostManager* manager = contents()->GetRenderManagerForTesting();
  RenderFrameHostImpl* host1 = manager->current_frame_host();
  EXPECT_TRUE(host1->IsRenderFrameLive());
  WebUIImpl* web_ui1 = host1->web_ui();
  EXPECT_TRUE(web_ui1);

  // Starts a reload of the WebUI page.
  contents()->GetController().Reload(ReloadType::NORMAL, true);

  // It should be a same-site navigation reusing the same WebUI.
  EXPECT_EQ(web_ui1, manager->GetNavigatingWebUI());
  EXPECT_EQ(web_ui1, host1->web_ui());
  EXPECT_EQ(web_ui1, host1->pending_web_ui());
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // Navigation another WebUI page, with a different type.
  set_webui_type(2);
  const GURL kUrl("chrome://bar/");
  NavigationEntryImpl entry(
      nullptr /* instance */, kUrl, Referrer(), base::string16() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */);
  RenderFrameHostImpl* host2 = NavigateToEntry(manager, entry);
  ASSERT_TRUE(host2);

  // The previous navigation should still be ongoing along with the new,
  // cross-site one.
  // Note: simultaneous navigations are weird: there are two ongoing
  // navigations, a same-site and a cross-site both going to WebUIs. So it's
  // unclear what GetNavigatingWebUI should return in this case. As it currently
  // favors the cross-site navigation it returns the speculative/pending
  // RenderFrameHost's WebUI instance.
  EXPECT_EQ(web_ui1, host1->web_ui());
  EXPECT_EQ(web_ui1, host1->pending_web_ui());
  WebUIImpl* web_ui2 = manager->GetNavigatingWebUI();
  EXPECT_TRUE(web_ui2);
  EXPECT_NE(web_ui2, web_ui1);

  EXPECT_NE(host2, host1);
  EXPECT_EQ(host2, GetPendingFrameHost(manager));
  EXPECT_EQ(web_ui2, host2->web_ui());
  EXPECT_FALSE(host2->pending_web_ui());

  commit_lambda(host1, host2, web_ui1, web_ui2, manager);
}

// Simulates two simultaneous navigations involving two WebUIs where the current
// RenderFrameHost commits.
TEST_F(RenderFrameHostManagerTest, SimultaneousNavigationWithTwoWebUIs1) {
  auto commit_current_frame_host = [this](
      RenderFrameHostImpl* host1, RenderFrameHostImpl* host2,
      WebUIImpl* web_ui1, WebUIImpl* web_ui2, RenderFrameHostManager* manager) {
    // The current RenderFrameHost commits; its WebUI should still be active.
    manager->DidNavigateFrame(host1, true /* was_caused_by_user_gesture */,
                              false /* is_same_document_navigation */);
    EXPECT_EQ(host1, manager->current_frame_host());
    EXPECT_EQ(web_ui1, host1->web_ui());
    EXPECT_FALSE(host1->pending_web_ui());
    EXPECT_FALSE(manager->GetNavigatingWebUI());
    EXPECT_FALSE(GetPendingFrameHost(manager));
  };

  BaseSimultaneousNavigationWithTwoWebUIs(commit_current_frame_host);
}

// Simulates two simultaneous navigations involving two WebUIs where the new,
// cross-site RenderFrameHost commits.
TEST_F(RenderFrameHostManagerTest, SimultaneousNavigationWithTwoWebUIs2) {
  auto commit_new_frame_host = [this](
      RenderFrameHostImpl* host1, RenderFrameHostImpl* host2,
      WebUIImpl* web_ui1, WebUIImpl* web_ui2, RenderFrameHostManager* manager) {
    // The new RenderFrameHost commits; its WebUI should now be active.
    manager->DidNavigateFrame(host2, true /* was_caused_by_user_gesture */,
                              false /* is_same_document_navigation */);
    EXPECT_EQ(host2, manager->current_frame_host());
    EXPECT_EQ(web_ui2, host2->web_ui());
    EXPECT_FALSE(host2->pending_web_ui());
    EXPECT_FALSE(manager->GetNavigatingWebUI());
    EXPECT_FALSE(GetPendingFrameHost(manager));
  };

  BaseSimultaneousNavigationWithTwoWebUIs(commit_new_frame_host);
}

TEST_F(RenderFrameHostManagerTest, CanCommitOrigin) {
  const GURL kUrl("http://a.com/");
  const GURL kUrlBar("http://a.com/bar");

  NavigateActiveAndCommit(kUrl);

  controller().LoadURL(
      kUrlBar, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  main_test_rfh()->PrepareForCommit();

  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = 0;
  params.did_create_new_entry = false;
  params.transition = ui::PAGE_TRANSITION_LINK;
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.method = "GET";
  params.page_state = PageState::CreateFromURL(kUrlBar);

  struct TestCase {
    const char* const url;
    const char* const origin;
    bool mismatch;
  } cases[] = {
    // Positive case where the two match.
    { "http://a.com/foo.html", "http://a.com", false },

    // Host mismatches.
    { "http://a.com/", "http://b.com", true },
    { "http://b.com/", "http://a.com", true },

    // Scheme mismatches.
    { "file://", "http://a.com", true },
    { "https://a.com/", "http://a.com", true },

    // about:blank URLs inherit the origin of the context that navigated them.
    { "about:blank", "http://a.com", false },

    // Unique origin.
    { "http://a.com", "null", false },
  };

  for (const auto& test_case : cases) {
    params.url = GURL(test_case.url);
    params.origin = url::Origin::Create(GURL(test_case.origin));

    int expected_bad_msg_count = process()->bad_msg_count();
    if (test_case.mismatch)
      expected_bad_msg_count++;

    main_test_rfh()->SendNavigateWithParams(
        &params, false /* was_within_same_document */);

    EXPECT_EQ(expected_bad_msg_count, process()->bad_msg_count())
      << " url:" << test_case.url
      << " origin:" << test_case.origin
      << " mismatch:" << test_case.mismatch;
  }
}

// RenderFrameHostManagerTest extension for PlzNavigate enabled tests.
// TODO(clamy): Make those regular RenderFrameHostManagerTests.
class RenderFrameHostManagerTestWithBrowserSideNavigation
    : public RenderFrameHostManagerTest {
 public:
  void SetUp() override {
    RenderFrameHostManagerTest::SetUp();
  }
};

// PlzNavigate: Tests that the correct intermediary and final navigation states
// are reached when navigating from a renderer that is not live to a WebUI URL.
TEST_F(RenderFrameHostManagerTestWithBrowserSideNavigation,
       NavigateFromDeadRendererToWebUI) {
  set_should_create_webui(true);
  RenderFrameHostManager* manager = contents()->GetRenderManagerForTesting();

  RenderFrameHostImpl* initial_host = manager->current_frame_host();
  ASSERT_TRUE(initial_host);
  EXPECT_FALSE(initial_host->IsRenderFrameLive());

  // Navigation request.
  const GURL kUrl("chrome://foo");
  NavigationEntryImpl entry(
      nullptr /* instance */, kUrl, Referrer(), base::string16() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */);
  FrameNavigationEntry* frame_entry = entry.root_node()->frame_entry.get();
  FrameTreeNode* frame_tree_node =
      manager->current_frame_host()->frame_tree_node();
  CommonNavigationParams common_params = entry.ConstructCommonNavigationParams(
      *frame_entry, nullptr, frame_entry->url(), frame_entry->referrer(),
      FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT, PREVIEWS_UNSPECIFIED,
      base::TimeTicks::Now(), base::TimeTicks::Now());
  RequestNavigationParams request_params =
      entry.ConstructRequestNavigationParams(
          *frame_entry, common_params.url, common_params.method, false,
          entry.GetSubframeUniqueNames(frame_tree_node),
          controller().GetPendingEntryIndex() == -1 /* intended_as_new_entry */,
          static_cast<NavigationControllerImpl&>(controller())
              .GetIndexOfEntry(&entry),
          controller().GetLastCommittedEntryIndex(),
          controller().GetEntryCount());

  std::unique_ptr<NavigationRequest> navigation_request =
      NavigationRequest::CreateBrowserInitiated(
          frame_tree_node, common_params, request_params,
          !entry.is_renderer_initiated(), entry.extra_headers(), *frame_entry,
          entry, nullptr /* request_body */, nullptr /* navigation_ui_data */);
  manager->DidCreateNavigationRequest(navigation_request.get());

  // As the initial RenderFrame was not live, the new RenderFrameHost should be
  // made as active/current immediately along with its WebUI at request time.
  RenderFrameHostImpl* host = manager->current_frame_host();
  ASSERT_TRUE(host);
  EXPECT_NE(host, initial_host);
  EXPECT_TRUE(host->IsRenderFrameLive());
  WebUIImpl* web_ui = host->web_ui();
  EXPECT_TRUE(web_ui);
  EXPECT_FALSE(host->pending_web_ui());
  EXPECT_FALSE(manager->GetNavigatingWebUI());
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // Prepare to commit, update the navigating RenderFrameHost.
  EXPECT_EQ(host, manager->GetFrameHostForNavigation(*navigation_request));

  // There should be a pending WebUI set to reuse the current one.
  EXPECT_EQ(web_ui, host->web_ui());
  EXPECT_EQ(web_ui, host->pending_web_ui());
  EXPECT_EQ(web_ui, manager->GetNavigatingWebUI());

  // No pending RenderFrameHost as the current one should be reused.
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // The RenderFrameHost committed.
  manager->DidNavigateFrame(host, true /* was_caused_by_user_gesture */,
                            false /* is_same_document_navigation */);
  EXPECT_EQ(host, manager->current_frame_host());
  EXPECT_FALSE(GetPendingFrameHost(manager));
  EXPECT_EQ(web_ui, host->web_ui());
  EXPECT_FALSE(host->pending_web_ui());
  EXPECT_FALSE(manager->GetNavigatingWebUI());
}

// PlzNavigate: Tests that the correct intermediary and final navigation states
// are reached when navigating same-site between two WebUIs of the same type.
TEST_F(RenderFrameHostManagerTestWithBrowserSideNavigation,
       NavigateSameSiteBetweenWebUIs) {
  set_should_create_webui(true);
  NavigateActiveAndCommit(GURL("chrome://foo"));

  RenderFrameHostManager* manager = contents()->GetRenderManagerForTesting();
  RenderFrameHostImpl* host = manager->current_frame_host();
  EXPECT_TRUE(host->IsRenderFrameLive());
  WebUIImpl* web_ui = host->web_ui();
  EXPECT_TRUE(web_ui);

  // Navigation request. No change in the returned WebUI type.
  const GURL kUrl("chrome://foo/bar");
  NavigationEntryImpl entry(
      nullptr /* instance */, kUrl, Referrer(), base::string16() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */);
  FrameNavigationEntry* frame_entry = entry.root_node()->frame_entry.get();
  FrameTreeNode* frame_tree_node =
      manager->current_frame_host()->frame_tree_node();
  CommonNavigationParams common_params = entry.ConstructCommonNavigationParams(
      *frame_entry, nullptr, frame_entry->url(), frame_entry->referrer(),
      FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT, PREVIEWS_UNSPECIFIED,
      base::TimeTicks::Now(), base::TimeTicks::Now());
  RequestNavigationParams request_params =
      entry.ConstructRequestNavigationParams(
          *frame_entry, common_params.url, common_params.method, false,
          entry.GetSubframeUniqueNames(frame_tree_node),
          controller().GetPendingEntryIndex() == -1 /* intended_as_new_entry */,
          static_cast<NavigationControllerImpl&>(controller())
              .GetIndexOfEntry(&entry),
          controller().GetLastCommittedEntryIndex(),
          controller().GetEntryCount());

  std::unique_ptr<NavigationRequest> navigation_request =
      NavigationRequest::CreateBrowserInitiated(
          frame_tree_node, common_params, request_params,
          !entry.is_renderer_initiated(), entry.extra_headers(), *frame_entry,
          entry, nullptr /* request_body */, nullptr /* navigation_ui_data */);
  manager->DidCreateNavigationRequest(navigation_request.get());

  // The current WebUI should still be in place and the pending WebUI should be
  // set to reuse it.
  EXPECT_EQ(web_ui, manager->GetNavigatingWebUI());
  EXPECT_EQ(web_ui, host->web_ui());
  EXPECT_EQ(web_ui, host->pending_web_ui());
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // Prepare to commit, update the navigating RenderFrameHost.
  EXPECT_EQ(host, manager->GetFrameHostForNavigation(*navigation_request));

  EXPECT_EQ(web_ui, manager->GetNavigatingWebUI());
  EXPECT_EQ(web_ui, host->web_ui());
  EXPECT_EQ(web_ui, host->pending_web_ui());
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // The RenderFrameHost committed.
  manager->DidNavigateFrame(host, true /* was_caused_by_user_gesture */,
                            false /* is_same_document_navigation */);
  EXPECT_EQ(web_ui, host->web_ui());
  EXPECT_FALSE(manager->GetNavigatingWebUI());
  EXPECT_FALSE(host->pending_web_ui());
}

// PlzNavigate: Tests that the correct intermediary and final navigation states
// are reached when navigating cross-site between two different WebUI types.
TEST_F(RenderFrameHostManagerTestWithBrowserSideNavigation,
       NavigateCrossSiteBetweenWebUIs) {
  // Cross-site navigations will always cause the change of the WebUI instance
  // but for consistency sake different types will be set for each navigation.
  set_should_create_webui(true);
  set_webui_type(1);
  NavigateActiveAndCommit(GURL("chrome://foo"));

  RenderFrameHostManager* manager = contents()->GetRenderManagerForTesting();
  RenderFrameHostImpl* host = manager->current_frame_host();
  EXPECT_TRUE(host->IsRenderFrameLive());
  EXPECT_TRUE(host->web_ui());

  // Set the WebUI controller to return a different WebUIType value. This will
  // cause the next navigation to "chrome://bar" to require a different WebUI
  // than the current one, forcing it to be treated as cross-site.
  set_webui_type(2);

  // Navigation request.
  const GURL kUrl("chrome://bar");
  NavigationEntryImpl entry(
      nullptr /* instance */, kUrl, Referrer(), base::string16() /* title */,
      ui::PAGE_TRANSITION_TYPED, false /* is_renderer_init */,
      nullptr /* blob_url_loader_factory */);
  FrameNavigationEntry* frame_entry = entry.root_node()->frame_entry.get();
  FrameTreeNode* frame_tree_node =
      manager->current_frame_host()->frame_tree_node();
  CommonNavigationParams common_params = entry.ConstructCommonNavigationParams(
      *frame_entry, nullptr, frame_entry->url(), frame_entry->referrer(),
      FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT, PREVIEWS_UNSPECIFIED,
      base::TimeTicks::Now(), base::TimeTicks::Now());
  RequestNavigationParams request_params =
      entry.ConstructRequestNavigationParams(
          *frame_entry, common_params.url, common_params.method, false,
          entry.GetSubframeUniqueNames(frame_tree_node),
          controller().GetPendingEntryIndex() == -1 /* intended_as_new_entry */,
          static_cast<NavigationControllerImpl&>(controller())
              .GetIndexOfEntry(&entry),
          controller().GetLastCommittedEntryIndex(),
          controller().GetEntryCount());

  std::unique_ptr<NavigationRequest> navigation_request =
      NavigationRequest::CreateBrowserInitiated(
          frame_tree_node, common_params, request_params,
          !entry.is_renderer_initiated(), entry.extra_headers(), *frame_entry,
          entry, nullptr /* request_body */, nullptr /* navigation_ui_data */);
  manager->DidCreateNavigationRequest(navigation_request.get());

  // The current WebUI should still be in place and there should be a new
  // active WebUI instance in the speculative RenderFrameHost.
  EXPECT_TRUE(manager->current_frame_host()->web_ui());
  EXPECT_FALSE(manager->current_frame_host()->pending_web_ui());
  RenderFrameHostImpl* speculative_host = GetPendingFrameHost(manager);
  EXPECT_TRUE(speculative_host);
  WebUIImpl* next_web_ui = manager->GetNavigatingWebUI();
  EXPECT_TRUE(next_web_ui);
  EXPECT_EQ(next_web_ui, speculative_host->web_ui());
  EXPECT_NE(next_web_ui, manager->current_frame_host()->web_ui());
  EXPECT_FALSE(speculative_host->pending_web_ui());

  // Prepare to commit, update the navigating RenderFrameHost.
  EXPECT_EQ(speculative_host,
            manager->GetFrameHostForNavigation(*navigation_request));

  EXPECT_TRUE(manager->current_frame_host()->web_ui());
  EXPECT_FALSE(manager->current_frame_host()->pending_web_ui());
  EXPECT_EQ(speculative_host, GetPendingFrameHost(manager));
  EXPECT_NE(next_web_ui, manager->current_frame_host()->web_ui());
  EXPECT_EQ(next_web_ui, speculative_host->web_ui());
  EXPECT_EQ(next_web_ui, manager->GetNavigatingWebUI());
  EXPECT_FALSE(speculative_host->pending_web_ui());

  // The RenderFrameHost committed.
  manager->DidNavigateFrame(speculative_host,
                            true /* was_caused_by_user_gesture */,
                            false /* is_same_document_navigation */);
  EXPECT_EQ(speculative_host, manager->current_frame_host());
  EXPECT_EQ(next_web_ui, manager->current_frame_host()->web_ui());
  EXPECT_FALSE(GetPendingFrameHost(manager));
  EXPECT_FALSE(speculative_host->pending_web_ui());
  EXPECT_FALSE(manager->GetNavigatingWebUI());
}

// Tests that frame proxies receive updates when a frame's enforcement
// of insecure request policy changes.
TEST_F(RenderFrameHostManagerTestWithSiteIsolation,
       ProxiesReceiveInsecureRequestPolicy) {
  const GURL kUrl1("http://www.google.test");
  const GURL kUrl2("http://www.google2.test");
  const GURL kUrl3("http://www.google2.test/foo");

  contents()->NavigateAndCommit(kUrl1);

  // Create a child frame and navigate it cross-site.
  main_test_rfh()->OnCreateChildFrame(
      main_test_rfh()->GetProcess()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
      blink::WebTreeScopeType::kDocument, "frame1", "uniqueName1", false,
      base::UnguessableToken::Create(), blink::FramePolicy(),
      FrameOwnerProperties(), blink::FrameOwnerElementType::kIframe);

  FrameTreeNode* root = contents()->GetFrameTree()->root();
  RenderFrameHostManager* child = root->child_at(0)->render_manager();

  // Navigate subframe to kUrl2.
  NavigationEntryImpl entry1(
      nullptr /* instance */, kUrl2,
      Referrer(kUrl1, network::mojom::ReferrerPolicy::kDefault),
      base::string16() /* title */, ui::PAGE_TRANSITION_LINK,
      false /* is_renderer_init */, nullptr /* blob_url_loader_factory */);
  TestRenderFrameHost* child_host =
      static_cast<TestRenderFrameHost*>(NavigateToEntry(child, entry1));
  child->DidNavigateFrame(child_host, true /* was_caused_by_user_gesture */,
                          false /* is_same_document_navigation */);

  // Verify that parent and child are in different processes.
  EXPECT_NE(child_host->GetProcess(), main_test_rfh()->GetProcess());

  // Change the parent's enforcement of strict mixed content checking,
  // and check that the correct IPC is sent to the child frame's
  // process.
  EXPECT_EQ(blink::kLeaveInsecureRequestsAlone,
            root->current_replication_state().insecure_request_policy);
  main_test_rfh()->DidEnforceInsecureRequestPolicy(
      blink::kBlockAllMixedContent);
  RenderFrameProxyHost* proxy_to_child =
      root->render_manager()->GetRenderFrameProxyHost(
          child_host->GetSiteInstance());
  EXPECT_NO_FATAL_FAILURE(
      CheckInsecureRequestPolicyIPC(child_host, blink::kBlockAllMixedContent,
                                    proxy_to_child->GetRoutingID()));
  EXPECT_EQ(blink::kBlockAllMixedContent,
            root->current_replication_state().insecure_request_policy);

  // Do the same for the child's enforcement. In general, the parent
  // needs to know the status of the child's flag in case a grandchild
  // is created: if A.com embeds B.com, and B.com enforces strict mixed
  // content checking, and B.com adds an iframe to A.com, then the
  // A.com process needs to know B.com's flag so that the grandchild
  // A.com frame can inherit it.
  EXPECT_EQ(
      blink::kLeaveInsecureRequestsAlone,
      root->child_at(0)->current_replication_state().insecure_request_policy);
  child_host->DidEnforceInsecureRequestPolicy(blink::kBlockAllMixedContent);
  RenderFrameProxyHost* proxy_to_parent =
      child->GetRenderFrameProxyHost(main_test_rfh()->GetSiteInstance());
  EXPECT_NO_FATAL_FAILURE(CheckInsecureRequestPolicyIPC(
      main_test_rfh(), blink::kBlockAllMixedContent,
      proxy_to_parent->GetRoutingID()));
  EXPECT_EQ(
      blink::kBlockAllMixedContent,
      root->child_at(0)->current_replication_state().insecure_request_policy);

  // Check that the flag for the parent's proxy to the child is reset
  // when the child navigates.
  main_test_rfh()->GetProcess()->sink().ClearMessages();
  FrameHostMsg_DidCommitProvisionalLoad_Params commit_params;
  commit_params.nav_entry_id = 0;
  commit_params.did_create_new_entry = false;
  commit_params.url = kUrl3;
  commit_params.transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
  commit_params.should_update_history = false;
  commit_params.gesture = NavigationGestureAuto;
  commit_params.method = "GET";
  commit_params.page_state = PageState::CreateFromURL(kUrl3);
  commit_params.insecure_request_policy = blink::kLeaveInsecureRequestsAlone;
  child_host->SendNavigateWithParams(&commit_params,
                                     false /* was_within_same_document */);
  EXPECT_NO_FATAL_FAILURE(CheckInsecureRequestPolicyIPC(
      main_test_rfh(), blink::kLeaveInsecureRequestsAlone,
      proxy_to_parent->GetRoutingID()));
  EXPECT_EQ(
      blink::kLeaveInsecureRequestsAlone,
      root->child_at(0)->current_replication_state().insecure_request_policy);
}

// Tests that a BeginNavigation IPC from a no longer active RFH is ignored.
TEST_F(RenderFrameHostManagerTestWithBrowserSideNavigation,
       BeginNavigationIgnoredWhenNotActive) {
  const GURL kUrl1("http://www.google.com");
  const GURL kUrl2("http://www.chromium.org");
  const GURL kUrl3("http://foo.com");

  contents()->NavigateAndCommit(kUrl1);

  TestRenderFrameHost* initial_rfh = main_test_rfh();
  RenderViewHostDeletedObserver delete_observer(
      initial_rfh->GetRenderViewHost());

  // Navigate cross-site but don't simulate the swap out ACK. The initial RFH
  // should be pending delete.
  RenderFrameHostManager* manager =
      main_test_rfh()->frame_tree_node()->render_manager();
  auto navigation_to_kUrl2 =
      NavigationSimulator::CreateBrowserInitiated(kUrl2, contents());
  navigation_to_kUrl2->ReadyToCommit();
  static_cast<TestRenderFrameHost*>(manager->speculative_frame_host())
      ->SimulateNavigationCommit(kUrl2);
  EXPECT_NE(initial_rfh, main_test_rfh());
  ASSERT_FALSE(delete_observer.deleted());
  EXPECT_FALSE(initial_rfh->is_active());

  // The initial RFH receives a BeginNavigation IPC. The navigation should not
  // start.
  auto navigation_to_kUrl3 =
      NavigationSimulator::CreateRendererInitiated(kUrl3, initial_rfh);
  navigation_to_kUrl3->Start();
  EXPECT_FALSE(main_test_rfh()->frame_tree_node()->navigation_request());
}

// Tests that sandbox flags received after a navigation away has started do not
// affect the document being navigated to.
TEST_F(RenderFrameHostManagerTest, ReceivedFramePolicyAfterNavigationStarted) {
  const GURL kUrl1("http://www.google.com");
  const GURL kUrl2("http://www.chromium.org");

  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* initial_rfh = main_test_rfh();

  // The RFH should start out with an empty frame policy.
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            initial_rfh->frame_tree_node()->active_sandbox_flags());

  // Navigate cross-site but don't commit the navigation.
  auto navigation_to_kUrl2 =
      NavigationSimulator::CreateBrowserInitiated(kUrl2, contents());
  navigation_to_kUrl2->ReadyToCommit();

  // Now send the frame policy for the initial page.
  initial_rfh->SendFramePolicy(blink::WebSandboxFlags::kAll, {});
  // Verify that the policy landed in the frame tree.
  EXPECT_EQ(blink::WebSandboxFlags::kAll,
            initial_rfh->frame_tree_node()->active_sandbox_flags());

  // Commit the naviagation; the new frame should have a clear frame policy.
  navigation_to_kUrl2->Commit();
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            main_test_rfh()->frame_tree_node()->active_sandbox_flags());
}

// Check that after a navigation, the final SiteInstance has the correct
// original URL that was used to determine its site URL.
TEST_F(RenderFrameHostManagerTest,
       SiteInstanceOriginalURLIsPreservedAfterNavigation) {
  const GURL kFooUrl("https://foo.com");
  const GURL kOriginalUrl("https://original.com");
  const GURL kTranslatedUrl("https://translated.com");
  EffectiveURLContentBrowserClient modified_client(kOriginalUrl,
                                                   kTranslatedUrl);
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&modified_client);

  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kFooUrl);
  scoped_refptr<SiteInstanceImpl> initial_instance =
      main_test_rfh()->GetSiteInstance();
  EXPECT_EQ(kFooUrl, initial_instance->original_url());
  EXPECT_EQ(kFooUrl, initial_instance->GetSiteURL());

  // Simulate a browser-initiated navigation to an app URL, which should swap
  // processes and create a new SiteInstance in a new BrowsingInstance.
  // This new SiteInstance should have correct |original_url()| and site URL.
  // The site URL should include both the |original_url()|'s site and the
  // translated URL's site.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kOriginalUrl);
  EXPECT_NE(initial_instance.get(), main_test_rfh()->GetSiteInstance());
  EXPECT_FALSE(initial_instance->IsRelatedSiteInstance(
      main_test_rfh()->GetSiteInstance()));
  EXPECT_EQ(kOriginalUrl, main_test_rfh()->GetSiteInstance()->original_url());
  GURL expected_site_url(kTranslatedUrl.spec() + "#" + kOriginalUrl.spec());
  EXPECT_EQ(expected_site_url,
            main_test_rfh()->GetSiteInstance()->GetSiteURL());

  SetBrowserClientForTesting(regular_client);
}

}  // namespace content
