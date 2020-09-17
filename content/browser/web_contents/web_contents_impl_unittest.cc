// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/download/public/common/download_url_parameters.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/content_web_ui_controller_factory.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_messages.h"
#include "content/common/page_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/fake_local_frame.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/url_constants.h"

namespace content {
namespace {
class WebContentsImplTestBrowserClient : public TestContentBrowserClient {
 public:
  WebContentsImplTestBrowserClient()
      : original_browser_client_(SetBrowserClientForTesting(this)) {}

  ~WebContentsImplTestBrowserClient() override {
    SetBrowserClientForTesting(original_browser_client_);
  }

  bool ShouldAssignSiteForURL(const GURL& url) override {
    if (site_assignment_for_url_.find(url) != site_assignment_for_url_.end()) {
      return site_assignment_for_url_[url];
    }

    return true;
  }

  void set_assign_site_for_url(bool assign, const GURL& url) {
    DCHECK(url.is_valid());
    site_assignment_for_url_[url] = assign;
  }

 private:
  std::map<GURL, bool> site_assignment_for_url_;
  ContentBrowserClient* original_browser_client_;
};

class WebContentsImplTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    WebUIControllerFactory::RegisterFactory(
        ContentWebUIControllerFactory::GetInstance());

    if (AreDefaultSiteInstancesEnabled()) {
      // Isolate |isolated_cross_site_url()| so we can't get a default
      // SiteInstance for it.
      ChildProcessSecurityPolicyImpl::GetInstance()->AddIsolatedOrigins(
          {url::Origin::Create(isolated_cross_site_url())},
          ChildProcessSecurityPolicy::IsolatedOriginSource::TEST,
          browser_context());

      // Reset the WebContents so the isolated origin will be honored by
      // all BrowsingInstances used in the test.
      SetContents(CreateTestWebContents());
    }
  }

  void TearDown() override {
    WebUIControllerFactory::UnregisterFactoryForTesting(
        ContentWebUIControllerFactory::GetInstance());
    RenderViewHostImplTestHarness::TearDown();
  }

  bool has_audio_wake_lock() {
    return contents()
        ->media_web_contents_observer()
        ->has_audio_wake_lock_for_testing();
  }

  GURL isolated_cross_site_url() const {
    return GURL("http://isolated-cross-site.com");
  }
};

class TestWebContentsObserver : public WebContentsObserver {
 public:
  explicit TestWebContentsObserver(WebContents* contents)
      : WebContentsObserver(contents) {}
  ~TestWebContentsObserver() override {}

  void DidFinishLoad(RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    last_url_ = validated_url;
  }
  void DidFailLoad(RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override {
    last_url_ = validated_url;
  }

  void DidFirstVisuallyNonEmptyPaint() override {
    observed_did_first_visually_non_empty_paint_ = true;
    EXPECT_TRUE(web_contents()->CompletedFirstVisuallyNonEmptyPaint());
  }

  void DidChangeThemeColor() override { ++theme_color_change_calls_; }

  void DidChangeVerticalScrollDirection(
      viz::VerticalScrollDirection scroll_direction) override {
    last_vertical_scroll_direction_ = scroll_direction;
  }

  void OnIsConnectedToBluetoothDeviceChanged(
      bool is_connected_to_bluetooth_device) override {
    ++num_is_connected_to_bluetooth_device_changed_;
    last_is_connected_to_bluetooth_device_ = is_connected_to_bluetooth_device;
  }

  const GURL& last_url() const { return last_url_; }
  int theme_color_change_calls() const { return theme_color_change_calls_; }
  base::Optional<viz::VerticalScrollDirection> last_vertical_scroll_direction()
      const {
    return last_vertical_scroll_direction_;
  }
  bool observed_did_first_visually_non_empty_paint() const {
    return observed_did_first_visually_non_empty_paint_;
  }
  int num_is_connected_to_bluetooth_device_changed() const {
    return num_is_connected_to_bluetooth_device_changed_;
  }
  bool last_is_connected_to_bluetooth_device() const {
    return last_is_connected_to_bluetooth_device_;
  }

 private:
  GURL last_url_;
  int theme_color_change_calls_ = 0;
  base::Optional<viz::VerticalScrollDirection> last_vertical_scroll_direction_;
  bool observed_did_first_visually_non_empty_paint_ = false;
  int num_is_connected_to_bluetooth_device_changed_ = 0;
  bool last_is_connected_to_bluetooth_device_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestWebContentsObserver);
};

// Pretends to be a normal browser that receives toggles and transitions to/from
// a fullscreened state.
class FakeFullscreenDelegate : public WebContentsDelegate {
 public:
  FakeFullscreenDelegate() : fullscreened_contents_(nullptr) {}
  ~FakeFullscreenDelegate() override {}

  void EnterFullscreenModeForTab(
      RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override {
    fullscreened_contents_ = WebContents::FromRenderFrameHost(requesting_frame);
  }

  void ExitFullscreenModeForTab(WebContents* web_contents) override {
    fullscreened_contents_ = nullptr;
  }

  bool IsFullscreenForTabOrPending(const WebContents* web_contents) override {
    return fullscreened_contents_ && web_contents == fullscreened_contents_;
  }

 private:
  WebContents* fullscreened_contents_;

  DISALLOW_COPY_AND_ASSIGN(FakeFullscreenDelegate);
};

class FakeWebContentsDelegate : public WebContentsDelegate {
 public:
  FakeWebContentsDelegate() : loading_state_changed_was_called_(false) {}
  ~FakeWebContentsDelegate() override {}

  void LoadingStateChanged(WebContents* source,
                           bool to_different_document) override {
    loading_state_changed_was_called_ = true;
  }

  bool loading_state_changed_was_called() const {
    return loading_state_changed_was_called_;
  }

 private:
  bool loading_state_changed_was_called_;

  DISALLOW_COPY_AND_ASSIGN(FakeWebContentsDelegate);
};

}  // namespace

TEST_F(WebContentsImplTest, UpdateTitle) {
  FakeWebContentsDelegate fake_delegate;
  contents()->SetDelegate(&fake_delegate);

  NavigationControllerImpl& cont =
      static_cast<NavigationControllerImpl&>(controller());
  cont.LoadURL(GURL(url::kAboutBlankURL), Referrer(), ui::PAGE_TRANSITION_TYPED,
               std::string());

  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  InitNavigateParams(&params, 0, true, GURL(url::kAboutBlankURL),
                     ui::PAGE_TRANSITION_TYPED);

  main_test_rfh()->SendNavigateWithParams(&params,
                                          false /* was_within_same_document */);

  contents()->UpdateTitle(main_test_rfh(),
                          base::ASCIIToUTF16("    Lots O' Whitespace\n"),
                          base::i18n::LEFT_TO_RIGHT);
  // Make sure that title updates get stripped of whitespace.
  EXPECT_EQ(base::ASCIIToUTF16("Lots O' Whitespace"), contents()->GetTitle());
  EXPECT_FALSE(contents()->IsWaitingForResponse());
  EXPECT_TRUE(fake_delegate.loading_state_changed_was_called());

  contents()->SetDelegate(nullptr);
}

TEST_F(WebContentsImplTest, UpdateTitleBeforeFirstNavigation) {
  ASSERT_TRUE(controller().IsInitialNavigation());
  const base::string16 title = base::ASCIIToUTF16("Initial Entry Title");
  contents()->UpdateTitle(main_test_rfh(), title, base::i18n::LEFT_TO_RIGHT);
  EXPECT_EQ(title, contents()->GetTitle());
}

TEST_F(WebContentsImplTest, SetMainFrameMimeType) {
  ASSERT_TRUE(controller().IsInitialNavigation());
  std::string mime = "text/html";
  RenderViewHostImpl* rvh =
      static_cast<RenderViewHostImpl*>(main_test_rfh()->GetRenderViewHost());
  rvh->SetContentsMimeType(mime);
  EXPECT_EQ(mime, contents()->GetContentsMimeType());
}

TEST_F(WebContentsImplTest, DontUseTitleFromPendingEntry) {
  const GURL kGURL(GetWebUIURL("blah"));
  controller().LoadURL(
      kGURL, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(base::string16(), contents()->GetTitle());

  // Also test setting title while the first navigation is still pending.
  const base::string16 title = base::ASCIIToUTF16("Initial Entry Title");
  contents()->UpdateTitle(main_test_rfh(), title, base::i18n::LEFT_TO_RIGHT);
  EXPECT_EQ(title, contents()->GetTitle());
}

TEST_F(WebContentsImplTest, UseTitleFromPendingEntryIfSet) {
  const GURL kGURL(GetWebUIURL("blah"));
  const base::string16 title = base::ASCIIToUTF16("My Title");
  controller().LoadURL(
      kGURL, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());

  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_EQ(kGURL, entry->GetURL());
  entry->SetTitle(title);

  EXPECT_EQ(title, contents()->GetTitle());
}

// Stub out local frame mojo binding. Intercepts calls to EnableViewSourceMode
// and marks the message as received. This class attaches to the first
// RenderFrameHostImpl created.
class EnableViewSourceLocalFrame : public content::FakeLocalFrame,
                                   public WebContentsObserver {
 public:
  explicit EnableViewSourceLocalFrame(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void RenderFrameCreated(RenderFrameHost* render_frame_host) override {
    if (!initialized_) {
      initialized_ = true;
      Init(render_frame_host->GetRemoteAssociatedInterfaces());
    }
  }

  void EnableViewSourceMode() final { enabled_view_source_ = true; }

  bool IsViewSourceModeEnabled() const { return enabled_view_source_; }

 private:
  bool enabled_view_source_ = false;
  bool initialized_ = false;
};

// Browser initiated navigations to view-source URLs of WebUI pages should work.
TEST_F(WebContentsImplTest, DirectNavigationToViewSourceWebUI) {
  const GURL kGURL("view-source:" + GetWebUIURLString("blah/"));
  // NavigationControllerImpl rewrites view-source URLs, simulating that here.
  const GURL kRewrittenURL(GetWebUIURL("blah"));

  EnableViewSourceLocalFrame local_frame(contents());
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kGURL);

  // Did we get the expected message?
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(local_frame.IsViewSourceModeEnabled());

  // This is the virtual URL.
  EXPECT_EQ(
      kGURL,
      contents()->GetController().GetLastCommittedEntry()->GetVirtualURL());

  // The actual URL navigated to.
  EXPECT_EQ(kRewrittenURL,
            contents()->GetController().GetLastCommittedEntry()->GetURL());
}

// Test simple same-SiteInstance navigation.
TEST_F(WebContentsImplTest, SimpleNavigation) {
  TestRenderFrameHost* orig_rfh = main_test_rfh();
  SiteInstance* instance1 = contents()->GetSiteInstance();
  EXPECT_EQ(nullptr, contents()->GetPendingMainFrame());

  // Navigate until ready to commit.
  const GURL url("http://www.google.com");
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(url, contents());
  navigation->ReadyToCommit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(instance1, orig_rfh->GetSiteInstance());
  // Controller's pending entry will have a null site instance until we assign
  // it in Commit.
  EXPECT_EQ(
      nullptr,
      NavigationEntryImpl::FromNavigationEntry(controller().GetVisibleEntry())->
          site_instance());

  navigation->Commit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());
  EXPECT_EQ(instance1, orig_rfh->GetSiteInstance());
  // Controller's entry should now have the SiteInstance, or else we won't be
  // able to find it later.
  EXPECT_EQ(
      instance1,
      NavigationEntryImpl::FromNavigationEntry(controller().GetVisibleEntry())->
          site_instance());
}

// Test that we reject NavigateToEntry if the url is over kMaxURLChars.
TEST_F(WebContentsImplTest, NavigateToExcessivelyLongURL) {
  // Construct a URL that's kMaxURLChars + 1 long of all 'a's.
  const GURL url(std::string("http://example.org/").append(
      url::kMaxURLChars + 1, 'a'));

  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_GENERATED, std::string());
  EXPECT_EQ(nullptr, controller().GetPendingEntry());
}

// Test that we reject NavigateToEntry if the url is invalid.
TEST_F(WebContentsImplTest, NavigateToInvalidURL) {
  // Invalid URLs should not trigger a navigation.
  const GURL invalid_url("view-source:http://example.org/%00");
  controller().LoadURL(
      invalid_url, Referrer(), ui::PAGE_TRANSITION_GENERATED, std::string());
  EXPECT_EQ(nullptr, controller().GetPendingEntry());

  // Empty URLs are supported and should start a navigation.
  controller().LoadURL(
      GURL(), Referrer(), ui::PAGE_TRANSITION_GENERATED, std::string());
  EXPECT_NE(nullptr, controller().GetPendingEntry());
}

