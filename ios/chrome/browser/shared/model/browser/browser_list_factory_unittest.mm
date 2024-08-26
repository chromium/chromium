// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"

#import <memory>

#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class BrowserListFactoryTest : public PlatformTest {
 public:
  BrowserListFactoryTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
  }

  ChromeBrowserState* browser_state() { return browser_state_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
};

// Tests that the factory returns a non-null instance.
TEST_F(BrowserListFactoryTest, GetForBrowserState) {
  EXPECT_TRUE(BrowserListFactory::GetForBrowserState(browser_state()));
}

// Tests that the factory returns the same instance for regular and
// off-the-record ChromeBrowserState.
TEST_F(BrowserListFactoryTest, GetForBrowserState_OffTheRecord) {
  EXPECT_EQ(BrowserListFactory::GetForBrowserState(browser_state()),
            BrowserListFactory::GetForBrowserState(
                browser_state()->GetOffTheRecordChromeBrowserState()));
}
