// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_shortcut_item.h"

#import <UIKit/UIKit.h>

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/app/application_delegate/fake_tab_opener.h"
#import "ios/chrome/app/application_mode.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/app/task_request+testing.h"
#import "ios/chrome/app/task_request_private.h"
#import "ios/chrome/browser/intents/model/intents_constants.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

class TaskRequestForShortcutItemTest : public PlatformTest {
 protected:
  TaskRequestForShortcutItemTest() {
    ResetEnableNewStartupFlowEnabledForTesting();
    scoped_feature_list_.InitAndEnableFeature(kEnableNewStartupFlow);
    SaveEnableNewStartupFlowForNextStart();
  }

  ~TaskRequestForShortcutItemTest() override {
    ResetEnableNewStartupFlowEnabledForTesting();
  }

  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();

    tab_opener_ = [[FakeTabOpener alloc] init];
    fake_scene_state_ =
        [[FakeSceneState alloc] initWithAppState:nil profile:profile_.get()];
    fake_scene_state_.controller = (SceneController*)tab_opener_;
  }

  void TearDown() override {
    [fake_scene_state_ shutdown];
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  FakeTabOpener* tab_opener_;
  FakeSceneState* fake_scene_state_;
  base::HistogramTester histogram_tester_;
};

// Tests that execute calls TabOpening with the correct parameters for a search
// shortcut.
TEST_F(TaskRequestForShortcutItemTest, TestExecuteSearchShortcut) {
  UIApplicationShortcutItem* shortcut =
      [[UIApplicationShortcutItem alloc] initWithType:kShortcutNewSearch
                                       localizedTitle:@"Search"];

  __block BOOL handler_called = NO;
  TaskRequestForShortcutItem* task = [[TaskRequestForShortcutItem alloc]
      initWithShortcutItem:shortcut
                sceneState:fake_scene_state_
                   handler:^(BOOL succeeded) {
                     handler_called = YES;
                     EXPECT_TRUE(succeeded);
                   }
               isColdStart:NO];

  [task execute];

  EXPECT_TRUE(tab_opener_.dismissModalsCalled);
  EXPECT_EQ(ApplicationModeForTabOpening::NORMAL, tab_opener_.applicationMode);
  EXPECT_EQ(FOCUS_OMNIBOX, tab_opener_.action);
  EXPECT_FALSE(tab_opener_.dismissOmnibox);
  EXPECT_TRUE(handler_called);

  histogram_tester_.ExpectBucketCount(
      kAppLaunchSource, AppLaunchSource::LONG_PRESS_ON_APP_ICON, 1);
}

// Tests that execute calls TabOpening with the correct parameters for an
// incognito search shortcut.
TEST_F(TaskRequestForShortcutItemTest, TestExecuteIncognitoSearchShortcut) {
  UIApplicationShortcutItem* shortcut = [[UIApplicationShortcutItem alloc]
        initWithType:kShortcutNewIncognitoSearch
      localizedTitle:@"Incognito Search"];

  __block BOOL handler_called = NO;
  TaskRequestForShortcutItem* task = [[TaskRequestForShortcutItem alloc]
      initWithShortcutItem:shortcut
                sceneState:fake_scene_state_
                   handler:^(BOOL succeeded) {
                     handler_called = YES;
                     EXPECT_TRUE(succeeded);
                   }
               isColdStart:NO];

  [task execute];

  EXPECT_TRUE(tab_opener_.dismissModalsCalled);
  EXPECT_EQ(ApplicationModeForTabOpening::INCOGNITO,
            tab_opener_.applicationMode);
  EXPECT_EQ(FOCUS_OMNIBOX, tab_opener_.action);
  EXPECT_FALSE(tab_opener_.dismissOmnibox);
  EXPECT_TRUE(handler_called);
}