// Test that we reject NavigateToEntry if the url is a renderer debug URL
// inside a view-source: URL. This verifies that the navigation is not allowed
// to proceed after the view-source: URL rewriting logic has run.
TEST_F(WebContentsImplTest, NavigateToViewSourceRendererDebugURL) {
  const GURL renderer_debug_url(kChromeUIKillURL);
  const GURL view_source_debug_url("view-source:" + renderer_debug_url.spec());
  EXPECT_TRUE(IsRendererDebugURL(renderer_debug_url));
  EXPECT_FALSE(IsRendererDebugURL(view_source_debug_url));
  controller().LoadURL(view_source_debug_url, Referrer(),
                       ui::PAGE_TRANSITION_GENERATED, std::string());
  EXPECT_EQ(nullptr, controller().GetPendingEntry());
}

// Test that navigating across a site boundary creates a new RenderViewHost
// with a new SiteInstance.  Going back should do the same.
TEST_F(WebContentsImplTest, CrossSiteBoundaries) {
  // This test assumes no interaction with the back forward cache.
  // Similar coverage when BFCache is on can be found in
  // BackForwardCacheBrowserTest.NavigateBackForwardRepeatedly.
  contents()->GetController().GetBackForwardCache().DisableForTesting(
      BackForwardCache::TEST_ASSUMES_NO_CACHING);

  TestRenderFrameHost* orig_rfh = main_test_rfh();
  int orig_rvh_delete_count = 0;
  orig_rfh->GetRenderViewHost()->set_delete_counter(&orig_rvh_delete_count);
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);

  // Keep the number of active frames in orig_rfh's SiteInstance non-zero so
  // that orig_rfh doesn't get deleted when it gets swapped out.
  orig_rfh->GetSiteInstance()->IncrementActiveFrameCount();

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh->GetRenderViewHost(), contents()->GetRenderViewHost());
  EXPECT_EQ(url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url, contents()->GetVisibleURL());

  // Navigate to new site
  const GURL url2("http://www.yahoo.com");
  auto new_site_navigation =
      NavigationSimulator::CreateBrowserInitiated(url2, contents());
  new_site_navigation->ReadyToCommit();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url2, contents()->GetVisibleURL());
  TestRenderFrameHost* pending_rfh = contents()->GetPendingMainFrame();
  EXPECT_TRUE(pending_rfh->GetLastCommittedURL().is_empty());
  int pending_rvh_delete_count = 0;
  pending_rfh->GetRenderViewHost()->set_delete_counter(
      &pending_rvh_delete_count);

  // DidNavigate from the pending page.
  new_site_navigation->Commit();
  SiteInstance* instance2 = contents()->GetSiteInstance();

  // Keep the number of active frames in pending_rfh's SiteInstance
  // non-zero so that orig_rfh doesn't get deleted when it gets
  // swapped out.
  pending_rfh->GetSiteInstance()->IncrementActiveFrameCount();

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(pending_rfh, main_test_rfh());
  EXPECT_EQ(url2, contents()->GetLastCommittedURL());
  EXPECT_EQ(url2, contents()->GetVisibleURL());
  EXPECT_NE(instance1, instance2);
  EXPECT_EQ(nullptr, contents()->GetPendingMainFrame());
  // We keep a proxy for the original RFH's SiteInstance.
  EXPECT_TRUE(contents()->GetRenderManagerForTesting()->GetRenderFrameProxyHost(
      instance1));
  EXPECT_EQ(orig_rvh_delete_count, 0);

  // Going back should switch SiteInstances again.  The first SiteInstance is
  // stored in the NavigationEntry, so it should be the same as at the start.
  // We should use the same RFH as before, swapping it back in.
  auto back_navigation =
      NavigationSimulator::CreateHistoryNavigation(-1, contents());
  back_navigation->ReadyToCommit();
  TestRenderFrameHost* goback_rfh = contents()->GetPendingMainFrame();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());

  // DidNavigate from the back action.
  back_navigation->Commit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(goback_rfh, main_test_rfh());
  EXPECT_EQ(url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url, contents()->GetVisibleURL());
  EXPECT_EQ(instance1, contents()->GetSiteInstance());
  // There should be a proxy for the pending RFH SiteInstance.
  EXPECT_TRUE(contents()->GetRenderManagerForTesting()->GetRenderFrameProxyHost(
      instance2));
  EXPECT_EQ(pending_rvh_delete_count, 0);

  // Close contents and ensure RVHs are deleted.
  DeleteContents();
  EXPECT_EQ(orig_rvh_delete_count, 1);
  EXPECT_EQ(pending_rvh_delete_count, 1);
}

// Test that navigating across a site boundary after a crash creates a new
// RFH without requiring a cross-site transition (i.e., PENDING state).
TEST_F(WebContentsImplTest, CrossSiteBoundariesAfterCrash) {
  // Ensure that the cross-site transition will also be cross-process on
  // Android.
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  TestRenderFrameHost* orig_rfh = main_test_rfh();

  int orig_rvh_delete_count = 0;
  orig_rfh->GetRenderViewHost()->set_delete_counter(&orig_rvh_delete_count);
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh->GetRenderViewHost(), contents()->GetRenderViewHost());

  // Simulate a renderer crash.
  EXPECT_TRUE(orig_rfh->IsRenderFrameLive());
  orig_rfh->GetProcess()->SimulateCrash();
  EXPECT_FALSE(orig_rfh->IsRenderFrameLive());

  // Start navigating to a new site. We should not go into PENDING.
  const GURL url2("http://www.yahoo.com");
  auto navigation_to_url2 =
      NavigationSimulator::CreateBrowserInitiated(url2, contents());
  navigation_to_url2->ReadyToCommit();

  TestRenderFrameHost* new_rfh = main_test_rfh();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(nullptr, contents()->GetPendingMainFrame());
  EXPECT_NE(orig_rfh, new_rfh);
  EXPECT_EQ(orig_rvh_delete_count, 1);

  navigation_to_url2->Commit();
  SiteInstance* instance2 = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(new_rfh, main_rfh());
  EXPECT_NE(instance1, instance2);
  EXPECT_EQ(nullptr, contents()->GetPendingMainFrame());

  // Close contents and ensure RVHs are deleted.
  DeleteContents();
  EXPECT_EQ(orig_rvh_delete_count, 1);
}

// Test that opening a new contents in the same SiteInstance and then navigating
// both contentses to a new site will place both contentses in a single
// SiteInstance.
TEST_F(WebContentsImplTest, NavigateTwoTabsCrossSite) {
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);

  // Open a new contents with the same SiteInstance, navigated to the same site.
  std::unique_ptr<TestWebContents> contents2(
      TestWebContents::Create(browser_context(), instance1));
  NavigationSimulator::NavigateAndCommitFromBrowser(contents2.get(), url);
  EXPECT_EQ(instance1, contents2->GetSiteInstance());

  // Navigate first contents to a new site.
  const GURL url2a = isolated_cross_site_url();
  auto navigation1 =
      NavigationSimulator::CreateBrowserInitiated(url2a, contents());
  navigation1->SetTransition(ui::PAGE_TRANSITION_LINK);
  navigation1->ReadyToCommit();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  navigation1->Commit();
  SiteInstance* instance2a = contents()->GetSiteInstance();
  EXPECT_NE(instance1, instance2a);

  // Navigate second contents to the same site as the first tab.
  const GURL url2b = isolated_cross_site_url().Resolve("/foo");
  auto navigation2 =
      NavigationSimulator::CreateBrowserInitiated(url2b, contents2.get());
  navigation2->SetTransition(ui::PAGE_TRANSITION_LINK);
  navigation2->ReadyToCommit();
  EXPECT_TRUE(contents2->CrossProcessNavigationPending());

  // NOTE(creis): We used to be in danger of showing a crash page here if the
  // second contents hadn't navigated somewhere first (bug 1145430).  That case
  // is now covered by the CrossSiteBoundariesAfterCrash test.
  navigation2->Commit();
  SiteInstance* instance2b = contents2->GetSiteInstance();
  EXPECT_NE(instance1, instance2b);

  // Both contentses should now be in the same SiteInstance.
  EXPECT_EQ(instance2a, instance2b);
}

// The embedder can request sites for certain urls not be be assigned to the
// SiteInstance through ShouldAssignSiteForURL() in content browser client,
// allowing to reuse the renderer backing certain chrome urls for subsequent
// navigation. The test verifies that the override is honored.
TEST_F(WebContentsImplTest, NavigateFromSitelessUrl) {
  WebContentsImplTestBrowserClient browser_client;
  SetBrowserClientForTesting(&browser_client);

  TestRenderFrameHost* orig_rfh = main_test_rfh();
  int orig_rvh_delete_count = 0;
  orig_rfh->GetRenderViewHost()->set_delete_counter(&orig_rvh_delete_count);
  SiteInstanceImpl* orig_instance = contents()->GetSiteInstance();

  // Navigate to an URL that will not assign a new SiteInstance.
  const GURL native_url("non-site-url://stuffandthings");
  browser_client.set_assign_site_for_url(false, native_url);
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), native_url);

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());
  EXPECT_EQ(native_url, contents()->GetLastCommittedURL());
  EXPECT_EQ(native_url, contents()->GetVisibleURL());
  EXPECT_EQ(orig_instance, contents()->GetSiteInstance());
  EXPECT_EQ(GURL(), contents()->GetSiteInstance()->GetSiteURL());
  EXPECT_FALSE(orig_instance->HasSite());

  // Navigate to new site (should keep same site instance).
  const GURL url("http://www.google.com");
  browser_client.set_assign_site_for_url(true, url);
  auto navigation1 =
      NavigationSimulator::CreateBrowserInitiated(url, contents());
  navigation1->ReadyToCommit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(native_url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url, contents()->GetVisibleURL());
  EXPECT_FALSE(contents()->GetPendingMainFrame());
  navigation1->Commit();

  // The first entry's SiteInstance should be reset to a new, related one. This
  // prevents wrongly detecting a SiteInstance mismatch when returning to it
  // later.
  SiteInstanceImpl* prev_entry_instance = contents()
                                              ->GetController()
                                              .GetEntryAtIndex(0)
                                              ->root_node()
                                              ->frame_entry->site_instance();
  EXPECT_NE(prev_entry_instance, orig_instance);
  EXPECT_TRUE(orig_instance->IsRelatedSiteInstance(prev_entry_instance));
  EXPECT_FALSE(prev_entry_instance->HasSite());

  SiteInstanceImpl* curr_entry_instance = contents()
                                              ->GetController()
                                              .GetEntryAtIndex(1)
                                              ->root_node()
                                              ->frame_entry->site_instance();
  EXPECT_EQ(curr_entry_instance, orig_instance);
  // Keep the number of active frames in orig_rfh's SiteInstance
  // non-zero so that orig_rfh doesn't get deleted when it gets
  // swapped out.
  orig_rfh->GetSiteInstance()->IncrementActiveFrameCount();

  EXPECT_EQ(orig_instance, contents()->GetSiteInstance());
  if (AreDefaultSiteInstancesEnabled()) {
    // Verify that the empty SiteInstance gets converted into a default
    // SiteInstance because |url| does not require a dedicated process.
    EXPECT_TRUE(contents()->GetSiteInstance()->IsDefaultSiteInstance());
  } else {
    EXPECT_TRUE(
        contents()->GetSiteInstance()->GetSiteURL().DomainIs("google.com"));
  }
  EXPECT_EQ(url, contents()->GetLastCommittedURL());

  // Navigate to another new site (should create a new site instance).
  const GURL url2 = isolated_cross_site_url();
  auto navigation2 =
      NavigationSimulator::CreateBrowserInitiated(url2, contents());
  navigation2->ReadyToCommit();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url2, contents()->GetVisibleURL());
  TestRenderFrameHost* pending_rfh = contents()->GetPendingMainFrame();
  int pending_rvh_delete_count = 0;
  pending_rfh->GetRenderViewHost()->set_delete_counter(
      &pending_rvh_delete_count);

  // DidNavigate from the pending page.
  navigation2->Commit();
  SiteInstance* new_instance = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(pending_rfh, main_test_rfh());
  EXPECT_EQ(url2, contents()->GetLastCommittedURL());
  EXPECT_EQ(url2, contents()->GetVisibleURL());
  EXPECT_NE(new_instance, orig_instance);
  EXPECT_FALSE(contents()->GetPendingMainFrame());
  EXPECT_EQ(orig_rvh_delete_count, 0);

  // Close contents and ensure RVHs are deleted.
  DeleteContents();
  EXPECT_EQ(orig_rvh_delete_count, 1);
  EXPECT_EQ(pending_rvh_delete_count, 1);
}

