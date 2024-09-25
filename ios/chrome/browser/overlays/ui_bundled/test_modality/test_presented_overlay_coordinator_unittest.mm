// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/test_modality/test_presented_overlay_coordinator.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/test_modality/test_presented_overlay_request_config.h"
#import "ios/chrome/browser/overlays/ui_bundled/test/fake_overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForUIElementTimeout;

// Test fixture for TestPresentedOverlayCoordinator.
class TestPresentedOverlayCoordinatorTest : public PlatformTest {
 public:
  TestPresentedOverlayCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    root_view_controller_ = [[UIViewController alloc] init];
    request_ = OverlayRequest::CreateWithConfig<TestPresentedOverlay>();
    coordinator_ = [[TestPresentedOverlayCoordinator alloc]
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
  TestPresentedOverlayCoordinator* coordinator_ = nil;
};

// Tests that the coordinator sets up its view correctly.
TEST_F(TestPresentedOverlayCoordinatorTest, ViewSetup) {
  // Start the coordinator and wait until the view is finished being presented.
  // This is necessary because UIViewController presentation is asynchronous,
  // even when performed without animation.
  [coordinator_ startAnimated:NO];
  UIViewController* view_controller = coordinator_.viewController;
  ASSERT_TRUE(view_controller);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^BOOL {
    return view_controller.presentingViewController &&
           !view_controller.beingPresented;
  }));
  EXPECT_TRUE(delegate_.HasUIBeenPresented(request_.get()));

  // Verify that the view hierarchy is set up properly.  UIViewControllers
  // presented with UIModalPresentationOverCurrentContext inserts its
  // presentation container view in front of the presenting view controller's
  // view.
  UIView* superview = root_view_controller_.view.superview;
  UIView* presentation_container_view =
      view_controller.presentationController.containerView;
  EXPECT_EQ(superview, presentation_container_view.superview);
  NSArray* siblings = superview.subviews;
  EXPECT_EQ([siblings indexOfObject:presentation_container_view],
            [siblings indexOfObject:root_view_controller_.view] + 1);

  // Stop the coordinator and wait for the dismissal to finish, verifying that
  // the delegate was notified.
  [coordinator_ stopAnimated:NO];
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^BOOL {
    return !root_view_controller_.presentedViewController;
  }));
  EXPECT_TRUE(delegate_.HasUIBeenDismissed(request_.get()));
}
