// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/download/public/common/download_url_parameters.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/content_web_ui_controller_factory.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/common/frame_messages.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/frame/sandbox_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/url_constants.h"

namespace content {
namespace {

class TestInterstitialPage;

class TestInterstitialPageDelegate : public InterstitialPageDelegate {
 public:
  explicit TestInterstitialPageDelegate(TestInterstitialPage* interstitial_page)
      : interstitial_page_(interstitial_page) {}
  void CommandReceived(const std::string& command) override;
  std::string GetHTMLContents() override { return std::string(); }
  void OnDontProceed() override;
  void OnProceed() override;

 private:
  TestInterstitialPage* interstitial_page_;
};

class TestInterstitialPage : public InterstitialPageImpl {
 public:
  enum InterstitialState {
    INVALID = 0,    // Hasn't yet been initialized.
    UNDECIDED,      // Initialized, but no decision taken yet.
    OKED,           // Proceed was called.
    CANCELED        // DontProceed was called.
  };

  class Delegate {
   public:
    virtual void TestInterstitialPageDeleted(
        TestInterstitialPage* interstitial) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // IMPORTANT NOTE: if you pass stack allocated values for |state| and
  // |deleted| (like all interstitial related tests do at this point), make sure
  // to create an instance of the TestInterstitialPageStateGuard class on the
  // stack in your test.  This will ensure that the TestInterstitialPage states
  // are cleared when the test finishes.
  // Not doing so will cause stack trashing if your test does not hide the
  // interstitial, as in such a case it will be destroyed in the test TearDown
  // method and will dereference the |deleted| local variable which by then is
  // out of scope.
  TestInterstitialPage(WebContentsImpl* contents,
                       bool new_navigation,
                       const GURL& url,
                       InterstitialState* state,
                       bool* deleted)
      : InterstitialPageImpl(
            contents,
            static_cast<RenderWidgetHostDelegate*>(contents),
            new_navigation, url, new TestInterstitialPageDelegate(this)),
        state_(state),
        deleted_(deleted),
        command_received_count_(0),
        delegate_(nullptr) {
    *state_ = UNDECIDED;
    *deleted_ = false;
  }

  ~TestInterstitialPage() override {
    if (deleted_)
      *deleted_ = true;
    if (delegate_)
      delegate_->TestInterstitialPageDeleted(this);
  }

  void OnDontProceed() {
    if (state_)
      *state_ = CANCELED;
  }
  void OnProceed() {
    if (state_)
      *state_ = OKED;
  }

  int command_received_count() const {
    return command_received_count_;
  }

  void TestDomOperationResponse(const std::string& json_string) {
    if (enabled())
      CommandReceived();
  }

  void TestDidNavigate(int nav_entry_id,
                       bool did_create_new_entry,
                       const GURL& url) {
    FrameHostMsg_DidCommitProvisionalLoad_Params params;
    InitNavigateParams(&params, nav_entry_id, did_create_new_entry,
                       url, ui::PAGE_TRANSITION_TYPED);
    DidNavigate(GetMainFrame()->GetRenderViewHost(), params);
  }

  void TestRenderViewTerminated(base::TerminationStatus status,
                                int error_code) {
    RenderViewTerminated(GetMainFrame()->GetRenderViewHost(), status,
                         error_code);
  }

  bool is_showing() const {
    return static_cast<TestRenderWidgetHostView*>(
               GetMainFrame()->GetRenderViewHost()->GetWidget()->GetView())
        ->is_showing();
  }

  void ClearStates() {
    state_ = nullptr;
    deleted_ = nullptr;
    delegate_ = nullptr;
  }

  void CommandReceived() {
    command_received_count_++;
  }

  void set_delegate(Delegate* delegate) {
    delegate_ = delegate;
  }

 protected:
  WebContentsView* CreateWebContentsView() override { return nullptr; }

 private:
  InterstitialState* state_;
  bool* deleted_;
  int command_received_count_;
  Delegate* delegate_;
};

void TestInterstitialPageDelegate::CommandReceived(const std::string& command) {
  interstitial_page_->CommandReceived();
}

void TestInterstitialPageDelegate::OnDontProceed() {
  interstitial_page_->OnDontProceed();
}

void TestInterstitialPageDelegate::OnProceed() {
  interstitial_page_->OnProceed();
}

class TestInterstitialPageStateGuard : public TestInterstitialPage::Delegate {
 public:
  explicit TestInterstitialPageStateGuard(
      TestInterstitialPage* interstitial_page)
      : interstitial_page_(interstitial_page) {
    DCHECK(interstitial_page_);
    interstitial_page_->set_delegate(this);
  }
  ~TestInterstitialPageStateGuard() override {
    if (interstitial_page_)
      interstitial_page_->ClearStates();
  }

  void TestInterstitialPageDeleted(
      TestInterstitialPage* interstitial) override {
    DCHECK(interstitial_page_ == interstitial);
    interstitial_page_ = nullptr;
  }

 private:
  TestInterstitialPage* interstitial_page_;
};

class WebContentsImplTestBrowserClient : public TestContentBrowserClient {
 public:
  WebContentsImplTestBrowserClient()
      : assign_site_for_url_(false),
        original_browser_client_(SetBrowserClientForTesting(this)) {}

  ~WebContentsImplTestBrowserClient() override {
    SetBrowserClientForTesting(original_browser_client_);
  }

  bool ShouldAssignSiteForURL(const GURL& url) override {
    return assign_site_for_url_;
  }

  void set_assign_site_for_url(bool assign) {
    assign_site_for_url_ = assign;
  }

 private:
  bool assign_site_for_url_;
  ContentBrowserClient* original_browser_client_;
};

class WebContentsImplTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    WebUIControllerFactory::RegisterFactory(
        ContentWebUIControllerFactory::GetInstance());
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

  bool has_video_wake_lock() {
    return contents()
        ->media_web_contents_observer()
        ->has_video_wake_lock_for_testing();
  }
};

class TestWebContentsObserver : public WebContentsObserver {
 public:
  explicit TestWebContentsObserver(WebContents* contents)
      : WebContentsObserver(contents),
        last_theme_color_(SK_ColorTRANSPARENT) {
  }
  ~TestWebContentsObserver() override {}

  void DidFinishLoad(RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    last_url_ = validated_url;
  }
  void DidFailLoad(RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description) override {
    last_url_ = validated_url;
  }

  void DidChangeThemeColor(SkColor theme_color) override {
    last_theme_color_ = theme_color;
  }

  const GURL& last_url() const { return last_url_; }
  SkColor last_theme_color() const { return last_theme_color_; }

 private:
  GURL last_url_;
  SkColor last_theme_color_;

  DISALLOW_COPY_AND_ASSIGN(TestWebContentsObserver);
};

// Pretends to be a normal browser that receives toggles and transitions to/from
// a fullscreened state.
class FakeFullscreenDelegate : public WebContentsDelegate {
 public:
  FakeFullscreenDelegate() : fullscreened_contents_(nullptr) {}
  ~FakeFullscreenDelegate() override {}

  void EnterFullscreenModeForTab(
      WebContents* web_contents,
      const GURL& origin,
      const blink::WebFullscreenOptions& options) override {
    fullscreened_contents_ = web_contents;
  }

  void ExitFullscreenModeForTab(WebContents* web_contents) override {
    fullscreened_contents_ = nullptr;
  }

  bool IsFullscreenForTabOrPending(
      const WebContents* web_contents) const override {
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

TEST_F(WebContentsImplTest, DontUseTitleFromPendingEntry) {
  const GURL kGURL("chrome://blah");
  controller().LoadURL(
      kGURL, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(base::string16(), contents()->GetTitle());

  // Also test setting title while the first navigation is still pending.
  const base::string16 title = base::ASCIIToUTF16("Initial Entry Title");
  contents()->UpdateTitle(main_test_rfh(), title, base::i18n::LEFT_TO_RIGHT);
  EXPECT_EQ(title, contents()->GetTitle());
}

TEST_F(WebContentsImplTest, UseTitleFromPendingEntryIfSet) {
  const GURL kGURL("chrome://blah");
  const base::string16 title = base::ASCIIToUTF16("My Title");
  controller().LoadURL(
      kGURL, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());

  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_EQ(kGURL, entry->GetURL());
  entry->SetTitle(title);

  EXPECT_EQ(title, contents()->GetTitle());
}

// Browser initiated navigations to view-source URLs of WebUI pages should work.
TEST_F(WebContentsImplTest, DirectNavigationToViewSourceWebUI) {
  NavigationControllerImpl& cont =
      static_cast<NavigationControllerImpl&>(controller());
  const GURL kGURL("view-source:chrome://blah");
  // NavigationControllerImpl rewrites view-source URLs, simulating that here.
  const GURL kRewrittenURL("chrome://blah");

  process()->sink().ClearMessages();

  // Use LoadURLWithParams instead of LoadURL, because the former properly
  // rewrites view-source:chrome://blah URLs to chrome://blah.
  NavigationController::LoadURLParams load_params(kGURL);
  load_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  load_params.extra_headers = "content-type: text/plain";
  load_params.load_type = NavigationController::LOAD_TYPE_DEFAULT;
  load_params.is_renderer_initiated = false;
  controller().LoadURLWithParams(load_params);

  NavigationRequest* request =
      main_test_rfh()->frame_tree_node()->navigation_request();
  CHECK(request);

  int entry_id = cont.GetPendingEntry()->GetUniqueID();
  // Did we get the expected message?
  EXPECT_TRUE(process()->sink().GetFirstMessageMatching(
      FrameMsg_EnableViewSourceMode::ID));

  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  InitNavigateParams(&params, entry_id, true, kRewrittenURL,
                     ui::PAGE_TRANSITION_TYPED);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SimulateCommitProcessed(
      request->navigation_handle()->GetNavigationId(),
      true /* was_successful */);
  main_test_rfh()->SendNavigateWithParams(&params,
                                          false /* was_within_same_document */);

  // This is the virtual URL.
  EXPECT_EQ(base::ASCIIToUTF16("view-source:chrome://blah"),
            contents()->GetTitle());

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

// Test that navigating across a site boundary creates a new RenderViewHost
// with a new SiteInstance.  Going back should do the same.
TEST_F(WebContentsImplTest, CrossSiteBoundaries) {
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
  int entry_id = controller().GetPendingEntry()->GetUniqueID();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url2, contents()->GetVisibleURL());
  TestRenderFrameHost* pending_rfh = contents()->GetPendingMainFrame();
  EXPECT_TRUE(pending_rfh->GetLastCommittedURL().is_empty());
  int pending_rvh_delete_count = 0;
  pending_rfh->GetRenderViewHost()->set_delete_counter(
      &pending_rvh_delete_count);

  // DidNavigate from the pending page.
  contents()->TestDidNavigateWithSequenceNumber(
      pending_rfh, entry_id, true, url2, Referrer(), ui::PAGE_TRANSITION_TYPED,
      false, 1, 1);
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
      orig_rfh->GetSiteInstance()));
  EXPECT_EQ(orig_rvh_delete_count, 0);

  // Going back should switch SiteInstances again.  The first SiteInstance is
  // stored in the NavigationEntry, so it should be the same as at the start.
  // We should use the same RFH as before, swapping it back in.
  auto back_navigation =
      NavigationSimulator::CreateHistoryNavigation(-1, contents());
  back_navigation->ReadyToCommit();
  entry_id = controller().GetPendingEntry()->GetUniqueID();
  TestRenderFrameHost* goback_rfh = contents()->GetPendingMainFrame();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());