// Regression test for http://crbug.com/386542 - variation of
// NavigateFromSitelessUrl in which the original navigation is a session
// restore.
TEST_F(WebContentsImplTest, NavigateFromRestoredSitelessUrl) {
  WebContentsImplTestBrowserClient browser_client;
  SetBrowserClientForTesting(&browser_client);
  SiteInstanceImpl* orig_instance = contents()->GetSiteInstance();
  TestRenderFrameHost* orig_rfh = main_test_rfh();

  // Restore a navigation entry for URL that should not assign site to the
  // SiteInstance.
  const GURL native_url("non-site-url://stuffandthings");
  browser_client.set_assign_site_for_url(false, native_url);
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  std::unique_ptr<NavigationEntry> new_entry =
      NavigationController::CreateNavigationEntry(
          native_url, Referrer(), base::nullopt, ui::PAGE_TRANSITION_LINK,
          false, std::string(), browser_context(),
          nullptr /* blob_url_loader_factory */);
  entries.push_back(std::move(new_entry));
  controller().Restore(0, RestoreType::LAST_SESSION_EXITED_CLEANLY, &entries);
  ASSERT_EQ(0u, entries.size());
  ASSERT_EQ(1, controller().GetEntryCount());

  EXPECT_TRUE(controller().NeedsReload());
  controller().LoadIfNecessary();
  NavigationEntry* entry = controller().GetPendingEntry();
  orig_rfh->PrepareForCommit();
  contents()->TestDidNavigate(orig_rfh, entry->GetUniqueID(), false,
                              native_url, ui::PAGE_TRANSITION_RELOAD);
  EXPECT_EQ(orig_instance, contents()->GetSiteInstance());
  EXPECT_EQ(GURL(), contents()->GetSiteInstance()->GetSiteURL());
  EXPECT_FALSE(orig_instance->HasSite());

  // Navigate to a regular site and verify that the SiteInstance was kept.
  const GURL url("http://www.google.com");
  browser_client.set_assign_site_for_url(true, url);
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);
  EXPECT_EQ(orig_instance, contents()->GetSiteInstance());

  // Cleanup.
  DeleteContents();
}

// Complement for NavigateFromRestoredSitelessUrl, verifying that when a regular
// tab is restored, the SiteInstance will change upon navigation.
TEST_F(WebContentsImplTest, NavigateFromRestoredRegularUrl) {
  WebContentsImplTestBrowserClient browser_client;
  SetBrowserClientForTesting(&browser_client);
  SiteInstanceImpl* orig_instance = contents()->GetSiteInstance();
  TestRenderFrameHost* orig_rfh = main_test_rfh();

  // Restore a navigation entry for a regular URL ensuring that the embedder
  // ShouldAssignSiteForUrl override is disabled (i.e. returns true).
  const GURL regular_url("http://www.yahoo.com");
  browser_client.set_assign_site_for_url(true, regular_url);
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  std::unique_ptr<NavigationEntry> new_entry =
      NavigationController::CreateNavigationEntry(
          regular_url, Referrer(), base::nullopt, ui::PAGE_TRANSITION_LINK,
          false, std::string(), browser_context(),
          nullptr /* blob_url_loader_factory */);
  entries.push_back(std::move(new_entry));
  controller().Restore(0, RestoreType::LAST_SESSION_EXITED_CLEANLY, &entries);
  ASSERT_EQ(0u, entries.size());

  ASSERT_EQ(1, controller().GetEntryCount());
  EXPECT_TRUE(controller().NeedsReload());
  controller().LoadIfNecessary();
  NavigationEntry* entry = controller().GetPendingEntry();
  orig_rfh->PrepareForCommit();
  contents()->TestDidNavigate(orig_rfh, entry->GetUniqueID(), false,
                              regular_url, ui::PAGE_TRANSITION_RELOAD);
  EXPECT_EQ(orig_instance, contents()->GetSiteInstance());
  EXPECT_TRUE(orig_instance->HasSite());
  EXPECT_EQ(AreDefaultSiteInstancesEnabled(),
            orig_instance->IsDefaultSiteInstance());

  // Navigate to another site and verify that a new SiteInstance was created.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);
  if (AreDefaultSiteInstancesEnabled()) {
    // Verify this remains the default SiteInstance since |url| does
    // not require a dedicated process.
    EXPECT_EQ(orig_instance, contents()->GetSiteInstance());

    // Navigate to a URL that does require a dedicated process and verify that
    // the SiteInstance changes.
    NavigationSimulator::NavigateAndCommitFromBrowser(
        contents(), isolated_cross_site_url());
    EXPECT_NE(orig_instance, contents()->GetSiteInstance());
  } else {
    EXPECT_NE(orig_instance, contents()->GetSiteInstance());
  }

  // Cleanup.
  DeleteContents();
}

// Test that we can find an opener RVH even if it's pending.
// http://crbug.com/176252.
TEST_F(WebContentsImplTest, FindOpenerRVHWhenPending) {

  // Navigate to a URL.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);

  // Start to navigate first tab to a new site, so that it has a pending RVH.
  const GURL url2("http://www.yahoo.com");
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(url2, contents());
  navigation->ReadyToCommit();
  TestRenderFrameHost* pending_rfh = contents()->GetPendingMainFrame();
  SiteInstance* instance = pending_rfh->GetSiteInstance();

  // While it is still pending, simulate opening a new tab with the first tab
  // as its opener.  This will call CreateOpenerProxies on the opener to ensure
  // that an RVH exists.
  std::unique_ptr<TestWebContents> popup(
      TestWebContents::Create(browser_context(), instance));
  popup->SetOpener(contents());
  contents()->GetRenderManager()->CreateOpenerProxies(instance, nullptr);

  // If swapped out is forbidden, a new proxy should be created for the opener
  // in |instance|, and we should ensure that its routing ID is returned here.
  // Otherwise, we should find the pending RFH and not create a new proxy.
  auto opener_frame_token =
      popup->GetRenderManager()->GetOpenerFrameToken(instance);
  RenderFrameProxyHost* proxy =
      contents()->GetRenderManager()->GetRenderFrameProxyHost(instance);
  EXPECT_TRUE(proxy);
  EXPECT_EQ(*opener_frame_token, proxy->GetFrameToken());

  // Ensure that committing the navigation removes the proxy.
  navigation->Commit();
  EXPECT_FALSE(
      contents()->GetRenderManager()->GetRenderFrameProxyHost(instance));
}

// Tests that WebContentsImpl uses the current URL, not the SiteInstance's site,
// to determine whether a navigation is cross-site.
TEST_F(WebContentsImplTest, CrossSiteComparesAgainstCurrentPage) {
  // The assumptions this test makes aren't valid with --site-per-process.  For
  // example, a cross-site URL won't ever commit in the old RFH.  The test also
  // assumes that default SiteInstances are enabled, and that aggressive
  // BrowsingInstance swapping (even on renderer-initiated navigations) is
  // disabled.
  if (AreAllSitesIsolatedForTesting() || !AreDefaultSiteInstancesEnabled() ||
      CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
    return;
  }

  TestRenderFrameHost* orig_rfh = main_test_rfh();
  SiteInstanceImpl* instance1 = contents()->GetSiteInstance();

  const GURL url("http://www.google.com");

  // Navigate to URL.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);

  // Open a related contents to a second site.
  std::unique_ptr<TestWebContents> contents2(
      TestWebContents::Create(browser_context(), instance1));
  const GURL url2("http://www.yahoo.com");
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(url2, contents2.get());
  navigation->ReadyToCommit();

  // The first RVH in contents2 isn't live yet, so we shortcut the cross site
  // pending.
  EXPECT_FALSE(contents2->CrossProcessNavigationPending());
  navigation->Commit();
  SiteInstance* instance2 = contents2->GetSiteInstance();
  // With default SiteInstances, navigations in both tabs should
  // share the same default SiteInstance, since neither requires a dedicated
  // process.
  EXPECT_EQ(instance1, instance2);
  EXPECT_TRUE(instance1->IsDefaultSiteInstance());
  EXPECT_FALSE(contents2->CrossProcessNavigationPending());

  // Simulate a link click in first contents to second site.  This doesn't
  // switch SiteInstances and stays in the default SiteInstance.
  NavigationSimulator::NavigateAndCommitFromDocument(url2, orig_rfh);
  SiteInstance* instance3 = contents()->GetSiteInstance();
  EXPECT_EQ(instance1, instance3);
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());

  // Navigate same-site.  This also stays in the default SiteInstance.
  const GURL url3("http://mail.yahoo.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url3);
  SiteInstance* instance4 = contents()->GetSiteInstance();
  EXPECT_EQ(instance1, instance4);
}

// Test that the onbeforeunload and onunload handlers run when navigating
// across site boundaries.
TEST_F(WebContentsImplTest, CrossSiteUnloadHandlers) {
  TestRenderFrameHost* orig_rfh = main_test_rfh();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());

  // Navigate to new site, but simulate an onbeforeunload denial.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(orig_rfh->is_waiting_for_beforeunload_completion());
  orig_rfh->SimulateBeforeUnloadCompleted(false);
  EXPECT_FALSE(orig_rfh->is_waiting_for_beforeunload_completion());
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());

  // Navigate again, but simulate an onbeforeunload approval.
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(orig_rfh->is_waiting_for_beforeunload_completion());
  auto navigation = NavigationSimulator::CreateFromPending(contents());
  navigation->ReadyToCommit();
  EXPECT_FALSE(orig_rfh->is_waiting_for_beforeunload_completion());
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  TestRenderFrameHost* pending_rfh = contents()->GetPendingMainFrame();

  // DidNavigate from the pending page.
  navigation->Commit();
  SiteInstance* instance2 = contents()->GetSiteInstance();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(pending_rfh, main_test_rfh());
  EXPECT_NE(instance1, instance2);
  EXPECT_EQ(nullptr, contents()->GetPendingMainFrame());
}

// Test that during a slow cross-site navigation, the original renderer can
// navigate to a different URL and have it displayed, canceling the slow
// navigation.
TEST_F(WebContentsImplTest, CrossSiteNavigationPreempted) {
  TestRenderFrameHost* orig_rfh = main_test_rfh();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderFrameHost.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());

  // Navigate to new site.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(orig_rfh->is_waiting_for_beforeunload_completion());
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());

  // Suppose the original renderer navigates before the new one is ready.
  const GURL url3("http://www.google.com/foo");
  NavigationSimulator::NavigateAndCommitFromDocument(url3, orig_rfh);

  // Verify that the pending navigation is cancelled.
  SiteInstance* instance2 = contents()->GetSiteInstance();
  if (CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    // If same-site ProactivelySwapBrowsingInstance or main-frame RenderDocument
    // is enabled, the RFH should change.
    EXPECT_NE(orig_rfh, main_test_rfh());
  } else {
    EXPECT_EQ(orig_rfh, main_test_rfh());
  }
  if (CanSameSiteMainFrameNavigationsChangeSiteInstances()) {
    // When ProactivelySwapBrowsingInstance is enabled on same-site navigations,
    // the SiteInstance will change.
    EXPECT_NE(instance1, instance2);
  } else {
    EXPECT_EQ(instance1, instance2);
  }
  EXPECT_FALSE(main_test_rfh()->is_waiting_for_beforeunload_completion());
  EXPECT_EQ(main_test_rfh()->GetLastCommittedURL(), url3);
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(nullptr, contents()->GetPendingMainFrame());
}

