// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_state_impl.h"

#import <stddef.h>

#import <memory>

#import <OCMock/OCMock.h>

#import "base/base64.h"
#import "base/bind.h"
#import "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/gmock_callback_support.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/web/common/features.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/serializable_user_data_manager_impl.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/deprecated/global_web_state_observer.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/public/test/fakes/async_web_state_policy_decider.h"
#import "ios/web/public/test/fakes/fake_java_script_dialog_presenter.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_state_delegate.h"
#import "ios/web/public/test/fakes/fake_web_state_observer.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/public/ui/java_script_dialog_presenter.h"
#import "ios/web/public/web_state_delegate.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/web_state/global_web_state_event_tracker.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_policy_decider_test_util.h"
#import "net/http/http_response_headers.h"
#import "net/http/http_util.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::_;
using testing::Assign;
using testing::AtMost;
using testing::DoAll;
using testing::Return;
using base::test::RunOnceCallback;
using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForPageLoadTimeout;

namespace web {
namespace {

// Test observer to check that the GlobalWebStateObserver methods are called as
// expected.
class TestGlobalWebStateObserver : public GlobalWebStateObserver {
 public:
  TestGlobalWebStateObserver()
      : GlobalWebStateObserver(),
        did_start_loading_called_(false),
        did_stop_loading_called_(false),
        did_start_navigation_called_(false),
        web_state_destroyed_called_(false) {}

  // Methods returning true if the corresponding GlobalWebStateObserver method
  // has been called.
  bool did_start_loading_called() const { return did_start_loading_called_; }
  bool did_stop_loading_called() const { return did_stop_loading_called_; }
  bool did_start_navigation_called() const {
    return did_start_navigation_called_;
  }
  bool web_state_destroyed_called() const {
    return web_state_destroyed_called_;
  }

 private:
  // GlobalWebStateObserver implementation:
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
  void WebStateDestroyed(WebState* web_state) override {
    web_state_destroyed_called_ = true;
  }

  bool did_start_loading_called_;
  bool did_stop_loading_called_;
  bool did_start_navigation_called_;
  bool web_state_destroyed_called_;
};

// Test decider to check that the WebStatePolicyDecider methods are called as
// expected.
class MockWebStatePolicyDecider : public WebStatePolicyDecider {
 public:
  explicit MockWebStatePolicyDecider(WebState* web_state)
      : WebStatePolicyDecider(web_state) {}
  ~MockWebStatePolicyDecider() override {}

  MOCK_METHOD3(ShouldAllowRequest,
               void(NSURLRequest* request,
                    WebStatePolicyDecider::RequestInfo request_info,
                    WebStatePolicyDecider::PolicyDecisionCallback callback));
  MOCK_METHOD3(ShouldAllowResponse,
               void(NSURLResponse* response,
                    WebStatePolicyDecider::ResponseInfo response_info,
                    WebStatePolicyDecider::PolicyDecisionCallback callback));
  MOCK_METHOD0(WebStateDestroyed, void());
};

// Test callback for script commands.
// Sets `is_called` to true if it is called, and checks that the parameters
// match their expected values.
void HandleScriptCommand(bool* is_called,
                         base::Value* expected_value,
                         const GURL& expected_url,
                         bool expected_user_is_interacting,
                         web::WebFrame* expected_sender_frame,
                         const base::Value& value,
                         const GURL& url,
                         bool user_is_interacting,
                         web::WebFrame* sender_frame) {
  *is_called = true;
  EXPECT_EQ(*expected_value, value);
  EXPECT_EQ(expected_url, url);
  EXPECT_EQ(expected_user_is_interacting, user_is_interacting);
  EXPECT_EQ(expected_sender_frame, sender_frame);
}

}  // namespace

// Test fixture for web::WebStateImpl class.
class WebStateImplTest : public web::WebTest {
 protected:
  WebStateImplTest() = default;

  void SetUp() override {
    web::WebTest::SetUp();
    web::WebState::CreateParams params(GetBrowserState());
    web_state_ = std::make_unique<web::WebStateImpl>(params);
  }

  // Adds PendingNavigationItem and commits it.
  void AddCommittedNavigationItem() {
    web_state_->GetNavigationManagerImpl().AddPendingItem(
        GURL::EmptyGURL(), web::Referrer(), ui::PAGE_TRANSITION_LINK,
        NavigationInitiationType::RENDERER_INITIATED,
        /*is_post_navigation=*/false, HttpsUpgradeType::kNone);
    web_state_->GetNavigationManagerImpl().CommitPendingItem();
  }

