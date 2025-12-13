// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/chrome_overlay_window/chrome_overlay_container_view.h"

@implementation ChromeOverlayContainerView

- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
  // The container view should not intercept touches. Let subviews handle them.
  UIView* hitView = [super hitTest:point withEvent:event];
  if (hitView == self) {
    return nil;
  }
  return hitView;
}

@end
