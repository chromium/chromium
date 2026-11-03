// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/itunes_urls/model/itunes_urls_handler_tab_helper.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/test/metrics/histogram_tester.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_modality.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/app_launcher_overlay.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/web_content_commands.h"
#import "ios/chrome/test/fakes/fake_web_content_handler.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class ITunesUrlsHandlerTabHelperTest : public PlatformTest {
 protected:
  ITunesUrlsHandlerTabHelperTest()
      : application_(OCMClassMock([UIApplication class])),
        fake_handler_([[FakeWebContentHandler alloc] init]),
        profile_(TestProfileIOS::Builder().Build()) {
    OCMStub([application_ sharedApplication]).andReturn(application_);
    web_state_.SetBrowserState(profile_->GetOriginalProfile());
    web_state_.WasShown();
    OverlayRequestQueue::CreateForWebState(&web_state_);
    ITunesUrlsHandlerTabHelper::CreateForWebState(&web_state_);
    ITunesUrlsHandlerTabHelper::FromWebState(&web_state_)
        ->SetWebContentsHandler(fake_handler_);
  }

  ~ITunesUrlsHandlerTabHelperTest() override { [application_ stopMocking]; }

  // Creates a `RequestInfo` for a link transition in the main frame or an
  // iframe.
  web::WebStatePolicyDecider::RequestInfo CreateRequestInfo(
      bool target_frame_is_main = true) {
    return web::WebStatePolicyDecider::RequestInfo(
        ui::PageTransition::PAGE_TRANSITION_LINK, target_frame_is_main,
        /*target_frame_is_cross_origin=*/false,
        /*target_window_is_cross_origin=*/false,
        /*has_user_gesture=*/false, /*user_tapped_recently=*/false);
  }

  // Returns the web content area `OverlayRequestQueue` for `web_state_`.
  OverlayRequestQueue* GetOverlayQueue() {
    return OverlayRequestQueue::FromWebState(&web_state_,
                                             OverlayModality::kWebContentArea);
  }

  // Calls ShouldAllowRequest for a request with the given `url_string` and
  // returns true if storekit was launched.
  bool VerifyStoreKitLaunched(NSString* url_string, bool main_frame) {
    fake_handler_.productParams = nil;
    __block bool callback_called = false;
    __block web::WebStatePolicyDecider::PolicyDecision request_policy =
        web::WebStatePolicyDecider::PolicyDecision::Allow();
    auto callback =
        base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
          request_policy = decision;
          callback_called = true;
        });
    web_state_.ShouldAllowRequest(
        [NSURLRequest requestWithURL:[NSURL URLWithString:url_string]],
        CreateRequestInfo(main_frame), std::move(callback));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(callback_called);
    EXPECT_EQ(request_policy.ShouldCancelNavigation(),
              fake_handler_.productParams != nil);
    return request_policy.ShouldCancelNavigation() &&
           fake_handler_.productParams != nil;
  }

  web::WebTaskEnvironment task_environment_;
  id application_;
  FakeWebContentHandler* fake_handler_;
  web::FakeWebState web_state_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that iTunes URLs in off-the-record mode trigger an App Store
// confirmation overlay, upgrade `http://` to `https://`, and open the URL via
// universal links (without StoreKit) if the user accepts the prompt.
TEST_F(ITunesUrlsHandlerTabHelperTest, OffTheRecordPromptAccepted) {
  base::HistogramTester histogram_tester;
  web_state_.SetBrowserState(profile_->GetOffTheRecordProfile());

  NSURL* url =
      [NSURL URLWithString:@"http://itunes.apple.com/us/app/app_name/id123"];
  NSURL* expected_launch_url =
      [NSURL URLWithString:@"https://itunes.apple.com/us/app/app_name/id123"];
  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision> future;
  web_state_.ShouldAllowRequest([NSURLRequest requestWithURL:url],
                                CreateRequestInfo(), future.GetCallback());

  OverlayRequestQueue* queue = GetOverlayQueue();
  ASSERT_TRUE(queue);
  OverlayRequest* request = queue->front_request();
  ASSERT_TRUE(request);
  app_launcher_overlays::AppLaunchConfirmationRequest* config =
      request->GetConfig<app_launcher_overlays::AppLaunchConfirmationRequest>();
  ASSERT_TRUE(config);
  EXPECT_EQ(app_launcher_overlays::AppLaunchConfirmationRequestCause::
                kOpenAppStoreFromIncognito,
            config->cause());
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(nil, fake_handler_.productParams);

  OCMExpect([application_ openURL:expected_launch_url
                          options:@{
                            UIApplicationOpenURLOptionUniversalLinksOnly : @YES
                          }
                completionHandler:[OCMArg checkWithBlock:^(void (
                                      ^completionHandler)(BOOL success)) {
                  completionHandler(YES);
                  return YES;
                }]]);
  request->GetCallbackManager()->SetCompletionResponse(
      OverlayResponse::CreateWithInfo<
          app_launcher_overlays::AllowAppLaunchResponse>());
  queue->CancelAllRequests();

  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().ShouldCancelNavigation());
  EXPECT_EQ(nil, fake_handler_.productParams);
  EXPECT_OCMOCK_VERIFY(application_);
  histogram_tester.ExpectUniqueSample(
      "Tab.ExternalApplicationOpened.AppStoreFromIncognito", true, 1);
}

