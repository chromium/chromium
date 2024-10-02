// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_main_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test fixture for the PrivacyGuideMainCoordinator.
class PrivacyGuideMainCoordinatorTest : public PlatformTest {
 protected:
  PrivacyGuideMainCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    root_view_controller_ = [[UIViewController alloc] init];
    scoped_window_.Get().rootViewController = root_view_controller_;

    coordinator_ = [[PrivacyGuideMainCoordinator alloc]
        initWithBaseViewController:root_view_controller_
                           browser:browser_.get()];
    [coordinator_ start];
  }

  ~PrivacyGuideMainCoordinatorTest() override { [coordinator_ stop]; }

  bool IsPrivacyGuidePresented() {
    return [root_view_controller_.presentedViewController
        isKindOfClass:[UINavigationController class]];
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* root_view_controller_;
  PrivacyGuideMainCoordinator* coordinator_;
  ScopedKeyWindow scoped_window_;
};

// Tests that the Privacy Guide correctly sets up its own navigation controller.
TEST_F(PrivacyGuideMainCoordinatorTest, PrivacyGuidePresented) {
  ASSERT_TRUE(IsPrivacyGuidePresented());
}