// Tests that execute calls TabOpening with incognito mode even for a normal
// search shortcut if incognito is forced.
TEST_F(TaskRequestForShortcutItemTest,
       TestExecuteSearchShortcut_IncognitoForced) {
  profile_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      base::Value(static_cast<int>(IncognitoModePrefs::kForced)));

  // A snackbar should be shown to the user to explain that the requested
  // mode was switched to incognito because of a policy.
  id snackbar_commands_mock = OCMProtocolMock(@protocol(SnackbarCommands));
  [fake_scene_state_.browserProviderInterface.mainBrowserProvider.browser
          ->GetCommandDispatcher()
      startDispatchingToTarget:snackbar_commands_mock
                   forProtocol:@protocol(SnackbarCommands)];

  OCMExpect([snackbar_commands_mock
      showSnackbarMessage:[OCMArg any]
           withHapticType:UINotificationFeedbackTypeError]);

  UIApplicationShortcutItem* shortcut =
      [[UIApplicationShortcutItem alloc] initWithType:kShortcutNewSearch
                                       localizedTitle:@"Search"];

  __block BOOL handler_called = NO;
  TaskRequestForShortcutItem* task = [[TaskRequestForShortcutItem alloc]
      initWithShortcutItem:shortcut
                sceneState:fake_scene_state_
                   handler:^(BOOL succeeded) {
                     handler_called = YES;
                     EXPECT_TRUE(succeeded);
                   }
               isColdStart:NO];

  [task execute];

  EXPECT_TRUE(tab_opener_.dismissModalsCalled);
  EXPECT_EQ(ApplicationModeForTabOpening::INCOGNITO,
            tab_opener_.applicationMode);
  EXPECT_EQ(FOCUS_OMNIBOX, tab_opener_.action);
  EXPECT_FALSE(tab_opener_.dismissOmnibox);
  EXPECT_TRUE(handler_called);

  EXPECT_OCMOCK_VERIFY(snackbar_commands_mock);
}

// Tests that execute calls TabOpening with normal mode even for an incognito
// search shortcut if incognito is disabled.
TEST_F(TaskRequestForShortcutItemTest,
       TestExecuteIncognitoSearchShortcut_IncognitoDisabled) {
  profile_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      base::Value(static_cast<int>(IncognitoModePrefs::kDisabled)));

  // A snackbar should be shown to the user to explain that the requested
  // mode was switched to normal because of a policy.
  id snackbar_commands_mock = OCMProtocolMock(@protocol(SnackbarCommands));
  [fake_scene_state_.browserProviderInterface.mainBrowserProvider.browser
          ->GetCommandDispatcher()
      startDispatchingToTarget:snackbar_commands_mock
                   forProtocol:@protocol(SnackbarCommands)];

  OCMExpect([snackbar_commands_mock
      showSnackbarMessage:[OCMArg any]
           withHapticType:UINotificationFeedbackTypeError]);

  UIApplicationShortcutItem* shortcut = [[UIApplicationShortcutItem alloc]
        initWithType:kShortcutNewIncognitoSearch
      localizedTitle:@"Incognito Search"];

  __block BOOL handler_called = NO;
  TaskRequestForShortcutItem* task = [[TaskRequestForShortcutItem alloc]
      initWithShortcutItem:shortcut
                sceneState:fake_scene_state_
                   handler:^(BOOL succeeded) {
                     handler_called = YES;
                     EXPECT_TRUE(succeeded);
                   }
               isColdStart:NO];

  [task execute];

  EXPECT_TRUE(tab_opener_.dismissModalsCalled);
  EXPECT_EQ(ApplicationModeForTabOpening::NORMAL, tab_opener_.applicationMode);
  EXPECT_EQ(FOCUS_OMNIBOX, tab_opener_.action);
  EXPECT_FALSE(tab_opener_.dismissOmnibox);
  EXPECT_TRUE(handler_called);

  EXPECT_OCMOCK_VERIFY(snackbar_commands_mock);
}

