// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_state_impl.h"

#include <stddef.h>

#include <memory>

#import <OCMock/OCMock.h>

#include "base/base64.h"
#include "base/bind.h"
#include "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#import "ios/web/interstitials/html_web_interstitial_impl.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/crw_navigation_item_storage.h"
#import "ios/web/public/crw_session_storage.h"
#include "ios/web/public/features.h"
#import "ios/web/public/java_script_dialog_presenter.h"
#include "ios/web/public/load_committed_details.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/public/test/fakes/test_web_state_delegate.h"
#import "ios/web/public/test/fakes/test_web_state_observer.h"
#include "ios/web/public/test/web_test.h"
#import "ios/web/public/web_state/context_menu_params.h"
#include "ios/web/public/web_state/global_web_state_observer.h"
#import "ios/web/public/web_state/web_state_delegate.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_policy_decider.h"
#import "ios/web/test/fakes/mock_interstitial_delegate.h"
#include "ios/web/web_state/global_web_state_event_tracker.h"
#import "ios/web/web_state/navigation_context_impl.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::_;
using testing::Assign;
using testing::AtMost;
using testing::DoAll;
using testing::Return;

namespace web {
namespace {

// WebStateImplTest is parameterized on this enum to test both implementations
// of navigation manager.
enum class NavigationManagerChoice {
  LEGACY,
  WK_BASED,
};

// Test observer to check that the GlobalWebStateObserver methods are called as
// expected.
class TestGlobalWebStateObserver : public GlobalWebStateObserver {
 public:
  TestGlobalWebStateObserver()
      : GlobalWebStateObserver(),
        navigation_items_pruned_called_(false),
        navigation_item_changed_called_(false),
        navigation_item_committed_called_(false),
        did_start_loading_called_(false),
        did_stop_loading_called_(false),
        did_start_navigation_called_(false),
        page_loaded_called_with_success_(false),
        web_state_destroyed_called_(false) {}

  // Methods returning true if the corresponding GlobalWebStateObserver method
  // has been called.
  bool navigation_items_pruned_called() const {
    return navigation_items_pruned_called_;
  }
  bool navigation_item_changed_called() const {
    return navigation_item_changed_called_;
  }
  bool navigation_item_committed_called() const {
    return navigation_item_committed_called_;
  }
  bool did_start_loading_called() const { return did_start_loading_called_; }
  bool did_stop_loading_called() const { return did_stop_loading_called_; }
  bool did_start_navigation_called() const {
    return did_start_navigation_called_;
  }
  bool page_loaded_called_with_success() const {
    return page_loaded_called_with_success_;
  }
  bool web_state_destroyed_called() const {
    return web_state_destroyed_called_;
  }

 private:
  // GlobalWebStateObserver implementation:
  void NavigationItemsPruned(WebState* web_state,
                             size_t pruned_item_count) override {
    navigation_items_pruned_called_ = true;
  }
  void NavigationItemChanged(WebState* web_state) override {
    navigation_item_changed_called_ = true;
  }
  void NavigationItemCommitted(
      WebState* web_state,
      const LoadCommittedDetails& load_details) override {
    navigation_item_committed_called_ = true;
  }
  void WebStateDidStartLoading(WebState* web_state) override {
    did_start_loading_called_ = true;
  }
  void WebStateDidStopLoading(WebState* web_state) override {
    did_stop_loading_called_ = true;
  }
  void WebStateDidStartNavigation(
      WebState* web_state,
      NavigationContext* navigation_context) override {
    did_start_navigation_called_ = true;
  }
  void PageLoaded(WebState* web_state,
                  PageLoadCompletionStatus load_completion_status) override {
    page_loaded_called_with_success_ =
        load_completion_status == PageLoadCompletionStatus::SUCCESS;
  }
  void WebStateDestroyed(WebState* web_state) override {
    web_state_destroyed_called_ = true;
  }

  bool navigation_items_pruned_called_;
  bool navigation_item_changed_called_;
  bool navigation_item_committed_called_;
  bool did_start_loading_called_;
  bool did_stop_loading_called_;
  bool did_start_navigation_called_;
  bool page_loaded_called_with_success_;
  bool web_state_destroyed_called_;
};

// Test decider to check that the WebStatePolicyDecider methods are called as
// expected.
class MockWebStatePolicyDecider : public WebStatePolicyDecider {
 public:
  explicit MockWebStatePolicyDecider(WebState* web_state)
      : WebStatePolicyDecider(web_state) {}
  virtual ~MockWebStatePolicyDecider() {}

  MOCK_METHOD2(ShouldAllowRequest,
               bool(NSURLRequest* request,
                    const WebStatePolicyDecider::RequestInfo& request_info));
  MOCK_METHOD2(ShouldAllowResponse,
               bool(NSURLResponse* response, bool for_main_frame));
  MOCK_METHOD0(WebStateDestroyed, void());
};

// Creates and returns an HttpResponseHeader using the string representation.
scoped_refptr<net::HttpResponseHeaders> HeadersFromString(const char* string) {
  std::string raw_string(string);
  std::string headers_string = net::HttpUtil::AssembleRawHeaders(
      raw_string.c_str(), raw_string.length());
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(headers_string));
  return headers;
}

// Test callback for script commands.
// Sets |is_called| to true if it is called, and checks that the parameters
// match their expected values.
bool HandleScriptCommand(bool* is_called,
                         bool should_handle,
                         base::DictionaryValue* expected_value,
                         const GURL& expected_url,
                         bool expected_user_is_interacting,
                         bool expected_is_main_frame,
                         web::WebFrame* expected_sender_frame,
                         const base::DictionaryValue& value,
                         const GURL& url,
                         bool user_is_interacting,
                         bool is_main_frame,
                         web::WebFrame* sender_frame) {
  *is_called = true;
  EXPECT_TRUE(expected_value->Equals(&value));
  EXPECT_EQ(expected_url, url);
  EXPECT_EQ(expected_user_is_interacting, user_is_interacting);
  EXPECT_EQ(expected_is_main_frame, is_main_frame);
  EXPECT_EQ(expected_sender_frame, sender_frame);
  return should_handle;
}

}  // namespace

