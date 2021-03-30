// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/browser_coordinator.h"

#include "base/files/file_util.h"
#include "ios/chrome/browser/download/download_directory_util.h"
#import "ios/chrome/browser/download/external_app_util.h"
#include "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
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

@interface BrowserCoordinator (Testing) <ActivityServiceCommands>
@end

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

// Tests that -sharePage is leaving fullscreena and starting the share
// coordinator.
TEST_F(BrowserCoordinatorTest, SharePage) {
  FullscreenModel model;
  std::unique_ptr<TestFullscreenController> controller =
      std::make_unique<TestFullscreenController>(&model);
  TestFullscreenController* controller_ptr = controller.get();

  browser_->SetUserData(TestFullscreenController::UserDataKeyForTesting(),
                        std::move(controller));

  controller_ptr->EnterFullscreen();
  ASSERT_EQ(0.0, controller_ptr->GetProgress());

  id classMock = OCMClassMock([SharingCoordinator class]);
  SharingCoordinator* mockSharingCoordinator = classMock;
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                initWithBaseViewController:[OCMArg any]
                                   browser:browser_.get()
                                    params:[OCMArg any]
                                originView:[OCMArg any]
                                originRect:CGRectZero])
      .andReturn(mockSharingCoordinator);
  OCMExpect([mockSharingCoordinator start]);

  BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();
  [browser_coordinator sharePage];

  // Check that fullscreen is exited.
  EXPECT_EQ(1.0, controller_ptr->GetProgress());
  // Check that -start has been called.
  EXPECT_OCMOCK_VERIFY(classMock);
}
