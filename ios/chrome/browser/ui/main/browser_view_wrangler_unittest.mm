// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class BrowserViewWranglerTest : public PlatformTest {
 protected:
  BrowserViewWranglerTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

TEST_F(BrowserViewWranglerTest, TestInitNilObserver) {
  // |task_environment_| must outlive all objects created by BVC, because those
  // objects may rely on threading API in dealloc.
  @autoreleasepool {
    BrowserViewWrangler* wrangler = [[BrowserViewWrangler alloc]
              initWithBrowserState:chrome_browser_state_.get()
              webStateListObserver:nil
        applicationCommandEndpoint:(id<ApplicationCommands>)nil
              appURLLoadingService:nil
                   storageSwitcher:nil];
    [wrangler createMainBrowser];
    // Test that BVC is created on demand.
    BrowserViewController* bvc = wrangler.mainInterface.bvc;
    EXPECT_NE(bvc, nil);

    // Test that once created the BVC isn't re-created.
    EXPECT_EQ(bvc, wrangler.mainInterface.bvc);

    // Test that the OTR objects are (a) OTR and (b) not the same as the non-OTR
    // objects.
    EXPECT_NE(bvc, wrangler.incognitoInterface.bvc);
    EXPECT_NE(wrangler.mainInterface.tabModel,
              wrangler.incognitoInterface.tabModel);
    EXPECT_TRUE(wrangler.incognitoInterface.browserState->IsOffTheRecord());

    [wrangler shutdown];
  }
}

}  // namespace
