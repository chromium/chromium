// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/passthrough_stack_view.h"

#import <UIKit/UIKit.h>

@implementation PassthroughStackView

- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
  // Use UIStackView's hit test method to see what view is found.
  UIView* stackViewHitTest = [super hitTest:point withEvent:event];

  // If the found view is self instead of a subview, then return nil to pass
  // through.
  return stackViewHitTest == self ? nil : stackViewHitTest;
}

@end
