// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_state_impl.h"

#import <stddef.h>

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/base64.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/gmock_callback_support.h"
#import "base/test/ios/wait_util.h"
#import "components/sessions/core/session_id.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/js_messaging/web_frames_manager_impl.h"
#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/serializable_user_data_manager_impl.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/deprecated/global_web_state_observer.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_util.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/proto/proto_util.h"
#import "ios/web/public/session/proto/storage.pb.h"
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

}  // namespace

// Test fixture for web::WebStateImpl class.
class WebStateImplTest : public web::WebTest {
 public:
  void SetUp() override {
    WebTest::SetUp();

    IgnoreOverRealizationCheck();
  }
};

// Tests WebState::GetWeakPtr.
TEST_F(WebStateImplTest, GetWeakPtr) {
  // Create a WebState as a unique pointer to allow destruction.
  std::unique_ptr<WebStateImpl> web_state =
      std::make_unique<WebStateImpl>(WebState::CreateParams(GetBrowserState()));

  // Verify that `weak_ptr` points to `web_state`.
  base::WeakPtr<WebState> weak_ptr = web_state->GetWeakPtr();
  EXPECT_EQ(weak_ptr.get(), web_state.get());

  // Verify that `weak_ptr` is null after `web_state` destruction.
  web_state.reset();
  EXPECT_EQ(weak_ptr.get(), nullptr);
}

TEST_F(WebStateImplTest, WebUsageEnabled) {
  // Default is false.
  WebStateImpl web_state =
      WebStateImpl(WebState::CreateParams(GetBrowserState()));
  ASSERT_TRUE(web_state.IsWebUsageEnabled());

  web_state.SetWebUsageEnabled(false);
  EXPECT_FALSE(web_state.IsWebUsageEnabled());
  EXPECT_FALSE(web_state.GetWebController().webUsageEnabled);

  web_state.SetWebUsageEnabled(true);
  EXPECT_TRUE(web_state.IsWebUsageEnabled());
  EXPECT_TRUE(web_state.GetWebController().webUsageEnabled);
}

