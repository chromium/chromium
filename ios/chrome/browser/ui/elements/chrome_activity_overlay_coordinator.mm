// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/elements/chrome_activity_overlay_coordinator.h"

#import "ios/chrome/browser/ui/elements/chrome_activity_overlay_view_controller.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ChromeActivityOverlayCoordinator ()
// View controller that displays a native active indicator.
@property(nonatomic, strong)
    ChromeActivityOverlayViewController* chromeActivityOverlayViewController;
@end

@implementation ChromeActivityOverlayCoordinator

- (void)start {
  if (self.chromeActivityOverlayViewController || self.started)
    return;

  self.chromeActivityOverlayViewController =
      [[ChromeActivityOverlayViewController alloc] init];

  self.chromeActivityOverlayViewController.messageText = self.messageText;
  [self.baseViewController
      addChildViewController:self.chromeActivityOverlayViewController];
  // Make sure frame of view is exactly the same size as its presenting view.
  // Especially important when the presenting view is a bubble.
  CGRect frame = self.chromeActivityOverlayViewController.view.frame;
  frame.origin.x = 0;
  frame.size.width = self.baseViewController.view.bounds.size.width;
  frame.size.height = self.baseViewController.view.bounds.size.height;
  self.chromeActivityOverlayViewController.view.frame = frame;
  [self.baseViewController.view
      addSubview:self.chromeActivityOverlayViewController.view];
  [self.chromeActivityOverlayViewController
      didMoveToParentViewController:self.baseViewController];
  self.started = YES;
}

- (void)stop {
  if (!self.chromeActivityOverlayViewController || !self.started)
    return;
  [self.chromeActivityOverlayViewController willMoveToParentViewController:nil];
  [self.chromeActivityOverlayViewController.view removeFromSuperview];
  [self.chromeActivityOverlayViewController removeFromParentViewController];
  self.chromeActivityOverlayViewController = nil;
  self.started = NO;
}

@end