// Tests that if we go back twice (same-site then cross-site), and the same-site
// RFH commits first, the cross-site RFH's navigation is canceled. If the
// same-site navigation is a cross-RFH navigation, however, the same-site
// navigation will get canceled instead and we are left with the newer
// cross-site navigation.
// TODO(avi,creis): Consider changing this behavior to better match the user's
// intent.
TEST_F(WebContentsImplTest, CrossSiteNavigationBackPreempted) {
  // Start with a web ui page, which gets a new RVH with WebUI bindings.
  GURL url1(std::string(kChromeUIScheme) + "://" +
            std::string(kChromeUIGpuHost));
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url1);
  TestRenderFrameHost* webui_rfh = main_test_rfh();
  NavigationEntry* entry1 = controller().GetLastCommittedEntry();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(url1, entry1->GetURL());
  EXPECT_EQ(instance1,
            NavigationEntryImpl::FromNavigationEntry(entry1)->site_instance());
  EXPECT_TRUE(webui_rfh->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);

  // Navigate to new site.
  const GURL url2("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url2);
  TestRenderFrameHost* google_rfh = main_test_rfh();
  NavigationEntry* entry2 = controller().GetLastCommittedEntry();
  SiteInstance* instance2 = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_NE(instance1, instance2);
  EXPECT_FALSE(contents()->GetPendingMainFrame());
  EXPECT_EQ(url2, entry2->GetURL());
  EXPECT_EQ(instance2,
            NavigationEntryImpl::FromNavigationEntry(entry2)->site_instance());
  EXPECT_FALSE(google_rfh->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);

  // Navigate to third page on same site.
  const GURL url3("http://news.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url3);
  NavigationEntry* entry3 = controller().GetLastCommittedEntry();
  SiteInstance* instance3 = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  if (CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    // If same-site ProactivelySwapBrowsingInstance or main-frame RenderDocument
    // is enabled, the RFH should change.
    EXPECT_NE(google_rfh, main_test_rfh());
  } else {
    EXPECT_EQ(google_rfh, main_test_rfh());
  }
  if (CanSameSiteMainFrameNavigationsChangeSiteInstances()) {
    // When ProactivelySwapBrowsingInstance is enabled on same-site navigations,
    // the SiteInstance will change.
    EXPECT_NE(instance2, instance3);
  } else {
    EXPECT_EQ(instance2, instance3);
  }
  EXPECT_FALSE(contents()->GetPendingMainFrame());
  EXPECT_EQ(url3, entry3->GetURL());
  EXPECT_EQ(instance3,
            NavigationEntryImpl::FromNavigationEntry(entry3)->site_instance());

  // Go back within the site.
  auto back_navigation1 =
      NavigationSimulatorImpl::CreateHistoryNavigation(-1, contents());
  back_navigation1->Start();

  auto* first_pending_rfh = contents()->GetPendingMainFrame();
  GlobalFrameRoutingId first_pending_rfh_id;
  if (CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    EXPECT_TRUE(contents()->CrossProcessNavigationPending());
    EXPECT_TRUE(first_pending_rfh);
    first_pending_rfh_id = first_pending_rfh->GetGlobalFrameRoutingId();
  } else {
    EXPECT_FALSE(contents()->CrossProcessNavigationPending());
    EXPECT_FALSE(first_pending_rfh);
  }
  EXPECT_EQ(entry2, controller().GetPendingEntry());

  // Before that commits, go back again.
  back_navigation1->ReadyToCommit();
  auto back_navigation2 =
      NavigationSimulatorImpl::CreateHistoryNavigation(-1, contents());
  back_navigation2->Start();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_TRUE(contents()->GetPendingMainFrame());
  EXPECT_EQ(entry1, controller().GetPendingEntry());
  if (CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    // When ProactivelySwapBrowsingInstance or RenderDocument is enabled on
    // same-site main frame navigation, the first back navigation will create a
    // speculative RFH even though it's a same-site navigation, and the
    // speculative RFH will be overwritten by the second back-navigation that
    // will also create a speculative RFH.
    EXPECT_NE(first_pending_rfh_id,
              contents()->GetPendingMainFrame()->GetGlobalFrameRoutingId());
    // Calling Commit() on the first back navigation below will cause a DCHECK
    // failure because we've already called DidFinishNavigaition on it, so we
    // will call it on the second back navigation instead.
    back_navigation2->Commit();
  } else {
    // DidNavigate from the first back. This aborts the second back's
    // speculative RFH.
    back_navigation1->Commit();
  }

  // We have committed this navigation and forgot about the second back if
  // CanSameSiteMainFrameNavigationsChangeRenderFrameHosts() is false, or the
  // first back if it's true.
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_FALSE(controller().GetPendingEntry());
  EXPECT_EQ(google_rfh, main_test_rfh());
  if (CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    // We committed the second back navigation and landed on the first page.
    EXPECT_EQ(url1, controller().GetLastCommittedEntry()->GetURL());
  } else {
    // We committed the second back navigation and landed on the second page.
    EXPECT_EQ(url2, controller().GetLastCommittedEntry()->GetURL());
  }

  // We should not have corrupted the NTP entry.
  EXPECT_EQ(instance3,
            NavigationEntryImpl::FromNavigationEntry(entry3)->site_instance());
  EXPECT_EQ(instance2,
            NavigationEntryImpl::FromNavigationEntry(entry2)->site_instance());
  EXPECT_EQ(instance1,
            NavigationEntryImpl::FromNavigationEntry(entry1)->site_instance());
  EXPECT_EQ(url1, entry1->GetURL());
}

// Tests that if we go back twice (same-site then cross-site), and the cross-
// site RFH commits first, we ignore the now-swapped-out RFH's commit.
TEST_F(WebContentsImplTest, CrossSiteNavigationBackOldNavigationIgnored) {
  // Start with a web ui page, which gets a new RFH with WebUI bindings.
  GURL url1(std::string(kChromeUIScheme) + "://" +
            std::string(kChromeUIGpuHost));
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url1);
  TestRenderFrameHost* webui_rfh = main_test_rfh();
  NavigationEntry* entry1 = controller().GetLastCommittedEntry();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(url1, entry1->GetURL());
  EXPECT_EQ(instance1,
            NavigationEntryImpl::FromNavigationEntry(entry1)->site_instance());
  EXPECT_TRUE(webui_rfh->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);

  // Navigate to new site.
  const GURL url2("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url2);
  TestRenderFrameHost* google_rfh = main_test_rfh();
  NavigationEntry* entry2 = controller().GetLastCommittedEntry();
  SiteInstance* instance2 = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_NE(instance1, instance2);
  EXPECT_FALSE(contents()->GetPendingMainFrame());
  EXPECT_EQ(url2, entry2->GetURL());
  EXPECT_EQ(instance2,
            NavigationEntryImpl::FromNavigationEntry(entry2)->site_instance());
  EXPECT_FALSE(google_rfh->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);

  // Navigate to third page on same site.
  const GURL url3("http://google.com/foo");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url3);
  NavigationEntry* entry3 = controller().GetLastCommittedEntry();
  SiteInstance* instance3 = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  if (CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    // If same-site ProactivelySwapBrowsingInstance or main-frame RenderDocument
    // is enabled, the RFH should change.
    EXPECT_NE(google_rfh, main_test_rfh());
    google_rfh = main_test_rfh();
  } else {
    EXPECT_EQ(google_rfh, main_test_rfh());
  }
  if (CanSameSiteMainFrameNavigationsChangeSiteInstances()) {
    // When ProactivelySwapBrowsingInstance is enabled on same-site navigations,
    // the SiteInstance will change.
    EXPECT_NE(instance2, instance3);
  } else {
    EXPECT_EQ(instance2, instance3);
  }
  EXPECT_FALSE(contents()->GetPendingMainFrame());
  EXPECT_EQ(url3, entry3->GetURL());
  EXPECT_EQ(instance3,
            NavigationEntryImpl::FromNavigationEntry(entry3)->site_instance());

  // Go back within the site.
  auto back_navigation1 =
      NavigationSimulator::CreateHistoryNavigation(-1, contents());
  back_navigation1->ReadyToCommit();
  if (CanSameSiteMainFrameNavigationsChangeSiteInstances()) {
    EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  } else {
    EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  }
  EXPECT_EQ(entry2, controller().GetPendingEntry());

  // Before that commits, go back again.
  auto back_navigation2 =
      NavigationSimulatorImpl::CreateHistoryNavigation(-1, contents());
  back_navigation2->set_drop_unload_ack(true);
  back_navigation2->ReadyToCommit();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_TRUE(contents()->GetPendingMainFrame());
  EXPECT_EQ(entry1, controller().GetPendingEntry());
  webui_rfh = contents()->GetPendingMainFrame();

  // DidNavigate from the second back.
  // Note that the process in instance1 is gone at this point, but we will still
  // use instance1 and entry1 because IsSuitableForURL will return true when
  // there is no process and the site URL matches.
  back_navigation2->Commit();

  // That should have landed us on the first entry.
  EXPECT_EQ(entry1, controller().GetLastCommittedEntry());

  // When the second back commits, it should be ignored.
  contents()->TestDidNavigate(google_rfh, entry2->GetUniqueID(), false, url2,
                              ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(entry1, controller().GetLastCommittedEntry());

  // The newly created process for url1 should be locked to chrome://gpu.
  RenderProcessHost* new_process = contents()->GetMainFrame()->GetProcess();
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  EXPECT_TRUE(policy->CanAccessDataForOrigin(new_process->GetID(), url1));
  EXPECT_FALSE(policy->CanAccessDataForOrigin(new_process->GetID(), url2));
}

// Test that during a slow cross-site navigation, a sub-frame navigation in the
// original renderer will not cancel the slow navigation (bug 42029).
TEST_F(WebContentsImplTest, CrossSiteNavigationNotPreemptedByFrame) {
  TestRenderFrameHost* orig_rfh = main_test_rfh();

  // Navigate to URL.  First URL should use the original RenderFrameHost.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());

  // Start navigating to new site.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());

  // Simulate a sub-frame navigation arriving and ensure the RVH is still
  // waiting for a before unload response.
  TestRenderFrameHost* child_rfh = orig_rfh->AppendChild("subframe");
  child_rfh->SendNavigateWithTransition(0, false,
                                        GURL("http://google.com/frame"),
                                        ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  EXPECT_TRUE(orig_rfh->is_waiting_for_beforeunload_completion());

  // Now simulate the onbeforeunload approval and verify the navigation is
  // not canceled.
  orig_rfh->PrepareForCommit();
  EXPECT_FALSE(orig_rfh->is_waiting_for_beforeunload_completion());
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
}

// Test that a cross-site navigation is not preempted if the previous
// renderer sends a FrameNavigate message just before being told to stop.
// We should only preempt the cross-site navigation if the previous renderer
// has started a new navigation. See http://crbug.com/79176.
TEST_F(WebContentsImplTest, CrossSiteNotPreemptedDuringBeforeUnload) {
  const GURL kUrl("http://foo");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kUrl);

  // First, make a non-user initiated same-site navigation.
  const GURL kSameSiteUrl("http://foo/1");
  TestRenderFrameHost* orig_rfh = main_test_rfh();
  // When ProactivelySwapBrowsingInstance or RenderDocument is enabled on
  // same-site main frame navigations, the same-site navigation below will
  // create a speculative RFH that will be overwritten when the cross-site
  // navigation starts, finishing the same-site navigation, so the scenario in
  // this test cannot be tested. We should disable same-site proactive
  // BrowsingInstance for |orig_rfh| before continuing.
  // Note: this will not disable RenderDocument.
  // TODO(crbug.com/936696): Skip this test when main-frame RenderDocument is
  // enabled.
  DisableProactiveBrowsingInstanceSwapFor(orig_rfh);
  auto same_site_navigation = NavigationSimulator::CreateRendererInitiated(
      kSameSiteUrl, main_test_rfh());
  same_site_navigation->SetHasUserGesture(false);
  same_site_navigation->ReadyToCommit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());

  // Navigate to a new site, with the beforeunload request in flight.
  const GURL kCrossSiteUrl("http://www.yahoo.com");
  auto cross_site_navigation = NavigationSimulatorImpl::CreateBrowserInitiated(
      kCrossSiteUrl, contents());
  cross_site_navigation->set_block_invoking_before_unload_completed_callback(
      true);
  cross_site_navigation->Start();
  TestRenderFrameHost* pending_rfh = contents()->GetPendingMainFrame();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_TRUE(orig_rfh->is_waiting_for_beforeunload_completion());
  EXPECT_NE(orig_rfh, pending_rfh);

  // Suppose the first navigation tries to commit now, with a
  // blink::mojom::LocalFrame::StopLoading() in flight. This should not cancel
  // the pending navigation, but it should act as if the beforeunload completion
  // callback had been invoked.
  same_site_navigation->Commit();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());
  EXPECT_FALSE(orig_rfh->is_waiting_for_beforeunload_completion());
  // It should commit.
  ASSERT_EQ(2, controller().GetEntryCount());
  EXPECT_EQ(kSameSiteUrl, controller().GetLastCommittedEntry()->GetURL());

  // The pending navigation should be able to commit successfully.
  cross_site_navigation->Commit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(pending_rfh, main_test_rfh());
  EXPECT_EQ(3, controller().GetEntryCount());
}

// Test that NavigationEntries have the correct page state after going
// forward and back.  Prevents regression for bug 1116137.
TEST_F(WebContentsImplTest, NavigationEntryContentState) {

  // Navigate to URL.  There should be no committed entry yet.
  const GURL url("http://www.google.com");
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(url, contents());
  navigation->ReadyToCommit();
  NavigationEntry* entry = controller().GetLastCommittedEntry();
  EXPECT_EQ(nullptr, entry);

  // Committed entry should have page state.
  navigation->Commit();
  entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry->GetPageState().IsValid());

  // Navigate to same site.
  const GURL url2("http://images.google.com");
  auto navigation2 =
      NavigationSimulator::CreateBrowserInitiated(url2, contents());
  navigation2->ReadyToCommit();
  entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry->GetPageState().IsValid());

  // Committed entry should have page state.
  navigation2->Commit();
  entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry->GetPageState().IsValid());

  // Now go back.  Committed entry should still have page state.
  NavigationSimulator::GoBack(contents());
  entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry->GetPageState().IsValid());
}

