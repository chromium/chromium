// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_launcher/app_launcher_browser_agent.h"

#import <UIKit/UIKit.h>
#import <map>

#import "ios/chrome/browser/app_launcher/app_launcher_tab_helper.h"
#import "ios/chrome/browser/app_launcher/fake_app_launcher_abuse_detector.h"
#import "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/public/web_content_area/app_launcher_overlay.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/mac/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using app_launcher_overlays::AppLaunchConfirmationRequest;
using app_launcher_overlays::AllowAppLaunchResponse;

// Test fixture for AppLauncherBrowserAgent.
class AppLauncherBrowserAgentTest : public PlatformTest {
 protected:
  AppLauncherBrowserAgentTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    AppLauncherBrowserAgent::CreateForBrowser(browser_.get());
    application_ = OCMClassMock([UIApplication class]);
    OCMStub([application_ sharedApplication]).andReturn(application_);
  }
  ~AppLauncherBrowserAgentTest() override {
    [application_ stopMocking];
    browser_->GetWebStateList()->CloseAllWebStates(
        WebStateList::CLOSE_NO_FLAGS);
  }

  // Returns the AppLauncherBrowserAgent.
  AppLauncherBrowserAgent* browser_agent() {
    return AppLauncherBrowserAgent::FromBrowser(browser_.get());
  }

  // Adds a WebState to `browser_` using `opener`.  The WebState's session
  // history is populated with `nav_item_count` items.  Returns the added
  // WebState.
  web::WebState* AddWebState(web::WebState* opener, size_t nav_item_count) {
    // Create the NavigationManager and populate it with `nav_item_count` items.
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    for (size_t i = 0; i < nav_item_count; ++i) {
      navigation_manager->AddItem(GURL("http://www.chromium.test"),
                                  ui::PAGE_TRANSITION_LINK);
    }
    // Create the WebState with the fake NavigationManager.
    auto passed_web_state = std::make_unique<web::FakeWebState>();
    web::FakeWebState* web_state = passed_web_state.get();
    web_state->SetNavigationManager(std::move(navigation_manager));
    web_state->SetHasOpener(opener);
    // Ensure that the tab helper is created.
    FakeAppLauncherAbuseDetector* abuse_detector =
        [[FakeAppLauncherAbuseDetector alloc] init];
    abuse_detectors_[web_state] = abuse_detector;
    AppLauncherTabHelper::CreateForWebState(web_state, abuse_detector);
    // Insert the WebState into the Browser's WebStateList.
    int index = browser_->GetWebStateList()->count();
    browser_->GetWebStateList()->InsertWebState(
        index, std::move(passed_web_state), WebStateList::INSERT_ACTIVATE,
        WebStateOpener(opener));
    return web_state;
  }

  // Returns whether the front OverlayRequest for `web_state`'s queue is
  // configured with an AppLaunchConfirmationRequest with
  // `is_repeated_request`.
  bool IsShowingDialog(web::WebState* web_state, bool is_repeated_request) {
    OverlayRequest* request = OverlayRequestQueue::FromWebState(
                                  web_state, OverlayModality::kWebContentArea)
                                  ->front_request();
    if (!request)
      return false;

    AppLaunchConfirmationRequest* config =
        request->GetConfig<AppLaunchConfirmationRequest>();
    return config && config->is_repeated_request() == is_repeated_request;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  std::map<web::WebState*, FakeAppLauncherAbuseDetector*> abuse_detectors_;
  id application_ = nil;
};

// Tests that the browser agent shows an alert for app store URLs.
TEST_F(AppLauncherBrowserAgentTest, AppStoreUrlShowsAlert) {
  const GURL kAppStoreUrl("itms://1234");
  const GURL kSourcePageUrl("http://www.chromium.test");
  web::WebState* web_state =
      AddWebState(/*opener=*/nullptr, /*nav_item_count=*/1);

  // Request an app launch for kAppStoreUrl.
  AppLauncherTabHelper::FromWebState(web_state)->RequestToLaunchApp(
      kAppStoreUrl, kSourcePageUrl, /*link_transition=*/false);

  // Verify that an app launch overlay request was added to `web_state`'s queue.
  EXPECT_TRUE(IsShowingDialog(web_state, /*is_repeated_request=*/false));

  // Add a response allowing the navigation.
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state, OverlayModality::kWebContentArea);
  queue->front_request()->GetCallbackManager()->SetCompletionResponse(
      OverlayResponse::CreateWithInfo<AllowAppLaunchResponse>());

  // Cancel requests in the queue so that the completion callback is executed,
  // expecting that the application will open the URL.
  OCMExpect([application_ openURL:net::NSURLWithGURL(kAppStoreUrl)
                          options:@{}
                completionHandler:nil]);
  queue->CancelAllRequests();

  // Verify that the application attempts to open the URL.
  [application_ verify];
}

