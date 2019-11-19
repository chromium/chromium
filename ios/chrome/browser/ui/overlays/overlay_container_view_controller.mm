// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_container_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OverlayContainerView : UIView
// The owning view controller.
@property(nonatomic, weak) OverlayContainerViewController* viewController;
@end

@implementation OverlayContainerView

- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
  // Allow touches to go to subviews, but ignore touches that are routed to the
  // container view itself.
  UIView* hitView = [super hitTest:point withEvent:event];
  return hitView == self ? nil : hitView;
}

- (void)didMoveToWindow {
  [super didMoveToWindow];
  [self.viewController.delegate containerViewController:self.viewController
                                        didMoveToWindow:self.window];
}

@end

@implementation OverlayContainerViewController

- (void)loadView {
  OverlayContainerView* containerView = [[OverlayContainerView alloc] init];
  containerView.viewController = self;
  self.view = containerView;
}

@end
