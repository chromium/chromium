// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/test_modality/test_contained_overlay_coordinator.h"

#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/test_modality/test_contained_overlay_request_config.h"
#include "ios/chrome/browser/ui/overlays/test/fake_overlay_request_coordinator_delegate.h"
#include "ios/chrome/test/scoped_key_window.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for TestContainedOverlayCoordinator.
class TestContainedOverlayCoordinatorTest : public PlatformTest {
 public:
  TestContainedOverlayCoordinatorTest()
      : root_view_controller_([[UIViewController alloc] init]),
        request_(OverlayRequest::CreateWithConfig<TestContainedOverlay>()),
        coordinator_([[TestContainedOverlayCoordinator alloc]
            initWithBaseViewController:root_view_controller_
                               browser:&browser_
                               request:request_.get()
                              delegate:&delegate_]) {
    scoped_window_.Get().rootViewController = root_view_controller_;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  TestBrowser browser_;
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