  // DidNavigate from the back action.
  contents()->TestDidNavigateWithSequenceNumber(
      goback_rfh, entry_id, false, url2, Referrer(), ui::PAGE_TRANSITION_TYPED,
      false, 2, 0);
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(goback_rfh, main_test_rfh());
  EXPECT_EQ(instance1, contents()->GetSiteInstance());
  // There should be a proxy for the pending RFH SiteInstance.
  EXPECT_TRUE(contents()->GetRenderManagerForTesting()->
      GetRenderFrameProxyHost(pending_rfh->GetSiteInstance()));
  EXPECT_EQ(pending_rvh_delete_count, 0);
  pending_rfh->OnSwappedOut();

  // Close contents and ensure RVHs are deleted.
  DeleteContents();
  EXPECT_EQ(orig_rvh_delete_count, 1);
  EXPECT_EQ(pending_rvh_delete_count, 1);
}

// Test that navigating across a site boundary after a crash creates a new
// RFH without requiring a cross-site transition (i.e., PENDING state).
TEST_F(WebContentsImplTest, CrossSiteBoundariesAfterCrash) {
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
  const GURL url2a("http://www.yahoo.com");
  controller().LoadURL(url2a, Referrer(), ui::PAGE_TRANSITION_LINK,
                       std::string());
  int entry_id = controller().GetPendingEntry()->GetUniqueID();
  contents()->GetMainFrame()->PrepareForCommit();
  TestRenderFrameHost* pending_rfh_a = contents()->GetPendingMainFrame();
  if (AreAllSitesIsolatedForTesting())
    EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  contents()->TestDidNavigate(pending_rfh_a, entry_id, true, url2a,
                              ui::PAGE_TRANSITION_LINK);
  SiteInstance* instance2a = contents()->GetSiteInstance();
  EXPECT_NE(instance1, instance2a);

  // Navigate second contents to the same site as the first tab.
  const GURL url2b("http://mail.yahoo.com");
  contents2->GetController().LoadURL(url2b, Referrer(),
                                     ui::PAGE_TRANSITION_LINK, std::string());
  entry_id = contents2->GetController().GetPendingEntry()->GetUniqueID();
  TestRenderFrameHost* rfh2 = contents2->GetMainFrame();
  rfh2->PrepareForCommit();
  TestRenderFrameHost* pending_rfh_b = contents2->GetPendingMainFrame();
  EXPECT_NE(nullptr, pending_rfh_b);
  if (AreAllSitesIsolatedForTesting())
    EXPECT_TRUE(contents2->CrossProcessNavigationPending());

  // NOTE(creis): We used to be in danger of showing a crash page here if the
  // second contents hadn't navigated somewhere first (bug 1145430).  That case
  // is now covered by the CrossSiteBoundariesAfterCrash test.
  contents2->TestDidNavigate(pending_rfh_b, entry_id, true, url2b,
                             ui::PAGE_TRANSITION_LINK);
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

  browser_client.set_assign_site_for_url(false);
  // Navigate to an URL that will not assign a new SiteInstance.
  const GURL native_url("non-site-url://stuffandthings");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), native_url);

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());
  EXPECT_EQ(native_url, contents()->GetLastCommittedURL());
  EXPECT_EQ(native_url, contents()->GetVisibleURL());
  EXPECT_EQ(orig_instance, contents()->GetSiteInstance());
  EXPECT_EQ(GURL(), contents()->GetSiteInstance()->GetSiteURL());
  EXPECT_FALSE(orig_instance->HasSite());

  browser_client.set_assign_site_for_url(true);
  // Navigate to new site (should keep same site instance).
  const GURL url("http://www.google.com");
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(url, contents());
  navigation->ReadyToCommit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(native_url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url, contents()->GetVisibleURL());
  EXPECT_FALSE(contents()->GetPendingMainFrame());
  navigation->Commit();

  // Keep the number of active frames in orig_rfh's SiteInstance
  // non-zero so that orig_rfh doesn't get deleted when it gets
  // swapped out.
  orig_rfh->GetSiteInstance()->IncrementActiveFrameCount();

  EXPECT_EQ(orig_instance, contents()->GetSiteInstance());
  EXPECT_TRUE(
      contents()->GetSiteInstance()->GetSiteURL().DomainIs("google.com"));
  EXPECT_EQ(url, contents()->GetLastCommittedURL());

  // Navigate to another new site (should create a new site instance).
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller().GetPendingEntry()->GetUniqueID();
  orig_rfh->PrepareForCommit();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url2, contents()->GetVisibleURL());
  TestRenderFrameHost* pending_rfh = contents()->GetPendingMainFrame();
  int pending_rvh_delete_count = 0;
  pending_rfh->GetRenderViewHost()->set_delete_counter(
      &pending_rvh_delete_count);

  // DidNavigate from the pending page.
  contents()->TestDidNavigate(pending_rfh, entry_id, true, url2,
                              ui::PAGE_TRANSITION_TYPED);
  SiteInstance* new_instance = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(pending_rfh, main_test_rfh());
  EXPECT_EQ(url2, contents()->GetLastCommittedURL());
  EXPECT_EQ(url2, contents()->GetVisibleURL());
  EXPECT_NE(new_instance, orig_instance);
  EXPECT_FALSE(contents()->GetPendingMainFrame());
  // We keep a proxy for the original RFH's SiteInstance.
  EXPECT_TRUE(contents()->GetRenderManagerForTesting()->GetRenderFrameProxyHost(
      orig_rfh->GetSiteInstance()));
  EXPECT_EQ(orig_rvh_delete_count, 0);
  orig_rfh->OnSwappedOut();

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
  browser_client.set_assign_site_for_url(false);
  const GURL native_url("non-site-url://stuffandthings");
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  std::unique_ptr<NavigationEntry> new_entry =
      NavigationController::CreateNavigationEntry(
          native_url, Referrer(), ui::PAGE_TRANSITION_LINK, false,
          std::string(), browser_context(),
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
  browser_client.set_assign_site_for_url(true);
  const GURL url("http://www.google.com");
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
  browser_client.set_assign_site_for_url(true);
  const GURL regular_url("http://www.yahoo.com");
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  std::unique_ptr<NavigationEntry> new_entry =
      NavigationController::CreateNavigationEntry(
          regular_url, Referrer(), ui::PAGE_TRANSITION_LINK, false,
          std::string(), browser_context(),
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

  // Navigate to another site and verify that a new SiteInstance was created.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);
  EXPECT_NE(orig_instance, contents()->GetSiteInstance());

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
  int opener_frame_routing_id =
      popup->GetRenderManager()->GetOpenerRoutingID(instance);
  RenderFrameProxyHost* proxy =
      contents()->GetRenderManager()->GetRenderFrameProxyHost(instance);
  EXPECT_TRUE(proxy);
  EXPECT_EQ(proxy->GetRoutingID(), opener_frame_routing_id);

  // Ensure that committing the navigation removes the proxy.
  navigation->Commit();
  EXPECT_FALSE(
      contents()->GetRenderManager()->GetRenderFrameProxyHost(instance));
}

// Tests that WebContentsImpl uses the current URL, not the SiteInstance's site,
// to determine whether a navigation is cross-site.
TEST_F(WebContentsImplTest, CrossSiteComparesAgainstCurrentPage) {
  // The assumptions this test makes aren't valid with --site-per-process.  For
  // example, a cross-site URL won't ever commit in the old RFH.
  if (AreAllSitesIsolatedForTesting())
    return;

  TestRenderFrameHost* orig_rfh = main_test_rfh();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.
  const GURL url("http://www.google.com");
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
  EXPECT_NE(instance1, instance2);
  EXPECT_FALSE(contents2->CrossProcessNavigationPending());

  // Simulate a link click in first contents to second site.  Doesn't switch
  // SiteInstances, because we don't intercept Blink navigations.
  NavigationSimulator::NavigateAndCommitFromDocument(url2, orig_rfh);
  SiteInstance* instance3 = contents()->GetSiteInstance();
  EXPECT_EQ(instance1, instance3);
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());

  // Navigate to the new site.  Doesn't switch SiteInstancees, because we
  // compare against the current URL, not the SiteInstance's site.
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
  EXPECT_TRUE(orig_rfh->is_waiting_for_beforeunload_ack());
  base::TimeTicks now = base::TimeTicks::Now();
  orig_rfh->OnMessageReceived(
      FrameHostMsg_BeforeUnload_ACK(0, false, now, now));
  EXPECT_FALSE(orig_rfh->is_waiting_for_beforeunload_ack());
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());

  // Navigate again, but simulate an onbeforeunload approval.
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller().GetPendingEntry()->GetUniqueID();
  EXPECT_TRUE(orig_rfh->is_waiting_for_beforeunload_ack());
  now = base::TimeTicks::Now();
  orig_rfh->PrepareForCommit();
  EXPECT_FALSE(orig_rfh->is_waiting_for_beforeunload_ack());
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  TestRenderFrameHost* pending_rfh = contents()->GetPendingMainFrame();

  // We won't hear DidNavigate until the onunload handler has finished running.

  // DidNavigate from the pending page.
  contents()->TestDidNavigate(pending_rfh, entry_id, true, url2,
                              ui::PAGE_TRANSITION_TYPED);
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

  // Navigate to new site, simulating an onbeforeunload approval.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(orig_rfh->is_waiting_for_beforeunload_ack());
  orig_rfh->PrepareForCommit();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());

  // Suppose the original renderer navigates before the new one is ready.
  orig_rfh->SendNavigate(0, true, GURL("http://www.google.com/foo"));

  // Verify that the pending navigation is cancelled.
  EXPECT_FALSE(orig_rfh->is_waiting_for_beforeunload_ack());
  SiteInstance* instance2 = contents()->GetSiteInstance();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());
  EXPECT_EQ(instance1, instance2);
  EXPECT_EQ(nullptr, contents()->GetPendingMainFrame());
}

