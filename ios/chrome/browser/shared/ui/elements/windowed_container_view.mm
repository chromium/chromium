// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/windowed_container_view.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation WindowedContainerView

- (void)addSubview:(UIView*)view {
  self.hidden = YES;
  UIWindow* keyWindow = GetAnyKeyWindow();
  if (self.superview != keyWindow) {
    [keyWindow insertSubview:self atIndex:0];
  }

  if (view.superview == self) {
    return;
  }

  [super addSubview:view];

  // Don't let the hidden `view` take the first responder.  Without the call to
  // -resignFirstResponder below, the keyboard would still appear for the hidden
  // `view`'s focused element.
  [GetFirstResponderSubview(view) resignFirstResponder];
}

@end
