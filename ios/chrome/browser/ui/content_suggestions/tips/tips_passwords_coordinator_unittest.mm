// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser//ui/content_suggestions/tips/tips_passwords_coordinator.h"

#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Fixture to test `TipsPasswordsCoordinator`.
class TipsPasswordsCoordinatorTest : public PlatformTest {
 protected:
  TipsPasswordsCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:view_controller_];
  }

  void TearDown() override {
    [coordinator_ stop];
    PlatformTest::TearDown();
  }

  TipsPasswordsCoordinator* CreateCoordinator(
      segmentation_platform::TipIdentifier identifier) {
    coordinator_ = [[TipsPasswordsCoordinator alloc]
        initWithBaseViewController:view_controller_
                           browser:browser_.get()
                        identifier:identifier];
    coordinator_.delegate = delegate_;
    return coordinator_;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  TipsPasswordsCoordinator* coordinator_;
  id delegate_ = OCMProtocolMock(@protocol(TipsPasswordsCoordinatorDelegate));
  ScopedKeyWindow scoped_key_window_;
  UIViewController* view_controller_;
};

#pragma mark - Tests.

// Tests that the coordinator can be started and stopped.
TEST_F(TipsPasswordsCoordinatorTest, StartStop) {
  CreateCoordinator(segmentation_platform::TipIdentifier::kSavePasswords);
  [coordinator_ start];
  // Stopping the coordinator should not crash.
}

// Tests that the coordinator calls the delegate's
// `tipsPasswordsCoordinatorDidFinish:`.
TEST_F(TipsPasswordsCoordinatorTest, DelegateCall) {
  CreateCoordinator(segmentation_platform::TipIdentifier::kSavePasswords);
  [[delegate_ expect] tipsPasswordsCoordinatorDidFinish:coordinator_];
  [coordinator_ start];
  [coordinator_ confirmationAlertPrimaryAction];
  EXPECT_OCMOCK_VERIFY(delegate_);
}

// Tests that creating the coordinator with an unknown tip identifier does not
// crash.
TEST_F(TipsPasswordsCoordinatorTest, UnknownIdentifier) {
  CreateCoordinator(segmentation_platform::TipIdentifier::kUnknown);
  [coordinator_ start];
}
