// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/threading/thread.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/application_delegate/url_opener.h"
#import "ios/chrome/app/application_delegate/url_opener_params.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller_testing.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider.h"
#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"
#import "ios/chrome/browser/ui/main/wrangled_browser.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// A block that takes the arguments of
// +handleLaunchOptions:applicationActive:tabOpener:startupInformation: and
// returns nothing.
typedef void (^HandleLaunchOptions)(id self,
                                    NSDictionary* options,
                                    id<TabOpening> tabOpener,
                                    id<StartupInformation> startupInformation,
                                    AppState* appState);

class TabOpenerTest : public PlatformTest {
 protected:
  void TearDown() override {
    PlatformTest::TearDown();
  }

  BOOL swizzleHasBeenCalled() { return swizzle_block_executed_; }

  void swizzleHandleLaunchOptions(
      URLOpenerParams* expectedParams,
      id<ConnectionInformation> expectedConnectionInformation,
      id<StartupInformation> expectedStartupInformation,
      AppState* expectedAppState) {
    swizzle_block_executed_ = NO;
    swizzle_block_ =
        [^(id self, URLOpenerParams* params, id<TabOpening> tabOpener,
           id<ConnectionInformation> connectionInformation,
           id<StartupInformation> startupInformation, AppState* appState) {
          swizzle_block_executed_ = YES;
          EXPECT_EQ(expectedParams, params);
          EXPECT_EQ(expectedConnectionInformation, connectionInformation);
          EXPECT_EQ(expectedStartupInformation, startupInformation);
          EXPECT_EQ(scene_controller_, tabOpener);
          EXPECT_EQ(expectedAppState, appState);
        } copy];
    URL_opening_handle_launch_swizzler_.reset(new ScopedBlockSwizzler(
        [URLOpener class],
        @selector(handleLaunchOptions:
                            tabOpener:connectionInformation:startupInformation
                                     :appState:prefService:),
        swizzle_block_));
  }

  SceneController* GetSceneController() {
    if (!scene_controller_) {
      id mock_wrangled_browser = OCMClassMock(WrangledBrowser.class);

      sync_preferences::PrefServiceMockFactory factory;
      scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
          new user_prefs::PrefRegistrySyncable);
      RegisterBrowserStatePrefs(registry.get());

      TestChromeBrowserState::Builder builder;
      builder.SetPrefService(factory.CreateSyncable(registry.get()));
      browser_state_ = builder.Build();

      OCMStub([mock_wrangled_browser browserState])
          .andReturn(browser_state_.get());

      SceneController* controller =
          [[SceneController alloc] initWithSceneState:scene_state_];

      mockController_ = OCMPartialMock(controller);
      OCMStub([mockController_ currentInterface])
          .andReturn(mock_wrangled_browser);

      scene_controller_ = controller;
    }
    return scene_controller_;
  }

 private:
  web::WebTaskEnvironment task_environment_;
  // Keep the partial mock object alive to avoid automatic deallocation when out
  // of scope.
  id mockController_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  SceneState* scene_state_;
  SceneController* scene_controller_;

  __block BOOL swizzle_block_executed_;
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

  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  id appStateMock = [OCMockObject mockForClass:[AppState class]];

  id<TabOpening> tabOpener = GetSceneController();
  id<ConnectionInformation> connectionInformation = GetSceneController();

  swizzleHandleLaunchOptions(params, connectionInformation,
                             startupInformationMock, appStateMock);

  // Action.
  [tabOpener openTabFromLaunchWithParams:params
                      startupInformation:startupInformationMock
                                appState:appStateMock];

  // Test.
  EXPECT_TRUE(swizzleHasBeenCalled());
}

// Tests that -newTabFromLaunchOptions do nothing if launchOptions is nil.
TEST_F(TabOpenerTest, openTabFromLaunchWithParamsWithNil) {
  // Setup.
  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  id appStateMock = [OCMockObject mockForClass:[AppState class]];

  id<TabOpening> tabOpener = GetSceneController();
  id<ConnectionInformation> connectionInformation = GetSceneController();
  swizzleHandleLaunchOptions(nil, connectionInformation, startupInformationMock,
                             appStateMock);

  // Action.
  [tabOpener openTabFromLaunchWithParams:nil
                      startupInformation:startupInformationMock
                                appState:appStateMock];

  // Test.
  EXPECT_FALSE(swizzleHasBeenCalled());
}
}  // namespace
