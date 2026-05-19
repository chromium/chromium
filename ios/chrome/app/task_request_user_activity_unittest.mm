// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_user_activity.h"

#import <CoreSpotlight/CoreSpotlight.h>

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/application_mode.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/spotlight/actions_spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Forward declarations.
@class URLOpenerParams;
@protocol StartupInformation;

@interface TestTabOpener : SceneController <TabOpening>
@property(nonatomic, assign) UrlLoadParams urlLoadParams;
@property(nonatomic, assign) ApplicationModeForTabOpening targetMode;
@property(nonatomic, assign) BOOL dismissOmnibox;
@property(nonatomic, assign) TabOpeningPostOpeningAction recordedAction;
@end

@implementation TestTabOpener
- (void)dismissModalsAndMaybeOpenSelectedTabInMode:
            (ApplicationModeForTabOpening)targetMode
                                 withUrlLoadParams:
                                     (const UrlLoadParams&)urlLoadParams
                                    dismissOmnibox:(BOOL)dismissOmnibox
                                        completion:(ProceduralBlock)completion {
  _urlLoadParams = urlLoadParams;
  _targetMode = targetMode;
  _dismissOmnibox = dismissOmnibox;
}

- (ProceduralBlock)completionBlockForTriggeringAction:
    (TabOpeningPostOpeningAction)action {
  _recordedAction = action;
  return ^{
  };
}

// Dummy implementations to satisfy TabOpening protocol.
- (void)dismissModalsAndOpenMultipleTabsWithURLs:(const std::vector<GURL>&)URLs
                                 inIncognitoMode:(BOOL)incognitoMode
                                  dismissOmnibox:(BOOL)dismissOmnibox
                                      completion:(ProceduralBlock)completion {
}

- (void)openTabFromLaunchWithParams:(URLOpenerParams*)params
                 startupInformation:(id<StartupInformation>)startupInformation {
}

- (BOOL)shouldOpenNTPTabOnActivationOfBrowser:(Browser*)browser {
  return NO;
}

- (void)openOrReuseTabInMode:(ApplicationMode)targetMode
           withUrlLoadParams:(const UrlLoadParams&)urlLoadParams
         tabOpenedCompletion:(ProceduralBlock)completion {
}

@end

class TaskRequestForUserActivityTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    ResetEnableNewStartupFlowEnabledForTesting();
    scoped_feature_list_.InitAndEnableFeature(kEnableNewStartupFlow);
    SaveEnableNewStartupFlowForNextStart();

    profile_ = TestProfileIOS::Builder().Build();
    scene_state_ = [[FakeSceneState alloc] initWithAppState:nil
                                                    profile:profile_.get()];

    mock_profile_state_ = OCMClassMock([ProfileState class]);
    OCMStub([mock_profile_state_ profile]).andReturn(profile_.get());
    scene_state_.profileState = mock_profile_state_;

    test_tab_opener_ = [[TestTabOpener alloc] initWithSceneState:scene_state_];
    scene_state_.controller = test_tab_opener_;

    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
  }

  void TearDown() override {
    browser_.reset();
    [scene_state_ shutdown];
    scene_state_ = nil;
    profile_.reset();
    ResetEnableNewStartupFlowEnabledForTesting();
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  ProfileState* mock_profile_state_;
  FakeSceneState* scene_state_;
  TestTabOpener* test_tab_opener_;
  std::unique_ptr<TestBrowser> browser_;

  // Helper method to execute a Spotlight action.
  void ExecuteSpotlightAction(NSString* action_name) {
    NSUserActivity* user_activity = [[NSUserActivity alloc]
        initWithActivityType:CSSearchableItemActionType];
    NSString* action =
        [NSString stringWithFormat:@"%@.%@",
                                   spotlight::StringFromSpotlightDomain(
                                       spotlight::DOMAIN_ACTIONS),
                                   action_name];
    NSDictionary* user_info = @{CSSearchableItemActivityIdentifier : action};
    [user_activity addUserInfoEntriesFromDictionary:user_info];

    TaskRequestForUserActivity* request =
        [[TaskRequestForUserActivity alloc] initWithUserActivity:user_activity
                                                      sceneState:scene_state_
                                                     isColdStart:NO];
    [request execute];
  }
};