  std::unique_ptr<WebStateImpl> web_state_;
};

// Tests WebState::GetWeakPtr.
TEST_F(WebStateImplTest, GetWeakPtr) {
  base::WeakPtr<WebState> weak_ptr = web_state_->GetWeakPtr();

  // Verify that `weak_ptr` points to `web_state_`.
  EXPECT_EQ(weak_ptr.get(), web_state_.get());

  // Verify that `weak_ptr` is null after `web_state_` destruction.
  web_state_.reset();
  EXPECT_EQ(weak_ptr.get(), nullptr);
}

TEST_F(WebStateImplTest, WebUsageEnabled) {
  // Default is false.
  ASSERT_TRUE(web_state_->IsWebUsageEnabled());

  web_state_->SetWebUsageEnabled(false);
  EXPECT_FALSE(web_state_->IsWebUsageEnabled());
  EXPECT_FALSE(web_state_->GetWebController().webUsageEnabled);

  web_state_->SetWebUsageEnabled(true);
  EXPECT_TRUE(web_state_->IsWebUsageEnabled());
  EXPECT_TRUE(web_state_->GetWebController().webUsageEnabled);
}

// Tests forwarding to WebStateObserver callbacks.
TEST_F(WebStateImplTest, ObserverTest) {
  auto observer = std::make_unique<FakeWebStateObserver>(web_state_.get());
  EXPECT_EQ(web_state_.get(), observer->web_state());

  // Test that WasShown() is called.
  ASSERT_FALSE(web_state_->IsVisible());
  ASSERT_FALSE(observer->was_shown_info());
  web_state_->WasShown();
  ASSERT_TRUE(observer->was_shown_info());
  EXPECT_EQ(web_state_.get(), observer->was_shown_info()->web_state);
  EXPECT_TRUE(web_state_->IsVisible());

  // Test that WasShown() callback is not called for the second time.
  observer = std::make_unique<FakeWebStateObserver>(web_state_.get());
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
  observer = std::make_unique<FakeWebStateObserver>(web_state_.get());
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
  auto main_frame = FakeWebFrame::CreateMainWebFrame(GURL::EmptyGURL());
  WebFrame* main_frame_ptr = main_frame.get();
  web_state_->WebFrameBecameAvailable(std::move(main_frame));
  ASSERT_TRUE(observer->web_frame_available_info());
  EXPECT_EQ(web_state_.get(), observer->web_frame_available_info()->web_state);
  EXPECT_EQ(main_frame_ptr, observer->web_frame_available_info()->web_frame);

  // Test that WebFrameWillBecomeUnavailable() is called.
  ASSERT_FALSE(observer->web_frame_unavailable_info());
  web_state_->WebFrameBecameUnavailable(main_frame_ptr->GetFrameId());
  ASSERT_TRUE(observer->web_frame_unavailable_info());
  EXPECT_EQ(web_state_.get(),
            observer->web_frame_unavailable_info()->web_state);
  EXPECT_EQ(main_frame_ptr, observer->web_frame_unavailable_info()->web_frame);

  // Test that RenderProcessGone() is called.
  SetIgnoreRenderProcessCrashesDuringTesting(true);
  ASSERT_FALSE(observer->render_process_gone_info());
  web_state_->OnRenderProcessGone();
  ASSERT_TRUE(observer->render_process_gone_info());
  EXPECT_EQ(web_state_.get(), observer->render_process_gone_info()->web_state);

  // Test that DidFinishNavigation() is called.
  ASSERT_FALSE(observer->did_finish_navigation_info());
  const GURL url("http://test");
  std::unique_ptr<NavigationContextImpl> context =
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
  observer = std::make_unique<FakeWebStateObserver>(web_state_.get());
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

// Tests that WebStateDelegate methods appropriately called.
TEST_F(WebStateImplTest, DelegateTest) {
  FakeWebStateDelegate delegate;
  web_state_->SetDelegate(&delegate);

  // Test that CreateNewWebState() is called.
  GURL child_url("https://child.test/");
  GURL opener_url("https://opener.test/");
  EXPECT_FALSE(delegate.last_create_new_web_state_request());
  web_state_->CreateNewWebState(child_url, opener_url, true);
  FakeCreateNewWebStateRequest* create_new_web_state_request =
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

  // Test that OpenURLFromWebState() is called without a virtual URL.
  WebState::OpenURLParams params(GURL("https://chromium.test/"), Referrer(),
                                 WindowOpenDisposition::CURRENT_TAB,
                                 ui::PAGE_TRANSITION_LINK, true);
  EXPECT_FALSE(delegate.last_open_url_request());
  web_state_->OpenURL(params);
  FakeOpenURLRequest* open_url_request = delegate.last_open_url_request();
  ASSERT_TRUE(open_url_request);
  EXPECT_EQ(web_state_.get(), open_url_request->web_state);
  WebState::OpenURLParams actual_params = open_url_request->params;
  EXPECT_EQ(params.url, actual_params.url);
  EXPECT_EQ(GURL::EmptyGURL(), params.virtual_url);
  EXPECT_EQ(GURL::EmptyGURL(), actual_params.virtual_url);
  EXPECT_EQ(params.referrer.url, actual_params.referrer.url);
  EXPECT_EQ(params.referrer.policy, actual_params.referrer.policy);
  EXPECT_EQ(params.disposition, actual_params.disposition);
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(params.transition, actual_params.transition));
  EXPECT_EQ(params.is_renderer_initiated, actual_params.is_renderer_initiated);

  // Test that OpenURLFromWebState() is called with a virtual URL.
  params = WebState::OpenURLParams(
      GURL("https://chromium.test/"), GURL("https://virtual.chromium.test/"),
      Referrer(), WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_LINK,
      true);
  web_state_->OpenURL(params);
  open_url_request = delegate.last_open_url_request();
  ASSERT_TRUE(open_url_request);
  EXPECT_EQ(web_state_.get(), open_url_request->web_state);
  actual_params = open_url_request->params;
  EXPECT_EQ(params.url, actual_params.url);
  EXPECT_EQ(params.virtual_url, actual_params.virtual_url);
  EXPECT_EQ(params.referrer.url, actual_params.referrer.url);
  EXPECT_EQ(params.referrer.policy, actual_params.referrer.policy);
  EXPECT_EQ(params.disposition, actual_params.disposition);
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(params.transition, actual_params.transition));
  EXPECT_EQ(params.is_renderer_initiated, actual_params.is_renderer_initiated);

  // Test that ShowRepostFormWarningDialog() is called.
  EXPECT_FALSE(delegate.last_repost_form_request());
  base::OnceCallback<void(bool)> repost_callback;
  web_state_->ShowRepostFormWarningDialog(std::move(repost_callback));
  ASSERT_TRUE(delegate.last_repost_form_request());
  EXPECT_EQ(delegate.last_repost_form_request()->web_state, web_state_.get());

  // Test that GetJavaScriptDialogPresenter() is called.
  FakeJavaScriptDialogPresenter* presenter =
      delegate.GetFakeJavaScriptDialogPresenter();
  EXPECT_FALSE(delegate.get_java_script_dialog_presenter_called());
  EXPECT_TRUE(presenter->requested_alert_dialogs().empty());
  EXPECT_FALSE(presenter->cancel_dialogs_called());

