// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include <memory>

#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/ui/fancy_ui/primary_action_button.h"
#import "ios/chrome/browser/ui/first_run/welcome_to_chrome_view.h"
#import "ios/chrome/browser/ui/first_run/welcome_to_chrome_view_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WelcomeToChromeView (ExposedForTesting)
@property(nonatomic, retain, readonly) UIButton* checkBoxButton;
- (void)checkBoxButtonWasTapped;
@end

namespace {

class WelcomeToChromeViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    WebStateList* web_state_list = nullptr;
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get(),
                                             web_state_list);
    controller_ =
        [[WelcomeToChromeViewController alloc] initWithBrowser:browser_.get()
                                                     presenter:nil
                                                    dispatcher:nil];
    [controller_ loadView];
  }

  void TearDown() override {
    controller_ = nil;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  WelcomeToChromeViewController* controller_;
};

TEST_F(WelcomeToChromeViewControllerTest, TestDefaultStatsCheckBoxValue) {
  BOOL checkbox_value =
      [WelcomeToChromeViewController defaultStatsCheckboxValue];
  ASSERT_TRUE(checkbox_value);
}

TEST_F(WelcomeToChromeViewControllerTest, TestConstructorDestructor) {
  EXPECT_TRUE(controller_);
  EXPECT_TRUE([controller_ view]);
}

TEST_F(WelcomeToChromeViewControllerTest, TestToggleCheckbox) {
  WelcomeToChromeView* welcome_view =
      static_cast<WelcomeToChromeView*>([controller_ view]);
  EXPECT_TRUE(welcome_view.checkBoxButton.selected);
  [welcome_view checkBoxButtonWasTapped];
  EXPECT_FALSE(welcome_view.checkBoxButton.selected);
  [welcome_view checkBoxButtonWasTapped];
  EXPECT_TRUE(welcome_view.checkBoxButton.selected);
}

}  // namespace