// Tests that when opening the App Store via universal links fails in
// off-the-record mode, a failure alert is inserted at the front of the queue
// and policy decisions remain delayed until the failure alert is dismissed.
TEST_F(ITunesUrlsHandlerTabHelperTest,
       OffTheRecordAppStoreLaunchFailureShowsAlert) {
  base::HistogramTester histogram_tester;
  web_state_.SetBrowserState(profile_->GetOffTheRecordProfile());

  NSURL* url =
      [NSURL URLWithString:@"https://apps.apple.com/us/app/app_name/id123"];
  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision> future;
  web_state_.ShouldAllowRequest([NSURLRequest requestWithURL:url],
                                CreateRequestInfo(), future.GetCallback());

  OverlayRequestQueue* queue = GetOverlayQueue();
  ASSERT_TRUE(queue);
  OverlayRequest* request = queue->front_request();
  ASSERT_TRUE(request);

  __block void (^open_url_completion)(BOOL) = nil;
  OCMExpect([application_ openURL:url
                          options:@{
                            UIApplicationOpenURLOptionUniversalLinksOnly : @YES
                          }
                completionHandler:[OCMArg checkWithBlock:^(void (
                                      ^completionHandler)(BOOL success)) {
                  open_url_completion = [completionHandler copy];
                  return YES;
                }]]);
  request->GetCallbackManager()->SetCompletionResponse(
      OverlayResponse::CreateWithInfo<
          app_launcher_overlays::AllowAppLaunchResponse>());
  queue->CancelAllRequests();

  EXPECT_OCMOCK_VERIFY(application_);
  ASSERT_TRUE(open_url_completion);
  histogram_tester.ExpectUniqueSample(
      "Tab.ExternalApplicationOpened.AppStoreFromIncognito", true, 1);
  EXPECT_FALSE(future.IsReady());

  // Queue another overlay request before the async `openURL:` completion
  // reports failure.
  queue->AddRequest(OverlayRequest::CreateWithConfig<
                    app_launcher_overlays::AppLaunchConfirmationRequest>(
      app_launcher_overlays::AppLaunchConfirmationRequestCause::kOther));
  ASSERT_EQ(1U, queue->size());

  // Simulate `openURL:` failing asynchronously.
  open_url_completion(NO);

  // Verify that the failure alert was inserted at index 0 ahead of the other
  // queued request, and that the policy decision is still delayed.
  ASSERT_EQ(2U, queue->size());
  OverlayRequest* failure_request = queue->front_request();
  ASSERT_TRUE(failure_request);
  app_launcher_overlays::AppLaunchConfirmationRequest* failure_config =
      failure_request
          ->GetConfig<app_launcher_overlays::AppLaunchConfirmationRequest>();
  ASSERT_TRUE(failure_config);
  EXPECT_EQ(app_launcher_overlays::AppLaunchConfirmationRequestCause::
                kAppLaunchFailed,
            failure_config->cause());
  EXPECT_FALSE(future.IsReady());

  // Dismiss the failure alert by tapping "Got it" and verify that the policy
  // decision completes.
  failure_request->GetCallbackManager()->SetCompletionResponse(
      OverlayResponse::CreateWithInfo<
          app_launcher_overlays::AllowAppLaunchResponse>());
  queue->CancelAllRequests();
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().ShouldCancelNavigation());
  histogram_tester.ExpectUniqueSample("Tab.ExternalApplicationOpened.Failed",
                                      true, 1);
}

// Tests that iTunes URLs do not launch StoreKit or queue an overlay prompt when
// the WebState is not visible.
TEST_F(ITunesUrlsHandlerTabHelperTest,
       DoesNotHandleUrlWhenWebStateIsNotVisible) {
  web_state_.WasHidden();

  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/us/app/app_name/id123", /*main_frame=*/true));

  web_state_.SetBrowserState(profile_->GetOffTheRecordProfile());
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/us/app/app_name/id123", /*main_frame=*/true));
  OverlayRequestQueue* queue = GetOverlayQueue();
  ASSERT_TRUE(queue);
  EXPECT_EQ(0U, queue->size());
}

