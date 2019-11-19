// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/browser_container_coordinator.h"

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_alert_overlay.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for BrowserContainerCoordinator.
class BrowserContainerCoordinatorTest : public PlatformTest {
 protected:
  BrowserContainerCoordinatorTest()
      : web_state_list_(&web_state_list_delegate_),
        base_view_controller_([[UIViewController alloc] init]) {
    // Create the Browser and coordinator.
    TestChromeBrowserState::Builder test_browser_state_builder;
    browser_state_ = test_browser_state_builder.Build();
    browser_ =
        std::make_unique<TestBrowser>(browser_state_.get(), &web_state_list_);
    coordinator_ = [[BrowserContainerCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];

    // Add a WebState to the Browser.
    web_state_list_.InsertWebState(0, std::make_unique<web::TestWebState>(),
                                   WebStateList::INSERT_ACTIVATE,
                                   WebStateOpener());

    // Set up the view hierarchy so that overlay presentation can occur.
    scoped_window_.Get().rootViewController = base_view_controller_;
    [coordinator_ start];
    [base_view_controller_ addChildViewController:coordinator_.viewController];
    [base_view_controller_.view addSubview:coordinator_.viewController.view];
    AddSameConstraints(base_view_controller_.view,
                       coordinator_.viewController.view);
    [coordinator_.viewController
        didMoveToParentViewController:base_view_controller_];
  }
  ~BrowserContainerCoordinatorTest() override { [coordinator_ stop]; }

  // Accessors:
  FullscreenController* fullscreen_controller() {
    return FullscreenControllerFactory::GetForBrowserState(
        browser_state_.get());
  }
  web::WebState* active_web_state() {
    return web_state_list_.GetActiveWebState();
  }

  web::WebTaskEnvironment task_environment_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  ScopedKeyWindow scoped_window_;
  UIViewController* base_view_controller_;
  BrowserContainerCoordinator* coordinator_;
};

// Tests that the FullscreenController is disabled when an overlay is shown over
// OverlayModality::kWebContentArea.
TEST_F(BrowserContainerCoordinatorTest, DisableFullscreenForWebContentOverlay) {
  ASSERT_TRUE(fullscreen_controller()->IsEnabled());
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      active_web_state(), OverlayModality::kWebContentArea);
  const GURL kUrl("http://chromium.test");
  JavaScriptDialogSource source(active_web_state(), kUrl, true);
  const std::string kMessage("message");
  queue->AddRequest(
      OverlayRequest::CreateWithConfig<JavaScriptAlertOverlayRequestConfig>(
          source, kMessage));
  ASSERT_FALSE(fullscreen_controller()->IsEnabled());
}