// Tests that the browser agent attempts to launch an external application for
// mailto URLs.
TEST_F(AppLauncherBrowserAgentTest, MailToUrlLaunchesApp) {
  const GURL kMailToUrl("mailto:user@chromium.test");
  const GURL kSourcePageUrl("http://www.chromium.test");
  web::WebState* web_state =
      AddWebState(/*opener=*/nullptr, /*nav_item_count=*/1);

  // Request an app launch for kMailToUrl with a link transition, expecting that
  // the application will open the URL.
  OCMExpect([application_ openURL:net::NSURLWithGURL(kMailToUrl)
                          options:@{}
                completionHandler:nil]);
  AppLauncherTabHelper::FromWebState(web_state)->RequestToLaunchApp(
      kMailToUrl, kSourcePageUrl, /*link_transition=*/true);

  // Verify that the application attempts to open the URL.
  [application_ verify];
}

// Tests that the browser agent attempts to launch an external application for
// app URLs.
TEST_F(AppLauncherBrowserAgentTest, AppUrlLaunchesApp) {
  const GURL kAppUrl("some-app://1234");
  const GURL kSourcePageUrl("http://www.chromium.test");
  web::WebState* web_state =
      AddWebState(/*opener=*/nullptr, /*nav_item_count=*/1);

  // Request an app launch for kAppUrl with a link transition, expecting that
  // the application will open the URL.
  OCMExpect([application_ openURL:net::NSURLWithGURL(kAppUrl)
                          options:@{}
                completionHandler:nil]);
  AppLauncherTabHelper::FromWebState(web_state)->RequestToLaunchApp(
      kAppUrl, kSourcePageUrl, /*link_transition=*/true);

  // Verify that the application attempts to open the URL.
  [application_ verify];
}

// Tests that the browser agent shows an alert for app URLs when the abuse
// detector returns ExternalAppLaunchPolicyPrompt.
TEST_F(AppLauncherBrowserAgentTest, RepeatedRequestShowsAlert) {
  const GURL kAppUrl("some-app://1234");
  const GURL kSourcePageUrl("http://www.chromium.test");
  web::WebState* web_state =
      AddWebState(/*opener=*/nullptr, /*nav_item_count=*/1);

  // Request an app launch for kAppUrl while the abuse detector returns
  // ExternalAppLaunchPolicyPrompt.
  abuse_detectors_[web_state].policy = ExternalAppLaunchPolicyPrompt;
  AppLauncherTabHelper::FromWebState(web_state)->RequestToLaunchApp(
      kAppUrl, kSourcePageUrl, /*link_transition=*/false);

  // Verify that an app launch overlay request for a repeated request was added
  // to `web_state`'s queue.
  EXPECT_TRUE(IsShowingDialog(web_state, /*is_repeated_request=*/true));

  // Add a response allowing the navigation.
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state, OverlayModality::kWebContentArea);
  queue->front_request()->GetCallbackManager()->SetCompletionResponse(
      OverlayResponse::CreateWithInfo<AllowAppLaunchResponse>());

  // Cancel requests in the queue so that the completion callback is executed,
  // expecting that the application will open the URL.
  OCMExpect([application_ openURL:net::NSURLWithGURL(kAppUrl)
                          options:@{}
                completionHandler:nil]);
  queue->CancelAllRequests();

  // Verify that the application attempts to open the URL.
  [application_ verify];
}

// Tests that the browser agent shows an alert for app URLs without a link
// transition.
TEST_F(AppLauncherBrowserAgentTest, AppUrlWithoutLinkShowsAlert) {
  const GURL kAppUrl("some-app://1234");
  const GURL kSourcePageUrl("http://www.chromium.test");
  web::WebState* web_state =
      AddWebState(/*opener=*/nullptr, /*nav_item_count=*/1);

  // Request an app launch for kAppUrl without a link transition.
  AppLauncherTabHelper::FromWebState(web_state)->RequestToLaunchApp(
      kAppUrl, kSourcePageUrl, /*link_transition=*/false);

  // Verify that an app launch overlay request was added to `web_state`'s queue.
  EXPECT_TRUE(IsShowingDialog(web_state, /*is_repeated_request=*/false));

  // Add a response allowing the navigation.
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state, OverlayModality::kWebContentArea);
  queue->front_request()->GetCallbackManager()->SetCompletionResponse(
      OverlayResponse::CreateWithInfo<AllowAppLaunchResponse>());

  // Cancel requests in the queue so that the completion callback is executed,
  // expecting that the application will open the URL.
  OCMExpect([application_ openURL:net::NSURLWithGURL(kAppUrl)
                          options:@{}
                completionHandler:nil]);
  queue->CancelAllRequests();

  // Verify that the application attempts to open the URL.
  [application_ verify];
}

// Tests that the browser agent shows a dialog in the opener's
// OverlayRequestQueue if an app launch is requested for a WebState with an
// empty session history.
TEST_F(AppLauncherBrowserAgentTest, ShowDialogInOpener) {
  const GURL kAppStoreUrl("itms://1234");
  const GURL kSourcePageUrl("http://www.chromium.test");
  web::WebState* opener = AddWebState(/*opener=*/nullptr, /*nav_item_count=*/1);
  web::WebState* web_state = AddWebState(opener, /*nav_item_count=*/0);

  // Request an app launch for kAppStoreUrl.
  AppLauncherTabHelper::FromWebState(web_state)->RequestToLaunchApp(
      kAppStoreUrl, kSourcePageUrl, /*link_transition=*/false);

  // Verify that an app launch overlay request was added to `web_state`'s queue.
  EXPECT_TRUE(IsShowingDialog(opener, /*is_repeated_request=*/false));
}