// Test fixture for web::WebStateImpl class.
class WebStateImplTest
    : public web::WebTest,
      public ::testing::WithParamInterface<NavigationManagerChoice> {
 protected:
  WebStateImplTest() : web::WebTest() {
    if (GetParam() == NavigationManagerChoice::LEGACY) {
      scoped_feature_list_.InitAndDisableFeature(
          features::kSlimNavigationManager);
    } else {
      scoped_feature_list_.InitAndEnableFeature(
          features::kSlimNavigationManager);
    }

    web::WebState::CreateParams params(GetBrowserState());
    web_state_ = std::make_unique<web::WebStateImpl>(params);
  }

  // Adds PendingNavigationItem and commits it.
  void AddCommittedNavigationItem() {
    web_state_->GetNavigationManagerImpl().InitializeSession();
    web_state_->GetNavigationManagerImpl().AddPendingItem(
        GURL::EmptyGURL(), web::Referrer(), ui::PAGE_TRANSITION_LINK,
        NavigationInitiationType::RENDERER_INITIATED,
        NavigationManager::UserAgentOverrideOption::DESKTOP);
    web_state_->GetNavigationManagerImpl().CommitPendingItem();
  }

  // Creates interstitial raw pointer and calls Show(). The pointer must be
  // deleted by dismissing the interstitial.
  WebInterstitialImpl* ShowInterstitial() {
    auto delegate = std::make_unique<MockInterstitialDelegate>();
    WebInterstitialImpl* result =
        new HtmlWebInterstitialImpl(web_state_.get(), /*new_navigation=*/true,
                                    GURL::EmptyGURL(), std::move(delegate));
    result->Show();
    return result;
  }
  std::unique_ptr<WebStateImpl> web_state_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(WebStateImplTest, WebUsageEnabled) {
  // Default is false.
  ASSERT_TRUE(web_state_->IsWebUsageEnabled());

  web_state_->SetWebUsageEnabled(false);
  EXPECT_FALSE(web_state_->IsWebUsageEnabled());
  EXPECT_FALSE(web_state_->GetWebController().webUsageEnabled);

  web_state_->SetWebUsageEnabled(true);
  EXPECT_TRUE(web_state_->IsWebUsageEnabled());
  EXPECT_TRUE(web_state_->GetWebController().webUsageEnabled);
}

TEST_P(WebStateImplTest, ShouldSuppressDialogs) {
  // Default is false.
  ASSERT_FALSE(web_state_->ShouldSuppressDialogs());

  web_state_->SetShouldSuppressDialogs(true);
  EXPECT_TRUE(web_state_->ShouldSuppressDialogs());
  EXPECT_TRUE(web_state_->GetWebController().shouldSuppressDialogs);

  web_state_->SetShouldSuppressDialogs(false);
  EXPECT_FALSE(web_state_->ShouldSuppressDialogs());
  EXPECT_FALSE(web_state_->GetWebController().shouldSuppressDialogs);
}

TEST_P(WebStateImplTest, ResponseHeaders) {
  GURL real_url("http://foo.com/bar");
  GURL frame_url("http://frames-r-us.com/");
  scoped_refptr<net::HttpResponseHeaders> real_headers(HeadersFromString(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html\r\n"
      "X-Should-Be-Here: yep\r\n"
      "\r\n"));
  scoped_refptr<net::HttpResponseHeaders> frame_headers(HeadersFromString(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/pdf\r\n"
      "X-Should-Not-Be-Here: oops\r\n"
      "\r\n"));
  // Simulate a load of a page with a frame.
  web_state_->OnHttpResponseHeadersReceived(real_headers.get(), real_url);
  web_state_->OnHttpResponseHeadersReceived(frame_headers.get(), frame_url);
  // Include a hash to be sure it's handled correctly.
  web_state_->UpdateHttpResponseHeaders(
      GURL(real_url.spec() + std::string("#baz")));

  // Verify that the right header set was kept.
  ASSERT_TRUE(web_state_->GetHttpResponseHeaders());
  EXPECT_TRUE(
      web_state_->GetHttpResponseHeaders()->HasHeader("X-Should-Be-Here"));
  EXPECT_FALSE(
      web_state_->GetHttpResponseHeaders()->HasHeader("X-Should-Not-Be-Here"));

  // And that it was parsed correctly.
  EXPECT_EQ("text/html", web_state_->GetContentsMimeType());
}

TEST_P(WebStateImplTest, ResponseHeaderClearing) {
  GURL url("http://foo.com/");
  scoped_refptr<net::HttpResponseHeaders> headers(HeadersFromString(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html\r\n"
      "\r\n"));
  web_state_->OnHttpResponseHeadersReceived(headers.get(), url);

  // There should be no headers before loading.
  EXPECT_EQ(NULL, web_state_->GetHttpResponseHeaders());

  // There should be headers and parsed values after loading.
  web_state_->UpdateHttpResponseHeaders(url);
  ASSERT_TRUE(web_state_->GetHttpResponseHeaders());
  EXPECT_TRUE(web_state_->GetHttpResponseHeaders()->HasHeader("Content-Type"));
  EXPECT_NE("", web_state_->GetContentsMimeType());

  // ... but not after loading another page, nor should there be specific
  // parsed values.
  web_state_->UpdateHttpResponseHeaders(GURL("http://elsewhere.com/"));
  EXPECT_EQ(NULL, web_state_->GetHttpResponseHeaders());
  EXPECT_EQ("", web_state_->GetContentsMimeType());
}

// Tests forwarding to WebStateObserver callbacks.
TEST_P(WebStateImplTest, ObserverTest) {
  std::unique_ptr<TestWebStateObserver> observer(
      new TestWebStateObserver(web_state_.get()));
  EXPECT_EQ(web_state_.get(), observer->web_state());

  // Test that WasShown() is called.
  ASSERT_FALSE(web_state_->IsVisible());
  ASSERT_FALSE(observer->was_shown_info());
  web_state_->WasShown();
  ASSERT_TRUE(observer->was_shown_info());
  EXPECT_EQ(web_state_.get(), observer->was_shown_info()->web_state);
  EXPECT_TRUE(web_state_->IsVisible());

  // Test that WasShown() callback is not called for the second time.
  observer = std::make_unique<TestWebStateObserver>(web_state_.get());
  web_state_->WasShown();
  EXPECT_FALSE(observer->was_shown_info());

  // Test that WasHidden() is called.
  ASSERT_TRUE(web_state_->IsVisible());
  ASSERT_FALSE(observer->was_hidden_info());
  web_state_->WasHidden();
  ASSERT_TRUE(observer->was_hidden_info());
  EXPECT_EQ(web_state_.get(), observer->was_hidden_info()->web_state);
  EXPECT_FALSE(web_state_->IsVisible());

  // Test that WasHidden() callback is not called for the second time.
  observer = std::make_unique<TestWebStateObserver>(web_state_.get());
  web_state_->WasHidden();
  EXPECT_FALSE(observer->was_hidden_info());

  // Test that LoadProgressChanged() is called.
  ASSERT_FALSE(observer->change_loading_progress_info());
  const double kTestLoadProgress = 0.75;
  web_state_->SendChangeLoadProgress(kTestLoadProgress);
  ASSERT_TRUE(observer->change_loading_progress_info());
  EXPECT_EQ(web_state_.get(),
            observer->change_loading_progress_info()->web_state);
  EXPECT_EQ(kTestLoadProgress,
            observer->change_loading_progress_info()->progress);

  // Test that TitleWasSet() is called.
  ASSERT_FALSE(observer->title_was_set_info());
  web_state_->OnTitleChanged();
  ASSERT_TRUE(observer->title_was_set_info());
  EXPECT_EQ(web_state_.get(), observer->title_was_set_info()->web_state);

  // Test that DidChangeVisibleSecurityState() is called.
  ASSERT_FALSE(observer->did_change_visible_security_state_info());
  web_state_->DidChangeVisibleSecurityState();
  ASSERT_TRUE(observer->did_change_visible_security_state_info());
  EXPECT_EQ(web_state_.get(),
            observer->did_change_visible_security_state_info()->web_state);

  // Test that DidSuppressDialog() is called.
  ASSERT_FALSE(observer->did_suppress_dialog_info());
  web_state_->SetShouldSuppressDialogs(true);
  web_state_->OnDialogSuppressed();
  ASSERT_TRUE(observer->did_suppress_dialog_info());
  EXPECT_EQ(web_state_.get(), observer->did_suppress_dialog_info()->web_state);

  // Test that FaviconUrlUpdated() is called.
  ASSERT_FALSE(observer->update_favicon_url_candidates_info());
  web::FaviconURL favicon_url(GURL("https://chromium.test/"),
                              web::FaviconURL::IconType::kTouchIcon,
                              {gfx::Size(5, 6)});
  web_state_->OnFaviconUrlUpdated({favicon_url});
  ASSERT_TRUE(observer->update_favicon_url_candidates_info());
  EXPECT_EQ(web_state_.get(),
            observer->update_favicon_url_candidates_info()->web_state);
  ASSERT_EQ(1U,
            observer->update_favicon_url_candidates_info()->candidates.size());
  const web::FaviconURL& actual_favicon_url =
      observer->update_favicon_url_candidates_info()->candidates[0];
  EXPECT_EQ(favicon_url.icon_url, actual_favicon_url.icon_url);
  EXPECT_EQ(favicon_url.icon_type, actual_favicon_url.icon_type);
  ASSERT_EQ(favicon_url.icon_sizes.size(),
            actual_favicon_url.icon_sizes.size());
  EXPECT_EQ(favicon_url.icon_sizes[0].width(),
            actual_favicon_url.icon_sizes[0].width());
  EXPECT_EQ(favicon_url.icon_sizes[0].height(),
            actual_favicon_url.icon_sizes[0].height());

  // Test that WebFrameDidBecomeAvailable() is called.
  ASSERT_FALSE(observer->web_frame_available_info());
  web::FakeWebFrame main_frame("main", true, GURL());
  web_state_->OnWebFrameAvailable(&main_frame);
  ASSERT_TRUE(observer->web_frame_available_info());
  EXPECT_EQ(web_state_.get(), observer->web_frame_available_info()->web_state);
  EXPECT_EQ(&main_frame, observer->web_frame_available_info()->web_frame);

  // Test that WebFrameWillBecomeUnavailable() is called.
  ASSERT_FALSE(observer->web_frame_unavailable_info());
  web_state_->OnWebFrameUnavailable(&main_frame);
  ASSERT_TRUE(observer->web_frame_unavailable_info());
  EXPECT_EQ(web_state_.get(),
            observer->web_frame_unavailable_info()->web_state);
  EXPECT_EQ(&main_frame, observer->web_frame_unavailable_info()->web_frame);

  // Test that RenderProcessGone() is called.
  SetIgnoreRenderProcessCrashesDuringTesting(true);
  ASSERT_FALSE(observer->render_process_gone_info());
  web_state_->OnRenderProcessGone();
  ASSERT_TRUE(observer->render_process_gone_info());
  EXPECT_EQ(web_state_.get(), observer->render_process_gone_info()->web_state);

  // Test that DidFinishNavigation() is called.
  ASSERT_FALSE(observer->did_finish_navigation_info());
  const GURL url("http://test");
  std::unique_ptr<web::NavigationContext> context =
      NavigationContextImpl::CreateNavigationContext(
          web_state_.get(), url, /*has_user_gesture=*/true,
          ui::PageTransition::PAGE_TRANSITION_AUTO_BOOKMARK,
          /*is_renderer_initiated=*/true);
  web_state_->OnNavigationFinished(context.get());
  ASSERT_TRUE(observer->did_finish_navigation_info());
  EXPECT_EQ(web_state_.get(),
            observer->did_finish_navigation_info()->web_state);
  NavigationContext* actual_context =
      observer->did_finish_navigation_info()->context.get();
  EXPECT_EQ(context->GetUrl(), actual_context->GetUrl());
  EXPECT_TRUE(PageTransitionTypeIncludingQualifiersIs(
      context->GetPageTransition(), actual_context->GetPageTransition()));
  EXPECT_FALSE(actual_context->IsSameDocument());
  EXPECT_FALSE(actual_context->IsPost());
  EXPECT_FALSE(actual_context->GetError());
  EXPECT_FALSE(actual_context->GetResponseHeaders());

  // Test that DidStartNavigation() is called.
  ASSERT_FALSE(observer->did_start_navigation_info());
  web_state_->OnNavigationStarted(context.get());
  ASSERT_TRUE(observer->did_start_navigation_info());
  EXPECT_EQ(web_state_.get(), observer->did_start_navigation_info()->web_state);
  actual_context = observer->did_start_navigation_info()->context.get();
  EXPECT_EQ(context->GetUrl(), actual_context->GetUrl());
  EXPECT_TRUE(PageTransitionTypeIncludingQualifiersIs(
      context->GetPageTransition(), actual_context->GetPageTransition()));
  EXPECT_FALSE(actual_context->IsSameDocument());
  EXPECT_FALSE(actual_context->IsPost());
  EXPECT_FALSE(actual_context->GetError());
  EXPECT_FALSE(actual_context->GetResponseHeaders());

  // Test that NavigationItemsPruned() is called.
  ASSERT_FALSE(observer->navigation_items_pruned_info());
  web_state_->OnNavigationItemsPruned(1);
  ASSERT_TRUE(observer->navigation_items_pruned_info());
  EXPECT_EQ(web_state_.get(),
            observer->navigation_items_pruned_info()->web_state);

  // Test that NavigationItemChanged() is called.
  ASSERT_FALSE(observer->navigation_item_changed_info());
  web_state_->OnNavigationItemChanged();
  ASSERT_TRUE(observer->navigation_item_changed_info());
  EXPECT_EQ(web_state_.get(),
            observer->navigation_item_changed_info()->web_state);

  // Test that NavigationItemCommitted() is called.
  ASSERT_FALSE(observer->commit_navigation_info());
  LoadCommittedDetails details;
  auto item = std::make_unique<NavigationItemImpl>();
  details.item = item.get();
  web_state_->OnNavigationItemCommitted(details);
  ASSERT_TRUE(observer->commit_navigation_info());
  EXPECT_EQ(web_state_.get(), observer->commit_navigation_info()->web_state);
  LoadCommittedDetails actual_details =
      observer->commit_navigation_info()->load_details;
  EXPECT_EQ(details.item, actual_details.item);
  EXPECT_EQ(details.previous_item_index, actual_details.previous_item_index);
  EXPECT_EQ(details.is_in_page, actual_details.is_in_page);

  // Test that OnPageLoaded() is called with success when there is no error.
  ASSERT_FALSE(observer->load_page_info());
  web_state_->OnPageLoaded(url, false);
  ASSERT_TRUE(observer->load_page_info());
  EXPECT_EQ(web_state_.get(), observer->load_page_info()->web_state);
  EXPECT_FALSE(observer->load_page_info()->success);
  web_state_->OnPageLoaded(url, true);
  ASSERT_TRUE(observer->load_page_info());
  EXPECT_EQ(web_state_.get(), observer->load_page_info()->web_state);
  EXPECT_TRUE(observer->load_page_info()->success);

  // Test that OnTitleChanged() is called.
  observer = std::make_unique<TestWebStateObserver>(web_state_.get());
  ASSERT_FALSE(observer->title_was_set_info());
  web_state_->OnTitleChanged();
  ASSERT_TRUE(observer->title_was_set_info());
  EXPECT_EQ(web_state_.get(), observer->title_was_set_info()->web_state);

  // Test that WebStateDestroyed() is called.
  EXPECT_FALSE(observer->web_state_destroyed_info());
  web_state_.reset();
  EXPECT_TRUE(observer->web_state_destroyed_info());

  EXPECT_EQ(nullptr, observer->web_state());
}

// Tests that placeholder navigations are not visible to WebStateObservers.
TEST_P(WebStateImplTest, PlaceholderNavigationNotExposedToObservers) {
  TestWebStateObserver observer(web_state_.get());
  FakeNavigationContext context;
  context.SetUrl(
      wk_navigation_util::CreatePlaceholderUrlForUrl(GURL("chrome://newtab")));

  // Test that OnPageLoaded() is not called.
  web_state_->OnPageLoaded(context.GetUrl(), true /* load_success */);
  EXPECT_FALSE(observer.load_page_info());
  web_state_->OnPageLoaded(context.GetUrl(), false /* load_success */);
  EXPECT_FALSE(observer.load_page_info());

  // Test that OnNavigationStarted() is not called.
  web_state_->OnNavigationStarted(&context);
  EXPECT_FALSE(observer.did_start_navigation_info());

  // Test that OnNavigationFinished() is not called.
  web_state_->OnNavigationFinished(&context);
  EXPECT_FALSE(observer.did_finish_navigation_info());
}

// Tests that WebStateDelegate methods appropriately called.
TEST_P(WebStateImplTest, DelegateTest) {
  TestWebStateDelegate delegate;
  web_state_->SetDelegate(&delegate);

  // Test that CreateNewWebState() is called.
  GURL child_url("https://child.test/");
  GURL opener_url("https://opener.test/");
  EXPECT_FALSE(delegate.last_create_new_web_state_request());
  web_state_->CreateNewWebState(child_url, opener_url, true);
  TestCreateNewWebStateRequest* create_new_web_state_request =
      delegate.last_create_new_web_state_request();
  ASSERT_TRUE(create_new_web_state_request);
  EXPECT_EQ(web_state_.get(), create_new_web_state_request->web_state);
  EXPECT_EQ(child_url, create_new_web_state_request->url);
  EXPECT_EQ(opener_url, create_new_web_state_request->opener_url);
  EXPECT_TRUE(create_new_web_state_request->initiated_by_user);

  // Test that CloseWebState() is called.
  EXPECT_FALSE(delegate.last_close_web_state_request());
  web_state_->CloseWebState();
  ASSERT_TRUE(delegate.last_close_web_state_request());
  EXPECT_EQ(web_state_.get(),
            delegate.last_close_web_state_request()->web_state);

  // Test that OpenURLFromWebState() is called.
  WebState::OpenURLParams params(GURL("https://chromium.test/"), Referrer(),
                                 WindowOpenDisposition::CURRENT_TAB,
                                 ui::PAGE_TRANSITION_LINK, true);
  EXPECT_FALSE(delegate.last_open_url_request());
  web_state_->OpenURL(params);
  TestOpenURLRequest* open_url_request = delegate.last_open_url_request();
  ASSERT_TRUE(open_url_request);
  EXPECT_EQ(web_state_.get(), open_url_request->web_state);
  WebState::OpenURLParams actual_params = open_url_request->params;
  EXPECT_EQ(params.url, actual_params.url);
  EXPECT_EQ(params.referrer.url, actual_params.referrer.url);
  EXPECT_EQ(params.referrer.policy, actual_params.referrer.policy);
  EXPECT_EQ(params.disposition, actual_params.disposition);
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(params.transition, actual_params.transition));
  EXPECT_EQ(params.is_renderer_initiated, actual_params.is_renderer_initiated);

  // Test that HandleContextMenu() is called.
  EXPECT_FALSE(delegate.handle_context_menu_called());
  web::ContextMenuParams context_menu_params;
  web_state_->HandleContextMenu(context_menu_params);
  EXPECT_TRUE(delegate.handle_context_menu_called());

  // Test that ShowRepostFormWarningDialog() is called.
  EXPECT_FALSE(delegate.last_repost_form_request());
  base::Callback<void(bool)> repost_callback;
  web_state_->ShowRepostFormWarningDialog(repost_callback);
  ASSERT_TRUE(delegate.last_repost_form_request());
  EXPECT_EQ(delegate.last_repost_form_request()->web_state, web_state_.get());

  // Test that GetJavaScriptDialogPresenter() is called.
  TestJavaScriptDialogPresenter* presenter =
      delegate.GetTestJavaScriptDialogPresenter();
  EXPECT_FALSE(delegate.get_java_script_dialog_presenter_called());
  EXPECT_TRUE(presenter->requested_dialogs().empty());
  EXPECT_FALSE(presenter->cancel_dialogs_called());

  __block bool callback_called = false;
  web_state_->RunJavaScriptDialog(GURL(), JAVASCRIPT_DIALOG_TYPE_ALERT, @"",
                                  nil, base::BindOnce(^(bool, NSString*) {
                                    callback_called = true;
                                  }));

  EXPECT_TRUE(delegate.get_java_script_dialog_presenter_called());
  EXPECT_EQ(1U, presenter->requested_dialogs().size());
  EXPECT_TRUE(callback_called);

  EXPECT_FALSE(presenter->cancel_dialogs_called());
  web_state_->CancelDialogs();
  EXPECT_TRUE(presenter->cancel_dialogs_called());

  // Test that OnAuthRequired() is called.
  EXPECT_FALSE(delegate.last_authentication_request());
  NSURLProtectionSpace* protection_space = [[NSURLProtectionSpace alloc] init];
  NSURLCredential* credential = [[NSURLCredential alloc] init];
  WebStateDelegate::AuthCallback callback;
  web_state_->OnAuthRequired(protection_space, credential, callback);
  ASSERT_TRUE(delegate.last_authentication_request());
  EXPECT_EQ(delegate.last_authentication_request()->web_state,
            web_state_.get());
  EXPECT_EQ(delegate.last_authentication_request()->protection_space,
            protection_space);
  EXPECT_EQ(delegate.last_authentication_request()->credential, credential);

  // Test that ShouldPreviewLink() is delegated correctly.
  GURL link_url("http://link.test/");
  delegate.SetShouldPreviewLink(false);
  delegate.ClearLastLinkURL();
  EXPECT_FALSE(web_state_->ShouldPreviewLink(link_url));
  EXPECT_EQ(link_url, delegate.last_link_url());
  delegate.SetShouldPreviewLink(true);
  delegate.ClearLastLinkURL();
  EXPECT_TRUE(web_state_->ShouldPreviewLink(link_url));
  EXPECT_EQ(link_url, delegate.last_link_url());

  // Test that GetPreviewingViewController() is delegated correctly.
  UIViewController* previewing_view_controller =
      OCMClassMock([UIViewController class]);
  delegate.SetPreviewingViewController(previewing_view_controller);
  delegate.ClearLastLinkURL();
  EXPECT_EQ(previewing_view_controller,
            web_state_->GetPreviewingViewController(link_url));
  EXPECT_EQ(link_url, delegate.last_link_url());

  // Test that CommitPreviewingViewController() is called.
  delegate.ClearLastPreviewingViewController();
  web_state_->CommitPreviewingViewController(previewing_view_controller);
  EXPECT_EQ(previewing_view_controller,
            delegate.last_previewing_view_controller());
}