// Tests that execute calls TabOpening with the correct parameters for a voice
// search shortcut.
TEST_F(TaskRequestForShortcutItemTest, TestExecuteVoiceSearchShortcut) {
  UIApplicationShortcutItem* shortcut =
      [[UIApplicationShortcutItem alloc] initWithType:kShortcutVoiceSearch
                                       localizedTitle:@"Voice Search"];

  __block BOOL handler_called = NO;
  TaskRequestForShortcutItem* task = [[TaskRequestForShortcutItem alloc]
      initWithShortcutItem:shortcut
                sceneState:fake_scene_state_
                   handler:^(BOOL succeeded) {
                     handler_called = YES;
                     EXPECT_TRUE(succeeded);
                   }
               isColdStart:NO];

  [task execute];

  EXPECT_TRUE(tab_opener_.dismissModalsCalled);
  EXPECT_EQ(ApplicationModeForTabOpening::NORMAL, tab_opener_.applicationMode);
  EXPECT_EQ(START_VOICE_SEARCH, tab_opener_.action);
  EXPECT_TRUE(tab_opener_.dismissOmnibox);
  EXPECT_TRUE(handler_called);
}

// Tests that execute calls TabOpening with the correct parameters for a QR
// scanner shortcut.
TEST_F(TaskRequestForShortcutItemTest, TestExecuteQRScannerShortcut) {
  UIApplicationShortcutItem* shortcut =
      [[UIApplicationShortcutItem alloc] initWithType:kShortcutQRScanner
                                       localizedTitle:@"QR Scanner"];

  __block BOOL handler_called = NO;
  TaskRequestForShortcutItem* task = [[TaskRequestForShortcutItem alloc]
      initWithShortcutItem:shortcut
                sceneState:fake_scene_state_
                   handler:^(BOOL succeeded) {
                     handler_called = YES;
                     EXPECT_TRUE(succeeded);
                   }
               isColdStart:NO];

  [task execute];

  EXPECT_TRUE(tab_opener_.dismissModalsCalled);
  EXPECT_EQ(ApplicationModeForTabOpening::NORMAL, tab_opener_.applicationMode);
  EXPECT_EQ(START_QR_CODE_SCANNER, tab_opener_.action);
  EXPECT_TRUE(tab_opener_.dismissOmnibox);
  EXPECT_TRUE(handler_called);
}

// Tests that execute calls TabOpening with the correct parameters for a Lens
// shortcut.
TEST_F(TaskRequestForShortcutItemTest, TestExecuteLensShortcut) {
  UIApplicationShortcutItem* shortcut = [[UIApplicationShortcutItem alloc]
        initWithType:kShortcutLensFromAppIconLongPress
      localizedTitle:@"Lens"];

  __block BOOL handler_called = NO;
  TaskRequestForShortcutItem* task = [[TaskRequestForShortcutItem alloc]
      initWithShortcutItem:shortcut
                sceneState:fake_scene_state_
                   handler:^(BOOL succeeded) {
                     handler_called = YES;
                     EXPECT_TRUE(succeeded);
                   }
               isColdStart:NO];

  [task execute];

  EXPECT_TRUE(tab_opener_.dismissModalsCalled);
  EXPECT_EQ(ApplicationModeForTabOpening::NORMAL, tab_opener_.applicationMode);
  EXPECT_EQ(START_LENS_FROM_APP_ICON_LONG_PRESS, tab_opener_.action);
  EXPECT_TRUE(tab_opener_.dismissOmnibox);
  EXPECT_TRUE(handler_called);
}

// Tests that execute fails for an unknown shortcut.
TEST_F(TaskRequestForShortcutItemTest, TestExecuteUnknownShortcut) {
  UIApplicationShortcutItem* shortcut =
      [[UIApplicationShortcutItem alloc] initWithType:@"UnknownType"
                                       localizedTitle:@"Unknown"];

  __block BOOL handler_called = NO;
  TaskRequestForShortcutItem* task = [[TaskRequestForShortcutItem alloc]
      initWithShortcutItem:shortcut
                sceneState:fake_scene_state_
                   handler:^(BOOL succeeded) {
                     handler_called = YES;
                     EXPECT_FALSE(succeeded);
                   }
               isColdStart:NO];

  [task execute];

  EXPECT_FALSE(tab_opener_.dismissModalsCalled);
  EXPECT_TRUE(handler_called);
}