// Tests that iTunes URLs in off-the-record mode do not open the URL or launch
// StoreKit if the user declines the confirmation overlay.
TEST_F(ITunesUrlsHandlerTabHelperTest, OffTheRecordPromptDeclined) {
  base::HistogramTester histogram_tester;
  web_state_.SetBrowserState(profile_->GetOffTheRecordProfile());

  NSURL* url =
      [NSURL URLWithString:@"http://itunes.apple.com/us/app/app_name/id123"];
  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision> future;
  web_state_.ShouldAllowRequest([NSURLRequest requestWithURL:url],
                                CreateRequestInfo(), future.GetCallback());

  OverlayRequestQueue* queue = GetOverlayQueue();
  ASSERT_TRUE(queue);
  ASSERT_TRUE(queue->front_request());
  EXPECT_FALSE(future.IsReady());

  OCMReject([application_ openURL:[OCMArg any]
                          options:[OCMArg any]
                completionHandler:[OCMArg any]]);
  queue->CancelAllRequests();

  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().ShouldCancelNavigation());
  EXPECT_EQ(nil, fake_handler_.productParams);
  EXPECT_OCMOCK_VERIFY(application_);
  histogram_tester.ExpectUniqueSample(
      "Tab.ExternalApplicationOpened.AppStoreFromIncognito", false, 1);
}

// Tests that subsequent `ShouldAllowRequest()` and `ShouldAllowResponse()`
// policy decisions are delayed while an off-the-record confirmation prompt is
// active, and that any previously pending decision is canceled when a new one
// arrives.
TEST_F(ITunesUrlsHandlerTabHelperTest,
       DelaysPolicyDecisionsWhilePromptIsActive) {
  web_state_.SetBrowserState(profile_->GetOffTheRecordProfile());

  const web::WebStatePolicyDecider::RequestInfo request_info =
      CreateRequestInfo();

  // Trigger the confirmation prompt with an iTunes URL.
  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
      itunes_request_future;
  web_state_.ShouldAllowRequest(
      [NSURLRequest
          requestWithURL:[NSURL
                             URLWithString:@"http://apps.apple.com/app/id123"]],
      request_info, itunes_request_future.GetCallback());

  OverlayRequestQueue* queue = GetOverlayQueue();
  ASSERT_TRUE(queue);
  EXPECT_EQ(1U, queue->size());
  EXPECT_FALSE(itunes_request_future.IsReady());

  // Send a second iTunes URL request while the prompt is active; it should not
  // queue a second overlay, and should cancel the first pending request.
  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
      second_itunes_request_future;
  web_state_.ShouldAllowRequest(
      [NSURLRequest
          requestWithURL:[NSURL
                             URLWithString:@"http://apps.apple.com/app/id456"]],
      request_info, second_itunes_request_future.GetCallback());
  EXPECT_EQ(1U, queue->size());
  ASSERT_TRUE(itunes_request_future.Wait());
  EXPECT_TRUE(itunes_request_future.Get().ShouldCancelNavigation());
  EXPECT_FALSE(second_itunes_request_future.IsReady());

  // Send a regular navigation request while the prompt is active; it should
  // cancel the second iTunes request and become the new pending request.
  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
      allowed_request_future;
  web_state_.ShouldAllowRequest(
      [NSURLRequest
          requestWithURL:[NSURL URLWithString:@"https://example.com"]],
      request_info, allowed_request_future.GetCallback());
  ASSERT_TRUE(second_itunes_request_future.Wait());
  EXPECT_TRUE(second_itunes_request_future.Get().ShouldCancelNavigation());
  EXPECT_FALSE(allowed_request_future.IsReady());

  // Send a navigation response while the prompt is active; it should cancel the
  // previous pending request and become the new pending decision.
  NSURLResponse* response = [[NSURLResponse alloc]
                initWithURL:[NSURL URLWithString:@"https://example.com"]
                   MIMEType:@"text/html"
      expectedContentLength:0
           textEncodingName:nil];
  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
      response_future;
  web_state_.ShouldAllowResponse(
      response,
      web::WebStatePolicyDecider::ResponseInfo(/*for_main_frame=*/true),
      response_future.GetCallback());
  ASSERT_TRUE(allowed_request_future.Wait());
  EXPECT_TRUE(allowed_request_future.Get().ShouldCancelNavigation());
  EXPECT_FALSE(response_future.IsReady());

  // Complete the prompt and verify that the latest delayed callback is invoked
  // with its decision.
  queue->CancelAllRequests();

  ASSERT_TRUE(response_future.Wait());
  EXPECT_TRUE(response_future.Get().ShouldAllowNavigation());
}

