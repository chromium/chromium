// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_launcher/model/app_launcher_browser_agent.h"

#import <UIKit/UIKit.h>

#import <map>

#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/app_launcher/model/app_launcher_tab_helper.h"
#import "ios/chrome/browser/app_launcher/model/app_launcher_tab_helper_browser_presentation_provider.h"
#import "ios/chrome/browser/app_launcher/model/fake_app_launcher_abuse_detector.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/app_launcher_overlay.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "url/gurl.h"

using app_launcher_overlays::AllowAppLaunchResponse;
using app_launcher_overlays::AppLaunchConfirmationRequest;

// A Fake AppLauncherTabHelper that allows to retrieve delegate and call
// directly AppLauncherTabHelperDelegate methods.
class FakeAppLauncherTabHelper : public AppLauncherTabHelper {
 public:
  explicit FakeAppLauncherTabHelper(web::WebState* web_state,
                                    AppLauncherAbuseDetector* abuse_detector,
                                    bool incognito)
      : AppLauncherTabHelper(web_state, abuse_detector, incognito) {}

  static void CreateForWebState(web::WebState* web_state,
                                AppLauncherAbuseDetector* abuse_detector,
                                bool incognito) {
    web_state->SetUserData(UserDataKey(),
                           std::make_unique<FakeAppLauncherTabHelper>(
                               web_state, abuse_detector, incognito));
  }

  void SetDelegate(AppLauncherTabHelperDelegate* delegate) override {
    AppLauncherTabHelper::SetDelegate(delegate);
    delegate_ = delegate;
  }

  AppLauncherTabHelperDelegate* delegate() { return delegate_; }

 private:
  raw_ptr<AppLauncherTabHelperDelegate> delegate_;
};

// Test fixture for AppLauncherBrowserAgent.
class AppLauncherBrowserAgentTest : public PlatformTest {
 protected:
  AppLauncherBrowserAgentTest() {
    profile_ = TestProfileIOS::Builder().Build();
    app_state_ = [[AppState alloc] initWithStartupInformation:nil];
    scene_state_ = [[SceneState alloc] initWithAppState:app_state_];
    scene_state_.activationLevel = SceneActivationLevelForegroundActive;
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    browser_->GetSceneState().activationLevel =
        SceneActivationLevelForegroundActive;
    AppLauncherBrowserAgent::CreateForBrowser(browser_.get());
    application_ = OCMClassMock([UIApplication class]);
    OCMStub([application_ sharedApplication]).andReturn(application_);
  }

  ~AppLauncherBrowserAgentTest() override {
    [application_ stopMocking];
    CloseAllWebStates(*browser_->GetWebStateList(),
                      WebStateList::CLOSE_NO_FLAGS);
  }

  // Returns the AppLauncherBrowserAgent.
  AppLauncherBrowserAgent* browser_agent() {
    return AppLauncherBrowserAgent::FromBrowser(browser_.get());
  }

  // Returns the AppLauncherTabHelperDelegate.
  AppLauncherTabHelperDelegate* GetTabHelperDelegate(web::WebState* web_state) {
    FakeAppLauncherTabHelper* fake_tab_helper =
        static_cast<FakeAppLauncherTabHelper*>(
            AppLauncherTabHelper::FromWebState(web_state));
    return fake_tab_helper->delegate();
  }

  // Adds a WebState to `browser_` using `opener`.  The WebState's session
  // history is populated with `nav_item_count` items.  Returns the added
  // WebState.
  web::WebState* AddWebState(web::WebState* opener,
                             size_t nav_item_count,
                             bool incognito = false) {
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
    web_state->WasShown();
    // Ensure that the tab helper is created.
    FakeAppLauncherAbuseDetector* abuse_detector =
        [[FakeAppLauncherAbuseDetector alloc] init];
    abuse_detectors_[web_state] = abuse_detector;
    OverlayRequestQueue::CreateForWebState(web_state);
    FakeAppLauncherTabHelper::CreateForWebState(web_state, abuse_detector,
                                                incognito);
    app_launcher_tab_helper_browser_presentation_provider_ = OCMProtocolMock(
        @protocol(AppLauncherTabHelperBrowserPresentationProvider));
    [[[app_launcher_tab_helper_browser_presentation_provider_ stub]
        andReturnValue:@NO] isBrowserPresentingUI];
    AppLauncherTabHelper::FromWebState(web_state)
        ->SetBrowserPresentationProvider(
            app_launcher_tab_helper_browser_presentation_provider_);

    // Insert the WebState into the Browser's WebStateList.
    browser_->GetWebStateList()->InsertWebState(
        std::move(passed_web_state),
        WebStateList::InsertionParams::Automatic().Activate().WithOpener(
            WebStateOpener(opener)));
    return web_state;
  }

