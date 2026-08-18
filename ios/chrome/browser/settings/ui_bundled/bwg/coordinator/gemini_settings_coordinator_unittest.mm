// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/gemini_settings_coordinator.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/gemini_settings_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_navigation_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class GeminiSettingsCoordinatorTest : public PlatformTest {
 protected:
  GeminiSettingsCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

// Tests that settingsViewControllerDidRequestDismissal: calls
// closeSettings on SettingsNavigationController.
TEST_F(GeminiSettingsCoordinatorTest,
       SettingsViewControllerDidRequestDismissal) {
  id base_navigation_controller =
      OCMPartialMock([[SettingsNavigationController alloc] init]);

  GeminiSettingsCoordinator* coordinator = [[GeminiSettingsCoordinator alloc]
      initWithBaseNavigationController:base_navigation_controller
                               browser:browser_.get()];

  OCMExpect([base_navigation_controller closeSettings]);

  id<GeminiSettingsDismissalDelegate> dismissalDelegate =
      static_cast<id<GeminiSettingsDismissalDelegate>>(coordinator);
  [dismissalDelegate settingsViewControllerDidRequestDismissal:nil];

  EXPECT_OCMOCK_VERIFY(base_navigation_controller);
}
