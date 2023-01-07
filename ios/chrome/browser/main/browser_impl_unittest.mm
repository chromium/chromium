// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_impl.h"

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/fake_browser_observer.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class BrowserImplTest : public PlatformTest {
 protected:
  BrowserImplTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

// Tests that the accessors return the expected values.
TEST_F(BrowserImplTest, TestAccessors) {
  BrowserImpl browser(chrome_browser_state_.get());
  EXPECT_EQ(chrome_browser_state_.get(), browser.GetBrowserState());
  EXPECT_TRUE(browser.GetWebStateList());
  EXPECT_TRUE(browser.GetCommandDispatcher());
}

// Tests that the BrowserDestroyed() callback is sent when a browser is deleted.
TEST_F(BrowserImplTest, BrowserDestroyed) {
  std::unique_ptr<FakeBrowserObserver> observer;
  {
    BrowserImpl browser(chrome_browser_state_.get());
    observer = std::make_unique<FakeBrowserObserver>(&browser);
  }
  ASSERT_TRUE(observer);
  EXPECT_TRUE(observer->browser_destroyed());
}