// Test that NavigationEntries have the correct page state and SiteInstance
// state after opening a new window to about:blank.  Prevents regression for
// bugs b/1116137 and http://crbug.com/111975.
TEST_F(WebContentsImplTest, NavigationEntryContentStateNewWindow) {
  TestRenderFrameHost* orig_rfh = main_test_rfh();

  // Navigate to about:blank.
  const GURL url(url::kAboutBlankURL);
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);

  // Should have a page state here.
  NavigationEntry* entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry->GetPageState().IsValid());

  // The SiteInstance should be available for other navigations to use.
  NavigationEntryImpl* entry_impl =
      NavigationEntryImpl::FromNavigationEntry(entry);
  EXPECT_FALSE(entry_impl->site_instance()->HasSite());
  int32_t site_instance_id = entry_impl->site_instance()->GetId();

  // Navigating to a normal page should not cause a process swap.
  const GURL new_url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), new_url);

  EXPECT_EQ(orig_rfh, main_test_rfh());
  NavigationEntryImpl* entry_impl2 = NavigationEntryImpl::FromNavigationEntry(
      controller().GetLastCommittedEntry());
  EXPECT_EQ(site_instance_id, entry_impl2->site_instance()->GetId());
  EXPECT_TRUE(entry_impl2->site_instance()->HasSite());
}

namespace {

void ExpectTrue(bool value) {
  DCHECK(value);
}

void ExpectFalse(bool value) {
  DCHECK(!value);
}

}  // namespace

// Tests that fullscreen is exited throughout the object hierarchy when
// navigating to a new page.
TEST_F(WebContentsImplTest, NavigationExitsFullscreen) {
  FakeFullscreenDelegate fake_delegate;
  contents()->SetDelegate(&fake_delegate);
  TestRenderFrameHost* orig_rfh = main_test_rfh();

  // Navigate to a site.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);
  EXPECT_EQ(orig_rfh, main_test_rfh());

  // Toggle fullscreen mode on (as if initiated via IPC from renderer).
  EXPECT_FALSE(contents()->IsFullscreen());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));
  main_test_rfh()->frame_tree_node()->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);
  orig_rfh->EnterFullscreen(blink::mojom::FullscreenOptions::New(),
                            base::BindOnce(&ExpectTrue));
  EXPECT_TRUE(contents()->IsFullscreen());
  EXPECT_TRUE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  // Navigate to a new site.
  const GURL url2("http://www.yahoo.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url2);

  // Confirm fullscreen has exited.
  EXPECT_FALSE(contents()->IsFullscreen());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  contents()->SetDelegate(nullptr);
}

// Tests that fullscreen is exited throughout the object hierarchy when
// instructing NavigationController to GoBack() or GoForward().
TEST_F(WebContentsImplTest, HistoryNavigationExitsFullscreen) {
  FakeFullscreenDelegate fake_delegate;
  contents()->SetDelegate(&fake_delegate);
  TestRenderFrameHost* orig_rfh = main_test_rfh();

  // Navigate to a site.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);
  EXPECT_EQ(orig_rfh, main_test_rfh());

  // Now, navigate to another page on the same site.
  const GURL url2("http://www.google.com/search?q=kittens");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url2);
  if (CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    // If ProactivelySwapBrowsingInstance is enabled on same-site navigations,
    // the same-site navigation above will use a new RFH.
    EXPECT_NE(orig_rfh, main_test_rfh());
  } else {
    EXPECT_EQ(orig_rfh, main_test_rfh());
  }

  // Sanity-check: Confirm we're not starting out in fullscreen mode.
  EXPECT_FALSE(contents()->IsFullscreen());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  for (int i = 0; i < 2; ++i) {
    // Toggle fullscreen mode on (as if initiated via IPC from renderer).
    main_test_rfh()->frame_tree_node()->UpdateUserActivationState(
        blink::mojom::UserActivationUpdateType::kNotifyActivation,
        blink::mojom::UserActivationNotificationType::kTest);
    main_test_rfh()->EnterFullscreen(blink::mojom::FullscreenOptions::New(),
                                     base::BindOnce(&ExpectTrue));
    EXPECT_TRUE(contents()->IsFullscreen());
    EXPECT_TRUE(fake_delegate.IsFullscreenForTabOrPending(contents()));

    // Navigate backward (or forward).
    if (i == 0)
      NavigationSimulator::GoBack(contents());
    else
      NavigationSimulator::GoForward(contents());

    // Confirm fullscreen has exited.
    EXPECT_FALSE(contents()->IsFullscreen());
    EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));
  }

  contents()->SetDelegate(nullptr);
}

// Tests that fullscreen is exited throughout the object hierarchy on a renderer
// crash.
TEST_F(WebContentsImplTest, CrashExitsFullscreen) {
  FakeFullscreenDelegate fake_delegate;
  contents()->SetDelegate(&fake_delegate);

  // Navigate to a site.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);

  // Toggle fullscreen mode on (as if initiated via IPC from renderer).
  EXPECT_FALSE(contents()->IsFullscreen());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));
  main_test_rfh()->frame_tree_node()->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);
  main_test_rfh()->EnterFullscreen(blink::mojom::FullscreenOptions::New(),
                                   base::BindOnce(&ExpectTrue));
  EXPECT_TRUE(contents()->IsFullscreen());
  EXPECT_TRUE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  // Crash the renderer.
  main_test_rfh()->GetProcess()->SimulateCrash();

  // Confirm fullscreen has exited.
  EXPECT_FALSE(contents()->IsFullscreen());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  contents()->SetDelegate(nullptr);
}

TEST_F(WebContentsImplTest,
       FailEnterFullscreenWhenNoUserActivationNoOrientationChange) {
  FakeFullscreenDelegate fake_delegate;
  contents()->SetDelegate(&fake_delegate);

  // Navigate to a site.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);

  // Toggle fullscreen mode on (as if initiated via IPC from renderer).
  EXPECT_FALSE(contents()->IsFullscreen());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  // When there is no user activation and no orientation change, entering
  // fullscreen will fail.
  main_test_rfh()->EnterFullscreen(blink::mojom::FullscreenOptions::New(),
                                   base::BindOnce(&ExpectFalse));
  EXPECT_FALSE(contents()->HasSeenRecentScreenOrientationChange());
  EXPECT_FALSE(
      main_test_rfh()->frame_tree_node()->HasTransientUserActivation());
  EXPECT_FALSE(contents()->IsFullscreen());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  contents()->SetDelegate(nullptr);
}

// Regression test for http://crbug.com/168611 - the URLs passed by the
// DidFinishLoad and DidFailLoadWithError IPCs should get filtered.
TEST_F(WebContentsImplTest, FilterURLs) {
  TestWebContentsObserver observer(contents());

  // A navigation to about:whatever should always look like a navigation to
  // about:blank
  GURL url_normalized(url::kAboutBlankURL);
  GURL url_from_ipc("about:whatever");
  GURL url_blocked(kBlockedURL);

  // We navigate the test WebContents to about:blank, since NavigateAndCommit
  // will use the given URL to create the NavigationEntry as well, and that
  // entry should contain the filtered URL.
  contents()->NavigateAndCommit(url_normalized);

  // Check that an IPC with about:whatever is correctly normalized.
  contents()->TestDidFinishLoad(url_from_ipc);

  EXPECT_EQ(url_blocked, observer.last_url());

  // Create and navigate another WebContents.
  std::unique_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  TestWebContentsObserver other_observer(other_contents.get());
  other_contents->NavigateAndCommit(url_normalized);

  // Check that an IPC with about:whatever is correctly normalized.
  other_contents->GetMainFrame()->DidFailLoadWithError(url_from_ipc, 1);
  EXPECT_EQ(url_blocked, other_observer.last_url());
}

// Test that if a pending contents is deleted before it is shown, we don't
// crash.
TEST_F(WebContentsImplTest, PendingContentsDestroyed) {
  auto other_contents = base::WrapUnique(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  content::TestWebContents* test_web_contents = other_contents.get();
  contents()->AddPendingContents(std::move(other_contents), GURL());
  RenderWidgetHost* widget =
      test_web_contents->GetMainFrame()->GetRenderWidgetHost();
  int process_id = widget->GetProcess()->GetID();
  int widget_id = widget->GetRoutingID();

  // TODO(erikchen): Fix ownership semantics of WebContents. Nothing should be
  // able to delete it beside from the owner. https://crbug.com/832879.
  delete test_web_contents;
  EXPECT_FALSE(contents()->GetCreatedWindow(process_id, widget_id).has_value());
}

TEST_F(WebContentsImplTest, PendingContentsShown) {
  GURL url("http://example.com");
  auto other_contents = base::WrapUnique(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  content::TestWebContents* test_web_contents = other_contents.get();
  contents()->AddPendingContents(std::move(other_contents), url);

  RenderWidgetHost* widget =
      test_web_contents->GetMainFrame()->GetRenderWidgetHost();
  int process_id = widget->GetProcess()->GetID();
  int widget_id = widget->GetRoutingID();

  // The first call to GetCreatedWindow pops it off the pending list.
  base::Optional<CreatedWindow> created_window =
      contents()->GetCreatedWindow(process_id, widget_id);
  EXPECT_TRUE(created_window.has_value());
  EXPECT_EQ(test_web_contents, created_window->contents.get());
  // Validate target_url.
  EXPECT_EQ(url, created_window->target_url);

  // A second call should return nullopt, verifying that it's been forgotten.
  EXPECT_FALSE(contents()->GetCreatedWindow(process_id, widget_id).has_value());
}

TEST_F(WebContentsImplTest, CapturerOverridesPreferredSize) {
  const gfx::Size original_preferred_size(1024, 768);
  contents()->UpdatePreferredSize(original_preferred_size);

  // With no capturers, expect the preferred size to be the one propagated into
  // WebContentsImpl via the RenderViewHostDelegate interface.
  EXPECT_FALSE(contents()->IsBeingCaptured());
  EXPECT_EQ(original_preferred_size, contents()->GetPreferredSize());

  // Increment capturer count, but without specifying a capture size.  Expect
  // a "not set" preferred size.
  contents()->IncrementCapturerCount(gfx::Size(), /* stay_hidden */ false);
  EXPECT_TRUE(contents()->IsBeingCaptured());
  EXPECT_EQ(gfx::Size(), contents()->GetPreferredSize());

  // Increment capturer count again, but with an overriding capture size.
  // Expect preferred size to now be overridden to the capture size.
  const gfx::Size capture_size(1280, 720);
  contents()->IncrementCapturerCount(capture_size, /* stay_hidden */ false);
  EXPECT_TRUE(contents()->IsBeingCaptured());
  EXPECT_EQ(capture_size, contents()->GetPreferredSize());

  // Increment capturer count a third time, but the expect that the preferred
  // size is still the first capture size.
  const gfx::Size another_capture_size(720, 480);
  contents()->IncrementCapturerCount(another_capture_size,
                                     /* stay_hidden */ false);
  EXPECT_TRUE(contents()->IsBeingCaptured());
  EXPECT_EQ(capture_size, contents()->GetPreferredSize());

  // Decrement capturer count twice, but expect the preferred size to still be
  // overridden.
  contents()->DecrementCapturerCount(/* stay_hidden */ false);
  contents()->DecrementCapturerCount(/* stay_hidden */ false);
  EXPECT_TRUE(contents()->IsBeingCaptured());
  EXPECT_EQ(capture_size, contents()->GetPreferredSize());

  // Decrement capturer count, and since the count has dropped to zero, the
  // original preferred size should be restored.
  contents()->DecrementCapturerCount(/* stay_hidden */ false);
  EXPECT_FALSE(contents()->IsBeingCaptured());
  EXPECT_EQ(original_preferred_size, contents()->GetPreferredSize());
}

TEST_F(WebContentsImplTest, UpdateWebContentsVisibility) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kWebContentsOcclusion);

  TestRenderWidgetHostView* view = static_cast<TestRenderWidgetHostView*>(
      main_test_rfh()->GetRenderViewHost()->GetWidget()->GetView());
  TestWebContentsObserver observer(contents());

  EXPECT_FALSE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());

  // WebContents must be made visible once before it can be hidden.
  contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  EXPECT_FALSE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());
  EXPECT_EQ(Visibility::VISIBLE, contents()->GetVisibility());

  contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  EXPECT_TRUE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());
  EXPECT_EQ(Visibility::VISIBLE, contents()->GetVisibility());

  // Hiding/occluding/showing the WebContents should hide and show |view|.
  contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  EXPECT_FALSE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());
  EXPECT_EQ(Visibility::HIDDEN, contents()->GetVisibility());

  contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  EXPECT_TRUE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());
  EXPECT_EQ(Visibility::VISIBLE, contents()->GetVisibility());

  contents()->UpdateWebContentsVisibility(Visibility::OCCLUDED);
  EXPECT_TRUE(view->is_showing());
  EXPECT_TRUE(view->is_occluded());
  EXPECT_EQ(Visibility::OCCLUDED, contents()->GetVisibility());

  contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  EXPECT_TRUE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());
  EXPECT_EQ(Visibility::VISIBLE, contents()->GetVisibility());

  contents()->UpdateWebContentsVisibility(Visibility::OCCLUDED);
  EXPECT_TRUE(view->is_showing());
  EXPECT_TRUE(view->is_occluded());
  EXPECT_EQ(Visibility::OCCLUDED, contents()->GetVisibility());

  contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  EXPECT_FALSE(view->is_showing());
  EXPECT_EQ(Visibility::HIDDEN, contents()->GetVisibility());
}