// Verifies that iTunes URLs are not handled when the request is from iframe.
TEST_F(ITunesUrlsHandlerTabHelperTest, NoHandlingInIframes) {
  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/us/app/app_name/id123", /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/us/app/app_name/id123", /*main_frame=*/false));
  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/app/bar/id243?at=12312", /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/app/bar/id243?at=12312", /*main_frame=*/false));
  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://apps.apple.com/app/bar/id243?at=12312", /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"http://apps.apple.com/app/bar/id243?at=12312", /*main_frame=*/false));
}

// Verifies that navigating to non iTunes product URLs, or not supported iTunes
// product type URLs does not launch storekit.
TEST_F(ITunesUrlsHandlerTabHelperTest, NonMatchingUrlsDoesntLaunchStoreKit) {
  EXPECT_FALSE(VerifyStoreKitLaunched(@"", /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(@"foobar", /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(@"foo://bar", /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(@"http://foo", /*main_frame=*/true));
  EXPECT_FALSE(
      VerifyStoreKitLaunched(@"http://foo?bar#qux", /*main_frame=*/true));
  EXPECT_FALSE(
      VerifyStoreKitLaunched(@"http://foo.bar/qux", /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"http://geo.itunes.apple.com/de/genre/apps/", /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"https://itunes.apple.com/us/tv-show/theshow/id1232",
      /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(@"http://apps.apple.com/podcast/id12345",
                                      /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"itms-apps://itunes.apple.com/us/app/appname/id123",
      /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"http://test.itunes.apple.com/app/bar/id243?at=12312",
      /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"http://geo.apps.apple.com/app/bar/id243?at=12312",
      /*main_frame=*/true));
  EXPECT_FALSE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/us/movie/testmovie/id12345",
      /*main_frame=*/true));
}

// Verifies that navigating to URLs for a product hosted on iTunes AppStore
// with supported media type launches storekit.
TEST_F(ITunesUrlsHandlerTabHelperTest, MatchingUrlsLaunchesStoreKit) {
  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://apps.apple.com/us/app/app_name/id123", /*main_frame=*/true));
  NSString* product_id = @"id";
  NSString* af_tkn = @"at";
  NSDictionary* expected_params = @{product_id : @"123"};

  EXPECT_NSEQ(expected_params, fake_handler_.productParams);
  EXPECT_TRUE(VerifyStoreKitLaunched(@"http://itunes.apple.com/app/bar/id123?",
                                     /*main_frame=*/true));
  EXPECT_NSEQ(expected_params, fake_handler_.productParams);

  EXPECT_TRUE(VerifyStoreKitLaunched(@"http://apps.apple.com/app/id123",
                                     /*main_frame=*/true));
  expected_params = @{product_id : @"123"};
  EXPECT_NSEQ(expected_params, fake_handler_.productParams);

  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/app/test/id123?qux&baz#foo",
      /*main_frame=*/true));
  expected_params = @{product_id : @"123", @"qux" : @"", @"baz" : @""};
  EXPECT_NSEQ(expected_params, fake_handler_.productParams);

  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://apps.apple.com/app/bar/id243?at=12312", /*main_frame=*/true));
  expected_params = @{product_id : @"243", af_tkn : @"12312"};
  EXPECT_NSEQ(expected_params, fake_handler_.productParams);

  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://itunes.apple.com/app/bar/idabc?at=213&ct=123",
      /*main_frame=*/true));
  expected_params = @{product_id : @"abc", af_tkn : @"213", @"ct" : @"123"};
  EXPECT_NSEQ(expected_params, fake_handler_.productParams);

  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://geo.itunes.apple.com/app/bar/id243?at=12312",
      /*main_frame=*/true));
  expected_params = @{product_id : @"243", af_tkn : @"12312"};
  EXPECT_NSEQ(expected_params, fake_handler_.productParams);

  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://apps.apple.com/de/app/bar/id123?at=2&uo=4#foo",
      /*main_frame=*/true));
  expected_params = @{product_id : @"123", af_tkn : @"2", @"uo" : @"4"};
  EXPECT_NSEQ(expected_params, fake_handler_.productParams);

  EXPECT_TRUE(VerifyStoreKitLaunched(
      @"http://apps.apple.com/app-bundle/bar/id243?at=12345",
      /*main_frame=*/true));
  expected_params = @{product_id : @"243", af_tkn : @"12345"};
  EXPECT_NSEQ(expected_params, fake_handler_.productParams);
}