  __block bool callback_called = false;
  web_state_->RunJavaScriptAlertDialog(GURL(), @"", base::BindOnce(^() {
                                         callback_called = true;
                                       }));

  EXPECT_TRUE(delegate.get_java_script_dialog_presenter_called());
  EXPECT_EQ(1U, presenter->requested_alert_dialogs().size());
  EXPECT_TRUE(callback_called);

  EXPECT_FALSE(presenter->cancel_dialogs_called());
  web_state_->CancelDialogs();
  EXPECT_TRUE(presenter->cancel_dialogs_called());

  // Test that OnAuthRequired() is called.
  EXPECT_FALSE(delegate.last_authentication_request());
  NSURLProtectionSpace* protection_space = [[NSURLProtectionSpace alloc] init];
  NSURLCredential* credential = [[NSURLCredential alloc] init];
  WebStateDelegate::AuthCallback callback;
  web_state_->OnAuthRequired(protection_space, credential, std::move(callback));
  ASSERT_TRUE(delegate.last_authentication_request());
  EXPECT_EQ(delegate.last_authentication_request()->web_state,
            web_state_.get());
  EXPECT_EQ(delegate.last_authentication_request()->protection_space,
            protection_space);
  EXPECT_EQ(delegate.last_authentication_request()->credential, credential);
}

// Verifies that GlobalWebStateObservers are called when expected.
TEST_F(WebStateImplTest, GlobalObserverTest) {
  std::unique_ptr<TestGlobalWebStateObserver> observer(
      new TestGlobalWebStateObserver());

  // Test that DidStartNavigation() is called.
  EXPECT_FALSE(observer->did_start_navigation_called());
  std::unique_ptr<NavigationContextImpl> context =
      NavigationContextImpl::CreateNavigationContext(
          web_state_.get(), GURL::EmptyGURL(), /*has_user_gesture=*/true,
          ui::PageTransition::PAGE_TRANSITION_AUTO_BOOKMARK,
          /*is_renderer_initiated=*/true);
  web_state_->OnNavigationStarted(context.get());
  EXPECT_TRUE(observer->did_start_navigation_called());

  // Test that WebStateDidStartLoading() is called.
  EXPECT_FALSE(observer->did_start_loading_called());
  web_state_->SetIsLoading(true);
  EXPECT_TRUE(observer->did_start_loading_called());

  // Test that WebStateDidStopLoading() is called.
  EXPECT_FALSE(observer->did_stop_loading_called());
  web_state_->SetIsLoading(false);
  EXPECT_TRUE(observer->did_stop_loading_called());

  // Test that WebStateDestroyed() is called.
  EXPECT_FALSE(observer->web_state_destroyed_called());
  web_state_.reset();
  EXPECT_TRUE(observer->web_state_destroyed_called());
}

// A Google Mock matcher which matches WebStatePolicyDecider::RequestInfo.
// This is needed because WebStatePolicyDecider::RequestInfo doesn't support
// operator==.
MATCHER_P(RequestInfoMatch, expected_request_info, /* argument_name = */ "") {
  return ::web::RequestInfoMatch(expected_request_info, arg);
}

// A Google Mock matcher which matches WebStatePolicyDecider::ResponseInfo.
// This is needed because WebStatePolicyDecider::ResponseInfo doesn't support
// operator==.
MATCHER_P(ResponseInfoMatch, expected_response_info, /* argument_name = */ "") {
  return ::web::ResponseInfoMatch(expected_response_info, arg);
}

