// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_presentation_context_coordinator.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/test_modality/test_presented_overlay_request_config.h"
#import "ios/chrome/browser/ui/overlays/overlay_presentation_context_impl.h"
#import "ios/chrome/browser/ui/overlays/overlay_presentation_context_util.h"
#include "ios/chrome/browser/ui/overlays/test/fake_overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/ui/overlays/test/test_overlay_presentation_context.h"
#include "ios/chrome/test/scoped_key_window.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForUIElementTimeout;

// Test fixture for OverlayPresentationContextCoordinator.
class OverlayPresentationContextCoordinatorTest : public PlatformTest {
 public:
  OverlayPresentationContextCoordinatorTest()
      : browser_(std::make_unique<TestBrowser>()),
        context_(browser_.get()),
        root_view_controller_([[UIViewController alloc] init]),
        coordinator_([[OverlayPresentationContextCoordinator alloc]
            initWithBaseViewController:root_view_controller_
                               browser:browser_.get()
                   presentationContext:&context_]) {
    root_view_controller_.definesPresentationContext = YES;
    scoped_window_.Get().rootViewController = root_view_controller_;
  }
  ~OverlayPresentationContextCoordinatorTest() override {
    // The browser needs to be destroyed before |context_| so that observers
    // can be unhooked due to BrowserDestroyed().  This is not a problem for
    // non-test OverlayPresentationContextImpls since they're owned by the
    // Browser and get destroyed after BrowserDestroyed() is called.
    browser_ = nullptr;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowser> browser_;
  TestOverlayPresentationContext context_;
  ScopedKeyWindow scoped_window_;
  UIViewController* root_view_controller_ = nil;
  OverlayPresentationContextCoordinator* coordinator_ = nil;
};

// Tests that the coordinator updates its OverlayPresentationContext's
// presentation capabilities when started and stopped.
TEST_F(OverlayPresentationContextCoordinatorTest,
       UpdatePresentationCapabilities) {
  ASSERT_FALSE(OverlayPresentationContextSupportsPresentedUI(&context_));

  // Start the coordinator and wait until the view is finished being presented.
  // This is necessary because UIViewController presentation is asynchronous,
  // even when performed without animation.
  [coordinator_ start];
  UIViewController* view_controller = coordinator_.viewController;
  ASSERT_TRUE(view_controller);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return view_controller.presentingViewController &&
           !view_controller.beingPresented;
  }));

  // Verify that the presentation context supports presentation.
  EXPECT_TRUE(OverlayPresentationContextSupportsPresentedUI(&context_));

  // Stop the coordinator and wait until the view is finished being dismissed.
  // This is necessary because UIViewController presentation is asynchronous,
  // even when performed without animation.
  [coordinator_ stop];
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return !view_controller.presentingViewController;
  }));

  // Verify that the presentation context no longer supports presentation.
  EXPECT_FALSE(OverlayPresentationContextSupportsPresentedUI(&context_));
}