// Verifies that GlobalWebStateObservers are called when expected.
TEST_P(WebStateImplTest, GlobalObserverTest) {
  std::unique_ptr<TestGlobalWebStateObserver> observer(
      new TestGlobalWebStateObserver());

  // Test that NavigationItemsPruned() is called.
  EXPECT_FALSE(observer->navigation_items_pruned_called());
  web_state_->OnNavigationItemsPruned(1);
  EXPECT_TRUE(observer->navigation_items_pruned_called());

  // Test that NavigationItemChanged() is called.
  EXPECT_FALSE(observer->navigation_item_changed_called());
  web_state_->OnNavigationItemChanged();
  EXPECT_TRUE(observer->navigation_item_changed_called());

  // Test that NavigationItemCommitted() is called.
  EXPECT_FALSE(observer->navigation_item_committed_called());
  LoadCommittedDetails details;
  auto item = std::make_unique<NavigationItemImpl>();
  details.item = item.get();
  web_state_->OnNavigationItemCommitted(details);
  EXPECT_TRUE(observer->navigation_item_committed_called());

  // Test that DidStartNavigation() is called.
  EXPECT_FALSE(observer->did_start_navigation_called());
  FakeNavigationContext context;
  web_state_->OnNavigationStarted(&context);
  EXPECT_TRUE(observer->did_start_navigation_called());

  // Test that WebStateDidStartLoading() is called.
  EXPECT_FALSE(observer->did_start_loading_called());
  web_state_->SetIsLoading(true);
  EXPECT_TRUE(observer->did_start_loading_called());

  // Test that WebStateDidStopLoading() is called.
  EXPECT_FALSE(observer->did_stop_loading_called());
  web_state_->SetIsLoading(false);
  EXPECT_TRUE(observer->did_stop_loading_called());

  // Test that OnPageLoaded() is called with success when there is no error.
  EXPECT_FALSE(observer->page_loaded_called_with_success());
  web_state_->OnPageLoaded(GURL("http://test"), false);
  EXPECT_FALSE(observer->page_loaded_called_with_success());
  web_state_->OnPageLoaded(GURL("http://test"), true);
  EXPECT_TRUE(observer->page_loaded_called_with_success());

  // Test that WebStateDestroyed() is called.
  EXPECT_FALSE(observer->web_state_destroyed_called());
  web_state_.reset();
  EXPECT_TRUE(observer->web_state_destroyed_called());
}

