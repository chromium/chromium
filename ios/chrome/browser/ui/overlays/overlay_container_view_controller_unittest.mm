// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_container_view_controller.h"

#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/test_modality/test_presented_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/test_modality/test_resizing_presented_overlay_request_config.h"
#import "ios/chrome/browser/ui/overlays/overlay_presentation_context_impl.h"
#include "ios/chrome/browser/ui/overlays/test/fake_overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/ui/overlays/test_modality/test_presented_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/test_modality/test_resizing_presented_overlay_coordinator.h"
#include "ios/chrome/test/scoped_key_window.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for OverlayContainerViewController.
class OverlayContainerViewControllerTest : public PlatformTest {
 public:
  OverlayContainerViewControllerTest()
      : root_view_controller_([[UIViewController alloc] init]),
        delegate_(OCMStrictProtocolMock(
            @protocol(OverlayContainerViewControllerDelegate))),
        view_controller_([[OverlayContainerViewController alloc] init]) {
    scoped_window_.Get().rootViewController = root_view_controller_;
    view_controller_.delegate = delegate_;
  }
  ~OverlayContainerViewControllerTest() override {
    EXPECT_OCMOCK_VERIFY(delegate_);
  }

 protected:
  ScopedKeyWindow scoped_window_;
  UIViewController* root_view_controller_ = nil;
  id<OverlayContainerViewControllerDelegate> delegate_ = nil;
  OverlayContainerViewController* view_controller_ = nil;
};

// Verifies that the view controller notifies its delegate when its view's
// window is changed.
TEST_F(OverlayContainerViewControllerTest, MoveToWindow) {
  OCMExpect([delegate_ containerViewController:view_controller_
                               didMoveToWindow:scoped_window_.Get()]);
  [root_view_controller_.view addSubview:view_controller_.view];

  OCMExpect([delegate_ containerViewController:view_controller_
                               didMoveToWindow:nil]);
  [view_controller_.view removeFromSuperview];
}

// Verifies that the container view ignores touches that fall outside of any
// subviews.
TEST_F(OverlayContainerViewControllerTest, TouchHandling) {
  // The container view will be laid out with |frame|.  A subview will be added
  // to the view and laid out |subview_inset| from the edges of the view.
  CGRect frame = CGRectMake(0.0, 0.0, 100.0, 100.0);
  CGFloat subview_inset = 25.0;
  CGRect subview_frame = CGRectInset(frame, subview_inset, subview_inset);

  // |center| is the center of the container view, which will land in the center
  // of the added subview.  |corner| is a point halfway between the container
  // view's origin and the subview's origin.
  CGPoint center = CGPointMake(CGRectGetMidX(frame), CGRectGetMidY(frame));
  CGPoint corner = CGPointMake(subview_inset / 2.0, subview_inset / 2.0);

  // Set up the view hierarchy using the calculated frames.
  UIView* view = view_controller_.view;
  view.frame = frame;
  UIView* subview = [[UIView alloc] initWithFrame:subview_frame];
  [view addSubview:subview];

  // Verify that touches are ignored when they fall outside of any subview.
  EXPECT_FALSE([view hitTest:corner withEvent:nil]);
  EXPECT_FALSE([view pointInside:corner withEvent:nil]);

  // Verify that touches are handled and forwarded to |subview| when falling
  // inside |subview|'s bounds.
  EXPECT_EQ([view hitTest:center withEvent:nil], subview);
  EXPECT_TRUE([view pointInside:center withEvent:nil]);

  // Remove the subview and verify that touches in the center are ignored.
  [subview removeFromSuperview];
  EXPECT_FALSE([view hitTest:center withEvent:nil]);
  EXPECT_FALSE([view pointInside:center withEvent:nil]);
}