// Verifies that policy deciders are correctly called by the web state.
TEST_F(WebStateImplTest, PolicyDeciderTest) {
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
  const WebStatePolicyDecider::RequestInfo request_info_main_frame(
      ui::PageTransition::PAGE_TRANSITION_LINK,
      /*target_main_frame=*/true,
      /*target_frame_is_cross_origin=*/false,
      /*has_user_gesture=*/false);
  EXPECT_CALL(
      decider,
      ShouldAllowRequest(request, RequestInfoMatch(request_info_main_frame), _))
      .Times(1)
      .WillOnce(
          RunOnceCallback<2>(WebStatePolicyDecider::PolicyDecision::Allow()));
  EXPECT_CALL(
      decider2,
      ShouldAllowRequest(request, RequestInfoMatch(request_info_main_frame), _))
      .Times(1)
      .WillOnce(
          RunOnceCallback<2>(WebStatePolicyDecider::PolicyDecision::Allow()));

  WebStatePolicyDecider::PolicyDecision policy_decision =
      WebStatePolicyDecider::PolicyDecision::Cancel();
  auto callback = base::BindRepeating(
      [](WebStatePolicyDecider::PolicyDecision* policy_decision,
         WebStatePolicyDecider::PolicyDecision result) {
        *policy_decision = result;
      },
      base::Unretained(&policy_decision));
  web_state_->ShouldAllowRequest(request, request_info_main_frame, callback);
  EXPECT_TRUE(policy_decision.ShouldAllowNavigation());
  EXPECT_FALSE(policy_decision.ShouldCancelNavigation());

  const WebStatePolicyDecider::RequestInfo request_info_iframe(
      ui::PageTransition::PAGE_TRANSITION_LINK,
      /*target_main_frame=*/false,
      /*target_frame_is_cross_origin=*/false,
      /*has_user_gesture=*/false);
  EXPECT_CALL(decider, ShouldAllowRequest(
                           request, RequestInfoMatch(request_info_iframe), _))
      .Times(1)
      .WillOnce(
          RunOnceCallback<2>(WebStatePolicyDecider::PolicyDecision::Allow()));
  EXPECT_CALL(decider2, ShouldAllowRequest(
                            request, RequestInfoMatch(request_info_iframe), _))
      .Times(1)
      .WillOnce(
          RunOnceCallback<2>(WebStatePolicyDecider::PolicyDecision::Allow()));

  web_state_->ShouldAllowRequest(request, request_info_iframe, callback);
  EXPECT_TRUE(policy_decision.ShouldAllowNavigation());
  EXPECT_FALSE(policy_decision.ShouldCancelNavigation());

  // Test that ShouldAllowRequest() is stopping on negative answer. Only one
  // of the deciders should be called.
  {
    bool decider_called = false;
    bool decider2_called = false;
    EXPECT_CALL(decider,
                ShouldAllowRequest(
                    request, RequestInfoMatch(request_info_main_frame), _))
        .Times(AtMost(1))
        .WillOnce(DoAll(Assign(&decider_called, true),
                        RunOnceCallback<2>(
                            WebStatePolicyDecider::PolicyDecision::Cancel())));
    EXPECT_CALL(decider2,
                ShouldAllowRequest(
                    request, RequestInfoMatch(request_info_main_frame), _))
        .Times(AtMost(1))
        .WillOnce(DoAll(Assign(&decider2_called, true),
                        RunOnceCallback<2>(
                            WebStatePolicyDecider::PolicyDecision::Cancel())));

    web_state_->ShouldAllowRequest(request, request_info_main_frame, callback);
    EXPECT_FALSE(policy_decision.ShouldAllowNavigation());
    EXPECT_TRUE(policy_decision.ShouldCancelNavigation());
    EXPECT_FALSE(decider_called && decider2_called);
  }

  const WebStatePolicyDecider::ResponseInfo response_info_main_frame(
      /* for_main_frame = */ true);

  // Test that ShouldAllowResponse() is called.
  EXPECT_CALL(decider,
              ShouldAllowResponse(
                  response, ResponseInfoMatch(response_info_main_frame), _))
      .Times(1)
      .WillOnce(
          RunOnceCallback<2>(WebStatePolicyDecider::PolicyDecision::Allow()));
  EXPECT_CALL(decider2,
              ShouldAllowResponse(
                  response, ResponseInfoMatch(response_info_main_frame), _))
      .Times(1)
      .WillOnce(
          RunOnceCallback<2>(WebStatePolicyDecider::PolicyDecision::Allow()));

  web_state_->ShouldAllowResponse(response, response_info_main_frame, callback);
  EXPECT_TRUE(policy_decision.ShouldAllowNavigation());
  EXPECT_FALSE(policy_decision.ShouldCancelNavigation());

  // Test that ShouldAllowResponse() is stopping on negative answer. Only the
  // first decider should be called.
  {
    const WebStatePolicyDecider::ResponseInfo response_info_iframe(
        /* for_main_frame = */ false);

    EXPECT_CALL(decider,
                ShouldAllowResponse(response,
                                    ResponseInfoMatch(response_info_iframe), _))
        .Times(1)
        .WillOnce(RunOnceCallback<2>(
            WebStatePolicyDecider::PolicyDecision::Cancel()));

    web_state_->ShouldAllowResponse(response, response_info_iframe,
                                    std::move(callback));
    EXPECT_FALSE(policy_decision.ShouldAllowNavigation());
    EXPECT_TRUE(policy_decision.ShouldCancelNavigation());
  }

  EXPECT_CALL(decider, WebStateDestroyed()).Times(1);
  EXPECT_CALL(decider2, WebStateDestroyed()).Times(1);
  web_state_.reset();
  EXPECT_EQ(nullptr, decider.web_state());
}

// Verifies that asynchronous decisions for
// WebStatePolicyDecider::ShouldAllowResponse are correctly handled by
// WebStateImpl::ShouldAllowResponse.
TEST_F(WebStateImplTest, AsyncShouldAllowResponseTest) {
  MockWebStatePolicyDecider sync_decider(web_state_.get());
  AsyncWebStatePolicyDecider async_decider1(web_state_.get());
  AsyncWebStatePolicyDecider async_decider2(web_state_.get());

  NSURL* url = [NSURL URLWithString:@"http://example.com"];
  NSURLResponse* response = [[NSURLResponse alloc] initWithURL:url
                                                      MIMEType:@"text/html"
                                         expectedContentLength:0
                                              textEncodingName:nil];

  __block WebStatePolicyDecider::PolicyDecision policy_decision =
      WebStatePolicyDecider::PolicyDecision::Allow();
  __block bool callback_called = false;

  const WebStatePolicyDecider::ResponseInfo expected_response_info(
      /*for_main_frame=*/true);

  base::RepeatingCallback<void(WebStatePolicyDecider::PolicyDecision)>
      callback =
          base::BindRepeating(^(WebStatePolicyDecider::PolicyDecision result) {
            policy_decision = result;
            callback_called = true;
          });

  // Case 1: All deciders allow the navigation.
  EXPECT_CALL(sync_decider,
              ShouldAllowResponse(response,
                                  ResponseInfoMatch(expected_response_info), _))
      .Times(1)
      .WillOnce(
          RunOnceCallback<2>(WebStatePolicyDecider::PolicyDecision::Allow()));
  web_state_->ShouldAllowResponse(response, expected_response_info, callback);
  EXPECT_FALSE(callback_called);
  async_decider1.InvokeCallback(WebStatePolicyDecider::PolicyDecision::Allow());
  EXPECT_FALSE(callback_called);
  async_decider2.InvokeCallback(WebStatePolicyDecider::PolicyDecision::Allow());
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(policy_decision.ShouldAllowNavigation());

  // Case 2: One decider allows the navigation, one decider wants to show an
  // error, and another decider wants to cancel the navigation. In this case,
  // the navigation should be cancelled, with no error shown.
  EXPECT_CALL(sync_decider,
              ShouldAllowResponse(response,
                                  ResponseInfoMatch(expected_response_info), _))
      .Times(1)
      .WillOnce(
          RunOnceCallback<2>(WebStatePolicyDecider::PolicyDecision::Allow()));
  callback_called = false;
  web_state_->ShouldAllowResponse(response, expected_response_info, callback);
  EXPECT_FALSE(callback_called);
  NSError* error1 = [NSError errorWithDomain:@"ErrorDomain"
                                        code:1
                                    userInfo:nil];
  async_decider2.InvokeCallback(
      WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(error1));
  EXPECT_FALSE(callback_called);
  async_decider1.InvokeCallback(
      WebStatePolicyDecider::PolicyDecision::Cancel());
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(policy_decision.ShouldCancelNavigation());
  EXPECT_FALSE(policy_decision.ShouldDisplayError());

  // Case 3: Two deciders want to show an error. In this case, the error to be
  // shown should be from the decider that responded first.
  EXPECT_CALL(sync_decider,
              ShouldAllowResponse(response,
                                  ResponseInfoMatch(expected_response_info), _))
      .Times(1)
      .WillOnce(
          RunOnceCallback<2>(WebStatePolicyDecider::PolicyDecision::Allow()));
  callback_called = false;
  web_state_->ShouldAllowResponse(response, expected_response_info, callback);
  EXPECT_FALSE(callback_called);
  NSError* error2 = [NSError errorWithDomain:@"ErrorDomain"
                                        code:2
                                    userInfo:nil];
  async_decider1.InvokeCallback(
      WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(error2));
  EXPECT_FALSE(callback_called);
  async_decider2.InvokeCallback(
      WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(error1));
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(policy_decision.ShouldCancelNavigation());
  EXPECT_TRUE(policy_decision.ShouldDisplayError());
  EXPECT_EQ(policy_decision.GetDisplayError().code, error2.code);
}

