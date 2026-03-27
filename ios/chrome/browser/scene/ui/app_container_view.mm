// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/ui/app_container_view.h"

@implementation AppContainerView

- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
  UIView* hitView = [super hitTest:point withEvent:event];
  if (hitView == self) {
    return nil;
  }
  return hitView;
}

@end