// A Google Mock matcher which matches WebStatePolicyDecider::RequestInfo.
// This is needed because WebStatePolicyDecider::RequestInfo doesn't support
// operator==.
MATCHER_P(RequestInfoMatch, expected_request_info, /* argument_name = */ "") {
  return ui::PageTransitionTypeIncludingQualifiersIs(
             arg.transition_type, expected_request_info.transition_type) &&
         arg.target_frame_is_main ==
             expected_request_info.target_frame_is_main &&
         arg.has_user_gesture == expected_request_info.has_user_gesture;
}

// Verifies that policy deciders are correctly called by the web state.
TEST_P(WebStateImplTest, PolicyDeciderTest) {
  MockWebStatePolicyDecider decider(web_state_.get());
  MockWebStatePolicyDecider decider2(web_state_.get());
  EXPECT_EQ(web_state_.get(), decider.web_state());

  NSURL* url = [NSURL URLWithString:@"http://example.com"];
  NSURLRequest* request = [NSURLRequest requestWithURL:url];
  NSURLResponse* response = [[NSURLResponse alloc] initWithURL:url
                                                      MIMEType:@"text/html"
                                         expectedContentLength:0
                                              textEncodingName:nil];

  // Test that ShouldAllowRequest() is called for the same parameters.
  WebStatePolicyDecider::RequestInfo request_info_main_frame(
      ui::PageTransition::PAGE_TRANSITION_LINK,
      /*target_main_frame=*/true,
      /*has_user_gesture=*/false);
  EXPECT_CALL(decider, ShouldAllowRequest(
                           request, RequestInfoMatch(request_info_main_frame)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(decider2, ShouldAllowRequest(
                            request, RequestInfoMatch(request_info_main_frame)))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_TRUE(web_state_->ShouldAllowRequest(request, request_info_main_frame));

  WebStatePolicyDecider::RequestInfo request_info_iframe(
      ui::PageTransition::PAGE_TRANSITION_LINK,
      /*target_main_frame=*/false,
      /*has_user_gesture=*/false);

  EXPECT_CALL(decider, ShouldAllowRequest(
                           request, RequestInfoMatch(request_info_iframe)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(decider2, ShouldAllowRequest(
                            request, RequestInfoMatch(request_info_iframe)))

      .Times(1)
      .WillOnce(Return(true));

  EXPECT_TRUE(web_state_->ShouldAllowRequest(request, request_info_iframe));

  // Test that ShouldAllowRequest() is stopping on negative answer. Only one
  // one the decider should be called.
  {
    bool decider_called = false;
    bool decider2_called = false;
    EXPECT_CALL(
        decider,
        ShouldAllowRequest(request, RequestInfoMatch(request_info_main_frame)))
        .Times(AtMost(1))
        .WillOnce(DoAll(Assign(&decider_called, true), Return(false)));
    EXPECT_CALL(
        decider2,
        ShouldAllowRequest(request, RequestInfoMatch(request_info_main_frame)))
        .Times(AtMost(1))
        .WillOnce(DoAll(Assign(&decider2_called, true), Return(false)));

    EXPECT_FALSE(
        web_state_->ShouldAllowRequest(request, request_info_main_frame));
    EXPECT_FALSE(decider_called && decider2_called);
  }

  // Test that ShouldAllowResponse() is called.
  EXPECT_CALL(decider, ShouldAllowResponse(response, true))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(decider2, ShouldAllowResponse(response, true))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_TRUE(web_state_->ShouldAllowResponse(response, true));

  // Test that ShouldAllowResponse() is stopping on negative answer. Only one
  // one the decider should be called.
  {
    bool decider_called = false;
    bool decider2_called = false;
    EXPECT_CALL(decider, ShouldAllowResponse(response, false))
        .Times(AtMost(1))
        .WillOnce(DoAll(Assign(&decider_called, true), Return(false)));
    EXPECT_CALL(decider2, ShouldAllowResponse(response, false))
        .Times(AtMost(1))
        .WillOnce(DoAll(Assign(&decider2_called, true), Return(false)));
    EXPECT_FALSE(web_state_->ShouldAllowResponse(response, false));
    EXPECT_FALSE(decider_called && decider2_called);
  }

  // Test that WebStateDestroyed() is called.
  EXPECT_CALL(decider, WebStateDestroyed()).Times(1);
  EXPECT_CALL(decider2, WebStateDestroyed()).Times(1);
  web_state_.reset();
  EXPECT_EQ(nullptr, decider.web_state());
}

// Tests that script command callbacks are called correctly.
TEST_P(WebStateImplTest, ScriptCommand) {
  // Set up three script command callbacks.
  const std::string kPrefix1("prefix1");
  const std::string kCommand1("prefix1.command1");
  base::DictionaryValue value_1;
  value_1.SetString("a", "b");
  const GURL kUrl1("http://foo");
  bool is_called_1 = false;
  web::FakeWebFrame main_frame("main", true, GURL());
  web_state_->AddScriptCommandCallback(
      base::BindRepeating(&HandleScriptCommand, &is_called_1,
                          /*should_handle*/ true, &value_1, kUrl1,
                          /*expected_user_is_interacting*/ false,
                          /*expected_is_main_frame*/ true, &main_frame),
      kPrefix1);

  const std::string kPrefix2("prefix2");
  const std::string kCommand2("prefix2.command2");
  base::DictionaryValue value_2;
  value_2.SetString("c", "d");
  const GURL kUrl2("http://bar");
  bool is_called_2 = false;
  web_state_->AddScriptCommandCallback(
      base::BindRepeating(&HandleScriptCommand, &is_called_2,
                          /*should_handle*/ false, &value_2, kUrl2,
                          /*expected_user_is_interacting*/ false,
                          /*expected_is_main_frame*/ true, &main_frame),
      kPrefix2);

  const std::string kPrefix3("prefix3");
  const std::string kCommand3("prefix3.command3");
  base::DictionaryValue value_3;
  value_3.SetString("e", "f");
  const GURL kUrl3("http://iframe");
  bool is_called_3 = false;
  web::FakeWebFrame subframe("subframe", false, GURL());
  web_state_->AddScriptCommandCallback(
      base::BindRepeating(&HandleScriptCommand, &is_called_3,
                          /*should_handle*/ true, &value_3, kUrl3,
                          /*expected_user_is_interacting*/ false,
                          /*expected_is_main_frame*/ false, &subframe),
      kPrefix3);

  // Check that a irrelevant or invalid command does not trigger the callbacks.
  EXPECT_FALSE(web_state_->OnScriptCommandReceived(
      "wohoo.blah", value_1, kUrl1,
      /*user_is_interacting*/ false, /*is_main_frame*/ true,
      /*sender_frame*/ &main_frame));
  EXPECT_FALSE(is_called_1);
  EXPECT_FALSE(is_called_2);
  EXPECT_FALSE(is_called_3);

  EXPECT_FALSE(web_state_->OnScriptCommandReceived(
      "prefix1ButMissingDot", value_1, kUrl1, /*user_is_interacting*/ false,
      /*is_main_frame*/ true, /*sender_frame*/ &main_frame));
  EXPECT_FALSE(is_called_1);
  EXPECT_FALSE(is_called_2);
  EXPECT_FALSE(is_called_3);

  // Check that only the callback matching the prefix is called, with the
  // expected parameters and return value;
  EXPECT_TRUE(
      web_state_->OnScriptCommandReceived(kCommand1, value_1, kUrl1,
                                          /*user_is_interacting*/ false,
                                          /*is_main_frame*/ true,
                                          /*sender_frame*/ &main_frame));
  EXPECT_TRUE(is_called_1);
  EXPECT_FALSE(is_called_2);
  EXPECT_FALSE(is_called_3);
  is_called_1 = false;
  // Check that sending message from iframe sets |is_main_frame| to false.
  EXPECT_TRUE(web_state_->OnScriptCommandReceived(kCommand3, value_3, kUrl3,
                                                  /*user_is_interacting*/ false,
                                                  /*is_main_frame*/ false,
                                                  /*sender_frame*/ &subframe));
  EXPECT_FALSE(is_called_1);
  EXPECT_FALSE(is_called_2);
  EXPECT_TRUE(is_called_3);
  is_called_3 = false;

  // Remove the callback and check it is no longer called.
  web_state_->RemoveScriptCommandCallback(kPrefix1);
  EXPECT_FALSE(web_state_->OnScriptCommandReceived(
      kCommand1, value_1, kUrl1,
      /*user_is_interacting*/ false, /*is_main_frame*/ true,
      /*sender_frame*/ &main_frame));
  EXPECT_FALSE(is_called_1);
  EXPECT_FALSE(is_called_2);
  EXPECT_FALSE(is_called_3);

  // Check that a false return value is forwarded correctly.
  EXPECT_FALSE(web_state_->OnScriptCommandReceived(
      kCommand2, value_2, kUrl2,
      /*user_is_interacting*/ false, /*is_main_frame*/ true,
      /*sender_frame*/ &main_frame));
  EXPECT_FALSE(is_called_1);
  EXPECT_TRUE(is_called_2);
  EXPECT_FALSE(is_called_3);

  web_state_->RemoveScriptCommandCallback(kPrefix2);
  web_state_->RemoveScriptCommandCallback(kPrefix3);
}

// Tests that WebState::CreateParams::created_with_opener is translated to
// WebState::HasOpener() return values.
TEST_P(WebStateImplTest, CreatedWithOpener) {
  // Verify that the HasOpener() returns false if not specified in the create
  // params.
  EXPECT_FALSE(web_state_->HasOpener());
  // Set |created_with_opener| to true and verify that HasOpener() returns true.
  WebState::CreateParams params_with_opener =
      WebState::CreateParams(GetBrowserState());
  params_with_opener.created_with_opener = true;
  std::unique_ptr<WebState> web_state_with_opener =
      WebState::Create(params_with_opener);
  EXPECT_TRUE(web_state_with_opener->HasOpener());
}

// Tests that WebStateObserver::FaviconUrlUpdated is called for same-document
// navigations.
TEST_P(WebStateImplTest, FaviconUpdateForSameDocumentNavigations) {
  auto observer = std::make_unique<TestWebStateObserver>(web_state_.get());

  // No callback if icons has not been fetched yet.
  FakeNavigationContext context;
  context.SetIsSameDocument(true);
  web_state_->OnNavigationFinished(&context);
  EXPECT_FALSE(observer->update_favicon_url_candidates_info());

  // Callback is called when icons were fetched.
  observer = std::make_unique<TestWebStateObserver>(web_state_.get());
  web::FaviconURL favicon_url(GURL("https://chromium.test/"),
                              web::FaviconURL::IconType::kTouchIcon,
                              {gfx::Size(5, 6)});
  web_state_->OnFaviconUrlUpdated({favicon_url});
  EXPECT_TRUE(observer->update_favicon_url_candidates_info());

  // Callback is now called after same-document navigation.
  observer = std::make_unique<TestWebStateObserver>(web_state_.get());
  web_state_->OnNavigationFinished(&context);
  ASSERT_TRUE(observer->update_favicon_url_candidates_info());
  ASSERT_EQ(1U,
            observer->update_favicon_url_candidates_info()->candidates.size());
  const web::FaviconURL& actual_favicon_url =
      observer->update_favicon_url_candidates_info()->candidates[0];
  EXPECT_EQ(favicon_url.icon_url, actual_favicon_url.icon_url);
  EXPECT_EQ(favicon_url.icon_type, actual_favicon_url.icon_type);
  ASSERT_EQ(favicon_url.icon_sizes.size(),
            actual_favicon_url.icon_sizes.size());
  EXPECT_EQ(favicon_url.icon_sizes[0].width(),
            actual_favicon_url.icon_sizes[0].width());
  EXPECT_EQ(favicon_url.icon_sizes[0].height(),
            actual_favicon_url.icon_sizes[0].height());

  // Document change navigation does not call callback.
  observer = std::make_unique<TestWebStateObserver>(web_state_.get());
  context.SetIsSameDocument(false);
  web_state_->OnNavigationFinished(&context);
  EXPECT_FALSE(observer->update_favicon_url_candidates_info());

  // Previous candidates were invalidated by the document change. No callback
  // if icons has not been fetched yet.
  context.SetIsSameDocument(true);
  web_state_->OnNavigationFinished(&context);
  EXPECT_FALSE(observer->update_favicon_url_candidates_info());
}

// Tests that BuildSessionStorage() and GetTitle() return information about the
// most recently restored session if no navigation item has been committed.
TEST_P(WebStateImplTest, UncommittedRestoreSession) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      web::features::kSlimNavigationManager);

  GURL url("http://test.com");
  CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
  session_storage.lastCommittedItemIndex = 0;
  CRWNavigationItemStorage* item_storage =
      [[CRWNavigationItemStorage alloc] init];
  item_storage.title = base::SysNSStringToUTF16(@"Title");
  item_storage.virtualURL = url;
  session_storage.itemStorages = @[ item_storage ];

  web::WebState::CreateParams params(GetBrowserState());
  WebStateImpl web_state(params, session_storage);

  CRWSessionStorage* extracted_session_storage =
      web_state.BuildSessionStorage();
  EXPECT_EQ(0, extracted_session_storage.lastCommittedItemIndex);
  EXPECT_EQ(1U, extracted_session_storage.itemStorages.count);
  EXPECT_NSEQ(@"Title", base::SysUTF16ToNSString(web_state.GetTitle()));
  EXPECT_EQ(url, web_state.GetVisibleURL());
}

TEST_P(WebStateImplTest, NoUncommittedRestoreSession) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      web::features::kSlimNavigationManager);

  CRWSessionStorage* session_storage = web_state_->BuildSessionStorage();
  EXPECT_EQ(-1, session_storage.lastCommittedItemIndex);
  EXPECT_NSEQ(@[], session_storage.itemStorages);
  EXPECT_TRUE(web_state_->GetTitle().empty());
  EXPECT_EQ(GURL::EmptyGURL(), web_state_->GetVisibleURL());
}