// Tests that script command callbacks are called correctly.
TEST_F(WebStateImplTest, ScriptCommand) {
  // Set up three script command callbacks.
  const std::string kPrefix1("prefix1");
  const std::string kCommand1("prefix1.command1");
  base::Value value_1(base::Value::Type::DICTIONARY);
  value_1.SetStringKey("a", "b");
  const GURL kUrl1("http://foo");
  bool is_called_1 = false;
  auto main_frame = FakeWebFrame::CreateMainWebFrame(GURL::EmptyGURL());
  base::CallbackListSubscription subscription_1 =
      web_state_->AddScriptCommandCallback(
          base::BindRepeating(
              &HandleScriptCommand, &is_called_1, &value_1, kUrl1,
              /*expected_user_is_interacting*/ false, main_frame.get()),
          kPrefix1);

  const std::string kPrefix2("prefix2");
  const std::string kCommand2("prefix2.command2");
  base::Value value_2(base::Value::Type::DICTIONARY);
  value_2.SetStringKey("c", "d");
  const GURL kUrl2("http://bar");
  bool is_called_2 = false;
  base::CallbackListSubscription subscription_2 =
      web_state_->AddScriptCommandCallback(
          base::BindRepeating(
              &HandleScriptCommand, &is_called_2, &value_2, kUrl2,
              /*expected_user_is_interacting*/ false, main_frame.get()),
          kPrefix2);

  const std::string kPrefix3("prefix3");
  const std::string kCommand3("prefix3.command3");
  base::Value value_3(base::Value::Type::DICTIONARY);
  value_3.SetStringKey("e", "f");
  const GURL kUrl3("http://iframe");
  bool is_called_3 = false;
  auto subframe = FakeWebFrame::CreateChildWebFrame(GURL::EmptyGURL());
  base::CallbackListSubscription subscription_3 =
      web_state_->AddScriptCommandCallback(
          base::BindRepeating(
              &HandleScriptCommand, &is_called_3, &value_3, kUrl3,
              /*expected_user_is_interacting*/ false, subframe.get()),
          kPrefix3);

  // Check that a irrelevant or invalid command does not trigger the callbacks.
  web_state_->OnScriptCommandReceived("wohoo.blah", value_1, kUrl1,
                                      /*user_is_interacting*/ false,
                                      /*sender_frame*/ main_frame.get());
  EXPECT_FALSE(is_called_1);
  EXPECT_FALSE(is_called_2);
  EXPECT_FALSE(is_called_3);

  web_state_->OnScriptCommandReceived("prefix1ButMissingDot", value_1, kUrl1,
                                      /*user_is_interacting*/ false,
                                      /*sender_frame*/ main_frame.get());
  EXPECT_FALSE(is_called_1);
  EXPECT_FALSE(is_called_2);
  EXPECT_FALSE(is_called_3);

  // Check that only the callback matching the prefix is called, with the
  // expected parameters and return value;

  web_state_->OnScriptCommandReceived(kCommand1, value_1, kUrl1,
                                      /*user_is_interacting*/ false,

                                      /*sender_frame*/ main_frame.get());
  EXPECT_TRUE(is_called_1);
  EXPECT_FALSE(is_called_2);
  EXPECT_FALSE(is_called_3);
  is_called_1 = false;
  // Check that sending message from iframe sets `is_main_frame` to false.
  web_state_->OnScriptCommandReceived(kCommand3, value_3, kUrl3,
                                      /*user_is_interacting*/ false,

                                      /*sender_frame*/ subframe.get());
  EXPECT_FALSE(is_called_1);
  EXPECT_FALSE(is_called_2);
  EXPECT_TRUE(is_called_3);
  is_called_3 = false;

  // Remove the callback and check it is no longer called.
  subscription_1 = {};
  web_state_->OnScriptCommandReceived(kCommand1, value_1, kUrl1,
                                      /*user_is_interacting*/ false,
                                      /*sender_frame*/ main_frame.get());
  EXPECT_FALSE(is_called_1);
  EXPECT_FALSE(is_called_2);
  EXPECT_FALSE(is_called_3);

  // Check that a false return value is forwarded correctly.
  web_state_->OnScriptCommandReceived(kCommand2, value_2, kUrl2,
                                      /*user_is_interacting*/ false,
                                      /*sender_frame*/ main_frame.get());
  EXPECT_FALSE(is_called_1);
  EXPECT_TRUE(is_called_2);
  EXPECT_FALSE(is_called_3);
}

