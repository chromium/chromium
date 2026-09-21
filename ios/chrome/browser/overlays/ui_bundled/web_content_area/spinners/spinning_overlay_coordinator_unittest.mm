// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/web_content_area/spinners/spinning_overlay_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/spinning_overlay_request_config.h"
#import "ios/chrome/browser/overlays/ui_bundled/test/fake_overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/overlays/ui_bundled/web_content_area/spinners/spinning_overlay_view.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

NSString* const kLabelText = @"Loading...";

// Creates a SpinningOverlay request for use in tests.
std::unique_ptr<OverlayRequest> CreateSpinningOverlayRequest(
    NSString* label_text,
    bool cancellable) {
  return OverlayRequest::CreateWithConfig<SpinningOverlayRequestConfig>(
      label_text, cancellable);
}

}  // namespace

class SpinningOverlayCoordinatorTest : public PlatformTest {
 public:
  SpinningOverlayCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    root_view_controller_ = [[UIViewController alloc] init];
    request_ = CreateSpinningOverlayRequest(kLabelText, true);
    coordinator_ = [[SpinningOverlayCoordinator alloc]
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
  SpinningOverlayCoordinator* coordinator_ = nil;
};

// Tests that the coordinator creates an overlay view, adds it to the base
// UIViewController's hierarchy, and correctly notifies the delegate on start
// and stop without animation.
TEST_F(SpinningOverlayCoordinatorTest, ViewSetup) {
  ASSERT_FALSE(delegate_.HasUIBeenPresented(request_.get()));
  [coordinator_ startAnimated:NO];
  UIViewController* view_controller = coordinator_.viewController;
  ASSERT_TRUE(view_controller);
  EXPECT_EQ(view_controller.parentViewController, root_view_controller_);
  EXPECT_TRUE(
      [view_controller.view isDescendantOfView:root_view_controller_.view]);
  EXPECT_TRUE(delegate_.HasUIBeenPresented(request_.get()));

  [coordinator_ stopAnimated:NO];
  EXPECT_TRUE(delegate_.HasUIBeenDismissed(request_.get()));
}

// Tests that the coordinator presents and dismisses properly with animation.
TEST_F(SpinningOverlayCoordinatorTest, ViewSetupAnimated) {
  ASSERT_FALSE(delegate_.HasUIBeenPresented(request_.get()));
  [coordinator_ startAnimated:YES];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return delegate_.HasUIBeenPresented(request_.get());
      }));

  [coordinator_ stopAnimated:YES];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return delegate_.HasUIBeenDismissed(request_.get());
      }));
}