namespace {

void HideOrOccludeWithCapturerTest(WebContentsImpl* contents,
                                   Visibility hidden_or_occluded) {
  TestRenderWidgetHostView* view = static_cast<TestRenderWidgetHostView*>(
      contents->GetRenderWidgetHostView());

  EXPECT_FALSE(view->is_showing());

  // WebContents must be made visible once before it can be hidden.
  contents->UpdateWebContentsVisibility(Visibility::VISIBLE);
  EXPECT_TRUE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());
  EXPECT_EQ(Visibility::VISIBLE, contents->GetVisibility());

  // Add a capturer when the contents is visible and then hide the contents.
  // |view| should remain visible.
  contents->IncrementCapturerCount(gfx::Size(), /* stay_hidden */ false);
  contents->UpdateWebContentsVisibility(hidden_or_occluded);
  EXPECT_TRUE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());
  EXPECT_EQ(hidden_or_occluded, contents->GetVisibility());

  // Remove the capturer when the contents is hidden/occluded. |view| should be
  // hidden/occluded.
  contents->DecrementCapturerCount(/* stay_hidden */ false);
  if (hidden_or_occluded == Visibility::HIDDEN) {
    EXPECT_FALSE(view->is_showing());
  } else {
    EXPECT_TRUE(view->is_showing());
    EXPECT_TRUE(view->is_occluded());
  }

  // Add a capturer when the contents is hidden. |view| should be unoccluded.
  contents->IncrementCapturerCount(gfx::Size(), /* stay_hidden */ false);
  EXPECT_FALSE(view->is_occluded());

  // Show the contents. The view should be visible.
  contents->UpdateWebContentsVisibility(Visibility::VISIBLE);
  EXPECT_TRUE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());
  EXPECT_EQ(Visibility::VISIBLE, contents->GetVisibility());

  // Remove the capturer when the contents is visible. The view should remain
  // visible.
  contents->DecrementCapturerCount(/* stay_hidden */ false);
  EXPECT_TRUE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());
}

}  // namespace

TEST_F(WebContentsImplTest, HideWithCapturer) {
  HideOrOccludeWithCapturerTest(contents(), Visibility::HIDDEN);
}

TEST_F(WebContentsImplTest, OccludeWithCapturer) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kWebContentsOcclusion);
  HideOrOccludeWithCapturerTest(contents(), Visibility::OCCLUDED);
}

TEST_F(WebContentsImplTest, HiddenCapture) {
  TestRenderWidgetHostView* rwhv = static_cast<TestRenderWidgetHostView*>(
      contents()->GetRenderWidgetHostView());

  contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  EXPECT_EQ(Visibility::HIDDEN, contents()->GetVisibility());

  contents()->IncrementCapturerCount(gfx::Size(), /* stay_hidden */ true);
  EXPECT_TRUE(rwhv->is_showing());

  contents()->IncrementCapturerCount(gfx::Size(), /* stay_hidden */ false);
  EXPECT_TRUE(rwhv->is_showing());

  contents()->DecrementCapturerCount(/* stay_hidden */ true);
  EXPECT_TRUE(rwhv->is_showing());

  contents()->DecrementCapturerCount(/* stay_hidden */ false);
  EXPECT_FALSE(rwhv->is_showing());
}

// Tests that GetLastActiveTime starts with a real, non-zero time and updates
// on activity.
TEST_F(WebContentsImplTest, GetLastActiveTime) {
  // The WebContents starts with a valid creation time.
  EXPECT_FALSE(contents()->GetLastActiveTime().is_null());

  // Reset the last active time to a known-bad value.
  contents()->last_active_time_ = base::TimeTicks();
  ASSERT_TRUE(contents()->GetLastActiveTime().is_null());

  // Simulate activating the WebContents. The active time should update.
  contents()->WasShown();
  EXPECT_FALSE(contents()->GetLastActiveTime().is_null());
}

class ContentsZoomChangedDelegate : public WebContentsDelegate {
 public:
  ContentsZoomChangedDelegate() :
    contents_zoom_changed_call_count_(0),
    last_zoom_in_(false) {
  }

  int GetAndResetContentsZoomChangedCallCount() {
    int count = contents_zoom_changed_call_count_;
    contents_zoom_changed_call_count_ = 0;
    return count;
  }

  bool last_zoom_in() const {
    return last_zoom_in_;
  }

  // WebContentsDelegate:
  void ContentsZoomChange(bool zoom_in) override {
    contents_zoom_changed_call_count_++;
    last_zoom_in_ = zoom_in;
  }

 private:
  int contents_zoom_changed_call_count_;
  bool last_zoom_in_;

  DISALLOW_COPY_AND_ASSIGN(ContentsZoomChangedDelegate);
};

// Tests that some mouseehweel events get turned into browser zoom requests.
TEST_F(WebContentsImplTest, HandleWheelEvent) {
  using blink::WebInputEvent;

  std::unique_ptr<ContentsZoomChangedDelegate> delegate(
      new ContentsZoomChangedDelegate());
  contents()->SetDelegate(delegate.get());

  int modifiers = 0;
  // Verify that normal mouse wheel events do nothing to change the zoom level.
  blink::WebMouseWheelEvent event =
      blink::SyntheticWebMouseWheelEventBuilder::Build(
          0, 0, 0, 1, modifiers, ui::ScrollGranularity::kScrollByPixel);
  EXPECT_FALSE(contents()->HandleWheelEvent(event));
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());

  // But whenever the ctrl modifier is applied zoom can be increased or
  // decreased. Except on MacOS where we never want to adjust zoom
  // with mousewheel.
  modifiers = WebInputEvent::kControlKey;
  event = blink::SyntheticWebMouseWheelEventBuilder::Build(
      0, 0, 0, 1, modifiers, ui::ScrollGranularity::kScrollByPixel);
  bool handled = contents()->HandleWheelEvent(event);
#if defined(USE_AURA)
  EXPECT_TRUE(handled);
  EXPECT_EQ(1, delegate->GetAndResetContentsZoomChangedCallCount());
  EXPECT_TRUE(delegate->last_zoom_in());
#else
  EXPECT_FALSE(handled);
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());
#endif

  modifiers = WebInputEvent::kControlKey | WebInputEvent::kShiftKey |
              WebInputEvent::kAltKey;
  event = blink::SyntheticWebMouseWheelEventBuilder::Build(
      0, 0, 2, -5, modifiers, ui::ScrollGranularity::kScrollByPixel);
  handled = contents()->HandleWheelEvent(event);
#if defined(USE_AURA)
  EXPECT_TRUE(handled);
  EXPECT_EQ(1, delegate->GetAndResetContentsZoomChangedCallCount());
  EXPECT_FALSE(delegate->last_zoom_in());
#else
  EXPECT_FALSE(handled);
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());
#endif

  // Unless there is no vertical movement.
  event = blink::SyntheticWebMouseWheelEventBuilder::Build(
      0, 0, 2, 0, modifiers, ui::ScrollGranularity::kScrollByPixel);
  EXPECT_FALSE(contents()->HandleWheelEvent(event));
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());

  // Events containing precise scrolling deltas also shouldn't result in the
  // zoom being adjusted, to avoid accidental adjustments caused by
  // two-finger-scrolling on a touchpad.
  modifiers = WebInputEvent::kControlKey;
  event = blink::SyntheticWebMouseWheelEventBuilder::Build(
      0, 0, 0, 5, modifiers, ui::ScrollGranularity::kScrollByPrecisePixel);
  EXPECT_FALSE(contents()->HandleWheelEvent(event));
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());

  // Ensure pointers to the delegate aren't kept beyond its lifetime.
  contents()->SetDelegate(nullptr);
}

// Tests that GetRelatedActiveContentsCount is shared between related
// SiteInstances and includes WebContents that have not navigated yet.
TEST_F(WebContentsImplTest, ActiveContentsCountBasic) {
  scoped_refptr<SiteInstance> instance1(
      SiteInstance::CreateForURL(browser_context(), GURL("http://a.com")));
  scoped_refptr<SiteInstance> instance2(
      instance1->GetRelatedSiteInstance(GURL("http://b.com")));

  EXPECT_EQ(0u, instance1->GetRelatedActiveContentsCount());
  EXPECT_EQ(0u, instance2->GetRelatedActiveContentsCount());

  std::unique_ptr<TestWebContents> contents1(
      TestWebContents::Create(browser_context(), instance1.get()));
  EXPECT_EQ(1u, instance1->GetRelatedActiveContentsCount());
  EXPECT_EQ(1u, instance2->GetRelatedActiveContentsCount());

  std::unique_ptr<TestWebContents> contents2(
      TestWebContents::Create(browser_context(), instance1.get()));
  EXPECT_EQ(2u, instance1->GetRelatedActiveContentsCount());
  EXPECT_EQ(2u, instance2->GetRelatedActiveContentsCount());

  contents1.reset();
  EXPECT_EQ(1u, instance1->GetRelatedActiveContentsCount());
  EXPECT_EQ(1u, instance2->GetRelatedActiveContentsCount());

  contents2.reset();
  EXPECT_EQ(0u, instance1->GetRelatedActiveContentsCount());
  EXPECT_EQ(0u, instance2->GetRelatedActiveContentsCount());
}

// Tests that GetRelatedActiveContentsCount is preserved correctly across
// same-site and cross-site navigations.
TEST_F(WebContentsImplTest, ActiveContentsCountNavigate) {
  scoped_refptr<SiteInstance> instance(
      SiteInstance::Create(browser_context()));

  EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());

  std::unique_ptr<TestWebContents> contents(
      TestWebContents::Create(browser_context(), instance.get()));
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL.
  auto navigation1 = NavigationSimulator::CreateBrowserInitiated(
      GURL("http://a.com/1"), contents.get());
  navigation1->Start();
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  navigation1->Commit();
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL in the same site.
  auto navigation2 = NavigationSimulator::CreateBrowserInitiated(
      GURL("http://a.com/2"), contents.get());
  navigation2->Start();
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  navigation2->Commit();
  if (CanSameSiteMainFrameNavigationsChangeSiteInstances()) {
    // When ProactivelySwapBrowsingInstance turned on for same-site navigations,
    // the BrowsingInstance will change on same-site navigations.
    EXPECT_NE(instance, contents->GetSiteInstance());
    // Check the previous instance's count.
    EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());
    // Update the current instance.
    instance = contents->GetSiteInstance();
    EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  }

  // Navigate to a URL in a different site in the same BrowsingInstance.
  const GURL kUrl2("http://b.com");
  auto navigation3 = NavigationSimulator::CreateRendererInitiated(
      kUrl2, contents->GetMainFrame());
  navigation3->ReadyToCommit();
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  if (AreAllSitesIsolatedForTesting() ||
      CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
    EXPECT_TRUE(contents->CrossProcessNavigationPending());
  } else {
    EXPECT_FALSE(contents->CrossProcessNavigationPending());
  }
  navigation3->Commit();
  if (CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
    // When ProactivelySwapBrowsingInstance turned on, the BrowsingInstance will
    // change on cross-site navigations.
    EXPECT_NE(instance, contents->GetSiteInstance());
    EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());
    // Update the current instance.
    instance = contents->GetSiteInstance();
  }
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL in a different site and different BrowsingInstance, by
  // using a TYPED page transition instead of LINK.
  const GURL kUrl3("http://c.com");
  auto navigation4 =
      NavigationSimulator::CreateBrowserInitiated(kUrl3, contents.get());
  navigation4->SetTransition(ui::PAGE_TRANSITION_TYPED);
  navigation4->ReadyToCommit();
  EXPECT_TRUE(contents->CrossProcessNavigationPending());
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  scoped_refptr<SiteInstance> new_instance =
      contents->GetPendingMainFrame()->GetSiteInstance();
  navigation4->Commit();
  EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());
  EXPECT_EQ(1u, new_instance->GetRelatedActiveContentsCount());
  EXPECT_FALSE(new_instance->IsRelatedSiteInstance(instance.get()));

  contents.reset();
  EXPECT_EQ(0u, new_instance->GetRelatedActiveContentsCount());
}