// Tests that WebState::CreateParams::created_with_opener is translated to
// WebState::HasOpener() return values.
TEST_F(WebStateImplTest, CreatedWithOpener) {
  // Verify that the HasOpener() returns false if not specified in the create
  // params.
  EXPECT_FALSE(web_state_->HasOpener());
  // Set `created_with_opener` to true and verify that HasOpener() returns true.
  WebState::CreateParams params_with_opener =
      WebState::CreateParams(GetBrowserState());
  params_with_opener.created_with_opener = true;
  std::unique_ptr<WebState> web_state_with_opener =
      WebState::Create(params_with_opener);
  EXPECT_TRUE(web_state_with_opener->HasOpener());
}

// Tests that WebStateObserver::FaviconUrlUpdated is called for same-document
// navigations.
TEST_F(WebStateImplTest, FaviconUpdateForSameDocumentNavigations) {
  auto observer = std::make_unique<FakeWebStateObserver>(web_state_.get());

  // No callback if icons has not been fetched yet.
  std::unique_ptr<NavigationContextImpl> context =
      NavigationContextImpl::CreateNavigationContext(
          web_state_.get(), GURL::EmptyGURL(),
          /*has_user_gesture=*/false, ui::PageTransition::PAGE_TRANSITION_LINK,
          /*is_renderer_initiated=*/false);
  context->SetIsSameDocument(true);
  web_state_->OnNavigationFinished(context.get());
  EXPECT_FALSE(observer->update_favicon_url_candidates_info());

  // Callback is called when icons were fetched.
  observer = std::make_unique<FakeWebStateObserver>(web_state_.get());
  web::FaviconURL favicon_url(GURL("https://chromium.test/"),
                              web::FaviconURL::IconType::kTouchIcon,
                              {gfx::Size(5, 6)});
  web_state_->OnFaviconUrlUpdated({favicon_url});
  EXPECT_TRUE(observer->update_favicon_url_candidates_info());

  // Callback is now called after same-document navigation.
  observer = std::make_unique<FakeWebStateObserver>(web_state_.get());
  web_state_->OnNavigationFinished(context.get());
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
  observer = std::make_unique<FakeWebStateObserver>(web_state_.get());
  context->SetIsSameDocument(false);
  web_state_->OnNavigationFinished(context.get());
  EXPECT_FALSE(observer->update_favicon_url_candidates_info());

  // Previous candidates were invalidated by the document change. No callback
  // if icons has not been fetched yet.
  context->SetIsSameDocument(true);
  web_state_->OnNavigationFinished(context.get());
  EXPECT_FALSE(observer->update_favicon_url_candidates_info());
}

// Tests that BuildSessionStorage() and GetTitle() return information about the
// most recently restored session if no navigation item has been committed. Also
// tests that re-restoring that session includes updated userData.
TEST_F(WebStateImplTest, UncommittedRestoreSession) {
  GURL url("http://test.com");
  CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
  session_storage.stableIdentifier = [[NSUUID UUID] UUIDString];
  session_storage.lastCommittedItemIndex = 0;
  CRWNavigationItemStorage* item_storage =
      [[CRWNavigationItemStorage alloc] init];
  item_storage.title = base::SysNSStringToUTF16(@"Title");
  item_storage.virtualURL = url;
  session_storage.itemStorages = @[ item_storage ];

  web::WebState::CreateParams params(GetBrowserState());
  WebStateImpl web_state(params, session_storage);

  // After restoring `web_state` change the uncommitted state's user data.
  web::SerializableUserDataManager* user_data_manager =
      web::SerializableUserDataManager::FromWebState(&web_state);
  user_data_manager->AddSerializableData(@(1), @"user_data_key");

  CRWSessionStorage* extracted_session_storage =
      web_state.BuildSessionStorage();
  EXPECT_EQ(0, extracted_session_storage.lastCommittedItemIndex);
  EXPECT_EQ(1U, extracted_session_storage.itemStorages.count);
  EXPECT_NSEQ(@"Title", base::SysUTF16ToNSString(web_state.GetTitle()));
  EXPECT_EQ(url, web_state.GetVisibleURL());

  WebStateImpl restored_web_state(params, extracted_session_storage);
  web::SerializableUserDataManager* restored_user_data_manager =
      web::SerializableUserDataManager::FromWebState(&restored_web_state);
  NSNumber* user_data_value = base::mac::ObjCCast<NSNumber>(
      restored_user_data_manager->GetValueForSerializationKey(
          @"user_data_key"));
  EXPECT_EQ(@(1), user_data_value);
}

// Test that lastCommittedItemIndex is end-of-list when there's no defined
// index, such as during a restore.
TEST_F(WebStateImplTest, NoUncommittedRestoreSession) {
  CRWSessionStorage* session_storage = web_state_->BuildSessionStorage();
  EXPECT_EQ(-1, session_storage.lastCommittedItemIndex);
  EXPECT_NSEQ(@[], session_storage.itemStorages);
  EXPECT_TRUE(web_state_->GetTitle().empty());
  EXPECT_EQ(GURL::EmptyGURL(), web_state_->GetVisibleURL());
}