// Tests forwarding to WebStateObserver callbacks.
TEST_F(WebStateImplTest, ObserverTest) {
  // Create a WebState as a unique pointer to allow destruction.
  std::unique_ptr<WebStateImpl> web_state =
      std::make_unique<WebStateImpl>(WebState::CreateParams(GetBrowserState()));

  auto observer = std::make_unique<FakeWebStateObserver>(web_state.get());
  EXPECT_EQ(web_state.get(), observer->web_state());

  // Test that WasShown() is called.
  ASSERT_FALSE(web_state->IsVisible());
  ASSERT_FALSE(observer->was_shown_info());
  web_state->WasShown();
  ASSERT_TRUE(observer->was_shown_info());
  EXPECT_EQ(web_state.get(), observer->was_shown_info()->web_state);
  EXPECT_TRUE(web_state->IsVisible());

  // Test that WasShown() callback is not called for the second time.
  observer = std::make_unique<FakeWebStateObserver>(web_state.get());
  web_state->WasShown();
  EXPECT_FALSE(observer->was_shown_info());

  // Test that WasHidden() is called.
  ASSERT_TRUE(web_state->IsVisible());
  ASSERT_FALSE(observer->was_hidden_info());
  web_state->WasHidden();
  ASSERT_TRUE(observer->was_hidden_info());
  EXPECT_EQ(web_state.get(), observer->was_hidden_info()->web_state);
  EXPECT_FALSE(web_state->IsVisible());

  // Test that WasHidden() callback is not called for the second time.
  observer = std::make_unique<FakeWebStateObserver>(web_state.get());
  web_state->WasHidden();
  EXPECT_FALSE(observer->was_hidden_info());

  // Test that LoadProgressChanged() is called.
  ASSERT_FALSE(observer->change_loading_progress_info());
  const double kTestLoadProgress = 0.75;
  web_state->SendChangeLoadProgress(kTestLoadProgress);
  ASSERT_TRUE(observer->change_loading_progress_info());
  EXPECT_EQ(web_state.get(),
            observer->change_loading_progress_info()->web_state);
  EXPECT_EQ(kTestLoadProgress,
            observer->change_loading_progress_info()->progress);

  // Test that TitleWasSet() is called.
  ASSERT_FALSE(observer->title_was_set_info());
  web_state->OnTitleChanged();
  ASSERT_TRUE(observer->title_was_set_info());
  EXPECT_EQ(web_state.get(), observer->title_was_set_info()->web_state);

  // Test that DidChangeVisibleSecurityState() is called.
  ASSERT_FALSE(observer->did_change_visible_security_state_info());
  web_state->DidChangeVisibleSecurityState();
  ASSERT_TRUE(observer->did_change_visible_security_state_info());
  EXPECT_EQ(web_state.get(),
            observer->did_change_visible_security_state_info()->web_state);

  // Test that FaviconUrlUpdated() is called.
  ASSERT_FALSE(observer->update_favicon_url_candidates_info());
  web::FaviconURL favicon_url(GURL("https://chromium.test/"),
                              web::FaviconURL::IconType::kTouchIcon,
                              {gfx::Size(5, 6)});
  web_state->OnFaviconUrlUpdated({favicon_url});
  ASSERT_TRUE(observer->update_favicon_url_candidates_info());
  EXPECT_EQ(web_state.get(),
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

  // Test that RenderProcessGone() is called.
  SetIgnoreRenderProcessCrashesDuringTesting(true);
  ASSERT_FALSE(observer->render_process_gone_info());
  web_state->OnRenderProcessGone();
  ASSERT_TRUE(observer->render_process_gone_info());
  EXPECT_EQ(web_state.get(), observer->render_process_gone_info()->web_state);

  // Test that DidFinishNavigation() is called.
  ASSERT_FALSE(observer->did_finish_navigation_info());
  const GURL url("http://test");
  std::unique_ptr<NavigationContextImpl> context =
      NavigationContextImpl::CreateNavigationContext(
          web_state.get(), url, /*has_user_gesture=*/true,
          ui::PageTransition::PAGE_TRANSITION_AUTO_BOOKMARK,
          /*is_renderer_initiated=*/true);
  web_state->OnNavigationFinished(context.get());
  ASSERT_TRUE(observer->did_finish_navigation_info());
  EXPECT_EQ(web_state.get(), observer->did_finish_navigation_info()->web_state);
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
  web_state->OnNavigationStarted(context.get());
  ASSERT_TRUE(observer->did_start_navigation_info());
  EXPECT_EQ(web_state.get(), observer->did_start_navigation_info()->web_state);
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
  web_state->OnPageLoaded(url, false);
  ASSERT_TRUE(observer->load_page_info());
  EXPECT_EQ(web_state.get(), observer->load_page_info()->web_state);
  EXPECT_FALSE(observer->load_page_info()->success);
  web_state->OnPageLoaded(url, true);
  ASSERT_TRUE(observer->load_page_info());
  EXPECT_EQ(web_state.get(), observer->load_page_info()->web_state);
  EXPECT_TRUE(observer->load_page_info()->success);

  // Test that OnTitleChanged() is called.
  observer = std::make_unique<FakeWebStateObserver>(web_state.get());
  ASSERT_FALSE(observer->title_was_set_info());
  web_state->OnTitleChanged();
  ASSERT_TRUE(observer->title_was_set_info());
  EXPECT_EQ(web_state.get(), observer->title_was_set_info()->web_state);

  // Test that UnderPageBackgroundColorChanged() is called.
  ASSERT_FALSE(observer->under_page_background_color_changed_info());
  web_state->OnUnderPageBackgroundColorChanged();
  ASSERT_TRUE(observer->under_page_background_color_changed_info());
  EXPECT_EQ(web_state.get(),
            observer->under_page_background_color_changed_info()->web_state);

  // Test that WebStateDestroyed() is called.
  EXPECT_FALSE(observer->web_state_destroyed_info());
  web_state.reset();
  EXPECT_TRUE(observer->web_state_destroyed_info());

  EXPECT_EQ(nullptr, observer->web_state());
}

// Tests that WebStateDelegate methods appropriately called.
TEST_F(WebStateImplTest, DelegateTest) {
  WebStateImpl web_state =
      WebStateImpl(WebState::CreateParams(GetBrowserState()));

  FakeWebStateDelegate delegate;
  web_state.SetDelegate(&delegate);

  // Test that CreateNewWebState() is called.
  GURL child_url("https://child.test/");
  GURL opener_url("https://opener.test/");
  EXPECT_FALSE(delegate.last_create_new_web_state_request());
  web_state.CreateNewWebState(child_url, opener_url, true);
  FakeCreateNewWebStateRequest* create_new_web_state_request =
      delegate.last_create_new_web_state_request();
  ASSERT_TRUE(create_new_web_state_request);
  EXPECT_EQ(&web_state, create_new_web_state_request->web_state);
  EXPECT_EQ(child_url, create_new_web_state_request->url);
  EXPECT_EQ(opener_url, create_new_web_state_request->opener_url);
  EXPECT_TRUE(create_new_web_state_request->initiated_by_user);

  // Test that CloseWebState() is called.
  EXPECT_FALSE(delegate.last_close_web_state_request());
  web_state.CloseWebState();
  ASSERT_TRUE(delegate.last_close_web_state_request());
  EXPECT_EQ(&web_state, delegate.last_close_web_state_request()->web_state);

  // Test that OpenURLFromWebState() is called without a virtual URL.
  WebState::OpenURLParams params(GURL("https://chromium.test/"), Referrer(),
                                 WindowOpenDisposition::CURRENT_TAB,
                                 ui::PAGE_TRANSITION_LINK, true);
  EXPECT_FALSE(delegate.last_open_url_request());
  web_state.OpenURL(params);
  FakeOpenURLRequest* open_url_request = delegate.last_open_url_request();
  ASSERT_TRUE(open_url_request);
  EXPECT_EQ(&web_state, open_url_request->web_state);
  WebState::OpenURLParams actual_params = open_url_request->params;
  EXPECT_EQ(params.url, actual_params.url);
  EXPECT_EQ(GURL(), params.virtual_url);
  EXPECT_EQ(GURL(), actual_params.virtual_url);
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
  web_state.OpenURL(params);
  open_url_request = delegate.last_open_url_request();
  ASSERT_TRUE(open_url_request);
  EXPECT_EQ(&web_state, open_url_request->web_state);
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
  web_state.ShowRepostFormWarningDialog(web::FormWarningType::kRepost,
                                        std::move(repost_callback));
  ASSERT_TRUE(delegate.last_repost_form_request());
  EXPECT_EQ(delegate.last_repost_form_request()->web_state, &web_state);

  // TODO(crbug.com/40941405): Check web::FormWarningType::kInsecureForm as
  // well.

  // Test that GetJavaScriptDialogPresenter() is called.
  FakeJavaScriptDialogPresenter* presenter =
      delegate.GetFakeJavaScriptDialogPresenter();
  EXPECT_FALSE(delegate.get_java_script_dialog_presenter_called());
  EXPECT_TRUE(presenter->requested_alert_dialogs().empty());
  EXPECT_FALSE(presenter->cancel_dialogs_called());

  __block bool callback_called = false;
  web_state.RunJavaScriptAlertDialog(GURL(), @"", base::BindOnce(^() {
                                       callback_called = true;
                                     }));

  EXPECT_TRUE(delegate.get_java_script_dialog_presenter_called());
  EXPECT_EQ(1U, presenter->requested_alert_dialogs().size());
  EXPECT_TRUE(callback_called);

  EXPECT_FALSE(presenter->cancel_dialogs_called());
  web_state.CancelDialogs();
  EXPECT_TRUE(presenter->cancel_dialogs_called());

  // Test that OnAuthRequired() is called.
  EXPECT_FALSE(delegate.last_authentication_request());
  NSURLProtectionSpace* protection_space = [[NSURLProtectionSpace alloc] init];
  NSURLCredential* credential = [[NSURLCredential alloc] init];
  WebStateDelegate::AuthCallback callback;
  web_state.OnAuthRequired(protection_space, credential, std::move(callback));
  ASSERT_TRUE(delegate.last_authentication_request());
  EXPECT_EQ(delegate.last_authentication_request()->web_state, &web_state);
  EXPECT_EQ(delegate.last_authentication_request()->protection_space,
            protection_space);
  EXPECT_EQ(delegate.last_authentication_request()->credential, credential);
}

// Verifies that GlobalWebStateObservers are called when expected.
TEST_F(WebStateImplTest, GlobalObserverTest) {
  // Create a WebState as a unique pointer to allow destruction.
  std::unique_ptr<WebStateImpl> web_state =
      std::make_unique<WebStateImpl>(WebState::CreateParams(GetBrowserState()));

  std::unique_ptr<TestGlobalWebStateObserver> observer =
      std::make_unique<TestGlobalWebStateObserver>();

  // Test that DidStartNavigation() is called.
  EXPECT_FALSE(observer->did_start_navigation_called());
  std::unique_ptr<NavigationContextImpl> context =
      NavigationContextImpl::CreateNavigationContext(
          web_state.get(), GURL(), /*has_user_gesture=*/true,
          ui::PageTransition::PAGE_TRANSITION_AUTO_BOOKMARK,
          /*is_renderer_initiated=*/true);
  web_state->OnNavigationStarted(context.get());
  EXPECT_TRUE(observer->did_start_navigation_called());

  // Test that WebStateDidStartLoading() is called.
  EXPECT_FALSE(observer->did_start_loading_called());
  web_state->SetIsLoading(true);
  EXPECT_TRUE(observer->did_start_loading_called());

  // Test that WebStateDidStopLoading() is called.
  EXPECT_FALSE(observer->did_stop_loading_called());
  web_state->SetIsLoading(false);
  EXPECT_TRUE(observer->did_stop_loading_called());

  // Test that WebStateDestroyed() is called.
  EXPECT_FALSE(observer->web_state_destroyed_called());
  web_state.reset();
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
  // Create a WebState as a unique pointer to allow destruction.
  std::unique_ptr<WebStateImpl> web_state =
      std::make_unique<WebStateImpl>(WebState::CreateParams(GetBrowserState()));

  MockWebStatePolicyDecider decider(web_state.get());
  MockWebStatePolicyDecider decider2(web_state.get());
  EXPECT_EQ(web_state.get(), decider.web_state());

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
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/false, /*user_tapped_recently=*/false);
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
  web_state->ShouldAllowRequest(request, request_info_main_frame, callback);
  EXPECT_TRUE(policy_decision.ShouldAllowNavigation());
  EXPECT_FALSE(policy_decision.ShouldCancelNavigation());

  const WebStatePolicyDecider::RequestInfo request_info_iframe(
      ui::PageTransition::PAGE_TRANSITION_LINK,
      /*target_main_frame=*/false,
      /*target_frame_is_cross_origin=*/false,
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/false, /*user_tapped_recently=*/false);
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

  web_state->ShouldAllowRequest(request, request_info_iframe, callback);
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

    web_state->ShouldAllowRequest(request, request_info_main_frame, callback);
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

  web_state->ShouldAllowResponse(response, response_info_main_frame, callback);
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

    web_state->ShouldAllowResponse(response, response_info_iframe,
                                   std::move(callback));
    EXPECT_FALSE(policy_decision.ShouldAllowNavigation());
    EXPECT_TRUE(policy_decision.ShouldCancelNavigation());
  }

  EXPECT_CALL(decider, WebStateDestroyed()).Times(1);
  EXPECT_CALL(decider2, WebStateDestroyed()).Times(1);
  web_state.reset();
  EXPECT_EQ(nullptr, decider.web_state());
}

