// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/browser_coordinator.h"

#include "base/files/file_util.h"
#include "base/test/scoped_feature_list.h"
#include "ios/chrome/browser/download/download_directory_util.h"
#import "ios/chrome/browser/download/external_app_util.h"
#include "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for BrowserCoordinator testing.
class BrowserCoordinatorTest : public PlatformTest {
 protected:
  BrowserCoordinatorTest()
      : base_view_controller_([[UIViewController alloc] init]),
        browser_(std::make_unique<TestBrowser>()),
        scene_state_([[SceneState alloc] initWithAppState:nil]) {
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());

    IncognitoReauthSceneAgent* reauthAgent = [[IncognitoReauthSceneAgent alloc]
        initWithReauthModule:[[ReauthenticationModule alloc] init]];
    [scene_state_ addAgent:reauthAgent];

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:reauthAgent
                             forProtocol:@protocol(IncognitoReauthCommands)];
  }

  BrowserCoordinator* GetBrowserCoordinator() {
    return [[BrowserCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
  }

  web::WebTaskEnvironment task_environment_;
  UIViewController* base_view_controller_;
  std::unique_ptr<TestBrowser> browser_;
  SceneState* scene_state_;
};

// Tests if the URL to open the downlads directory from files.app is valid.
TEST_F(BrowserCoordinatorTest, ShowDownloadsFolder) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{fullscreen::features::kFullscreenControllerBrowserScoped, {}}}, {});

  base::FilePath download_dir;
  GetDownloadsDirectory(&download_dir);

  NSURL* url = GetFilesAppUrl();
  ASSERT_TRUE(url);

  UIApplication* shared_application = [UIApplication sharedApplication];
  ASSERT_TRUE([shared_application canOpenURL:url]);

  id shared_application_mock = OCMPartialMock(shared_application);

  OCMExpect([shared_application_mock openURL:url
                                     options:[OCMArg any]
                           completionHandler:nil]);

  BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();

  [browser_coordinator start];

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<BrowserCoordinatorCommands> handler =
      HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);
  [handler showDownloadsFolder];

  [browser_coordinator stop];

  EXPECT_OCMOCK_VERIFY(shared_application_mock);
}
