// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"

#import "base/test/task_environment.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/intents/intents_constants.h"
#import "ios/chrome/browser/intents/user_activity_browser_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/main/wrangled_browser.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@interface InternalFakeSceneController : SceneController
// Browser and ChromeBrowserState used to mock the currentInterface.
@property(nonatomic, assign) Browser* browser;
@property(nonatomic, assign) ChromeBrowserState* chromeBrowserState;
// Mocked currentInterface.
@property(nonatomic, assign) WrangledBrowser* currentInterface;
// Argument for
// -dismissModalsAndMaybeOpenSelectedTabInMode:withUrlLoadParams:dismissOmnibox:
//  completion:.
@property(nonatomic, readonly) ApplicationModeForTabOpening applicationMode;
@end

@implementation InternalFakeSceneController

- (BOOL)isIncognitoForced {
  return NO;
}

- (void)dismissModalsAndMaybeOpenSelectedTabInMode:
            (ApplicationModeForTabOpening)targetMode
                                 withUrlLoadParams:
                                     (const UrlLoadParams&)urlLoadParams
                                    dismissOmnibox:(BOOL)dismissOmnibox
                                        completion:(ProceduralBlock)completion {
  _applicationMode = targetMode;
}

- (BOOL)URLIsOpenedInRegularMode:(const GURL&)URL {
  return NO;
}
@end

namespace {

class SceneControllerTest : public PlatformTest {
 protected:
  SceneControllerTest() {
    AppState* appState = CreateMockAppState(InitStageFinal);
    scene_state_ = [[SceneState alloc] initWithAppState:appState];

    scene_controller_ =
        [[InternalFakeSceneController alloc] initWithSceneState:scene_state_];
    scene_state_.controller = scene_controller_;

    TestChromeBrowserState::Builder builder;
    browser_state_ = builder.Build();
    browser_ =
        std::make_unique<TestBrowser>(browser_state_.get(), scene_state_);
    UserActivityBrowserAgent::CreateForBrowser(browser_.get());

    scene_controller_.browser = browser_.get();
    scene_controller_.chromeBrowserState = browser_state_.get();
    scene_controller_.currentInterface = CreateMockCurrentInterface();
    connection_information_ = scene_state_.controller;
  }

  ~SceneControllerTest() override { [scene_controller_ teardownUI]; }

  // Mock & stub an AppState object with an arbitrary `init_stage` property.
  id CreateMockAppState(InitStage init_stage) {
    id mock_app_state = OCMClassMock([AppState class]);
    OCMStub([(AppState*)mock_app_state initStage]).andReturn(init_stage);
    return mock_app_state;
  }

  // Mock & stub a WrangledBrowser object.
  id CreateMockCurrentInterface() {
    id mock_wrangled_browser = OCMClassMock(WrangledBrowser.class);
    OCMStub([mock_wrangled_browser browserState])
        .andReturn(browser_state_.get());
    OCMStub([mock_wrangled_browser browser]).andReturn(browser_.get());
    return mock_wrangled_browser;
  }

  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  InternalFakeSceneController* scene_controller_;
  SceneState* scene_state_;
  id<ConnectionInformation> connection_information_;
  base::test::TaskEnvironment task_environment;
};

// TODO(crbug.com/1084905): Add a test for keeping validity of detecting a fresh
// open in new window coming from ios dock. 'Dock' is considered the default
// when the new window opening request is external to chrome and unknown.

// Tests that scene controller updates scene state's incognitoContentVisible
// when the relevant application command is called.
TEST_F(SceneControllerTest, UpdatesIncognitoContentVisibility) {
  [scene_controller_ setIncognitoContentVisible:NO];
  EXPECT_FALSE(scene_state_.incognitoContentVisible);
  [scene_controller_ setIncognitoContentVisible:YES];
  EXPECT_TRUE(scene_state_.incognitoContentVisible);
  [scene_controller_ setIncognitoContentVisible:NO];
  EXPECT_FALSE(scene_state_.incognitoContentVisible);
}

// Tests that scene controller correctly handles an external intent to
// OpenIncognitoSearch.
// TODO(crbug.com/1506950): re-enabled the test.
TEST_F(SceneControllerTest, DISABLED_TestOpenIncognitoSearchForShortcutItem) {
  UIApplicationShortcutItem* shortcut = [[UIApplicationShortcutItem alloc]
        initWithType:kShortcutNewIncognitoSearch
      localizedTitle:kShortcutNewIncognitoSearch];
  [scene_controller_ performActionForShortcutItem:shortcut
                                completionHandler:nil];
  EXPECT_TRUE(scene_state_.startupHadExternalIntent);
  EXPECT_EQ(ApplicationModeForTabOpening::INCOGNITO,
            [scene_controller_ applicationMode]);
  EXPECT_EQ(FOCUS_OMNIBOX,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that scene controller correctly handles an external intent to
// OpenNewSearch.
TEST_F(SceneControllerTest, TestOpenNewSearchForShortcutItem) {
  UIApplicationShortcutItem* shortcut =
      [[UIApplicationShortcutItem alloc] initWithType:kShortcutNewSearch
                                       localizedTitle:kShortcutNewSearch];
  [scene_controller_ performActionForShortcutItem:shortcut
                                completionHandler:nil];
  EXPECT_TRUE(scene_state_.startupHadExternalIntent);
  EXPECT_EQ(ApplicationModeForTabOpening::NORMAL,
            [scene_controller_ applicationMode]);
  EXPECT_EQ(FOCUS_OMNIBOX,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that scene controller correctly handles an external intent to
// OpenVoiceSearch.
TEST_F(SceneControllerTest, TestOpenVoiceSearchForShortcutItem) {
  UIApplicationShortcutItem* shortcut =
      [[UIApplicationShortcutItem alloc] initWithType:kShortcutVoiceSearch
                                       localizedTitle:kShortcutVoiceSearch];
  [scene_controller_ performActionForShortcutItem:shortcut
                                completionHandler:nil];
  EXPECT_TRUE(scene_state_.startupHadExternalIntent);
  EXPECT_EQ(ApplicationModeForTabOpening::NORMAL,
            [scene_controller_ applicationMode]);
  EXPECT_EQ(START_VOICE_SEARCH,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that scene controller correctly handles an external intent to
// OpenQRScanner.
TEST_F(SceneControllerTest, TestOpenQRScannerForShortcutItem) {
  UIApplicationShortcutItem* shortcut =
      [[UIApplicationShortcutItem alloc] initWithType:kShortcutQRScanner
                                       localizedTitle:kShortcutQRScanner];
  [scene_controller_ performActionForShortcutItem:shortcut
                                completionHandler:nil];
  EXPECT_TRUE(scene_state_.startupHadExternalIntent);
  EXPECT_EQ(ApplicationModeForTabOpening::NORMAL,
            [scene_controller_ applicationMode]);
  EXPECT_EQ(START_QR_CODE_SCANNER,
            [connection_information_ startupParameters].postOpeningAction);
}

}  // namespace