TEST_F(WebStateImplTest, BuildStorageDuringRestore) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({},
                                       {features::kSynthesizedRestoreSession});

  GURL urls[3] = {GURL("https://chromium.test/1"),
                  GURL("https://chromium.test/2"),
                  GURL("https://chromium.test/3")};
  std::vector<std::unique_ptr<NavigationItem>> items;
  for (size_t index = 0; index < std::size(urls); ++index) {
    items.push_back(NavigationItem::Create());
    items.back()->SetURL(urls[index]);
  }

  // Force generation of child views; necessary for some tests.
  web_state_->GetView();
  web_state_->SetKeepRenderProcessAlive(true);

  web_state_->GetNavigationManager()->Restore(0, std::move(items));
  __block bool restore_done = false;
  web_state_->GetNavigationManager()->AddRestoreCompletionCallback(
      base::BindOnce(^{
        restore_done = true;
      }));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return restore_done;
  }));
  // Trying to grab the lastCommittedItemIndex while a restore is happening is
  // undefined, so the last committed item defaults to end-of-list.
  CRWSessionStorage* session_storage = web_state_->BuildSessionStorage();
  EXPECT_EQ(2, session_storage.lastCommittedItemIndex);

  // Now wait until the last committed item is fully loaded, and
  // lastCommittedItemIndex goes back to 0.
  auto block = ^{
    EXPECT_FALSE(
        wk_navigation_util::IsWKInternalUrl(web_state_->GetVisibleURL()));

    return !web_state_->GetNavigationManager()->GetPendingItem() &&
           !web_state_->IsLoading() && web_state_->GetLoadingProgress() == 1.0;
  };
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, block));
  session_storage = web_state_->BuildSessionStorage();
  EXPECT_EQ(0, session_storage.lastCommittedItemIndex);

  // Wait for the error to be displayed.
  EXPECT_TRUE(web::test::WaitForWebViewContainingText(
      web_state_.get(), "error", base::test::ios::kWaitForJSCompletionTimeout));
}

// Tests that CanTakeSnapshot() is false when a JavaScript dialog is being
// presented.
TEST_F(WebStateImplTest, DisallowSnapshotsDuringDialogPresentation) {
  FakeWebStateDelegate delegate;
  web_state_->SetDelegate(&delegate);

  EXPECT_TRUE(web_state_->CanTakeSnapshot());

  // Pause the callback execution to allow testing while the dialog is
  // presented.
  delegate.GetFakeJavaScriptDialogPresenter()->set_callback_execution_paused(
      true);
  web_state_->RunJavaScriptAlertDialog(GURL(), @"message", base::DoNothing());

  // Verify that CanTakeSnapshot() returns no while the dialog is presented.
  EXPECT_FALSE(web_state_->CanTakeSnapshot());

  // Unpause the presenter and verify that snapshots are enabled again.
  delegate.GetFakeJavaScriptDialogPresenter()->set_callback_execution_paused(
      false);
  EXPECT_TRUE(web_state_->CanTakeSnapshot());
}

// Tests that IsJavaScriptDialogRunning() is true when a JavaScript dialog is
// being presented.
TEST_F(WebStateImplTest, VerifyDialogRunningBoolean) {
  FakeWebStateDelegate delegate;
  web_state_->SetDelegate(&delegate);

  EXPECT_FALSE(web_state_->IsJavaScriptDialogRunning());

  // Pause the callback execution to allow testing while the dialog is
  // presented.
  delegate.GetFakeJavaScriptDialogPresenter()->set_callback_execution_paused(
      true);
  web_state_->RunJavaScriptAlertDialog(GURL(), @"message", base::DoNothing());

  // Verify that IsJavaScriptDialogRunning() returns true while the dialog is
  // presented.
  EXPECT_TRUE(web_state_->IsJavaScriptDialogRunning());

  // Unpause the presenter and verify that IsJavaScriptDialogRunning() returns
  // false when the dialog is no longer presented
  delegate.GetFakeJavaScriptDialogPresenter()->set_callback_execution_paused(
      false);
  EXPECT_FALSE(web_state_->IsJavaScriptDialogRunning());
}

// Tests that CreateFullPagePdf invokes completion callback nil when a
// javascript dialog is running
TEST_F(WebStateImplTest, CreateFullPagePdfJavaScriptDialog) {
  if (@available(iOS 14, *)) {
    FakeWebStateDelegate delegate;
    web_state_->SetDelegate(&delegate);

    // Load the HTML content.
    CRWWebController* web_controller = web_state_->GetWebController();
    NSString* html_content =
        @"<html><body><div style='background-color:#FF0000; width:50%; "
         "height:100%;'></div>Hello world</body></html>";
    [web_controller loadHTML:html_content forURL:GURL("http://example.org")];

    ASSERT_TRUE(
        test::WaitForWebViewContainingText(web_state_.get(), "Hello world"));

    // Pause the callback execution to allow testing while the dialog is
    // presented.
    delegate.GetFakeJavaScriptDialogPresenter()->set_callback_execution_paused(
        true);
    web_state_->RunJavaScriptAlertDialog(GURL(), @"message", base::DoNothing());

    // Attempt to create a PDF for this page and validate that it return nil.
    __block NSData* callback_data_when_dialog = nil;
    __block BOOL callback_called_when_dialog = NO;
    web_state_->CreateFullPagePdf(base::BindOnce(^(NSData* pdf_document_data) {
      callback_data_when_dialog = [pdf_document_data copy];
      callback_called_when_dialog = YES;
    }));

    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return callback_called_when_dialog;
    }));

    EXPECT_FALSE(callback_data_when_dialog);

    // Unpause the presenter and verify that it return data instead of nil when
    // the dialog is no longer on the screen
    delegate.GetFakeJavaScriptDialogPresenter()->set_callback_execution_paused(
        false);

    __block NSData* callback_data_no_dialog = nil;
    __block BOOL callback_called_no_dialog = NO;
    web_state_->CreateFullPagePdf(base::BindOnce(^(NSData* pdf_document_data) {
      callback_data_no_dialog = [pdf_document_data copy];
      callback_called_no_dialog = YES;
    }));

    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return callback_called_no_dialog;
    }));

    EXPECT_TRUE(callback_data_no_dialog);
  }
}

