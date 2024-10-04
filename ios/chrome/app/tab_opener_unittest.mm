// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/application_delegate/url_opener.h"
#import "ios/chrome/app/application_delegate/url_opener_params.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller_testing.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"
#import "ios/chrome/browser/ui/main/wrangled_browser.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

// Prototype of the block used to swizzle the method under test of URLOpener.
using HandleLaunchOptions = void (^)(id,
                                     URLOpenerParams*,
                                     id<TabOpening>,
                                     id<ConnectionInformation>,
                                     id<StartupInformation>,
                                     PrefService*,
                                     ProfileInitStage);

}  // namespace

class TabOpenerTest : public PlatformTest {
 protected:
  void TearDown() override {
    if (scene_controller_) {
      [scene_controller_ teardownUI];
      scene_controller_ = nil;
    }
    PlatformTest::TearDown();
  }

  BOOL HasSwizzledMethodBeenCalled() { return swizzle_block_executed_; }

  void InstallSwizzleForHandleLauncheOptionsMethod(
      URLOpenerParams* expected_params,
      id<ConnectionInformation> expected_connection_information,
      id<StartupInformation> expected_startup_information,
      ProfileInitStage expected_init_stage) {
    swizzle_block_executed_ = NO;
    swizzle_block_ =
        ^(id self, URLOpenerParams* params, id<TabOpening> tab_opener,
          id<ConnectionInformation> connection_information,
          id<StartupInformation> startup_information, PrefService* pref_service,
          ProfileInitStage init_stage) {
          swizzle_block_executed_ = YES;
          EXPECT_EQ(expected_params, params);
          EXPECT_EQ(expected_connection_information, connection_information);
          EXPECT_EQ(expected_startup_information, startup_information);
          EXPECT_EQ(scene_controller_, tab_opener);
          EXPECT_EQ(expected_init_stage, init_stage);
        };
    URL_opening_handle_launch_swizzler_ = std::make_unique<ScopedBlockSwizzler>(
        [URLOpener class],
        @selector(handleLaunchOptions:
                            tabOpener:connectionInformation:startupInformation
                                     :prefService:initStage:),
        swizzle_block_);
  }

  SceneController* GetSceneController() {
    if (!scene_controller_) {
      profile_ = TestProfileIOS::Builder().Build();

      profile_state_ = OCMClassMock([ProfileState class]);
      OCMStub([profile_state_ initStage]).andReturn(ProfileInitStage::kFinal);

      scene_state_ = [[FakeSceneState alloc] initWithAppState:nil
                                                      profile:profile_.get()];
      scene_state_.profileState = profile_state_;

      SceneController* controller =
          [[SceneController alloc] initWithSceneState:scene_state_];

      id mock_wrangled_browser = OCMClassMock([WrangledBrowser class]);
      OCMStub([mock_wrangled_browser profile]).andReturn(profile_.get());

      scene_controller_ = OCMPartialMock(controller);
      OCMStub([scene_controller_ currentInterface])
          .andReturn(mock_wrangled_browser);

      scene_controller_ = controller;
    }
    return scene_controller_;
  }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  ProfileState* profile_state_;
  SceneState* scene_state_;
  SceneController* scene_controller_;

  BOOL swizzle_block_executed_;
  HandleLaunchOptions swizzle_block_;
  std::unique_ptr<ScopedBlockSwizzler> URL_opening_handle_launch_swizzler_;
};

#pragma mark - Tests.

// Tests that -newTabFromLaunchOptions calls +handleLaunchOption and reset
// options.
TEST_F(TabOpenerTest, openTabFromLaunchWithParamsWithOptions) {
  // Setup.
  NSString* sourceApplication = @"com.apple.mobilesafari";
  URLOpenerParams* params =
      [[URLOpenerParams alloc] initWithURL:nil
                         sourceApplication:sourceApplication];

  id mock_startup_information =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];

  id<TabOpening> tab_opener = GetSceneController();
  id<ConnectionInformation> connection_information = GetSceneController();

  InstallSwizzleForHandleLauncheOptionsMethod(params, connection_information,
                                              mock_startup_information,
                                              ProfileInitStage::kFinal);

  // Action.
  [tab_opener openTabFromLaunchWithParams:params
                       startupInformation:mock_startup_information];

  // Test.
  EXPECT_TRUE(HasSwizzledMethodBeenCalled());
}

// Tests that -newTabFromLaunchOptions do nothing if launchOptions is nil.
TEST_F(TabOpenerTest, openTabFromLaunchWithParamsWithNil) {
  // Setup.
  id mock_startup_information =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];

  id<TabOpening> tab_opener = GetSceneController();
  id<ConnectionInformation> connection_information = GetSceneController();
  InstallSwizzleForHandleLauncheOptionsMethod(nil, connection_information,
                                              mock_startup_information,
                                              ProfileInitStage::kFinal);

  // Action.
  [tab_opener openTabFromLaunchWithParams:nil
                       startupInformation:mock_startup_information];

  // Test.
  EXPECT_FALSE(HasSwizzledMethodBeenCalled());
}
