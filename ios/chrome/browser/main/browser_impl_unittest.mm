// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_impl.h"

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/fake_browser_observer.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class BrowserImplTest : public PlatformTest {
 protected:
  BrowserImplTest()
      : web_state_list_(
            std::make_unique<WebStateList>(&web_state_list_delegate_)) {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;

  FakeWebStateListDelegate web_state_list_delegate_;
  // Unique ptr to the web_state_list_ to transfer into new Browser instances.
  std::unique_ptr<WebStateList> web_state_list_;
};

// Tests that the accessors return the expected values.
TEST_F(BrowserImplTest, TestAccessors) {
  WebStateList* web_state_list_weak_reference = web_state_list_.get();
  BrowserImpl browser(chrome_browser_state_.get(), std::move(web_state_list_));

  EXPECT_EQ(chrome_browser_state_.get(), browser.GetBrowserState());
  EXPECT_EQ(web_state_list_weak_reference, browser.GetWebStateList());
}

// Tests that the BrowserDestroyed() callback is sent when a browser is deleted.
TEST_F(BrowserImplTest, BrowserDestroyed) {
  std::unique_ptr<Browser> browser =
      std::make_unique<BrowserImpl>(chrome_browser_state_.get());
  FakeBrowserObserver observer(browser.get());
  browser = nullptr;
  EXPECT_TRUE(observer.browser_destroyed());
}