// Tests showing and clearing interstitial when NavigationManager is
// empty.
TEST_P(WebStateImplTest, ShowAndClearInterstitialWithNoCommittedItems) {
  web_state_->GetNavigationManagerImpl().InitializeSession();

  // Existence of a pending item is a precondition for a transient item.
  web_state_->GetNavigationManagerImpl().AddPendingItem(
      GURL::EmptyGURL(), web::Referrer(), ui::PAGE_TRANSITION_LINK,
      NavigationInitiationType::BROWSER_INITIATED,
      NavigationManager::UserAgentOverrideOption::DESKTOP);

  // Show the interstitial.
  ASSERT_FALSE(web_state_->IsShowingWebInterstitial());
  ASSERT_FALSE(web_state_->GetWebInterstitial());
  WebInterstitialImpl* interstitial = ShowInterstitial();
  ASSERT_EQ(interstitial, web_state_->GetWebInterstitial());
  ASSERT_TRUE(web_state_->IsShowingWebInterstitial());

  // Clear the interstitial.
  TestWebStateObserver observer(web_state_.get());
  ASSERT_FALSE(observer.did_change_visible_security_state_info());
  web_state_->ClearTransientContent();

  // Verify that interstitial was removed and DidChangeVisibleSecurityState was
  // called.
  EXPECT_FALSE(web_state_->IsShowingWebInterstitial());
  EXPECT_FALSE(web_state_->GetWebInterstitial());
  ASSERT_TRUE(observer.did_change_visible_security_state_info());
  EXPECT_EQ(web_state_.get(),
            observer.did_change_visible_security_state_info()->web_state);
}