  // Returns whether the front OverlayRequest for `web_state`'s queue is
  // configured with an AppLaunchConfirmationRequest with
  // `is_repeated_request`.
  bool IsShowingDialog(
      web::WebState* web_state,
      app_launcher_overlays::AppLaunchConfirmationRequestCause cause) {
    OverlayRequest* request = OverlayRequestQueue::FromWebState(
                                  web_state, OverlayModality::kWebContentArea)
                                  ->front_request();
    if (!request) {
      return false;
    }

    AppLaunchConfirmationRequest* config =
        request->GetConfig<AppLaunchConfirmationRequest>();
    return config && config->cause() == cause;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  AppState* app_state_;
  SceneState* scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  std::map<web::WebState*, FakeAppLauncherAbuseDetector*> abuse_detectors_;
  id application_ = nil;
  id app_launcher_tab_helper_browser_presentation_provider_ = nil;
};

// Tests that the browser agent shows an alert for app store URLs.
TEST_F(AppLauncherBrowserAgentTest, AppStoreUrlShowsAlert) {
  const GURL kAppStoreUrl("itms://1234");
  const GURL kSourcePageUrl("http://www.chromium.test");
  web::WebState* web_state =
      AddWebState(/*opener=*/nullptr, /*nav_item_count=*/1);

  // Request an app launch for kAppStoreUrl.
  AppLauncherTabHelper::FromWebState(web_state)->RequestToLaunchApp(
      kAppStoreUrl, kSourcePageUrl, /*link_transition=*/false,
      /*is_user_initiated=*/true, /*user_tapped_recently=*/true);

  // Verify that an app launch overlay request was added to `web_state`'s queue.
  EXPECT_TRUE(IsShowingDialog(
      web_state,
      app_launcher_overlays::AppLaunchConfirmationRequestCause::kOther));

  // Add a response allowing the navigation.
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state, OverlayModality::kWebContentArea);
  queue->front_request()->GetCallbackManager()->SetCompletionResponse(
      OverlayResponse::CreateWithInfo<AllowAppLaunchResponse>());

  // Cancel requests in the queue so that the completion callback is executed,
  // expecting that the application will open the URL.
  OCMExpect([application_ openURL:net::NSURLWithGURL(kAppStoreUrl)
                          options:@{}
                completionHandler:[OCMArg isNotNil]]);
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
                completionHandler:[OCMArg isNotNil]]);
  AppLauncherTabHelper::FromWebState(web_state)->RequestToLaunchApp(
      kMailToUrl, kSourcePageUrl, /*link_transition=*/true,
      /*is_user_initiated=*/true, /*user_tapped_recently=*/true);

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
                completionHandler:[OCMArg isNotNil]]);
  AppLauncherTabHelper::FromWebState(web_state)->RequestToLaunchApp(
      kAppUrl, kSourcePageUrl, /*link_transition=*/true,
      /*is_user_initiated=*/true, /*user_tapped_recently=*/true);

  // Verify that the application attempts to open the URL.
  [application_ verify];
}

