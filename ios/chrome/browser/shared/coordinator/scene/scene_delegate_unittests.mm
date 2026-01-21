// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/scene_delegate.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/main_application_delegate.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class SceneDelegateTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    delegate_ = [[SceneDelegate alloc] init];

    mock_app_ = OCMClassMock([UIApplication class]);
    OCMStub([mock_app_ sharedApplication]).andReturn(mock_app_);

    mock_main_delegate_ = OCMClassMock([MainApplicationDelegate class]);
    OCMStub([mock_app_ delegate]).andReturn(mock_main_delegate_);

    id mock_app_state = OCMClassMock([AppState class]);
    OCMStub([mock_main_delegate_ appState]).andReturn(mock_app_state);

    mock_scene_ = OCMClassMock([UIWindowScene class]);
    id mock_session = OCMClassMock([UISceneSession class]);
    id mock_options = OCMClassMock([UISceneConnectionOptions class]);

    [delegate_ scene:mock_scene_
        willConnectToSession:mock_session
                     options:mock_options];
  }

  void TearDown() override {
    [delegate_ sceneDidDisconnect:mock_scene_];
    delegate_ = nil;
    PlatformTest::TearDown();
  }

  SceneDelegate* delegate_;
  id mock_app_;
  id mock_main_delegate_;
  id mock_scene_;
};

// Tests that performActionForShortcutItem sets startupHadExternalIntent.
TEST_F(SceneDelegateTest, TestShortcutSetsExternalIntent) {
  // Initial state should be NO as mock_options in SetUp were empty.
  ASSERT_FALSE(delegate_.sceneState.startupHadExternalIntent);

  UIApplicationShortcutItem* shortcut =
      [[UIApplicationShortcutItem alloc] initWithType:@"test"
                                       localizedTitle:@"test"];

  [delegate_ windowScene:mock_scene_
      performActionForShortcutItem:shortcut
                 completionHandler:^(BOOL succeeded){
                 }];

  EXPECT_TRUE(delegate_.sceneState.startupHadExternalIntent);
}

// Tests that openURLContexts sets startupHadExternalIntent.
TEST_F(SceneDelegateTest, TestOpenURLSetsExternalIntent) {
  EXPECT_FALSE(delegate_.sceneState.startupHadExternalIntent);
  [delegate_ scene:mock_scene_ openURLContexts:[NSSet set]];
  EXPECT_TRUE(delegate_.sceneState.startupHadExternalIntent);
}

// Tests that continueUserActivity sets startupHadExternalIntent.
TEST_F(SceneDelegateTest, TestContinueActivitySetsExternalIntent) {
  EXPECT_FALSE(delegate_.sceneState.startupHadExternalIntent);
  NSUserActivity* activity =
      [[NSUserActivity alloc] initWithActivityType:@"test"];
  [delegate_ scene:mock_scene_ continueUserActivity:activity];
  EXPECT_TRUE(delegate_.sceneState.startupHadExternalIntent);
}

// Tests that connectionOptions with a shortcut during initial connection sets
// the flag.
TEST_F(SceneDelegateTest, TestInitialConnectionWithShortcutSetsIntent) {
  // Create a new delegate for this specific case to test the connection logic
  SceneDelegate* coldStartDelegate = [[SceneDelegate alloc] init];

  id mock_options = OCMClassMock([UISceneConnectionOptions class]);
  UIApplicationShortcutItem* shortcut =
      [[UIApplicationShortcutItem alloc] initWithType:@"test"
                                       localizedTitle:@"test"];
  OCMStub([mock_options shortcutItem]).andReturn(shortcut);

  EXPECT_FALSE(coldStartDelegate.sceneState.startupHadExternalIntent);

  [coldStartDelegate scene:mock_scene_
      willConnectToSession:OCMClassMock([UISceneSession class])
                   options:mock_options];

  EXPECT_TRUE(coldStartDelegate.sceneState.startupHadExternalIntent);
  [coldStartDelegate sceneDidDisconnect:mock_scene_];
}