// Tests showing and clearing interstitial when NavigationManager has a
// committed item.
TEST_P(WebStateImplTest, ShowAndClearInterstitialWithCommittedItem) {
  if (GetParam() == NavigationManagerChoice::WK_BASED) {
    // TODO(crbug.com/862733): This test requires injecting a committed item to
    // navigation manager, which can't be done with WKBasedNavigationManager.
    // Re-enable this test after switching to TestNavigationManager.
    return;
  }

  // Add SECURITY_STYLE_AUTHENTICATED committed item to navigation manager.
  AddCommittedNavigationItem();
  web_state_->GetNavigationManagerImpl()
      .GetLastCommittedItem()
      ->GetSSL()
      .security_style = SECURITY_STYLE_AUTHENTICATED;

  // Show the interstitial.
  ASSERT_FALSE(web_state_->IsShowingWebInterstitial());
  ASSERT_FALSE(web_state_->GetWebInterstitial());
  WebInterstitialImpl* interstitial = ShowInterstitial();
  ASSERT_TRUE(web_state_->IsShowingWebInterstitial());
  ASSERT_EQ(interstitial, web_state_->GetWebInterstitial());

  // Clear the interstitial.
  TestWebStateObserver observer(web_state_.get());
  ASSERT_FALSE(observer.did_change_visible_security_state_info());
  web_state_->ClearTransientContent();

  // Verify that interstitial was removed and DidChangeVisibleSecurityState was
  // called.
  EXPECT_FALSE(web_state_->IsShowingWebInterstitial());
  EXPECT_FALSE(web_state_->GetWebInterstitial());
  ASSERT_TRUE(observer.did_change_visible_security_state_info());
  EXPECT_EQ(web_state_.get(),
            observer.did_change_visible_security_state_info()->web_state);
}