// Tests that the browser agent shows an alert for app URLs when the abuse
// detector returns ExternalAppLaunchPolicyPrompt.
TEST_F(AppLauncherBrowserAgentTest, RepeatedRequestShowsAlert) {
  const base::HistogramTester histogram_tester;
  const GURL kAppUrl("some-app://1234");
  const GURL kSourcePageUrl("http://www.chromium.test");
  web::WebState* web_state =
      AddWebState(/*opener=*/nullptr, /*nav_item_count=*/1);

  // Request an app launch for kAppUrl while the abuse detector returns
  // ExternalAppLaunchPolicyPrompt.
  abuse_detectors_[web_state].policy = ExternalAppLaunchPolicyPrompt;
  AppLauncherTabHelper::FromWebState(web_state)->RequestToLaunchApp(
      kAppUrl, kSourcePageUrl, /*link_transition=*/true,
      /*is_user_initiated=*/true, /*user_tapped_recently=*/true);

  // Verify that an app launch overlay request for a repeated request was added
  // to `web_state`'s queue.
  EXPECT_TRUE(IsShowingDialog(
      web_state, app_launcher_overlays::AppLaunchConfirmationRequestCause::
                     kRepeatedRequest));

  // Add a response allowing the navigation.
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state, OverlayModality::kWebContentArea);
  queue->front_request()->GetCallbackManager()->SetCompletionResponse(
      OverlayResponse::CreateWithInfo<AllowAppLaunchResponse>());

  // Cancel requests in the queue so that the completion callback is executed,
  // expecting that the application will open the URL.
  OCMExpect([application_ openURL:net::NSURLWithGURL(kAppUrl)
                          options:@{}
                completionHandler:[OCMArg isNotNil]]);
  queue->CancelAllRequests();

  histogram_tester.ExpectBucketCount("Tab.ExternalApplicationOpened.Repeated",
                                     /*true*/ 1, 1);
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
      kAppUrl, kSourcePageUrl, /*link_transition=*/false,
      /*is_user_initiated=*/true, /*user_tapped_recently=*/true);

  // Verify that an app launch overlay request was added to `web_state`'s queue.
  EXPECT_TRUE(IsShowingDialog(
      web_state,
      app_launcher_overlays::AppLaunchConfirmationRequestCause::kOther));

  // Add a response allowing the navigation.
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state, OverlayModality::kWebContentArea);
  queue->front_request()->GetCallbackManager()->SetCompletionResponse(
      OverlayResponse::CreateWithInfo<AllowAppLaunchResponse>());

  // Cancel requests in the queue so that the completion callback is executed,
  // expecting that the application will open the URL.
  OCMExpect([application_ openURL:net::NSURLWithGURL(kAppUrl)
                          options:@{}
                completionHandler:[OCMArg isNotNil]]);
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
      kAppStoreUrl, kSourcePageUrl, /*link_transition=*/false,
      /*is_user_initiated=*/true, /*user_tapped_recently=*/true);

  // Verify that an app launch overlay request was added to `web_state`'s queue.
  EXPECT_TRUE(IsShowingDialog(
      web_state,
      app_launcher_overlays::AppLaunchConfirmationRequestCause::kOther));
}

// Tests that the browser agent shows an alert when opening a URL from
// incognito.
TEST_F(AppLauncherBrowserAgentTest, IncognitoRequestShowsAlert) {
  const base::HistogramTester histogram_tester;
  const GURL kAppUrl("some-app://1234");
  const GURL kSourcePageUrl("http://www.chromium.test");
  web::WebState* web_state =
      AddWebState(/*opener=*/nullptr, /*nav_item_count=*/1, /*incognito=*/true);

  // Request an app launch for kAppUrl while the abuse detector returns
  // ExternalAppLaunchPolicyPrompt.
  abuse_detectors_[web_state].policy = ExternalAppLaunchPolicyAllow;
  AppLauncherTabHelper::FromWebState(web_state)->RequestToLaunchApp(
      kAppUrl, kSourcePageUrl, /*link_transition=*/true,
      /*is_user_initiated=*/true, /*user_tapped_recently=*/true);

  // Verify that an app launch overlay request for a repeated request was added
  // to `web_state`'s queue.
  EXPECT_TRUE(IsShowingDialog(
      web_state, app_launcher_overlays::AppLaunchConfirmationRequestCause::
                     kOpenFromIncognito));

  // Add a response allowing the navigation.
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state, OverlayModality::kWebContentArea);
  queue->front_request()->GetCallbackManager()->SetCompletionResponse(
      OverlayResponse::CreateWithInfo<AllowAppLaunchResponse>());

  // Cancel requests in the queue so that the completion callback is executed,
  // expecting that the application will open the URL.
  OCMExpect([application_ openURL:net::NSURLWithGURL(kAppUrl)
                          options:@{}
                completionHandler:[OCMArg isNotNil]]);
  queue->CancelAllRequests();

  histogram_tester.ExpectBucketCount(
      "Tab.ExternalApplicationOpened.FromIncognito",
      /*true*/ 1, 1);

  // Verify that the application attempts to open the URL.
  [application_ verify];
}