// Tests that the WebView is removed from the view hierarchy and the
// visibilitychange JavaScript event is fired when covering/revealing the
// WebContent.
TEST_F(WebStateImplTest, VisibilitychangeEventFired) {
  // Mark the WebState as visibile before adding the observer.
  web_state_->WasShown();

  auto observer = std::make_unique<FakeWebStateObserver>(web_state_.get());

  // Add the WebState to the view hierarchy so the visibilitychange event is
  // fired.
  UIWindow* window = GetAnyKeyWindow();
  [window addSubview:web_state_->GetView()];

  // Load the HTML content.
  CRWWebController* web_controller = web_state_->GetWebController();
  NSString* html_content = @"<html><head><script>"
                            "document.addEventListener('visibilitychange', "
                            "function() {document.body.innerHTML = "
                            "document.visibilityState;});</script>"
                            "</head><body>Hello world</body></html>";
  [web_controller loadHTML:html_content forURL:GURL("http://example.org")];

  ASSERT_TRUE(
      test::WaitForWebViewContainingText(web_state_.get(), "Hello world"));

  // Check that covering the WebState is notifying the observers that it is
  // hidden and that the visibilitychange event is fired
  ASSERT_EQ(nullptr, observer->was_hidden_info());

  web_state_->DidCoverWebContent();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state_.get(), "hidden"));
  ASSERT_NE(nullptr, observer->was_hidden_info());
  EXPECT_EQ(web_state_.get(), observer->was_hidden_info()->web_state);

  // Check that revealing the WebState is notifying the observers that it is
  // shown and that the visibilitychange event is fired
  ASSERT_EQ(nullptr, observer->was_shown_info());

  web_state_->DidRevealWebContent();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state_.get(), "visible"));
  ASSERT_NE(nullptr, observer->was_shown_info());
  EXPECT_EQ(web_state_.get(), observer->was_shown_info()->web_state);

  // Cleanup.
  [web_state_->GetView() removeFromSuperview];
}

// Test that changing visibility update the WebState last active time.
TEST_F(WebStateImplTest, LastActiveTimeUpdatedWhenBecomeVisible) {
  base::Time last_active_time = web_state_->GetLastActiveTime();
  base::Time creation_time = web_state_->GetCreationTime();

  // Spin the RunLoop a bit to ensure that the active time changes.
  {
    base::RunLoop run_loop;
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(1));
    run_loop.Run();
  }

  // Check that the last active time has not changed.
  EXPECT_EQ(web_state_->GetLastActiveTime(), last_active_time);

  // Mark the WebState has visible. The last active time should be updated.
  web_state_->WasShown();
  EXPECT_GT(web_state_->GetLastActiveTime(), last_active_time);
  EXPECT_EQ(web_state_->GetCreationTime(), creation_time);
}

// Tests that WebState sessionState data doesn't load things with unsafe
// restore.
TEST_F(WebStateImplTest, MixedSafeUnsafeRestore) {
  if (@available(iOS 15, *)) {
  } else {
    return;
  }

  // There's never any unsafe restore with kSynthesizedRestoreSession and iOS15,
  // so disable it first, and test again enabled after.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({},
                                       {features::kSynthesizedRestoreSession});

  GURL urls[3] = {GURL("https://chromium.test/1"),
                  GURL("https://chromium.test/2"),
                  GURL("https://chromium.test/3")};
  std::vector<std::unique_ptr<NavigationItem>> items;
  for (size_t index = 0; index < std::size(urls); ++index) {
    items.push_back(NavigationItem::Create());
    items.back()->SetURL(urls[index]);
  }
  // Force generation of child views; necessary for some tests.
  web_state_->GetView();
  web_state_->SetKeepRenderProcessAlive(true);
  web_state_->GetNavigationManager()->Restore(0, std::move(items));
  __block bool restore_done = false;
  web_state_->GetNavigationManager()->AddRestoreCompletionCallback(
      base::BindOnce(^{
        restore_done = true;
      }));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return restore_done;
  }));
  EXPECT_EQ(nullptr, web_state_->SessionStateData());

  // Enable kSynthesizedRestoreSession to test the opposite.
  scoped_feature_list.Reset();
  scoped_feature_list.InitWithFeatures({features::kSynthesizedRestoreSession},
                                       {});
  for (size_t index = 0; index < std::size(urls); ++index) {
    items.push_back(NavigationItem::Create());
    items.back()->SetURL(urls[index]);
  }
  web::WebState::CreateParams params(GetBrowserState());
  __block auto web_state = std::make_unique<web::WebStateImpl>(params);
  web_state->GetView();
  web_state->SetKeepRenderProcessAlive(true);
  web_state->GetNavigationManager()->Restore(0, std::move(items));
  EXPECT_NE(nullptr, web_state->SessionStateData());
}

// Tests that WebState sessionState data can be read and writen.
TEST_F(WebStateImplTest, ReadAndWriteSessionStateData) {
  if (@available(iOS 15, *)) {
  } else {
    return;
  }
  web::WebState::CreateParams params(GetBrowserState());
  __block auto web_state = std::make_unique<web::WebStateImpl>(params);
  CRWWebController* web_controller = web_state->GetWebController();
  NSString* html_content = @"<html><body>Hello world</body></html>";
  [web_controller loadHTML:html_content forURL:GURL("http://example.org")];

  NSData* data = web_state->SessionStateData();
  EXPECT_NE(nullptr, data);

  web_state_->SetSessionStateData(data);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state_->GetVisibleURL() == web_state->GetVisibleURL();
  }));
}
}  // namespace web