// Tests showing and clearing interstitial when visible SSL status does not
// change.
TEST_P(WebStateImplTest, ShowAndClearInterstitialWithoutChangingSslStatus) {
  if (GetParam() == NavigationManagerChoice::WK_BASED) {
    // TODO(crbug.com/862733): This test requires injecting a committed item to
    // navigation manager, which can't be done with WKBasedNavigationManager.
    // Re-enable this test after switching to TestNavigationManager.
    return;
  }

  // Add a committed item to navigation manager with default SSL status.
  AddCommittedNavigationItem();

  // Show the interstitial.
  ASSERT_FALSE(web_state_->IsShowingWebInterstitial());
  ASSERT_FALSE(web_state_->GetWebInterstitial());
  WebInterstitialImpl* interstitial = ShowInterstitial();
  ASSERT_TRUE(web_state_->IsShowingWebInterstitial());
  ASSERT_EQ(interstitial, web_state_->GetWebInterstitial());

  // Clear the interstitial.
  TestWebStateObserver observer(web_state_.get());
  ASSERT_FALSE(observer.did_change_visible_security_state_info());
  web_state_->ClearTransientContent();

  // Verify that interstitial was removed.
  EXPECT_FALSE(web_state_->IsShowingWebInterstitial());
  EXPECT_FALSE(web_state_->GetWebInterstitial());
  // DidChangeVisibleSecurityState is not called, because last committed and
  // transient items had the same SSL status.
  EXPECT_FALSE(observer.did_change_visible_security_state_info());
}

INSTANTIATE_TEST_CASE_P(ProgrammaticWebStateImplTest,
                        WebStateImplTest,
                        ::testing::Values(NavigationManagerChoice::LEGACY,
                                          NavigationManagerChoice::WK_BASED));

}  // namespace web
