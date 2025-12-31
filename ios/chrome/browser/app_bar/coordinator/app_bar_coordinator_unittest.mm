// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/coordinator/app_bar_coordinator.h"

#import <memory>

#import "ios/chrome/browser/app_bar/ui/app_bar_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class AppBarCoordinatorTest : public PlatformTest {
 protected:
  AppBarCoordinatorTest() {
    regular_profile_ = TestProfileIOS::Builder().Build();
    incognito_profile_ = TestProfileIOS::Builder().Build();
    regular_browser_ = std::make_unique<TestBrowser>(regular_profile_.get());
    incognito_browser_ =
        std::make_unique<TestBrowser>(incognito_profile_.get());
    coordinator_ = [[AppBarCoordinator alloc]
        initWithRegularBrowser:regular_browser_.get()
              incognitoBrowser:incognito_browser_.get()];
    application_handler_ = OCMProtocolMock(@protocol(ApplicationCommands));
    [regular_browser_->GetCommandDispatcher()
        startDispatchingToTarget:application_handler_
                     forProtocol:@protocol(ApplicationCommands)];
  }

  ~AppBarCoordinatorTest() override { [coordinator_ stop]; }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> regular_profile_;
  std::unique_ptr<TestProfileIOS> incognito_profile_;
  std::unique_ptr<TestBrowser> regular_browser_;
  std::unique_ptr<TestBrowser> incognito_browser_;
  AppBarCoordinator* coordinator_;
  id application_handler_;
};

// Tests that the coordinator creates a view controller when started.
TEST_F(AppBarCoordinatorTest, TestStart) {
  [coordinator_ start];
  EXPECT_TRUE(coordinator_.viewController);
  EXPECT_TRUE(
      [coordinator_.viewController isKindOfClass:[AppBarViewController class]]);
}
