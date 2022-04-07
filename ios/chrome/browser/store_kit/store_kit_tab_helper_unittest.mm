// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/store_kit/store_kit_tab_helper.h"

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class StoreKitTabHelperTest : public PlatformTest {
 public:
  StoreKitTabHelperTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();

    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);
  }

 protected:
  web::WebState* web_state() { return web_state_.get(); }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
};

TEST_F(StoreKitTabHelperTest, Constructor) {
  StoreKitTabHelper::CreateForWebState(web_state());
  StoreKitTabHelper* tab_helper = StoreKitTabHelper::FromWebState(web_state());
  ASSERT_TRUE(tab_helper);

  id mock_launcher =
      [OCMockObject niceMockForProtocol:@protocol(StoreKitLauncher)];
  // Verifies that GetLauncher returns the mock launcher object that was set.
  tab_helper->SetLauncher(mock_launcher);
  EXPECT_EQ(mock_launcher, tab_helper->GetLauncher());
}
