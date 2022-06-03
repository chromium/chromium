// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_browser_observer.h"

#include "base/test/task_environment.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_mediator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_state_list_observer.h"
#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests FullscreenBrowserObserver functionality.
class FullscreenBrowserObserverTest : public PlatformTest {
 public:
  FullscreenBrowserObserverTest()
      : browser_(std::make_unique<TestBrowser>()),
        controller_(&model_),
        mediator_(&controller_, &model_),
        web_state_list_observer_(&controller_, &model_, &mediator_),
        browser_observer_(&web_state_list_observer_, browser_.get()) {
    web_state_list_observer_.SetWebStateList(browser_.get()->GetWebStateList());
  }

  ~FullscreenBrowserObserverTest() override { mediator_.Disconnect(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestBrowser> browser_;
  TestFullscreenController controller_;
  FullscreenModel model_;
  FullscreenMediator mediator_;
  FullscreenWebStateListObserver web_state_list_observer_;
  FullscreenBrowserObserver browser_observer_;
};

// Tests that FullscreenBrowserObserver resets the FullscreenController's
// WebStateList.
TEST_F(FullscreenBrowserObserverTest, BrowserDestroyed) {
  EXPECT_TRUE(web_state_list_observer_.GetWebStateList());
  browser_ = nullptr;
  EXPECT_FALSE(web_state_list_observer_.GetWebStateList());
}