// Verifies that asynchronous decisions for
// WebStatePolicyDecider::ShouldAllowResponse are correctly handled by
// WebStateImpl::ShouldAllowResponse.
TEST_F(WebStateImplTest, AsyncShouldAllowResponseTest) {
  WebStateImpl web_state =
      WebStateImpl(WebState::CreateParams(GetBrowserState()));

  MockWebStatePolicyDecider sync_decider(&web_state);
  AsyncWebStatePolicyDecider async_decider1(&web_state);
  AsyncWebStatePolicyDecider async_decider2(&web_state);

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
  web_state.ShouldAllowResponse(response, expected_response_info, callback);
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
  web_state.ShouldAllowResponse(response, expected_response_info, callback);
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
  web_state.ShouldAllowResponse(response, expected_response_info, callback);
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

// Tests that WebState::CreateParams::created_with_opener is translated to
// WebState::HasOpener() return values.
TEST_F(WebStateImplTest, CreatedWithOpener) {
  WebStateImpl web_state =
      WebStateImpl(WebState::CreateParams(GetBrowserState()));

  // Verify that the HasOpener() returns false if not specified in the create
  // params.
  EXPECT_FALSE(web_state.HasOpener());
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
  WebStateImpl web_state =
      WebStateImpl(WebState::CreateParams(GetBrowserState()));
  auto observer = std::make_unique<FakeWebStateObserver>(&web_state);

  // No callback if icons has not been fetched yet.
  std::unique_ptr<NavigationContextImpl> context =
      NavigationContextImpl::CreateNavigationContext(
          &web_state, GURL(),
          /*has_user_gesture=*/false, ui::PageTransition::PAGE_TRANSITION_LINK,
          /*is_renderer_initiated=*/false);
  context->SetIsSameDocument(true);
  web_state.OnNavigationFinished(context.get());
  EXPECT_FALSE(observer->update_favicon_url_candidates_info());

  // Callback is called when icons were fetched.
  observer = std::make_unique<FakeWebStateObserver>(&web_state);
  web::FaviconURL favicon_url(GURL("https://chromium.test/"),
                              web::FaviconURL::IconType::kTouchIcon,
                              {gfx::Size(5, 6)});
  web_state.OnFaviconUrlUpdated({favicon_url});
  EXPECT_TRUE(observer->update_favicon_url_candidates_info());

  // Callback is now called after same-document navigation.
  observer = std::make_unique<FakeWebStateObserver>(&web_state);
  web_state.OnNavigationFinished(context.get());
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
  observer = std::make_unique<FakeWebStateObserver>(&web_state);
  context->SetIsSameDocument(false);
  web_state.OnNavigationFinished(context.get());
  EXPECT_FALSE(observer->update_favicon_url_candidates_info());

  // Previous candidates were invalidated by the document change. No callback
  // if icons has not been fetched yet.
  context->SetIsSameDocument(true);
  web_state.OnNavigationFinished(context.get());
  EXPECT_FALSE(observer->update_favicon_url_candidates_info());
}

// Tests that BuildSessionStorage() and GetTitle() return information about the
// most recently restored session if no navigation item has been committed. Also
// tests that re-restoring that session includes updated userData.
TEST_F(WebStateImplTest, UncommittedRestoreSession) {
  GURL url("http://test.com");
  CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
  session_storage.stableIdentifier = [[NSUUID UUID] UUIDString];
  session_storage.uniqueIdentifier = web::WebStateID::NewUnique();
  session_storage.lastCommittedItemIndex = 0;
  CRWNavigationItemStorage* item_storage =
      [[CRWNavigationItemStorage alloc] init];
  item_storage.title = base::SysNSStringToUTF16(@"Title");
  item_storage.virtualURL = url;
  session_storage.itemStorages = @[ item_storage ];

  WebStateImpl web_state =
      WebStateImpl(WebState::CreateParams(GetBrowserState()), session_storage,
                   base::ReturnValueOnce<NSData*>(nil));

  // After restoring `web_state` change the uncommitted state's user data.
  web::SerializableUserDataManager* user_data_manager =
      web::SerializableUserDataManager::FromWebState(&web_state);
  user_data_manager->AddSerializableData(@(1), @"user_data_key");

  CRWSessionStorage* extracted_session_storage =
      web_state.BuildSessionStorage();
  EXPECT_EQ(0, extracted_session_storage.lastCommittedItemIndex);
  EXPECT_EQ(1U, extracted_session_storage.itemStorages.count);
  ASSERT_FALSE(web_state.IsRealized());
  EXPECT_EQ(u"Title", web_state.GetTitle());
  EXPECT_EQ(url, web_state.GetVisibleURL());

  // Check that even if the WebState becomes realized, then GetTitle() and
  // GetVisibleURL() are correct during the navigation history restoration.
  web_state.SetWebUsageEnabled(false);
  web_state.ForceRealized();

  ASSERT_TRUE(web_state.IsRealized());
  EXPECT_EQ(u"Title", web_state.GetTitle());
  EXPECT_EQ(url, web_state.GetVisibleURL());

  WebStateImpl restored_web_state(WebState::CreateParams(GetBrowserState()),
                                  extracted_session_storage,
                                  base::ReturnValueOnce<NSData*>(nil));
  web::SerializableUserDataManager* restored_user_data_manager =
      web::SerializableUserDataManager::FromWebState(&restored_web_state);
  NSNumber* user_data_value = base::apple::ObjCCast<NSNumber>(
      restored_user_data_manager->GetValueForSerializationKey(
          @"user_data_key"));
  EXPECT_EQ(@(1), user_data_value);
}

// Tests that SerializeToProto() and GetTitle() return information about the
// most recently restored session if no navigation item has been committed. Also
// tests that re-restoring that session includes updated userData.
TEST_F(WebStateImplTest, UncommittedRestoreSessionOptimisedStorage) {
  GURL url("http://test.com");
  proto::WebStateStorage storage;
  proto::NavigationStorage* navigation_storage = storage.mutable_navigation();
  navigation_storage->set_last_committed_item_index(0);
  proto::NavigationItemStorage* navigation_item_storage =
      navigation_storage->add_items();
  navigation_item_storage->set_title("Title");
  navigation_item_storage->set_virtual_url(url.spec());

  proto::WebStateMetadataStorage metadata;
  metadata.set_navigation_item_count(navigation_storage->items_size());
  proto::PageMetadataStorage* active_page = metadata.mutable_active_page();
  active_page->set_page_title("Title");
  active_page->set_page_url(url.spec());

  WebStateImpl web_state =
      WebStateImpl(GetBrowserState(), web::WebStateID::NewUnique(), metadata,
                   base::ReturnValueOnce(std::move(storage)),
                   base::ReturnValueOnce<NSData*>(nil));

  // Check that the title and url are correct.
  ASSERT_FALSE(web_state.IsRealized());
  EXPECT_EQ(u"Title", web_state.GetTitle());
  EXPECT_EQ(url, web_state.GetVisibleURL());

  // Check that even if the WebState becomes realized, then GetTitle() and
  // GetVisibleURL() are correct during the navigation history restoration.
  web_state.SetWebUsageEnabled(false);
  web_state.ForceRealized();

  ASSERT_TRUE(web_state.IsRealized());
  EXPECT_EQ(u"Title", web_state.GetTitle());
  EXPECT_EQ(url, web_state.GetVisibleURL());
}

// Test that lastCommittedItemIndex is end-of-list when there's no defined
// index, such as during a restore.
TEST_F(WebStateImplTest, NoUncommittedRestoreSession) {
  WebStateImpl web_state =
      WebStateImpl(WebState::CreateParams(GetBrowserState()));

  CRWSessionStorage* session_storage = web_state.BuildSessionStorage();
  EXPECT_EQ(-1, session_storage.lastCommittedItemIndex);
  EXPECT_NSEQ(@[], session_storage.itemStorages);
  EXPECT_TRUE(web_state.GetTitle().empty());
  EXPECT_EQ(GURL(), web_state.GetVisibleURL());
}

// Test that lastCommittedItemIndex is end-of-list when there's no defined
// index, such as during a restore.
TEST_F(WebStateImplTest, NoUncommittedRestoreSessionOptimisedStorage) {
  WebStateImpl web_state =
      WebStateImpl(WebState::CreateParams(GetBrowserState()));

  proto::WebStateStorage storage;
  web_state.SerializeToProto(storage);
  EXPECT_EQ(-1, storage.navigation().last_committed_item_index());
  EXPECT_EQ(0, storage.navigation().items_size());

  EXPECT_TRUE(web_state.GetTitle().empty());
  EXPECT_EQ(GURL(), web_state.GetVisibleURL());
}

// Tests that CanTakeSnapshot() is false when a JavaScript dialog is being
// presented.
TEST_F(WebStateImplTest, DisallowSnapshotsDuringDialogPresentation) {
  WebStateImpl web_state =
      WebStateImpl(WebState::CreateParams(GetBrowserState()));

  FakeWebStateDelegate delegate;
  web_state.SetDelegate(&delegate);

  EXPECT_TRUE(web_state.CanTakeSnapshot());

  // Pause the callback execution to allow testing while the dialog is
  // presented.
  delegate.GetFakeJavaScriptDialogPresenter()->set_callback_execution_paused(
      true);
  web_state.RunJavaScriptAlertDialog(GURL(), @"message", base::DoNothing());

  // Verify that CanTakeSnapshot() returns no while the dialog is presented.
  EXPECT_FALSE(web_state.CanTakeSnapshot());

  // Unpause the presenter and verify that snapshots are enabled again.
  delegate.GetFakeJavaScriptDialogPresenter()->set_callback_execution_paused(
      false);
  EXPECT_TRUE(web_state.CanTakeSnapshot());
}

// Tests that IsJavaScriptDialogRunning() is true when a JavaScript dialog is
// being presented.
TEST_F(WebStateImplTest, VerifyDialogRunningBoolean) {
  WebStateImpl web_state =
      WebStateImpl(WebState::CreateParams(GetBrowserState()));

  FakeWebStateDelegate delegate;
  web_state.SetDelegate(&delegate);

  EXPECT_FALSE(web_state.IsJavaScriptDialogRunning());

  // Pause the callback execution to allow testing while the dialog is
  // presented.
  delegate.GetFakeJavaScriptDialogPresenter()->set_callback_execution_paused(
      true);
  web_state.RunJavaScriptAlertDialog(GURL(), @"message", base::DoNothing());

  // Verify that IsJavaScriptDialogRunning() returns true while the dialog is
  // presented.
  EXPECT_TRUE(web_state.IsJavaScriptDialogRunning());

  // Unpause the presenter and verify that IsJavaScriptDialogRunning() returns
  // false when the dialog is no longer presented
  delegate.GetFakeJavaScriptDialogPresenter()->set_callback_execution_paused(
      false);
  EXPECT_FALSE(web_state.IsJavaScriptDialogRunning());
}

// Tests that CreateFullPagePdf invokes completion callback nil when a
// javascript dialog is running
TEST_F(WebStateImplTest, CreateFullPagePdfJavaScriptDialog) {
  WebStateImpl web_state =
      WebStateImpl(WebState::CreateParams(GetBrowserState()));

  FakeWebStateDelegate delegate;
  web_state.SetDelegate(&delegate);

  // Load the HTML content.
  CRWWebController* web_controller = web_state.GetWebController();
  NSString* html_content =
      @"<html><body><div style='background-color:#FF0000; width:50%; "
       "height:100%;'></div>Hello world</body></html>";
  [web_controller loadHTML:html_content forURL:GURL("http://example.org")];

  ASSERT_TRUE(test::WaitForWebViewContainingText(&web_state, "Hello world"));

  // Pause the callback execution to allow testing while the dialog is
  // presented.
  delegate.GetFakeJavaScriptDialogPresenter()->set_callback_execution_paused(
      true);
  web_state.RunJavaScriptAlertDialog(GURL(), @"message", base::DoNothing());

  // Attempt to create a PDF for this page and validate that it return nil.
  __block NSData* callback_data_when_dialog = nil;
  __block BOOL callback_called_when_dialog = NO;
  web_state.CreateFullPagePdf(base::BindOnce(^(NSData* pdf_document_data) {
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
  web_state.CreateFullPagePdf(base::BindOnce(^(NSData* pdf_document_data) {
    callback_data_no_dialog = [pdf_document_data copy];
    callback_called_no_dialog = YES;
  }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return callback_called_no_dialog;
  }));

  EXPECT_TRUE(callback_data_no_dialog);
}

// Tests that the WebView is removed from the view hierarchy and the
// visibilitychange JavaScript event is fired when covering/revealing the
// WebContent.
TEST_F(WebStateImplTest, VisibilitychangeEventFired) {
  WebStateImpl web_state =
      WebStateImpl(WebState::CreateParams(GetBrowserState()));

  // Mark the WebState as visibile before adding the observer.
  web_state.WasShown();

  auto observer = std::make_unique<FakeWebStateObserver>(&web_state);

  // Add the WebState to the view hierarchy so the visibilitychange event is
  // fired.
  UIWindow* window = GetAnyKeyWindow();
  [window addSubview:web_state.GetView()];

  // Load the HTML content.
  CRWWebController* web_controller = web_state.GetWebController();
  NSString* html_content = @"<html><head><script>"
                            "document.addEventListener('visibilitychange', "
                            "function() {document.body.innerHTML = "
                            "document.visibilityState;});</script>"
                            "</head><body>Hello world</body></html>";
  [web_controller loadHTML:html_content forURL:GURL("http://example.org")];

  ASSERT_TRUE(test::WaitForWebViewContainingText(&web_state, "Hello world"));

  // Check that covering the WebState is notifying the observers that it is
  // hidden and that the visibilitychange event is fired
  ASSERT_EQ(nullptr, observer->was_hidden_info());

  web_state.DidCoverWebContent();
  ASSERT_TRUE(test::WaitForWebViewContainingText(&web_state, "hidden"));
  ASSERT_NE(nullptr, observer->was_hidden_info());
  EXPECT_EQ(&web_state, observer->was_hidden_info()->web_state);

  // Check that revealing the WebState is notifying the observers that it is
  // shown and that the visibilitychange event is fired
  ASSERT_EQ(nullptr, observer->was_shown_info());

  web_state.DidRevealWebContent();
  ASSERT_TRUE(test::WaitForWebViewContainingText(&web_state, "visible"));
  ASSERT_NE(nullptr, observer->was_shown_info());
  EXPECT_EQ(&web_state, observer->was_shown_info()->web_state);

  // Cleanup.
  [web_state.GetView() removeFromSuperview];
}

// Test that changing visibility update the WebState last active time.
TEST_F(WebStateImplTest, LastActiveTimeUpdatedWhenBecomeVisible) {
  WebStateImpl web_state =
      WebStateImpl(WebState::CreateParams(GetBrowserState()));

  base::Time last_active_time = web_state.GetLastActiveTime();
  base::Time creation_time = web_state.GetCreationTime();

  // Spin the RunLoop a bit to ensure that the active time changes.
  {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(1));
    run_loop.Run();
  }

  // Check that the last active time has not changed.
  EXPECT_EQ(web_state.GetLastActiveTime(), last_active_time);

  // Mark the WebState has visible. The last active time should be updated.
  web_state.WasShown();
  EXPECT_GT(web_state.GetLastActiveTime(), last_active_time);
  EXPECT_EQ(web_state.GetCreationTime(), creation_time);
}

// Tests that at creation the last active time is initialized to the creation
// time if unspecified in CreateParams.
TEST_F(WebStateImplTest, LastActiveTimeSetOnCreation) {
  WebStateImpl web_state =
      WebStateImpl(WebState::CreateParams(GetBrowserState()));

  EXPECT_NE(web_state.GetLastActiveTime(), base::Time());
  EXPECT_EQ(web_state.GetLastActiveTime(), web_state.GetCreationTime());
}

// Tests that at creation the last active time is initialized to the time
// specified in CreateParams.
TEST_F(WebStateImplTest, LastActiveTimeSetOnCreationToCreateParamsValue) {
  const base::Time last_active_time = base::Time::Now() + base::Days(1);
  WebState::CreateParams params = WebState::CreateParams(GetBrowserState());
  params.last_active_time = last_active_time;

  WebStateImpl web_state = WebStateImpl(params);

  EXPECT_NE(web_state.GetLastActiveTime(), base::Time());
  EXPECT_NE(web_state.GetLastActiveTime(), web_state.GetCreationTime());
  EXPECT_EQ(web_state.GetLastActiveTime(), last_active_time);
}

// Tests that at creation the last active time is initialized to the time
// specified in CreateParams, even if set to the epoch.
TEST_F(WebStateImplTest, LastActiveTimeCanBeForcedToEpochViaCreateParams) {
  WebState::CreateParams params = WebState::CreateParams(GetBrowserState());
  params.last_active_time = base::Time();

  WebStateImpl web_state = WebStateImpl(params);

  EXPECT_EQ(web_state.GetLastActiveTime(), base::Time());
  EXPECT_NE(web_state.GetLastActiveTime(), web_state.GetCreationTime());
}

// Tests that WebState sessionState data can be read and writen.
TEST_F(WebStateImplTest, ReadAndWriteSessionStateData) {
  // Create a WebState, navigate and capture the session state data.
  WebStateImpl web_state =
      WebStateImpl(web::WebState::CreateParams(GetBrowserState()));

  CRWWebController* web_controller = web_state.GetWebController();
  NSString* html_content = @"<html><body>Hello world</body></html>";
  [web_controller loadHTML:html_content forURL:GURL("http://example.org")];

  NSData* data = web_state.SessionStateData();
  EXPECT_NE(nullptr, data);

  // Create another WebState, set the session state and check the two WebState
  // eventually display the same URL.
  WebStateImpl other_web_state =
      WebStateImpl(web::WebState::CreateParams(GetBrowserState()));
  other_web_state.SetSessionStateData(data);

  // Use pointers as the block cannot reference WebState via object.
  WebStateImpl* web_state_ptr = &web_state;
  WebStateImpl* other_web_state_ptr = &other_web_state;

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state_ptr->GetVisibleURL() ==
           other_web_state_ptr->GetVisibleURL();
  }));
}