// Tests that a new incognito tab is opened for the New Incognito Tab action.
TEST_F(TaskRequestForUserActivityTest, SpotlightActionNewIncognitoTab) {
  // Only test Spotlight if it is enabled and available on the device.
  if (!spotlight::IsSpotlightAvailable()) {
    return;
  }
  // Action.
  ExecuteSpotlightAction(spotlight::kSpotlightActionNewIncognitoTab);

  // Verify.
  EXPECT_EQ(test_tab_opener_.targetMode,
            ApplicationModeForTabOpening::INCOGNITO);
  EXPECT_EQ(test_tab_opener_.urlLoadParams.web_params.url,
            GURL(kChromeUINewTabURL));
}

// Tests that voice search is started for the Voice Search action.
TEST_F(TaskRequestForUserActivityTest, SpotlightActionVoiceSearch) {
  // Only test Spotlight if it is enabled and available on the device.
  if (!spotlight::IsSpotlightAvailable()) {
    return;
  }
  // Action.
  ExecuteSpotlightAction(spotlight::kSpotlightActionVoiceSearch);

  // Verify.
  EXPECT_EQ(test_tab_opener_.targetMode, ApplicationModeForTabOpening::NORMAL);
  EXPECT_EQ(test_tab_opener_.urlLoadParams.web_params.url,
            GURL(kChromeUINewTabURL));
}

// Tests that the QR scanner is started for the QR Scanner action.
TEST_F(TaskRequestForUserActivityTest, SpotlightActionQRScanner) {
  // Only test Spotlight if it is enabled and available on the device.
  if (!spotlight::IsSpotlightAvailable()) {
    return;
  }
  // Action.
  ExecuteSpotlightAction(spotlight::kSpotlightActionQRScanner);

  // Verify.
  EXPECT_EQ(test_tab_opener_.targetMode, ApplicationModeForTabOpening::NORMAL);
  EXPECT_EQ(test_tab_opener_.urlLoadParams.web_params.url,
            GURL(kChromeUINewTabURL));
}

// Tests that a new tab is opened for the New Tab action.
TEST_F(TaskRequestForUserActivityTest, SpotlightActionNewTab) {
  // Only test Spotlight if it is enabled and available on the device.
  if (!spotlight::IsSpotlightAvailable()) {
    return;
  }
  // Action.
  ExecuteSpotlightAction(spotlight::kSpotlightActionNewTab);

  // Verify.
  EXPECT_EQ(test_tab_opener_.targetMode, ApplicationModeForTabOpening::NORMAL);
  EXPECT_EQ(test_tab_opener_.urlLoadParams.web_params.url,
            GURL(kChromeUINewTabURL));
}

// Tests that the settings are opened for the Set Default Browser action.
TEST_F(TaskRequestForUserActivityTest, SpotlightActionSetDefaultBrowser) {
  // Only test Spotlight if it is enabled and available on the device.
  if (!spotlight::IsSpotlightAvailable()) {
    return;
  }
  // Setup.
  id mock_application = OCMPartialMock([UIApplication sharedApplication]);

  // Expect that openURL:options:completionHandler: is called and prevent it
  // from forwarding to the real UIApplication (which would open Settings).
  [[[mock_application expect] andDo:^(NSInvocation* invocation){
  }] openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:[OCMArg any]
      completionHandler:[OCMArg any]];

  // Action.
  ExecuteSpotlightAction(spotlight::kSpotlightActionSetDefaultBrowser);

  // Verify.
  EXPECT_OCMOCK_VERIFY(mock_application);

  // Cleanup.
  [mock_application stopMocking];
}