// Tests that GetRelatedActiveContentsCount tracks BrowsingInstance changes
// from WebUI.
TEST_F(WebContentsImplTest, ActiveContentsCountChangeBrowsingInstance) {
  scoped_refptr<SiteInstance> instance(
      SiteInstance::Create(browser_context()));

  EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());

  std::unique_ptr<TestWebContents> contents(
      TestWebContents::Create(browser_context(), instance.get()));
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL.
  contents->NavigateAndCommit(GURL("http://a.com"));
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL which sort of looks like a chrome:// url.
  contents->NavigateAndCommit(GURL("http://gpu"));
  if (CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
    // The navigation from "a.com" to "gpu" is using a new BrowsingInstance.
    EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());
    // The rest of the test expects |instance| to match the one in the main
    // frame.
    instance = contents->GetMainFrame()->GetSiteInstance();
  }
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL with WebUI. This will change BrowsingInstances.
  const GURL kWebUIUrl = GURL(GetWebUIURL(kChromeUIGpuHost));
  auto web_ui_navigation =
      NavigationSimulator::CreateBrowserInitiated(kWebUIUrl, contents.get());
  web_ui_navigation->Start();
  EXPECT_TRUE(contents->CrossProcessNavigationPending());
  scoped_refptr<SiteInstance> instance_webui(
      contents->GetPendingMainFrame()->GetSiteInstance());
  EXPECT_FALSE(instance->IsRelatedSiteInstance(instance_webui.get()));

  // At this point, contents still counts for the old BrowsingInstance.
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  EXPECT_EQ(0u, instance_webui->GetRelatedActiveContentsCount());

  // Commit and contents counts for the new one.
  web_ui_navigation->Commit();
  EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());
  EXPECT_EQ(1u, instance_webui->GetRelatedActiveContentsCount());

  contents.reset();
  EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());
  EXPECT_EQ(0u, instance_webui->GetRelatedActiveContentsCount());
}

class LoadingWebContentsObserver : public WebContentsObserver {
 public:
  explicit LoadingWebContentsObserver(WebContents* contents)
      : WebContentsObserver(contents),
        is_loading_(false),
        did_receive_response_(false) {}
  ~LoadingWebContentsObserver() override {}

  // The assertions on these messages ensure that they are received in order.
  void DidStartLoading() override {
    ASSERT_FALSE(did_receive_response_);
    ASSERT_FALSE(is_loading_);
    is_loading_ = true;
  }
  void DidReceiveResponse() override {
    ASSERT_TRUE(is_loading_);
    did_receive_response_ = true;
  }
  void DidStopLoading() override {
    ASSERT_TRUE(is_loading_);
    is_loading_ = false;
    did_receive_response_ = false;
  }

  bool is_loading() const { return is_loading_; }
  bool did_receive_response() const { return did_receive_response_; }

 private:
  bool is_loading_;
  bool did_receive_response_;

  DISALLOW_COPY_AND_ASSIGN(LoadingWebContentsObserver);
};

// Subclass of WebContentsImplTest for cases that need out-of-process iframes.
class WebContentsImplTestWithSiteIsolation : public WebContentsImplTest {
 public:
  WebContentsImplTestWithSiteIsolation() {
    IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  }
};

// Ensure that DidStartLoading/DidStopLoading events balance out properly with
// interleaving cross-process navigations in multiple subframes.
// See https://crbug.com/448601 for details of the underlying issue. The
// sequence of events that reproduce it are as follows:
// * Navigate top-level frame with one subframe.
// * Subframe navigates more than once before the top-level frame has had a
//   chance to complete the load.
// The subframe navigations cause the loading_frames_in_progress_ to drop down
// to 0, while the loading_progresses_ map is not reset.
TEST_F(WebContentsImplTestWithSiteIsolation, StartStopEventsBalance) {
  // The bug manifests itself in regular mode as well, but browser-initiated
  // navigation of subframes is only possible in --site-per-process mode within
  // unit tests.
  const GURL initial_url("about:blank");
  const GURL main_url("http://www.chromium.org");
  const GURL foo_url("http://foo.chromium.org");
  const GURL bar_url("http://bar.chromium.org");
  TestRenderFrameHost* orig_rfh = main_test_rfh();

  // Use a WebContentsObserver to observe the behavior of the tab's spinner.
  LoadingWebContentsObserver observer(contents());

  // Navigate the main RenderFrame and commit. The frame should still be
  // loading.
  auto main_frame_navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(main_url, contents());
  main_frame_navigation->SetKeepLoading(true);
  main_frame_navigation->Commit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());
  EXPECT_TRUE(contents()->IsLoading());

  // The Observer callback implementations contain assertions to ensure that the
  // events arrive in the correct order.
  EXPECT_TRUE(observer.is_loading());
  EXPECT_TRUE(observer.did_receive_response());

  // Create a child frame to navigate multiple times.
  TestRenderFrameHost* subframe = orig_rfh->AppendChild("subframe");

  // Navigate the child frame to about:blank, which will send DidStopLoading
  // message.
  NavigationSimulator::NavigateAndCommitFromDocument(initial_url, subframe);

  // Navigate the frame to another URL, which will send again
  // DidStartLoading and DidStopLoading messages.
  NavigationSimulator::NavigateAndCommitFromDocument(foo_url, subframe);

  // Since the main frame hasn't sent any DidStopLoading messages, it is
  // expected that the WebContents is still in loading state.
  EXPECT_TRUE(contents()->IsLoading());
  EXPECT_TRUE(observer.is_loading());
  EXPECT_TRUE(observer.did_receive_response());

  // After navigation, the RenderFrameHost may change.
  subframe = static_cast<TestRenderFrameHost*>(
      contents()->GetFrameTree()->root()->child_at(0)->current_frame_host());
  // Navigate the frame again, this time using LoadURLWithParams. This causes
  // RenderFrameHost to call into WebContents::DidStartLoading, which starts
  // the spinner.
  {
    auto navigation =
        NavigationSimulatorImpl::CreateBrowserInitiated(bar_url, contents());

    NavigationController::LoadURLParams load_params(bar_url);
    load_params.referrer = Referrer(GURL("http://referrer"),
                                    network::mojom::ReferrerPolicy::kDefault);
    load_params.transition_type = ui::PAGE_TRANSITION_MANUAL_SUBFRAME;
    load_params.extra_headers = "content-type: text/plain";
    load_params.load_type = NavigationController::LOAD_TYPE_DEFAULT;
    load_params.is_renderer_initiated = false;
    load_params.override_user_agent = NavigationController::UA_OVERRIDE_TRUE;
    load_params.frame_tree_node_id =
        subframe->frame_tree_node()->frame_tree_node_id();
    navigation->SetLoadURLParams(&load_params);

    navigation->Commit();
    subframe = static_cast<TestRenderFrameHost*>(
        navigation->GetFinalRenderFrameHost());
  }

  // At this point the status should still be loading, since the main frame
  // hasn't sent the DidstopLoading message yet.
  EXPECT_TRUE(contents()->IsLoading());
  EXPECT_TRUE(observer.is_loading());
  EXPECT_TRUE(observer.did_receive_response());

  // Send the DidStopLoading for the main frame and ensure it isn't loading
  // anymore.
  main_frame_navigation->StopLoading();
  EXPECT_FALSE(contents()->IsLoading());
  EXPECT_FALSE(observer.is_loading());
  EXPECT_FALSE(observer.did_receive_response());
}

// Tests that WebContentsImpl::IsLoadingToDifferentDocument only reports main
// frame loads. Browser-initiated navigation of subframes is only possible in
// --site-per-process mode within unit tests.
TEST_F(WebContentsImplTestWithSiteIsolation, IsLoadingToDifferentDocument) {
  const GURL main_url("http://www.chromium.org");
  TestRenderFrameHost* orig_rfh = main_test_rfh();

  // Navigate the main RenderFrame and commit. The frame should still be
  // loading.
  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(main_url, contents());
  navigation->SetKeepLoading(true);
  navigation->Commit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());
  EXPECT_TRUE(contents()->IsLoading());
  EXPECT_TRUE(contents()->IsLoadingToDifferentDocument());

  // Send the DidStopLoading for the main frame and ensure it isn't loading
  // anymore.
  navigation->StopLoading();
  EXPECT_FALSE(contents()->IsLoading());
  EXPECT_FALSE(contents()->IsLoadingToDifferentDocument());

  // Create a child frame to navigate.
  TestRenderFrameHost* subframe = orig_rfh->AppendChild("subframe");

  // Navigate the child frame to about:blank, make sure the web contents is
  // marked as "loading" but not "loading to different document".
  subframe->SendNavigateWithTransition(0, false, GURL("about:blank"),
                                       ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  EXPECT_TRUE(contents()->IsLoading());
  EXPECT_FALSE(contents()->IsLoadingToDifferentDocument());
  static_cast<mojom::FrameHost*>(subframe)->DidStopLoading();
  EXPECT_FALSE(contents()->IsLoading());
}

// Ensure that WebContentsImpl does not stop loading too early when there still
// is a pending renderer. This can happen if a same-process non user-initiated
// navigation completes while there is an ongoing cross-process navigation.
// TODO(clamy): Rewrite that test when the renderer-initiated non-user-initiated
// navigation no longer kills the speculative RenderFrameHost. See
// https://crbug.com/889039.
TEST_F(WebContentsImplTest, DISABLED_NoEarlyStop) {
  const GURL kUrl1("http://www.chromium.org");
  const GURL kUrl2("http://www.google.com");
  const GURL kUrl3("http://www.chromium.org/foo");

  contents()->NavigateAndCommit(kUrl1);

  TestRenderFrameHost* current_rfh = main_test_rfh();

  // Start a browser-initiated cross-process navigation to |kUrl2|. The
  // WebContents should be loading.
  auto cross_process_navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl2, contents());
  cross_process_navigation->ReadyToCommit();
  TestRenderFrameHost* pending_rfh = contents()->GetPendingMainFrame();
  EXPECT_TRUE(contents()->IsLoading());

  // The current RenderFrameHost starts a non user-initiated render-initiated
  // navigation. The WebContents should still be loading.
  auto same_process_navigation =
      NavigationSimulator::CreateRendererInitiated(kUrl3, current_rfh);
  same_process_navigation->SetHasUserGesture(false);
  same_process_navigation->Start();
  EXPECT_TRUE(contents()->IsLoading());

  // Simulate the commit and DidStopLoading from the renderer-initiated
  // navigation in the current RenderFrameHost. There should still be a pending
  // RenderFrameHost and the WebContents should still be loading.
  same_process_navigation->Commit();
  static_cast<mojom::FrameHost*>(current_rfh)->DidStopLoading();
  EXPECT_EQ(contents()->GetPendingMainFrame(), pending_rfh);
  EXPECT_TRUE(contents()->IsLoading());

  // The same-process navigation should have committed.
  ASSERT_EQ(2, controller().GetEntryCount());
  EXPECT_EQ(kUrl3, controller().GetLastCommittedEntry()->GetURL());

  // Commit the cross-process navigation. The formerly pending RenderFrameHost
  // should now be the current RenderFrameHost and the WebContents should still
  // be loading.
  cross_process_navigation->Commit();
  EXPECT_FALSE(contents()->GetPendingMainFrame());
  TestRenderFrameHost* new_current_rfh = main_test_rfh();
  EXPECT_EQ(new_current_rfh, pending_rfh);
  EXPECT_TRUE(contents()->IsLoading());
  EXPECT_EQ(3, controller().GetEntryCount());

  // Simulate the new current RenderFrameHost DidStopLoading. The WebContents
  // should now have stopped loading.
  static_cast<mojom::FrameHost*>(new_current_rfh)->DidStopLoading();
  EXPECT_EQ(main_test_rfh(), new_current_rfh);
  EXPECT_FALSE(contents()->IsLoading());
}

TEST_F(WebContentsImplTest, MediaWakeLock) {
  EXPECT_FALSE(has_audio_wake_lock());

  AudioStreamMonitor* monitor = contents()->audio_stream_monitor();

  // Ensure RenderFrame is initialized before simulating events coming from it.
  main_test_rfh()->InitializeRenderFrameIfNeeded();

  // Send a fake audio stream monitor notification.  The audio wake lock
  // should be created.
  monitor->set_was_recently_audible_for_testing(true);
  contents()->NotifyNavigationStateChanged(INVALIDATE_TYPE_AUDIO);
  EXPECT_TRUE(has_audio_wake_lock());

  // Send another fake notification, this time when WasRecentlyAudible() will
  // be false.  The wake lock should be released.
  monitor->set_was_recently_audible_for_testing(false);
  contents()->NotifyNavigationStateChanged(INVALIDATE_TYPE_AUDIO);
  EXPECT_FALSE(has_audio_wake_lock());

  main_test_rfh()->GetProcess()->SimulateCrash();

  // Verify that all the wake locks have been released.
  EXPECT_FALSE(has_audio_wake_lock());
}

