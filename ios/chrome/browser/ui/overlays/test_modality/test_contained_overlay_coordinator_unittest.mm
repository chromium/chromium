// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/test_modality/test_contained_overlay_coordinator.h"

#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/test_modality/test_contained_overlay_request_config.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/overlays/test/fake_overlay_request_coordinator_delegate.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Test fixture for TestContainedOverlayCoordinator.
class TestContainedOverlayCoordinatorTest : public PlatformTest {
 public:
  TestContainedOverlayCoordinatorTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    root_view_controller_ = [[UIViewController alloc] init];
    request_ = OverlayRequest::CreateWithConfig<TestContainedOverlay>();
    coordinator_ = [[TestContainedOverlayCoordinator alloc]
        initWithBaseViewController:root_view_controller_
                           browser:browser_.get()
                           request:request_.get()
                          delegate:&delegate_];
    scoped_window_.Get().rootViewController = root_view_controller_;
  }

  ~TestContainedOverlayCoordinatorTest() override { [coordinator_ stop]; }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  ScopedKeyWindow scoped_window_;
  UIViewController* root_view_controller_ = nil;
  std::unique_ptr<OverlayRequest> request_;
  FakeOverlayRequestCoordinatorDelegate delegate_;
  TestContainedOverlayCoordinator* coordinator_ = nil;
};

// Tests that the coordinator sets up its view correctly.
TEST_F(TestContainedOverlayCoordinatorTest, ViewSetup) {
  [coordinator_ startAnimated:NO];
  EXPECT_EQ(coordinator_.viewController.view.superview,
            root_view_controller_.view);
  EXPECT_TRUE(delegate_.HasUIBeenPresented(request_.get()));

  [coordinator_ stopAnimated:NO];
  EXPECT_TRUE(delegate_.HasUIBeenDismissed(request_.get()));
}
