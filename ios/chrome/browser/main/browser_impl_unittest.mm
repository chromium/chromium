// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_impl.h"

#import "ios/chrome/browser/shared/model/browser/test/fake_browser_observer.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

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
  EXPECT_FALSE(browser.IsInactive());
  EXPECT_EQ(browser.GetActiveBrowser(), &browser);
  EXPECT_EQ(browser.GetInactiveBrowser(), nullptr);

  // Check that the inactive and the regular Browsers are correctly referencing
  // each other after creating the inactive Browser.
  Browser* inactive_browser = browser.CreateInactiveBrowser();
  EXPECT_TRUE(inactive_browser->IsInactive());
  EXPECT_EQ(inactive_browser->GetActiveBrowser(), &browser);
  EXPECT_EQ(inactive_browser->GetInactiveBrowser(), inactive_browser);
  EXPECT_FALSE(browser.IsInactive());
  EXPECT_EQ(browser.GetActiveBrowser(), &browser);
  EXPECT_EQ(browser.GetInactiveBrowser(), inactive_browser);

  // Check that destroying the inactive browser resets the reference to the
  // inactive Browser.
  browser.DestroyInactiveBrowser();
  EXPECT_FALSE(browser.IsInactive());
  EXPECT_EQ(browser.GetActiveBrowser(), &browser);
  EXPECT_EQ(browser.GetInactiveBrowser(), nullptr);
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

// Tests that the BrowserDestroyed() callback is sent when destroying the
// inactive Browser.
TEST_F(BrowserImplTest, InactiveBrowserDestroyed) {
  BrowserImpl browser(chrome_browser_state_.get());
  Browser* inactive_browser = browser.CreateInactiveBrowser();
  std::unique_ptr<FakeBrowserObserver> observer =
      std::make_unique<FakeBrowserObserver>(inactive_browser);
  browser.DestroyInactiveBrowser();
  ASSERT_TRUE(observer);
  EXPECT_TRUE(observer->browser_destroyed());
}
