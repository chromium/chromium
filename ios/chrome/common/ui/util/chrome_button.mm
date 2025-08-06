// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/chrome_button.h"

#import "ios/chrome/common/ui/util/button_util.h"

@implementation ChromeButton

- (void)setTunedDownStyle:(BOOL)tunedDownStyle {
  if (_tunedDownStyle == tunedDownStyle) {
    return;
  }
  _tunedDownStyle = tunedDownStyle;
  [self setNeedsUpdateConfiguration];
}

- (UIControlState)state {
  UIControlState originalState = [super state];
  if (self.tunedDownStyle) {
    return originalState | UIControlStateTunedDown;
  }
  return originalState;
}

@end