TEST_F(WebContentsImplTest, ThemeColorChangeDependingOnFirstVisiblePaint) {
  TestWebContentsObserver observer(contents());
  TestRenderFrameHost* rfh = main_test_rfh();
  rfh->InitializeRenderFrameIfNeeded();

  EXPECT_EQ(base::nullopt, contents()->GetThemeColor());
  EXPECT_EQ(0, observer.theme_color_change_calls());

  // Theme color changes should not propagate past the WebContentsImpl before
  // the first visually non-empty paint has occurred.
  rfh->DidChangeThemeColor(SK_ColorRED);

  EXPECT_EQ(SK_ColorRED, contents()->GetThemeColor());
  EXPECT_EQ(0, observer.theme_color_change_calls());

  // Simulate that the first visually non-empty paint has occurred. This will
  // propagate the current theme color to the delegates.
  RenderViewHostTester::SimulateFirstPaint(test_rvh());

  EXPECT_EQ(SK_ColorRED, contents()->GetThemeColor());
  EXPECT_EQ(1, observer.theme_color_change_calls());

  // Additional changes made by the web contents should propagate as well.
  rfh->DidChangeThemeColor(SK_ColorGREEN);

  EXPECT_EQ(SK_ColorGREEN, contents()->GetThemeColor());
  EXPECT_EQ(2, observer.theme_color_change_calls());
}

TEST_F(WebContentsImplTest, ParseDownloadHeaders) {
  download::DownloadUrlParameters::RequestHeadersType request_headers =
      WebContentsImpl::ParseDownloadHeaders("A: 1\r\nB: 2\r\nC: 3\r\n\r\n");
  ASSERT_EQ(3u, request_headers.size());
  EXPECT_EQ("A", request_headers[0].first);
  EXPECT_EQ("1", request_headers[0].second);
  EXPECT_EQ("B", request_headers[1].first);
  EXPECT_EQ("2", request_headers[1].second);
  EXPECT_EQ("C", request_headers[2].first);
  EXPECT_EQ("3", request_headers[2].second);

  request_headers = WebContentsImpl::ParseDownloadHeaders("A:1\r\nA:2\r\n");
  ASSERT_EQ(2u, request_headers.size());
  EXPECT_EQ("A", request_headers[0].first);
  EXPECT_EQ("1", request_headers[0].second);
  EXPECT_EQ("A", request_headers[1].first);
  EXPECT_EQ("2", request_headers[1].second);

  request_headers = WebContentsImpl::ParseDownloadHeaders("A 1\r\nA: 2");
  ASSERT_EQ(1u, request_headers.size());
  EXPECT_EQ("A", request_headers[0].first);
  EXPECT_EQ("2", request_headers[0].second);

  request_headers = WebContentsImpl::ParseDownloadHeaders("A: 1");
  ASSERT_EQ(1u, request_headers.size());
  EXPECT_EQ("A", request_headers[0].first);
  EXPECT_EQ("1", request_headers[0].second);

  request_headers = WebContentsImpl::ParseDownloadHeaders("A 1");
  ASSERT_EQ(0u, request_headers.size());
}

namespace {

class TestJavaScriptDialogManager : public JavaScriptDialogManager {
 public:
  TestJavaScriptDialogManager() {}
  ~TestJavaScriptDialogManager() override {}

  size_t reset_count() { return reset_count_; }

  // JavaScriptDialogManager

  void RunJavaScriptDialog(WebContents* web_contents,
                           RenderFrameHost* render_frame_host,
                           JavaScriptDialogType dialog_type,
                           const base::string16& message_text,
                           const base::string16& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override {
    *did_suppress_message = true;
  }

  void RunBeforeUnloadDialog(WebContents* web_contents,
                             RenderFrameHost* render_frame_host,
                             bool is_reload,
                             DialogClosedCallback callback) override {}

  bool HandleJavaScriptDialog(WebContents* web_contents,
                              bool accept,
                              const base::string16* prompt_override) override {
    return true;
  }

  void CancelDialogs(WebContents* web_contents,
                     bool reset_state) override {
    if (reset_state)
      ++reset_count_;
  }

 private:
  size_t reset_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestJavaScriptDialogManager);
};

}  // namespace

TEST_F(WebContentsImplTest, ResetJavaScriptDialogOnUserNavigate) {
  const GURL kUrl("http://www.google.com");
  const GURL kUrl2("http://www.google.com/sub");
  TestJavaScriptDialogManager dialog_manager;
  contents()->SetJavaScriptDialogManagerForTesting(&dialog_manager);

  // A user-initiated navigation.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kUrl);
  EXPECT_EQ(1u, dialog_manager.reset_count());

  // An automatic navigation.
  auto navigation =
      NavigationSimulator::CreateRendererInitiated(kUrl2, main_test_rfh());
  navigation->SetHasUserGesture(false);
  navigation->Commit();
  if (CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    // If we changed RenderFrameHost on a renderer-initiated navigation above,
    // we would trigger RenderFrameHostManager::UnloadOldFrame, similar to the
    // first (user/browser-initiated) navigation, which will trigger dialog
    // cancellations and increment the reset_count to 2.
    EXPECT_EQ(2u, dialog_manager.reset_count());
  } else {
    EXPECT_EQ(1u, dialog_manager.reset_count());
  }

  contents()->SetJavaScriptDialogManagerForTesting(nullptr);
}

TEST_F(WebContentsImplTest, StartingSandboxFlags) {
  WebContents::CreateParams params(browser_context());
  network::mojom::WebSandboxFlags expected_flags =
      network::mojom::WebSandboxFlags::kPopups |
      network::mojom::WebSandboxFlags::kModals |
      network::mojom::WebSandboxFlags::kTopNavigation;
  params.starting_sandbox_flags = expected_flags;
  std::unique_ptr<WebContentsImpl> new_contents(
      WebContentsImpl::CreateWithOpener(params, nullptr));
  FrameTreeNode* root = new_contents->GetFrameTree()->root();
  network::mojom::WebSandboxFlags pending_flags =
      root->pending_frame_policy().sandbox_flags;
  EXPECT_EQ(pending_flags, expected_flags);
  network::mojom::WebSandboxFlags effective_flags =
      root->effective_frame_policy().sandbox_flags;
  EXPECT_EQ(effective_flags, expected_flags);
}

TEST_F(WebContentsImplTest, DidFirstVisuallyNonEmptyPaint) {
  TestWebContentsObserver observer(contents());

  RenderWidgetHostOwnerDelegate* rwhod = test_rvh();
  rwhod->RenderWidgetDidFirstVisuallyNonEmptyPaint();

  EXPECT_TRUE(observer.observed_did_first_visually_non_empty_paint());
}

TEST_F(WebContentsImplTest, DidChangeVerticalScrollDirection) {
  TestWebContentsObserver observer(contents());

  EXPECT_FALSE(observer.last_vertical_scroll_direction().has_value());

  contents()->OnVerticalScrollDirectionChanged(
      viz::VerticalScrollDirection::kUp);

  EXPECT_EQ(viz::VerticalScrollDirection::kUp,
            observer.last_vertical_scroll_direction().value());
}

namespace {

class MockWebContentsDelegate : public WebContentsDelegate {
 public:
  MOCK_METHOD2(HandleContextMenu,
               bool(RenderFrameHost*, const ContextMenuParams&));
  MOCK_METHOD4(RegisterProtocolHandler,
               void(RenderFrameHost*, const std::string&, const GURL&, bool));
};

}  // namespace

TEST_F(WebContentsImplTest, HandleContextMenuDelegate) {
  MockWebContentsDelegate delegate;
  contents()->SetDelegate(&delegate);

  RenderFrameHost* rfh = main_test_rfh();
  EXPECT_CALL(delegate, HandleContextMenu(rfh, ::testing::_))
      .WillOnce(::testing::Return(true));

  ContextMenuParams params;
  contents()->ShowContextMenu(rfh, params);

  contents()->SetDelegate(nullptr);
}

TEST_F(WebContentsImplTest, RegisterProtocolHandlerDifferentOrigin) {
  MockWebContentsDelegate delegate;
  contents()->SetDelegate(&delegate);

  GURL url("https://www.google.com");
  GURL handler_url1("https://www.google.com/handler/%s");
  GURL handler_url2("https://www.example.com/handler/%s");

  contents()->NavigateAndCommit(url);

  // Only the first call to RegisterProtocolHandler should register because the
  // other call has a handler from a different origin.
  EXPECT_CALL(delegate, RegisterProtocolHandler(main_test_rfh(), "mailto",
                                                handler_url1, true))
      .Times(1);
  EXPECT_CALL(delegate, RegisterProtocolHandler(main_test_rfh(), "mailto",
                                                handler_url2, true))
      .Times(0);

  {
    contents()->RegisterProtocolHandler(main_test_rfh(), "mailto", handler_url1,
                                        base::string16(),
                                        /*user_gesture=*/true);
  }

  {
    contents()->RegisterProtocolHandler(main_test_rfh(), "mailto", handler_url2,
                                        base::string16(),
                                        /*user_gesture=*/true);
  }

  contents()->SetDelegate(nullptr);
}

TEST_F(WebContentsImplTest, RegisterProtocolHandlerDataURL) {
  MockWebContentsDelegate delegate;
  contents()->SetDelegate(&delegate);

  GURL data("data:text/html,<html><body><b>hello world</b></body></html>");
  GURL data_handler(data.spec() + "%s");

  contents()->NavigateAndCommit(data);

  // Data URLs should fail.
  EXPECT_CALL(delegate, RegisterProtocolHandler(contents()->GetMainFrame(),
                                                "mailto", data_handler, true))
      .Times(0);

  {
    contents()->RegisterProtocolHandler(main_test_rfh(), "mailto", data_handler,
                                        base::string16(),
                                        /*user_gesture=*/true);
  }

  contents()->SetDelegate(nullptr);
}

TEST_F(WebContentsImplTest, Bluetooth) {
  TestWebContentsObserver observer(contents());
  EXPECT_EQ(observer.num_is_connected_to_bluetooth_device_changed(), 0);
  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());

  contents()->TestIncrementBluetoothConnectedDeviceCount();
  EXPECT_EQ(observer.num_is_connected_to_bluetooth_device_changed(), 1);
  EXPECT_TRUE(observer.last_is_connected_to_bluetooth_device());
  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());

  contents()->TestDecrementBluetoothConnectedDeviceCount();
  EXPECT_EQ(observer.num_is_connected_to_bluetooth_device_changed(), 2);
  EXPECT_FALSE(observer.last_is_connected_to_bluetooth_device());
  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
}

TEST_F(WebContentsImplTest, FaviconURLsSet) {
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  const auto kFavicon =
      blink::mojom::FaviconURL(GURL("https://example.com/favicon.ico"),
                               blink::mojom::FaviconIconType::kFavicon, {});

  contents()->NavigateAndCommit(GURL("https://example.com"));
  EXPECT_EQ(0u, contents()->GetFaviconURLs().size());

  favicon_urls.push_back(blink::mojom::FaviconURL::New(kFavicon));
  contents()->UpdateFaviconURL(contents()->GetMainFrame(),
                               std::move(favicon_urls));
  EXPECT_EQ(1u, contents()->GetFaviconURLs().size());

  favicon_urls.push_back(blink::mojom::FaviconURL::New(kFavicon));
  favicon_urls.push_back(blink::mojom::FaviconURL::New(kFavicon));
  contents()->UpdateFaviconURL(contents()->GetMainFrame(),
                               std::move(favicon_urls));
  EXPECT_EQ(2u, contents()->GetFaviconURLs().size());

  favicon_urls.push_back(blink::mojom::FaviconURL::New(kFavicon));
  contents()->UpdateFaviconURL(contents()->GetMainFrame(),
                               std::move(favicon_urls));
  EXPECT_EQ(1u, contents()->GetFaviconURLs().size());
}

TEST_F(WebContentsImplTest, FaviconURLsResetWithNavigation) {
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      GURL("https://example.com/favicon.ico"),
      blink::mojom::FaviconIconType::kFavicon, std::vector<gfx::Size>()));

  contents()->NavigateAndCommit(GURL("https://example.com"));
  EXPECT_EQ(0u, contents()->GetFaviconURLs().size());

  contents()->UpdateFaviconURL(contents()->GetMainFrame(),
                               std::move(favicon_urls));
  EXPECT_EQ(1u, contents()->GetFaviconURLs().size());

  contents()->NavigateAndCommit(GURL("https://example.com/navigation.html"));
  EXPECT_EQ(0u, contents()->GetFaviconURLs().size());
}

}  // namespace content