// Tests that if we go back twice (same-site then cross-site), and the same-site
// RFH commits first, the cross-site RFH's navigation is canceled.
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
  EXPECT_EQ(google_rfh, main_test_rfh());
  EXPECT_EQ(instance2, instance3);
  EXPECT_FALSE(contents()->GetPendingMainFrame());
  EXPECT_EQ(url3, entry3->GetURL());
  EXPECT_EQ(instance3,
            NavigationEntryImpl::FromNavigationEntry(entry3)->site_instance());

  // Go back within the site.
  controller().GoBack();
  NavigationEntry* goback_entry = controller().GetPendingEntry();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(entry2, controller().GetPendingEntry());

  // Before that commits, go back again.
  controller().GoBack();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_TRUE(contents()->GetPendingMainFrame());
  EXPECT_EQ(entry1, controller().GetPendingEntry());

  // Simulate beforeunload approval.
  EXPECT_TRUE(google_rfh->is_waiting_for_beforeunload_ack());
  base::TimeTicks now = base::TimeTicks::Now();
  google_rfh->PrepareForCommit();
  google_rfh->OnMessageReceived(
      FrameHostMsg_BeforeUnload_ACK(0, true, now, now));

  // DidNavigate from the first back. This aborts the second back's pending RFH.
  contents()->TestDidNavigateWithSequenceNumber(
      google_rfh, goback_entry->GetUniqueID(), false, url2, Referrer(),
      ui::PAGE_TRANSITION_TYPED, false, 1, 1);

  // We should commit this page and forget about the second back.
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_FALSE(controller().GetPendingEntry());
  EXPECT_EQ(google_rfh, main_test_rfh());
  EXPECT_EQ(url2, controller().GetLastCommittedEntry()->GetURL());

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
  EXPECT_EQ(google_rfh, main_test_rfh());
  EXPECT_EQ(instance2, instance3);
  EXPECT_FALSE(contents()->GetPendingMainFrame());
  EXPECT_EQ(url3, entry3->GetURL());
  EXPECT_EQ(instance3,
            NavigationEntryImpl::FromNavigationEntry(entry3)->site_instance());

  // Go back within the site.
  controller().GoBack();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(entry2, controller().GetPendingEntry());

  // Before that commits, go back again.
  controller().GoBack();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_TRUE(contents()->GetPendingMainFrame());
  EXPECT_EQ(entry1, controller().GetPendingEntry());
  webui_rfh = contents()->GetPendingMainFrame();

  // DidNavigate from the second back.
  contents()->TestDidNavigate(webui_rfh, entry1->GetUniqueID(), false, url1,
                              ui::PAGE_TRANSITION_TYPED);

  // That should have landed us on the first entry.
  EXPECT_EQ(entry1, controller().GetLastCommittedEntry());

  // When the second back commits, it should be ignored.
  contents()->TestDidNavigate(google_rfh, entry2->GetUniqueID(), false, url2,
                              ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(entry1, controller().GetLastCommittedEntry());
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
  EXPECT_TRUE(orig_rfh->is_waiting_for_beforeunload_ack());

  // Now simulate the onbeforeunload approval and verify the navigation is
  // not canceled.
  orig_rfh->PrepareForCommit();
  EXPECT_FALSE(orig_rfh->is_waiting_for_beforeunload_ack());
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
}

namespace {
void SetAsNonUserGesture(FrameHostMsg_DidCommitProvisionalLoad_Params* params) {
  params->gesture = NavigationGestureAuto;
}
}  // namespace

