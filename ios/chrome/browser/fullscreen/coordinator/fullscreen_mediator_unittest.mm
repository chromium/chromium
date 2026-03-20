// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/coordinator/fullscreen_mediator.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/platform_test.h"

// Test fixture for testing FullscreenMediator class.
class FullscreenMediatorTest : public PlatformTest {
 protected:
  FullscreenMediatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    FullscreenBrowserAgent::CreateForBrowser(browser_.get());
    mediator_ = [[FullscreenMediator alloc]
        initWithBrowserAgent:FullscreenBrowserAgent::FromBrowser(
                                 browser_.get())];
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  FullscreenMediator* mediator_;
};

// Tests that the mediator can be disconnected.
TEST_F(FullscreenMediatorTest, Disconnect) {
  [mediator_ disconnect];
}
