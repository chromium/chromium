// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_container_view.h"

@implementation OmniboxPopupContainerView

/// Ignore user interaction with itself or PopupEmptySpaceView.
- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
  UIView* hitResult = [super hitTest:point withEvent:event];

  if (hitResult == self)
    return NULL;

  return hitResult;
}

@end