// Test that a cross-site navigation is not preempted if the previous
// renderer sends a FrameNavigate message just before being told to stop.
// We should only preempt the cross-site navigation if the previous renderer
// has started a new navigation.  See http://crbug.com/79176.
TEST_F(WebContentsImplTest, CrossSiteNotPreemptedDuringBeforeUnload) {
  // Navigate to WebUI URL.
  const GURL url("chrome://gpu");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry1_id = controller().GetPendingEntry()->GetUniqueID();
  TestRenderFrameHost* orig_rfh = main_test_rfh();
  orig_rfh->PrepareForCommit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());

  // Navigate to new site, with the beforeunload request in flight.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry2_id = controller().GetPendingEntry()->GetUniqueID();
  TestRenderFrameHost* pending_rfh = contents()->GetPendingMainFrame();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_TRUE(orig_rfh->is_waiting_for_beforeunload_ack());
  EXPECT_NE(orig_rfh, pending_rfh);

  // Suppose the first navigation tries to commit now, with a
  // FrameMsg_Stop in flight.  This should not cancel the pending navigation,
  // but it should act as if the beforeunload ack arrived.
  orig_rfh->SendNavigateWithModificationCallback(
      entry1_id, true, url, base::Bind(SetAsNonUserGesture));
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());
  EXPECT_FALSE(orig_rfh->is_waiting_for_beforeunload_ack());
  // It should commit.
  ASSERT_EQ(1, controller().GetEntryCount());
  EXPECT_EQ(url, controller().GetLastCommittedEntry()->GetURL());

  // The pending navigation should be able to commit successfully.
  contents()->TestDidNavigate(pending_rfh, entry2_id, true, url2,
                              ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(pending_rfh, main_test_rfh());
  EXPECT_EQ(2, controller().GetEntryCount());
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
  EXPECT_FALSE(contents()->IsFullscreenForCurrentTab());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));
  orig_rfh->OnMessageReceived(FrameHostMsg_EnterFullscreen(
      orig_rfh->GetRoutingID(), blink::WebFullscreenOptions()));
  EXPECT_TRUE(contents()->IsFullscreenForCurrentTab());
  EXPECT_TRUE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  // Navigate to a new site.
  const GURL url2("http://www.yahoo.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url2);

  // Confirm fullscreen has exited.
  EXPECT_FALSE(contents()->IsFullscreenForCurrentTab());
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
  EXPECT_EQ(orig_rfh, main_test_rfh());

  // Sanity-check: Confirm we're not starting out in fullscreen mode.
  EXPECT_FALSE(contents()->IsFullscreenForCurrentTab());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  for (int i = 0; i < 2; ++i) {
    // Toggle fullscreen mode on (as if initiated via IPC from renderer).
    orig_rfh->OnMessageReceived(FrameHostMsg_EnterFullscreen(
        orig_rfh->GetRoutingID(), blink::WebFullscreenOptions()));
    EXPECT_TRUE(contents()->IsFullscreenForCurrentTab());
    EXPECT_TRUE(fake_delegate.IsFullscreenForTabOrPending(contents()));

    // Navigate backward (or forward).
    if (i == 0)
      NavigationSimulator::GoBack(contents());
    else
      NavigationSimulator::GoForward(contents());

    // Confirm fullscreen has exited.
    EXPECT_FALSE(contents()->IsFullscreenForCurrentTab());
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
  EXPECT_FALSE(contents()->IsFullscreenForCurrentTab());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));
  main_test_rfh()->OnMessageReceived(FrameHostMsg_EnterFullscreen(
      main_test_rfh()->GetRoutingID(), blink::WebFullscreenOptions()));
  EXPECT_TRUE(contents()->IsFullscreenForCurrentTab());
  EXPECT_TRUE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  // Crash the renderer.
  main_test_rfh()->GetProcess()->SimulateCrash();

  // Confirm fullscreen has exited.
  EXPECT_FALSE(contents()->IsFullscreenForCurrentTab());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  contents()->SetDelegate(nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// Interstitial Tests
////////////////////////////////////////////////////////////////////////////////

// Test navigating to a page (with the navigation initiated from the browser,
// as when a URL is typed in the location bar) that shows an interstitial and
// creates a new navigation entry, then hiding it without proceeding.
TEST_F(WebContentsImplTest,
       ShowInterstitialFromBrowserWithNewNavigationDontProceed) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Initiate a browser navigation that will trigger the interstitial.
  controller().LoadURL(GURL("http://www.evil.com"), Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());
  NavigationEntry* entry = controller().GetPendingEntry();

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  int interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_EQ(nullptr, contents()->GetInterstitialPage());
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(interstitial_entry_id, true, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);
  entry = controller().GetVisibleEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_TRUE(entry->GetURL() == url2);

  // Now don't proceed.
  interstitial->DontProceed();
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_EQ(nullptr, contents()->GetInterstitialPage());
  entry = controller().GetVisibleEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_TRUE(entry->GetURL() == url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page (with the navigation initiated from the renderer,
// as when clicking on a link in the page) that shows an interstitial and
// creates a new navigation entry, then hiding it without proceeding.
TEST_F(WebContentsImplTest,
       ShowInterstitialFromRendererWithNewNavigationDontProceed) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial (no pending entry, the interstitial would have been
  // triggered by clicking on a link).
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  int interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_EQ(nullptr, contents()->GetInterstitialPage());
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(interstitial_entry_id, true, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_TRUE(entry->GetURL() == url2);

  // Now don't proceed.
  interstitial->DontProceed();
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_EQ(nullptr, contents()->GetInterstitialPage());
  entry = controller().GetVisibleEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_TRUE(entry->GetURL() == url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial without creating a new
// navigation entry (this happens when the interstitial is triggered by a
// sub-resource in the page), then hiding it without proceeding.
TEST_F(WebContentsImplTest, ShowInterstitialNoNewNavigationDontProceed) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), false, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_EQ(nullptr, contents()->GetInterstitialPage());
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(0, true, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_NE(nullptr, entry);
  // The URL specified to the interstitial should have been ignored.
  EXPECT_TRUE(entry->GetURL() == url1);

  // Now don't proceed.
  interstitial->DontProceed();
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_EQ(nullptr, contents()->GetInterstitialPage());
  entry = controller().GetVisibleEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_TRUE(entry->GetURL() == url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page (with the navigation initiated from the browser,
// as when a URL is typed in the location bar) that shows an interstitial and
// creates a new navigation entry, then proceeding.
TEST_F(WebContentsImplTest,
       ShowInterstitialFromBrowserNewNavigationProceed) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Initiate a browser navigation that will trigger the interstitial
  controller().LoadURL(GURL("http://www.evil.com"), Referrer(),
                        ui::PAGE_TRANSITION_TYPED, std::string());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  int interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_EQ(nullptr, contents()->GetInterstitialPage());
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(interstitial_entry_id, true, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_TRUE(entry->GetURL() == url2);

  // Then proceed.
  interstitial->Proceed();
  // The interstitial should show until the new navigation commits.
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(deleted);
  EXPECT_EQ(TestInterstitialPage::OKED, state);
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);

  // Simulate the navigation to the page, that's when the interstitial gets
  // hidden.
  GURL url3("http://www.thepage.com");
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(0, true, url3);

  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_EQ(nullptr, contents()->GetInterstitialPage());
  entry = controller().GetVisibleEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_TRUE(entry->GetURL() == url3);

  EXPECT_EQ(2, controller().GetEntryCount());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page (with the navigation initiated from the renderer,
// as when clicking on a link in the page) that shows an interstitial and
// creates a new navigation entry, then proceeding.
TEST_F(WebContentsImplTest,
       ShowInterstitialFromRendererNewNavigationProceed) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  int interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_EQ(nullptr, contents()->GetInterstitialPage());
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(interstitial_entry_id, true, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_TRUE(entry->GetURL() == url2);

  // Then proceed.
  interstitial->Proceed();
  // The interstitial should show until the new navigation commits.
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(deleted);
  EXPECT_EQ(TestInterstitialPage::OKED, state);
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);

  // Simulate the navigation to the page, that's when the interstitial gets
  // hidden.
  GURL url3("http://www.thepage.com");
  NavigationSimulator::NavigateAndCommitFromDocument(url3, main_test_rfh());

  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_EQ(nullptr, contents()->GetInterstitialPage());
  entry = controller().GetVisibleEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_TRUE(entry->GetURL() == url3);

  EXPECT_EQ(2, controller().GetEntryCount());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial without creating a new
// navigation entry (this happens when the interstitial is triggered by a
// sub-resource in the page), then proceeding.
TEST_F(WebContentsImplTest, ShowInterstitialNoNewNavigationProceed) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL url1("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), false, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_EQ(nullptr, contents()->GetInterstitialPage());
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(0, true, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_NE(nullptr, entry);
  // The URL specified to the interstitial should have been ignored.
  EXPECT_TRUE(entry->GetURL() == url1);

  // Then proceed.
  interstitial->Proceed();
  // Since this is not a new navigation, the previous page is dismissed right
  // away and shows the original page.
  EXPECT_EQ(TestInterstitialPage::OKED, state);
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_EQ(nullptr, contents()->GetInterstitialPage());
  entry = controller().GetVisibleEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_TRUE(entry->GetURL() == url1);

  EXPECT_EQ(1, controller().GetEntryCount());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial, then navigating away.
TEST_F(WebContentsImplTest, ShowInterstitialThenNavigate) {
  // Show interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  int interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();
  interstitial->TestDidNavigate(interstitial_entry_id, true, url);

  // While interstitial showing, navigate to a new URL.
  const GURL url2("http://www.yahoo.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url2);

  EXPECT_EQ(TestInterstitialPage::CANCELED, state);

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial, then going back.
TEST_F(WebContentsImplTest, ShowInterstitialThenGoBack) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL url1("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL interstitial_url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, interstitial_url,
                               &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  int interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();
  interstitial->TestDidNavigate(interstitial_entry_id, true,
                                interstitial_url);
  EXPECT_EQ(2, controller().GetEntryCount());

  // While the interstitial is showing, go back. This will dismiss the
  // interstitial and not initiate a navigation, but just show the existing
  // RenderFrameHost.
  controller().GoBack();

  // Make sure we are back to the original page and that the interstitial is
  // gone.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(url1.spec(), entry->GetURL().spec());
  EXPECT_EQ(1, controller().GetEntryCount());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial, has a renderer crash,
// and then goes back.
TEST_F(WebContentsImplTest, ShowInterstitialCrashRendererThenGoBack) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL url1("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url1);
  EXPECT_EQ(1, controller().GetEntryCount());
  NavigationEntry* entry = controller().GetLastCommittedEntry();

  // Show interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL interstitial_url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, interstitial_url,
                               &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  int interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();
  interstitial->TestDidNavigate(interstitial_entry_id, true,
                                interstitial_url);

  // Crash the renderer
  main_test_rfh()->GetProcess()->SimulateCrash();

  // While the interstitial is showing, go back. This will dismiss the
  // interstitial and not initiate a navigation, but just show the existing
  // RenderFrameHost.
  controller().GoBack();

  // Make sure we are back to the original page and that the interstitial is
  // gone.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(url1.spec(), entry->GetURL().spec());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial, has the renderer crash,
// and then navigates to the interstitial.
TEST_F(WebContentsImplTest, ShowInterstitialCrashRendererThenNavigate) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL url1("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL interstitial_url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, interstitial_url,
                               &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  int interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();

  // Crash the renderer
  main_test_rfh()->GetProcess()->SimulateCrash();

  interstitial->TestDidNavigate(interstitial_entry_id, true,
                                interstitial_url);
}

// Test navigating to a page that shows an interstitial, then close the
// contents.
TEST_F(WebContentsImplTest, ShowInterstitialThenCloseTab) {
  // Show interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  int interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();
  interstitial->TestDidNavigate(interstitial_entry_id, true, url);

  // Now close the contents.
  DeleteContents();
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial, then close the
// contents.
TEST_F(WebContentsImplTest, ShowInterstitialThenCloseAndShutdown) {
  // Show interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  int interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();
  interstitial->TestDidNavigate(interstitial_entry_id, true, url);
  TestRenderFrameHost* rfh =
      static_cast<TestRenderFrameHost*>(interstitial->GetMainFrame());

  // Now close the contents.
  DeleteContents();
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);

  // Before the interstitial has a chance to process its shutdown task,
  // simulate quitting the browser.  This goes through all processes and
  // tells them to destruct.
  rfh->GetProcess()->SimulateCrash();

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test for https://crbug.com/730592, where deleting a WebContents while its
// interstitial is navigating could lead to a crash.
TEST_F(WebContentsImplTest, CreateInterstitialForClosingTab) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Initiate a browser navigation that will trigger an interstitial.
  controller().LoadURL(GURL("http://www.evil.com"), Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state = TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  TestRenderFrameHost* interstitial_rfh =
      static_cast<TestRenderFrameHost*>(interstitial->GetMainFrame());

  // Ensure the InterfaceProvider for the initial empty document is bound.
  interstitial_rfh->InitializeRenderFrameIfNeeded();

  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_EQ(nullptr, contents()->GetInterstitialPage());

  // Close the tab before the interstitial commits.
  DeleteContents();
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);

  // Simulate a commit in the interstitial page, which should not crash.
  interstitial_rfh->SimulateNavigationCommit(url2);

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test for https://crbug.com/703655, where navigating a tab and showing an
// interstitial could race.
TEST_F(WebContentsImplTest, TabNavigationDoesntRaceInterstitial) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Initiate a browser navigation that will trigger an interstitial.
  GURL evil_url("http://www.evil.com");
  controller().LoadURL(evil_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());
  NavigationEntry* entry = contents()->GetController().GetPendingEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(evil_url, entry->GetURL());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state = TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_EQ(nullptr, contents()->GetInterstitialPage());

  // At this point, there is an interstitial that has been instructed to show
  // but has not yet committed its own navigation. This is a window; navigate
  // back one page within this window.
  //
  // Because the page with the interstitial did not commit, this invokes an
  // early return in NavigationControllerImpl::NavigateToPendingEntry which just
  // drops the pending entry, so no committing is required.
  controller().GoBack();
  entry = contents()->GetController().GetPendingEntry();
  ASSERT_FALSE(entry);

  // The interstitial should be gone.
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test that after Proceed is called and an interstitial is still shown, no more
// commands get executed.
TEST_F(WebContentsImplTest, ShowInterstitialProceedMultipleCommands) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL url1("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  int interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();
  interstitial->TestDidNavigate(interstitial_entry_id, true, url2);

  // Run a command.
  EXPECT_EQ(0, interstitial->command_received_count());
  interstitial->TestDomOperationResponse("toto");
  EXPECT_EQ(1, interstitial->command_received_count());

  // Then proceed.
  interstitial->Proceed();
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(deleted);

  // While the navigation to the new page is pending, send other commands, they
  // should be ignored.
  interstitial->TestDomOperationResponse("hello");
  interstitial->TestDomOperationResponse("hi");
  EXPECT_EQ(1, interstitial->command_received_count());
}

// Test showing an interstitial while another interstitial is already showing.
TEST_F(WebContentsImplTest, ShowInterstitialOnInterstitial) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL start_url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), start_url);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state1 =
      TestInterstitialPage::INVALID;
  bool deleted1 = false;
  GURL url1("http://interstitial1");
  TestInterstitialPage* interstitial1 =
      new TestInterstitialPage(contents(), true, url1, &state1, &deleted1);
  TestInterstitialPageStateGuard state_guard1(interstitial1);
  interstitial1->Show();
  int interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();
  interstitial1->TestDidNavigate(interstitial_entry_id, true, url1);

  // Now show another interstitial.
  TestInterstitialPage::InterstitialState state2 =
      TestInterstitialPage::INVALID;
  bool deleted2 = false;
  GURL url2("http://interstitial2");
  TestInterstitialPage* interstitial2 =
      new TestInterstitialPage(contents(), true, url2, &state2, &deleted2);
  TestInterstitialPageStateGuard state_guard2(interstitial2);
  interstitial2->Show();
  interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();
  interstitial2->TestDidNavigate(interstitial_entry_id, true, url2);

  // Showing interstitial2 should have caused interstitial1 to go away.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state1);
  EXPECT_EQ(TestInterstitialPage::UNDECIDED, state2);

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted1);
  ASSERT_FALSE(deleted2);

  // Let's make sure interstitial2 is working as intended.
  interstitial2->Proceed();
  GURL landing_url("http://www.thepage.com");
  main_test_rfh()->SendNavigate(0, true, landing_url);

  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_EQ(nullptr, contents()->GetInterstitialPage());
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_TRUE(entry->GetURL() == landing_url);
  EXPECT_EQ(2, controller().GetEntryCount());
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted2);
}

// Test showing an interstitial, proceeding and then navigating to another
// interstitial.
TEST_F(WebContentsImplTest, ShowInterstitialProceedShowInterstitial) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL start_url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), start_url);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state1 =
      TestInterstitialPage::INVALID;
  bool deleted1 = false;
  GURL url1("http://interstitial1");
  TestInterstitialPage* interstitial1 =
      new TestInterstitialPage(contents(), true, url1, &state1, &deleted1);
  TestInterstitialPageStateGuard state_guard1(interstitial1);
  interstitial1->Show();
  int interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();
  interstitial1->TestDidNavigate(interstitial_entry_id, true, url1);

  // Take action.  The interstitial won't be hidden until the navigation is
  // committed.
  interstitial1->Proceed();
  EXPECT_EQ(TestInterstitialPage::OKED, state1);

  // Now show another interstitial (simulating the navigation causing another
  // interstitial).
  TestInterstitialPage::InterstitialState state2 =
      TestInterstitialPage::INVALID;
  bool deleted2 = false;
  GURL url2("http://interstitial2");
  TestInterstitialPage* interstitial2 =
      new TestInterstitialPage(contents(), true, url2, &state2, &deleted2);
  TestInterstitialPageStateGuard state_guard2(interstitial2);
  interstitial2->Show();
  interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();
  interstitial2->TestDidNavigate(interstitial_entry_id, true, url2);

  // Showing interstitial2 should have caused interstitial1 to go away.
  EXPECT_EQ(TestInterstitialPage::UNDECIDED, state2);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted1);
  ASSERT_FALSE(deleted2);

  // Let's make sure interstitial2 is working as intended.
  interstitial2->Proceed();
  GURL landing_url("http://www.thepage.com");
  main_test_rfh()->SendNavigate(0, true, landing_url);

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted2);
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_EQ(nullptr, contents()->GetInterstitialPage());
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_TRUE(entry->GetURL() == landing_url);
  EXPECT_EQ(2, controller().GetEntryCount());
}

// Test that navigating away from an interstitial while it's loading cause it
// not to show.
TEST_F(WebContentsImplTest, NavigateBeforeInterstitialShows) {
  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL interstitial_url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, interstitial_url,
                               &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  int interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();

  // Let's simulate a navigation initiated from the browser before the
  // interstitial finishes loading.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_FALSE(interstitial->is_showing());
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(deleted);

  // Now let's make the interstitial navigation commit.
  interstitial->TestDidNavigate(interstitial_entry_id, true,
                                interstitial_url);

  // After it loaded the interstitial should be gone.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test that a new request to show an interstitial while an interstitial is
// pending does not cause problems. htp://crbug/29655 and htp://crbug/9442.
TEST_F(WebContentsImplTest, TwoQuickInterstitials) {
  GURL interstitial_url("http://interstitial");

  // Show a first interstitial.
  TestInterstitialPage::InterstitialState state1 =
      TestInterstitialPage::INVALID;
  bool deleted1 = false;
  TestInterstitialPage* interstitial1 =
      new TestInterstitialPage(contents(), true, interstitial_url,
                               &state1, &deleted1);
  TestInterstitialPageStateGuard state_guard1(interstitial1);
  interstitial1->Show();

  // Show another interstitial on that same contents before the first one had
  // time to load.
  TestInterstitialPage::InterstitialState state2 =
      TestInterstitialPage::INVALID;
  bool deleted2 = false;
  TestInterstitialPage* interstitial2 =
      new TestInterstitialPage(contents(), true, interstitial_url,
                               &state2, &deleted2);
  TestInterstitialPageStateGuard state_guard2(interstitial2);
  interstitial2->Show();
  int interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();

  // The first interstitial should have been closed and deleted.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state1);
  // The 2nd one should still be OK.
  EXPECT_EQ(TestInterstitialPage::UNDECIDED, state2);

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted1);
  ASSERT_FALSE(deleted2);

  // Make the interstitial navigation commit it should be showing.
  interstitial2->TestDidNavigate(interstitial_entry_id, true,
                                 interstitial_url);
  EXPECT_EQ(interstitial2, contents()->GetInterstitialPage());
}

// Test showing an interstitial and have its renderer crash.
TEST_F(WebContentsImplTest, InterstitialCrasher) {
  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // Simulate a renderer crash before the interstitial is shown.
  interstitial->TestRenderViewTerminated(
      base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  // The interstitial should have been dismissed.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);

  // Now try again but this time crash the interstitial after it was shown.
  interstitial =
      new TestInterstitialPage(contents(), true, url, &state, &deleted);
  interstitial->Show();
  int interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();
  interstitial->TestDidNavigate(interstitial_entry_id, true, url);
  // Simulate a renderer crash.
  interstitial->TestRenderViewTerminated(
      base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  // The interstitial should have been dismissed.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Tests that showing an interstitial as a result of a browser initiated
// navigation while an interstitial is showing does not remove the pending
// entry (see http://crbug.com/9791).
TEST_F(WebContentsImplTest, NewInterstitialDoesNotCancelPendingEntry) {
  const char kUrl[] = "http://www.badguys.com/";
  const GURL kGURL(kUrl);

  // Start a navigation to a page
  contents()->GetController().LoadURL(
      kGURL, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());

  // Simulate that navigation triggering an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, kGURL, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  int interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();
  interstitial->TestDidNavigate(interstitial_entry_id, true, kGURL);

  // Initiate a new navigation from the browser that also triggers an
  // interstitial.
  contents()->GetController().LoadURL(
      kGURL, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  TestInterstitialPage::InterstitialState state2 =
      TestInterstitialPage::INVALID;
  bool deleted2 = false;
  TestInterstitialPage* interstitial2 =
      new TestInterstitialPage(contents(), true, kGURL, &state2, &deleted2);
  TestInterstitialPageStateGuard state_guard2(interstitial2);
  interstitial2->Show();
  interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();
  interstitial2->TestDidNavigate(interstitial_entry_id, true, kGURL);

  // Make sure we still have an entry.
  NavigationEntry* entry = contents()->GetController().GetPendingEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(kUrl, entry->GetURL().spec());

  // And that the first interstitial is gone, but not the second.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  EXPECT_EQ(TestInterstitialPage::UNDECIDED, state2);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
  EXPECT_FALSE(deleted2);
}

// Tests that Javascript messages are not shown while an interstitial is
// showing.
TEST_F(WebContentsImplTest, NoJSMessageOnInterstitials) {
  const char kUrl[] = "http://www.badguys.com/";
  const GURL kGURL(kUrl);

  // Start a navigation to a page
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kGURL);

  // Simulate showing an interstitial while the page is showing.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, kGURL, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  int interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();
  interstitial->TestDidNavigate(interstitial_entry_id, true, kGURL);

  // While the interstitial is showing, let's simulate the hidden page
  // attempting to show a JS message.
  IPC::Message* dummy_message = new IPC::Message;
  contents()->RunJavaScriptDialog(
      main_test_rfh(), base::ASCIIToUTF16("This is an informative message"),
      base::ASCIIToUTF16("OK"), JAVASCRIPT_DIALOG_TYPE_ALERT, dummy_message);
  EXPECT_TRUE(contents()->last_dialog_suppressed_);
}

// Makes sure that if the source passed to CopyStateFromAndPrune has an
// interstitial it isn't copied over to the destination.
TEST_F(WebContentsImplTest, CopyStateFromAndPruneSourceInterstitial) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Initiate a browser navigation that will trigger the interstitial
  controller().LoadURL(GURL("http://www.evil.com"), Referrer(),
                        ui::PAGE_TRANSITION_TYPED, std::string());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  int interstitial_entry_id = controller().GetTransientEntry()->GetUniqueID();
  interstitial->TestDidNavigate(interstitial_entry_id, true, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_EQ(2, controller().GetEntryCount());

  // Create another NavigationController.
  GURL url3("http://foo2");
  std::unique_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  NavigationControllerImpl& other_controller = other_contents->GetController();
  other_contents->NavigateAndCommit(url3);
  other_contents->ExpectSetHistoryOffsetAndLength(1, 2);
  other_controller.CopyStateFromAndPrune(&controller(), false);

  // The merged controller should only have two entries: url1 and url2.
  ASSERT_EQ(2, other_controller.GetEntryCount());
  EXPECT_EQ(1, other_controller.GetCurrentEntryIndex());
  EXPECT_EQ(url1, other_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3, other_controller.GetEntryAtIndex(1)->GetURL());

  // And the merged controller shouldn't be showing an interstitial.
  EXPECT_FALSE(other_contents->ShowingInterstitialPage());
}

// Makes sure that CopyStateFromAndPrune cannot be called if the target is
// showing an interstitial.
TEST_F(WebContentsImplTest, CopyStateFromAndPruneTargetInterstitial) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  contents()->NavigateAndCommit(url1);

  // Create another NavigationController.
  std::unique_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  NavigationControllerImpl& other_controller = other_contents->GetController();

  // Navigate it to url2.
  GURL url2("http://foo2");
  other_contents->NavigateAndCommit(url2);

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url3("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(other_contents.get(), true, url3, &state,
                               &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  int interstitial_entry_id =
      other_controller.GetTransientEntry()->GetUniqueID();
  interstitial->TestDidNavigate(interstitial_entry_id, true, url3);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_EQ(2, other_controller.GetEntryCount());

  // Ensure that we do not allow calling CopyStateFromAndPrune when an
  // interstitial is showing in the target.
  EXPECT_FALSE(other_controller.CanPruneAllButLastCommitted());
}

// Regression test for http://crbug.com/168611 - the URLs passed by the
// DidFinishLoad and DidFailLoadWithError IPCs should get filtered.
TEST_F(WebContentsImplTest, FilterURLs) {
  TestWebContentsObserver observer(contents());

  // A navigation to about:whatever should always look like a navigation to
  // about:blank
  GURL url_normalized(url::kAboutBlankURL);
  GURL url_from_ipc("about:whatever");

  // We navigate the test WebContents to about:blank, since NavigateAndCommit
  // will use the given URL to create the NavigationEntry as well, and that
  // entry should contain the filtered URL.
  contents()->NavigateAndCommit(url_normalized);

  // Check that an IPC with about:whatever is correctly normalized.
  contents()->TestDidFinishLoad(url_from_ipc);

  EXPECT_EQ(url_normalized, observer.last_url());

  // Create and navigate another WebContents.
  std::unique_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  TestWebContentsObserver other_observer(other_contents.get());
  other_contents->NavigateAndCommit(url_normalized);

  // Check that an IPC with about:whatever is correctly normalized.
  other_contents->TestDidFailLoadWithError(url_from_ipc, 1, base::string16());
  EXPECT_EQ(url_normalized, other_observer.last_url());
}

// Test that if a pending contents is deleted before it is shown, we don't
// crash.
TEST_F(WebContentsImplTest, PendingContentsDestroyed) {
  auto other_contents = base::WrapUnique(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  content::TestWebContents* test_web_contents = other_contents.get();
  contents()->AddPendingContents(std::move(other_contents));
  RenderWidgetHost* widget =
      test_web_contents->GetMainFrame()->GetRenderWidgetHost();
  int process_id = widget->GetProcess()->GetID();
  int widget_id = widget->GetRoutingID();

  // TODO(erikchen): Fix ownership semantics of WebContents. Nothing should be
  // able to delete it beside from the owner. https://crbug.com/832879.
  delete test_web_contents;
  EXPECT_EQ(nullptr, contents()->GetCreatedWindow(process_id, widget_id));
}

TEST_F(WebContentsImplTest, PendingContentsShown) {
  auto other_contents = base::WrapUnique(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  content::TestWebContents* test_web_contents = other_contents.get();
  contents()->AddPendingContents(std::move(other_contents));

  RenderWidgetHost* widget =
      test_web_contents->GetMainFrame()->GetRenderWidgetHost();
  int process_id = widget->GetProcess()->GetID();
  int widget_id = widget->GetRoutingID();

  // The first call to GetCreatedWindow pops it off the pending list.
  EXPECT_EQ(test_web_contents,
            contents()->GetCreatedWindow(process_id, widget_id).get());
  // A second call should return nullptr, verifying that it's been forgotten.
  EXPECT_EQ(nullptr, contents()->GetCreatedWindow(process_id, widget_id));
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
  contents()->IncrementCapturerCount(gfx::Size());
  EXPECT_TRUE(contents()->IsBeingCaptured());
  EXPECT_EQ(gfx::Size(), contents()->GetPreferredSize());

  // Increment capturer count again, but with an overriding capture size.
  // Expect preferred size to now be overridden to the capture size.
  const gfx::Size capture_size(1280, 720);
  contents()->IncrementCapturerCount(capture_size);
  EXPECT_TRUE(contents()->IsBeingCaptured());
  EXPECT_EQ(capture_size, contents()->GetPreferredSize());

  // Increment capturer count a third time, but the expect that the preferred
  // size is still the first capture size.
  const gfx::Size another_capture_size(720, 480);
  contents()->IncrementCapturerCount(another_capture_size);
  EXPECT_TRUE(contents()->IsBeingCaptured());
  EXPECT_EQ(capture_size, contents()->GetPreferredSize());

  // Decrement capturer count twice, but expect the preferred size to still be
  // overridden.
  contents()->DecrementCapturerCount();
  contents()->DecrementCapturerCount();
  EXPECT_TRUE(contents()->IsBeingCaptured());
  EXPECT_EQ(capture_size, contents()->GetPreferredSize());

  // Decrement capturer count, and since the count has dropped to zero, the
  // original preferred size should be restored.
  contents()->DecrementCapturerCount();
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
  contents->IncrementCapturerCount(gfx::Size());
  contents->UpdateWebContentsVisibility(hidden_or_occluded);
  EXPECT_TRUE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());
  EXPECT_EQ(hidden_or_occluded, contents->GetVisibility());

  // Remove the capturer when the contents is hidden/occluded. |view| should be
  // hidden/occluded.
  contents->DecrementCapturerCount();
  if (hidden_or_occluded == Visibility::HIDDEN) {
    EXPECT_FALSE(view->is_showing());
  } else {
    EXPECT_TRUE(view->is_showing());
    EXPECT_TRUE(view->is_occluded());
  }

  // Add a capturer when the contents is hidden. |view| should be unoccluded.
  contents->IncrementCapturerCount(gfx::Size());
  EXPECT_FALSE(view->is_occluded());

  // Show the contents. The view should be visible.
  contents->UpdateWebContentsVisibility(Visibility::VISIBLE);
  EXPECT_TRUE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());
  EXPECT_EQ(Visibility::VISIBLE, contents->GetVisibility());

  // Remove the capturer when the contents is visible. The view should remain
  // visible.
  contents->DecrementCapturerCount();
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
      SyntheticWebMouseWheelEventBuilder::Build(0, 0, 0, 1, modifiers, false);
  EXPECT_FALSE(contents()->HandleWheelEvent(event));
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());

  // But whenever the ctrl modifier is applied zoom can be increased or
  // decreased. Except on MacOS where we never want to adjust zoom
  // with mousewheel.
  modifiers = WebInputEvent::kControlKey;
  event =
      SyntheticWebMouseWheelEventBuilder::Build(0, 0, 0, 1, modifiers, false);
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
  event =
      SyntheticWebMouseWheelEventBuilder::Build(0, 0, 2, -5, modifiers, false);
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
  event =
      SyntheticWebMouseWheelEventBuilder::Build(0, 0, 2, 0, modifiers, false);
  EXPECT_FALSE(contents()->HandleWheelEvent(event));
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());

  // Events containing precise scrolling deltas also shouldn't result in the
  // zoom being adjusted, to avoid accidental adjustments caused by
  // two-finger-scrolling on a touchpad.
  modifiers = WebInputEvent::kControlKey;
  event =
      SyntheticWebMouseWheelEventBuilder::Build(0, 0, 0, 5, modifiers, true);
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
  contents->GetController().LoadURL(GURL("http://a.com/1"),
                                    Referrer(),
                                    ui::PAGE_TRANSITION_TYPED,
                                    std::string());
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  contents->CommitPendingNavigation();
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL in the same site.
  contents->GetController().LoadURL(GURL("http://a.com/2"),
                                    Referrer(),
                                    ui::PAGE_TRANSITION_TYPED,
                                    std::string());
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  contents->CommitPendingNavigation();
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL in a different site in the same BrowsingInstance.
  const GURL kUrl2("http://b.com");
  contents->GetController().LoadURL(kUrl2, Referrer(), ui::PAGE_TRANSITION_LINK,
                                    std::string());
  int entry_id = contents->GetController().GetPendingEntry()->GetUniqueID();
  contents->GetMainFrame()->PrepareForCommit();
  if (AreAllSitesIsolatedForTesting())
    EXPECT_TRUE(contents->CrossProcessNavigationPending());
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  contents->GetPendingMainFrame()->SendNavigate(entry_id, true, kUrl2);
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL in a different site and different BrowsingInstance, by
  // using a TYPED page transition instead of LINK.
  const GURL kUrl3("http://c.com");
  contents->GetController().LoadURL(kUrl3, Referrer(),
                                    ui::PAGE_TRANSITION_TYPED, std::string());
  entry_id = contents->GetController().GetPendingEntry()->GetUniqueID();
  contents->GetMainFrame()->PrepareForCommit();
  EXPECT_TRUE(contents->CrossProcessNavigationPending());
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  scoped_refptr<SiteInstance> new_instance =
      contents->GetPendingMainFrame()->GetSiteInstance();
  contents->GetPendingMainFrame()->SendNavigate(entry_id, true, kUrl3);
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
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL with WebUI. This will change BrowsingInstances.
  const GURL kWebUIUrl = GURL("chrome://gpu");
  contents->GetController().LoadURL(kWebUIUrl,
                                    Referrer(),
                                    ui::PAGE_TRANSITION_TYPED,
                                    std::string());
  int entry_id = contents->GetController().GetPendingEntry()->GetUniqueID();
  contents->GetMainFrame()->PrepareForCommit();
  EXPECT_TRUE(contents->CrossProcessNavigationPending());
  scoped_refptr<SiteInstance> instance_webui(
      contents->GetPendingMainFrame()->GetSiteInstance());
  EXPECT_FALSE(instance->IsRelatedSiteInstance(instance_webui.get()));

  // At this point, contents still counts for the old BrowsingInstance.
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  EXPECT_EQ(0u, instance_webui->GetRelatedActiveContentsCount());

  // Commit and contents counts for the new one.
  contents->GetPendingMainFrame()->SendNavigate(entry_id, true, kWebUIUrl);
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
  controller().LoadURL(
      main_url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller().GetPendingEntry()->GetUniqueID();

  main_test_rfh()->PrepareForCommit();
  contents()->TestDidNavigate(orig_rfh, entry_id, true, main_url,
                              ui::PAGE_TRANSITION_TYPED);
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
  {
    subframe->SendNavigateWithTransition(0, false, initial_url,
                                         ui::PAGE_TRANSITION_AUTO_SUBFRAME);
    subframe->OnMessageReceived(
        FrameHostMsg_DidStopLoading(subframe->GetRoutingID()));
  }

  // Navigate the frame to another URL, which will send again
  // DidStartLoading and DidStopLoading messages.
  NavigationSimulator::NavigateAndCommitFromDocument(foo_url, subframe);
  subframe->OnMessageReceived(
      FrameHostMsg_DidStopLoading(subframe->GetRoutingID()));

  // Since the main frame hasn't sent any DidStopLoading messages, it is
  // expected that the WebContents is still in loading state.
  EXPECT_TRUE(contents()->IsLoading());
  EXPECT_TRUE(observer.is_loading());
  EXPECT_TRUE(observer.did_receive_response());

  // Navigate the frame again, this time using LoadURLWithParams. This causes
  // RenderFrameHost to call into WebContents::DidStartLoading, which starts
  // the spinner.
  {
    NavigationController::LoadURLParams load_params(bar_url);
    load_params.referrer = Referrer(GURL("http://referrer"),
                                    network::mojom::ReferrerPolicy::kDefault);
    load_params.transition_type = ui::PAGE_TRANSITION_GENERATED;
    load_params.extra_headers = "content-type: text/plain";
    load_params.load_type = NavigationController::LOAD_TYPE_DEFAULT;
    load_params.is_renderer_initiated = false;
    load_params.override_user_agent = NavigationController::UA_OVERRIDE_TRUE;
    load_params.frame_tree_node_id =
        subframe->frame_tree_node()->frame_tree_node_id();
    controller().LoadURLWithParams(load_params);
    entry_id = controller().GetPendingEntry()->GetUniqueID();

    // Commit the navigation in the child frame and send the DidStopLoading
    // message.
    subframe->PrepareForCommit();
    contents()->TestDidNavigate(subframe, entry_id, true, bar_url,
                                ui::PAGE_TRANSITION_MANUAL_SUBFRAME);
    subframe->OnMessageReceived(
        FrameHostMsg_DidStopLoading(subframe->GetRoutingID()));
  }

  // At this point the status should still be loading, since the main frame
  // hasn't sent the DidstopLoading message yet.
  EXPECT_TRUE(contents()->IsLoading());
  EXPECT_TRUE(observer.is_loading());
  EXPECT_TRUE(observer.did_receive_response());

  // Send the DidStopLoading for the main frame and ensure it isn't loading
  // anymore.
  orig_rfh->OnMessageReceived(
      FrameHostMsg_DidStopLoading(orig_rfh->GetRoutingID()));
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
  controller().LoadURL(main_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());
  int entry_id = controller().GetPendingEntry()->GetUniqueID();

  main_test_rfh()->PrepareForCommit();
  contents()->TestDidNavigate(orig_rfh, entry_id, true, main_url,
                              ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());
  EXPECT_TRUE(contents()->IsLoading());
  EXPECT_TRUE(contents()->IsLoadingToDifferentDocument());

  // Send the DidStopLoading for the main frame and ensure it isn't loading
  // anymore.
  orig_rfh->OnMessageReceived(
      FrameHostMsg_DidStopLoading(orig_rfh->GetRoutingID()));
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
  subframe->OnMessageReceived(
      FrameHostMsg_DidStopLoading(subframe->GetRoutingID()));
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
  current_rfh->OnMessageReceived(
      FrameHostMsg_DidStopLoading(current_rfh->GetRoutingID()));
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
  new_current_rfh->OnMessageReceived(
      FrameHostMsg_DidStopLoading(new_current_rfh->GetRoutingID()));
  EXPECT_EQ(main_test_rfh(), new_current_rfh);
  EXPECT_FALSE(contents()->IsLoading());
}

TEST_F(WebContentsImplTest, MediaWakeLock) {
  // Verify that both negative and positive player ids don't blow up.
  const int kPlayerAudioVideoId = 15;
  const int kPlayerAudioOnlyId = -15;
  const int kPlayerVideoOnlyId = 30;
  const int kPlayerRemoteId = -30;

  EXPECT_FALSE(has_audio_wake_lock());
  EXPECT_FALSE(has_video_wake_lock());

  TestRenderFrameHost* rfh = main_test_rfh();
  AudioStreamMonitor* monitor = contents()->audio_stream_monitor();

  // Ensure RenderFrame is initialized before simulating events coming from it.
  main_test_rfh()->InitializeRenderFrameIfNeeded();

  // Send a fake audio stream monitor notification.  The audio wake lock
  // should be created.
  monitor->set_was_recently_audible_for_testing(true);
  contents()->NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
  EXPECT_TRUE(has_audio_wake_lock());

  // Send another fake notification, this time when WasRecentlyAudible() will
  // be false.  The wake lock should be released.
  monitor->set_was_recently_audible_for_testing(false);
  contents()->NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
  EXPECT_FALSE(has_audio_wake_lock());

  // Start a player with both audio and video.  A video wake lock
  // should be created.  If audio stream monitoring is available, an audio power
  // save blocker should be created too.
  rfh->OnMessageReceived(MediaPlayerDelegateHostMsg_OnMediaPlaying(
      0, kPlayerAudioVideoId, true, true, false,
      media::MediaContentType::Persistent));
  EXPECT_TRUE(has_video_wake_lock());
  EXPECT_FALSE(has_audio_wake_lock());

  // Upon hiding the video wake lock should be released.
  contents()->WasHidden();
  EXPECT_FALSE(has_video_wake_lock());

  // Start another player that only has video.  There should be no change in
  // the wake locks.  The notification should take into account the
  // visibility state of the WebContents.
  rfh->OnMessageReceived(MediaPlayerDelegateHostMsg_OnMediaPlaying(
      0, kPlayerVideoOnlyId, true, false, false,
      media::MediaContentType::Persistent));
  EXPECT_FALSE(has_video_wake_lock());
  EXPECT_FALSE(has_audio_wake_lock());

  // Showing the WebContents should result in the creation of the blocker.
  contents()->WasShown();
  EXPECT_TRUE(has_video_wake_lock());

  // Start another player that only has audio.  There should be no change in
  // the wake locks.
  rfh->OnMessageReceived(MediaPlayerDelegateHostMsg_OnMediaPlaying(
      0, kPlayerAudioOnlyId, false, true, false,
      media::MediaContentType::Persistent));
  EXPECT_TRUE(has_video_wake_lock());
  EXPECT_FALSE(has_audio_wake_lock());

  // Start a remote player. There should be no change in the power save
  // blockers.
  rfh->OnMessageReceived(MediaPlayerDelegateHostMsg_OnMediaPlaying(
      0, kPlayerRemoteId, true, true, true,
      media::MediaContentType::Persistent));
  EXPECT_TRUE(has_video_wake_lock());
  EXPECT_FALSE(has_audio_wake_lock());

  // Destroy the original audio video player.  Both wake locks should
  // remain.
  rfh->OnMessageReceived(
      MediaPlayerDelegateHostMsg_OnMediaPaused(0, kPlayerAudioVideoId, false));
  EXPECT_TRUE(has_video_wake_lock());
  EXPECT_FALSE(has_audio_wake_lock());

  // Destroy the audio only player.  The video wake lock should remain.
  rfh->OnMessageReceived(
      MediaPlayerDelegateHostMsg_OnMediaPaused(0, kPlayerAudioOnlyId, false));
  EXPECT_TRUE(has_video_wake_lock());
  EXPECT_FALSE(has_audio_wake_lock());

  // Destroy the video only player.  No wake locks should remain.
  rfh->OnMessageReceived(
      MediaPlayerDelegateHostMsg_OnMediaPaused(0, kPlayerVideoOnlyId, false));
  EXPECT_FALSE(has_video_wake_lock());
  EXPECT_FALSE(has_audio_wake_lock());

  // Destroy the remote player. No wake locks should remain.
  rfh->OnMessageReceived(
      MediaPlayerDelegateHostMsg_OnMediaPaused(0, kPlayerRemoteId, false));
  EXPECT_FALSE(has_video_wake_lock());
  EXPECT_FALSE(has_audio_wake_lock());

  // Start a player with both audio and video.  A video wake lock
  // should be created.  If audio stream monitoring is available, an audio power
  // save blocker should be created too.
  rfh->OnMessageReceived(MediaPlayerDelegateHostMsg_OnMediaPlaying(
      0, kPlayerAudioVideoId, true, true, false,
      media::MediaContentType::Persistent));
  EXPECT_TRUE(has_video_wake_lock());
  EXPECT_FALSE(has_audio_wake_lock());

  viz::SurfaceId surface_id =
      viz::SurfaceId(viz::FrameSinkId(1, 1),
                     viz::LocalSurfaceId(
                         11, base::UnguessableToken::Deserialize(0x111111, 0)));

  // Send the video in Picture-in-Picture mode, power blocker should still be
  // on.
  rfh->OnMessageReceived(
      MediaPlayerDelegateHostMsg_OnPictureInPictureModeStarted(
          0, kPlayerAudioVideoId, surface_id, gfx::Size(), 0, true));
  EXPECT_TRUE(has_video_wake_lock());
  EXPECT_FALSE(has_audio_wake_lock());

  // Hiding the window should keep the power blocker.
  contents()->WasHidden();
  EXPECT_TRUE(has_video_wake_lock());
  EXPECT_FALSE(has_audio_wake_lock());

  // Leaving Picture-in-Picture should reset the power blocker.
  rfh->OnMessageReceived(MediaPlayerDelegateHostMsg_OnPictureInPictureModeEnded(
      0, kPlayerAudioVideoId, 1 /* request_id */));
  EXPECT_FALSE(has_video_wake_lock());
  EXPECT_FALSE(has_audio_wake_lock());

  // Crash the renderer.
  main_test_rfh()->GetProcess()->SimulateCrash();

  // Verify that all the wake locks have been released.
  EXPECT_FALSE(has_video_wake_lock());
  EXPECT_FALSE(has_audio_wake_lock());
}

TEST_F(WebContentsImplTest, ThemeColorChangeDependingOnFirstVisiblePaint) {
  TestWebContentsObserver observer(contents());
  TestRenderFrameHost* rfh = main_test_rfh();
  rfh->InitializeRenderFrameIfNeeded();

  SkColor transparent = SK_ColorTRANSPARENT;

  EXPECT_EQ(transparent, contents()->GetThemeColor());
  EXPECT_EQ(transparent, observer.last_theme_color());

  // Theme color changes should not propagate past the WebContentsImpl before
  // the first visually non-empty paint has occurred.
  rfh->OnMessageReceived(
      FrameHostMsg_DidChangeThemeColor(rfh->GetRoutingID(), SK_ColorRED));

  EXPECT_EQ(SK_ColorRED, contents()->GetThemeColor());
  EXPECT_EQ(transparent, observer.last_theme_color());

  // Simulate that the first visually non-empty paint has occurred. This will
  // propagate the current theme color to the delegates.
  RenderViewHostTester::SimulateFirstPaint(test_rvh());

  EXPECT_EQ(SK_ColorRED, contents()->GetThemeColor());
  EXPECT_EQ(SK_ColorRED, observer.last_theme_color());

  // Additional changes made by the web contents should propagate as well.
  rfh->OnMessageReceived(
      FrameHostMsg_DidChangeThemeColor(rfh->GetRoutingID(), SK_ColorGREEN));

  EXPECT_EQ(SK_ColorGREEN, contents()->GetThemeColor());
  EXPECT_EQ(SK_ColorGREEN, observer.last_theme_color());
}

class PictureInPictureDelegate : public WebContentsDelegate {
 public:
  PictureInPictureDelegate() = default;

  MOCK_METHOD2(EnterPictureInPicture,
               gfx::Size(const viz::SurfaceId&, const gfx::Size&));

 private:
  DISALLOW_COPY_AND_ASSIGN(PictureInPictureDelegate);
};

class TestOverlayWindow : public OverlayWindow {
 public:
  TestOverlayWindow() = default;
  ~TestOverlayWindow() override{};

  static std::unique_ptr<OverlayWindow> Create(
      PictureInPictureWindowController* controller) {
    return std::unique_ptr<OverlayWindow>(new TestOverlayWindow());
  }

  bool IsActive() const override { return false; }
  void Close() override {}
  void Show() override {}
  void Hide() override {}
  void SetPictureInPictureCustomControls(
      const std::vector<blink::PictureInPictureControlInfo>& controls)
      override {}
  bool IsVisible() const override { return false; }
  bool IsAlwaysOnTop() const override { return false; }
  ui::Layer* GetLayer() override { return nullptr; }
  gfx::Rect GetBounds() const override { return gfx::Rect(); }
  void UpdateVideoSize(const gfx::Size& natural_size) override {}
  void SetPlaybackState(PlaybackState playback_state) override {}
  void SetAlwaysHidePlayPauseButton(bool is_visible) override {}
  ui::Layer* GetWindowBackgroundLayer() override { return nullptr; }
  ui::Layer* GetVideoLayer() override { return nullptr; }
  gfx::Rect GetVideoBounds() override { return gfx::Rect(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestOverlayWindow);
};

class PictureInPictureTestBrowserClient : public TestContentBrowserClient {
 public:
  PictureInPictureTestBrowserClient()
      : original_browser_client_(SetBrowserClientForTesting(this)) {}

  ~PictureInPictureTestBrowserClient() override {
    SetBrowserClientForTesting(original_browser_client_);
  }

  std::unique_ptr<OverlayWindow> CreateWindowForPictureInPicture(
      PictureInPictureWindowController* controller) override {
    return TestOverlayWindow::Create(controller);
  }

 private:
  ContentBrowserClient* original_browser_client_;
};

TEST_F(WebContentsImplTest, EnterPictureInPicture) {
  PictureInPictureTestBrowserClient browser_client;
  SetBrowserClientForTesting(&browser_client);

  const int kPlayerVideoOnlyId = 30; /* arbitrary and used for tests */

  PictureInPictureDelegate delegate;
  contents()->SetDelegate(&delegate);

  MediaWebContentsObserver* observer =
      contents()->media_web_contents_observer();
  TestRenderFrameHost* rfh = main_test_rfh();
  rfh->InitializeRenderFrameIfNeeded();

  // If Picture-in-Picture was never triggered, the media player id would not be
  // set.
  EXPECT_FALSE(observer->GetPictureInPictureVideoMediaPlayerId().has_value());

  viz::SurfaceId surface_id =
      viz::SurfaceId(viz::FrameSinkId(1, 1),
                     viz::LocalSurfaceId(
                         11, base::UnguessableToken::Deserialize(0x111111, 0)));

  EXPECT_CALL(delegate, EnterPictureInPicture(surface_id, gfx::Size(42, 42)));

  rfh->OnMessageReceived(
      MediaPlayerDelegateHostMsg_OnPictureInPictureModeStarted(
          rfh->GetRoutingID(), kPlayerVideoOnlyId, surface_id /* surface_id */,
          gfx::Size(42, 42) /* natural_size */, 1 /* request_id */,
          true /* show_play_pause_button */));
  EXPECT_TRUE(observer->GetPictureInPictureVideoMediaPlayerId().has_value());
  EXPECT_EQ(kPlayerVideoOnlyId,
            observer->GetPictureInPictureVideoMediaPlayerId()->delegate_id);

  // Picture-in-Picture media player id should not be reset when the media is
  // destroyed (e.g. video stops playing). This allows the Picture-in-Picture
  // window to continue to control the media.
  rfh->OnMessageReceived(MediaPlayerDelegateHostMsg_OnMediaDestroyed(
      rfh->GetRoutingID(), kPlayerVideoOnlyId));
  EXPECT_TRUE(observer->GetPictureInPictureVideoMediaPlayerId().has_value());
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
  };

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
  EXPECT_EQ(1u, dialog_manager.reset_count());

  contents()->SetJavaScriptDialogManagerForTesting(nullptr);
}

TEST_F(WebContentsImplTest, StartingSandboxFlags) {
  WebContents::CreateParams params(browser_context());
  const blink::WebSandboxFlags expected_flags =
      blink::WebSandboxFlags::kPopups | blink::WebSandboxFlags::kModals |
      blink::WebSandboxFlags::kTopNavigation;
  params.starting_sandbox_flags = expected_flags;
  std::unique_ptr<WebContentsImpl> new_contents(
      WebContentsImpl::CreateWithOpener(params, nullptr));
  FrameTreeNode* root = new_contents->GetFrameTree()->root();
  blink::WebSandboxFlags pending_flags =
      root->pending_frame_policy().sandbox_flags;
  EXPECT_EQ(pending_flags, expected_flags);
  blink::WebSandboxFlags effective_flags =
      root->effective_frame_policy().sandbox_flags;
  EXPECT_EQ(effective_flags, expected_flags);
}

}  // namespace content
