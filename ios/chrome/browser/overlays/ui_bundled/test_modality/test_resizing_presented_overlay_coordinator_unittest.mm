// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/test_modality/test_resizing_presented_overlay_coordinator.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/test_modality/test_resizing_presented_overlay_request_config.h"
#import "ios/chrome/browser/overlays/ui_bundled/test/fake_overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForUIElementTimeout;

namespace {
// The frame of the overlay UI presentation container view in window
// coordinates.
const CGRect kWindowFrame = {{100.0, 100.0}, {100.0, 100.0}};
}  // namespace

// Test fixture for TestResizingPresentedOverlayCoordinator.
class TestResizingPresentedOverlayCoordinatorTest : public PlatformTest {
 public:
  TestResizingPresentedOverlayCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    root_view_controller_ = [[UIViewController alloc] init];
    request_ = OverlayRequest::CreateWithConfig<TestResizingPresentedOverlay>(
        kWindowFrame);
    coordinator_ = [[TestResizingPresentedOverlayCoordinator alloc]
        initWithBaseViewController:root_view_controller_
                           browser:browser_.get()
                           request:request_.get()
                          delegate:&delegate_];
    scoped_window_.Get().rootViewController = root_view_controller_;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  ScopedKeyWindow scoped_window_;
  UIViewController* root_view_controller_ = nil;
  std::unique_ptr<OverlayRequest> request_;
  FakeOverlayRequestCoordinatorDelegate delegate_;
  TestResizingPresentedOverlayCoordinator* coordinator_ = nil;
};

// Tests that the coordinator sets up its view correctly.
TEST_F(TestResizingPresentedOverlayCoordinatorTest, ViewSetup) {
  // Start the coordinator and wait until the view is finished being presented.
  // This is necessary because UIViewController presentation is asynchronous,
  // even when performed without animation.
  [coordinator_ startAnimated:NO];
  UIViewController* view_controller = coordinator_.viewController;
  ASSERT_TRUE(view_controller);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return view_controller.presentingViewController &&
           !view_controller.beingPresented;
  }));

  // Verify that `coordinator_`'s presentation container view is resized to
  // kWindowFrame.
  UIView* container_view = view_controller.presentationController.containerView;
  CGRect container_view_frame =
      [container_view convertRect:container_view.bounds toView:nil];
  EXPECT_TRUE(CGRectEqualToRect(container_view_frame, kWindowFrame));

  // Stop the coordinator and wait for the dismissal to finish.
  [coordinator_ stopAnimated:NO];
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return !view_controller.presentingViewController;
  }));
}