// Tests that the browser agent shows an alert when opening a URL without user
// interaction.
TEST_F(AppLauncherBrowserAgentTest, NoUserInteractionRequestShowsAlert) {
  const base::HistogramTester histogram_tester;
  const GURL kAppUrl("some-app://1234");
  const GURL kSourcePageUrl("http://www.chromium.test");
  web::WebState* web_state =
      AddWebState(/*opener=*/nullptr, /*nav_item_count=*/1);

  // Request an app launch for kAppUrl while the abuse detector returns
  // ExternalAppLaunchPolicyPrompt.
  abuse_detectors_[web_state].policy = ExternalAppLaunchPolicyAllow;
  AppLauncherTabHelper::FromWebState(web_state)->RequestToLaunchApp(
      kAppUrl, kSourcePageUrl, /*link_transition=*/true,
      /*is_user_initiated=*/false, /*user_tapped_recently=*/false);

  // Verify that an app launch overlay request for a repeated request was added
  // to `web_state`'s queue.
  EXPECT_TRUE(IsShowingDialog(
      web_state, app_launcher_overlays::AppLaunchConfirmationRequestCause::
                     kNoUserInteraction));

  // Add a response allowing the navigation.
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state, OverlayModality::kWebContentArea);
  queue->front_request()->GetCallbackManager()->SetCompletionResponse(
      OverlayResponse::CreateWithInfo<AllowAppLaunchResponse>());

  // Cancel requests in the queue so that the completion callback is executed,
  // expecting that the application will open the URL.
  OCMExpect([application_ openURL:net::NSURLWithGURL(kAppUrl)
                          options:@{}
                completionHandler:[OCMArg isNotNil]]);
  queue->CancelAllRequests();

  histogram_tester.ExpectBucketCount(
      "Tab.ExternalApplicationOpened.NoUserInteraction",
      /*true*/ 1, 1);

  // Verify that the application attempts to open the URL.
  [application_ verify];
}

// Tests that completion is called on scene state activation
TEST_F(AppLauncherBrowserAgentTest, CompletionCalledOnSceneActivation) {
  const GURL kAppUrl("some-app://1234");

  web::WebState* web_state =
      AddWebState(/*opener=*/nullptr, /*nav_item_count=*/1);
  AppLauncherTabHelperDelegate* delegate = GetTabHelperDelegate(web_state);

  OCMExpect([application_ openURL:net::NSURLWithGURL(kAppUrl)
                          options:@{}
                completionHandler:[OCMArg checkWithBlock:^(void (
                                      ^completionHandler)(BOOL success)) {
                  completionHandler(YES);
                  return YES;
                }]]);

  __block bool completion_called = false;
  __block bool back_to_app_called = false;
  delegate->LaunchAppForTabHelper(AppLauncherTabHelper::FromWebState(web_state),
                                  kAppUrl, base::BindOnce(^(bool) {
                                    completion_called = true;
                                  }),
                                  base::BindOnce(^() {
                                    back_to_app_called = true;
                                  }));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(completion_called);
  EXPECT_FALSE(back_to_app_called);
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  EXPECT_FALSE(back_to_app_called);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_TRUE(back_to_app_called);
}

// Tests that back to app completion is not called after an application launch
// failure.
TEST_F(AppLauncherBrowserAgentTest, CompletionCalledOnCompletionOnFailure) {
  const GURL kAppUrl("some-app://1234");

  web::WebState* web_state =
      AddWebState(/*opener=*/nullptr, /*nav_item_count=*/1);
  AppLauncherTabHelperDelegate* delegate = GetTabHelperDelegate(web_state);

  OCMExpect([application_ openURL:net::NSURLWithGURL(kAppUrl)
                          options:@{}
                completionHandler:[OCMArg checkWithBlock:^(void (
                                      ^completionHandler)(BOOL success)) {
                  completionHandler(NO);
                  return YES;
                }]]);

  __block bool completion_called = false;
  __block bool back_to_app_called = false;
  delegate->LaunchAppForTabHelper(AppLauncherTabHelper::FromWebState(web_state),
                                  kAppUrl, base::BindOnce(^(bool) {
                                    completion_called = true;
                                  }),
                                  base::BindOnce(^() {
                                    back_to_app_called = true;
                                  }));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(completion_called);
  EXPECT_FALSE(back_to_app_called);
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  EXPECT_FALSE(back_to_app_called);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_FALSE(back_to_app_called);
}