// Tests that SerializeMetadataToProto() can be called on an unrealized
// or realized WebState.
TEST_F(WebStateImplTest, SerializeMetadataToProto) {
  const std::u16string title = u"Title";
  const base::Time creation_time = base::Time::Now();
  const GURL visible_url = GURL("testwebui://test/");

  proto::WebStateStorage storage = CreateWebStateStorage(
      NavigationManager::WebLoadParams(visible_url), title,
      /*created_with_opener=*/false, UserAgentType::MOBILE, creation_time);
  ASSERT_TRUE(storage.has_metadata());

  proto::WebStateMetadataStorage original_metadata;
  original_metadata.Swap(storage.mutable_metadata());

  // Create an unrealized WebState.
  web::WebStateImpl web_state =
      WebStateImpl(GetBrowserState(), WebStateID::NewUnique(),
                   original_metadata, base::ReturnValueOnce(std::move(storage)),
                   base::ReturnValueOnce<NSData*>(nil));

  // Check that the metadata can be fetched from the unrealized WebState.
  {
    proto::WebStateMetadataStorage metadata;
    web_state.SerializeMetadataToProto(metadata);

    EXPECT_EQ(metadata.navigation_item_count(), 1);
    EXPECT_EQ(TimeFromProto(metadata.creation_time()), creation_time);
    EXPECT_EQ(TimeFromProto(metadata.last_active_time()), creation_time);
    EXPECT_EQ(metadata.active_page().page_title(), base::UTF16ToUTF8(title));
    EXPECT_EQ(metadata.active_page().page_url(), visible_url.spec());
  }

  // Force realization of the WebState.
  web_state.ForceRealized();
  ASSERT_TRUE(web_state.IsRealized());

  // Calling WasShown() will change the last active time for the WebState.
  web_state.WasShown();
  ASSERT_NE(web_state.GetLastActiveTime(), creation_time);

  // Check that the metadata can be fetched from the WebState after realization.
  {
    proto::WebStateMetadataStorage metadata;
    web_state.SerializeMetadataToProto(metadata);

    EXPECT_EQ(metadata.navigation_item_count(), 1);
    EXPECT_EQ(TimeFromProto(metadata.creation_time()), creation_time);
    EXPECT_NE(TimeFromProto(metadata.last_active_time()), creation_time);
    EXPECT_EQ(metadata.active_page().page_title(), base::UTF16ToUTF8(title));
    EXPECT_EQ(metadata.active_page().page_url(), visible_url.spec());
  }
}

}  // namespace web
